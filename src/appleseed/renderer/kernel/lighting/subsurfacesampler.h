
//
// This source file is part of appleseed.
// Visit http://appleseedhq.net/ for additional information and resources.
//
// This software is released under the MIT license.
//
// Copyright (c) 2015 Francois Beaune, The appleseedhq Organization
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

#ifndef APPLESEED_RENDERER_KERNEL_LIGHTING_SUBSURFACESAMPLER_H
#define APPLESEED_RENDERER_KERNEL_LIGHTING_SUBSURFACESAMPLER_H

// appleseed.renderer headers.
#include "renderer/global/globaltypes.h"
#include "renderer/kernel/shading/shadingpoint.h"

// appleseed.foundation headers.
#include "foundation/core/concepts/noncopyable.h"

// Standard headers.
#include <cstddef>

// Forward declarations.
namespace renderer  { class BSSRDF; }
namespace renderer  { class ShadingContext; }

namespace renderer
{

//
// Subsurface sample: the result of sampling a BSSRDF profile to find incoming points given an outgoing point.
//

class SubsurfaceSample
{
  public:
    ShadingPoint    m_point;
    double          m_probability;
    double          m_eta;
};


//
// Subsurface sampler.
//

class SubsurfaceSampler
  : public foundation::NonCopyable
{
  public:
    // Constructor.
    explicit SubsurfaceSampler(
        const ShadingContext&   shading_context);

    size_t sample(
        SamplingContext&        sampling_context,
        const ShadingPoint&     outgoing_point,
        const BSSRDF&           bssrdf,
        const void*             bssrdf_data,
        SubsurfaceSample        samples[],
        const size_t            max_sample_count);

  private:
    const ShadingContext&       m_shading_context;
};

}       // namespace renderer

#endif  // !APPLESEED_RENDERER_KERNEL_LIGHTING_SUBSURFACESAMPLER_H