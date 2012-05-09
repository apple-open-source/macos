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
	PCMBlitterLibX86.c
	
=============================================================================*/

#include <TargetConditionals.h>

#if __i386__ || __LP64__
#define _MM_MALLOC_H_INCLUDED 1	// we don't want this header
#include <xmmintrin.h>
#include "IOAudioBlitterLib.h"
#include <libkern/OSByteOrder.h>

#define kMaxFloat32 2147483520.0f
	// this is the biggest floating point number that result from a 32-bit int (bits are lost)
	// it's 2^31 - 128

static inline __m128i  byteswap16( __m128i v )
{
	//rotate each 16 bit quantity by 8 bits
	return _mm_or_si128( _mm_slli_epi16( v, 8 ), _mm_srli_epi16( v, 8 ) );
}

static inline __m128i  byteswap32( __m128i v )
{
	//rotate each 32 bit quantity by 16 bits
	// 0xB1 = 10110001 = 2,3,0,1
	v = _mm_shufflehi_epi16( _mm_shufflelo_epi16( v, 0xB1 ), 0xB1 );
	return byteswap16( v );
}


// ===================================================================================================
#pragma mark -
#pragma mark Float -> Int

void Float32ToNativeInt16_X86( const Float32 *src, SInt16 *dst, unsigned int numToConvert )
{
	const float *src0 = src;
	int16_t *dst0 = dst;
	unsigned int count = numToConvert;
	
	if (count >= 8) {
		// vector -- requires 8+ samples
		ROUNDMODE_NEG_INF
		const __m128 vround = (const __m128) { 0.5f, 0.5f, 0.5f, 0.5f };
		const __m128 vmin = (const __m128) { -32768.0f, -32768.0f, -32768.0f, -32768.0f };
		const __m128 vmax = (const __m128) { 32767.0f, 32767.0f, 32767.0f, 32767.0f  };
		const __m128 vscale = (const __m128) { 32768.0f, 32768.0f, 32768.0f, 32768.0f  };
		__m128 vf0, vf1;
		__m128i vi0, vi1, vpack0;
	
#define F32TOLE16 \
		vf0 = _mm_mul_ps(vf0, vscale);			\
		vf1 = _mm_mul_ps(vf1, vscale);			\
		vf0 = _mm_add_ps(vf0, vround);			\
		vf1 = _mm_add_ps(vf1, vround);			\
		vf0 = _mm_max_ps(vf0, vmin);			\
		vf1 = _mm_max_ps(vf1, vmin);			\
		vf0 = _mm_min_ps(vf0, vmax);			\
		vf1 = _mm_min_ps(vf1, vmax);			\
		vi0 = _mm_cvtps_epi32(vf0);			\
		vi1 = _mm_cvtps_epi32(vf1);			\
		vpack0 = _mm_packs_epi32(vi0, vi1);

		int falign = (uintptr_t)src & 0xF;
		int ialign = (uintptr_t)dst & 0xF;
	
		if (falign != 0 || ialign != 0) {
			// do one unaligned conversion
			vf0 = _mm_loadu_ps(src);
			vf1 = _mm_loadu_ps(src+4);
			F32TOLE16
			_mm_storeu_si128((__m128i *)dst, vpack0);
			
			// advance such that the destination ints are aligned
			unsigned int n = (16 - ialign) / 2;
			src += n;
			dst += n;
			count -= n;

			falign = (uintptr_t)src & 0xF;
			if (falign != 0) {
				// unaligned loads, aligned stores
				while (count >= 8) {
					vf0 = _mm_loadu_ps(src);
					vf1 = _mm_loadu_ps(src+4);
					F32TOLE16
					_mm_store_si128((__m128i *)dst, vpack0);
					src += 8;
					dst += 8;
					count -= 8;
				}
				goto VectorCleanup;
			}
		}
	
		// aligned loads, aligned stores
		while (count >= 8) {
			vf0 = _mm_load_ps(src);
			vf1 = _mm_load_ps(src+4);
			F32TOLE16
			_mm_store_si128((__m128i *)dst, vpack0);
			
			src += 8;
			dst += 8;
			count -= 8;
		}
VectorCleanup:
		if (count > 0) {
			// unaligned cleanup -- just do one unaligned vector at the end
			src = src0 + numToConvert - 8;
			dst = dst0 + numToConvert - 8;
			vf0 = _mm_loadu_ps(src);
			vf1 = _mm_loadu_ps(src+4);
			F32TOLE16
			_mm_storeu_si128((__m128i *)dst, vpack0);
		}
		RESTORE_ROUNDMODE
		return;
	}
	
	// scalar for small numbers of samples
	if (count > 0) {
		double scale = 2147483648.0, round = 32768.0, max32 = 2147483648.0 - 1.0 - 32768.0, min32 = 0.;
		ROUNDMODE_NEG_INF
		
		while (count-- > 0) {
			double f0 = *src++;
			f0 = f0 * scale + round;
			SInt32 i0 = FloatToInt(f0, min32, max32);
			i0 >>= 16;
			*dst++ = i0;
		}
		RESTORE_ROUNDMODE
	}
}

