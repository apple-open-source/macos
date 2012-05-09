/*	Copyright: 	© Copyright 2005 Apple Computer, Inc. All rights reserved.

	Disclaimer:	IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc.
			("Apple") in consideration of your agreement to the following terms, and your
			use, installation, modification or redistribution of this Apple software
			constitutes acceptance of these terms.  If you do not agree with these terms,
			please do not use, install, modify or redistribute this Apple software.

			In consideration of your agreement to abide by the following terms, and subject
			to these terms, Apple grants you a personal, non-exclusive license, under Apple’s
			copyrights in this original Apple software (the "Apple Software"), to use,
			reproduce, modify and redistribute the Apple Software, with or without
			modifications, in source and/or binary forms; provided that if you redistribute
			the Apple Software in its entirety and without modifications, you must retain
			this notice and the following text and disclaimers in all such redistributions of
			the Apple Software.  Neither the name, trademarks, service marks or logos of
			Apple Computer, Inc. may be used to endorse or promote products derived from the
			Apple Software without specific prior written permission from Apple.  Except as
			expressly stated in this notice, no other rights or licenses, express or implied,
			are granted by Apple herein, including but not limited to any patent rights that
			may be infringed by your derivative works or by other works in which the Apple
			Software may be incorporated.

			The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES NO
			WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED
			WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A PARTICULAR
			PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION ALONE OR IN
			COMBINATION WITH YOUR PRODUCTS.

			IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR
			CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
			GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
			ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION
			OF THE APPLE SOFTWARE, HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT, TORT
			(INCLUDING NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN
			ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
/*=============================================================================
	PCMBlitterLibDispatch.h
	
=============================================================================*/

#include "IOAudioBlitterLibDispatch.h"
#include "IOAudioBlitterLib.h"
#include <xmmintrin.h>
#include <smmintrin.h>

/*
	PCM int<->float library.

	These are the high-level interfaces which dispatch to (often processor-specific) optimized routines.
	Avoid calling the lower-level routines directly; they are subject to renaming etc.
	
	There are two sets of interfaces:
	[1] integer formats are either "native" or "swap"
	[2] integer formats are "BE" or "LE", signifying big or little endian. These are simply macros for the other functions.
	
	All floating point numbers are 32-bit native-endian.
	Supports 16, 24, and 32-bit integers, big and little endian.
	
	32-bit floats and ints must be 4-byte aligned.
	24-bit samples have no alignment requirements.
	16-bit ints must be 2-byte aligned.
	
	On Intel, the haveVector argument is ignored and some implementations assume SSE2.
*/


void IOAF_NativeInt16ToFloat32( const SInt16 *src, Float32 *dest, unsigned int count )
{
	NativeInt16ToFloat32_X86(src, dest, count);
}

void IOAF_SwapInt16ToFloat32( const SInt16 *src, Float32 *dest, unsigned int count )
{
	SwapInt16ToFloat32_X86(src, dest, count);
}

void IOAF_NativeInt24ToFloat32( const UInt8 *src, Float32 *dest, unsigned int count )
{
	NativeInt24ToFloat32_Portable(src, dest, count);
}

void IOAF_SwapInt24ToFloat32( const UInt8 *src, Float32 *dest, unsigned int count )
{
	SwapInt24ToFloat32_Portable(src, dest, count);
}

void IOAF_NativeInt32ToFloat32( const SInt32 *src, Float32 *dest, unsigned int count )
{
	NativeInt32ToFloat32_X86(src, dest, count);
}

void IOAF_SwapInt32ToFloat32( const SInt32 *src, Float32 *dest, unsigned int count )
{
	SwapInt32ToFloat32_X86(src, dest, count);
}

void IOAF_Float32ToNativeInt16( const Float32 *src, SInt16 *dest, unsigned int count )
{
	Float32ToNativeInt16_X86(src, dest, count);
}

