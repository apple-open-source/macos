#include <libkern/OSTypes.h>
#include <IOKit/IOReturn.h>
//#include <libkern/libkern.h>

#include <IOKit/audio/IOAudioTypes.h>
#include <IOKit/audio/IOAudioDebug.h>

#include "AppleDBDMAClipLib.h"	

#include "fp_internal.h"	

#pragma mark ------------------------ 
#pragma mark ••• Constants and Tables
#pragma mark ------------------------ 

//	floating point types
typedef	float	Float32;
typedef double	Float64;

static const double kPI 							= 3.141592653589793116e+00;
static const float 	kFourDotTwentyScaleFactor	 	= 1048576.0f;
static const float  kMaxSignedFourDotTwenty         = 8.0f - 1.0f / 1048576.0f;
static const float 	kMixingToMonoScale 				= 0.5f;
static const float  kiSubCrossoverFrequency			= 240.0f;

// -12 dB to +12 dB in 0.5 dB steps
const UInt16 kInputGaindBConvTableOffset = 24;
static float inputGaindBConvTable[] = {
	0.251189f,	0.266073f,	0.281838f,	0.298538f,
	0.316228f,	0.334965f,	0.354813f,	0.375837f,
	0.398107f,	0.421697f,	0.446684f,	0.473151f,
	0.501187f,	0.530884f,	0.562341f,	0.595662f,
	0.630957f,	0.668344f,	0.707946f,	0.749894f,
	0.794328f,	0.841395f,	0.891251f,	0.944061f,
	1.000000f,	1.059254f,	1.122018f,	1.188502f,
	1.258925f,	1.333521f,	1.412538f,	1.496236f,
	1.584893f,	1.678804f,	1.778279f,	1.883649f,
	1.995262f,	2.113489f,	2.238721f,	2.371374f,
	2.511886f,	2.660725f,	2.818383f,	2.985383f,
	3.162278f,	3.349654f,	3.548134f,	3.758374f,
	3.981072f
};


#pragma mark ------------------------ 
#pragma mark ••• Processing Routines
#pragma mark ------------------------ 

void volume (float* inFloatBufferPtr, UInt32 numSamples, float* inLeftVolume, float* inRightVolume, float* inPreviousLeftVolume, float* inPreviousRightVolume)
{
	register UInt32 i;
	register UInt32 numFrames;
	register UInt32 leftOver;
    register float* inPtr;
    register float* outPtr;
	register float leftGain;
	register float rightGain;
	register float inL0;
	register float inR0;
	register float inL1;
	register float inR1;
	register float inL2;
	register float inR2;
	register float inL3;
	register float inR3;
	register float timeConstant, oneMinusTimeConstant, oldLeftGain, oldRightGain, leftGain_s, rightGain_s;
	
	inPtr = inFloatBufferPtr;
	outPtr = inFloatBufferPtr;  
	
	leftGain = *inLeftVolume;
	rightGain = *inRightVolume;
	
	numFrames = numSamples >> 1;
	leftOver = numFrames % 4;
	numSamples = numFrames >> 2;

	timeConstant = 0.000453411f;
	oneMinusTimeConstant = 1.0f - timeConstant;

	oldLeftGain = *inPreviousLeftVolume;
	oldRightGain = *inPreviousRightVolume;

    for (i = 0; i < numSamples; i++ ) 
    {
		leftGain_s = leftGain*timeConstant;
		rightGain_s = rightGain*timeConstant;

		leftGain_s += oneMinusTimeConstant*oldLeftGain;
		rightGain_s += oneMinusTimeConstant*oldRightGain;

		oldLeftGain = leftGain_s;
		oldRightGain = rightGain_s;

		inL0 = *(inPtr++);					

		inR0 = *(inPtr++);			

		inL1 = *(inPtr++);			
		inL0 *= leftGain_s;

		inR1 = *(inPtr++);		
		inR0 *= rightGain_s;

		leftGain_s = leftGain*timeConstant;
		rightGain_s = rightGain*timeConstant;

		leftGain_s += oneMinusTimeConstant*oldLeftGain;
		rightGain_s += oneMinusTimeConstant*oldRightGain;

		oldLeftGain = leftGain_s;
		oldRightGain = rightGain_s;

		inL2 = *(inPtr++);					
		inL1 *= leftGain_s;
		*(outPtr++) = inL0;			

		inR2 = *(inPtr++);			
		inR1 *= rightGain_s;
		*(outPtr++) = inR0;		

		leftGain_s = leftGain*timeConstant;
		rightGain_s = rightGain*timeConstant;

		leftGain_s += oneMinusTimeConstant*oldLeftGain;
		rightGain_s += oneMinusTimeConstant*oldRightGain;

		oldLeftGain = leftGain_s;
		oldRightGain = rightGain_s;

		inL3 = *(inPtr++);			
		inL2 *= leftGain_s;
		*(outPtr++) = inL1;		

		inR3 = *(inPtr++);		
		inR2 *= rightGain_s;
		*(outPtr++) = inR1;		

		leftGain_s = leftGain*timeConstant;
		rightGain_s = rightGain*timeConstant;

		leftGain_s += oneMinusTimeConstant*oldLeftGain;
		rightGain_s += oneMinusTimeConstant*oldRightGain;

		oldLeftGain = leftGain_s;
		oldRightGain = rightGain_s;

		inL3 *= leftGain_s;
		*(outPtr++) = inL2;			

		inR3 *= rightGain_s;
		*(outPtr++) = inR2;				

		*(outPtr++) = inL3;

		*(outPtr++) = inR3;		
	}

    for (i = 0; i < leftOver; i ++ ) 
    {
		leftGain_s = leftGain*timeConstant;
		rightGain_s = rightGain*timeConstant;
		leftGain_s += oneMinusTimeConstant*oldLeftGain;
		rightGain_s += oneMinusTimeConstant*oldRightGain;
		inL0 = *(inPtr++);
		inR0 = *(inPtr++);
		inL0 *= leftGain_s;
		inR0 *= rightGain_s;
		oldLeftGain = leftGain_s;
		oldRightGain = rightGain_s;
		*(outPtr++) = inL0;			
		*(outPtr++) = inR0;		
	}

	*inPreviousLeftVolume = oldLeftGain;
	*inPreviousRightVolume = oldRightGain;
}


// ------------------------------------------------------------------------
// Mix left and right channels together, and mute the right channel
// ------------------------------------------------------------------------
void mixAndMuteRightChannel(float* inFloatBufferPtr, float* outFloatBufferPtr, UInt32 numSamples) 
{
	UInt32 i, leftOver;
    register float* inPtr;
    register float* outPtr;
	register float inL0;
	register float inL1;
	register float inL2;
	register float inL3;
	register float inR0;
	register float inR1;
	register float inR2;
	register float inR3;
	
	inPtr = inFloatBufferPtr;  
	outPtr = outFloatBufferPtr;  
	
	leftOver = numSamples % 8;
	numSamples = numSamples >> 3;
	
    for (i = 0; i < numSamples; i++ ) 
    {
		inL0 = *(inPtr++);
		inR0 = *(inPtr++);
		
		inL1 = *(inPtr++);
		inR1 = *(inPtr++);
		inL0 += inR0;
		
		inL2 = *(inPtr++);
		inR2 = *(inPtr++);
		inL1 += inR1;
		inL0 *= kMixingToMonoScale;
		
		inL3 = *(inPtr++);
		inR3 = *(inPtr++);
		*(outPtr++) = inL0;
		*(outPtr++) = 0.0f;
		inL1 *= kMixingToMonoScale;
		inL2 += inR2;
		
		inL3 += inR3;
		*(outPtr++) = inL1;
		*(outPtr++) = 0.0f;
		inL2 *= kMixingToMonoScale;

		*(outPtr++) = inL2;
		*(outPtr++) = 0.0f;
		inL3 *= kMixingToMonoScale;

		*(outPtr++) = inL3;
		*(outPtr++) = 0.0f;
	}

    for (i = 0; i < leftOver; i += 2 ) 
    {
		inL0 = *(inPtr++);
		inR0 =  *(inPtr++);
		inL0 += inR0;
		inL0 *= kMixingToMonoScale;
		*(outPtr++) = inL0;
		*(outPtr++) = 0.0f;
	}
}


#pragma mark ------------------------ 
#pragma mark ••• iSub Processing Routines
#pragma mark ------------------------ 

void iSubDownSampleLinearAndConvert( float* inData, float* srcPhase, float* srcState, UInt32 adaptiveSampleRate, UInt32 outputSampleRate, UInt32 sampleIndex, UInt32 maxSampleIndex, SInt16 *iSubBufferMemory, SInt32 *iSubBufferOffset, UInt32 iSubBufferLen, UInt32 *loopCount )
{
    Float32		iSubSampleFloat;
    SInt16		iSubSampleInt;
	UInt32		baseIndex;
	float*		inDataPtr;
	
    float 		x0, x1, temp;
    float 		phaseInc;		// phase increment = Fs_in/Fs_out
    float 		phase;			// current phase location

	inDataPtr = inData;
	baseIndex = sampleIndex;
	phase = *srcPhase;
	phaseInc = ((float)adaptiveSampleRate)/((float)outputSampleRate);
	
	// linear interpolation src (good enough since we have a 4th order lp in front of us, 
	// down -90 dB at Nyquist for 6kHz sample rate)
	while (sampleIndex < maxSampleIndex) {
				
		if (phase >= 1.0f) {	
			phase -= 1.0f;
			sampleIndex+=2;
		} else {   
			// check for beginning of frame case, use saved last sample if needed
			if (sampleIndex == baseIndex) { 
				x0 = *srcState;
			} else {
				// mix x[n-1] to mono
				x0 = inDataPtr[sampleIndex-2];
				temp = inDataPtr[sampleIndex-1];
				x0 = 0.5f*(x0 + temp);
			}
			
			// mix x[n] to mono
			x1 = inDataPtr[sampleIndex];
			temp = inDataPtr[sampleIndex+1];
			x1 = 0.5f*(x1 + temp);
							
			// linearly interpolate between x0 and x1
			iSubSampleFloat = x0 + phase*(x1 - x0);
			
//#if 0
			// clip
			if (iSubSampleFloat > 1.0f) {
				iSubSampleFloat = 1.0f;
			} else if (iSubSampleFloat < -1.0f) {
				iSubSampleFloat = -1.0f;
			}
			
			// convert to fixed
			iSubSampleInt = (SInt16) (iSubSampleFloat * 32767.0f);
//#else			
//			scaled = iSubSampleFloat * scale + round;
//			converted = __fctiw( scaled );
//#endif
			
			// check for end of buffer condition
			if (*iSubBufferOffset >= (SInt32)iSubBufferLen) {
				*iSubBufferOffset = 0;
				(*loopCount)++;
			}
			
			// byteswap to USB format and copy to iSub buffer
			iSubBufferMemory[(*iSubBufferOffset)++] = ((((UInt16)iSubSampleInt) << 8) & 0xFF00) | ((((UInt16)iSubSampleInt) >> 8) & 0x00FF);

			// increment phase and update input buffer pointer
			phase += phaseInc;		
		}
	}
	if (phase < 1) {
		// mix and save last sample in buffer to mono if it will be needed for the next loop
		x1 = inDataPtr[maxSampleIndex-2];
		temp = inDataPtr[maxSampleIndex-1];
		*srcState = 0.5f*(x1 + temp);
	} else {
		*srcState = 0.0f;
	}
	// cache current phase for use next time we enter the clip loop
	*srcPhase = phase;
	
	return;
}

// fourth order coefficient setting functions
Boolean Set4thOrderCoefficients (iSubCoefficients* coefficients, UInt32 inSampleRate)
{
	coefficients->b0 = 1.0f;
	coefficients->b1 = 0.0f;
	coefficients->b2 = 0.0f;
	coefficients->a1 = 0.0f;
	coefficients->a2 = 0.0f;
    return(TRUE);
}

// stereo 4th order LR crossover
// this needs lots of optimization!
void StereoCrossover4thOrderPhaseComp (Float32 *in, Float32 *low, Float32 *high, UInt32 frames, UInt32 SamplingRate, iSubCoefficients* coefficients, PreviousValues *section1State, PreviousValues *section2State, PreviousValues *phaseCompState)
{
    UInt32	i;
    Float32	inL, inR, outL1, outR1, outL, outR, inPhaseCompL, inPhaseCompR;

    // shared coefficients for second order sections
    Float32	b0, b1, b2, a1, a2;
    // taps for second order section 1
    Float32	inLTap1, inLTap2, inRTap1, inRTap2;
    Float32	outLTap1, outLTap2, outRTap1, outRTap2;
    // taps for second order section 2
    Float32	inLTap1_2, inLTap2_2, inRTap1_2, inRTap2_2;
    Float32	outLTap1_2, outLTap2_2, outRTap1_2, outRTap2_2;
    // taps for phase compensator
    Float32	inLTap1_p, inLTap2_p, inRTap1_p, inRTap2_p;
    Float32	outLTap1_p, outLTap2_p, outRTap1_p, outRTap2_p;

    // copy to state local variables to avoid structure referencing during inner loop
    // section 1
    inLTap1 = section1State->xl_1;
    inLTap2 = section1State->xl_2;
    inRTap1 = section1State->xr_1;
    inRTap2 = section1State->xr_2;

    outLTap1 = section1State->yl_1;
    outLTap2 = section1State->yl_2;
    outRTap1 = section1State->yr_1;
    outRTap2 = section1State->yr_2;
    
    // section 2
    inLTap1_2 = section2State->xl_1;
    inLTap2_2 = section2State->xl_2;
    inRTap1_2 = section2State->xr_1;
    inRTap2_2 = section2State->xr_2;

    outLTap1_2 = section2State->yl_1;
    outLTap2_2 = section2State->yl_2;
    outRTap1_2 = section2State->yr_1;
    outRTap2_2 = section2State->yr_2;
    
    // phase compensator
    inLTap1_p = phaseCompState->xl_1;
    inLTap2_p = phaseCompState->xl_2;
    inRTap1_p = phaseCompState->xr_1;
    inRTap2_p = phaseCompState->xr_2;

    outLTap1_p = phaseCompState->yl_1;
    outLTap2_p = phaseCompState->yl_2;
    outRTap1_p = phaseCompState->yr_1;
    outRTap2_p = phaseCompState->yr_2;
 
	b0 = coefficients->b0;
	b1 = coefficients->b1;
	b2 = coefficients->b2;
	a1 = coefficients->a1;
	a2 = coefficients->a2;

    // need to unroll this loop to get rid of stalls!
    for ( i = 0 ; i < frames ; i ++ )
    {
        inL = in[2*i];
        inR = in[2*i+1];
        
        // Low-pass filter first pass
        outL1 = b0*inL + b1*inLTap1 + b2*inLTap2 - a1*outLTap1 - a2*outLTap2;
        outR1 = b0*inR + b1*inRTap1 + b2*inRTap2 - a1*outRTap1 - a2*outRTap2;
 
        // update section 1 filter taps
        inLTap2 = inLTap1;
        inRTap2 = inRTap1;
        inLTap1 = inL;
        inRTap1 = inR;
        outLTap2 = outLTap1;
        outRTap2 = outRTap1;
        outLTap1 = outL1;
        outRTap1 = outR1;
        
        // Low-pass filter second pass
        outL = b0*outL1 + b1*inLTap1_2 + b2*inLTap2_2 - a1*outLTap1_2 - a2*outLTap2_2;
        outR = b0*outR1 + b1*inRTap1_2 + b2*inRTap2_2 - a1*outRTap1_2 - a2*outRTap2_2;
        
        // update section 2 filter taps
        inLTap2_2 = inLTap1_2;
        inRTap2_2 = inRTap1_2;
        inLTap1_2 = outL1;
        inRTap1_2 = outR1;
        outLTap2_2 = outLTap1_2;
        outRTap2_2 = outRTap1_2;
        outLTap1_2 = outL;
        outRTap1_2 = outR;

        // phase compensate the input
        inPhaseCompL = a2*inL + a1*inLTap1_p + inLTap2_p - a1*outLTap1_p - a2*outLTap2_p;
        inPhaseCompR = a2*inR + a1*inRTap1_p + inRTap2_p - a1*outRTap1_p - a2*outRTap2_p;
        
        // update phase compensate filter taps
        inLTap2_p = inLTap1_p;
        inRTap2_p = inRTap1_p;
        inLTap1_p = inL;
        inRTap1_p = inR;
        outLTap2_p = outLTap1_p;
        outRTap2_p = outRTap1_p;
        outLTap1_p = inPhaseCompL;
        outRTap1_p = inPhaseCompR;

        // Storage
        low[2*i] = outL;
        low[2*i+1] = outR;
        high[2*i] = inPhaseCompL-outL;
        high[2*i+1] = inPhaseCompR-outR;
    }

    // update state structures
    
    // section 1 state
    section1State->xl_1 = inLTap1;
    section1State->xl_2 = inLTap2;
    section1State->xr_1 = inRTap1;
    section1State->xr_2 = inRTap2;

    section1State->yl_1 = outLTap1;
    section1State->yl_2 = outLTap2;
    section1State->yr_1 = outRTap1;
    section1State->yr_2 = outRTap2;
    
    // section 2 state
    section2State->xl_1 = inLTap1_2;
    section2State->xl_2 = inLTap2_2;
    section2State->xr_1 = inRTap1_2;
    section2State->xr_2 = inRTap2_2;

    section2State->yl_1 = outLTap1_2;
    section2State->yl_2 = outLTap2_2;
    section2State->yr_1 = outRTap1_2;
    section2State->yr_2 = outRTap2_2;
    
    // phase compensator state
    phaseCompState->xl_1 = inLTap1_p;
    phaseCompState->xl_2 = inLTap2_p;
    phaseCompState->xr_1 = inRTap1_p;
    phaseCompState->xr_2 = inRTap2_p;

    phaseCompState->yl_1 = outLTap1_p;
    phaseCompState->yl_2 = outLTap2_p;
    phaseCompState->yr_1 = outRTap1_p;
    phaseCompState->yr_2 = outRTap2_p;

    return;
}

