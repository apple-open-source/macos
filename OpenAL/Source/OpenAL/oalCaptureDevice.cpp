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

#include "OALCaptureDevice.h"

#define LOG_CAPTURE         0

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#pragma mark ***** OALCaptureDevices *****
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

OALCaptureDevice::OALCaptureDevice (const char* 	 inDeviceName, uintptr_t   inSelfToken, UInt32 inSampleRate, UInt32 inFormat, UInt32 inBufferSize)
	:
#if LOG_CAPTUREDEVICE_VERBOSE
        mSelfToken (inSelfToken),
#endif
		mCurrentError(ALC_NO_ERROR),
		mCaptureOn(false),
		mStoreSampleTime(0),
		mFetchSampleTime(0),
		mInputUnit(0),
		mRingBuffer(NULL),
		mBufferData(NULL),
		mSampleRateRatio(1.0),
		mRequestedRingFrames(inBufferSize),
		mAudioInputPtrs(NULL),
		mInUseFlag(0)
{
    char        *useThisDevice = (char *) inDeviceName;
	
	try {
		// translate the sample rate and data format parameters into an ASBD - this method throws
		FillInASBD(mRequestedFormat, inFormat, inSampleRate);

		// inBufferSize must be at least as big as one packet of the requested output formnat
		if (inBufferSize < mRequestedFormat.mBytesPerPacket)
           throw ((OSStatus) AL_INVALID_VALUE);
					
		// until the ALC_ENUMERATION_EXT extension is supported only use the default input device
        useThisDevice = NULL;
		
		InitializeAU(useThisDevice);

		if(mRequestedFormat.mSampleRate != mNativeFormat.mSampleRate)
		{
#if LOG_CAPTURE
	DebugMessageN2("OALCaptureDevice::OALCaptureDevice - Hardware Sample Rate: %5.1f Requested Sample Rate: %5.1f", mNativeFormat.mSampleRate, mRequestedFormat.mSampleRate);
#endif
			mSampleRateRatio = mNativeFormat.mSampleRate / mRequestedFormat.mSampleRate;
			mBufferData = (UInt8*)malloc(mRequestedFormat.mBytesPerFrame*mRequestedRingFrames*mSampleRateRatio);
			OSStatus result = AudioConverterNew(&mOutputFormat, &mRequestedFormat, &mAudioConverter);
				THROW_RESULT
		}
		
		mAudioInputPtrs = CABufferList::New("WriteBufferList", mRequestedFormat);
		
		mRingBuffer =  new OALRingBuffer();
		mRingBuffer->Allocate(mRequestedFormat.mBytesPerFrame, mRequestedRingFrames*mSampleRateRatio);
				
	}
	catch (OSStatus	result) {
		if (mRingBuffer) delete (mRingBuffer);
		if (mBufferData) delete mBufferData;
		if (mAudioInputPtrs) delete (mAudioInputPtrs);
		throw result;
	}
	catch (...) {
		if (mRingBuffer) delete (mRingBuffer);
		if (mBufferData) delete mBufferData;
		if (mAudioInputPtrs) delete (mAudioInputPtrs);
		throw -1;
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OALCaptureDevice::~OALCaptureDevice()
{
#if LOG_CAPTUREDEVICE_VERBOSE
	DebugMessageN1("OALCaptureDevice::~OALCaptureDevice() - OALCaptureDevice = %ld", (long int) mSelfToken);
#endif
	if (mInputUnit)
		CloseComponent(mInputUnit);
	if (mBufferData)
		delete mBufferData;
	delete mRingBuffer;
	delete mAudioInputPtrs;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	OALCaptureDevice::SetError(ALenum errorCode)
{
#if LOG_CAPTUREDEVICE_VERBOSE
	DebugMessageN2("OALCaptureDevice::SetError() - OALCaptureDevice:errorCode = %ld:%d", (long int) mSelfToken, errorCode);
#endif
	if (mCurrentError == ALC_NO_ERROR)
		return;
	
	mCurrentError = errorCode;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALenum	OALCaptureDevice::GetError()
{
#if LOG_CAPTUREDEVICE_VERBOSE
	DebugMessageN1("OALCaptureDevice::~OALCaptureDevice() - OALCaptureDevice = %ld", (long int) mSelfToken);
#endif
	ALenum	latestError = mCurrentError;
	mCurrentError = ALC_NO_ERROR;
	
	return latestError;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	OALCaptureDevice::InitializeAU(const char* 	inDeviceName)
{
#if LOG_CAPTUREDEVICE_VERBOSE
	DebugMessageN2("OALCaptureDevice::InitializeAU() - OALCaptureDevice = %ld:%s", (long int) mSelfToken, inDeviceName);
#endif
	// open input unit
	OSStatus				result = noErr;
	Component				comp;
	ComponentDescription	desc;
	
	try {
		desc.componentType = kAudioUnitType_Output;
		desc.componentSubType = kAudioUnitSubType_HALOutput;
		desc.componentManufacturer = kAudioUnitManufacturer_Apple;
		desc.componentFlags = 0;
		desc.componentFlagsMask = 0;
		comp = FindNextComponent(NULL, &desc);
		if (comp == NULL)
			throw -1;
			
		result = OpenAComponent(comp, &mInputUnit);
			THROW_RESULT
		
		UInt32 enableIO;
		UInt32 propSize;
		
		// turn off output
		enableIO = 0;
		result = AudioUnitSetProperty(mInputUnit, kAudioOutputUnitProperty_EnableIO,	kAudioUnitScope_Output,	0,	&enableIO,	sizeof(enableIO));
			THROW_RESULT

		// turn on input
		enableIO = 1;
		result = AudioUnitSetProperty(mInputUnit, kAudioOutputUnitProperty_EnableIO,	kAudioUnitScope_Input, 1, &enableIO, sizeof(enableIO));
			THROW_RESULT
		
		// get the default input device
		propSize = sizeof(AudioDeviceID);
		AudioDeviceID inputDevice;
		result = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultInputDevice, &propSize, &inputDevice);
			THROW_RESULT

		if (inputDevice == kAudioDeviceUnknown)
			throw -1; // there is no input device
			
		// track the default input device with our AUHal
		result = AudioUnitSetProperty(mInputUnit, kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global, 0, &inputDevice, sizeof(inputDevice));
			THROW_RESULT
		
		// set render callback
		AURenderCallbackStruct input;
		input.inputProc = InputProc;
		input.inputProcRefCon = this;
		
		result = AudioUnitSetProperty(mInputUnit, kAudioOutputUnitProperty_SetInputCallback, kAudioUnitScope_Global, 0, &input, sizeof(input));
			THROW_RESULT
		
		result = AudioUnitInitialize(mInputUnit);
			THROW_RESULT
				
		// get the hardware format
		propSize = sizeof(mNativeFormat);
		result = AudioUnitGetProperty(mInputUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 1, &mNativeFormat, &propSize);
			THROW_RESULT

		mOutputFormat = mRequestedFormat;
		mOutputFormat.mSampleRate = mNativeFormat.mSampleRate;
		
		// the output format should be the requested format, but using the native hardware sample rate, i.e. the output format
		result = AudioUnitSetProperty(mInputUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 1, (void *)&mOutputFormat, sizeof(mOutputFormat));
			THROW_RESULT

	}
	catch (OSStatus	result) {
		if (mInputUnit)	CloseComponent(mInputUnit);
		throw result;
	}
	catch (...) {
		if (mInputUnit)	CloseComponent(mInputUnit);
		throw - 1;
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus	OALCaptureDevice::InputProc(void *						inRefCon,
										AudioUnitRenderActionFlags *ioActionFlags,
										const AudioTimeStamp *		inTimeStamp,
										UInt32 						inBusNumber,
										UInt32 						inNumberFrames,
										AudioBufferList *			ioData)
{
#if LOG_CAPTUREDEVICE_VERBOSE
	DebugMessage("OALCaptureDevice::InputProc() - OALCaptureDevice");
#endif
	OALCaptureDevice *This = static_cast<OALCaptureDevice *>(inRefCon);
	AudioUnitRenderActionFlags flags = 0;
	
	AudioBufferList *abl = &This->mAudioInputPtrs->GetModifiableBufferList();
	for (UInt32 i = 0; i < abl->mNumberBuffers; ++i)
		abl->mBuffers[i].mData = NULL;
	
	OSStatus err = AudioUnitRender(This->mInputUnit, &flags, inTimeStamp, 1, inNumberFrames, abl);
	if (err)
		return err;
	
	if(This->mRingBuffer->Store((const Byte*)abl->mBuffers[0].mData, inNumberFrames, This->mStoreSampleTime))
		This->mStoreSampleTime += inNumberFrames;
		
	return noErr;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus OALCaptureDevice::ACComplexInputDataProc	(	AudioConverterRef				inAudioConverter,
														UInt32							*ioNumberDataPackets,
														AudioBufferList					*ioData,
														AudioStreamPacketDescription	**outDataPacketDescription,
														void*							inUserData)
{
	OALCaptureDevice* 		THIS = (OALCaptureDevice*)inUserData;
	UInt32 nFramesRemaining;
	
	ioData->mNumberBuffers = 1;
	ioData->mBuffers[0].mNumberChannels = THIS->mRequestedFormat.NumberChannels();
	ioData->mBuffers[0].mData = THIS->mRingBuffer->GetFramePtr(THIS->mFetchSampleTime, nFramesRemaining);
	
	*ioNumberDataPackets = ((*ioNumberDataPackets) > nFramesRemaining) ? nFramesRemaining : (*ioNumberDataPackets);
	
	ioData->mBuffers[0].mDataByteSize = (*ioNumberDataPackets) * THIS->mRequestedFormat.mBytesPerFrame;

	if (nFramesRemaining == 0)
	{
#if LOG_CAPTURE
	DebugMessage("OALCaptureDevice::ACComplexInputDataProc - buffer is empty"	);
#endif
		return -1;	
	}
	
	THIS->mFetchSampleTime += *ioNumberDataPackets;
	
	return noErr;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// PUBLIC METHODS
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	OALCaptureDevice::StartCapture()
{
#if LOG_CAPTUREDEVICE_VERBOSE
	DebugMessageN1("OALCaptureDevice::StartCapture() - OALCaptureDevice = %ld", (long int) mSelfToken);
#endif
	OSStatus	result = AudioOutputUnitStart(mInputUnit);
		THROW_RESULT
	mCaptureOn = true;
	mRingBuffer->Clear();
#if LOG_CAPTURE
	DebugMessage("OALCaptureDevice::StartCapture");
#endif
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	OALCaptureDevice::StopCapture()
{
#if LOG_CAPTUREDEVICE_VERBOSE
	DebugMessageN1("OALCaptureDevice::StopCapture() - OALCaptureDevice = %ld", (long int) mSelfToken);
#endif
	OSStatus	result = AudioOutputUnitStop(mInputUnit);
		THROW_RESULT
	mCaptureOn = false;
	mRingBuffer->Clear();
#if LOG_CAPTURE
	DebugMessage("OALCaptureDevice::StopCapture");
#endif
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus	OALCaptureDevice::GetFrames(UInt32 inFrameCount, UInt8*	inBuffer)
{
#if LOG_CAPTUREDEVICE_VERBOSE
	DebugMessageN1("OALCaptureDevice::GetFrames() - OALCaptureDevice = %ld", (long int) mSelfToken);
#endif
	OSStatus	result = noErr;
	
	if (!mCaptureOn)
		throw ((OSStatus) AL_INVALID_OPERATION);  // error condition, device not currently capturing


#if LOG_CAPTURE
	DebugMessageN1("OALCaptureDevice::GetFrames - requested frames: %ld", inFrameCount);
#endif

	if (inFrameCount > AvailableFrames())
	{
#if LOG_CAPTURE
		DebugMessage("OALCaptureDevice::GetFrames - Not enough frames available");
#endif	
		return -1; // error condition, there aren't enough valid frames to satisfy request
	}

	if (mSampleRateRatio != 1.0)
	{
		UInt32 theFramesToCopy = inFrameCount;
		AudioBufferList abl;
		abl.mNumberBuffers = 1;
		abl.mBuffers[0].mNumberChannels = mRequestedFormat.NumberChannels();
		abl.mBuffers[0].mDataByteSize = theFramesToCopy * mRequestedFormat.mBytesPerFrame;
		abl.mBuffers[0].mData = inBuffer;
		
		result = AudioConverterFillComplexBuffer(mAudioConverter, ACComplexInputDataProc, this, &theFramesToCopy, &abl, NULL);

		if (result)
		{
#if LOG_CAPTURE
			DebugMessageN1("OALCaptureDevice::GetFrames - AudioConverterFillComplexBuffer Failed result = %ld", result);
#endif		
			return result;
		}
		
		if (theFramesToCopy != inFrameCount)
		{
#if LOG_CAPTURE
			DebugMessageN1("OALCaptureDevice::GetFrames - AudioConverterFillComplexBuffer returned invalid number of frames = %ld", theFramesToCopy);
#endif
		return -1;
		}
	}
	
	else
	{
		result = mRingBuffer->Fetch((Byte*)inBuffer, inFrameCount, mFetchSampleTime);
		
		if (result)
		{
#if LOG_CAPTURE
			DebugMessageN1("OALCaptureDevice::GetFrames - mRingBuffer->Fetch Failed result = %ld", result);
#endif
			return result;
		}
		
		if (result == noErr)
			mFetchSampleTime += inFrameCount;
	}
	
#if LOG_CAPTURE
		DebugMessageN1("OALCaptureDevice::GetFrames - new mFetchSampleTime = %qd", mFetchSampleTime);
#endif

	return result;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt32	OALCaptureDevice::AvailableFrames()
{
#if LOG_CAPTUREDEVICE_VERBOSE
	DebugMessageN1("OALCaptureDevice::AvailableFrames() - OALCaptureDevice = %ld", (long int) mSelfToken);
#endif
	SInt64	start, end;
	mRingBuffer->GetTimeBounds(start, end);
		
	if (mFetchSampleTime < start)
		mFetchSampleTime = start;	// move up our fetch starting point, we have fallen too far behind
	
	UInt32	availableFrames = end - mFetchSampleTime;

	if (availableFrames > mRequestedRingFrames*mSampleRateRatio)
		availableFrames = mRequestedRingFrames*mSampleRateRatio;
		
#if LOG_CAPTURE
			DebugMessageN2("OALCaptureDevice::AvailableFrames - buffer: %ld actual: %ld", availableFrames, (UInt32)(availableFrames / mSampleRateRatio));
#endif
	return availableFrames / mSampleRateRatio;
}