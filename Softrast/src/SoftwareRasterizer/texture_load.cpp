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
#include "../MemoryMappedFile.h"
#include <assert.h>
#include <math.h>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb_image.h>
#include <stb_image_resize.h>

extern "C" {

	////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////

	extern buddy_allocator* _softrastAllocator;
	extern DEBUG_SETTINGS Debug;

	////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////

	uint32_t softrast_texture_load ( softrast_texture* tex, const char* path )
	{
		//--------------------------------
		// Load file
		//--------------------------------
		MemoryMappedFile file ( path );
		if ( !file.IsValid ( ) )
			return -1;	// File not found (probably)
		
		//--------------------------------
		// Load as texture
		//--------------------------------
		int width, height, components;
		unsigned char* data = stbi_load_from_memory ( (const stbi_uc*)file.GetData ( ), (int)file.GetSize ( ), &width, &height, &components, 4 );

		if ( data == nullptr )
			return -2;	// Could not load image

		//--------------------------------
		// Check parameters
		//--------------------------------
		if ( width != height )
			return (stbi_image_free ( data ), -5); // Only square textures are supported
		
		if ( width & (width-1) )
			return (stbi_image_free ( data ), -6); // Only power-of-two textures are supported
		
		if ( width > 0xFFFF )
			return (stbi_image_free ( data ), -7); // Only power-of-two sizes that fit in 16 bits are supported
		
		//--------------------------------
		// Allocate memory for all mips
		//--------------------------------
		const uint32_t mipLevels = 1 + (uint32_t)floorf ( 0.5f + log2f ( (float)width ) );
		size_t allocSize;
		switch ( Debug.textureAddressingMode )
		{
		case TEXTURE_ADDRESSING_LINEAR:
			allocSize = mipLevels * sizeof ( uint32_t* )
				+ (width * height + (width * height)/3) * sizeof ( uint32_t );
			break;
		
		case TEXTURE_ADDRESSING_TILED:
			{
				size_t mippedPixCount = (width * height + (width * height)/3);
				mippedPixCount += (16 - 1*1);	// Pad mip 1x1
				if ( mipLevels > 1 )
					mippedPixCount += (16 - 2*2);	// Pad mip 2x2
				allocSize = mipLevels * sizeof ( uint32_t* )
					+ mippedPixCount * sizeof ( uint32_t );
			}
			break;
		
		case TEXTURE_ADDRESSING_SWIZZLED:
			allocSize = mipLevels * sizeof ( uint32_t* )
				+ (width * height + (width * height)/3+3) * sizeof ( uint32_t );
			break;
		
		default:
			assert ( false );
			break;
		}

		void* tempMipData = (void*)miltyalloc_buddy_allocator_alloc ( _softrastAllocator, (width/2 + width/4) * (height/2 + height/4) * sizeof ( uint32_t ) );
		void* memory = (void*)miltyalloc_buddy_allocator_alloc ( _softrastAllocator, allocSize );
		if ( memory == nullptr || tempMipData == nullptr )
			return (miltyalloc_buddy_allocator_free ( _softrastAllocator, memory ), miltyalloc_buddy_allocator_free ( _softrastAllocator, tempMipData ), stbi_image_free ( data ), -8);	// Could not allocate enough memory
		
		//--------------------------------
		// Fill texture data
		//--------------------------------
		tex->mipData   = (uint32_t**)memory;
		tex->mipLevels = (uint32_t)mipLevels;
		tex->width     = (uint16_t)width;
		tex->height    = (uint16_t)height;
		
		//--------------------------------
		// Mip data for swapping
		//--------------------------------
		uint32_t* mipData[2] = { (uint32_t*)tempMipData, ((uint32_t*)tempMipData) + ((width/2) * (height/2)) };

		//--------------------------------
		// Fill mips
		//--------------------------------
		uint32_t* memPtr = (uint32_t*)((uintptr_t)memory + tex->mipLevels * sizeof ( uint32_t* ));
		uint32_t mipSize = (uint32_t)width;
		uint32_t* imgpixels = (uint32_t*)data;
		for ( uint32_t i = 0; i < mipLevels; i++, mipSize /= 2 )
		{
			uint32_t rowPitch = mipSize * sizeof ( uint32_t );
			uintptr_t imgaddr = (uintptr_t)imgpixels;
			tex->mipData[i] = memPtr;
		
			if ( Debug.textureAddressingMode == TEXTURE_ADDRESSING_LINEAR )
			{
				for ( uint32_t r = 0; r < mipSize; r++, imgaddr += rowPitch, memPtr += mipSize )
					memcpy ( memPtr, (void*)imgaddr, mipSize * sizeof ( uint32_t ) );
			}
			else if ( Debug.textureAddressingMode == TEXTURE_ADDRESSING_TILED )
			{
				const uint32_t tileCols = (mipSize + 3)/4;
				const uint32_t tileRows = (mipSize + 3)/4;
					  uint32_t* dptr = tex->mipData[i];
		
				for ( uint32_t r = 0; r < tileRows; r++ )
				{
					for ( uint32_t c = 0; c < tileCols; c++, memPtr += 16 )
					{
						const uint32_t startSrcX = c * 4;
						const uint32_t startSrcY = r * 4;
						const uint32_t pixCols   = (mipSize > 4 ? 4 : mipSize);
						const uint32_t pixRows   = (mipSize > 4 ? 4 : mipSize);
						      uint32_t* dptr     = memPtr;
		
						for ( uint32_t pr = 0; pr < pixRows; pr++, dptr += 4 )
						{
							const uint32_t* src = (uint32_t*)(imgpixels + ((startSrcY+pr) * rowPitch)) + startSrcX;
							      uint32_t* dst = dptr;
							for ( uint32_t pc = 0; pc < pixCols; pc++, src++, dst++ )
							{
								*dst = *src;
							}
						}
					}
				}
			}
			else if ( Debug.textureAddressingMode == TEXTURE_ADDRESSING_SWIZZLED )
			{
					  uint32_t  idx  = 0;
					  uint32_t* dptr = tex->mipData[i];
				const uint32_t* eptr = dptr + mipSize * mipSize;
				for ( ; dptr != eptr; idx++, dptr++ )
				{
					//     yxyx yxyx yxyx yxyx yxyx yxyx yxyx yxyx
					// x = 0x55555555
					// y = 0xAAAAAAAA
					uint32_t srcXPadded =  idx & 0x55555555;
					uint32_t srcYPadded = (idx & 0xAAAAAAAA) >> 1;
		
					//    0x0x 0x0x 0x0x 0x0x 0x0x 0x0x 0x0x 0x0x
					// => 0000 0000 0000 0000 xxxx xxxx xxxx xxxx
					uint32_t srcX = 0, srcY = 0;
					for ( uint32_t i = 0; i < 16; i++ )
					{
						srcX |= ((srcXPadded & (1<<(2*i))) >> i);
						srcY |= ((srcYPadded & (1<<(2*i))) >> i);
					}
					*dptr = ((uint32_t*)(imgpixels + srcY * rowPitch))[srcX];
				}
				memPtr += mipSize * mipSize;
			}
			else
				assert ( false );

			//--------------------------------
			// Swap mips
			//--------------------------------
			stbir_resize_uint8_srgb_edgemode ( (const unsigned char*)imgpixels, mipSize, mipSize, mipSize * sizeof ( uint32_t ), (unsigned char*)mipData[i&1], mipSize/2, mipSize/2, mipSize/2 * sizeof ( uint32_t ), 4, 3, 0, STBIR_EDGE_WRAP );
			imgpixels = mipData[i&1];
		}
		
		if ( Debug.textureAddressingMode == TEXTURE_ADDRESSING_SWIZZLED )
		{
			*(memPtr++) = 0xFF00FF;
			*(memPtr++) = 0xFF00FF;
			*(memPtr++) = 0xFF00FF;
		}
		
		//--------------------------------
		// Memory checks
		//--------------------------------
		assert ( (uintptr_t)memPtr == (uintptr_t)memory + allocSize );

		//--------------------------------
		// Cleanup
		//--------------------------------
		miltyalloc_buddy_allocator_free ( _softrastAllocator, tempMipData );
		stbi_image_free ( data );
		
		//--------------------------------
		// We're done here!
		//--------------------------------
		return 0;
	}

	uint32_t softrast_texture_free ( softrast_texture* tex )
	{
		if ( miltyalloc_buddy_allocator_free ( _softrastAllocator, tex->mipData ) == MILTYALLOC_SUCCESS )
			return 0;
		else
			return -1; // Could not free memory
	}
};