// ===================================================================================================

void Float32ToSwapInt16_X86( const Float32 *src, SInt16 *dst, unsigned int numToConvert )
{
	const float *src0 = src;
	int16_t *dst0 = dst;
	unsigned int count = numToConvert;
	
	if (count >= 8) {
		// vector -- requires 8+ samples
		ROUNDMODE_NEG_INF
		const __m128 vround = (const __m128) { 0.5f, 0.5f, 0.5f, 0.5f };
		const __m128 vmin = (const __m128) { -32768.0f, -32768.0f, -32768.0f, -32768.0f };
		const __m128 vmax = (const __m128) { 32767.0f, 32767.0f, 32767.0f, 32767.0f  };
		const __m128 vscale = (const __m128) { 32768.0f, 32768.0f, 32768.0f, 32768.0f  };
		__m128 vf0, vf1;
		__m128i vi0, vi1, vpack0;
	
#define F32TOBE16 \
		vf0 = _mm_mul_ps(vf0, vscale);			\
		vf1 = _mm_mul_ps(vf1, vscale);			\
		vf0 = _mm_add_ps(vf0, vround);			\
		vf1 = _mm_add_ps(vf1, vround);			\
		vf0 = _mm_max_ps(vf0, vmin);			\
		vf1 = _mm_max_ps(vf1, vmin);			\
		vf0 = _mm_min_ps(vf0, vmax);			\
		vf1 = _mm_min_ps(vf1, vmax);			\
		vi0 = _mm_cvtps_epi32(vf0);			\
		vi1 = _mm_cvtps_epi32(vf1);			\
		vpack0 = _mm_packs_epi32(vi0, vi1);		\
		vpack0 = byteswap16(vpack0);

		int falign = (uintptr_t)src & 0xF;
		int ialign = (uintptr_t)dst & 0xF;
	
		if (falign != 0 || ialign != 0) {
			// do one unaligned conversion
			vf0 = _mm_loadu_ps(src);
			vf1 = _mm_loadu_ps(src+4);
			F32TOBE16
			_mm_storeu_si128((__m128i *)dst, vpack0);

			// and advance such that the destination ints are aligned
			unsigned int n = (16 - ialign) / 2;
			src += n;
			dst += n;
			count -= n;

			falign = (uintptr_t)src & 0xF;
			if (falign != 0) {
				// unaligned loads, aligned stores
				while (count >= 8) {
					vf0 = _mm_loadu_ps(src);
					vf1 = _mm_loadu_ps(src+4);
					F32TOBE16
					_mm_store_si128((__m128i *)dst, vpack0);
					src += 8;
					dst += 8;
					count -= 8;
				}
				goto VectorCleanup;
			}
		}
	
		// aligned loads, aligned stores
		while (count >= 8) {
			vf0 = _mm_load_ps(src);
			vf1 = _mm_load_ps(src+4);
			F32TOBE16
			_mm_store_si128((__m128i *)dst, vpack0);
			
			src += 8;
			dst += 8;
			count -= 8;
		}
VectorCleanup:
		if (count > 0) {
			// unaligned cleanup -- just do one unaligned vector at the end
			src = src0 + numToConvert - 8;
			dst = dst0 + numToConvert - 8;
			vf0 = _mm_loadu_ps(src);
			vf1 = _mm_loadu_ps(src+4);
			F32TOBE16
			_mm_storeu_si128((__m128i *)dst, vpack0);
		}
		RESTORE_ROUNDMODE
		return;
	}
	
	// scalar for small numbers of samples
	if (count > 0) {
		double scale = 2147483648.0, round = 32768.0, max32 = 2147483648.0 - 1.0 - 32768.0, min32 = 0.;
		ROUNDMODE_NEG_INF
		
		while (count-- > 0) {
			double f0 = *src++;
			f0 = f0 * scale + round;
			SInt32 i0 = FloatToInt(f0, min32, max32);
			i0 >>= 16;
#if __ppc__			
			*dst++ = OSSwapInt16(i0);
#else
			*dst++ = i0;
#endif

		}
		RESTORE_ROUNDMODE
	}
}

