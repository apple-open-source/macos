/**********************************************************************************************************************************
 *
 *   OpenAL cross platform audio library
 *	Copyright (c) 2004, Apple Computer, Inc., Copyright (c) 2012, Apple Inc. All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without modification, are permitted provided
 *   that the following conditions are met:
 *
 *   1.  Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 *   2.  Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided with the distribution.
 *   3.  Neither the name of Apple Inc. ("Apple") nor the names of its contributors may be used to endorse or promote
 *       products derived from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 *   THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS
 *   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 *   TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 *   USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 **********************************************************************************************************************************/

#ifndef __OpenAL__oalCaptureMixer__
#define __OpenAL__oalCaptureMixer__

//local includes
#include "oalRingBuffer.h"

// System includes
#include <AudioToolbox/AudioToolbox.h>

// Public Utility includes
#include  "CAStreamBasicDescription.h"
#include  "CABufferList.h"

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// OALCaptureMixer
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#pragma mark _____OALCaptureMixer_____
class OALCaptureMixer
{
public:
    OALCaptureMixer(AudioUnit inMixer, Float64 inSampleRate, UInt32 inOALFormat, UInt32 inBufferSize);
    ~OALCaptureMixer();
    
    void						StartCapture();
	void						StopCapture();
    OSStatus					GetFrames(UInt32 inFrameCount, UInt8*	inBuffer);
	UInt32						AvailableFrames();
    CAStreamBasicDescription	GetOutputFormat() { return mRequestedFormat; }
    
    static OSStatus RenderCallback( void *							inRefCon,
                                   AudioUnitRenderActionFlags *     ioActionFlags,
                                   const AudioTimeStamp *			inTimeStamp,
                                   UInt32							inBusNumber,
                                   UInt32							inNumberFrames,
                                   AudioBufferList *				ioData);
    
    static OSStatus ConverterProc(  AudioConverterRef               inAudioConverter,
                                  UInt32*                           ioNumberDataPackets,
                                  AudioBufferList*                  ioData,
                                  AudioStreamPacketDescription**    outDataPacketDescription,
                                  void*                             inUserData);
    
    void                        SetCaptureFlag();
    void                        ClearCaptureFlag();
    bool                        IsCapturing() { return static_cast<bool>(mCaptureOn); }
    
private:
    AudioUnit                   mMixerUnit;
    CAStreamBasicDescription    mRequestedFormat;
    OALRingBuffer*              mRingBuffer;
    UInt32                      mRequestedRingFrames;
    SInt64                      mStoreSampleTime;
    SInt64                      mFetchSampleTime;
    volatile int32_t            mCaptureOn;
    
    AudioConverterRef           mAudioConverter;
    CABufferList*               mConvertedDataABL;
};

#endif /* defined(__OpenAL__oalCaptureMixer__) */
