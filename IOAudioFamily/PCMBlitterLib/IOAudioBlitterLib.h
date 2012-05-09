/*	Copyright: 	© Copyright 2005-2010 Apple Computer, Inc. All rights reserved.
 
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
 IOAudioBlitterLib.h
 
 =============================================================================*/

#ifndef __IOAudioBlitterLib_h__
#define __IOAudioBlitterLib_h__

#include <TargetConditionals.h>
#include <libkern/OSTypes.h>
#include <libkern/OSByteOrder.h>

typedef float	Float32;
typedef double	Float64;

#define NO_EXPORT			__attribute__((visibility("hidden")))

// ============================================================================
//
// N.B. These functions should not be called directly; use the interfaces in 
//	PCMBlitterLibDispatch.h.
//
// ============================================================================

// can turn these off for debugging
#define PCMBLIT_PPC	TARGET_CPU_PPC
#define PCMBLIT_X86 (__i386__ || __LP64__)

#if TARGET_CPU_PPC && !PCMBLIT_PPC
#warning "PPC optimizations turned off"
#endif

#if __i386__ && !PCMBLIT_X86
#warning "X86 optimizations turned off"
#endif

#define PCMBLIT_INTERLEAVE_SUPPORT TARGET_CPU_PPC	// uses Altivec

typedef const void *ConstVoidPtr;

#pragma mark -
#pragma mark Utilities
#if TARGET_OS_MAC && TARGET_CPU_PPC
#define SET_ROUNDMODE \
double oldSetting; \
/* Set the FPSCR to round to -Inf mode */ \
{ \
union { \
double	d; \
int		i[2]; \
} setting; \
register double newSetting; \
/* Read the the current FPSCR value */ \
__asm__ __volatile__ ( "mffs %0" : "=f" ( oldSetting ) ); \
/* Store it to the stack */ \
setting.d = oldSetting; \
/* Read in the low 32 bits and mask off the last two bits so they are zero      */ \
/* in the integer unit. These two bits set to zero means round to nearest mode. */ \
/* Finally, then store the result back */ \
setting.i[1] |= 3; \
/* Load the new FPSCR setting into the FP register file again */ \
newSetting = setting.d; \
/* Change the volatile to the new setting */ \
__asm__ __volatile__( "mtfsf 7, %0" : : "f" (newSetting ) ); \
}

#define RESTORE_ROUNDMODE \
/* restore the old FPSCR setting */ \
__asm__ __volatile__ ( "mtfsf 7, %0" : : "f" (oldSetting) );
#define DISABLE_DENORMALS
#define RESTORE_DENORMALS

#elif TARGET_OS_MAC && __i386__
// our compiler does ALL floating point with SSE
#define GETCSR()    ({ int _result; asm volatile ("stmxcsr %0" : "=m" (*&_result) ); /*return*/ _result; })
#define SETCSR( a )    { int _temp = a; asm volatile( "ldmxcsr %0" : : "m" (*&_temp ) ); }

#define DISABLE_DENORMALS int _savemxcsr = GETCSR(); SETCSR(_savemxcsr | 0x8040);
#define RESTORE_DENORMALS SETCSR(_savemxcsr);

#define ROUNDMODE_NEG_INF int _savemxcsr = GETCSR(); SETCSR((_savemxcsr & ~0x6000) | 0x2000);
#define ROUNDMODE_NEAREST int _savemxcsr = GETCSR(); SETCSR((_savemxcsr & ~0x6000));

#define RESTORE_ROUNDMODE SETCSR(_savemxcsr);
#define SET_ROUNDMODE 		ROUNDMODE_NEG_INF
#elif TARGET_OS_MAC && __LP64__
// our compiler does ALL floating point with SSE
#define GETCSR()    ({ int _result; asm volatile ("stmxcsr %0" : "=m" (*&_result) ); /*return*/ _result; })
#define SETCSR( a )    { int _temp = a; asm volatile( "ldmxcsr %0" : : "m" (*&_temp ) ); }

#define DISABLE_DENORMALS int _savemxcsr = GETCSR(); SETCSR(_savemxcsr | 0x8040);
#define RESTORE_DENORMALS SETCSR(_savemxcsr);

