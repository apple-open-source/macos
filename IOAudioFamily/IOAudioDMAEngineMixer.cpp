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

#include <IOKit/audio/IOAudioDMAEngine.h>
#include <IOKit/audio/IOAudioStream.h>
#include <IOKit/audio/IOAudioTypes.h>
#include <IOKit/audio/IOAudioDebug.h>

IOReturn IOAudioDMAEngine::mixAndClip(const void *sourceBuf, void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream)
{
    IOReturn result = kIOReturnSuccess;
    
#ifdef DEBUG_OUTPUT_CALLS
    IOLog("IOAudioDMAEngine[0x%x]::mixAndClip(0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x)\n", this, sourceBuf, mixBuf, sampleBuf, firstSampleFrame, numSampleFrames, streamFormat, audioStream);
#endif

    // We could do a sanity check that this is an output DMA engine we are dealing with
    if (!sourceBuf || !mixBuf || !sampleBuf || !streamFormat || !audioStream) {
        return kIOReturnBadArgument;
    }
        
    if ((firstSampleFrame + numSampleFrames) > numSampleFramesPerBuffer) {
        result = mixAndClip(sourceBuf, mixBuf, sampleBuf, firstSampleFrame, numSampleFramesPerBuffer - firstSampleFrame, streamFormat, audioStream);
        if (result == kIOReturnSuccess) {
            result = mixAndClip(&((float *)sourceBuf)[(numSampleFramesPerBuffer - firstSampleFrame) * streamFormat->fNumChannels], mixBuf, sampleBuf, 0, numSampleFrames - (numSampleFramesPerBuffer - firstSampleFrame), streamFormat, audioStream);
        }
    } else {
        float *floatSourceBuf, *floatMixBuf;
        UInt32 numSamplesLeft;
        
        floatSourceBuf = (float *)sourceBuf;
        floatMixBuf = &(((float *)mixBuf)[firstSampleFrame * streamFormat->fNumChannels]);
        
        numSamplesLeft = numSampleFrames * audioStream->format.fNumChannels;
        
        while (numSamplesLeft > 0) {
            *floatMixBuf += *floatSourceBuf;
            floatMixBuf++;
            floatSourceBuf++;
            numSamplesLeft--;
        }
        
        result = clipToOutputStream(mixBuf, sampleBuf, firstSampleFrame, numSampleFrames, streamFormat, audioStream);
    }
    
    return result;
}
