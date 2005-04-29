/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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
#include "AppleAC97AudioEngine.h"

#define kMaxSInt16ValueInFloat    (32768.0)
#define kMaxFloatMinusLSBSInt16   (32767.0 / 32768.0)
#define kMinusOne                 (-1.0)

#define ClipFloatValue(x)                \
do {                                     \
    if ((x) > kMaxFloatMinusLSBSInt16)   \
        (x) = kMaxFloatMinusLSBSInt16;   \
    else if ((x) < kMinusOne)            \
        (x) = kMinusOne;                 \
} while(0)

#define ConvertFloatToSInt16(x)          \
    ((SInt16)((x) * kMaxSInt16ValueInFloat))

#define CLASS AppleAC97AudioEngine
#define super IOAudioEngine
OSDefineMetaClassAndAbstractStructors( AppleAC97AudioEngine, IOAudioEngine )

//---------------------------------------------------------------------------

bool CLASS::init( IOAudioDevice * provider, IOAC97AudioCodec * codec )
{
    if (super::init(0) == false)
        return false;

    return true;
}

//---------------------------------------------------------------------------
// clipOutputSamples - Copied from AppleDBDMAAudioClip.c

IOReturn CLASS::clipOutputSamples(
                          const void *                mixBuf,
                          void *                      sampleBuf,
                          UInt32                      firstSampleFrame,
                          UInt32                      numSampleFrames,
                          const IOAudioStreamFormat * streamFormat,
                          IOAudioStream *             audioStream )
{
    float *  inFloatPtr;
    SInt16 * outSInt16Ptr;
    UInt32   numChannels;

#if 0
    kprintf("mix:%p sample:%p 1st:%d num:%d ch:%d\n",
            mixBuf, sampleBuf, firstSampleFrame, numSampleFrames,
            streamFormat->fNumChannels);
#endif

    numChannels  = streamFormat->fNumChannels;
    inFloatPtr   = (float  *)    mixBuf + (firstSampleFrame * numChannels);
    outSInt16Ptr = (SInt16 *) sampleBuf + (firstSampleFrame * numChannels);

    if (numChannels == 6)
    {
        for ( UInt32 i = 0; i < numSampleFrames; i++ ) 
        {
            float tempFloat1 = *(inFloatPtr++);  // L
            float tempFloat2 = *(inFloatPtr++);  // R
            float tempFloat3 = *(inFloatPtr++);  // C
            float tempFloat4 = *(inFloatPtr++);  // SL
            float tempFloat5 = *(inFloatPtr++);  // SR
            float tempFloat6 = *(inFloatPtr++);  // LFE

            ClipFloatValue( tempFloat1 );
            ClipFloatValue( tempFloat2 );
            ClipFloatValue( tempFloat3 );
            ClipFloatValue( tempFloat4 );
            ClipFloatValue( tempFloat5 );
            ClipFloatValue( tempFloat6 );

            *(outSInt16Ptr++) = ConvertFloatToSInt16( tempFloat1 );
            *(outSInt16Ptr++) = ConvertFloatToSInt16( tempFloat2 );
            *(outSInt16Ptr++) = ConvertFloatToSInt16( tempFloat4 );
            *(outSInt16Ptr++) = ConvertFloatToSInt16( tempFloat5 );
            *(outSInt16Ptr++) = ConvertFloatToSInt16( tempFloat3 );
            *(outSInt16Ptr++) = ConvertFloatToSInt16( tempFloat6 );
        }
    }
    else
    {
        UInt32 numSamples = numSampleFrames * numChannels;
    
        for ( UInt32 i = 0; i < ( numSamples / 4 ); i++ ) 
        {
            float tempFloat1 = *(inFloatPtr++);
            float tempFloat2 = *(inFloatPtr++);
            float tempFloat3 = *(inFloatPtr++);
            float tempFloat4 = *(inFloatPtr++);

            ClipFloatValue( tempFloat1 );
            ClipFloatValue( tempFloat2 );
            ClipFloatValue( tempFloat3 );
            ClipFloatValue( tempFloat4 );

            *(outSInt16Ptr++) = ConvertFloatToSInt16( tempFloat1 );
            *(outSInt16Ptr++) = ConvertFloatToSInt16( tempFloat2 );
            *(outSInt16Ptr++) = ConvertFloatToSInt16( tempFloat3 );
            *(outSInt16Ptr++) = ConvertFloatToSInt16( tempFloat4 );
        }

        switch ( numSamples % 4 )
        {
            case 3:
            {
                float tempFloat = *(inFloatPtr++);
                ClipFloatValue( tempFloat );
                *(outSInt16Ptr++) = ConvertFloatToSInt16( tempFloat );
            }
            case 2:
            {
                float tempFloat = *(inFloatPtr++);
                ClipFloatValue( tempFloat );
                *(outSInt16Ptr++) = ConvertFloatToSInt16( tempFloat );
            }
            case 1:
            {
                float tempFloat = *(inFloatPtr++);
                ClipFloatValue( tempFloat );
                *(outSInt16Ptr++) = ConvertFloatToSInt16( tempFloat );
            }
        }
    }
    
    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------

IOReturn CLASS::convertInputSamples(
                            const void *                sampleBuf,
                            void *                      destBuf,
                            UInt32                      firstSampleFrame,
                            UInt32                      numSampleFrames,
                            const IOAudioStreamFormat * streamFormat,
                            IOAudioStream *             audioStream )
{
    UInt32   numSamplesLeft;
    float *  floatDestBuf;
    SInt16 * inputBuf;
    UInt32   numChannels;

    numChannels  = streamFormat->fNumChannels;
    floatDestBuf = (float *) destBuf;
    inputBuf = &(((SInt16 *) sampleBuf)[firstSampleFrame * numChannels]);

    numSamplesLeft = numSampleFrames * numChannels;

    while ( numSamplesLeft > 0 )
    {
        *floatDestBuf = (*inputBuf) / 32767.0;

        inputBuf++;
        floatDestBuf++;
        numSamplesLeft--;
    }

    return kIOReturnSuccess;
}