#define ROUNDMODE_NEG_INF int _savemxcsr = GETCSR(); SETCSR((_savemxcsr & ~0x6000) | 0x2000);
#define ROUNDMODE_NEAREST int _savemxcsr = GETCSR(); SETCSR((_savemxcsr & ~0x6000));

#define RESTORE_ROUNDMODE SETCSR(_savemxcsr);
#define SET_ROUNDMODE 		ROUNDMODE_NEG_INF
#else
#define DISABLE_DENORMALS
#define RESTORE_DENORMALS

#define ROUNDMODE_NEG_INF
#define RESTORE_ROUNDMODE
#endif

// ____________________________________________________________________________
//
// FloatToInt
// N.B. Functions which use this should invoke SET_ROUNDMODE / RESTORE_ROUNDMODE.
static inline SInt32 FloatToInt(double inf, double min32, double max32)
{
#if TARGET_CPU_PPC
#pragma unused ( min32,max32)
	SInt32 i;
	union {
		Float64	d;
		UInt32	i[2];
	} u;
	// fctiw rounds, doesn't truncate towards zero like fctiwz
	__asm__ ("fctiw %0, %1" 
			 /* outputs:  */ : "=f" (u.d) 
			 /* inputs:   */ : "f" (inf));
	i = u.i[1];
	return i;
#elif defined( __i386__ )  || defined( __LP64__ )
#pragma unused ( min32 )
	
	if (inf >= max32) return 0x7FFFFFFF;
	return (SInt32)inf;
#else
	if (inf >= max32) return 0x7FFFFFFF;
	else if (inf <= min32) return 0x80000000;
	return (SInt32)inf;
#endif
}

#ifdef __cplusplus
#pragma mark -
#pragma mark C++ templates
// ____________________________________________________________________________
//
//	PCMBlitter
class PCMBlitter {
public:
	virtual void	Convert(const void *vsrc, void *vdest, unsigned int nSamples) = 0;
	// nSamples = nFrames * nChannels
	virtual ~PCMBlitter() { }
	
#if PCMBLIT_INTERLEAVE_SUPPORT
	// optional methods
	virtual void	Interleave(ConstVoidPtr , void *, int , unsigned int ) { }
	virtual void	Deinterleave(ConstVoidPtr , void *, int , unsigned int ) {  }
#endif
};

// ____________________________________________________________________________
//
//	Types for use in templates
class PCMFloat32 {
public:
	typedef Float32 value_type;
	
	static value_type load(const value_type *p) { return *p; }
	static void store(value_type *p, float val)	{ *p = val; }
};

class PCMFloat64 {
public:
	typedef Float64 value_type;
	
	static value_type load(const value_type *p) { return *p; }
	static void store(value_type *p, value_type val) { *p = val; }
};

class PCMSInt8 {
public:
	typedef SInt8 value_type;
	
	static value_type load(const value_type *p) { return *p; }
	static void store(value_type *p, int val)	{ *p = val; }
};

class PCMUInt8 {
public:
	typedef SInt8 value_type;	// signed so that sign-extending works right
	
	static value_type load(const value_type *p) { return *p ^ 0x80; }
	static void store(value_type *p, int val)	{ *p = val ^ 0x80; }
};

class PCMSInt16Native {
public:
	typedef SInt16 value_type;
	
	static value_type load(const value_type *p) { return *p; }
	static void store(value_type *p, int val)	{ *p = val; }
};

class PCMSInt16Swap {
public:
	typedef SInt16 value_type;
	
	static value_type load(const value_type *p)
	{
#if PCMBLIT_PPC
		register SInt32 result;
		__asm__ volatile("lhbrx %0, %1, %2" 
						 /* outputs:  */ : "=r" (result) 
						 /* inputs:   */ : "b%" (0), "r" (p) 
						 /* clobbers: */ : "memory");
		return result;
#elif PCMBLIT_X86 && !TARGET_OS_WIN32
		return OSReadSwapInt16(p, 0);
#else
		return Endian16_Swap(*p);
#endif
	}
	static void store(value_type *p, int val)
	{
#if PCMBLIT_PPC
		__asm__ volatile("sthbrx %0, %1, %2" : : "r" (val), "b%" (0), "r" (p) : "memory");
#elif PCMBLIT_X86 && !TARGET_OS_WIN32
		OSWriteSwapInt16(p, 0, val);
#else
		*p = Endian16_Swap(val);
#endif
	}
};