// ===================================================================================================

void Float32ToNativeInt32_X86( const Float32 *src, SInt32 *dst, unsigned int numToConvert )
{
	const float *src0 = src;
	SInt32 *dst0 = dst;
	unsigned int count = numToConvert;
	
	if (count >= 4) {
		// vector -- requires 4+ samples
		ROUNDMODE_NEG_INF
		const __m128 vround = (const __m128) { 0.5f, 0.5f, 0.5f, 0.5f };
		const __m128 vmin = (const __m128) { -2147483648.0f, -2147483648.0f, -2147483648.0f, -2147483648.0f };
		const __m128 vmax = (const __m128) { kMaxFloat32, kMaxFloat32, kMaxFloat32, kMaxFloat32  };
		const __m128 vscale = (const __m128) { 2147483648.0f, 2147483648.0f, 2147483648.0f, 2147483648.0f  };
		__m128 vf0;
		__m128i vi0;
	
#define F32TOLE32(x) \
		vf##x = _mm_mul_ps(vf##x, vscale);			\
		vf##x = _mm_add_ps(vf##x, vround);			\
		vf##x = _mm_max_ps(vf##x, vmin);			\
		vf##x = _mm_min_ps(vf##x, vmax);			\
		vi##x = _mm_cvtps_epi32(vf##x);			\

		int falign = (uintptr_t)src & 0xF;
		int ialign = (uintptr_t)dst & 0xF;
	
		if (falign != 0 || ialign != 0) {
			// do one unaligned conversion
			vf0 = _mm_loadu_ps(src);
			F32TOLE32(0)
			_mm_storeu_si128((__m128i *)dst, vi0);
			
			// and advance such that the destination ints are aligned
			unsigned int n = (16 - ialign) / 4;
			src += n;
			dst += n;
			count -= n;

			falign = (uintptr_t)src & 0xF;
			if (falign != 0) {
				// unaligned loads, aligned stores
				while (count >= 4) {
					vf0 = _mm_loadu_ps(src);
					F32TOLE32(0)
					_mm_store_si128((__m128i *)dst, vi0);
					src += 4;
					dst += 4;
					count -= 4;
				}
				goto VectorCleanup;
			}
		}
	
		while (count >= 4) {
			vf0 = _mm_load_ps(src);
			F32TOLE32(0)
			_mm_store_si128((__m128i *)dst, vi0);
			
			src += 4;
			dst += 4;
			count -= 4;
		}
VectorCleanup:
		if (count > 0) {
			// unaligned cleanup -- just do one unaligned vector at the end
			src = src0 + numToConvert - 4;
			dst = dst0 + numToConvert - 4;
			vf0 = _mm_loadu_ps(src);
			F32TOLE32(0)
			_mm_storeu_si128((__m128i *)dst, vi0);
		}
		RESTORE_ROUNDMODE
		return;
	}
	
	// scalar for small numbers of samples
	if (count > 0) {
		double scale = 2147483648.0, round = 0.5, max32 = 2147483648.0 - 1.0 - 0.5, min32 = 0.;
		ROUNDMODE_NEG_INF
		
		while (count-- > 0) {
			double f0 = *src++;
			f0 = f0 * scale + round;
			SInt32 i0 = FloatToInt(f0, min32, max32);
			*dst++ = i0;
		}
		RESTORE_ROUNDMODE
	}
}

