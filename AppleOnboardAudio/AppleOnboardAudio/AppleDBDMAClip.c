#include <libkern/OSTypes.h>
#include <IOKit/IOReturn.h>

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

float gOldSample 								= 0.0f;

static const double kPI 						= 3.141592653589793116e+00;
static const double kdB2Log2Conversion 			= 0.1660964047443682;
static const double kOneOverSqrtOf2 			= 0.7071067811865475;

static const float kFourDotTwentyScaleFactor 	= 1048576.0f;

static const float kDefaultLimiterThreshold 	= -0.01660964f;					// -0.1 dB in log2, -0.083048202f = -0.5dB in log2
static const float kDefaultOneMinusOneOverRatio = 1.0f;							// 1 - 1/ratio, set for ratio = infinity
static const float kDefaultTa 					= 1.0f - 0.977579425f;			// 1 ms
static const float kDefaultTr 					= 1.0f - 0.999244427f;			// 30 ms
static const float kDefaultLookahead 			= 30; 							// 333 us @ 44.1kHz

static const UInt32 kNumFilters					= 1;

static const float kMixingToMonoScale 			= 0.5f;

static const float kOneOver65535 				= 1.0f/65535.0f;
static const float kOneOver1000000000 			= 1.0f/1000000000.0f;

// -24 dB to +24 dB in 1 dB steps
static const UInt16 kZeroGaindBConvTableOffset 	= 24;
static const UInt16 kMaxZeroGain 				= 24;
static const UInt16 kMinZeroGain 				= 24;

static float zeroGaindBConvTable[] = {
    0.0631,  0.0708,  0.0794,  0.0891,  0.1000,  0.1122,  0.1259, 
    0.1413,  0.1585,  0.1778,  0.1995,  0.2239,  0.2512,  0.2818,
    0.3162,  0.3548,  0.3981,  0.4467,  0.5012,  0.5623,  0.6310, 
    0.7079,  0.7943,  0.8913,  1.0000,  1.1220,  1.2589,  1.4125, 
    1.5849,  1.7783,  1.9953,  2.2387,  2.5119,  2.8184,  3.1623, 
    3.5481,  3.9811,  4.4668,  5.0119,  5.6234,  6.3096,  7.0795, 
    7.9433,  8.9125, 10.0000, 11.2202, 12.5893, 14.1254, 15.8489
};

// -12 dB to +12 dB in 0.5 dB steps
const UInt16 kInputGaindBConvTableOffset = 24;
static float inputGaindBConvTable[] = {
	0.251189,	0.266073,	0.281838,	0.298538,
	0.316228,	0.334965,	0.354813,	0.375837,
	0.398107,	0.421697,	0.446684,	0.473151,
	0.501187,	0.530884,	0.562341,	0.595662,
	0.630957,	0.668344,	0.707946,	0.749894,
	0.794328,	0.841395,	0.891251,	0.944061,
	1.000000,	1.059254,	1.122018,	1.188502,
	1.258925,	1.333521,	1.412538,	1.496236,
	1.584893,	1.678804,	1.778279,	1.883649,
	1.995262,	2.113489,	2.238721,	2.371374,
	2.511886,	2.660725,	2.818383,	2.985383,
	3.162278,	3.349654,	3.548134,	3.758374,
	3.981072
};

static float aoa_log2table[] = {
	-16.00000000f, -6.000000000f, -5.000000000f, -4.415037499f,
	-4.000000000f, -3.678071905f, -3.415037499f, -3.192645078f,
	-3.000000000f, -2.830074999f, -2.678071905f, -2.540568381f,
	-2.415037499f, -2.299560282f, -2.192645078f, -2.093109404f,
	-2.000000000f, -1.912537159f, -1.830074999f, -1.752072487f,
	-1.678071905f, -1.607682577f, -1.540568381f, -1.476438044f,
	-1.415037499f, -1.356143810f, -1.299560282f, -1.245112498f,
	-1.192645078f, -1.142019005f, -1.093109404f, -1.045803690f,
	-1.000000000f, -0.955605881f, -0.912537159f, -0.870716983f,
	-0.830074999f, -0.790546634f, -0.752072487f, -0.714597781f,
	-0.678071905f, -0.642447995f, -0.607682577f, -0.573735245f,
	-0.540568381f, -0.508146904f, -0.476438044f, -0.445411148f,
	-0.415037499f, -0.385290156f, -0.356143810f, -0.327574658f,
	-0.299560282f, -0.272079545f, -0.245112498f, -0.218640286f,
	-0.192645078f, -0.167109986f, -0.142019005f, -0.117356951f,
	-0.093109404f, -0.069262662f, -0.045803690f, -0.022720077f,
	 0.000000000f,  0.022367813f,  0.044394119f,  0.066089190f,
	 0.087462841f,  0.108524457f,  0.129283017f,  0.149747120f,
	 0.169925001f,  0.189824559f,  0.209453366f,  0.228818690f,
	 0.247927513f,  0.266786541f,  0.285402219f,  0.303780748f,
	 0.321928095f,  0.339850003f,  0.357552005f,  0.375039431f,
	 0.392317423f,  0.409390936f,  0.426264755f,  0.442943496f,
	 0.459431619f,  0.475733431f,  0.491853096f,  0.507794640f,
	 0.523561956f,  0.539158811f,  0.554588852f,  0.569855608f,
	 0.584962501f,  0.599912842f,  0.614709844f,  0.629356620f,
	 0.643856190f,  0.658211483f,  0.672425342f,  0.686500527f,
	 0.700439718f,  0.714245518f,  0.727920455f,  0.741466986f,
	 0.754887502f,  0.768184325f,  0.781359714f,  0.794415866f,
	 0.807354922f,  0.820178962f,  0.832890014f,  0.845490051f,
	 0.857980995f,  0.870364720f,  0.882643049f,  0.894817763f,
	 0.906890596f,  0.918863237f,  0.930737338f,  0.942514505f,
	 0.954196310f,  0.965784285f,  0.977279923f,  0.988684687f
};

static float aoa_antilog2table[] = {
	0.994599423f, 0.912051693f, 0.836355090f, 0.766940999f,
	0.703287997f, 0.644917937f, 0.591392355f, 0.542309181f,
	0.497299712f, 0.456025846f, 0.418177545f, 0.383470499f,
	0.351643998f, 0.322458968f, 0.295696178f, 0.271154591f,
	0.248649856f, 0.228012923f, 0.209088772f, 0.191735250f,
	0.175821999f, 0.161229484f, 0.147848089f, 0.135577295f,
	0.124324928f, 0.114006462f, 0.104544386f, 0.095867625f,
	0.087911000f, 0.080614742f, 0.073924044f, 0.067788648f,
	0.062162464f, 0.057003231f, 0.052272193f, 0.047933812f,
	0.043955500f, 0.040307371f, 0.036962022f, 0.033894324f,
	0.031081232f, 0.028501615f, 0.026136097f, 0.023966906f,
	0.021977750f, 0.020153686f, 0.018481011f, 0.016947162f,
	0.015540616f, 0.014250808f, 0.013068048f, 0.011983453f,
	0.010988875f, 0.010076843f, 0.009240506f, 0.008473581f,
	0.007770308f, 0.007125404f, 0.006534024f, 0.005991727f,
	0.005494437f, 0.005038421f, 0.004620253f, 0.004236790f,
	0.003885154f, 0.003562702f, 0.003267012f, 0.002995863f,
	0.002747219f, 0.002519211f, 0.002310126f, 0.002118395f,
	0.001942577f, 0.001781351f, 0.001633506f, 0.001497932f,
	0.001373609f, 0.001259605f, 0.001155063f, 0.001059198f,
	0.000971288f, 0.000890675f, 0.000816753f, 0.000748966f,
	0.000686805f, 0.000629803f, 0.000577532f, 0.000529599f,
	0.000485644f, 0.000445338f, 0.000408377f, 0.000374483f,
	0.000343402f, 0.000314901f, 0.000288766f, 0.000264799f,
	0.000242822f, 0.000222669f, 0.000204188f, 0.000187241f,
	0.000171701f, 0.000157451f, 0.000144383f, 0.000132400f,
	0.000121411f, 0.000111334f, 0.000102094f, 0.000093621f,
	0.000085851f, 0.000078725f, 0.000072191f, 0.000066200f,
	0.000060706f, 0.000055667f, 0.000051047f, 0.000046810f,
	0.000042925f, 0.000039363f, 0.000036096f, 0.000033100f,
	0.000030353f, 0.000027834f, 0.000025524f, 0.000023405f,
	0.000021463f, 0.000019681f, 0.000018048f, 0.000016550f
};

const static UInt16 kAOALog2TableLength 		= 128;
const static UInt16 kAOAAntiLog2TableLength 	= 128;
const static UInt16 kAOAAntiLog2TableRatio 		= 8;

#pragma mark ------------------------ 
#pragma mark ••• Processing Routines
#pragma mark ------------------------ 

// ------------------------------------------------------------------------
// Delay right channel audio data one sample, used to fix TAS 3004 phase problem
// ------------------------------------------------------------------------
void delayRightChannel(float* inFloatBufferPtr, UInt32 numSamples) 
{
    register float* inPtr;
    register float* outPtr;
	register UInt32 i, numFrames;
	register float inSampleR;
	register float oldSample;
	 
    numFrames = numSamples >> 1;
	inPtr = inFloatBufferPtr;
	inPtr++;
	outPtr = inPtr;
	oldSample = gOldSample;
	
	for (i = 0; i < numFrames; i++) 
    {
		inSampleR = *inPtr;
		inPtr += 2;
		*outPtr = oldSample;
		outPtr += 2;
		oldSample = inSampleR;
	}
	
	gOldSample = oldSample;
}

// ------------------------------------------------------------------------
// Invert right channel audio data
// ------------------------------------------------------------------------
void invertRightChannel(float* inFloatBufferPtr, UInt32 numSamples) 
{
	register UInt32 i;
	register UInt32 leftOver;
    register float* inPtr;
    register float* outPtr;
	register float inR0;
	register float inR1;
	register float inR2;
	register float inR3;
	register float inR4;
	register float inR5;
	register float inR6;
	register float inR7;
	
	inPtr = inFloatBufferPtr;
	inPtr++;  
	outPtr = inPtr;  
	
	leftOver = numSamples % 16;
	numSamples = numSamples >> 4;
	
    for (i = 0; i < numSamples; i++ ) 
    {
		inR0 = *(inPtr);					
		inPtr+=2;
		
		inR1 = *(inPtr);			
		inPtr+=2;

		inR2 = *(inPtr);			
		inPtr+=2;
		inR0 *= -1.0f;

		inR3 = *(inPtr);		
		inPtr+=2;
		inR1 *= -1.0f;

		inR4 = *(inPtr);					
		inPtr+=2;
		inR2 *= -1.0f;
		*(outPtr) = inR0;			
		outPtr+=2;
		
		inR5 = *(inPtr);			
		inPtr+=2;
		inR3 *= -1.0f;
		*(outPtr) = inR1;		
		outPtr+=2;

		inR6 = *(inPtr);			
		inPtr+=2;
		inR4 *= -1.0f;
		*(outPtr) = inR2;		
		outPtr+=2;
		
		inR7 = *(inPtr);		
		inPtr+=2;
		inR5 *= -1.0f;
		*(outPtr) = inR3;		
		outPtr+=2;

		inR6 *= -1.0f;
		*(outPtr) = inR4;			
		outPtr+=2;

		inR7 *= -1.0f;
		*(outPtr) = inR5;		
		outPtr+=2;
		
		*(outPtr) = inR6;		
		outPtr+=2;

		*(outPtr) = inR7;		
		outPtr+=2;
	}

    for (i = 0; i < leftOver; i += 2 ) 
    {
		inR0 = *(inPtr);
		inPtr+=2;
		inR0 *= -1.0f;
		*(outPtr) = inR0;
		outPtr+=2;
	}
}

