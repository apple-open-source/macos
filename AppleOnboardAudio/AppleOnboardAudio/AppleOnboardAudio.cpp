 /*
 *  AppleOnboardAudio.h
 *  AppleOnboardAudio
 *
 *  Created by cerveau on Mon Jun 04 2001.
 *  Copyright (c) 2001 Apple Computer Inc. All rights reserved.
 *
 */

#include <IOKit/IOCommandGate.h>
#include <IOKit/IOMessage.h>
#include <IOKit/pwr_mgt/RootDomain.h>
#include <IOKit/IOTimerEventSource.h>
#include "IOKit/audio/IOAudioDevice.h"

#include "AppleOnboardAudio.h"
#include "AudioHardwareUtilities.h"
#include "AudioHardwareObjectInterface.h"

#include "PlatformFactory.h"
#include "PlatformInterface.h"
#include "TransportFactory.h"

OSDefineMetaClassAndStructors(AppleOnboardAudio, IOAudioDevice)

UInt32 AppleOnboardAudio::sInstanceCount = 0;

#define super IOAudioDevice

#define LOCALIZABLE 1

// Neoborg bringup defines
#define FORCE_MULTIPLE_DEVICES_TO_LOAD
//#define ONLY_PUBLISH_ONE_BUILIT_DEVICE
#define DONT_THREAD_INIT

#pragma mark +UNIX LIKE FUNCTIONS

bool AppleOnboardAudio::init (OSDictionary *properties)
{
    debugIOLog (3, "+ AppleOnboardAudio[%p]::init", this);

    if (!super::init(properties)) return false;
        
    currentDevices = 0xFFFF;
    
	mHasHardwareInputGain = true;	// aml 5.10.02
	
	mInternalMicDualMonoMode = e_Mode_Disabled;	// aml 6.17.02, turn off by default
	
	mSampleRateSelectInProcessSemaphore = false;
	mClockSelectInProcessSemaphore = false;

	mCurrentProcessingOutputString = OSString::withCString ("none");
	mCurrentProcessingInputString = OSString::withCString ("none");

	mUCState.ucPowerState = kIOAudioDeviceActive;

    debugIOLog (3, "- AppleOnboardAudio[%p]::init", this);
    return true;
}

bool AppleOnboardAudio::start (IOService * provider) {
	bool								result;

    debugIOLog (3, "+ AppleOnboardAudio[%ld]::start (%p)", mInstanceIndex, provider);
	result = FALSE;

	AppleOnboardAudio::sInstanceCount++;
	
	mInstanceIndex = AppleOnboardAudio::sInstanceCount;

	mProvider = provider;
	provider->open (this);

	mPowerThread = thread_call_allocate ((thread_call_func_t)AppleOnboardAudio::performPowerStateChangeThread, (thread_call_param_t)this);
	FailIf (NULL == mPowerThread, Exit);

	mInitHardwareThread = thread_call_allocate ((thread_call_func_t)AppleOnboardAudio::initHardwareThread, (thread_call_param_t)this);
	FailIf (NULL == mInitHardwareThread, Exit);

    debugIOLog (3, "- AppleOnboardAudio[%ld]::start (%p)", mInstanceIndex, provider);

	result = super::start (provider);				// Causes our initHardware routine to be called.

Exit:
	return result;
}

bool AppleOnboardAudio::handleOpen (IOService * forClient, IOOptionBits options, void *	arg ) 
{
	bool							result;

	debugIOLog (3, "AppleOnboardAudio::handleOpen(%p)", forClient);

	if (kFamilyOption_OpenMultiple == options) {
		result = true;
	} else {
		result = super::handleOpen ( forClient, options, arg );
		FailIf (!result, Exit);
	}

	registerPlugin ( (AudioHardwareObjectInterface *)forClient );

Exit:
	debugIOLog (3, "AppleOnboardAudio::handleOpen(%p) returns %s", forClient, result == true ? "true" : "false");
	return result;
}

void AppleOnboardAudio::handleClose (IOService * forClient, IOOptionBits options ) 
{
	debugIOLog (3, "AppleOnboardAudio::handleClose(%p)", forClient);

	unRegisterPlugin ( (AudioHardwareObjectInterface *)forClient );

	if (options != kFamilyOption_OpenMultiple) {
		super::handleClose ( forClient, options );
	}
	
	return;
}

bool AppleOnboardAudio::willTerminate ( IOService * provider, IOOptionBits options )
{
#if 0
	UInt32 count;
	UInt32 index;
	AudioHardwareObjectInterface* thePluginObject;
	
	if (NULL != mPluginObjects) {
		count = mPluginObjects->getCount ();
		for (index = 0; index < count; index++) {
			thePluginObject = getIndexedPluginObject (index);
			if((NULL != thePluginObject)) {
				thePluginObject->terminate();
				mPluginObjects->removeObject(index);
				debugIOLog (3, "AppleOnboardAudio::willTerminate terminated  (%p)", thePluginObject);
			}
		}
	}
#endif
	
	bool result = super::willTerminate ( provider, options );

	provider->close(this);

	debugIOLog (3, "AppleOnboardAudio::willTerminate(%p) returns %d", provider, result);

	return result;
}

// Called by the plugin objects from their start() method so that AppleOnboardAudio knows about them and will call them as needed.
void AppleOnboardAudio::registerPlugin (AudioHardwareObjectInterface *thePlugin) {
//	IOCommandGate *				cg;

	debugIOLog (3, "AppleOnboardAudio::registerPlugin (%p)", thePlugin);
/*	cg = getCommandGate ();
	if (NULL != cg) {
		cg->runAction (registerPluginAction, thePlugin);
	} else {
		debugIOLog (3, "no command gate for registering a plugin!");
	}*/
	
	FailIf ( NULL == mPluginObjects, Exit );
	mPluginObjects->setObject (thePlugin);

Exit:
	return;
}

void AppleOnboardAudio::stop (IOService * provider) {

	if ( NULL != aoaNotifier ) {
		aoaNotifier->remove();
	}
	
	FailIf ( mProvider != provider, Exit );
	if (NULL != mSysPowerDownNotifier) {
		mSysPowerDownNotifier->remove ();
		mSysPowerDownNotifier = NULL;
	}

	mTerminating = TRUE;
	
	if (idleTimer) {
		if (workLoop) {
			workLoop->removeEventSource (idleTimer);
		}

		idleTimer->release ();
		idleTimer = NULL;
	}
	
	if (mSoftwareInterruptHandler) {
		if (workLoop) {
			workLoop->removeEventSource (mSoftwareInterruptHandler);
			mSoftwareInterruptHandler->release ();
		}
	}
	
	if ( pollTimer ) {
		if ( workLoop ) {
			workLoop->removeEventSource ( pollTimer );
		}

		pollTimer->release ();
		pollTimer = NULL;
	}

	// [3253678]
	callPluginsInOrder ( kSetMuteState, TRUE );						//	[3435307]	rbm
	
	if (mPowerThread) {
		thread_call_cancel (mPowerThread);
	}

	debugIOLog (3, "AppleOnboardAudio::stop(), audioEngines = %p, isInactive() = %d", audioEngines, isInactive());
Exit:
	super::stop (provider);
}

void AppleOnboardAudio::unRegisterPlugin (AudioHardwareObjectInterface *inPlugin) {
	AudioHardwareObjectInterface *		thePluginObject;
	UInt32								index;
	UInt32								count;

	debugIOLog (3, "AppleOnboardAudio::unRegisterPlugin (%p)", inPlugin);

	if (NULL != mPluginObjects) {
		count = mPluginObjects->getCount ();
		for (index = 0; index < count; index++) {
			thePluginObject = getIndexedPluginObject (index);
			if ((NULL != thePluginObject) && (inPlugin == thePluginObject)) {
				mPluginObjects->removeObject(index);
				debugIOLog (3, "removed  (%p)", inPlugin);
			}
		}
	}

	return;
}

IOReturn AppleOnboardAudio::registerPluginAction (OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4) {
	AppleOnboardAudio *			device;

	device = OSDynamicCast (AppleOnboardAudio, owner);
	debugIOLog (3, "AppleOnboardAudio::plugin %p registering", arg1);

	if (NULL == device->mPluginObjects) {
		device->mPluginObjects = OSArray::withCapacity (1);
	}

	FailIf (NULL == device->mPluginObjects, Exit);
	device->mPluginObjects->setObject ((AudioHardwareObjectInterface *)arg1);

Exit:
	return kIOReturnSuccess;
}

OSObject * AppleOnboardAudio::getLayoutEntry (const char * entryID, AppleOnboardAudio * theAOA) {
	OSArray *							layouts;
	OSDictionary *						layoutEntry;
	OSObject *							entry;

	entry = NULL;

	layouts = OSDynamicCast (OSArray, theAOA->getProperty (kLayouts));
	FailIf (NULL == layouts, Exit);

#ifdef REMOVE_LAYOUTS
	layoutEntry = OSDynamicCast (OSDictionary, layouts->getObject (0));
#else
	layoutEntry = OSDynamicCast (OSDictionary, layouts->getObject (mMatchingIndex));
#endif

	FailIf (NULL == layoutEntry, Exit);

	entry = OSDynamicCast (OSObject, layoutEntry->getObject (entryID));

Exit:
	return entry;
}

bool AppleOnboardAudio::hasMasterVolumeControl (const UInt32 inCode) {
	char *			connectionString;
	
	connectionString = getConnectionKeyFromCharCode (inCode, kIOAudioStreamDirectionOutput);
	return hasMasterVolumeControl (connectionString);
}

bool AppleOnboardAudio::hasMasterVolumeControl (const char * outputEntry) {
	OSDictionary *					dictEntry;
	OSArray *						controlsArray;
	OSString *						controlString;
	UInt32							controlsCount;
	UInt32							index;
	bool							hasMasterVolControl;

	hasMasterVolControl = FALSE;

	dictEntry = OSDynamicCast (OSDictionary, getLayoutEntry (outputEntry, this));
	FailIf (NULL == dictEntry, Exit);
	controlsArray = OSDynamicCast (OSArray, dictEntry->getObject (kControls));
	FailIf (NULL == controlsArray, Exit);
	controlsCount = controlsArray->getCount ();
	for (index = 0; index < controlsCount; index++) {
		controlString = OSDynamicCast (OSString, controlsArray->getObject (index));
		if ((NULL != controlString) && controlString->isEqualTo (kMasterVolControlString)) {
			hasMasterVolControl = TRUE;
		}
	}

Exit:
	return hasMasterVolControl;
}

bool AppleOnboardAudio::hasLeftVolumeControl (const UInt32 inCode) {
	char *			connectionString;
	
	connectionString = getConnectionKeyFromCharCode (inCode, kIOAudioStreamDirectionOutput);
	return hasLeftVolumeControl (connectionString);
}

bool AppleOnboardAudio::hasLeftVolumeControl (const char * outputEntry) {
	OSDictionary *					dictEntry;
	OSArray *						controlsArray;
	OSString *						controlString;
	UInt32							controlsCount;
	UInt32							index;
	bool							hasLeftVolControl;

	hasLeftVolControl = FALSE;

	dictEntry = OSDynamicCast (OSDictionary, getLayoutEntry (outputEntry, this));
	FailIf (NULL == dictEntry, Exit);
	controlsArray = OSDynamicCast (OSArray, dictEntry->getObject (kControls));
	FailIf (NULL == controlsArray, Exit);
	controlsCount = controlsArray->getCount ();
	for (index = 0; index < controlsCount; index++) {
		controlString = OSDynamicCast (OSString, controlsArray->getObject (index));
		if ((NULL != controlString) && controlString->isEqualTo (kLeftVolControlString)) {
			hasLeftVolControl = TRUE;
		}
	}

Exit:
	return hasLeftVolControl;
}

bool AppleOnboardAudio::hasRightVolumeControl (const UInt32 inCode) {
	char *			connectionString;
	
	connectionString = getConnectionKeyFromCharCode (inCode, kIOAudioStreamDirectionOutput);
	return hasRightVolumeControl (connectionString);
}

bool AppleOnboardAudio::hasRightVolumeControl (const char * outputEntry) {
	OSDictionary *					dictEntry;
	OSArray *						controlsArray;
	OSString *						controlString;
	UInt32							controlsCount;
	UInt32							index;
	bool							hasRightVolControl;

	hasRightVolControl = FALSE;

	dictEntry = OSDynamicCast (OSDictionary, getLayoutEntry (outputEntry, this));
	FailIf (NULL == dictEntry, Exit);
	controlsArray = OSDynamicCast (OSArray, dictEntry->getObject (kControls));
	FailIf (NULL == controlsArray, Exit);
	controlsCount = controlsArray->getCount ();
	for (index = 0; index < controlsCount; index++) {
		controlString = OSDynamicCast (OSString, controlsArray->getObject (index));
		if ((NULL != controlString) && controlString->isEqualTo (kRightVolControlString)) {
			hasRightVolControl = TRUE;
		}
	}

Exit:
	return hasRightVolControl;
}

