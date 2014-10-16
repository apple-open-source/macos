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

#ifndef __OAL_CONTEXT__
#define __OAL_CONTEXT__

#include "oalDevice.h"
#include "alc.h"
#include "MacOSX_OALExtensions.h"
#include "oalCaptureMixer.h"
#include "CAGuard.h"

#include <Carbon/Carbon.h>
#include <map>
#include <libkern/OSAtomic.h>

#define CAPTURE_AUDIO_TO_FILE 0

#if	CAPTURE_AUDIO_TO_FILE
	#include "CAAudioUnitOutputCapturer.h"
#endif
	
#define LOG_BUS_CONNECTIONS  	0
#define LOG_CONTEXT_VERBOSE     0

#define kDefaultReferenceDistance   1.0
#define kDefaultMaximumDistance     1000000.0
#define kDefaultRolloff             1.0
#define kPreferredMixerVersion 		0x21000 
#define kMinimumMixerVersion 		0x10300

// Default Low Quality Stereo Spatial Setting:
#define	kDefaultLowQuality      kSpatializationAlgorithm_EqualPowerPanning

// Default High Quality Stereo Spatial Setting:
#define	kDefaultHighQuality     kSpatializationAlgorithm_HRTF

// Default MultiChannel Spatial Setting:
#define	kDefaultMultiChannelQuality kSpatializationAlgorithm_SoundField


/*
	An OALContext is basically the equivalent of a 'room' or 'scene'. It is attached to an OALDevice which
	is a piece of hardware and contains a single 3DMixer. Each context has it's own source objects
	and a single listener. It also has it's own settings for the 3DMixer, such as Reverb, Doppler, etc.
*/

enum { kNoSourceAttached = (ALuint)-1 };

struct BusInfo {
					ALuint		mSourceAttached;	// the token of the source attached
					UInt32		mNumberChannels;    // mono/stereo setting of the bus
                    UInt32      mReverbState;     // unused until Reverb extension is added to the implementation
};

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class OALSource;        // forward declaration
class OALSourceMap;     // forward declaration

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// OALContexts
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#pragma mark _____OALContext_____

class OALContext
{
	public:
	OALContext(const uintptr_t	inSelfToken, OALDevice *inOALDevice, const ALCint *inAttributeList, UInt32  &inBusCount, Float64	&inMixerRate);
	~OALContext();
	
	bool			DoSetDistance() { return mCalculateDistance;}
	UInt32			GetDesiredRenderChannels(UInt32	inDeviceChannels);
	AudioUnit		GetMixerUnit() { return mMixerUnit;}
	AUNode			GetMixerNode() { return mMixerNode;}
	AUGraph			GetGraph()	   { return mOwningDevice->GetGraph(); }
	UInt32			GetRenderQuality() {return mRenderQuality;}
	Float32         GetFramesPerSlice() { return mOwningDevice->GetFramesPerSlice();}
	Float64			GetMixerRate () const { return mMixerOutputRate; }
	Float32         GetDefaultReferenceDistance() { return mDefaultReferenceDistance;}
	Float32         GetDefaultMaxDistance() { return mDefaultMaxDistance;}
	bool            IsDistanceScalingRequired() { return mDistanceScalingRequired;}
	UInt32			GetAvailableMonoBus (ALuint inSourceToken);
	UInt32			GetAvailableStereoBus (ALuint inSourceToken);
	UInt32			GetBusCount () { return mBusCount;}
	uintptr_t		GetDeviceToken () { return mOwningDevice->GetDeviceToken();}
	UInt32			GetDistanceModel() { return mDistanceModel;}
	Float32			GetSpeedOfSound() {return (mSpeedOfSound);}
	Float32			GetDopplerFactor() {return (mDopplerFactor);}
	Float32			GetDopplerVelocity() {return (mDopplerVelocity);}
	Float32			GetListenerGain() {return (mListenerGain);}
	void			GetListenerPosition(Float32	*posX, Float32	*posY, Float32	*posZ) {*posX = mListenerPosition[0];
																								*posY = mListenerPosition[1];
																								*posZ = mListenerPosition[2];}
																								
	void			GetListenerVelocity(Float32	*posX, Float32	*posY, Float32	*posZ) {	*posX = mListenerVelocity[0];
																							*posY = mListenerVelocity[1];
																							*posZ = mListenerVelocity[2];}
																								