// ------------------------------------------------------------------------
// Apply stereo gain, needed on machines like Q27 to compensate for
// different speaker output due to enclosure differences.
// ------------------------------------------------------------------------
void balanceAdjust(float* inFloatBufferPtr, UInt32 numSamples, EQStructPtr inEQ) 
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
	
	inPtr = inFloatBufferPtr;
	outPtr = inFloatBufferPtr;  
	
	leftGain = inEQ->leftSoftVolume;
	rightGain = inEQ->rightSoftVolume;
	
	numFrames = numSamples >> 1;
	leftOver = numFrames % 4;
	numSamples = numFrames >> 2;
	
    for (i = 0; i < numSamples; i++ ) 
    {
		inL0 = *(inPtr++);					

		inR0 = *(inPtr++);			

		inL1 = *(inPtr++);			
		inL0 *= leftGain;

		inR1 = *(inPtr++);		
		inR0 *= rightGain;

		inL2 = *(inPtr++);					
		inL1 *= leftGain;
		*(outPtr++) = inL0;			

		inR2 = *(inPtr++);			
		inR1 *= rightGain;
		*(outPtr++) = inR0;		

		inL3 = *(inPtr++);			
		inL2 *= leftGain;
		*(outPtr++) = inL1;		

		inR3 = *(inPtr++);		
		inR2 *= rightGain;
		*(outPtr++) = inR1;		

		inL3 *= leftGain;
		*(outPtr++) = inL2;			

		inR3 *= rightGain;
		*(outPtr++) = inR2;				

		*(outPtr++) = inL3;

		*(outPtr++) = inR3;		
	}

    for (i = 0; i < leftOver; i ++ ) 
    {
		inL0 = *(inPtr++);
		inR0 = *(inPtr++);
		inL0 *= leftGain;
		inR0 *= rightGain;
		*(outPtr++) = inL0;			
		*(outPtr++) = inR0;		
	}
}

// ------------------------------------------------------------------------
// Mix left and right channels together, and mute the right channel
// ------------------------------------------------------------------------
void mixAndMuteRightChannel(float* inFloatBufferPtr, UInt32 numSamples) 
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
	outPtr = inFloatBufferPtr;  
	
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

void equalizer(float *inBuf, UInt32 numSamples, EQStructPtr inEQ) {

	register UInt32			numFrames, framesLeft, j;
	register float			*inFloatBufferPtr;
	register float			*outFloatBufferPtr;
	Boolean					extra;
	
	numFrames = numSamples/2;
			
	if (0 != (numFrames % 2)) {
		extra = TRUE;
	} else {
		extra = FALSE;
	}

	numFrames = numFrames/2;	
	//
	// Loop over number of filters
	//
	for (j = 0; j < inEQ->numSoftwareFilters; j++) {

		if (inEQ->bypassFilter[j]) {
			continue;
		}
		framesLeft = numFrames;
		
		inFloatBufferPtr = inBuf;
		outFloatBufferPtr = inBuf;
	
		inFloatBufferPtr--;
		outFloatBufferPtr--;

		// 23 float registers total, 3 integer total
		register float b0, b1, b2, a1, a2;
		register float outL1, outR1, inL1, inR1, outL0, outR0, inL0, inR0;
		register float out_tapL1, out_tapL2, out_tapR1, out_tapR2;
		register float in_tapL1, in_tapL2, in_tapR1, in_tapR2;

		b0 = inEQ->b0[j]; 
		b1 = inEQ->b1[j]; 
		b2 = inEQ->b2[j]; 
		a1 = inEQ->a1[j]; 
		a2 = inEQ->a2[j]; 

		in_tapL1 = inEQ->in_del1L[j]; 
		in_tapL2 = inEQ->in_del2L[j];
		out_tapL1 = inEQ->out_del1L[j]; 
		out_tapL2 = inEQ->out_del2L[j];

		in_tapR1 = inEQ->in_del1R[j]; 
		in_tapR2 = inEQ->in_del2R[j];
		out_tapR1 = inEQ->out_del1R[j];
		out_tapR2 = inEQ->out_del2R[j];
		//
		// Loop over each frame
		//
		while (framesLeft-- > 0) {
		
			inL1 = *(++inFloatBufferPtr);
			inR1 = *(++inFloatBufferPtr);
			inL0 = *(++inFloatBufferPtr);
			inR0 = *(++inFloatBufferPtr);

			outL1 = b0*inL1; 
			outR1 = b0*inR1;
			outL0 = b0*inL0; 
			outR0 = b0*inR0;

			outL1 += b1*in_tapL1;
			outR1 += b1*in_tapR1;
			outL0 += b1*inL1;			// new in_tapL1
			outR0 += b1*inR1;			// new in_tapR1

			outL1 += b2*in_tapL2;
			outR1 += b2*in_tapR2;
			outL0 += b2*in_tapL1;		// new in_tapL2
			outR0 += b2*in_tapR1;		// new in_tapR2

			outL1 -= a1*out_tapL1;
			outR1 -= a1*out_tapR1;
			outL0 -= a2*out_tapL1;		// new out_tapL2
			outR0 -= a2*out_tapR1;		// new out_tapR2

			outL1 -= a2*out_tapL2;
			outR1 -= a2*out_tapR2;

			in_tapL2 = inL1;			// oldest input
			in_tapL1 = inL0;			// second oldest input

//			outL1 *= 2.0f;	
//			outR1 *= 2.0f;	

			in_tapR2 = inR1;
			in_tapR1 = inR0;

			outL0 -= a1*outL1; 			// new out_tapL1

			*(++outFloatBufferPtr) = outL1;

			outR0 -= a1*outR1;			// new out_tapR1

//			outL0 *= 2.0f;	

			*(++outFloatBufferPtr) = outR1;

//			outR0 *= 2.0f;	

			out_tapL2 = outL1;			// last output
			out_tapR2 = outR1;			

			*(++outFloatBufferPtr) = outL0;

			out_tapL1 = outL0;			// current output
			out_tapR1 = outR0;

			*(++outFloatBufferPtr) = outR0;
		}
		
		if (extra) {
			inL1 = *(++inFloatBufferPtr);
			inR1 = *(++inFloatBufferPtr);

//			outL1 = 2.0f*(b0*inL1 + b1*in_tapL1 + b2*in_tapL2 - a1*out_tapL1 - a2*out_tapL2);
//			outR1 = 2.0f*(b0*inR1 + b1*in_tapR1 + b2*in_tapR2 - a1*out_tapR1 - a2*out_tapR2);
			outL1 = (b0*inL1 + b1*in_tapL1 + b2*in_tapL2 - a1*out_tapL1 - a2*out_tapL2);
			outR1 = (b0*inR1 + b1*in_tapR1 + b2*in_tapR2 - a1*out_tapR1 - a2*out_tapR2);
			in_tapL2 = in_tapL1;
			in_tapL1 = inL1;
			in_tapR2 = in_tapR1;
			in_tapR1 = inR1;
			out_tapL2 = out_tapL1;
			out_tapL1 = outL1;
			out_tapR2 = out_tapR1;
			out_tapR1 = outR1;

			*(++outFloatBufferPtr) = outL1;
			*(++outFloatBufferPtr) = outR1;
		}
		
		inEQ->in_del1L[j] = in_tapL1; 
		inEQ->in_del2L[j] = in_tapL2;
		inEQ->out_del1L[j] = out_tapL1; 
		inEQ->out_del2L[j] = out_tapL2;

		inEQ->in_del1R[j] = in_tapR1; 
		inEQ->in_del2R[j] = in_tapR2;
		inEQ->out_del1R[j] = out_tapR1;
		inEQ->out_del2R[j] = out_tapR2;
	}
	
	return;
}

#if 1 // unoptimized
IOReturn limiter(float* inFloatBufferPtr, UInt32 inNumSamples, LimiterStructPtr ioLimiterState, UInt32 index)
{
    UInt32 				numFrames;
    float *				outFloatBufferPtr;
    	
	register UInt32 	i, writeIndex, lookahead;
	register float*		floatMixBuf;
	register float* 	inDelayBuffer;
	
	register UInt32		table_index_int;
	register Float32 	inL, inR, threshold, gain, table_index, percent, oneMinusOneOverRatio;
	register Float32	g, oldg, value, log2Peak, oldoutL, oldoutR;
	register Float32 	diffPL, diffPR, diffP, peak, peakL, peakR, oldpeakL, oldpeakR;
	register Float32	Ta, Tr, OneMinusTa, OneMinusTr, temp;
	
	register Float32	log2TableLengthFloat, antilog2TableRatio, antilog2TableLengthFloat;

	if (ioLimiterState->bypass[index])
		return kIOReturnSuccess;
		
	numFrames = inNumSamples >> 1;    
	floatMixBuf = inFloatBufferPtr;
	outFloatBufferPtr = inFloatBufferPtr;
	
	log2TableLengthFloat = (float)kAOALog2TableLength;
	antilog2TableRatio = (float)kAOAAntiLog2TableRatio;
	antilog2TableLengthFloat = (float)kAOAAntiLog2TableLength;
	
	oneMinusOneOverRatio = ioLimiterState->oneMinusOneOverRatio[index];
	threshold = ioLimiterState->threshold[index];
	gain = ioLimiterState->gain[index];
	Ta = ioLimiterState->attackTc[index];
	Tr = ioLimiterState->releaseTc[index];
	OneMinusTa = 1.0f - Ta;
	OneMinusTr = 1.0f - Tr;
	
	oldg = ioLimiterState->prev_g[index];	
	oldpeakL = ioLimiterState->prev_peakL[index];
	oldpeakR = ioLimiterState->prev_peakR[index];

	inDelayBuffer = ioLimiterState->lookaheadDelayBuffer[index];
	writeIndex = ioLimiterState->writeIndex[index];
	lookahead = ioLimiterState->lookahead[index];

	if (kFeedback == ioLimiterState->type[index]) {

		oldoutL = ioLimiterState->prev_outputL[index];
		oldoutR = ioLimiterState->prev_outputR[index];
	
		for (i = 0; i < numFrames; i++) {		
			//
			// Generate peak signal
			//		
			if (oldoutL < 0.0f)
				oldoutL *= -1;
			
			diffPL = oldoutL - oldpeakL;

			if (oldoutR < 0.0f)
				oldoutR *= -1;
			diffPR = oldoutR - oldpeakR;
				
			if(diffPL > 0.0f)
				peakL = OneMinusTa*diffPL + oldpeakL;
			else
				peakL = OneMinusTr * oldpeakL;
	
			oldpeakL = peakL;
			
			if(diffPR > 0.0f)
				peakR = OneMinusTa*diffPR + oldpeakR;
			else
				peakR = OneMinusTr * oldpeakR;
	
			oldpeakR = peakR;
			
			if (peakR > peakL)
				peak = peakR;
			else
				peak = peakL;
			//
			// log2 of peak signal
			//		
#if 1
			temp = peak - 1.0f;
			log2Peak = temp - 0.5f*temp*temp;
#else		
			table_index = peak*log2TableLengthFloat;
	
			if (table_index > log2TableLengthFloat) {
				table_index = log2TableLengthFloat;
			}
			table_index_int = (UInt16)table_index;
			percent = table_index - table_index_int;
			value = aoa_log2table[table_index_int];
			log2Peak = value + percent*(aoa_log2table[table_index_int + 1] - value);
#endif
			//
			// compare to threshold and apply ratio
			//		
			diffP = log2Peak - threshold;
					
			//
			// antilog gain and apply attack/release time
			//		
			if (diffP > 0.0f) {
				
				diffP *= oneMinusOneOverRatio;
			
				table_index = diffP*antilog2TableRatio;
	
				if (table_index > antilog2TableLengthFloat) {
					table_index = antilog2TableLengthFloat;
				}
				table_index_int = (UInt16)table_index;
				percent = table_index - table_index_int;
				value = aoa_antilog2table[table_index_int];
				g = value + percent*(aoa_antilog2table[table_index_int + 1] - value);
	
				g = OneMinusTa*oldg + Ta*g;
			} else {
				g = OneMinusTr*oldg + Tr;
			}
			
			if (lookahead) {
				inL = inDelayBuffer[writeIndex];
				inR = inDelayBuffer[writeIndex+1];
				
				inDelayBuffer[writeIndex] = *(floatMixBuf++);
				inDelayBuffer[++writeIndex] = *(floatMixBuf++);
	
				if (++writeIndex > kDefaultLookahead)
					writeIndex = 0;
			} else {
				inL = *(floatMixBuf++);
				inR = *(floatMixBuf++);
			}					
			//
			// apply input gain
			//
			inL *= gain;
			inR *= gain;	
			//
			// apply gain to signal
			//		
			oldoutL = inL*g;
			oldoutR = inR*g;
			
			*(outFloatBufferPtr++) = oldoutL;
			*(outFloatBufferPtr++) = oldoutR;
	
			oldg = g;
		}

		ioLimiterState->prev_outputL[index] = oldoutL;	
		ioLimiterState->prev_outputR[index] = oldoutR;	

	} else {

		for (i = 0; i < numFrames; i++) {		
		
			//inL = *(floatMixBuf++);
			//inR = *(floatMixBuf++);
			inL = *(floatMixBuf);
			inR = *(floatMixBuf + 1);
			//
			// apply input gain
			//
			inL *= gain;
			inR *= gain;	

			if (inL < 0.0f)
				inL *= -1;
			diffPL = inL - oldpeakL;

			if (inR < 0.0f)
				inR *= -1;
			diffPR = inR - oldpeakR;
			
			if(diffPL > 0.0f)
				peakL = OneMinusTa*diffPL + oldpeakL;
			else
				peakL = OneMinusTr * oldpeakL;
	
			oldpeakL = peakL;
			
			if(diffPR > 0.0f)
				peakR = OneMinusTa*diffPR + oldpeakR;
			else
				peakR = OneMinusTr * oldpeakR;
	
			oldpeakR = peakR;
			
			if (peakR > peakL)
				peak = peakR;
			else
				peak = peakL;
			
			table_index = peak*log2TableLengthFloat;
	
			if (table_index > log2TableLengthFloat) {
				table_index = log2TableLengthFloat;
			}
			table_index_int = (UInt16)table_index;
			percent = table_index - table_index_int;
			value = aoa_log2table[table_index_int];
			log2Peak = value + percent*(aoa_log2table[table_index_int + 1] - value);
	
			diffP = log2Peak - threshold;
					
			if (diffP > 0.0f) {
				
				diffP *= oneMinusOneOverRatio;

				table_index = diffP*antilog2TableRatio;
	
				if (table_index > antilog2TableLengthFloat) {
					table_index = antilog2TableLengthFloat;
				}
				table_index_int = (UInt16)table_index;
				percent = table_index - table_index_int;
				value = aoa_antilog2table[table_index_int];
				g = value + percent*(aoa_antilog2table[table_index_int + 1] - value);
	
				g = OneMinusTa*oldg + Ta*g;
			} else {
				g = OneMinusTr*oldg + Tr;
			}

			if (lookahead) {
				inL = inDelayBuffer[writeIndex];
				inR = inDelayBuffer[writeIndex+1];
				
				inDelayBuffer[writeIndex] = *(floatMixBuf++);
				inDelayBuffer[++writeIndex] = *(floatMixBuf++);
	
				if (++writeIndex > kDefaultLookahead)
					writeIndex = 0;
			} else {
				inL = *(floatMixBuf++);
				inR = *(floatMixBuf++);
			}
			//
			// apply input gain
			//
			inL *= gain;
			inR *= gain;	

			*(outFloatBufferPtr++) = inL*g;
			*(outFloatBufferPtr++) = inR*g;
	
			oldg = g;
		}
	}
	ioLimiterState->prev_g[index] = oldg;
	ioLimiterState->prev_peakL[index] = oldpeakL;	
	ioLimiterState->prev_peakR[index] = oldpeakR;	

	ioLimiterState->writeIndex[index] = writeIndex;
		    
    return kIOReturnSuccess;
}