// stereo 4th order LR crossover
// this needs lots of optimization!
void StereoLowPass4thOrder (Float32 *in, Float32 *low, UInt32 frames, UInt32 SamplingRate, iSubCoefficients* coefficients, PreviousValues *section1State, PreviousValues *section2State)
{
    UInt32	i;
    Float32	inL, inR, outL1, outR1, outL, outR;

    // shared coefficients for second order sections
    Float32	b0, b1, b2, a1, a2;
    // taps for second order section 1
    Float32	inLTap1, inLTap2, inRTap1, inRTap2;
    Float32	outLTap1, outLTap2, outRTap1, outRTap2;
    // taps for second order section 2
    Float32	inLTap1_2, inLTap2_2, inRTap1_2, inRTap2_2;
    Float32	outLTap1_2, outLTap2_2, outRTap1_2, outRTap2_2;

    // copy to state local variables to avoid structure referencing during inner loop
    // section 1
    inLTap1 = section1State->xl_1;
    inLTap2 = section1State->xl_2;
    inRTap1 = section1State->xr_1;
    inRTap2 = section1State->xr_2;

    outLTap1 = section1State->yl_1;
    outLTap2 = section1State->yl_2;
    outRTap1 = section1State->yr_1;
    outRTap2 = section1State->yr_2;
    
    // section 2
    inLTap1_2 = section2State->xl_1;
    inLTap2_2 = section2State->xl_2;
    inRTap1_2 = section2State->xr_1;
    inRTap2_2 = section2State->xr_2;

    outLTap1_2 = section2State->yl_1;
    outLTap2_2 = section2State->yl_2;
    outRTap1_2 = section2State->yr_1;
    outRTap2_2 = section2State->yr_2;
 
	b0 = coefficients->b0;
	b1 = coefficients->b1;
	b2 = coefficients->b2;
	a1 = coefficients->a1;
	a2 = coefficients->a2;
 
    // need to unroll this loop to get rid of stalls!
    for ( i = 0 ; i < frames ; i ++ )
    {
        inL = in[2*i];
        inR = in[2*i+1];
        
        // Low-pass filter first pass
        outL1 = b0*inL + b1*inLTap1 + b2*inLTap2 - a1*outLTap1 - a2*outLTap2;
        outR1 = b0*inR + b1*inRTap1 + b2*inRTap2 - a1*outRTap1 - a2*outRTap2;
 
        // update section 1 filter taps
        inLTap2 = inLTap1;
        inRTap2 = inRTap1;
        inLTap1 = inL;
        inRTap1 = inR;
        outLTap2 = outLTap1;
        outRTap2 = outRTap1;
        outLTap1 = outL1;
        outRTap1 = outR1;
        
        // Low-pass filter second pass
        outL = b0*outL1 + b1*inLTap1_2 + b2*inLTap2_2 - a1*outLTap1_2 - a2*outLTap2_2;
        outR = b0*outR1 + b1*inRTap1_2 + b2*inRTap2_2 - a1*outRTap1_2 - a2*outRTap2_2;
        
        // update section 2 filter taps
        inLTap2_2 = inLTap1_2;
        inRTap2_2 = inRTap1_2;
        inLTap1_2 = outL1;
        inRTap1_2 = outR1;
        outLTap2_2 = outLTap1_2;
        outRTap2_2 = outRTap1_2;
        outLTap1_2 = outL;
        outRTap1_2 = outR;

        // Storage
        low[2*i] = outL;
        low[2*i+1] = outR;
    }

    // update state structures
    
    // section 1 state
    section1State->xl_1 = inLTap1;
    section1State->xl_2 = inLTap2;
    section1State->xr_1 = inRTap1;
    section1State->xr_2 = inRTap2;

    section1State->yl_1 = outLTap1;
    section1State->yl_2 = outLTap2;
    section1State->yr_1 = outRTap1;
    section1State->yr_2 = outRTap2;
    
    // section 2 state
    section2State->xl_1 = inLTap1_2;
    section2State->xl_2 = inLTap2_2;
    section2State->xr_1 = inRTap1_2;
    section2State->xr_2 = inRTap2_2;

    section2State->yl_1 = outLTap1_2;
    section2State->yl_2 = outLTap2_2;
    section2State->yr_1 = outRTap1_2;
    section2State->yr_2 = outRTap2_2;

    return;
}

// new routines [2964790]
#pragma mark ------------------------ 
#pragma mark ••• Conversion Routines
#pragma mark ------------------------ 

#if	defined(__ppc__)

// this behaves incorrectly in Float32ToSwapInt24 if not declared volatile
#define __lwbrx( index, base )	({ register long result; __asm__ __volatile__("lwbrx %0, %1, %2" : "=r" (result) : "b%" (index), "r" (base) : "memory" ); result; } )

#define __lhbrx(index, base)	\
  ({ register signed short lhbrxResult; \
	 __asm__ ("lhbrx %0, %1, %2" : "=r" (lhbrxResult) : "b%" (index), "r" (base) : "memory"); \
	 /*return*/ lhbrxResult; } )
	// dsw: make signed to get sign-extension

#define __rlwimi( rA, rS, cnt, mb, me ) \
	({ __asm__ __volatile__( "rlwimi %0, %2, %3, %4, %5" : "=r" (rA) : "0" (rA), "r" (rS), "n" (cnt), "n" (mb), "n" (me) ); /*return*/ rA; })

#define __stwbrx( value, index, base ) \
	__asm__( "stwbrx %0, %1, %2" : : "r" (value), "b%" (index), "r" (base) : "memory" )

#define __rlwimi_volatile( rA, rS, cnt, mb, me ) \
	({ __asm__ __volatile__( "rlwimi %0, %2, %3, %4, %5" : "=r" (rA) : "0" (rA), "r" (rS), "n" (cnt), "n" (mb), "n" (me) ); /*return*/ rA; })

#define __stfiwx( value, offset, addr )			\
	asm( "stfiwx %0, %1, %2" : /*no result*/ : "f" (value), "b%" (offset), "r" (addr) : "memory" )

static inline double __fctiw( register double B )
{
	register double result;
	__asm__ ( "fctiw %0, %1" : "=f" (result) : "f" (B)  );
	return result;
}

void Int8ToFloat32( SInt8 *src, float *dest, unsigned int count )
{
	register float bias;
	register long exponentMask = ((0x97UL - 8) << 23) | 0x8000;	//FP exponent + bias for sign
	register long int0, int1, int2, int3;
	register float float0, float1, float2, float3;
	register unsigned long loopCount;
	union
	{
		float	f;
		long	i;
	}exponent;

	exponent.i = exponentMask;
	bias = exponent.f;

	src--;
	if( count >= 8 )
	{
		//Software Cycle 1
		int0 = (++src)[0];

		//Software Cycle 2
		int1 = (++src)[0];
		int0 += exponentMask;

		//Software Cycle 3
		int2 = (++src)[0];
		int1 += exponentMask;
		((long*) dest)[0] = int0;

		//Software Cycle 4
		int3 = (++src)[0];
		int2 += exponentMask;
		((long*) dest)[1] = int1;
		//delay one loop for the store to complete

		//Software Cycle 5
		int0 = (++src)[0];
		int3 += exponentMask;
		((long*) dest)[2] = int2;
		float0 = dest[0];

		//Software Cycle 6
		int1 = (++src)[0];
		int0 += exponentMask;
		((long*) dest)[3] = int3;
		float1 = dest[1];
		float0 -= bias;

		//Software Cycle 7
		int2 = (++src)[0];
		int1 += exponentMask;
		((long*) dest)[4] = int0;
		float2 = dest[2];
		float1 -= bias;

		dest--;
		//Software Cycle 8
		int3 = (++src)[0];
		int2 += exponentMask;
		((long*) dest)[6] = int1;
		float3 = dest[4];
		float2 -= bias;
		(++dest)[0] = float0;

		count -= 8;
		loopCount = count / 4;
		count &= 3;
		while( loopCount-- )
		{

			//Software Cycle A
			int0 = (++src)[0];
			int3 += exponentMask;
			((long*) dest)[6] = int2;
			float0 = dest[4];
			float3 -= bias;
			(++dest)[0] = float1;

			//Software Cycle B
			int1 = (++src)[0];
			int0 += exponentMask;
			((long*) dest)[6] = int3;
			float1 = dest[4];
			float0 -= bias;
			(++dest)[0] = float2;

			//Software Cycle C
			int2 = (++src)[0];
			int1 += exponentMask;
			((long*) dest)[6] = int0;
			float2 = dest[4];
			float1 -= bias;
			(++dest)[0] = float3;

			//Software Cycle D
			int3 = (++src)[0];
			int2 += exponentMask;
			((long*) dest)[6] = int1;
			float3 = dest[4];
			float2 -= bias;
			(++dest)[0] = float0;
		}

		//Software Cycle 7
		int3 += exponentMask;
		((long*) dest)[6] = int2;
		float0 = dest[4];
		float3 -= bias;
		(++dest)[0] = float1;

		//Software Cycle 6
		((long*) dest)[6] = int3;
		float1 = dest[4];
		float0 -= bias;
		(++dest)[0] = float2;

		//Software Cycle 5
		float2 = dest[4];
		float1 -= bias;
		(++dest)[0] = float3;

		//Software Cycle 4
		float3 = dest[4];
		float2 -= bias;
		(++dest)[0] = float0;

		//Software Cycle 3
		float3 -= bias;
		(++dest)[0] = float1;

		//Software Cycle 2
		(++dest)[0] = float2;

		//Software Cycle 1
		(++dest)[0] = float3;

		dest++;
	}


	while( count-- )
	{
		register long value = (++src)[0];
		value += exponentMask;
		((long*) dest)[0] = value;
		dest[0] -= bias;
		dest++;
	}
}

// bitDepth may be less than 16, e.g. for low-aligned 12 bit samples
void NativeInt16ToFloat32( signed short *src, float *dest, unsigned int count, int bitDepth )
{
	register float bias;
	register long exponentMask = ((0x97UL - bitDepth) << 23) | 0x8000;	//FP exponent + bias for sign
	register long int0, int1, int2, int3;
	register float float0, float1, float2, float3;
	register unsigned long loopCount;
	union
	{
		float	f;
		long	i;
	} exponent;

	exponent.i = exponentMask;
	bias = exponent.f;

	src--;
	if( count >= 8 )
	{
		//Software Cycle 1
		int0 = (++src)[0];

		//Software Cycle 2
		int1 = (++src)[0];
		int0 += exponentMask;

		//Software Cycle 3
		int2 = (++src)[0];
		int1 += exponentMask;
		((long*) dest)[0] = int0;

		//Software Cycle 4
		int3 = (++src)[0];
		int2 += exponentMask;
		((long*) dest)[1] = int1;
		//delay one loop for the store to complete

		//Software Cycle 5
		int0 = (++src)[0];
		int3 += exponentMask;
		((long*) dest)[2] = int2;
		float0 = dest[0];

		//Software Cycle 6
		int1 = (++src)[0];
		int0 += exponentMask;
		((long*) dest)[3] = int3;
		float1 = dest[1];
		float0 -= bias;

		//Software Cycle 7
		int2 = (++src)[0];
		int1 += exponentMask;
		((long*) dest)[4] = int0;
		float2 = dest[2];
		float1 -= bias;

		dest--;
		//Software Cycle 8
		int3 = (++src)[0];
		int2 += exponentMask;
		((long*) dest)[6] = int1;
		float3 = dest[4];
		float2 -= bias;
		(++dest)[0] = float0;

		count -= 8;
		loopCount = count / 4;
		count &= 3;
		while( loopCount-- )
		{

			//Software Cycle A
			int0 = (++src)[0];
			int3 += exponentMask;
			((long*) dest)[6] = int2;
			float0 = dest[4];
			float3 -= bias;
			(++dest)[0] = float1;

			//Software Cycle B
			int1 = (++src)[0];
			int0 += exponentMask;
			((long*) dest)[6] = int3;
			float1 = dest[4];
			float0 -= bias;
			(++dest)[0] = float2;

			//Software Cycle C
			int2 = (++src)[0];
			int1 += exponentMask;
			((long*) dest)[6] = int0;
			float2 = dest[4];
			float1 -= bias;
			(++dest)[0] = float3;

			//Software Cycle D
			int3 = (++src)[0];
			int2 += exponentMask;
			((long*) dest)[6] = int1;
			float3 = dest[4];
			float2 -= bias;
			(++dest)[0] = float0;
		}

		//Software Cycle 7
		int3 += exponentMask;
		((long*) dest)[6] = int2;
		float0 = dest[4];
		float3 -= bias;
		(++dest)[0] = float1;

		//Software Cycle 6
		((long*) dest)[6] = int3;
		float1 = dest[4];
		float0 -= bias;
		(++dest)[0] = float2;

		//Software Cycle 5
		float2 = dest[4];
		float1 -= bias;
		(++dest)[0] = float3;

		//Software Cycle 4
		float3 = dest[4];
		float2 -= bias;
		(++dest)[0] = float0;

		//Software Cycle 3
		float3 -= bias;
		(++dest)[0] = float1;

		//Software Cycle 2
		(++dest)[0] = float2;

		//Software Cycle 1
		(++dest)[0] = float3;

		dest++;
	}


	while( count-- )
	{
		register long value = (++src)[0];
		value += exponentMask;
		((long*) dest)[0] = value;
		dest[0] -= bias;
		dest++;
	}
}