	void			GetListenerOrientation(	Float32	*forwardX, Float32	*forwardY, Float32	*forwardZ,
											Float32	*upX, Float32	*upY, Float32	*upZ) {		*forwardX = mListenerOrientationForward[0];
																								*forwardY = mListenerOrientationForward[1];
																								*forwardZ = mListenerOrientationForward[2];
																								*upX = mListenerOrientationUp[0];
																								*upY = mListenerOrientationUp[1];
																								*upZ = mListenerOrientationUp[2];		}
																							
	UInt32			GetAttributeListSize(){return mAttributeListSize;}
	void			CopyAttributeList( ALCint*	outAttrList);

	void			SetReverbQuality(UInt32 inQuality);
	void			SetReverbEQGain(Float32 inLevel);
	void			SetReverbEQBandwidth(Float32 inBandwidth);
	void			SetReverbEQFrequency(Float32 inFrequency);

	UInt32			GetReverbQuality();
	Float32			GetReverbEQGain();
	Float32			GetReverbEQBandwidth();
	Float32			GetReverbEQFrequency();


	// set info methods
	void			SetBusAsAvailable (UInt32 inBusIndex);
	void			SetDistanceAttenuation(UInt32    inBusIndex, Float64 inRefDist, Float64 inMaxDist, Float64 inRolloff);
	void			SetRenderQuality (UInt32 inRenderQuality);
    void            SetSourceDesiredRenderQualityOnBus (UInt32 inRenderQuality, int inBus);
    UInt32          GetRenderQualityForBus (int inBus);
	void			SetDistanceModel(UInt32	inDistanceModel);
	void			SetDopplerFactor(Float32	inDopplerFactor);
	void			SetDopplerVelocity(Float32	inDopplerVelocity);
	void			SetSpeedOfSound(Float32	inSpeedOfSound);
	void			SetListenerPosition(Float32	posX, Float32	posY, Float32	posZ);
	void			SetListenerVelocity(Float32	posX, Float32	posY, Float32	posZ);
	void			SetListenerGain(Float32 inGain);
	void			SetListenerOrientation( Float32  forwardX,   Float32  forwardY, Float32  forwardZ,
											Float32  upX,        Float32  upY, 		Float32  upZ);
	void			SetReverbPreset (FSRef* inRef);

	// ASA Support: Reverb, Occlusion
	void			SetReverbState(UInt32 inReverbState);
	UInt32			GetReverbState() {return mASAReverbState;}
	void			SetReverbRoomType(UInt32 inRoomType);
	void			SetReverbLevel(Float32 inReverbLevel);
	UInt32			GetReverbRoomType() {return mASAReverbRoomType;}
	Float32			GetReverbLevel() {return mASAReverbGlobalLevel;}
    
    // Context Output Capturer Methods
    OSStatus        OutputCapturerCreate(Float64 inSampleRate, UInt32 inOALFormat, UInt32 inBufferSize);
    OSStatus		OutputCapturerStart();
	OSStatus		OutputCapturerStop();
    OSStatus		OutputCapturerGetFrames(UInt32 inFrameCount, UInt8*	inBuffer);
	UInt32			OutputCapturerAvailableFrames();

	// notification proc methods
	OSStatus			DoPostRender ();
	OSStatus			DoPreRender ();
	static	OSStatus	ContextNotificationProc (	void                        *inRefCon, 
													AudioUnitRenderActionFlags 	*inActionFlags,
													const AudioTimeStamp 		*inTimeStamp, 
													UInt32 						inBusNumber,
													UInt32 						inNumberFrames, 
													AudioBufferList 			*ioData);
													

	// threading protection methods
	bool				CallingInRenderThread () const { return (pthread_self() == mRenderThreadID); }
	
	volatile int32_t	IsInUse()			{ return mInUseFlag; }
	void				SetInUseFlag()		{ OSAtomicIncrement32Barrier(&mInUseFlag); }
	void				ClearInUseFlag()	{ OSAtomicDecrement32Barrier(&mInUseFlag); }
	
	// context activity methods
	void			ProcessContext();
	void			SuspendContext();
	
	// source methods
	void			AddSource(ALuint inSourceToken);
	void			RemoveSource(ALuint inSourceToken);
	OALSource*		ProtectSource(ALuint inSourceToken);
	OALSource*		GetSourceForRender(ALuint inSourceToken);
	OALSource*		GetDeadSourceForRender(ALuint inSourceToken);
	void			ReleaseSource(OALSource* inSource);
	UInt32			GetSourceCount();
	void			CleanUpDeadSourceList();

