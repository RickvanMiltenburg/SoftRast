# Softrast

Softrast is a software rasterizer developed as a freetime project as an excersise in API design, optimization and low-level graphics.

The project is written in C for the most part, with some components written in C++ because of dependencies.

## Features

Following is a list of features of SoftRast:

* Sub-pixel and sub-texel correct rendering
* Perspective correct texture mapping
* Support for trilinear filtering
* Cross-platform support*
* SOA math library (/w SSE & AVX versions)

(* The software rasterizer, model loader and texture loader are cross-platform, only the actual rendering API initialization is not cross-platform)

## TODO

The following table outlines a subset of tasks remaining:

| Task description        | Status           | Comment                                                                      |
|-------------------------|------------------|------------------------------------------------------------------------------|
| AABB frustum culling    | Work in progress | Working but appears to have some failure cases                               |
| Implement scene graph   | TODO             | Currently more of a scene array than a scene graph                           |
| Rasterizer cleanup      | TODO             |                                                                              |
| Rasterizer optimization | TODO             | Outline filling and rasterization are far slower than they should ideally be |
| Lighting                | TODO             |                                                                              |

## License

MIT license

> Copyright (c) 2015 Rick van Miltenburg
> 
> Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
> 
> The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
> 
> THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.