#else // optimizations attempted...

IOReturn limiter(float* inFloatBufferPtr, UInt32 inNumSamples, LimiterStructPtr ioLimiterState, UInt32 index)
{
    UInt32 				numFrames;
    float *				outFloatBufferPtr;
    	
	register UInt32 	i, writeIndex, lookahead;
	register float *	floatMixBuf;
	register float* 	inDelayBuffer;
	
	register Float32 	threshold, oneMinusOneOverRatio;
	register Float32	oldg, oldpeakL, oldpeakR;
	register Float32	Ta, Tr, OneMinusTa, OneMinusTr;
	
	register Float32	log2TableLengthFloat, antilog2TableRatio, antilog2TableLengthFloat;

	if (ioLimiterState->bypass[index])
		return kIOReturnSuccess;
		
	numFrames = inNumSamples >> 1;    
	floatMixBuf = inFloatBufferPtr;
	outFloatBufferPtr = inFloatBufferPtr;
	
	log2TableLengthFloat = (float)kAOALog2TableLength;
	antilog2TableRatio = (float)kAOAAntiLog2TableRatio;
	antilog2TableLengthFloat = (float)kAOAAntiLog2TableLength;
	
	oneMinusOneOverRatio = ioLimiterState->oneMinusOneOverRatio[index];
	threshold = ioLimiterState->threshold[index];
	Ta = ioLimiterState->attackTc[index];
	Tr = ioLimiterState->releaseTc[index];
	OneMinusTa = 1.0f - Ta;
	OneMinusTr = 1.0f - Tr;
	
	oldg = ioLimiterState->prev_g[index];	
	oldpeakL = ioLimiterState->prev_peakL[index];
	oldpeakR = ioLimiterState->prev_peakR[index];

	inDelayBuffer = ioLimiterState->lookaheadDelayBuffer[index];
	writeIndex = ioLimiterState->writeIndex[index];
	lookahead = ioLimiterState->lookahead[index];

	if (kFeedback == ioLimiterState->type[index]) {

		register UInt32		table_index_int;
		register Float32 	inL, inR, table_index, percent, tempPL, tempPR, peakL, peakR, temp, diffP;
		register Float32	gA, gR, value, log2PeakL, log2PeakR, oldoutLR, oldoutLA, oldoutRR, oldoutRA, oldoutL, oldoutR;
		register Float32 	diffPLpos, diffPLneg, diffPRpos, diffPRneg, diffPL, diffPR, peakLpos, peakLneg, peakRpos, peakRneg;

		oldoutL = ioLimiterState->prev_outputL[index];
		oldoutR = ioLimiterState->prev_outputR[index];
	
		for (i = 0; i < numFrames; i++) {		
			//
			// Generate peak signal
			//		
			diffPLpos = oldoutL;
			oldoutL *= -1;
			diffPRpos = oldoutR;
			oldoutR *= -1;
			diffPLpos -= oldpeakL;
			diffPRpos -= oldpeakR;
			diffPLneg = oldoutL;
			diffPRneg = oldoutR;
			diffPLneg -= oldpeakL; 
			diffPRneg -= oldpeakR; 
			
			if (oldoutL >= 0.0f)
				diffPL = diffPLneg;
			else
				diffPL = diffPLpos;
			if (oldoutR >= 0.0f)
				diffPR = diffPRneg;
			else
				diffPR = diffPRpos;

			peakLpos = OneMinusTa*diffPL;
			peakRpos = OneMinusTa*diffPR;
			peakLneg = OneMinusTr * oldpeakL;
			peakRneg = OneMinusTr * oldpeakR;
			peakLpos += oldpeakL;
			peakRpos += oldpeakR;

			if (diffPL > 0.0f)
				peakL = peakLpos;
			else
				peakL = peakLneg;
			
			if (diffPR > 0.0f)
				peakR = peakRpos;
			else
				peakR = peakRneg;
			
			tempPL = peakL - 1.0f;
			tempPR = peakR - 1.0f;
			temp = oneMinusOneOverRatio*antilog2TableRatio;
			oldpeakL = peakL;
			oldpeakR = peakR;
			log2PeakL = tempPL;
			log2PeakR = tempPR; 
			tempPL = tempPL*tempPL;
			tempPR = tempPR*tempPR;
			log2PeakL -= 0.5f*tempPL;
			log2PeakR -= 0.5f*tempPR;
			diffPL = log2PeakL - threshold;
			diffPR = log2PeakR - threshold;
			diffPL *= temp;
			diffPR *= temp;
			
			if (peakR > peakL) {
				table_index = diffPR;
				diffP = diffPR;
			} else {
				table_index = diffPL;
				diffP = diffPL;
			}
			if (table_index > antilog2TableLengthFloat) {
				table_index = antilog2TableLengthFloat;
			}
			table_index_int = (UInt16)table_index;
			percent = table_index - table_index_int;
			value = aoa_antilog2table[table_index_int];
			temp = aoa_antilog2table[table_index_int + 1];
				 				
			if (lookahead) {
				inL = inDelayBuffer[writeIndex];
				inR = inDelayBuffer[writeIndex+1];
				gA = value;
				temp -= value;
				gR = Tr;

				inDelayBuffer[writeIndex] = *(floatMixBuf++);
				inDelayBuffer[++writeIndex] = *(floatMixBuf++);

				gA += percent*temp;
				gR += OneMinusTr*oldg;
	
				if (++writeIndex > kDefaultLookahead)
					writeIndex = 0;
			} else {
				temp -= value;
				gA = value;

				inL = *(floatMixBuf++);

				gA += percent*temp;
				gR = Tr;
				gR += OneMinusTr*oldg;

				inR = *(floatMixBuf++);
			}					

			gA = Ta*gA;
			gA += OneMinusTa*oldg;

			oldoutLR = inL*gR;
			oldoutRR = inR*gR;
			oldoutLA = inL*gA;
			oldoutRA = inR*gA;

			if (diffP > 0.0f) {
				oldg = gA;
				*(outFloatBufferPtr++) = oldoutLA;
				*(outFloatBufferPtr++) = oldoutRA;
			} else {
				oldg = gR;
				*(outFloatBufferPtr++) = oldoutLR;
				*(outFloatBufferPtr++) = oldoutRR;
			}
		}

		ioLimiterState->prev_outputL[index] = oldoutL;	
		ioLimiterState->prev_outputR[index] = oldoutR;	

	} else {

		register UInt32		table_index_int;
		register Float32 	inL, inR, table_index, percent;
		register Float32	value, log2Peak;
		register Float32	g, diffP;
		register Float32 	diffPL, diffPR, peak, peakL, peakR;

		for (i = 0; i < numFrames; i++) {		
		
			inL = *(floatMixBuf);
			inR = *(floatMixBuf + 1);

			if (inL < 0.0f)
				inL *= -1;
			diffPL = inL - oldpeakL;

			if (inR < 0.0f)
				inR *= -1;
			diffPR = inR - oldpeakR;
			
			if(diffPL > 0.0f)
				peakL = OneMinusTa*diffPL + oldpeakL;
			else
				peakL = OneMinusTr * oldpeakL;
	
			oldpeakL = peakL;
			
			if(diffPR > 0.0f)
				peakR = OneMinusTa*diffPR + oldpeakR;
			else
				peakR = OneMinusTr * oldpeakR;
	
			oldpeakR = peakR;
			
			if (peakR > peakL)
				peak = peakR;
			else
				peak = peakL;
			
			table_index = peak*log2TableLengthFloat;
	
			if (table_index > log2TableLengthFloat) {
				table_index = log2TableLengthFloat;
			}
			table_index_int = (UInt16)table_index;
			percent = table_index - table_index_int;
			value = aoa_log2table[table_index_int];
			log2Peak = value + percent*(aoa_log2table[table_index_int + 1] - value);
	
			diffP = log2Peak - threshold;
					
			if (diffP > 0.0f) {
				
				diffP *= oneMinusOneOverRatio;

				table_index = diffP*antilog2TableRatio;
	
				if (table_index > antilog2TableLengthFloat) {
					table_index = antilog2TableLengthFloat;
				}
				table_index_int = (UInt16)table_index;
				percent = table_index - table_index_int;
				value = aoa_antilog2table[table_index_int];
				g = value + percent*(aoa_antilog2table[table_index_int + 1] - value);
	
				g = OneMinusTa*oldg + Ta*g;
			} else {
				g = OneMinusTr*oldg + Tr;
			}

			if (lookahead) {
				inL = inDelayBuffer[writeIndex];
				inR = inDelayBuffer[writeIndex+1];
				
				inDelayBuffer[writeIndex] = *(floatMixBuf++);
				inDelayBuffer[++writeIndex] = *(floatMixBuf++);
	
				if (++writeIndex > kDefaultLookahead)
					writeIndex = 0;
			} else {
				inL = *(floatMixBuf++);
				inR = *(floatMixBuf++);
			}
			*(outFloatBufferPtr++) = inL*g;
			*(outFloatBufferPtr++) = inR*g;
	
			oldg = g;
		}
	}
	ioLimiterState->prev_g[index] = oldg;
	ioLimiterState->prev_peakL[index] = oldpeakL;	
	ioLimiterState->prev_peakR[index] = oldpeakR;	

	ioLimiterState->writeIndex[index] = writeIndex;
		    
    return kIOReturnSuccess;
}
#endif

