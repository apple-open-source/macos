/*
 *  AppleDBDMAAudioFloatLib.h
 *  AppleOnboardAudio
 *
 * 	Public interface for floating point library.
 *  The library includes input and output conversion 
 *  routines with different bit depths and processing
 *  requirements.  It also contains utilities that 
 *  require floating point.
 *	
 *  Created by Aram Lindahl on Thu Nov 14 2002.
 *  Copyright (c) 2002 Apple Computer. All rights reserved.
 *
 */

#include "AppleDBDMAClipLib.h"

extern "C" {

#pragma mark ----------------------------- 
#pragma mark ••• Processing Functions
#pragma mark ----------------------------- 

void delayRightChannel(float* inFloatBufferPtr, UInt32 numSamples);
void balanceAdjust(float* inFloatBufferPtr, UInt32 numSamples, EQStructPtr inEQ);
void invertRightChannel(float* inFloatBufferPtr, UInt32 numSamples);
void mixAndMuteRightChannel(float* inFloatBufferPtr, UInt32 numSamples); 
void limiter(float* inFloatBufferPtr, UInt32 numSamples, LimiterStructPtr ioLimiterState, UInt32 index); 
void equalizer(float* inFloatBufferPtr, UInt32 numSamples, EQStructPtr inEQ);
void crossover2way (float* inFloatBufferPtr, UInt32 numSamples, CrossoverStructPtr ioCrossover);
void multibandLimiter(float *inBuf, UInt32 numSamples, CrossoverStructPtr ioCrossover, LimiterStructPtr ioLimiter);

void setEQCoefficients (EQParamStructPtr inParams, EQStructPtr inEQ, UInt32 index, UInt32 inSampleRate);
void setLimiterCoefficients (LimiterParamStructPtr inParams, LimiterStructPtr ioLimiter, UInt32 index, UInt32 inSampleRate);
void setCrossoverCoefficients (CrossoverParamStructPtr inParams, CrossoverStructPtr ioCrossover, UInt32 inSampleRate);
void resetEQ (EQStructPtr inEQ);
void resetLimiter (LimiterStructPtr ioLimiter);
void resetCrossover (CrossoverStructPtr ioCrossover);

#pragma mark ----------------------------- 
#pragma mark ••• iSub Processing Functions
#pragma mark ----------------------------- 

void iSubDownSampleLinearAndConvert(float* inData, float* srcPhase, float* srcState, UInt32 adaptiveSampleRate, UInt32 outputSampleRate, UInt32 sampleIndex, UInt32 maxSampleIndex, SInt16 *iSubBufferMemory, SInt32 *iSubBufferOffset, UInt32 iSubBufferLen, UInt32 *loopCount);
Boolean Set4thOrderPhaseCompCoefficients (float *b0, float *b1, float *a1, float *a2, UInt32 samplingRate);
void StereoCrossover4thOrderPhaseComp (float *in, float *low, float *high, UInt32 frames, UInt32 samplingRate, PreviousValues *section1State, PreviousValues *section2State, PreviousValues *phaseCompState);
void StereoLowPass4thOrder (float *in, float *low, UInt32 frames, UInt32 samplingRate, PreviousValues *section1State, PreviousValues *section2State);

#pragma mark ----------------------------- 
#pragma mark ••• Integer to Float
#pragma mark ----------------------------- 

void Int8ToFloat32( SInt8 *src, float *dest, unsigned int count );

void NativeInt16ToFloat32( signed short *src, float *dest, unsigned int count, int bitDepth );
void NativeInt24ToFloat32( long *src, float *dest, unsigned int count, int bitDepth );
void NativeInt32ToFloat32( long *src, float *dest, unsigned int count, int bitDepth );

void NativeInt16ToFloat32Gain( signed short *src, float *dest, unsigned int count, int bitDepth, float* inGainLPtr, float* inGainRPtr );
void NativeInt16ToFloat32CopyRightToLeft( signed short *src, float *dest, unsigned int count, int bitDepth );

void NativeInt32ToFloat32Gain( signed long *src, float *dest, unsigned int count, int bitDepth, float* inGainLPtr, float* inGainRPtr );

void SwapInt16ToFloat32( signed short *src, float *dest, unsigned int count, int bitDepth );
void SwapInt24ToFloat32( long *src, float *dest, unsigned int count, int bitDepth );
void SwapInt32ToFloat32( long *src, float *dest, unsigned int count, int bitDepth );

#pragma mark ----------------------------- 
#pragma mark ••• Float to Integer
#pragma mark ----------------------------- 

void Float32ToInt8( float *src, SInt8 *dst, unsigned int count );
void Float32ToNativeInt16( float *src, signed short *dst, unsigned int count );
void Float32ToNativeInt24( float *src, signed long *dst, unsigned int count );
void Float32ToNativeInt32( float *src, signed long *dst, unsigned int count );
void Float32ToSwapInt16( float *src, signed short *dst, unsigned int count );
void Float32ToSwapInt24( float *src, signed long *dst, unsigned int count );
void Float32ToSwapInt32( float *src, signed long *dst, unsigned int count );

//void NativeInt16ToFloat32CopyLeftToRight( signed short *src, float *dest, unsigned int count, int bitDepth );
//void NativeInt16ToFloat32CopyLeftToRightGain( signed short *src, float *dest, unsigned int count, int bitDepth, float inGain );
//void NativeInt16ToFloat32CopyRightToLeftGain( signed short *src, float *dest, unsigned int count, int bitDepth, float inGain );
//void NativeInt32ToFloat32CopyLeftToRight( signed long *src, float *dest, unsigned int count, int bitDepth );
//void NativeInt32ToFloat32CopyRightToLeft( signed long *src, float *dest, unsigned int count, int bitDepth );
//void NativeInt32ToFloat32CopyLeftToRightGain( signed long *src, float *dest, unsigned int count, int bitDepth, float inGain );
//void NativeInt32ToFloat32CopyRightToLeftGain( signed long *src, float *dest, unsigned int count, int bitDepth, float inGain );
//void Float32ToNativeInt16MixAndMuteRight( float *src, signed short *dst, unsigned int count );
//void Float32ToNativeInt32MixAndMuteRight( float *src, signed long *dst, unsigned int count );

#pragma mark ---------------------------------------- 
#pragma mark ••• Utilities
#pragma mark ---------------------------------------- 

UInt32 	CalculateOffset (UInt64 nanoseconds, UInt32 sampleRate);
void 	dBfixed2float (UInt32 indBfixed, float* ioGainPtr);
void 	inputGainConverter (UInt32 inGainIndex, float* ioGainPtr);
void 	convertToFourDotTwenty(FourDotTwenty* ioFourDotTwenty, float* inFloatPtr);
void 	dB2linear (float * inDB, float * outLinear);
};
