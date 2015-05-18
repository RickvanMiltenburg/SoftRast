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

#ifdef __cplusplus
extern "C" {
#endif

#include "types.h"
#include <miltyalloc.h>

#define DEBUG_STUFF

#ifdef DEBUG_STUFF
	enum
	{
		RENDER_MODE_FLAT_COLOR,
		RENDER_MODE_UV,
		RENDER_MODE_ZBUFFER,
		RENDER_MODE_TEXTURED,
		RENDER_MODE_MIPMAP
	};
	static const char* RenderModes[] = { "Flat fill", "UV", "Z-Buffer", "Textured", "Mipmap" };

	enum
	{
		TEXTURE_ADDRESSING_LINEAR,
		TEXTURE_ADDRESSING_TILED,
		TEXTURE_ADDRESSING_SWIZZLED,
	};
	static const char* TextureAddressingModes[] = { "Linear", "Tiled", "Swizzled" };

	enum
	{
		TEXTURE_FILTERING_POINT,
		TEXTURE_FILTERING_BILINEAR,
	};
	static const char* TextureFilteringModes[] = { "Point", "Bilinear" };

	enum
	{
		TEXTURE_MIPMAP_NONE,
		TEXTURE_MIPMAP_POINT,
		TEXTURE_MIPMAP_LINEAR,
	};
	static const char* TextureMipmapModes[] = { "None", "Point", "Linear" };

	enum
	{
		FLAG_BACKFACE_CULLING_ENABLED  = (1<<0),
		FLAG_BACKFACE_CULLING_INVERTED = (1<<1),
		FLAG_DEPTH_TESTING             = (1<<2),
		FLAG_CLIP_W                    = (1<<3),
		FLAG_CLIP_FRUSTUM              = (1<<4),
		FLAG_ENABLE_QUAD_RASTERIZATION = (1<<5),
		FLAG_TEXTURE_DITHERING         = (1<<6),
		FLAG_FILTER_LUT                = (1<<7),
		FLAG_QUAD_RASTERIZATION_SIMD   = (1<<8),
		FLAG_AABB_FRUSTUM_CHECK        = (1<<9),
		FLAG_FILL_OUTLINES             = (1<<10),
		FLAG_RASTERIZE                 = (1<<11),

		//FLAG_DERP = (1<<6),
		//FLAG_DERP2 = (1<<7),
		//FLAG_DERP3 = (1<<8),
	};

	typedef struct
	{
		uint32_t flags;
		int renderMode;
		int textureAddressingMode;
		int textureFilteringMode;
		int textureMipmapMode;
		float lodScale, lodBias;
		float clipBorderDist;
	} DEBUG_SETTINGS;
#endif

uint32_t softrast_initialize ( buddy_allocator* allocator );

uint32_t softrast_set_render_target ( uint32_t width, uint32_t height, uint32_t* colorBuffer, uint32_t pitchInBytes );
uint32_t softrast_set_view_matrix ( const bbm_aos_mat4* projMat );
uint32_t softrast_set_projection_matrix ( const bbm_aos_mat4* projMat );

uint32_t softrast_clear_render_target ( );
uint32_t softrast_clear_depth_render_target ( );

uint32_t softrast_texture_load ( softrast_texture* tex, const char* path );
uint32_t softrast_texture_free ( softrast_texture* tex );

uint32_t softrast_model_load ( softrast_model* model, const char* path );
uint32_t softrast_model_free ( softrast_model* model );

uint32_t softrast_render ( softrast_model* model );

#ifdef __cplusplus
};
#endif