#include <libkern/OSTypes.h>
#include <IOKit/IOReturn.h>

#include <IOKit/audio/IOAudioTypes.h>
#include <IOKit/audio/IOAudioDebug.h>




//	floating point types
typedef	float				Float32;
typedef double				Float64;

#define FLOATLIB			FALSE

#define kMaxSInt32ValueInFloat 				(2.1474836480000e9)
#define kInvMaxSInt32ValueInFloat			(4.6566128730774e-10)
#define kMaxSInt16ValueInFloat				(3.2768000000000e4)
#define kInvMaxSInt16ValueInFloat			(3.0517578125000e-5)

#define kMaxSInt8ValueInFloat				(127.0)
#define kInvMaxSInt8ValueInFloat			(7.8740157e-3)


#define kMaxFloatMinusLSBSInt32				(0.99999999953434)
#define kMaxFloatMinusLSBSInt16				(0.99996948242188)
#define kMaxFloatMinusLSBSInt8				(0.9921259843)

const UInt32 kNumChannels = 2;

typedef struct _sPreviousValues {
    Float32	xl_1;
    Float32	xr_1;
    Float32	xl_2;
    Float32	xr_2;
    Float32	yl_1;
    Float32	yr_1;
    Float32	yl_2;
    Float32	yr_2;
} PreviousValues;

#if FLOATLIB
void CoeffsFilterOrder2 (Float32 *Coeff, Float32 CutOffFreq, Float32 AttAtCutOffFreq , Float64 SamplingRate);
#else
Boolean CoeffsFilterOrder2Table (Float32 *Coeff, UInt32 samplingRate);
#endif
void MonoFilter (Float32 *in, Float32 *low, Float32 *high, UInt32 frames, UInt32 samplingRate);
void StereoFilter (Float32 *in, Float32 *low, Float32 *high, UInt32 frames, UInt32 samplingRate, PreviousValues *theValue);

UInt32 CalculateOffset (UInt64 nanoseconds, UInt32 sampleRate) {
	return ((double)sampleRate / 1000000000.0) * nanoseconds;
}