void NativeInt24ToFloat32( long *src, float *dest, unsigned int count, int bitDepth )
{
	union
	{
		double			d[4];
		unsigned int	i[8];
	} transfer;
	register double			dBias;
	register unsigned int	loopCount, load0SignMask;
	register unsigned long	load0, load1, load2;
	register unsigned long	int0, int1, int2, int3;
	register double			d0, d1, d2, d3;
	register float		f0, f1, f2, f3;

	transfer.i[0] = transfer.i[2] = transfer.i[4] = transfer.i[6] = (0x434UL - bitDepth) << 20; //0x41C00000UL;
	transfer.i[1] = 0x00800000;
	int0 = int1 = int2 = int3 = 0;
	load0SignMask = 0x80000080UL;

	dBias = transfer.d[0];

	src--;
	dest--;

	if( count >= 8 )
	{
		count -= 8;
		loopCount = count / 4;
		count &= 3;

		//Virtual cycle 1
		load0 = (++src)[0];

		//Virtual cycle 2
		load1 = (++src)[0];
		load0 ^= load0SignMask;

		//Virtual cycle 3
		load2 = (++src)[0];
		load1 ^= 0x00008000UL;
		int0 = load0 >> 8;
		int1 = __rlwimi( int1, load0, 16, 8, 15);

		//Virtual cycle 4
		//No load3 -- already loaded last cycle
		load2 ^= 0x00800000UL;
		int1 = __rlwimi( int1, load1, 16, 16, 31);
		int2 = __rlwimi( int2, load1, 8, 8, 23 );
		transfer.i[1] = int0;

		//Virtual cycle 5
		load0 = (++src)[0];
		int2 = __rlwimi( int2, load2, 8, 24, 31 );
		int3 = load2 & 0x00FFFFFF;
		transfer.i[3] = int1;

		//Virtual cycle 6
		load1 = (++src)[0];
		load0 ^= load0SignMask;
		transfer.i[5] = int2;
		d0 = transfer.d[0];

		//Virtual cycle 7
		load2 = (++src)[0];
		load1 ^= 0x00008000UL;
		int0 = load0 >> 8;
		int1 = __rlwimi( int1, load0, 16, 8, 15 );
		transfer.i[7] = int3;
		d1 = transfer.d[1];
		d0 -= dBias;

		//Virtual cycle 8
		//No load3 -- already loaded last cycle
		load2 ^= 0x00800000UL;
		int1 = __rlwimi( int1, load1, 16, 16, 31);
		int2 = __rlwimi( int2, load1, 8, 8, 23 );
		transfer.i[1] = int0;
		d2 = transfer.d[2];
		d1 -= dBias;
		f0 = d0;

		while( loopCount-- )
		{
			//Virtual cycle A
			load0 = (++src)[0];
			int2 = __rlwimi( int2, load2, 8, 24, 31 );
			int3 = load2 & 0x00FFFFFF;
			transfer.i[3] = int1;
			d3 = transfer.d[3];
			d2 -= dBias;
			f1 = d1;
			(++dest)[0] = f0;

			//Virtual cycle B
			load1 = (++src)[0];
			load0 ^= load0SignMask;
			transfer.i[5] = int2;
			d0 = transfer.d[0];
			d3 -= dBias;
			f2 = d2;
			(++dest)[0] = f1;

			//Virtual cycle C
			load2 = (++src)[0];
			load1 ^= 0x00008000UL;
			int0 = load0 >> 8;
			int1 = __rlwimi( int1, load0, 16, 8, 15 );
			transfer.i[7] = int3;
			d1 = transfer.d[1];
			d0 -= dBias;
			f3 = d3;
			(++dest)[0] = f2;

			//Virtual cycle D
			load2 ^= 0x00800000UL;
			int1 = __rlwimi( int1, load1, 16, 16, 31);
			int2 = __rlwimi( int2, load1, 8, 8, 23 );
			transfer.i[1] = int0;
			d2 = transfer.d[2];
			d1 -= dBias;
			f0 = d0;
			(++dest)[0] = f3;
		}

		//Virtual cycle 8
		int2 = __rlwimi( int2, load2, 8, 24, 31 );
		int3 = load2 & 0x00FFFFFF;
		transfer.i[3] = int1;
		d3 = transfer.d[3];
		d2 -= dBias;
		f1 = d1;
		(++dest)[0] = f0;

		//Virtual cycle 7
		transfer.i[5] = int2;
		d0 = transfer.d[0];
		d3 -= dBias;
		f2 = d2;
		(++dest)[0] = f1;

		//Virtual cycle 6
		transfer.i[7] = int3;
		d1 = transfer.d[1];
		d0 -= dBias;
		f3 = d3;
		(++dest)[0] = f2;

		//Virtual cycle 5
		d2 = transfer.d[2];
		d1 -= dBias;
		f0 = d0;
		(++dest)[0] = f3;

		//Virtual cycle 4
		d3 = transfer.d[3];
		d2 -= dBias;
		f1 = d1;
		(++dest)[0] = f0;

		//Virtual cycle 3
		d3 -= dBias;
		f2 = d2;
		(++dest)[0] = f1;

		//Virtual cycle 2
		f3 = d3;
		(++dest)[0] = f2;

		//Virtual cycle 1
		(++dest)[0] = f3;
	}

	src = (long*) ((char*) src + 1 );
	while( count-- )
	{
		int0 = ((unsigned char*)(src = (long*)( (char*) src + 3 )))[0];
		int1 = ((unsigned short*)( (char*) src + 1 ))[0];
		int0 ^= 0x00000080UL;
		int1 = __rlwimi( int1, int0, 16, 8, 15 );
		transfer.i[1] = int1;

		d0 = transfer.d[0];
		d0 -= dBias;
		f0 = d0;
		(++dest)[0] = f0;
   }
}

// bitDepth may be less than 32, e.g. for 24 bits low-aligned in 32-bit words
void NativeInt32ToFloat32( long *src, float *dest, unsigned int count, int bitDepth )
{
	union
	{
		double			d[4];
		unsigned int	i[8];
	}transfer;
	register double 		dBias;
	register unsigned int 	loopCount;
	register long			int0, int1, int2, int3;
	register double			d0, d1, d2, d3;
	register float			f0, f1, f2, f3;

	transfer.i[0] = transfer.i[2] = transfer.i[4] = transfer.i[6] = (0x434UL - bitDepth) << 20;
		//0x41400000UL;
	transfer.i[1] = 0x80000000;

	dBias = transfer.d[0];

	src--;
	dest--;

	if( count >= 8 )
	{
		count -= 8;
		loopCount = count / 4;
		count &= 3;

		//Virtual cycle 1
		int0 = (++src)[0];

		//Virtual cycle 2
		int1 = (++src)[0];
		int0 ^= 0x80000000UL;

		//Virtual cycle 3
		int2 = (++src)[0];
		int1 ^= 0x80000000UL;
		transfer.i[1] = int0;

		//Virtual cycle 4
		int3 = (++src)[0];
		int2 ^= 0x80000000UL;
		transfer.i[3] = int1;

		//Virtual cycle 5
		int0 = (++src)[0];
		int3 ^= 0x80000000UL;
		transfer.i[5] = int2;

		//Virtual cycle 6
		int1 = (++src)[0];
		int0 ^= 0x80000000UL;
		transfer.i[7] = int3;
		d0 = transfer.d[0];

		//Virtual cycle 7
		int2 = (++src)[0];
		int1 ^= 0x80000000UL;
		transfer.i[1] = int0;
		d1 = transfer.d[1];
		d0 -= dBias;

		//Virtual cycle 8
		int3 = (++src)[0];
		int2 ^= 0x80000000UL;
		transfer.i[3] = int1;
		d2 = transfer.d[2];
		d1 -= dBias;
		f0 = d0;

		while( loopCount-- )
		{
			//Virtual cycle A
			int0 = (++src)[0];
			int3 ^= 0x80000000UL;
			transfer.i[5] = int2;
			d3 = transfer.d[3];
			d2 -= dBias;
			f1 = d1;
			(++dest)[0] = f0;

			//Virtual cycle B
			int1 = (++src)[0];
			int0 ^= 0x80000000UL;
			transfer.i[7] = int3;
			d0 = transfer.d[0];
			d3 -= dBias;
			f2 = d2;
			(++dest)[0] = f1;

			//Virtual cycle C
			int2 = (++src)[0];
			int1 ^= 0x80000000UL;
			transfer.i[1] = int0;
			d1 = transfer.d[1];
			d0 -= dBias;
			f3 = d3;
			(++dest)[0] = f2;

			//Virtual cycle D
			int3 = (++src)[0];
			int2 ^= 0x80000000UL;
			transfer.i[3] = int1;
			d2 = transfer.d[2];
			d1 -= dBias;
			f0 = d0;
			(++dest)[0] = f3;
		}

		//Virtual cycle 8
		int3 ^= 0x80000000UL;
		transfer.i[5] = int2;
		d3 = transfer.d[3];
		d2 -= dBias;
		f1 = d1;
		(++dest)[0] = f0;

		//Virtual cycle 7
		transfer.i[7] = int3;
		d0 = transfer.d[0];
		d3 -= dBias;
		f2 = d2;
		(++dest)[0] = f1;

		//Virtual cycle 6
		d1 = transfer.d[1];
		d0 -= dBias;
		f3 = d3;
		(++dest)[0] = f2;

		//Virtual cycle 5
		d2 = transfer.d[2];
		d1 -= dBias;
		f0 = d0;
		(++dest)[0] = f3;

		//Virtual cycle 4
		d3 = transfer.d[3];
		d2 -= dBias;
		f1 = d1;
		(++dest)[0] = f0;

		//Virtual cycle 3
		d3 -= dBias;
		f2 = d2;
		(++dest)[0] = f1;

		//Virtual cycle 2
		f3 = d3;
		(++dest)[0] = f2;

		//Virtual cycle 1
		(++dest)[0] = f3;
	}


	while( count-- )
	{
		int0 = (++src)[0];
		int0 ^= 0x80000000UL;
		transfer.i[1] = int0;

		d0 = transfer.d[0];
		d0 -= dBias;
		f0 = d0;
		(++dest)[0] = f0;
	}
}

// bitDepth may be less than 16, e.g. for low-aligned 12 bit samples
void NativeInt16ToFloat32Gain( signed short *src, float *dest, unsigned int count, int bitDepth, float* inGainLPtr, float* inGainRPtr )
{
	register float bias, gainL, gainR;
	register long exponentMask = ((0x97UL - bitDepth) << 23) | 0x8000;	//FP exponent + bias for sign
	register long int0, int1, int2, int3;
	register float float0, float1, float2, float3;
	register unsigned long loopCount;
	union
	{
		float	f;
		long	i;
	} exponent;

	exponent.i = exponentMask;
	bias = exponent.f;

	if (inGainLPtr) {
		gainL = *inGainLPtr;
	} else {
		gainL = 1.0f;
	}
	if (inGainRPtr) {
		gainR = *inGainRPtr;	
	} else {
		gainR = 1.0f;
	}
	
	src--;
	if( count >= 8 )
	{
		//Software Cycle 1
		int0 = (++src)[0];

		//Software Cycle 2
		int1 = (++src)[0];
		int0 += exponentMask;

		//Software Cycle 3
		int2 = (++src)[0];
		int1 += exponentMask;
		((long*) dest)[0] = int0;

		//Software Cycle 4
		int3 = (++src)[0];
		int2 += exponentMask;
		((long*) dest)[1] = int1;
		//delay one loop for the store to complete

		//Software Cycle 5
		int0 = (++src)[0];
		int3 += exponentMask;
		((long*) dest)[2] = int2;
		float0 = dest[0];

		//Software Cycle 6
		int1 = (++src)[0];
		int0 += exponentMask;
		((long*) dest)[3] = int3;
		float1 = dest[1];
		float0 -= bias;

		//Software Cycle 7
		int2 = (++src)[0];
		int1 += exponentMask;
		((long*) dest)[4] = int0;
		float0 *= gainL;
		float2 = dest[2];
		float1 -= bias;

		dest--;
		//Software Cycle 8
		int3 = (++src)[0];
		int2 += exponentMask;
		((long*) dest)[6] = int1;
		float1 *= gainR;
		float3 = dest[4];
		float2 -= bias;
		(++dest)[0] = float0;

		count -= 8;
		loopCount = count / 4;
		count &= 3;
		while( loopCount-- )
		{

			//Software Cycle A
			int0 = (++src)[0];
			int3 += exponentMask;
			((long*) dest)[6] = int2;
			float2 *= gainL;
			float0 = dest[4];
			float3 -= bias;
			(++dest)[0] = float1;

			//Software Cycle B
			int1 = (++src)[0];
			int0 += exponentMask;
			((long*) dest)[6] = int3;
			float3 *= gainR;
			float1 = dest[4];
			float0 -= bias;
			(++dest)[0] = float2;

			//Software Cycle C
			int2 = (++src)[0];
			int1 += exponentMask;
			((long*) dest)[6] = int0;
			float0 *= gainL;
			float2 = dest[4];
			float1 -= bias;
			(++dest)[0] = float3;

			//Software Cycle D
			int3 = (++src)[0];
			int2 += exponentMask;
			((long*) dest)[6] = int1;
			float1 *= gainR;
			float3 = dest[4];
			float2 -= bias;
			(++dest)[0] = float0;
		}

		//Software Cycle 7
		int3 += exponentMask;
		((long*) dest)[6] = int2;
		float2 *= gainL;
		float0 = dest[4];
		float3 -= bias;
		(++dest)[0] = float1;

		//Software Cycle 6
		((long*) dest)[6] = int3;
		float3 *= gainR;
		float1 = dest[4];
		float0 -= bias;
		(++dest)[0] = float2;

		//Software Cycle 5
		float0 *= gainL;
		float2 = dest[4];
		float1 -= bias;
		(++dest)[0] = float3;

		//Software Cycle 4
		float1 *= gainR;
		float3 = dest[4];
		float2 -= bias;
		(++dest)[0] = float0;

		//Software Cycle 3
		float2 *= gainL;
		float3 -= bias;
		(++dest)[0] = float1;

		//Software Cycle 2
		float3 *= gainR;
		(++dest)[0] = float2;

		//Software Cycle 1
		(++dest)[0] = float3;

		dest++;
	}

	loopCount = count/2;
	while( loopCount-- )
	{
		register long value = (++src)[0];
		value += exponentMask;
		((long*) dest)[0] = value;
		value = (++src)[0];
		dest[0] -= bias;
		dest[0] *= gainL;
		value += exponentMask;
		dest++;
		((long*) dest)[0] = value;
		dest[0] -= bias;
		dest[0] *= gainR;
		dest++;
	}
	if (count % 2) {
		register long value = (++src)[0];
		value += exponentMask;
		((long*) dest)[0] = value;
		dest[0] -= bias;
		dest[0] *= gainL;
	}
}

// bitDepth may be less than 16, e.g. for low-aligned 12 bit samples
void NativeInt16ToFloat32CopyRightToLeft( signed short *src, float *dest, unsigned int count, int bitDepth )
{
	register float bias;
	register long exponentMask = ((0x97UL - bitDepth) << 23) | 0x8000;	//FP exponent + bias for sign
	register long int0, int1, int2, int3;
	register float float0, float1, float2, float3;
	register unsigned long loopCount;
	union
	{
		float	f;
		long	i;
	} exponent;

	exponent.i = exponentMask;
	bias = exponent.f;

	if( count >= 8 )
	{
		//Software Cycle 1
		int0 = (++src)[0];		// right 1

		//Software Cycle 2
		int1 = (src++)[0];		// reuse right 1, skip left 1
		int0 += exponentMask;

		//Software Cycle 3
		int2 = (++src)[0];		// right 2
		int1 += exponentMask;
		((long*) dest)[0] = int0;

		//Software Cycle 4
		int3 = (src++)[0];		// reuse right 2, skip left 2
		int2 += exponentMask;
		((long*) dest)[1] = int1;
		//delay one loop for the store to complete

		//Software Cycle 5
		int0 = (++src)[0];		// right 3
		int3 += exponentMask;
		((long*) dest)[2] = int2;
		float0 = dest[0];

		//Software Cycle 6
		int1 = (src++)[0];		// reuse right 3, skip left 3
		int0 += exponentMask;
		((long*) dest)[3] = int3;
		float1 = dest[1];
		float0 -= bias;

		//Software Cycle 7
		int2 = (++src)[0];		// right 4
		int1 += exponentMask;
		((long*) dest)[4] = int0;
		float2 = dest[2];
		float1 -= bias;

		dest--;
		//Software Cycle 8
		int3 = (src++)[0];		// reuse left 4, skip right 4
		int2 += exponentMask;
		((long*) dest)[6] = int1;
		float3 = dest[4];
		float2 -= bias;
		(++dest)[0] = float0;

		count -= 8;
		loopCount = count / 4;
		count &= 3;
		while( loopCount-- )
		{

			//Software Cycle A
			int0 = (++src)[0];
			int3 += exponentMask;
			((long*) dest)[6] = int2;
			float0 = dest[4];
			float3 -= bias;
			(++dest)[0] = float1;

			//Software Cycle B
			int1 = (src++)[0];
			int0 += exponentMask;
			((long*) dest)[6] = int3;
			float1 = dest[4];
			float0 -= bias;
			(++dest)[0] = float2;

			//Software Cycle C
			int2 = (++src)[0];
			int1 += exponentMask;
			((long*) dest)[6] = int0;
			float2 = dest[4];
			float1 -= bias;
			(++dest)[0] = float3;

			//Software Cycle D
			int3 = (src++)[0];
			int2 += exponentMask;
			((long*) dest)[6] = int1;
			float3 = dest[4];
			float2 -= bias;
			(++dest)[0] = float0;
		}

		//Software Cycle 7
		int3 += exponentMask;
		((long*) dest)[6] = int2;
		float0 = dest[4];
		float3 -= bias;
		(++dest)[0] = float1;

		//Software Cycle 6
		((long*) dest)[6] = int3;
		float1 = dest[4];
		float0 -= bias;
		(++dest)[0] = float2;

		//Software Cycle 5
		float2 = dest[4];
		float1 -= bias;
		(++dest)[0] = float3;

		//Software Cycle 4
		float3 = dest[4];
		float2 -= bias;
		(++dest)[0] = float0;

		//Software Cycle 3
		float3 -= bias;
		(++dest)[0] = float1;

		//Software Cycle 2
		(++dest)[0] = float2;

		//Software Cycle 1
		(++dest)[0] = float3;

		dest++;
	}

	loopCount = count/2;
	while( loopCount-- )
	{
		register long value = (++src)[0];
		value += exponentMask;
		((long*) dest)[0] = value;
		dest[0] -= bias;
		dest++;
		++src;
		((long*) dest)[0] = value;
		dest[0] -= bias;
		dest++;
	}
	if (count % 2) {
		register long value = (++src)[0];
		value += exponentMask;
		((long*) dest)[0] = value;
		dest[0] -= bias;
	}
}

