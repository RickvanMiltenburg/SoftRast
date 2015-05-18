// The MIT License (MIT)
// 
// Copyright (c) 2015 Rick van Miltenburg
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

#ifndef __MILTYALLOC_STACK_ALLOCATOR_H
#define __MILTYALLOC_STACK_ALLOCATOR_H

#ifdef __cplusplus
	extern "C" {
#endif

#include "defines.h"

#include <stddef.h>
#include <stdint.h>

typedef enum
{
	MILTYALLOC_SUCCESS = 0,

	MILTYALLOC_ERROR_FREE_OUT_OF_ORDER,
	MILTYALLOC_ERROR_FREE_MISALIGNED,
	MILTYALLOC_ERROR_FREE_OUT_OF_BUFFER,
	MILTYALLOC_ERROR_NOT_ALLOCATED,
} miltyalloc_error;

typedef struct
{
	uintptr_t start;
	uintptr_t end;
	uintptr_t current;
} linear_allocator;

typedef struct
{
	uintptr_t startPtr;
	uintptr_t endPtr;
	uintptr_t curLeft;
	uintptr_t curRight;
} double_ended_linear_allocator;

typedef struct
{
	uintptr_t bufferUserPtr;
	uintptr_t bufferStart;
} stack_allocator_bookkeeping_entry;

typedef struct
{
	uintptr_t start;
	uintptr_t end;
	uintptr_t current;

	stack_allocator_bookkeeping_entry* bookkeepingBuffer;
	size_t bookkeepingCurEntry;
	size_t bookkeepingEntryCount;
} stack_allocator;

typedef struct
{
	uintptr_t startPtr;
	uintptr_t endPtr;
	uintptr_t curLeft;
	uintptr_t curRight;

	stack_allocator_bookkeeping_entry* bookkeepingBuffer;
	size_t bookkeepingCurLeftEntry;
	size_t bookkeepingCurRightEntry;
	size_t bookkeepingEntryCount;
} double_ended_stack_allocator;

typedef struct
{
	uintptr_t startPtr;
	uint32_t* poolBookkeeping;

	uint32_t poolSize;
	uint32_t poolCount;
	uint32_t poolPtr;
} pool_allocator;

typedef struct
{
	uint32_t count;
	uint32_t* index;
} buddy_allocator_size_bucket;

typedef struct
{
	uintptr_t startPtr;
	buddy_allocator_size_bucket* sizeBuckets;
	uint32_t* pageData;

	uint32_t minBucketSize;
	uint32_t sizeLevels;
} buddy_allocator;

// Linear allocator
MILTYALLOC_API void  miltyalloc_linear_allocator_initialize ( linear_allocator* allocator, void* memory, size_t memorySizeInBytes );
MILTYALLOC_API void  miltyalloc_linear_allocator_reset ( linear_allocator* allocator );
MILTYALLOC_API void* miltyalloc_linear_allocator_alloc ( linear_allocator* allocator, size_t size );
MILTYALLOC_API void* miltyalloc_linear_allocator_alloc_aligned_pow_two ( linear_allocator* allocator, size_t size, size_t align );

// Double-Ended Linear allocator
MILTYALLOC_API void  miltyalloc_double_ended_linear_allocator_initialize ( double_ended_linear_allocator* allocator, void* memory, size_t memorySizeInBytes );
MILTYALLOC_API void  miltyalloc_double_ended_linear_allocator_reset_left ( double_ended_linear_allocator* allocator );
MILTYALLOC_API void  miltyalloc_double_ended_linear_allocator_reset_right ( double_ended_linear_allocator* allocator );
MILTYALLOC_API void* miltyalloc_double_ended_linear_allocator_alloc_left ( double_ended_linear_allocator* allocator, size_t size );
MILTYALLOC_API void* miltyalloc_double_ended_linear_allocator_alloc_right ( double_ended_linear_allocator* allocator, size_t size );
MILTYALLOC_API void* miltyalloc_double_ended_linear_allocator_alloc_left_aligned_pow_two ( double_ended_linear_allocator* allocator, size_t size, size_t align );
MILTYALLOC_API void* miltyalloc_double_ended_linear_allocator_alloc_right_aligned_pow_two ( double_ended_linear_allocator* allocator, size_t size, size_t align );

// Stack allocator
MILTYALLOC_API void  miltyalloc_stack_allocator_initialize ( stack_allocator* allocator, void* memory, size_t memorySizeInBytes, stack_allocator_bookkeeping_entry* bookkeepingBuffer, size_t bookkeepingEntryCount );
MILTYALLOC_API void  miltyalloc_stack_allocator_reset ( stack_allocator* allocator );
MILTYALLOC_API miltyalloc_error miltyalloc_stack_allocator_free ( stack_allocator* allocator, void* data );
MILTYALLOC_API void* miltyalloc_stack_allocator_alloc ( stack_allocator* allocator, size_t size );
MILTYALLOC_API void* miltyalloc_stack_allocator_alloc_aligned_pow_two ( stack_allocator* allocator, size_t size, size_t align );

// Double-Ended Stack allocator
MILTYALLOC_API void  miltyalloc_double_ended_stack_allocator_initialize ( double_ended_stack_allocator* allocator, void* memory, size_t memorySizeInBytes, stack_allocator_bookkeeping_entry* bookkeepingBuffer, size_t bookkeepingEntryCount );
MILTYALLOC_API void  miltyalloc_double_ended_stack_allocator_reset_left ( double_ended_stack_allocator* allocator );
MILTYALLOC_API void  miltyalloc_double_ended_stack_allocator_reset_right ( double_ended_stack_allocator* allocator );
MILTYALLOC_API miltyalloc_error miltyalloc_double_ended_stack_allocator_free_left ( double_ended_stack_allocator* allocator, void* data );
MILTYALLOC_API miltyalloc_error miltyalloc_double_ended_stack_allocator_free_right ( double_ended_stack_allocator* allocator, void* data );
MILTYALLOC_API void* miltyalloc_double_ended_stack_allocator_alloc_left ( double_ended_stack_allocator* allocator, size_t size );
MILTYALLOC_API void* miltyalloc_double_ended_stack_allocator_alloc_right ( double_ended_stack_allocator* allocator, size_t size );
MILTYALLOC_API void* miltyalloc_double_ended_stack_allocator_alloc_left_aligned_pow_two ( double_ended_stack_allocator* allocator, size_t size, size_t align );
MILTYALLOC_API void* miltyalloc_double_ended_stack_allocator_alloc_right_aligned_pow_two ( double_ended_stack_allocator* allocator, size_t size, size_t align );

// Pool allocator
MILTYALLOC_API void  miltyalloc_pool_allocator_initialize ( pool_allocator* allocator, void* memory, uint32_t* poolBookkeepingBuffer, uint32_t poolSize, uint32_t poolCount );
MILTYALLOC_API miltyalloc_error miltyalloc_pool_allocator_free ( pool_allocator* allocator, void* memory );
MILTYALLOC_API void* miltyalloc_pool_allocator_alloc ( pool_allocator* allocator );

// Buddy system allocator
MILTYALLOC_API size_t miltyalloc_buddy_allocator_calculate_bookkeeping_size ( size_t bufferSize, size_t minSize );
MILTYALLOC_API void  miltyalloc_buddy_allocator_initialize ( buddy_allocator* allocator, void* memory, size_t size, size_t minSize, void* bookkeepingBuffer );
MILTYALLOC_API void* miltyalloc_buddy_allocator_alloc ( buddy_allocator* allocator, size_t size );
MILTYALLOC_API miltyalloc_error miltyalloc_buddy_allocator_free ( buddy_allocator* allocator, void* memory );

#ifdef __cplusplus
	};
#endif

#endif