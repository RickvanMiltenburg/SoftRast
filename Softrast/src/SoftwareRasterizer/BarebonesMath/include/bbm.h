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

#ifdef __cplusplus
extern "C" {
#endif

#include "config.h"
#include "types.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////

void bbm_soa_vec2_init ( bbm_soa_vec2* v, float* x, float* y, uint32_t vecCount );
void bbm_soa_vec3_init ( bbm_soa_vec3* v, float* x, float* y, float* z, uint32_t vecCount );
void bbm_soa_vec4_init ( bbm_soa_vec4* v, float* x, float* y, float* z, float* w, uint32_t vecCount );
void bbm_soa_mat4_init_auto ( bbm_soa_mat4* m, float* data, uint32_t matCount );

///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////

void bbm_soa_vec3_extract ( bbm_aos_vec3* o, const bbm_soa_vec3* v, uint32_t i );
void bbm_soa_vec4_extract ( bbm_aos_vec4* o, const bbm_soa_vec4* v, uint32_t i );
void bbm_soa_mat4_extract ( bbm_aos_mat4* out, const bbm_soa_mat4* mats, uint32_t matIndex );

///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////

void bbm_soa_vec2_overwrite_add_xy_float ( bbm_soa_vec2* v, const float add );
void bbm_soa_vec2_overwrite_mul_soa_vec4_w ( bbm_soa_vec2* o, const bbm_soa_vec4* v );

void bbm_soa_vec4_mul_x_float  ( bbm_soa_vec1* out, const bbm_soa_vec4* v, float f );
void bbm_soa_vec4_mul_y_float  ( bbm_soa_vec1* out, const bbm_soa_vec4* v, float f );
void bbm_soa_vec4_mul_xy_float ( bbm_soa_vec2* out, const bbm_soa_vec4* v, float f );
void bbm_soa_vec4_mad_x  ( bbm_soa_vec1* out, const bbm_soa_vec4* v, float multiply, float add );
void bbm_soa_vec4_mad_y  ( bbm_soa_vec1* out, const bbm_soa_vec4* v, float multiply, float add );
void bbm_soa_vec4_mad_xy ( bbm_soa_vec2* out, const bbm_soa_vec4* v, float multiply, float add );

void bbm_soa_vec4_overwrite_mul_x_float  ( bbm_soa_vec4* v, float f );
void bbm_soa_vec4_overwrite_mul_y_float  ( bbm_soa_vec4* v, float f );
void bbm_soa_vec4_overwrite_mul_xy_float ( bbm_soa_vec4* v, float f );
void bbm_soa_vec4_overwrite_mad_x  ( bbm_soa_vec4* v, float multiply, float add );
void bbm_soa_vec4_overwrite_mad_y  ( bbm_soa_vec4* v, float multiply, float add );
void bbm_soa_vec4_overwrite_mad_xy ( bbm_soa_vec4* v, float multiply, float add );

void bbm_soa_vec4_div_xy_w  ( bbm_soa_vec2* out, const bbm_soa_vec4* v );
void bbm_soa_vec4_div_xyz_w ( bbm_soa_vec3* out, const bbm_soa_vec4* v );

void bbm_soa_vec4_overwrite_mul_xy_w  ( bbm_soa_vec4* v );
void bbm_soa_vec4_overwrite_mul_xyz_w ( bbm_soa_vec4* v );
void bbm_soa_vec4_overwrite_div_xy_w  ( bbm_soa_vec4* v );
void bbm_soa_vec4_overwrite_div_xyz_w ( bbm_soa_vec4* v );

void bbm_soa_vec4_overwrite_rcp_w ( bbm_soa_vec4* v );

///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////

float bbm_aos_mat4_max ( const bbm_aos_mat4* m );
float bbm_aos_mat4_min ( const bbm_aos_mat4* m );
void bbm_soa_vec3_min_xyz ( bbm_aos_vec3* out, const bbm_soa_vec3* v );
void bbm_soa_vec3_max_xyz ( bbm_aos_vec3* out, const bbm_soa_vec3* v );

void bbm_aos_mat4_mul_aos_mat4 ( bbm_aos_mat4* out, const bbm_aos_mat4* m1, const bbm_aos_mat4* m2 );
void bbm_aos_mat4_mul_aos_vec3w0 ( bbm_aos_vec3* out, const bbm_aos_mat4* m, const bbm_aos_vec3* v );
void bbm_aos_mat4_mul_aos_vec3w1 ( bbm_aos_vec3* out, const bbm_aos_mat4* m, const bbm_aos_vec3* v );
void bbm_aos_mat4_mul_aos_vec3w1_out_vec4 ( bbm_aos_vec4* out, const bbm_aos_mat4* m, const bbm_aos_vec3* v );
void bbm_aos_mat4_mul_aos_vec4 ( bbm_aos_vec4* out, const bbm_aos_mat4* m, const bbm_aos_vec4* v );

void bbm_aos_mat4_mul_soa_mat4 ( bbm_soa_mat4* out, const bbm_aos_mat4* m1, const bbm_soa_mat4* m2 );
void bbm_aos_mat4_mul_soa_vec3w0 ( bbm_soa_vec3* out, const bbm_aos_mat4* m, const bbm_soa_vec3* v );
void bbm_aos_mat4_mul_soa_vec3w1 ( bbm_soa_vec3* out, const bbm_aos_mat4* m, const bbm_soa_vec3* v );
void bbm_aos_mat4_mul_soa_vec3w1_out_vec4 ( bbm_soa_vec4* out, const bbm_aos_mat4* m, const bbm_soa_vec3* v );
void bbm_aos_mat4_mul_soa_vec4 ( bbm_soa_vec4* out, const bbm_aos_mat4* m, const bbm_soa_vec4* v );

///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////

float bbm_aos_mat4_determinant ( const bbm_aos_mat4* m );
void bbm_aos_mat4_inverse ( bbm_aos_mat4* out, const bbm_aos_mat4* m );

///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
};
#endif