void AppleOnboardAudio::setUseInputGainControls (const char * inputEntry) {
	OSDictionary *					dictEntry;
	OSArray *						controlsArray;
	OSString *						controlString;
	UInt32							controlsCount;
	UInt32							index;

	debugIOLog (3, "AppleOnboardAudio::setUseInputGainControls(%s)", inputEntry);
	mUseInputGainControls = kNoInputGainControls;

	dictEntry = OSDynamicCast (OSDictionary, getLayoutEntry (inputEntry, this));
	FailIf (NULL == dictEntry, Exit);

	controlsArray = OSDynamicCast (OSArray, dictEntry->getObject (kControls));
	FailIf (NULL == controlsArray, Exit);

	controlsCount = controlsArray->getCount ();
	for (index = 0; index < controlsCount; index++) {
		controlString = OSDynamicCast (OSString, controlsArray->getObject (index));
		if ((NULL != controlString) && (controlString->isEqualTo (kLeftVolControlString) || controlString->isEqualTo (kRightVolControlString))) {
			mUseInputGainControls = kStereoInputGainControls;
			debugIOLog (3, "mUseInputGainControls = kStereoInputGainControls");
		} else if ((NULL != controlString) && (controlString->isEqualTo (kMasterVolControlString))) {
			mUseInputGainControls = kMonoInputGainControl;
			debugIOLog (3, "mUseInputGainControls = kMonoInputGainControl");
		}
	}
Exit:
	return;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	[3281535]	begin {
void AppleOnboardAudio::setUsePlaythroughControl (const char * inputEntry) {
	OSDictionary *					dictEntry;
	OSBoolean *						playthroughOSBoolean;

	debugIOLog (3,  "AppleOnboardAudio::setUsePlaythroughControl(%s)", inputEntry );
	mUsePlaythroughControl = FALSE;

	dictEntry = OSDynamicCast ( OSDictionary, getLayoutEntry ( inputEntry, this) );
	FailIf ( NULL == dictEntry, Exit );

	playthroughOSBoolean = OSDynamicCast ( OSBoolean, dictEntry->getObject ( kPlaythroughControlString ) );
	FailIf ( NULL == playthroughOSBoolean, Exit );

	mUsePlaythroughControl = playthroughOSBoolean->getValue ();
Exit:
	debugIOLog (3,  "mUsePlaythroughControl = %d", mUsePlaythroughControl );
	return;
}
//	[3281535]	} end
			
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleOnboardAudio::formatChangeRequest (const IOAudioStreamFormat * inFormat, const IOAudioSampleRate * inRate) {
	IOReturn							result;
	OSNumber *							connectionCodeNumber;
	
	result = kIOReturnError;

	callPluginsInOrder ( kSetMuteState, TRUE );						//	[3253678], [3435307]	rbm

	if (NULL != inFormat) {
		debugIOLog (3, "AppleOnboardAudio::formatChangeRequest with bit width %d", inFormat->fBitWidth);
		result = mTransportInterface->transportSetSampleWidth (inFormat->fBitDepth, inFormat->fBitWidth);
		callPluginsInOrder (kSetSampleBitDepth, inFormat->fBitDepth);
		if (kIOAudioStreamSampleFormat1937AC3 == inFormat->fSampleFormat) {
			if (NULL != mOutputSelector) {
				connectionCodeNumber = OSNumber::withNumber (kIOAudioOutputPortSubTypeSPDIF, 32);
				mOutputSelector->setValue (connectionCodeNumber);
				mEncodedOutputFormat = true;
			}
		} else {
			mEncodedOutputFormat = false;		
		}
		result = callPluginsInOrder ( kSetSampleType, inFormat->fSampleFormat );
	}
	if (NULL != inRate) {
		mSampleRateSelectInProcessSemaphore = true;
		
		debugIOLog (3, "AppleOnboardAudio::formatChangeRequest with rate %ld", inRate->whole);
		result = mTransportInterface->transportSetSampleRate (inRate->whole);
		callPluginsInOrder (kSetSampleRate, inRate->whole);
		
		//	Maintain a member copy of the sample rate so that changes in sample rate
		//	when running on an externally clocked sample rate can be detected so that
		//	the HAL can be notified of sample rate changes.
		
		if ( kIOReturnSuccess == result ) {
			mTransportSampleRate.whole = inRate->whole;
			mTransportSampleRate.fraction = inRate->fraction;
			mSampleRateSelectInProcessSemaphore = false;
		}
	}

	// [3253678], [3435307]	Don't change amplifier mutes in response to UI.	RBM
	if ( NULL != mOutputSelector ) {
		selectOutput (mOutputSelector->getIntValue (), mIsMute);
	}
	return result;
}

IOReturn AppleOnboardAudio::callPluginsInReverseOrder (UInt32 inSelector, UInt32 newValue) {
	AudioHardwareObjectInterface *		thePluginObject;
	OSArray *							pluginOrderArray;
	OSString *							pluginName;
	SInt32								index;
	UInt32								pluginOrderArrayCount;
	IOReturn							result;

	debugIOLog (3, "AppleOnboardAudio::callPluginsInReverseOrder (%lX, %lX)", inSelector, newValue);
	result = kIOReturnBadArgument;

	pluginOrderArray = OSDynamicCast (OSArray, getLayoutEntry (kPluginRecoveryOrder, this));
	FailIf (NULL == pluginOrderArray, Exit);

	pluginOrderArrayCount = pluginOrderArray->getCount ();

	for (index = pluginOrderArrayCount - 1; index >= 0; index--) {
		pluginName = OSDynamicCast(OSString, pluginOrderArray->getObject(index));
		if (NULL == pluginName) {
			debugIOLog (3, "Corrupt %s entry in AppleOnboardAudio Info.plist", kPluginRecoveryOrder);
			continue;
		}
		thePluginObject = getPluginObjectWithName (pluginName);
		if (NULL == thePluginObject) {
			debugIOLog (3, "Can't find required AppleOnboardAudio plugin from %s entry loaded", kPluginRecoveryOrder);
			continue;
		}

		switch (inSelector) {
			case kRequestCodecRecoveryStatus:
				result = thePluginObject->recoverFromFatalError ((FatalRecoverySelector)newValue);
				break;
			case kBreakClockSelect:
				result = thePluginObject->breakClockSelect (newValue);
				break;
			case kMakeClockSelect:
				result = thePluginObject->makeClockSelect (newValue);
				break;
			case kSetSampleRate:
				result = thePluginObject->setSampleRate (newValue);
				break;
			case kSetSampleBitDepth:
				result = thePluginObject->setSampleDepth (newValue);
				break;
			case kPowerStateChange:
				if (kIOAudioDeviceSleep == newValue) {
					result = thePluginObject->performDeviceSleep ();
				} else if (kIOAudioDeviceActive == newValue) {
					result = thePluginObject->performDeviceWake ();
				} else if (kIOAudioDeviceIdle == newValue) {
					// thePluginObject->performDeviceIdle ();			// Not implemented because no hardware supports it
				}
				break;
			default:
				break;
		}		
	}

Exit:
	return result;
}

IOReturn AppleOnboardAudio::callPluginsInOrder (UInt32 inSelector, UInt32 newValue) {
	AudioHardwareObjectInterface *		thePluginObject;
	OSArray *							pluginOrderArray;
	OSString *							pluginName;
	UInt32								index;
	UInt32								pluginOrderArrayCount;
	IOReturn							tempResult;
	IOReturn							result;
	bool								boolResult;
	
	debugIOLog (7, "AppleOnboardAudio::callPluginsInOrder (%lX, %lX)", inSelector, newValue);

	result = kIOReturnBadArgument;

	pluginOrderArray = OSDynamicCast (OSArray, getLayoutEntry (kPluginRecoveryOrder, this));
	FailIf (NULL == pluginOrderArray, Exit);

	pluginOrderArrayCount = pluginOrderArray->getCount ();

	result = kIOReturnSuccess;
	for (index = 0; index < pluginOrderArrayCount; index++) {
		pluginName = OSDynamicCast(OSString, pluginOrderArray->getObject(index));
		if (NULL == pluginName) {
			debugIOLog (3, "Corrupt %s entry in AppleOnboardAudio Info.plist", kPluginRecoveryOrder);
			continue;
		}
		thePluginObject = getPluginObjectWithName (pluginName);
		if (NULL == thePluginObject) {
			debugIOLog (3, "Can't find required AppleOnboardAudio plugin from %s entry loaded", kPluginRecoveryOrder);
			continue;
		}

		tempResult = kIOReturnSuccess;
		switch (inSelector) {
			case kRequestCodecRecoveryStatus:
				tempResult = thePluginObject->recoverFromFatalError ((FatalRecoverySelector)newValue);
				break;
			case kClockInterruptedRecovery:
				tempResult = thePluginObject->recoverFromFatalError ((FatalRecoverySelector)newValue);
				break;
			case kBreakClockSelect:
				tempResult = thePluginObject->breakClockSelect (newValue);
				break;
			case kMakeClockSelect:
				tempResult = thePluginObject->makeClockSelect (newValue);
				break;
			case kSetSampleRate:
				tempResult = thePluginObject->setSampleRate (newValue);
				break;
			case kSetSampleBitDepth:
				tempResult = thePluginObject->setSampleDepth (newValue);
				break;
			case kSetSampleType:
				tempResult = thePluginObject->setSampleType ( newValue );
				break;
			case kPreDMAEngineInit:
				boolResult = thePluginObject->preDMAEngineInit ();
				if ( !boolResult ) {
					debugIOLog (1, "AppleOnboardAudio::callPluginsInOrder: preDMAEngineInit	on thePluginObject %p failed!\n", thePluginObject);	
					mPluginObjects->removeObject ( index );
					index--;
					tempResult = kIOReturnError;
				}
				break;
			case kPostDMAEngineInit:
				boolResult = thePluginObject->postDMAEngineInit ();
				if ( !boolResult ) {
					debugIOLog (1, "AppleOnboardAudio::callPluginsInOrder: postDMAEngineInit on thePluginObject %p failed!\n", thePluginObject);	
					mPluginObjects->removeObject ( index );
					index--;
					tempResult = kIOReturnError;
				}
				break;
			case kPowerStateChange:
				if (kIOAudioDeviceSleep == newValue) {
					debugIOLog (3, "AppleOnboardAudio::callPluginsInOrder ### Telling %s to sleep", pluginName->getCStringNoCopy ());
					tempResult = thePluginObject->performDeviceSleep ();
				} else if (kIOAudioDeviceActive == newValue) {
					debugIOLog (3, "AppleOnboardAudio::callPluginsInOrder ### Telling %s to wake", pluginName->getCStringNoCopy ());
					tempResult = thePluginObject->performDeviceWake ();
				} else if (kIOAudioDeviceIdle == newValue) {
					// thePluginObject->performDeviceIdle ();			// Not implemented because no hardware supports it
				}
				break;
			case kCodecErrorInterruptStatus:
				//	CAUTION:	Calling the plugin here may result in a nested call back to 'callPluginsInOrder'!!!
				//				Nested calls to 'callPluginsInOrder' must not pass the same 'inSelector' value.
				thePluginObject->notifyHardwareEvent ( inSelector, newValue );
				break;
			case kCodecInterruptStatus:
				//	CAUTION:	Calling the plugin here may result in a nested call back to 'callPluginsInOrder'!!!
				//				Nested calls to 'callPluginsInOrder' must not pass the same 'inSelector' value.
				thePluginObject->notifyHardwareEvent ( inSelector, newValue );
				break;
			case kRunPollTask:
				thePluginObject->poll ();
				break;
			case kSetMuteState:
				debugIOLog (3,  "callPluginsInOrder calls thePluginObject->setMute ( %ld )", newValue );
				tempResult = thePluginObject->setMute ( newValue );
				break;
			case kSetAnalogMuteState:			//	[3435307]	
				debugIOLog (3,  "callPluginsInOrder calls thePluginObject->setMute ( %ld, kAnalogAudioSelector )", newValue );
				tempResult = thePluginObject->setMute ( newValue, kAnalogAudioSelector );
				break;
			case kSetDigitalMuteState:			//	[3435307]	
				debugIOLog (3,  "callPluginsInOrder calls thePluginObject->setMute ( %ld, kDigitalAudioSelector )", newValue );
				tempResult = thePluginObject->setMute ( newValue, kDigitalAudioSelector );
				break;
			default:
				tempResult = kIOReturnBadArgument;
				break;
		}
		//	If any plugin returned an error, make sure that an error is returned (see 'runPolledTasks() ) so
		//	that a plugin called later successfully will not cause a kIOReturnSuccess.  This is necessary for
		//	error processing outside of this method.
		if ( kIOReturnSuccess != tempResult ) {
			result = tempResult;
		}	
	}		
	
Exit:
	return result;
}	


// --------------------------------------------------------------------------
//	Iterates through the plugin objects, asking each plugin object what
//	type (i.e. digital or analog) that the plugin is.  When a plugin of
//	the type requested is found then return the object that is that plugin.
AudioHardwareObjectInterface *	AppleOnboardAudio::findPluginForType ( HardwarePluginType pluginType ) {
	AudioHardwareObjectInterface *		thePluginObject;
	AudioHardwareObjectInterface *		result;
	OSArray *							pluginOrderArray;
	OSString *							pluginName;
	UInt32								index;
	UInt32								pluginOrderArrayCount;
	
//	debugIOLog (3,  "+ AppleOnboardAudio::findPluginForType (%d )", (unsigned int)pluginType );

	result = NULL;
	pluginOrderArray = OSDynamicCast (OSArray, getLayoutEntry (kPluginRecoveryOrder, this));
	FailIf (NULL == pluginOrderArray, Exit);

	pluginOrderArrayCount = pluginOrderArray->getCount ();

	for (index = 0; index < pluginOrderArrayCount && NULL == result; index++) {
		thePluginObject = NULL;
		pluginName = OSDynamicCast(OSString, pluginOrderArray->getObject(index));
		if ( NULL != pluginName ) {
			thePluginObject = getPluginObjectWithName (pluginName);
			if ( NULL != thePluginObject ) {
				if ( pluginType == thePluginObject->getPluginType() ) {
					result = thePluginObject;
				}
			}
		}
	}
Exit:
//	debugIOLog (3,  "- AppleOnboardAudio::findPluginForType (%d ) returns %p", (unsigned int)pluginType, result );
	return result;
}


//	--------------------------------------------------------------------------------
void AppleOnboardAudio::setPollTimer () {
    AbsoluteTime				fireTime;
    UInt64						nanos;

	if ( pollTimer ) {
		clock_get_uptime (&fireTime);
		absolutetime_to_nanoseconds (fireTime, &nanos);
		nanos += NSEC_PER_SEC;
		nanoseconds_to_absolutetime (nanos, &fireTime);
		pollTimer->wakeAtTime (fireTime);
	}
}

//	--------------------------------------------------------------------------------
void AppleOnboardAudio::pollTimerCallback ( OSObject *owner, IOTimerEventSource *device ) {
	AppleOnboardAudio *			audioDevice;
	
	audioDevice = OSDynamicCast ( AppleOnboardAudio, owner );
	FailIf ( NULL == audioDevice, Exit );
	audioDevice->runPolledTasks ();
Exit:
	return;
}


//	--------------------------------------------------------------------------------
//	The AppleOnboardAudio object has a polled timer task that is used to provide
//	periodic service of objects owned by AppleOnboardAudio.  An object can obtain
//	periodic service by implementing the 'poll()' method.
void AppleOnboardAudio::runPolledTasks ( void ) {
	IOReturn			err;
	UInt32				errorCount = 0;
	
	//	[3326541]	begin {	
	//	Polling is only allowed while the audio driver is not in a sleep state.
	if ( kIOAudioDeviceActive == ourPowerState ) {
		if ( !mClockSelectInProcessSemaphore && !mSampleRateSelectInProcessSemaphore ) {
		
			//	Ask the transport interface for the current sample rate and post
			//	that sample rate to the IOAudioEngine when the rate changes when
			//	running in slaved clock mode an not in the process of changing the
			//	clock select control.
			
			IOAudioSampleRate		transportSampleRate;
			transportSampleRate.whole = mTransportInterface->transportGetSampleRate ();
			transportSampleRate.fraction = 0;
	
			if ( 0 != transportSampleRate.whole ) {
				if ( mTransportSampleRate.whole != transportSampleRate.whole || mTransportSampleRate.fraction != transportSampleRate.fraction ) {
					do {
						err = callPluginsInOrder ( kSetSampleRate, transportSampleRate.whole );
						if ( kIOReturnSuccess != err ) {
							IOSleep(1);
							errorCount++;
						}
					} while ( ( errorCount < 3 ) && ( kIOReturnSuccess != err ) );
					
					//	If an externally clocked sample rate change is valid for all of the
					//	hardware plugin objects then retain the new sample rate and notify
					//	the HAL of the new operating sample rate.  If the sample rate change
					//	associated with the externally clocked sample rate is not valid for 
					//	any of the hardware plugin objects then switch back to the internal
					//	clock to retain a valid sample rate for the hardware.  The hardware
					//	plugin objects should always be set to the current sample rate regardless
					//	whether the sample rate is internally or externally clocked.
					
					if ( kIOReturnSuccess == err ) {
						mTransportSampleRate.whole = transportSampleRate.whole;
						mTransportSampleRate.fraction = transportSampleRate.fraction;
						mDriverDMAEngine->hardwareSampleRateChanged ( &mTransportSampleRate );
					} else {
						//	Set the hardware to MASTER mode.
						clockSelectorChanged ( kClockSourceSelectionInternal );
						if ( NULL != mExternalClockSelector ) {
							//	Flush the control value (i.e. MASTER = internal).
							OSNumber *			clockSourceSelector;
							clockSourceSelector = OSNumber::withNumber (kClockSourceSelectionInternal, 32);
		
							mExternalClockSelector->hardwareValueChanged (clockSourceSelector);
			
							err = callPluginsInOrder ( kSetSampleRate, mTransportSampleRate.whole );
						}
					}
				}
			}
			//	[3305011]	begin {
			//		If the DMA engine dies with no indication of a hardware error then
			//		recovery must be performed by stopping and starting the engine.
			if ( ( kTRANSPORT_MASTER_CLOCK != mTransportInterface->transportGetClockSelect() ) && mDriverDMAEngine->engineDied() ) {
				ConfigChangeHelper theConfigeChangeHelper(mDriverDMAEngine);	
				
				UInt32 currentBitDepth = mTransportInterface->transportGetSampleWidth();
				mTransportInterface->transportSetSampleWidth ( currentBitDepth, mTransportInterface->transportGetDMAWidth() );
				callPluginsInOrder ( kSetSampleBitDepth, currentBitDepth );
			}
			//	} end	[3305011]
		}
		
		//	Then give other objects requiring a poll to have an opportunity to execute.
		
		mPlatformInterface->poll ();
		mTransportInterface->poll ();
		callPluginsInOrder ( kRunPollTask, 0 );
	}	//	} end	[3326541]
	
	setPollTimer ();
}


// --------------------------------------------------------------------------
//	Returns a target output selection for the current detect states as
//	follows:
//
//									Line Out	Headphone	External
//									Detect		Detect		Speaker
//															Detect
//									--------	---------	--------
//	Internal Speaker				out			out			out
//	External Speaker				out			out			in
//	Headphone						out			in			out
//	Headphone						out			in			in
//	Line Output						in			out			out
//	Line Output						in			out			in
//	Line Output & headphone			in			in			out
//	Line Output & headphone			in			in			in
//
UInt32 AppleOnboardAudio::parseOutputDetectCollection ( void ) {
	UInt32		result;
	
	debugIOLog (3,  "+ AppleOnboardAudio::ParseDetectCollection() detectCollection 0x%lX", mDetectCollection );

	if ( mDetectCollection & kSndHWLineOutput ) {
		result = kIOAudioOutputPortSubTypeLine;
	} else if ( mDetectCollection & kSndHWCPUHeadphone ) {
		result = kIOAudioOutputPortSubTypeHeadphones;
	} else if ( mDetectCollection & kSndHWCPUExternalSpeaker ) {
		result = kIOAudioOutputPortSubTypeExternalSpeaker;
	} else {
		result = kIOAudioOutputPortSubTypeInternalSpeaker;
	}

	debugIOLog (3,  "- AppleOnboardAudio::ParseDetectCollection returns %4s", (char*)&result );
	return result;
}

UInt32 AppleOnboardAudio::parseInputDetectCollection ( void ) {
	UInt32		result;
	
	debugIOLog (3,  "+ AppleOnboardAudio::parseInputDetectCollection() detectCollection 0x%lX", mDetectCollection );

	if ( mDetectCollection & kSndHWLineInput ) {
		result = kIOAudioOutputPortSubTypeLine;
	} else {
		result = kIOAudioInputPortSubTypeInternalMicrophone;
	}

	debugIOLog (3,  "- AppleOnboardAudio::parseInputDetectCollection returns %lX", result );
	return result;
}

void AppleOnboardAudio::initializeDetectCollection ( void ) {
	debugIOLog (3,  "+ AppleOnboardAudio::initializeDetectCollection() " );

	if ( kGPIO_Connected == mPlatformInterface->getHeadphoneConnected() ) {
		mDetectCollection |= kSndHWCPUHeadphone;
		debugIOLog (3,  "AppleOnboardAudio::initializeDetectCollection() - headphones are connected." );
	} 
	if ( kGPIO_Connected == mPlatformInterface->getLineOutConnected() ) {
		mDetectCollection |= kSndHWLineOutput;
	} 
	if ( kGPIO_Connected == mPlatformInterface->getDigitalOutConnected() ) {
		debugIOLog (3,  "AppleOnboardAudio::initializeDetectCollection() - digital out is connected." );
		mDetectCollection |= kSndHWDigitalOutput;
	} 
	if ( kGPIO_Connected == mPlatformInterface->getLineInConnected() ) {
		mDetectCollection |= kSndHWLineInput;
	} 
	if ( kGPIO_Connected == mPlatformInterface->getDigitalInConnected() ) {
		mDetectCollection |= kSndHWDigitalInput;
	} 
	if ( kGPIO_Connected == mPlatformInterface->getSpeakerConnected() ) {	//	[3398729]	start	{
		debugIOLog (3,  "AppleOnboardAudio::initializeDetectCollection() - external speakers are connected." );
		mDetectCollection |= kSndHWCPUExternalSpeaker;
		mDetectCollection &= ~kSndHWInternalSpeaker;
	} else/* if ( kGPIO_Disconnected == mPlatformInterface->getSpeakerConnected() )*/ {
		debugIOLog (3,  "AppleOnboardAudio::initializeDetectCollection() - in internal speakers are connected." );
		mDetectCollection |= kSndHWInternalSpeaker;
		mDetectCollection &= ~kSndHWCPUExternalSpeaker;
	}																		//	}	end	[3398729]
	
	debugIOLog (3,  "- AppleOnboardAudio::initializeDetectCollection() %lX", mDetectCollection );
	return;
}

void AppleOnboardAudio::updateOutputDetectCollection (UInt32 statusSelector, UInt32 newValue) {
	switch (statusSelector) {
		case kHeadphoneStatus:
			debugIOLog (3,  "kHeadphoneStatus mDetectCollection prior to modification %lX", mDetectCollection );
			if (newValue == kInserted) {
				mDetectCollection |= kSndHWCPUHeadphone;
				mDetectCollection &= ~kSndHWInternalSpeaker;
				debugIOLog (3, "headphones inserted, mDetectCollection = %lX", mDetectCollection);
			} else if (newValue == kRemoved) {
				mDetectCollection &= ~kSndHWCPUHeadphone;
				mDetectCollection |= kSndHWInternalSpeaker;
				debugIOLog (3, "headphones removed, mDetectCollection = %lX", mDetectCollection);
			} else {
				debugIOLog (3, "Unknown headphone jack status, mDetectCollection = %lX", mDetectCollection);
			}
			break;
		case kLineOutStatus:
			debugIOLog (3,  "kLineOutStatus mDetectCollection prior to modification %lX", mDetectCollection );
			if (newValue == kInserted) {
				mDetectCollection |= kSndHWLineOutput;
				mDetectCollection &= ~kSndHWInternalSpeaker;
				debugIOLog (3, "line out inserted.");
				if ( ( NULL != mDigitalOutputString ) && ( NULL != mLineOutputString ) && (kGPIO_Unknown != mPlatformInterface->getComboOutJackTypeConnected ()) ) {
					ConfigChangeHelper theConfigeChangeHelper(mDriverDMAEngine);	
					mOutputSelector->removeAvailableSelection ( kIOAudioOutputPortSubTypeSPDIF );
					mOutputSelector->addAvailableSelection (kIOAudioOutputPortSubTypeLine, mLineOutputString);
				}
			} else if (newValue == kRemoved) {
				mDetectCollection &= ~kSndHWLineOutput;
				mDetectCollection |= kSndHWInternalSpeaker;
				debugIOLog (3, "line out removed.");
				if ( ( NULL != mDigitalOutputString ) && ( NULL != mLineOutputString ) && (kGPIO_Unknown != mPlatformInterface->getComboOutJackTypeConnected ()) ) {
					ConfigChangeHelper theConfigeChangeHelper(mDriverDMAEngine);	
					mOutputSelector->removeAvailableSelection ( kIOAudioOutputPortSubTypeLine );
					mOutputSelector->removeAvailableSelection ( kIOAudioOutputPortSubTypeSPDIF );
				}
			} else {
				debugIOLog (3, "Unknown line out jack status.");
			}
			break;
		case kDigitalOutStatus:
			debugIOLog (3,  "kDigitalOutStatus mDetectCollection prior to modification %lX", mDetectCollection );
			if (newValue == kInserted) {
				mDetectCollection |= kSndHWDigitalOutput;
				mDetectCollection &= ~kSndHWInternalSpeaker;
				debugIOLog (3, "digital out inserted.");
				if ( ( NULL != mDigitalOutputString ) && ( NULL != mLineOutputString ) && (kGPIO_Unknown != mPlatformInterface->getComboOutJackTypeConnected ()) ) {
					ConfigChangeHelper theConfigeChangeHelper(mDriverDMAEngine);	
					mOutputSelector->removeAvailableSelection ( kIOAudioOutputPortSubTypeLine );
					mOutputSelector->addAvailableSelection (kIOAudioOutputPortSubTypeSPDIF, mDigitalOutputString);
				}
			} else if (newValue == kRemoved) {
				mDetectCollection &= ~kSndHWDigitalOutput;
				mDetectCollection |= kSndHWInternalSpeaker;
				debugIOLog (3, "digital out removed.");
				if ( ( NULL != mDigitalOutputString ) && ( NULL != mLineOutputString ) && (kGPIO_Unknown != mPlatformInterface->getComboOutJackTypeConnected ()) ) {
					ConfigChangeHelper theConfigeChangeHelper(mDriverDMAEngine);	
					mOutputSelector->removeAvailableSelection ( kIOAudioOutputPortSubTypeLine );
					mOutputSelector->removeAvailableSelection ( kIOAudioOutputPortSubTypeSPDIF );
				}
			} else {
				debugIOLog (3, "Unknown digital out jack status.");
			}
			break;
		case kLineInStatus:
			debugIOLog (3,  "kLineInStatus mDetectCollection prior to modification %lX", mDetectCollection );
			if (newValue == kInserted) {
				mDetectCollection |= kSndHWLineInput;
			} else if (newValue == kRemoved) {
				mDetectCollection &= ~kSndHWLineInput;
			} else {
				debugIOLog (3, "Unknown line in status.");
			}
			break;
		case kExtSpeakersStatus:												//	[3398729]	begin	{
			debugIOLog (3,  "kExtSpeakersStatus mDetectCollection prior to modification %lX", mDetectCollection );
			if (newValue == kInserted) {
				mDetectCollection &= ~kSndHWInternalSpeaker;
				mDetectCollection |= kSndHWCPUExternalSpeaker;
				//	[3413551]	begin	{
				if ( ( NULL != mInternalSpeakerOutputString ) && ( NULL != mExternalSpeakerOutputString ) ) {
					ConfigChangeHelper theConfigeChangeHelper(mDriverDMAEngine);	
					mOutputSelector->removeAvailableSelection ( kIOAudioOutputPortSubTypeInternalSpeaker );
					mOutputSelector->addAvailableSelection (kIOAudioOutputPortSubTypeExternalSpeaker, mExternalSpeakerOutputString);
				}
				//	}	end		[3413551]
				debugIOLog (3, "external speakers inserted, mDetectCollection = %lX", mDetectCollection);
			} else if (newValue == kRemoved) {
				mDetectCollection &= ~kSndHWCPUExternalSpeaker;
				mDetectCollection |= kSndHWInternalSpeaker;
				//	[3413551]	begin	{
				if ( ( NULL != mInternalSpeakerOutputString ) && ( NULL != mExternalSpeakerOutputString ) ) {
					ConfigChangeHelper theConfigeChangeHelper(mDriverDMAEngine);	
					mOutputSelector->removeAvailableSelection ( kIOAudioOutputPortSubTypeExternalSpeaker );
					mOutputSelector->addAvailableSelection (kIOAudioOutputPortSubTypeInternalSpeaker, mInternalSpeakerOutputString);
				}
				//	}	end		[3413551]
				debugIOLog (3, "external speakers removed, mDetectCollection = %lX", mDetectCollection);
			} else {
				debugIOLog (3, "Unknown external speakers jack status, mDetectCollection = %lX", mDetectCollection);
			}
			break;																//	}	end	[3398729]
	}
}

UInt32  AppleOnboardAudio::getSelectorCodeForOutputEvent (UInt32 eventSelector) {
	UInt32 								selectorCode;

	selectorCode = kIOAudioOutputPortSubTypeInternalSpeaker;
	
	// [3279525] when exclusive, handle redirection
	if (kLineOutStatus == eventSelector) {
		if ( mDetectCollection & kSndHWLineOutput ) {
			selectorCode = kIOAudioOutputPortSubTypeLine;
		} else if ( mDetectCollection & kSndHWCPUHeadphone ) {
			selectorCode = kIOAudioOutputPortSubTypeHeadphones;
		} else if ( mDetectCollection & kSndHWDigitalOutput ) {
			selectorCode = kIOAudioOutputPortSubTypeSPDIF;
		} else if ( mDetectCollection & kSndHWCPUExternalSpeaker ) {		//	[3398729]	begin	{
			selectorCode = kIOAudioOutputPortSubTypeExternalSpeaker;
		} else if ( mDetectCollection & kSndHWInternalSpeaker ) {
			selectorCode = kIOAudioOutputPortSubTypeInternalSpeaker;
		} else {
			debugIOLog (3,  "UNKNOWN device so selecting INTERNAL SPEAKER" );
			selectorCode = kIOAudioOutputPortSubTypeInternalSpeaker;
			mDetectCollection |= kSndHWInternalSpeaker;
		}																	//	}	end	[3398729]
	} else if (kHeadphoneStatus == eventSelector) {
		if ( mDetectCollection & kSndHWCPUHeadphone ) {
			selectorCode = kIOAudioOutputPortSubTypeHeadphones;
		} else if ( mDetectCollection & kSndHWLineOutput ) {
			selectorCode = kIOAudioOutputPortSubTypeLine;
		} else if ( mDetectCollection & kSndHWDigitalOutput ) {
			selectorCode = kIOAudioOutputPortSubTypeSPDIF;
		} else if ( mDetectCollection & kSndHWCPUExternalSpeaker ) {		//	[3398729]	begin	{
			selectorCode = kIOAudioOutputPortSubTypeExternalSpeaker;
		} else if ( mDetectCollection & kSndHWInternalSpeaker ) {
			selectorCode = kIOAudioOutputPortSubTypeInternalSpeaker;
		} else {															//	}	end	[3398729]
			debugIOLog (3,  "UNKNOWN device so selecting INTERNAL SPEAKER" );
			selectorCode = kIOAudioOutputPortSubTypeInternalSpeaker;
			mDetectCollection |= kSndHWInternalSpeaker;
		}
	} else if (kExtSpeakersStatus == eventSelector) {						//	[3398729]	begin	{
		if ( mDetectCollection & kSndHWLineOutput ) {
			selectorCode = kIOAudioOutputPortSubTypeLine;
		} else if ( mDetectCollection & kSndHWCPUHeadphone ) {
			selectorCode = kIOAudioOutputPortSubTypeHeadphones;
		} else if ( mDetectCollection & kSndHWDigitalOutput ) {
			selectorCode = kIOAudioOutputPortSubTypeSPDIF;
		} else if ( mDetectCollection & kSndHWCPUExternalSpeaker ) {
			selectorCode = kIOAudioOutputPortSubTypeExternalSpeaker;
		} else if ( mDetectCollection & kSndHWInternalSpeaker ) {
			selectorCode = kIOAudioOutputPortSubTypeInternalSpeaker;
		} else {
			debugIOLog (3,  "UNKNOWN device so selecting INTERNAL SPEAKER" );
			selectorCode = kIOAudioOutputPortSubTypeInternalSpeaker;
			mDetectCollection |= kSndHWInternalSpeaker;
		}
	} else if (kDigitalOutStatus == eventSelector) {						//	[3398729]	begin	{
		if ( mDetectCollection & kSndHWDigitalOutput ) {
			selectorCode = kIOAudioOutputPortSubTypeSPDIF;
		} else if ( mDetectCollection & kSndHWLineOutput ) {
			selectorCode = kIOAudioOutputPortSubTypeLine;
		} else if ( mDetectCollection & kSndHWCPUHeadphone ) {
			selectorCode = kIOAudioOutputPortSubTypeHeadphones;
		} else if ( mDetectCollection & kSndHWCPUExternalSpeaker ) {
			selectorCode = kIOAudioOutputPortSubTypeExternalSpeaker;
		} else if ( mDetectCollection & kSndHWInternalSpeaker ) {
			selectorCode = kIOAudioOutputPortSubTypeInternalSpeaker;
		} else {
			debugIOLog (3,  "UNKNOWN device so selecting INTERNAL SPEAKER" );
			selectorCode = kIOAudioOutputPortSubTypeInternalSpeaker;
			mDetectCollection |= kSndHWInternalSpeaker;
		}
	}																		//	}	end	[3398729]
	debugIOLog (3,  "mDetectCollection %lX, selectorCode '%4s'", mDetectCollection, (char*)&selectorCode );

	return selectorCode;
}

void AppleOnboardAudio::selectOutput (const UInt32 inSelection, const bool inMuteState, const bool inUpdateAll)
{
	bool								needToWaitForAmps;
	
	debugIOLog (3,  "+ AppleOnboardAudio::selectOutput %4s, inMuteState = %d", (char *)&(inSelection), inMuteState );

	needToWaitForAmps = true;
	
	// [3253678] mute analog outs if playing encoded content, unmute only if not playing 
	// encoded content (eg. on jack inserts/removals)
	//	[3435307]	The amplifier mutes are associated with a port selection but the system mute
	//	state should not be applied to the amplifier mutes but only to the CODECs themselves.
	//	Amplifier mutes are only manipulated in response to a jack state change or power management
	//	tasks but are not manipulated in response to UI selections!!!	RBM
	
	switch (inSelection) {
		case kIOAudioOutputPortSubTypeHeadphones:
			debugIOLog (3,  "[AOA] switching amps to headphones.  mEncodedOutputFormat %d", mEncodedOutputFormat );
			mPlatformInterface->setSpeakerMuteState ( kGPIO_Muted );
			if (!mEncodedOutputFormat) {
				callPluginsInOrder ( kSetAnalogMuteState, inMuteState );				//	[3435307]	rbm
				mPlatformInterface->setHeadphoneMuteState ( kGPIO_Unmuted );
			}
			if (mHeadLineDigExclusive) {
				mPlatformInterface->setLineOutMuteState ( kGPIO_Muted );
			} else {
				if ((mDetectCollection & kSndHWLineOutput) && !mEncodedOutputFormat) {
					callPluginsInOrder ( kSetAnalogMuteState, inMuteState );			//	[3435307]	rbm
					mPlatformInterface->setLineOutMuteState ( kGPIO_Unmuted );
				}	
			}
			break;
		case kIOAudioOutputPortSubTypeLine:
			debugIOLog (3,  "[AOA] switching amps to line output.  mEncodedOutputFormat %d", mEncodedOutputFormat );
			mPlatformInterface->setSpeakerMuteState ( kGPIO_Muted );
			if (!mEncodedOutputFormat) {
				callPluginsInOrder ( kSetAnalogMuteState, inMuteState );				//	[3435307]	rbm
				mPlatformInterface->setLineOutMuteState ( kGPIO_Unmuted );
			}
			if (!mHeadLineDigExclusive) {
				if ((mDetectCollection & kSndHWCPUHeadphone) && !mEncodedOutputFormat) {
					callPluginsInOrder ( kSetAnalogMuteState, FALSE );				//	[3435307]	rbm
					mPlatformInterface->setHeadphoneMuteState ( kGPIO_Unmuted );
				}	
			}
			break;
		case kIOAudioOutputPortSubTypeExternalSpeaker:								//	[3398729]	fall through to kIOAudioOutputPortSubTypeInternalSpeaker
		case kIOAudioOutputPortSubTypeInternalSpeaker:
			debugIOLog (3,  "[AOA] switching amps to speakers.  mEncodedOutputFormat %d", mEncodedOutputFormat );
			if (!mEncodedOutputFormat) {
				callPluginsInOrder ( kSetAnalogMuteState, inMuteState );				//	[3435307]	rbm
				mPlatformInterface->setSpeakerMuteState ( kGPIO_Unmuted );
				// [3250195], don't want to get EQ on these outputs if we're on internal speaker.
				mPlatformInterface->setLineOutMuteState ( kGPIO_Muted );
				mPlatformInterface->setHeadphoneMuteState ( kGPIO_Muted );
			}
			break;
		case kIOAudioOutputPortSubTypeSPDIF:
			mPlatformInterface->setSpeakerMuteState ( kGPIO_Muted );
			callPluginsInOrder ( kSetDigitalMuteState, inMuteState );				//	[3435307]	rbm
			if (mEncodedOutputFormat) {
				callPluginsInOrder ( kSetAnalogMuteState, TRUE );					//	[3435307]	rbm
 				needToWaitForAmps = false;
			} else {
				if (inUpdateAll) {
					if ((mDetectCollection & kSndHWCPUHeadphone)  && !inMuteState) {
						callPluginsInOrder ( kSetAnalogMuteState, inMuteState );	//	[3435307]	rbm
						mPlatformInterface->setHeadphoneMuteState ( kGPIO_Unmuted );
						if ( mDetectCollection & kSndHWLineOutput && !mHeadLineDigExclusive) {
							mPlatformInterface->setLineOutMuteState ( kGPIO_Unmuted );
						}
					} else if (mDetectCollection & kSndHWLineOutput && !inMuteState) {
						callPluginsInOrder ( kSetAnalogMuteState, inMuteState );	//	[3435307]	rbm
						mPlatformInterface->setLineOutMuteState ( kGPIO_Unmuted );
					}
				}
			}
			break;
		default:
			debugIOLog (3, "Amplifier not changed, selection = %ld", inSelection);
			needToWaitForAmps = false;
			break;
	}
	if (needToWaitForAmps) {
		IOSleep (mAmpRecoveryMuteDuration);
	}
	debugIOLog (3,  "- AppleOnboardAudio::selectOutput %4s, inMuteState = %d", (char *)&(inSelection), inMuteState );
    return;
}

void AppleOnboardAudio::free()
{
    debugIOLog (3, "+ AppleOnboardAudio::free");
    
	removeTimerEvent ( this );

    if (mDriverDMAEngine) {
		debugIOLog (3, "  AppleOnboardAudio::free - mDriverDMAEngine retain count = %d", mDriverDMAEngine->getRetainCount());
        mDriverDMAEngine->release();
		mDriverDMAEngine = NULL;
	}
	if (NULL != mPowerThread) {
		thread_call_free (mPowerThread);
	}

	CLEAN_RELEASE(mOutMuteControl);
	CLEAN_RELEASE(mHeadphoneConnected);
	CLEAN_RELEASE(mOutLeftVolumeControl);
	CLEAN_RELEASE(mOutRightVolumeControl);
	CLEAN_RELEASE(mOutMasterVolumeControl);
	CLEAN_RELEASE(mInLeftGainControl);
	CLEAN_RELEASE(mInRightGainControl);
	CLEAN_RELEASE(mInMasterGainControl);
	CLEAN_RELEASE(mOutputSelector);
	CLEAN_RELEASE(mInputSelector);
	CLEAN_RELEASE(mPluginObjects);
	CLEAN_RELEASE(mTransportInterface);
	// must release last
	CLEAN_RELEASE(mPlatformInterface);
	CLEAN_RELEASE(mSignal);

    super::free();
    debugIOLog (3, "- AppleOnboardAudio::free, (void)");
}

#pragma mark +PORT HANDLER FUNCTIONS
void AppleOnboardAudio::setCurrentDevices(UInt32 devices){
    UInt32					odevice;

	odevice = 0;
    if (devices != currentDevices) {
        odevice = currentDevices;
        currentDevices = devices;
    }
    
	debugIOLog (3, "currentDevices = %ld", currentDevices);

	if (devices & kSndHWInputDevices || odevice & kSndHWInputDevices) {
		if (NULL != mInputConnectionControl) {
			OSNumber *			inputState;
			UInt32				active;

			active = devices & kSndHWInputDevices ? 1 : 0;		// If something is plugged in, that's good enough for now.
			inputState = OSNumber::withNumber ((long long unsigned int)active, 32);
			(void)mInputConnectionControl->hardwareValueChanged (inputState);
		}
	}

}

AudioHardwareObjectInterface * AppleOnboardAudio::getIndexedPluginObject (UInt32 index) {
	AudioHardwareObjectInterface *		thePluginObject;
	OSObject *							theObject;
	
	thePluginObject = NULL;
	theObject = mPluginObjects->getObject(index);
	
	if (NULL != theObject) {
		thePluginObject = (AudioHardwareObjectInterface *)(theObject->metaCast("AudioHardwareObjectInterface"));
	}
	
	return thePluginObject;
}

PlatformInterface * AppleOnboardAudio::getPlatformInterfaceObject () {
	return mPlatformInterface;
}

#pragma mark +INTERRUPTS
void AppleOnboardAudio::softwareInterruptHandler (OSObject *object, IOInterruptEventSource *source, int count) {
	AppleOnboardAudio *					aoa;
    UInt16								cachedProducerCount;

	debugIOLog (2, "softwareInterruptHandler ( %p, %p %d )", object, source, count );
	aoa = OSDynamicCast (AppleOnboardAudio, object);
	for ( UInt32 index = 0; index < kNumberOfActionSelectors; index++ ) {
		cachedProducerCount = aoa->mInterruptProduced[index];
		if ( 0 != cachedProducerCount - aoa->mInterruptConsumed[index] ) {
			aoa->protectedInterruptEventHandler ( index, 0 );
			aoa->mInterruptConsumed[index] = cachedProducerCount;
			debugIOLog (2, "Processed interrupt at index %d", index );
		}
	}
}

void AppleOnboardAudio::interruptEventHandler (UInt32 statusSelector, UInt32 newValue) {
	IOCommandGate *						cg;
	
	cg = getCommandGate ();
	if (NULL != cg) {
		cg->runAction (interruptEventHandlerAction, (void *)statusSelector, (void *)newValue);
	}

	return;
}

// Must be called on the workloop via runAction()
IOReturn AppleOnboardAudio::interruptEventHandlerAction (OSObject * owner, void * arg1, void * arg2, void * arg3, void * arg4) {
	IOReturn 							result;
	AppleOnboardAudio * 				aoa;
	
	result = kIOReturnError;
	
	debugIOLog (7, "AppleOnboardAudio:: interruptEventAction - (%p, %ld, %ld, %ld, %ld)", owner, (UInt32)arg1, (UInt32)arg2, (UInt32)arg3, (UInt32)arg4);
	
	aoa = (AppleOnboardAudio *)owner;
	FailIf (NULL == aoa, Exit);
	aoa->protectedInterruptEventHandler ((UInt32)arg1, (UInt32)arg2);

	result = kIOReturnSuccess;
Exit:
	return result;
}

bool AppleOnboardAudio::isTargetForMessage ( UInt32 actionSelector, AppleOnboardAudio * theAOA ) {
	OSArray *	swInterruptsArray;
	OSString *	theInterruptString;
	bool		result = FALSE;
	
	swInterruptsArray = OSDynamicCast ( OSArray, getLayoutEntry ( kSWInterrupts, theAOA ) );
	for ( UInt32 index = 0; index < swInterruptsArray->getCount (); index++ ) {
		theInterruptString = OSDynamicCast ( OSString, swInterruptsArray->getObject ( index ) );
		if ( NULL != theInterruptString ) {
			switch ( actionSelector ) {
				case kClockLockStatus:
					if ( !strcmp ( kClockLockIntMessage, theInterruptString->getCStringNoCopy() ) ) {
						result = TRUE;
					}
					break;
				case kClockUnLockStatus:
					if ( !strcmp ( kClockUnLockIntMessage, theInterruptString->getCStringNoCopy() ) ) {
						result = TRUE;
					}
					break;
				case kDigitalInInsertStatus:
					if ( !strcmp ( kDigitalInInsertIntMessage, theInterruptString->getCStringNoCopy() ) ) {
						result = TRUE;
					}
					break;
				case kDigitalInRemoveStatus:
					if ( !strcmp ( kDigitalInRemoveIntMessage, theInterruptString->getCStringNoCopy() ) ) {
						result = TRUE;
					}
					break;
			}
		}
	}
	return result;
}
	
// Must run on the workloop because we might modify audio controls.
void AppleOnboardAudio::protectedInterruptEventHandler (UInt32 statusSelector, UInt32 newValue) {
	AudioHardwareObjectInterface *		thePlugin;
	OSNumber * 							connectionCodeNumber;
	char * 								pluginString;
	UInt32 								selectorCode;

	selectorCode = getCharCodeForIntCode (statusSelector);

	switch ( statusSelector ) {
		case kInternalSpeakerStatus:		debugIOLog (6, "AppleOnboardAudio::protectedInterruptEventHandler ( kInternalSpeakerStatus, '%4s', %ld )", (char*)&selectorCode, newValue);		break;
		case kHeadphoneStatus:				debugIOLog (6, "AppleOnboardAudio::protectedInterruptEventHandler ( kHeadphoneStatus, '%4s', %ld )", (char*)&selectorCode, newValue);			break;
		case kExtSpeakersStatus:			debugIOLog (6, "AppleOnboardAudio::protectedInterruptEventHandler ( kExtSpeakersStatus, '%4s', %ld )", (char*)&selectorCode, newValue);			break;
		case kLineOutStatus:				debugIOLog (6, "AppleOnboardAudio::protectedInterruptEventHandler ( kLineOutStatus, '%4s', %ld )", (char*)&selectorCode, newValue);				break;
		case kDigitalOutStatus:				debugIOLog (6, "AppleOnboardAudio::protectedInterruptEventHandler ( kDigitalOutStatus, '%4s', %ld )", (char*)&selectorCode, newValue);			break;
		case kLineInStatus:					debugIOLog (6, "AppleOnboardAudio::protectedInterruptEventHandler ( kLineInStatus, '%4s', %ld )", (char*)&selectorCode, newValue);				break;
		case kInputMicStatus:				debugIOLog (6, "AppleOnboardAudio::protectedInterruptEventHandler ( kInputMicStatus, '%4s', %ld )", (char*)&selectorCode, newValue);				break;
		case kExternalMicInStatus:			debugIOLog (6, "AppleOnboardAudio::protectedInterruptEventHandler ( kExternalMicInStatus, '%4s', %ld )", (char*)&selectorCode, newValue);		break;
		case kDigitalInInsertStatus:		debugIOLog (6, "AppleOnboardAudio::protectedInterruptEventHandler ( kDigitalInInsertStatus, '%4s', %ld )", (char*)&selectorCode, newValue);			break;
		case kDigitalInRemoveStatus:		debugIOLog (6, "AppleOnboardAudio::protectedInterruptEventHandler ( kDigitalInRemoveStatus, '%4s', %ld )", (char*)&selectorCode, newValue);			break;
		case kRequestCodecRecoveryStatus:	debugIOLog (6, "AppleOnboardAudio::protectedInterruptEventHandler ( kRequestCodecRecoveryStatus, '%4s', %ld )", (char*)&selectorCode, newValue);	break;
		case kClockInterruptedRecovery:		debugIOLog (6, "AppleOnboardAudio::protectedInterruptEventHandler ( kClockInterruptedRecovery, '%4s', %ld )", (char*)&selectorCode, newValue);	break;
		case kClockUnLockStatus:			debugIOLog (6, "AppleOnboardAudio::protectedInterruptEventHandler ( kClockUnLockStatus, '%4s', %ld )", (char*)&selectorCode, newValue);			break;
		case kClockLockStatus:				debugIOLog (6, "AppleOnboardAudio::protectedInterruptEventHandler ( kClockLockStatus, '%4s', %ld )", (char*)&selectorCode, newValue);			break;
		case kAES3StreamErrorStatus:		debugIOLog (6, "AppleOnboardAudio::protectedInterruptEventHandler ( kAES3StreamErrorStatus, '%4s', %ld )", (char*)&selectorCode, newValue);		break;
		case kComboIsAnalogStatus:			debugIOLog (6, "AppleOnboardAudio::protectedInterruptEventHandler ( kComboIsAnalogStatus, '%4s', %ld )", (char*)&selectorCode, newValue);		break;
		case kComboIsDigitalStatus:			debugIOLog (6, "AppleOnboardAudio::protectedInterruptEventHandler ( kComboIsDigitalStatus, '%4s', %ld )", (char*)&selectorCode, newValue);		break;
		case kCodecErrorInterruptStatus:	debugIOLog (7, "AppleOnboardAudio::protectedInterruptEventHandler ( kCodecErrorInterruptStatus, '%4s', %ld )", (char*)&selectorCode, newValue);	break;
		case kCodecInterruptStatus:			debugIOLog (7, "AppleOnboardAudio::protectedInterruptEventHandler ( kCodecInterruptStatus, '%4s', %ld )", (char*)&selectorCode, newValue);		break;
		case kBreakClockSelect:				debugIOLog (6, "AppleOnboardAudio::protectedInterruptEventHandler ( kBreakClockSelect, '%4s', %ld )", (char*)&selectorCode, newValue);			break;
		case kMakeClockSelect:				debugIOLog (6, "AppleOnboardAudio::protectedInterruptEventHandler ( kMakeClockSelect, '%4s', %ld )", (char*)&selectorCode, newValue);			break;
		case kSetSampleRate:				debugIOLog (6, "AppleOnboardAudio::protectedInterruptEventHandler ( kSetSampleRate, '%4s', %ld )", (char*)&selectorCode, newValue);				break;
		case kSetSampleBitDepth:			debugIOLog (6, "AppleOnboardAudio::protectedInterruptEventHandler ( kSetSampleBitDepth, '%4s', %ld )", (char*)&selectorCode, newValue);			break;
		case kPowerStateChange:				debugIOLog (6, "AppleOnboardAudio::protectedInterruptEventHandler ( kPreDMAEngineInit, '%4s', %ld )", (char*)&selectorCode, newValue);			break;
		case kPreDMAEngineInit:				debugIOLog (6, "AppleOnboardAudio::protectedInterruptEventHandler ( kPreDMAEngineInit, '%4s', %ld )", (char*)&selectorCode, newValue);			break;
		case kPostDMAEngineInit:			debugIOLog (6, "AppleOnboardAudio::protectedInterruptEventHandler ( kPostDMAEngineInit, '%4s', %ld )", (char*)&selectorCode, newValue);			break;
		case kRestartTransport:				debugIOLog (6, "AppleOnboardAudio::protectedInterruptEventHandler ( kRestartTransport, '%4s', %ld )", (char*)&selectorCode, newValue);			break;
	}

	updateOutputDetectCollection (statusSelector, newValue);

	switch (statusSelector) {
		case kHeadphoneStatus:
		case kLineOutStatus:
		case kExtSpeakersStatus:													//	[3398729]
		case kDigitalOutStatus:

			selectorCode = getSelectorCodeForOutputEvent (statusSelector);

			connectionCodeNumber = OSNumber::withNumber (selectorCode, 32);
			// [3250195], must update this too!  Otherwise you get the right output selected but with the wrong clip routine/EQ.
			pluginString = getConnectionKeyFromCharCode (selectorCode, kIOAudioStreamDirectionOutput);
			debugIOLog (3, "pluginString updated to %s.", pluginString);
			// [3323073], move code from above, and now set the current output plugin if it's changing!
			thePlugin = getPluginObjectForConnection (pluginString);
			FailIf (NULL == thePlugin, Exit);
			
			cacheOutputVolumeLevels (mCurrentOutputPlugin);
			mCurrentOutputPlugin = thePlugin;
			
			// [3250195], current plugin doesn't matter here, always need these updated
			setClipRoutineForOutput (pluginString);
			selectOutput (selectorCode, mIsMute);
			if (NULL == mOutputSelector) debugIOLog (3, "\n!!!mOutputSelector = NULL!!!");
			FailIf (NULL == mOutputSelector, Exit);
			mOutputSelector->hardwareValueChanged (connectionCodeNumber);

			connectionCodeNumber->release();

			// [3317593], always update controls, regardless of who the current output plugin is
			AdjustOutputVolumeControls (thePlugin, selectorCode);
			
			// [3284911]
			OSNumber *			headphoneState;
			if (mDetectCollection & kSndHWCPUHeadphone) {
				headphoneState = OSNumber::withNumber (1, 32);
			} else {
				headphoneState = OSNumber::withNumber ((long long unsigned int)0, 32);
			}
			mHeadphoneConnected->hardwareValueChanged (headphoneState);
			break;
		case kLineInStatus:
			// [3250612] don't do anything on insert events!
			pluginString = getConnectionKeyFromCharCode (selectorCode, kIOAudioStreamDirectionInput);
			debugIOLog (3, "AppleOnboardAudio::protectedInterruptEventHandler - pluginString = %s.", pluginString);
			thePlugin = getPluginObjectForConnection (pluginString);
			FailIf (NULL == thePlugin, Exit);
			debugIOLog (3, "AppleOnboardAudio::protectedInterruptEventHandler - thePlugin = %p.", thePlugin);

			FailIf (NULL == mInputSelector, Exit);
			selectorCode = mInputSelector->getIntValue ();
			debugIOLog (3, "mInputSelector->getIntValue () returns %4s", (char *)&selectorCode);

			if ((mDetectCollection & kSndHWLineInput) && (kIOAudioInputPortSubTypeLine == selectorCode)) {
				thePlugin->setInputMute (FALSE);
			} else if (!(mDetectCollection & kSndHWLineInput) && (kIOAudioInputPortSubTypeLine == selectorCode)) {
				thePlugin->setInputMute (TRUE);
			}
			break;
		case kExternalMicInStatus:
			break;
		case kDigitalInInsertStatus:
		case kDigitalInRemoveStatus:
			if ( !broadcastSoftwareInterruptMessage ( statusSelector ) ) {
				debugIOLog (2, "kDigitalInRemoveStatus NOT BROADCASTED" );
			}
			break;
		case kRequestCodecRecoveryStatus:
			//	Walk through the available plugin objects and invoke the recoverFromFatalError on each object in the correct order.
			callPluginsInOrder (kRequestCodecRecoveryStatus, newValue);
			break;
		case kRestartTransport:
			//	This message is used to restart the transport hardware without invoking a general recovery or
			//	a recovery from clock interruption and is used to perform sequence sensitive initialization.
			if (NULL != mTransportInterface) {
				mTransportInterface->transportSetSampleRate ( mTransportInterface->transportGetSampleRate() );
			}
			break;
		case kCodecErrorInterruptStatus:
			callPluginsInOrder ( kCodecErrorInterruptStatus, 0 );
			break;
		case kCodecInterruptStatus:
			callPluginsInOrder ( kCodecInterruptStatus, 0 );
			break;
		case kClockUnLockStatus:
		case kClockLockStatus:
			//	Codec clock loss errors are to be ignored if in the process of switching clock sources.
			if ( !mClockSelectInProcessSemaphore ) {
				//	A non-zero 'newValue' indicates an 'UNLOCK' clock lock status and requires that the clock source
				//	be redirected back to an internal source (i.e. the internal hardware is to act as a MASTER).
				if ( kClockLockStatus == statusSelector ) {
					//	Set the hardware to MASTER mode.
					clockSelectorChanged ( kClockSourceSelectionInternal );
					if ( NULL != mExternalClockSelector ) {
						//	Flush the control value (i.e. MASTER = internal).
						OSNumber *			clockSourceSelector;
						clockSourceSelector = OSNumber::withNumber (kClockSourceSelectionInternal, 32);
						
						mExternalClockSelector->hardwareValueChanged (clockSourceSelector);
					}
				} else {
					//	[3435307]	Dont touch amplifier mutes related to UI.	RBM
					//	[3253678]	successful lock detected, now safe to unmute analog part
					callPluginsInOrder ( kSetMuteState, mIsMute );
				}
			} else {
				debugIOLog (3,  "Attempted to post kClockLockStatus blocked by semaphore" );
			}
			broadcastSoftwareInterruptMessage ( statusSelector );
			break;
		case kAES3StreamErrorStatus:
			//	indicates that the V bit states data is invalid or may be compressed data
			debugIOLog (7,  "... kAES3StreamErrorStatus %d", (unsigned int)newValue );
			if ( newValue ) {
				//	As appropriate (TBD)...
			}
			break;
		default:
			break;
	}
Exit:
	debugIOLog (5,  "- AppleOnboardAudio::protedtedInterruptEventHandler" );

	return;
}


bool AppleOnboardAudio::broadcastSoftwareInterruptMessage ( UInt32 actionSelector ) {
	AppleOnboardAudio *		theAOA;
	bool					result = FALSE;

	if ( NULL != mAOAInstanceArray ) {
		for ( UInt32 index = 0; index < mAOAInstanceArray->getCount(); index++ ) {
			theAOA = OSDynamicCast ( AppleOnboardAudio, mAOAInstanceArray->getObject ( index ) );
			if ( isTargetForMessage ( index, theAOA ) ) {
				if (theAOA->mSoftwareInterruptHandler) {
					theAOA->mInterruptProduced[actionSelector]++;
					theAOA->mSoftwareInterruptHandler->interruptOccurred (NULL, NULL, 0);
					result = TRUE;
				}
			}
		}
	}
	return result;
}


#pragma mark +IOAUDIO INIT
bool AppleOnboardAudio::initHardware (IOService * provider) {
	bool								result;

	result = FALSE;

	mSignal = IOSyncer::create (FALSE);

    debugIOLog (3, "+ AppleOnboardAudio[%ld]::initHardware", mInstanceIndex);
	FailIf (NULL == mInitHardwareThread, Exit);

#ifdef DONT_THREAD_INIT
	// this version allows unloading of the kext if initHW fails
	if (kIOReturnSuccess == protectedInitHardware (provider)) {
		result = TRUE;
	}
#else
	thread_call_enter1 (mInitHardwareThread, (void *)provider);
	
	result = TRUE;
#endif
	
Exit:
    debugIOLog (3, "- AppleOnboardAudio[%ld]::initHardware returns %d", mInstanceIndex, result);

	return result;
}

void AppleOnboardAudio::initHardwareThread (AppleOnboardAudio * aoa, void * provider) {
	IOCommandGate *						cg;
	IOReturn							result;

	FailIf (NULL == aoa, Exit);
	FailIf (TRUE == aoa->mTerminating, Exit);	

	cg = aoa->getCommandGate ();
	if (cg) {
		result = cg->runAction (aoa->initHardwareThreadAction, provider);
	}

Exit:
	return;
}

IOReturn AppleOnboardAudio::initHardwareThreadAction (OSObject * owner, void * provider, void * arg2, void * arg3, void * arg4) {
	AppleOnboardAudio *					aoa;
	IOReturn							result;

	result = kIOReturnError;

	aoa = (AppleOnboardAudio *)owner;
	FailIf (NULL == aoa, Exit);

	result = aoa->protectedInitHardware ((IOService *)provider);

Exit:
	return result;
}

IOReturn AppleOnboardAudio::protectedInitHardware (IOService * provider) {
	OSArray *							layouts;
	OSArray *							hardwareObjectsList;
	OSArray *							multipleDevicesArray;
	OSDictionary *						layoutEntry;
	OSNumber *							layoutIDNumber;
	OSNumber *							ampRecoveryNumber;
	OSNumber *							headphoneState;
	OSString *							platformObjectString;
	OSString *							transportObjectString;
	OSString *							comboInAssociationString;
	OSString *							comboOutAssociationString;
	OSData *							tmpData;
	UInt32 *							layoutID;
	UInt32								layoutIDInt;
	UInt32								timeWaited;
	UInt32								layoutsCount;
	UInt32								index;
	UInt32								numPlugins;
	bool								done;
	AudioHardwareObjectInterface *		thePluginObject;
	OSDictionary *						AOAprop;
	IOWorkLoop *						workLoop;
	OSBoolean *							softwareInputGainBoolean;
	OSNumber * 							inputLatencyNumber;
	OSNumber * 							outputLatencyNumber;
	OSNumber * 							connectionCodeNumber;
	OSNumber * 							volumeNumber;
	char * 								connectionString;
	UInt32 								connectionCode;
	UInt32								tempLatency;
	UInt32								inputLatency;
	UInt32								selectorCode;
	UInt32								count;
    IOReturn							result;
	char								deviceName[256];
	char								num[4];

	result = kIOReturnError;

    debugIOLog (3, "+ AppleOnboardAudio[%ld]::protectedInitHardware", mInstanceIndex);
		
	debugIOLog (3, "provider's name is: %s", provider->getName ());
	
	tmpData = OSDynamicCast (OSData, provider->getProperty (kLayoutID));
	debugIOLog (3, "provider layoutID data = %p", tmpData);
	
	workLoop = getWorkLoop();
	FailIf (NULL == workLoop, Exit);
		
	mSoftwareInterruptHandler = IOInterruptEventSource::interruptEventSource (this, softwareInterruptHandler);
	FailIf (NULL == mSoftwareInterruptHandler, Exit);
	workLoop->addEventSource (mSoftwareInterruptHandler);
	mSoftwareInterruptHandler->enable ();
	
	FailIf (NULL == tmpData, Exit);
	layoutID = (UInt32 *)tmpData->getBytesNoCopy ();
	debugIOLog (3, "layoutID pointer = %p", layoutID);
	FailIf (NULL == layoutID, Exit)
	mLayoutID = *layoutID;
	mUCState.ucLayoutID = mLayoutID;

	// Figure out which plugins need to be loaded for this machine.
	// Fix up the registry to get needed plugins to load using ourselves as a nub.
	layouts = OSDynamicCast (OSArray, getProperty (kLayouts));
	debugIOLog (3, "layout array = %p", layouts);
	FailIf (NULL == layouts, Exit);

	// First thing to do is to find the array entry that describes the machine that we are on.
	layoutsCount = layouts->getCount ();
	layoutEntry = NULL;
	index = 0;
	
	mMatchingIndex = 0xFFFFFFFF;
	layoutIDInt = 0;
	debugIOLog (3,  "AppleOnboardAudio[%ld] ><><>< mLayoutID 0x%lX (from provider node)", mInstanceIndex, mLayoutID);
	
	while (layoutsCount--) {
		layoutEntry = OSDynamicCast (OSDictionary, layouts->getObject (index));
		FailIf (NULL == layoutEntry, Exit);
		layoutIDNumber = OSDynamicCast (OSNumber, layoutEntry->getObject (kLayoutIDInfoPlist));
		FailIf (NULL == layoutIDNumber, Exit);
		layoutIDInt = layoutIDNumber->unsigned32BitValue ();
#ifdef REMOVE_LAYOUTS
		if (layoutIDInt != mLayoutID) {
			debugIOLog (3,  "found layout id 0x%lX and deleted it", layoutIDInt);
			layouts->removeObject (index);			// Remove wrong entries from the IORegistry to save space
		} else {
			debugIOLog (3,  "found matchine layout id 0x%lX @ index %ld", layoutIDInt, index);
			mMatchingIndex = index;
			index++;
		}
#else
		if (layoutIDInt == mLayoutID) {
			debugIOLog (3,  "AppleOnboardAudio[%ld] found matchine layout id 0x%lX @ index %ld", mInstanceIndex, layoutIDInt, index);
			mMatchingIndex = index;
			break;
		} else {
			index++;
		}
#endif
	}
	debugIOLog (3,  "AppleOnboardAudio[%ld] ><><>< layoutIDInt 0x%lX", mInstanceIndex, layoutIDInt );

#ifdef REMOVE_LAYOUTS
	layoutEntry = OSDynamicCast (OSDictionary, layouts->getObject (0));
#else
	FailIf (0xFFFFFFFF == mMatchingIndex, Exit);	
	layoutEntry = OSDynamicCast (OSDictionary, layouts->getObject (mMatchingIndex));
#endif

	debugIOLog (3, "layoutEntry = %p", layoutEntry);
	FailIf (NULL == layoutEntry, Exit);

	ampRecoveryNumber = OSDynamicCast ( OSNumber, layoutEntry->getObject (kAmpRecoveryTime) );
	FailIf (NULL == ampRecoveryNumber, Exit);
	mAmpRecoveryMuteDuration = ampRecoveryNumber->unsigned32BitValue();
	debugIOLog (3, "AppleOnboardAudio[%ld]::protectedInitHardware - mAmpRecoveryMuteDuration = %ld", mInstanceIndex, mAmpRecoveryMuteDuration);

	// Find out what the correct platform object is and request the platform factory to build it for us.
	platformObjectString = OSDynamicCast ( OSString, layoutEntry->getObject ( kPlatformObject ) );
	FailIf (NULL == platformObjectString, Exit);
	debugIOLog (3, "AppleOnboardAudio[%ld]::protectedInitHardware - platformObjectString = %s", mInstanceIndex, platformObjectString->getCStringNoCopy());

	mPlatformInterface = PlatformFactory::createPlatform (platformObjectString);
	FailIf (NULL == mPlatformInterface, Exit);
	debugIOLog (3, "AppleOnboardAudio[%ld]::protectedInitHardware - mPlatformInterface = %p", mInstanceIndex, mPlatformInterface);
	FailIf (!mPlatformInterface->init(provider, this, AppleDBDMAAudio::kDBDMADeviceIndex), Exit);
	
	//	[3453799]	begin {
	comboInAssociationString = OSDynamicCast ( OSString, layoutEntry->getObject ( kComboInObject ) );
	if ( NULL != comboInAssociationString ) {
		debugIOLog (3, "comboInAssociationString = %s", comboInAssociationString->getCStringNoCopy ());
		if ( comboInAssociationString->isEqualTo ("LineInDetect") ) {
			mPlatformInterface->setAssociateComboInTo ( kGPIO_Selector_LineInDetect );
		} else if ( comboInAssociationString->isEqualTo ("ExternalMicDetect") ) {
			mPlatformInterface->setAssociateComboInTo ( kGPIO_Selector_ExternalMicDetect );
		}
	}
	
	comboOutAssociationString = OSDynamicCast ( OSString, layoutEntry->getObject ( kComboOutObject ) );
	if ( NULL != comboOutAssociationString ) {
		debugIOLog (3, "comboOutAssociationString = %s", comboOutAssociationString->getCStringNoCopy ());
		if ( comboOutAssociationString->isEqualTo ("LineOutDetect") ) {
			mPlatformInterface->setAssociateComboOutTo ( kGPIO_Selector_LineOutDetect );
		} else if ( comboOutAssociationString->isEqualTo ("HeadphonesDetect") ) {
			mPlatformInterface->setAssociateComboOutTo ( kGPIO_Selector_HeadphoneDetect );
		} else if ( comboOutAssociationString->isEqualTo ("ExtSpeakersDetect") ) {
			mPlatformInterface->setAssociateComboOutTo ( kGPIO_Selector_SpeakerDetect );
		}
	}
	//	} end	[3453799]
		
	debugIOLog (3, "AppleOnboardAudio[%ld]::protectedInitHardware - about to mute all amps.", mInstanceIndex );

	mPlatformInterface->setHeadphoneMuteState ( kGPIO_Muted );
	mPlatformInterface->setLineOutMuteState ( kGPIO_Muted );
	mPlatformInterface->setSpeakerMuteState ( kGPIO_Muted );
	IOSleep (mAmpRecoveryMuteDuration);

	// Find out what the correct transport object is and request the transport factory to build it for us.
	transportObjectString = OSDynamicCast ( OSString, layoutEntry->getObject ( kTransportObject ) );
	FailIf ( NULL == transportObjectString, Exit );
	debugIOLog (3, "AppleOnboardAudio[%ld]::protectedInitHardware - transportObjectString = %s", mInstanceIndex, transportObjectString->getCStringNoCopy());

	mTransportInterface = TransportFactory::createTransport ( transportObjectString );
	debugIOLog (3, "AppleOnboardAudio[%ld]::protectedInitHardware - mTransportInterface = %p", mInstanceIndex, mTransportInterface);
	FailIf (NULL == mTransportInterface, Exit);
	FailIf (!mTransportInterface->init ( mPlatformInterface ), Exit);

	// If we have the entry we were looking for then get the list of plugins that need to be loaded
	hardwareObjectsList = OSDynamicCast (OSArray, layoutEntry->getObject (kHardwareObjects));
	if ( NULL == hardwareObjectsList ) { debugIOLog (3,  "><><>< NULL == hardwareObjectsList" ); }
	FailIf (NULL == hardwareObjectsList, Exit);

	// Set the IORegistry entries that will cause the plugins to load
	numPlugins = hardwareObjectsList->getCount ();
	debugIOLog (3,  "AppleOnboardAudio[%ld] numPlugins to load = %ld", mInstanceIndex, numPlugins);

	if (NULL == mPluginObjects) {
		mPluginObjects = OSArray::withCapacity (0);
	}
	FailIf (NULL == mPluginObjects, Exit);

	for (index = 0; index < numPlugins; index++) {
		setProperty (((OSString *)hardwareObjectsList->getObject(index))->getCStringNoCopy(), "YES");
	}
	
//	registerService (kIOServiceSynchronous);								// Gets the plugins to load.
	registerService ();														// Gets the plugins to load.

	// Wait for the plugins to load so that when we get to initHardware everything will be up and ready to go.
	timeWaited = 0;
	done = FALSE;
	while (!done) {
		if (NULL == mPluginObjects) {
			IOSleep (10);
		} else {
			if (mPluginObjects->getCount () != numPlugins) {
				IOSleep (10);
			} else {
				done = TRUE;
			}
		}
		timeWaited++;
	}

//	if ((0 == timeout) && (FALSE == done)) {
//		debugIOLog (3, "$$$$$$ timeout and not enough plugins $$$$$$");
//		setProperty ("Plugin load failed", "TRUE");
//	}
	volumeNumber = OSNumber::withNumber((long long unsigned int)0, 32);

    if (!super::initHardware (provider)) {
        goto Exit;
    }

	debugIOLog (3, "A: about to set work loop");
	
	mHeadLineDigExclusive = FALSE;		// [3276398], default to working like we always have.
//	workLoop = getWorkLoop();
//	FailIf (NULL == workLoop, Exit);

	// must occur in this order, and must be called in initHardware or later to have a valid workloop
	mPlatformInterface->setWorkLoop (workLoop);
	
	FailIf (NULL == mPluginObjects, Exit);

	count = mPluginObjects->getCount ();
	FailIf (0 == count, Exit);
	
	debugIOLog (3, "AppleOnboardAudio[%ld] about to init %ld plugins", mInstanceIndex, count);

	for (index = 0; index < count; index++) {
		thePluginObject = getIndexedPluginObject (index);

		FailIf (NULL == thePluginObject, Exit);

		thePluginObject->setWorkLoop (workLoop);

		thePluginObject->initPlugin (mPlatformInterface);

		thePluginObject->setProperty (kPluginPListMasterVol, volumeNumber);
		thePluginObject->setProperty (kPluginPListLeftVol, volumeNumber);
		thePluginObject->setProperty (kPluginPListRightVol, volumeNumber);
		thePluginObject->setProperty (kPluginPListLeftGain, volumeNumber);
		thePluginObject->setProperty (kPluginPListRightGain, volumeNumber);

		// XXX FIX - this is a temporary init
		mCurrentOutputPlugin = thePluginObject;
		mCurrentInputPlugin = thePluginObject;
	}

	volumeNumber->release ();

	// FIX - check the result of this call and remove plugin if it fails!
	callPluginsInOrder (kPreDMAEngineInit, 0);

	mSignal->signal (kIOReturnSuccess, FALSE);

#if LOCALIZABLE
	sprintf ( deviceName, "%s", "DeviceName");
	if (mInstanceIndex > 1) {
		sprintf (num, "%d", mInstanceIndex);
		strcat (deviceName, num);
	}
//	setDeviceName ("DeviceName");
	setDeviceName (deviceName);
	sprintf ( deviceName, "%s", "DeviceShortName");
	if (mInstanceIndex > 1) {
		sprintf (num, "%d", mInstanceIndex);
		strcat (deviceName, num);
	}
//	setDeviceShortName ("DeviceShortName");
	setDeviceShortName (deviceName);
	setManufacturerName ("ManufacturerName");
	setProperty (kIOAudioDeviceLocalizedBundleKey, "AppleOnboardAudio.kext");
#else
    setDeviceName ("Built-in Audio");
    setDeviceShortName ("Built-in");
    setManufacturerName ("Apple");
#endif

#ifdef ONLY_PUBLISH_ONE_BUILIT_DEVICE	
	if (mInstanceIndex > 1) {
		setDeviceTransportType (kIOAudioDeviceTransportTypePCI);
	} else {
		setDeviceTransportType (kIOAudioDeviceTransportTypeBuiltIn);
	}
#else
	setDeviceTransportType (kIOAudioDeviceTransportTypeBuiltIn);
#endif
	
	setProperty (kIOAudioEngineCoreAudioPlugInKey, "IOAudioFamily.kext/Contents/PlugIns/AOAHALPlugin.bundle");

	configureDMAEngines (provider);
	debugIOLog (3, "AppleOnboardAudio[%ld] finished configure DMA engine (%p) ", mInstanceIndex, mDriverDMAEngine);
	FailIf (NULL == mDriverDMAEngine, Exit);

	// Have to create the audio controls before calling activateAudioEngine
	mAutoUpdatePRAM = FALSE;			// Don't update the PRAM value while we're initing from it
    result = createDefaultControls ();
	FailIf (kIOReturnSuccess != result, Exit);

	debugIOLog (3, "  AppleOnboardAudio::initHardware - mDriverDMAEngine retain count before activateAudioEngine = %d", mDriverDMAEngine->getRetainCount());
    if (kIOReturnSuccess != activateAudioEngine (mDriverDMAEngine)) {
		mDriverDMAEngine->release ();
		mDriverDMAEngine = NULL;
        goto Exit;
    }
	debugIOLog (3, "  AppleOnboardAudio::initHardware - mDriverDMAEngine retain count after activateAudioEngine = %d", mDriverDMAEngine->getRetainCount());

	idleTimer = IOTimerEventSource::timerEventSource (this, sleepHandlerTimer);
	if (!idleTimer) {
		goto Exit;
	}
	workLoop->addEventSource (idleTimer);
	
	pollTimer = IOTimerEventSource::timerEventSource ( this, pollTimerCallback );
	if ( pollTimer ) {
		workLoop->addEventSource ( pollTimer );
	}

	// Set this to a default for desktop machines (portables will get a setAggressiveness call later in the boot sequence).
	ourPowerState = kIOAudioDeviceActive;
	mUCState.ucPowerState = ourPowerState;
	setProperty ("IOAudioPowerState", ourPowerState, 32);
	idleSleepDelayTime = kNoIdleAudioPowerDown;
	// [3107909] Turn the hardware off because IOAudioFamily defaults to the off state, so make sure the hardware is off or we get out of synch with the family.
	setIdleAudioSleepTime (idleSleepDelayTime);

	// Set the default volume to that stored in the PRAM in case we don't get a setValue call from the Sound prefs before being activated.
	mAutoUpdatePRAM = FALSE;			// Don't update the PRAM value while we're initing from it
	if (NULL != mOutMasterVolumeControl) {
		UInt32			volume;
		volume = (mOutMasterVolumeControl->getMaxValue () - mOutMasterVolumeControl->getMinValue () + 1) * 90 / 100;
		mOutMasterVolumeControl->setValue (volume);
	}
	if (NULL != mOutLeftVolumeControl) {
		UInt32			volume;
		volume = (mOutLeftVolumeControl->getMaxValue () - mOutLeftVolumeControl->getMinValue () + 1) * 65 / 100;
		mOutLeftVolumeControl->setValue (volume);
	}
	if (NULL != mOutRightVolumeControl) {
		UInt32			volume;
		volume = (mOutRightVolumeControl->getMaxValue () - mOutRightVolumeControl->getMinValue () + 1) * 65 / 100;
		mOutRightVolumeControl->setValue (volume);
	}

	// Give drivers a chance to do something after the DMA engine and IOAudioFamily have been created/started
	// FIX - check the result of this call and remove plugin if it fails!
	callPluginsInOrder (kPostDMAEngineInit, 0);

	initializeDetectCollection();
	
	connectionCode = parseOutputDetectCollection ();
	connectionCodeNumber = OSNumber::withNumber(connectionCode, 32);

	// [3284911]
	if ( mHeadphoneConnected ) {
		if (mDetectCollection & kSndHWCPUHeadphone) {
			headphoneState = OSNumber::withNumber (1, 32);
		} else {
			headphoneState = OSNumber::withNumber ((long long unsigned int)0, 32);
		}
		mHeadphoneConnected->hardwareValueChanged (headphoneState);
	}
	
	if ( NULL != mOutputSelector ) {
		mOutputSelector->hardwareValueChanged (connectionCodeNumber);
	}
	
	connectionCodeNumber->release();
		
	if ( NULL != mOutputSelector ) {
		selectorCode = mOutputSelector->getIntValue ();
		debugIOLog (3, "mOutputSelector->getIntValue () returns %lX", selectorCode);
		if (0 != selectorCode) {
			connectionString = getConnectionKeyFromCharCode (selectorCode, kIOAudioStreamDirectionOutput);
			debugIOLog (3, "mOutputSelector->getIntValue () char code is %s", connectionString);
			if (NULL != connectionString) {
				AudioHardwareObjectInterface* theHWObject;
				theHWObject = getPluginObjectForConnection (connectionString);
				if (NULL != theHWObject) {
					mCurrentOutputPlugin = theHWObject;
				} else {
					debugIOLog (1, "can't find hardware plugin for output %s", connectionString);
				}
			}
		}
		debugIOLog (3, "AppleOnboardAudio[%ld] mCurrentOutputPlugin = %p", mInstanceIndex, mCurrentOutputPlugin);
	}
	
	FailIf (NULL == mInputSelector, Exit);
	selectorCode = mInputSelector->getIntValue ();
	debugIOLog (3, "mInputSelector->getIntValue () returns %4s", (char *)&selectorCode);
	if (0 != selectorCode) {
		connectionString = getConnectionKeyFromCharCode (selectorCode, kIOAudioStreamDirectionInput);
		debugIOLog (3, "mInputSelector->getIntValue () char code is %s", connectionString);
		if (NULL != connectionString) {
			AudioHardwareObjectInterface* theHWObject;
			theHWObject = getPluginObjectForConnection (connectionString);
			if (NULL != theHWObject) {
				mCurrentInputPlugin = theHWObject;
			} else {
				debugIOLog (1, "can't find hardware plugin for input %s", connectionString);
			}
		}
	}
	debugIOLog (3, "AppleOnboardAudio[%ld] mCurrentInputPlugin = %p", mInstanceIndex, mCurrentInputPlugin);
	
	mCurrentInputPlugin->setActiveInput (selectorCode);

	AOAprop = OSDynamicCast (OSDictionary, mCurrentInputPlugin->getProperty (kPluginPListAOAAttributes));
	FailIf (NULL == AOAprop, Exit);

	softwareInputGainBoolean = OSDynamicCast (OSBoolean, AOAprop->getObject (kPluginPListSoftwareInputGain));
	if (NULL != softwareInputGainBoolean) {
		mDriverDMAEngine->setUseSoftwareInputGain (softwareInputGainBoolean->getValue ());
		mCurrentPluginHasSoftwareInputGain = softwareInputGainBoolean->getValue ();
	} else {
		mDriverDMAEngine->setUseSoftwareInputGain (false);
		mCurrentPluginHasSoftwareInputGain = false;
	}

	inputLatency = 0;			// init them to safe default values, a bit high, but safe.
	inputLatencyNumber = OSDynamicCast (OSNumber, AOAprop->getObject (kPluginPListInputLatency));
	if (NULL != inputLatencyNumber) {
		inputLatency = inputLatencyNumber->unsigned32BitValue();
	}

// [3277271], find, store and use the largest output latency - this doesn't change anymore since we can't keep track of 
// which hardware is receiving audio in all cases
	mOutputLatency = 0;
	
	count = mPluginObjects->getCount ();
	for (index = 0; index < count; index++) {
		thePluginObject = getIndexedPluginObject (index);

		FailIf (NULL == thePluginObject, Exit); 
		
		AOAprop = OSDynamicCast (OSDictionary, thePluginObject->getProperty (kPluginPListAOAAttributes));
		FailIf (NULL == AOAprop, Exit);
	
		outputLatencyNumber = OSDynamicCast (OSNumber, AOAprop->getObject (kPluginPListOutputLatency));
		if (NULL != outputLatencyNumber) {
			tempLatency = outputLatencyNumber->unsigned32BitValue();
			if (tempLatency > mOutputLatency) {
				mOutputLatency = tempLatency;
			}
		}	
	}

	mDriverDMAEngine->setSampleLatencies (mOutputLatency, inputLatency);

	// Has to be after creating the controls so that interruptEventHandler isn't called before the selector controls exist.
	mPlatformInterface->registerInterrupts ( (IOService*)mPlatformInterface );

#if 0
	debugIOLog (3, "about to flush all controls");
	
	OSSet *		theControls;
	theControls = mDriverDMAEngine->defaultAudioControls;
	debugIOLog (3, "theControls count = %d", mDriverDMAEngine->defaultAudioControls->getCount ());
#endif
    flushAudioControls ();
	if (NULL != mExternalClockSelector) { mExternalClockSelector->flushValue (); }		// Specifically flush the clock selector's values because flushAudioControls() doesn't seem to call it... ???

 	// Install power change handler so we get notified about shutdown
	mSysPowerDownNotifier = registerPrioritySleepWakeInterest (&sysPowerDownHandler, this, 0);
	
	//	Some objects need polled.  Examples include a GPIO without interrupt capability, limiting
	//	the frequency of a GPIO interrupt or posting of the sample rate from an external clock source.
	//	Some of these requirements reside in lower level object such as the platform interface object.
	//	Some requirements, such as posting the sample rate, have behavior implemented in AppleOnboardAudio
	//	with dependencies on lower level objects.  NOTE:  The infoPlist will encode polling requirements 
	//	in the future!!!  Use an IOTimerEventSource here as the highest frequency timer in the IOAudioDevice
	//	will trigger a callback (the erase head will run at a faster interval than is desired).
	
	setPollTimer ();

//	selectOutput (connectionCode, mIsMute);	//	when interrupts are registere/enabled then the interrupt dispatch will perform this function.

#ifdef FORCE_MULTIPLE_DEVICES_TO_LOAD
	// if we are the first instance around and we need to fire up another AOA, find the other i2s node
	// we want and set a property that will force our other AOA personality to load
	multipleDevicesArray = OSDynamicCast (OSArray, layoutEntry->getObject (kMultipleDevices));
	debugIOLog (3, "multipleDevicesArray = %p", multipleDevicesArray);

	if (1 == mInstanceIndex && (NULL != multipleDevicesArray)) {
		UInt32						deviceIndex;
		UInt32						devicesToLoad;
		mach_timespec_t				timeout;
		
		timeout.tv_sec = 5;
		timeout.tv_nsec = 0;
		devicesToLoad = 0;
		
		devicesToLoad = multipleDevicesArray->getCount();
		debugIOLog (3, "devicesToLoad = %ld", devicesToLoad);
		
		for (deviceIndex = 0; deviceIndex < devicesToLoad; deviceIndex++) {
			OSDictionary *			deviceDict;
			OSString *				i2sNodeString;
			OSString *				soundNodePathString;
			OSString *				matchPropertyString;
			IOService *				i2sService;
			IORegistryEntry * 		soundRegEntry;
			IOService * 			sound;
			
			deviceDict = OSDynamicCast (OSDictionary, multipleDevicesArray->getObject(deviceIndex));
			debugIOLog (3, "deviceDict = %p", deviceDict);
			FailIf (NULL == deviceDict, Exit);

			i2sNodeString = OSDynamicCast (OSString, deviceDict->getObject(kI2SNode));
			debugIOLog (3, "i2sNodeString = %p", i2sNodeString);
			FailIf (NULL == i2sNodeString, Exit);

			soundNodePathString = OSDynamicCast (OSString, deviceDict->getObject(kSoundNodePath));
			debugIOLog (3, "soundNodePathString = %p", soundNodePathString);
			FailIf (NULL == soundNodePathString, Exit);

			matchPropertyString = OSDynamicCast (OSString, deviceDict->getObject(kMatchProperty));
			debugIOLog (3, "matchPropertyString = %p", matchPropertyString);
			FailIf (NULL == matchPropertyString, Exit);
		
			i2sService = IOService::waitForService (IOService::nameMatching(i2sNodeString->getCStringNoCopy()), &timeout);
			debugIOLog (3, "i2sService = %p", deviceDict);
			FailIf (NULL == i2sService, Exit);			
			soundRegEntry = i2sService->childFromPath (soundNodePathString->getCStringNoCopy(), gIOServicePlane);
			debugIOLog (3, "soundRegEntry = %p", soundRegEntry);
			FailIf (NULL == i2sService, Exit);
			
			sound = OSDynamicCast (IOService, soundRegEntry);
			FailIf (NULL == i2sService, Exit);
			debugIOLog (3, "soundRegEntry = %p", soundRegEntry);

			sound->setProperty (matchPropertyString->getCStringNoCopy(), "YES");
			sound->registerService ();
		}
	} else {
		debugIOLog (3,  "><><>< NULL == multipleDevicesArray on instance %ld", mInstanceIndex );
	}	
	aoaNotifier = addNotification (gIOPublishNotification, serviceMatching ("AppleOnboardAudio"), (IOServiceNotificationHandler)&aoaPublished, this);
#endif

	result = kIOReturnSuccess;
	mAutoUpdatePRAM = TRUE;

Exit:
	if (NULL != mInitHardwareThread) {
		thread_call_free (mInitHardwareThread);
	}

    debugIOLog (3, "- AppleOnboardAudio[%ld]::protectedInitHardware returns 0x%x", mInstanceIndex, result); 
	return (result);
}

bool AppleOnboardAudio::aoaPublished (AppleOnboardAudio * aoaObject, void * refCon, IOService * newService) {
	bool	result = FALSE;
	
	if ( newService != aoaObject ) {
		if ( NULL == aoaObject->mAOAInstanceArray ) {
			aoaObject->mAOAInstanceArray = OSArray::withObjects ( (const OSObject**)&newService, 1 );
			FailIf ( NULL == aoaObject->mAOAInstanceArray, Exit );
		} else {
			aoaObject->mAOAInstanceArray->setObject ( newService );
		}
		result = TRUE;
	}
Exit:
	return result;
}

IOReturn AppleOnboardAudio::configureDMAEngines(IOService *provider) {
    IOReturn 						result;
    bool							hasInput;
    bool							hasOutput;
	OSArray *						formatsArray;
	OSArray *						inputListArray;
	OSArray *						outputListArray;
    
    result = kIOReturnError;

	inputListArray = OSDynamicCast (OSArray, getLayoutEntry (kInputsList, this));
	hasInput = (NULL != inputListArray);
	
	outputListArray = OSDynamicCast (OSArray, getLayoutEntry (kOutputsList, this));
	hasOutput = ( NULL != outputListArray );
	
    debugIOLog (3, "+ AppleOnboardAudio[%ld]::configureDMAEngines (%p)", mInstanceIndex, provider);

	// All this config should go in a single method
    mDriverDMAEngine = new AppleDBDMAAudio;
    // make sure we get an engine
    FailIf (NULL == mDriverDMAEngine, Exit);

	formatsArray = OSDynamicCast (OSArray, getLayoutEntry (kFormats, this));

    if (!mDriverDMAEngine->init (NULL, mPlatformInterface, (IOService *)provider->getParentEntry (gIODTPlane), hasInput, hasOutput, formatsArray)) {
        mDriverDMAEngine->release ();
		mDriverDMAEngine = NULL;
        goto Exit;
    }
   
	result = kIOReturnSuccess;

Exit:
    debugIOLog (3, "- AppleOnboardAudio[%ld]::configureDMAEngines (%p) returns %x", mInstanceIndex, provider, result);
    return result;
}

UInt16 AppleOnboardAudio::getTerminalTypeForCharCode (UInt32 outputSelection) {
	UInt16								terminalType;

	switch (outputSelection) {
		case kIOAudioSelectorControlSelectionValueInternalSpeaker:
			terminalType = OUTPUT_SPEAKER;
			break;
		case kIOAudioSelectorControlSelectionValueHeadphones:
			terminalType = OUTPUT_HEADPHONES;
			break;
		case kIOAudioSelectorControlSelectionValueLine:
			terminalType = EXTERNAL_LINE_CONNECTOR;
			break;
		case kIOAudioSelectorControlSelectionValueSPDIF:
			terminalType = EXTERNAL_SPDIF_INTERFACE;
			break;
		case kIOAudioSelectorControlSelectionValueInternalMicrophone:
			terminalType = INPUT_MICROPHONE;
			break;
		case kIOAudioSelectorControlSelectionValueExternalMicrophone:
			terminalType = INPUT_DESKTOP_MICROPHONE;
			break;
		default:
			terminalType = OUTPUT_UNDEFINED;
	}

	return terminalType;
}

UInt32 AppleOnboardAudio::getCharCodeForString (OSString * string) {
	UInt32								charCode;

	if (string->isEqualTo (kInternalSpeakers)) {
		charCode = kIOAudioOutputPortSubTypeInternalSpeaker;
	} else if (string->isEqualTo (kExternalSpeakers)) {
		charCode = kIOAudioOutputPortSubTypeExternalSpeaker;
	} else if (string->isEqualTo (kHeadphones)) {
		charCode = kIOAudioOutputPortSubTypeHeadphones;
	} else if (string->isEqualTo (kLineOut)) {
		charCode = kIOAudioOutputPortSubTypeLine;
	} else if (string->isEqualTo (kInternalMic)) {
		charCode = kIOAudioInputPortSubTypeInternalMicrophone;
	} else if (string->isEqualTo (kExternalMic)) {
		charCode = kIOAudioInputPortSubTypeExternalMicrophone;
	} else if (string->isEqualTo (kLineIn)) {
		charCode = kIOAudioInputPortSubTypeLine;
	} else if (string->isEqualTo (kDigitalIn) || string->isEqualTo (kDigitalOut)) {
		charCode = kIOAudioInputPortSubTypeSPDIF;
	} else {
		charCode = 0x3F3F3F3F; 			// because '????' is a trigraph....
	}

	return charCode;
}

UInt32 AppleOnboardAudio::getCharCodeForIntCode (UInt32 inCode) {
	UInt32								charCode;

	if (kInternalSpeakerStatus == inCode) {
		charCode = kIOAudioOutputPortSubTypeInternalSpeaker;
	} else if (kExtSpeakersStatus == inCode) {
		charCode = kIOAudioOutputPortSubTypeExternalSpeaker;
	} else if (kHeadphoneStatus == inCode) {
		charCode = kIOAudioOutputPortSubTypeHeadphones;
	} else if (kLineOutStatus == inCode) {
		charCode = kIOAudioOutputPortSubTypeLine;
	} else if (kInputMicStatus == inCode) {
		charCode = kIOAudioInputPortSubTypeInternalMicrophone;
	} else if (kExternalMicInStatus == inCode) {
		charCode = kIOAudioInputPortSubTypeExternalMicrophone;
	} else if (kLineInStatus == inCode) {
		charCode = kIOAudioInputPortSubTypeLine;
	} else if ((kDigitalInInsertStatus == inCode) || (kDigitalInRemoveStatus == inCode)) {
		charCode = kIOAudioInputPortSubTypeSPDIF;
	} else if (kDigitalOutStatus == inCode) {
		charCode = kIOAudioOutputPortSubTypeSPDIF;
	} else {
		charCode = 0x3F3F3F3F; 			// because '????' is a trigraph....
	}

	return charCode;
}

OSString * AppleOnboardAudio::getStringForCharCode (UInt32 charCode) {
	OSString *							theString;

	switch (charCode) {
		case kIOAudioOutputPortSubTypeInternalSpeaker:
			theString = OSString::withCString ("Internal Speakers");
			break;
		case kIOAudioOutputPortSubTypeExternalSpeaker:
			theString = OSString::withCString ("External Speakers");
			break;
		case kIOAudioOutputPortSubTypeHeadphones:
			theString = OSString::withCString ("Headphones");
			break;
		case kIOAudioOutputPortSubTypeLine:
			theString = OSString::withCString ("Line");
			break;
		case kIOAudioOutputPortSubTypeSPDIF:
			theString = OSString::withCString ("Digital");
			break;
		case kIOAudioInputPortSubTypeInternalMicrophone:
			theString = OSString::withCString ("Internal Microphone");
			break;
		case kIOAudioInputPortSubTypeExternalMicrophone:
			theString = OSString::withCString ("External Microphone");
			break;
		default:
			theString = NULL;
			break;
	}

	return theString;
}

char * AppleOnboardAudio::getConnectionKeyFromCharCode (const SInt32 inSelection, const UInt32 inDirection) {
	char * selectedOutput;

	switch (inSelection) {
		// output
		case kIOAudioOutputPortSubTypeInternalSpeaker:
			selectedOutput = kInternalSpeakers;
			break;
		case kIOAudioOutputPortSubTypeExternalSpeaker:
			selectedOutput = kExternalSpeakers;
			break;
		case kIOAudioOutputPortSubTypeHeadphones:
			selectedOutput = kHeadphones;
			break;
		// input
		case kIOAudioInputPortSubTypeInternalMicrophone:
			selectedOutput = kInternalMic;
			break;
		case kIOAudioInputPortSubTypeExternalMicrophone:
			selectedOutput = kExternalMic;
			break;
		// input/output
		case kIOAudioOutputPortSubTypeSPDIF:
			if (kIOAudioStreamDirectionOutput == inDirection) {
				selectedOutput = kDigitalOut;
			} else {
				selectedOutput = kDigitalIn;
			}
			break;
		case kIOAudioOutputPortSubTypeLine:
			if (kIOAudioStreamDirectionOutput == inDirection) {
				selectedOutput = kLineOut;
			} else {
				selectedOutput = kLineIn;
			}
			break;
		default:
			selectedOutput = 0;
			break;
	}

	return selectedOutput;
}

IOReturn AppleOnboardAudio::createInputSelectorControl (void) {
	OSArray *							inputsList;
	OSString *							inputString;
#if !LOCALIZABLE
	OSString *							selectionString;
#endif
	IOReturn							result;
	UInt32								inputsCount;
	UInt32								inputSelection;
	UInt32								index;

	result = kIOReturnError;
	inputsList = OSDynamicCast (OSArray, getLayoutEntry (kInputsList, this));
	FailIf (NULL == inputsList, Exit);

	inputsCount = inputsList->getCount ();
	inputString = OSDynamicCast (OSString, inputsList->getObject (0));
	FailIf (NULL == inputString, Exit);

	inputSelection = getCharCodeForString (inputString);

	mInputSelector = IOAudioSelectorControl::createInputSelector (inputSelection, kIOAudioControlChannelIDAll);
	if (NULL != mInputSelector) {
		mDriverDMAEngine->addDefaultAudioControl (mInputSelector);
		mInputSelector->setValueChangeHandler ((IOAudioControl::IntValueChangeHandler)inputControlChangeHandler, this);
		for (index = 0; index < inputsCount; index++) {
			inputString = OSDynamicCast (OSString, inputsList->getObject (index));
			FailIf (NULL == inputString, Exit);
			inputSelection = getCharCodeForString (inputString);
#if LOCALIZABLE
			mInputSelector->addAvailableSelection (inputSelection, inputString);
#else
			selectionString = getStringForCharCode (inputSelection);
			mInputSelector->addAvailableSelection (inputSelection, selectionString);
			selectionString->release ();
#endif
		}
	}

	debugIOLog (3, "AppleOnboardAudio::createInputSelectorControl - mInputSelector = %p", mInputSelector);

	result = kIOReturnSuccess;

Exit:
	return result;
}

IOReturn AppleOnboardAudio::createOutputSelectorControl (void) {
	char								outputSelectionCString[5];
	OSDictionary *						theDictionary;
	OSNumber *							terminalTypeNum;
	OSString *							outputString;
	OSString *							outputSelectionString;
	OSArray *							outputsList;
#if !LOCALIZABLE
	OSString *							selectionString;
#endif
	IOReturn							result;
	UInt32								outputsCount;
	UInt32								outputSelection;
	UInt32								index;
	UInt16								terminalType;

	result = kIOReturnError;
	outputsList = OSDynamicCast (OSArray, getLayoutEntry (kOutputsList, this));
	if ( NULL != outputsList ) {
		outputsCount = outputsList->getCount ();
		outputString = OSDynamicCast (OSString, outputsList->getObject (0));
		FailIf (NULL == outputString, Exit);
	
		theDictionary = OSDictionary::withCapacity (outputsCount);
		FailIf (NULL == theDictionary, Exit);
	
		outputSelection = getCharCodeForString (outputString);
		mOutputSelector = IOAudioSelectorControl::createOutputSelector (outputSelection, kIOAudioControlChannelIDAll);
		if ( NULL == mOutputSelector ) { debugIOLog ( 3, "createOutputSelector for %4s FAILED", (char*)&outputSelection ); }
		FailIf (NULL == mOutputSelector, Exit);
		
		mDriverDMAEngine->addDefaultAudioControl (mOutputSelector);
		mOutputSelector->setValueChangeHandler ((IOAudioControl::IntValueChangeHandler)outputControlChangeHandler, this);
		for (index = 0; index < outputsCount; index++) {
			outputString = OSDynamicCast (OSString, outputsList->getObject (index));
			FailIf (NULL == outputString, Exit);
			outputSelection = getCharCodeForString (outputString);
			//	Radar 3413551 requires that the selector values for external or internal speakers be dynamically
			//	removed or added to the output selector control when the external speaker detect changes state.
			//	This can only be done if a history is maintained that indicates that both of these selections
			//	were available when the output selector control was constructed.  The control is constructed
			//	with all selectors from the dictionary but if both an external and an internal speaker control
			//	selector are added to the output selector control then the protectedInterruptEventHandler() will
			//	remove the unavailable selector.  
			//	[3413551]	begin	{
			switch (outputSelection) {
				case kIOAudioOutputPortSubTypeInternalSpeaker:
					mInternalSpeakerOutputString = outputString;
					break;
				case kIOAudioOutputPortSubTypeExternalSpeaker:
					mExternalSpeakerOutputString = outputString;
					break;
				case kIOAudioOutputPortSubTypeLine:
					mLineOutputString = outputString;
					break;
				case kIOAudioOutputPortSubTypeSPDIF:
					mDigitalOutputString = outputString;
					break;
				case kIOAudioOutputPortSubTypeHeadphones:
					mHeadphoneOutputString = outputString;
					break;
				default:
					debugIOLog (2, "AppleOnboardAudio::createOutputSelectorControl: unknown output selection");
			}
			//	}	end		[3413551]
			terminalType = getTerminalTypeForCharCode (outputSelection);
			terminalTypeNum = OSNumber::withNumber (terminalType, 16);
			FailIf (NULL == terminalTypeNum, Exit);
			*(UInt32 *)outputSelectionCString = outputSelection;
			outputSelectionCString[4] = 0;
			outputSelectionString = OSString::withCString (outputSelectionCString);
			FailIf (NULL == outputSelectionString, Exit);
			theDictionary->setObject (outputSelectionString, terminalTypeNum);
			terminalTypeNum->release ();
			outputSelectionString->release ();
#if LOCALIZABLE
			debugIOLog (3,  "mOutputSelector->addAvailableSelection ( '%4s', %p )", (char*)&outputSelection, outputString );
			mOutputSelector->addAvailableSelection (outputSelection, outputString);
#else
			selectionString = getStringForCharCode (outputSelection);
			mOutputSelector->addAvailableSelection (outputSelection, selectionString);
			selectionString->release ();
#endif
		}
		//	[3413551]	If both an internal and external speaker selector were added then these selectors must be
		//	removed here.  The protectedInterruptEventHandler() will dynamically add these selectors as appropriate
		//	for the jack state.  Removal of these selectors will prevent adding redundant selectors as there is 
		//	no test to determine if the selector has already been added in order to avoid redundantly adding a selector.
		if ( ( NULL != mInternalSpeakerOutputString ) && ( NULL != mExternalSpeakerOutputString ) ) {
			if ( kGPIO_Connected == mPlatformInterface->getSpeakerConnected() ) {
				mOutputSelector->removeAvailableSelection ( kIOAudioOutputPortSubTypeInternalSpeaker );
			} else {
				mOutputSelector->removeAvailableSelection ( kIOAudioOutputPortSubTypeExternalSpeaker );
			}
		}
		if ( ( NULL != mDigitalOutputString ) && ( NULL != mLineOutputString ) && (kGPIO_Unknown != mPlatformInterface->getComboOutJackTypeConnected ()) ) {
			if ( kGPIO_Connected == mPlatformInterface->getLineOutConnected () ) {
				mOutputSelector->removeAvailableSelection ( kIOAudioOutputPortSubTypeSPDIF );
			} else if ( kGPIO_Connected == mPlatformInterface->getDigitalOutConnected () ) {
				mOutputSelector->removeAvailableSelection ( kIOAudioOutputPortSubTypeLine );
			} else {
				mOutputSelector->removeAvailableSelection ( kIOAudioOutputPortSubTypeSPDIF );
				mOutputSelector->removeAvailableSelection ( kIOAudioOutputPortSubTypeLine );
			}
		}
	
		mDriverDMAEngine->setProperty ("MappingDictionary", theDictionary);
	}
	
	debugIOLog (3, "AppleOnboardAudio::createOutputSelectorControl - mOutputSelector = %p", mOutputSelector);

	result = kIOReturnSuccess;

Exit:
	return result;
}

AudioHardwareObjectInterface * AppleOnboardAudio::getPluginObjectForConnection (const char * entry) {
	AudioHardwareObjectInterface *		thePluginObject;
	OSDictionary *						dictEntry;
	OSString *							pluginIDMatch;

	thePluginObject = NULL;
	pluginIDMatch = NULL;
	dictEntry = NULL;
	
	dictEntry = OSDynamicCast (OSDictionary, getLayoutEntry (entry, this));
	FailIf (NULL == dictEntry, Exit);

	pluginIDMatch = OSDynamicCast (OSString, dictEntry->getObject (kPluginID));
	FailIf (NULL == pluginIDMatch, Exit);

	thePluginObject = getPluginObjectWithName (pluginIDMatch);
	
	debugIOLog (3, "AppleOnboardAudio::getPluginObjectForConnection - pluginID = %s", pluginIDMatch->getCStringNoCopy());

Exit:
	return thePluginObject;
}

GpioAttributes AppleOnboardAudio::getInputDataMuxForConnection (const char * entry) {
	OSDictionary *						dictEntry;
	OSNumber *							inputDataMuxOSNumber;
	GpioAttributes						result;

	dictEntry = NULL;
	result = kGPIO_Unknown;
	
	dictEntry = OSDynamicCast (OSDictionary, getLayoutEntry (entry, this));
	FailIf (NULL == dictEntry, Exit);

	inputDataMuxOSNumber = OSDynamicCast (OSNumber, dictEntry->getObject (kInputDataMux));
	FailIf (NULL == inputDataMuxOSNumber, Exit);

	if ( 0 == inputDataMuxOSNumber->unsigned32BitValue() ) {
		result = kGPIO_MuxSelectDefault;
	} else {
		result = kGPIO_MuxSelectAlternate;
	}
	
	debugIOLog (3, "AppleOnboardAudio::getInputDataMuxForConnection - GpioAttributes result = %d", result);

Exit:
	return result;
}

AudioHardwareObjectInterface * AppleOnboardAudio::getPluginObjectWithName (OSString * inName) {
	AudioHardwareObjectInterface *		thePluginObject;
    OSDictionary *						AOAprop;
	OSString *							thePluginID;
	UInt32								index;
	UInt32								count;
	Boolean								found;

	thePluginObject = NULL;

	count = mPluginObjects->getCount ();
	found = FALSE;
	index = 0;
	while (!found && index < count) {
		thePluginObject = getIndexedPluginObject (index);
		FailIf (NULL == thePluginObject, Exit);
		AOAprop = OSDynamicCast (OSDictionary, thePluginObject->getProperty (kPluginPListAOAAttributes));
		FailIf (NULL == AOAprop, Exit);
		thePluginID = OSDynamicCast (OSString, AOAprop->getObject (kPluginID));
		FailIf (NULL == thePluginID, Exit);

		if (thePluginID->isEqualTo (inName)) {
			debugIOLog (7, "AppleOnboardAudio found matching plugin with ID %s", thePluginID->getCStringNoCopy());
			found = TRUE;
		}
		index++;
	}
	
Exit:	
	return thePluginObject;
}

IOReturn AppleOnboardAudio::createInputGainControls () {
	AudioHardwareObjectInterface *		thePluginObject;
	char *								selectedInput;
	IOReturn							result;
	IOFixed								mindBGain = 0;
	IOFixed								maxdBGain = 0;
	UInt32								curSelection = 0;
	SInt32								minGain = 0;
	SInt32								maxGain = 0;

	result = kIOReturnError;

	curSelection = mInputSelector->getIntValue ();
	
	selectedInput = getConnectionKeyFromCharCode (curSelection, kIOAudioStreamDirectionInput);

	setUseInputGainControls (selectedInput);

	if ( kNoInputGainControls != mUseInputGainControls ) {
		thePluginObject = getPluginObjectForConnection (selectedInput);
		FailIf (NULL == thePluginObject, Exit);

		debugIOLog (3, "creating input gain controls for input %s", selectedInput);
	
		mindBGain = thePluginObject->getMinimumdBGain ();
		maxdBGain = thePluginObject->getMaximumdBGain ();
		minGain = thePluginObject->getMinimumGain ();
		maxGain = thePluginObject->getMaximumGain ();
	
	}
	if ( kStereoInputGainControls == mUseInputGainControls ) {
		mInLeftGainControl = IOAudioLevelControl::createVolumeControl ((maxGain-minGain) / 2, minGain, maxGain, mindBGain, maxdBGain, kIOAudioControlChannelIDDefaultLeft, kIOAudioControlChannelNameLeft, 0, kIOAudioControlUsageInput);
		if (NULL != mInLeftGainControl) {
			mDriverDMAEngine->addDefaultAudioControl (mInLeftGainControl);
			mInLeftGainControl->setValueChangeHandler ((IOAudioControl::IntValueChangeHandler)inputControlChangeHandler, this);
			// Don't release it because we might reference it later
		}
	
		mInRightGainControl = IOAudioLevelControl::createVolumeControl ((maxGain-minGain) / 2, minGain, maxGain, mindBGain, maxdBGain, kIOAudioControlChannelIDDefaultRight, kIOAudioControlChannelNameRight, 0, kIOAudioControlUsageInput);
		if (NULL != mInRightGainControl) {
			mDriverDMAEngine->addDefaultAudioControl (mInRightGainControl);
			mInRightGainControl->setValueChangeHandler ((IOAudioControl::IntValueChangeHandler)inputControlChangeHandler, this);
			// Don't release it because we might reference it later
		}
	} else if ( kMonoInputGainControl == mUseInputGainControls ){
		mInMasterGainControl = IOAudioLevelControl::createVolumeControl ((maxGain-minGain) / 2, minGain, maxGain, mindBGain, maxdBGain, kIOAudioControlChannelIDAll, kIOAudioControlChannelNameAll, 0, kIOAudioControlUsageInput);
		if (NULL != mInMasterGainControl) {
			mDriverDMAEngine->addDefaultAudioControl ( mInMasterGainControl );
			mInMasterGainControl->setValueChangeHandler ( (IOAudioControl::IntValueChangeHandler)inputControlChangeHandler, this );
			// Don't release it because we might reference it later
		}
	}

	//	[3281535]	begin {
	removePlayThruControl ();
	setUsePlaythroughControl ( selectedInput );
	if ( mUsePlaythroughControl ) {
		createPlayThruControl ();
	}
	//	[3281535]	} end

	result = kIOReturnSuccess;

Exit:
	return result;
}

OSArray * AppleOnboardAudio::getControlsArray (const char * inSelectedOutput) {
	OSArray *							theArray;
	OSDictionary *						theOutput;
	
	theArray = NULL;
	
	theOutput = OSDynamicCast(OSDictionary, getLayoutEntry(inSelectedOutput, this));
	FailIf (NULL == theOutput, Exit);
	theArray = OSDynamicCast(OSArray, theOutput->getObject(kControls));
	
Exit:
	return theArray;
}

UInt32 AppleOnboardAudio::getNumHardwareEQBandsForCurrentOutput () {
	OSDictionary *						AOApropOutput;
	OSNumber *							numBandsNumber;
	UInt32								numBands;
	
	numBands = 0;
	AOApropOutput = OSDynamicCast (OSDictionary, mCurrentOutputPlugin->getProperty (kPluginPListAOAAttributes));
	if (NULL != AOApropOutput) {
		numBandsNumber = OSDynamicCast (OSNumber, AOApropOutput->getObject (kPluginPListNumHardwareEQBands));
		if (NULL != numBandsNumber) {
			numBands = numBandsNumber->unsigned32BitValue();
		}
	}
	return numBands;
}

UInt32 AppleOnboardAudio::getMaxVolumeOffsetForOutput (const UInt32 inCode) {
	char *			connectionString;
	
	connectionString = getConnectionKeyFromCharCode (inCode, kIOAudioStreamDirectionOutput);
	return getMaxVolumeOffsetForOutput (connectionString);
}

UInt32 AppleOnboardAudio::getMaxVolumeOffsetForOutput (const char * inSelectedOutput) {
	return 0;
}

void AppleOnboardAudio::setSoftwareOutputDSP (const char * inSelectedOutput) {
	return;
}

// [3306305]
void AppleOnboardAudio::setSoftwareInputDSP (const char * inSelectedInput) {
	return;
}

UInt32 AppleOnboardAudio::setClipRoutineForOutput (const char * inSelectedOutput) {
	OSDictionary *						theOutput;
	OSString *							clipRoutineString;
	OSArray *							theArray;
	IOReturn							result;
	UInt32								arrayCount;
	UInt32								index;
	
	debugIOLog (3,  "+ AppleOnboardAudio::setClipRoutineForOutput ( '%4s' )", inSelectedOutput );
	result = kIOReturnSuccess;
	theArray = NULL;
	
	theOutput = OSDynamicCast(OSDictionary, getLayoutEntry(inSelectedOutput, this));
	FailIf (NULL == theOutput, Exit);
	
	theArray = OSDynamicCast(OSArray, theOutput->getObject(kClipRoutines));
	FailIf (NULL == theArray, Exit);
	
	arrayCount = theArray->getCount();

	mDriverDMAEngine->resetOutputClipOptions();
	
	for (index = 0; index < arrayCount; index++) {
		clipRoutineString = OSDynamicCast(OSString, theArray->getObject(index));
		debugIOLog (3, "getClipRoutineForOutput: clip routine[%ld] = %s", index, clipRoutineString->getCStringNoCopy());

		if (clipRoutineString->isEqualTo (kStereoToRightChanClipString)) {
			mDriverDMAEngine->setRightChanMixed (true);
		} else {
			mDriverDMAEngine->resetOutputClipOptions();
		}
	}	

	result = kIOReturnSuccess;
Exit:
	debugIOLog (3,  "- AppleOnboardAudio::setClipRoutineForOutput ( '%4s' ) returns %X", inSelectedOutput, result );
	return result;
}

UInt32 AppleOnboardAudio::setClipRoutineForInput (const char * inSelectedInput) {
	OSDictionary *						theInput;
	OSString *							clipRoutineString;
	OSArray *							theArray;
	IOReturn							result;
	UInt32								arrayCount;
	UInt32								index;
	
	result = kIOReturnSuccess;
	theArray = NULL;
	
	theInput = OSDynamicCast(OSDictionary, getLayoutEntry(inSelectedInput, this));
	FailIf (NULL == theInput, Exit);
	
	theArray = OSDynamicCast(OSArray, theInput->getObject(kClipRoutines));
	FailIf (NULL == theArray, Exit);
	
	arrayCount = theArray->getCount();

	mDriverDMAEngine->resetInputClipOptions();
	
	for (index = 0; index < arrayCount; index++) {
		clipRoutineString = OSDynamicCast(OSString, theArray->getObject(index));
		debugIOLog (3, "getClipRoutineForInput: clip routine[%ld] = %s", index, clipRoutineString->getCStringNoCopy());

		if (clipRoutineString->isEqualTo (kCopyLeftToRight)) {
			mDriverDMAEngine->setDualMonoMode (e_Mode_CopyLeftToRight);
		} else if (clipRoutineString->isEqualTo (kCopyRightToLeft)) {
			mDriverDMAEngine->setDualMonoMode (e_Mode_CopyRightToLeft);
		}
	}	

	result = kIOReturnSuccess;
Exit:
	debugIOLog (3, "getClipRoutineForInput returns %X", result);
	return result;
}

void AppleOnboardAudio::cacheOutputVolumeLevels (AudioHardwareObjectInterface * thePluginObject) {
	if (NULL != mOutMasterVolumeControl) {
		mCurrentOutputPlugin->setProperty(kPluginPListMasterVol, mOutMasterVolumeControl->getValue ());
	} else {
		OSNumber *				theNumber;
		
		theNumber = OSNumber::withNumber ((unsigned long long)-1, 32);
		if (NULL != mOutLeftVolumeControl) {
			mCurrentOutputPlugin->setProperty(kPluginPListLeftVol, mOutLeftVolumeControl->getValue ());
			mCurrentOutputPlugin->setProperty(kPluginPListMasterVol, theNumber);
		}
		if (NULL != mOutRightVolumeControl) {
			mCurrentOutputPlugin->setProperty(kPluginPListRightVol, mOutRightVolumeControl->getValue ());
			mCurrentOutputPlugin->setProperty(kPluginPListMasterVol, theNumber);
		}
		theNumber->release ();
	}	
	return;
}

void AppleOnboardAudio::cacheInputGainLevels (AudioHardwareObjectInterface * thePluginObject) {
	if (NULL != mInLeftGainControl) {
		mCurrentInputPlugin->setProperty(kPluginPListLeftGain, mInLeftGainControl->getValue ());
	}
	if (NULL != mInRightGainControl) {
		mCurrentInputPlugin->setProperty(kPluginPListRightGain, mInRightGainControl->getValue ());
	}
	if ( NULL != mInMasterGainControl ) {
		mCurrentInputPlugin->setProperty ( kPluginPListMasterGain, mInMasterGainControl->getValue () );
	}
	return;
}

IOReturn AppleOnboardAudio::createOutputVolumeControls (void) {
	AudioHardwareObjectInterface *		thePluginObject;
	OSArray *							controlsArray;
	OSString *							controlString;
	char *								selectedOutput;
	IOReturn							result;
	UInt32								curSelection;
	UInt32								count;
	UInt32								index;

	debugIOLog (3, "+ AppleOnboardAudio::createOutputVolumeControls");

	result = kIOReturnError;
	if ( NULL != mOutputSelector ) {
		curSelection = mOutputSelector->getIntValue ();
		
		selectedOutput = getConnectionKeyFromCharCode(curSelection, kIOAudioStreamDirectionOutput);
	
		thePluginObject = getPluginObjectForConnection (selectedOutput);
		FailIf (NULL == thePluginObject, Exit);
	
		AdjustOutputVolumeControls (thePluginObject, curSelection);
	
		controlsArray = getControlsArray (selectedOutput);
		FailIf (NULL == controlsArray, Exit);
		count = controlsArray->getCount ();
		for (index = 0; index < count; index++) {
			controlString = OSDynamicCast (OSString, controlsArray->getObject (index));
			if (controlString->isEqualTo (kMuteControlString)) {
				mOutMuteControl = IOAudioToggleControl::createMuteControl (false, kIOAudioControlChannelIDAll, kIOAudioControlChannelNameAll, 0, kIOAudioControlUsageOutput);
				if (NULL != mOutMuteControl) {
					mDriverDMAEngine->addDefaultAudioControl(mOutMuteControl);
					mOutMuteControl->setValueChangeHandler((IOAudioControl::IntValueChangeHandler)outputControlChangeHandler, this);
					// Don't release it because we might reference it later
				}
				break;		// out of for loop now that we have created the mute control
			}
		}
	} else {
		result = kIOReturnSuccess;
	}

Exit:

	debugIOLog (3, "- AppleOnboardAudio::createOutputVolumeControls");
	return result;
}

IOReturn AppleOnboardAudio::createDefaultControls () {
	AudioHardwareObjectInterface *		thePluginObject;
	IOAudioToggleControl *				outHeadLineDigExclusiveControl;
    OSDictionary *						AOAprop;
	OSBoolean *							clockSelectBoolean;
	OSArray *							inputListArray;
	UInt32								index;
	UInt32								count;
    IOReturn							result;
	Boolean								hasPlaythrough;
	Boolean								hasInput;
	
	debugIOLog (3, "+ AppleOnboardAudio::createDefaultControls");

	hasPlaythrough = FALSE;
	result = kIOReturnError;
	FailIf (NULL == mDriverDMAEngine, Exit);

	count = mPluginObjects->getCount ();
	for (index = 0; index < count; index++) {
		thePluginObject = getIndexedPluginObject (index);
		FailIf (NULL == thePluginObject, Exit);
		AOAprop = OSDynamicCast (OSDictionary, thePluginObject->getProperty (kPluginPListAOAAttributes));
		FailIf (NULL == AOAprop, Exit);
	}

	result = createOutputSelectorControl ();
	FailIf (kIOReturnSuccess != result, Exit);

	// The call to createOutputVolumeControls has to come after the creation of the selector control because
	// it references the current value of the selector control to know what controls to create.
	createOutputVolumeControls ();

	mPRAMVolumeControl = IOAudioLevelControl::create (PRAMToVolumeValue (), 0, 7, 0x00120000, 0, kIOAudioControlChannelIDAll, "BootBeepVolume", 0, kIOAudioLevelControlSubTypePRAMVolume, kIOAudioControlUsageOutput);
	if (NULL != mPRAMVolumeControl) {
		mDriverDMAEngine->addDefaultAudioControl (mPRAMVolumeControl);
		mPRAMVolumeControl->setValueChangeHandler ((IOAudioControl::IntValueChangeHandler)outputControlChangeHandler, this);
		mPRAMVolumeControl->release ();
		mPRAMVolumeControl = NULL;
	}

	// Create a toggle control for reporting the status of the headphone jack
	mHeadphoneConnected = IOAudioToggleControl::create (FALSE, kIOAudioControlChannelIDAll, kIOAudioControlChannelNameAll, 0, kIOAudioControlTypeJack, kIOAudioControlUsageOutput);

	if (NULL != mHeadphoneConnected) {
		mDriverDMAEngine->addDefaultAudioControl (mHeadphoneConnected);
		mHeadphoneConnected->setReadOnlyFlag ();		// 3292105
		// no value change handler because this isn't a settable control
		// Don't release it because we might reference it later
	}

	// [3276398] only build control if we DON'T have a combo jack
	if (kGPIO_Unknown == mPlatformInterface->getComboOutJackTypeConnected ()) {
		outHeadLineDigExclusiveControl = IOAudioToggleControl::create (mHeadLineDigExclusive, kIOAudioControlChannelIDAll, kIOAudioControlChannelNameAll, 0, 'hpex', kIOAudioControlUsageOutput);
		if (NULL != outHeadLineDigExclusiveControl) {
			mDriverDMAEngine->addDefaultAudioControl (outHeadLineDigExclusiveControl);
			outHeadLineDigExclusiveControl->setValueChangeHandler ((IOAudioControl::IntValueChangeHandler)outputControlChangeHandler, this);
			outHeadLineDigExclusiveControl->release ();
		}
	}

	inputListArray = OSDynamicCast (OSArray, getLayoutEntry (kInputsList, this));
	hasInput = (NULL != inputListArray);
	
	if (hasInput) {
		createInputSelectorControl ();
	
		createInputGainControls ();
	}

	clockSelectBoolean = OSDynamicCast ( OSBoolean, getLayoutEntry (kExternalClockSelect, this) );
	if (NULL != clockSelectBoolean) {
		if (TRUE == clockSelectBoolean->getValue ()) {
			mExternalClockSelector = IOAudioSelectorControl::create (kClockSourceSelectionInternal, kIOAudioControlChannelIDAll, kIOAudioControlChannelNameAll, 0, kIOAudioSelectorControlSubTypeClockSource, kIOAudioControlUsageInput);		
			FailIf (NULL == mExternalClockSelector, Exit);
			mDriverDMAEngine->addDefaultAudioControl (mExternalClockSelector);
			mExternalClockSelector->setValueChangeHandler ((IOAudioControl::IntValueChangeHandler)inputControlChangeHandler, this);
			mExternalClockSelector->addAvailableSelection (kClockSourceSelectionInternal, kInternalClockString);
			mExternalClockSelector->addAvailableSelection (kClockSourceSelectionExternal, kExternalClockString);
			// don't release, may be used in event of loss of clock lock
		}
	}
  	
	result = kIOReturnSuccess;
Exit:    
    debugIOLog (3, "- %lX = AppleOnboardAudio::createDefaultControls", result);
    return result;
}

#pragma mark +IOAUDIO CONTROL HANDLERS
// --------------------------------------------------------------------------
// You either have only a master volume control, or you have both volume controls.
IOReturn AppleOnboardAudio::AdjustOutputVolumeControls (AudioHardwareObjectInterface * thePluginObject, UInt32 selectionCode) {
	IOFixed								mindBVol;
	IOFixed								maxdBVol;
	SInt32								minVolume;
	SInt32								maxVolume;
	Boolean								hasMaster;
	Boolean								hasLeft;
	Boolean								hasRight;
	Boolean								stereoOutputConnected;

	FailIf (NULL == mDriverDMAEngine, Exit);

	mindBVol = thePluginObject->getMinimumdBVolume ();
	maxdBVol = thePluginObject->getMaximumdBVolume ();
	minVolume = thePluginObject->getMinimumVolume ();
	maxVolume = thePluginObject->getMaximumVolume ();

	maxVolume += getMaxVolumeOffsetForOutput (selectionCode);

	debugIOLog (3, "AppleOnboardAudio::AdjustOutputVolumeControls - mindBVol %lX, maxdBVol %lX, minVolume %ld, maxVolume %ld", mindBVol, maxdBVol, minVolume, maxVolume);

	mDriverDMAEngine->pauseAudioEngine ();
	mDriverDMAEngine->beginConfigurationChange ();

	hasMaster = hasMasterVolumeControl (selectionCode);
	hasLeft = hasLeftVolumeControl (selectionCode);
	hasRight = hasRightVolumeControl (selectionCode);

	// [3339273], don't remove stereo controls if headphones, external speakers or lineout are connected (i.e. any stereo thing)
	if ((kGPIO_Connected == mPlatformInterface->getHeadphoneConnected ()) || 
		(kGPIO_Connected == mPlatformInterface->getSpeakerConnected ()) ||
		(kGPIO_Connected == mPlatformInterface->getLineOutConnected ()) ||
		(kGPIO_Connected == mPlatformInterface->getDigitalOutConnected ())) {
		stereoOutputConnected = true;
	} else {
		stereoOutputConnected = false;
	}
		
	if ((TRUE == hasMaster && NULL == mOutMasterVolumeControl) || (FALSE == hasMaster && NULL != mOutMasterVolumeControl) ||
		(NULL != mOutMasterVolumeControl && mOutMasterVolumeControl->getMinValue () != minVolume) ||
		(NULL != mOutMasterVolumeControl && mOutMasterVolumeControl->getMaxValue () != maxVolume)) {

		if (TRUE == hasMaster) {
			// We have only the master volume control (possibly not created yet) and have to remove the other volume controls (possibly don't exist)
			if (NULL != mOutMasterVolumeControl) {
				mOutMasterVolumeControl->setMinValue (minVolume);
				mOutMasterVolumeControl->setMinDB (mindBVol);
				mOutMasterVolumeControl->setMaxValue (maxVolume);
				mOutMasterVolumeControl->setMaxDB (maxdBVol);
				if (mOutMasterVolumeControl->getIntValue () > maxVolume) {
					mOutMasterVolumeControl->setValue (maxVolume);
				}
				mOutMasterVolumeControl->flushValue ();
			} else {				
				createMasterVolumeControl (mindBVol, maxdBVol, minVolume, maxVolume);
			}
		} else {
			removeMasterVolumeControl();
		}
	}

	if ((TRUE == hasLeft && NULL == mOutLeftVolumeControl) || (FALSE == hasLeft && NULL != mOutLeftVolumeControl) ||
		(NULL != mOutLeftVolumeControl && mOutLeftVolumeControl->getMinValue () != minVolume) ||
		(NULL != mOutLeftVolumeControl && mOutLeftVolumeControl->getMaxValue () != maxVolume)) {
		if (TRUE == hasLeft) {
			if (NULL != mOutLeftVolumeControl) {
				mOutLeftVolumeControl->setMinValue (minVolume);
				mOutLeftVolumeControl->setMinDB (mindBVol);
				mOutLeftVolumeControl->setMaxValue (maxVolume);
				mOutLeftVolumeControl->setMaxDB (maxdBVol);
				if (mOutLeftVolumeControl->getIntValue () > maxVolume) {
					mOutLeftVolumeControl->setValue (maxVolume);
				}
				mOutLeftVolumeControl->flushValue ();
			} else {
				createLeftVolumeControl(mindBVol, maxdBVol, minVolume, maxVolume);
			}
		} else {
			if (!stereoOutputConnected) {
				removeLeftVolumeControl();
			}
		}
	}

	if ((TRUE == hasRight && NULL == mOutRightVolumeControl) || (FALSE == hasRight && NULL != mOutRightVolumeControl) ||
		(NULL != mOutRightVolumeControl && mOutRightVolumeControl->getMinValue () != minVolume) ||
		(NULL != mOutRightVolumeControl && mOutRightVolumeControl->getMaxValue () != maxVolume)) {
		if (TRUE == hasRight) {
			if (NULL != mOutRightVolumeControl) {
				mOutRightVolumeControl->setMinValue (minVolume);
				mOutRightVolumeControl->setMinDB (mindBVol);
				mOutRightVolumeControl->setMaxValue (maxVolume);
				mOutRightVolumeControl->setMaxDB (maxdBVol);
				if (mOutRightVolumeControl->getIntValue () > maxVolume) {
					mOutRightVolumeControl->setValue (maxVolume);
				}
				mOutRightVolumeControl->flushValue ();
			} else {
				createRightVolumeControl(mindBVol, maxdBVol, minVolume, maxVolume);
			}
		} else {
			if (!stereoOutputConnected) {
				removeRightVolumeControl ();
			}
		}
	}

	mDriverDMAEngine->completeConfigurationChange ();
	mDriverDMAEngine->resumeAudioEngine ();

Exit:
	return kIOReturnSuccess;
}

// --------------------------------------------------------------------------
// You either have only a master volume control, or you have both volume controls.
IOReturn AppleOnboardAudio::AdjustInputGainControls (AudioHardwareObjectInterface * thePluginObject) {
	IOFixed								mindBGain;
	IOFixed								maxdBGain;
	SInt32								minGain;
	SInt32								maxGain;

	FailIf (NULL == mDriverDMAEngine, Exit);

	mindBGain = thePluginObject->getMinimumdBGain ();
	maxdBGain = thePluginObject->getMaximumdBGain ();
	minGain = thePluginObject->getMinimumGain ();
	maxGain = thePluginObject->getMaximumGain ();

	debugIOLog (3, "AppleOnboardAudio::AdjustInputGainControls - mindBGain %lX, maxdBGain %lX, minGain %ld, maxGain %ld", mindBGain, maxdBGain, minGain, maxGain);

	mDriverDMAEngine->pauseAudioEngine ();
	mDriverDMAEngine->beginConfigurationChange ();

	removePlayThruControl ();
	//	[3281535]	begin {
	if ( mUsePlaythroughControl ) {
		createPlayThruControl ();
	}
	//	[3281535]	} end

	if ( kStereoInputGainControls == mUseInputGainControls ) {
		debugIOLog (3, "AdjustInputGainControls - creating input gain controls.");
		// or we have both controls (possibly not created yet) and we have to remove the master volume control (possibly doesn't exist)
		if (NULL != mInLeftGainControl) {
			mInLeftGainControl->setMinValue (minGain);
			mInLeftGainControl->setMinDB (mindBGain);
			mInLeftGainControl->setMaxValue (maxGain);
			mInLeftGainControl->setMaxDB (maxdBGain);
			if (mInLeftGainControl->getIntValue () > maxGain) {
				mInLeftGainControl->setValue (maxGain);
			}
			mInLeftGainControl->flushValue ();
		} else {
			createLeftGainControl(mindBGain, maxdBGain, minGain, maxGain);
		}
	
		if (NULL != mInRightGainControl) {
			mInRightGainControl->setMinValue (minGain);
			mInRightGainControl->setMinDB (mindBGain);
			mInRightGainControl->setMaxValue (maxGain);
			mInRightGainControl->setMaxDB (maxdBGain);
			if (mInRightGainControl->getIntValue () > maxGain) {
				mInRightGainControl->setValue (maxGain);
			}
			mInRightGainControl->flushValue ();
		} else {
			createRightGainControl(mindBGain, maxdBGain, minGain, maxGain);
		}
		removeMasterGainControl();
	} else if ( kMonoInputGainControl == mUseInputGainControls ) {
		if (NULL != mInMasterGainControl) {
			mInMasterGainControl->setMinValue (minGain);
			mInMasterGainControl->setMinDB (mindBGain);
			mInMasterGainControl->setMaxValue (maxGain);
			mInMasterGainControl->setMaxDB (maxdBGain);
			if (mInMasterGainControl->getIntValue () > maxGain) {
				mInMasterGainControl->setValue (maxGain);
			}
			mInMasterGainControl->flushValue ();
		} else {
			createMasterGainControl(mindBGain, maxdBGain, minGain, maxGain);
		}
		removeLeftGainControl();
		removeRightGainControl();
	} else {
		debugIOLog (3, "AdjustInputGainControls - removing input gain controls.");
		removeLeftGainControl();
		removeRightGainControl();
		removeMasterGainControl();
	}

	mDriverDMAEngine->completeConfigurationChange ();
	mDriverDMAEngine->resumeAudioEngine ();

Exit:
	return kIOReturnSuccess;
}

IORegistryEntry * AppleOnboardAudio::FindEntryByNameAndProperty (const IORegistryEntry * start, const char * name, const char * key, UInt32 value) {
	OSIterator				*iterator;
	IORegistryEntry			*theEntry;
	IORegistryEntry			*tmpReg;
	OSNumber				*tmpNumber;

	theEntry = NULL;
	iterator = NULL;
	FailIf (NULL == start, Exit);

	iterator = start->getChildIterator (gIOServicePlane);
	FailIf (NULL == iterator, Exit);

	while (NULL == theEntry && (tmpReg = OSDynamicCast (IORegistryEntry, iterator->getNextObject ())) != NULL) {
		if (strcmp (tmpReg->getName (), name) == 0) {
			tmpNumber = OSDynamicCast (OSNumber, tmpReg->getProperty (key));
			if (NULL != tmpNumber && tmpNumber->unsigned32BitValue () == value) {
				theEntry = tmpReg;
				// remove retain?
				//theEntry->retain();
			}
		}
	}

Exit:
	if (NULL != iterator) {
		iterator->release ();
	}
	return theEntry;
}

void AppleOnboardAudio::createLeftVolumeControl (IOFixed mindBVol, IOFixed maxdBVol, SInt32 minVolume, SInt32 maxVolume) {
	OSNumber * 						theNumber;
	SInt32							leftVol;
	
	leftVol = 0;
	theNumber = OSDynamicCast(OSNumber, mCurrentOutputPlugin->getProperty(kPluginPListLeftVol));
	if (NULL != theNumber) {
		leftVol = theNumber->unsigned32BitValue();
	}

	if (NULL == theNumber || leftVol == 0) {
		theNumber = OSDynamicCast(OSNumber, mCurrentOutputPlugin->getProperty(kPluginPListMasterVol));
		if (NULL != theNumber) {
			leftVol = theNumber->unsigned32BitValue();
			if (leftVol == -1) {
				leftVol = maxVolume / 2;
			}
		}
	}
	
	debugIOLog (3, "createLeftVolumeControl - leftVol = %ld", leftVol);

	mOutLeftVolumeControl = IOAudioLevelControl::createVolumeControl (leftVol, minVolume, maxVolume, mindBVol, maxdBVol,
										kIOAudioControlChannelIDDefaultLeft,
										kIOAudioControlChannelNameLeft,
										0,
										kIOAudioControlUsageOutput);
	if (NULL != mOutLeftVolumeControl) {
		mDriverDMAEngine->addDefaultAudioControl (mOutLeftVolumeControl);
		mOutLeftVolumeControl->setValueChangeHandler ((IOAudioControl::IntValueChangeHandler)outputControlChangeHandler, this);
		mOutLeftVolumeControl->flushValue ();
	}
}

void AppleOnboardAudio::createRightVolumeControl (IOFixed mindBVol, IOFixed maxdBVol, SInt32 minVolume, SInt32 maxVolume) {
	OSNumber * 						theNumber;
	SInt32							rightVol;
	
	rightVol = 0;
	theNumber = OSDynamicCast(OSNumber, mCurrentOutputPlugin->getProperty(kPluginPListRightVol));
	if (NULL != theNumber) {
		rightVol = theNumber->unsigned32BitValue();
	}

	if (NULL == theNumber || rightVol == 0) {
		theNumber = OSDynamicCast(OSNumber, mCurrentOutputPlugin->getProperty(kPluginPListMasterVol));
		if (NULL != theNumber) {
			rightVol = theNumber->unsigned32BitValue();
			if (rightVol == -1) {
				rightVol = maxVolume / 2;
			}
		}
	}

	debugIOLog (3, "createRightVolumeControl - rightVol = %ld", rightVol);

	mOutRightVolumeControl = IOAudioLevelControl::createVolumeControl (rightVol, minVolume, maxVolume, mindBVol, maxdBVol,
										kIOAudioControlChannelIDDefaultRight,
										kIOAudioControlChannelNameRight,
										0,
										kIOAudioControlUsageOutput);
	if (NULL != mOutRightVolumeControl) {
		mDriverDMAEngine->addDefaultAudioControl (mOutRightVolumeControl);
		mOutRightVolumeControl->setValueChangeHandler ((IOAudioControl::IntValueChangeHandler)outputControlChangeHandler, this);
		mOutRightVolumeControl->flushValue ();
	}
}

void AppleOnboardAudio::createMasterVolumeControl (IOFixed mindBVol, IOFixed maxdBVol, SInt32 minVolume, SInt32 maxVolume) {
	OSNumber * 						theNumber;
	SInt32							masterVol;
	
	masterVol = 0;
	theNumber = OSDynamicCast(OSNumber, mCurrentOutputPlugin->getProperty(kPluginPListMasterVol));
	if (NULL != theNumber) {
		masterVol = theNumber->unsigned32BitValue();
	}

	if (-1 == masterVol || NULL == theNumber) {
		theNumber = OSDynamicCast(OSNumber, mCurrentOutputPlugin->getProperty(kPluginPListRightVol));
		if (NULL == theNumber) {
			masterVol = maxVolume;
		} else {
			masterVol = theNumber->unsigned32BitValue();
		}
		theNumber = OSDynamicCast(OSNumber, mCurrentOutputPlugin->getProperty(kPluginPListLeftVol));
		if (NULL != theNumber) {
			masterVol += theNumber->unsigned32BitValue();
		}
		masterVol >>= 1;
	}
	
	debugIOLog (3, "createMasterVolumeControl - masterVol = %ld", masterVol);
	
	mOutMasterVolumeControl = IOAudioLevelControl::createVolumeControl (masterVol, minVolume, maxVolume, mindBVol, maxdBVol,
																		kIOAudioControlChannelIDAll,
																		kIOAudioControlChannelNameAll,
																		0, 
																		kIOAudioControlUsageOutput);

	if (NULL != mOutMasterVolumeControl) {
		mDriverDMAEngine->addDefaultAudioControl(mOutMasterVolumeControl);
		mOutMasterVolumeControl->setValueChangeHandler((IOAudioControl::IntValueChangeHandler)outputControlChangeHandler, this);
		mOutMasterVolumeControl->flushValue ();
	}

}


void AppleOnboardAudio::createLeftGainControl (IOFixed mindBGain, IOFixed maxdBGain, SInt32 minGain, SInt32 maxGain) {
	OSNumber * 						theNumber;
	SInt32							leftGain;
	
	theNumber = OSDynamicCast(OSNumber, mCurrentInputPlugin->getProperty("left-gain"));
	if (NULL == theNumber) {
		leftGain = 0;
	} else {
		leftGain = theNumber->unsigned32BitValue();
	}
	
	debugIOLog (3, "createLeftGainControl - leftVol = %ld", leftGain);

	mInLeftGainControl = IOAudioLevelControl::createVolumeControl (leftGain, minGain, maxGain, mindBGain, maxdBGain,
										kIOAudioControlChannelIDDefaultLeft,
										kIOAudioControlChannelNameLeft,
										0,
										kIOAudioControlUsageInput);
	if (NULL != mInLeftGainControl) {
		mDriverDMAEngine->addDefaultAudioControl (mInLeftGainControl);
		mInLeftGainControl->setValueChangeHandler ((IOAudioControl::IntValueChangeHandler)inputControlChangeHandler, this);
		mInLeftGainControl->flushValue ();
	}
}

void AppleOnboardAudio::createRightGainControl (IOFixed mindBGain, IOFixed maxdBGain, SInt32 minGain, SInt32 maxGain) {
	OSNumber * 						theNumber;
	SInt32							rightGain;
	
	theNumber = OSDynamicCast(OSNumber, mCurrentInputPlugin->getProperty("right-gain"));
	if (NULL == theNumber) {
		rightGain = 0;
	} else {
		rightGain = theNumber->unsigned32BitValue();
	}
	
	debugIOLog (3, "createRightGainControl - rightVol = %ld", rightGain);

	mInRightGainControl = IOAudioLevelControl::createVolumeControl (rightGain, minGain, maxGain, mindBGain, maxdBGain,
										kIOAudioControlChannelIDDefaultRight,
										kIOAudioControlChannelNameRight,
										0,
										kIOAudioControlUsageInput);
	if (NULL != mInRightGainControl) {
		mDriverDMAEngine->addDefaultAudioControl (mInRightGainControl);
		mInRightGainControl->setValueChangeHandler ((IOAudioControl::IntValueChangeHandler)inputControlChangeHandler, this);
		mInRightGainControl->flushValue ();
	}
}

void AppleOnboardAudio::createMasterGainControl (IOFixed mindBGain, IOFixed maxdBGain, SInt32 minGain, SInt32 maxGain) {
	OSNumber * 						theNumber;
	SInt32							masterGain;
	
	theNumber = OSDynamicCast(OSNumber, mCurrentInputPlugin->getProperty("master-gain"));
	if (NULL == theNumber) {
		masterGain = 0;
	} else {
		masterGain = theNumber->unsigned32BitValue();
	}
	
	debugIOLog (3, "createMasterGainControl - masterVol = %ld", masterGain);

	mInMasterGainControl = IOAudioLevelControl::createVolumeControl (masterGain, minGain, maxGain, mindBGain, maxdBGain,
										kIOAudioControlChannelIDAll,
										kIOAudioControlChannelNameAll,
										0,
										kIOAudioControlUsageInput);
	if (NULL != mInMasterGainControl) {
		mDriverDMAEngine->addDefaultAudioControl (mInMasterGainControl);
		mInMasterGainControl->setValueChangeHandler ((IOAudioControl::IntValueChangeHandler)inputControlChangeHandler, this);
		mInMasterGainControl->flushValue ();
	}
}

void AppleOnboardAudio::removeLeftVolumeControl() {
	if (NULL != mOutLeftVolumeControl) {
		mDriverDMAEngine->removeDefaultAudioControl (mOutLeftVolumeControl);
		mOutLeftVolumeControl = NULL;
	}
}

void AppleOnboardAudio::removeRightVolumeControl() {
	if (NULL != mOutRightVolumeControl) {
		mDriverDMAEngine->removeDefaultAudioControl (mOutRightVolumeControl);
		mOutRightVolumeControl = NULL;
	}
}

void AppleOnboardAudio::removeMasterVolumeControl() {
	if (NULL != mOutMasterVolumeControl) {
		mDriverDMAEngine->removeDefaultAudioControl (mOutMasterVolumeControl);
		mOutMasterVolumeControl = NULL;
	}
}

void AppleOnboardAudio::removeLeftGainControl() {
	if (NULL != mInLeftGainControl) {
		mDriverDMAEngine->removeDefaultAudioControl (mInLeftGainControl);
		mInLeftGainControl = NULL;
	}
}

void AppleOnboardAudio::removeRightGainControl() {
	if (NULL != mInRightGainControl) {
		mDriverDMAEngine->removeDefaultAudioControl (mInRightGainControl);
		mInRightGainControl = NULL;
	}
}

void AppleOnboardAudio::removeMasterGainControl() {
	if ( NULL != mInMasterGainControl ) {
		mDriverDMAEngine->removeDefaultAudioControl ( mInMasterGainControl );
		mInMasterGainControl = NULL;
	}
}

IOReturn AppleOnboardAudio::outputControlChangeHandler (IOService *target, IOAudioControl *control, SInt32 oldValue, SInt32 newValue) {
	IOReturn						result;
	AppleOnboardAudio *				audioDevice;
	IOAudioLevelControl *			levelControl;
	IODTPlatformExpert * 			platform;
	UInt32							leftVol;
	UInt32							rightVol;
	Boolean							wasPoweredDown;
	UInt32							subType;

	debugIOLog (3, "+ AppleOnboardAudio::outputControlChangeHandler (%p, %p, %lX, %lX)", target, control, oldValue, newValue);

	result = kIOReturnError;
	audioDevice = OSDynamicCast (AppleOnboardAudio, target);
	wasPoweredDown = FALSE;
	FailIf (NULL == audioDevice, Exit);

	// We have to make sure the hardware is on before we can send it any control changes [2981190]
	if (kIOAudioDeviceSleep == audioDevice->ourPowerState) {

		IOAudioDevicePowerState newState;
		newState = kIOAudioDeviceActive;

		//	Wake requires that the transport object go active prior
		//	to the hardware plugin object(s) going active.
		if ( NULL != audioDevice->mTransportInterface ) {
			result = audioDevice->mTransportInterface->performTransportWake ();
		}
		result = audioDevice->callPluginsInOrder (kPowerStateChange, kIOAudioDeviceActive);
		audioDevice->ourPowerState = kIOAudioDeviceActive;
		audioDevice->mUCState.ucPowerState = audioDevice->ourPowerState;
		if (NULL != audioDevice->mExternalClockSelector) {
			audioDevice->mExternalClockSelector->flushValue ();
		}
		if (NULL != audioDevice->mOutMuteControl) {
			audioDevice->mOutMuteControl->flushValue ();					// Restore hardware to the user's selected state
		}

		wasPoweredDown = TRUE;
	}

	switch (control->getType ()) {
		case kIOAudioControlTypeLevel:
			switch (control->getSubType ()) {
				case kIOAudioLevelControlSubTypeVolume:
					levelControl = OSDynamicCast (IOAudioLevelControl, control);
		
					switch (control->getChannelID ()) {
						case kIOAudioControlChannelIDAll:
							result = audioDevice->volumeMasterChange (newValue);
							if (newValue == levelControl->getMinValue ()) {
								// If it's set to it's min, then it's mute, so tell the HAL it's muted
								OSNumber *			muteState;
								muteState = OSNumber::withNumber ((long long unsigned int)1, 32);
								if (NULL != audioDevice->mOutMuteControl) {
									audioDevice->mOutMuteControl->hardwareValueChanged (muteState);
									debugIOLog (3, "volume control change calling audioDevice->callPluginsInOrder ( kSetMuteState, TRUE )");
									result = audioDevice->callPluginsInOrder ( kSetMuteState, TRUE );
								}
							} else if (oldValue == levelControl->getMinValue () && FALSE == audioDevice->mIsMute) {
								OSNumber *			muteState;
								muteState = OSNumber::withNumber ((long long unsigned int)0, 32);
								if (NULL != audioDevice->mOutMuteControl) {
									audioDevice->mOutMuteControl->hardwareValueChanged (muteState);
									debugIOLog (3, "volume control change calling audioDevice->callPluginsInOrder ( kSetMuteState, FALSE )");
									result = audioDevice->callPluginsInOrder ( kSetMuteState, FALSE );
								}
							}
							break;
						case kIOAudioControlChannelIDDefaultLeft:
							result = audioDevice->volumeLeftChange (newValue);
							break;
						case kIOAudioControlChannelIDDefaultRight:
							result = audioDevice->volumeRightChange (newValue);
							break;
					}
					break;
				case kIOAudioLevelControlSubTypePRAMVolume:
					platform = OSDynamicCast (IODTPlatformExpert, getPlatform());
					if (platform) {
						UInt8 							curPRAMVol;
		
						result = platform->readXPRAM ((IOByteCount)kPRamVolumeAddr, &curPRAMVol, (IOByteCount)1);
						curPRAMVol = (curPRAMVol & 0xF8) | newValue;
						result = platform->writeXPRAM ((IOByteCount)kPRamVolumeAddr, &curPRAMVol, (IOByteCount) 1);
						audioDevice->mUCState.ucPramData = (UInt32)curPRAMVol;
						audioDevice->mUCState.ucPramVolume = audioDevice->mUCState.ucPramData & 0x00000007;
					}
					break;
				default:
					result = kIOReturnBadArgument;
			}
			break;
		case kIOAudioControlTypeToggle:
			subType = control->getSubType ();
			switch (control->getSubType ()) {
				case kIOAudioToggleControlSubTypeMute:
					debugIOLog (3, "outputControlChangeHandler calling audioDevice->callPluginsInOrder ( kSetMuteState, %ld )", newValue);
					result = audioDevice->callPluginsInOrder ( kSetMuteState, newValue );
					if (kIOReturnSuccess == result) {
						audioDevice->mIsMute = newValue;
					}
					break;
				case kAOAPropertyHeadphoneExclusive:
					//	Do you want your headphone and line out jacks to be exclusive or do you want
					//	to be able to use both at the same time.
					audioDevice->mHeadLineDigExclusive = newValue;
					// [3279525] update the amps
					if ( NULL != audioDevice->mOutputSelector ) {
						audioDevice->selectOutput (audioDevice->mOutputSelector->getIntValue (), audioDevice->mIsMute);
					}
					result = kIOReturnSuccess;
					break;
				default:
					result = kIOReturnBadArgument;
					break;
			}
			break;
		case kIOAudioControlTypeSelector:
			result = audioDevice->outputSelectorChanged (newValue);
			break;
		default:
			;
	}

	if (control->getSubType () == kIOAudioLevelControlSubTypeVolume) {
		levelControl = OSDynamicCast (IOAudioLevelControl, control);
		if (audioDevice->mOutRightVolumeControl && audioDevice->mOutLeftVolumeControl) {
			if (audioDevice->mOutRightVolumeControl->getMinValue () == audioDevice->mVolRight &&
				audioDevice->mOutLeftVolumeControl->getMinValue () == audioDevice->mVolLeft) {
				// If it's set to it's min, then it's mute, so tell the HAL it's muted
				OSNumber *			muteState;
				muteState = OSNumber::withNumber ((long long unsigned int)1, 32);
				if (NULL != audioDevice->mOutMuteControl) {
					audioDevice->mOutMuteControl->hardwareValueChanged (muteState);
				}
			} else if (newValue != levelControl->getMinValue () && oldValue == levelControl->getMinValue () && FALSE == audioDevice->mIsMute) {
				OSNumber *			muteState;
				muteState = OSNumber::withNumber ((long long unsigned int)0, 32);
				if (NULL != audioDevice->mOutMuteControl) {
					audioDevice->mOutMuteControl->hardwareValueChanged (muteState);
				}
			}
		}
	}

	if (audioDevice->mIsMute) {
		leftVol = 0;
		rightVol = 0;
	} else {
		leftVol = audioDevice->mVolLeft;
		rightVol = audioDevice->mVolRight;
	}

	if (TRUE == audioDevice->mAutoUpdatePRAM) {				// We do that only if we are on a OS 9 like UI guideline
		audioDevice->WritePRAMVol (leftVol, rightVol);
	}

Exit:
	// Second half of [2981190], put us back to sleep after the right amount of time if we were off
	if (TRUE == wasPoweredDown) {
		audioDevice->setTimerForSleep ();
	}
	debugIOLog (3, "- AppleOnboardAudio::outputControlChangeHandler (%p, %p, %lX, %lX) returns %X", target, control, oldValue, newValue, result);

	return result;
}

void AppleOnboardAudio::createPlayThruControl (void) {
	OSDictionary *						AOAprop;

	AOAprop = OSDynamicCast (OSDictionary, mCurrentInputPlugin->getProperty (kPluginPListAOAAttributes));
	FailIf (NULL == AOAprop, Exit);

	if (kOSBooleanTrue == OSDynamicCast (OSBoolean, AOAprop->getObject ("Playthrough"))) {
		mPlaythruToggleControl = IOAudioToggleControl::createMuteControl (TRUE, kIOAudioControlChannelIDAll, kIOAudioControlChannelNameAll, 0, kIOAudioControlUsagePassThru);

		if (NULL != mPlaythruToggleControl) {
			mDriverDMAEngine->addDefaultAudioControl (mPlaythruToggleControl);
			mPlaythruToggleControl->setValueChangeHandler ((IOAudioControl::IntValueChangeHandler)inputControlChangeHandler, this);
			// Don't release it because we might reference it later
		}
	}

Exit:
	return;
}

void AppleOnboardAudio::removePlayThruControl (void) {
	if (NULL != mPlaythruToggleControl) {
		mDriverDMAEngine->removeDefaultAudioControl (mPlaythruToggleControl);
		mPlaythruToggleControl->release ();
		mPlaythruToggleControl = NULL;
	}
}

IOReturn AppleOnboardAudio::outputSelectorChanged (SInt32 newValue) {
	AudioHardwareObjectInterface *		thePluginObject;
	IOAudioStream *						outputStream;
	const IOAudioStreamFormat *			theFormat;
	char *								connectionString;
	IOReturn							result;
	UInt32								inputLatency;

	debugIOLog (3, "+ AppleOnboardAudio::outputSelectorChanged(%4s)", (char *)&newValue);

	result = kIOReturnSuccess;
	inputLatency = 0;
	
	// [3318095], only allow selections to connected devices
	if (kIOAudioOutputPortSubTypeLine == newValue) {
		if ( !(mDetectCollection & kSndHWLineOutput) ) {
			result = kIOReturnError;
		}	
	} else if (kIOAudioOutputPortSubTypeHeadphones == newValue) {
		if ( !(mDetectCollection & kSndHWCPUHeadphone) ) {
			result = kIOReturnError;
		}
	// if we have a combo jack which allows us to tell if digital out is connected, disallow selection if it isn't connected	
	} else if ((kIOAudioOutputPortSubTypeSPDIF == newValue) && (kGPIO_Unknown != mPlatformInterface->getComboOutJackTypeConnected ())) {
		if ( !(mDetectCollection & kSndHWDigitalOutput) ) {
			result = kIOReturnError;
		}	
	}
	// [3326566], don't allow internal or external speaker selection if headphones or line out are connected
	if ( (kIOAudioOutputPortSubTypeInternalSpeaker == newValue) || (kIOAudioOutputPortSubTypeExternalSpeaker == newValue) ) {	//	[3398729]
		if ( (mDetectCollection & kSndHWLineOutput) || (mDetectCollection & kSndHWCPUHeadphone) ) {
			result = kIOReturnError;
		}	
	}

	if ( kIOReturnSuccess == result ) {
		connectionString = getConnectionKeyFromCharCode (newValue, kIOAudioStreamDirectionOutput);
		FailIf (0 == connectionString, Exit);
	
		thePluginObject = getPluginObjectForConnection (connectionString);
		FailIf (NULL == thePluginObject, Exit);
	
		outputStream = mDriverDMAEngine->getAudioStream (kIOAudioStreamDirectionOutput, 1);
		FailIf (NULL == outputStream, Exit);
		
		theFormat = outputStream->getFormat ();
		FailWithAction (kIOAudioStreamSampleFormat1937AC3 == theFormat->fSampleFormat, result = kIOReturnNotPermitted, Exit);
	
		outputStream->setTerminalType (getTerminalTypeForCharCode (newValue));
	
		cacheOutputVolumeLevels (mCurrentOutputPlugin);
		setClipRoutineForOutput (connectionString);
		
		if (mCurrentOutputPlugin != thePluginObject) {
			mCurrentOutputPlugin = thePluginObject;
		}
		// [3277271], only report the worst case latency once on initialization, not on output selection changes because they're not exclusive. 
	
		AdjustOutputVolumeControls(mCurrentOutputPlugin, newValue);
	
		selectOutput(newValue, mIsMute, FALSE);		//	Tell selectOutput that this is an explicit selection
	}

Exit:
	debugIOLog (3, "- AppleOnboardAudio::outputSelectorChanged returns %X", result);
	return result;
}

// This is called when we're on hardware that only has one active volume control (either right or left)
// otherwise the respective right or left volume handler will be called.
// This calls both volume handers becasue it doesn't know which one is really the active volume control.
IOReturn AppleOnboardAudio::volumeMasterChange (SInt32 newValue) {
	IOReturn						result = kIOReturnSuccess;

	debugIOLog (3, "+ AppleOnboardAudio::volumeMasterChange (%ld)", newValue);

	result = kIOReturnError;

	// Don't know which volume control really exists, so adjust both -- they'll ignore the change if they don't exist
	result = volumeLeftChange (newValue);
	result = volumeRightChange (newValue);

	result = kIOReturnSuccess;

	debugIOLog (3, "- AppleOnboardAudio::volumeMasterChange, 0x%x", result);
	return result;
}

IOReturn AppleOnboardAudio::volumeLeftChange (SInt32 newValue) {
	IOReturn							result;
	UInt32 								count;
	UInt32 								index;
	AudioHardwareObjectInterface* 		thePluginObject;

	debugIOLog (3, "+ AppleOnboardAudio::volumeLeftChange (%ld)", newValue);

	// [3339273] set volume on all plugins
	//mCurrentOutputPlugin->setVolume (newValue, mVolRight);
	if (NULL != mPluginObjects) {
		count = mPluginObjects->getCount ();
		for (index = 0; index < count; index++) {
			thePluginObject = getIndexedPluginObject (index);
			if((NULL != thePluginObject)) {
				thePluginObject->setVolume (newValue, mVolRight);
			}
		}
	}

	mVolLeft = newValue;

	result = kIOReturnSuccess;

	debugIOLog (3, "- AppleOnboardAudio::volumeLeftChange, 0x%x", result);

	return result;
}

IOReturn AppleOnboardAudio::volumeRightChange (SInt32 newValue) {
	IOReturn							result;

//	debugIOLog (3, "+ AppleOnboardAudio::volumeRightChange (%ld)", newValue);

	if ( mIsMute ) {		//	[3435307]
		mCurrentOutputPlugin->setMute (mIsMute);
	} else {
		mCurrentOutputPlugin->setVolume (mVolLeft, newValue);
	}

	mVolRight = newValue;

	result = kIOReturnSuccess;

	debugIOLog (3, "- AppleOnboardAudio::volumeRightChange, result = 0x%x", result);

	return result;
}

IOReturn AppleOnboardAudio::outputMuteChange (SInt32 newValue) {
	debugIOLog (3, "+ AppleOnboardAudio::outputMuteChange (%ld)", newValue);

	callPluginsInOrder ( kSetMuteState, newValue );

	debugIOLog (3, "- AppleOnboardAudio::outputMuteChange");
    return kIOReturnSuccess;
}

IOReturn AppleOnboardAudio::inputControlChangeHandler (IOService *target, IOAudioControl *control, SInt32 oldValue, SInt32 newValue) {
	IOReturn						result = kIOReturnError;
	AppleOnboardAudio *				audioDevice;
	IOAudioLevelControl *			levelControl;
	Boolean							wasPoweredDown;

	debugIOLog (3, "+ AppleOnboardAudio::inputControlChangeHandler (%p, %p, %lX, %lX)", target, control, oldValue, newValue);

	audioDevice = OSDynamicCast (AppleOnboardAudio, target);
	wasPoweredDown = FALSE;
	FailIf (NULL == audioDevice, Exit);

	// We have to make sure the hardware is on before we can send it any control changes [2981190]
	if (kIOAudioDeviceSleep == audioDevice->ourPowerState) {

		IOAudioDevicePowerState newState;
		newState = kIOAudioDeviceActive;

		//	Wake requires that the transport object go active prior
		//	to the hardware plugin object(s) going active.
		if ( NULL != audioDevice->mTransportInterface ) {
			result = audioDevice->mTransportInterface->performTransportWake ();
		}
		result = audioDevice->callPluginsInOrder (kPowerStateChange, kIOAudioDeviceActive);
		audioDevice->ourPowerState = kIOAudioDeviceActive;
		audioDevice->mUCState.ucPowerState = audioDevice->ourPowerState;
		if (NULL != audioDevice->mExternalClockSelector) {
			audioDevice->mExternalClockSelector->flushValue ();
		}
		if (NULL != audioDevice->mOutMuteControl) {
			audioDevice->mOutMuteControl->flushValue ();					// Restore hardware to the user's selected state
		}

		wasPoweredDown = TRUE;
	}

	switch (control->getType ()) {
		case kIOAudioControlTypeLevel:
			//switch (control->getSubType ()) {
			//	case kIOAudioLevelControlSubTypeVolume:
					levelControl = OSDynamicCast (IOAudioLevelControl, control);
		
					switch (control->getChannelID ()) {
						case kIOAudioControlChannelIDDefaultLeft:
							result = audioDevice->gainLeftChanged (newValue);
							break;
						case kIOAudioControlChannelIDDefaultRight:
							result = audioDevice->gainRightChanged (newValue);
							break;
						case kIOAudioControlChannelIDAll:
							result = audioDevice->gainRightChanged (newValue);
							break;
					}
					break;
			//}
			//break;
		case kIOAudioControlTypeToggle:
			//switch (control->getSubType ()) {
			//	case kIOAudioToggleControlSubTypeMute:
					result = audioDevice->passThruChanged (newValue);
			//		break;
			//}
			break;
		case kIOAudioControlTypeSelector:
			debugIOLog (3, "input selector change handler");
			switch (control->getSubType ()) {
				case kIOAudioSelectorControlSubTypeInput:
					result = audioDevice->inputSelectorChanged (newValue);
					break;
				case kIOAudioSelectorControlSubTypeClockSource:
					result = audioDevice->clockSelectorChanged (newValue);
					break;
				default:
					debugIOLog (3, "unknown control type in input selector change handler");
					break;
			}		
		default:
			;
	}

Exit:
	// Second half of [2981190], put us back to sleep after the right amount of time if we were off
	if (TRUE == wasPoweredDown) {
		audioDevice->setTimerForSleep ();
	}

	return result;
}

IOReturn AppleOnboardAudio::gainLeftChanged (SInt32 newValue) {
	IOReturn							result;

    debugIOLog (3, "+ AppleOnboardAudio::gainLeftChanged");    

	if (mCurrentPluginHasSoftwareInputGain) {
		mDriverDMAEngine->setInputGainL (newValue);
	} else {
		mCurrentInputPlugin->setInputGain (newValue, mGainRight);
	}
    mGainLeft = newValue;

	result = kIOReturnSuccess;

    debugIOLog (3, "- AppleOnboardAudio::gainLeftChanged, %d", (result == kIOReturnSuccess));
    return result;
}

IOReturn AppleOnboardAudio::gainRightChanged (SInt32 newValue) {
	IOReturn							result;

    debugIOLog (3, "+ AppleOnboardAudio::gainRightChanged");    

	if (mCurrentPluginHasSoftwareInputGain) {
		mDriverDMAEngine->setInputGainR (newValue);
	} else {
		mCurrentInputPlugin->setInputGain (newValue, mGainRight);
	}
    mGainRight = newValue;

	result = kIOReturnSuccess;

    debugIOLog (3, "- AppleOnboardAudio::gainRightChanged, %d", (result == kIOReturnSuccess));
    return result;
}

IOReturn AppleOnboardAudio::gainMasterChanged (SInt32 newValue) {
	IOReturn							result;

    debugIOLog (3, "+ AppleOnboardAudio::gainMasterChanged");    

	if (mCurrentPluginHasSoftwareInputGain) {
		mDriverDMAEngine->setInputGainR (newValue);
		mDriverDMAEngine->setInputGainL (newValue);
	} else {
		mCurrentInputPlugin->setInputGain (newValue, newValue);
	}
	result = kIOReturnSuccess;

    debugIOLog (3, "- AppleOnboardAudio::gainMasterChanged, %d", (result == kIOReturnSuccess));
    return result;
}

IOReturn AppleOnboardAudio::passThruChanged (SInt32 newValue) {
	IOReturn							result;

    debugIOLog (3, "+ AppleOnboardAudio::passThruChanged");

    result = kIOReturnError;

	mCurrentInputPlugin->setPlayThrough (!newValue);

	result = kIOReturnSuccess;

    debugIOLog (3, "- AppleOnboardAudio::passThruChanged");
    return result;
}

IOReturn AppleOnboardAudio::inputSelectorChanged (SInt32 newValue) {
	AudioHardwareObjectInterface *		thePluginObject;
	IOAudioStream *						inputStream;
	OSDictionary *						AOApropInput;
	OSBoolean *							softwareInputGainBoolean;
	OSNumber *							inputLatencyNumber;
	char *								connectionString;
	IOReturn							result;
	UInt32								inputLatency;

	debugIOLog (3, "+ AppleOnboardAudio::inputSelectorChanged (%4s)", (char *)&newValue);

	result = kIOReturnError;
	inputLatency = 0;
	
	connectionString = getConnectionKeyFromCharCode (newValue, kIOAudioStreamDirectionInput);
	FailIf (0 == connectionString, Exit);

	thePluginObject = getPluginObjectForConnection (connectionString);
	FailIf (NULL == thePluginObject, Exit);

	// FIX  Make this change as the selection changes.
	inputStream = mDriverDMAEngine->getAudioStream (kIOAudioStreamDirectionInput, 1);
	FailIf (NULL == inputStream, Exit);
	inputStream->setTerminalType (getTerminalTypeForCharCode (newValue));

	setUseInputGainControls (connectionString);
	
	setUsePlaythroughControl (connectionString);	// [3250612], this was missing

	setClipRoutineForInput (connectionString);
	
	// [3250612], fix update logic regarding current input plugin	
	mCurrentInputPlugin->setInputMute (TRUE);

	if (mCurrentInputPlugin != thePluginObject) {

		thePluginObject->setInputMute (TRUE);

		// in future this may need to be on a per input basis (which would move this out of this if statement)
		cacheInputGainLevels (mCurrentInputPlugin);
		
		ConfigChangeHelper theConfigeChangeHelper(mDriverDMAEngine);	
	
		setInputDataMuxForConnection ( connectionString );
			
		// in future may need to update this based on individual inputs, not the part as a whole
		AOApropInput = OSDynamicCast (OSDictionary, thePluginObject->getProperty (kPluginPListAOAAttributes));
		if (NULL != AOApropInput) {
			softwareInputGainBoolean = OSDynamicCast (OSBoolean, AOApropInput->getObject (kPluginPListSoftwareInputGain));
			if (NULL != softwareInputGainBoolean) {
				mDriverDMAEngine->setUseSoftwareInputGain (softwareInputGainBoolean->getValue ());
				mCurrentPluginHasSoftwareInputGain = softwareInputGainBoolean->getValue ();
			} else {
				mDriverDMAEngine->setUseSoftwareInputGain (false);
				mCurrentPluginHasSoftwareInputGain = false;
			}
		}

		if (NULL != AOApropInput) {
			inputLatencyNumber = OSDynamicCast (OSNumber, AOApropInput->getObject (kPluginPListInputLatency));
			if (NULL != inputLatencyNumber) {
				inputLatency = inputLatencyNumber->unsigned32BitValue();
			}
		}
		// [3277271], output latency doesn't change
		mDriverDMAEngine->setSampleLatencies (mOutputLatency, inputLatency);

		mCurrentInputPlugin = thePluginObject;
		debugIOLog (3, "+ AppleOnboardAudio::inputSelectorChanged - mCurrentInputPlugin updated to %p", mCurrentInputPlugin);
	}
	
	mCurrentInputPlugin->setActiveInput (newValue);
	
	AdjustInputGainControls (mCurrentInputPlugin);

	if ((mDetectCollection & kSndHWLineInput) && (kIOAudioInputPortSubTypeLine == newValue)) {
		mCurrentInputPlugin->setInputMute (FALSE);
	} else if (!(mDetectCollection & kSndHWLineInput) && (kIOAudioInputPortSubTypeLine == newValue)) {
		mCurrentInputPlugin->setInputMute (TRUE);
	}

	result = kIOReturnSuccess;

Exit:
	debugIOLog (3, "- AppleOnboardAudio::inputSelectorChanged");
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	clockSelectorChanged:
//
//	This method performs a 'make before break' operation involving the transport interface and the hardware plugin objects (called in 
//	order).  A system specific illustration, invovling the I2S transport object, the Topaz S/PDIF digital audioobject and the TAS3004 
//	analog audio object follows:
//
//	TRANSITION						CYCLE					ACTION
//	----------------------------|-----------------------|-------------------------------------------------------------------------------
//	kTRANSPORT_MASTER_CLOCK to	|	1 Transport Break	|	Set MUX to alternate clock source, set I2S to SLAVE (BCLKMaster = SLAVE).
//	kTRANSPORT_SLAVE_CLOCK		|	2 Topaz Break		|	Stop CS84xx & mute TX.  Set all registers to act as a clock master.
//								|						|	A.	Data Flow Control Register:		TXD = 01, SPD = 10, SRCD = 0
//								|						|	B.	Clock Source Control Register:	OUTC = 1, INC = 0, RXD = 01
//								|						|	C.	Serial Input Control Register:	SIMS = 0
//								|						|	D.	Serial Output Control Register:	SOMS = 1
//								|	3 TAS3004 Break		|	No Action.
//								|	4 Transport Make	|	No Action.
//								|	5 Topaz Make		|	Start CS84xx.  Send request to restart transport hardware.
//								|	6 TAS3004 Make		|	Reset and flush register cache to hardware.
//	----------------------------|-----------------------|-------------------------------------------------------------------------------
//	kTRANSPORT_SLAVE_CLOCK to	|	1 Transport Break	|	No Action.
//	kTRANSPORT_MASTER_CLOCK		|	2 Topaz Break		|	Stop CS84xx & disable TX.  Set all registers to act as a clock slave.
//								|						|	A.	Data Flow Control Register:		TXD = 01, SPD = 00, SRCD = 1
//								|						|	B.	Clock Source Control Register:	OUTC = 0, INC = 0, RXD = 01
//								|						|	C.	Serial Input Control Register:	SIMS = 0
//								|						|	D.	Serial Output Control Register:	SOMS = 0
//								|	3 TAS3004 Break		|	No Action.
//								|	4 Transport Make	|	Set MUX to default clock source, set I2S to SLAVE (BCLKMaster = MASTER).
//								|	5 Topaz Make		|	Start CS84xx & unmute TX.  Send request to restart transport hardware.
//								|						|	A.	Clear pending receiver errors.
//								|						|	B.	Enable receiver errors.
//								|						|	C.	Set CS8420 to RUN.
//								|						|	D.	Request a restart of the I2S transport.
//								|	6 TAS3004 Make		|	Reset and flush register cache to hardware.
//	----------------------------|-----------------------|-------------------------------------------------------------------------------	
IOReturn AppleOnboardAudio::clockSelectorChanged (SInt32 newValue) {
	IOReturn							result;

	debugIOLog (7, "+ AppleOnboardAudio::clockSelectorChanged (%4s)", (char *)&newValue);

	result = kIOReturnError;

	mClockSelectInProcessSemaphore = true;	//	block 'UNLOCK' errors while switching clock sources
	
	FailIf ( NULL == mDriverDMAEngine, Exit );
	FailIf ( NULL == mTransportInterface, Exit );
	if ( mCurrentClockSelector != newValue ) {

		callPluginsInOrder ( kSetMuteState, TRUE );						//	[3435307],[3253678], mute outputs during clock selection

		if ( kClockSourceSelectionInternal == newValue ) {
			ConfigChangeHelper theConfigeChangeHelper(mDriverDMAEngine, 10);	

			mTransportInterface->transportBreakClockSelect ( kTRANSPORT_MASTER_CLOCK );
			callPluginsInOrder ( kBreakClockSelect, kTRANSPORT_MASTER_CLOCK );
			
			mTransportInterface->transportMakeClockSelect ( kTRANSPORT_MASTER_CLOCK );
			callPluginsInOrder ( kMakeClockSelect, kTRANSPORT_MASTER_CLOCK );
			
			//	[3253678]	safe to unmute analog part when going to internal clock
			//	[3435307]	Don't change amplifier mutes in response to UI.	RBM
			if ( NULL != mOutputSelector ) {
				selectOutput ( mOutputSelector->getIntValue (), mIsMute);
			}
						
		} else if ( kClockSourceSelectionExternal == newValue ) {
			ConfigChangeHelper theConfigeChangeHelper(mDriverDMAEngine, 10);	
			mTransportInterface->transportBreakClockSelect ( kTRANSPORT_SLAVE_CLOCK );
			callPluginsInOrder ( kBreakClockSelect, kTRANSPORT_SLAVE_CLOCK );
			
			mTransportInterface->transportMakeClockSelect ( kTRANSPORT_SLAVE_CLOCK );
			callPluginsInOrder ( kMakeClockSelect, kTRANSPORT_SLAVE_CLOCK );

			//	[3435307]	Don't change amplifier mutes in response to UI.	RBM
			if ( NULL != mOutputSelector ) {
				selectOutput ( mOutputSelector->getIntValue (), mIsMute);
			}
		} else {
			debugIOLog (3,  "Unknown clock source selection." );
			FailIf (TRUE, Exit);
		}

		mCurrentClockSelector = newValue;
	}
	result = kIOReturnSuccess;
Exit:
	mClockSelectInProcessSemaphore = false;	//	enable 'UNLOCK' errors after switching clock sources
	debugIOLog (7, "- AppleOnboardAudio::inputSelectorChanged");
	return result;
}

UInt32 AppleOnboardAudio::getCurrentSampleFrame (void) {
	return mPlatformInterface->getFrameCount ();
}

void AppleOnboardAudio::setCurrentSampleFrame (UInt32 inValue) {
	mPlatformInterface->setFrameCount (inValue);
	return;
}

void AppleOnboardAudio::setInputDataMuxForConnection ( char * connectionString ) {
	GpioAttributes		theMuxSelect;
	
	theMuxSelect = getInputDataMuxForConnection ( connectionString );
	if ( kGPIO_Unknown != theMuxSelect ) {
		debugIOLog (3,  "AppleOnboardAudio::setInputDataMuxForConnection setting input data mux to %d", (unsigned int)theMuxSelect );
		mPlatformInterface->setInputDataMux ( theMuxSelect );
	}
}

#pragma mark +POWER MANAGEMENT
void AppleOnboardAudio::setTimerForSleep () {
    AbsoluteTime				fireTime;
    UInt64						nanos;

	if (idleTimer && idleSleepDelayTime != kNoIdleAudioPowerDown) {
		clock_get_uptime (&fireTime);
		absolutetime_to_nanoseconds (fireTime, &nanos);
		nanos += idleSleepDelayTime;
		nanoseconds_to_absolutetime (nanos, &fireTime);
		idleTimer->wakeAtTime (fireTime);		// will call idleAudioSleepHandlerTimer
	}
}

void AppleOnboardAudio::sleepHandlerTimer (OSObject *owner, IOTimerEventSource *sender) {
	AppleOnboardAudio *				audioDevice;
	UInt32							time = 0;

	audioDevice = OSDynamicCast (AppleOnboardAudio, owner);
	FailIf (NULL == audioDevice, Exit);

	if (audioDevice->getPowerState () != kIOAudioDeviceActive) {
		audioDevice->performPowerStateChange (audioDevice->getPowerState (), kIOAudioDeviceIdle, &time);
	}

Exit:
	return;
}

// Have to call super::setAggressiveness to complete the function call
IOReturn AppleOnboardAudio::setAggressiveness (unsigned long type, unsigned long newLevel) {
	UInt32					time = 0;

	if (type == kPMPowerSource) {
		debugIOLog (3, "setting power aggressivness state to ");
		switch (newLevel) {
			case kIOPMInternalPower:								// Running on battery only
				debugIOLog (3, "battery power");
				idleSleepDelayTime = kBatteryPowerDownDelayTime;
				setIdleAudioSleepTime (idleSleepDelayTime);
				if (getPowerState () != kIOAudioDeviceActive) {
					performPowerStateChange (getPowerState (), kIOAudioDeviceIdle, &time);
				}
				break;
			case kIOPMExternalPower:								// Running on AC power
				debugIOLog (3, "wall power");
				// idleSleepDelayTime = kACPowerDownDelayTime;		// idle power down after 5 minutes
				idleSleepDelayTime = kNoIdleAudioPowerDown;
				setIdleAudioSleepTime (idleSleepDelayTime);			// don't tell us about going to the idle state
				if (getPowerState () != kIOAudioDeviceActive) {
					performPowerStateChange (getPowerState (), kIOAudioDeviceActive, &time);
				}
				break;
			default:
				break;
		}
	}

	return super::setAggressiveness (type, newLevel);
}

IOReturn AppleOnboardAudio::performPowerStateChange (IOAudioDevicePowerState oldPowerState, IOAudioDevicePowerState newPowerState, UInt32 *microsecondsUntilComplete)
{
	IOReturn							result;

	debugIOLog (3, "+ AppleOnboardAudio::performPowerStateChange (%d, %d) -- ourPowerState = %d", oldPowerState, newPowerState, ourPowerState);

	*microsecondsUntilComplete = 2000000;

	result = super::performPowerStateChange (oldPowerState, newPowerState, microsecondsUntilComplete);
    
	if ( kIOAudioDeviceIdle == ourPowerState && kIOAudioDeviceActive == newPowerState ) {
		result = performPowerStateChangeThreadAction ( this, (void*)newPowerState, 0, 0, 0 );
	} else {
		if (mPowerThread) {
			thread_call_enter1(mPowerThread, (thread_call_param_t)newPowerState);
		}
	}

	debugIOLog (3, "- AppleOnboardAudio::performPowerStateChange -- ourPowerState = %d", ourPowerState);

	return result; 
}

void AppleOnboardAudio::performPowerStateChangeThread (AppleOnboardAudio * aoa, void * newPowerState) {
	IOCommandGate *						cg;
	IOReturn							result;

	debugIOLog (3, "+ AppleOnboardAudio::performPowerStateChangeThread (%p, %ld)", aoa, (UInt32)newPowerState);

	FailIf (NULL == aoa, Exit);

	aoa->mSignal->wait (FALSE);

	FailWithAction (TRUE == aoa->mTerminating, aoa->completePowerStateChange (), Exit);	
	cg = aoa->getCommandGate ();
	if (cg) {
		result = cg->runAction (aoa->performPowerStateChangeThreadAction, newPowerState, (void *)aoa);
	}

Exit:
	return;
}

IOReturn AppleOnboardAudio::performPowerStateChangeThreadAction (OSObject * owner, void * newPowerState, void * us, void * arg3, void * arg4) {
	AppleOnboardAudio *					aoa;
	IOReturn							result;

	result = kIOReturnError;

	aoa = OSDynamicCast (AppleOnboardAudio, (OSObject *)us);
	FailIf (NULL == aoa, Exit);

//	kprintf ("+AppleOnboardAudio::performPowerStateChangeThreadAction (%p, %ld), %d", owner, (UInt32)newPowerState, aoa->ourPowerState);
	debugIOLog (3, "+ AppleOnboardAudio::performPowerStateChangeThreadAction (%p, %ld) -- ourPowerState = %d", owner, (UInt32)newPowerState, aoa->ourPowerState);

	FailIf (NULL == aoa->mTransportInterface, Exit);

	switch ((UInt32)newPowerState) {
		case kIOAudioDeviceSleep:
			//	Sleep requires that the hardware plugin object(s) go to sleep
			//	prior to the transport object going to sleep.
			if (kIOAudioDeviceActive == aoa->ourPowerState) {
				debugIOLog (3, "active to sleep power change calling outputMuteChange(TRUE)");
				aoa->outputMuteChange (TRUE);			// Mute before turning off power
				result = aoa->callPluginsInReverseOrder (kPowerStateChange, kIOAudioDeviceSleep);
				if ( NULL != aoa->mTransportInterface ) { 
					result = aoa->mTransportInterface->performTransportSleep ();
				}
				//	[3453989]	begin	{
				if ( NULL != aoa->mPlatformInterface ) { 
					aoa->mPlatformInterface->performPlatformSleep ();
				}
				//	}	end	[3453989]
				aoa->ourPowerState = kIOAudioDeviceSleep;
			}
			break;
		case kIOAudioDeviceIdle:
			if (kIOAudioDeviceActive == aoa->ourPowerState) {
				debugIOLog (3, "active to idle power change calling outputMuteChange(TRUE)");
				aoa->outputMuteChange (TRUE);			// Mute before turning off power
				//	Going idle from wake requires that the hardware plugin object(s) go to sleep
				//	prior to the transport object going to sleep.
				result = aoa->callPluginsInReverseOrder (kPowerStateChange, kIOAudioDeviceSleep);
				if ( NULL != aoa->mTransportInterface ) { 
					result = aoa->mTransportInterface->performTransportSleep ();
				}
				//	[3453989]	begin	{
				if ( NULL != aoa->mPlatformInterface ) { 
					aoa->mPlatformInterface->performPlatformSleep ();
				}
				//	}	end	[3453989]
				aoa->ourPowerState = kIOAudioDeviceSleep;
			} else if (kIOAudioDeviceSleep == aoa->ourPowerState && kNoIdleAudioPowerDown == aoa->idleSleepDelayTime) {
				//	Wake to idle from sleep requires that the transport object go active prior
				//	to the hardware plugin object(s) going active.
				if ( NULL != aoa->mTransportInterface ) {
					result = aoa->mTransportInterface->performTransportWake ();
				}
				result = aoa->callPluginsInOrder (kPowerStateChange, kIOAudioDeviceActive);
				if (NULL != aoa->mExternalClockSelector) {
					aoa->mExternalClockSelector->flushValue ();
				}
				aoa->ourPowerState = kIOAudioDeviceActive;
				//	[3453989]	begin	{
				if ( NULL != aoa->mPlatformInterface ) { 
					aoa->mPlatformInterface->performPlatformWake ( aoa );
				}
				//	}	end	[3453989]
				if ( NULL != aoa->mOutputSelector ) {
					aoa->selectOutput (aoa->mOutputSelector->getIntValue (), aoa->mIsMute);	//	Radar 3416318:	mOutMuteControl does not touch GPIOs so do so here!
				}
				if (NULL != aoa->mOutMuteControl) {
					aoa->mOutMuteControl->flushValue ();					// Restore hardware to the user's selected state
				}
			}
			break;
		case kIOAudioDeviceActive:
			if (kIOAudioDeviceActive != aoa->ourPowerState) {
				//	Wake requires that the transport object go active prior
				//	to the hardware plugin object(s) going active.
				if ( NULL != aoa->mTransportInterface ) {
					result = aoa->mTransportInterface->performTransportWake ();
				}
				result = aoa->callPluginsInOrder (kPowerStateChange, kIOAudioDeviceActive);
				aoa->ourPowerState = kIOAudioDeviceActive;
				//	[3453989]	begin	{
				if ( NULL != aoa->mPlatformInterface ) { 
					aoa->mPlatformInterface->performPlatformWake ( aoa );
				}
				//	}	end	[3453989]
				if (NULL != aoa->mExternalClockSelector) {
					aoa->mExternalClockSelector->flushValue ();
				}
				if ( NULL != aoa->mOutputSelector ) {
					aoa->selectOutput (aoa->mOutputSelector->getIntValue (), aoa->mIsMute);	//	Radar 3416318:	mOutMuteControl does not touch GPIOs so do so here!
				}
				if (NULL != aoa->mOutMuteControl) {
					aoa->mOutMuteControl->flushValue ();					// Restore hardware to the user's selected state
				}
			} else {
				debugIOLog (3, "trying to wake, but we're already awake");
			}
			break;
		default:
			break;
	}

	aoa->setProperty ("IOAudioPowerState", aoa->ourPowerState, 32);
	aoa->mUCState.ucPowerState = aoa->ourPowerState;
Exit:
	if (NULL != aoa) {
		aoa->protectedCompletePowerStateChange ();
	}
//	kprintf ("-AppleOnboardAudio::performPowerStateChangeThreadAction (%p, %ld), %d", owner, (UInt32)newPowerState, aoa->ourPowerState);
	debugIOLog (3, "- AppleOnboardAudio::performPowerStateChangeThreadAction (%p, %ld) -- ourPowerState = %d", owner, (UInt32)newPowerState, aoa->ourPowerState);
	return result;
}

//********************************************************************************
//
// Receives a notification when the RootDomain changes state. 
//
// Allows us to take action on system sleep, power down, and restart after
// applications have received their power change notifications and replied,
// but before drivers have powered down. We tell the device to go to sleep for a 
// silent shutdown on P80 and DACA.
//*********************************************************************************
IOReturn AppleOnboardAudio::sysPowerDownHandler (void * target, void * refCon, UInt32 messageType, IOService * provider, void * messageArgument, vm_size_t argSize) {
	AppleOnboardAudio *				appleOnboardAudio;
	IOReturn						result;
//	char							message[100];

	result = kIOReturnUnsupported;
	appleOnboardAudio = OSDynamicCast (AppleOnboardAudio, (OSObject *)target);
	FailIf (NULL == appleOnboardAudio, Exit);

	switch (messageType) {
		case kIOMessageSystemWillPowerOff:
		case kIOMessageSystemWillRestart:
			// Interested applications have been notified of an impending power
			// change and have acked (when applicable).
			// This is our chance to save whatever state we can before powering
			// down.
//			Debugger ("about to shut down the hardware");
			result = kIOReturnSuccess;
			break;
		default:
//			sprintf (message, "unknown selector %lx", messageType);
//			Debugger (message);
			break;
	}

Exit:
	return result;
}

#pragma mark +PRAM VOLUME
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Calculates the PRAM volume value for stereo volume.
UInt8 AppleOnboardAudio::VolumeToPRAMValue (UInt32 inLeftVol, UInt32 inRightVol) {
	UInt32			pramVolume;						// Volume level to store in PRAM
	UInt32 			averageVolume;					// summed volume
    UInt32		 	volumeRange;
    UInt32 			volumeSteps;
	UInt32			leftVol;
	UInt32			rightVol;

	debugIOLog (3,  "+ AppleOnboardAudio::VolumeToPRAMValue ( 0x%X, 0x%X )", (unsigned int)inLeftVol, (unsigned int)inRightVol );
	pramVolume = 0;											//	[2886446]	Always pass zero as a result when muting!!!
	if ( ( 0 != inLeftVol ) || ( 0 != inRightVol ) ) {		//	[2886446]
		leftVol = inLeftVol;
		rightVol = inRightVol;
		if (NULL != mOutLeftVolumeControl) {
			leftVol -= mOutLeftVolumeControl->getMinValue ();
		}
	
		if (NULL != mOutRightVolumeControl) {
			rightVol -= mOutRightVolumeControl->getMinValue ();
		}
		debugIOLog (3,  "... leftVol = 0x%X, rightVol = 0x%X", (unsigned int)leftVol, (unsigned int)rightVol );
	
		if (NULL != mOutMasterVolumeControl) {
			volumeRange = (mOutMasterVolumeControl->getMaxValue () - mOutMasterVolumeControl->getMinValue () + 1);
			debugIOLog (3,  "... mOutMasterVolumeControl volumeRange = 0x%X", (unsigned int)volumeRange );
		} else if (NULL != mOutLeftVolumeControl) {
			volumeRange = (mOutLeftVolumeControl->getMaxValue () - mOutLeftVolumeControl->getMinValue () + 1);
			debugIOLog (3,  "... mOutLeftVolumeControl volumeRange = 0x%X", (unsigned int)volumeRange );
		} else if (NULL != mOutRightVolumeControl) {
			volumeRange = (mOutRightVolumeControl->getMaxValue () - mOutRightVolumeControl->getMinValue () + 1);
			debugIOLog (3,  "... mOutRightVolumeControl volumeRange = 0x%X", (unsigned int)volumeRange );
		} else {
			volumeRange = kMaximumPRAMVolume;
			debugIOLog (3,  "... volumeRange = 0x%X **** NO AUDIO LEVEL CONTROLS!", (unsigned int)volumeRange );
		}

		averageVolume = (leftVol + rightVol) >> 1;		// sum the channel volumes and get an average
		debugIOLog (3,  "... averageVolume = 0x%X", (unsigned int)volumeRange );
		debugIOLog (3,  "... volumeRange %X, kMaximumPRAMVolume %X", (unsigned int)volumeRange, (unsigned int)kMaximumPRAMVolume );
		volumeSteps = volumeRange / kMaximumPRAMVolume;	// divide the range by the range of the pramVolume
		pramVolume = averageVolume / volumeSteps;    
	
		// Since the volume level in PRAM is only worth three bits,
		// we round small values up to 1. This avoids SysBeep from
		// flashing the menu bar when it thinks sound is off and
		// should really just be very quiet.
	
		if ((pramVolume == 0) && (leftVol != 0 || rightVol !=0 )) {
			pramVolume = 1;
		}
	
	}
	debugIOLog (3,  "- AppleOnboardAudio::VolumeToPRAMValue returns 0x%X", (unsigned int)pramVolume );
	return (pramVolume & 0x07);
}

UInt32 AppleOnboardAudio::PRAMToVolumeValue (void) {
	UInt32		 	volumeRange;
	UInt32 			volumeSteps;

	if (NULL != mOutMasterVolumeControl) {
		volumeRange = (mOutMasterVolumeControl->getMaxValue () - mOutMasterVolumeControl->getMinValue () + 1);
	} else if (NULL != mOutLeftVolumeControl) {
		volumeRange = (mOutLeftVolumeControl->getMaxValue () - mOutLeftVolumeControl->getMinValue () + 1);
	} else if (NULL != mOutRightVolumeControl) {
		volumeRange = (mOutRightVolumeControl->getMaxValue () - mOutRightVolumeControl->getMinValue () + 1);
	} else {
		volumeRange = kMaximumPRAMVolume;
	}

	volumeSteps = volumeRange / KNumPramVolumeSteps;	// divide the range by the range of the pramVolume

	return (volumeSteps * ReadPRAMVol ());
}

void AppleOnboardAudio::WritePRAMVol (UInt32 leftVol, UInt32 rightVol) {
	UInt8						pramVolume;
	UInt8 						curPRAMVol;
	IODTPlatformExpert * 		platform;
	IOReturn					err;
		
	platform = OSDynamicCast(IODTPlatformExpert,getPlatform());
    
    debugIOLog (3, "+ AppleOnboardAudio::WritePRAMVol leftVol=%lu, rightVol=%lu",leftVol,  rightVol);
    
    if (platform) {
		debugIOLog (3,  "... platform 0x%X", (unsigned int)platform );
		pramVolume = VolumeToPRAMValue (leftVol, rightVol);
#if 0
		curPRAMVol = pramVolume ^ 0xFF;
		debugIOLog (3,  "... target pramVolume = 0x%X, curPRAMVol = 0x%X", pramVolume, curPRAMVol );
#endif
		// get the old value to compare it with
		err = platform->readXPRAM((IOByteCount)kPRamVolumeAddr, &curPRAMVol, (IOByteCount)1);
		if ( kIOReturnSuccess == err ) {
			debugIOLog (3,  "... curPRAMVol = 0x%X before write", (curPRAMVol & 0x07) );
			// Update only if there is a change
			if (pramVolume != (curPRAMVol & 0x07)) {
				// clear bottom 3 bits of volume control byte from PRAM low memory image
				curPRAMVol = (curPRAMVol & 0xF8) | pramVolume;
				debugIOLog (3, "... curPRAMVol = 0x%x",curPRAMVol);
				// write out the volume control byte to PRAM
				err = platform->writeXPRAM((IOByteCount)kPRamVolumeAddr, &curPRAMVol,(IOByteCount) 1);
				if ( kIOReturnSuccess != err ) {
					debugIOLog (3,  "0x%X = platform->writeXPRAM( 0x%X, & 0x%X, 1 ), value = 0x%X", err, (unsigned int)kPRamVolumeAddr, (unsigned int)&curPRAMVol, (unsigned int)curPRAMVol );
				} else {
#if 0
					err = platform->readXPRAM((IOByteCount)kPRamVolumeAddr, &curPRAMVol, (IOByteCount)1);
					if ( kIOReturnSuccess == err ) {
						if ( ( 0x07 & curPRAMVol ) != pramVolume ) {
							debugIOLog (3,  "PRAM Read after Write did not compare:  Write = 0x%X, Read = 0x%X", (unsigned int)pramVolume, (unsigned int)curPRAMVol );
						} else {
							debugIOLog (3,  "PRAM verified after write!" );
						}
					} else {
						debugIOLog (3,  "Could not readXPRAM to verify write!" );
					}
#endif
					mUCState.ucPramData = (UInt32)curPRAMVol;
					mUCState.ucPramVolume = mUCState.ucPramData & 0x00000007;
				}
			} else {
				debugIOLog (3,  "PRAM write request is to current value: no I/O" );
			}
		} else {
			debugIOLog (3,  "Could not readXPRAM prior to write! Error 0x%X", err );
		}
	} else {
		debugIOLog (3,  "... no platform" );
	}
    debugIOLog (3, "- AppleOnboardAudio::WritePRAMVol");
}

UInt8 AppleOnboardAudio::ReadPRAMVol (void) {
	UInt8						curPRAMVol;
	IODTPlatformExpert * 		platform;

	curPRAMVol = 0;
	platform = OSDynamicCast(IODTPlatformExpert,getPlatform());

    if (platform) {
		platform->readXPRAM((IOByteCount)kPRamVolumeAddr, &curPRAMVol, (IOByteCount)1);
		curPRAMVol &= 0x07;
	}

	return curPRAMVol;
}

#pragma mark +USER CLIENT

//===========================================================================================================================
//	newUserClient
//===========================================================================================================================

IOReturn AppleOnboardAudio::newUserClient( task_t 			inOwningTask,
										 void *				inSecurityID,
										 UInt32 			inType,
										 IOUserClient **	outHandler )
{
	#pragma unused( inType )
	
	IOUserClient *		userClientPtr;
    IOReturn 			err;
	bool				result;
	
//	debugIOLog (3,  "[AppleOnboardAudio] creating user client for task 0x%08lX", ( UInt32 ) inOwningTask );
	err = kIOReturnError;
	result = false;
	
	// Create the user client object
	userClientPtr = AppleOnboardAudioUserClient::Create( this, inOwningTask );
	FailIf (NULL == userClientPtr, Exit);
    
	// Set up the user client	
	err = kIOReturnError;
	result = userClientPtr->attach( this );
	FailIf (!result, Exit);

	result = userClientPtr->start( this );
	FailIf (!result, Exit);
	
	// Success
    *outHandler = userClientPtr;
	err = kIOReturnSuccess;
	
Exit:
//	debugIOLog (3,  "[AppleOnboardAudio] newUserClient done (err=%d)", err );
	if( err != kIOReturnSuccess )
	{
		if( userClientPtr )
		{
			userClientPtr->detach( this );
			userClientPtr->release();
		}
	}
	return( err );
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleOnboardAudio::getPlatformState ( UInt32 arg2, void * outState ) {
#pragma unused ( arg2 )
	IOReturn		result = kIOReturnError;
	
	FailIf ( NULL == outState, Exit );
	FailIf ( NULL == mPlatformInterface, Exit );
	result = mPlatformInterface->getPlatformState ( (PlatformStateStructPtr)outState );
Exit:
	return result;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleOnboardAudio::getPluginState ( HardwarePluginType thePluginType, void * outState ) {
	AudioHardwareObjectInterface *		thePluginObject;
	IOReturn							result = kIOReturnError;

	debugIOLog (7,  "AppleOnboardAudio[%ld]::getPluginState for type %d", mInstanceIndex, thePluginType );
	
	FailIf ( NULL == outState, Exit );
	thePluginObject = findPluginForType ( thePluginType );
	//	No fail messages here please!  The AOA Viewer always queries for plugin modules that may not
	//	be loaded and a fail message will overflow the message log!
	if ( NULL != thePluginObject ) {
		result = thePluginObject->getPluginState ( (HardwarePluginDescriptorPtr)outState );
	}
Exit:
	return result;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleOnboardAudio::getDMAStateAndFormat ( UInt32 arg2, void * outState ) {
	IOReturn			result;
	
	switch ( (DMA_STATE_SELECTOR)arg2 ) {
		case kGetDMAStateAndFormat:			result = mDriverDMAEngine->copyDMAStateAndFormat ( (DBDMAUserClientStructPtr)outState );	break;
		case kGetDMAInputChannelCommands:	result = mDriverDMAEngine->copyInputChannelCommands ( outState );							break;
		case kGetDMAOutputChannelCommands:	result = mDriverDMAEngine->copyOutputChannelCommands ( outState );							break;
		case kGetInputChannelRegisters:		result = mDriverDMAEngine->copyInputChannelRegisters ( outState );							break;
		case kGetOutputChannelRegisters:	result = mDriverDMAEngine->copyOutputChannelRegisters ( outState );							break;
		default:							result = kIOReturnBadArgument;																break;
	}
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleOnboardAudio::getSoftwareProcessingState ( UInt32 arg2, void * outState ) {
#pragma unused ( arg2 )
	return kIOReturnSuccess;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleOnboardAudio::getAOAState ( UInt32 arg2, void * outState ) {
#pragma unused ( arg2 )
	IOReturn		result;
	
	result = kIOReturnError;
	FailIf ( 0 != arg2, Exit );
	FailIf (NULL == outState, Exit );
	
	debugIOLog (3,  "AppleOnboardAudio[%ld]::getAOAState for type %ld", mInstanceIndex, arg2 );

	((AOAStateUserClientStructPtr)outState)->ucPramData = mUCState.ucPramData;
	((AOAStateUserClientStructPtr)outState)->ucPramVolume = mUCState.ucPramVolume;
	((AOAStateUserClientStructPtr)outState)->ucPowerState = mUCState.ucPowerState;
	((AOAStateUserClientStructPtr)outState)->ucLayoutID = mLayoutID;
	result = kIOReturnSuccess;
Exit:
	return result;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleOnboardAudio::getTransportInterfaceState ( UInt32 arg2, void * outState ) {
#pragma unused ( arg2 )
	IOReturn		result = kIOReturnError;
	
	if ( NULL != mTransportInterface ) {
		result = mTransportInterface->getTransportInterfaceState ( (TransportStateStructPtr)outState );
	}
	return result;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleOnboardAudio::setPlatformState ( UInt32 arg2, void * inState ) {
#pragma unused ( arg2 )
	IOReturn		result = kIOReturnError;
	
	FailIf ( NULL == inState, Exit );
	FailIf ( NULL == mPlatformInterface, Exit );
	result = mPlatformInterface->setPlatformState ( (PlatformStateStructPtr)inState );
Exit:
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleOnboardAudio::setPluginState ( HardwarePluginType thePluginType, void * inState ) {
	AudioHardwareObjectInterface *		thePluginObject;
	IOReturn							result = kIOReturnError;
	
	FailIf ( NULL == inState, Exit );
	thePluginObject = findPluginForType ( thePluginType );
	FailIf ( NULL == thePluginObject, Exit );
	result = thePluginObject->setPluginState ( (HardwarePluginDescriptorPtr)inState );
Exit:
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleOnboardAudio::setDMAState ( UInt32 arg2, void * inState ) {
#pragma unused ( arg2 )
	return kIOReturnError;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleOnboardAudio::setSoftwareProcessingState ( UInt32 arg2, void * inState ) {
#pragma unused ( arg2 )
	return kIOReturnSuccess;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleOnboardAudio::setAOAState ( UInt32 arg2, void * inState ) {
#pragma unused ( arg2 )
	return kIOReturnError;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleOnboardAudio::setTransportInterfaceState ( UInt32 arg2, void * inState ) {
#pragma unused ( arg2 )
	IOReturn		result = kIOReturnError;
	
	if ( NULL != mTransportInterface ) {
		result = mTransportInterface->setTransportInterfaceState ( (TransportStateStructPtr)inState );
	}
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

ConfigChangeHelper::ConfigChangeHelper (IOAudioEngine * inEngine, UInt32 inSleep) {
	mDriverDMAEngine = inEngine;
	if (NULL != mDriverDMAEngine) {
		debugIOLog (4, "ConfigChangeHelper (%p, %ld): calling pauseAudioEngine and beginConfigurationChange", inEngine, inSleep);
		mDriverDMAEngine->pauseAudioEngine ();
		if (0 != inSleep) {
			debugIOLog (4, "waiting %d ms between pause and begin", inSleep);
			IOSleep ( 10 );
		}
		mDriverDMAEngine->beginConfigurationChange ();
	}
}

ConfigChangeHelper::~ConfigChangeHelper () {
	if (NULL != mDriverDMAEngine) {
		debugIOLog (4, "~ConfigChangeHelper: calling completeConfigurationChange and resumeAudioEngine");
		mDriverDMAEngine->completeConfigurationChange ();
		mDriverDMAEngine->resumeAudioEngine ();
	}
}