class PCMSInt32Native {
public:
	typedef SInt32 value_type;
	
	static value_type load(const value_type *p) { return *p; }
	static void store(value_type *p, int val)	{ *p = val; }
};

class PCMSInt32Swap {
public:
	typedef UInt32 value_type;
	
	static value_type load(const value_type *p)
	{
#if PCMBLIT_PPC
		register long lwbrxResult;
		__asm__ volatile("lwbrx %0, %1, %2" : "=r" (lwbrxResult) : "b%" (0), "r" (p) : "memory");
		return lwbrxResult;
#elif PCMBLIT_X86 && !TARGET_OS_WIN32
		return OSReadSwapInt32(p, 0);
#else
		return Endian32_Swap(*p);
#endif
	}
	static void store(value_type *p, int val)
	{
		*p = val;
#if PCMBLIT_PPC
		__asm__ volatile("stwbrx %0, %1, %2" : : "r" (val), "b%" (0), "r" (p) : "memory");
#elif PCMBLIT_X86 && !TARGET_OS_WIN32
		OSWriteSwapInt32(p, 0, val);
#else
		*p = Endian32_Swap(val);
#endif
	}
};

class PCMFloat64Swap {
public:
	typedef Float64 value_type;
	
	static value_type load(const value_type *vp) {
		union {
			Float64 d;
			UInt32	i[2];
		} u;
		UInt32 *p = (UInt32 *)vp;
		u.i[0] = PCMSInt32Swap::load(p + 1);
		u.i[1] = PCMSInt32Swap::load(p + 0);
		return u.d;
	}
	static void store(value_type *vp, value_type val) {
		union {
			Float64 d;
			UInt32	i[2];
		} u;
		u.d = val;
		UInt32 *p = (UInt32 *)vp;
		PCMSInt32Swap::store(p + 1, u.i[0]);
		PCMSInt32Swap::store(p + 0, u.i[1]);
	}
};

// ____________________________________________________________________________
//
// FloatToIntBlitter
class FloatToIntBlitter : public PCMBlitter {
public:
	FloatToIntBlitter(int bitDepth)
	{
		int rightShift = 32 - bitDepth;
		mShift = rightShift;
		mRound = (rightShift > 0) ? double(1L << (rightShift - 1)) : 0.;
	}
	
protected:
	double	mRound;
	int		mShift;	// how far to shift a 32 bit value right
};

// ____________________________________________________________________________
//
// TFloatToIntBlitter
// only used for: 8-bit, low-aligned, or non-PPC
template <class FloatType, class IntType>
class TFloatToIntBlitter : public FloatToIntBlitter {
public:
	typedef typename FloatType::value_type float_val;
	typedef typename IntType::value_type int_val;
	
	TFloatToIntBlitter(int bitDepth) : FloatToIntBlitter(bitDepth) { }
	
