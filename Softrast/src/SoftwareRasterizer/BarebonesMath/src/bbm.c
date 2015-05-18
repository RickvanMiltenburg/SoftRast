/*
	BareBones Math, by Rick van Miltenburg

	Custom math library meant to provide the exact functions I want and the way I want to use them.

	---

	Copyright (c) 2015 Rick van Miltenburg

	Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <assert.h>
#include <immintrin.h>
#include <float.h>

#include "../include/bbm.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////

void bbm_soa_vec2_init ( bbm_soa_vec2* v, float* x, float* y, uint32_t vecCount )
{
#if BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_SSE
	assert ( (((uintptr_t)x) & 15) == 0 );	// Not 16-byte aligned!
	assert ( (((uintptr_t)y) & 15) == 0 );	// Not 16-byte aligned!
#endif

	v->x = x;
	v->y = y;
	v->vectorCount = vecCount;
}

void bbm_soa_vec3_init ( bbm_soa_vec3* v, float* x, float* y, float* z, uint32_t vecCount )
{
#if BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_SSE
	assert ( (((uintptr_t)x) & 15) == 0 );	// Not 16-byte aligned!
	assert ( (((uintptr_t)y) & 15) == 0 );	// Not 16-byte aligned!
	assert ( (((uintptr_t)z) & 15) == 0 );	// Not 16-byte aligned!
#endif

	v->x = x;
	v->y = y;
	v->z = z;
	v->vectorCount = vecCount;
}

void bbm_soa_vec4_init ( bbm_soa_vec4* v, float* x, float* y, float* z, float* w, uint32_t vecCount )
{
#if BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_SSE
	assert ( (((uintptr_t)x) & 15) == 0 );	// Not 16-byte aligned!
	assert ( (((uintptr_t)y) & 15) == 0 );	// Not 16-byte aligned!
	assert ( (((uintptr_t)z) & 15) == 0 );	// Not 16-byte aligned!
	assert ( (((uintptr_t)w) & 15) == 0 );	// Not 16-byte aligned!
#endif

	v->x = x;
	v->y = y;
	v->z = z;
	v->w = w;
	v->vectorCount = vecCount;
}

void bbm_soa_mat4_init_auto ( bbm_soa_mat4* m, float* data, uint32_t matCount )
{
#if BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_SSE
	assert ( (((uintptr_t)data) & 15) == 0 );	// Not 16-byte aligned!
	assert ( (matCount & 3) == 0 );				// Must be a multiple of 4 for this version of the constructor!
#endif

	for ( uint32_t i = 0; i < (4*4); i++, data += matCount )
		m->cells[i] = data;
	m->matrixCount = matCount;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////

void bbm_soa_vec3_extract ( bbm_aos_vec3* o, const bbm_soa_vec3* v, uint32_t i )
{
	assert ( i < v->vectorCount );
	o->x = v->x[i], o->y = v->y[i], o->z = v->z[i];
}

void bbm_soa_vec4_extract ( bbm_aos_vec4* o, const bbm_soa_vec4* v, uint32_t i )
{
	assert ( i < v->vectorCount );
	o->x = v->x[i], o->y = v->y[i], o->z = v->z[i], o->w = v->w[i];
}

void bbm_soa_mat4_extract ( bbm_aos_mat4* out, const bbm_soa_mat4* mats, uint32_t matIndex )
{
	assert ( matIndex < mats->matrixCount );
	float* outptr = out->cells;
	const float* const* inptr = mats->cells;
	for ( uint32_t i = 0; i < 16; i++ )
		*(outptr++) = (*inptr++)[matIndex];
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////

void bbm_soa_vec2_overwrite_add_xy_float ( bbm_soa_vec2* v, const float add )
{
#if BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_NONE
	float *x = v->x, *y = v->y;
	for ( uint32_t i = 0; i < v->vectorCount; i++, x++, y++ )
	{
		*(x) = *(x) + add;
		*(y) = *(y) + add;
	}
#elif BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_SSE
	__m128 a = _mm_set1_ps ( add );
	__m128 *x = (__m128*)v->x, *y = (__m128*)v->y;
	for ( uint32_t i = 0; i < (v->vectorCount+3)/4; i++, x++ )
		*(x) = _mm_add_ps ( *x, a );
	for ( uint32_t i = 0; i < (v->vectorCount+3)/4; i++, y++ )
		*(y) = _mm_add_ps ( *y, a );
#elif BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_AVX
#error Not implemented
#endif
}

void bbm_soa_vec2_overwrite_mul_soa_vec4_w ( bbm_soa_vec2* o, const bbm_soa_vec4* v )
{
	assert ( o->vectorCount >= v->vectorCount );
#if BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_NONE
	float *w = v->w;
	float *ox = o->x, *oy = o->y;
	for ( uint32_t i = 0; i < v->vectorCount; i++, ox++, oy++, w++ )
	{
		*(ox) = *(ox) * *(w);
		*(oy) = *(oy) * *(w);
	}
#elif BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_SSE
	__m128 *w  = (__m128*)v->w;
	__m128 *ox = (__m128*)o->x, *oy = (__m128*)o->y;
	for ( uint32_t i = 0; i < (v->vectorCount+3)/4; i++, ox++, w++ )
		*(ox) = _mm_mul_ps ( *ox, *w );
	w = (__m128*)v->w;
	for ( uint32_t i = 0; i < (v->vectorCount+3)/4; i++, oy++, w++ )
		*(oy) = _mm_mul_ps ( *oy, *w );
#elif BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_AVX
#error Not implemented
#endif
}

void bbm_soa_vec4_mul_x_float ( bbm_soa_vec1* out, const bbm_soa_vec4* v, float f )
{
	assert ( out->vectorCount >= v->vectorCount );
#if BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_NONE
	float *x = v->x;
	float *ox = out->x;
	for ( uint32_t i = 0; i < v->vectorCount; i++ )
		*(ox++) = *(x++) * f;
#elif BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_SSE
	__m128 fv  = _mm_set1_ps ( f );
	__m128 *x  = (__m128*)v->x;
	__m128 *ox = (__m128*)out->x;
	for ( uint32_t i = 0; i < (v->vectorCount+3)/4; i++ )
		*(ox++) = _mm_mul_ps ( *(x++), fv );
#elif BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_AVX
#error Not implemented
#endif
}

void bbm_soa_vec4_mul_y_float ( bbm_soa_vec1* out, const bbm_soa_vec4* v, float f )
{
	assert ( out->vectorCount >= v->vectorCount );
#if BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_NONE
	float *y = v->y;
	float *oy = out->x;
	for ( uint32_t i = 0; i < v->vectorCount; i++ )
		*(oy++) = *(y++) * f;
#elif BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_SSE
	__m128 fv  = _mm_set1_ps ( f );
	__m128 *y  = (__m128*)v->y;
	__m128 *oy = (__m128*)out->x;
	for ( uint32_t i = 0; i < (v->vectorCount+3)/4; i++ )
		*(oy++) = _mm_mul_ps ( *(y++), fv );
#elif BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_AVX
#error Not implemented
#endif
}

void bbm_soa_vec4_mul_xy_float ( bbm_soa_vec2* out, const bbm_soa_vec4* v, float f )
{
	assert ( out->vectorCount >= v->vectorCount );
#if BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_NONE
	float *x = v->x, *y = v->y;
	float *ox = out->x, *oy = out->y;
	for ( uint32_t i = 0; i < v->vectorCount; i++ )
		*(ox++) = *(x++) * f;
	for ( uint32_t i = 0; i < v->vectorCount; i++ )
		*(oy++) = *(y++) * f;
#elif BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_SSE
	__m128 fv  = _mm_set1_ps ( f );
	__m128 *x  = (__m128*)v->x,   *y  = (__m128*)v->y;
	__m128 *ox = (__m128*)out->x, *oy = (__m128*)out->y;
	for ( uint32_t i = 0; i < (v->vectorCount+3)/4; i++ )
		*(ox++) = _mm_mul_ps ( *(x++), fv );
	for ( uint32_t i = 0; i < (v->vectorCount+3)/4; i++ )
		*(oy++) = _mm_mul_ps ( *(y++), fv );
#elif BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_AVX
#error Not implemented
#endif
}

void bbm_soa_vec4_mad_x ( bbm_soa_vec1* out, const bbm_soa_vec4* v, float multiply, float add )
{
	assert ( out->vectorCount >= v->vectorCount );
#if BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_NONE
	float *x = v->x;
	float *ox = out->x;
	for ( uint32_t i = 0; i < v->vectorCount; i++ )
		*(ox++) = *(x++) * multiply + add;
#elif BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_SSE
	__m128 mv  = _mm_set1_ps ( multiply ), av = _mm_set1_ps ( add );
	__m128 *x  = (__m128*)v->x;
	__m128 *ox = (__m128*)out->x;
	for ( uint32_t i = 0; i < (v->vectorCount+3)/4; i++ )
		*(ox++) = _mm_add_ps ( _mm_mul_ps ( *(x++), mv ), av );
#elif BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_AVX
#error Not implemented
#endif
}

void bbm_soa_vec4_mad_y ( bbm_soa_vec1* out, const bbm_soa_vec4* v, float multiply, float add )
{
	assert ( out->vectorCount >= v->vectorCount );
#if BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_NONE
	float *y = v->y;
	float *oy = out->x;
	for ( uint32_t i = 0; i < v->vectorCount; i++ )
		*(oy++) = *(y++) * multiply + add;
#elif BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_SSE
	__m128 mv  = _mm_set1_ps ( multiply ), av = _mm_set1_ps ( add );
	__m128 *y  = (__m128*)v->y;
	__m128 *oy = (__m128*)out->x;
	for ( uint32_t i = 0; i < (v->vectorCount+3)/4; i++ )
		*(oy++) = _mm_add_ps ( _mm_mul_ps ( *(y++), mv ), av );
#elif BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_AVX
#error Not implemented
#endif
}

void bbm_soa_vec4_mad_xy ( bbm_soa_vec2* out, const bbm_soa_vec4* v, float multiply, float add )
{
	assert ( out->vectorCount >= v->vectorCount );
#if BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_NONE
	float *x = v->x, *y = v->y;
	float *ox = out->x, *oy = out->y;
	for ( uint32_t i = 0; i < v->vectorCount; i++ )
		*(ox++) = *(x++) * multiply + add;
	for ( uint32_t i = 0; i < v->vectorCount; i++ )
		*(oy++) = *(y++) * multiply + add;
#elif BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_SSE
	__m128 mv  = _mm_set1_ps ( multiply ), av = _mm_set1_ps ( add );
	__m128 *x  = (__m128*)v->x,   *y  = (__m128*)v->y;
	__m128 *ox = (__m128*)out->x, *oy = (__m128*)out->y;
	for ( uint32_t i = 0; i < (v->vectorCount+3)/4; i++ )
		*(ox++) = _mm_add_ps ( _mm_mul_ps ( *(x++), mv ), av );
	for ( uint32_t i = 0; i < (v->vectorCount+3)/4; i++ )
		*(oy++) = _mm_add_ps ( _mm_mul_ps ( *(y++), mv ), av );
#elif BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_AVX
#error Not implemented
#endif
}

void bbm_soa_vec4_overwrite_mul_x_float ( bbm_soa_vec4* v, float f )
{
#if BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_NONE
	float *x = v->x;
	for ( uint32_t i = 0; i < v->vectorCount; i++, x++ )
		*(x) = *(x) * f;
#elif BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_SSE
	__m128 fv  = _mm_set1_ps ( f );
	__m128 *x  = (__m128*)v->x;
	for ( uint32_t i = 0; i < (v->vectorCount+3)/4; i++, x++ )
		*(x) = _mm_mul_ps ( *(x), fv );
#elif BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_AVX
#error Not implemented
#endif
}

void bbm_soa_vec4_overwrite_mul_y_float ( bbm_soa_vec4* v, float f )
{
#if BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_NONE
	float *y = v->y;
	for ( uint32_t i = 0; i < v->vectorCount; i++, y++ )
		*(y) = *(y) * f;
#elif BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_SSE
	__m128 fv  = _mm_set1_ps ( f );
	__m128 *y  = (__m128*)v->y;
	for ( uint32_t i = 0; i < (v->vectorCount+3)/4; i++, y++ )
		*(y) = _mm_mul_ps ( *(y), fv );
#elif BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_AVX
#error Not implemented
#endif
}

void bbm_soa_vec4_overwrite_mul_xy_float ( bbm_soa_vec4* v, float f )
{
#if BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_NONE
	float *x = v->x, *y = v->y;
	for ( uint32_t i = 0; i < v->vectorCount; i++, x++ )
		*(x) = *(x) * f;
	for ( uint32_t i = 0; i < v->vectorCount; i++, y++ )
		*(y) = *(y) * f;
#elif BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_SSE
	__m128 fv  = _mm_set1_ps ( f );
	__m128 *x  = (__m128*)v->x, *y  = (__m128*)v->y;
	for ( uint32_t i = 0; i < (v->vectorCount+3)/4; i++, x++ )
		*(x) = _mm_mul_ps ( *(x), fv );
	for ( uint32_t i = 0; i < (v->vectorCount+3)/4; i++, y++ )
		*(y) = _mm_mul_ps ( *(y), fv );
#elif BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_AVX
#error Not implemented
#endif
}

void bbm_soa_vec4_overwrite_mad_x ( bbm_soa_vec4* v, float multiply, float add )
{
#if BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_NONE
	float *x = v->x;
	for ( uint32_t i = 0; i < v->vectorCount; i++, x++ )
		*(x) = *(x) * multiply + add;
#elif BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_SSE
	__m128 mv  = _mm_set1_ps ( multiply ), av = _mm_set1_ps ( add );
	__m128 *x  = (__m128*)v->x;
	for ( uint32_t i = 0; i < (v->vectorCount+3)/4; i++, x++ )
		*(x) = _mm_add_ps ( _mm_mul_ps ( *(x), mv ), av );
#elif BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_AVX
#error Not implemented
#endif
}

void bbm_soa_vec4_overwrite_mad_y ( bbm_soa_vec4* v, float multiply, float add )
{
#if BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_NONE
	float *y = v->y;
	for ( uint32_t i = 0; i < v->vectorCount; i++, y++ )
		*(y) = *(y) * multiply + add;
#elif BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_SSE
	__m128 mv  = _mm_set1_ps ( multiply ), av = _mm_set1_ps ( add );
	__m128 *y  = (__m128*)v->y;
	for ( uint32_t i = 0; i < (v->vectorCount+3)/4; i++, y++ )
		*(y) = _mm_add_ps ( _mm_mul_ps ( *(y), mv ), av );
#elif BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_AVX
#error Not implemented
#endif
}

void bbm_soa_vec4_overwrite_mad_xy ( bbm_soa_vec4* v, float multiply, float add )
{
#if BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_NONE
	float *x = v->x, *y = v->y;
	for ( uint32_t i = 0; i < v->vectorCount; i++, x++ )
		*(x) = *(x) * multiply + add;
	for ( uint32_t i = 0; i < v->vectorCount; i++, y++ )
		*(y) = *(y) * multiply + add;
#elif BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_SSE
	__m128 mv  = _mm_set1_ps ( multiply ), av = _mm_set1_ps ( add );
	__m128 *x  = (__m128*)v->x, *y  = (__m128*)v->y;
	for ( uint32_t i = 0; i < (v->vectorCount+3)/4; i++, x++ )
		*(x) = _mm_add_ps ( _mm_mul_ps ( *(x), mv ), av );
	for ( uint32_t i = 0; i < (v->vectorCount+3)/4; i++, y++ )
		*(y) = _mm_add_ps ( _mm_mul_ps ( *(y), mv ), av );
#elif BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_AVX
#error Not implemented
#endif
}

void bbm_soa_vec4_div_xy_w ( bbm_soa_vec2* out, const bbm_soa_vec4* v )
{
	assert ( out->vectorCount >= v->vectorCount );
#if BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_NONE
	float *x = v->x, *y = v->y, *w = v->w;
	float *ox = out->x, *oy = out->y;
	for ( uint32_t i = 0; i < v->vectorCount; i++ )
		*(ox++) = *(x++) / *(w++);
	w = v->w;
	for ( uint32_t i = 0; i < v->vectorCount; i++ )
		*(oy++) = *(y++) / *(w++);
#elif BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_SSE
	__m128 *x  = (__m128*)v->x,   *y  = (__m128*)v->y, *w = (__m128*)v->w;
	__m128 *ox = (__m128*)out->x, *oy = (__m128*)out->y;
	for ( uint32_t i = 0; i < (v->vectorCount+3)/4; i++ )
		*(ox++) = _mm_div_ps ( *(x++), *(w++) );
	w = (__m128*)v->w;
	for ( uint32_t i = 0; i < (v->vectorCount+3)/4; i++ )
		*(oy++) = _mm_div_ps ( *(y++), *(w++) );
#elif BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_AVX
#error Not implemented
#endif
}

void bbm_soa_vec4_div_xyz_w ( bbm_soa_vec3* out, const bbm_soa_vec4* v )
{
	assert ( out->vectorCount >= v->vectorCount );
#if BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_NONE
	float *x = v->x, *y = v->y, *z = v->z, *w = v->w;
	float *ox = out->x, *oy = out->y, *oz = out->z;
	for ( uint32_t i = 0; i < v->vectorCount; i++ )
		*(ox++) = *(x++) / *(w++);
	w = v->w;
	for ( uint32_t i = 0; i < v->vectorCount; i++ )
		*(oy++) = *(y++) / *(w++);
	w = v->w;
	for ( uint32_t i = 0; i < v->vectorCount; i++ )
		*(oz++) = *(z++) / *(w++);
#elif BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_SSE
	__m128 *x  = (__m128*)v->x,   *y  = (__m128*)v->y,   *z  = (__m128*)v->z, *w = (__m128*)v->w;
	__m128 *ox = (__m128*)out->x, *oy = (__m128*)out->y, *oz = (__m128*)out->z;
	for ( uint32_t i = 0; i < v->vectorCount; i++ )
		*(ox++) = _mm_div_ps ( *(x++), *(w++) );
	w = (__m128*)v->w;
	for ( uint32_t i = 0; i < v->vectorCount; i++ )
		*(oy++) = _mm_div_ps ( *(y++), *(w++) );
	w = (__m128*)v->w;
	for ( uint32_t i = 0; i < v->vectorCount; i++ )
		*(oz++) = _mm_div_ps ( *(z++), *(w++) );
#elif BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_AVX
#error Not implemented
#endif
}

void bbm_soa_vec4_overwrite_mul_xy_w ( bbm_soa_vec4* v )
{
#if BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_NONE
	float *x = v->x, *y = v->y, *w = v->w;
	for ( uint32_t i = 0; i < v->vectorCount; i++, x++ )
		*(x) = *(x) * *(w++);
	w = v->w;
	for ( uint32_t i = 0; i < v->vectorCount; i++, y++ )
		*(y) = *(y) * *(w++);
#elif BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_SSE
	__m128 *x = (__m128*)v->x, *y = (__m128*)v->y, *w = (__m128*)v->w;
	for ( uint32_t i = 0; i < (v->vectorCount+3)/4; i++, x++ )
		*(x) = _mm_mul_ps ( *(x), *(w++) );
	w = (__m128*)v->w;
	for ( uint32_t i = 0; i < (v->vectorCount+3)/4; i++, y++ )
		*(y) = _mm_mul_ps ( *(y), *(w++) );
#elif BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_AVX
#error Not implemented
#endif
}

void bbm_soa_vec4_overwrite_mul_xyz_w ( bbm_soa_vec4* v )
{
#if BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_NONE
	float *x = v->x, *y = v->y, *z = v->z, *w = v->w;
	for ( uint32_t i = 0; i < v->vectorCount; i++, x++ )
		*(x) = *(x) * *(w++);
	w = v->w;
	for ( uint32_t i = 0; i < v->vectorCount; i++, y++ )
		*(y) = *(y) * *(w++);
	w = v->w;
	for ( uint32_t i = 0; i < v->vectorCount; i++, z++ )
		*(z) = *(z) * *(w++);
#elif BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_SSE
	__m128 *x = (__m128*)v->x, *y = (__m128*)v->y, *z = (__m128*)v->z, *w = (__m128*)v->w;
	for ( uint32_t i = 0; i < (v->vectorCount+3)/4; i++, x++ )
		*(x) = _mm_mul_ps ( *(x), *(w++) );
	w = (__m128*)v->w;
	for ( uint32_t i = 0; i < (v->vectorCount+3)/4; i++, y++ )
		*(y) = _mm_mul_ps ( *(y), *(w++) );
	w = (__m128*)v->w;
	for ( uint32_t i = 0; i < (v->vectorCount+3)/4; i++, z++ )
		*(z) = _mm_mul_ps ( *(z), *(w++) );
#elif BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_AVX
#error Not implemented
#endif
}

void bbm_soa_vec4_overwrite_div_xy_w ( bbm_soa_vec4* v )
{
#if BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_NONE
	float *x = v->x, *y = v->y, *w = v->w;
	for ( uint32_t i = 0; i < v->vectorCount; i++, x++ )
		*(x) = *(x) / *(w++);
	w = v->w;
	for ( uint32_t i = 0; i < v->vectorCount; i++, y++ )
		*(y) = *(y) / *(w++);
#elif BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_SSE
	__m128 *x = (__m128*)v->x, *y = (__m128*)v->y, *w = (__m128*)v->w;
	for ( uint32_t i = 0; i < (v->vectorCount+3)/4; i++, x++ )
		*(x) = _mm_div_ps ( *(x), *(w++) );
	w = (__m128*)v->w;
	for ( uint32_t i = 0; i < (v->vectorCount+3)/4; i++, y++ )
		*(y) = _mm_div_ps ( *(y), *(w++) );
#elif BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_AVX
#error Not implemented
#endif
}

void bbm_soa_vec4_overwrite_div_xyz_w ( bbm_soa_vec4* v )
{
#if BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_NONE
	float *x = v->x, *y = v->y, *z = v->z, *w = v->w;
	for ( uint32_t i = 0; i < v->vectorCount; i++, x++ )
		*(x) = *(x) / *(w++);
	w = v->w;
	for ( uint32_t i = 0; i < v->vectorCount; i++, y++ )
		*(y) = *(y) / *(w++);
	w = v->w;
	for ( uint32_t i = 0; i < v->vectorCount; i++, z++ )
		*(z) = *(z) / *(w++);
#elif BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_SSE
	__m128 *x = (__m128*)v->x, *y = (__m128*)v->y, *z = (__m128*)v->z, *w = (__m128*)v->w;
	for ( uint32_t i = 0; i < (v->vectorCount+3)/4; i++, x++ )
		*(x) = _mm_div_ps ( *(x), *(w++) );
	w = (__m128*)v->w;
	for ( uint32_t i = 0; i < (v->vectorCount+3)/4; i++, y++ )
		*(y) = _mm_div_ps ( *(y), *(w++) );
	w = (__m128*)v->w;
	for ( uint32_t i = 0; i < (v->vectorCount+3)/4; i++, z++ )
		*(z) = _mm_div_ps ( *(z), *(w++) );
#elif BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_AVX
#error Not implemented
#endif
}

void bbm_soa_vec4_overwrite_rcp_w ( bbm_soa_vec4* v )
{
#if BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_NONE
	float *w = v->w;
	for ( uint32_t i = 0; i < v->vectorCount; i++, w++ )
		*(w) = 1.0f / *(w);
#elif BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_SSE
	__m128 one = _mm_set1_ps ( 1.0f );
	__m128 *w = (__m128*)v->w;
	for ( uint32_t i = 0; i < (v->vectorCount+3)/4; i++, w++ )
		// _mm_rcp_ps has problems with precision!
		*(w) = _mm_div_ps ( one, *w ); ;//_mm_rcp_ps ( *(w) );
#elif BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_AVX
#error Not implemented
#endif
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////

float bbm_aos_mat4_max ( const bbm_aos_mat4* m )
{
#if BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_NONE
	float max = -FLT_MAX;
	const float* c = m->cells;
	for ( uint32_t i = 0; i < 16; i++, c++ )
	{
		if ( *c > max )
			max = *c;
	}
	return max;
#elif BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_SSE
	//1 6 5 2 | min: 1
	//8 2 5 1 | max: 8
	//7 4 3 8 |
	//1 5 6 3 |
	//
	//1 6 5 2     7 4 3 8
	//8 2 5 1     1 5 6 3 
	//------- m   ------- m
	//8 6 5 2     7 5 6 8
	//7 5 6 8 <-
	//------- m
	//8 6 6 8
	//8 6 6 8 s(wzyx)
	//------- m
	//8 6 6 8 // w == x, z == y
	//6 8 8 6 s(yxwz)
	//------- m
	//8 8 8 8 // x == y, x == z, x == w
	//
	//5 min/max, 2 shuffle
	float* c = (float*)m->cells;

	__m128 r0 = _mm_loadu_ps ( m->cells );
	__m128 r1 = _mm_loadu_ps ( m->cells + 4 );
	__m128 r2 = _mm_loadu_ps ( m->cells + 8 );
	__m128 r3 = _mm_loadu_ps ( m->cells + 12 );

	r0 = _mm_max_ps ( r0, r1 );
	r1 = _mm_max_ps ( r2, r3 );

	r0 = _mm_max_ps ( r0, r1 );
	r1 = _mm_shuffle_ps ( r0, r0, _MM_SHUFFLE(0,1,2,3) );

	r0 = _mm_max_ps ( r0, r1 );
	r1 = _mm_shuffle_ps ( r0, r0, _MM_SHUFFLE(2,3,0,1) );

	r0 = _mm_max_ps ( r0, r1 );
	return r0.m128_f32[0];
#elif BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_AVX
#error Not implemented
#endif
}

float bbm_aos_mat4_min ( const bbm_aos_mat4* m )
{
#if BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_NONE
	float min = FLT_MAX;
	const float* c = m->cells;
	for ( uint32_t i = 0; i < 16; i++, c++ )
	{
		if ( *c < min )
			min = *c;
	}
	return min;
#elif BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_SSE
	//1 6 5 2 | min: 1
	//8 2 5 1 | max: 8
	//7 4 3 8 |
	//1 5 6 3 |
	//
	//1 6 5 2     7 4 3 8
	//8 2 5 1     1 5 6 3 
	//------- m   ------- m
	//8 6 5 2     7 5 6 8
	//7 5 6 8 <-
	//------- m
	//8 6 6 8
	//8 6 6 8 s(wzyx)
	//------- m
	//8 6 6 8 // w == x, z == y
	//6 8 8 6 s(yxwz)
	//------- m
	//8 8 8 8 // x == y, x == z, x == w
	//
	//5 min/max, 2 shuffle
	float* c = (float*)m->cells;

	__m128 r0 = _mm_loadu_ps ( m->cells );
	__m128 r1 = _mm_loadu_ps ( m->cells + 4 );
	__m128 r2 = _mm_loadu_ps ( m->cells + 8 );
	__m128 r3 = _mm_loadu_ps ( m->cells + 12 );

	r0 = _mm_min_ps ( r0, r1 );
	r1 = _mm_min_ps ( r2, r3 );

	r0 = _mm_min_ps ( r0, r1 );
	r1 = _mm_shuffle_ps ( r0, r0, _MM_SHUFFLE(0,1,2,3) );

	r0 = _mm_min_ps ( r0, r1 );
	r1 = _mm_shuffle_ps ( r0, r0, _MM_SHUFFLE(2,3,0,1) );

	r0 = _mm_min_ps ( r0, r1 );
	return r0.m128_f32[0];
#elif BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_AVX
#error Not implemented
#endif
}

void bbm_soa_vec3_min_xyz ( bbm_aos_vec3* out, const bbm_soa_vec3* v )
{
#if BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_NONE
	for ( uint32_t cell = 0; cell < 3; cell++ )
	{
		float* co = &(out->cells[cell]);
		float* ci = v->cells[cell];
		*co = FLT_MAX;
		for ( uint32_t i = 0; i < v->vectorCount; i++, ci++ )
		{
			if ( *ci < *co )
				*co = *ci;
		}
	}
#elif BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_SSE
	const uint32_t passCount  = (v->vectorCount)/4;
	const uint32_t undefCount = v->vectorCount - 4 * passCount;
	const __m128 undefMask = _mm_cmplt_ps ( _mm_set_ps ( 3, 2, 1, 0 ), _mm_set1_ps ( undefCount - 0.1f ) );
	float* o = &(out->x);
	for ( uint32_t cell = 0; cell < 3; cell++, o++ )
	{
		__m128 min = _mm_set1_ps ( FLT_MAX );
		__m128* ci = (__m128*)v->cells[cell];
		
		for ( uint32_t i = 0; i < passCount; i++, ci++ )
		{
			min = _mm_min_ps ( min, *ci );
		}

		// If N is not aligned by 4, the (up to) 3 last floats for each cell can be undefined
		__m128 undef = _mm_or_ps ( _mm_and_ps ( undefMask, *ci ), _mm_andnot_ps ( undefMask, _mm_set1_ps ( FLT_MAX ) ) );
		min = _mm_min_ps ( min, undef );

		min = _mm_min_ps ( min, _mm_shuffle_ps ( min, min, _MM_SHUFFLE(0,1,2,3) ) );
		min = _mm_min_ps ( min, _mm_shuffle_ps ( min, min, _MM_SHUFFLE(2,3,0,1) ) );

		*o = min.m128_f32[0];
	}
#elif BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_AVX
#error Not implemented
#endif
}

void bbm_soa_vec3_max_xyz ( bbm_aos_vec3* out, const bbm_soa_vec3* v )
{
#if BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_NONE
	for ( uint32_t cell = 0; cell < 3; cell++ )
	{
		float* co = &(out->cells[cell]);
		float* ci = v->cells[cell];
		*co = -FLT_MAX;
		for ( uint32_t i = 0; i < v->vectorCount; i++, ci++ )
		{
			if ( *ci > *co )
				*co = *ci;
		}
	}
#elif BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_SSE
	const uint32_t passCount  = (v->vectorCount)/4;
	const uint32_t undefCount = v->vectorCount - 4 * passCount;
	const __m128 undefMask = _mm_cmplt_ps ( _mm_set_ps ( 3, 2, 1, 0 ), _mm_set1_ps ( undefCount - 0.1f ) );
	float* o = &(out->x);
	for ( uint32_t cell = 0; cell < 3; cell++, o++ )
	{
		__m128 max = _mm_set1_ps ( -FLT_MAX );
		__m128* ci = (__m128*)v->cells[cell];
		
		for ( uint32_t i = 0; i < passCount; i++, ci++ )
		{
			max = _mm_max_ps ( max, *ci );
		}

		// If N is not aligned by 4, the (up to) 3 last floats for each cell can be undefined
		__m128 undef = _mm_or_ps ( _mm_and_ps ( undefMask, *ci ), _mm_andnot_ps ( undefMask, _mm_set1_ps ( -FLT_MAX ) ) );
		max = _mm_max_ps ( max, undef );

		max = _mm_max_ps ( max, _mm_shuffle_ps ( max, max, _MM_SHUFFLE(0,1,2,3) ) );
		max = _mm_max_ps ( max, _mm_shuffle_ps ( max, max, _MM_SHUFFLE(2,3,0,1) ) );

		*o = max.m128_f32[0];
	}
#elif BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_AVX
#error Not implemented
#endif
}


void bbm_aos_mat4_mul_aos_mat4 ( bbm_aos_mat4* out, const bbm_aos_mat4* m1, const bbm_aos_mat4* m2 )
{
#define MUL_OPS(r,c) out->rows[r][c] = (m1->rows[0][c] * m2->rows[r][0] + m1->rows[1][c] * m2->rows[r][1] + m1->rows[2][c] * m2->rows[r][2] + m1->rows[3][c] * m2->rows[r][3])
	MUL_OPS ( 0, 0 ), MUL_OPS ( 0, 1 ), MUL_OPS ( 0, 2 ), MUL_OPS ( 0, 3 );
	MUL_OPS ( 1, 0 ), MUL_OPS ( 1, 1 ), MUL_OPS ( 1, 2 ), MUL_OPS ( 1, 3 );
	MUL_OPS ( 2, 0 ), MUL_OPS ( 2, 1 ), MUL_OPS ( 2, 2 ), MUL_OPS ( 2, 3 );
	MUL_OPS ( 3, 0 ), MUL_OPS ( 3, 1 ), MUL_OPS ( 3, 2 ), MUL_OPS ( 3, 3 );
#undef MUL_OPS
}

void bbm_aos_mat4_mul_aos_vec3w0 ( bbm_aos_vec3* out, const bbm_aos_mat4* m, const bbm_aos_vec3* v )
{
	float x = v->x, y = v->y, z = v->z;

	out->x = m->rows[0][0] * (x) + m->rows[1][0] * (y) + m->rows[2][0] * (z);
	out->y = m->rows[0][1] * (x) + m->rows[1][1] * (y) + m->rows[2][1] * (z);
	out->z = m->rows[0][2] * (x) + m->rows[1][2] * (y) + m->rows[2][2] * (z);
}

void bbm_aos_mat4_mul_aos_vec3w1 ( bbm_aos_vec3* out, const bbm_aos_mat4* m, const bbm_aos_vec3* v )
{
	float x = v->x, y = v->y, z = v->z;
	
	out->x = m->rows[0][0] * (x) + m->rows[1][0] * (y) + m->rows[2][0] * (z) + m->rows[3][0];
	out->y = m->rows[0][1] * (x) + m->rows[1][1] * (y) + m->rows[2][1] * (z) + m->rows[3][1];
	out->z = m->rows[0][2] * (x) + m->rows[1][2] * (y) + m->rows[2][2] * (z) + m->rows[3][2];
}

void bbm_aos_mat4_mul_aos_vec3w1_out_vec4 ( bbm_aos_vec4* out, const bbm_aos_mat4* m, const bbm_aos_vec3* v )
{
	float x = v->x, y = v->y, z = v->z;
	
	out->x = m->rows[0][0] * (x) + m->rows[1][0] * (y) + m->rows[2][0] * (z) + m->rows[3][0];
	out->y = m->rows[0][1] * (x) + m->rows[1][1] * (y) + m->rows[2][1] * (z) + m->rows[3][1];
	out->z = m->rows[0][2] * (x) + m->rows[1][2] * (y) + m->rows[2][2] * (z) + m->rows[3][2];
	out->w = m->rows[0][3] * (x) + m->rows[1][3] * (y) + m->rows[2][3] * (z) + m->rows[3][3];
}

void bbm_aos_mat4_mul_aos_vec4 ( bbm_aos_vec4* out, const bbm_aos_mat4* m, const bbm_aos_vec4* v )
{
	float x = v->x, y = v->y, z = v->z, w = v->w;
	
	out->x = m->rows[0][0] * (x) + m->rows[1][0] * (y) + m->rows[2][0] * (z) + m->rows[3][0] * (w);
	out->y = m->rows[0][1] * (x) + m->rows[1][1] * (y) + m->rows[2][1] * (z) + m->rows[3][1] * (w);
	out->z = m->rows[0][2] * (x) + m->rows[1][2] * (y) + m->rows[2][2] * (z) + m->rows[3][2] * (w);
	out->w = m->rows[0][3] * (x) + m->rows[1][3] * (y) + m->rows[2][3] * (z) + m->rows[3][3] * (w);
}


void bbm_aos_mat4_mul_soa_mat4 ( bbm_soa_mat4* out, const bbm_aos_mat4* m1, const bbm_soa_mat4* m2 )
{
#if BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_NONE
	assert ( out->matrixCount == m2->matrixCount );
	float celldata1[4], *cellptr2[4], *outptr;

	for ( uint32_t r = 0; r < 4; r++ )
	{
		for ( uint32_t c = 0; c < 4; c++ )
		{
			outptr = out->rows[r][c];
			for ( uint32_t i = 0; i < 4; i++ )
				celldata1[i] = m1->rows[i][c];
			for ( uint32_t i = 0; i < 4; i++ )
				cellptr2[i] = m2->rows[r][i];
			for ( uint32_t i = 0; i < m2->matrixCount; i++ )
				*(outptr++) = (celldata1[0]) * *(cellptr2[0]++) + (celldata1[1]) * *(cellptr2[1]++) + (celldata1[2]) * *(cellptr2[2]++) + (celldata1[3]) * *(cellptr2[3]++);
		}
	}
#elif BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_SSE
	assert ( out->matrixCount == m2->matrixCount );

	__m128 celldata1[4], *cellptr2[4], *outptr;

	for ( uint32_t r = 0; r < 4; r++ )
	{
		for ( uint32_t c = 0; c < 4; c++ )
		{
			outptr = (__m128*)( out->rows[r][c] );
			for ( uint32_t i = 0; i < 4; i++ )
				celldata1[i] = _mm_set1_ps ( m1->rows[i][c] );
			for ( uint32_t i = 0; i < 4; i++ )
				cellptr2[i] = (__m128*) ( m2->rows[r][i] );
			for ( uint32_t i = 0; i < (m2->matrixCount + 3) / 4; i++ )
				*(outptr++) = _mm_add_ps ( _mm_add_ps ( _mm_mul_ps ( (celldata1[0]), *(cellptr2[0]++) ), _mm_mul_ps ( (celldata1[1]), *(cellptr2[1]++) ) ), _mm_add_ps ( _mm_mul_ps ( (celldata1[2]), *(cellptr2[2]++) ), _mm_mul_ps ( (celldata1[3]), *(cellptr2[3]++) ) ) );
		}
	}
#elif BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_AVX
#error Not implemented
#endif
}

void bbm_aos_mat4_mul_soa_vec3w0 ( bbm_soa_vec3* out, const bbm_aos_mat4* m, const bbm_soa_vec3* v )
{
	assert ( out->vectorCount >= v->vectorCount );
#if BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_NONE
	for ( uint32_t cell = 0; cell < 3; cell++ )
	{
		float *ix = v->cells[0], *iy = v->cells[1], *iz = v->cells[2];
		float* o = out->cells[cell];
		float md[3] = { m->rows[0][cell], m->rows[1][cell], m->rows[2][cell] };
		for ( uint32_t vec = 0; vec < v->vectorCount; vec++, ix++, iy++, iz++, o++ )
			*o = ( md[0] * *ix ) + ( ( md[1] * *iy ) + ( md[2] * *iz ) );
	}
#elif BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_SSE
	for ( uint32_t cell = 0; cell < 3; cell++ )
	{
		__m128 *ix = (__m128*)v->cells[0], *iy = (__m128*)v->cells[1], *iz = (__m128*)v->cells[2];
		__m128* o = (__m128*)out->cells[cell];
		__m128 md[3] = { _mm_set1_ps ( m->rows[0][cell] ), _mm_set1_ps ( m->rows[1][cell] ), _mm_set1_ps ( m->rows[2][cell] ) };
		for ( uint32_t vec = 0; vec < (v->vectorCount+3)/4; vec++, ix++, iy++, iz++, o++ )
			*o = _mm_add_ps ( _mm_mul_ps ( md[0], *ix ), _mm_add_ps ( _mm_mul_ps ( md[1], *iy ), _mm_mul_ps ( md[2], *iz ) ) );
	}
#elif BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_AVX
	#error Not implemented
#endif
}

void bbm_aos_mat4_mul_soa_vec3w1 ( bbm_soa_vec3* out, const bbm_aos_mat4* m, const bbm_soa_vec3* v )
{
	assert ( out->vectorCount >= v->vectorCount );
#if BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_NONE
	for ( uint32_t cell = 0; cell < 3; cell++ )
	{
		float *ix = v->cells[0], *iy = v->cells[1], *iz = v->cells[2];
		float* o = out->cells[cell];
		float md[4] = { m->rows[0][cell], m->rows[1][cell], m->rows[2][cell], m->rows[3][cell] };
		for ( uint32_t vec = 0; vec < v->vectorCount; vec++, ix++, iy++, iz++, o++ )
			*o = ( ( md[0] * *ix ) + ( md[1] * *iy ) ) + ( ( md[2] * *iz ) + md[3] );
	}
#elif BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_SSE
	for ( uint32_t cell = 0; cell < 3; cell++ )
	{
		__m128 *ix = (__m128*)v->cells[0], *iy = (__m128*)v->cells[1], *iz = (__m128*)v->cells[2];
		__m128* o = (__m128*)out->cells[cell];
		__m128 md[4] = { _mm_set1_ps ( m->rows[0][cell] ), _mm_set1_ps ( m->rows[1][cell] ), _mm_set1_ps ( m->rows[2][cell] ), _mm_set1_ps ( m->rows[3][cell] ) };
		for ( uint32_t vec = 0; vec < (v->vectorCount+3)/4; vec++, ix++, iy++, iz++, o++ )
			*o = _mm_add_ps ( _mm_add_ps ( _mm_mul_ps ( md[0], *ix ), _mm_mul_ps ( md[1], *iy ) ), _mm_add_ps ( _mm_mul_ps ( md[2], *iz ), md[3] ) );
	}
#elif BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_AVX
	#error Not implemented
#endif
}

void bbm_aos_mat4_mul_soa_vec3w1_out_vec4 ( bbm_soa_vec4* out, const bbm_aos_mat4* m, const bbm_soa_vec3* v )
{
	assert ( out->vectorCount >= v->vectorCount );
#if BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_NONE
	for ( uint32_t cell = 0; cell < 4; cell++ )
	{
		float *ix = v->cells[0], *iy = v->cells[1], *iz = v->cells[2];
		float* o = out->cells[cell];
		float md[4] = { m->rows[0][cell], m->rows[1][cell], m->rows[2][cell], m->rows[3][cell] };
		for ( uint32_t vec = 0; vec < v->vectorCount; vec++, ix++, iy++, iz++, o++ )
			*o = ( ( md[0] * *ix ) + ( md[1] * *iy ) ) + ( ( md[2] * *iz ) + md[3] );
	}
#elif BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_SSE
	for ( uint32_t cell = 0; cell < 4; cell++ )
	{
		__m128 *ix = (__m128*)v->cells[0], *iy = (__m128*)v->cells[1], *iz = (__m128*)v->cells[2];
		__m128* o = (__m128*)out->cells[cell];
		__m128 md[4] = { _mm_set1_ps ( m->rows[0][cell] ), _mm_set1_ps ( m->rows[1][cell] ), _mm_set1_ps ( m->rows[2][cell] ), _mm_set1_ps ( m->rows[3][cell] ) };
		for ( uint32_t vec = 0; vec < (v->vectorCount+3)/4; vec++, ix++, iy++, iz++, o++ )
			*o = _mm_add_ps ( _mm_add_ps ( _mm_mul_ps ( md[0], *ix ), _mm_mul_ps ( md[1], *iy ) ), _mm_add_ps ( _mm_mul_ps ( md[2], *iz ), md[3] ) );
	}
#elif BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_AVX
	#error Not implemented
#endif
}

void bbm_aos_mat4_mul_soa_vec4 ( bbm_soa_vec4* out, const bbm_aos_mat4* m, const bbm_soa_vec4* v )
{
	assert ( out->vectorCount >= v->vectorCount );
#if BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_NONE
	float* xptr = v->x, *yptr = v->y, *zptr = v->z, *wptr = v->w;
	float* xptro = out->x, *yptro = out->y, *zptro = out->z, *wptro = out->w;
	for ( uint32_t i = 0; i < v->vectorCount; i++, xptr++, yptr++, zptr++, wptr++, xptro++, yptro++, zptro++, wptro++ )
	{
		*xptro = m->rows[0][0] * (*xptr) + m->rows[1][0] * (*yptr) + m->rows[2][0] * (*zptr) + m->rows[3][0] * (*wptr);
		*yptro = m->rows[0][1] * (*xptr) + m->rows[1][1] * (*yptr) + m->rows[2][1] * (*zptr) + m->rows[3][1] * (*wptr);
		*zptro = m->rows[0][2] * (*xptr) + m->rows[1][2] * (*yptr) + m->rows[2][2] * (*zptr) + m->rows[3][2] * (*wptr);
		*wptro = m->rows[0][3] * (*xptr) + m->rows[1][3] * (*yptr) + m->rows[2][3] * (*zptr) + m->rows[3][3] * (*wptr);
	}
#elif BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_SSE
	for ( uint32_t cell = 0; cell < 4; cell++ )
	{
		__m128 *ix = (__m128*)v->cells[0], *iy = (__m128*)v->cells[1], *iz = (__m128*)v->cells[2], *iw = (__m128*)v->cells[3];
		__m128* o = (__m128*)out->cells[cell];
		__m128 md[4] = { _mm_set1_ps ( m->rows[0][cell] ), _mm_set1_ps ( m->rows[1][cell] ), _mm_set1_ps ( m->rows[2][cell] ), _mm_set1_ps ( m->rows[3][cell] ) };
		for ( uint32_t vec = 0; vec < (v->vectorCount+3)/4; vec++, ix++, iy++, iz++, iw++, o++ )
			*o = _mm_add_ps ( _mm_add_ps ( _mm_mul_ps ( md[0], *ix ), _mm_mul_ps ( md[1], *iy ) ), _mm_add_ps ( _mm_mul_ps ( md[2], *iz ), _mm_mul_ps ( md[3], *iw ) ) );
	}
#elif BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_AVX
	#error Not implemented
#endif
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////

// http://en.wikipedia.org/wiki/Laplace_expansion
float bbm_aos_mat4_determinant ( const bbm_aos_mat4* m )
{
	return    (m->cells[ 0] * m->cells[ 5] - m->cells[ 1] * m->cells[ 4]) * (m->cells[10] * m->cells[15] - m->cells[11] * m->cells[14])
			- (m->cells[ 0] * m->cells[ 6] - m->cells[ 2] * m->cells[ 4]) * (m->cells[ 9] * m->cells[15] - m->cells[11] * m->cells[13])
			+ (m->cells[ 0] * m->cells[ 7] - m->cells[ 3] * m->cells[ 4]) * (m->cells[ 9] * m->cells[14] - m->cells[10] * m->cells[13])
			+ (m->cells[ 1] * m->cells[ 6] - m->cells[ 2] * m->cells[ 5]) * (m->cells[ 8] * m->cells[15] - m->cells[11] * m->cells[12])
			- (m->cells[ 1] * m->cells[ 7] - m->cells[ 3] * m->cells[ 5]) * (m->cells[ 8] * m->cells[14] - m->cells[10] * m->cells[12])
			+ (m->cells[ 2] * m->cells[ 7] - m->cells[ 3] * m->cells[ 6]) * (m->cells[ 8] * m->cells[13] - m->cells[ 9] * m->cells[12]);
}

// https://www.youtube.com/watch?v=nNOz6Ez8Fn4
void bbm_aos_mat4_inverse ( bbm_aos_mat4* out, const bbm_aos_mat4* m )
{
	float detFactor = 1.0f / bbm_aos_mat4_determinant ( m );

#define DET3(m,c00,c01,c02,c10,c11,c12,c20,c21,c22)			\
		( (m->cells[c00] * m->cells[c11] * m->cells[c22])	\
		+ (m->cells[c01] * m->cells[c12] * m->cells[c20])	\
		+ (m->cells[c02] * m->cells[c10] * m->cells[c21])	\
		- (m->cells[c00] * m->cells[c12] * m->cells[c21])	\
		- (m->cells[c01] * m->cells[c10] * m->cells[c22])	\
		- (m->cells[c02] * m->cells[c11] * m->cells[c20]))

	//  0,  1,  2,  3
	//  4,  5,  6,  7
	//  8,  9, 10, 11
	// 12, 13, 14, 15

	// Cofactors matrix
	// -----------------------------------------------------
	// |  5,  6,  7 |  4,  6,  7 |  4,  5,  7 |  4,  5,  6 |
	// |  9, 10, 11 |  8, 10, 11 |  8,  9, 11 |  8,  9, 10 |
	// | 13, 14, 15 | 12, 14, 15 | 12, 13, 15 | 12, 13, 14 |
	// -----------------------------------------------------
	// |  1,  2,  3 |  0,  2,  3 |  0,  1,  3 |  0,  1,  2 |
	// |  9, 10, 11 |  8, 10, 11 |  8,  9, 11 |  8,  9, 10 |
	// | 13, 14, 15 | 12, 14, 15 | 12, 13, 15 | 12, 13, 14 |
	// -----------------------------------------------------
	// |  1,  2,  3 |  0,  2,  3 |  0,  1,  3 |  0,  1,  2 |
	// |  5,  6,  7 |  4,  6,  7 |  4,  5,  7 |  4,  5,  6 |
	// | 13, 14, 15 | 12, 14, 15 | 12, 13, 15 | 12, 13, 14 |
	// -----------------------------------------------------
	// |  1,  2,  3 |  0,  2,  3 |  0,  1,  3 |  0,  1,  2 |
	// |  5,  6,  7 |  4,  6,  7 |  4,  5,  7 |  4,  5,  6 |
	// |  9, 10, 11 |  8, 10, 11 |  8,  9, 11 |  8,  9, 10 |
	// -----------------------------------------------------

	// Cofactors transposed
	// -----------------------------------------------------
	// |  5,  6,  7 |  1,  2,  3 |  1,  2,  3 |  1,  2,  3 |
	// |  9, 10, 11 |  9, 10, 11 |  5,  6,  7 |  5,  6,  7 |
	// | 13, 14, 15 | 13, 14, 15 | 13, 14, 15 |  9, 10, 11 |
	// -----------------------------------------------------
	// |  4,  6,  7 |  0,  2,  3 |  0,  2,  3 |  0,  2,  3 |
	// |  8, 10, 11 |  8, 10, 11 |  4,  6,  7 |  4,  6,  7 |
	// | 12, 14, 15 | 12, 14, 15 | 12, 14, 15 |  8, 10, 11 |
	// -----------------------------------------------------
	// |  4,  5,  7 |  0,  1,  3 |  0,  1,  3 |  0,  1,  3 |
	// |  8,  9, 11 |  8,  9, 11 |  4,  5,  7 |  4,  5,  7 |
	// | 12, 13, 15 | 12, 13, 15 | 12, 13, 15 |  8,  9, 11 |
	// -----------------------------------------------------
	// |  4,  5,  6 |  0,  1,  2 |  0,  1,  2 |  0,  1,  2 |
	// |  8,  9, 10 |  8,  9, 10 |  4,  5,  6 |  4,  5,  6 |
	// | 12, 13, 14 | 12, 13, 14 | 12, 13, 14 |  8,  9, 10 |
	// -----------------------------------------------------
			
			
	// -----------------------------------------------------
	// |  5,  6,  7 |  1,  2,  3 |  1,  2,  3 |  1,  2,  3 |
	// |  9, 10, 11 |  9, 10, 11 |  5,  6,  7 |  5,  6,  7 |
	// | 13, 14, 15 | 13, 14, 15 | 13, 14, 15 |  9, 10, 11 |
	// -----------------------------------------------------
	out->rows[0][0] =  detFactor * DET3(m,  5,  6,  7,
								            9, 10, 11,
								            13, 14, 15 );
	out->rows[0][1] = -detFactor * DET3(m,  1,  2,  3,
								            9, 10, 11,
								            13, 14, 15 );
	out->rows[0][2] =  detFactor * DET3(m,  1,  2,  3,
								            5,  6,  7,
								            13, 14, 15 );
	out->rows[0][3] = -detFactor * DET3(m,  1,  2,  3,
								            5,  6,  7,
								            9, 10, 11 );

	// -----------------------------------------------------
	// |  4,  6,  7 |  0,  2,  3 |  0,  2,  3 |  0,  2,  3 |
	// |  8, 10, 11 |  8, 10, 11 |  4,  6,  7 |  4,  6,  7 |
	// | 12, 14, 15 | 12, 14, 15 | 12, 14, 15 |  8, 10, 11 |
	// -----------------------------------------------------
	out->rows[1][0] = -detFactor * DET3(m,  4,  6,  7,
								            8, 10, 11,
								            12, 14, 15 );
	out->rows[1][1] =  detFactor * DET3(m,  0,  2,  3,
								            8, 10, 11,
								            12, 14, 15 );
	out->rows[1][2] = -detFactor * DET3(m,  0,  2,  3,
								            4,  6,  7,
								            12, 14, 15 );
	out->rows[1][3] =  detFactor * DET3(m,  0,  2,  3,
								            4,  6,  7,
								            8, 10, 11 );

	// -----------------------------------------------------
	// |  4,  5,  7 |  0,  1,  3 |  0,  1,  3 |  0,  1,  3 |
	// |  8,  9, 11 |  8,  9, 11 |  4,  5,  7 |  4,  5,  7 |
	// | 12, 13, 15 | 12, 13, 15 | 12, 13, 15 |  8,  9, 11 |
	// -----------------------------------------------------
	out->rows[2][0] =  detFactor * DET3(m,  4,  5,  7,
								            8,  9, 11,
								            12, 13, 15 );
	out->rows[2][1] = -detFactor * DET3(m,  0,  1,  3,
								            8,  9, 11,
								            12, 13, 15 );
	out->rows[2][2] =  detFactor * DET3(m,  0,  1,  3,
								            4,  5,  7,
								            12, 13, 15 );
	out->rows[2][3] = -detFactor * DET3(m,  0,  1,  3,
								            4,  5,  7,
								            8,  9, 11 );

	// -----------------------------------------------------
	// |  4,  5,  6 |  0,  1,  2 |  0,  1,  2 |  0,  1,  2 |
	// |  8,  9, 10 |  8,  9, 10 |  4,  5,  6 |  4,  5,  6 |
	// | 12, 13, 14 | 12, 13, 14 | 12, 13, 14 |  8,  9, 10 |
	// -----------------------------------------------------
	out->rows[3][0] = -detFactor * DET3(m,  4,  5,  6,
								            8,  9, 10,
								            12, 13, 14 );
	out->rows[3][1] =  detFactor * DET3(m,  0,  1,  2,
								            8,  9, 10,
								            12, 13, 14 );
	out->rows[3][2] = -detFactor * DET3(m,  0,  1,  2,
								            4,  5,  6,
								            12, 13, 14 );
	out->rows[3][3] =  detFactor * DET3(m,  0,  1,  2,
								            4,  5,  6,
								            8,  9, 10 );
#undef DET3
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////