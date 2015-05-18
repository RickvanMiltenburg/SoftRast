/*
	BareBones Math, by Rick van Miltenburg

	Custom math library meant to provide the exact functions I want and the way I want to use them.

	---

	Copyright (c) 2015 Rick van Miltenburg

	Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#pragma once

#include <stdint.h>

typedef struct { union { struct { float *x; }; float* cells[1]; }; uint32_t vectorCount; } bbm_soa_vec1;
typedef struct { union { struct { float *x, *y; }; float* cells[2]; }; uint32_t vectorCount; } bbm_soa_vec2;
typedef struct { union { struct { float *x, *y, *z; }; float* cells[3]; }; uint32_t vectorCount; } bbm_soa_vec3;
typedef struct { union { struct { float *x, *y, *z, *w; }; float* cells[4]; }; uint32_t vectorCount; } bbm_soa_vec4;
typedef struct { union { float* rows[4][4]; float* cells[16]; }; uint32_t matrixCount; } bbm_soa_mat4;

typedef struct { union { struct { float x, y; }; float cells[2]; }; } bbm_aos_vec2;
typedef struct { union { struct { float x, y, z; }; float cells[3]; }; } bbm_aos_vec3;
typedef struct { union { struct { float x, y, z, w; }; float cells[4]; }; } bbm_aos_vec4;
typedef struct { union { float rows[4][4]; float cells[16]; }; } bbm_aos_mat4;