// ===================================================================================================

void Float32ToSwapInt32_X86( const Float32 *src, SInt32 *dst, unsigned int numToConvert )
{
	const float *src0 = src;
	SInt32 *dst0 = dst;
	unsigned int count = numToConvert;
	
	if (count >= 4) {
		// vector -- requires 4+ samples
		ROUNDMODE_NEG_INF
		const __m128 vround = (const __m128) { 0.5f, 0.5f, 0.5f, 0.5f };
		const __m128 vmin = (const __m128) { -2147483648.0f, -2147483648.0f, -2147483648.0f, -2147483648.0f };
		const __m128 vmax = (const __m128) { kMaxFloat32, kMaxFloat32, kMaxFloat32, kMaxFloat32  };
		const __m128 vscale = (const __m128) { 2147483648.0f, 2147483648.0f, 2147483648.0f, 2147483648.0f  };
		__m128 vf0;
		__m128i vi0;
	
#define F32TOBE32(x) \
		vf##x = _mm_mul_ps(vf##x, vscale);			\
		vf##x = _mm_add_ps(vf##x, vround);			\
		vf##x = _mm_max_ps(vf##x, vmin);			\
		vf##x = _mm_min_ps(vf##x, vmax);			\
		vi##x = _mm_cvtps_epi32(vf##x);			\
		vi##x = byteswap32(vi##x);

		int falign = (uintptr_t)src & 0xF;
		int ialign = (uintptr_t)dst & 0xF;
	
		if (falign != 0 || ialign != 0) {
			// do one unaligned conversion
			vf0 = _mm_loadu_ps(src);
			F32TOBE32(0)
			_mm_storeu_si128((__m128i *)dst, vi0);
			
			// and advance such that the destination ints are aligned
			unsigned int n = (16 - ialign) / 4;
			src += n;
			dst += n;
			count -= n;

			falign = (uintptr_t)src & 0xF;
			if (falign != 0) {
				// unaligned loads, aligned stores
				while (count >= 4) {
					vf0 = _mm_loadu_ps(src);
					F32TOBE32(0)
					_mm_store_si128((__m128i *)dst, vi0);
					src += 4;
					dst += 4;
					count -= 4;
				}
				goto VectorCleanup;
			}
		}
	
		while (count >= 4) {
			vf0 = _mm_load_ps(src);
			F32TOBE32(0)
			_mm_store_si128((__m128i *)dst, vi0);
			
			src += 4;
			dst += 4;
			count -= 4;
		}
VectorCleanup:
		if (count > 0) {
			// unaligned cleanup -- just do one unaligned vector at the end
			src = src0 + numToConvert - 4;
			dst = dst0 + numToConvert - 4;
			vf0 = _mm_loadu_ps(src);
			F32TOBE32(0)
			_mm_storeu_si128((__m128i *)dst, vi0);
		}
		RESTORE_ROUNDMODE
		return;
	}
	
	// scalar for small numbers of samples
	if (count > 0) {
		double scale = 2147483648.0, round = 0.5, max32 = 2147483648.0 - 1.0 - 0.5, min32 = 0.;
		ROUNDMODE_NEG_INF
		
		while (count-- > 0) {
			double f0 = *src++;
			f0 = f0 * scale + round;
			SInt32 i0 = FloatToInt(f0, min32, max32);
#if __ppc__			
			*dst++ = OSSwapInt32(i0);
#else
			*dst++ =  i0;
#endif

		}
		RESTORE_ROUNDMODE
	}
}

// ===================================================================================================

