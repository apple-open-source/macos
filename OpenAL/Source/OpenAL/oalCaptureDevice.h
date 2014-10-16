/**********************************************************************************************************************************
*
*   OpenAL cross platform audio library
*   Copyright (c) 2005, Apple Computer, Inc. All rights reserved.
*
*   Redistribution and use in source and binary forms, with or without modification, are permitted provided 
*   that the following conditions are met:
*
*   1.  Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer. 
*   2.  Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following 
*       disclaimer in the documentation and/or other materials provided with the distribution. 
*   3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of its contributors may be used to endorse or promote 
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

#ifndef __OAL_CAPTURE_DEVICE__
#define __OAL_CAPTURE_DEVICE__

#include <CoreAudio/AudioHardware.h>
#include <AudioToolbox/AudioToolbox.h>
#include <AudioUnit/AudioUnit.h>
#include <map>
#include <libkern/OSAtomic.h>

#include "oalImp.h"
#include "oalRingBuffer.h"
#include "CAStreamBasicDescription.h"
#include "CABufferList.h"

#define LOG_CAPTUREDEVICE_VERBOSE         0

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// OALCaptureDevices
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#pragma mark _____OALCaptureDevice_____
class OALCaptureDevice
{
#pragma mark __________ Public_Class_Members
	public:

	OALCaptureDevice(const char* 	 inDeviceName, uintptr_t   inSelfToken, UInt32 inSampleRate, UInt32 inFormat, UInt32 inBufferSize);
	~OALCaptureDevice();
	
	void				StartCapture();
	void				StopCapture();

	OSStatus			GetFrames(UInt32 inFrameCount, UInt8*	inBuffer);
	UInt32				AvailableFrames();
	void				SetError(ALenum errorCode);
	ALenum				GetError();

	// we need to mark the capture device if it is being used to prevent deletion from another thread
	void				SetInUseFlag()		{ OSAtomicIncrement32Barrier(&mInUseFlag); }	
	void				ClearInUseFlag()	{ OSAtomicDecrement32Barrier(&mInUseFlag); }
	volatile int32_t	IsInUse()			{ return mInUseFlag; }

#pragma mark __________ Private_Class_Members

	private:
#if LOG_CAPTUREDEVICE_VERBOSE
		uintptr_t						mSelfToken;
#endif
		ALenum							mCurrentError;
		bool							mCaptureOn;
		SInt64							mStoreSampleTime;				// increment on each read in the input proc, and pass to the ring buffer class when writing, reset on each stop
		SInt64							mFetchSampleTime;				// increment on each read in the input proc, and pass to the ring buffer class when writing, reset on each stop
		AudioUnit						mInputUnit;
		CAStreamBasicDescription		mNativeFormat;
		CAStreamBasicDescription		mRequestedFormat;
		CAStreamBasicDescription		mOutputFormat;
		OALRingBuffer*					mRingBuffer;					// the ring buffer
		UInt8*							mBufferData;
		AudioConverterRef				mAudioConverter;
		Float64							mSampleRateRatio;
		UInt32							mRequestedRingFrames;			
		CABufferList*					mAudioInputPtrs;
		volatile int32_t				mInUseFlag;						// flag to indicate the device is currently being used by one or more threads

	void				InitializeAU (const char* 	inDeviceName);
	static OSStatus		InputProc(	void *						inRefCon,
									AudioUnitRenderActionFlags *ioActionFlags,
									const AudioTimeStamp *		inTimeStamp,
									UInt32 						inBusNumber,
									UInt32 						inNumberFrames,
									AudioBufferList *			ioData);
									
	static OSStatus		ACComplexInputDataProc	(AudioConverterRef				inAudioConverter,
												 UInt32							*ioNumberDataPackets,
												 AudioBufferList				*ioData,
												 AudioStreamPacketDescription	**outDataPacketDescription,
												 void*							inUserData);
										
};

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#pragma mark _____OALCaptureDeviceMap_____
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
class OALCaptureDeviceMap : std::multimap<uintptr_t, OALCaptureDevice*, std::less<uintptr_t> > {
public:
    
    void Add (const	uintptr_t	inDeviceToken, OALCaptureDevice **inDevice)  {
		iterator it = upper_bound(inDeviceToken);
		insert(it, value_type (inDeviceToken, *inDevice));
	}

    OALCaptureDevice* Get(uintptr_t	inDeviceToken) {
        iterator	it = find(inDeviceToken);
        if (it != end())
            return ((*it).second);
		return (NULL);
    }
    
    void Remove (const	uintptr_t	inDeviceToken) {
        iterator 	it = find(inDeviceToken);
        if (it != end())
            erase(it);
    }
	
    UInt32 Size () const { return size(); }
    bool Empty () const { return empty(); }
};


#endif
