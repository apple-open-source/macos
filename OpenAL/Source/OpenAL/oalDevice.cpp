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

#include "oalDevice.h"
#include "oalContext.h"

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#define LOG_DEVICE_CHANGES  	0
#define PROFILE_IO_USAGE		0
#define LOG_VERBOSE             0

#if PROFILE_IO_USAGE
static int debugCounter         = -1;
static int numCyclesToPrint     = 1000;

static UInt64 lastHostTime;
static UInt64 totalHostTime;
static UInt64 minUsage;
static UInt64 maxUsage;
static UInt64 totalUsage;

#define PROFILE_IO_CYCLE 0
#if PROFILE_IO_CYCLE
static UInt64 maxHT;
static UInt64 minHT;
#endif

#include <CoreAudio/HostTime.h>
#endif

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#if GET_OVERLOAD_NOTIFICATIONS
OSStatus	PrintTheOverloadMessage(	AudioDeviceID			inDevice,
										UInt32					inChannel,
										Boolean					isInput,
										AudioDevicePropertyID	inPropertyID,
										void*					inClientData)
{
	DebugMessage("OVERLOAD OCCURRED");
}
#endif

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// OALDevices
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#pragma mark ***** OALDevices *****

/*
	If caller wants a specific HAL device (instead of the default output device), a NULL terminated 
	C-String representation of the CFStringRef returned from the HAL APIs for the 
	kAudioDevicePropertyDeviceUID property
*/
OALDevice::OALDevice (const char* 	 inDeviceName, uintptr_t   inSelfToken, UInt32	inRenderChannelSetting)
	: 	mSelfToken (inSelfToken),
		mCurrentError(ALC_NO_ERROR),
        mHALDevice (0),
        mDistanceScalingRequired(false),
		mGraphInitialized(false),
        mAUGraph(0),
		mOutputNode(0), 
		mOutputUnit(0),
		mMixerNode(0),
		mChannelLayoutTag(0),
		mConnectedContext(NULL),
		mDeviceSampleRate(kDefaultMixerRate),
		mRenderChannelCount(0),
        mRenderChannelSetting(inRenderChannelSetting),
		mFramesPerSlice(512),
		mInUseFlag(0)
{
#if LOG_VERBOSE
	DebugMessageN1("OALDevice::OALDevice() - OALDevice = %ld", (long int) mSelfToken);
#endif
	OSStatus	result = noErr;
	UInt32		size = 0;
	CFStringRef	cfString = NULL;
    char        *useThisDevice = (char *) inDeviceName;
	
	try {
		// make sure a proper render channel setting was passed to teh constructor
		if ((inRenderChannelSetting != ALC_MAC_OSX_RENDER_CHANNEL_COUNT_MULTICHANNEL) && (inRenderChannelSetting != ALC_MAC_OSX_RENDER_CHANNEL_COUNT_STEREO))
			throw (OSStatus) AL_INVALID_VALUE;

        // until the ALC_ENUMERATION_EXT extension is supported only use the default output device
        useThisDevice = NULL;
        
		// first, get the requested HAL device's ID
		if (useThisDevice)
		{
			// turn the inDeviceName into a CFString
			cfString = CFStringCreateWithCString(NULL, useThisDevice, kCFStringEncodingUTF8);
			if (cfString)
			{
				AudioValueTranslation	translation;
				
				translation.mInputData = &cfString;
				translation.mInputDataSize = sizeof(cfString);
				translation.mOutputData = &mHALDevice;
				translation.mOutputDataSize = sizeof(mHALDevice);
				
				size = sizeof(AudioValueTranslation);
				result = AudioHardwareGetProperty(kAudioHardwarePropertyDeviceForUID, &size, &translation);
                CFRelease (cfString);
			}
			else
				result = -1; // couldn't get string ref
				
			THROW_RESULT
		}
		else
		{
			size = sizeof(AudioDeviceID);
			result = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultOutputDevice, &size, &mHALDevice);
				THROW_RESULT
		}
		
		InitializeGraph(useThisDevice);

		mRenderChannelCount = GetDesiredRenderChannelCount();
		
#if PROFILE_IO_USAGE
		debugCounter = -1;
#endif

	}
	catch (...) {
        throw;
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OALDevice::~OALDevice()
{
#if LOG_VERBOSE
	DebugMessageN1("OALDevice::~OALDevice() - OALDevice = %ld", (long int) mSelfToken);
#endif
	TeardownGraph();
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void	OALDevice::SetError(ALenum errorCode)
{
#if LOG_VERBOSE
	DebugMessageN2("OALDevice::SetError() - OALDevice:errorCode = %ld:%d", (long int) mSelfToken, errorCode);
#endif
	if (mCurrentError == ALC_NO_ERROR)
		return;
	
	mCurrentError = errorCode;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ALenum	OALDevice::GetError()
{
#if LOG_VERBOSE
	DebugMessageN1("OALDevice::GetError() - OALDevice = %ld", (long int) mSelfToken);
#endif
	ALenum	latestError = mCurrentError;
	mCurrentError = ALC_NO_ERROR;
	
	return latestError;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void OALDevice::TeardownGraph()
{
#if LOG_VERBOSE || LOG_DEVICE_CHANGES
	DebugMessageN1("OALDevice::TeardownGraph() - OALDevice = %ld", (long int) mSelfToken);
#endif

#if GET_OVERLOAD_NOTIFICATIONS
	AudioDeviceID	device = 0;
	UInt32			size = sizeof(device);
	
	AudioHardwareGetProperty(kAudioHardwarePropertyDefaultOutputDevice, &size, &device);
	if (device != 0)
	{
		DebugMessage("********** Removing Overload Notification ***********");
		AudioDeviceRemovePropertyListener(	device, 0, false, kAudioDeviceProcessorOverload, PrintTheOverloadMessage);	
	}
#endif

	if (mAUGraph) 
	{
		StopGraph();
		DisposeAUGraph (mAUGraph);
		mAUGraph = 0;
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// also resets the audio channel layout if necessary
void	OALDevice::ResetRenderChannelSettings()
{
#if LOG_VERBOSE || LOG_DEVICE_CHANGES
	DebugMessageN1("OALDevice::ResetRenderChannelSettings() - OALDevice = %ld", (long int) mSelfToken);
#endif

	// verify that the channel count has actually changed before doing all this work...
	UInt32	channelCount = GetDesiredRenderChannelCount();

	if (mRenderChannelCount == channelCount)
		return; // only reset the graph if the channel count has changed

	mRenderChannelCount = channelCount;

	Boolean		wasRunning = false;
	AUGraphIsRunning (mAUGraph, &wasRunning);
	if (wasRunning)
		StopGraph();

	// disconnect the mixer (mMixerNode) from the  output au if necessary
	OSStatus	result = noErr;
	if (mMixerNode)
	{
		// mixer is currently connected
		result = AUGraphDisconnectNodeInput (mAUGraph, mOutputNode, 0);
			THROW_RESULT
		// update the graph
		result = AUGraphUpdate (mAUGraph, NULL);
			THROW_RESULT
	}
	
	// set AU properties
	{
		CAStreamBasicDescription	format;
		UInt32                      propSize = sizeof(format);
		// get the current input scope format  of the output device, so we can just change the channel count
		result = AudioUnitGetProperty(mOutputUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &format, &propSize);
			THROW_RESULT

		format.SetCanonical (mRenderChannelCount, false);     // not interleaved
		result = AudioUnitSetProperty (mOutputUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &format, sizeof(format));
			THROW_RESULT
			
		// The Output Format of the Contexts that use this device will be set later by ReconfigureContextsOfThisDevice()
	
		// channel layout may have changed do to a different render channel count
		AudioChannelLayout		layout;
		layout.mChannelLayoutTag = GetChannelLayoutTag();
		layout.mChannelBitmap = 0;			
		layout.mNumberChannelDescriptions = 0;
		result = AudioUnitSetProperty (mOutputUnit, kAudioUnitProperty_AudioChannelLayout, kAudioUnitScope_Input, 0, &layout, sizeof(layout));
			THROW_RESULT
	}

	// tell the contexts using this device to change their mixer output format - so when they are reconnected to the output au - formats will be reset
	ReconfigureContextsOfThisDevice(mSelfToken);
	
	// reconnect mixer to output unit if it was previously connected
	if (mMixerNode)
	{
		result = AUGraphConnectNodeInput (mAUGraph, mMixerNode, 0, mOutputNode, 0);
			THROW_RESULT
		
		// update the graph
		result = AUGraphUpdate (mAUGraph, NULL);
			THROW_RESULT
	}
	
	if (wasRunning)
		AUGraphStart(mAUGraph); // restart the graph if it was already running
	
	return;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void OALDevice::GraphFormatPropertyListener (	void				*inRefCon, 
												AudioUnit			ci, 
												AudioUnitPropertyID inID, 
												AudioUnitScope		inScope, 
												AudioUnitElement	inElement)
{
#if LOG_VERBOSE || LOG_DEVICE_CHANGES
	DebugMessageN1("OALDevice::GraphFormatPropertyListener - OALDevice: %ld", ((OALDevice*)inRefCon)->mSelfToken);
#endif

	try {
		if (inScope == kAudioUnitScope_Output)
		{
			((OALDevice*)inRefCon)->ResetRenderChannelSettings ();
		}		
	} 
	catch (...) {
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void OALDevice::SetRenderChannelSetting (UInt32 inRenderChannelSetting)
{
#if LOG_VERBOSE || LOG_DEVICE_CHANGES
	DebugMessageN1("OALDevice::SetRenderChannelSetting() - OALDevice = %ld", (long int) mSelfToken);
#endif

    try {
		if ((inRenderChannelSetting != ALC_MAC_OSX_RENDER_CHANNEL_COUNT_MULTICHANNEL) && (inRenderChannelSetting != ALC_MAC_OSX_RENDER_CHANNEL_COUNT_STEREO))
			throw (OSStatus) AL_INVALID_VALUE;
		
		if (inRenderChannelSetting == mRenderChannelSetting)
			return; //nothing to do
			
		mRenderChannelSetting = inRenderChannelSetting;
		
		if (inRenderChannelSetting == ALC_MAC_OSX_RENDER_CHANNEL_COUNT_STEREO)
		{
			// clamping to stereo
			if (mRenderChannelCount == 2)
				return; // already rendering to stereo, so there's nothing to do
		}
		else
		{
			// allowing multi channel now
			if (mRenderChannelCount > 2)
				return; // already rendering to mc, so there's nothing to do
		}    

		// work to be done now, it is necessary to change the channel layout and stream format from multi channel to stereo
		// this requires the graph to be stopped and reconfigured
		ResetRenderChannelSettings ();
	}
	catch (OSStatus		result) {
		DebugMessageN2("OALDevice::SetRenderChannelSetting - OALDevice: %ld:%ld", mSelfToken, (long int) result);
	}
	catch (...) {
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void OALDevice::InitializeGraph (const char* 		inDeviceName)
{	
#if LOG_VERBOSE || LOG_DEVICE_CHANGES
	DebugMessageN1("OALDevice::InitializeGraph() - OALDevice = %ld", (long int) mSelfToken);
#endif

	if (mAUGraph)
		throw static_cast<OSStatus>('init');
	
    OSStatus result = noErr;
    
    // ~~~~~~~~~~~~~~~~~~~~ CREATE GRAPH

	result = NewAUGraph(&mAUGraph);
		THROW_RESULT
	 
    // ~~~~~~~~~~~~~~~~~~~~ SET UP OUTPUT NODE

	ComponentDescription	cd;
	cd.componentFlags = 0;        
	cd.componentFlagsMask = 0;     

	// At this time, only allow the default output device to be used and ignore the inDeviceName parameter
	cd.componentType = kAudioUnitType_Output;          
	cd.componentSubType = kAudioUnitSubType_DefaultOutput;       	
	cd.componentManufacturer = kAudioUnitManufacturer_Apple;  
	result = AUGraphNewNode (mAUGraph, &cd, 0, NULL, &mOutputNode);
		THROW_RESULT
		        		
	// ~~~~~~~~~~~~~~~~~~~~ OPEN GRAPH
	
	result = AUGraphOpen (mAUGraph);
		THROW_RESULT
	
	result = AUGraphGetNodeInfo (mAUGraph, mOutputNode, 0, 0, 0, &mOutputUnit);
		THROW_RESULT   
	
 	result = AudioUnitInitialize (mOutputUnit);
		THROW_RESULT

	result = AudioUnitAddPropertyListener (mOutputUnit, kAudioUnitProperty_StreamFormat, GraphFormatPropertyListener, this);
		THROW_RESULT

	// Frame Per Slice
	// get the device's frame count and set the AUs to match, will be set to 512 if this fails
	AudioDeviceID	device = 0;
	UInt32	dataSize = sizeof(device);
	result = AudioUnitGetProperty(mOutputUnit, kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global, 0, &device, &dataSize);
	if (result == noErr)
	{
		dataSize = sizeof(mFramesPerSlice);
		result = AudioDeviceGetProperty(device, 0, false, kAudioDevicePropertyBufferFrameSize, &dataSize, &mFramesPerSlice);
		if (result == noErr)
		{
			/*result =*/ AudioUnitSetProperty(  mOutputUnit, kAudioUnitProperty_MaximumFramesPerSlice,
											kAudioUnitScope_Global, 0, &mFramesPerSlice, sizeof(mFramesPerSlice));
		}
	}
	
	// get the device's outputRate
	CAStreamBasicDescription	format;
	UInt32                      propSize = sizeof(format);
	result = AudioUnitGetProperty(mOutputUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, &format, &propSize);
	if (result == noErr)
		mDeviceSampleRate = format.mSampleRate;	

}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt32 OALDevice::GetDesiredRenderChannelCount ()
{
#if LOG_VERBOSE
	DebugMessageN1("OALDevice::GetDesiredRenderChannelCount - OALDevice: %ld", mSelfToken);
#endif

	UInt32			returnValue = 2;	// return stereo by default
	
    // observe the mRenderChannelSetting flag and clamp to stereo if necessary
    // This allows the user to request the libary to render to stereo in the case where only 2 speakers
    // are connected to multichannel hardware
    if (mRenderChannelSetting == ALC_MAC_OSX_RENDER_CHANNEL_COUNT_STEREO)
        return (returnValue);

	// get the HAL device id form the output AU
	AudioDeviceID	deviceID;
	UInt32			propSize =  sizeof(deviceID);
	OSStatus	result = AudioUnitGetProperty(mOutputUnit, kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Output, 1, &deviceID, &propSize);
		THROW_RESULT
	
	// get the channel layout set by the user in AMS		
	result = AudioDeviceGetPropertyInfo(deviceID, 0, false, kAudioDevicePropertyPreferredChannelLayout, &propSize, NULL);

	if (result == noErr)
	{
		AudioChannelLayout* layout = (AudioChannelLayout *) calloc(1, propSize);
		if (layout != NULL)
		{
			/*result =*/ AudioDeviceGetProperty(deviceID, 0, false, kAudioDevicePropertyPreferredChannelLayout, &propSize, layout);

			if (layout->mChannelLayoutTag == kAudioChannelLayoutTag_UseChannelDescriptions)
			{
				// no channel layout tag is returned, so walk through the channel descriptions and count 
				// the channels that are associated with a speaker
				if (layout->mNumberChannelDescriptions == 2)
				{	
					returnValue = 2;        // there is no channel info for stereo
				}
				else
				{
					returnValue = 0;
					for (UInt32 i = 0; i < layout->mNumberChannelDescriptions; i++)
					{
						if ((layout->mChannelDescriptions[i].mChannelLabel != kAudioChannelLabel_Unknown) && (layout->mChannelDescriptions[i].mChannelLabel != kAudioChannelLabel_LFEScreen))
							returnValue++;
					}
				}
				mChannelLayoutTag = GetLayoutTagForLayout(layout, returnValue);
			}
			else
			{
				mChannelLayoutTag = layout->mChannelLayoutTag;
				switch (layout->mChannelLayoutTag)
				{
					case kAudioChannelLayoutTag_AudioUnit_5_0:
					case kAudioChannelLayoutTag_AudioUnit_5_1:
						returnValue = 5;
						break;
					case kAudioChannelLayoutTag_AudioUnit_6_0:
					case kAudioChannelLayoutTag_AudioUnit_6_1:
						returnValue = 6;
						break;
					case kAudioChannelLayoutTag_AudioUnit_7_0:
					case kAudioChannelLayoutTag_AudioUnit_7_1:
					case kAudioChannelLayoutTag_AudioUnit_7_0_Front:
						returnValue = 7;
						break;
					case kAudioChannelLayoutTag_AudioUnit_8:
						returnValue = 8;
						break;
					case kAudioChannelLayoutTag_AudioUnit_4:
						returnValue = 4;
						break;
					default:
						returnValue = 2;
						break;
				}
			}	
		
			free(layout);
		}
    }
	// pass in num channels on the hw, 
	// how many channels the user has requested, and which 3DMixer is present
	returnValue	= GetDesiredRenderChannelsFor3DMixer(returnValue);
        
	return (returnValue);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// called from alcDestroyContext(), when all context's that use this device are gone
void OALDevice::StopGraph()
{
#if LOG_VERBOSE
	DebugMessageN1("OALDevice::StopGraph() - OALDevice = %ld", (long int) mSelfToken);
#endif
	AUGraphStop (mAUGraph);
	Boolean flag;
	do {
		AUGraphIsRunning (mAUGraph, &flag);
	} while (flag);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/*
	When a Context is connected for the first time, the graph is initialized & started. 
	It will not be explicitly stopped until all the contexts that use this device have been destroyed.
*/
void	OALDevice::ConnectContext (OALContext*	inContext)
{	
#if LOG_VERBOSE || LOG_DEVICE_CHANGES
	DebugMessageN1("OALDevice::ConnectContext() - OALDevice = %ld", (long int) mSelfToken);
#endif

	OSStatus	result = noErr;
	OALContext*	oldContext = mConnectedContext; // save in case it needs to be restored

	if (inContext == mConnectedContext)
		return;	// already connected

	try {
		// we only have to disconnect when the 3DMixer node has changed
		if (mConnectedContext && (inContext->GetMixerNode() != mMixerNode)){ 
			result = AUGraphDisconnectNodeInput(mAUGraph, mOutputNode, 0);
				THROW_RESULT
			
			mMixerNode = 0;
			mConnectedContext = 0;
		}

		// set AU properties
		{
			CAStreamBasicDescription	format;
			UInt32                      propSize = sizeof(format);
			result = AudioUnitGetProperty(mOutputUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, &format, &propSize);
				THROW_RESULT

			format.SetCanonical (mRenderChannelCount, false);     // not interleaved
			format.mSampleRate = inContext->GetMixerRate();

			result = AudioUnitSetProperty (mOutputUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &format, sizeof(format));
				THROW_RESULT

			result = AudioUnitSetProperty (inContext->GetMixerUnit(), kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 0, &format, sizeof(format));
				THROW_RESULT

			// used to be a configure graph AUs (outout AU and mixerAU to be connected)
			AudioChannelLayout		layout;
			layout.mChannelLayoutTag = GetChannelLayoutTag();
			layout.mChannelBitmap = 0;			
			layout.mNumberChannelDescriptions = 0;
			result = AudioUnitSetProperty (mOutputUnit, kAudioUnitProperty_AudioChannelLayout, kAudioUnitScope_Input, 0, &layout, sizeof(layout));
				THROW_RESULT
		}

		// connect new mixer to output unit
		result = AUGraphConnectNodeInput (mAUGraph, inContext->GetMixerNode(), 0, mOutputNode, 0);
			THROW_RESULT

		mMixerNode = inContext->GetMixerNode();
		mConnectedContext = inContext;

		// initialize the graph on the 1st connection of a mixer to the graph
		if (!mGraphInitialized) {
			AUGraphInitialize(mAUGraph);
			mGraphInitialized = true;
		}
		else
		{
			// if graph in uninitialized or not running , where should this go
			result = AUGraphUpdate (mAUGraph, NULL);
				THROW_RESULT
		}
		
		// the graph may not be running yet
		if (!IsGraphStillRunning ())
			AUGraphStart (mAUGraph);
	}
	catch (OSStatus		result) {
		mConnectedContext = oldContext;
		throw result;
	}
	catch (...) {
		mConnectedContext = oldContext;
		throw -1;
	}
	return;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void		OALDevice::DisconnectContext(OALContext* inContext)
{
#if LOG_VERBOSE || LOG_DEVICE_CHANGES
	DebugMessageN1("OALDevice::DisconnectContext() - OALDevice = %ld", (long int) mSelfToken);
#endif
	if (inContext == mConnectedContext) 
		mConnectedContext = NULL;
		
	AUGraphDisconnectNodeInput(mAUGraph, mOutputNode, 0);
	
	// AUGraphUpdate will block here, until the node is removed
	AUGraphUpdate(mAUGraph, NULL);	
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void		OALDevice::RemoveContext(OALContext* inContext)
{
#if LOG_VERBOSE || LOG_DEVICE_CHANGES
	DebugMessageN1("OALDevice::RemoveContext() - OALDevice = %ld", (long int) mSelfToken);
#endif
	if(inContext == mConnectedContext)
		mConnectedContext = NULL;
	
	// now remove the remove the mixer node for the context
	AUGraphRemoveNode(mAUGraph, inContext->GetMixerNode());
	// AUGraphUpdate will block here, until the node is removed
	AUGraphUpdate(mAUGraph, NULL);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AudioChannelLayoutTag OALDevice::GetLayoutTagForLayout(AudioChannelLayout *inLayout, UInt32 inNumChannels)
{
#if LOG_VERBOSE
	DebugMessageN1("OALDevice::GetLayoutTagForLayout() - OALDevice = %ld", (long int) mSelfToken);
#endif
	if (inNumChannels == 5)
		return kAudioChannelLayoutTag_AudioUnit_5_0;

	// Quad not supported prior to 3d mixer ver. 2.0
	else if ((inNumChannels == 4) && (Get3DMixerVersion() >= k3DMixerVersion_2_0))
		return kAudioChannelLayoutTag_AudioUnit_4;

	// now check for new multichannel formats in the 3d mixer v. 2.3 and higher	
	else if (inNumChannels == 6 && (Get3DMixerVersion() >= k3DMixerVersion_2_3))
		return kAudioChannelLayoutTag_AudioUnit_6_0;	

	else if (inNumChannels == 7 && (Get3DMixerVersion() >= k3DMixerVersion_2_3))
	{
		//return kAudioChannelLayoutTag_AudioUnit_7_0;
		for (UInt32 i=0; i< inLayout->mNumberChannelDescriptions; i++)
		{
			if((inLayout->mChannelDescriptions[i].mChannelLabel == kAudioChannelLabel_RearSurroundLeft) || 
				(inLayout->mChannelDescriptions[i].mChannelLabel == kAudioChannelLabel_RearSurroundRight))
			{
				//if we have rear channels, we need kAudioChannelLayoutTag_AudioUnit_7_0
				return kAudioChannelLayoutTag_AudioUnit_7_0;
			}
		}
		//if we didn't find rear channels, default 7.0 front
		return kAudioChannelLayoutTag_AudioUnit_7_0_Front;
	}

	else if (inNumChannels == 8 && (Get3DMixerVersion() >= k3DMixerVersion_2_3))
		return kAudioChannelLayoutTag_AudioUnit_8;
	
	return  kAudioChannelLayoutTag_Stereo; // default case
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt32		OALDevice::GetChannelLayoutTag()
{
#if LOG_VERBOSE
	DebugMessageN1("OALDevice::GetChannelLayoutTag() - OALDevice = %ld", (long int) mSelfToken);
#endif
	return mChannelLayoutTag;
}