// bitDepth may be less than 32, e.g. for 24 bits low-aligned in 32-bit words
void NativeInt32ToFloat32Gain( signed long *src, float *dest, unsigned int count, int bitDepth, float* inGainLPtr, float* inGainRPtr )
{
	union
	{
		double			d[4];
		unsigned int	i[8];
	} transfer;
	register double 		dBias, gainL, gainR;
	register unsigned int 	loopCount;
	register long			int0, int1, int2, int3;
	register double			d0, d1, d2, d3;
	register float			f0, f1, f2, f3;

	transfer.i[0] = transfer.i[2] = transfer.i[4] = transfer.i[6] = (0x434UL - bitDepth) << 20;
		//0x41400000UL;
	transfer.i[1] = 0x80000000;

	dBias = transfer.d[0];

	if (inGainLPtr) {
		gainL = *inGainLPtr;
	} else {
		gainL = 1.0f;
	}
	if (inGainRPtr) {
		gainR = *inGainRPtr;	
	} else {
		gainR = 1.0f;
	}

	src--;
	dest--;

	if( count >= 8 )
	{
		count -= 8;
		loopCount = count / 4;
		count &= 3;

		//Virtual cycle 1
		int0 = (++src)[0];

		//Virtual cycle 2
		int1 = (++src)[0];
		int0 ^= 0x80000000UL;

		//Virtual cycle 3
		int2 = (++src)[0];
		int1 ^= 0x80000000UL;
		transfer.i[1] = int0;

		//Virtual cycle 4
		int3 = (++src)[0];
		int2 ^= 0x80000000UL;
		transfer.i[3] = int1;

		//Virtual cycle 5
		int0 = (++src)[0];
		int3 ^= 0x80000000UL;
		transfer.i[5] = int2;

		//Virtual cycle 6
		int1 = (++src)[0];
		int0 ^= 0x80000000UL;
		transfer.i[7] = int3;
		d0 = transfer.d[0];

		//Virtual cycle 7
		int2 = (++src)[0];
		int1 ^= 0x80000000UL;
		transfer.i[1] = int0;
		d1 = transfer.d[1];
		d0 -= dBias;

		//Virtual cycle 8
		int3 = (++src)[0];
		int2 ^= 0x80000000UL;
		transfer.i[3] = int1;
		d2 = transfer.d[2];
		d1 -= dBias;
		f0 = d0*gainL;

		while( loopCount-- )
		{
			//Virtual cycle A
			int0 = (++src)[0];
			int3 ^= 0x80000000UL;
			transfer.i[5] = int2;
			d3 = transfer.d[3];
			d2 -= dBias;
			f1 = d1*gainR;
			(++dest)[0] = f0;

			//Virtual cycle B
			int1 = (++src)[0];
			int0 ^= 0x80000000UL;
			transfer.i[7] = int3;
			d0 = transfer.d[0];
			d3 -= dBias;
			f2 = d2*gainL;
			(++dest)[0] = f1;

			//Virtual cycle C
			int2 = (++src)[0];
			int1 ^= 0x80000000UL;
			transfer.i[1] = int0;
			d1 = transfer.d[1];
			d0 -= dBias;
			f3 = d3*gainR;
			(++dest)[0] = f2;

			//Virtual cycle D
			int3 = (++src)[0];
			int2 ^= 0x80000000UL;
			transfer.i[3] = int1;
			d2 = transfer.d[2];
			d1 -= dBias;
			f0 = d0*gainL;
			(++dest)[0] = f3;
		}

		//Virtual cycle 8
		int3 ^= 0x80000000UL;
		transfer.i[5] = int2;
		d3 = transfer.d[3];
		d2 -= dBias;
		f1 = d1*gainR;
		(++dest)[0] = f0;

		//Virtual cycle 7
		transfer.i[7] = int3;
		d0 = transfer.d[0];
		d3 -= dBias;
		f2 = d2*gainL;
		(++dest)[0] = f1;

		//Virtual cycle 6
		d1 = transfer.d[1];
		d0 -= dBias;
		f3 = d3*gainR;
		(++dest)[0] = f2;

		//Virtual cycle 5
		d2 = transfer.d[2];
		d1 -= dBias;
		f0 = d0*gainL;
		(++dest)[0] = f3;

		//Virtual cycle 4
		d3 = transfer.d[3];
		d2 -= dBias;
		f1 = d1*gainR;
		(++dest)[0] = f0;

		//Virtual cycle 3
		d3 -= dBias;
		f2 = d2*gainL;
		(++dest)[0] = f1;

		//Virtual cycle 2
		f3 = d3*gainR;
		(++dest)[0] = f2;

		//Virtual cycle 1
		(++dest)[0] = f3;
	}


	loopCount = count/2;
	while( loopCount-- )
	{
		int0 = (++src)[0];
		int0 ^= 0x80000000UL;
		transfer.i[1] = int0;

		d0 = transfer.d[0];
		d0 -= dBias;
		f0 = d0*gainL;
		(++dest)[0] = f0;

		int0 = (++src)[0];
		int0 ^= 0x80000000UL;
		transfer.i[1] = int0;

		d0 = transfer.d[0];
		d0 -= dBias;
		f0 = d0*gainR;
		(++dest)[0] = f0;
	}
	if (count % 2) {
		int0 = (++src)[0];
		int0 ^= 0x80000000UL;
		transfer.i[1] = int0;

		d0 = transfer.d[0];
		d0 -= dBias;
		f0 = d0*gainL;
		(++dest)[0] = f0;
	}
}


// bitDepth may be less than 16, e.g. for low-aligned 12 bit samples
void SwapInt16ToFloat32( signed short *src, float *dest, unsigned int count, int bitDepth )
{
	register float bias;
	register long exponentMask = ((0x97UL - bitDepth) << 23) | 0x8000;	//FP exponent + bias for sign
	register long int0, int1, int2, int3;
	register float float0, float1, float2, float3;
	register unsigned long loopCount;
	union
	{
		float	f;
		long	i;
	}exponent;

	exponent.i = exponentMask;
	bias = exponent.f;

	src--;
	if( count >= 8 )
	{
		//Software Cycle 1
		int0 = __lhbrx(0, ++src);

		//Software Cycle 2
		int1 = __lhbrx(0, ++src);
		int0 += exponentMask;

		//Software Cycle 3
		int2 = __lhbrx(0, ++src);
		int1 += exponentMask;
		((long*) dest)[0] = int0;

		//Software Cycle 4
		int3 = __lhbrx(0, ++src);
		int2 += exponentMask;
		((long*) dest)[1] = int1;
		//delay one loop for the store to complete

		//Software Cycle 5
		int0 = __lhbrx(0, ++src);
		int3 += exponentMask;
		((long*) dest)[2] = int2;
		float0 = dest[0];

		//Software Cycle 6
		int1 = __lhbrx(0, ++src);
		int0 += exponentMask;
		((long*) dest)[3] = int3;
		float1 = dest[1];
		float0 -= bias;

		//Software Cycle 7
		int2 = __lhbrx(0, ++src);
		int1 += exponentMask;
		((long*) dest)[4] = int0;
		float2 = dest[2];
		float1 -= bias;

		dest--;
		//Software Cycle 8
		int3 = __lhbrx(0, ++src);
		int2 += exponentMask;
		((long*) dest)[6] = int1;
		float3 = dest[4];
		float2 -= bias;
		(++dest)[0] = float0;

		count -= 8;
		loopCount = count / 4;
		count &= 3;
		while( loopCount-- )
		{

			//Software Cycle A
			int0 = __lhbrx(0, ++src);
			int3 += exponentMask;
			((long*) dest)[6] = int2;
			float0 = dest[4];
			float3 -= bias;
			(++dest)[0] = float1;

			//Software Cycle B
			int1 = __lhbrx(0, ++src);
			int0 += exponentMask;
			((long*) dest)[6] = int3;
			float1 = dest[4];
			float0 -= bias;
			(++dest)[0] = float2;

			//Software Cycle C
			int2 = __lhbrx(0, ++src);
			int1 += exponentMask;
			((long*) dest)[6] = int0;
			float2 = dest[4];
			float1 -= bias;
			(++dest)[0] = float3;

			//Software Cycle D
			int3 = __lhbrx(0, ++src);
			int2 += exponentMask;
			((long*) dest)[6] = int1;
			float3 = dest[4];
			float2 -= bias;
			(++dest)[0] = float0;
		}

		//Software Cycle 7
		int3 += exponentMask;
		((long*) dest)[6] = int2;
		float0 = dest[4];
		float3 -= bias;
		(++dest)[0] = float1;

		//Software Cycle 6
		((long*) dest)[6] = int3;
		float1 = dest[4];
		float0 -= bias;
		(++dest)[0] = float2;

		//Software Cycle 5
		float2 = dest[4];
		float1 -= bias;
		(++dest)[0] = float3;

		//Software Cycle 4
		float3 = dest[4];
		float2 -= bias;
		(++dest)[0] = float0;

		//Software Cycle 3
		float3 -= bias;
		(++dest)[0] = float1;

		//Software Cycle 2
		(++dest)[0] = float2;

		//Software Cycle 1
		(++dest)[0] = float3;

		dest++;
	}


	while( count-- )
	{
		register long value = __lhbrx(0, ++src);
		value += exponentMask;
		((long*) dest)[0] = value;
		dest[0] -= bias;
		dest++;
	}
}

// CAUTION: bitDepth is ignored
void SwapInt24ToFloat32( long *src, float *dest, unsigned int count, int bitDepth )
{
	union
	{
		double		d[4];
		unsigned int	i[8];
	}transfer;
	register double			dBias;
	register unsigned int	loopCount, load2SignMask;
	register unsigned long	load0, load1, load2;
	register unsigned long	int0, int1, int2, int3;
	register double			d0, d1, d2, d3;
	register float		f0, f1, f2, f3;

	transfer.i[0] = transfer.i[2] = transfer.i[4] = transfer.i[6] = 0x41400000UL;
	transfer.i[1] = 0x80000000;
	int0 = int1 = int2 = int3 = 0;
	load2SignMask = 0x80000080UL;

	dBias = transfer.d[0];

	src--;
	dest--;

	if( count >= 8 )
	{
		count -= 8;
		loopCount = count / 4;
		count &= 3;

		//Virtual cycle 1
		load0 = (++src)[0];

		//Virtual cycle 2
		load1 = (++src)[0];
		load0 ^= 0x00008000;

		//Virtual cycle 3
		load2 = (++src)[0];
		load1 ^= 0x00800000UL;
		int0 = load0 >> 8;
		int1 = __rlwimi( int1, load0, 16, 8, 15);

		//Virtual cycle 4
		//No load3 -- already loaded last cycle
		load2 ^= load2SignMask;
		int1 = __rlwimi( int1, load1, 16, 16, 31);
		int2 = __rlwimi( int2, load1, 8, 8, 23 );
		__stwbrx( int0, 0, &transfer.i[1]);

		//Virtual cycle 5
		load0 = (++src)[0];
		int2 = __rlwimi( int2, load2, 8, 24, 31 );
		int3 = load2 & 0x00FFFFFF;
		__stwbrx( int1, 0, &transfer.i[3]);

		//Virtual cycle 6
		load1 = (++src)[0];
		load0 ^= 0x00008000;
		__stwbrx( int2, 0, &transfer.i[5]);
		d0 = transfer.d[0];

		//Virtual cycle 7
		load2 = (++src)[0];
		load1 ^= 0x00800000UL;
		int0 = load0 >> 8;
		int1 = __rlwimi( int1, load0, 16, 8, 15 );
		__stwbrx( int3, 0, &transfer.i[7]);
		d1 = transfer.d[1];
		d0 -= dBias;

		//Virtual cycle 8
		//No load3 -- already loaded last cycle
		load2 ^= load2SignMask;
		int1 = __rlwimi( int1, load1, 16, 16, 31);
		int2 = __rlwimi( int2, load1, 8, 8, 23 );
		__stwbrx( int0, 0, &transfer.i[1]);
		d2 = transfer.d[2];
		d1 -= dBias;
		f0 = d0;

		while( loopCount-- )
		{
			//Virtual cycle A
			load0 = (++src)[0];
			int2 = __rlwimi( int2, load2, 8, 24, 31 );
			int3 = load2 & 0x00FFFFFF;
			__stwbrx( int1, 0, &transfer.i[3]);
			d3 = transfer.d[3];
			d2 -= dBias;
			f1 = d1;
			(++dest)[0] = f0;

			//Virtual cycle B
			load1 = (++src)[0];
			load0 ^= 0x00008000;
			__stwbrx( int2, 0, &transfer.i[5]);
			d0 = transfer.d[0];
			d3 -= dBias;
			f2 = d2;
			(++dest)[0] = f1;

			//Virtual cycle C
			load2 = (++src)[0];
			load1 ^= 0x00800000UL;
			int0 = load0 >> 8;
			int1 = __rlwimi( int1, load0, 16, 8, 15 );
			__stwbrx( int3, 0, &transfer.i[7]);
			d1 = transfer.d[1];
			d0 -= dBias;
			f3 = d3;
			(++dest)[0] = f2;

			//Virtual cycle D
			load2 ^= load2SignMask;
			int1 = __rlwimi( int1, load1, 16, 16, 31);
			int2 = __rlwimi( int2, load1, 8, 8, 23 );
			__stwbrx( int0, 0, &transfer.i[1]);
			d2 = transfer.d[2];
			d1 -= dBias;
			f0 = d0;
			(++dest)[0] = f3;
		}

		//Virtual cycle 8
		int2 = __rlwimi( int2, load2, 8, 24, 31 );
		int3 = load2 & 0x00FFFFFF;
		__stwbrx( int1, 0, &transfer.i[3]);
		d3 = transfer.d[3];
		d2 -= dBias;
		f1 = d1;
		(++dest)[0] = f0;

		//Virtual cycle 7
		__stwbrx( int2, 0, &transfer.i[5]);
		d0 = transfer.d[0];
		d3 -= dBias;
		f2 = d2;
		(++dest)[0] = f1;

		//Virtual cycle 6
		__stwbrx( int3, 0, &transfer.i[7]);
		d1 = transfer.d[1];
		d0 -= dBias;
		f3 = d3;
		(++dest)[0] = f2;

		//Virtual cycle 5
		d2 = transfer.d[2];
		d1 -= dBias;
		f0 = d0;
		(++dest)[0] = f3;

		//Virtual cycle 4
		d3 = transfer.d[3];
		d2 -= dBias;
		f1 = d1;
		(++dest)[0] = f0;

		//Virtual cycle 3
		d3 -= dBias;
		f2 = d2;
		(++dest)[0] = f1;

		//Virtual cycle 2
		f3 = d3;
		(++dest)[0] = f2;

		//Virtual cycle 1
		(++dest)[0] = f3;
	}

	if( count > 0 )
	{

		int1 = ((unsigned char*) src)[6];
		int0 = ((unsigned short*)(++src))[0];
		int1 ^= 0x80;
		int1 = __rlwimi( int1, int0, 8, 8, 23 );
		__stwbrx( int1, 0, &transfer.i[1]);
		d0 = transfer.d[0];
		d0 -= dBias;
		f0 = d0;
		(++dest)[0] = f0;

		src = (long*) ((char*)src - 1 );
		while( --count )
		{
			int0 = (src = (long*)( (char*) src + 3 ))[0];
			int0 ^= 0x80UL;
			int0 &= 0x00FFFFFFUL;
			__stwbrx( int0, 0, &transfer.i[1]);

			d0 = transfer.d[0];
			d0 -= dBias;
			f0 = d0;
			(++dest)[0] = f0;
		}
	}
}

