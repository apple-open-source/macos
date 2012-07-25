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

#include "oalContext.h"
#include "oalSource.h"

#define LOG_VERBOSE             0

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// OALContexts
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#pragma mark ***** OALContexts - Public Methods *****
OALContext::OALContext (const uintptr_t	inSelfToken, OALDevice    *inOALDevice, const ALCint *inAttributeList, UInt32  &inBusCount, Float64	&inMixerRate)
	: 	mSelfToken (inSelfToken),
		mProcessingActive(true),
		mOwningDevice(inOALDevice),
		mMixerNode(0), 
		mMixerUnit (0),
		mSourceMap (NULL),
		mSourceMapLock ("OALContext::SourceMapLock"),
		mDeadSourceMap (NULL),
		mDeadSourceMapLock ("OALContext::DeadSourceMapLock"),
		mDistanceModel(AL_INVERSE_DISTANCE_CLAMPED),			
		mSpeedOfSound(343.3),
		mDopplerFactor(1.0),
		mDopplerVelocity(1.0),
		mListenerGain(1.0),
		mAttributeListSize(0),
		mAttributeList(NULL),
		mDistanceScalingRequired(false),
		mCalculateDistance(true),
		mStoredInverseAttenuation(1.0),
		mRenderQuality(ALC_MAC_OSX_SPATIAL_RENDERING_QUALITY_LOW),
		mSpatialSetting(0),
		mBusCount(inBusCount),
		mInUseFlag(0),
		mMixerOutputRate(inMixerRate),
		mDefaultReferenceDistance(1.0),
        mDefaultMaxDistance(100000.0),
		mUserSpecifiedBusCounts(false),
		mRenderThreadID(0),
		mSettableMixerAttenuationCurves(false),
		mASAReverbState(0),		
		mASAReverbRoomType(0),
		mASAReverbGlobalLevel(0.0),
		mASAReverbQuality(ALC_ASA_REVERB_QUALITY_Low),
		mASAReverbEQGain(0.0),
		mASAReverbEQBandwidth(3.0),
		mASAReverbEQFrequency(800.0)
#if LOG_BUS_CONNECTIONS
		, mMonoSourcesConnected(0),
		mStereoSourcesConnected(0)
