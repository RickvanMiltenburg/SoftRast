/*
	SoftRast software rasterizer, by Rick van Miltenburg

	Software rasterizer created for fun and educational purposes.

	---

	Copyright (c) 2015 Rick van Miltenburg

	Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include "softrast.h"

#include <string.h>
#include <assert.h>
#include <math.h>
#include <immintrin.h>
#include <float.h>

////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

buddy_allocator* _softrastAllocator;

////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

enum
{
	VIEW_PROJECTION_DIRTY_BIT = (1<<0),
};

#define AOS_OUTLINE_TABLE 1
#if AOS_OUTLINE_TABLE
typedef struct
{
	uint32_t flags, padding;

	float minX, maxX;
	float minZ, maxZ;

	float minU, maxU;
	float minV, maxV;

	float minNX, maxNX;
	float minNY, maxNY;
	float minNZ, maxNZ;
} outline_table_entry;
#endif

struct
{
	bbm_aos_mat4 projectionMatrix, viewMatrix, viewProjectionMatrix;
	float nearClip, farClip;
	uint32_t flags;

#if AOS_OUTLINE_TABLE
	outline_table_entry* outlineTable;
#else
	struct
	{
		float *minX, *maxX;
		float *minZ, *maxZ;

		float *minU, *maxU;
		float *minV, *maxV;

		float *minNX, *maxNX;
		float *minNY, *maxNY;
		float *minNZ, *maxNZ;
	} outlineTable;
#endif

	struct
	{
		uint32_t* colorBuffer;
		float* depthBuffer;
		uint32_t depthBufferQuadFloatStride;
		uint32_t width;
		uint32_t height;
		uint32_t pitch;
	} renderTarget;
} globalData;

//#ifdef _DEBUG
	DEBUG_SETTINGS Debug;
//#endif

////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

uint32_t softrast_initialize ( buddy_allocator* allocator )
{
	_softrastAllocator = allocator;
	return 0;
}

////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

uint32_t softrast_set_render_target ( uint32_t width, uint32_t height, uint32_t* colorBuffer, uint32_t pitchInBytes )
{
	//--------------------------------
	// Make sure we have an allocator before continuing
	//--------------------------------
	if ( !_softrastAllocator )
		return -1;	// No allocator

	//--------------------------------
	// Check whether or not we need to reallocate outline tables and the such
	//--------------------------------
	if ( width != globalData.renderTarget.width || height != globalData.renderTarget.height )
	{
		//--------------------------------
		// Free internally allocated memory (all allocated in one row, so deallocating the depth buffer deallocates everything)
		//--------------------------------
		if ( globalData.renderTarget.depthBuffer )
			miltyalloc_buddy_allocator_free ( _softrastAllocator, globalData.renderTarget.depthBuffer );

		//--------------------------------
		// Allocate new internal memory
		//--------------------------------
		uint32_t alignedWidth  = (width  + 1) & (~1);
		uint32_t alignedHeight = (height + 1) & (~1);

		uint32_t outlineTableSize;
#if AOS_OUTLINE_TABLE
		outlineTableSize = alignedHeight * sizeof ( outline_table_entry );
#else
		outlineTableSize = 2 * 7 * alignedHeight * sizeof ( float );
#endif

		uint32_t allocSize = alignedWidth * alignedHeight * sizeof ( float ) + outlineTableSize;
		void* memory = miltyalloc_buddy_allocator_alloc ( _softrastAllocator, allocSize );
		if ( memory == NULL )
		{
			memset ( &globalData.outlineTable, 0, sizeof ( globalData.outlineTable ) );
			memset ( &globalData.renderTarget, 0, sizeof ( globalData.renderTarget ) );
			return -2;	// Could not allocate (enough) memory
		}

		globalData.renderTarget.depthBuffer = (float*)memory;
	
		//--------------------------------
		// Prepare outline table pointers
		//--------------------------------
		float* outlinePtr = globalData.renderTarget.depthBuffer + alignedWidth * alignedHeight;
		globalData.renderTarget.depthBufferQuadFloatStride = 2 * alignedWidth;

#if AOS_OUTLINE_TABLE
		globalData.outlineTable = (outline_table_entry*)outlinePtr;
#else
		globalData.outlineTable.minX = outlinePtr + 0 * alignedHeight, globalData.outlineTable.maxX = outlinePtr + 1 * alignedHeight;
		globalData.outlineTable.minZ = outlinePtr + 2 * alignedHeight, globalData.outlineTable.maxZ = outlinePtr + 3 * alignedHeight;

		globalData.outlineTable.minU = outlinePtr + 4 * alignedHeight, globalData.outlineTable.maxU = outlinePtr + 5 * alignedHeight;
		globalData.outlineTable.minV = outlinePtr + 6 * alignedHeight, globalData.outlineTable.maxV = outlinePtr + 7 * alignedHeight;

		globalData.outlineTable.minNX = outlinePtr + 8  * alignedHeight, globalData.outlineTable.maxNX = outlinePtr + 9  * alignedHeight;
		globalData.outlineTable.minNY = outlinePtr + 10 * alignedHeight, globalData.outlineTable.maxNY = outlinePtr + 11 * alignedHeight;
		globalData.outlineTable.minNZ = outlinePtr + 12 * alignedHeight, globalData.outlineTable.maxNZ = outlinePtr + 13 * alignedHeight;
#endif

		//--------------------------------
		// Prepare outline table default values where required
		//--------------------------------
#if AOS_OUTLINE_TABLE
		for ( uint32_t i = 0; i < alignedHeight; i++ )
		{
			globalData.outlineTable[i].flags = 0;
			globalData.outlineTable[i].minX = (float)width, globalData.outlineTable[i].maxX = 0;
		}
#else
		for ( uint32_t i = 0; i < alignedHeight; i++ )
		{
			globalData.outlineTable.minX[i] = (float)width, globalData.outlineTable.maxX[i] = 0;
		}
#endif
	}

	//--------------------------------
	// Set variables
	//--------------------------------
	globalData.renderTarget.colorBuffer = colorBuffer;
	globalData.renderTarget.width       = width;
	globalData.renderTarget.height      = height;
	globalData.renderTarget.pitch       = pitchInBytes;

	return 0;
}

uint32_t softrast_set_view_matrix ( const bbm_aos_mat4* viewMat )
{
	globalData.viewMatrix = *viewMat;
	globalData.flags |= VIEW_PROJECTION_DIRTY_BIT;
	return 0;
}

uint32_t softrast_set_projection_matrix ( const bbm_aos_mat4* projMat )
{
	//--------------------------------
	// Calculate near and far clip distances
	//--------------------------------
	bbm_aos_vec4 minZOut, minZIn = { .x = 0.0f, .y = 0.0f, .z = -1.0f, .w = 1.0f };
	bbm_aos_vec4 maxZOut, maxZIn = { .x = 0.0f, .y = 0.0f, .z =  1.0f, .w = 1.0f };
	bbm_aos_mat4 projInv;
	bbm_aos_mat4_inverse ( &projInv, projMat );
	bbm_aos_mat4_mul_aos_vec4 ( &minZOut, &projInv, &minZIn );
	bbm_aos_mat4_mul_aos_vec4 ( &maxZOut, &projInv, &maxZIn );
	globalData.nearClip = 1.0f / minZOut.w, globalData.farClip = 1.0f / maxZOut.w;

	//--------------------------------
	// Set projection matrix
	//--------------------------------
	globalData.projectionMatrix = *projMat;
	globalData.flags           |= VIEW_PROJECTION_DIRTY_BIT;

	//--------------------------------
	// Return success
	//--------------------------------
	return 0;
}

////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

uint32_t softrast_clear_render_target ( )
{
	if ( !globalData.renderTarget.colorBuffer )
		return -1;
	memset ( globalData.renderTarget.colorBuffer, 0x80, globalData.renderTarget.pitch * globalData.renderTarget.height );
	return 0;
}

uint32_t softrast_clear_depth_render_target ( )
{
	if ( !globalData.renderTarget.depthBuffer )
		return -1;
	memset ( globalData.renderTarget.depthBuffer, 0x00, globalData.renderTarget.width * globalData.renderTarget.height * sizeof ( float ) );
	return 0;
}

////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

#define SOACLIP 0
#if SOACLIP
// clipPoint should always be the positive value; sign indicates whether or not the positive or negative side should be clipped!
static uint32_t __softrast_clip ( bbm_soa_vec4* out, bbm_soa_vec2* ouv, const bbm_soa_vec4* in, const bbm_soa_vec2* iuv, const uint32_t inCount, uint32_t clipDim, float sign, float clipPoint )
{
	if ( inCount < 3 )
		return 0;

	uint32_t vectorCount = 0;

	const float* x   = in->x;
	const float* y   = in->y;
	const float* z   = in->z;
	const float* w   = in->w;
	const float* u   = iuv->x;
	const float* v   = iuv->y;
	const float* val = in->cells[clipDim];

		  float* ox  = out->x;
		  float* oy  = out->y;
		  float* oz  = out->z;
		  float* ow  = out->w;
		  float* ou  = ouv->x;
		  float* ov  = ouv->y;

	uint32_t last  = inCount-1;
	//float lastDist = (1.0f - sign * val[last]) - 0.05f;
	float lastDist = (clipPoint - sign * val[last]);
						
	float sdist;
	for ( uint32_t i = 0; i < inCount; last = i++, lastDist = sdist )
	{
		sdist = (clipPoint - sign * *(val++));
		//sdist = (1.0f - sign * (*val++)) - 0.05f;
		if ( lastDist <= 0.0f )
		{
			// Last was OUT
			if ( sdist > 0.0f )
			{
				// Cur is IN
				// Push intersection of vector between last and now with the plane

				// Determine lerp factor between last and current
				float f = sdist / (sdist - lastDist);
				*(ox++) = x[last] * f + x[i] * (1.0f-f);
				*(oy++) = y[last] * f + y[i] * (1.0f-f);
				*(oz++) = z[last] * f + z[i] * (1.0f-f);
				*(ow++) = w[last] * f + w[i] * (1.0f-f);
				*(ou++) = u[last] * f + u[i] * (1.0f-f);
				*(ov++) = v[last] * f + v[i] * (1.0f-f);

				*(ox++) = x[i];
				*(oy++) = y[i];
				*(oz++) = z[i];
				*(ow++) = w[i];
				*(ou++) = u[i];
				*(ov++) = v[i];

				vectorCount += 2;
			}
		}
		else
		{
			// Last was IN
			if ( sdist > 0.0f )
			{
				// Cur is IN
				// Push
				*(ox++) = x[i];
				*(oy++) = y[i];
				*(oz++) = z[i];
				*(ow++) = w[i];
				*(ou++) = u[i];
				*(ov++) = v[i];

				vectorCount++;
			}
			else
			{
				// Cur is OUT
				// Push intersection of vector between last and now with the plane
				float f = lastDist / (lastDist - sdist);
				*(ox++) = x[i] * f + x[last] * (1.0f-f);
				*(oy++) = y[i] * f + y[last] * (1.0f-f);
				*(oz++) = z[i] * f + z[last] * (1.0f-f);
				*(ow++) = w[i] * f + w[last] * (1.0f-f);
				*(ou++) = u[i] * f + u[last] * (1.0f-f);
				*(ov++) = v[i] * f + v[last] * (1.0f-f);

				//sdist = 0.001f;

				vectorCount++;
			}
		}
	}
	return vectorCount;
}
#else
typedef struct
{
	struct
	{
		union
		{
			struct { float x, y, z, w; };
			float cell[4];
		};
	} position;
	float u, v;
} vertex;

// clipPoint should always be the positive value; sign indicates whether or not the positive or negative side should be clipped!
static uint32_t __softrast_clip ( vertex* outv, const vertex* inv, const uint32_t inCount, uint32_t clipDim, float sign, float clipPoint )
{
	if ( inCount < 3 )
		return 0;

	uint32_t vectorCount = 0;

	uint32_t last  = inCount-1;
	
	const vertex* curinv  = inv;
	const vertex* lastinv = inv + last;
	      vertex* curov   = outv;

	float sdist;
	float lastDist = (clipPoint - sign * lastinv->position.cell[clipDim]);
	
	for ( uint32_t i = 0; i < inCount; last = i++, lastDist = sdist )
	{
		sdist = (clipPoint - sign * (curinv++)->position.cell[clipDim]);
		if ( lastDist <= 0.0f )
		{
			// Last was OUT
			if ( sdist > 0.0f )
			{
				// Cur is IN
				// Push intersection of vector between last and now with the plane

				// Determine lerp factor between last and current
				float f = sdist / (sdist - lastDist);
				curov->position.x = inv[last].position.x * f + inv[i].position.x * (1.0f-f);
				curov->position.y = inv[last].position.y * f + inv[i].position.y * (1.0f-f);
				curov->position.z = inv[last].position.z * f + inv[i].position.z * (1.0f-f);
				curov->position.w = inv[last].position.w * f + inv[i].position.w * (1.0f-f);
				curov->u          = inv[last].u          * f + inv[i].u          * (1.0f-f);
				curov->v          = inv[last].v          * f + inv[i].v          * (1.0f-f);
				curov++;

				curov->position.x = inv[i].position.x;
				curov->position.y = inv[i].position.y;
				curov->position.z = inv[i].position.z;
				curov->position.w = inv[i].position.w;
				curov->u          = inv[i].u;
				curov->v          = inv[i].v;
				curov++;

				vectorCount += 2;
			}
		}
		else
		{
			// Last was IN
			if ( sdist > 0.0f )
			{
				// Cur is IN
				// Push
				curov->position.x = inv[i].position.x;
				curov->position.y = inv[i].position.y;
				curov->position.z = inv[i].position.z;
				curov->position.w = inv[i].position.w;
				curov->u          = inv[i].u;
				curov->v          = inv[i].v;
				curov++;

				vectorCount++;
			}
			else
			{
				// Cur is OUT
				// Push intersection of vector between last and now with the plane
				float f = lastDist / (lastDist - sdist);
				curov->position.x = inv[i].position.x * f + inv[last].position.x * (1.0f-f);
				curov->position.y = inv[i].position.y * f + inv[last].position.y * (1.0f-f);
				curov->position.z = inv[i].position.z * f + inv[last].position.z * (1.0f-f);
				curov->position.w = inv[i].position.w * f + inv[last].position.w * (1.0f-f);
				curov->u          = inv[i].u          * f + inv[last].u          * (1.0f-f);
				curov->v          = inv[i].v          * f + inv[last].v          * (1.0f-f);
				curov++;

				vectorCount++;
			}
		}
	}
	return vectorCount;
}
#endif

typedef enum
{
	AABB_FRUSTUM_INSIDE,
	AABB_FRUSTUM_OUTSIDE,
	AABB_FRUSTUM_INTERSECT,
} aabb_frustum_result;

static aabb_frustum_result __aabb_check_frustum ( softrast_mesh* mesh )
{
	//--------------------------------
	// Prepare pre-transform AABB corner vectors, and storage for the post-transform output
	//--------------------------------
	__declspec(align(16))
		float _aabbCornersIn[8*3] = {
			// ---           --+              -+-              -++              +--              +-+              ++-              +++
			mesh->aabbMin.x, mesh->aabbMin.x, mesh->aabbMin.x, mesh->aabbMin.x, mesh->aabbMax.x, mesh->aabbMax.x, mesh->aabbMax.x, mesh->aabbMax.x,
			mesh->aabbMin.y, mesh->aabbMin.y, mesh->aabbMax.y, mesh->aabbMax.y, mesh->aabbMin.y, mesh->aabbMin.y, mesh->aabbMax.y, mesh->aabbMax.y,
			mesh->aabbMin.z, mesh->aabbMax.z, mesh->aabbMin.z, mesh->aabbMax.z, mesh->aabbMin.z, mesh->aabbMax.z, mesh->aabbMin.z, mesh->aabbMax.z,
		};
	__declspec(align(16))
		float _aabbCornersOut[8*4];
	bbm_soa_vec3 aabbCornersIn;
	bbm_soa_vec4 aabbCornersOut;

	//--------------------------------
	// Tramsform the AABB corners with the VP matrix and divide XYZ by W
	//--------------------------------
	bbm_soa_vec3_init ( &aabbCornersIn,  _aabbCornersIn,  _aabbCornersIn  + 8, _aabbCornersIn  + 16, 8 );
	bbm_soa_vec4_init ( &aabbCornersOut, _aabbCornersOut, _aabbCornersOut + 8, _aabbCornersOut + 16, _aabbCornersOut + 24, 8 );

	bbm_aos_mat4_mul_soa_vec3w1_out_vec4 ( &aabbCornersOut, &globalData.viewProjectionMatrix, &aabbCornersIn );

	bbm_soa_vec4_overwrite_div_xyz_w ( &aabbCornersOut );

	//--------------------------------
	// Prepare storage for the out-of-range codes
	//--------------------------------
	uint8_t outcode[8] = { 0 };

	//--------------------------------
	// Check XYZ out of range codes
	//--------------------------------
	for ( uint32_t cell = 0; cell < 3; cell++ )
	{
		float* v = aabbCornersOut.cells[cell];
		uint8_t* oc = outcode;
		for ( uint32_t i = 0; i < 8; i++, v++, oc++ )
		{
			if ( *v < -1.0f )
				*oc |= (0x01<<cell);
			if ( *v > 1.0f )
				*oc |= (0x10<<cell);
		}
	}

	//--------------------------------
	// Check W out of range code
	//--------------------------------
	float* w    = aabbCornersOut.w;
	uint8_t* oc = outcode;
	for ( uint32_t i = 0; i < 8; i++, w++, oc++ )
	{
		if ( *w < globalData.nearClip )
			*oc |= 0x08;
		if ( *w > globalData.farClip )
			*oc |= 0x80;
	}

	//--------------------------------
	// Determine planes that all corners are outside of, and all planes that have at least one corner outside
	//--------------------------------
	oc = outcode;
	uint8_t sharedoutcode = 0xFF;
	uint8_t indivoutcode  = 0x00;
	for ( uint32_t i = 0; i < 8; i++, oc++ )
	{
		sharedoutcode &= *oc;
		indivoutcode  |= *oc;
	}

	//--------------------------------
	// Determine output
	//--------------------------------
	if ( indivoutcode == 0x00 )			// Not a single plane has a single corner outside? Return INSIDE (no clipping required)
		return AABB_FRUSTUM_INSIDE;
	else if ( sharedoutcode )			// At least one plane where all corners are outside? Return OUTSIDE (skip mesh)
		return AABB_FRUSTUM_OUTSIDE;
	else								// Intersections, but not shared by all corners? Return INTERSECT (render; clipping required)
		return AABB_FRUSTUM_INTERSECT;
}

uint32_t softrast_render ( softrast_model* model )
{
	//--------------------------------
	// Update view projection matrix if needed
	//--------------------------------
	if ( globalData.flags & VIEW_PROJECTION_DIRTY_BIT )
	{
		bbm_aos_mat4_mul_aos_mat4 ( &globalData.viewProjectionMatrix, &globalData.projectionMatrix, &globalData.viewMatrix );
		globalData.flags &= ~VIEW_PROJECTION_DIRTY_BIT;
	}

	//--------------------------------
	// Transform mesh vertex positions
	//--------------------------------
	softrast_mesh* mesh = model->meshes;
	for ( uint32_t i = 0; i < model->meshCount; i++, mesh++ )
	{
		bbm_aos_mat4_mul_soa_vec3w1_out_vec4 ( &mesh->transformedPositions, &globalData.viewProjectionMatrix, &mesh->positions );
	}

	//--------------------------------
	// Variables
	//--------------------------------
#if SOACLIP
	__declspec(align(16)) float clippedVertData[12*8];
	bbm_soa_vec4 clippedVerts[2];
	bbm_soa_vec2 clippedVertUVs[2];
	bbm_soa_vec4_init ( &clippedVerts[0],   clippedVertData,      clippedVertData + 8,  clippedVertData + 16, clippedVertData + 24, 3 );
	bbm_soa_vec4_init ( &clippedVerts[1],   clippedVertData + 32, clippedVertData + 40, clippedVertData + 48, clippedVertData + 56, 0 );
	bbm_soa_vec2_init ( &clippedVertUVs[0], clippedVertData + 64, clippedVertData + 72, 3 );
	bbm_soa_vec2_init ( &clippedVertUVs[1], clippedVertData + 80, clippedVertData + 88, 0 );
#else
	__declspec(align(64)) vertex clippedVerts[2][6];
	vertex* curVerts = &clippedVerts[0][0], *tempVerts = &clippedVerts[1][0];
#endif

	const float offset[3]  = { globalData.renderTarget.width / 2.0f, globalData.renderTarget.height / 2.0f, 0.0f };
	const float bias[3]    = { globalData.renderTarget.width / 2.0f, globalData.renderTarget.height / 2.0f, 0.0f };
	const float epsilon[3] = { Debug.clipBorderDist / bias[0], Debug.clipBorderDist / bias[1], 0.0f };

	//--------------------------------
	// Rasterize
	//--------------------------------
	mesh = model->meshes;
	for ( uint32_t i = 0; i < model->meshCount; i++, mesh++ )
	{
		bbm_soa_vec4 transformedPositions = mesh->transformedPositions;
		bbm_soa_vec2 texcoords            = mesh->texcoords;

		aabb_frustum_result res = AABB_FRUSTUM_INTERSECT;
		if ( Debug.flags & FLAG_AABB_FRUSTUM_CHECK )
		{
			res = __aabb_check_frustum ( mesh );
			if ( res == AABB_FRUSTUM_OUTSIDE )
				continue;
		}

		if ( !(Debug.flags & FLAG_FILL_OUTLINES) )
			continue;

		softrast_submesh* submesh = mesh->submeshes;
		for ( uint32_t j = 0; j < mesh->submeshCount; j++, submesh++ )
		{
			//--------------------------------
			// Variables
			//--------------------------------
			const uint32_t triCount = submesh->indexCount / 3;
			uint32_t* index = submesh->indices;

			//--------------------------------
			// Sanity checks
			//--------------------------------
			assert ( (submesh->indexCount % 3) == 0 );

			//--------------------------------
			// Fill triangles into outline table
			//--------------------------------
			for ( uint32_t k = 0; k < triCount; k++ )
			{
				int32_t minTriY = globalData.renderTarget.height, maxTriY = 0;

				//--------------------------------
				// Initialize vertices for clipping
				//--------------------------------
#if SOACLIP
#if 1
				  clippedVerts[0].x[0] = mesh->transformedPositions.x[*(index)],   clippedVerts[0].x[1] = mesh->transformedPositions.x[*(index+1)],   clippedVerts[0].x[2] = mesh->transformedPositions.x[*(index+2)];
				  clippedVerts[0].y[0] = mesh->transformedPositions.y[*(index)],   clippedVerts[0].y[1] = mesh->transformedPositions.y[*(index+1)],   clippedVerts[0].y[2] = mesh->transformedPositions.y[*(index+2)];
				  clippedVerts[0].z[0] = mesh->transformedPositions.z[*(index)],   clippedVerts[0].z[1] = mesh->transformedPositions.z[*(index+1)],   clippedVerts[0].z[2] = mesh->transformedPositions.z[*(index+2)];
				  clippedVerts[0].w[0] = mesh->transformedPositions.w[*(index)],   clippedVerts[0].w[1] = mesh->transformedPositions.w[*(index+1)],   clippedVerts[0].w[2] = mesh->transformedPositions.w[*(index+2)];
				clippedVertUVs[0].x[0] = mesh->texcoords.x[*(index)],            clippedVertUVs[0].x[1] = mesh->texcoords.x[*(index+1)],            clippedVertUVs[0].x[2] = mesh->texcoords.x[*(index+2)];
				clippedVertUVs[0].y[0] = mesh->texcoords.y[*(index)],            clippedVertUVs[0].y[1] = mesh->texcoords.y[*(index+1)],            clippedVertUVs[0].y[2] = mesh->texcoords.y[*(index+2)];
				index += 3;
#else
				// Slower
				clippedVerts.x[0] = mesh->transformedPositions.x[*(index)];
				clippedVerts.y[0] = mesh->transformedPositions.y[*(index)];
				clippedVerts.z[0] = mesh->transformedPositions.z[*(index)];
				clippedVerts.w[0] = mesh->transformedPositions.w[*(index)];
				index++;
				clippedVerts.x[1] = mesh->transformedPositions.x[*(index)];
				clippedVerts.y[1] = mesh->transformedPositions.y[*(index)];
				clippedVerts.z[1] = mesh->transformedPositions.z[*(index)];
				clippedVerts.w[1] = mesh->transformedPositions.w[*(index)];
				index++;
				clippedVerts.x[2] = mesh->transformedPositions.x[*(index)];
				clippedVerts.y[2] = mesh->transformedPositions.y[*(index)];
				clippedVerts.z[2] = mesh->transformedPositions.z[*(index)];
				clippedVerts.w[2] = mesh->transformedPositions.w[*(index)];
#endif
				clippedVerts[0].vectorCount = 3;
				clippedVertUVs[0].vectorCount = 3;

				//--------------------------------
				// Clip w against near clip plane
				//--------------------------------
				uint32_t vectorCount = 3;
				if ( Debug.flags & FLAG_CLIP_W )
				{
					vectorCount = __softrast_clip ( &clippedVerts[1], &clippedVertUVs[1], &clippedVerts[0], &clippedVertUVs[0], 3, 3, -1.0f, -globalData.nearClip );
					if ( vectorCount < 3 )
						continue;
					clippedVerts[1].vectorCount   = vectorCount;
					clippedVertUVs[1].vectorCount = vectorCount;
				}
				else
				{
					memcpy ( clippedVerts[1].x, clippedVerts[0].x, 3 * sizeof ( float ) );
					memcpy ( clippedVerts[1].y, clippedVerts[0].y, 3 * sizeof ( float ) );
					memcpy ( clippedVerts[1].z, clippedVerts[0].z, 3 * sizeof ( float ) );
					memcpy ( clippedVerts[1].w, clippedVerts[0].w, 3 * sizeof ( float ) );
					memcpy ( clippedVertUVs[1].x, clippedVertUVs[0].x, 3 * sizeof ( float ) );
					memcpy ( clippedVertUVs[1].y, clippedVertUVs[0].y, 3 * sizeof ( float ) );
					clippedVerts[1].vectorCount   = vectorCount;
					clippedVertUVs[1].vectorCount = vectorCount;
				}

				//--------------------------------
				// Take the reciprocal of w, and perspective-correct X, Y, Z, U and V
				//--------------------------------
				bbm_soa_vec4_overwrite_rcp_w ( &clippedVerts[1] );
				bbm_soa_vec4_overwrite_mul_xyz_w ( &clippedVerts[1] );
				bbm_soa_vec2_overwrite_mul_soa_vec4_w ( &clippedVertUVs[1], &clippedVerts[1] );

				//--------------------------------
				// Check winding order
				//--------------------------------
				if ( Debug.flags & FLAG_BACKFACE_CULLING_ENABLED )
				{
					float dx1 = clippedVerts[1].x[1] - clippedVerts[1].x[0];
					float dx2 = clippedVerts[1].x[2] - clippedVerts[1].x[0];
					float dy1 = clippedVerts[1].y[1] - clippedVerts[1].y[0];
					float dy2 = clippedVerts[1].y[2] - clippedVerts[1].y[0];
					float cz  = dx1 * dy2 - dx2 * dy1;
					if ( ((Debug.flags & FLAG_BACKFACE_CULLING_INVERTED) ? 1 : 0) ^ (cz < 0.0f) )
						continue;
				}

				//--------------------------------
				// Clipping
				//--------------------------------
				if ( Debug.flags & FLAG_CLIP_FRUSTUM )
				{
					vectorCount = __softrast_clip ( &clippedVerts[0], &clippedVertUVs[0], &clippedVerts[1], &clippedVertUVs[1], vectorCount, 0, -1.0f, 1.0f - epsilon[0] );
					vectorCount = __softrast_clip ( &clippedVerts[1], &clippedVertUVs[1], &clippedVerts[0], &clippedVertUVs[0], vectorCount, 1, -1.0f, 1.0f - epsilon[1] );
					//vectorCount = __softrast_clip ( &clippedVerts[1], &clippedVertUVs[1], &clippedVerts[0], &clippedVertUVs[0], vectorCount, 2, -1.0f, 1.0f - epsilon[2] );
				
					vectorCount = __softrast_clip ( &clippedVerts[0], &clippedVertUVs[0], &clippedVerts[1], &clippedVertUVs[1], vectorCount, 0,  1.0f, 1.0f - epsilon[0] );
					vectorCount = __softrast_clip ( &clippedVerts[1], &clippedVertUVs[1], &clippedVerts[0], &clippedVertUVs[0], vectorCount, 1,  1.0f, 1.0f - epsilon[1] );
					vectorCount = __softrast_clip ( &clippedVerts[0], &clippedVertUVs[0], &clippedVerts[1], &clippedVertUVs[1], vectorCount, 2,  1.0f, 1.0f - epsilon[2] );

					//--------------------------------
					// Check whether or not enough vertices remain for rasterization
					//--------------------------------
					if ( vectorCount < 3 )
						continue;
					clippedVerts[0].vectorCount = vectorCount;
					clippedVertUVs[0].vectorCount = vectorCount;
				}
				else
				{
					memcpy ( clippedVerts[0].x, clippedVerts[1].x, vectorCount * sizeof ( float ) );
					memcpy ( clippedVerts[0].y, clippedVerts[1].y, vectorCount * sizeof ( float ) );
					memcpy ( clippedVerts[0].z, clippedVerts[1].z, vectorCount * sizeof ( float ) );
					memcpy ( clippedVerts[0].w, clippedVerts[1].w, vectorCount * sizeof ( float ) );
					memcpy ( clippedVertUVs[0].x, clippedVertUVs[1].x, vectorCount * sizeof ( float ) );
					memcpy ( clippedVertUVs[0].y, clippedVertUVs[1].y, vectorCount * sizeof ( float ) );
					clippedVerts[0].vectorCount   = vectorCount;
					clippedVertUVs[0].vectorCount = vectorCount;
				}

				//--------------------------------
				// Sanity checks
				//--------------------------------
#ifndef NDEBUG
				for ( uint32_t cell = 0; cell < 3; cell++ )
				{
					for ( uint32_t i = 0; i < vectorCount; i++ )
					{
						assert ( clippedVerts[0].cells[cell][i] >= -1.0f && clippedVerts[0].cells[cell][i] <= 1.0f );
					}
				}
#endif

				//--------------------------------
				// Warp XY into screen space
				//--------------------------------
				bbm_soa_vec4_overwrite_mad_x ( &clippedVerts[0], bias[0], offset[0] );
				bbm_soa_vec4_overwrite_mad_y ( &clippedVerts[0], bias[1], offset[1] );

				//--------------------------------
				// Fill edges into outline table
				//--------------------------------
				for ( uint32_t i = 0; i < clippedVerts[0].vectorCount; i++ )
				{
					//--------------------------------
					// Determine edge indices
					//--------------------------------
					uint32_t i1 =  i;
					uint32_t i2 = (i + 1) % clippedVerts[0].vectorCount;

					//--------------------------------
					// We want the edge to go from up to down, so swap if inversed
					//--------------------------------
					if ( clippedVerts[0].y[i1] > clippedVerts[0].y[i2] )
					{
						uint32_t t = i1;
						i1 = i2;
						i2 = t;
					}

					//--------------------------------
					// Prepare for the loop
					//--------------------------------
					float y1 = clippedVerts[0].y[i1];
					float y2 = clippedVerts[0].y[i2];
					float x1 = clippedVerts[0].x[i1];
					float x2 = clippedVerts[0].x[i2];
					float z1 = clippedVerts[0].w[i1];	// Taking w instead of z (w = 1.0f / zInViewSpace)
					float z2 = clippedVerts[0].w[i2];	// Taking w instead of z (w = 1.0f / zInViewSpace)
					float u1 = clippedVertUVs[0].x[i1];
					float u2 = clippedVertUVs[0].x[i2];
					float v1 = clippedVertUVs[0].y[i1];
					float v2 = clippedVertUVs[0].y[i2];

					//if ( Debug.flags & FLAG_DERP3 )
					//{
					//	float a = clippedVerts[0].z[i1] * 0.5f + 0.5f;
					//	float b = clippedVerts[0].z[i2] * 0.5f + 0.5f;
					//
					//	z1 = 1.0f / (a * globalData.farClip + (1.0f-a) * globalData.nearClip);
					//	z2 = 1.0f / (b * globalData.farClip + (1.0f-b) * globalData.nearClip);
					//}

					float dy = (y2 - y1);// + (Debug.flags & FLAG_DERP ? 1 : 0);
					float dx = (x2 - x1)/dy;
					float dz = (z2 - z1)/dy;
					float du = (u2 - u1)/dy;
					float dv = (v2 - v1)/dy;

					int32_t iMinY = ((int32_t)y1) + 1;
					int32_t iMaxY = (int32_t)y2;
					if ( iMaxY < iMinY )
						continue;
					assert ( iMinY >= 0 && iMinY < (int32_t)globalData.renderTarget.height );
					assert ( iMaxY >= 0 && iMaxY < (int32_t)globalData.renderTarget.height );

					float topClipY    = iMinY - y1;
					float bottomClipY = y2 - iMaxY;

					//--------------------------------
					// Do sub-pixel correction
					//--------------------------------
					x1 += dx * topClipY;
					x2 -= dx * bottomClipY;

					z1 += dz * topClipY;
					z2 -= dz * bottomClipY;

					u1 += du * topClipY;
					u2 -= du * bottomClipY;
					v1 += dv * topClipY;
					v2 -= dv * bottomClipY;

					//dy = (float)(iMaxY - iMinY);
					//
					//dx = (x2 - x1)/dy;
					//dz = (z2 - z1)/dy;
					//du = (u2 - u1)/dy;
					//dv = (v2 - v1)/dy;

#if 0
					float x = x1;
					float z = z1;
					float u = u1;
					float v = v1;
#endif

					//--------------------------------
					// Calculate min, max submesh Y
					//--------------------------------
					if ( iMinY < minTriY )
						minTriY = iMinY;
					if ( iMaxY > maxTriY )
						maxTriY = iMaxY;

					//--------------------------------
					// Loop over the rows
					//--------------------------------
					float* outlineMinX = globalData.outlineTable.minX + iMinY;
					float* outlineMaxX = globalData.outlineTable.maxX + iMinY;
					float* outlineMinZ = globalData.outlineTable.minZ + iMinY;
					float* outlineMaxZ = globalData.outlineTable.maxZ + iMinY;
					float* outlineMinU = globalData.outlineTable.minU + iMinY;
					float* outlineMaxU = globalData.outlineTable.maxU + iMinY;
					float* outlineMinV = globalData.outlineTable.minV + iMinY;
					float* outlineMaxV = globalData.outlineTable.maxV + iMinY;

#if 0
					for ( int32_t y = iMinY; y <= iMaxY; y++, outlineMinX++, outlineMaxX++, outlineMinZ++, outlineMaxZ++, outlineMinU++, outlineMaxU++, outlineMinV++, outlineMaxV++, x += dx, z += dz, u += du, v += dv )
					{
#else
					int32_t yinc = 0;
					for ( int32_t y = iMinY; y <= iMaxY; y++, yinc++, outlineMinX++, outlineMaxX++, outlineMinZ++, outlineMaxZ++, outlineMinU++, outlineMaxU++, outlineMinV++, outlineMaxV++ )
					{
						float x = x1 + yinc * dx;
						float z = z1 + yinc * dz;
						float u = u1 + yinc * du;
						float v = v1 + yinc * dv;
#endif
						if ( x < *outlineMinX )
						{
#pragma message ( "TODO: Better out-of-bound checks!" )
							//assert ( x > 10.0f );
							*outlineMinX = x;
							*outlineMinZ = z;
							*outlineMinU = u;
							*outlineMinV = v;
						}
						if ( x > *outlineMaxX )
						{
							//assert ( x > 10.0f );
							*outlineMaxX = x;
							*outlineMaxZ = z;
							*outlineMaxU = u;
							*outlineMaxV = v;
						}
					}
				}
#else
				
				{
					vertex* curv = curVerts;
					for ( uint32_t i = 0; i < 3; i++, curv++, index++ )
					{
						curv->position.x = transformedPositions.x[*index], curv->position.y = transformedPositions.y[*index], curv->position.z = transformedPositions.z[*index], curv->position.w = transformedPositions.w[*index];
						curv->u = texcoords.x[*index], curv->v = texcoords.y[*index];
					}
				}

				//--------------------------------
				// Clip w against near clip plane
				//--------------------------------
				uint32_t vectorCount = 3;
				if ( res == AABB_FRUSTUM_INTERSECT && (Debug.flags & FLAG_CLIP_W) )
				{
					vectorCount = __softrast_clip ( tempVerts, curVerts, 3, 3, -1.0f, -globalData.nearClip );
					if ( vectorCount < 3 )
						continue;
					
					vertex* temp = tempVerts;
					tempVerts = curVerts;
					curVerts  = temp;
				}

				//--------------------------------
				// Take the reciprocal of w, and perspective-correct X, Y, Z, U and V
				//--------------------------------
				{
					vertex* curv = curVerts;
					for ( uint32_t i = 0; i < vectorCount; i++, curv++ )
					{
						curv->position.w  = 1.0f / curv->position.w;
						curv->position.x *= curv->position.w, curv->position.y *= curv->position.w, curv->position.z *= curv->position.w;
						curv->u          *= curv->position.w, curv->v          *= curv->position.w;
					}
				}

				//--------------------------------
				// Check winding order
				//--------------------------------
				if ( Debug.flags & FLAG_BACKFACE_CULLING_ENABLED )
				{
					float dx1 = curVerts[1].position.x - curVerts[0].position.x;
					float dx2 = curVerts[2].position.x - curVerts[0].position.x;
					float dy1 = curVerts[1].position.y - curVerts[0].position.y;
					float dy2 = curVerts[2].position.y - curVerts[0].position.y;
					float cz  = dx1 * dy2 - dx2 * dy1;
					if ( ((Debug.flags & FLAG_BACKFACE_CULLING_INVERTED) ? 1 : 0) ^ (cz < 0.0f) )
						continue;
				}

				//--------------------------------
				// Clipping
				//--------------------------------
				if ( res == AABB_FRUSTUM_INTERSECT && Debug.flags & FLAG_CLIP_FRUSTUM )
				{
					vectorCount = __softrast_clip ( tempVerts, curVerts, vectorCount, 0, -1.0f, 1.0f - epsilon[0] );
					vectorCount = __softrast_clip ( curVerts, tempVerts, vectorCount, 1, -1.0f, 1.0f - epsilon[1] );
					
					vectorCount = __softrast_clip ( tempVerts, curVerts, vectorCount, 0, 1.0f, 1.0f - epsilon[0] );
					vectorCount = __softrast_clip ( curVerts, tempVerts, vectorCount, 1, 1.0f, 1.0f - epsilon[1] );
					vectorCount = __softrast_clip ( tempVerts, curVerts, vectorCount, 2, 1.0f, 1.0f - epsilon[2] );

					//--------------------------------
					// Check whether or not enough vertices remain for rasterization
					//--------------------------------
					if ( vectorCount < 3 )
						continue;

					vertex* temp = tempVerts;
					tempVerts = curVerts;
					curVerts  = temp;
				}

				assert ( vectorCount <= sizeof ( clippedVerts[0] ) / sizeof ( clippedVerts[0][0] ) );

				//--------------------------------
				// Sanity checks
				//--------------------------------
//#ifndef NDEBUG
//				for ( uint32_t cell = 0; cell < 3; cell++ )
//				{
//					for ( uint32_t i = 0; i < vectorCount; i++ )
//					{
//						assert ( clippedVerts[0].cells[cell][i] >= -1.0f && clippedVerts[0].cells[cell][i] <= 1.0f );
//					}
//				}
//#endif

				//--------------------------------
				// Warp XY into screen space
				//--------------------------------
				{
					vertex* curv = curVerts;
					for ( uint32_t i = 0; i < vectorCount; i++, curv++ )
					{
						curv->position.x  = curv->position.x * bias[0] + offset[0], curv->position.y = curv->position.y * bias[1] + offset[1];
					}
				}

				//--------------------------------
				// Fill edges into outline table
				//--------------------------------
				uint32_t li = vectorCount-1;
				for ( uint32_t i = 0; i < vectorCount; li = i++ )
				{
					//--------------------------------
					// Determine edge indices
					//--------------------------------
					uint32_t i1 = li;
					uint32_t i2 = i;

					//--------------------------------
					// We want the edge to go from up to down, so swap if inversed
					//--------------------------------
					if ( curVerts[i1].position.y > curVerts[i2].position.y )
					{
						uint32_t t = i1;
						i1 = i2;
						i2 = t;
					}

					//--------------------------------
					// Prepare for the loop
					//--------------------------------
					const vertex* vert1 = curVerts + i1;
					const vertex* vert2 = curVerts + i2;

					float x1 = vert1->position.x;
					float y1 = vert1->position.y;
					float z1 = vert1->position.w;	// Taking w instead of z (w = 1.0f / zInViewSpace)
					float u1 = vert1->u;
					float v1 = vert1->v;

					float x2 = vert2->position.x;
					float y2 = vert2->position.y;
					float z2 = vert2->position.w;	// Taking w instead of z (w = 1.0f / zInViewSpace)
					float u2 = vert2->u;
					float v2 = vert2->v;

					float dy = (y2 - y1);// + (Debug.flags & FLAG_DERP ? 1 : 0);
					float dx = (x2 - x1)/dy;
					float dz = (z2 - z1)/dy;
					float du = (u2 - u1)/dy;
					float dv = (v2 - v1)/dy;

					int32_t iMinY = ((int32_t)y1) + 1;
					int32_t iMaxY = (int32_t)y2;
					if ( iMaxY < iMinY )
						continue;
					assert ( iMinY >= 0 && iMinY < (int32_t)globalData.renderTarget.height );
					assert ( iMaxY >= 0 && iMaxY < (int32_t)globalData.renderTarget.height );

					float topClipY    = iMinY - y1;
					float bottomClipY = y2 - iMaxY;

					//--------------------------------
					// Do sub-pixel correction
					//--------------------------------
					x1 += dx * topClipY;
					x2 -= dx * bottomClipY;

					z1 += dz * topClipY;
					z2 -= dz * bottomClipY;

					u1 += du * topClipY;
					u2 -= du * bottomClipY;
					v1 += dv * topClipY;
					v2 -= dv * bottomClipY;

					//dy = (float)(iMaxY - iMinY);
					//
					//dx = (x2 - x1)/dy;
					//dz = (z2 - z1)/dy;
					//du = (u2 - u1)/dy;
					//dv = (v2 - v1)/dy;

#if 0
					float x = x1;
					float z = z1;
					float u = u1;
					float v = v1;
#endif

					//--------------------------------
					// Calculate min, max submesh Y
					//--------------------------------
					if ( iMinY < minTriY )
						minTriY = iMinY;
					if ( iMaxY > maxTriY )
						maxTriY = iMaxY;

					//--------------------------------
					// Loop over the rows
					//--------------------------------
#if AOS_OUTLINE_TABLE
					outline_table_entry* outline = globalData.outlineTable + iMinY;

#if 0
					for ( int32_t y = iMinY; y <= iMaxY; y++, outlineMinX++, outlineMaxX++, outlineMinZ++, outlineMaxZ++, outlineMinU++, outlineMaxU++, outlineMinV++, outlineMaxV++, x += dx, z += dz, u += du, v += dv )
					{
#else
					int32_t yinc = 0;
					for ( int32_t y = iMinY; y <= iMaxY; y++, yinc++, outline++ )
					{
						float x = x1 + yinc * dx;
						float z = z1 + yinc * dz;
						float u = u1 + yinc * du;
						float v = v1 + yinc * dv;
#endif
						if ( x < outline->minX || !(outline->flags & 0x1) )
						{
#pragma message ( "TODO: Better out-of-bound checks!" )
							//assert ( x > 10.0f );
							outline->minX = x;
							outline->minZ = z;
							outline->minU = u;
							outline->minV = v;
						}
						if ( x > outline->maxX || !(outline->flags & 0x1) )
						{
							//assert ( x > 10.0f );
							outline->maxX = x;
							outline->maxZ = z;
							outline->maxU = u;
							outline->maxV = v;
						}
						outline->flags = 0x1;
					}

					outline_table_entry* preEntry = globalData.outlineTable + (iMinY - 1);
					outline_table_entry* postEntry = outline;//globalData.outlineTable + (iMaxY + 1);

					float preX  = x1 - 1 * dx;
					float postX = x1 + (iMaxY - iMinY + 1) * dx;

					if ( !(preEntry->flags & 0x1) && preX < preEntry->minX )
					{
						preEntry->minX = x1 - 1 * dx;
						preEntry->minZ = z1 - 1 * dz;
						preEntry->minU = u1 - 1 * du;
						preEntry->minV = v1 - 1 * dv;
					}
					if ( !(preEntry->flags & 0x1) && preX > preEntry->maxX )
					{
						preEntry->maxX = x1 + (iMaxY - iMinY + 1) * dx;
						preEntry->maxZ = z1 + (iMaxY - iMinY + 1) * dz;
						preEntry->maxU = u1 + (iMaxY - iMinY + 1) * du;
						preEntry->maxV = v1 + (iMaxY - iMinY + 1) * dv;
					}
					if ( !(postEntry->flags & 0x1) && postX < postEntry->minX )
					{
						postEntry->minX = x1 - 1 * dx;
						postEntry->minZ = z1 - 1 * dz;
						postEntry->minU = u1 - 1 * du;
						postEntry->minV = v1 - 1 * dv;
					}
					if ( !(postEntry->flags & 0x1) && postX > postEntry->maxX )
					{
						postEntry->maxX = x1 + (iMaxY - iMinY + 1) * dx;
						postEntry->maxZ = z1 + (iMaxY - iMinY + 1) * dz;
						postEntry->maxU = u1 + (iMaxY - iMinY + 1) * du;
						postEntry->maxV = v1 + (iMaxY - iMinY + 1) * dv;
					}
#else
					float* outlineMinX = globalData.outlineTable.minX + iMinY;
					float* outlineMaxX = globalData.outlineTable.maxX + iMinY;
					float* outlineMinZ = globalData.outlineTable.minZ + iMinY;
					float* outlineMaxZ = globalData.outlineTable.maxZ + iMinY;
					float* outlineMinU = globalData.outlineTable.minU + iMinY;
					float* outlineMaxU = globalData.outlineTable.maxU + iMinY;
					float* outlineMinV = globalData.outlineTable.minV + iMinY;
					float* outlineMaxV = globalData.outlineTable.maxV + iMinY;

#if 0
					for ( int32_t y = iMinY; y <= iMaxY; y++, outlineMinX++, outlineMaxX++, outlineMinZ++, outlineMaxZ++, outlineMinU++, outlineMaxU++, outlineMinV++, outlineMaxV++, x += dx, z += dz, u += du, v += dv )
					{
#else
					int32_t yinc = 0;
					for ( int32_t y = iMinY; y <= iMaxY; y++, yinc++, outlineMinX++, outlineMaxX++, outlineMinZ++, outlineMaxZ++, outlineMinU++, outlineMaxU++, outlineMinV++, outlineMaxV++ )
					{
						float x = x1 + yinc * dx;
						float z = z1 + yinc * dz;
						float u = u1 + yinc * du;
						float v = v1 + yinc * dv;
#endif
						if ( x < *outlineMinX )
						{
#pragma message ( "TODO: Better out-of-bound checks!" )
							//assert ( x > 10.0f );
							*outlineMinX = x;
							*outlineMinZ = z;
							*outlineMinU = u;
							*outlineMinV = v;
						}
						if ( x > *outlineMaxX )
						{
							//assert ( x > 10.0f );
							*outlineMaxX = x;
							*outlineMaxZ = z;
							*outlineMaxU = u;
							*outlineMaxV = v;
						}
					}
#endif
				}
#endif

				if ( (Debug.flags & FLAG_RASTERIZE) && (Debug.flags & FLAG_ENABLE_QUAD_RASTERIZATION) )
#pragma region Quad rasterization
				{
#if 1
					//--------------------------------
					// Fill surrounding outlines
					//--------------------------------
#if AOS_OUTLINE_TABLE
					globalData.outlineTable[minTriY-1] = globalData.outlineTable[minTriY];
					globalData.outlineTable[maxTriY+1] = globalData.outlineTable[maxTriY];

					globalData.outlineTable[minTriY-1].minX = (float)globalData.renderTarget.width, globalData.outlineTable[minTriY-1].maxX = 0;
					globalData.outlineTable[maxTriY+1].minX = (float)globalData.renderTarget.width, globalData.outlineTable[maxTriY+1].maxX = 0;
#else
					*(globalData.outlineTable.minZ + (minTriY-1)) = *(globalData.outlineTable.minZ + minTriY);
					*(globalData.outlineTable.maxZ + (minTriY-1)) = *(globalData.outlineTable.maxZ + minTriY);
					*(globalData.outlineTable.minU + (minTriY-1)) = *(globalData.outlineTable.minU + minTriY);
					*(globalData.outlineTable.maxU + (minTriY-1)) = *(globalData.outlineTable.maxU + minTriY);
					*(globalData.outlineTable.minV + (minTriY-1)) = *(globalData.outlineTable.minV + minTriY);
					*(globalData.outlineTable.maxV + (minTriY-1)) = *(globalData.outlineTable.maxV + minTriY);

					*(globalData.outlineTable.minZ + (maxTriY+1)) = *(globalData.outlineTable.minZ + maxTriY);
					*(globalData.outlineTable.maxZ + (maxTriY+1)) = *(globalData.outlineTable.maxZ + maxTriY);
					*(globalData.outlineTable.minU + (maxTriY+1)) = *(globalData.outlineTable.minU + maxTriY);
					*(globalData.outlineTable.maxU + (maxTriY+1)) = *(globalData.outlineTable.maxU + maxTriY);
					*(globalData.outlineTable.minV + (maxTriY+1)) = *(globalData.outlineTable.minV + maxTriY);
					*(globalData.outlineTable.maxV + (maxTriY+1)) = *(globalData.outlineTable.maxV + maxTriY);
#endif
#endif

					//--------------------------------
					// Rasterize time
					//--------------------------------
					const int32_t minBlockY = minTriY & (~(1));
					const int32_t maxBlockY = maxTriY & (~(1));

#pragma message ( "TODO: Support non-textured meshes pls!" )
					if ( submesh->texture )
					{
						int32_t y1 = minBlockY;
						int32_t y2 = minBlockY + 1;

#if AOS_OUTLINE_TABLE
						outline_table_entry* outline[2] = { globalData.outlineTable + y1, globalData.outlineTable + y2 };
#else
						float* outlineMinX[2] = { globalData.outlineTable.minX + y1, globalData.outlineTable.minX + y2 };
						float* outlineMaxX[2] = { globalData.outlineTable.maxX + y1, globalData.outlineTable.maxX + y2 };
						float* outlineMinZ[2] = { globalData.outlineTable.minZ + y1, globalData.outlineTable.minZ + y2 };
						float* outlineMaxZ[2] = { globalData.outlineTable.maxZ + y1, globalData.outlineTable.maxZ + y2 };
						float* outlineMinU[2] = { globalData.outlineTable.minU + y1, globalData.outlineTable.minU + y2 };
						float* outlineMaxU[2] = { globalData.outlineTable.maxU + y1, globalData.outlineTable.maxU + y2 };
						float* outlineMinV[2] = { globalData.outlineTable.minV + y1, globalData.outlineTable.minV + y2 };
						float* outlineMaxV[2] = { globalData.outlineTable.maxV + y1, globalData.outlineTable.maxV + y2 };
#endif

						uint32_t* colorRowPtr[2] = {
							(uint32_t*)((uintptr_t)globalData.renderTarget.colorBuffer + (globalData.renderTarget.height - y1 - 1) * globalData.renderTarget.pitch),
							(uint32_t*)((uintptr_t)globalData.renderTarget.colorBuffer + (globalData.renderTarget.height - y2 - 1) * globalData.renderTarget.pitch)
						};
						float* depthRowPtr[2]    = {
							globalData.renderTarget.depthBuffer + y1 * globalData.renderTarget.width,
							globalData.renderTarget.depthBuffer + y2 * globalData.renderTarget.width
						};

						for ( ; y1 <= maxBlockY; y1 +=2, y2 += 2,
												colorRowPtr[0] = (uint32_t*)((uintptr_t)colorRowPtr[0] - 2 * globalData.renderTarget.pitch),
												colorRowPtr[1] = (uint32_t*)((uintptr_t)colorRowPtr[1] - 2 * globalData.renderTarget.pitch),
												depthRowPtr[0] += 2 * globalData.renderTarget.width, depthRowPtr[1] += 2 * globalData.renderTarget.width,
#if AOS_OUTLINE_TABLE
												outline[0] += 2, outline[1] += 2 )
#else
												outlineMinX[0] += 2, outlineMinX[1] += 2, outlineMaxX[0] += 2, outlineMaxX[1] += 2,
												outlineMinZ[0] += 2, outlineMinZ[1] += 2, outlineMaxZ[0] += 2, outlineMaxZ[1] += 2,
												outlineMinU[0] += 2, outlineMinU[1] += 2, outlineMaxU[0] += 2, outlineMaxU[1] += 2,
												outlineMinV[0] += 2, outlineMinV[1] += 2, outlineMaxV[0] += 2, outlineMaxV[1] += 2 )
#endif
						{
							//--------------------------------
							// Prepare useful constant data
							//--------------------------------
#if AOS_OUTLINE_TABLE
							const float x1[2] = { outline[0]->minX, outline[1]->minX };
							const float x2[2] = { outline[0]->maxX, outline[1]->maxX };

							const int32_t ix1[2] = { (int32_t)x1[0], (int32_t)x1[1] };
							const int32_t ix2[2] = { (int32_t)x2[0], (int32_t)x2[1] };

							float z1[2] = { outline[0]->minZ, outline[1]->minZ };
							float u1[2] = { outline[0]->minU, outline[1]->minU };
							float v1[2] = { outline[0]->minV, outline[1]->minV };

							float z2[2] = { outline[0]->maxZ, outline[1]->maxZ };
							float u2[2] = { outline[0]->maxU, outline[1]->maxU };
							float v2[2] = { outline[0]->maxV, outline[1]->maxV };
#else
							const float x1[2] = { *outlineMinX[0], *outlineMinX[1] };
							const float x2[2] = { *outlineMaxX[0], *outlineMaxX[1] };

							const int32_t ix1[2] = { (int32_t)x1[0], (int32_t)x1[1] };
							const int32_t ix2[2] = { (int32_t)x2[0], (int32_t)x2[1] };

							float z1[2] = { *outlineMinZ[0], *outlineMinZ[1] };
							float u1[2] = { *outlineMinU[0], *outlineMinU[1] };
							float v1[2] = { *outlineMinV[0], *outlineMinV[1] };

							const float z2[2] = { *outlineMaxZ[0], *outlineMaxZ[1] };
							const float u2[2] = { *outlineMaxU[0], *outlineMaxU[1] };
							const float v2[2] = { *outlineMaxV[0], *outlineMaxV[1] };
#endif

							const int32_t iMinX = (ix1[0] < ix1[1] ? ix1[0] : ix1[1]) & (~1);
							const int32_t iMaxX = (ix2[0] > ix2[1] ? ix2[0] : ix2[1]) & (~1);

							//--------------------------------
							// Calculate deltas and steps
							//--------------------------------
							const float dx[2] = { x2[0] - x1[0], x2[1] - x1[1] };
						
							const float zstep[2] = { (z2[0] - z1[0]) / dx[0], (z2[1] - z1[1]) / dx[1] };
							const float ustep[2] = { (u2[0] - u1[0]) / dx[0], (u2[1] - u1[1]) / dx[1] };
							const float vstep[2] = { (v2[0] - v1[0]) / dx[0], (v2[1] - v1[1]) / dx[1] };

							//--------------------------------
							// Prepare useful mutable data
							//--------------------------------
							uint32_t* ptr[2][2] = {
								{ colorRowPtr[0] + iMinX, colorRowPtr[0] + iMinX + 1 },
								{ colorRowPtr[1] + iMinX, colorRowPtr[1] + iMinX + 1 },
							};
							float* dptr[2][2] = {
								{ depthRowPtr[0] + iMinX, depthRowPtr[0] + iMinX + 1 },
								{ depthRowPtr[1] + iMinX, depthRowPtr[1] + iMinX + 1 },
							};
							int32_t spanCorrection[2] = { iMinX - ix1[0], iMinX - ix1[1] };
							z1[0] = z1[0] + zstep[0] * spanCorrection[0], z1[1] = z1[1] + zstep[1] * spanCorrection[1];
							u1[0] = u1[0] + ustep[0] * spanCorrection[0], u1[1] = u1[1] + ustep[1] * spanCorrection[1];
							v1[0] = v1[0] + vstep[0] * spanCorrection[0], v1[1] = v1[1] + vstep[1] * spanCorrection[1];
							
#if 0
							float z[2] = { z1[0], z1[1] };
							float u[2] = { u1[0], u1[1] };
							float v[2] = { v1[0], v1[1] };
							
							const float zstep2[2] = { 2.0f * zstep[0], 2.0f * zstep[1] };
							const float ustep2[2] = { 2.0f * ustep[0], 2.0f * ustep[1] };
							const float vstep2[2] = { 2.0f * vstep[0], 2.0f * vstep[1] };

							//--------------------------------
							// Time to fill some spans
							//--------------------------------
							for ( int32_t ix = iMinX; ix <= iMaxX; ix+=2,	ptr[0][0] += 2, ptr[0][1] += 2, ptr[1][0] += 2, ptr[1][1] += 2,
																			dptr[0][0] += 2, dptr[0][1] += 2, dptr[1][0] += 2, dptr[1][1] += 2,
																			z[0] += zstep2[0], z[1] += zstep2[1],
																			u[0] += ustep2[0], u[1] += ustep2[1],
																			v[0] += vstep2[0], v[1] += vstep2[1] )
							{
#else
							//--------------------------------
							// Time to fill some spans
							//--------------------------------
							int32_t xinc = 0;
							for ( int32_t ix = iMinX; ix <= iMaxX; ix+=2, xinc+=2,	ptr[0][0] += 2, ptr[0][1] += 2, ptr[1][0] += 2, ptr[1][1] += 2,
																			dptr[0][0] += 2, dptr[0][1] += 2, dptr[1][0] += 2, dptr[1][1] += 2 )
							{
								float z[2] = { z1[0] + xinc * zstep[0], z1[1] + xinc * zstep[1] };
								float u[2] = { u1[0] + xinc * ustep[0], u1[1] + xinc * ustep[1] };
								float v[2] = { v1[0] + xinc * vstep[0], v1[1] + xinc * vstep[1] };
#endif
#define MIN(x,y) (((x) < (y)) ? (x) : (y))
#define MAX(x,y) (((x) > (y)) ? (x) : (y))
#define CLAMP(x,min,max) (MIN((max),MAX((min),(x))))

								// Take dx, dy of U and V
								//	-> dx[0] = du[0], dx[1] = du[1]
								//  -> dy[0] = u[1] - u[0], u[1] - u[0] - (ustep[1] - ustep[0])
								// [per pixel] Take max of dx and dy, and find the mipmap that would make that max <= 1

								//--------------------------------
								// Calculate reciprocal Z for each pixel in the block (actually reciprocal of reciprocal of z, being z, but I digress)
								//--------------------------------
								const float rz[2][2] = {
									{ 1.0f / z[0], 1.0f/(z[0] + zstep[0]) },
									{ 1.0f / z[1], 1.0f/(z[1] + zstep[1]) },
								};

								//--------------------------------
								// Calculate UV for each pixel in the block
								//--------------------------------
								float pxu[2][2] = {
									{ u[0] * rz[0][0], (u[0] + ustep[0]) * rz[0][1] },
									{ u[1] * rz[1][0], (u[1] + ustep[1]) * rz[1][1] },
								};

								float pxv[2][2] = {
									{ v[0] * rz[0][0], (v[0] + vstep[0]) * rz[0][1] },
									{ v[1] * rz[1][0], (v[1] + vstep[1]) * rz[1][1] },
								};

#define SINGLE_DESIRED_MIP 1
#if SINGLE_DESIRED_MIP
								//--------------------------------
								// Determine mipmap data
								//--------------------------------
								uint32_t desiredMip, desiredMip2;
								uint32_t mipWidth;
								uint32_t shiftScale;
								float mipT;
								float uvScale;

								
								if ( Debug.textureMipmapMode != TEXTURE_MIPMAP_NONE )
								{
									//--------------------------------
									// Calculate UV deltas
									//--------------------------------
									float dux[2] = {
										submesh->texture->width * fabsf ( pxu[0][0] - pxu[0][1] ), // top left -> top right
										submesh->texture->width * fabsf ( pxu[1][0] - pxu[1][1] ), // bottom left -> bottom right
									};
									float dvx[2] = {
										submesh->texture->width * fabsf ( pxv[0][0] - pxv[0][1] ), // top left -> top right
										submesh->texture->width * fabsf ( pxv[1][0] - pxv[1][1] ), // bottom left -> bottom right
									};
									float duy[2] = {
										submesh->texture->width * fabsf ( pxu[0][0] - pxu[1][0] ), // top left -> bottom left
										submesh->texture->width * fabsf ( pxu[0][1] - pxu[1][1] ), // top right -> bottom right
									};
									float dvy[2] = {
										submesh->texture->width * fabsf ( pxv[0][0] - pxv[1][0] ), // top left -> bottom left
										submesh->texture->width * fabsf ( pxv[0][1] - pxv[1][1] ), // top right -> bottom right
									};

									////--------------------------------
									//// Fix delta UV involving dead pixels to prevent artifacts
									////--------------------------------
									//for ( int32_t r = 0; r < 2; r++ )
									//{
									//	int32_t px = ix;
									//	for ( int32_t c = 0; c < 2; c++, px++ )
									//	{
									//		//if ( !(outline[r]->flags & 0x1) /*|| y1 < minTriY || y2 > maxTriY*/ /*|| px < ix1[r] || px > ix2[r]*/ )
									//		{
									//			// Ded
									//			dux[r] = 1.0f;
									//			dvx[r] = 1.0f;
									//
									//			duy[c] = 1.0f;
									//			dvy[c] = 1.0f;
									//		}
									//	}
									//}

									//--------------------------------
									// Calculate mip data
									//--------------------------------
									const float maxdux = MAX ( dux[0], dux[1] ), maxduy = MAX ( duy[0], duy[1] );
									const float maxdvx = MAX ( dvx[0], dvx[1] ), maxdvy = MAX ( dvy[0], dvy[1] );

									const float maxdu       = MAX ( maxdux, maxduy ), maxdv  = MAX ( maxdvx, maxdvy );
									const float blockMaxDUV = MAX ( maxdu, maxdv );

									const float scaledDUV = Debug.lodBias + Debug.lodScale * blockMaxDUV;
									const float scale     = MAX ( 1.0f, scaledDUV );

									const float logScale = log2f ( scale );

									desiredMip  = (uint32_t)logScale;// (uint32_t)CLAMP( desiredMipUnclamped, 0, (int32_t)submesh->texture->mipLevels-1);
									mipT        = logScale - desiredMip;
									desiredMip  = MIN ( desiredMip, submesh->texture->mipLevels-1 );
									desiredMip2 = MIN ( desiredMip+1, submesh->texture->mipLevels-1 );
									shiftScale  = desiredMip2 - desiredMip;
									mipWidth    = (submesh->texture->width >> desiredMip);
									uvScale     = 1.0f / (1<<desiredMip);
								}
								else
								{
									desiredMip = 0;
									mipWidth   = submesh->texture->width;
									uvScale    = 1.0f;
								}