// bitDepth may be less than 32, e.g. for 24 bits low-aligned in 32-bit words
void SwapInt32ToFloat32( long *src, float *dest, unsigned int count, int bitDepth )
{
	union
	{
		double		d[4];
		unsigned int	i[8];
	}transfer;
	register double dBias;
	register unsigned int loopCount;
	register long	int0, int1, int2, int3;
	register double		d0, d1, d2, d3;
	register float	f0, f1, f2, f3;

	transfer.i[0] = transfer.i[2] = transfer.i[4] = transfer.i[6] = (0x434UL - bitDepth) << 20;
		//0x41400000UL;
	transfer.i[1] = 0x80000000;

	dBias = transfer.d[0];

	src--;
	dest--;

	if( count >= 8 )
	{
		count -= 8;
		loopCount = count / 4;
		count &= 3;

		//Virtual cycle 1
		int0 = __lwbrx( 0, ++src);

		//Virtual cycle 2
		int1 = __lwbrx( 0, ++src);
		int0 ^= 0x80000000UL;

		//Virtual cycle 3
		int2 = __lwbrx( 0, ++src);
		int1 ^= 0x80000000UL;
		transfer.i[1] = int0;

		//Virtual cycle 4
		int3 = __lwbrx( 0, ++src);
		int2 ^= 0x80000000UL;
		transfer.i[3] = int1;

		//Virtual cycle 5
		int0 = __lwbrx( 0, ++src);
		int3 ^= 0x80000000UL;
		transfer.i[5] = int2;

		//Virtual cycle 6
		int1 = __lwbrx( 0, ++src);
		int0 ^= 0x80000000UL;
		transfer.i[7] = int3;
		d0 = transfer.d[0];

		//Virtual cycle 7
		int2 = __lwbrx( 0, ++src);
		int1 ^= 0x80000000UL;
		transfer.i[1] = int0;
		d1 = transfer.d[1];
		d0 -= dBias;

		//Virtual cycle 8
		int3 = __lwbrx( 0, ++src);
		int2 ^= 0x80000000UL;
		transfer.i[3] = int1;
		d2 = transfer.d[2];
		d1 -= dBias;
		f0 = d0;

		while( loopCount-- )
		{
			//Virtual cycle A
			int0 = __lwbrx( 0, ++src);
			int3 ^= 0x80000000UL;
			transfer.i[5] = int2;
			d3 = transfer.d[3];
			d2 -= dBias;
			f1 = d1;
			(++dest)[0] = f0;

			//Virtual cycle B
			int1 = __lwbrx( 0, ++src);
			int0 ^= 0x80000000UL;
			transfer.i[7] = int3;
			d0 = transfer.d[0];
			d3 -= dBias;
			f2 = d2;
			(++dest)[0] = f1;

			//Virtual cycle C
			int2 = __lwbrx( 0, ++src);
			int1 ^= 0x80000000UL;
			transfer.i[1] = int0;
			d1 = transfer.d[1];
			d0 -= dBias;
			f3 = d3;
			(++dest)[0] = f2;

			//Virtual cycle D
			int3 = __lwbrx( 0, ++src);
			int2 ^= 0x80000000UL;
			transfer.i[3] = int1;
			d2 = transfer.d[2];
			d1 -= dBias;
			f0 = d0;
			(++dest)[0] = f3;
		}

		//Virtual cycle 8
		int3 ^= 0x80000000UL;
		transfer.i[5] = int2;
		d3 = transfer.d[3];
		d2 -= dBias;
		f1 = d1;
		(++dest)[0] = f0;

		//Virtual cycle 7
		transfer.i[7] = int3;
		d0 = transfer.d[0];
		d3 -= dBias;
		f2 = d2;
		(++dest)[0] = f1;

		//Virtual cycle 6
		d1 = transfer.d[1];
		d0 -= dBias;
		f3 = d3;
		(++dest)[0] = f2;

		//Virtual cycle 5
		d2 = transfer.d[2];
		d1 -= dBias;
		f0 = d0;
		(++dest)[0] = f3;

		//Virtual cycle 4
		d3 = transfer.d[3];
		d2 -= dBias;
		f1 = d1;
		(++dest)[0] = f0;

		//Virtual cycle 3
		d3 -= dBias;
		f2 = d2;
		(++dest)[0] = f1;

		//Virtual cycle 2
		f3 = d3;
		(++dest)[0] = f2;

		//Virtual cycle 1
		(++dest)[0] = f3;
	}


	while( count-- )
	{
		int0 = __lwbrx( 0, ++src);
		int0 ^= 0x80000000UL;
		transfer.i[1] = int0;

		d0 = transfer.d[0];
		d0 -= dBias;
		f0 = d0;
		(++dest)[0] = f0;
	}
}

void Float32ToInt8( float *src, SInt8 *dst, unsigned int count )
{
	register double		scale = 2147483648.0;
	register double		round = 128.0;
	unsigned long		loopCount = count / 4;
	long				buffer[2];
	register float		startingFloat;
	register double 	scaled;
	register double 	converted;
	register SInt8		copy;

	//
	//	The fastest way to do this is to set up a staggered loop that models a 7 stage virtual pipeline:
	//
	//		stage 1:		load the src value
	//		stage 2:		scale it to LONG_MIN...LONG_MAX and add a rounding value to it
	//		stage 3:		convert it to an integer within the FP register
	//		stage 4:		store that to the stack
	//		stage 5:		 (do nothing to let the store complete)
	//		stage 6:		load the high half word from the stack
	//		stage 7:		store it to the destination
	//
	//	We set it up so that at any given time 7 different pieces of data are being worked on at a time.
	//	Because of the do nothing stage, the inner loop had to be unrolled by one, so in actuality, each
	//	inner loop iteration represents two virtual clock cycles that push data through our virtual pipeline.
	//
	//	The reason why this works is that this allows us to break data dependency chains and insert 5 real 
	//	operations in between every virtual pipeline stage. This means 5 instructions between each data 
	//	dependency, which is just enough to keep all of our real pipelines happy. The data flow follows 
	//	standard pipeline diagrams:
	//
	//					stage1	stage2	stage3	stage4	stage5	stage6	stage7
	//	virtual cycle 1:	data1	-		-		-		-		-		-
	//	virtual cycle 2:	data2	data1	-		-		-		-		-
	//	virtual cycle 3:	data3	data2	data1	-		-		-		-
	//	virtual cycle 4:	data4	data3	data2	data1	-		-		-		
	//	virtual cycle 5:	data5	data4	data3	data2	data1	-		-			   
	//	virtual cycle 6:	data6	data5	data4	data3	data2	data1	-	
	//
	//	inner loop:
	//	  virtual cycle A:	data7	data6	data5	data4	data3	data2	data1					  
	//	  virtual cycle B:	data8	data7	data6	data5	data4	data3	data2	
	//
	//	virtual cycle 7 -		dataF	dataE	dataD	dataC	dataB	dataA
	//	virtual cycle 8 -		-		dataF	dataE	dataD	dataC	dataB	
	//	virtual cycle 9 -		-		-		dataF	dataE	dataD	dataC
	//	virtual cycle 10	-		-		-		-		dataF	dataE	dataD  
	//	virtual cycle 11	-		-		-		-		-		dataF	dataE	
	//	virtual cycle 12	-		-		-		-		-		-		dataF						 
	
	if( count >= 6 )
	{
		//virtual cycle 1
		startingFloat = (src++)[0];
		
		//virtual cycle 2
		scaled = startingFloat * scale + round;
		startingFloat = (src++)[0];
		
		//virtual cycle 3
		converted = __fctiw( scaled );
		scaled = startingFloat * scale + round;
		startingFloat = (src++)[0];
		
		//virtual cycle 4
		__stfiwx( converted, 0, buffer );
		converted = __fctiw( scaled );
		scaled = startingFloat * scale + round;
		startingFloat = (src++)[0];
		
		//virtual cycle 5
		__stfiwx( converted, sizeof(float), buffer );
		converted = __fctiw( scaled );
		scaled = startingFloat * scale + round;
		startingFloat = (src++)[0];
		
		//virtual cycle 6
		copy = ((SInt8*) buffer)[0];	
		__stfiwx( converted, 0, buffer );
		converted = __fctiw( scaled );
		scaled = startingFloat * scale + round;
		startingFloat = (src++)[0];
		
		count -= 6;
		loopCount = count / 2;
		count &= 1;
		while( loopCount-- )
		{
			register float	startingFloat2;
			register double scaled2;
			register double converted2;
			register SInt8	copy2;
			
			//virtual Cycle A
			(dst++)[0] = copy;
			__asm__ __volatile__ ( "fctiw %0, %1" : "=f" (converted2) : "f" ( scaled ) );
			copy2 = ((SInt8*) buffer)[4];		
			__asm__ __volatile__ ( "fmadd %0, %1, %2, %3" : "=f" (scaled2) : "f" ( startingFloat), "f" (scale), "f" (round) );
			__asm__ __volatile__ ( "stfiwx %0, %1, %2" : : "f" (converted), "b%" (sizeof(float)), "r" (buffer) : "memory" );
			startingFloat2 = (src++)[0];

			//virtual cycle B
			(dst++)[0] = copy2;
			__asm__ __volatile__ ( "fctiw %0, %1" : "=f" (converted) : "f" ( scaled2 ) );
			copy = ((SInt8*) buffer)[0];
			__asm__ __volatile__ ( "fmadd %0, %1, %2, %3" : "=f" (scaled) : "f" ( startingFloat2), "f" (scale), "f" (round) );
			__asm__ __volatile__ ( "stfiwx %0, %1, %2" : : "f" (converted2), "b%" (0), "r" (buffer) : "memory" );
			startingFloat = (src++)[0];
		}
		
		//Virtual Cycle 7
		(dst++)[0] = copy;
		copy = ((SInt8*) buffer)[4];
		__stfiwx( converted, sizeof(float), buffer );
		converted = __fctiw( scaled );
		scaled = startingFloat * scale + round;
		
		//Virtual Cycle 8
		(dst++)[0] = copy;
		copy = ((SInt8*) buffer)[0];
		__stfiwx( converted, 0, buffer );
		converted = __fctiw( scaled );
	
		//Virtual Cycle 9
		(dst++)[0] = copy;
		copy = ((SInt8*) buffer)[4];
		__stfiwx( converted, sizeof(float), buffer );
		
		//Virtual Cycle 10
		(dst++)[0] = copy;
		copy = ((SInt8*) buffer)[0];
	
		//Virtual Cycle 11
		(dst++)[0] = copy;
		copy = ((SInt8*) buffer)[4];

		//Virtual Cycle 11
		(dst++)[0] = copy;
	}

	//clean up any extras
	while( count-- )
	{
		double scaled = src[0] * scale + round;
		double converted = __fctiw( scaled );
		__stfiwx( converted, 0, buffer );
		dst[0] = buffer[0] >> 24;
		src++;
		dst++;
	}
}


void Float32ToNativeInt16( float *src, signed short *dst, unsigned int count )
{
	register double		scale = 2147483648.0;
	register double		round = 32768.0;
	unsigned long		loopCount = count / 4;
	long				buffer[2];
	register float		startingFloat;
	register double 	scaled;
	register double 	converted;
	register short		copy;

	//
	//	The fastest way to do this is to set up a staggered loop that models a 7 stage virtual pipeline:
	//
	//		stage 1:		load the src value
	//		stage 2:		scale it to LONG_MIN...LONG_MAX and add a rounding value to it
	//		stage 3:		convert it to an integer within the FP register
	//		stage 4:		store that to the stack
	//		stage 5:		 (do nothing to let the store complete)
	//		stage 6:		load the high half word from the stack
	//		stage 7:		store it to the destination
	//
	//	We set it up so that at any given time 7 different pieces of data are being worked on at a time.
	//	Because of the do nothing stage, the inner loop had to be unrolled by one, so in actuality, each
	//	inner loop iteration represents two virtual clock cycles that push data through our virtual pipeline.
	//
	//	The reason why this works is that this allows us to break data dependency chains and insert 5 real 
	//	operations in between every virtual pipeline stage. This means 5 instructions between each data 
	//	dependency, which is just enough to keep all of our real pipelines happy. The data flow follows 
	//	standard pipeline diagrams:
	//
	//					stage1	stage2	stage3	stage4	stage5	stage6	stage7
	//	virtual cycle 1:	data1	-		-		-		-		-		-
	//	virtual cycle 2:	data2	data1	-		-		-		-		-
	//	virtual cycle 3:	data3	data2	data1	-		-		-		-
	//	virtual cycle 4:	data4	data3	data2	data1	-		-		-		
	//	virtual cycle 5:	data5	data4	data3	data2	data1	-		-			   
	//	virtual cycle 6:	data6	data5	data4	data3	data2	data1	-	
	//
	//	inner loop:
	//	  virtual cycle A:	data7	data6	data5	data4	data3	data2	data1					  
	//	  virtual cycle B:	data8	data7	data6	data5	data4	data3	data2	
	//
	//	virtual cycle 7 -		dataF	dataE	dataD	dataC	dataB	dataA
	//	virtual cycle 8 -		-		dataF	dataE	dataD	dataC	dataB	
	//	virtual cycle 9 -		-		-		dataF	dataE	dataD	dataC
	//	virtual cycle 10	-		-		-		-		dataF	dataE	dataD  
	//	virtual cycle 11	-		-		-		-		-		dataF	dataE	
	//	virtual cycle 12	-		-		-		-		-		-		dataF						 
	
	if( count >= 6 )
	{
		//virtual cycle 1
		startingFloat = (src++)[0];
		
		//virtual cycle 2
		scaled = startingFloat * scale + round;
		startingFloat = (src++)[0];
		
		//virtual cycle 3
		converted = __fctiw( scaled );
		scaled = startingFloat * scale + round;
		startingFloat = (src++)[0];
		
		//virtual cycle 4
		__stfiwx( converted, 0, buffer );
		converted = __fctiw( scaled );
		scaled = startingFloat * scale + round;
		startingFloat = (src++)[0];
		
		//virtual cycle 5
		__stfiwx( converted, sizeof(float), buffer );
		converted = __fctiw( scaled );
		scaled = startingFloat * scale + round;
		startingFloat = (src++)[0];
		
		//virtual cycle 6
		copy = ((short*) buffer)[0];
		__stfiwx( converted, 0, buffer );
		converted = __fctiw( scaled );
		scaled = startingFloat * scale + round;
		startingFloat = (src++)[0];
		
		count -= 6;
		loopCount = count / 2;
		count &= 1;
		while( loopCount-- )
		{
			register float	startingFloat2;
			register double scaled2;
			register double converted2;
			register short	copy2;
			
			//virtual Cycle A
			(dst++)[0] = copy;
			__asm__ __volatile__ ( "fctiw %0, %1" : "=f" (converted2) : "f" ( scaled ) );
			copy2 = ((short*) buffer)[2];
			__asm__ __volatile__ ( "fmadd %0, %1, %2, %3" : "=f" (scaled2) : "f" ( startingFloat), "f" (scale), "f" (round) );
			__asm__ __volatile__ ( "stfiwx %0, %1, %2" : : "f" (converted), "b%" (sizeof(float)), "r" (buffer) : "memory" );
			startingFloat2 = (src++)[0];

			//virtual cycle B
			(dst++)[0] = copy2;
			__asm__ __volatile__ ( "fctiw %0, %1" : "=f" (converted) : "f" ( scaled2 ) );
			copy = ((short*) buffer)[0];
			__asm__ __volatile__ ( "fmadd %0, %1, %2, %3" : "=f" (scaled) : "f" ( startingFloat2), "f" (scale), "f" (round) );
			__asm__ __volatile__ ( "stfiwx %0, %1, %2" : : "f" (converted2), "b%" (0), "r" (buffer) : "memory" );
			startingFloat = (src++)[0];
		}
		
		//Virtual Cycle 7
		(dst++)[0] = copy;
		copy = ((short*) buffer)[2];
		__stfiwx( converted, sizeof(float), buffer );
		converted = __fctiw( scaled );
		scaled = startingFloat * scale + round;
		
		//Virtual Cycle 8
		(dst++)[0] = copy;
		copy = ((short*) buffer)[0];
		__stfiwx( converted, 0, buffer );
		converted = __fctiw( scaled );
	
		//Virtual Cycle 9
		(dst++)[0] = copy;
		copy = ((short*) buffer)[2];
		__stfiwx( converted, sizeof(float), buffer );
		
		//Virtual Cycle 10
		(dst++)[0] = copy;
		copy = ((short*) buffer)[0];
	
		//Virtual Cycle 11
		(dst++)[0] = copy;
		copy = ((short*) buffer)[2];

		//Virtual Cycle 11
		(dst++)[0] = copy;
	}

	//clean up any extras
	while( count-- )
	{
		double scaled = src[0] * scale + round;
		double converted = __fctiw( scaled );
		__stfiwx( converted, 0, buffer );
		dst[0] = buffer[0] >> 16;
		src++;
		dst++;
	}
}

