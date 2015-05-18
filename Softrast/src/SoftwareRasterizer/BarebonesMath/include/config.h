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

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Options

#define BAREBONES_MATH_VECTOR_INSTR_SET_NONE 0
#define BAREBONES_MATH_VECTOR_INSTR_SET_SSE  1
#define BAREBONES_MATH_VECTOR_INSTR_SET_AVX  2

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// User config

#define BAREBONES_MATH_VECTOR_INSTR_SET BAREBONES_MATH_VECTOR_INSTR_SET_NONE

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// End of user config

#if BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_SSE
	#define BAREBONES_MATH_VECTOR_WIDTH 4
#elif BAREBONES_MATH_VECTOR_INSTR_SET == BAREBONES_MATH_VECTOR_INSTR_SET_AVX
	#define BAREBONES_MATH_VECTOR_WIDTH 8
#else
	#define BAREBONES_MATH_VECTOR_WIDTH 1
#endif