#else
								//--------------------------------
								// Determine mipmap data
								//--------------------------------
								uint32_t desiredMip[2][2] = { { 0, 0 }, { 0, 0 } };
								uint32_t mipWidth[2][2]   = { { submesh->texture->width, submesh->texture->width }, { submesh->texture->width, submesh->texture->width } };
								float uvScale[2][2]       = { { 1.0f, 1.0f }, { 1.0f, 1.0f } };
								
								if ( Debug.textureMipmapMode == TEXTURE_MIPMAP_POINT )
								{
									//--------------------------------
									// Calculate UV deltas
									//--------------------------------
									float dux[2] = {
										submesh->texture->width * fabsf ( pxu[0][0] - pxu[0][1] ), // top left -> top right
										submesh->texture->width * fabsf ( pxu[1][0] - pxu[1][1] ), // bottom left -> bottom right
									};
									float dvx[2] = {
										submesh->texture->width * fabsf ( pxv[0][0] - pxv[0][1] ), // top left -> top right
										submesh->texture->width * fabsf ( pxv[1][0] - pxv[1][1] ), // bottom left -> bottom right
									};
									float duy[2] = {
										submesh->texture->width * fabsf ( pxu[0][0] - pxu[1][0] ), // top left -> bottom left
										submesh->texture->width * fabsf ( pxu[0][1] - pxu[1][1] ), // top right -> bottom right
									};
									float dvy[2] = {
										submesh->texture->width * fabsf ( pxv[0][0] - pxv[1][0] ), // top left -> bottom left
										submesh->texture->width * fabsf ( pxv[0][1] - pxv[1][1] ), // top right -> bottom right
									};

									//--------------------------------
									// Fix delta UV involving dead pixels to prevent artifacts
									//--------------------------------
									for ( int32_t r = 0; r < 2; r++ )
									{
										int32_t px = ix;
										for ( int32_t c = 0; c < 2; c++, px++ )
										{
											if ( y1 < minTriY || y2 > maxTriY /*|| px < ix1[r] || px > ix2[r]*/ )
											{
												// Ded
												dux[r] = 0.01f;
												dvx[r] = 0.01f;
									
												duy[c] = 0.01f;
												dvy[c] = 0.01f;
											}
										}
									}

									//--------------------------------
									// Calculate UV deltas
									//--------------------------------
									float pxdu[2][2] = {
										{ MAX ( dux[0], duy[0] ), MAX ( dux[0], duy[1] ) },
										{ MAX ( dux[1], duy[0] ), MAX ( dux[1], duy[1] ) },
									};
									float pxdv[2][2] = {
										{ MAX ( dvx[0], dvy[0] ), MAX ( dvx[0], dvy[1] ) },
										{ MAX ( dvx[1], dvy[0] ), MAX ( dvx[1], dvy[1] ) },
									};
									float pxduv[2][2] = {
										{ MAX ( pxdu[0][0], pxdv[0][0] ), MAX ( pxdu[0][1], pxdv[0][1] ) },
										{ MAX ( pxdu[1][0], pxdv[1][0] ), MAX ( pxdu[1][1], pxdv[1][1] ) },
									};
									
									int32_t desiredMipUnclamped[2][2] = {
										{ (int32_t)log2f ( ceilf ( Debug.lodBias + Debug.lodScale * pxduv[0][0] ) ), (int32_t)log2f ( ceilf ( Debug.lodBias + Debug.lodScale * pxduv[0][1] ) ) },
										{ (int32_t)log2f ( ceilf ( Debug.lodBias + Debug.lodScale * pxduv[1][0] ) ), (int32_t)log2f ( ceilf ( Debug.lodBias + Debug.lodScale * pxduv[1][1] ) ) },
									};
									desiredMip[0][0] = (uint32_t)CLAMP ( desiredMipUnclamped[0][0], 0, (int32_t)submesh->texture->mipLevels-1 ), desiredMip[0][1] = (uint32_t)CLAMP ( desiredMipUnclamped[0][1], 0, (int32_t)submesh->texture->mipLevels-1 );
									desiredMip[1][0] = (uint32_t)CLAMP ( desiredMipUnclamped[1][0], 0, (int32_t)submesh->texture->mipLevels-1 ), desiredMip[1][1] = (uint32_t)CLAMP ( desiredMipUnclamped[1][1], 0, (int32_t)submesh->texture->mipLevels-1 );

									mipWidth[0][0] = (submesh->texture->width >> desiredMip[0][0]), mipWidth[0][1] = (submesh->texture->width >> desiredMip[0][1]);
									mipWidth[1][0] = (submesh->texture->width >> desiredMip[1][0]), mipWidth[1][1] = (submesh->texture->width >> desiredMip[1][1]);

									uvScale[0][0] = 1.0f / (1<<desiredMip[0][0]), uvScale[0][1] = 1.0f / (1<<desiredMip[0][1]);
									uvScale[1][0] = 1.0f / (1<<desiredMip[1][0]), uvScale[1][1] = 1.0f / (1<<desiredMip[1][1]);

									////--------------------------------
									//// Calculate mip data
									////--------------------------------
									//const float maxdux = MAX ( dux[0], dux[1] ), maxduy = MAX ( duy[0], duy[1] );
									//const float maxdvx = MAX ( dvx[0], dvx[1] ), maxdvy = MAX ( dvy[0], dvy[1] );
									//
									//const float maxdu       = MAX ( maxdux, maxduy ), maxdv  = MAX ( maxdvx, maxdvy );
									//const float blockMaxDUV = MAX ( maxdu, maxdv );
									//
									//const int32_t desiredMipUnclamped  = (int32_t)log2f ( ceilf ( Debug.lodBias + Debug.lodScale * blockMaxDUV ) );
									//
									//desiredMip = (uint32_t)CLAMP( desiredMipUnclamped, 0, (int32_t)submesh->texture->mipLevels-1);
									//mipWidth   = (submesh->texture->width >> desiredMip);
									//uvScale    = 1.0f / (1<<desiredMip);
								}