#if 0
void crossover3way (const Float32* inSourceP, UInt32 inFramesToProcess)
{
	UInt32 			i;
	Float32* 		output;
	Float32* 		sourceP;
	Float32			ap1, ap2, in, mid;
	
	sourceP = inSourceP;
	
	Float32* outputLow = mLowData;
	Float32* outputMid = mMidData;
	Float32* outputHigh = mHighData;

	Float32 c11 = mAP1_ac1;
	Float32 c12 = mAP1_bc2;
	Float32 c13 = mAP1_bc3;
	Float32 c21 = mAP2_ac1;
	Float32 c22 = mAP2_bc2;
	Float32 c23 = mAP2_bc3;

	Float32 crossover1a_InTap = mCrossover1a_InTap;
	Float32 crossover1a_OutTap = mCrossover1a_OutTap;

	Float32 crossover1b_InTap1 = mCrossover1b_InTap1;
	Float32 crossover1b_InTap2 = mCrossover1b_InTap2;
	Float32 crossover1b_OutTap1 = mCrossover1b_OutTap1;
	Float32 crossover1b_OutTap2 = mCrossover1b_OutTap2;

	Float32 crossover2a_InTap = mCrossover2a_InTap;
	Float32 crossover2a_OutTap = mCrossover2a_OutTap;

	Float32 crossover2b_InTap1 = mCrossover2b_InTap1;
	Float32 crossover2b_InTap2 = mCrossover2b_InTap2;
	Float32 crossover2b_OutTap1 = mCrossover2b_OutTap1;
	Float32 crossover2b_OutTap2 = mCrossover2b_OutTap2;

	for (i = 0; i < inFramesToProcess; i++) {
		in = *sourceP;
		sourceP += inNumChannels;
		
		in *= 0.5f;
		
		ap1 = c11*in + crossover1a_InTap - c11*crossover1a_OutTap;
		ap2 = c13*in + c12*crossover1b_InTap1 + crossover1b_InTap2 - c12*crossover1b_OutTap1 - c13*crossover1b_OutTap2;
		
		*(outputLow++) = ap1 + ap2;
		//*(outputMid++) = mid;
		
		crossover1a_InTap = in;
		crossover1a_OutTap = ap1;

		crossover1b_InTap2 = crossover1b_InTap1;
		crossover1b_InTap1 = in;
		crossover1b_OutTap2 = crossover1b_OutTap1;
		crossover1b_OutTap1 = ap2;
		
		mid = 0.5f*(ap1 - ap2);

		ap1 = c21*mid + crossover2a_InTap - c21*crossover2a_OutTap;
		ap2 = c23*mid + c22*crossover2b_InTap1 + crossover2b_InTap2 - c22*crossover2b_OutTap1 - c23*crossover2b_OutTap2;
		
		// FILTER LOW WITH FIRST ORDER ALLPASS FROM SECOND BRANCH
		
		*(outputMid++) = ap1 + ap2;
		*(outputHigh++) = ap1 - ap2;
		
		crossover2a_InTap = mid;
		crossover2a_OutTap = ap1;

		crossover2b_InTap2 = crossover2b_InTap1;
		crossover2b_InTap1 = mid;
		crossover2b_OutTap2 = crossover2b_OutTap1;
		crossover2b_OutTap1 = ap2;
		 		
	}
	
	mCrossover1a_InTap = crossover1a_InTap;
	mCrossover1a_OutTap = crossover1a_OutTap;

	mCrossover1b_InTap1 = crossover1b_InTap1;
	mCrossover1b_InTap2 = crossover1b_InTap2;
	mCrossover1b_OutTap1 = crossover1b_OutTap1;
	mCrossover1b_OutTap2 = crossover1b_OutTap2;

	mCrossover2a_InTap = crossover2a_InTap;
	mCrossover2a_OutTap = crossover2a_OutTap;

	mCrossover2b_InTap1 = crossover2b_InTap1;
	mCrossover2b_InTap2 = crossover2b_InTap2;
	mCrossover2b_OutTap1 = crossover2b_OutTap1;
	mCrossover2b_OutTap2 = crossover2b_OutTap2;

}
#endif

#if 0	// unoptimized
void crossover2way (float * inFloatBufferPtr, UInt32 numSamples, CrossoverStructPtr ioCrossover)
{
	UInt32 			i, numFrames;
	float* 			input;
	float* 			outputLow;
	float* 			outputHigh;
	float			inTap_1stL, outTap_1stL, inTap_1stR, outTap_1stR;
	float			inTap1_2ndL, inTap2_2ndL, outTap1_2ndL, outTap2_2ndL;
	float			inTap1_2ndR, inTap2_2ndR, outTap1_2ndR, outTap2_2ndR;
	float			ap1L, ap2L, inL;
	float			ap1R, ap2R, inR;
	float			c11, c21, c22;
	
	numFrames = numSamples/2;
	
	input = inFloatBufferPtr;
	
	outputLow = ioCrossover->outBufferPtr[0];
	outputHigh = ioCrossover->outBufferPtr[1];
	
	c11 = ioCrossover->c1_1st[0];
	c21 = ioCrossover->c1_2nd[0];
	c22 = ioCrossover->c2_2nd[0];

	inTap_1stL = ioCrossover->inTap_1stL[0];
	outTap_1stL = ioCrossover->outTap_1stL[0];

	inTap_1stR = ioCrossover->inTap_1stR[0];
	outTap_1stR = ioCrossover->outTap_1stR[0];

	inTap1_2ndL = ioCrossover->inTap1_2ndL[0];
	inTap2_2ndL = ioCrossover->inTap2_2ndL[0];
	outTap1_2ndL = ioCrossover->outTap1_2ndL[0];
	outTap2_2ndL = ioCrossover->outTap2_2ndL[0];

	inTap1_2ndR = ioCrossover->inTap1_2ndR[0];
	inTap2_2ndR = ioCrossover->inTap2_2ndR[0];
	outTap1_2ndR = ioCrossover->outTap1_2ndR[0];
	outTap2_2ndR = ioCrossover->outTap2_2ndR[0];

	--input;
	--outputLow;
	--outputHigh;

	for (i = 0; i < numFrames; i++) {
		inL = *(++input);
		inR = *(++input);
		
		inL *= 0.5f;
		inR *= 0.5f;
		
		ap1L = c11*inL + inTap_1stL - c11*outTap_1stL;
		ap2L = c22*inL + c21*inTap1_2ndL + inTap2_2ndL - c21*outTap1_2ndL - c22*outTap2_2ndL;

		ap1R = c11*inR + inTap_1stR - c11*outTap_1stR;
		ap2R = c22*inR + c21*inTap1_2ndR + inTap2_2ndR - c21*outTap1_2ndR - c22*outTap2_2ndR;
		
		*(++outputLow) = ap1L + ap2L;
		*(++outputLow) = ap1R + ap2R;
		
		*(++outputHigh) = ap1L - ap2L;
		*(++outputHigh) = ap1R - ap2R;

		inTap_1stL = inL;
		outTap_1stL = ap1L;

		inTap_1stR = inR;
		outTap_1stR = ap1R;

		inTap2_2ndL = inTap1_2ndL;
		inTap1_2ndL = inL;
		outTap2_2ndL = outTap1_2ndL;
		outTap1_2ndL = ap2L;
		
		inTap2_2ndR = inTap1_2ndR;
		inTap1_2ndR = inR;
		outTap2_2ndR = outTap1_2ndR;
		outTap1_2ndR = ap2R;		
		 		
	}
	
	ioCrossover->inTap_1stL[0] = inTap_1stL;
	ioCrossover->outTap_1stL[0] = outTap_1stL;

	ioCrossover->inTap_1stR[0] = inTap_1stR;
	ioCrossover->outTap_1stR[0] = outTap_1stR;

	ioCrossover->inTap1_2ndL[0] = inTap1_2ndL;
	ioCrossover->inTap2_2ndL[0] = inTap2_2ndL;
	ioCrossover->outTap1_2ndL[0] = outTap1_2ndL;
	ioCrossover->outTap2_2ndL[0] = outTap2_2ndL;

	ioCrossover->inTap1_2ndR[0] = inTap1_2ndR;
	ioCrossover->inTap2_2ndR[0] = inTap2_2ndR;
	ioCrossover->outTap1_2ndR[0] = outTap1_2ndR;
	ioCrossover->outTap2_2ndR[0] = outTap2_2ndR;
}
#else	// optimized version
void crossover2way (float * inFloatBufferPtr, UInt32 numSamples, CrossoverStructPtr ioCrossover)
{
	register UInt32			i, numFrames, extra, phaseReverseHigh;
	register float * 		input;
	register float * 		outputLow;
	register float *  		outputHigh; 
	
	// 37 fp registers
	register float			outTap_1stL1, outTap_1stR1;
	register float			inTap1_2ndL1, inTap2_2ndL1, outTap1_2ndL1, outTap2_2ndL1;
	register float			inTap1_2ndR1, inTap2_2ndR1, outTap1_2ndR1, outTap2_2ndR1;
	register float			ap13L, ap23L, ap12L, ap22L, ap11L, ap21L, ap10L, ap20L, inL3, inL2, inL1, inL0;
	register float			ap13R, ap23R, ap12R, ap22R, ap11R, ap21R, ap10R, ap20R, inR3, inR2, inR1, inR0;
	register float			c11, c21, c22;

	/*
	Note : all inTap_1stL1 and inTap_1stR1 references where replaced
		   with inTap1_2ndL1 and R1 since they are always the same
		   
		   This version has more flt. pt. registers than the other
		   unrolled-4x crossover, but this one is slightly faster and
		   much easier to understand
	*/
	
	numFrames = numSamples/2;
	
	input = inFloatBufferPtr;
	
	outputLow = ioCrossover->outBufferPtr[0];
	outputHigh = ioCrossover->outBufferPtr[1];
	
	phaseReverseHigh = ioCrossover->phaseReverseHigh;
	
	c11 = ioCrossover->c1_1st[0];
	c21 = ioCrossover->c1_2nd[0];
	c22 = ioCrossover->c2_2nd[0];
	
	outTap_1stL1 = ioCrossover->outTap_1stL[0];
	outTap_1stR1 = ioCrossover->outTap_1stR[0];

	inTap1_2ndL1 = ioCrossover->inTap1_2ndL[0];
	inTap2_2ndL1 = ioCrossover->inTap2_2ndL[0];
	outTap1_2ndL1 = ioCrossover->outTap1_2ndL[0];
	outTap2_2ndL1 = ioCrossover->outTap2_2ndL[0];

	inTap1_2ndR1 = ioCrossover->inTap1_2ndR[0];
	inTap2_2ndR1 = ioCrossover->inTap2_2ndR[0];
	outTap1_2ndR1 = ioCrossover->outTap1_2ndR[0];
	outTap2_2ndR1 = ioCrossover->outTap2_2ndR[0];
        
	--input;
	--outputLow;
	--outputHigh;

	extra = numFrames % 4; //determine if frame size not divisible by for, if not, need to do extra
    numFrames /= 4;
	
	for (i = 0; i < numFrames; i++) { //crossover unrolled 4x to speed up
		inL3 = *(++input);
		inR3 = *(++input);
		inL2 = *(++input);
		inR2 = *(++input);
		inL1 = *(++input);
		inR1 = *(++input);
		inL0 = *(++input);
		inR0 = *(++input);
		
		inL3 *= 0.5f;
		inR3 *= 0.5f;
		inL2 *= 0.5f;
		inR2 *= 0.5f;
		inL1 *= 0.5f;
		inR1 *= 0.5f;
		inL0 *= 0.5f;
		inR0 *= 0.5f;
		
		ap13L = inTap1_2ndL1;
		ap13R = inTap1_2ndR1;
		
		ap23L = c22*inL3;
		ap23R = c22*inR3;
		
		ap12L = inL3;
		ap12R = inR3;
		
		ap22L = c22*inL2;
		ap22R = c22*inR2;
		
		ap13L += c11*inL3;
		ap13R += c11*inR3;
		
		ap23L += c21*inTap1_2ndL1;
		ap23R += c21*inTap1_2ndR1;
		
		ap12L += c11*inL2;
		ap12R += c11*inR2;
		
		ap22L += c21*inL3;
		ap22R += c21*inR3;
		
		ap13L -= c11*outTap_1stL1;
		ap13R -= c11*outTap_1stR1;
				
		ap23L += inTap2_2ndL1;
		ap23R += inTap2_2ndR1;
		
		ap22L += inTap1_2ndL1;
		ap22R += inTap1_2ndR1;
		
		ap11L = inL2;
		ap11R = inR2;
		
		ap23L -= c21*outTap1_2ndL1;
		ap23R -= c21*outTap1_2ndR1;
		
		ap22L -= c22*outTap1_2ndL1;
		ap22R -= c22*outTap1_2ndR1;
		
		ap11L += c11*inL1;
		ap11R += c11*inR1;
		
		ap23L -= c22*outTap2_2ndL1;
		ap23R -= c22*outTap2_2ndR1;
		
		ap21L = c22*inL1;
		ap21R = c22*inR1;
		
		ap10L = inL1;
		ap10R = inR1;
		
		*(++outputLow) = ap13L + ap23L;
		*(++outputLow) = ap13R + ap23R;

		if (phaseReverseHigh) {
			*(++outputHigh) = ap23L - ap13L;
			*(++outputHigh) = ap23R - ap13R;
		} else {
			*(++outputHigh) = ap13L - ap23L;
			*(++outputHigh) = ap13R - ap23R;
		}
		
		ap21L += inL3;
		ap21R += inR3;
				
		ap12L -= c11*ap13L;
		ap12R -= c11*ap13R;
		
		ap22L -= c21*ap23L;
		ap22R -= c21*ap23R;
		
		ap10L += c11*inL0;
		ap10R += c11*inR0;
		
		*(++outputLow) = ap12L + ap22L;
		*(++outputLow) = ap12R + ap22R;

		if (phaseReverseHigh) {
			*(++outputHigh) = ap22L - ap12L;
			*(++outputHigh) = ap22R - ap12R;
		} else {
			*(++outputHigh) = ap12L - ap22L;
			*(++outputHigh) = ap12R - ap22R;
		}
			
		ap11L -= c11*ap12L;
		ap11R -= c11*ap12R;
				
		ap21L += c21*inL2;
		ap21R += c21*inR2;
		
		ap20L = c22*inL0;
		ap20R = c22*inR0;
		
		ap21L -= c21*ap22L;
		ap21R -= c21*ap22R;
		
		ap20L += inL2;
		ap20R += inR2;
		
		ap21L -= c22*ap23L;
		ap21R -= c22*ap23R;
		
		ap20L += c21*inL1;
		ap20R += c21*inR1;
		
		ap10L -= c11*ap11L;
		ap10R -= c11*ap11R;
		
		if (phaseReverseHigh) {
			*(++outputHigh) = ap21L - ap11L;
			*(++outputHigh) = ap21R - ap11R;
		} else {
			*(++outputHigh) = ap11L - ap21L;
			*(++outputHigh) = ap11R - ap21R;
		}
		*(++outputLow) = ap11L + ap21L;
		*(++outputLow) = ap11R + ap21R;
		
		ap20L -= c21*ap21L;
		ap20R -= c21*ap21R;
		
		inTap2_2ndL1 = inL1;
		inTap2_2ndR1 = inR1;
		inTap1_2ndL1 = inL0;
		inTap1_2ndR1 = inR0;
		outTap2_2ndL1 = ap21L;
		outTap2_2ndR1 = ap21R;
		
		ap20L -= c22*ap22L;
		ap20R -= c22*ap22R;
		
		outTap_1stL1 = ap10L;
		outTap_1stR1 = ap10R;
		
		outTap1_2ndL1 = ap20L;
		outTap1_2ndR1 = ap20R;
		
		if (phaseReverseHigh) {
			*(++outputHigh) = ap20L - ap10L;
			*(++outputHigh) = ap20R - ap10R;
		} else {
			*(++outputHigh) = ap10L - ap20L;
			*(++outputHigh) = ap10R - ap20R;
		}
		*(++outputLow) = ap10L + ap20L;
		*(++outputLow) = ap10R + ap20R;
	}
	
	for (i = 0; i < extra; i++) { //do extra ones (ie ones left after rolled out code)
	
		inL3 = *(++input);
		inR3 = *(++input);
		
		ap13L = inTap1_2ndL1;
		ap13R = inTap1_2ndR1;
		
		inL3 *= 0.5f;
		inR3 *= 0.5f;
		
		ap23L = c21*inTap1_2ndL1;
		ap23R = c21*inTap1_2ndR1;
		
		ap13L += c11*inL3;
		ap13R += c11*inR3;
		
		ap23L += c22*inL3;
		ap23R += c22*inR3;
		
		ap13L -= c11*outTap_1stL1;
		ap13R -= c11*outTap_1stR1;
		
		ap23L += inTap2_2ndL1;
		ap23R += inTap2_2ndR1;
		
		inTap2_2ndL1 = inTap1_2ndL1;
		inTap2_2ndR1 = inTap1_2ndR1;
		
		ap23L -= c22*outTap2_2ndL1;
		ap23R -= c22*outTap2_2ndR1;
		
		outTap2_2ndL1 = outTap1_2ndL1;
		outTap2_2ndR1 = outTap1_2ndR1;
		inTap1_2ndL1 = inL3;
		inTap1_2ndR1 = inR3;
		
		ap23L -= c21*outTap1_2ndL1;
		ap23R -= c21*outTap1_2ndR1;

		outTap_1stL1 = ap13L;
		outTap_1stR1 = ap13R;
		outTap1_2ndL1 = ap23L;
		outTap1_2ndR1 = ap23R;
		
		if (phaseReverseHigh) {
			*(++outputHigh) = ap23L - ap13L;
			*(++outputHigh) = ap23R - ap13R;
		} else {
			*(++outputHigh) = ap13L - ap23L;
			*(++outputHigh) = ap13R - ap23R;
		}
		*(++outputLow) = ap13L + ap23L;
		*(++outputLow) = ap13R + ap23R;
	}
	
	ioCrossover->inTap_1stL[0] = inTap1_2ndL1;
	ioCrossover->outTap_1stL[0] = outTap_1stL1;
	
	ioCrossover->inTap_1stR[0] = inTap1_2ndR1;
	ioCrossover->outTap_1stR[0] = outTap_1stR1;
	
	ioCrossover->inTap1_2ndL[0] = inTap1_2ndL1;
	ioCrossover->inTap2_2ndL[0] = inTap2_2ndL1;
	ioCrossover->outTap1_2ndL[0] = outTap1_2ndL1;
	ioCrossover->outTap2_2ndL[0] = outTap2_2ndL1;
	
	ioCrossover->inTap1_2ndR[0] = inTap1_2ndR1;
	ioCrossover->inTap2_2ndR[0] = inTap2_2ndR1;
	ioCrossover->outTap1_2ndR[0] = outTap1_2ndR1;
	ioCrossover->outTap2_2ndR[0] = outTap2_2ndR1;
}
#endif