	virtual void	Convert(const void *vsrc, void *vdest, unsigned int nSamples)
	{
		const float_val *src = (const float_val *)vsrc;
		int_val *dest = (int_val *)vdest;
		double maxInt32 = 2147483648.0;	// 1 << 31
		double round = mRound;
		double max32 = maxInt32 - 1.0 - round;
		double min32 = -2147483648.0;
		int shift = mShift, count;
		double f1, f2, f3, f4;
		int i1, i2, i3, i4;
		
		SET_ROUNDMODE
		
		if (nSamples >= 8) {
			f1 = FloatType::load(src + 0);
			
			f2 = FloatType::load(src + 1);
			f1 = f1 * maxInt32 + round;
			
			f3 = FloatType::load(src + 2);
			f2 = f2 * maxInt32 + round;
			i1 = FloatToInt(f1, min32, max32);
			
			src += 3;
			
			nSamples -= 4;
			count = nSamples >> 2;
			nSamples &= 3;
			
			while (count--) {
				f4 = FloatType::load(src + 0);
				f3 = f3 * maxInt32 + round;
				i2 = FloatToInt(f2, min32, max32);
				IntType::store(dest + 0, i1 >> shift);
				
				f1 = FloatType::load(src + 1);
				f4 = f4 * maxInt32 + round;
				i3 = FloatToInt(f3, min32, max32);
				IntType::store(dest + 1, i2 >> shift);
				
				f2 = FloatType::load(src + 2);
				f1 = f1 * maxInt32 + round;
				i4 = FloatToInt(f4, min32, max32);
				IntType::store(dest + 2, i3 >> shift);
				
				f3 = FloatType::load(src + 3);
				f2 = f2 * maxInt32 + round;
				i1 = FloatToInt(f1, min32, max32);
				IntType::store(dest + 3, i4 >> shift);
				
				src += 4;
				dest += 4;
			}
			
			f4 = FloatType::load(src);
			f3 = f3 * maxInt32 + round;
			i2 = FloatToInt(f2, min32, max32);
			IntType::store(dest + 0, i1 >> shift);
			
			f4 = f4 * maxInt32 + round;
			i3 = FloatToInt(f3, min32, max32);
			IntType::store(dest + 1, i2 >> shift);
			
			i4 = FloatToInt(f4, min32, max32);
			IntType::store(dest + 2, i3 >> shift);
			
			IntType::store(dest + 3, i4 >> shift);
			
			src += 1;
			dest += 4;
		}
		
		count = nSamples;
		while (count--) {
			f1 = FloatType::load(src) * maxInt32 + round;
			i1 = FloatToInt(f1, min32, max32) >> shift;
			IntType::store(dest, i1);
			src += 1;
			dest += 1;
		}
		RESTORE_ROUNDMODE
	}
	
#if 0//PCMBLIT_INTERLEAVE_SUPPORT
	virtual void	Interleave(ConstVoidPtr vsrc[], void *vdest, int nChannels, unsigned int nFrames)
	{
		verify_action(nChannels == 2, return);
		
		const float_val *srcA = (const float_val *)vsrc[0];
		const float_val *srcB = (const float_val *)vsrc[1];
		int_val *dest = (int_val *)vdest;
		double maxInt32 = 2147483648.0;	// 1 << 31
		double round = mRound;
		double max32 = maxInt32 - 1.0 - round;
		double min32 = -2147483648.0;
		int shift = mShift, count;
		double f1, f2, f3, f4;
		int i1, i2, i3, i4;
		
		SET_ROUNDMODE
		// two at a time
		count = nFrames >> 1;
		while (count--) {
			f1 = FloatType::load(srcA);
			f2 = FloatType::load(srcB);
			f3 = FloatType::load(srcA+1);
			f4 = FloatType::load(srcB+1);
			f1 = f1 * maxInt32 + round;
			f2 = f2 * maxInt32 + round;
			f3 = f3 * maxInt32 + round;
			f4 = f4 * maxInt32 + round;
			i1 = FloatToInt(f1, min32, max32) >> shift;
			i2 = FloatToInt(f2, min32, max32) >> shift;
			i3 = FloatToInt(f3, min32, max32) >> shift;
			i4 = FloatToInt(f4, min32, max32) >> shift;
			IntType::store(dest, i1);
			IntType::store(dest+1, i2);
			IntType::store(dest+2, i3);
			IntType::store(dest+3, i4);
			srcA += 2;
			srcB += 2;
			dest += 4;
		}
		// leftover
		count = nFrames & 1;
		while (count--) {
			f1 = FloatType::load(srcA);
			f2 = FloatType::load(srcB);
			f1 = f1 * maxInt32 + round;
			f2 = f2 * maxInt32 + round;
			i1 = FloatToInt(f1, min32, max32) >> shift;
			i2 = FloatToInt(f2, min32, max32) >> shift;
			IntType::store(dest, i1);
			IntType::store(dest+1, i2);
			srcA += 1;
			srcB += 1;
			dest += 2;
		}
		RESTORE_ROUNDMODE
	}
#endif // PCMBLIT_INTERLEAVE_SUPPORT
};