#endif

#if !SINGLE_DESIRED_MIP
	#define uvScale uvScale[r][c]
	#define desiredMip desiredMip[r][c]
	#define mipWidth mipWidth[r][c]
#endif

								//--------------------------------
								// Wrap UV and transform to pixel units
								//--------------------------------
								for ( int32_t r = 0; r < 2; r++ )
								{
									for ( int32_t c = 0; c < 2; c++ )
									{
										pxu[r][c] = (pxu[r][c] - (int32_t)pxu[r][c]) * submesh->texture->width;
										pxv[r][c] = (pxv[r][c] - (int32_t)pxv[r][c]) * submesh->texture->width;
									}
								}

								//--------------------------------
								// Apply dithering ( http://www.flipcode.com/archives/Texturing_As_In_Unreal.shtml )
								//--------------------------------
								if ( Debug.flags & FLAG_TEXTURE_DITHERING )
								{
									float ditherLookup[2][2][2] = {
										{ { 0.25f, 0.0f }, { 0.5f, 0.75f } },
										{ { 0.75f, 0.5f }, { 0.0f, 0.25f } },
									};

									for ( int32_t r = 0; r < 2; r++ )
									{
										for ( int32_t c = 0; c < 2; c++ )
										{
											pxu[r][c] += ditherLookup[(y1 + r) & 1][(ix + c) & 1][0];
											pxv[r][c] += ditherLookup[(y1 + r) & 1][(ix + c) & 1][1];
										}
									}
								}

								//--------------------------------
								// Plot pixels
								//--------------------------------
								if ( Debug.flags & FLAG_QUAD_RASTERIZATION_SIMD )
								{
									const uint32_t blockIDX = (uint32_t)(ix) >> 1;
									const uint32_t blockIDY = (uint32_t)(y1) >> 1;

									float* dbquadptr = globalData.renderTarget.depthBuffer + blockIDY * globalData.renderTarget.depthBufferQuadFloatStride + 4 * blockIDX;

									const __m128 fi4 = _mm_set_ps ( 3, 2, 1, 0 );
									const __m128i ii4 = _mm_set_epi32 ( 3, 2, 1, 0 );
									(void)fi4, (void)ii4;

									//const __m128i sc4   = _mm_set_epi32 ( *ptr[1][1], *ptr[1][0], *ptr[0][1], *ptr[0][0] );
									const __m128  d4    = _mm_load_ps ( dbquadptr );//_mm_set_ps ( *dptr[1][1], *dptr[1][0], *dptr[0][1], *dptr[0][0] );
									const __m128  z4    = _mm_set_ps ( z[1] + zstep[1], z[1], z[0] + zstep[0], z[0] );
									const __m128i x4    = _mm_set_epi32 ( ix + 1, ix, ix + 1, ix );
									const __m128i minx4 = _mm_set_epi32 ( ix1[1] - 1, ix1[1] - 1, ix1[0] - 1, ix1[0] - 1 );
									const __m128i maxx4 = _mm_set_epi32 ( ix2[1] + 1, ix2[1] + 1, ix2[0] + 1, ix2[0] + 1 );

									const __m128  depthTestMask  = _mm_cmpgt_ps ( z4, d4 );
									const __m128i depthTestMaski = *(__m128i*)&depthTestMask;
									const __m128i pixelMaski     = _mm_and_si128 ( _mm_and_si128 ( _mm_cmpgt_epi32 ( x4, minx4 ), _mm_cmplt_epi32 ( x4, maxx4 ) ), depthTestMaski ); // if ( px >= ix1[r] && px <= ix2[r] && pz > *dptr[r][c] )
									const __m128  pixelMask      = *(__m128*)&pixelMaski;

									const __m128 do4 = _mm_or_ps ( _mm_and_ps ( pixelMask, z4 ), _mm_andnot_ps ( pixelMask, d4 ) );
									_mm_store_ps ( dbquadptr, do4 );

									if ( Debug.renderMode == RENDER_MODE_FLAT_COLOR )
									{
										//const __m128i dc4 = _mm_or_si128 ( _mm_and_si128 ( pixelMaski, _mm_set1_epi32 ( 0xFFFF0000 ) ), _mm_andnot_si128 ( pixelMaski, sc4 ) );

										union
										{
											__m128 f;
											__m128i i;
										} a;
										union
										{
											__m128 f;
											__m128i i;
										} b;
										a.f = _mm_shuffle_ps ( pixelMask, _mm_set1_ps ( 0 ), _MM_SHUFFLE ( 3, 2, 1, 0 ) );
										b.f = _mm_shuffle_ps ( pixelMask, _mm_set1_ps ( 0 ), _MM_SHUFFLE ( 3, 2, 3, 2 ) );

										_mm_maskmoveu_si128 ( _mm_set1_epi32 ( 0xFFFF0000 ), a.i, (char*)ptr[0][0] );
										_mm_maskmoveu_si128 ( _mm_set1_epi32 ( 0xFFFF0000 ), b.i, (char*)ptr[1][0] );

										//// Repeat this for breakpoint purposes =D
										//*dptr[0][0] = do4.m128_f32[0], *dptr[0][1] = do4.m128_f32[1], *dptr[1][0] = do4.m128_f32[2], *dptr[1][1] = do4.m128_f32[3];
										//
										//(void)a;
										//*ptr[0][0] = dc4.m128i_u32[0], *ptr[0][1] = dc4.m128i_u32[1], *ptr[1][0] = dc4.m128i_u32[2], *ptr[1][1] = dc4.m128i_u32[3];
									}
									//else if ( Debug.renderMode == RENDER_MODE_UV )
									//else if ( Debug.renderMode == RENDER_MODE_ZBUFFER )

									//for ( uint32_t r = 0; r < 2; r++ )
									//		for ( uint32_t c = 0; c < 2; c++ )
									//			*ptr[r][c] = (uint32_t)((((1.0f/(*dptr[r][c]))-globalData.nearClip)/(globalData.farClip-globalData.nearClip)) * 255.0f);
								}
								else
								{
									for ( int32_t r = 0; r < 2; r++ )
									{
										int32_t px = ix;
										float   pz = z[r];

										for ( int32_t c = 0; c < 2; c++, px++, pz += zstep[r] )
										{
											if ( (outline[r]->flags & 0x1) && px >= ix1[r] && px <= ix2[r] && pz > *dptr[r][c] )
											{
												*dptr[r][c] = pz;

												if ( Debug.renderMode == RENDER_MODE_FLAT_COLOR )
													*ptr[r][c] = 0xFFFF0000;
												else if ( Debug.renderMode == RENDER_MODE_UV )
												{
													float fx = pxu[r][c] / submesh->texture->width;
													float fy = pxv[r][c] / submesh->texture->height;
													*ptr[r][c] = (((uint32_t)(fx * 256.0f))<<16) | (((uint32_t)(fy * 256.0f))<<8);
												}
												else if ( Debug.renderMode == RENDER_MODE_ZBUFFER )
													*ptr[r][c] = (uint32_t)(((rz[r][c]-globalData.nearClip)/(globalData.farClip-globalData.nearClip)) * 255.0f);
												else if ( Debug.renderMode == RENDER_MODE_MIPMAP )
												{
													static const uint32_t mipmapLUT[] = {
														0xFF0000,
														0x00FF00,
														0xFFFF00,
														0x0000FF,
														0xFF00FF,
														0x00FFFF,
														0xFFFFFF,
													};

													if ( Debug.textureMipmapMode == TEXTURE_MIPMAP_LINEAR )
														*ptr[r][c] = mipmapLUT[MIN(desiredMip2,sizeof(mipmapLUT)/sizeof(mipmapLUT[0])-1)];
													else
														*ptr[r][c] = mipmapLUT[MIN(desiredMip,sizeof(mipmapLUT)/sizeof(mipmapLUT[0])-1)];
												}
												else if ( Debug.renderMode == RENDER_MODE_TEXTURED )
												{
													if ( submesh->texture )
													{
														uint32_t color[2];
														const uint32_t itCount = Debug.textureMipmapMode == TEXTURE_MIPMAP_LINEAR ? 2 : 1;

														uint32_t tdesiredMip = desiredMip;
														uint32_t tmipWidth   = mipWidth;
														float tuvScale       = uvScale;

														for ( uint32_t it = 0; it < itCount; it++ )
														{
															if ( Debug.textureFilteringMode == TEXTURE_FILTERING_POINT )
															{
																//--------------------------------
																// Mipmap stuff
																//--------------------------------
																int32_t iy = (int32_t)(pxv[r][c] * uvScale);
																int32_t ix = (int32_t)(pxu[r][c] * uvScale);
													
																if ( Debug.textureAddressingMode == TEXTURE_ADDRESSING_LINEAR )
																{
																	color[it] = submesh->texture->mipData[desiredMip][iy * mipWidth + ix];
																	assert ( ix < (int32_t)mipWidth && iy < (int32_t)mipWidth && ix >= 0 && iy >= 0 );
																}
																else if ( Debug.textureAddressingMode == TEXTURE_ADDRESSING_TILED )
																{
																	uint32_t tileidx = (iy >> 2) * (mipWidth >> 2) + (ix >> 2);
																	uint32_t pixidx  = ((iy & 3) << 2) + (ix & 3);
																	color[it] = submesh->texture->mipData[desiredMip][(tileidx << 4) + pixidx];
																}
																else if ( Debug.textureAddressingMode == TEXTURE_ADDRESSING_SWIZZLED )
																{
																	if ( !(Debug.flags & FLAG_FILTER_LUT ) )
																	{
																		uint32_t swizIdx;
										
																		// yxyx yxyx yxyx yxyx yxyx yxyx yxyx yxyx
																		swizIdx = 0;

																		//    0000 0000 0000 0000 xxxx xxxx xxxx xxxx
																		// => 0x0x 0x0x 0x0x 0x0x 0x0x 0x0x 0x0x 0x0x
																		for ( uint32_t i = 0; i < 16; i++ )
																		{
																			swizIdx |= ((ix & (1<<(i))) << (i));
																			swizIdx |= ((iy & (1<<(i))) << (i+1));
																		}
																		color[it] = submesh->texture->mipData[desiredMip][swizIdx];
																	}
																	else
																	{
																		uint32_t swizIdx;

																		// xxxx => 0x0x 0x0x
																		static const uint32_t mortonLUT[] = {
																			0x00, // 0000 (0x0) => 0000 0000 (0x00)
																			0x01, // 0001 (0x1) => 0000 0001 (0x01)
																			0x04, // 0010 (0x2) => 0000 0100 (0x04)
																			0x05, // 0011 (0x3) => 0000 0101 (0x05)
																			0x10, // 0100 (0x4) => 0001 0000 (0x10)
																			0x11, // 0101 (0x5) => 0001 0001 (0x11)
																			0x14, // 0110 (0x6) => 0001 0100 (0x14)
																			0x15, // 0111 (0x7) => 0001 0101 (0x15)
																			0x40, // 1000 (0x8) => 0100 0000 (0x40)
																			0x41, // 1001 (0x9) => 0100 0001 (0x41)
																			0x44, // 1010 (0xA) => 0100 0100 (0x44)
																			0x45, // 1011 (0xB) => 0100 0101 (0x45)
																			0x50, // 1100 (0xC) => 0101 0000 (0x50)
																			0x51, // 1101 (0xD) => 0101 0001 (0x51)
																			0x54, // 1110 (0xE) => 0101 0100 (0x54)
																			0x55, // 1111 (0xF) => 0101 0101 (0x55)
																		};

																		uint32_t swizX   = mortonLUT[(ix    ) & 0xF]
																						| (mortonLUT[(ix>> 4) & 0xF] <<  8)
																						| (mortonLUT[(ix>> 8) & 0xF] << 16)
																						| (mortonLUT[(ix>>12) & 0xF] << 24);
																		uint32_t swizY   = mortonLUT[(iy    ) & 0xF]
																						| (mortonLUT[(iy>> 4) & 0xF] <<  8)
																						| (mortonLUT[(iy>> 8) & 0xF] << 16)
																						| (mortonLUT[(iy>>12) & 0xF] << 24);
																					swizIdx = swizX | (swizY<<1);

																		color[it] = submesh->texture->mipData[desiredMip][swizIdx];
																	}
																}
															}
															else if ( Debug.textureFilteringMode == TEXTURE_FILTERING_BILINEAR )
															{
																float fx = pxu[r][c] * uvScale;
																float fy = pxv[r][c] * uvScale;
															
																int32_t ix1 = ((int32_t)(fx)) & (mipWidth-1);
																int32_t iy1 = ((int32_t)(fy)) & (mipWidth-1);
																int32_t ix2 = (ix1 + 1)       & (mipWidth-1);
																int32_t iy2 = (iy1 + 1)       & (mipWidth-1);

																uint32_t c00, c01, c10, c11;

																if ( Debug.textureAddressingMode == TEXTURE_ADDRESSING_LINEAR )
																{
																	c00 = submesh->texture->mipData[desiredMip][iy1 * mipWidth + ix1];
																	c01 = submesh->texture->mipData[desiredMip][iy1 * mipWidth + ix2];
																	c10 = submesh->texture->mipData[desiredMip][iy2 * mipWidth + ix1];
																	c11 = submesh->texture->mipData[desiredMip][iy2 * mipWidth + ix2];
																}
																else if ( Debug.textureAddressingMode == TEXTURE_ADDRESSING_TILED )
																{
																	uint32_t tileidx = (iy1 >> 2) * (mipWidth >> 2) + (ix1 >> 2);
																	uint32_t pixidx  = ((iy1 & 3) << 2) + (ix1 & 3);
																	c00 = submesh->texture->mipData[desiredMip][(tileidx << 4) + pixidx];
																	tileidx = (iy1 >> 2) * (mipWidth >> 2) + (ix2 >> 2);
																	pixidx  = ((iy1 & 3) << 2) + (ix2 & 3);
																	c01 = submesh->texture->mipData[desiredMip][(tileidx << 4) + pixidx];
																	tileidx = (iy2 >> 2) * (mipWidth >> 2) + (ix1 >> 2);
																	pixidx  = ((iy2 & 3) << 2) + (ix1 & 3);
																	c10 = submesh->texture->mipData[desiredMip][(tileidx << 4) + pixidx];
																	tileidx = (iy2 >> 2) * (mipWidth >> 2) + (ix2 >> 2);
																	pixidx  = ((iy2 & 3) << 2) + (ix2 & 3);
																	c11 = submesh->texture->mipData[desiredMip][(tileidx << 4) + pixidx];
																}
																else if ( Debug.textureAddressingMode == TEXTURE_ADDRESSING_SWIZZLED )
																{
																	if ( !(Debug.flags & FLAG_FILTER_LUT ) )
																	{
																		uint32_t swizIdx;
										
																		//    yxyx yxyx yxyx yxyx yxyx yxyx yxyx yxyx
																		//    0000 0000 0000 0000 xxxx xxxx xxxx xxxx
																		// => 0x0x 0x0x 0x0x 0x0x 0x0x 0x0x 0x0x 0x0x
																		swizIdx = 0;
																		for ( uint32_t i = 0; i < 16; i++ )
																		{
																			swizIdx |= ((ix1 & (1<<(i))) << (i));
																			swizIdx |= ((iy1 & (1<<(i))) << (i+1));
																		}
																		c00 = submesh->texture->mipData[desiredMip][swizIdx];
																		swizIdx = 0;
																		for ( uint32_t i = 0; i < 16; i++ )
																		{
																			swizIdx |= ((ix2 & (1<<(i))) << (i));
																			swizIdx |= ((iy1 & (1<<(i))) << (i+1));
																		}
																		c01 = submesh->texture->mipData[desiredMip][swizIdx];
																		swizIdx = 0;
																		for ( uint32_t i = 0; i < 16; i++ )
																		{
																			swizIdx |= ((ix1 & (1<<(i))) << (i));
																			swizIdx |= ((iy2 & (1<<(i))) << (i+1));
																		}
																		c10 = submesh->texture->mipData[desiredMip][swizIdx];
																		swizIdx = 0;
																		for ( uint32_t i = 0; i < 16; i++ )
																		{
																			swizIdx |= ((ix2 & (1<<(i))) << (i));
																			swizIdx |= ((iy2 & (1<<(i))) << (i+1));
																		}
																		c11 = submesh->texture->mipData[desiredMip][swizIdx];
																	}
																	else
																	{
																		uint32_t swizIdx;

																		// xxxx => 0x0x 0x0x
																		static const uint32_t mortonLUT[] = {
																			0x00, // 0000 (0x0) => 0000 0000 (0x00)
																			0x01, // 0001 (0x1) => 0000 0001 (0x01)
																			0x04, // 0010 (0x2) => 0000 0100 (0x04)
																			0x05, // 0011 (0x3) => 0000 0101 (0x05)
																			0x10, // 0100 (0x4) => 0001 0000 (0x10)
																			0x11, // 0101 (0x5) => 0001 0001 (0x11)
																			0x14, // 0110 (0x6) => 0001 0100 (0x14)
																			0x15, // 0111 (0x7) => 0001 0101 (0x15)
																			0x40, // 1000 (0x8) => 0100 0000 (0x40)
																			0x41, // 1001 (0x9) => 0100 0001 (0x41)
																			0x44, // 1010 (0xA) => 0100 0100 (0x44)
																			0x45, // 1011 (0xB) => 0100 0101 (0x45)
																			0x50, // 1100 (0xC) => 0101 0000 (0x50)
																			0x51, // 1101 (0xD) => 0101 0001 (0x51)
																			0x54, // 1110 (0xE) => 0101 0100 (0x54)
																			0x55, // 1111 (0xF) => 0101 0101 (0x55)
																		};

																		uint32_t swizX   = mortonLUT[(ix1    ) & 0xF]
																						| (mortonLUT[(ix1>> 4) & 0xF] <<  8)
																						| (mortonLUT[(ix1>> 8) & 0xF] << 16)
																						| (mortonLUT[(ix1>>12) & 0xF] << 24);
																		uint32_t swizY   = mortonLUT[(iy1    ) & 0xF]
																						| (mortonLUT[(iy1>> 4) & 0xF] <<  8)
																						| (mortonLUT[(iy1>> 8) & 0xF] << 16)
																						| (mortonLUT[(iy1>>12) & 0xF] << 24);
																					swizIdx = swizX | (swizY<<1);

																		c00 = submesh->texture->mipData[desiredMip][swizIdx];

																		swizX    = mortonLUT[(ix2    ) & 0xF]
																				| (mortonLUT[(ix2>> 4) & 0xF] <<  8)
																				| (mortonLUT[(ix2>> 8) & 0xF] << 16)
																				| (mortonLUT[(ix2>>12) & 0xF] << 24);
																		swizY    = mortonLUT[(iy1    ) & 0xF]
																				| (mortonLUT[(iy1>> 4) & 0xF] <<  8)
																				| (mortonLUT[(iy1>> 8) & 0xF] << 16)
																				| (mortonLUT[(iy1>>12) & 0xF] << 24);
																			swizIdx = swizX | (swizY<<1);

																		c01 = submesh->texture->mipData[desiredMip][swizIdx];

																		swizX    = mortonLUT[(ix1    ) & 0xF]
																				| (mortonLUT[(ix1>> 4) & 0xF] <<  8)
																				| (mortonLUT[(ix1>> 8) & 0xF] << 16)
																				| (mortonLUT[(ix1>>12) & 0xF] << 24);
																		swizY    = mortonLUT[(iy2    ) & 0xF]
																				| (mortonLUT[(iy2>> 4) & 0xF] <<  8)
																				| (mortonLUT[(iy2>> 8) & 0xF] << 16)
																				| (mortonLUT[(iy2>>12) & 0xF] << 24);
																			swizIdx = swizX | (swizY<<1);

																		c10 = submesh->texture->mipData[desiredMip][swizIdx];

																		swizX    = mortonLUT[(ix2    ) & 0xF]
																				| (mortonLUT[(ix2>> 4) & 0xF] <<  8)
																				| (mortonLUT[(ix2>> 8) & 0xF] << 16)
																				| (mortonLUT[(ix2>>12) & 0xF] << 24);
																		swizY    = mortonLUT[(iy2    ) & 0xF]
																				| (mortonLUT[(iy2>> 4) & 0xF] <<  8)
																				| (mortonLUT[(iy2>> 8) & 0xF] << 16)
																				| (mortonLUT[(iy2>>12) & 0xF] << 24);
																			swizIdx = swizX | (swizY<<1);

																		c11 = submesh->texture->mipData[desiredMip][swizIdx];
																	}
																}
																else
																	c00 = 0xFF00FF, c01 = 0xFFFFFFFF, c10 = 0xFFFFFFFF, c11 = 0xFF00FF;
															
																uint32_t fracXFactor = (uint32_t)((fx - ix1) * 65536);
																uint32_t fracYFactor = (uint32_t)((fy - iy1) * 65536);
															
																uint8_t cr =
																		(((65536 - fracYFactor) * ((((65536 - fracXFactor) * (c00 & 0xFF)) + ((fracXFactor) * (c01 & 0xFF))) >> 16)) >> 16)
																	+ (((        fracYFactor) * ((((65536 - fracXFactor) * (c10 & 0xFF)) + ((fracXFactor) * (c11 & 0xFF))) >> 16)) >> 16);
																uint8_t cg =
																		(((65536 - fracYFactor) * ((((65536 - fracXFactor) * ((c00>>8) & 0xFF)) + ((fracXFactor) * ((c01>>8) & 0xFF))) >> 16)) >> 16)
																	+ (((        fracYFactor) * ((((65536 - fracXFactor) * ((c10>>8) & 0xFF)) + ((fracXFactor) * ((c11>>8) & 0xFF))) >> 16)) >> 16);
																uint8_t cb =
																		(((65536 - fracYFactor) * ((((65536 - fracXFactor) * ((c00>>16) & 0xFF)) + ((fracXFactor) * ((c01>>16) & 0xFF))) >> 16)) >> 16)
																	+ (((        fracYFactor) * ((((65536 - fracXFactor) * ((c10>>16) & 0xFF)) + ((fracXFactor) * ((c11>>16) & 0xFF))) >> 16)) >> 16);
															
																uint32_t blendedColor = (cb<<16) | (cg<<8) | (cr);
															
																color[it] = blendedColor;
															}

															desiredMip = desiredMip2;
															mipWidth   = (submesh->texture->width >> desiredMip);
															uvScale    = 1.0f / (1<<desiredMip);
														}

														desiredMip = tdesiredMip;
														mipWidth   = tmipWidth;
														uvScale    = tuvScale;

														if ( Debug.textureMipmapMode == TEXTURE_MIPMAP_LINEAR )
														{
															uint32_t f2 = (uint32_t)(mipT * 65536);
															uint32_t f1 = 65536 - f2;

															//(color[0] * f1 + color[1] * f2) >> 16

															uint8_t cr = (f1 * ((color[0]    )&0xFF) + f2 * ((color[1]    )&0xFF))>>16;
															uint8_t cg = (f1 * ((color[0]>>8 )&0xFF) + f2 * ((color[1]>>8 )&0xFF))>>16;
															uint8_t cb = (f1 * ((color[0]>>16)&0xFF) + f2 * ((color[1]>>16)&0xFF))>>16;

															*ptr[r][c] = (cb<<16) | (cg<<8) | (cr);
														}
														else
														{
															*ptr[r][c] = color[0];
														}
													}
													else
														*ptr[r][c] = 0xFF00FF;
												}
											}
										}
									}
								}
							}
						}
					}

					//--------------------------------
					// Reset outline table
					//--------------------------------
