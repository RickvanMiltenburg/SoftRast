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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "../include/miltyalloc.h"
#include "util.h"
#include <math.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

MILTYALLOC_API void miltyalloc_linear_allocator_initialize ( linear_allocator* allocator, void* memory, size_t memorySizeInBytes )
{
	allocator->current = allocator->start = (uintptr_t)memory;
	allocator->end     = allocator->start + memorySizeInBytes;
}

MILTYALLOC_API void miltyalloc_linear_allocator_reset ( linear_allocator* allocator )
{
	allocator->current = allocator->start;
}

MILTYALLOC_API void* miltyalloc_linear_allocator_alloc ( linear_allocator* allocator, size_t size )
{
	uintptr_t newPtr, ret;

	newPtr = allocator->current + size;

	if ( newPtr > allocator->end )
		return NULL;

	ret                = allocator->current;
	allocator->current = newPtr;

	return (void*)ret;
}

MILTYALLOC_API void* miltyalloc_linear_allocator_alloc_aligned_pow_two ( linear_allocator* allocator, size_t size, size_t align )
{
	uintptr_t alignedRet;

	MILTYALLOC_ASSERT ( MILTYALLOC_IS_POWER_OF_TWO ( align ) );

	alignedRet = MILTYALLOC_ALIGN_UP_POW_2 ( allocator->current, align );
	
	if ( alignedRet + size > allocator->end )
		return NULL;

	allocator->current = alignedRet + size;

	return (void*)alignedRet;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

MILTYALLOC_API void miltyalloc_double_ended_linear_allocator_initialize ( double_ended_linear_allocator* allocator, void* memory, size_t memorySizeInBytes )
{
	allocator->curLeft  = allocator->startPtr = (uintptr_t)memory;
	allocator->curRight = allocator->endPtr   = allocator->startPtr + memorySizeInBytes;
}

MILTYALLOC_API void miltyalloc_double_ended_linear_allocator_reset_left ( double_ended_linear_allocator* allocator )
{
	allocator->curLeft = allocator->startPtr;
}

MILTYALLOC_API void miltyalloc_double_ended_linear_allocator_reset_right ( double_ended_linear_allocator* allocator )
{
	allocator->curRight = allocator->endPtr;
}

MILTYALLOC_API void* miltyalloc_double_ended_linear_allocator_alloc_left ( double_ended_linear_allocator* allocator, size_t size )
{
	uintptr_t newPtr, ret;

	newPtr = allocator->curLeft + size;

	if ( newPtr > allocator->curRight )
		return NULL;

	ret                = allocator->curLeft;
	allocator->curLeft = newPtr;

	return (void*)ret;
}

MILTYALLOC_API void* miltyalloc_double_ended_linear_allocator_alloc_right ( double_ended_linear_allocator* allocator, size_t size )
{
	uintptr_t ret;

	ret = allocator->curRight - size;

	if ( ret < allocator->curLeft )
		return NULL;

	allocator->curRight = ret;

	return (void*)ret;
}

MILTYALLOC_API void* miltyalloc_double_ended_linear_allocator_alloc_left_aligned_pow_two ( double_ended_linear_allocator* allocator, size_t size, size_t align )
{
	uintptr_t alignedRet;

	MILTYALLOC_ASSERT ( MILTYALLOC_IS_POWER_OF_TWO ( align ) );

	alignedRet = MILTYALLOC_ALIGN_UP_POW_2 ( allocator->curLeft, align );
	
	if ( alignedRet + size > allocator->curRight )
		return NULL;

	allocator->curLeft = alignedRet + size;

	return (void*)alignedRet;
}

MILTYALLOC_API void* miltyalloc_double_ended_linear_allocator_alloc_right_aligned_pow_two ( double_ended_linear_allocator* allocator, size_t size, size_t align )
{
	uintptr_t ret;

	MILTYALLOC_ASSERT ( MILTYALLOC_IS_POWER_OF_TWO ( align ) );

	ret = MILTYALLOC_ALIGN_DOWN_POW_2 ( allocator->curRight - size, align );

	if ( ret < allocator->curLeft )
		return NULL;

	allocator->curRight = ret;

	return (void*)ret;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

MILTYALLOC_API void  miltyalloc_stack_allocator_initialize ( stack_allocator* allocator, void* memory, size_t memorySizeInBytes, stack_allocator_bookkeeping_entry* bookkeepingBuffer, size_t bookkeepingEntryCount )
{
	allocator->current = allocator->start = (uintptr_t)memory;
	allocator->end     = allocator->start + memorySizeInBytes;

	allocator->bookkeepingBuffer     = bookkeepingBuffer;
	allocator->bookkeepingCurEntry   = 0;
	allocator->bookkeepingEntryCount = bookkeepingEntryCount;
}

MILTYALLOC_API void  miltyalloc_stack_allocator_reset ( stack_allocator* allocator )
{
	allocator->current             = allocator->start;
	allocator->bookkeepingCurEntry = 0;
}

MILTYALLOC_API miltyalloc_error miltyalloc_stack_allocator_free ( stack_allocator* allocator, void* data )
{
	if ( allocator->bookkeepingCurEntry == 0 || allocator->bookkeepingBuffer[allocator->bookkeepingCurEntry-1].bufferUserPtr != (uintptr_t)data )
		return MILTYALLOC_ERROR_FREE_OUT_OF_ORDER;

	allocator->bookkeepingCurEntry--;
	allocator->current = allocator->bookkeepingBuffer[allocator->bookkeepingCurEntry].bufferStart;

	return MILTYALLOC_SUCCESS;
}

MILTYALLOC_API void* miltyalloc_stack_allocator_alloc ( stack_allocator* allocator, size_t size )
{
	uintptr_t newPtr, ret;
	stack_allocator_bookkeeping_entry* bookkeepingEntry;

	newPtr = allocator->current + size;

	if ( allocator->bookkeepingCurEntry >= allocator->bookkeepingEntryCount || newPtr > allocator->end )
		return NULL;

	ret                = allocator->current;
	allocator->current = newPtr;

	bookkeepingEntry = &allocator->bookkeepingBuffer[allocator->bookkeepingCurEntry++];
	bookkeepingEntry->bufferStart   = ret;
	bookkeepingEntry->bufferUserPtr = ret;

	return (void*)ret;
}

MILTYALLOC_API void* miltyalloc_stack_allocator_alloc_aligned_pow_two ( stack_allocator* allocator, size_t size, size_t align )
{
	uintptr_t unalignedRet, alignedRet;
	stack_allocator_bookkeeping_entry* bookkeepingEntry;

	MILTYALLOC_ASSERT ( MILTYALLOC_IS_POWER_OF_TWO ( align ) );

	unalignedRet = allocator->current;
	alignedRet   = MILTYALLOC_ALIGN_UP_POW_2 ( unalignedRet, align );
	
	if ( allocator->bookkeepingCurEntry >= allocator->bookkeepingEntryCount || alignedRet + size > allocator->end )
		return NULL;

	allocator->current = alignedRet + size;

	bookkeepingEntry = &allocator->bookkeepingBuffer[allocator->bookkeepingCurEntry++];
	bookkeepingEntry->bufferStart   = unalignedRet;
	bookkeepingEntry->bufferUserPtr = alignedRet;

	return (void*)alignedRet;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

MILTYALLOC_API void miltyalloc_double_ended_stack_allocator_initialize ( double_ended_stack_allocator* allocator, void* memory, size_t memorySizeInBytes, stack_allocator_bookkeeping_entry* bookkeepingBuffer, size_t bookkeepingEntryCount )
{
	allocator->curLeft  = allocator->startPtr = (uintptr_t)memory;
	allocator->curRight = allocator->endPtr   = allocator->startPtr + memorySizeInBytes;

	allocator->bookkeepingBuffer        = bookkeepingBuffer;
	allocator->bookkeepingCurLeftEntry  = 0;
	allocator->bookkeepingCurRightEntry = bookkeepingEntryCount;
	allocator->bookkeepingEntryCount    = bookkeepingEntryCount;
}

MILTYALLOC_API void miltyalloc_double_ended_stack_allocator_reset_left ( double_ended_stack_allocator* allocator )
{
	allocator->curLeft                 = allocator->startPtr;
	allocator->bookkeepingCurLeftEntry = 0;
}

MILTYALLOC_API void miltyalloc_double_ended_stack_allocator_reset_right ( double_ended_stack_allocator* allocator )
{
	allocator->curRight                 = allocator->endPtr;
	allocator->bookkeepingCurRightEntry = allocator->bookkeepingEntryCount;
}

MILTYALLOC_API miltyalloc_error miltyalloc_double_ended_stack_allocator_free_left ( double_ended_stack_allocator* allocator, void* data )
{
	if ( allocator->bookkeepingCurLeftEntry == 0 || allocator->bookkeepingBuffer[allocator->bookkeepingCurLeftEntry-1].bufferUserPtr != (uintptr_t)data )
		return MILTYALLOC_ERROR_FREE_OUT_OF_ORDER;

	allocator->bookkeepingCurLeftEntry--;
	allocator->curLeft = allocator->bookkeepingBuffer[allocator->bookkeepingCurLeftEntry].bufferStart;
	
	return MILTYALLOC_SUCCESS;
}

MILTYALLOC_API miltyalloc_error miltyalloc_double_ended_stack_allocator_free_right ( double_ended_stack_allocator* allocator, void* data )
{
	if ( allocator->bookkeepingCurRightEntry == allocator->bookkeepingEntryCount || allocator->bookkeepingBuffer[allocator->bookkeepingCurRightEntry].bufferUserPtr != (uintptr_t)data )
		return MILTYALLOC_ERROR_FREE_OUT_OF_ORDER;

	allocator->bookkeepingCurRightEntry++;
	if ( allocator->bookkeepingCurRightEntry == allocator->bookkeepingEntryCount )
		allocator->curRight = allocator->endPtr;
	else
		allocator->curRight = allocator->bookkeepingBuffer[allocator->bookkeepingCurRightEntry].bufferStart;
	
	return MILTYALLOC_SUCCESS;
}

MILTYALLOC_API void* miltyalloc_double_ended_stack_allocator_alloc_left ( double_ended_stack_allocator* allocator, size_t size )
{
	uintptr_t newPtr, ret;
	stack_allocator_bookkeeping_entry* bookkeepingEntry;

	newPtr = allocator->curLeft + size;

	if ( newPtr > allocator->curRight || allocator->bookkeepingCurLeftEntry >= allocator->bookkeepingCurRightEntry )
		return NULL;

	ret                = allocator->curLeft;
	allocator->curLeft = newPtr;

	bookkeepingEntry = &allocator->bookkeepingBuffer[allocator->bookkeepingCurLeftEntry++];
	bookkeepingEntry->bufferStart   = ret;
	bookkeepingEntry->bufferUserPtr = ret;

	return (void*)ret;
}

MILTYALLOC_API void* miltyalloc_double_ended_stack_allocator_alloc_right ( double_ended_stack_allocator* allocator, size_t size )
{
	uintptr_t ret;
	stack_allocator_bookkeeping_entry* bookkeepingEntry;

	ret = allocator->curRight - size;

	if ( ret < allocator->curLeft || allocator->bookkeepingCurRightEntry <= allocator->bookkeepingCurLeftEntry )
		return NULL;

	allocator->curRight = ret;

	bookkeepingEntry = &allocator->bookkeepingBuffer[--allocator->bookkeepingCurRightEntry];
	bookkeepingEntry->bufferStart   = ret;
	bookkeepingEntry->bufferUserPtr = ret;

	return (void*)ret;
}

MILTYALLOC_API void* miltyalloc_double_ended_stack_allocator_alloc_left_aligned_pow_two ( double_ended_stack_allocator* allocator, size_t size, size_t align )
{
	uintptr_t unalignedRet, alignedRet;
	stack_allocator_bookkeeping_entry* bookkeepingEntry;

	MILTYALLOC_ASSERT ( MILTYALLOC_IS_POWER_OF_TWO ( align ) );

	unalignedRet = allocator->curLeft;
	alignedRet   = MILTYALLOC_ALIGN_UP_POW_2 ( unalignedRet, align );
	
	if ( alignedRet + size > allocator->curRight || allocator->bookkeepingCurLeftEntry >= allocator->bookkeepingCurRightEntry )
		return NULL;

	allocator->curLeft = alignedRet + size;

	bookkeepingEntry = &allocator->bookkeepingBuffer[allocator->bookkeepingCurLeftEntry++];
	bookkeepingEntry->bufferStart   = unalignedRet;
	bookkeepingEntry->bufferUserPtr = alignedRet;

	return (void*)alignedRet;
}

MILTYALLOC_API void* miltyalloc_double_ended_stack_allocator_alloc_right_aligned_pow_two ( double_ended_stack_allocator* allocator, size_t size, size_t align )
{
	uintptr_t ret;
	stack_allocator_bookkeeping_entry* bookkeepingEntry;

	MILTYALLOC_ASSERT ( MILTYALLOC_IS_POWER_OF_TWO ( align ) );

	ret = MILTYALLOC_ALIGN_DOWN_POW_2 ( allocator->curRight - size, align );

	if ( ret < allocator->curLeft || allocator->bookkeepingCurRightEntry <= allocator->bookkeepingCurLeftEntry )
		return NULL;

	allocator->curRight = ret;

	bookkeepingEntry = &allocator->bookkeepingBuffer[--allocator->bookkeepingCurRightEntry];
	bookkeepingEntry->bufferStart   = ret;
	bookkeepingEntry->bufferUserPtr = ret;

	return (void*)ret;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

MILTYALLOC_API void miltyalloc_pool_allocator_initialize ( pool_allocator* allocator, void* memory, uint32_t* poolBookkeepingBuffer, uint32_t poolSize, uint32_t poolCount )
{
	allocator->poolBookkeeping = poolBookkeepingBuffer;
	allocator->startPtr        = (uintptr_t)memory;
	allocator->poolSize        = poolSize;
	allocator->poolCount       = poolCount;
	allocator->poolPtr         = 0;

	for ( uint32_t i = 0; i < poolCount-1; i++ )
		poolBookkeepingBuffer[i] = i + 1;
	poolBookkeepingBuffer[poolCount-1] = 0x7FFFFFFF;
}

MILTYALLOC_API miltyalloc_error miltyalloc_pool_allocator_free ( pool_allocator* allocator, void* memory )
{
	uintptr_t uintptrMemory = (uintptr_t)memory;
	uint32_t poolIndex      = (uint32_t)((uintptrMemory - allocator->startPtr) / allocator->poolSize);

	if ( uintptrMemory < allocator->startPtr || uintptrMemory >= allocator->startPtr + allocator->poolSize * allocator->poolCount )
		return MILTYALLOC_ERROR_FREE_OUT_OF_BUFFER;
	if ( ((uintptrMemory - allocator->startPtr) % allocator->poolSize) != 0 )
		return MILTYALLOC_ERROR_FREE_MISALIGNED;
	if ( (allocator->poolBookkeeping[poolIndex] & 0x80000000) == 0 )
		return MILTYALLOC_ERROR_NOT_ALLOCATED;

	allocator->poolBookkeeping[poolIndex] = allocator->poolPtr;
	allocator->poolPtr                    = poolIndex;

	return MILTYALLOC_SUCCESS;
}

MILTYALLOC_API void* miltyalloc_pool_allocator_alloc ( pool_allocator* allocator )
{
	uintptr_t poolPtr;

	poolPtr = allocator->poolPtr;

	if ( (poolPtr & 0x7FFFFFFF) == 0x7FFFFFFF )
		return NULL;

	allocator->poolPtr                  = allocator->poolBookkeeping[poolPtr];
	allocator->poolBookkeeping[poolPtr] = 0xFFFFFFFF;

	return (void*)(allocator->startPtr + poolPtr * allocator->poolSize);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// http://en.wikipedia.org/wiki/Buddy_memory_allocation
#define BUDDY_ALLOCATOR_CALCULATE_BOOKKEEPING_SIZE(levelCount,pageCount,nodeCount) ((levelCount) * sizeof ( buddy_allocator_size_bucket ) + (pageCount + nodeCount) * sizeof ( uint32_t ))
MILTYALLOC_API size_t miltyalloc_buddy_allocator_calculate_bookkeeping_size ( size_t bufferSize, size_t minSize )
{
	uint32_t maxLevel, pageCount, nodeCount;

	MILTYALLOC_ASSERT ( MILTYALLOC_IS_POWER_OF_TWO ( minSize ) );
	MILTYALLOC_ASSERT ( minSize <= bufferSize );

	pageCount = (uint32_t)(bufferSize / (float)minSize);
	maxLevel  = (uint32_t)floorf ( log2f ( (float)pageCount ) );
	nodeCount = (1<<(maxLevel+1))-1;

	return BUDDY_ALLOCATOR_CALCULATE_BOOKKEEPING_SIZE(maxLevel+1, pageCount, nodeCount);
}

MILTYALLOC_API void  miltyalloc_buddy_allocator_initialize ( buddy_allocator* allocator, void* memory, size_t bufferSize, size_t minSize, void* bookkeepingBuffer )
{
	uint32_t pageCount, maxLevel, levelNodeCount, nodeCount;
	uint32_t* nodes;

	MILTYALLOC_ASSERT ( MILTYALLOC_IS_POWER_OF_TWO ( minSize ) );
	MILTYALLOC_ASSERT ( minSize <= bufferSize );

	nodeCount = 0;
	pageCount = (uint32_t)( bufferSize / (float)minSize );
	maxLevel  = (uint32_t)floorf ( log2f ( (float)pageCount ) );

	allocator->pageData    = (uint32_t*)bookkeepingBuffer;
	allocator->sizeBuckets = (buddy_allocator_size_bucket*)((uintptr_t)bookkeepingBuffer + pageCount * sizeof ( uint32_t ));
	nodes                  = (uint32_t*)((uintptr_t)allocator->sizeBuckets + (maxLevel+1) * sizeof ( buddy_allocator_size_bucket ));

	levelNodeCount = (1<<maxLevel);
	
	for ( uint32_t i = 0; i < pageCount; i++ )
	{
		// MSB not set (not free) and level of -1 (not set)
		allocator->pageData[i] = 0x7FFFFFFF;
	}

	// maxLevel has 1 node
	// 0 has (1<<maxLevel) nodes
	// levelNodeCount = (1<<(maxLevel-i))

	for ( uint32_t i = 0; i <= maxLevel; i++ )
	{
		allocator->sizeBuckets[i].count = 0;
		allocator->sizeBuckets[i].index = nodes;

		assert ( levelNodeCount == (1<<(maxLevel-i)));

		nodeCount      += levelNodeCount;
		nodes          += levelNodeCount;
		levelNodeCount /= 2;
	}

	assert ( nodeCount == (1<<(maxLevel+1))-1 );
	assert ( (uintptr_t)nodes - (uintptr_t)bookkeepingBuffer == BUDDY_ALLOCATOR_CALCULATE_BOOKKEEPING_SIZE(maxLevel+1, pageCount, nodeCount) );

	allocator->pageData[0] = 0x80000000 | maxLevel;	// Free and level maxLevel

	allocator->sizeBuckets[maxLevel].count    = 1;
	allocator->sizeBuckets[maxLevel].index[0] = 0;

	allocator->minBucketSize = (uint32_t)minSize;
	allocator->sizeLevels    = maxLevel + 1;
	allocator->startPtr      = (uintptr_t)memory;
}

MILTYALLOC_API void* miltyalloc_buddy_allocator_alloc ( buddy_allocator* allocator, size_t size )
{
	int32_t desiredLevelOfAllocation;

	desiredLevelOfAllocation = (int32_t)ceilf ( log2f ( size / (float)allocator->minBucketSize ) );
	if ( desiredLevelOfAllocation < 0 )
		desiredLevelOfAllocation = 0;

	void* ptr;
	uint32_t pageIndex;

	if ( allocator->sizeBuckets[desiredLevelOfAllocation].count )
	{
		uint32_t sectorIndex = allocator->sizeBuckets[desiredLevelOfAllocation].index[--allocator->sizeBuckets[desiredLevelOfAllocation].count];
		pageIndex = (1<<desiredLevelOfAllocation) * sectorIndex;
		
		assert ( allocator->pageData[pageIndex] & 0x80000000 );
		assert ( (allocator->pageData[pageIndex] & 0x7FFFFFFF) == desiredLevelOfAllocation );
		allocator->pageData[pageIndex] &= ~(0x80000000);

		ptr = (void*)(allocator->startPtr + (1<<desiredLevelOfAllocation) * sectorIndex * allocator->minBucketSize);
	}
	else
	{
		// Find the first level that can be split
		uint32_t splitLevel = 0xFFFFFFFF;
		for ( uint32_t i = desiredLevelOfAllocation + 1; i < allocator->sizeLevels; i++ )
		{
			if ( allocator->sizeBuckets[i].count )
			{
				splitLevel = i;
				break;
			}
		}

		if ( splitLevel == 0xFFFFFFFF )
			return NULL;

		uint32_t splitIndex = allocator->sizeBuckets[splitLevel].index[--allocator->sizeBuckets[splitLevel].count];
		for ( int32_t i = splitLevel; i > desiredLevelOfAllocation; i-- )
		{
			splitIndex = splitIndex * 2;
			uint32_t a = (splitIndex+1) * (1<<(i-1));
			allocator->sizeBuckets[i-1].index[allocator->sizeBuckets[i-1].count++] = splitIndex + 1;
			allocator->pageData[a] = 0x80000000 | (i-1);	// Free and on level i-1
		}

		pageIndex = splitIndex * (1<<(desiredLevelOfAllocation));
		assert ( pageIndex < (1U<<(allocator->sizeLevels-1U)));
		allocator->pageData[pageIndex] = 0x00000000 | desiredLevelOfAllocation;

		ptr = (void*)(allocator->startPtr + pageIndex * allocator->minBucketSize);
	}

	for ( int i = 1; i < (1<<desiredLevelOfAllocation); i++ )
		allocator->pageData[pageIndex + i] = 0x7FFFFFFF;

#if MILTYALLOC_DEBUG_SAFETY_CHECKS
	for ( uint32_t i = 0; i < allocator->sizeLevels; i++ )
	{
		assert ( allocator->sizeBuckets[i].count <= (1U<<((allocator->sizeLevels-1U)-i)));
		for ( uint32_t j = 0; j < allocator->sizeBuckets[i].count; j++ )
		{
			assert ( allocator->pageData[allocator->sizeBuckets[i].index[j] * (1<<i)] & 0x80000000 );
		}
	}
#endif

#if MILTYALLOC_DEBUG_SAFETY_CHECKS
	for ( uint32_t i = 0; i < (1U<<(allocator->sizeLevels-1U)); i++ )
	{
		if ( allocator->pageData[i] & 0x80000000 )
		{
			uint32_t level = allocator->pageData[i] & (~0x80000000);
			uint32_t found = 0;
			for ( uint32_t j = 0; j < allocator->sizeBuckets[level].count; j++ )
			{
				found = found || (allocator->sizeBuckets[level].index[j] * (1<<level)) == i;
				
			}
			assert ( found );
		}
	}
#endif

	return ptr;
}

miltyalloc_error miltyalloc_buddy_allocator_free ( buddy_allocator* allocator, void* memory )
{
	uintptr_t offset = (uintptr_t)memory - allocator->startPtr;
	assert ( (offset & (allocator->minBucketSize - 1)) == 0 );
	uint32_t page = (uint32_t)(offset / allocator->minBucketSize);
	assert ( page < (1U<<(allocator->sizeLevels-1U)));
	uint32_t level;

	do
	{
		uint32_t pageInfo = allocator->pageData[page];
		assert ( (pageInfo & 0x80000000) == 0 );

		level = pageInfo & 0x7FFFFFFF;

		uint32_t levelDivision = (1<<level);

		assert ( (page & (levelDivision-1)) == 0 );
		uint32_t levelIndex = page / levelDivision;
		uint32_t buddyLevelIndex = levelIndex ^ 0x1;
		uint32_t buddyIndex = buddyLevelIndex * levelDivision;
		uint32_t buddyLevel = allocator->pageData[buddyIndex] & 0x7FFFFFFF;

		if ( allocator->pageData[buddyIndex] & 0x80000000 && buddyLevel == level )
		{
			uint32_t thaIndex = 0xFFFFFFFF;
			for ( uint32_t i = 0; i < allocator->sizeBuckets[level].count; i++ )
			{
				if ( allocator->sizeBuckets[level].index[i] == buddyLevelIndex )
				{
					thaIndex = i;
					break;
				}
			}
			assert ( thaIndex != 0xFFFFFFFF );
			allocator->sizeBuckets[level].index[thaIndex] = allocator->sizeBuckets[level].index[--allocator->sizeBuckets[level].count];

			if ( (buddyLevelIndex & 0x1) == 0 )
			{
				uint32_t temp = buddyIndex;
				buddyIndex = page;
				page = temp;
			}
			
			allocator->pageData[page] = level + 1;
			allocator->pageData[buddyIndex] = 0x7FFFFFFF;
		}
		else
		{
			allocator->pageData[page] |= 0x80000000;
			allocator->sizeBuckets[level].index[allocator->sizeBuckets[level].count++] = levelIndex;
			break;
		}
	} while ( level != allocator->sizeLevels );

#if MILTYALLOC_DEBUG_SAFETY_CHECKS
	for ( uint32_t i = 0; i < allocator->sizeLevels; i++ )
	{
		assert ( allocator->sizeBuckets[i].count <= (1U<<((allocator->sizeLevels-1U)-i)));
		for ( uint32_t j = 0; j < allocator->sizeBuckets[i].count; j++ )
		{
			assert ( allocator->pageData[allocator->sizeBuckets[i].index[j] * (1<<i)] & 0x80000000 );
		}
	}
#endif

#if MILTYALLOC_DEBUG_SAFETY_CHECKS
	for ( uint32_t i = 0; i < (1U<<(allocator->sizeLevels-1U)); i++ )
	{
		if ( allocator->pageData[i] & 0x80000000 )
		{
			uint32_t level = allocator->pageData[i] & (~0x80000000);
			uint32_t found = 0;
			for ( uint32_t j = 0; j < allocator->sizeBuckets[level].count; j++ )
			{
				found = found || (allocator->sizeBuckets[level].index[j] * (1<<level)) == i;
				
			}
			assert ( found );
		}
	}
#endif
	return MILTYALLOC_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////