void Float32ToSwapInt16( float *src, signed short *dst, unsigned int count )
{
	register double		scale = 2147483648.0;
	register double		round = 32768.0;
	unsigned long	loopCount = count / 4;
	long		buffer[2];
	register float	startingFloat;
	register double scaled;
	register double converted;
	register short	copy;

	//
	//	The fastest way to do this is to set up a staggered loop that models a 7 stage virtual pipeline:
	//
	//		stage 1:		load the src value
	//		stage 2:		scale it to LONG_MIN...LONG_MAX and add a rounding value to it
	//		stage 3:		convert it to an integer within the FP register
	//		stage 4:		store that to the stack
	//		stage 5:		 (do nothing to let the store complete)
	//		stage 6:		load the high half word from the stack
	//		stage 7:		store it to the destination
	//
	//	We set it up so that at any given time 7 different pieces of data are being worked on at a time.
	//	Because of the do nothing stage, the inner loop had to be unrolled by one, so in actuality, each
	//	inner loop iteration represents two virtual clock cycles that push data through our virtual pipeline.
	//
	//	The reason why this works is that this allows us to break data dependency chains and insert 5 real 
	//	operations in between every virtual pipeline stage. This means 5 instructions between each data 
	//	dependency, which is just enough to keep all of our real pipelines happy. The data flow follows 
	//	standard pipeline diagrams:
	//
	//					stage1	stage2	stage3	stage4	stage5	stage6	stage7
	//	virtual cycle 1:	data1	-		-		-		-		-		-
	//	virtual cycle 2:	data2	data1	-		-		-		-		-
	//	virtual cycle 3:	data3	data2	data1	-		-		-		-
	//	virtual cycle 4:	data4	data3	data2	data1	-		-		-		
	//	virtual cycle 5:	data5	data4	data3	data2	data1	-		-			   
	//	virtual cycle 6:	data6	data5	data4	data3	data2	data1	-	
	//
	//	inner loop:
	//	  virtual cycle A:	data7	data6	data5	data4	data3	data2	data1					  
	//	  virtual cycle B:	data8	data7	data6	data5	data4	data3	data2	
	//
	//	virtual cycle 7 -		dataF	dataE	dataD	dataC	dataB	dataA
	//	virtual cycle 8 -		-		dataF	dataE	dataD	dataC	dataB	
	//	virtual cycle 9 -		-		-		dataF	dataE	dataD	dataC
	//	virtual cycle 10	-		-		-		-		dataF	dataE	dataD  
	//	virtual cycle 11	-		-		-		-		-		dataF	dataE	
	//	virtual cycle 12	-		-		-		-		-		-		dataF						 
	
	if( count >= 6 )
	{
		//virtual cycle 1
		startingFloat = (src++)[0];
		
		//virtual cycle 2
		scaled = startingFloat * scale + round;
		startingFloat = (src++)[0];
		
		//virtual cycle 3
		converted = __fctiw( scaled );
		scaled = startingFloat * scale + round;
		startingFloat = (src++)[0];
		
		//virtual cycle 4
		__stfiwx( converted, 0, buffer );
		converted = __fctiw( scaled );
		scaled = startingFloat * scale + round;
		startingFloat = (src++)[0];
		
		//virtual cycle 5
		__stfiwx( converted, sizeof(float), buffer );
		converted = __fctiw( scaled );
		scaled = startingFloat * scale + round;
		startingFloat = (src++)[0];
		
		//virtual cycle 6
		copy = ((short*) buffer)[0];
		__stfiwx( converted, 0, buffer );
		converted = __fctiw( scaled );
		scaled = startingFloat * scale + round;
		startingFloat = (src++)[0];
		
		count -= 6;
		loopCount = count / 2;
		count &= 1;
		while( loopCount-- )
		{
			register float	startingFloat2;
			register double scaled2;
			register double converted2;
			register short	copy2;
			
			//virtual Cycle A
//			  (dst++)[0] = copy;
			__asm__ __volatile__ ( "sthbrx %0, 0, %1" : : "r" (copy), "r" (dst) );
			__asm__ __volatile__ ( "fctiw %0, %1" : "=f" (converted2) : "f" ( scaled ) );
			copy2 = ((short*) buffer)[2];
			__asm__ __volatile__ ( "fmadd %0, %1, %2, %3" : "=f" (scaled2) : "f" ( startingFloat), "f" (scale), "f" (round) );
			__asm__ __volatile__ ( "stfiwx %0, %1, %2" : : "f" (converted), "b%" (sizeof(float)), "r" (buffer) : "memory" );
			startingFloat2 = (src)[0];	src+=2;

			//virtual cycle B
//			  (dst++)[0] = copy2;
			dst+=2;
			__asm__ __volatile__ ( "sthbrx %0, %1, %2" : : "r" (copy2), "b" (-2), "r" (dst) );	
			__asm__ __volatile__ ( "fctiw %0, %1" : "=f" (converted) : "f" ( scaled2 ) );
			copy = ((short*) buffer)[0];
			__asm__ __volatile__ ( "fmadd %0, %1, %2, %3" : "=f" (scaled) : "f" ( startingFloat2), "f" (scale), "f" (round) );
			__asm__ __volatile__ ( "stfiwx %0, %1, %2" : : "f" (converted2), "b%" (0), "r" (buffer) : "memory" );
			startingFloat = (src)[-1];	
		}
		
		//Virtual Cycle 7
		__asm__ __volatile__ ( "sthbrx %0, 0, %1" : : "r" (copy),  "r" (dst) );	 dst++;
		copy = ((short*) buffer)[2];
		__stfiwx( converted, sizeof(float), buffer );
		converted = __fctiw( scaled );
		scaled = startingFloat * scale + round;
		
		//Virtual Cycle 8
		__asm__ __volatile__ ( "sthbrx %0, 0, %1" : : "r" (copy),  "r" (dst) );	 dst++;
		copy = ((short*) buffer)[0];
		__stfiwx( converted, 0, buffer );
		converted = __fctiw( scaled );
	
		//Virtual Cycle 9
		__asm__ __volatile__ ( "sthbrx %0, 0, %1" : : "r" (copy),  "r" (dst) );	 dst++;
		copy = ((short*) buffer)[2];
		__stfiwx( converted, sizeof(float), buffer );
		
		//Virtual Cycle 10
		__asm__ __volatile__ ( "sthbrx %0, 0, %1" : : "r" (copy),  "r" (dst) );	 dst++;
		copy = ((short*) buffer)[0];
	
		//Virtual Cycle 11
		__asm__ __volatile__ ( "sthbrx %0, 0, %1" : : "r" (copy),  "r" (dst) );	 dst++;
		copy = ((short*) buffer)[2];

		//Virtual Cycle 11
		__asm__ __volatile__ ( "sthbrx %0, 0, %1" : : "r" (copy),  "r" (dst) );	 dst++;
	}

	//clean up any extras
	while( count-- )
	{
		double scaled = src[0] * scale + round;
		double converted = __fctiw( scaled );
		__stfiwx( converted, 0, buffer );
		copy = buffer[0] >> 16;
		__asm__ __volatile__ ( "sthbrx %0, 0, %1" : : "r" (copy),  "r" (dst) );	 
		src++;
		dst++;
	}
}


void Float32ToNativeInt24( float *src, signed long *dst, unsigned int count )
{
	register double		scale = 2147483648.0;
	register double		round = 0.5 * 256.0;
	unsigned long	loopCount = count / 4;
	long		buffer[4];
	register float	startingFloat, startingFloat2;
	register double scaled, scaled2;
	register double converted, converted2;
	register long	copy1;//, merge1, rotate1;
	register long	copy2;//, merge2, rotate2;
	register long	copy3;//, merge3, rotate3;
	register long	copy4;//, merge4, rotate4;
	register double		oldSetting;


	//Set the FPSCR to round to -Inf mode
	{
		union
		{
			double	d;
			int		i[2];
		}setting;
		register double newSetting;

		//Read the the current FPSCR value
		__asm__ volatile ( "mffs %0" : "=f" ( oldSetting ) );

		//Store it to the stack
		setting.d = oldSetting;

		//Read in the low 32 bits and mask off the last two bits so they are zero
		//in the integer unit. These two bits set to zero means round to nearest mode.
		//Finally, then store the result back
		setting.i[1] |= 3;

		//Load the new FPSCR setting into the FP register file again
		newSetting = setting.d;

		//Change the FPSCR to the new setting
		asm volatile( "mtfsf 7, %0" : : "f" (newSetting ) );
	}


	//
	//	The fastest way to do this is to set up a staggered loop that models a 7 stage virtual pipeline:
	//
	//		stage 1:		load the src value
	//		stage 2:		scale it to LONG_MIN...LONG_MAX and add a rounding value to it
	//		stage 3:		convert it to an integer within the FP register
	//		stage 4:		store that to the stack
	//		stage 5:		 (do nothing to let the store complete)
	//		stage 6:		load the high half word from the stack
		//		stage 7:		merge with later data to form a 32 bit word
		//		stage 8:		possible rotate to correct byte order
	//		stage 9:		store it to the destination
	//
	//	We set it up so that at any given time 7 different pieces of data are being worked on at a time.
	//	Because of the do nothing stage, the inner loop had to be unrolled by one, so in actuality, each
	//	inner loop iteration represents two virtual clock cycles that push data through our virtual pipeline.
	//
	//	The reason why this works is that this allows us to break data dependency chains and insert 5 real
	//	operations in between every virtual pipeline stage. This means 5 instructions between each data
	//	dependency, which is just enough to keep all of our real pipelines happy. The data flow follows
	//	standard pipeline diagrams:
	//
	//				stage1	stage2	stage3	stage4	stage5	stage6	stage7	stage8	stage9
	//	virtual cycle 1:	data1	-	-	-		-		-		-		-		-
	//	virtual cycle 2:	data2	data1	-	-		-		-		-		-		-
	//	virtual cycle 3:	data3	data2	data1	-		-		-		-		-		-
	//	virtual cycle 4:	data4	data3	data2	data1	-		-		-		-		-
	//	virtual cycle 5:	data5	data4	data3	data2	data1	-		-		-		-
	//	virtual cycle 6:	data6	data5	data4	data3	data2	data1	-		-		-
	//	virtual cycle 7:	data7	data6	data5	data4	data3	data2	data1	-		-
	//	virtual cycle 8:	data8	data7	data6	data5	data4	data3	data2	data1	-
	//
	//	inner loop:
	//	  virtual cycle A:	data9	data8	data7	data6	data5	data4	data3	data2	data1
	//	  virtual cycle B:	data10	data9	data8	data7	data6	data5	data4	data3	data2
	//	  virtual cycle C:	data11	data10	data9	data8	data7	data6	data5	data4	data3
	//	  virtual cycle D:	data12	data11	data10	data9	data8	data7	data6	data5	data4
	//
	//	virtual cycle 9		-	dataH	dataG	dataF	dataE	dataD	dataC	dataB	dataA
	//	virtual cycle 10	-	-	dataH	dataG	dataF	dataE	dataD	dataC	dataB
	//	virtual cycle 11	-	-	-	dataH	dataG	dataF	dataE	dataD	dataC
	//	virtual cycle 12	-	-	-	-		dataH	dataG	dataF	dataE	dataD
	//	virtual cycle 13	-	-	-	-		-		dataH	dataG	dataF	dataE
	//	virtual cycle 14	-	-	-	-		-		-		dataH	dataG	dataF
	//	virtual cycle 15	-	-	-	-		-		-		-	dataH	dataG	
	//	virtual cycle 16	-	-	-	-		-		-		-	-	dataH

	src--;
	dst--;

	if( count >= 8 )
	{
		//virtual cycle 1
		startingFloat = (++src)[0];

		//virtual cycle 2
		scaled = startingFloat * scale + round;
		startingFloat = (++src)[0];

		//virtual cycle 3
		converted = __fctiw( scaled );
		scaled = startingFloat * scale + round;
		startingFloat = (++src)[0];

		//virtual cycle 4
		__stfiwx( converted, 0, buffer );
		converted = __fctiw( scaled );
		scaled = startingFloat * scale + round;
		startingFloat = (++src)[0];

		//virtual cycle 5
		__stfiwx( converted, sizeof(float), buffer );
		converted = __fctiw( scaled );
		scaled = startingFloat * scale + round;
		startingFloat = (++src)[0];

		//virtual cycle 6
		copy1 = buffer[0];
		__stfiwx( converted, 2 * sizeof( float), buffer );
		converted = __fctiw( scaled );
		scaled = startingFloat * scale + round;
		startingFloat = (++src)[0];

		//virtual cycle 7
		copy2 = buffer[1];
		__stfiwx( converted, 3 * sizeof(float), buffer );
		converted = __fctiw( scaled );
		scaled = startingFloat * scale + round;
		startingFloat = (++src)[0];

		//virtual cycle 8
		copy1 = __rlwimi( copy1, copy2, 8, 24, 31 );
		copy3 = buffer[2];
		__stfiwx( converted, 0, buffer );
		converted = __fctiw( scaled );
		scaled = startingFloat * scale + round;
		startingFloat = (++src)[0];

		count -= 8;
		loopCount = count / 4;
		count &= 3;
		while( loopCount-- )
		{
			//virtual cycle A
			//no store yet						//store
			//no rotation needed for copy1,				//rotate
			__asm__ __volatile__( "fmadds %0, %1, %2, %3" : "=f"(scaled2) : "f" (startingFloat), "f" ( scale ), "f" ( round ));		//scale for clip and add rounding
			startingFloat2 = (++src)[0];				//load the float
			__asm__ __volatile__( "fctiw %0, %1" : "=f" (converted2) : "f" ( scaled ) );				//convert to int and clip
			 copy4 = buffer[3];						//load clipped int back in
			copy2 = __rlwimi_volatile( copy2, copy3, 8, 24, 7 );			//merge
			__stfiwx( converted, 1 * sizeof(float), buffer );		//store clipped int

			//virtual Cycle B
			__asm__ __volatile__( "fmadds %0, %1, %2, %3" : "=f"(scaled) : "f" (startingFloat2), "f" ( scale ), "f" ( round ));		//scale for clip and add rounding
			startingFloat = (++src)[0];					//load the float
			 __asm__ __volatile__( "fctiw %0, %1" : "=f" (converted) : "f" ( scaled2 ) );				//convert to int and clip
			(++dst)[0] = copy1;						//store
			copy3 = __rlwimi_volatile( copy3, copy4, 8, 24, 15 );	//merge with adjacent pixel
			copy1 = buffer[0];						//load clipped int back in
			copy2 = __rlwimi_volatile( copy2, copy2, 8, 0, 31 );	//rotate
			__stfiwx( converted2, 2 * sizeof(float), buffer );		//store clipped int

			//virtual Cycle C
			__asm__ __volatile__( "fmadds %0, %1, %2, %3" : "=f"(scaled2) : "f" (startingFloat), "f" ( scale ), "f" ( round ));		//scale for clip and add rounding
			startingFloat2 = (++src)[0];				//load the float
			//We dont store copy 4 so no merge needs to be done to it	//merge with adjacent pixel
			converted2 = __fctiw( scaled );				//convert to int and clip
			(++dst)[0] = copy2;						//store
			copy3 = __rlwimi_volatile( copy3, copy3, 16, 0, 31 );		//rotate
			copy2 = buffer[1];						//load clipped int back in
			__stfiwx( converted, 3 * sizeof(float), buffer );		//store clipped int

			//virtual Cycle D
			__asm__ ( "fmadds %0, %1, %2, %3" : "=f"(scaled) : "f" (startingFloat2), "f" ( scale ), "f" ( round ));		//scale for clip and add rounding
			startingFloat = (++src)[0];					//load the float
			converted = __fctiw( scaled2 );				//convert to int and clip
			//We dont store copy 4 so no rotation needs to be done to it//rotate
			(++dst)[0] = copy3;						//store
			copy1 = __rlwimi_volatile( copy1, copy2, 8, 24, 31 );		//merge with adjacent pixel
			 __stfiwx( converted2, 0 * sizeof(float), buffer );		//store clipped int
			copy3 = buffer[2];						//load clipped int back in
		}

		//virtual cycle 9
		//no store yet						//store
		//no rotation needed for copy1,				//rotate
		copy2 = __rlwimi( copy2, copy3, 8, 24, 7 );		//merge
		copy4 = buffer[3];					//load clipped int back in
		__stfiwx( converted, 1 * sizeof(float), buffer );	//store clipped int
		converted2 = __fctiw( scaled );				//convert to int and clip
		scaled2 = startingFloat * scale + round;		//scale for clip and add rounding

		//virtual Cycle 10
		(++dst)[0] = copy1;						//store
		copy2 = __rlwimi( copy2, copy2, 8, 0, 31 );			//rotate
		copy3 = __rlwimi( copy3, copy4, 8, 24, 15 );		//merge with adjacent pixel
		copy1 = buffer[0];					//load clipped int back in
		__stfiwx( converted2, 2 * sizeof(float), buffer );	//store clipped int
		converted = __fctiw( scaled2 );				//convert to int and clip

		//virtual Cycle 11
		(++dst)[0] = copy2;						//store
		copy3 = __rlwimi( copy3, copy3, 16, 0, 31 );		//rotate
		//We dont store copy 4 so no merge needs to be done to it//merge with adjacent pixel
		copy2 = buffer[1];					//load clipped int back in
		__stfiwx( converted, 3 * sizeof(float), buffer );	//store clipped int

		//virtual Cycle 12
		(++dst)[0] = copy3;						//store
		//We dont store copy 4 so no rotation needs to be done to it//rotate
		copy1 = __rlwimi( copy1, copy2, 8, 24, 31 );		//merge with adjacent pixel
		copy3 = buffer[2];						//load clipped int back in

		//virtual cycle 13
		//no store yet						//store
		//no rotation needed for copy1,				//rotate
		copy2 = __rlwimi( copy2, copy3, 8, 24, 7 );		//merge
		copy4 = buffer[3];					//load clipped int back in

		//virtual Cycle 14
		(++dst)[0] = copy1;						//store
		copy2 = __rlwimi( copy2, copy2, 8, 0, 31 );			//rotate
		copy3 = __rlwimi( copy3, copy4, 8, 24, 15 );		//merge with adjacent pixel

		//virtual Cycle 15
		(++dst)[0] = copy2;						//store
		copy3 = __rlwimi( copy3, copy3, 16, 0, 31 );		//rotate

		//virtual Cycle 16
		(++dst)[0] = copy3;						//store
	}

	//clean up any extras
	dst++;
	while( count-- )
	{
		startingFloat = (++src)[0];				//load the float
		scaled = startingFloat * scale + round;			//scale for clip and add rounding
		converted = __fctiw( scaled );				//convert to int and clip
		__stfiwx( converted, 0, buffer );			//store clipped int
		copy1 = buffer[0];					//load clipped int back in
		((signed char*) dst)[0] = copy1 >> 24;
		dst = (signed long*) ((signed char*) dst + 1 );
		((unsigned short*) dst)[0] = copy1 >> 8;
		dst = (signed long*) ((unsigned short*) dst + 1 );
	}

	//restore the old FPSCR setting
	__asm__ __volatile__ ( "mtfsf 7, %0" : : "f" (oldSetting) );
}

