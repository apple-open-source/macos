/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <IOKit/audio/IOAudioTypes.h>
#include "AC97AudioEngine.h"

#define kMaxSInt16ValueInFloat           (3.2768000000000e4)
#define kMaxFloatMinusLSBSInt32          (0.99999999953434)
#define kMaxFloatMinusLSBSInt16          (0.99996948242188)
#define kMaxFloatMinusLSBSInt8           (0.9921259843)
#define kMinusOne                        (-1.0)

#define ClipFloatValue(x)                \
do {                                     \
    if ((x) > kMaxFloatMinusLSBSInt16)   \
        (x) = kMaxFloatMinusLSBSInt16;   \
    else if ((x) < kMinusOne)            \
        (x) = kMinusOne;                 \
} while(0)

#define ConvertFloatToSInt16(x)          \
	((SInt16)((x) * kMaxSInt16ValueInFloat))

//---------------------------------------------------------------------------
// clipOutputSamples - Copied from AppleDBDMAAudioClip.c

IOReturn
AppleIntelAC97AudioEngine::clipOutputSamples(
                           const void *                mixBuf,
                           void *                      sampleBuf,
                           UInt32                      firstSampleFrame,
                           UInt32                      numSampleFrames,
                           const IOAudioStreamFormat * streamFormat,
                           IOAudioStream *             audioStream )
{
    float *  inFloatBufferPtr;
    SInt16 * outSInt16BufferPtr;
    UInt32	 numSamples;

#if 0
    IOLog("mix:%p sample:%p 1st:%d num:%d ch:%d\n",
          mixBuf, sampleBuf, firstSampleFrame, numSampleFrames,
          streamFormat->fNumChannels);
#endif

    inFloatBufferPtr   = (float *) mixBuf +
                         firstSampleFrame * streamFormat->fNumChannels;
    
    outSInt16BufferPtr = (SInt16 *) sampleBuf +
                         firstSampleFrame * streamFormat->fNumChannels;

    inFloatBufferPtr--;
    outSInt16BufferPtr--;

    numSamples = numSampleFrames * streamFormat->fNumChannels;

    for ( UInt32 i = 0; i < ( numSamples / 4 ); i++ ) 
	{
		float tempFloat1 = *(++inFloatBufferPtr);
		float tempFloat2 = *(++inFloatBufferPtr);
		float tempFloat3 = *(++inFloatBufferPtr);
		float tempFloat4 = *(++inFloatBufferPtr);

        ClipFloatValue( tempFloat1 );
        ClipFloatValue( tempFloat2 );
        ClipFloatValue( tempFloat3 );
        ClipFloatValue( tempFloat4 );

		*(++outSInt16BufferPtr) = ConvertFloatToSInt16( tempFloat1 );
		*(++outSInt16BufferPtr) = ConvertFloatToSInt16( tempFloat2 );
		*(++outSInt16BufferPtr) = ConvertFloatToSInt16( tempFloat3 );
		*(++outSInt16BufferPtr) = ConvertFloatToSInt16( tempFloat4 );
    }

    switch ( numSamples % 4 )
    {
        case 3:
        {
            float tempFloat = *(++inFloatBufferPtr);
            
            ClipFloatValue( tempFloat );

            *(++outSInt16BufferPtr) = ConvertFloatToSInt16( tempFloat );
        }
        case 2:
        {	
            float tempFloat = *(++inFloatBufferPtr);
            
            ClipFloatValue( tempFloat );
		
            *(++outSInt16BufferPtr) = ConvertFloatToSInt16( tempFloat );
        }
        case 1:
        {
            float tempFloat = *(++inFloatBufferPtr);
		
			ClipFloatValue( tempFloat );
		
            *(++outSInt16BufferPtr) = ConvertFloatToSInt16( tempFloat );
        }
    }

    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------

IOReturn
AppleIntelAC97AudioEngine::convertInputSamples(
                           const void *                sampleBuf,
                           void *                      destBuf,
                           UInt32                      firstSampleFrame,
                           UInt32                      numSampleFrames,
                           const IOAudioStreamFormat * streamFormat,
                           IOAudioStream *             audioStream )
{
    return kIOReturnSuccess;
}
