/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <IOKit/audio/IOAudioEngine.h>
#include <IOKit/audio/IOAudioStream.h>
#include <IOKit/audio/IOAudioTypes.h>
#include <IOKit/audio/IOAudioDebug.h>

#ifdef DEBUG_OUTPUT
#include <IOKit/IOLib.h>
#endif

IOReturn IOAudioEngine::mixOutputSamples(const void *sourceBuf, void *mixBuf, UInt32 firstSampleFrame, UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream)
{
    IOReturn result = kIOReturnBadArgument;
    
#ifdef DEBUG_OUTPUT_CALLS
    //IOLog("IOAudioEngine[%p]::mixOutputSamples(%p, %p, 0x%lx, 0x%lx, %p, %p)\n", sourceBuf, mixBuf, firstSampleFrame, numSampleFrames, streamFormat, audioStream);
    IOLog("mix(%p,%p,%lx,%lx) cur=(%lx,%lx) erase=%lx\n", sourceBuf, mixBuf, firstSampleFrame, numSampleFrames, status->fCurrentLoopCount, getCurrentSampleFrame(), status->fEraseHeadSampleFrame);
#endif

    if (sourceBuf && mixBuf) {
        float *floatSourceBuf, *floatMixBuf;
        UInt32 numSamplesLeft, numPartialSamples;
        
        floatSourceBuf = (float *)sourceBuf;
        floatMixBuf = &(((float *)mixBuf)[firstSampleFrame * streamFormat->fNumChannels]);
        
        numSamplesLeft = numSampleFrames * streamFormat->fNumChannels;
        numPartialSamples = numSamplesLeft % 4;	// Unroll the loop 4x
        
        while (numSamplesLeft > numPartialSamples) {
            register float mixFloat1 = *floatMixBuf;
            register float sourceFloat1 = *floatSourceBuf;
            register float mixFloat2 = *(floatMixBuf + 1);
            register float sourceFloat2 = *(floatSourceBuf + 1);
            register float mixFloat3 = *(floatMixBuf + 2);
            register float sourceFloat3 = *(floatSourceBuf + 2);
            register float mixFloat4 = *(floatMixBuf + 3);
            register float sourceFloat4 = *(floatSourceBuf + 3);
            
            floatSourceBuf += 4;
            numSamplesLeft -= 4;

            mixFloat1 += sourceFloat1;
            mixFloat2 += sourceFloat2;
            mixFloat3 += sourceFloat3;
            mixFloat4 += sourceFloat4;
            
            *floatMixBuf = mixFloat1;
            *(floatMixBuf + 1) = mixFloat2;
            *(floatMixBuf + 2) = mixFloat3;
            *(floatMixBuf + 3) = mixFloat4;
            
            floatMixBuf += 4;
        }
        
        while (numSamplesLeft > 0) {
            register float mixFloat1 = *floatMixBuf;
            register float sourceFloat1 = *floatSourceBuf;
            
            ++floatSourceBuf;
            --numSamplesLeft;

            mixFloat1 += sourceFloat1;
            
            *floatMixBuf = mixFloat1;
            
            ++floatMixBuf;
        }
        
        result = kIOReturnSuccess;
    }
    
    return result;
}