void Float32ToSwapInt24( float *src, signed long *dst, unsigned int count )
{
	register double		scale = 2147483648.0;
	register double		round = 0.5 * 256.0;
	unsigned long	loopCount = count / 4;
	long		buffer[4];
	register float	startingFloat, startingFloat2;
	register double scaled, scaled2;
	register double converted, converted2;
	register long	copy1;
	register long	copy2;
	register long	copy3;
	register long	copy4;
	register double		oldSetting;


	//Set the FPSCR to round to -Inf mode
	{
		union
		{
			double	d;
			int		i[2];
		}setting;
		register double newSetting;

		//Read the the current FPSCR value
		asm volatile ( "mffs %0" : "=f" ( oldSetting ) );

		//Store it to the stack
		setting.d = oldSetting;

		//Read in the low 32 bits and mask off the last two bits so they are zero
		//in the integer unit. These two bits set to zero means round to nearest mode.
		//Finally, then store the result back
		setting.i[1] |= 3;

		//Load the new FPSCR setting into the FP register file again
		newSetting = setting.d;

		//Change the FPSCR to the new setting
		asm volatile( "mtfsf 7, %0" : : "f" (newSetting ) );
	}


	//
	//	The fastest way to do this is to set up a staggered loop that models a 7 stage virtual pipeline:
	//
	//		stage 1:		load the src value
	//		stage 2:		scale it to LONG_MIN...LONG_MAX and add a rounding value to it
	//		stage 3:		convert it to an integer within the FP register
	//		stage 4:		store that to the stack
	//		stage 5:		 (do nothing to let the store complete)
	//		stage 6:		load the high half word from the stack
		//		stage 7:		merge with later data to form a 32 bit word
		//		stage 8:		possible rotate to correct byte order
	//		stage 9:		store it to the destination
	//
	//	We set it up so that at any given time 7 different pieces of data are being worked on at a time.
	//	Because of the do nothing stage, the inner loop had to be unrolled by one, so in actuality, each
	//	inner loop iteration represents two virtual clock cycles that push data through our virtual pipeline.
	//
	//	The reason why this works is that this allows us to break data dependency chains and insert 5 real
	//	operations in between every virtual pipeline stage. This means 5 instructions between each data
	//	dependency, which is just enough to keep all of our real pipelines happy. The data flow follows
	//	standard pipeline diagrams:
	//
	//				stage1	stage2	stage3	stage4	stage5	stage6	stage7	stage8	stage9
	//	virtual cycle 1:	data1	-	-	-		-		-		-		-		-
	//	virtual cycle 2:	data2	data1	-	-		-		-		-		-		-
	//	virtual cycle 3:	data3	data2	data1	-		-		-		-		-		-
	//	virtual cycle 4:	data4	data3	data2	data1	-		-		-		-		-
	//	virtual cycle 5:	data5	data4	data3	data2	data1	-		-		-		-
	//	virtual cycle 6:	data6	data5	data4	data3	data2	data1	-		-		-
	//	virtual cycle 7:	data7	data6	data5	data4	data3	data2	data1	-		-
	//	virtual cycle 8:	data8	data7	data6	data5	data4	data3	data2	data1	-
	//
	//	inner loop:
	//	  virtual cycle A:	data9	data8	data7	data6	data5	data4	data3	data2	data1
	//	  virtual cycle B:	data10	data9	data8	data7	data6	data5	data4	data3	data2
	//	  virtual cycle C:	data11	data10	data9	data8	data7	data6	data5	data4	data3
	//	  virtual cycle D:	data12	data11	data10	data9	data8	data7	data6	data5	data4
	//
	//	virtual cycle 9		-	dataH	dataG	dataF	dataE	dataD	dataC	dataB	dataA
	//	virtual cycle 10	-	-	dataH	dataG	dataF	dataE	dataD	dataC	dataB
	//	virtual cycle 11	-	-	-	dataH	dataG	dataF	dataE	dataD	dataC
	//	virtual cycle 12	-	-	-	-		dataH	dataG	dataF	dataE	dataD
	//	virtual cycle 13	-	-	-	-		-		dataH	dataG	dataF	dataE
	//	virtual cycle 14	-	-	-	-		-		-		dataH	dataG	dataF
	//	virtual cycle 15	-	-	-	-		-		-		-	dataH	dataG	
	//	virtual cycle 16	-	-	-	-		-		-		-	-	dataH

	src--;
	dst--;

	if( count >= 8 )
	{
		//virtual cycle 1
		startingFloat = (++src)[0];

		//virtual cycle 2
		scaled = startingFloat * scale + round;
		startingFloat = (++src)[0];

		//virtual cycle 3
		converted = __fctiw( scaled );
		scaled = startingFloat * scale + round;
		startingFloat = (++src)[0];

		//virtual cycle 4
		__stfiwx( converted, 0, buffer );
		converted = __fctiw( scaled );
		scaled = startingFloat * scale + round;
		startingFloat = (++src)[0];

		//virtual cycle 5
		__stfiwx( converted, sizeof(float), buffer );
		converted = __fctiw( scaled );
		scaled = startingFloat * scale + round;
		startingFloat = (++src)[0];

		//virtual cycle 6
		copy1 = __lwbrx( 0, buffer );
		__stfiwx( converted, 2 * sizeof( float), buffer );
		converted = __fctiw( scaled );
		scaled = startingFloat * scale + round;
		startingFloat = (++src)[0];

		//virtual cycle 7
		copy2 = __lwbrx( 4, buffer );
		__stfiwx( converted, 3 * sizeof(float), buffer );
		converted = __fctiw( scaled );
		scaled = startingFloat * scale + round;
		startingFloat = (++src)[0];

		//virtual cycle 8
		copy1 = __rlwimi( copy1, copy2, 8, 0, 7 );
		copy3 = __lwbrx( 8, buffer );;
		__stfiwx( converted, 0, buffer );
		converted = __fctiw( scaled );
		scaled = startingFloat * scale + round;
		startingFloat = (++src)[0];

		count -= 8;
		loopCount = count / 4;
		count &= 3;
		while( loopCount-- )
		{
			//virtual cycle A
			//no store yet						//store
			copy1 = __rlwimi( copy1, copy1, 8, 0, 31 );			//rotate
			__asm__ __volatile__( "fmadds %0, %1, %2, %3" : "=f"(scaled2) : "f" (startingFloat), "f" ( scale ), "f" ( round ));		//scale for clip and add rounding
			startingFloat2 = (++src)[0];				//load the float
			__asm__ __volatile__( "fctiw %0, %1" : "=f" (converted2) : "f" ( scaled ) );				//convert to int and clip
			 copy4 = __lwbrx( 12, buffer );						//load clipped int back in
			copy2 = __rlwimi_volatile( copy2, copy3, 8, 0, 15 );			//merge
			__stfiwx( converted, 1 * sizeof(float), buffer );		//store clipped int

			//virtual Cycle B
			__asm__ __volatile__( "fmadds %0, %1, %2, %3" : "=f"(scaled) : "f" (startingFloat2), "f" ( scale ), "f" ( round ));		//scale for clip and add rounding
			startingFloat = (++src)[0];					//load the float
			 __asm__ __volatile__( "fctiw %0, %1" : "=f" (converted) : "f" ( scaled2 ) );				//convert to int and clip
			(++dst)[0] = copy1;						//store
			copy4 = __rlwimi_volatile( copy4, copy3, 24, 0, 7 );	//merge with adjacent pixel
			copy1 = __lwbrx( 0, buffer );						//load clipped int back in
			copy2 = __rlwimi_volatile( copy2, copy2, 16, 0, 31 );	//rotate
			__stfiwx( converted2, 2 * sizeof(float), buffer );		//store clipped int

			//virtual Cycle C
			__asm__ __volatile__( "fmadds %0, %1, %2, %3" : "=f"(scaled2) : "f" (startingFloat), "f" ( scale ), "f" ( round ));		//scale for clip and add rounding
			startingFloat2 = (++src)[0];				//load the float
			converted2 = __fctiw( scaled );				//convert to int and clip
			(++dst)[0] = copy2;						//store
			copy2 = __lwbrx( 4, buffer );						//load clipped int back in
			__stfiwx( converted, 3 * sizeof(float), buffer );		//store clipped int


			//virtual Cycle D
			__asm__ ( "fmadds %0, %1, %2, %3" : "=f"(scaled) : "f" (startingFloat2), "f" ( scale ), "f" ( round ));		//scale for clip and add rounding
			startingFloat = (++src)[0];					//load the float
			converted = __fctiw( scaled2 );				//convert to int and clip
			(++dst)[0] = copy4;						//store
			copy1 = __rlwimi_volatile( copy1, copy2, 8, 0, 7 );		//merge with adjacent pixel
			 __stfiwx( converted2, 0 * sizeof(float), buffer );		//store clipped int
			copy3 = __lwbrx( 8, buffer );						//load clipped int back in
		}

		//virtual cycle A
		//no store yet						//store
		copy1 = __rlwimi( copy1, copy1, 8, 0, 31 );			//rotate
		__asm__ __volatile__( "fmadds %0, %1, %2, %3" : "=f"(scaled2) : "f" (startingFloat), "f" ( scale ), "f" ( round ));		//scale for clip and add rounding
		__asm__ __volatile__( "fctiw %0, %1" : "=f" (converted2) : "f" ( scaled ) );				//convert to int and clip
			copy4 = __lwbrx( 12, buffer );						//load clipped int back in
		copy2 = __rlwimi_volatile( copy2, copy3, 8, 0, 15 );			//merge
		__stfiwx( converted, 1 * sizeof(float), buffer );		//store clipped int

		//virtual Cycle B
			__asm__ __volatile__( "fctiw %0, %1" : "=f" (converted) : "f" ( scaled2 ) );				//convert to int and clip
		(++dst)[0] = copy1;						//store
		copy4 = __rlwimi_volatile( copy4, copy3, 24, 0, 7 );	//merge with adjacent pixel
		copy1 = __lwbrx( 0, buffer );						//load clipped int back in
		copy2 = __rlwimi_volatile( copy2, copy2, 16, 0, 31 );	//rotate
		__stfiwx( converted2, 2 * sizeof(float), buffer );		//store clipped int

		//virtual Cycle C
		(++dst)[0] = copy2;						//store
		copy2 = __lwbrx( 4, buffer );						//load clipped int back in
		__stfiwx( converted, 3 * sizeof(float), buffer );		//store clipped int


		//virtual Cycle D
		(++dst)[0] = copy4;						//store
		copy1 = __rlwimi_volatile( copy1, copy2, 8, 0, 7 );		//merge with adjacent pixel
		copy3 = __lwbrx( 8, buffer );						//load clipped int back in

		//virtual cycle A
		//no store yet						//store
		copy1 = __rlwimi( copy1, copy1, 8, 0, 31 );			//rotate
		copy4 = __lwbrx( 12, buffer );						//load clipped int back in
		copy2 = __rlwimi_volatile( copy2, copy3, 8, 0, 15 );			//merge

		//virtual Cycle B
		(++dst)[0] = copy1;						//store
		copy4 = __rlwimi_volatile( copy4, copy3, 24, 0, 7 );	//merge with adjacent pixel
		copy2 = __rlwimi_volatile( copy2, copy2, 16, 0, 31 );	//rotate

		//virtual Cycle C
		(++dst)[0] = copy2;						//store


		//virtual Cycle D
		(++dst)[0] = copy4;						//store
	}

	//clean up any extras
	dst++;
	while( count-- )
	{
		startingFloat = (++src)[0];				//load the float
		scaled = startingFloat * scale + round;			//scale for clip and add rounding
		converted = __fctiw( scaled );				//convert to int and clip
		__stfiwx( converted, 0, buffer );			//store clipped int
		copy1 = __lwbrx( 0, buffer);					//load clipped int back in
		((signed char*) dst)[0] = copy1 >> 16;
		dst = (signed long*) ((signed char*) dst + 1 );
		((unsigned short*) dst)[0] = copy1;
		dst = (signed long*) ((unsigned short*) dst + 1 );
	}

	//restore the old FPSCR setting
	__asm__ __volatile__ ( "mtfsf 7, %0" : : "f" (oldSetting) );
}