void multibandLimiter(float *inFloatBufferPtr, UInt32 numSamples, CrossoverStructPtr ioCrossover, LimiterStructPtr ioLimiter) {
	
	float *			lowDataPtr;
	float *			highDataPtr;
	float *			outDataPtr;
	UInt32 			i;
	
	if (2 == ioCrossover->numBands) {
		// 2 way crossover
		crossover2way (inFloatBufferPtr, numSamples, ioCrossover);
		// low band limiter
		limiter (ioCrossover->outBufferPtr[0], numSamples, ioLimiter, 0);
		// high band limiter
		limiter (ioCrossover->outBufferPtr[1], numSamples, ioLimiter, 1);
		// mix to output
		lowDataPtr = ioCrossover->outBufferPtr[0];
		highDataPtr = ioCrossover->outBufferPtr[1];
		
		outDataPtr = inFloatBufferPtr;
		for (i = 0; i < numSamples; i++) {
			*(outDataPtr++) = *(lowDataPtr++) + *(highDataPtr++);
		}
	} else if (1 == ioCrossover->numBands) {
		limiter (inFloatBufferPtr, numSamples, ioLimiter, 0);
	}
}

#pragma mark ------------------------ 
#pragma mark ••• Processing State
#pragma mark ------------------------ 

void setLimiterCoefficients(LimiterParamStructPtr inParams, LimiterStructPtr ioLimiter, UInt32 index, UInt32 inSampleRate) {
	// feedforward or feedback
	ioLimiter->type[index] = inParams->type;
	// convert from dB to log2
	ioLimiter->threshold[index] = inParams->threshold*kdB2Log2Conversion;
	// gain, already scalar
	ioLimiter->gain[index] = inParams->gain;
	// save ratio in form it will be used
	ioLimiter->oneMinusOneOverRatio[index] = 1.0 - 1.0/inParams->ratio;
	// lookahead in ms to sample delay - not yet, right now is just an on/off value
	ioLimiter->lookahead[index] = inParams->lookahead/**inSampleRate*/;
	// convert ms to time constants
	ioLimiter->attackTc[index] = 1.0 - exp(-1.0/(inParams->attack*0.001*inSampleRate));
	ioLimiter->releaseTc[index] = 1.0 - exp(-1.0/(inParams->release*0.001*inSampleRate));
	
	return;
}

void setEQCoefficients(EQParamStructPtr inParams, EQStructPtr ioEQ, UInt32 index, UInt32 inSampleRate) {
	double omega, cosOmega, alpha, A, beta, sinOmega, oneOverA0;

	omega = 2*kPI*inParams->fc/inSampleRate;			
	cosOmega = cos(omega);
	alpha = sin(omega)*sinh(1/(2*inParams->Q));
	
	ioEQ->type[index] = inParams->type;
	
	switch (inParams->type) {
		case kLowPass:

			oneOverA0 = 1.0/(1 + alpha);
		
			ioEQ->b0[index] = 0.5*(1.0 - cosOmega)*oneOverA0;
			ioEQ->b1[index] = (1.0 - cosOmega)*oneOverA0;
			ioEQ->b2[index] = 0.5*(1.0 - cosOmega)*oneOverA0;
			ioEQ->a1[index] = -2*cosOmega*oneOverA0;
			ioEQ->a2[index] = (1 - alpha)*oneOverA0;
			
			//ioEQ->bypassFilter[index] = 0;
			break;
		case kHighPass:

			oneOverA0 = 1.0/(1 + alpha);
		
			ioEQ->b0[index] = 0.5*(1.0 + cosOmega)*oneOverA0;
			ioEQ->b1[index] = (-1.0 - cosOmega)*oneOverA0;
			ioEQ->b2[index] = 0.5*(1.0 + cosOmega)*oneOverA0;
			ioEQ->a1[index] = -2*cosOmega*oneOverA0;
			ioEQ->a2[index] = (1 - alpha)*oneOverA0;
			
			//ioEQ->bypassFilter[index] = 0;
			break;
		case kBandPass:

			oneOverA0 = 1.0/(1 + alpha);
		
			ioEQ->b0[index] = alpha*oneOverA0;
			ioEQ->b1[index] = 0;
			ioEQ->b2[index] = -alpha*oneOverA0;
			ioEQ->a1[index] = -2*cosOmega*oneOverA0;
			ioEQ->a2[index] = (1 - alpha)*oneOverA0;
			
			//ioEQ->bypassFilter[index] = 0;
			break;
		case kBandReject:

			oneOverA0 = 1.0/(1 + alpha);
		
			ioEQ->b0[index] = oneOverA0;
			ioEQ->b1[index] = -2*cosOmega*oneOverA0;
			ioEQ->b2[index] = oneOverA0;
			ioEQ->a1[index] = -2*cosOmega*oneOverA0;
			ioEQ->a2[index] = (1 - alpha)*oneOverA0;
			
			//ioEQ->bypassFilter[index] = 0;
			break;
		case kParametric:
			
			A = pow (10, (inParams->gain*0.025));
			oneOverA0 = 1.0/(1+alpha/A);
		
			ioEQ->b0[index] = (1 + alpha*A)*oneOverA0;
			ioEQ->b1[index] = -2*cosOmega*oneOverA0;
			ioEQ->b2[index] = (1 - alpha*A)*oneOverA0;
			ioEQ->a1[index] = -2*cosOmega*oneOverA0;
			ioEQ->a2[index] = (1 - alpha/A)*oneOverA0;
			
			//ioEQ->bypassFilter[index] = 0;
			break;
		case kLowShelf:
			
			sinOmega = sin(omega);
			A = pow (10, (inParams->gain*0.025));
			beta = sqrt(((A*A) + 1)/inParams->Q - ((A - 1)*(A - 1)));
			oneOverA0 = 1.0/((A+1)+(A-1)*cosOmega+beta*sinOmega);
		
			ioEQ->b0[index] = A*((A+1)-(A-1)*cosOmega+beta*sinOmega)*oneOverA0;
			ioEQ->b1[index] = 2*A*((A-1)-(A+1)*cosOmega)*oneOverA0;
			ioEQ->b2[index] = A*((A+1)-(A-1)*cosOmega-beta*sinOmega)*oneOverA0;
			ioEQ->a1[index] = -2*((A-1)+(A+1)*cosOmega)*oneOverA0;
			ioEQ->a2[index] = ((A+1)+(A-1)*cosOmega-beta*sinOmega)*oneOverA0;
			
			//ioEQ->bypassFilter[index] = 0;
			break;
		case kHighShelf:
			
			sinOmega = sin(omega);
			A = pow (10, (inParams->gain*0.025));
			beta = sqrt(((A*A) + 1)/inParams->Q - ((A - 1)*(A - 1)));
			oneOverA0 = 1.0/((A+1)-(A-1)*cosOmega+beta*sinOmega);
		
			ioEQ->b0[index] = A*((A+1)+(A-1)*cosOmega+beta*sinOmega)*oneOverA0;
			ioEQ->b1[index] = -2*A*((A-1)+(A+1)*cosOmega)*oneOverA0;
			ioEQ->b2[index] = A*((A+1)+(A-1)*cosOmega-beta*sinOmega)*oneOverA0;
			ioEQ->a1[index] = 2*((A-1)-(A+1)*cosOmega)*oneOverA0;
			ioEQ->a2[index] = ((A+1)-(A-1)*cosOmega-beta*sinOmega)*oneOverA0;
			
			//ioEQ->bypassFilter[index] = 0;
			break;
		default:
			break;
	}
}