IOReturn clipAppleDBDMAToOutputStream(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat)
{
/*
    UInt32 sampleIndex, maxSampleIndex;
    float *floatMixBuf;
    SInt16 *outputBuf;
*/
    float	*inFloatBufferPtr;
    SInt16	*outSInt16BufferPtr;
    UInt32	numSamples;
    long i;
    
    int straglers;
     
    float minusOnePointOh = -1.0;
    float maxSInt16 = kMaxSInt16ValueInFloat;
    float maxFloatMinusLSBSInt16 = kMaxFloatMinusLSBSInt16;

    inFloatBufferPtr = (float *)mixBuf+firstSampleFrame*streamFormat->fNumChannels;
    outSInt16BufferPtr = (SInt16 *)sampleBuf+firstSampleFrame * streamFormat->fNumChannels;
    inFloatBufferPtr--;
    outSInt16BufferPtr--;
    numSamples = numSampleFrames*streamFormat->fNumChannels;
    straglers = numSamples % 4;

    for(i = 0; i < numSamples / 4 ; i++) 
	{
		float tempFloat1 = *(++inFloatBufferPtr);
		float tempFloat2 = *(++inFloatBufferPtr);
		float tempFloat3 = *(++inFloatBufferPtr);
		float tempFloat4 = *(++inFloatBufferPtr);

		
		if(tempFloat1 > maxFloatMinusLSBSInt16)
			tempFloat1 = maxFloatMinusLSBSInt16;
		else if(tempFloat1 < minusOnePointOh)
			tempFloat1 = minusOnePointOh;
                        
		if(tempFloat2 > maxFloatMinusLSBSInt16)
			tempFloat2 = maxFloatMinusLSBSInt16;
		else if(tempFloat2 < minusOnePointOh)
			tempFloat2 = minusOnePointOh;
                        
                if(tempFloat3 > maxFloatMinusLSBSInt16)
			tempFloat3 = maxFloatMinusLSBSInt16;
		else if(tempFloat3 < minusOnePointOh)
			tempFloat3 = minusOnePointOh;
                        		
		if(tempFloat4 > maxFloatMinusLSBSInt16)
			tempFloat4 = maxFloatMinusLSBSInt16;
		else if(tempFloat4 < minusOnePointOh)
			tempFloat4 = minusOnePointOh;
                        
                        
		*(++outSInt16BufferPtr) = (short)(tempFloat1*maxSInt16);
		*(++outSInt16BufferPtr) = (short)(tempFloat2*maxSInt16);
		*(++outSInt16BufferPtr) = (short)(tempFloat3*maxSInt16);
		*(++outSInt16BufferPtr) = (short)(tempFloat4*maxSInt16);
		
	}
        
         switch (straglers)
        {
            case 3:
            {
                float tempFloat;
		tempFloat = *(++inFloatBufferPtr);
		
		if(tempFloat > kMaxFloatMinusLSBSInt16)
			tempFloat = kMaxFloatMinusLSBSInt16;
		else if(tempFloat < -1.0)
			tempFloat = -1.0;
		
		*(++outSInt16BufferPtr) = (short)(tempFloat*kMaxSInt16ValueInFloat);
            }
            case 2:
            {	
                float tempFloat;
		tempFloat = *(++inFloatBufferPtr);
		
		if(tempFloat > kMaxFloatMinusLSBSInt16)
			tempFloat = kMaxFloatMinusLSBSInt16;
		else if(tempFloat < -1.0)
			tempFloat = -1.0;
		
		*(++outSInt16BufferPtr) = (short)(tempFloat*kMaxSInt16ValueInFloat);
            }
            case 1:
            {
                float tempFloat;
		tempFloat = *(++inFloatBufferPtr);
		
		if(tempFloat > kMaxFloatMinusLSBSInt16)
			tempFloat = kMaxFloatMinusLSBSInt16;
		else if(tempFloat < -1.0)
			tempFloat = -1.0;
		
		*(++outSInt16BufferPtr) = (short)(tempFloat*kMaxSInt16ValueInFloat);
            }
        }        

    
//    floatMixBuf = (float *)mixBuf;
//    outputBuf = (SInt16 *)sampleBuf; 

/*    maxSampleIndex = (firstSampleFrame + numSampleFrames) * streamFormat->fNumChannels;

    for (sampleIndex = (firstSampleFrame * streamFormat->fNumChannels); sampleIndex < maxSampleIndex; sampleIndex++) {
        float inSample;

        inSample = floatMixBuf[sampleIndex];
        if (inSample > 1.0) {
                inSample = 1.0;
        } else if (inSample < -1.0) {
                inSample = -1.0;
        }

        if (inSample >= 0) {
                outputBuf[sampleIndex] = (SInt16) (inSample * 32767.0);
        } else {
                outputBuf[sampleIndex] = (SInt16) (inSample * 32767.0);
        }
    }*/

    return kIOReturnSuccess;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Clip the data and invert the right channel
// This makes the iMac's speakers back in phase
IOReturn clipAppleDBDMAToOutputStreamInvertRightChannel(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat)
{
    UInt32 		sampleIndex, maxSampleIndex;
    float *		floatMixBuf;
    SInt16 *	outputBuf;
    Boolean 	isTwoChannel;
    
    floatMixBuf = (float *)mixBuf;
    outputBuf = (SInt16 *)sampleBuf;
  
    maxSampleIndex = (firstSampleFrame + numSampleFrames) * streamFormat->fNumChannels;
    isTwoChannel = (2 == streamFormat->fNumChannels); // save a register indirection in a tight loop
    
    for (sampleIndex = (firstSampleFrame * streamFormat->fNumChannels); sampleIndex < maxSampleIndex; sampleIndex++) 
    {
        float inSample;
        
        inSample = floatMixBuf[sampleIndex];
        
        if (inSample > 1.0) 
        {
            inSample = 1.0;
        }
        else if (inSample < -1.0) 
        {
            inSample = -1.0;
        }
        
        
        if (inSample >= 0) 
        {
            outputBuf[sampleIndex] = (SInt16) (inSample * 32767.0);
        } 
        else 
        {
            outputBuf[sampleIndex] = (SInt16) (inSample * 32767.0);
        }
            
        // if right channel invert it
        if (isTwoChannel && sampleIndex % 2)
        {
             outputBuf[sampleIndex] ^= (SInt16)0xFFFFFFFF;
        }
    }
    
    return kIOReturnSuccess;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Clip the data, mix the right channel to the left, and mute the right channel
IOReturn clipAppleDBDMAToOutputStreamMixRightChannel(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat)
{
    UInt32 		sampleIndex, maxSampleIndex;
    float *		floatMixBuf;
    SInt16 *	outputBuf;
    
    floatMixBuf = (float *)mixBuf;
    outputBuf = (SInt16 *)sampleBuf;
  
    maxSampleIndex = (firstSampleFrame + numSampleFrames) * streamFormat->fNumChannels;
    
    for (sampleIndex = (firstSampleFrame * streamFormat->fNumChannels); sampleIndex < maxSampleIndex; sampleIndex += 2) 
    {
        float inSample;
        
		// Mix the left sample with the right sample
        inSample = floatMixBuf[sampleIndex] + floatMixBuf[sampleIndex + 1];
		
        if (inSample > 1.0) 
        {
            inSample = 1.0;
        }
        else if (inSample < -1.0) 
        {
            inSample = -1.0;
        }
        
		if (inSample >= 0) 
		{
			outputBuf[sampleIndex] = (SInt16) (inSample * 32767.0);
		} 
		else 
		{
			outputBuf[sampleIndex] = (SInt16) (inSample * 32767.0);
		}

		// Mute the right channel
		outputBuf[sampleIndex + 1] = 0;
    }
    
    return kIOReturnSuccess;
}

IOReturn clipAppleDBDMAToOutputStreamiSub(const void *mixBuf, void *sampleBuf, PreviousValues * filterState, Float32 *low, Float32 *high, UInt32 firstSampleFrame, UInt32 numSampleFrames, UInt32 sampleRate, const IOAudioStreamFormat *streamFormat, SInt16 *iSubBufferMemory, UInt32 *loopCount, SInt32 *iSubBufferOffset, UInt32 iSubBufferLen)
{
    UInt32 sampleIndex, maxSampleIndex;
    float *floatMixBuf;
    SInt16 *outputBuf;
	float highSample;
	Float32	iSubSampleFloat;
	SInt16	iSubSampleInt;

    floatMixBuf = (float *)mixBuf;
    outputBuf = (SInt16 *)sampleBuf;
 
    maxSampleIndex = (firstSampleFrame + numSampleFrames) * streamFormat->fNumChannels;

	// Filter out the highs and lows for use with the iSub
	if (1 == streamFormat->fNumChannels) {
		MonoFilter (&floatMixBuf[firstSampleFrame * streamFormat->fNumChannels], &low[firstSampleFrame * streamFormat->fNumChannels], &high[firstSampleFrame * streamFormat->fNumChannels], numSampleFrames, sampleRate);
	} else if (2 == streamFormat->fNumChannels) {
		StereoFilter (&floatMixBuf[firstSampleFrame * streamFormat->fNumChannels], &low[firstSampleFrame * streamFormat->fNumChannels], &high[firstSampleFrame * streamFormat->fNumChannels], numSampleFrames, sampleRate, filterState);
	}

	for (sampleIndex = (firstSampleFrame * streamFormat->fNumChannels); sampleIndex < maxSampleIndex; sampleIndex++) {
		highSample = high[sampleIndex];
		if (highSample > 1.0) {
			highSample = 1.0;
		} else if (highSample < -1.0) {
			highSample = -1.0;
		}

		if (highSample >= 0) { 
			outputBuf[sampleIndex] = (SInt16) (highSample * 32767.0);
		} else {
			outputBuf[sampleIndex] = (SInt16) (highSample * 32767.0);
		}

		iSubSampleFloat = low[sampleIndex];
		if (iSubSampleFloat > 1.0) {
			iSubSampleFloat = 1.0;
		} else if (iSubSampleFloat < -1.0) {
			iSubSampleFloat = -1.0;
		}

		if (iSubSampleFloat >= 0) {
			iSubSampleInt = (SInt16) (iSubSampleFloat * 32767.0);
		} else {
			iSubSampleInt = (SInt16) (iSubSampleFloat * 32767.0);
		}

		if (*iSubBufferOffset >= iSubBufferLen) {
			*iSubBufferOffset = 0;
			(*loopCount)++;
		}

		iSubBufferMemory[(*iSubBufferOffset)++] = ((((UInt16)iSubSampleInt) << 8) & 0xFF00) | ((((UInt16)iSubSampleInt) >> 8) & 0x00FF);
	}

    return kIOReturnSuccess;
}

IOReturn clipAppleDBDMAToOutputStreamiSubInvertRightChannel(const void *mixBuf, void *sampleBuf, PreviousValues * filterState, Float32 *low, Float32 *high, UInt32 firstSampleFrame, UInt32 numSampleFrames, UInt32 sampleRate, const IOAudioStreamFormat *streamFormat, SInt16 *iSubBufferMemory, UInt32 *loopCount, SInt32 *iSubBufferOffset, UInt32 iSubBufferLen)
{
    UInt32 sampleIndex, maxSampleIndex;
    float *floatMixBuf;
    SInt16 *outputBuf;
	float highSample;
	Float32	iSubSampleFloat;
	SInt16	iSubSampleInt;
    register Boolean isTwoChannel;
    floatMixBuf = (float *)mixBuf;
    outputBuf = (SInt16 *)sampleBuf;
    
    isTwoChannel = (2 == streamFormat->fNumChannels); // save a register indirection in a tight loop
    
	maxSampleIndex = (firstSampleFrame + numSampleFrames) * streamFormat->fNumChannels;

	// Filter out the highs and lows for use with the iSub
	if (1 == streamFormat->fNumChannels) {
		MonoFilter (&floatMixBuf[firstSampleFrame * streamFormat->fNumChannels], &low[firstSampleFrame * streamFormat->fNumChannels], &high[firstSampleFrame * streamFormat->fNumChannels], numSampleFrames, sampleRate);
	} else if (2 == streamFormat->fNumChannels) {
		StereoFilter (&floatMixBuf[firstSampleFrame * streamFormat->fNumChannels], &low[firstSampleFrame * streamFormat->fNumChannels], &high[firstSampleFrame * streamFormat->fNumChannels], numSampleFrames, sampleRate, filterState);
	}

	for (sampleIndex = (firstSampleFrame * streamFormat->fNumChannels); sampleIndex < maxSampleIndex; sampleIndex++) {
		highSample = high[sampleIndex];
		if (highSample > 1.0) {
			highSample = 1.0;
		} else if (highSample < -1.0) {
			highSample = -1.0;
		}

		if (highSample >= 0) { 
			outputBuf[sampleIndex] = (SInt16) (highSample * 32767.0);
		} else {
			outputBuf[sampleIndex] = (SInt16) (highSample * 32767.0);
		}
        
        // if right channel then invert it
        if (isTwoChannel && sampleIndex % 2)
        {
             outputBuf[sampleIndex] ^= (SInt16)0xFFFFFFFF;
        }

		iSubSampleFloat = low[sampleIndex];
		if (iSubSampleFloat > 1.0) {
			iSubSampleFloat = 1.0;
		} else if (iSubSampleFloat < -1.0) {
			iSubSampleFloat = -1.0;
		}

		if (iSubSampleFloat >= 0) {
			iSubSampleInt = (SInt16) (iSubSampleFloat * 32767.0);
		} else {
			iSubSampleInt = (SInt16) (iSubSampleFloat * 32767.0);
		}

		if (*iSubBufferOffset >= iSubBufferLen) {
			*iSubBufferOffset = 0;
			(*loopCount)++;
		}

		iSubBufferMemory[(*iSubBufferOffset)++] = ((((UInt16)iSubSampleInt) << 8) & 0xFF00) | ((((UInt16)iSubSampleInt) >> 8) & 0x00FF);
	}

    return kIOReturnSuccess;
}

IOReturn clipAppleDBDMAToOutputStreamiSubMixRightChannel(const void *mixBuf, void *sampleBuf, PreviousValues * filterState, Float32 *low, Float32 *high, UInt32 firstSampleFrame, UInt32 numSampleFrames, UInt32 sampleRate, const IOAudioStreamFormat *streamFormat, SInt16 *iSubBufferMemory, UInt32 *loopCount, SInt32 *iSubBufferOffset, UInt32 iSubBufferLen)
{
    UInt32 sampleIndex, maxSampleIndex;
    float *floatMixBuf;
    SInt16 *outputBuf;
	float highSample;
	Float32	iSubSampleFloat;
	SInt16	iSubSampleInt;
    floatMixBuf = (float *)mixBuf;
    outputBuf = (SInt16 *)sampleBuf;
    
	maxSampleIndex = (firstSampleFrame + numSampleFrames) * streamFormat->fNumChannels;

	// Filter out the highs and lows for use with the iSub
	if (1 == streamFormat->fNumChannels) {
		MonoFilter (&floatMixBuf[firstSampleFrame * streamFormat->fNumChannels], &low[firstSampleFrame * streamFormat->fNumChannels], &high[firstSampleFrame * streamFormat->fNumChannels], numSampleFrames, sampleRate);
	} else if (2 == streamFormat->fNumChannels) {
		StereoFilter (&floatMixBuf[firstSampleFrame * streamFormat->fNumChannels], &low[firstSampleFrame * streamFormat->fNumChannels], &high[firstSampleFrame * streamFormat->fNumChannels], numSampleFrames, sampleRate, filterState);
	}

	for (sampleIndex = (firstSampleFrame * streamFormat->fNumChannels); sampleIndex < maxSampleIndex; sampleIndex++) {
		// Mix the left and right channels together
		highSample = high[sampleIndex] + high[sampleIndex + 1];
		if (highSample > 1.0) {
			highSample = 1.0;
		} else if (highSample < -1.0) {
			highSample = -1.0;
		}

		// Mute right channel
        if (sampleIndex % 2) {
			outputBuf[sampleIndex] = 0;
		} else {
			if (highSample >= 0) { 
				outputBuf[sampleIndex] = (SInt16) (highSample * 32767.0);
			} else {
				outputBuf[sampleIndex] = (SInt16) (highSample * 32767.0);
			}
		}
        
		iSubSampleFloat = low[sampleIndex];
		if (iSubSampleFloat > 1.0) {
			iSubSampleFloat = 1.0;
		} else if (iSubSampleFloat < -1.0) {
			iSubSampleFloat = -1.0;
		}

		if (iSubSampleFloat >= 0) {
			iSubSampleInt = (SInt16) (iSubSampleFloat * 32767.0);
		} else {
			iSubSampleInt = (SInt16) (iSubSampleFloat * 32767.0);
		}

		if (*iSubBufferOffset >= iSubBufferLen) {
			*iSubBufferOffset = 0;
			(*loopCount)++;
		}

		iSubBufferMemory[(*iSubBufferOffset)++] = ((((UInt16)iSubSampleInt) << 8) & 0xFF00) | ((((UInt16)iSubSampleInt) >> 8) & 0x00FF);
	}

    return kIOReturnSuccess;
}

IOReturn convertAppleDBDMAFromInputStream(const void *sampleBuf, void *destBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat)
{
    UInt32 numSamplesLeft;
    float *floatDestBuf;
    SInt16 *inputBuf;
        
    floatDestBuf = (float *)destBuf;
    inputBuf = &(((SInt16 *)sampleBuf)[firstSampleFrame * streamFormat->fNumChannels]);
    
    numSamplesLeft = numSampleFrames * streamFormat->fNumChannels;
    
    while (numSamplesLeft > 0) {
        SInt16 inputSample;
        
        inputSample = *inputBuf;
    
        if (inputSample >= 0) {
            *floatDestBuf = inputSample / 32767.0;
        } else {
            *floatDestBuf = inputSample / 32767.0;
        }
                
        ++inputBuf;
        ++floatDestBuf;
        --numSamplesLeft;
    }
        
    return kIOReturnSuccess;
}

#if FLOATLIB
/*
	***CoeffsFilterOrder2***

	This function fills in the order2 filter coefficients table used in the MonoFilter & StereoFilter functions
	Float32  *Coeff			: is a pointer to the coefficients table
	Float32  CutOffFreq		: is the cut off frequency of the filter (in Hertz)
	Float32  AttAtCutOffFreq: is the attenuation at the cut off frequency (in linear)
	Float64  SamplingRate	: is the sampling rate frequency (in Hertz)
*/
void CoeffsFilterOrder2 (Float32 *Coeff, Float32 CutOffFreq, Float32 AttAtCutOffFreq , Float64 SamplingRate)
{
	Float32	k, nu0, pi=3.14159, Att, norm;

	nu0 = (Float32) (CutOffFreq / SamplingRate);
	Att = 1 / AttAtCutOffFreq;
	k = 1/(tan(pi*nu0));
	norm = k*(k+Att)+1;

	/*
	the first 3 coefficients are Num[0], Num[1] & Num[2] in that order
	the last 2 coeffients are Den[1] & Den[2]
	where [.] is the z exposant
	*/
	Coeff[0] = 1.0 / norm;
	Coeff[1] = 2.0 / norm;
	Coeff[2] = 1.0 / norm;
	Coeff[3] = 2*(1-k*k) / norm;
	Coeff[4] = (k*(k-Att)+1) / norm;

	return;
}
#else
/*
    ***CoeffsFilterOrder2Table***

    This function choose an order2 filter coefficients table used in the MonoFilter & StereoFilter functions
    The coefficients table depend on the sampling rate
    Float32  *Coeff		: is a pointer on the coefficients table
    Float64  SamplingRate	: is the sampling rate frequency (in Hertz)
    return: - FALSE if the sampling rate frequency doesn't exist
            - TRUE  otherwise...
*/
Boolean CoeffsFilterOrder2Table (Float32 *Coeff, UInt32 samplingRate)
{
    Boolean 	success = TRUE;

    switch ( samplingRate )
    {
        case 8000:  Coeff[0] =  0.00208054389804601669;
                    Coeff[1] =  0.00416108779609203339;
                    Coeff[2] =  0.00208054389804601669;
                    Coeff[3] = -1.86687481403350830078;
                    Coeff[4] =  0.87519699335098266602;
                    break;
        case 11025: Coeff[0] =  0.00111490569543093443;
                    Coeff[1] =  0.00222981139086186886;
                    Coeff[2] =  0.00111490569543093443;
                    Coeff[3] = -1.90334117412567138672;
                    Coeff[4] =  0.90780085325241088867;
                    break;
        case 22050: Coeff[0] =  0.00028538206242956221;
                    Coeff[1] =  0.00057076412485912442;
                    Coeff[2] =  0.00028538206242956221;
                    Coeff[3] = -1.95164430141448974609;
                    Coeff[4] =  0.95278578996658325195;
                    break;
        case 44100: Coeff[0] =  0.00007220284896902740;
                    Coeff[1] =  0.00014440569793805480;
                    Coeff[2] =  0.00007220284896902740;
                    Coeff[3] = -1.97581851482391357422;
                    Coeff[4] =  0.97610741853713989258;
                    break;
        case 48000: Coeff[0] =  0.00006100598693592474;
                    Coeff[1] =  0.00012201197387184948;
                    Coeff[2] =  0.00006100598693592474;
                    Coeff[3] = -1.97778332233428955078;
                    Coeff[4] =  0.97802722454071044922;
                    break;
        case 96000: Coeff[0] =  0.00001533597242087126;
                    Coeff[1] =  0.00003067194484174252;
                    Coeff[2] =  0.00001533597242087126;
                    Coeff[3] = -1.98889136314392089844;
                    Coeff[4] =  0.98895263671875000000;
                    break;
        default:    // IOLog("\nNot a registered frequency...\n");
                    success = FALSE;
                    break;
    }

    return(success);
}
#endif

/*
	***MonoFilter***

	Mono Order2 Filter
	Float32  *in	: is a pointer to the entry array -> signal to filter...
	Float32  *low	: is a pointer to the low-pass filtered signal
	Float32  *high 	: is a pointer to the high-pass filtered signal
	UInt32   samples: is the number of samples in each array
	Float64  SamplingRate	: is the sampling rate frequency (in Hertz)
	At the n instant: x is x[n], xx is x[n-1], xxx is x[n-2] (it's the same for y)
*/
void MonoFilter (Float32 *in, Float32 *low, Float32 *high, UInt32 frames, UInt32 samplingRate)
{
	UInt32	idx;
#if !FLOATLIB
	Boolean	success;
#endif
	Float32	LP_Coeff[5];
	Float32	x, xx, xxx, y, yy, yyy;

	// init
#if FLOATLIB
	CoeffsFilterOrder2 (LP_Coeff, 120, 1/sqrt(2), 44100);
#else
	success = CoeffsFilterOrder2Table (LP_Coeff, samplingRate);
    if (success == FALSE) goto End;
#endif
	x=xx=xxx=y=yy=yyy=0;
	// convolution
	for ( idx = 0 ; idx < frames ; idx++ )
	{
		x = in[idx];
		// Low-pass filter
		y = (LP_Coeff[0]*x + LP_Coeff[1]*xx + LP_Coeff[2]*xxx - LP_Coeff[3]*yy - LP_Coeff[4]*yyy);
		// Update
		xxx = xx;
		xx = x;
		yyy = yy;
		yy = y;
		// Storage
		low[idx] = y;
		high[idx] = x-y;
	}

#if !FLOATLIB
End:
#endif
	return;
}

/*
    ***StereoFilter***

    Stereo Order2 Filter
    Float32  *in		: is a pointer on the entry array -> signal to filter...
    Float32  *low		: is a pointer on the low-pass filtered signal
    Float32  *high 		: is a pointer on the high-pass filtered signal
    UInt32   samples		: is the number of samples in each array
    Float64  SamplingRate	: is the sampling rate frequency (in Hertz)
    At the n instant: x is x[n], x_1 is x[n-1], x_2 is x[n-2] (it's the same for y)
*/
void StereoFilter (Float32 *in, Float32 *low, Float32 *high, UInt32 frames, UInt32 SamplingRate, PreviousValues *theValue)
{
    UInt32	idx;
    Boolean	success;
    Float32	LP_Coeff[5];
    Float32	xl, xr, yl, yr;

    // Get the filter coefficents
    //CoeffsFilterOrder2 (&LP_Coeff, 120, 0.707, SamplingRate); //don't used because of the tan() function
    success = CoeffsFilterOrder2Table (LP_Coeff, SamplingRate);
    if (success == FALSE)  goto End;
    // convolution
    for ( idx = 0 ; idx < frames ; idx ++ )
    {
        xl = in[2*idx];
        xr = in[2*idx+1];
     // Low-pass filter
        yl = (LP_Coeff[0]*xl + LP_Coeff[1]*theValue->xl_1 + LP_Coeff[2]*theValue->xl_2 - LP_Coeff[3]*theValue->yl_1 - LP_Coeff[4]*theValue->yl_2);
        yr = (LP_Coeff[0]*xr + LP_Coeff[1]*theValue->xr_1 + LP_Coeff[2]*theValue->xr_2 - LP_Coeff[3]*theValue->yr_1 - LP_Coeff[4]*theValue->yr_2);
     // Update
        theValue->xl_2 = theValue->xl_1;
        theValue->xr_2 = theValue->xr_1;
        theValue->xl_1 = xl;
        theValue->xr_1 = xr;
        theValue->yl_2 = theValue->yl_1;
        theValue->yr_2 = theValue->yr_1;
        theValue->yl_1 = yl;
        theValue->yr_1 = yr;
     // Storage
        low[2*idx] = yl;
        low[2*idx+1] = yr;
        high[2*idx] = xl-yl;
        high[2*idx+1] = xr-yr;
    }
End:
    return;
}