void Float32ToSwapInt32( float *src, signed long *dst, unsigned int count )
{
	register double		scale = 2147483648.0;
	unsigned long	loopCount = count / 4;
	long			buffer[2];
	register float		startingFloat;
	register double scaled;
	register double converted;
	register long		copy;
	register double		oldSetting;

	//Set the FPSCR to round to -Inf mode
	{
		union
		{
			double	d;
			int		i[2];
		}setting;
		register double newSetting;
		
		//Read the the current FPSCR value
		asm volatile ( "mffs %0" : "=f" ( oldSetting ) );
		
		//Store it to the stack
		setting.d = oldSetting;
		
		//Read in the low 32 bits and mask off the last two bits so they are zero 
		//in the integer unit. These two bits set to zero means round to nearest mode.
		//Finally, then store the result back
		setting.i[1] &= 0xFFFFFFFC;
		
		//Load the new FPSCR setting into the FP register file again
		newSetting = setting.d;
		
		//Change the FPSCR to the new setting
		asm volatile( "mtfsf 7, %0" : : "f" (newSetting ) );
	}


	//
	//	The fastest way to do this is to set up a staggered loop that models a 7 stage virtual pipeline:
	//
	//		stage 1:		load the src value
	//		stage 2:		scale it to LONG_MIN...LONG_MAX and add a rounding value to it
	//		stage 3:		convert it to an integer within the FP register
	//		stage 4:		store that to the stack
	//		stage 5:		 (do nothing to let the store complete)
	//		stage 6:		load the high half word from the stack
	//		stage 7:		store it to the destination
	//
	//	We set it up so that at any given time 7 different pieces of data are being worked on at a time.
	//	Because of the do nothing stage, the inner loop had to be unrolled by one, so in actuality, each
	//	inner loop iteration represents two virtual clock cycles that push data through our virtual pipeline.
	//
	//	The reason why this works is that this allows us to break data dependency chains and insert 5 real 
	//	operations in between every virtual pipeline stage. This means 5 instructions between each data 
	//	dependency, which is just enough to keep all of our real pipelines happy. The data flow follows 
	//	standard pipeline diagrams:
	//
	//				stage1	stage2	stage3	stage4	stage5	stage6	stage7
	//	virtual cycle 1:	data1	-	-	-		-		-		-
	//	virtual cycle 2:	data2	data1	-	-		-		-		-
	//	virtual cycle 3:	data3	data2	data1	-		-		-		-
	//	virtual cycle 4:	data4	data3	data2	data1	-		-		-		
	//	virtual cycle 5:	data5	data4	data3	data2	data1	-		-			   
	//	virtual cycle 6:	data6	data5	data4	data3	data2	data1	-	
	//
	//	inner loop:
	//	  virtual cycle A:	data7	data6	data5	data4	data3	data2	data1					  
	//	  virtual cycle B:	data8	data7	data6	data5	data4	data3	data2	
	//
	//	virtual cycle 7		-	dataF	dataE	dataD	dataC	dataB	dataA
	//	virtual cycle 8		-	-	dataF	dataE	dataD	dataC	dataB	
	//	virtual cycle 9		-	-	-	dataF	dataE	dataD	dataC
	//	virtual cycle 10	-	-	-	-		dataF	dataE	dataD  
	//	virtual cycle 11	-	-	-	-		-		dataF	dataE	
	//	virtual cycle 12	-	-	-	-		-		-		dataF						 
	
	if( count >= 6 )
	{
		//virtual cycle 1
		startingFloat = (src++)[0];
		
		//virtual cycle 2
		scaled = startingFloat * scale;
		startingFloat = (src++)[0];
		
		//virtual cycle 3
		converted = __fctiw( scaled );
		scaled = startingFloat * scale;
		startingFloat = (src++)[0];
		
		//virtual cycle 4
		__stfiwx( converted, 0, buffer );
		converted = __fctiw( scaled );
		scaled = startingFloat * scale;
		startingFloat = (src++)[0];
		
		//virtual cycle 5
		__stfiwx( converted, sizeof(float), buffer );
		converted = __fctiw( scaled );
		scaled = startingFloat * scale;
		startingFloat = (src++)[0];
		
		//virtual cycle 6
		copy = buffer[0];
		__stfiwx( converted, 0, buffer );
		converted = __fctiw( scaled );
		scaled = startingFloat * scale;
		startingFloat = (src++)[0];
		
		count -= 6;
		loopCount = count / 2;
		count &= 1;
		while( loopCount-- )
		{
			register float	startingFloat2;
			register double scaled2;
			register double converted2;
			register long	copy2;
			
			//virtual Cycle A
//			  (dst++)[0] = copy;
			__asm__ __volatile__ ( "stwbrx %0, 0, %1" : : "r" (copy), "r" (dst) );
			__asm__ __volatile__ ( "fctiw %0, %1" : "=f" (converted2) : "f" ( scaled ) );
			copy2 = buffer[1];
			__asm__ __volatile__ ( "fmuls %0, %1, %2" : "=f" (scaled2) : "f" ( startingFloat), "f" (scale) );
			__asm__ __volatile__ ( "stfiwx %0, %1, %2" : : "f" (converted), "b%" (sizeof(*buffer)), "r" (buffer) : "memory" );
			startingFloat2 = (src)[0];	src+=2;

			//virtual cycle B
//			  (dst++)[0] = copy2;
			dst+=2;
			__asm__ __volatile__ ( "stwbrx %0, %1, %2" : : "r" (copy2), "r" (-sizeof(dst[0])), "r" (dst) );	 
			__asm__ __volatile__ ( "fctiw %0, %1" : "=f" (converted) : "f" ( scaled2 ) );
			copy = buffer[0];
			__asm__ __volatile__ ( "fmuls %0, %1, %2" : "=f" (scaled) : "f" ( startingFloat2), "f" (scale) );
			__asm__ __volatile__ ( "stfiwx %0, %1, %2" : : "f" (converted2), "b%" (0), "r" (buffer) : "memory" );
			startingFloat = (src)[-1];	
		}
		
		//Virtual Cycle 7
		__asm__ __volatile__ ( "stwbrx %0, 0, %1" : : "r" (copy),  "r" (dst) );	 dst++;
		copy = buffer[1];
		__stfiwx( converted, sizeof(float), buffer );
		converted = __fctiw( scaled );
		scaled = startingFloat * scale;
		
		//Virtual Cycle 8
		__asm__ __volatile__ ( "stwbrx %0, 0, %1" : : "r" (copy),  "r" (dst) );	 dst++;
		copy =	buffer[0];
		__stfiwx( converted, 0, buffer );
		converted = __fctiw( scaled );
	
		//Virtual Cycle 9
		__asm__ __volatile__ ( "stwbrx %0, 0, %1" : : "r" (copy),  "r" (dst) );	 dst++;
		copy = buffer[1];
		__stfiwx( converted, sizeof(float), buffer );
		
		//Virtual Cycle 10
		__asm__ __volatile__ ( "stwbrx %0, 0, %1" : : "r" (copy),  "r" (dst) );	 dst++;
		copy = buffer[0];
	
		//Virtual Cycle 11
		__asm__ __volatile__ ( "stwbrx %0, 0, %1" : : "r" (copy),  "r" (dst) );	 dst++;
		copy = buffer[1];

		//Virtual Cycle 11
		__asm__ __volatile__ ( "stwbrx %0, 0, %1" : : "r" (copy),  "r" (dst) );	 dst++;
	}

	//clean up any extras
	while( count-- )
	{
		double scaled = src[0] * scale;
		double converted = __fctiw( scaled );
		__stfiwx( converted, 0, buffer );
		copy = buffer[0];
		__asm__ __volatile__ ( "stwbrx %0, 0, %1" : : "r" (copy),  "r" (dst) );	 
		src++;
		dst++;
	}
	
	//restore the old FPSCR setting
	__asm__ __volatile__ ( "mtfsf 7, %0" : : "f" (oldSetting) );
}

void Float32ToNativeInt32( float *src, signed long *dst, unsigned int count )
{
	register double		scale = 2147483648.0;
	unsigned long	loopCount;
	register float	startingFloat;
	register double scaled;
	register double converted;
	register double		oldSetting;

	//Set the FPSCR to round to -Inf mode
	{
		union
		{
			double	d;
			int		i[2];
		}setting;
		register double newSetting;
		
		//Read the the current FPSCR value
		asm volatile ( "mffs %0" : "=f" ( oldSetting ) );
		
		//Store it to the stack
		setting.d = oldSetting;
		
		//Read in the low 32 bits and mask off the last two bits so they are zero 
		//in the integer unit. These two bits set to zero means round to -infinity mode.
		//Finally, then store the result back
		setting.i[1] &= 0xFFFFFFFC;
		
		//Load the new FPSCR setting into the FP register file again
		newSetting = setting.d;
		
		//Change the FPSCR to the new setting
		asm volatile( "mtfsf 7, %0" : : "f" (newSetting ) );
	}

	//
	//	The fastest way to do this is to set up a staggered loop that models a 7 stage virtual pipeline:
	//
	//		stage 1:		load the src value
	//		stage 2:		scale it to LONG_MIN...LONG_MAX and add a rounding value to it
	//		stage 3:		convert it to an integer within the FP register
	//		stage 4:		store that to the destination
	//
	//	We set it up so that at any given time 7 different pieces of data are being worked on at a time.
	//	Because of the do nothing stage, the inner loop had to be unrolled by one, so in actuality, each
	//	inner loop iteration represents two virtual clock cycles that push data through our virtual pipeline.
	//
		//	The data flow follows standard pipeline diagrams:
	//
	//				stage1	stage2	stage3	stage4	
	//	virtual cycle 1:	data1	-	-	-	   
	//	virtual cycle 2:	data2	data1	-	-	   
	//	virtual cycle 3:	data3	data2	data1	-			 
	//
	//	inner loop:
	//	  virtual cycle A:	data4	data3	data2	data1					  
	//	  virtual cycle B:	data5	data4	data3	data2	
	//	  ...
	//	virtual cycle 4		-	dataD	dataC	dataB	
	//	virtual cycle 5		-	-		dataD	dataC
	//	virtual cycle 6		-	-	-	dataD  
	
	if( count >= 3 )
	{
		//virtual cycle 1
		startingFloat = (src++)[0];
		
		//virtual cycle 2
		scaled = startingFloat * scale;
		startingFloat = (src++)[0];
		
		//virtual cycle 3
		converted = __fctiw( scaled );
		scaled = startingFloat * scale;
		startingFloat = (src++)[0];
				
		count -= 3;
		loopCount = count / 2;
		count &= 1;
		while( loopCount-- )
		{
			register float	startingFloat2;
			register double scaled2;
			register double converted2;
			//register short	copy2;
			
			//virtual Cycle A
			startingFloat2 = (src)[0];
			__asm__ __volatile__ ( "fmul %0, %1, %2" : "=f" (scaled2) : "f" ( startingFloat), "f" (scale) );
			__asm__ __volatile__ ( "stfiwx %0, %1, %2" : : "f" (converted), "b%" (0), "r" (dst) : "memory" );
			__asm__ __volatile__ ( "fctiw %0, %1" : "=f" (converted2) : "f" ( scaled ) );

			//virtual cycle B
		   startingFloat = (src)[1];	 src+=2; 
			__asm__ __volatile__ ( "fmul %0, %1, %2" : "=f" (scaled) : "f" ( startingFloat2), "f" (scale) );
			__asm__ __volatile__ ( "stfiwx %0, %1, %2" : : "f" (converted2), "b%" (4), "r" (dst) : "memory" );
			__asm__ __volatile__ ( "fctiw %0, %1" : "=f" (converted) : "f" ( scaled2 ) );
			dst+=2;
		}
		
		//Virtual Cycle 4
		__stfiwx( converted, 0, dst++ );
		converted = __fctiw( scaled );
		__asm__ __volatile__ ( "fmul %0, %1, %2" : "=f" (scaled) : "f" ( startingFloat), "f" (scale) );
		
		//Virtual Cycle 5
		__stfiwx( converted, 0, dst++ );
		converted = __fctiw( scaled );
	
		//Virtual Cycle 6
		__stfiwx( converted, 0, dst++ );
	}

	//clean up any extras
	while( count-- )
	{
		double scaled = src[0] * scale;
		double converted = __fctiw( scaled );
		__stfiwx( converted, 0, dst );
		dst++;
		src++;
	}

	//restore the old FPSCR setting
	asm volatile( "mtfsf 7, %0" : : "f" (oldSetting) );
}

#endif


#pragma mark ------------------------ 
#pragma mark ••• Utility Routines
#pragma mark ------------------------ 


int validateSoftwareVolumes( float left, float right,UInt32 maxVolumedB, UInt32 minVolumedB)
{
	int result = TRUE;
	
	if ((left > maxVolumedB) || 
		(left < minVolumedB) ||
		(right > maxVolumedB) ||
		(right < minVolumedB))
	{
		result = FALSE;
	}

	return result;
}


void inputGainConverter(UInt32 inGainIndex, float* ioGainPtr) {
    float out = 1.0f;
    // check bounds		
    if (inGainIndex > (UInt32)(2*kInputGaindBConvTableOffset)) {
        inGainIndex = 2*kInputGaindBConvTableOffset;
    }

    // get the base dB converted value
    out = inputGaindBConvTable[inGainIndex];

    *ioGainPtr = out;
    
    return;
}

void convertToFourDotTwenty(FourDotTwenty* ioFourDotTwenty, float* inFloatPtr)
{
    float 				scale, floatValue;
    SInt32 				result_int;
    
	scale = kFourDotTwentyScaleFactor;
    ioFourDotTwenty->integerAndFraction1 = 0;
    ioFourDotTwenty->fraction2 = 0;
    ioFourDotTwenty->fraction3 = 0;
    floatValue = *inFloatPtr;
	
	if(floatValue > kMaxSignedFourDotTwenty) // clip to make sure number fits in 4.20 range
        floatValue = kMaxSignedFourDotTwenty;
    else if(floatValue < -8.0)
        floatValue = -8.0;

	result_int = (SInt32)(floatValue*scale);
	
    ioFourDotTwenty->fraction3 = (UInt8)(result_int & 0x000000FF); // mask off result and store in result
    ioFourDotTwenty->fraction2 = (UInt8)((result_int & 0x0000FF00) >> 8);
    ioFourDotTwenty->integerAndFraction1 = (UInt8)((result_int & 0x00FF0000) >> 16);
	
    return;
}

void volumeConverter (UInt32 inVolume, UInt32 inMinLinear, UInt32 inMaxLinear, SInt32 inMindB, SInt32 inMaxdB, float* outVolume)
{
	float slope, offset, volumedB, maxdB, mindB;
	
	// convert from 16.16 to float
	maxdB = (float)(inMaxdB >> 16);						// integer
	maxdB += (float)(inMaxdB & 0x0000FFFF)/65536.0f;	// fraction

	mindB = (float)(inMindB >> 16);						// integer
	mindB += (float)(inMindB & 0x0000FFFF)/65536.0f;	// fraction
	
	// convert linear gain value to dB value, then pass out new linear gain
	slope = (maxdB - mindB)/((float)(inMaxLinear - inMinLinear));
	offset = slope*inMinLinear - mindB;
	volumedB = slope*inVolume - offset;
	
	*outVolume = pow (10.0f, volumedB*0.05f);
	
#ifdef TESTING_VOLUME_CONVERSION
	*t1 = mindB;
	*t2 = maxdB;
	*t3 = slope;
	*t4 = offset;
	*t5 = volumedB;
#endif	
}

