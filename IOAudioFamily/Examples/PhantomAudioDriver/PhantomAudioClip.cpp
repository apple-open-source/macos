/*
  File:PhantomAudioClip.cpp

  Contains:

  Version:1.0.0

  Copyright:Copyright ) 1997-2000 by Apple Computer, Inc., All Rights Reserved.

Disclaimer:IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc. 
("Apple") in consideration of your agreement to the following terms, and your use, 
installation, modification or redistribution of this Apple software constitutes acceptance 
of these terms.  If you do not agree with these terms, please do not use, install, modify or 
redistribute this Apple software.

In consideration of your agreement to abide by the following terms, and subject
to these terms, Apple grants you a personal, non-exclusive license, under Apple's
copyrights in this original Apple software (the "Apple Software"), to use, reproduce, 
modify and redistribute the Apple Software, with or without modifications, in source and/or
binary forms; provided that if you redistribute the Apple Software in its entirety
and without modifications, you must retain this notice and the following text
and disclaimers in all such redistributions of the Apple Software.  Neither the
name, trademarks, service marks or logos of Apple Computer, Inc. may be used to
endorse or promote products derived from the Apple Software without specific prior
written permission from Apple.  Except as expressly stated in this notice, no
other rights or licenses, express or implied, are granted by Apple herein,
including but not limited to any patent rights that may be infringed by your derivative
works or by other works in which the Apple Software may be incorporated.

The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES NO WARRANTIES, 
EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED WARRANTIES OF NON-INFRINGEMENT,
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE
OR ITS USE AND OPERATION ALONE OR IN COMBINATION WITH YOUR PRODUCTS. IN NO EVENT SHALL APPLE 
BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
OR PROFITS; OR BUSINESS INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE,
REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED
AND WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING NEGLIGENCE), STRICT
LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#include "PhantomAudioEngine.h"
#include <IOKit/IOLib.h>

#define	Use_Optimized	1

#if	Use_Optimized
	#include "PCMBlitterLibPPC.h"
#endif

// The function clipOutputSamples() is called to clip and convert samples from the float mix buffer into the actual
// hardware sample buffer.  The samples to be clipped, are guaranteed not to wrap from the end of the buffer to the
// beginning.
// This implementation is very inefficient, but illustrates the clip and conversion process that must take place.
// Each floating-point sample must be clipped to a range of -1.0 to 1.0 and then converted to the hardware buffer
// format

// The parameters are as follows:
//		mixBuf - a pointer to the beginning of the float mix buffer - its size is based on the number of sample frames
// 					times the number of channels for the stream
//		sampleBuf - a pointer to the beginning of the hardware formatted sample buffer - this is the same buffer passed
//					to the IOAudioStream using setSampleBuffer()
//		firstSampleFrame - this is the index of the first sample frame to perform the clipping and conversion on
//		numSampleFrames - the total number of sample frames to clip and convert
//		streamFormat - the current format of the IOAudioStream this function is operating on
//		audioStream - the audio stream this function is operating on
IOReturn PhantomAudioEngine::clipOutputSamples(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream)
{
    return kIOReturnSuccess;
}

extern "C" {

IOReturn clip24BitSamples(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream)
{
	if (streamFormat->fIsMixable) {
	#if	Use_Optimized
		Float32ToNativeInt32(&(((float *)mixBuf)[firstSampleFrame]), &(((long *)sampleBuf)[firstSampleFrame]), numSampleFrames * streamFormat->fNumChannels);
	#else
		float *floatMixBuf;
		SInt32 *outputBuf;    
		UInt32 sampleIndex;
		UInt32 maxSampleIndex;
		float mixSample;
		
		IOLog( "clip24(%x,%x)\n", firstSampleFrame, numSampleFrames );
		
		floatMixBuf = &(((float *)mixBuf)[firstSampleFrame]);
		outputBuf = (SInt32 *)sampleBuf;
		
		maxSampleIndex = firstSampleFrame + numSampleFrames;
		
		for ( sampleIndex = firstSampleFrame; sampleIndex < maxSampleIndex; sampleIndex++ ) {
			mixSample = *floatMixBuf++;
			mixSample *= 2147483648.0;
			if (mixSample > 2147483392.0) {
				mixSample = 2147483392.0;
			} else if (mixSample < -2147483648.0) {
				mixSample = -2147483648.0;
			}
		
			outputBuf[sampleIndex] = (SInt32)(mixSample);
		}
	#endif
	} else {
		UInt32			offset;

		offset = firstSampleFrame * streamFormat->fNumChannels * (streamFormat->fBitWidth / 8);

		memcpy ((UInt8 *)sampleBuf + offset, (UInt8 *)mixBuf, numSampleFrames * streamFormat->fNumChannels * (streamFormat->fBitWidth / 8));
	}

    return kIOReturnSuccess;
}

IOReturn clip16BitSamples(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream)
{
	if (streamFormat->fIsMixable) {
	#if	Use_Optimized
		Float32ToNativeInt16(&(((float *)mixBuf)[firstSampleFrame]), &(((int16_t *)sampleBuf)[firstSampleFrame]), numSampleFrames * streamFormat->fNumChannels);
	#else
		float *floatMixBuf;
		SInt32 *outputBuf;    
		UInt32 sampleIndex;
		UInt32 maxSampleIndex;
		float mixSample;
		
		floatMixBuf = &(((float *)mixBuf)[firstSampleFrame]);
		outputBuf = &(((SInt32 *)sampleBuf)[firstSampleFrame]);
		
		maxSampleIndex = firstSampleFrame + numSampleFrames;
		
		for ( sampleIndex = firstSampleFrame; sampleIndex < maxSampleIndex; sampleIndex++ ) {
			mixSample = *floatMixBuf++;
			mixSample *= 2147483648.0;
			if (mixSample > 2147483392.0) {
				mixSample = 2147483392.0;
			} else if (mixSample < -2147483648.0) {
				mixSample = -2147483648.0;
			}
	
		outputBuf[sampleIndex] = (SInt32)(mixSample);
	
			++outputBuf;
		}
	#endif
	} else {
		UInt32			offset;

		offset = firstSampleFrame * streamFormat->fNumChannels * (streamFormat->fBitWidth / 8);

		memcpy ((UInt8 *)sampleBuf + offset, (UInt8 *)mixBuf, numSampleFrames * streamFormat->fNumChannels * (streamFormat->fBitWidth / 8));
	}

    return kIOReturnSuccess;
}

IOReturn process24BitSamples(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream)
{
    //IOLog("process24BitSamples(%lx,%lx)\n", firstSampleFrame, numSampleFrames);
    return kIOReturnSuccess;
}

IOReturn process16BitSamples(const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream)
{
    //IOLog("process16BitSamples(%lx,%lx)\n", firstSampleFrame, numSampleFrames);
    return kIOReturnSuccess;
}

}


// The function convertInputSamples() is responsible for converting from the hardware format 
// in the input sample buffer to float samples in the destination buffer and scale the samples 
// to a range of -1.0 to 1.0.  This function is guaranteed not to have the samples wrapped
// from the end of the buffer to the beginning.
// This function only needs to be implemented if the device has any input IOAudioStreams

// The parameters are as follows:
//		sampleBuf - a pointer to the beginning of the hardware formatted sample buffer - this is the same buffer passed
//					to the IOAudioStream using setSampleBuffer()
//		destBuf - a pointer to the float destination buffer - this is the buffer that the CoreAudio.framework uses
//					its size is numSampleFrames * numChannels * sizeof(float)
//		firstSampleFrame - this is the index of the first sample frame to the input conversion on
//		numSampleFrames - the total number of sample frames to convert and scale
//		streamFormat - the current format of the IOAudioStream this function is operating on
//		audioStream - the audio stream this function is operating on
IOReturn PhantomAudioEngine::convertInputSamples(const void *sampleBuf, void *destBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream)
{
#if	Use_Optimized
	NativeInt32ToFloat32(&(((long *)sampleBuf)[firstSampleFrame]), &(((float *)sampleBuf)[firstSampleFrame]), numSampleFrames * streamFormat->fNumChannels, 32);
#else
    SInt32	*inputBuf;
    float	*floatDestBuf;
    SInt32 inputSample;
    
    IOLog( "conv24(%x,%x)\n", firstSampleFrame, numSampleFrames );
    
    floatDestBuf = (float *)destBuf;
    inputBuf = &(((SInt32 *)sampleBuf)[firstSampleFrame]);
    
    while ( numSampleFrames > 0 )
    {
        inputSample = *inputBuf;
        *floatDestBuf++ = (float)(inputSample) * 4.656612873077392578125e-10;
        inputBuf++;
        --numSampleFrames;
    }
#endif
    return kIOReturnSuccess;
}
