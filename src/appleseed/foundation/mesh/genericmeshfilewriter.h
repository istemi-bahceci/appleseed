
//
// This source file is part of appleseed.
// Visit http://appleseedhq.net/ for additional information and resources.
//
// This software is released under the MIT license.
//
// Copyright (c) 2010-2013 Francois Beaune, Jupiter Jazz Limited
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

#ifndef APPLESEED_FOUNDATION_MESH_GENERICMESHFILEWRITER_H
#define APPLESEED_FOUNDATION_MESH_GENERICMESHFILEWRITER_H

// appleseed.foundation headers.
#include "foundation/mesh/imeshfilewriter.h"
#include "foundation/platform/compiler.h"

// appleseed.main headers.
#include "main/dllsymbol.h"

// Forward declarations.
namespace foundation    { class IMeshWalker; }

namespace foundation
{

//
// Write a mesh file using the right writer based on the extension of the mesh file name.
//

class DLLSYMBOL GenericMeshFileWriter
  : public IMeshFileWriter
{
  public:
    // Constructor.
    explicit GenericMeshFileWriter(const char* filename);

    // Destructor.
    virtual ~GenericMeshFileWriter();

    // Write a mesh.
    virtual void write(const IMeshWalker& walker) OVERRIDE;

  private:
    IMeshFileWriter* m_writer;
};

}       // namespace foundation

#endif  // !APPLESEED_FOUNDATION_MESH_GENERICMESHFILEWRITER_H