#endif
{
#if LOG_VERBOSE
	DebugMessageN1("OALContext::OALContext() - OALContext = %ld", (long int) mSelfToken);
#endif
		mBusInfo = (BusInfo *) calloc (1, sizeof(BusInfo) * mBusCount);

		UInt32		monoSources = 0,
					stereoSources = 1; // default

		UInt32		inAttributeListSize = 0;
		Boolean		userSetMixerOutputRate = false;
		
		if (inAttributeList)
		{
			ALCint*		currentAttribute = ( ALCint*) inAttributeList;
			// ATTRIBUTE LIST
			while (*currentAttribute != 0)
			{
				switch (*currentAttribute)
				{
					case ALC_FREQUENCY:
						mMixerOutputRate = (Float64)currentAttribute[1];
						userSetMixerOutputRate = true;
						break;
					case ALC_REFRESH:
						break;
					case ALC_SYNC:
						break;
					case ALC_MONO_SOURCES:
						mUserSpecifiedBusCounts = true;
						monoSources = currentAttribute[1];
						break;
					case ALC_STEREO_SOURCES:
						mUserSpecifiedBusCounts = true;
						stereoSources = currentAttribute[1];
						break;
					default:
						// is this a failure?
						break;
				}
				currentAttribute += 2;
				inAttributeListSize += 2;
			}

			// if no mixer output rate was set, pound in the default rate
			if (!userSetMixerOutputRate)
			{
				// add 2 for the key and value, and 1 for the terminator
				mAttributeListSize = inAttributeListSize + 3;
				mAttributeList =  (ALCint*) calloc (1, mAttributeListSize * sizeof(ALCint));
				mAttributeList[inAttributeListSize] = (ALCint)ALC_FREQUENCY;
				mAttributeList[inAttributeListSize+1] = (ALCint)mMixerOutputRate;
				// add the null terminator
				mAttributeList[inAttributeListSize+2] = 0;
			}
			
			else {
				// add for the terminator
				mAttributeListSize = inAttributeListSize + 1;
				mAttributeList =  (ALCint*) calloc (1, mAttributeListSize * sizeof(ALCint));
			}
						
			memcpy(mAttributeList, inAttributeList, inAttributeListSize * sizeof(ALCint));
		}

		// initialize  mContextInfo
		mListenerPosition[0] = 0.0;
		mListenerPosition[1] = 0.0;
		mListenerPosition[2] = 0.0;
		
		mListenerVelocity[0] = 0.0;
		mListenerVelocity[1] = 0.0;
		mListenerVelocity[2] = 0.0;
		
		mListenerOrientationForward[0] = 0.0;
		mListenerOrientationForward[1] = 0.0;
		mListenerOrientationForward[2] = -1.0;
		
		mListenerOrientationUp[0] = 0.0;
		mListenerOrientationUp[1] = 1.0;
		mListenerOrientationUp[2] = 0.0;

		InitializeMixer(stereoSources);

		mSourceMap = new OALSourceMap();
		mDeadSourceMap = new OALSourceMap();
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OALContext::~OALContext()
{
#if LOG_VERBOSE
	DebugMessageN1("OALContext::~OALContext() - OALContext = %ld", (long int) mSelfToken);
#endif
    
#if CAPTURE_AUDIO_TO_FILE
	gTheCapturer->Stop();
	delete(gTheCapturer);
	gTheCapturer = NULL;
#endif

	mOwningDevice->RemoveContext(this);

	// delete all the sources that were created by this context
	if (mSourceMap)
	{
		for (UInt32  i = 0; i < mSourceMap->Size(); i++)
		{
			OALSource	*oalSource = mSourceMap->GetSourceByIndex(0);
			if (oalSource)
			{
				mSourceMap->Remove(oalSource->GetToken());
				delete oalSource;
			}
		}
		delete mSourceMap;
	}
	
	if (mDeadSourceMap)
	{
		CleanUpDeadSourceList();
		delete mDeadSourceMap;
	}
	
	if (mAttributeList)
		free(mAttributeList);
	
	if(mBusInfo)
		free(mBusInfo);
		
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	OALContext::CleanUpDeadSourceList()
{
#if LOG_VERBOSE
	DebugMessageN1("OALContext::CleanUpDeadSourceList() - OALContext = %ld", (long int) mSelfToken);
#endif
	if (mDeadSourceMap)
	{
		UInt32	index = 0;
		for (UInt32 i = 0; i < mDeadSourceMap->Size(); i++)
		{
			OALSource*		source = mDeadSourceMap->GetSourceByIndex(index);
			if (source)
			{
				if (source->IsSafeForDeletion())
				{
					//DebugMessageN1("OALContext::CleanUpTheDeadSourceList removing source id = %ld", source->GetToken());
					mDeadSourceMap->Remove(source->GetToken());
					delete (source);
				}
				else
				{
					//DebugMessageN1("OALContext::CleanUpTheDeadSourceList NOT SAFE RIGHT NOW to delete source id = %ld", source->GetToken());
					index++;
				}
			}
			else
				index++;
		}
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/*
	3DMixer Version Info
	
	- Pre 2.0 Mixer must be at least version 1.3 for OpenAL
	- 3DMixer 2.0
		Bug in Distance Attenuationg/Reverb code requires that distances be scaled in OAL before passing to mixer
	- 3DMixer 2.1
		Fixes bug in 2.0 but also has a bug related to OAL fixes for correctly caclulating the vector of moving object
	- 3DMixer 2.1.x
		Fixes bugs in 2.1 and adds support for Linear Attenuation
	- 3DMixer 2.2
		Adds Linear/Exponential Attenuation and Reverb/Occlusion/Obstruction.
		(note) Linear & Exponential Attenuation is done manually by the OALSource object if 2.2 Mixer is not present
*/

void		OALContext::InitializeMixer(UInt32	inStereoBusCount)
{
#if LOG_VERBOSE
	DebugMessageN2("OALContext::InitializeMixer() - OALContext:inStereoBusCount = %ld:%d", (long int) mSelfToken, inStereoBusCount);
#endif
    OSStatus	result = noErr;
	UInt32		propSize;

	try {
		// ~~~~~~~~~~~~~~~~~~~ GET 3DMIXER VERSION
		if (Get3DMixerVersion() < k3DMixerVersion_1_3)
			throw -1;                           // should not happen because OpenDevice should have failed beforehand
		
		if (Get3DMixerVersion() == k3DMixerVersion_2_0)
		{
			mDistanceScalingRequired = true; 
		}
		else if (Get3DMixerVersion() >= k3DMixerVersion_2_2)
		{
			mSettableMixerAttenuationCurves = true;
		}

		ComponentDescription	mixerCD;
		mixerCD.componentFlags = 0;        
		mixerCD.componentFlagsMask = 0;     
		mixerCD.componentType = kAudioUnitType_Mixer;          
		mixerCD.componentSubType = kAudioUnitSubType_3DMixer;       
		mixerCD.componentManufacturer = kAudioUnitManufacturer_Apple;  
		
		// CREATE NEW NODE FOR THE GRAPH
		result = AUGraphNewNode (mOwningDevice->GetGraph(), &mixerCD, 0, NULL, &mMixerNode);
			THROW_RESULT

		result = AUGraphGetNodeInfo (mOwningDevice->GetGraph(), mMixerNode, 0, 0, 0, &mMixerUnit);
			THROW_RESULT   

		// Get Default Distance Setting when the good 3DMixer is around
		if (Get3DMixerVersion() >= k3DMixerVersion_2_0)
		{
			MixerDistanceParams		distanceParams;
			propSize = sizeof(distanceParams);
			result = AudioUnitGetProperty(mMixerUnit, kAudioUnitProperty_3DMixerDistanceParams, kAudioUnitScope_Input, 1, &distanceParams, &propSize);
			if (result == noErr)
			{
				mDefaultReferenceDistance = distanceParams.mReferenceDistance;
				mDefaultMaxDistance = distanceParams.mMaxDistance;
			}
		}
		
		// Set the Output Format of the Mixer AU
		CAStreamBasicDescription	format;
		UInt32                      propSize = sizeof(format);
		result = AudioUnitGetProperty(mOwningDevice->GetOutputAU(), kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, &format, &propSize);

		format.SetCanonical (mOwningDevice->GetDesiredRenderChannelCount(), false);	// determine how many channels to render to
		format.mSampleRate = GetMixerRate();										// Sample Rate (either the default out rate of the Output AU or a User Specified rate)

		result = AudioUnitSetProperty (mMixerUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, &format, sizeof(format));
			THROW_RESULT
		
		// Frames Per Slice - moved from ConnectContext call in device class
		UInt32		mixerFPS = 0;
		UInt32		dataSize = sizeof(mixerFPS);
		result = AudioUnitGetProperty(mMixerUnit, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global, 0, &mixerFPS, &dataSize);
		if (mixerFPS < mOwningDevice->GetFramesPerSlice())
		{
			mixerFPS = mOwningDevice->GetFramesPerSlice();
			result = AudioUnitSetProperty(  mMixerUnit, kAudioUnitProperty_MaximumFramesPerSlice, kAudioUnitScope_Global, 0, &mixerFPS, sizeof(mixerFPS));
				THROW_RESULT
		}

		// REVERB off by default
		result = AudioUnitSetProperty(mMixerUnit, kAudioUnitProperty_UsesInternalReverb, kAudioUnitScope_Global, 0, &mASAReverbState, sizeof(mASAReverbState));
		// ignore result

		// MIXER BUS COUNT
		if (Get3DMixerVersion() < k3DMixerVersion_2_0)
		{
			mBusCount = kDefaultMaximumMixerBusCount; // 1.3 version of the mixer did not allow a change in the bus count
		}
		else
		{
			// set the bus count on the mixer if necessary	
			UInt32  currentBusCount;
			propSize = sizeof(currentBusCount);
			result = AudioUnitGetProperty (	mMixerUnit, kAudioUnitProperty_ElementCount, kAudioUnitScope_Input, 0, &currentBusCount, &propSize);
			if ((result == noErr) && (mBusCount != currentBusCount))
			{
				result = AudioUnitSetProperty (	mMixerUnit, kAudioUnitProperty_ElementCount, kAudioUnitScope_Input, 0, &mBusCount, propSize);
				if (result != noErr)
				{
					// couldn't set the bus count so make sure we know just how many busses there are
					propSize = sizeof(mBusCount);
					AudioUnitGetProperty (	mMixerUnit, kAudioUnitProperty_ElementCount, kAudioUnitScope_Input, 0, &mBusCount, &propSize);
				}
			}
		}

		// SET UP STEREO/MONO BUSSES
		CAStreamBasicDescription 	theOutFormat;
		theOutFormat.mSampleRate = mMixerOutputRate;
		theOutFormat.mFormatID = kAudioFormatLinearPCM;
		theOutFormat.mFramesPerPacket = 1;	
		theOutFormat.mBytesPerFrame = sizeof (Float32);
		theOutFormat.mBitsPerChannel = sizeof (Float32) * 8;	
		theOutFormat.mFormatFlags = kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved;
		theOutFormat.mBytesPerPacket = sizeof (Float32);	

		for (UInt32	i = 0; i < mBusCount; i++)
		{
			// Distance Attenuation: for pre v2.0 mixer
			SetDistanceAttenuation (i, kDefaultReferenceDistance, kDefaultMaximumDistance, kDefaultRolloff);			

			theOutFormat.mChannelsPerFrame = (i < inStereoBusCount) ? 2 : 1;
			OSStatus	result = AudioUnitSetProperty (	mMixerUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 
															i, &theOutFormat, sizeof(CAStreamBasicDescription));
				THROW_RESULT

			mBusInfo[i].mNumberChannels = theOutFormat.mChannelsPerFrame; 
			mBusInfo[i].mSourceAttached = kNoSourceAttached;
			mBusInfo[i].mReverbState = mASAReverbState;

			// set kAudioUnitProperty_SpatializationAlgorithm
			UInt32		spatAlgo = (theOutFormat.mChannelsPerFrame == 2) ? kSpatializationAlgorithm_StereoPassThrough : mSpatialSetting;
			AudioUnitSetProperty(	mMixerUnit, kAudioUnitProperty_SpatializationAlgorithm, kAudioUnitScope_Input, i, &spatAlgo, sizeof(spatAlgo));

			// set kAudioUnitProperty_3DMixerRenderingFlags (distance attenuation) for mono busses
			if (theOutFormat.mChannelsPerFrame == 1)
			{
				UInt32 		render_flags_3d = k3DMixerRenderingFlags_DistanceAttenuation;
				if (mRenderQuality == ALC_MAC_OSX_SPATIAL_RENDERING_QUALITY_HIGH)
					render_flags_3d += k3DMixerRenderingFlags_InterAuralDelay; // off by default, on if the user sets High Quality rendering

				// Render Flags
				result = AudioUnitSetProperty(	mMixerUnit, kAudioUnitProperty_3DMixerRenderingFlags, kAudioUnitScope_Input, i, &render_flags_3d, sizeof(render_flags_3d));
			}
		}

		// Initialize Busses - attributes may affect this operation
		InitRenderQualityOnBusses(); 	
	}
	catch(OSStatus	result){
		throw result;
	}
	catch(...){
		throw -1;
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// should only be called when the mixer is NOT connected
void	OALContext::ConfigureMixerFormat()
{
#if LOG_VERBOSE
	DebugMessageN1("OALContext::ConfigureMixerFormat() - OALContext = %ld", (long int) mSelfToken);
#endif
	// Set the Output Format of the Mixer AU
	CAStreamBasicDescription	format;
	UInt32                      propSize = sizeof(format);
	OSStatus	result = AudioUnitGetProperty(mMixerUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, &format, &propSize);

	format.SetCanonical (mOwningDevice->GetDesiredRenderChannelCount(), false);	// determine how many channels to render to
	format.mSampleRate = GetMixerRate();										// Sample Rate (either the default out rate of the Output AU or a User Specified rate)

	result = AudioUnitSetProperty (mMixerUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, &format, sizeof(format));
		THROW_RESULT

	// Initialize Busses - render channel attributes attributes may affect this operation
	InitRenderQualityOnBusses(); 	
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	OALContext::CopyAttributeList( ALCint*	outAttrList)
{
#if LOG_VERBOSE
	DebugMessageN1("OALContext::CopyAttributeList() - OALContext = %ld", (long int) mSelfToken);
#endif    
	memcpy(outAttrList, mAttributeList, mAttributeListSize*sizeof(ALCint));
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void		OALContext::AddSource(ALuint	inSourceToken)
{
#if LOG_VERBOSE
	DebugMessageN2("OALContext::AddSource() - OALContext:inSourceToken = %ld:%d", (long int) mSelfToken, inSourceToken);
#endif
	try {
			OALSource	*newSource = new OALSource (inSourceToken, this);
			
			{
				CAGuard::Locker locked(mSourceMapLock);
				mSourceMap->Add(inSourceToken, &newSource);
			}
			{
				CAGuard::Locker locked(mDeadSourceMapLock);
				CleanUpDeadSourceList();
			}
	}
	catch (...) {
		throw;
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// You MUST call ReleaseSource after you have completed use of the source object
OALSource*		OALContext::ProtectSource(ALuint	inSourceToken)
{
#if LOG_VERBOSE
	DebugMessageN2("OALContext::ProtectSource() - OALContext:inSourceToken = %ld:%d", (long int) mSelfToken, inSourceToken);
#endif
	OALSource *newSource = NULL;
	
	CAGuard::Locker locked(mSourceMapLock);

	newSource = mSourceMap->Get(inSourceToken);
	
	if (newSource)
		newSource->SetInUseFlag();
		
	return newSource;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// You MUST call ReleaseSource after you have completed use of the source object
OALSource*		OALContext::GetSourceForRender(ALuint	inSourceToken)
{
#if LOG_VERBOSE
	DebugMessageN2("OALContext::GetSourceForRender() - OALContext:inSourceToken = %ld:%d", (long int) mSelfToken, inSourceToken);
#endif  
	OALSource *newSource = NULL;
	
	CAMutex::Tryer tryer(mSourceMapLock);
	
	if (tryer.HasLock())
	{		
		newSource = mSourceMap->Get(inSourceToken);
		
		if (newSource)
			newSource->SetInUseFlag();
	}
	
	return newSource;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// You MUST call ReleaseSource after you have completed use of the dead source object
OALSource*		OALContext::GetDeadSourceForRender(ALuint	inSourceToken)
{
#if LOG_VERBOSE
	DebugMessageN2("OALContext::GetDeadSourceForRender() - OALContext:inSourceToken = %ld:%d", (long int) mSelfToken, inSourceToken);
#endif
	OALSource *deadSource = NULL;
	
	CAMutex::Tryer tryer(mDeadSourceMapLock);
	
	if (tryer.HasLock())
	{
		deadSource = mDeadSourceMap->Get(inSourceToken);
	
		if (deadSource)
			deadSource->SetInUseFlag();
	}
		
	return deadSource;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void OALContext::ReleaseSource(OALSource* inSource)
{
#if LOG_VERBOSE
	DebugMessageN2("OALContext::ReleaseSource() - OALContext:inSource = %ld:%d", (long int) mSelfToken, inSource->GetToken());
#endif
	if (inSource) 
		inSource->ClearInUseFlag();
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void		OALContext::RemoveSource(ALuint	inSourceToken)
{
#if LOG_VERBOSE
	DebugMessageN2("OALContext::RemoveSource() - OALContext:inSourceToken = %ld:%d", (long int) mSelfToken, inSourceToken);
#endif
	OALSource	*oalSource = mSourceMap->Get(inSourceToken);
	if (oalSource != NULL)
	{
		oalSource->SetUpDeconstruction();
		
		{
			CAGuard::Locker locked(mSourceMapLock);
			mSourceMap->Remove(inSourceToken);					// do not allow any more threads to use this source object
		}
		{
			CAGuard::Locker locked(mDeadSourceMapLock);
			mDeadSourceMap->Add(inSourceToken, &oalSource);		// remove it later when it is safe
			CleanUpDeadSourceList();							// now is a good time to actually delete other sources marked for deletion
		}
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void		OALContext::ProcessContext()
{
#if LOG_VERBOSE
	DebugMessageN1("OALContext::ProcessContext() - OALContext = %ld", (long int) mSelfToken);
#endif
	return; // NO OP
	
#if 0
	// This code breaks Doom 3 [4554491] - The 1.0 implementation was a no op.
	// Since alcProcessContext()/alcSuspendContext() are also no ops on sound c ards with OpenAL imps, this should be ok
	
	if (mProcessingActive == true)
		return; // NOP
	
	ConnectMixerToDevice();
	mProcessingActive = true;
	return;
#endif
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void		OALContext::SuspendContext()
{
#if LOG_VERBOSE
	DebugMessageN1("OALContext::SuspendContext() - OALContext = %ld", (long int) mSelfToken);
#endif
	return; // NO OP

#if 0
	// This code breaks Doom 3 [4554491] - The 1.0 implementation was a no op.
	// Since alcProcessContext()/alcSuspendContext() are also no ops on sound c ards with OpenAL imps, this should be ok
 
	if (mProcessingActive == false)
		return; // NOP

	DeviceDisconnect();
	mProcessingActive = false;
#endif
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void		OALContext::ConnectMixerToDevice()
{
#if LOG_VERBOSE
	DebugMessageN1("OALContext::ConnectMixerToDevice() - OALContext = %ld", (long int) mSelfToken);
#endif
	mOwningDevice->ConnectContext(this);

#if CAPTURE_AUDIO_TO_FILE
    DebugMessage ("ABOUT TO START CAPTURE");

	gCapturerDataFormat.mSampleRate = 44100.0;
	gCapturerDataFormat.mFormatID = kAudioFormatLinearPCM;
	gCapturerDataFormat.mFormatFlags = kAudioFormatFlagIsPacked | kLinearPCMFormatFlagIsSignedInteger;
	gCapturerDataFormat.mBytesPerPacket = 4;
	gCapturerDataFormat.mFramesPerPacket = 1;
	gCapturerDataFormat.mBytesPerFrame = 4;
	gCapturerDataFormat.mChannelsPerFrame = 2;
	gCapturerDataFormat.mBitsPerChannel = 16;
	gCapturerDataFormat.mReserved = 0;

	gFileURL = CFURLCreateWithFileSystemPath(NULL, CFSTR("<PathToCapturedFileLocation>CapturedAudio.wav"), kCFURLPOSIXPathStyle, false);
	gTheCapturer = new CAAudioUnitOutputCapturer(mMixerUnit, gFileURL, 'WAVE', gCapturerDataFormat);

	gTheCapturer->Start();
#endif

	OSStatus result = AUGraphAddRenderNotify(mOwningDevice->GetGraph(), ContextNotificationProc, this);
		THROW_RESULT
}

void		OALContext::DisconnectMixerFromDevice()
{			
	mOwningDevice->DisconnectContext(this);

#if CAPTURE_AUDIO_TO_FILE
    DebugMessage ("STOPPING CAPTURE");

	gTheCapturer->Stop();
#endif

	OSStatus result = AUGraphRemoveRenderNotify(mOwningDevice->GetGraph(), ContextNotificationProc, this);
		THROW_RESULT
}			


// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void		OALContext::SetDistanceModel(UInt32	inDistanceModel)
{
#if LOG_VERBOSE || LOG_GRAPH_AND_MIXER_CHANGES
	DebugMessageN2("OALContext::SetDistanceModel() - OALContext:inDistanceModel = %ld:%d", (long int) mSelfToken, inDistanceModel);
#endif
	OSStatus	result = noErr;
	UInt32		curve;
	
	if (mDistanceModel != inDistanceModel)
	{
		//UInt32	curve;
		switch (inDistanceModel)
		{
			case AL_INVERSE_DISTANCE:
			case AL_INVERSE_DISTANCE_CLAMPED:
				mCalculateDistance = true;
				if (mSettableMixerAttenuationCurves)
				{
					curve =2 /* k3DMixerAttenuationCurve_Inverse*/;
					for (UInt32	i = 0; i < mBusCount; i++)
					{
						result = AudioUnitSetProperty(	mMixerUnit, 3013 /*kAudioUnitProperty_3DMixerAttenuationCurve*/, kAudioUnitScope_Input, i, &curve, sizeof(curve));
					}
				}
				else
				{
					// NOTHING TO DO
					if (Get3DMixerVersion() >= k3DMixerVersion_2_0)
					{
						// unnecessary if changing between AL_INVERSE_DISTANCE & AL_INVERSE_DISTANCE_CLAMPED
						if ((mDistanceModel != AL_INVERSE_DISTANCE) && (mDistanceModel != AL_INVERSE_DISTANCE_CLAMPED))
						{
							// this is the 2.0-2.1 mixer
						}
					}
					else
					{
						// kAudioUnitProperty_3DMixerDistanceAtten gets set by the source each time via a call to SetDistanceAttenuation()
						// nothing more to do now
					}
				}
				
				break;
				
			case AL_LINEAR_DISTANCE:
			case AL_LINEAR_DISTANCE_CLAMPED:
				if (mSettableMixerAttenuationCurves)
				{	
					mCalculateDistance = true;
					curve = 3 /*k3DMixerAttenuationCurve_Linear*/;
					for (UInt32	i = 0; i < mBusCount; i++)
					{
						result = AudioUnitSetProperty(	mMixerUnit, 3013 /*kAudioUnitProperty_3DMixerAttenuationCurve*/, kAudioUnitScope_Input, i, &curve, sizeof(curve));
					}
				}
				else
				{
					mCalculateDistance = false;
					// turn off distance attenuation altogether
					// the source will then apply the linear distance formula as a gain scalar and set the bus gain instead of setting any distance
				}
				break;
			
			case AL_EXPONENT_DISTANCE:
			case AL_EXPONENT_DISTANCE_CLAMPED:
				if (mSettableMixerAttenuationCurves)
				{
					mCalculateDistance = true;
					// set the mixer for Exponential Attenuation
					curve = 1 /*k3DMixerAttenuationCurve_Exponential*/;
					for (UInt32	i = 0; i < mBusCount; i++)
					{
						result = AudioUnitSetProperty(	mMixerUnit, 3013 /*kAudioUnitProperty_3DMixerAttenuationCurve*/, kAudioUnitScope_Input, i, &curve, sizeof(curve));
					}
				}
				else
				{
					mCalculateDistance = false;
					// turn off distance attenuation altogether
					// the source will then apply the exponential distance formula as a gain scalar and set the bus gain instead of setting any distance
				}
				break;
				
			case AL_NONE:
				{
					mCalculateDistance = false;
					// turn off distance attenuation altogether
				}
			
				break;
				
				
			default:
				break;
		}

		mDistanceModel = inDistanceModel;
		if (mSourceMap)
		{
			CAGuard::Locker locked(mSourceMapLock);
			mSourceMap->MarkAllSourcesForRecalculation();
		}
	}
}	

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void		OALContext::SetDopplerFactor(Float32		inDopplerFactor)
{
#if LOG_VERBOSE
	DebugMessageN2("OALContext::SetDopplerFactor() - OALContext:inDopplerFactor = %ld:%f", (long int) mSelfToken, inDopplerFactor);
#endif
	if (mDopplerFactor != inDopplerFactor)
	{
		mDopplerFactor = inDopplerFactor;
		if (mSourceMap)
		{
			CAGuard::Locker locked(mSourceMapLock);
			mSourceMap->MarkAllSourcesForRecalculation();
		}
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void		OALContext::SetDopplerVelocity(Float32	inDopplerVelocity)
{
#if LOG_VERBOSE
	DebugMessageN2("OALContext::SetDopplerVelocity() - OALContext:inDopplerVelocity = %ld:%f", (long int) mSelfToken, inDopplerVelocity);
#endif
	if (mDopplerVelocity != inDopplerVelocity)
	{
		mDopplerVelocity = inDopplerVelocity;
		// if (mSourceMap)
		//	 mSourceMap->MarkAllSourcesForRecalculation();
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void		OALContext::SetSpeedOfSound(Float32	inSpeedOfSound)
{
#if LOG_VERBOSE
	DebugMessageN2("OALContext::SetSpeedOfSound() - OALContext:inSpeedOfSound = %ld:%f", (long int) mSelfToken, inSpeedOfSound);
#endif
	if (mSpeedOfSound != inSpeedOfSound)
	{
		mSpeedOfSound = inSpeedOfSound;
		if (mSourceMap)
		{
			CAGuard::Locker locked(mSourceMapLock);
			mSourceMap->MarkAllSourcesForRecalculation();
		}
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void		OALContext::SetListenerGain(Float32	inGain)
{
#if LOG_VERBOSE
	DebugMessageN2("OALContext::SetListenerGain() - OALContext:inGain = %ld:%f", (long int) mSelfToken, inGain);
#endif
	if (inGain < 0.0f)
		throw (OSStatus) AL_INVALID_VALUE;
	
	if (mListenerGain != inGain)
	{
		mListenerGain = inGain;
		
		Float32	db = 20.0 * log10(inGain); 				// convert to db
		AudioUnitSetParameter (mMixerUnit, k3DMixerParam_Gain, kAudioUnitScope_Output, 0, db, 0);
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt32		OALContext::GetSourceCount()
{
#if LOG_VERBOSE
	DebugMessageN1("OALContext::GetSourceCount() - OALContext = %ld", (long int) mSelfToken);
#endif
	UInt32 count = 0;

	if (mSourceMap)
	{
		CAGuard::Locker locked(mSourceMapLock);
		count = mSourceMap->Size();
	}

	return count;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void		OALContext::SetListenerPosition(Float32	posX, Float32	posY, Float32	posZ) 
{
#if LOG_VERBOSE
	DebugMessageN4("OALContext::SetListenerPosition() - OALContext:posX:posY:posZ = %ld:%f:%f:%f", (long int) mSelfToken, posX, posY, posZ);
#endif
	if (isnan(posX) || isnan(posY) || isnan(posZ))
		throw ((OSStatus) AL_INVALID_VALUE);                        

	if (	(mListenerPosition[0] == posX) 	&& 
			(mListenerPosition[1] == posY )	&& 
			(mListenerPosition[2] == posZ)		)
		return;
	
	mListenerPosition[0] = posX;
	mListenerPosition[1] = posY;
	mListenerPosition[2] = posZ;

	if (mSourceMap)
	{
#if LOG_GRAPH_AND_MIXER_CHANGES
	DebugMessageN4("OALContext::SetListenerPosition called - OALSource = %f:%f:%f/%ld\n", posX, posY, posZ, mSelfToken);
#endif
		CAGuard::Locker locked(mSourceMapLock);
		// moving the listener effects the coordinate translation for ALL the sources
		mSourceMap->MarkAllSourcesForRecalculation();
	}	
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void		OALContext::SetListenerVelocity(Float32	posX, Float32	posY, Float32	posZ) 
{
#if LOG_VERBOSE
	DebugMessageN4("OALContext::SetListenerVelocity() - OALContext:posX:posY:posZ = %ld:%f:%f:%f", (long int) mSelfToken, posX, posY, posZ);
#endif
	mListenerVelocity[0] = posX;
	mListenerVelocity[1] = posY;
	mListenerVelocity[2] = posZ;

	if (mSourceMap)
	{
#if LOG_GRAPH_AND_MIXER_CHANGES
	DebugMessage("OALContext::SetListenerVelocity: MarkAllSourcesForRecalculation called\n");
#endif
		CAGuard::Locker locked(mSourceMapLock);
		// moving the listener effects the coordinate translation for ALL the sources
        mSourceMap->MarkAllSourcesForRecalculation();
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	OALContext::SetListenerOrientation( Float32	forwardX, 	Float32	forwardY,	Float32	forwardZ,
											Float32	 upX, 		Float32	 upY, 		Float32	 upZ)
{
#if LOG_VERBOSE
	DebugMessageN7("OALContext::SetListenerOrientation() - OALContext:forwardX:forwardY:forwardZ:upX:upY:upZ = %ld:%f:%f:%f:%f:%f:%f", (long int) mSelfToken, forwardX, forwardY, forwardZ, upX, upY, upZ);
#endif

	if (isnan(forwardX) || isnan(forwardY) || isnan(forwardZ) || isnan(upX) || isnan(upY) || isnan(upZ))
		throw ((OSStatus) AL_INVALID_VALUE);                        

	if (	(mListenerOrientationForward[0] == forwardX) 	&& 
			(mListenerOrientationForward[1] == forwardY )	&& 
			(mListenerOrientationForward[2] == forwardZ) 	&&
			(mListenerOrientationUp[0] == upX) 				&& 
			(mListenerOrientationUp[1] == upY ) 			&& 
			(mListenerOrientationUp[2] == upZ)					)
		return;

	mListenerOrientationForward[0] = forwardX;
	mListenerOrientationForward[1] = forwardY;
	mListenerOrientationForward[2] = forwardZ;
	mListenerOrientationUp[0] = upX;
	mListenerOrientationUp[1] = upY;
	mListenerOrientationUp[2] = upZ;

	if (mSourceMap)
	{
#if LOG_GRAPH_AND_MIXER_CHANGES
	DebugMessage("OALContext::SetListenerOrientation: MarkAllSourcesForRecalculation called\n");
#endif
		CAGuard::Locker locked(mSourceMapLock);
		// moving the listener effects the coordinate translation for ALL the sources
		mSourceMap->MarkAllSourcesForRecalculation();
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt32	OALContext::GetDesiredRenderChannels(UInt32	inDeviceChannels)
{
#if LOG_VERBOSE
	DebugMessageN2("OALContext::GetDesiredRenderChannels() - OALContext:inDeviceChannels = %ld:%d", (long int) mSelfToken, inDeviceChannels);
#endif
    UInt32	returnValue = inDeviceChannels;
	
	if ((Get3DMixerVersion() < k3DMixerVersion_2_0) && (returnValue == 4))
    {
        // quad did not work properly before version 2.0 of the 3DMixer, so just render to stereo
        returnValue = 2;
    }
    else if (inDeviceChannels < 4)
    {
        // guard against the possibility of multi channel hw that has never been given a preferred channel layout
        // Or, that a 3 channel layout was returned (which is unsupported by the 3DMixer)
        returnValue = 2; 
    } 
    else if ((inDeviceChannels > 5) && (Get3DMixerVersion() < k3DMixerVersion_2_3))
    {
		// 3DMixer ver. 2.2 and below could only render a maximum of 5 channels
		returnValue = 5;    
    }
	else if(inDeviceChannels > 8)
	{
		// Current 3DMixer can handle a maximum of 8 channels
		returnValue = 8;
	}
	
	return returnValue;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void OALContext::InitRenderQualityOnBusses()
{
#if LOG_VERBOSE
	DebugMessageN1("OALContext::InitRenderQualityOnBusses() - OALContext = %ld", (long int) mSelfToken);
#endif
	UInt32		channelCount = mOwningDevice->GetDesiredRenderChannelCount();

	if (channelCount > 2)
	{
        // at this time, there is only one spatial quality being used for multi channel hw
        DebugMessage("********** InitRenderQualityOnBusses:kDefaultMultiChannelQuality ***********");
		mSpatialSetting = kDefaultMultiChannelQuality;
	}
	else if (mRenderQuality == ALC_MAC_OSX_SPATIAL_RENDERING_QUALITY_LOW)
	{
		// this is the default case for stereo
        DebugMessage("********** InitRenderQualityOnBusses:kDefaultLowQuality ***********");
		mSpatialSetting =  kDefaultLowQuality;
	}
	else
	{
		DebugMessage("********** InitRenderQualityOnBusses:kDefaultHighQuality ***********");
		mSpatialSetting = kDefaultHighQuality;
	}
	
	UInt32 		render_flags_3d = k3DMixerRenderingFlags_DistanceAttenuation;
	if (mRenderQuality == ALC_MAC_OSX_SPATIAL_RENDERING_QUALITY_HIGH)
	{
    	 // off by default, on if the user sets High Quality rendering, as HRTF requires InterAuralDelay to be on
         render_flags_3d += k3DMixerRenderingFlags_InterAuralDelay;     
	}
    
    if (mASAReverbState > 0)
	{
    	 // off by default, on if the user turns on Reverb, as it requires DistanceDiffusion to be on
    	render_flags_3d += k3DMixerRenderingFlags_DistanceDiffusion;
		render_flags_3d += (1L << 6 /* k3DMixerRenderingFlags_ConstantReverbBlend*/);    
	}
    
	OSStatus                    result = noErr;
    UInt32                      propSize;
    CAStreamBasicDescription	format;
	for (UInt32	i = 0; i < mBusCount; i++)
	{
		propSize = sizeof(format);
		result = AudioUnitGetProperty (	mMixerUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, i, &format, &propSize);
		
		// only reset the mono channels, stereo channels are always set to stereo pass thru regardless of render quality setting
        if ((result == noErr) && (format.NumberChannels() == 1)) 
		{
			// Spatialization
			result = AudioUnitSetProperty(	mMixerUnit, kAudioUnitProperty_SpatializationAlgorithm, kAudioUnitScope_Input, 
											i, &mSpatialSetting, sizeof(mSpatialSetting));

			// Render Flags                
            result = AudioUnitSetProperty(	mMixerUnit, kAudioUnitProperty_3DMixerRenderingFlags, kAudioUnitScope_Input, 
											i, &render_flags_3d, sizeof(render_flags_3d));						
		}
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void OALContext::SetRenderQuality (UInt32 inRenderQuality)
{
#if LOG_VERBOSE
	DebugMessageN2("OALContext::SetRenderQuality() - OALContext:inRenderQuality = %ld:%d", (long int) mSelfToken, inRenderQuality);
#endif
	if (mRenderQuality == inRenderQuality)
		return;	// nothing to do;

	// make sure a valid quality setting is requested
	if (!IsValidRenderQuality(inRenderQuality))
		throw (OSStatus) AL_INVALID_VALUE;

	mRenderQuality = inRenderQuality;
			
	// change the spatialization for all mono busses on the mixer
	InitRenderQualityOnBusses(); 
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void    OALContext::SetDistanceAttenuation(UInt32    inBusIndex, Float64 inRefDist, Float64 inMaxDist, Float64 inRolloff)
{
	if (Get3DMixerVersion() >= k3DMixerVersion_2_0)
		return;     // unnecessary with v2.0 mixer
        
    Float64     maxattenuationDB = 20 * log10(inRefDist / (inRefDist + (inRolloff * (inMaxDist - inRefDist))));
    Float64     maxattenuation = pow(10, (maxattenuationDB/20));                    
    Float64     distAttenuation = (log(1/maxattenuation))/(log(inMaxDist)) - 1.0;

	#if 0
		DebugMessageN1("SetDistanceAttenuation:Reference Distance =  %f", inRefDist);
		DebugMessageN1("SetDistanceAttenuation:Maximum Distance =  %f", inMaxDist);
		DebugMessageN1("SetDistanceAttenuation:Rolloff =  %f", inRolloff);
		DebugMessageN1("SetDistanceAttenuati2on:Max Attenuation DB =  %f", maxattenuationDB);
		DebugMessageN1("SetDistanceAttenuation:Max Attenuation Scalar =  %f", maxattenuation);
		DebugMessageN1("SetDistanceAttenuation:distAttenuation =  %f", distAttenuation);

	#endif
    
    AudioUnitSetProperty(mMixerUnit, kAudioUnitProperty_3DMixerDistanceAtten, kAudioUnitScope_Input, inBusIndex, &distAttenuation, sizeof(distAttenuation));
    return;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt32		OALContext::GetAvailableMonoBus (ALuint inSourceToken)
{
#if LOG_VERBOSE
	DebugMessageN2("OALContext::GetAvailableMonoBus() - OALContext:inSourceToken = %ld:%d", (long int) mSelfToken, inSourceToken);
#endif
	// look for a bus already set for mono
	for (UInt32 i = 0; i < mBusCount; i++)
	{
		if ((mBusInfo[i].mSourceAttached == kNoSourceAttached) && mBusInfo[i].mNumberChannels == 1) 
		{
			mBusInfo[i].mSourceAttached = inSourceToken;
#if LOG_BUS_CONNECTIONS
			mMonoSourcesConnected++;
			DebugMessageN2("GetAvailableMonoBus1: Sources Connected, Mono =  %ld, Stereo = %ld", mMonoSourcesConnected, mStereoSourcesConnected);
			DebugMessageN1("GetAvailableMonoBus1: BUS_NUMBER = %ld", i);
#endif
			return (i);
		}
	}

	// do not try and switch a bus to mono if the appliction specified mono and stereo bus counts
	if (!mUserSpecifiedBusCounts)
	{	
		// couldn't find a mono bus, so find any available channel and make it mono
		for (UInt32 i = 0; i < mBusCount; i++)
		{
			if (mBusInfo[i].mSourceAttached == kNoSourceAttached) 
			{
	#if LOG_BUS_CONNECTIONS
				mMonoSourcesConnected++;
				DebugMessageN2("GetAvailableMonoBus2: Sources Connected, Mono =  %ld, Stereo = %ld", mMonoSourcesConnected, mStereoSourcesConnected);
	#endif
				CAStreamBasicDescription 	theOutFormat;
				theOutFormat.mChannelsPerFrame = 1;
				theOutFormat.mSampleRate = GetMixerRate();          // as a default, set the bus to the mixer's output rate, it should get reset if necessary later on
				theOutFormat.mFormatID = kAudioFormatLinearPCM;
				theOutFormat.mFormatFlags = kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved;
				theOutFormat.mBytesPerPacket = sizeof (Float32);	
				theOutFormat.mFramesPerPacket = 1;	
				theOutFormat.mBytesPerFrame = sizeof (Float32);
				theOutFormat.mBitsPerChannel = sizeof (Float32) * 8;	
				OSStatus	result = AudioUnitSetProperty (	mMixerUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 
															i, &theOutFormat, sizeof(CAStreamBasicDescription));
					THROW_RESULT

				mBusInfo[i].mSourceAttached = inSourceToken; 
				mBusInfo[i].mNumberChannels = 1; 			
				AudioUnitSetProperty(	mMixerUnit, kAudioUnitProperty_SpatializationAlgorithm, kAudioUnitScope_Input, 
										i, &mSpatialSetting, sizeof(mSpatialSetting));

				UInt32 		render_flags_3d = k3DMixerRenderingFlags_DistanceAttenuation;
				if (mRenderQuality == ALC_MAC_OSX_SPATIAL_RENDERING_QUALITY_HIGH)
					render_flags_3d += k3DMixerRenderingFlags_InterAuralDelay; // off by default, on if the user sets High Quality rendering

				// Render Flags
				result = AudioUnitSetProperty(	mMixerUnit, kAudioUnitProperty_3DMixerRenderingFlags, kAudioUnitScope_Input, 
												i, &render_flags_3d, sizeof(render_flags_3d));
				
				return (i);
			}
		}
	}
	
	DebugMessage("ERROR: GetAvailableMonoBus: COULD NOT GET A MONO BUS");
	throw (-1); // no inputs available
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt32		OALContext::GetAvailableStereoBus (ALuint inSourceToken)
{
#if LOG_VERBOSE
	DebugMessageN2("OALContext::GetAvailableStereoBus() - OALContext:inSourceToken = %ld:%d", (long int) mSelfToken, inSourceToken);
#endif
	for (UInt32 i = 0; i < mBusCount; i++)
	{
		if ((mBusInfo[i].mSourceAttached == kNoSourceAttached) && mBusInfo[i].mNumberChannels == 2) 
		{
			mBusInfo[i].mSourceAttached = inSourceToken;
#if LOG_BUS_CONNECTIONS
			mStereoSourcesConnected++;
			DebugMessageN2("GetAvailableStereoBus1: Sources Connected, Mono =  %ld, Stereo = %ld", mMonoSourcesConnected, mStereoSourcesConnected);
			DebugMessageN1("GetAvailableStereoBus1: BUS_NUMBER = %ld", i);
#endif
			return (i);
		}
	}

	// do not try and switch a bus to stereo if the appliction specified mono and stereo bus counts
	if (!mUserSpecifiedBusCounts)
	{
		// couldn't find one, so look for a mono channel, make it stereo and set to kSpatializationAlgorithm_StereoPassThrough
		for (UInt32 i = 0; i < mBusCount; i++)
		{
			if (mBusInfo[i].mSourceAttached == kNoSourceAttached) 
			{

	#if LOG_BUS_CONNECTIONS
				mStereoSourcesConnected++;
				DebugMessageN2("GetAvailableStereoBus2: Sources Connected, Mono =  %ld, Stereo = %ld", mMonoSourcesConnected, mStereoSourcesConnected);
				DebugMessageN1("GetAvailableStereoBus2: BUS_NUMBER = %ld", i);
	#endif
				CAStreamBasicDescription 	theOutFormat;
				theOutFormat.mChannelsPerFrame = 2;
				theOutFormat.mSampleRate = GetMixerRate();
				theOutFormat.mFormatID = kAudioFormatLinearPCM;
				theOutFormat.mFormatFlags = kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved;
				theOutFormat.mBytesPerPacket = sizeof (Float32);	
				theOutFormat.mFramesPerPacket = 1;	
				theOutFormat.mBytesPerFrame = sizeof (Float32);
				theOutFormat.mBitsPerChannel = sizeof (Float32) * 8;	
				OSStatus	result = AudioUnitSetProperty (	mMixerUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 
															i, &theOutFormat, sizeof(CAStreamBasicDescription));
					THROW_RESULT

				mBusInfo[i].mSourceAttached = inSourceToken;
				mBusInfo[i].mNumberChannels = 2; 

				UInt32		spatAlgo = kSpatializationAlgorithm_StereoPassThrough;
				AudioUnitSetProperty(	mMixerUnit, kAudioUnitProperty_SpatializationAlgorithm, kAudioUnitScope_Input, 
										i, &spatAlgo, sizeof(spatAlgo));

				return (i);
			}
		}
	}
	
	DebugMessage("ERROR: GetAvailableStereoBus: COULD NOT GET A STEREO BUS");
	throw (-1); // no inputs available
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	OALContext::SetBusAsAvailable (UInt32 inBusIndex)
{
#if LOG_VERBOSE
	DebugMessageN2("OALContext::SetBusAsAvailable() - OALContext:inBusIndex = %ld:%d", (long int) mSelfToken, inBusIndex);
#endif
	mBusInfo[inBusIndex].mSourceAttached = kNoSourceAttached;

#if LOG_BUS_CONNECTIONS
	if (mBusInfo[inBusIndex].mNumberChannels == 1)
		mMonoSourcesConnected--;
	else
		mStereoSourcesConnected--;

		DebugMessageN2("SetBusAsAvailable: Sources Connected, Mono =  %ld, Stereo = %ld", mMonoSourcesConnected, mStereoSourcesConnected);
#endif
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Apple Environmental Audio (ASA) Extension
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	OALContext::SetReverbRoomType(UInt32 inRoomType)
{
#if LOG_VERBOSE
	DebugMessageN2("OALContext::SetReverbRoomType() - OALContext:inRoomType = %ld:%d", (long int) mSelfToken, inRoomType);
#endif
	if (mASAReverbRoomType == inRoomType)
		return;	// nothing to do;

    mASAReverbRoomType = inRoomType;
        
    OSStatus    result = AudioUnitSetProperty(mMixerUnit, kAudioUnitProperty_ReverbRoomType, kAudioUnitScope_Global, 0, &mASAReverbRoomType, sizeof(mASAReverbRoomType));			
		THROW_RESULT
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	OALContext::SetReverbLevel(Float32 inReverbLevel)
{
#if LOG_VERBOSE
	DebugMessageN2("OALContext::SetReverbLevel() - OALContext:inReverbLevel = %ld:%f", (long int) mSelfToken, inReverbLevel);
#endif

	if (mASAReverbGlobalLevel == inReverbLevel)
		return;	// nothing to do;

    mASAReverbGlobalLevel = inReverbLevel;
        
	OSStatus	result = AudioUnitSetParameter (mMixerUnit, 6 /*k3DMixerParam_GlobalReverbGain*/, kAudioUnitScope_Global, 0, mASAReverbGlobalLevel, 0);
		THROW_RESULT
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	OALContext::SetReverbState(UInt32 inReverbState)
{
#if LOG_VERBOSE
	DebugMessageN2("OALContext::SetReverbState() - OALContext:inReverbState = %ld:%d", (long int) mSelfToken, inReverbState);
#endif
	if (mASAReverbState == inReverbState)
		return;	// nothing to do;

    mASAReverbState = inReverbState;
        
    OSStatus    result = AudioUnitSetProperty(mMixerUnit, kAudioUnitProperty_UsesInternalReverb, kAudioUnitScope_Global, 0, &mASAReverbState, sizeof(mASAReverbState));			
	if (result == noErr)
        InitRenderQualityOnBusses(); // distance diffusion needs to be reset on the busses now
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void			OALContext::SetReverbQuality(UInt32 inQuality)
{
#if LOG_VERBOSE
	DebugMessageN2("OALContext::SetReverbQuality() - OALContext:inQuality = %ld:%d", (long int) mSelfToken, inQuality);
#endif
	if (mASAReverbQuality == inQuality)
		return;	// nothing to do;

    mASAReverbQuality = inQuality;

   AudioUnitSetProperty(mMixerUnit, kAudioUnitProperty_RenderQuality, kAudioUnitScope_Global, 0, &mASAReverbQuality, sizeof(mASAReverbQuality));			
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void			OALContext::SetReverbEQGain(Float32 inGain)
{
#if LOG_VERBOSE
	DebugMessageN2("OALContext::SetReverbEQGain() - OALContext:inGain = %ld:%f", (long int) mSelfToken, inGain);
#endif
	if (mASAReverbEQGain != inGain)
	{
		mASAReverbEQGain = inGain;
		OSStatus	result = AudioUnitSetParameter (mMixerUnit, 20000 + 16 /*kReverbParam_FilterGain*/, kAudioUnitScope_Global, 0, mASAReverbEQGain, 0);
			THROW_RESULT
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void			OALContext::SetReverbEQBandwidth(Float32 inBandwidth)
{
#if LOG_VERBOSE
	DebugMessageN2("OALContext::SetReverbEQBandwidth() - OALContext:inBandwidth = %ld:%f", (long int) mSelfToken, inBandwidth);
#endif
	if (mASAReverbEQBandwidth != inBandwidth)
	{
		mASAReverbEQBandwidth = inBandwidth;
		OSStatus	result = AudioUnitSetParameter (mMixerUnit, 20000 + 15 /*kReverbParam_FilterBandwidth*/, kAudioUnitScope_Global, 0, mASAReverbEQBandwidth, 0);
			THROW_RESULT
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void			OALContext::SetReverbEQFrequency(Float32 inFrequency)
{
#if LOG_VERBOSE
	DebugMessageN2("OALContext::SetReverbEQFrequency() - OALContext:inFrequency = %ld:%f", (long int) mSelfToken, inFrequency);
#endif
	if (mASAReverbEQFrequency != inFrequency)
	{
		mASAReverbEQFrequency = inFrequency;
		OSStatus	result = AudioUnitSetParameter (mMixerUnit, 20000 + 14 /*kReverbParam_FilterFrequency*/, kAudioUnitScope_Global, 0, mASAReverbEQFrequency, 0);
			THROW_RESULT
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt32			OALContext::GetReverbQuality()
{
#if LOG_VERBOSE
	DebugMessageN1("OALContext::GetReverbQuality() - OALContext = %ld", (long int) mSelfToken);
#endif
	return mASAReverbQuality;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Float32			OALContext::GetReverbEQGain()
{
#if LOG_VERBOSE
	DebugMessageN1("OALContext::GetReverbEQGain() - OALContext = %ld", (long int) mSelfToken);
#endif
	OSStatus	result = AudioUnitGetParameter(mMixerUnit, 20000 + 16 /*kReverbParam_FilterGain*/,kAudioUnitScope_Global,0, &mASAReverbEQGain);
	if (result)
		return 0.0;
	
	return mASAReverbEQGain;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Float32			OALContext::GetReverbEQBandwidth()
{
#if LOG_VERBOSE
	DebugMessageN1("OALContext::GetReverbEQBandwidth() - OALContext = %ld", (long int) mSelfToken);
#endif
	OSStatus	result = AudioUnitGetParameter(mMixerUnit, 20000 + 15 /*kReverbParam_FilterBandwidth*/,kAudioUnitScope_Global,0, &mASAReverbEQBandwidth);
	if (result)
		return 0.0;

	return mASAReverbEQBandwidth;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Float32			OALContext::GetReverbEQFrequency()
{
#if LOG_VERBOSE
	DebugMessageN1("OALContext::GetReverbEQFrequency() - OALContext = %ld", (long int) mSelfToken);
#endif
	OSStatus	result =  AudioUnitGetParameter(mMixerUnit, 20000 + 14 /*kReverbParam_FilterFrequency*/,kAudioUnitScope_Global,0, &mASAReverbEQFrequency);
	if (result)
		return 0.0;

	return mASAReverbEQFrequency;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	OALContext::SetReverbPreset (FSRef* inRef)
{
#if LOG_VERBOSE
	DebugMessageN1("OALContext::SetReverbPreset() - OALContext = %ld", (long int) mSelfToken);
#endif
	try {
			Boolean				status; 
			SInt32				result = 0;
			CFURLRef			fileURL = CFURLCreateFromFSRef (kCFAllocatorDefault, inRef);
			if (fileURL) 
			{
				// Read the XML file.
				CFDataRef		resourceData = NULL;
				
				status = CFURLCreateDataAndPropertiesFromResource (kCFAllocatorDefault, fileURL, &resourceData,	NULL, NULL, &result);
				CFRelease (fileURL);	// no longer needed
				
				if (status == false || result)				
					throw (OSStatus) -1;			
				else
				{
					CFStringRef			errString = NULL;
					CFPropertyListRef   theData = NULL;
					theData = CFPropertyListCreateFromXMLData (kCFAllocatorDefault, resourceData, kCFPropertyListImmutable, &errString);
					CFRelease (resourceData);
					if (errString)
						CFRelease (errString);
					
					if (theData == NULL || errString) 
					{
						if (theData)
							CFRelease (theData);
						throw (OSStatus) -1;			
					}
					else
					{
						result = AudioUnitSetProperty(mMixerUnit, 3012 /*kAudioUnitProperty_ReverbPreset*/, kAudioUnitScope_Global, 0, &theData, sizeof(theData) );
						CFRelease (theData);
						THROW_RESULT
					}				
				}
			}
			else
				throw (OSStatus) -1;			
	}
	catch (OSStatus result) {
		throw result;
	}
	catch (...) {
		throw (OSStatus) -1;			
	}
	
	return;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus	OALContext::ContextNotificationProc (	void 						*inRefCon, 
													AudioUnitRenderActionFlags 	*inActionFlags,
													const AudioTimeStamp 		*inTimeStamp, 
													UInt32 						inBusNumber,
													UInt32 						inNumberFrames, 
													AudioBufferList 			*ioData)
{
#if LOG_VERBOSE
	DebugMessage("OALContext::ContextNotificationProc()");
#endif
	OALContext* THIS = (OALContext*)inRefCon;
	
	// we have no use for a pre-render notification, we only care about the post-render
	if (*inActionFlags & kAudioUnitRenderAction_PreRender)
	{
		THIS->mRenderThreadID = pthread_self();
		THIS->DoPreRender();
	}
		
	else if (*inActionFlags & kAudioUnitRenderAction_PostRender)
		return THIS->DoPostRender();
		
	return (noErr);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus OALContext::DoPreRender ()
{
#if LOG_VERBOSE
	DebugMessageN1("OALContext::DoPreRender() - OALContext = %ld", (long int) mSelfToken);
#endif
#if	LOG_MESSAGE_QUEUE					
	DebugMessageN1("OALContext::DoPreRender");
#endif

	for (UInt32 i=0; i < mBusCount; i++)
	{
		OALSource *oalSource = GetSourceForRender(mBusInfo[i].mSourceAttached);
		if (oalSource != NULL)
		{
			oalSource->DoPreRender();
			ReleaseSource(oalSource);
		}
	}
	
	return noErr;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSStatus OALContext::DoPostRender ()
{
#if LOG_VERBOSE
	DebugMessageN1("OALContext::DoPostRender() - OALContext = %ld", (long int) mSelfToken);
#endif
#if	LOG_MESSAGE_QUEUE					
	DebugMessageN1("OALContext::DoPostRender");
#endif

	for (UInt32 i=0; i < mBusCount; i++)
	{
		OALSource *oalSource = GetSourceForRender(mBusInfo[i].mSourceAttached);
		if (oalSource == NULL)
		{
			//if we have a source that needs post-render but has been deleted. We need to just deconstruct
			oalSource = GetDeadSourceForRender(mBusInfo[i].mSourceAttached);
			if (oalSource)
			{
				//clear all the current messages
				oalSource->ClearMessageQueue();
				oalSource->AddPlaybackMessage((UInt32)kMQ_DeconstructionStop, NULL, 0);
			}
			else continue;
		}
		
		oalSource->DoPostRender();
		ReleaseSource(oalSource);
	}
	
	return noErr;
}
