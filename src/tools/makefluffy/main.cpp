
//
// This source file is part of appleseed.
// Visit http://appleseedhq.net/ for additional information and resources.
//
// This software is released under the MIT license.
//
// Copyright (c) 2014 Francois Beaune, The appleseedhq Organization
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

// Project headers.
#include "commandlinehandler.h"

// appleseed.shared headers.
#include "application/application.h"
#include "application/superlogger.h"

// appleseed.renderer headers.
#include "renderer/api/object.h"
#include "renderer/api/project.h"
#include "renderer/api/scene.h"

// appleseed.foundation headers.
#include "foundation/math/sampling/mappings.h"
#include "foundation/math/beziercurve.h"
#include "foundation/math/cdf.h"
#include "foundation/math/qmc.h"
#include "foundation/math/rng.h"
#include "foundation/math/scalar.h"
#include "foundation/math/vector.h"
#include "foundation/utility/autoreleaseptr.h"
#include "foundation/utility/foreach.h"
#include "foundation/utility/uid.h"

// boost headers.
#include "boost/filesystem/path.hpp"

// Standard headers.
#include <cassert>
#include <cstddef>
#include <cstring>
#include <map>
#include <string>
#include <vector>

using namespace appleseed::makefluffy;
using namespace appleseed::shared;
using namespace boost;
using namespace foundation;
using namespace renderer;
using namespace std;

namespace
{
    //
    // Fluffification parameters.
    //

    struct FluffParams
    {
        size_t  m_curve_count;
        double  m_curve_length;
        double  m_root_width;
        double  m_tip_width;
        double  m_length_fuzziness;
        double  m_curliness;
        size_t  m_split_count;

        explicit FluffParams(const CommandLineHandler& cl)
        {
            m_curve_count = cl.m_curves.values()[0];
            m_curve_length = cl.m_length.values()[0];
            m_root_width = cl.m_root_width.values()[0];
            m_tip_width = cl.m_tip_width.values()[0];
            m_length_fuzziness = cl.m_length_fuzziness.values()[0];
            m_curliness = cl.m_curliness.values()[0];
            m_split_count = cl.m_presplits.values()[0];
        }
    };


    //
    // Fluffification algorithm.
    //

    struct SupportTriangle
    {
        Vector3d    m_v0, m_v1, m_v2;
        Vector3d    m_normal;
        double      m_area;
    };

    void extract_support_triangles(
        const MeshObject&           object,
        vector<SupportTriangle>&    support_triangles,
        CDF<size_t, double>&        cdf)
    {
        const size_t triangle_count = object.get_triangle_count();
        for (size_t triangle_index = 0; triangle_index < triangle_count; ++triangle_index)
        {
            // Fetch the triangle.
            const Triangle& triangle = object.get_triangle(triangle_index);

            // Retrieve object instance space vertices of the triangle.
            const Vector3d v0 = Vector3d(object.get_vertex(triangle.m_v0));
            const Vector3d v1 = Vector3d(object.get_vertex(triangle.m_v1));
            const Vector3d v2 = Vector3d(object.get_vertex(triangle.m_v2));

            // Compute the geometric normal to the triangle and the area of the triangle.
            Vector3d normal = cross(v1 - v0, v2 - v0);
            const double normal_norm = norm(normal);
            if (normal_norm == 0.0)
                continue;
            const double rcp_normal_norm = 1.0 / normal_norm;
            const double rcp_area = 2.0 * rcp_normal_norm;
            const double area = 0.5 * normal_norm;
            normal *= rcp_normal_norm;
            assert(is_normalized(normal));

            // Create and store the support triangle.
            SupportTriangle support_triangle;
            support_triangle.m_v0 = v0;
            support_triangle.m_v1 = v1;
            support_triangle.m_v2 = v2;
            support_triangle.m_normal = normal;
            support_triangle.m_area = area;
            support_triangles.push_back(support_triangle);

            // Insert the support triangle into the CDF.
            cdf.insert(support_triangles.size() - 1, area);
        }

        assert(cdf.valid());
        cdf.prepare();
    }

    template <typename RNG>
    static Vector2d rand_vector2d(RNG& rng)
    {
        Vector2d v;
        v[0] = rand_double2(rng);
        v[1] = rand_double2(rng);
        return v;
    }

    void split_and_store(
        CurveObject&                object,
        const BezierCurve3d&        curve,
        const size_t                split_count)
    {
        if (split_count > 0)
        {
            BezierCurve3d child1, child2;
            curve.split(child1, child2);
            split_and_store(object, child1, split_count - 1);
            split_and_store(object, child2, split_count - 1);
        }
        else object.push_curve(curve);
    }