void IOAF_Float32ToSwapInt16( const Float32 *src, SInt16 *dest, unsigned int count )
{
	Float32ToSwapInt16_X86(src, dest, count);
}

void IOAF_Float32ToNativeInt24( const Float32 *src, UInt8 *dest, unsigned int count )
{
	Float32ToNativeInt24_X86(src, dest, count);
}

void IOAF_Float32ToSwapInt24( const Float32 *src, UInt8 *dest, unsigned int count )
{
	Float32ToSwapInt24_Portable(src, dest, count);
}

void IOAF_Float32ToNativeInt32( const Float32 *src, SInt32 *dest, unsigned int count )
{
	Float32ToNativeInt32_X86(src, dest, count);
}

void IOAF_Float32ToSwapInt32( const Float32 *src, SInt32 *dest, unsigned int count )
{
	Float32ToSwapInt32_X86(src, dest, count);
}

void IOAF_bcopy_WriteCombine(const void *pSrc, void *pDst, unsigned int count)
{
	unsigned int n;
	
	bool salign = !((uintptr_t)pSrc & 0xF);
	bool dalign = !((uintptr_t)pDst & 0xF);
	unsigned int size4Left = count & 0xF;
	UInt8*	src_data = (UInt8*) pSrc;
	UInt8*	dst_data = (UInt8*) pDst;
	
	count &= ~0xF;
	
	if ( dalign )
	{
		if ( salign )
		{
			// #1 efficient loop - both src/dst are 16-byte aligned
			
			unsigned int size16Left = count & 0x3F;
			count &= ~0x3F;
			
			// First loop operates on 64 byte chunks
			for (n=0; n<count; n+=64)
			{
				__m128i data1, data2, data3, data4;
				
				data1 = _mm_stream_load_si128( (__m128i*) src_data + 0);
				data2 = _mm_stream_load_si128( (__m128i*) src_data + 1);
				data3 = _mm_stream_load_si128( (__m128i*) src_data + 2);
				data4 = _mm_stream_load_si128( (__m128i*) src_data + 3);
				
				_mm_store_si128((__m128i*)dst_data + 0, data1);
				_mm_store_si128((__m128i*)dst_data + 1, data2);
				_mm_store_si128((__m128i*)dst_data + 2, data3);
				_mm_store_si128((__m128i*)dst_data + 3, data4);
				
				src_data += 64;
				dst_data += 64;
			}
			
			// Second loop works on 16-byte chunks
			for (n=0; n<size16Left; n+=16)
			{
				__m128i data1;
				data1 = _mm_stream_load_si128( (__m128i*) src_data);
				_mm_store_si128((__m128i*)dst_data, data1);
				
				src_data += 16;
				dst_data += 16;				
			}
		}
		else
		{
			// #3 efficient loop - src unaligned (no streaming reads). dst aligned
			for (n=0; n<count; n+=16)
			{
				__m128i data128 = _mm_loadu_si128( (__m128i*) src_data);
				_mm_store_si128((__m128i*)dst_data, data128);
				
				src_data += 16;
				dst_data += 16;
			}			
		}
		
	}
	else
	{
		if ( salign)
		{
			// #2 efficient loop - src aligned, dst not aligned
			for (n=0; n<count; n+=16)
			{
				__m128i data128 = _mm_stream_load_si128( (__m128i*) src_data);
				_mm_storeu_si128((__m128i*)dst_data, data128);
				
				src_data += 16;
				dst_data += 16;
			}
		}
		else
		{
			// #4 efficient loop - src unaligned (no streaming reads). dst unaligned
			for (n=0; n<count; n+=16)
			{
				__m128i data128 = _mm_loadu_si128( (__m128i*) src_data);
				_mm_storeu_si128((__m128i*)dst_data, data128);
				
				src_data += 16;
				dst_data += 16;
			}			
		}
	}
	
	// Last loop works on any remaining data not transfered
	for (n=0; n < size4Left; n++)
		*(((char*)dst_data++)) = *((char*)src_data++);
	
	_mm_mfence();
}