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

#include <AudioToolbox/AudioToolbox.h>

#ifndef __OpenAL_Aspen__oalRingBuffer__
#define __OpenAL_Aspen__oalRingBuffer__

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#pragma mark _____OALRingBuffer_____
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// OALRingBuffer:
// This class implements an audio ring buffer. Multi-channel data can be either
// interleaved or deinterleaved.
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

enum {
	kOALRingBufferError_WayBehind = -2, // both fetch times are earlier than buffer start time
	kOALRingBufferError_SlightlyBehind = -1, // fetch start time is earlier than buffer start time (fetch end time OK)
	kOALRingBufferError_OK = 0,
	kOALRingBufferError_SlightlyAhead = 1, // fetch end time is later than buffer end time (fetch start time OK)
	kOALRingBufferError_WayAhead = 2, // both fetch times are later than buffer end time
	kOALRingBufferError_TooMuch = 3, // fetch start time is earlier than buffer start time and fetch end time is later than buffer end time
	kOALRingBufferError_CPUOverload = 4 // the reader is unable to get enough CPU cycles to capture a consistent snapshot of the time bounds
};

typedef SInt32 OALRingBufferError;
typedef SInt64 SampleTime;

const UInt32 kGeneralRingTimeBoundsQueueSize = 32;
const UInt32 kGeneralRingTimeBoundsQueueMask = kGeneralRingTimeBoundsQueueSize - 1;

class OALRingBuffer {
public:
	OALRingBuffer();
	OALRingBuffer(UInt32 bytesPerFrame, UInt32 capacityFrames);
	~OALRingBuffer();
	
	void		Allocate(UInt32 bytesPerFrame, UInt32 capacityFrames);
	void		Deallocate();
	void		Clear();
	bool		Store(const Byte *data, UInt32 nFrames, SInt64 frameNumber);
	OSStatus	Fetch(Byte *data, UInt32 nFrames, SInt64 frameNumber);
	Byte*		GetFramePtr(SInt64 frameNumber, UInt32 &outNFrames);
	
	void		GetTimeBounds(SInt64 &start, SInt64&end) { start = mStartFrame; end = mEndFrame; }
	
protected:
	UInt32		FrameOffset(SInt64 frameNumber) { return (mStartOffset + UInt32(frameNumber - mStartFrame) * mBytesPerFrame) % mCapacityBytes; }
    
protected:
	Byte *		mBuffer;
	UInt32		mBytesPerFrame;
	UInt32		mCapacityFrames;
	UInt32		mCapacityBytes;
	UInt32		mStartOffset;
	SInt64		mStartFrame;
	SInt64		mEndFrame;
};

#endif  /* defined(__OpenAL_Aspen__oalRingBuffer__) */