void setCrossoverCoefficients(CrossoverParamStructPtr inParams, CrossoverStructPtr ioCrossover, UInt32 inSampleRate)
{
	float 					omega, sinOmega, tanHalfOmega, fc;
	UInt32					index;

	ioCrossover->numBands = inParams->numBands;
	ioCrossover->phaseReverseHigh = inParams->phaseReverseHigh;

	for (index = 0; index < (inParams->numBands - 1); index++) {

		fc = inSampleRate*0.5f - inParams->frequency[index];
		omega = 2*kPI*fc/inSampleRate;			
		sinOmega = sin(omega);
		tanHalfOmega = tan(omega*0.5f);
	
		ioCrossover->c1_1st[index] = (1.0f - tanHalfOmega)/(1.0f + tanHalfOmega);
		ioCrossover->c1_2nd[index] = (4.0f*cos(omega))/(2.0f + sinOmega); 
		ioCrossover->c2_2nd[index] = (2.0f - sinOmega)/(2.0f + sinOmega); 
	}	
}

void resetLimiter (LimiterStructPtr ioLimiter) {
	UInt32 				i;

	for (i = 0; i < kMaxNumLimiters; i++) {
		ioLimiter->prev_g[i] = 1.0;	
		ioLimiter->prev_peakL[i] = 0.0;
		ioLimiter->prev_peakR[i] = 0.0;
		ioLimiter->prev_outputL[i] = 0.0;
		ioLimiter->prev_outputR[i] = 0.0;
		ioLimiter->writeIndex[i] = 0;	
	}
	for (i = 0; i < kMaxLookahead; i++) {
		ioLimiter->lookaheadDelayBuffer[0][i] = 0.0;
	}
	return;
}

void resetEQ (EQStructPtr inEQ) {
	UInt32 			index;
	for (index = 0; index < kMaxNumFilters; index++) {
		inEQ->in_del1L[index] = 0.0f;
		inEQ->in_del2L[index] = 0.0f;
		inEQ->out_del1L[index] = 0.0f;
		inEQ->out_del2L[index] = 0.0f;
		inEQ->in_del1R[index] = 0.0f;
		inEQ->in_del2R[index] = 0.0f;
		inEQ->out_del1R[index] = 0.0f;
		inEQ->out_del2R[index] = 0.0f;
	}
}