	// device methods
	void			ConnectMixerToDevice();
	void			DisconnectMixerFromDevice();
	void			InitRenderQualityOnBusses();
    void            InitRenderQualityOnSources();
	void			ConfigureMixerFormat();
		
	private:
#if LOG_CONTEXT_VERBOSE
		uintptr_t			mSelfToken;
#endif
//		bool				mProcessingActive;
		OALDevice			*mOwningDevice;
        AUNode				mMixerNode;
        AudioUnit			mMixerUnit;
		OALSourceMap		*mSourceMap;
		CAGuard				mSourceMapLock;					// the map is not thread-safe. We need a mutex to serialize operations on it
		OALSourceMap		*mDeadSourceMap;
		CAGuard				mDeadSourceMapLock;					// the map is not thread-safe. We need a mutex to serialize operations on it
		UInt32				mDistanceModel;
		Float32				mSpeedOfSound;
		Float32				mDopplerFactor;
		Float32				mDopplerVelocity;
		Float32				mListenerPosition[3];
		Float32				mListenerVelocity[3];
		Float32				mListenerGain;
		Float32				mListenerOrientationForward[3];
		Float32				mListenerOrientationUp[3];
		UInt32				mAttributeListSize;
		ALCint*				mAttributeList;
        bool				mDistanceScalingRequired;
		bool				mCalculateDistance;				// true except: for 1.3 mixer Inverse curve, OR pre 2.2 mixer and either Exponential or Linear curves
		UInt32				mRenderQuality;                 // Hi or Lo for now
        UInt32				mSpatialSetting;
        UInt32				mBusCount;
		volatile int32_t	mInUseFlag;		
        Float64				mMixerOutputRate;
        Float32				mDefaultReferenceDistance;
        Float32				mDefaultMaxDistance;
		bool				mUserSpecifiedBusCounts;
        BusInfo				*mBusInfo;
   		pthread_t			mRenderThreadID;
		bool				mSettableMixerAttenuationCurves;
		UInt32				mASAReverbState;
		UInt32				mASAReverbRoomType;
		Float32				mASAReverbGlobalLevel;
		UInt32				mASAReverbQuality;
		Float32				mASAReverbEQGain;
		Float32				mASAReverbEQBandwidth;
		Float32				mASAReverbEQFrequency;
        OALCaptureMixer*    mOutputCapturer;

#if LOG_BUS_CONNECTIONS
        UInt32		mMonoSourcesConnected;
        UInt32		mStereoSourcesConnected;
#endif
#if CAPTURE_AUDIO_TO_FILE
		CAAudioUnitOutputCapturer		*gTheCapturer;
		CFURLRef						gFileURL;
		AudioStreamBasicDescription		gCapturerDataFormat;	
#endif

	void	InitializeMixer(UInt32	inStereoBusCount);

};

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// OALDevices contain a single OALContextMap to keep track of the contexts that belong to it.
#pragma mark _____OALContextMap_____
class OALContextMap : std::multimap<uintptr_t, OALContext*, std::less<uintptr_t> > {
public:
    
    void Add (const	uintptr_t	inContextToken, OALContext **inContext)  {
		iterator it = upper_bound(inContextToken);
		insert(it, value_type (inContextToken, *inContext));
	}

    OALContext* Get (uintptr_t	inContextToken) {
        iterator	it = find(inContextToken);
        if (it != end())
            return ((*it).second);
		return (NULL);
    }
    
    bool Remove (const	uintptr_t	inContextToken) {
        iterator 	it = find(inContextToken);
        if (it != end()) {
            erase(it);
			return true; // success
		}
		return false;
    }
	
	OALContext* GetContextByIndex(UInt32	inIndex, uintptr_t	&outContextToken) {
		iterator	it = begin();		
        std::advance(it, inIndex);		
		if (it != end())
		{
			outContextToken = (*it).first;
			return (*it).second;
		}
		return (NULL);
	}

	uintptr_t	GetDeviceTokenForContext(uintptr_t	inContextToken) {
		OALContext	*context = Get(inContextToken);
		if (context != NULL)
			return (context->GetDeviceToken());
		else
			return (0);
	}
    
    UInt32 Size() const { return size(); }
    bool Empty() const { return empty(); }
};

#endif