/*
	SoftRast software rasterizer, by Rick van Miltenburg

	Software rasterizer created for fun and educational purposes.

	---

	Copyright (c) 2015 Rick van Miltenburg

	Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#pragma once

#include <stdint.h>
#include "BarebonesMath/include/bbm.h"

typedef struct
{
	uint32_t** mipData;
	const char* path;
	uint32_t mipLevels;
	uint16_t width;
	uint16_t height;
} softrast_texture;

typedef struct
{
	softrast_texture* texture;
	uint32_t* indices;
	uint32_t indexCount;
} softrast_submesh;

typedef struct
{
	bbm_soa_vec3 positions;
	bbm_soa_vec4 transformedPositions;
	bbm_soa_vec3 normals;
	bbm_soa_vec2 texcoords;
	bbm_aos_vec3 aabbMin;
	bbm_aos_vec3 aabbMax;
	softrast_submesh* submeshes;
	uint32_t submeshCount;
} softrast_mesh;

typedef struct
{
	softrast_texture* textures;
	softrast_mesh* meshes;
	uint32_t textureCount;
	uint32_t meshCount;
} softrast_model;