void resetCrossover (CrossoverStructPtr inCrossover) {
	UInt32 			index;
	
	for (index = 0; index < kMaxNumCrossoverBands; index++) {
		inCrossover->c1_1st[index] = 0.0;	
		inCrossover->c1_2nd[index] = 0.0;	
		inCrossover->c2_2nd[index] = 0.0;	

		inCrossover->inTap_1stL[index] = 0.0;	
		inCrossover->outTap_1stL[index] = 0.0;

		inCrossover->inTap_1stR[index] = 0.0;	
		inCrossover->outTap_1stR[index] = 0.0;
	
		inCrossover->inTap1_2ndL[index] = 0.0;
		inCrossover->inTap2_2ndL[index] = 0.0;
		inCrossover->outTap1_2ndL[index] = 0.0;
		inCrossover->outTap2_2ndL[index] = 0.0;	

		inCrossover->inTap1_2ndR[index] = 0.0;
		inCrossover->inTap2_2ndR[index] = 0.0;
		inCrossover->outTap1_2ndR[index] = 0.0;
		inCrossover->outTap2_2ndR[index] = 0.0;	
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
Boolean Set4thOrderCoefficients (Float32 *b0, Float32 *b1, Float32 *b2, Float32 *a1, Float32 *a2, UInt32 samplingRate)
{
    Boolean 	success = TRUE;

    switch ( samplingRate )
    {
        case 8000:  *b0 =  0.00782020803350;
                    *b1 =  0.01564041606699;
                    *b2 =  0.00782020803350;
                    *a1 = -1.73472576880928;
                    *a2 =  0.76600660094326;
                    break;
       case 11025:  *b0 =  0.00425905333005;
                    *b1 =  0.00851810666010;
                    *b2 =  0.00425905333005;
                    *a1 = -1.80709136077571;
                    *a2 =  0.82412757409590;
                    break;
       case 22050:  *b0 =  0.00111491512001;
                    *b1 =  0.00222983024003;
                    *b2 =  0.00111491512001;
                    *a1 = -1.90335434048751;
                    *a2 =  0.90781400096756;
                    break;
       case 32000:  *b0 =  0.00053716977481;
                    *b1 =  0.00107433954962;
                    *b2 =  0.00053716977481;
                    *a1 = -1.93338022587993;
                    *a2 =  0.93552890497918;
                    break;
	   case 44100: 	*b0 =  0.00028538351548666;
                    *b1 =  0.00057076703097332;
                    *b2 =  0.00028538351548666;
                    *a1 = -1.95165117996464;
                    *a2 =  0.95279271402659;
                    break;
       case 48000:  *b0 =  0.00024135904904198;
                    *b1 =  0.00048271809808396;
                    *b2 =  0.00024135904904198;
                    *a1 = -1.95557824031504;
                    *a2 =  0.95654367651120;
                    break;
        case 96000: *b0 =  0.00006100617875806425;
                    *b1 =  0.0001220123575161285;
                    *b2 =  0.00006100617875806425;
                    *a1 = -1.977786483776763;
                    *a2 =  0.9780305084917958;
                    break;
        default:    // IOLog("\nNot a registered frequency...\n");
                    success = FALSE;
                    break;
    }

    return(success);
}

// this function sets the parameters of a second order all-pass filter that is used to compensate for the phase
// shift of the 4th order lowpass IIR filter used in the iSub crossover.  Note that a0 and b2 are both 1.0.
Boolean Set4thOrderPhaseCompCoefficients (Float32 *b0, Float32 *b1, Float32 *a1, Float32 *a2, UInt32 samplingRate)
{
    Boolean 	success = TRUE;

    switch ( samplingRate )
    {
        case 8000:  *a1 = -1.734725768809275;
                    *a2 =  0.7660066009432638;
                    *b0 =  *a2;
                    *b1 =  *a1;
                    break;
        case 11025: *a1 = -1.807091360775707;
                    *a2 =  0.8241275740958973;
                    *b0 =  *a2;
                    *b1 =  *a1;
                    break;
        case 22050: *a1 = -1.903354340487510;
                    *a2 =  0.9078140009675627;
                    *b0 =  *a2;
                    *b1 =  *a1;
                    break;
        case 32000: *a1 = -1.93338022587993;
                    *a2 =  0.93552890497918;
                    *b0 =  *a2;
                    *b1 =  *a1;
                    break;
        case 44100: *a1 = -1.951651179964643;
                    *a2 =  0.9527927140265903;
                    *b0 =  *a2;
                    *b1 =  *a1;
                    break;
        case 48000: *a1 = -1.955578240315035;
                    *a2 =  0.9565436765112033;
                    *b0 =  *a2;
                    *b1 =  *a1;
                    break;
        case 96000: *a1 = -1.977786483776763;
                    *a2 =  0.9780305084917958;
                    *b0 =  *a2;
                    *b1 =  *a1;
                    break;
        default:    
                    success = FALSE;
                    break;
    }

    return(success);
}

// stereo 4th order LR crossover
// this needs lots of optimization!
void StereoCrossover4thOrderPhaseComp (Float32 *in, Float32 *low, Float32 *high, UInt32 frames, UInt32 SamplingRate, PreviousValues *section1State, PreviousValues *section2State, PreviousValues *phaseCompState)
{
    UInt32	i;
    Float32	inL, inR, outL1, outR1, outL, outR, inPhaseCompL, inPhaseCompR;

    // shared coefficients for second order sections
    Float32	b0, b1, b2, a1, a2;
    // coefficients for phase compensator
    Float32	bp0, bp1, ap1, ap2;
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
 
    // set all coefficients
    if (Set4thOrderCoefficients (&b0, &b1, &b2, &a1, &a2, SamplingRate) == FALSE)
        return;
    if (Set4thOrderPhaseCompCoefficients (&bp0, &bp1, &ap1, &ap2, SamplingRate) == FALSE)
        return;

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

        // phase compensate the input, note that b2 is 1.0
        inPhaseCompL = bp0*inL + bp1*inLTap1_p + inLTap2_p - ap1*outLTap1_p - ap2*outLTap2_p;
        inPhaseCompR = bp0*inR + bp1*inRTap1_p + inRTap2_p - ap1*outRTap1_p - ap2*outRTap2_p;
        
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
void StereoLowPass4thOrder (Float32 *in, Float32 *low, UInt32 frames, UInt32 SamplingRate, PreviousValues *section1State, PreviousValues *section2State)
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
 
    // set all coefficients
    if (Set4thOrderCoefficients (&b0, &b1, &b2, &a1, &a2, SamplingRate) == FALSE)
        return;

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
	asm( "fctiw %0, %1" : "=f" (result) : "f" (B)  );
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
			__asm__ __volatile__ ( "sthbrx %0, %1, %2" : : "r" (copy2), "r" (-2), "r" (dst) );	
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

UInt32 CalculateOffset (UInt64 nanoseconds, UInt32 sampleRate) {
	return (UInt32)((double)sampleRate * kOneOver1000000000) * nanoseconds;
}

void dBfixed2float(UInt32 indBfixed, float* ioGainPtr) {
    float out, temp, frac;
    // get integer part
    int index = (SInt16)(indBfixed >> 16);		
    // if we're out of bounds, saturate both integer and fraction		
    if (index >= kMaxZeroGain) {
        index = kMaxZeroGain;
        indBfixed = 0;
    } else if (index <= -kMinZeroGain) {
        index = -kMinZeroGain;
        indBfixed = 0;
    }    

    // get fractional part
    frac = ((float)((UInt32)(indBfixed & 0x0000FFFF)))*kOneOver65535;		

    // get the base dB converted value
    out = zeroGaindBConvTable[index + kZeroGaindBConvTableOffset];
    // if we have a fractional part, do linear interpolation on our table
    // this is accurate to about 2 decimal places, which is okay but not great
    if (frac > 0.0f) {
        if (index >= 0) {
            temp = zeroGaindBConvTable[index + kZeroGaindBConvTableOffset + 1];
            out = out + frac*(temp - out);			
        } else {
            temp = zeroGaindBConvTable[index + kZeroGaindBConvTableOffset - 1];
            out = out + frac*(temp - out);
        }
    }

    *ioGainPtr = out;
    
    return;
}

void inputGainConverter(UInt32 inGainIndex, float* ioGainPtr) {
    float out = 1.0f;
    // check bounds		
    if (inGainIndex > (2*kInputGaindBConvTableOffset)) {
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
	
	if(floatValue > 8.0) // clip to make sure number fits in 4.20 range
        floatValue = 8.0;
    else if(floatValue < -8.0)
        floatValue = -8.0;

	result_int = (SInt32)(floatValue*scale);
	
    ioFourDotTwenty->fraction3 = (UInt8)(result_int & 0x000000FF); // mask off result and store in result
    ioFourDotTwenty->fraction2 = (UInt8)((result_int & 0x0000FF00) >> 8);
    ioFourDotTwenty->integerAndFraction1 = (UInt8)((result_int & 0x00FF0000) >> 16);
	
    return;
}

void dB2linear (float * indB, float * outLinear)
{
	*outLinear = pow (10, (*indB) * 0.05f);
}

#pragma mark ------------------------ 
#pragma mark ••• Unused Routines
#pragma mark ------------------------ 

#if 0

// 2nd order phase compensator coefficient setting function
// this function sets the parameters of a first order all-pass filter that is used to compensate for the phase
// shift when using a 2nd order lowpass IIR filter for the iSub crossover.  Note that a0 and b1 are both 1.0.
Boolean Set2ndOrderPhaseCompCoefficients (float *b0, float *a1, UInt32 samplingRate)
{
    Boolean 	success = TRUE;

    switch ( samplingRate )
    {
        case 8000:  *a1 = -0.7324848836653277;
                    *b0 =  *a1;
                    break;
        case 11025: *a1 = -0.7985051758519318;
                    *b0 =  *a1;
                    break;
        case 22050: *a1 = -0.8939157008398341;
                    *b0 =  *a1;
                    break;
        case 44100: *a1 = -0.9455137594199962;
                    *b0 =  *a1;
                    break;
        case 48000: *a1 = -0.9498297607998617;
                    *b0 =  *a1;
                    break;
        case 96000: *a1 = -0.9745963490718829;
                    *b0 =  *a1;
                    break;
        default:    // IOLog("\nNot a registered frequency...\n");
                    success = FALSE;
                    break;
    }

    return(success);
}

// bitDepth may be less than 16, e.g. for low-aligned 12 bit samples
void NativeInt16ToFloat32CopyLeftToRight( signed short *src, float *dest, unsigned int count, int bitDepth )
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
		int0 = (++src)[0];		// left 1

		//Software Cycle 2
		int1 = (src++)[0];		// reuse left 1, skip right 1
		int0 += exponentMask;

		//Software Cycle 3
		int2 = (++src)[0];		// left 2
		int1 += exponentMask;
		((long*) dest)[0] = int0;

		//Software Cycle 4
		int3 = (src++)[0];		// reuse left 2, skip right 2
		int2 += exponentMask;
		((long*) dest)[1] = int1;
		//delay one loop for the store to complete

		//Software Cycle 5
		int0 = (++src)[0];		// left 3
		int3 += exponentMask;
		((long*) dest)[2] = int2;
		float0 = dest[0];

		//Software Cycle 6
		int1 = (src++)[0];		// reuse left 3, skip right 3
		int0 += exponentMask;
		((long*) dest)[3] = int3;
		float1 = dest[1];
		float0 -= bias;

		//Software Cycle 7
		int2 = (++src)[0];		// left 4
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


// bitDepth may be less than 16, e.g. for low-aligned 12 bit samples
void NativeInt16ToFloat32CopyLeftToRightGain( signed short *src, float *dest, unsigned int count, int bitDepth, float inGain )
{
	register float bias, gain;
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
	gain = inGain;
	
	src--;
	if( count >= 8 )
	{
		//Software Cycle 1
		int0 = (++src)[0];		// left 1

		//Software Cycle 2
		int1 = (src++)[0];		// reuse left 1, skip right 1
		int0 += exponentMask;

		//Software Cycle 3
		int2 = (++src)[0];		// left 2
		int1 += exponentMask;
		((long*) dest)[0] = int0;

		//Software Cycle 4
		int3 = (src++)[0];		// reuse left 2, skip right 2
		int2 += exponentMask;
		((long*) dest)[1] = int1;
		//delay one loop for the store to complete

		//Software Cycle 5
		int0 = (++src)[0];		// left 3
		int3 += exponentMask;
		((long*) dest)[2] = int2;
		float0 = dest[0];

		//Software Cycle 6
		int1 = (src++)[0];		// reuse left 3, skip right 3
		int0 += exponentMask;
		((long*) dest)[3] = int3;
		float1 = dest[1];
		float0 -= bias;

		//Software Cycle 7
		int2 = (++src)[0];		// left 4
		int1 += exponentMask;
		((long*) dest)[4] = int0;
		float0 *= gain;
		float2 = dest[2];
		float1 -= bias;

		dest--;
		//Software Cycle 8
		int3 = (src++)[0];		// reuse left 4, skip right 4
		int2 += exponentMask;
		((long*) dest)[6] = int1;
		float1 *= gain;
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
			float2 *= gain;
			float0 = dest[4];
			float3 -= bias;
			(++dest)[0] = float1;

			//Software Cycle B
			int1 = (src++)[0];
			int0 += exponentMask;
			((long*) dest)[6] = int3;
			float3 *= gain;
			float1 = dest[4];
			float0 -= bias;
			(++dest)[0] = float2;

			//Software Cycle C
			int2 = (++src)[0];
			int1 += exponentMask;
			((long*) dest)[6] = int0;
			float0 *= gain;
			float2 = dest[4];
			float1 -= bias;
			(++dest)[0] = float3;

			//Software Cycle D
			int3 = (src++)[0];
			int2 += exponentMask;
			((long*) dest)[6] = int1;
			float1 *= gain;
			float3 = dest[4];
			float2 -= bias;
			(++dest)[0] = float0;
		}

		//Software Cycle 7
		int3 += exponentMask;
		((long*) dest)[6] = int2;
		float2 *= gain;
		float0 = dest[4];
		float3 -= bias;
		(++dest)[0] = float1;

		//Software Cycle 6
		((long*) dest)[6] = int3;
		float3 *= gain;
		float1 = dest[4];
		float0 -= bias;
		(++dest)[0] = float2;

		//Software Cycle 5
		float0 *= gain;
		float2 = dest[4];
		float1 -= bias;
		(++dest)[0] = float3;

		//Software Cycle 4
		float1 *= gain;
		float3 = dest[4];
		float2 -= bias;
		(++dest)[0] = float0;

		//Software Cycle 3
		float2 *= gain;
		float3 -= bias;
		(++dest)[0] = float1;

		//Software Cycle 2
		float3 *= gain;
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
		dest[0] *= gain;
		dest++;
		++src;
		((long*) dest)[0] = value;
		dest[0] -= bias;
		dest[0] *= gain;
		dest++;
	}
}

// bitDepth may be less than 16, e.g. for low-aligned 12 bit samples
void NativeInt16ToFloat32CopyRightToLeftGain( signed short *src, float *dest, unsigned int count, int bitDepth, float inGain )
{
	register float bias, gain;
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
	gain = inGain;

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
		float0 *= gain;
		float2 = dest[2];
		float1 -= bias;

		dest--;
		//Software Cycle 8
		int3 = (src++)[0];		// reuse left 4, skip right 4
		int2 += exponentMask;
		((long*) dest)[6] = int1;
		float1 *= gain;
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
			float2 *= gain;
			float0 = dest[4];
			float3 -= bias;
			(++dest)[0] = float1;

			//Software Cycle B
			int1 = (src++)[0];
			int0 += exponentMask;
			((long*) dest)[6] = int3;
			float3 *= gain;
			float1 = dest[4];
			float0 -= bias;
			(++dest)[0] = float2;

			//Software Cycle C
			int2 = (++src)[0];
			int1 += exponentMask;
			((long*) dest)[6] = int0;
			float0 *= gain;
			float2 = dest[4];
			float1 -= bias;
			(++dest)[0] = float3;

			//Software Cycle D
			int3 = (src++)[0];
			int2 += exponentMask;
			((long*) dest)[6] = int1;
			float1 *= gain;
			float3 = dest[4];
			float2 -= bias;
			(++dest)[0] = float0;
		}

		//Software Cycle 7
		int3 += exponentMask;
		((long*) dest)[6] = int2;
		float2 *= gain;
		float0 = dest[4];
		float3 -= bias;
		(++dest)[0] = float1;

		//Software Cycle 6
		((long*) dest)[6] = int3;
		float3 *= gain;
		float1 = dest[4];
		float0 -= bias;
		(++dest)[0] = float2;

		//Software Cycle 5
		float0 *= gain;
		float2 = dest[4];
		float1 -= bias;
		(++dest)[0] = float3;

		//Software Cycle 4
		float1 *= gain;
		float3 = dest[4];
		float2 -= bias;
		(++dest)[0] = float0;

		//Software Cycle 3
		float2 *= gain;
		float3 -= bias;
		(++dest)[0] = float1;

		//Software Cycle 2
		float3 *= gain;
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
		dest[0] *= gain;
		dest++;
		src++;
		((long*) dest)[0] = value;
		dest[0] -= bias;
		dest[0] *= gain;
		dest++;
	}
}


void Float32ToNativeInt16MixAndMuteRight( float *src, signed short *dst, unsigned int count )
{
	register double		scale = 2147483648.0*0.5;
	register double		round = 32768.0*0.5;
	unsigned long		loopCount = count / 8;	// includes count / 2
	long				buffer[2];
	register float		startingFloat;
	register double 	scaled;
	register double 	converted;
	register short		copy;

	count = count / 2;
	
	if( count >= 6 )
	{
		//virtual cycle 1
		startingFloat = (src++)[0];
		startingFloat += (src++)[0];
		
		//virtual cycle 2
		scaled = startingFloat * scale + round;
		startingFloat = (src++)[0];
		startingFloat += (src++)[0];
		
		//virtual cycle 3
		converted = __fctiw( scaled );
		scaled = startingFloat * scale + round;
		startingFloat = (src++)[0];
		startingFloat += (src++)[0];
		
		//virtual cycle 4
		__stfiwx( converted, 0, buffer );
		converted = __fctiw( scaled );
		scaled = startingFloat * scale + round;
		startingFloat = (src++)[0];
		startingFloat += (src++)[0];
		
		//virtual cycle 5
		__stfiwx( converted, sizeof(float), buffer );
		converted = __fctiw( scaled );
		scaled = startingFloat * scale + round;
		startingFloat = (src++)[0];
		startingFloat += (src++)[0];
		
		//virtual cycle 6
		copy = ((short*) buffer)[0];
		__stfiwx( converted, 0, buffer );
		converted = __fctiw( scaled );
		scaled = startingFloat * scale + round;
		startingFloat = (src++)[0];
		startingFloat += (src++)[0];
		
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
			(dst++)[0] = 0;
			startingFloat2 = (src++)[0];
			startingFloat2 += (src++)[0];

			//virtual cycle B
			(dst++)[0] = copy2;
			__asm__ __volatile__ ( "fctiw %0, %1" : "=f" (converted) : "f" ( scaled2 ) );
			copy = ((short*) buffer)[0];
			__asm__ __volatile__ ( "fmadd %0, %1, %2, %3" : "=f" (scaled) : "f" ( startingFloat2), "f" (scale), "f" (round) );
			__asm__ __volatile__ ( "stfiwx %0, %1, %2" : : "f" (converted2), "b%" (0), "r" (buffer) : "memory" );
			(dst++)[0] = 0;
			startingFloat = (src++)[0];
			startingFloat += (src++)[0];
		}
		
		//Virtual Cycle 7
		(dst++)[0] = copy;
		copy = ((short*) buffer)[2];
		__stfiwx( converted, sizeof(float), buffer );
		(dst++)[0] = 0;
		converted = __fctiw( scaled );
		scaled = startingFloat * scale + round;
		
		//Virtual Cycle 8
		(dst++)[0] = copy;
		copy = ((short*) buffer)[0];
		__stfiwx( converted, 0, buffer );
		(dst++)[0] = 0;
		converted = __fctiw( scaled );
	
		//Virtual Cycle 9
		(dst++)[0] = copy;
		copy = ((short*) buffer)[2];
		__stfiwx( converted, sizeof(float), buffer );
		(dst++)[0] = 0;
		
		//Virtual Cycle 10
		(dst++)[0] = copy;
		copy = ((short*) buffer)[0];
		(dst++)[0] = 0;
	
		//Virtual Cycle 11
		(dst++)[0] = copy;
		copy = ((short*) buffer)[2];
		(dst++)[0] = 0;

		//Virtual Cycle 11
		(dst++)[0] = copy;
		(dst++)[0] = 0;
	}

	//clean up any extras
	while( count-- )
	{
		double scaled = (src++)[0] * scale + round;
		scaled += (src++)[0] * scale + round;
		double converted = __fctiw( scaled );
		__stfiwx( converted, 0, buffer );
		(dst++)[0] = buffer[0] >> 16;
		(dst++)[0] = 0;
	}
}