#if AOS_OUTLINE_TABLE
					outline_table_entry* outline = globalData.outlineTable + minTriY;
					for ( int32_t y = minTriY; y <= maxTriY; y++, outline++ )
					{
						outline->flags = 0;
						outline->minX = (float)globalData.renderTarget.width;
						outline->maxX = 0.0f;
					}
#else
					float* outlineMinX = globalData.outlineTable.minX + minTriY;
					float* outlineMaxX = globalData.outlineTable.maxX + minTriY;
					for ( int32_t y = minTriY; y <= maxTriY; y++, outlineMinX++, outlineMaxX++ )
					{
						*outlineMinX = (float)globalData.renderTarget.width;
						*outlineMaxX = 0.0f;
					}
#endif
				}
#pragma endregion
				else if ( Debug.flags & FLAG_RASTERIZE )
#pragma region Pixel rasterization
				{
					//--------------------------------
					// Rasterize time
					//--------------------------------
#if AOS_OUTLINE_TABLE
					outline_table_entry* outline = globalData.outlineTable + minTriY;
					uint32_t* colorRowPtr        = (uint32_t*)((uintptr_t)globalData.renderTarget.colorBuffer + (globalData.renderTarget.height - minTriY - 1) * globalData.renderTarget.pitch);
					float* depthRowPtr           = globalData.renderTarget.depthBuffer + minTriY * globalData.renderTarget.width;
					for ( int32_t y = minTriY; y <= maxTriY; y++, outline++, depthRowPtr += globalData.renderTarget.width )
#else
					float* outlineMinX    = globalData.outlineTable.minX + minTriY;
					float* outlineMaxX    = globalData.outlineTable.maxX + minTriY;
					float* outlineMinZ    = globalData.outlineTable.minZ + minTriY;
					float* outlineMaxZ    = globalData.outlineTable.maxZ + minTriY;
					float* outlineMinU    = globalData.outlineTable.minU + minTriY;
					float* outlineMaxU    = globalData.outlineTable.maxU + minTriY;
					float* outlineMinV    = globalData.outlineTable.minV + minTriY;
					float* outlineMaxV    = globalData.outlineTable.maxV + minTriY;
					uint32_t* colorRowPtr = (uint32_t*)((uintptr_t)globalData.renderTarget.colorBuffer + (globalData.renderTarget.height - minTriY - 1) * globalData.renderTarget.pitch);
					float* depthRowPtr    = globalData.renderTarget.depthBuffer + minTriY * globalData.renderTarget.width;
					for ( int32_t y = minTriY; y <= maxTriY; y++, outlineMinX++, outlineMaxX++, outlineMinZ++, outlineMaxZ++, outlineMinU++, outlineMinV++, outlineMaxU++, outlineMaxV++, depthRowPtr += globalData.renderTarget.width )
#endif
					{
#if AOS_OUTLINE_TABLE
						uint32_t* ptr     = colorRowPtr + (uint32_t)outline->minX;
						uint32_t* endptr  = colorRowPtr + (uint32_t)outline->maxX;
						float* zptr       = depthRowPtr + (uint32_t)outline->minX;
						float* endzptr    = depthRowPtr + (uint32_t)outline->maxX;
						float x1 = outline->minX;
						float x2 = outline->maxX;
#else
						uint32_t* ptr     = colorRowPtr + (uint32_t)*outlineMinX;
						uint32_t* endptr  = colorRowPtr + (uint32_t)*outlineMaxX;
						float* zptr       = depthRowPtr + (uint32_t)*outlineMinX;
						float* endzptr    = depthRowPtr + (uint32_t)*outlineMaxX;
						float x1 = *outlineMinX;
						float x2 = *outlineMaxX;
#endif

						//assert ( x1 > 10.0f );

#if AOS_OUTLINE_TABLE
						float z1 = outline->minZ;
						float z2 = outline->maxZ;

						float u1 = outline->minU;
						float u2 = outline->maxU;
						float v1 = outline->minV;
						float v2 = outline->maxV;
#else
						float z1 = *outlineMinZ;
						float z2 = *outlineMaxZ;

						float u1 = *outlineMinU;
						float u2 = *outlineMaxU;
						float v1 = *outlineMinV;
						float v2 = *outlineMaxV;
#endif

						float dx = (x2 - x1);//+(Debug.flags & FLAG_DERP ? 1 : 0);
						float dz = (z2 - z1) / dx;
						float du = (u2 - u1) / dx;
						float dv = (v2 - v1) / dx;

						//if ( Debug.flags & FLAG_DERP2 )
						//{
						//	int32_t ix1  = (int32_t)x1 + 1;
						//	float subtex = ix1 - x1;
						//
						//	z1 += subtex * dz;
						//	u1 += subtex * du;
						//	v1 += subtex * dv;
						//}

#if 0
						// Faster, but causes extremely jumpy textures. Avoid using!
						float z = z1;
						float u = u1;
						float v = v1;
						for ( ; ptr <= endptr; ptr++, zptr++, z += dz, u += du, v += dv )
						{
#else
						for ( int32_t xinc = 0; ptr <= endptr; ptr++, zptr++, xinc++ )
						{
							float z = z1 + xinc * dz;
							float u = u1 + xinc * du;
							float v = v1 + xinc * dv;
#endif
							if ( !(Debug.flags & FLAG_DEPTH_TESTING) || z > *zptr )
							{
								if ( Debug.renderMode == RENDER_MODE_FLAT_COLOR )
									*ptr = 0xFFFF0000;
								else if ( Debug.renderMode == RENDER_MODE_UV )
								{
									float fx = u * (1.0f/z);
									float fy = v * (1.0f/z);
									fx = fx - (int32_t)fx;
									fy = fy - (int32_t)fy;
									*ptr = (((uint8_t)(fx * 256.0f))<<16) | (((uint8_t)(fy * 256.0f))<<8);
								}
								else if ( Debug.renderMode == RENDER_MODE_ZBUFFER )
									*ptr = (uint32_t)((1.0f-((1.0f/z)-globalData.nearClip)/(globalData.farClip-globalData.nearClip)) * 255.0f);
								else if ( Debug.renderMode == RENDER_MODE_TEXTURED )
								{
									if ( submesh->texture )
									{
										float fx = u * (1.0f/z);
										float fy = v * (1.0f/z);
										int32_t ix = (int32_t)((fx - (int32_t)fx) * submesh->texture->width );
										int32_t iy = (int32_t)((fy - (int32_t)fy) * submesh->texture->height);
										assert ( ix >= 0 && ix < submesh->texture->width );
										assert ( iy >= 0 && iy < submesh->texture->height );

										if ( Debug.textureAddressingMode == TEXTURE_ADDRESSING_LINEAR )
										{
											*ptr = submesh->texture->mipData[0][iy * submesh->texture->width + ix];
										}
										else if ( Debug.textureAddressingMode == TEXTURE_ADDRESSING_TILED )
										{
											uint32_t tileidx = (iy >> 2) * (submesh->texture->width >> 2) + (ix >> 2);
											uint32_t pixidx  = ((iy & 3) << 2) + (ix & 3);
											*ptr = submesh->texture->mipData[0][(tileidx << 4) + pixidx];
										}
										else if ( Debug.textureAddressingMode == TEXTURE_ADDRESSING_SWIZZLED )
										{
											if ( !(Debug.flags & FLAG_FILTER_LUT) )
											{
												uint32_t swizIdx;
										
												// yxyx yxyx yxyx yxyx yxyx yxyx yxyx yxyx
												swizIdx = 0;

												//    0000 0000 0000 0000 xxxx xxxx xxxx xxxx
												// => 0x0x 0x0x 0x0x 0x0x 0x0x 0x0x 0x0x 0x0x
												for ( uint32_t i = 0; i < 16; i++ )
												{
													swizIdx |= ((ix & (1<<(i))) << (i));
													swizIdx |= ((iy & (1<<(i))) << (i+1));
												}
												*ptr = submesh->texture->mipData[0][swizIdx];
											}
											else
											{
												uint32_t swizIdx;

												// xxxx => 0x0x 0x0x
												static const uint32_t mortonLUT[] = {
													0x00, // 0000 (0x0) => 0000 0000 (0x00)
													0x01, // 0001 (0x1) => 0000 0001 (0x01)
													0x04, // 0010 (0x2) => 0000 0100 (0x04)
													0x05, // 0011 (0x3) => 0000 0101 (0x05)
													0x10, // 0100 (0x4) => 0001 0000 (0x10)
													0x11, // 0101 (0x5) => 0001 0001 (0x11)
													0x14, // 0110 (0x6) => 0001 0100 (0x14)
													0x15, // 0111 (0x7) => 0001 0101 (0x15)
													0x40, // 1000 (0x8) => 0100 0000 (0x40)
													0x41, // 1001 (0x9) => 0100 0001 (0x41)
													0x44, // 1010 (0xA) => 0100 0100 (0x44)
													0x45, // 1011 (0xB) => 0100 0101 (0x45)
													0x50, // 1100 (0xC) => 0101 0000 (0x50)
													0x51, // 1101 (0xD) => 0101 0001 (0x51)
													0x54, // 1110 (0xE) => 0101 0100 (0x54)
													0x55, // 1111 (0xF) => 0101 0101 (0x55)
												};

												uint32_t swizX   = mortonLUT[(ix    ) & 0xF]
																| (mortonLUT[(ix>> 4) & 0xF] <<  8)
																| (mortonLUT[(ix>> 8) & 0xF] << 16)
																| (mortonLUT[(ix>>12) & 0xF] << 24);
												uint32_t swizY   = mortonLUT[(iy    ) & 0xF]
																| (mortonLUT[(iy>> 4) & 0xF] <<  8)
																| (mortonLUT[(iy>> 8) & 0xF] << 16)
																| (mortonLUT[(iy>>12) & 0xF] << 24);
															swizIdx = swizX | (swizY<<1);

												*ptr = submesh->texture->mipData[0][swizIdx];
											}
										}
									}
									else
										*ptr = 0xFF00FF;
								}
								//else if ( Debug.renderMode == RENDER_MODE_TEXTURE_BILINEAR )
								//{
								//	if ( submesh->texture )
								//	{
								//		float fx = (u * (1.0f/z));
								//		float fy = (v * (1.0f/z));
								//		fx = (fx - (int32_t)fx) * submesh->texture->width;
								//		fy = (fy - (int32_t)fy) * submesh->texture->height;
								//
								//		int32_t ix1 = ((int32_t)(fx)) & (submesh->texture->width -1);
								//		int32_t iy1 = ((int32_t)(fy)) & (submesh->texture->height-1);
								//		int32_t ix2 = (ix1 + 1)                                  & (submesh->texture->width -1);
								//		int32_t iy2 = (iy1 + 1)                                  & (submesh->texture->height-1);
								//
								//		uint32_t fracXFactor = (uint32_t)((fx - ix1) * 65536);
								//		uint32_t fracYFactor = (uint32_t)((fy - iy1) * 65536);
								//
								//		uint32_t c00 = submesh->texture->mipData[0][iy1 * submesh->texture->width + ix1];
								//		uint32_t c01 = submesh->texture->mipData[0][iy1 * submesh->texture->width + ix2];
								//		uint32_t c10 = submesh->texture->mipData[0][iy2 * submesh->texture->width + ix1];
								//		uint32_t c11 = submesh->texture->mipData[0][iy2 * submesh->texture->width + ix2];
								//
								//		uint8_t r =
								//			  (((65536 - fracYFactor) * ((((65536 - fracXFactor) * (c00 & 0xFF)) + ((fracXFactor) * (c01 & 0xFF))) >> 16)) >> 16)
								//			+ (((        fracYFactor) * ((((65536 - fracXFactor) * (c10 & 0xFF)) + ((fracXFactor) * (c11 & 0xFF))) >> 16)) >> 16);
								//		uint8_t g =
								//			  (((65536 - fracYFactor) * ((((65536 - fracXFactor) * ((c00>>8) & 0xFF)) + ((fracXFactor) * ((c01>>8) & 0xFF))) >> 16)) >> 16)
								//			+ (((        fracYFactor) * ((((65536 - fracXFactor) * ((c10>>8) & 0xFF)) + ((fracXFactor) * ((c11>>8) & 0xFF))) >> 16)) >> 16);
								//		uint8_t b =
								//			  (((65536 - fracYFactor) * ((((65536 - fracXFactor) * ((c00>>16) & 0xFF)) + ((fracXFactor) * ((c01>>16) & 0xFF))) >> 16)) >> 16)
								//			+ (((        fracYFactor) * ((((65536 - fracXFactor) * ((c10>>16) & 0xFF)) + ((fracXFactor) * ((c11>>16) & 0xFF))) >> 16)) >> 16);
								//
								//		uint32_t color = (b<<16) | (g<<8) | (r);
								//
								//		*ptr = color;
								//	}
								//	else
								//		*ptr = 0xFF00FF;
								//}
								*zptr = z;
							}
						}

						colorRowPtr = (uint32_t*)((uintptr_t)colorRowPtr - globalData.renderTarget.pitch);
					}

					//--------------------------------
					// Reset outline table
					//--------------------------------
#if AOS_OUTLINE_TABLE
					outline = globalData.outlineTable + minTriY;
					for ( int32_t y = minTriY; y <= maxTriY; y++, outline++ )
					{
						outline->flags = 0;
						outline->minX  = (float)globalData.renderTarget.width;
						outline->maxX  = 0.0f;
					}
#else
					outlineMinX = globalData.outlineTable.minX + minTriY;
					outlineMaxX = globalData.outlineTable.maxX + minTriY;
					for ( int32_t y = minTriY; y <= maxTriY; y++, outlineMinX++, outlineMaxX++ )
					{
						*outlineMinX = (float)globalData.renderTarget.width;
						*outlineMaxX = 0.0f;
					}
#endif
				}
#pragma endregion
			}
		}
	}
	
	return 0;
}

////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////