// ~14 instructions
static inline __m128i Pack32ToLE24(__m128i val, __m128i mask)
{
	__m128i store;
	val = _mm_srli_si128(val, 1);
	store = _mm_and_si128(val, mask);

	val = _mm_srli_si128(val, 1);
	mask = _mm_slli_si128(mask, 3);
	store = _mm_or_si128(store, _mm_and_si128(val, mask));

	val = _mm_srli_si128(val, 1);
	mask = _mm_slli_si128(mask, 3);
	store = _mm_or_si128(store, _mm_and_si128(val, mask));

	val = _mm_srli_si128(val, 1);
	mask = _mm_slli_si128(mask, 3);
	store = _mm_or_si128(store, _mm_and_si128(val, mask));
	return store;
}

// marginally faster than scalar
void Float32ToNativeInt24_X86( const Float32 *src, UInt8 *dst, unsigned int numToConvert )
{
	const Float32 *src0 = src;
	UInt8 *dst0 = dst;
	unsigned int count = numToConvert;
	
	if (count >= 6) {
		// vector -- requires 6+ samples
		ROUNDMODE_NEG_INF
		const __m128 vround = (const __m128) { 0.5f, 0.5f, 0.5f, 0.5f };
		const __m128 vmin = (const __m128) { -2147483648.0f, -2147483648.0f, -2147483648.0f, -2147483648.0f };
		const __m128 vmax = (const __m128) { kMaxFloat32, kMaxFloat32, kMaxFloat32, kMaxFloat32  };
		const __m128 vscale = (const __m128) { 2147483648.0f, 2147483648.0f, 2147483648.0f, 2147483648.0f  };
		__m128i mask = _mm_setr_epi32(0x00FFFFFF, 0, 0, 0);
			// it is actually cheaper to copy and shift this mask on the fly than to have 4 of them

		__m128i store;
		union {
			UInt32 i[4];
			__m128i v;
		} u;

		__m128 vf0;
		__m128i vi0;

		int falign = (uintptr_t)src & 0xF;
	
		if (falign != 0) {
			// do one unaligned conversion
			vf0 = _mm_loadu_ps(src);
			F32TOLE32(0)
			store = Pack32ToLE24(vi0, mask);
			_mm_storeu_si128((__m128i *)dst, store);

			// and advance such that the source floats are aligned
			unsigned int n = (16 - falign) / 4;
			src += n;
			dst += 3*n;	// bytes
			count -= n;
		}
	
		while (count >= 6) {
			vf0 = _mm_load_ps(src);
			F32TOLE32(0)
			store = Pack32ToLE24(vi0, mask);
			_mm_storeu_si128((__m128i *)dst, store);	// destination always unaligned
			
			src += 4;
			dst += 12;	// bytes
			count -= 4;
		}
		
		
		if (count >= 4) {
			vf0 = _mm_load_ps(src);
			F32TOLE32(0)
			u.v = Pack32ToLE24(vi0, mask);
			((UInt32 *)dst)[0] = u.i[0];
			((UInt32 *)dst)[1] = u.i[1];
			((UInt32 *)dst)[2] = u.i[2];
			
			src += 4;
			dst += 12;	// bytes
			count -= 4;
		}

		if (count > 0) {
			// unaligned cleanup -- just do one unaligned vector at the end
			src = src0 + numToConvert - 4;
			dst = dst0 + 3*numToConvert - 12;
			vf0 = _mm_loadu_ps(src);
			F32TOLE32(0)
			u.v = Pack32ToLE24(vi0, mask);
			((UInt32 *)dst)[0] = u.i[0];
			((UInt32 *)dst)[1] = u.i[1];
			((UInt32 *)dst)[2] = u.i[2];
		}
		RESTORE_ROUNDMODE
		return;
	}
	
	// scalar for small numbers of samples
	if (count > 0) {
		double scale = 2147483648.0, round = 0.5, max32 = 2147483648.0 - 1.0 - 0.5, min32 = 0.;
		ROUNDMODE_NEG_INF
		
		while (count-- > 0) {
			double f0 = *src++;
			f0 = f0 * scale + round;
			UInt32 i0 = FloatToInt(f0, min32, max32);
			dst[0] = (UInt8)(i0 >> 8);
			dst[1] = (UInt8)(i0 >> 16);
			dst[2] = (UInt8)(i0 >> 24);
			dst += 3;
		}
		RESTORE_ROUNDMODE
	}
}