void Float32ToNativeInt32MixAndMuteRight( float *src, signed long *dst, unsigned int count )
{
	register double		scale = 2147483648.0*0.5;
	unsigned long		loopCount;
	register float		startingFloat;
	register double 	scaled;
	register double 	converted;
	register double		oldSetting;

	count = count / 2;

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

	
	if( count >= 3 )
	{
		//virtual cycle 1
		startingFloat = (src++)[0];
		startingFloat += (src++)[0];
		
		//virtual cycle 2
		scaled = startingFloat * scale;
		startingFloat = (src++)[0];
		startingFloat += (src++)[0];
		
		//virtual cycle 3
		converted = __fctiw( scaled );
		scaled = startingFloat * scale;
		startingFloat = (src++)[0];
		startingFloat += (src++)[0];
				
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
			startingFloat2 = (src++)[0];
			__asm__ __volatile__ ( "fmul %0, %1, %2" : "=f" (scaled2) : "f" ( startingFloat), "f" (scale) );
			__asm__ __volatile__ ( "stfiwx %0, %1, %2" : : "f" (converted), "b%" (0), "r" (dst) : "memory" );
			startingFloat2 += (src++)[0];
			__asm__ __volatile__ ( "fctiw %0, %1" : "=f" (converted2) : "f" ( scaled ) );
			(++dst)[0] = 0;
			++dst;
			//virtual cycle B
			startingFloat = (src++)[0];	 
			__asm__ __volatile__ ( "fmul %0, %1, %2" : "=f" (scaled) : "f" ( startingFloat2), "f" (scale) );
			__asm__ __volatile__ ( "stfiwx %0, %1, %2" : : "f" (converted2), "b%" (0), "r" (dst) : "memory" );
			startingFloat += (src++)[0];
			__asm__ __volatile__ ( "fctiw %0, %1" : "=f" (converted) : "f" ( scaled2 ) );
			(++dst)[0] = 0;
			++dst;
		}
		
		//Virtual Cycle 4
		__stfiwx( converted, 0, dst++ );
		converted = __fctiw( scaled );
		__asm__ __volatile__ ( "fmul %0, %1, %2" : "=f" (scaled) : "f" ( startingFloat), "f" (scale) );
		(dst++)[0] = 0;
		
		//Virtual Cycle 5
		__stfiwx( converted, 0, dst++ );
		converted = __fctiw( scaled );
		(dst++)[0] = 0;
	
		//Virtual Cycle 6
		__stfiwx( converted, 0, dst++ );
		(dst++)[0] = 0;
	}

	//clean up any extras
	while( count-- )
	{
		double scaled = (src++)[0] * scale;
		scaled += (src++)[0] * scale;
		double converted = __fctiw( scaled );
		__stfiwx( converted, 0, dst++ );
		(dst++)[0] = 0;
	}

	//restore the old FPSCR setting
	asm volatile( "mtfsf 7, %0" : : "f" (oldSetting) );
}


// bitDepth may be less than 32, e.g. for 24 bits low-aligned in 32-bit words
void NativeInt32ToFloat32CopyLeftToRight( signed long *src, float *dest, unsigned int count, int bitDepth )
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
		int1 = (src++)[0];
		int0 ^= 0x80000000UL;

		//Virtual cycle 3
		int2 = (++src)[0];
		int1 ^= 0x80000000UL;
		transfer.i[1] = int0;

		//Virtual cycle 4
		int3 = (src++)[0];
		int2 ^= 0x80000000UL;
		transfer.i[3] = int1;

		//Virtual cycle 5
		int0 = (++src)[0];
		int3 ^= 0x80000000UL;
		transfer.i[5] = int2;

		//Virtual cycle 6
		int1 = (src++)[0];
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
		int3 = (src++)[0];
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
			int1 = (src++)[0];
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
			int3 = (src++)[0];
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

	count = count / 2;
	while( count-- )
	{
		int0 = (++src)[0];
		int0 ^= 0x80000000UL;
		transfer.i[1] = int0;

		d0 = transfer.d[0];
		d0 -= dBias;
		f0 = d0;
		(++dest)[0] = f0;

		src++;
		(++dest)[0] = f0;
	}
}

// bitDepth may be less than 32, e.g. for 24 bits low-aligned in 32-bit words
void NativeInt32ToFloat32CopyRightToLeft( signed long *src, float *dest, unsigned int count, int bitDepth )
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

	dest--;

	if( count >= 8 )
	{
		count -= 8;
		loopCount = count / 4;
		count &= 3;

		//Virtual cycle 1
		int0 = (++src)[0];

		//Virtual cycle 2
		int1 = (src++)[0];
		int0 ^= 0x80000000UL;

		//Virtual cycle 3
		int2 = (++src)[0];
		int1 ^= 0x80000000UL;
		transfer.i[1] = int0;

		//Virtual cycle 4
		int3 = (src++)[0];
		int2 ^= 0x80000000UL;
		transfer.i[3] = int1;

		//Virtual cycle 5
		int0 = (++src)[0];
		int3 ^= 0x80000000UL;
		transfer.i[5] = int2;

		//Virtual cycle 6
		int1 = (src++)[0];
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
		int3 = (src++)[0];
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
			int1 = (src++)[0];
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
			int3 = (src++)[0];
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

	count = count / 2;
	while( count-- )
	{
		int0 = (++src)[0];
		int0 ^= 0x80000000UL;
		transfer.i[1] = int0;

		d0 = transfer.d[0];
		d0 -= dBias;
		f0 = d0;
		(++dest)[0] = f0;
		src++;
		(++dest)[0] = f0;
	}
}

// bitDepth may be less than 32, e.g. for 24 bits low-aligned in 32-bit words
void NativeInt32ToFloat32CopyLeftToRightGain( signed long *src, float *dest, unsigned int count, int bitDepth, float inGain )
{
	union
	{
		double			d[4];
		unsigned int	i[8];
	} transfer;
	register double 		dBias, gain;
	register unsigned int 	loopCount;
	register long			int0, int1, int2, int3;
	register double			d0, d1, d2, d3;
	register float			f0, f1, f2, f3;

	transfer.i[0] = transfer.i[2] = transfer.i[4] = transfer.i[6] = (0x434UL - bitDepth) << 20;
		//0x41400000UL;
	transfer.i[1] = 0x80000000;

	dBias = transfer.d[0];
	gain = inGain;

	src--;
	dest--;

	if( count >= 8 )
	{
		count -= 8;
		loopCount = count / 4;
		count &= 3;

		//Virtual cycle 1
		int0 = (++src)[0];		//left

		//Virtual cycle 2
		int1 = (src++)[0];		// reuse left skip right
		int0 ^= 0x80000000UL;

		//Virtual cycle 3
		int2 = (++src)[0];		// left
		int1 ^= 0x80000000UL;
		transfer.i[1] = int0;

		//Virtual cycle 4
		int3 = (src++)[0];		// reuse left skip right
		int2 ^= 0x80000000UL;
		transfer.i[3] = int1;

		//Virtual cycle 5
		int0 = (++src)[0];		// left
		int3 ^= 0x80000000UL;
		transfer.i[5] = int2;

		//Virtual cycle 6
		int1 = (src++)[0];		// reuse left skip right
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
		int3 = (src++)[0];
		int2 ^= 0x80000000UL;
		transfer.i[3] = int1;
		d2 = transfer.d[2];
		d1 -= dBias;
		f0 = d0*gain;

		while( loopCount-- )
		{
			//Virtual cycle A
			int0 = (++src)[0];
			int3 ^= 0x80000000UL;
			transfer.i[5] = int2;
			d3 = transfer.d[3];
			d2 -= dBias;
			f1 = d1*gain;
			(++dest)[0] = f0;

			//Virtual cycle B
			int1 = (src++)[0];
			int0 ^= 0x80000000UL;
			transfer.i[7] = int3;
			d0 = transfer.d[0];
			d3 -= dBias;
			f2 = d2*gain;
			(++dest)[0] = f1;

			//Virtual cycle C
			int2 = (++src)[0];
			int1 ^= 0x80000000UL;
			transfer.i[1] = int0;
			d1 = transfer.d[1];
			d0 -= dBias;
			f3 = d3*gain;
			(++dest)[0] = f2;

			//Virtual cycle D
			int3 = (src++)[0];
			int2 ^= 0x80000000UL;
			transfer.i[3] = int1;
			d2 = transfer.d[2];
			d1 -= dBias;
			f0 = d0*gain;
			(++dest)[0] = f3;
		}

		//Virtual cycle 8
		int3 ^= 0x80000000UL;
		transfer.i[5] = int2;
		d3 = transfer.d[3];
		d2 -= dBias;
		f1 = d1*gain;
		(++dest)[0] = f0;

		//Virtual cycle 7
		transfer.i[7] = int3;
		d0 = transfer.d[0];
		d3 -= dBias;
		f2 = d2*gain;
		(++dest)[0] = f1;

		//Virtual cycle 6
		d1 = transfer.d[1];
		d0 -= dBias;
		f3 = d3*gain;
		(++dest)[0] = f2;

		//Virtual cycle 5
		d2 = transfer.d[2];
		d1 -= dBias;
		f0 = d0*gain;
		(++dest)[0] = f3;

		//Virtual cycle 4
		d3 = transfer.d[3];
		d2 -= dBias;
		f1 = d1*gain;
		(++dest)[0] = f0;

		//Virtual cycle 3
		d3 -= dBias;
		f2 = d2*gain;
		(++dest)[0] = f1;

		//Virtual cycle 2
		f3 = d3*gain;
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
		f0 = d0*gain;
		(++dest)[0] = f0;
		++src;
		(++dest)[0] = f0;
	}
}


// bitDepth may be less than 32, e.g. for 24 bits low-aligned in 32-bit words
void NativeInt32ToFloat32CopyRightToLeftGain( signed long *src, float *dest, unsigned int count, int bitDepth, float inGain )
{
	union
	{
		double			d[4];
		unsigned int	i[8];
	} transfer;
	register double 		dBias, gain;
	register unsigned int 	loopCount;
	register long			int0, int1, int2, int3;
	register double			d0, d1, d2, d3;
	register float			f0, f1, f2, f3;

	transfer.i[0] = transfer.i[2] = transfer.i[4] = transfer.i[6] = (0x434UL - bitDepth) << 20;
		//0x41400000UL;
	transfer.i[1] = 0x80000000;

	dBias = transfer.d[0];
	gain = inGain;

	dest--;

	if( count >= 8 )
	{
		count -= 8;
		loopCount = count / 4;
		count &= 3;

		//Virtual cycle 1
		int0 = (++src)[0];

		//Virtual cycle 2
		int1 = (src++)[0];
		int0 ^= 0x80000000UL;

		//Virtual cycle 3
		int2 = (++src)[0];
		int1 ^= 0x80000000UL;
		transfer.i[1] = int0;

		//Virtual cycle 4
		int3 = (src++)[0];
		int2 ^= 0x80000000UL;
		transfer.i[3] = int1;

		//Virtual cycle 5
		int0 = (++src)[0];
		int3 ^= 0x80000000UL;
		transfer.i[5] = int2;

		//Virtual cycle 6
		int1 = (src++)[0];
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
		int3 = (src++)[0];
		int2 ^= 0x80000000UL;
		transfer.i[3] = int1;
		d2 = transfer.d[2];
		d1 -= dBias;
		f0 = d0*gain;

		while( loopCount-- )
		{
			//Virtual cycle A
			int0 = (++src)[0];
			int3 ^= 0x80000000UL;
			transfer.i[5] = int2;
			d3 = transfer.d[3];
			d2 -= dBias;
			f1 = d1*gain;
			(++dest)[0] = f0;

			//Virtual cycle B
			int1 = (src++)[0];
			int0 ^= 0x80000000UL;
			transfer.i[7] = int3;
			d0 = transfer.d[0];
			d3 -= dBias;
			f2 = d2*gain;
			(++dest)[0] = f1;

			//Virtual cycle C
			int2 = (++src)[0];
			int1 ^= 0x80000000UL;
			transfer.i[1] = int0;
			d1 = transfer.d[1];
			d0 -= dBias;
			f3 = d3*gain;
			(++dest)[0] = f2;

			//Virtual cycle D
			int3 = (src++)[0];
			int2 ^= 0x80000000UL;
			transfer.i[3] = int1;
			d2 = transfer.d[2];
			d1 -= dBias;
			f0 = d0*gain;
			(++dest)[0] = f3;
		}

		//Virtual cycle 8
		int3 ^= 0x80000000UL;
		transfer.i[5] = int2;
		d3 = transfer.d[3];
		d2 -= dBias;
		f1 = d1*gain;
		(++dest)[0] = f0;

		//Virtual cycle 7
		transfer.i[7] = int3;
		d0 = transfer.d[0];
		d3 -= dBias;
		f2 = d2*gain;
		(++dest)[0] = f1;

		//Virtual cycle 6
		d1 = transfer.d[1];
		d0 -= dBias;
		f3 = d3*gain;
		(++dest)[0] = f2;

		//Virtual cycle 5
		d2 = transfer.d[2];
		d1 -= dBias;
		f0 = d0*gain;
		(++dest)[0] = f3;

		//Virtual cycle 4
		d3 = transfer.d[3];
		d2 -= dBias;
		f1 = d1*gain;
		(++dest)[0] = f0;

		//Virtual cycle 3
		d3 -= dBias;
		f2 = d2*gain;
		(++dest)[0] = f1;

		//Virtual cycle 2
		f3 = d3*gain;
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
		f0 = d0*gain;
		(++dest)[0] = f0;
		src++;
		(++dest)[0] = f0;
	}
}

#endif