    auto_release_ptr<CurveObject> create_curve_object(
        const Assembly&             assembly,
        const MeshObject&           support_object,
        const FluffParams&          params)
    {
        const size_t ControlPointCount = 4;

        vector<SupportTriangle> support_triangles;
        CDF<size_t, double> cdf;
        extract_support_triangles(support_object, support_triangles, cdf);

        const string curve_object_name = string(support_object.get_name()) + "_curves";
        auto_release_ptr<CurveObject> curve_object =
            CurveObjectFactory::create(
                curve_object_name.c_str(),
                ParamArray());

        curve_object->reserve_curves(params.m_curve_count);

        Vector3d points[ControlPointCount];
        double widths[ControlPointCount];

        MersenneTwister rng;

        for (size_t i = 0; i < params.m_curve_count; ++i)
        {
            static const size_t Bases[] = { 2, 3 };
            const Vector3d s = hammersley_sequence<double, 3>(Bases, i, params.m_curve_count);

            const size_t triangle_index = cdf.sample(s[0]).first;
            const SupportTriangle& st = support_triangles[triangle_index];
            const Vector3d bary = sample_triangle_uniform(Vector2d(s[1], s[2]));

            points[0] = st.m_v0 * bary[0] + st.m_v1 * bary[1] + st.m_v2 * bary[2];
            widths[0] = params.m_root_width;

            const double f = rand_double1(rng, -params.m_length_fuzziness, +params.m_length_fuzziness);
            const double length = params.m_curve_length * (1.0 + f);

            for (size_t p = 1; p < ControlPointCount; ++p)
            {
                const double r = static_cast<double>(p) / (ControlPointCount - 1);
                const Vector3d f = params.m_curliness * sample_sphere_uniform(rand_vector2d(rng));
                points[p] = points[0] + length * (r * st.m_normal + f);
                widths[p] = lerp(params.m_root_width, params.m_tip_width, r);
            }

            const BezierCurve3d curve(&points[0], &widths[0]);
            split_and_store(curve_object.ref(), curve, params.m_split_count);
        }

        return curve_object;
    }

    void make_fluffy(const Assembly& assembly, const FluffParams& params)
    {
        const ObjectContainer& objects = assembly.objects();

        // Link object instances to objects.
        map<UniqueID, vector<UniqueID> > objects_to_instances;
        for (const_each<ObjectInstanceContainer> i = assembly.object_instances(); i; ++i)
        {
            const ObjectInstance& object_instance = *i;
            const Object* object = object_instance.find_object();
            if (object)
                objects_to_instances[object->get_uid()].push_back(object_instance.get_uid());
        }

        // Index-based iteration since we add objects during iteration.
        for (size_t i = 0; i < objects.size(); ++i)
        {
            const Object& object = *objects.get_by_index(i);

            if (strcmp(object.get_model(), MeshObjectFactory::get_model()))
                continue;

            if (string(object.get_name()).find("light") != string::npos)
                continue;

            auto_release_ptr<CurveObject> curve_object =
                create_curve_object(
                    assembly,
                    static_cast<const MeshObject&>(object),
                    params);

            const vector<UniqueID>& support_instances = objects_to_instances[object.get_uid()];

            for (size_t j = 0; j < support_instances.size(); ++j)
            {
                const ObjectInstance* support_instance =
                    assembly.object_instances().get_by_uid(support_instances[j]);
                assert(support_instance);

                const string curve_object_instance_name = string(curve_object->get_name()) + "_inst";
                auto_release_ptr<ObjectInstance> curve_object_instance =
                    ObjectInstanceFactory::create(
                        curve_object_instance_name.c_str(),
                        support_instance->get_parameters(),
                        curve_object->get_name(),
                        support_instance->get_transform(),
                        support_instance->get_front_material_mappings(),
                        support_instance->get_back_material_mappings());

                assembly.object_instances().insert(curve_object_instance);
            }

            assembly.objects().insert(auto_release_ptr<Object>(curve_object));
        }
    }

    void make_fluffy(Project& project, const FluffParams& params)
    {
        assert(project.get_scene());

        const Scene& scene = *project.get_scene();

        for (const_each<AssemblyContainer> i = scene.assemblies(); i; ++i)
            make_fluffy(*i, params);
    }
}


//
// Entry point of makefluffy.
//

int main(int argc, const char* argv[])
{
    SuperLogger logger;
    Application::check_installation(logger);

    CommandLineHandler cl;
    cl.parse(argc, argv, logger);

    // Initialize the renderer's logger.
    global_logger().add_target(&logger.get_log_target());

    // Retrieve the command line arguments.
    const string& input_filepath = cl.m_filenames.values()[0];
    const string& output_filepath = cl.m_filenames.values()[1];
    const FluffParams params(cl);

    // Construct the schema file path.
    const filesystem::path schema_filepath =
          filesystem::path(Application::get_root_path())
        / "schemas"
        / "project.xsd";

    // Read the input project from disk.
    ProjectFileReader reader;
    auto_release_ptr<Project> project(
        reader.read(
            input_filepath.c_str(),
            schema_filepath.string().c_str()));

    // Bail out if the project couldn't be loaded.
    if (project.get() == 0)
        return 1;

    // Fluffify the project.
    make_fluffy(project.ref(), params);

    // Write the project back to disk.
    const bool success =
        ProjectFileWriter::write(
            project.ref(),
            output_filepath.c_str());

    return success ? 0 : 1;
}