// ===================================================================================================
#pragma mark -
#pragma mark Int -> Float

void NativeInt16ToFloat32_X86( const SInt16 *src, Float32 *dst, unsigned int numToConvert )
{
	const SInt16 *src0 = src;
	Float32 *dst0 = dst;
	unsigned int count = numToConvert;

	if (count >= 8) {
		// vector -- requires 8+ samples
		// convert the 16-bit words to the high word of 32-bit values
#define LEI16TOF32(x, y) \
	vi##x = _mm_unpacklo_epi16(zero, vpack##x); \
	vi##y = _mm_unpackhi_epi16(zero, vpack##x); \
	vf##x = _mm_cvtepi32_ps(vi##x); \
	vf##y = _mm_cvtepi32_ps(vi##y); \
	vf##x = _mm_mul_ps(vf##x, vscale); \
	vf##y = _mm_mul_ps(vf##y, vscale);
		
		const __m128 vscale = (const __m128) { 1.0/2147483648.0f, 1.0/2147483648.0f, 1.0/2147483648.0f, 1.0/2147483648.0f  };
		const __m128i zero = _mm_setzero_si128();
		__m128 vf0, vf1;
		__m128i vi0, vi1, vpack0;

		int ialign = (uintptr_t)src & 0xF;
		int falign = (uintptr_t)dst & 0xF;
	
		if (falign != 0 || ialign != 0) {
			// do one unaligned conversion
			vpack0 = _mm_loadu_si128((__m128i const *)src);
			LEI16TOF32(0, 1)
			_mm_storeu_ps(dst, vf0);
			_mm_storeu_ps(dst+4, vf1);
			
			// and advance such that the destination floats are aligned
			unsigned int n = (16 - falign) / 4;
			src += n;
			dst += n;
			count -= n;

			ialign = (uintptr_t)src & 0xF;
			if (ialign != 0) {
				// unaligned loads, aligned stores
				while (count >= 8) {
					vpack0 = _mm_loadu_si128((__m128i const *)src);
					LEI16TOF32(0, 1)
					_mm_store_ps(dst, vf0);
					_mm_store_ps(dst+4, vf1);
					src += 8;
					dst += 8;
					count -= 8;
				}
				goto VectorCleanup;
			}
		}
	
		// aligned loads, aligned stores
		while (count >= 8) {
			vpack0 = _mm_load_si128((__m128i const *)src);
			LEI16TOF32(0, 1)
			_mm_store_ps(dst, vf0);
			_mm_store_ps(dst+4, vf1);
			src += 8;
			dst += 8;
			count -= 8;
		}
		
VectorCleanup:
		if (count > 0) {
			// unaligned cleanup -- just do one unaligned vector at the end
			src = src0 + numToConvert - 8;
			dst = dst0 + numToConvert - 8;
			vpack0 = _mm_loadu_si128((__m128i const *)src);
			LEI16TOF32(0, 1)
			_mm_storeu_ps(dst, vf0);
			_mm_storeu_ps(dst+4, vf1);
		}
		return;
	}
	// scalar for small numbers of samples
	if (count > 0) {
		double scale = 1./32768.f;
		while (count-- > 0) {
			SInt16 i = *src++;
			double f = (double)i * scale;
			*dst++ = f;
		}
	}
}

// ===================================================================================================

