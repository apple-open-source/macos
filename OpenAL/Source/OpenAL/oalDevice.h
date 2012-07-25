/**********************************************************************************************************************************
*
*   OpenAL cross platform audio library
*	Copyright (c) 2004, Apple Computer, Inc., Copyright (c) 2012, Apple Inc. All rights reserved.
*
*	Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following 
*	conditions are met:
*
*	1.  Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer. 
*	2.  Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following 
*		disclaimer in the documentation and/or other materials provided with the distribution. 
*	3.  Neither the name of Apple Inc. ("Apple") nor the names of its contributors may be used to endorse or promote products derived 
*		from this software without specific prior written permission. 
*
*	THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED 
*	TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS 
*	CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
*	LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED 
*	AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN 
*	ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
**********************************************************************************************************************************/

#ifndef __OAL_DEVICE__
#define __OAL_DEVICE__

#include <Carbon/Carbon.h>
#include <CoreAudio/AudioHardware.h>
#include <AudioToolbox/AudioToolbox.h>
#include <AudioUnit/AudioUnit.h>
#include <libkern/OSAtomic.h>
#include <map>

#include "oalImp.h"

#include  "CAStreamBasicDescription.h"


class OALContext;        // forward declaration

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Some build flags for gathering performance and data flow information

#if USE_AU_TRACER
	#include "AUTracer.h"
#endif

#if DEBUG
	#define AUHAL_LOG_OUTPUT 0
#endif

#if AUHAL_LOG_OUTPUT
	#include "AudioLogger.h"
#endif
     
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Device Constants
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// Default Mixer Output Sample Rate Setting:
#define	kDefaultMixerRate       44100.0

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// OALDevices
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#pragma mark _____OALDevice_____
class OALDevice
{
	public:

	OALDevice(const char* 	 inDeviceName, uintptr_t   inSelfToken, UInt32	inRenderChannelSetting);
	~OALDevice();
	
	void				ConnectContext		(OALContext*	inContext);
	void				DisconnectContext	(OALContext*	inContext);
	void				RemoveContext		(OALContext*	inContext);
	void				StopGraph();

	// thread safety
	volatile int32_t	IsInUse()			{ return mInUseFlag; }
	void				SetInUseFlag()		{ OSAtomicIncrement32Barrier(&mInUseFlag); }
	void				ClearInUseFlag()	{ OSAtomicDecrement32Barrier(&mInUseFlag); }

	// set info
	void				SetRenderChannelSetting (UInt32 inRenderChannelSetting);
	void				SetError(ALenum errorCode);

	// get info
	uintptr_t			GetDeviceToken () const { return mSelfToken; }
	Float64				GetDeviceSampleRate () const { return mDeviceSampleRate; }
	UInt32				GetRenderChannelSetting() { return  mRenderChannelSetting; }
	ALenum				GetError();
	UInt32				GetFramesPerSlice() { return mFramesPerSlice;}
	AUGraph				GetGraph () {return mAUGraph;}
	AudioUnit			GetOutputAU(){return mOutputUnit;}
	OSStatus			UpdateDeviceChannelLayout();
	UInt32				GetDesiredRenderChannelCount ();

	// misc.
	bool				IsValidRenderQuality (UInt32 inRenderQuality);
    static void			GraphFormatPropertyListener 	(	void 					*inRefCon, 
															AudioUnit 				ci, 
															AudioUnitPropertyID 	inID, 
															AudioUnitScope 			inScope, 
															AudioUnitElement 		inElement);

	bool	IsGraphStillRunning ()
	{
		Boolean running;
		OSStatus result = AUGraphIsRunning (mAUGraph, &running);
			THROW_RESULT
		return bool(running);	
	}

	void 	Print () const 
	{
#if	DEBUG || CoreAudio_Debug
		CAShow (mAUGraph); 
#endif
	}

	private:
		uintptr_t				mSelfToken;
		ALenum					mCurrentError;
        AudioDeviceID			mHALDevice;                     // the HAL device used to render audio to the user		
        bool					mDistanceScalingRequired;
		bool					mGraphInitialized;
		AUGraph					mAUGraph;
        AUNode					mOutputNode;
        AudioUnit				mOutputUnit;
        AUNode					mMixerNode;
		AudioChannelLayoutTag	mChannelLayoutTag;
		OALContext*				mConnectedContext;
		Float64					mDeviceSampleRate;
        UInt32					mRenderChannelCount;
        UInt32					mRenderChannelSetting;			// currently either stereo or multichannel
		UInt32					mFramesPerSlice;
		volatile int32_t		mInUseFlag;						// flag to indicate if the device is currently being edited by one or more threads
#if AUHAL_LOG_OUTPUT
        AudioLogger			mLogger;
#endif
#if USE_AU_TRACER
        AUTracer			mAUTracer;
#endif

	void					InitializeGraph (const char* 		inDeviceName);
	void					TeardownGraph();
	void					ResetRenderChannelSettings();
	AudioChannelLayoutTag	GetLayoutTagForLayout(AudioChannelLayout *inLayout, UInt32 inNumChannels);
	UInt32					GetChannelLayoutTag();
};

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#pragma mark _____OALDeviceMap_____
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
class OALDeviceMap : std::multimap<uintptr_t, OALDevice*, std::less<uintptr_t> > {
public:
    
    void Add (const	uintptr_t	inDeviceToken, OALDevice **inDevice)  {
		iterator it = upper_bound(inDeviceToken);
		insert(it, value_type (inDeviceToken, *inDevice));
	}

    OALDevice* Get(uintptr_t	inDeviceToken) {
        iterator	it = find(inDeviceToken);
        if (it != end())
            return ((*it).second);
		return (NULL);
    }

	OALDevice* GetDeviceByIndex(UInt32	inIndex, uintptr_t	&outDeviceToken) {
		iterator	it = begin();		
        std::advance(it, inIndex);		
		if (it != end())
		{
			outDeviceToken = (*it).first;
			return (*it).second;
		}
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