// ____________________________________________________________________________
//
// IntToFloatBlitter
class IntToFloatBlitter : public PCMBlitter {
public:
	IntToFloatBlitter(int bitDepth) :
	mBitDepth(bitDepth)
	{
		mScale = static_cast<Float32>(1.0 / float(1UL << (bitDepth - 1)));
	}
	
	Float32		mScale;
	UInt32		mBitDepth;
};

// ____________________________________________________________________________
//
// TIntToFloatBlitter - only used for non-PPC
// On PPC these are roughly only half as fast as the optimized versions
template <class IntType, class FloatType>
class TIntToFloatBlitter : public IntToFloatBlitter {
public:
	typedef typename FloatType::value_type float_val;
	typedef typename IntType::value_type int_val;
	
	TIntToFloatBlitter(int bitDepth) : IntToFloatBlitter(bitDepth) { }
	
	virtual void	Convert(const void *vsrc, void *vdest, unsigned int nSamples)
	{
		const int_val *src = (const int_val *)vsrc;
		float_val *dest = (float_val *)vdest;
		int count = nSamples;
		Float32 scale = mScale;
		int_val i0, i1, i2, i3;
		float_val f0, f1, f2, f3;
		
		/*
		 $i = IntType::load(src); ++src;
		 $f = $i;
		 $f *= scale;
		 FloatType::store(dest, $f); ++dest;
		 */
		
		if (count >= 4) {
			// Cycle 1
			i0 = IntType::load(src); ++src;
			
			// Cycle 2
			i1 = IntType::load(src); ++src;
			f0 = i0;
			
			// Cycle 3
			i2 = IntType::load(src); ++src;
			f1 = i1;
			f0 *= scale;
			
			// Cycle 4
			i3 = IntType::load(src); ++src;
			f2 = i2;
			f1 *= scale;
			FloatType::store(dest, f0); ++dest;
			
			count -= 4;
			int loopCount = count / 4;
			count -= 4 * loopCount;
			
			while (loopCount--) {
				// Cycle A
				i0 = IntType::load(src); ++src;
				f3 = i3;
				f2 *= scale;
				FloatType::store(dest, f1); ++dest;
				
				// Cycle B
				i1 = IntType::load(src); ++src;
				f0 = i0;
				f3 *= scale;
				FloatType::store(dest, f2); ++dest;
				
				// Cycle C
				i2 = IntType::load(src); ++src;
				f1 = i1;
				f0 *= scale;
				FloatType::store(dest, f3); ++dest;
				
				// Cycle D
				i3 = IntType::load(src); ++src;
				f2 = i2;
				f1 *= scale;
				FloatType::store(dest, f0); ++dest;
			}
			
			// Cycle 3
			f3 = i3;
			f2 *= scale;
			FloatType::store(dest, f1); ++dest;
			
			// Cycle 2
			f3 *= scale;
			FloatType::store(dest, f2); ++dest;
			
			// Cycle 1
			FloatType::store(dest, f3); ++dest;
		}
		
		while (count--) {
			i0 = IntType::load(src); ++src;
			f0 = i0;
			f0 *= scale;
			FloatType::store(dest, f0); ++dest;
		}
	}
	
#if 0//PCMBLIT_INTERLEAVE_SUPPORT
	virtual void	Deinterleave(ConstVoidPtr vsrc, void *vdest[], int nChannels, unsigned int nFrames) 
	{
		verify_action(nChannels == 2, return);
		
		const int_val *src = (const int_val *)vsrc;
		float_val *destA = (float_val *)vdest[0];
		float_val *destB = (float_val *)vdest[1];
		int count;
		Float32 scale = mScale;
		
		count = nFrames;
		while (count--) {
			int i1 = IntType::load(src);
			int i2 = IntType::load(src+1);
			float_val f1 = i1 * scale;
			float_val f2 = i2 * scale;
			FloatType::store(destA, f1);
			FloatType::store(destB, f2);
			
			src += 2;
			destA += 1;
			destB += 1;
		}
	}
#endif // PCMBLIT_INTERLEAVE_SUPPORT
};
#endif // __cplusplus