void SwapInt16ToFloat32_X86( const SInt16 *src, Float32 *dst, unsigned int numToConvert )
{
	const SInt16 *src0 = src;
	Float32 *dst0 = dst;
	unsigned int count = numToConvert;

	if (count >= 8) {
		// vector -- requires 8+ samples
		// convert the 16-bit words to the high word of 32-bit values
#define BEI16TOF32 \
	vpack0 = byteswap16(vpack0); \
	vi0 = _mm_unpacklo_epi16(zero, vpack0); \
	vi1 = _mm_unpackhi_epi16(zero, vpack0); \
	vf0 = _mm_cvtepi32_ps(vi0); \
	vf1 = _mm_cvtepi32_ps(vi1); \
	vf0 = _mm_mul_ps(vf0, vscale); \
	vf1 = _mm_mul_ps(vf1, vscale);
		
		const __m128 vscale = (const __m128) { 1.0/2147483648.0f, 1.0/2147483648.0f, 1.0/2147483648.0f, 1.0/2147483648.0f  };
		const __m128i zero = _mm_setzero_si128();
		__m128 vf0, vf1;
		__m128i vi0, vi1, vpack0;

		int ialign = (uintptr_t)src & 0xF;
		int falign = (uintptr_t)dst & 0xF;
	
		if (falign != 0 || ialign != 0) {
			// do one unaligned conversion
			vpack0 = _mm_loadu_si128((__m128i const *)src);
			BEI16TOF32
			_mm_storeu_ps(dst, vf0);
			_mm_storeu_ps(dst+4, vf1);

			// and advance such that the destination floats are aligned
			unsigned int n = (16 - falign) / 4;
			src += n;
			dst += n;
			count -= n;

			ialign = (uintptr_t)src & 0xF;
			if (ialign != 0) {
				// unaligned loads, aligned stores
				while (count >= 8) {
					vpack0 = _mm_loadu_si128((__m128i const *)src);
					BEI16TOF32
					_mm_store_ps(dst, vf0);
					_mm_store_ps(dst+4, vf1);
					src += 8;
					dst += 8;
					count -= 8;
				}
				goto VectorCleanup;
			}
		}
	
		// aligned loads, aligned stores
		while (count >= 8) {
			vpack0 = _mm_load_si128((__m128i const *)src);
			BEI16TOF32
			_mm_store_ps(dst, vf0);
			_mm_store_ps(dst+4, vf1);
			src += 8;
			dst += 8;
			count -= 8;
		}
		
VectorCleanup:
		if (count > 0) {
			// unaligned cleanup -- just do one unaligned vector at the end
			src = src0 + numToConvert - 8;
			dst = dst0 + numToConvert - 8;
			vpack0 = _mm_loadu_si128((__m128i const *)src);
			BEI16TOF32
			_mm_storeu_ps(dst, vf0);
			_mm_storeu_ps(dst+4, vf1);
		}
		return;
	}
	// scalar for small numbers of samples
	if (count > 0) {
		double scale = 1./32768.f;
		while (count-- > 0) {
			SInt16 i = *src++;
#if __ppc__
			i = OSSwapInt16(i);
#endif
			double f = (double)i * scale;
			*dst++ = f;
		}
	}
}

// ===================================================================================================

void NativeInt32ToFloat32_X86( const SInt32 *src, Float32 *dst, unsigned int numToConvert )
{
	const SInt32 *src0 = src;
	Float32 *dst0 = dst;
	unsigned int count = numToConvert;

	if (count >= 4) {
		// vector -- requires 4+ samples
#define LEI32TOF32(x) \
	vf##x = _mm_cvtepi32_ps(vi##x); \
	vf##x = _mm_mul_ps(vf##x, vscale); \
		
		const __m128 vscale = (const __m128) { 1.0/2147483648.0f, 1.0/2147483648.0f, 1.0/2147483648.0f, 1.0/2147483648.0f  };
		__m128 vf0;
		__m128i vi0;

		int ialign = (uintptr_t)src & 0xF;
		int falign = (uintptr_t)dst & 0xF;
	
		if (falign != 0 || ialign != 0) {
			// do one unaligned conversion
			vi0 = _mm_loadu_si128((__m128i const *)src);
			LEI32TOF32(0)
			_mm_storeu_ps(dst, vf0);
			
			// and advance such that the destination floats are aligned
			unsigned int n = (16 - falign) / 4;
			src += n;
			dst += n;
			count -= n;

			ialign = (uintptr_t)src & 0xF;
			if (ialign != 0) {
				// unaligned loads, aligned stores
				while (count >= 4) {
					vi0 = _mm_loadu_si128((__m128i const *)src);
					LEI32TOF32(0)
					_mm_store_ps(dst, vf0);
					src += 4;
					dst += 4;
					count -= 4;
				}
				goto VectorCleanup;
			}
		}
	
		// aligned loads, aligned stores
		while (count >= 4) {
			vi0 = _mm_load_si128((__m128i const *)src);
			LEI32TOF32(0)
			_mm_store_ps(dst, vf0);
			src += 4;
			dst += 4;
			count -= 4;
		}
		
VectorCleanup:
		if (count > 0) {
			// unaligned cleanup -- just do one unaligned vector at the end
			src = src0 + numToConvert - 4;
			dst = dst0 + numToConvert - 4;
			vi0 = _mm_loadu_si128((__m128i const *)src);
			LEI32TOF32(0)
			_mm_storeu_ps(dst, vf0);
		}
		return;
	}
	// scalar for small numbers of samples
	if (count > 0) {
		double scale = 1./2147483648.0f;
		while (count-- > 0) {
			SInt32 i = *src++;
			double f = (double)i * scale;
			*dst++ = f;
		}
	}
}

// ===================================================================================================

void SwapInt32ToFloat32_X86( const SInt32 *src, Float32 *dst, unsigned int numToConvert )
{
	const SInt32 *src0 = src;
	Float32 *dst0 = dst;
	unsigned int count = numToConvert;

	if (count >= 4) {
		// vector -- requires 4+ samples
#define BEI32TOF32(x) \
	vi##x = byteswap32(vi##x); \
	vf##x = _mm_cvtepi32_ps(vi##x); \
	vf##x = _mm_mul_ps(vf##x, vscale); \
		
		const __m128 vscale = (const __m128) { 1.0/2147483648.0f, 1.0/2147483648.0f, 1.0/2147483648.0f, 1.0/2147483648.0f  };
		__m128 vf0;
		__m128i vi0;

		int ialign = (uintptr_t)src & 0xF;
		int falign = (uintptr_t)dst & 0xF;
	
		if (falign != 0 || ialign != 0) {
			// do one unaligned conversion
			vi0 = _mm_loadu_si128((__m128i const *)src);
			BEI32TOF32(0)
			_mm_storeu_ps(dst, vf0);
			
			// and advance such that the destination floats are aligned
			unsigned int n = (16 - falign) / 4;
			src += n;
			dst += n;
			count -= n;

			ialign = (uintptr_t)src & 0xF;
			if (ialign != 0) {
				// unaligned loads, aligned stores
				while (count >= 4) {
					vi0 = _mm_loadu_si128((__m128i const *)src);
					BEI32TOF32(0)
					_mm_store_ps(dst, vf0);
					src += 4;
					dst += 4;
					count -= 4;
				}
				goto VectorCleanup;
			}
		}
	
		// aligned loads, aligned stores
		while (count >= 4) {
			vi0 = _mm_load_si128((__m128i const *)src);
			BEI32TOF32(0)
			_mm_store_ps(dst, vf0);
			src += 4;
			dst += 4;
			count -= 4;
		}
		
VectorCleanup:
		if (count > 0) {
			// unaligned cleanup -- just do one unaligned vector at the end
			src = src0 + numToConvert - 4;
			dst = dst0 + numToConvert - 4;
			vi0 = _mm_loadu_si128((__m128i const *)src);
			BEI32TOF32(0)
			_mm_storeu_ps(dst, vf0);
		}
		return;
	}
	// scalar for small numbers of samples
	if (count > 0) {
		double scale = 1./2147483648.0f;
		while (count-- > 0) {
			SInt32 i = *src++;
#if __ppc__
			i = OSSwapInt32(i);
#endif

			double f = (double)i * scale;
			*dst++ = f;
		}
	}
}


#endif // __i386__