#ifdef __cplusplus
extern "C" {
#endif
	
#if TARGET_CPU_PPC
#pragma mark -
#pragma mark PPC scalar
	
	// ____________________________________________________________________________________
	// PPC scalar
	NO_EXPORT void	NativeInt16ToFloat32_Scalar( const SInt16 *src, Float32 *dest, unsigned int count );
	NO_EXPORT void	SwapInt16ToFloat32_Scalar( const SInt16 *src, Float32 *dest, unsigned int count );
	NO_EXPORT void	NativeInt24ToFloat32_Scalar( const SInt32 *src, Float32 *dest, unsigned int count );
	NO_EXPORT void	SwapInt24ToFloat32_Scalar( const SInt32 *src, Float32 *dest, unsigned int count );
	NO_EXPORT void	NativeInt32ToFloat32_Scalar( const SInt32 *src, Float32 *dest, unsigned int count );
	NO_EXPORT void	SwapInt32ToFloat32_Scalar( const SInt32 *src, Float32 *dest, unsigned int count );
	
	NO_EXPORT void	Float32ToNativeInt16_Scalar( const Float32 *src, SInt16 *dest, unsigned int count );
	NO_EXPORT void	Float32ToSwapInt16_Scalar( const Float32 *src, SInt16 *dest, unsigned int count );
	NO_EXPORT void	Float32ToNativeInt24_Scalar( const Float32 *src, SInt32 *dest, unsigned int count );
	NO_EXPORT void	Float32ToSwapInt24_Scalar( const Float32 *src, SInt32 *dest, unsigned int count );
	NO_EXPORT void	Float32ToNativeInt32_Scalar( const Float32 *src, SInt32 *dest, unsigned int count );
	NO_EXPORT void	Float32ToSwapInt32_Scalar( const Float32 *src, SInt32 *dest, unsigned int count );
	
#pragma mark -
#pragma mark PPC Altivec
	// ____________________________________________________________________________________
	// PPC Altivec
	NO_EXPORT void	Float32ToInt16_Altivec(const Float32 *fsrc, SInt16 *idst, unsigned int numToConvert, int bigEndian);
	NO_EXPORT void	Float32ToInt24_Altivec(const Float32 *fsrc, UInt8 *dst, unsigned int numToConvert, int bigEndian);
	NO_EXPORT void	Float32ToInt32_Altivec(const Float32 *fsrc, SInt32 *dst, unsigned int numToConvert, int bigEndian);
	NO_EXPORT void	Int16ToFloat32_Altivec(const SInt16 *isrc, Float32 *fdst, unsigned int numToConvert, int bigEndian);
	NO_EXPORT void	Int24ToFloat32_Altivec(const UInt8 *isrc, Float32 *fdst, unsigned int numToConvert, int bigEndian);
	NO_EXPORT void	Int32ToFloat32_Altivec(const SInt32 *isrc, Float32 *fdst, unsigned int numToConvert, int bigEndian);
	
	// These don't have wrappers; check their implementations for limitations
	NO_EXPORT void	DeinterleaveInt16ToFloat32_Altivec(const SInt16 *isrc, Float32 *dstA, Float32 *dstB, unsigned int numFrames, int bigEndian);
	//void	Deinterleave32_Altivec(const void *vsrc, void *vdstA, void *vdstB, unsigned int numFrames); untested
	NO_EXPORT void	Interleave32_Altivec(const void *vsrcA, const void *vsrcB, void *vdst, unsigned int numFrames);
	
#elif defined( __i386__ ) || defined( __LP64__ )
#pragma mark -
#pragma mark X86 SSE2
	// ____________________________________________________________________________________
	// X86 SSE2
	NO_EXPORT void	NativeInt16ToFloat32_X86( const SInt16 *src, Float32 *dest, unsigned int count );
	NO_EXPORT void	SwapInt16ToFloat32_X86( const SInt16 *src, Float32 *dest, unsigned int count );
	NO_EXPORT void	Float32ToNativeInt16_X86( const Float32 *src, SInt16 *dest, unsigned int count );
	NO_EXPORT void	Float32ToSwapInt16_X86( const Float32 *src, SInt16 *dest, unsigned int count );
	
	NO_EXPORT void	Float32ToNativeInt24_X86( const Float32 *src, UInt8 *dst, unsigned int numToConvert );
	NO_EXPORT void	Float32ToSwapInt24_X86( const Float32 *src, UInt8 *dst, unsigned int numToConvert );
	
	NO_EXPORT void	NativeInt32ToFloat32_X86( const SInt32 *src, Float32 *dest, unsigned int count );
	NO_EXPORT void	SwapInt32ToFloat32_X86( const SInt32 *src, Float32 *dest, unsigned int count );
	NO_EXPORT void	Float32ToNativeInt32_X86( const Float32 *src, SInt32 *dest, unsigned int count );
	NO_EXPORT void	Float32ToSwapInt32_X86( const Float32 *src, SInt32 *dest, unsigned int count );
#elif __LP64__
#pragma mark -
#pragma mark X86 SSE2
	// ____________________________________________________________________________________
	// X86 SSE2
	NO_EXPORT void	NativeInt16ToFloat32_X86( const SInt16 *src, Float32 *dest, unsigned int count );
	NO_EXPORT void	SwapInt16ToFloat32_X86( const SInt16 *src, Float32 *dest, unsigned int count );
	NO_EXPORT void	Float32ToNativeInt16_X86( const Float32 *src, SInt16 *dest, unsigned int count );
	NO_EXPORT void	Float32ToSwapInt16_X86( const Float32 *src, SInt16 *dest, unsigned int count );
	
	NO_EXPORT void	Float32ToNativeInt24_X86( const Float32 *src, UInt8 *dst, unsigned int numToConvert );
	NO_EXPORT void	Float32ToSwapInt24_X86( const Float32 *src, UInt8 *dst, unsigned int numToConvert );
	
	NO_EXPORT void	NativeInt32ToFloat32_X86( const SInt32 *src, Float32 *dest, unsigned int count );
	NO_EXPORT void	SwapInt32ToFloat32_X86( const SInt32 *src, Float32 *dest, unsigned int count );
	NO_EXPORT void	Float32ToNativeInt32_X86( const Float32 *src, SInt32 *dest, unsigned int count );
	NO_EXPORT void	Float32ToSwapInt32_X86( const Float32 *src, SInt32 *dest, unsigned int count );
	
	
#endif
	
#pragma mark -
#pragma mark Portable
	// ____________________________________________________________________________________
	// Portable
	NO_EXPORT void	Float32ToNativeInt16_Portable( const Float32 *src, SInt16 *dest, unsigned int count );
	NO_EXPORT void	Float32ToSwapInt16_Portable( const Float32 *src, SInt16 *dest, unsigned int count );
	NO_EXPORT void	NativeInt16ToFloat32_Portable( const SInt16 *src, Float32 *dest, unsigned int count );
	NO_EXPORT void	SwapInt16ToFloat32_Portable( const SInt16 *src, Float32 *dest, unsigned int count );
	
	NO_EXPORT void	Float32ToNativeInt24_Portable( const Float32 *src, UInt8 *dest, unsigned int count );
	NO_EXPORT void	Float32ToSwapInt24_Portable( const Float32 *src, UInt8 *dest, unsigned int count );
	NO_EXPORT void	NativeInt24ToFloat32_Portable( const UInt8 *src, Float32 *dest, unsigned int count );
	NO_EXPORT void	SwapInt24ToFloat32_Portable( const UInt8 *src, Float32 *dest, unsigned int count );
	
	NO_EXPORT void	Float32ToNativeInt32_Portable( const Float32 *src, SInt32 *dest, unsigned int count );
	NO_EXPORT void	Float32ToSwapInt32_Portable( const Float32 *src, SInt32 *dest, unsigned int count );
	NO_EXPORT void	NativeInt32ToFloat32_Portable( const SInt32 *src, Float32 *dest, unsigned int count );
	NO_EXPORT void	SwapInt32ToFloat32_Portable( const SInt32 *src, Float32 *dest, unsigned int count );
	
#ifdef __cplusplus
};
#endif

#endif // __IOAudioBlitterLib_h__
