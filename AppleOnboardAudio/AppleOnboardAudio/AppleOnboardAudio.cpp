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

#define kBUILD_FOR_Q78_DEADBUG_CS8406				0

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

	debugIOLog (3, "AppleOnboardAudio[%ld]::handleOpen(%p)", mInstanceIndex, forClient);

	if (kFamilyOption_OpenMultiple == options) {
		result = true;
	} else {
		result = super::handleOpen ( forClient, options, arg );
		FailIf (!result, Exit);
	}

	registerPlugin ( (AudioHardwareObjectInterface *)forClient );

Exit:
	debugIOLog (3, "AppleOnboardAudio[%ld]::handleOpen(%p) returns %s", mInstanceIndex, forClient, result == true ? "true" : "false");
	return result;
}

void AppleOnboardAudio::handleClose (IOService * forClient, IOOptionBits options ) 
{
	debugIOLog (3, "AppleOnboardAudio[%ld]::handleClose(%p)", mInstanceIndex, forClient);

	unRegisterPlugin ( (AudioHardwareObjectInterface *)forClient );

	if (options != kFamilyOption_OpenMultiple) {
		super::handleClose ( forClient, options );
	}
	
	return;
}

bool AppleOnboardAudio::willTerminate ( IOService * provider, IOOptionBits options )
{
	bool result = super::willTerminate ( provider, options );

	provider->close(this);

	debugIOLog (3, "AppleOnboardAudio[%ld]::willTerminate(%p) returns %d", mInstanceIndex, provider, result);

	return result;
}

// Called by the plugin objects from their start() method so that AppleOnboardAudio knows about them and will call them as needed.
void AppleOnboardAudio::registerPlugin (AudioHardwareObjectInterface *thePlugin) {
//	IOCommandGate *				cg;

	debugIOLog (3, "AppleOnboardAudio[%ld]::registerPlugin (%p)", mInstanceIndex, thePlugin);
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

	debugIOLog (3, "AppleOnboardAudio[%ld]::stop(), audioEngines = %p, isInactive() = %d", mInstanceIndex, audioEngines, isInactive());
Exit:
	super::stop (provider);
}

void AppleOnboardAudio::unRegisterPlugin (AudioHardwareObjectInterface *inPlugin) {
	AudioHardwareObjectInterface *		thePluginObject;
	UInt32								index;
	UInt32								count;

	debugIOLog (3, "AppleOnboardAudio[%ld]::unRegisterPlugin (%p)", mInstanceIndex, inPlugin);

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
	debugIOLog (3, "AppleOnboardAudio[%ld]::plugin %p registering", device->mInstanceIndex, arg1);

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
	layoutEntry = OSDynamicCast (OSDictionary, layouts->getObject (theAOA->mMatchingIndex));
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

	debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::hasMasterVolumeControl('%4s')", mInstanceIndex, outputEntry );
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
	debugIOLog ( 3, "- AppleOnboardAudio[%ld]::hasMasterVolumeControl('%4s') returns %d", mInstanceIndex, outputEntry, hasMasterVolControl );
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

	debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::hasLeftVolumeControl('%4s')", mInstanceIndex, outputEntry );
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
	debugIOLog ( 3, "- AppleOnboardAudio[%ld]::hasLeftVolumeControl('%4s') returns %d", mInstanceIndex, outputEntry, hasLeftVolControl );
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

	debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::hasRightVolumeControl('%4s')", mInstanceIndex, outputEntry );
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
	debugIOLog ( 3, "- AppleOnboardAudio[%ld]::hasRightVolumeControl('%4s') returns %d", mInstanceIndex, outputEntry, hasRightVolControl );
	return hasRightVolControl;
}

void AppleOnboardAudio::setUseInputGainControls (const char * inputEntry) {
	OSDictionary *					dictEntry;
	OSArray *						controlsArray;
	OSString *						controlString;
	UInt32							controlsCount;
	UInt32							index;

	debugIOLog (3, "AppleOnboardAudio[%ld]::setUseInputGainControls(%s)", mInstanceIndex, inputEntry);
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

	debugIOLog (3,  "+ AppleOnboardAudio[%ld]::setUsePlaythroughControl(%s)", mInstanceIndex, inputEntry );
	mUsePlaythroughControl = FALSE;

	dictEntry = OSDynamicCast ( OSDictionary, getLayoutEntry ( inputEntry, this) );
	FailIf ( NULL == dictEntry, Exit );

	playthroughOSBoolean = OSDynamicCast ( OSBoolean, dictEntry->getObject ( kPlaythroughControlString ) );
	FailIf ( NULL == playthroughOSBoolean, Exit );

	mUsePlaythroughControl = playthroughOSBoolean->getValue ();
Exit:
	debugIOLog (3,  "- AppleOnboardAudio[%ld]::setUsePlaythroughControl(%s), mUsePlaythroughControl = %d", mInstanceIndex, inputEntry, mUsePlaythroughControl );
	return;
}
//	[3281535]	} end
			
IOReturn AppleOnboardAudio::validateOutputFormatChangeRequest (const IOAudioStreamFormat * inFormat, const IOAudioSampleRate * inRate) {
	IOReturn							result;

	result = kIOReturnSuccess;

	if (kIOAudioStreamSampleFormat1937AC3 == inFormat->fSampleFormat) {
		if (NULL != mOutputSelector) {
			OSNumber *					connectionCodeNumber;
			connectionCodeNumber = OSNumber::withNumber (kIOAudioOutputPortSubTypeSPDIF, 32);

			// [3656784] must check if digital output is available, otherwise this encoded format selection is denied - aml
			// should add a safety check for machines with no digital out even though DVD player doesn't allow ac3 selection - check output bitmap?
			if (!(kGPIO_Unknown == mPlatformInterface->getComboOutJackTypeConnected() || kGPIO_Connected == mPlatformInterface->getDigitalOutConnected())) {
				debugIOLog (3, "AppleOnboardAudio::validateOutputFormatChangeRequest found invalid encoded format request");
				result = kIOReturnError;
			}
		}
	}

	return result;
}

IOReturn AppleOnboardAudio::validateInputFormatChangeRequest (const IOAudioStreamFormat * inFormat, const IOAudioSampleRate * inRate) {
	return kIOReturnSuccess;
}
			
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleOnboardAudio::formatChangeRequest (const IOAudioStreamFormat * inFormat, const IOAudioSampleRate * inRate) {
	IOReturn							result;
	OSNumber *							connectionCodeNumber;
	bool								needsUpdate = FALSE;	//  [3628559]   (22 April 2004) rbm
	
	result = kIOReturnError;

	debugIOLog (3, "+ AppleOnboardAudio[%ld]::formatChangeRequest (%p, %p)", mInstanceIndex, inFormat, inRate);

	//  [3628559]   Format changes (i.e. bit depth or encoding) should be applied to any audio hardware.
	//  Sample rate changes should only be applied to hardware that supports sample rate changes.  Since
	//  'Slave Only' hardware determines sample rate from recovery of an external clock source, the 
	//  request to change the sample rate on a 'Slave Only' device should be blocked.  This will avoid
	//  pausing and resuming the audio engine on devices that cannot apply the request to the hardware.

	//  [3628559]   begin   {   (22 April 2004) rbm
	if ( NULL != inFormat ) {
		needsUpdate = TRUE;
	}
	if ( NULL != inRate ) {
		if ( kTransportInterfaceType_I2S_Slave_Only != mTransportInterface->transportGetTransportInterfaceType() ) {	//  [3628559]   Dont attempt to change sample rate on a slave only device
			needsUpdate = TRUE;
		}
	}
	//  }   end		[3628559]   (22 April 2004) rbm
	
	if ( needsUpdate ) {																		//  [3628559]   (22 April 2004) rbm
		if (mMuteAmpWhenClockInterrupted) {														// aml, this was checking the UI mutes amps flag which is wrong
			muteAllAmps ();
		}

		callPluginsInOrder (kBeginFormatChange, NULL);											//	[3558796]	aml

		if (NULL != inFormat) {
			debugIOLog (3, "AppleOnboardAudio[%ld]::formatChangeRequest with bit width %d", mInstanceIndex, inFormat->fBitWidth);
			result = mTransportInterface->transportSetSampleWidth (inFormat->fBitDepth, inFormat->fBitWidth);
			callPluginsInOrder (kSetSampleBitDepth, inFormat->fBitDepth);
			if (kIOAudioStreamSampleFormat1937AC3 == inFormat->fSampleFormat) {
				if (NULL != mOutputSelector) {
					connectionCodeNumber = OSNumber::withNumber (kIOAudioOutputPortSubTypeSPDIF, 32);

					// [3656784] must check if digital output is available, otherwise this encoded format selection is denied - aml
					// should add a safety check for machines with no digital out even though DVD player doesn't allow ac3 selection - check output bitmap?
					debugIOLog (3, "mPlatformInterface->getComboOutJackTypeConnected() = %ld", mPlatformInterface->getComboOutJackTypeConnected());
					debugIOLog (3, "mPlatformInterface->getDigitalOutConnected() = %ld", mPlatformInterface->getDigitalOutConnected());
					if (kGPIO_Unknown == mPlatformInterface->getComboOutJackTypeConnected() || kGPIO_Connected == mPlatformInterface->getDigitalOutConnected()) {
						mOutputSelector->setValue (connectionCodeNumber);
						connectionCodeNumber->release ();
						mEncodedOutputFormat = true;
						debugIOLog (3, "encoded format request honored");
					} else {
						debugIOLog (3, "encoded format requested, but no digital hardware is connected");
						callPluginsInOrder (kEndFormatChange, NULL);
						selectCodecOutputWithMuteState (mIsMute);
						result = kIOReturnError;
						goto Exit;
					}
				}
			} else {
				mEncodedOutputFormat = false;		
				debugIOLog (5, "non-encoded format");
			}
			result = callPluginsInOrder ( kSetSampleType, inFormat->fSampleFormat );
		}
		if (NULL != inRate) {
			mSampleRateSelectInProcessSemaphore = true;
			
			debugIOLog (3, "AppleOnboardAudio[%ld]::formatChangeRequest with rate %ld", mInstanceIndex, inRate->whole);
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

		callPluginsInOrder (kEndFormatChange, NULL);											//	[3558796]	aml
		selectCodecOutputWithMuteState (mIsMute);
		if ( NULL != mOutputSelector ) {
			selectOutputAmplifiers (mOutputSelector->getIntValue (), mIsMute);
		}
	}

Exit:
	debugIOLog (3, "- AppleOnboardAudio[%ld]::formatChangeRequest returns %d", result);
	return result;
}

IOReturn AppleOnboardAudio::callPluginsInReverseOrder (UInt32 inSelector, UInt32 newValue) {
	AudioHardwareObjectInterface *		thePluginObject;
	OSArray *							pluginOrderArray;
	OSString *							pluginName;
	SInt32								index;
	UInt32								pluginOrderArrayCount;
	IOReturn							result;

	debugIOLog (7, "+ AppleOnboardAudio[%ld]::callPluginsInReverseOrder (%lX, %lX)", mInstanceIndex, inSelector, newValue);
	result = kIOReturnBadArgument;

	pluginOrderArray = OSDynamicCast (OSArray, getLayoutEntry (kPluginRecoveryOrder, this));
	FailIf (NULL == pluginOrderArray, Exit);

	pluginOrderArrayCount = pluginOrderArray->getCount ();

	for (index = pluginOrderArrayCount - 1; index >= 0; index--) {
		pluginName = OSDynamicCast(OSString, pluginOrderArray->getObject(index));
		if (NULL == pluginName) {
			debugIOLog (3, "Corrupt %s entry in AppleOnboardAudio[%ld] Info.plist", kPluginRecoveryOrder, mInstanceIndex);
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
	debugIOLog (7, "- AppleOnboardAudio[%ld]::callPluginsInReverseOrder (%lX, %lX) returns %lX", mInstanceIndex, inSelector, newValue, result);
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
	
	debugIOLog (7, "+ AppleOnboardAudio[%ld]::callPluginsInOrder (%lX, %lX)", mInstanceIndex, inSelector, newValue);

	result = kIOReturnBadArgument;

	pluginOrderArray = OSDynamicCast (OSArray, getLayoutEntry (kPluginRecoveryOrder, this));
	FailIf (NULL == pluginOrderArray, Exit);

	pluginOrderArrayCount = pluginOrderArray->getCount ();

	result = kIOReturnSuccess;
	for (index = 0; index < pluginOrderArrayCount; index++) {
		pluginName = OSDynamicCast(OSString, pluginOrderArray->getObject(index));
		if (NULL == pluginName) {
			debugIOLog (3, "  Corrupt %s entry in AppleOnboardAudio[%ld] Info.plist", kPluginRecoveryOrder, mInstanceIndex);
			continue;
		}
		thePluginObject = getPluginObjectWithName (pluginName);
		if (NULL == thePluginObject) {
			debugIOLog (3, "  Can't find required AppleOnboardAudio plugin from %s entry loaded", kPluginRecoveryOrder);
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
			case kSetEnableSPDIFOut:
				tempResult = thePluginObject->setSPDIFOutEnable ( newValue );
				break;
			case kPreDMAEngineInit:
				boolResult = thePluginObject->preDMAEngineInit ();
				if ( !boolResult ) {
					debugIOLog (1, "  AppleOnboardAudio[%ld]::callPluginsInOrder: preDMAEngineInit on thePluginObject %p failed!", mInstanceIndex, thePluginObject);	
					mPluginObjects->removeObject ( index );
					index--;
					tempResult = kIOReturnError;
				}
				break;
			case kPostDMAEngineInit:
				boolResult = thePluginObject->postDMAEngineInit ();
				if ( !boolResult ) {
					debugIOLog (1, "  AppleOnboardAudio[%ld]::callPluginsInOrder: postDMAEngineInit on thePluginObject %p failed!", mInstanceIndex, thePluginObject);	
					mPluginObjects->removeObject ( index );
					index--;
					tempResult = kIOReturnError;
				}
				break;
			case kPowerStateChange:
				if (kIOAudioDeviceSleep == newValue) {
					debugIOLog (3, "  AppleOnboardAudio[%ld]::callPluginsInOrder ### Telling %s to sleep", mInstanceIndex, pluginName->getCStringNoCopy ());
					tempResult = thePluginObject->performDeviceSleep ();
				} else if (kIOAudioDeviceActive == newValue) {
					debugIOLog (3, "  AppleOnboardAudio[%ld]::callPluginsInOrder ### Telling %s to wake", mInstanceIndex, pluginName->getCStringNoCopy ());
					tempResult = thePluginObject->performDeviceWake ();
				} else if (kIOAudioDeviceIdle == newValue) {
					// thePluginObject->performDeviceIdle ();			// Not implemented because no hardware supports it
				}
				break;
			case kDigitalInStatus:
				thePluginObject->notifyHardwareEvent ( inSelector, newValue );  //  [3628559]
				break;
			case kCodecErrorInterruptStatus:
				//	CAUTION:	Calling the plugin here may result in a nested call back to 'callPluginsInOrder'!!!
				//				Nested calls to 'callPluginsInOrder' must not pass the same 'inSelector' value.
				debugIOLog ( 6, "  AppleOnboardAudio[%d]::callPluginsInOrder INVOKES thePluginObject->notifyHardwareEvent ( kCodecErrorInterruptStatus, %d )", mInstanceIndex, newValue );
				thePluginObject->notifyHardwareEvent ( inSelector, newValue );
				break;
			case kCodecInterruptStatus:
				//	CAUTION:	Calling the plugin here may result in a nested call back to 'callPluginsInOrder'!!!
				//				Nested calls to 'callPluginsInOrder' must not pass the same 'inSelector' value.
				debugIOLog ( 6, "  AppleOnboardAudio[%d]::callPluginsInOrder INVOKES thePluginObject->notifyHardwareEvent ( kCodecInterruptStatus, %d )", mInstanceIndex, newValue );
				thePluginObject->notifyHardwareEvent ( inSelector, newValue );
				break;
			case kRunPollTask:
				thePluginObject->poll ();
				break;
			case kSetMuteState:
				debugIOLog (3,  "  callPluginsInOrder calls thePluginObject->setMute ( %ld )", newValue );
				tempResult = thePluginObject->setMute ( newValue );
				break;
			case kSetAnalogMuteState:			//	[3435307]	
				debugIOLog (3,  "  callPluginsInOrder calls thePluginObject->setMute ( %ld, kAnalogAudioSelector )", newValue );
				tempResult = thePluginObject->setMute ( newValue, kAnalogAudioSelector );
				break;
			case kSetDigitalMuteState:			//	[3435307]	
				debugIOLog (3,  "  callPluginsInOrder calls thePluginObject->setMute ( %ld, kDigitalAudioSelector )", newValue );
				tempResult = thePluginObject->setMute ( newValue, kDigitalAudioSelector );
				break;
			case kBeginFormatChange:		//	[3558796]
				debugIOLog (3,  "  callPluginsInOrder calls thePluginObject->beginFormatChange ()");
				tempResult = thePluginObject->beginFormatChange ();
				break;
			case kEndFormatChange:			//	[3558796]	
				debugIOLog (3,  "  callPluginsInOrder calls thePluginObject->endFormatChange ()");
				tempResult = thePluginObject->endFormatChange ();
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
	debugIOLog (7, "- AppleOnboardAudio[%ld]::callPluginsInOrder (%lX, %lX) returns %lX", mInstanceIndex, inSelector, newValue, result);
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
	
//	debugIOLog (3,  "+ AppleOnboardAudio[%ld]::findPluginForType (%d )", mInstanceIndex, (unsigned int)pluginType );

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
//	debugIOLog (3,  "- AppleOnboardAudio[%ld]::findPluginForType (%d ) returns %p", mInstanceIndex, (unsigned int)pluginType, result );
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
//	[3515371]
void AppleOnboardAudio::pollTimerCallback ( OSObject *owner, IOTimerEventSource *device ) {
	AppleOnboardAudio *			audioDevice;

	audioDevice = OSDynamicCast ( AppleOnboardAudio, owner );
	FailIf ( NULL == audioDevice, Exit );
	audioDevice->runPollTasksEventHandler ();
Exit:
	return;
}


//	--------------------------------------------------------------------------------
//	[3515371]
void	AppleOnboardAudio::runPollTasksEventHandler ( void ) {
	IOCommandGate *				cg;
	
	cg = getCommandGate ();
	if (NULL != cg) {
		cg->runAction ( runPolledTasks, (void*)0, (void*)0 );
	}
	return;
}


//	--------------------------------------------------------------------------------
//	[3515371]
IOReturn AppleOnboardAudio::runPolledTasks (OSObject * owner, void * arg1, void * arg2, void * arg3, void * arg4) {
	FailIf ( NULL == owner, Exit );
	((AppleOnboardAudio*)owner)->protectedRunPolledTasks();
Exit:
	return kIOReturnSuccess;
}


//	--------------------------------------------------------------------------------
//	The AppleOnboardAudio object has a polled timer task that is used to provide
//	periodic service of objects owned by AppleOnboardAudio.  An object can obtain
//	periodic service by implementing the 'poll()' method.
void AppleOnboardAudio::protectedRunPolledTasks ( void ) {
	IOReturn			err;
	UInt32				errorCount = 0;
	UInt32				transportType;
	
	debugIOLog ( 5, "+ AppleOnboardAudio[%ld]::runPolledTasks(), mDelayPollAfterWakeFromSleep %d", mInstanceIndex, mDelayPollAfterWakeFromSleep );
	
	if ( 0 != mDelayPollAfterWakeFromSleep ) {										//  [3686032]
		mDelayPollAfterWakeFromSleep--;												//  [3686032]
	}
	if ( 0 == mDelayPollAfterWakeFromSleep ) {										//  [3686032]
		transportType = mTransportInterface->transportGetTransportInterfaceType();  //  [3655075]

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
						//  Note that when switching from internal clock to external clock that it
						//  may be possible to obtain an illegal rate until the clock source has
						//  stablized (i.e. a PLL may need to lock) so several attempts are made
						//  to set the sample rate at the CODEC if the rate is rejected in the hope
						//  that a valid rate will be achieved when the clock stabilizes.
						do {
							err = callPluginsInOrder ( kSetSampleRate, transportSampleRate.whole );
							if ( kIOReturnSuccess != err ) {
								IOSleep(1);
								errorCount++;
							}
						} while ( ( errorCount < kMiniumumPollsToAquireClockLock ) && ( kIOReturnSuccess != err ) );
						
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
							debugIOLog ( 4, "  *** about to mDriverDMAEngine->hardwareSampleRateChanged ( %d )", mTransportSampleRate.whole );	//  [3684994]
							mDriverDMAEngine->hardwareSampleRateChanged ( &mTransportSampleRate );
						} else {
							if ( kTransportInterfaceType_I2S_Slave_Only != transportType ) {  //  [3628559]
								//	Set the hardware to MASTER mode.
								debugIOLog ( 5, "  ** AppleOnboardAudio[%ld]::runPolledTasks switching to INTERNAL CLOCK", mInstanceIndex );
								clockSelectorChanged ( kClockSourceSelectionInternal );
								if ( NULL != mExternalClockSelector ) {
									//	Flush the control value (i.e. MASTER = internal).
									OSNumber *			clockSourceSelector;
									clockSourceSelector = OSNumber::withNumber (kClockSourceSelectionInternal, 32);
				
									mExternalClockSelector->hardwareValueChanged (clockSourceSelector);
									clockSourceSelector->release ();
					
									transportSampleRate.whole = mTransportInterface->transportGetSampleRate (); //  acquire the new sample rate from the internal clock source
									err = callPluginsInOrder ( kSetSampleRate, mTransportSampleRate.whole );
								}
							}
						}
					}
				}
				//	[3305011, 3514709]	begin {
				//		If the DMA engine dies with no indication of a hardware error then recovery must be performed 
				//		by stopping and starting the engine.  If the transport object is a 'SlaveOnly' subclass then 
				//		do not attempt to change the clock source but do perform the configuration change helper object
				//		so that the DMA engine is stopped and restarted (required to recover from a DBDMA dead status).
				if ( mDriverDMAEngine->engineDied() ) {
					debugIOLog ( 5, "  ** AppleOnboardAudio[%ld]::runPolledTasks invoking 'ConfigChangeHelper' to recover from 'DEAD' DMA", mInstanceIndex );
					ConfigChangeHelper theConfigeChangeHelper(mDriverDMAEngine);	
					if ( ( kTransportInterfaceType_I2S_Slave_Only != transportType ) && ( kTransportInterfaceType_I2S_Opaque_Slave_Only != transportType ) ) {
						if ( kTRANSPORT_MASTER_CLOCK != mTransportInterface->transportGetClockSelect() ) {
							UInt32 currentBitDepth = mTransportInterface->transportGetSampleWidth();
							mTransportInterface->transportSetSampleWidth ( currentBitDepth, mTransportInterface->transportGetDMAWidth() );
							callPluginsInOrder ( kSetSampleBitDepth, currentBitDepth );
						}
					}
				}
				//	} end	[3305011, 3514709]
			}
			
			//	Then give other objects requiring a poll to have an opportunity to execute.
			
			mPlatformInterface->poll ();
			mTransportInterface->poll ();
			callPluginsInOrder ( kRunPollTask, 0 );
		}	//	} end	[3326541]
	} else {	//  [3686032]   only invoke hardware plugin poll as no polled interrupt notifications are desired until full polling is enabled
		if ( kIOAudioDeviceActive == ourPowerState ) {
			callPluginsInOrder ( kRunPollTask, 0 );
		}
	}
	
	setPollTimer ();
	debugIOLog ( 5, "- AppleOnboardAudio[%ld]::runPolledTasks()", mInstanceIndex );
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
	
	debugIOLog (3,  "+ AppleOnboardAudio[%ld]::ParseDetectCollection() detectCollection 0x%lX", mInstanceIndex, mDetectCollection );

	if ( kGPIO_Selector_NotAssociated == mPlatformInterface->getComboOutAssociation() ) {	//	[3514514]
		if ( mDetectCollection & kSndHWLineOutput ) {
			result = kIOAudioOutputPortSubTypeLine;
		} else if ( mDetectCollection & kSndHWDigitalOutput ) {
			result = kIOAudioOutputPortSubTypeSPDIF;
		} else if ( mDetectCollection & kSndHWCPUHeadphone ) {
			result = kIOAudioOutputPortSubTypeHeadphones;
		} else if ( mDetectCollection & kSndHWCPUExternalSpeaker ) {
			result = kIOAudioOutputPortSubTypeExternalSpeaker;
		} else {
			result = kIOAudioOutputPortSubTypeInternalSpeaker;
		}
	} else {
		if ( mDetectCollection & kSndHWLineOutput ) {
			result = kIOAudioOutputPortSubTypeLine;
		} else if ( mDetectCollection & kSndHWCPUHeadphone ) {
			result = kIOAudioOutputPortSubTypeHeadphones;
		} else if ( mDetectCollection & kSndHWCPUExternalSpeaker ) {
			result = kIOAudioOutputPortSubTypeExternalSpeaker;
		} else {
			result = kIOAudioOutputPortSubTypeInternalSpeaker;
		}
	}

	debugIOLog (3,  "- AppleOnboardAudio[%ld]::ParseDetectCollection returns %4s", mInstanceIndex, (char*)&result );
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt32 AppleOnboardAudio::parseInputDetectCollection ( void ) {
	UInt32		result;
	
	debugIOLog (3,  "+ AppleOnboardAudio[%ld]::parseInputDetectCollection() detectCollection 0x%lX", mInstanceIndex, mDetectCollection );

	if ( mDetectCollection & kSndHWLineInput ) {
		result = kIOAudioOutputPortSubTypeLine;
	} else {
		result = kIOAudioInputPortSubTypeInternalMicrophone;
	}

	debugIOLog (3,  "- AppleOnboardAudio[%ld]::parseInputDetectCollection returns %lX", mInstanceIndex, result );
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	[3514514]	Updated for combo output jacks:
//
//		Line out is to be excluded if a digital output plug is inserted in a
//		combo output jack associated with the line output detect.
//
void AppleOnboardAudio::initializeDetectCollection ( void ) {
	debugIOLog (3,  "+ AppleOnboardAudio[%ld]::initializeDetectCollection() ", mInstanceIndex);

	if ( kGPIO_Connected == mPlatformInterface->getHeadphoneConnected() ) {
		//	[3514514]	begin {
		if ( kGPIO_Selector_HeadphoneDetect == mPlatformInterface->getComboOutAssociation() ) {
			if ( kGPIO_Unknown == mPlatformInterface->getComboOutJackTypeConnected () ) {
				mDetectCollection |= kSndHWCPUHeadphone;
				debugIOLog (3,  "AppleOnboardAudio[%ld]::initializeDetectCollection() - headphones are connected.", mInstanceIndex);
			} else {
				if ( kGPIO_TypeIsAnalog == mPlatformInterface->getComboOutJackTypeConnected () ) {
					mDetectCollection |= kSndHWCPUHeadphone;
					debugIOLog (3,  "AppleOnboardAudio[%ld]::initializeDetectCollection() - headphones are connected.", mInstanceIndex);
				} else {
					mDetectCollection |= kSndHWDigitalOutput;
					debugIOLog (3,  "AppleOnboardAudio[%ld]::initializeDetectCollection() - digital output is connected.", mInstanceIndex);
				}
			}
		} else {
			mDetectCollection |= kSndHWCPUHeadphone;
			debugIOLog (3,  "AppleOnboardAudio[%ld]::initializeDetectCollection() - headphones are connected.", mInstanceIndex);
		}
		//	} end	[3514514]
	} 
	if ( kGPIO_Connected == mPlatformInterface->getLineOutConnected() ) {
		//	[3514514]	begin {
		if ( kGPIO_Selector_LineOutDetect == mPlatformInterface->getComboOutAssociation() ) {
			if ( kGPIO_Unknown == mPlatformInterface->getComboOutJackTypeConnected () ) {
				mDetectCollection |= kSndHWLineOutput;
				debugIOLog (3,  "AppleOnboardAudio[%ld]::initializeDetectCollection() - line output is connected.", mInstanceIndex);
			} else {
				if ( kGPIO_TypeIsAnalog == mPlatformInterface->getComboOutJackTypeConnected () ) {
					mDetectCollection |= kSndHWLineOutput;
					debugIOLog (3,  "AppleOnboardAudio[%ld]::initializeDetectCollection() - line output is connected.", mInstanceIndex);
				} else {
					mDetectCollection |= kSndHWDigitalOutput;
					debugIOLog (3,  "AppleOnboardAudio[%ld]::initializeDetectCollection() - digital output is connected.", mInstanceIndex);
				}
			}
		} else {
			mDetectCollection |= kSndHWLineOutput;
			debugIOLog (3,  "AppleOnboardAudio[%ld]::initializeDetectCollection() - line output is connected.", mInstanceIndex);
		}
		//	} end	[3514514]
	} 
	if ( kGPIO_Connected == mPlatformInterface->getDigitalOutConnected() ) {
		debugIOLog (3,  "AppleOnboardAudio[%ld]::initializeDetectCollection() - digital out is connected.", mInstanceIndex);
		mDetectCollection |= kSndHWDigitalOutput;
	} 
	if ( kGPIO_Connected == mPlatformInterface->getLineInConnected() ) {
		mDetectCollection |= kSndHWLineInput;
	} 
	if ( kGPIO_Connected == mPlatformInterface->getDigitalInConnected() ) {
		mDetectCollection |= kSndHWDigitalInput;
	} 
	if ( kGPIO_Connected == mPlatformInterface->getSpeakerConnected() ) {	//	[3398729]	start	{
		debugIOLog (3,  "AppleOnboardAudio[%ld]::initializeDetectCollection() - external speakers are connected.", mInstanceIndex);
		mDetectCollection |= kSndHWCPUExternalSpeaker;
		mDetectCollection &= ~kSndHWInternalSpeaker;
	} else/* if ( kGPIO_Disconnected == mPlatformInterface->getSpeakerConnected() )*/ {
		debugIOLog (3,  "AppleOnboardAudio[%ld]::initializeDetectCollection() - in internal speakers are connected.", mInstanceIndex);
		mDetectCollection |= kSndHWInternalSpeaker;
		mDetectCollection &= ~kSndHWCPUExternalSpeaker;
	}																		//	}	end	[3398729]
	
	debugIOLog (3,  "- AppleOnboardAudio[%ld]::initializeDetectCollection(), mDetectCollection = %lX", mInstanceIndex, mDetectCollection );
	return;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  [3639422]   This method is responsible for dynamically publishing output ports based on the current state of 
//  the detects.  Built-in sound cards fall into two major classifications in this regard:
//
//  1.  Non-exclusive digital output support
//
//		These cards support digital outputs where the digital output has a dedicated connector (i.e. the digital 
//		output connector does not support analog output).  The connector may or may not have a dedicated digital 
//		output detect signal.
//
//		Such cards support limited override of the auto redirection selection
//		of an output port based on the detect state through UI control.
//
//		Publication of output ports is as follows:
//
//		HP DETECT   LINEOUT DETECT				:   DIGITAL OUT		HEADPHONE		LINEOUT			INT SPEAKER
//
//		OUT			OUT							:	AVAILABLE		UNAVAILBLE		UNAVAILABLE		AVAILABLE
//		OUT			IN							:	AVAILABLE		UNAVAILBLE		AVAILABLE		UNAVAILABLE
//		IN			OUT							:	AVAILABLE		AVAILABLE		UNAVAILABLE		UNAVAILABLE
//		IN			IN							:	AVAILABLE		AVAILABLE		AVAILABLE		UNAVAILABLE
//
//		UI OVERRIDE of auto redirection is available as follows:
//
//		UI SELECTION							:	DIGITAL OUT		HEADPHONE		LINEOUT			INT SPEAKER
//
//		Internal speaker by auto redirection	:   UNMUTED			MUTED			MUTED			UNMUTED
//		Headphone by auto redirection			:   UNMUTED			UNMUTED			MUTED			MUTED
//		Line Out by auto redirection			:   UNMUTED			MUTED			UNMUTED			MUTED
//		Digital by UI override					:   UNMUTED			MUTED			MUTED			MUTED
//
//  2.  Exclusive output support
//
//		These cards may or may not support digital output.  If digital output is supported then the digital output 
//		connector has the ability to support analog on the same connector.  These connectors are commonly referred 
//		to as a 'combo jack'.  Since it is physically impossible to simultaneously connect digital and analog, the 
//		support of digital and analog output is said to be mutually exclusive.
//
//		Such cards do not support override of auto redirection selection.  UI controls are exposed only to present 
//		information about which output port has been selected for the user based on the current detect state.
//
//		HP DETECT   LINEOUT DETECT				:   DIGITAL OUT		HEADPHONE		LINEOUT			INT SPEAKER
//
//		OUT			OUT							:	UNAVAILBLE		UNAVAILBLE		UNAVAILABLE		AVAILABLE
//		OUT			IN - ANALOG					:	UNAVAILBLE		UNAVAILBLE		AVAILABLE		UNAVAILABLE
//		IN			OUT							:	UNAVAILBLE		AVAILABLE		UNAVAILABLE		UNAVAILABLE
//		IN			IN - ANALOG					:	UNSUPPORTED		UNSUPPORTED		UNSUPPORTED		UNSUPPORTED
//		OUT			OUT							:	UNAVAILBLE		UNAVAILBLE		UNAVAILABLE		AVAILABLE
//		OUT			IN - DIGITAL				:	AVAILABLE		UNAVAILBLE		UNAVAILBLE		UNAVAILABLE
//		IN			OUT							:	UNAVAILBLE		AVAILABLE		UNAVAILABLE		UNAVAILABLE
//		IN			IN - DIGITAL				:	UNSUPPORTED		UNSUPPORTED		UNSUPPORTED		UNSUPPORTED
//
//  (7 May 2004 - rbm)
//
void AppleOnboardAudio::updateOutputDetectCollection (UInt32 statusSelector, UInt32 newValue) {
	debugIOLog ( 5, "+ AppleOnboardAudio[%ld]::updateOutputDetectCollection ( %ld, %ld )", mInstanceIndex, statusSelector, newValue );
	
	switch (statusSelector) {
		case kHeadphoneStatus:
			debugIOLog ( 6,  "kHeadphoneStatus mDetectCollection prior to modification %lX", mDetectCollection );
			if (newValue == kInserted) {
				mDetectCollection |= kSndHWCPUHeadphone;
				mDetectCollection &= ~kSndHWInternalSpeaker;
				debugIOLog ( 6, "headphones inserted, mDetectCollection = %lX", mDetectCollection);
				debugIOLog ( 6, "** AppleOnboardAudio[%ld]::updateDetectCollection invoking 'ConfigChangeHelper'", mInstanceIndex );
				ConfigChangeHelper theConfigeChangeHelper(mDriverDMAEngine);	
				mOutputSelector->addAvailableSelection ( kIOAudioOutputPortSubTypeHeadphones, mHeadphoneOutputString );
				debugIOLog ( 6, "added headphones to output selector *");
				mOutputSelector->removeAvailableSelection ( kIOAudioOutputPortSubTypeInternalSpeaker );
				debugIOLog ( 6, "removed internal speaker from output selector *");
			} else if (newValue == kRemoved) {
				mDetectCollection &= ~kSndHWCPUHeadphone;
				mDetectCollection |= kSndHWInternalSpeaker;
				debugIOLog ( 6, "headphones removed, mDetectCollection = %lX", mDetectCollection);
				debugIOLog ( 6, "** AppleOnboardAudio[%ld]::updateDetectCollection invoking 'ConfigChangeHelper'", mInstanceIndex );
				ConfigChangeHelper theConfigeChangeHelper(mDriverDMAEngine);	
				mOutputSelector->removeAvailableSelection ( kIOAudioOutputPortSubTypeHeadphones );
				debugIOLog ( 6, "headphones removed, mDetectCollection = %lX", mDetectCollection);
				//  If headphones are removed and line out is removed then add internal speakers
				if ( 0 == ( mDetectCollection & kSndHWLineOutput ) ) {
					mOutputSelector->addAvailableSelection ( kIOAudioOutputPortSubTypeInternalSpeaker, mInternalSpeakerOutputString );
					debugIOLog ( 6, "added internal speaker to output selector *");
				}
			} else {
				debugIOLog ( 6, "Unknown headphone jack status, mDetectCollection = %lX", mDetectCollection);
			}
			break;
		case kLineOutStatus:
			debugIOLog ( 6,  "kLineOutStatus mDetectCollection prior to modification %lX", mDetectCollection );
			if (newValue == kInserted) {
				mDetectCollection |= kSndHWLineOutput;
				mDetectCollection &= ~kSndHWInternalSpeaker;
				debugIOLog ( 6, "line out inserted.");
				debugIOLog ( 6, "** AppleOnboardAudio[%ld]::updateDetectCollection invoking 'ConfigChangeHelper'", mInstanceIndex );
				ConfigChangeHelper theConfigeChangeHelper(mDriverDMAEngine);	
				mOutputSelector->removeAvailableSelection ( kIOAudioOutputPortSubTypeInternalSpeaker );
				debugIOLog ( 6, "removed internal speaker from output selector **");
				//	[3514514]	If the line out is IN and is associated with a combo out jack then remove
				//				the S/PDIF selector and add the Line Out selector.
				if ( kGPIO_Selector_LineOutDetect == mPlatformInterface->getComboOutAssociation() ) {
					if ( NULL != mLineOutputString ) {
						mOutputSelector->removeAvailableSelection ( kIOAudioOutputPortSubTypeSPDIF );
						debugIOLog ( 6, "removed SPDIF from output selector **");
					}
				}
				debugIOLog ( 6, "add LineOut from output selector");
				mOutputSelector->addAvailableSelection (kIOAudioOutputPortSubTypeLine, mLineOutputString);
			} else if (newValue == kRemoved) {
				mDetectCollection &= ~kSndHWLineOutput;
				mDetectCollection |= kSndHWInternalSpeaker;
				debugIOLog ( 6, "line out removed.");
				debugIOLog ( 6, "** AppleOnboardAudio[%ld]::updateDetectCollection invoking 'ConfigChangeHelper'", mInstanceIndex );
				ConfigChangeHelper theConfigeChangeHelper(mDriverDMAEngine);	
				mOutputSelector->removeAvailableSelection ( kIOAudioOutputPortSubTypeLine );
				debugIOLog ( 6, "removed Line from output selector");
				//	[3514514]	If the line out is OUT and is associated with a combo out jack then remove
				//				the S/PDIF AND Line Out selectors.
				if ( (kGPIO_Unknown != mPlatformInterface->getComboOutJackTypeConnected ()) ) {
					mOutputSelector->removeAvailableSelection ( kIOAudioOutputPortSubTypeSPDIF );
					debugIOLog ( 6, "removed SPDIF from output selector");
				}
				//  If headphones are removed and line out is removed then add internal speakers
				if ( 0 == ( mDetectCollection & kSndHWCPUHeadphone ) ) {
					mOutputSelector->addAvailableSelection ( kIOAudioOutputPortSubTypeInternalSpeaker, mInternalSpeakerOutputString );
					debugIOLog ( 6, "added internal speaker to output selector *");
				}
			} else {
				debugIOLog ( 6, "Unknown line out jack status.");
			}
			break;
		case kDigitalOutStatus:
			debugIOLog ( 6,  "kDigitalOutStatus mDetectCollection prior to modification %lX", mDetectCollection );
			if (newValue == kInserted) {
				mDetectCollection |= kSndHWDigitalOutput;
				mDetectCollection &= ~kSndHWInternalSpeaker;
				debugIOLog ( 6, "digital out inserted.");
				debugIOLog ( 6, "mDigitalOutputString = %p, getComboOutJackTypeConnected = %ld", mDigitalOutputString, mPlatformInterface->getComboOutJackTypeConnected ());
				//	[3514514]	If switching TO an exclusive digital output then remove all other selectors 
				//				associated with the combo output jack supporting that digital output.
				if ( ( NULL != mDigitalOutputString ) && (kGPIO_Unknown != mPlatformInterface->getComboOutJackTypeConnected ()) ) {
					debugIOLog ( 5, "** AppleOnboardAudio[%ld]::updateDetectCollection invoking 'ConfigChangeHelper'", mInstanceIndex );
					ConfigChangeHelper theConfigeChangeHelper(mDriverDMAEngine);	
					if ( kGPIO_Selector_LineOutDetect == mPlatformInterface->getComboOutAssociation() ) {
						mOutputSelector->removeAvailableSelection ( kIOAudioOutputPortSubTypeInternalSpeaker );
						debugIOLog (6, "removed internal speaker from output selector *");
						mOutputSelector->removeAvailableSelection ( kIOAudioOutputPortSubTypeLine );
						debugIOLog (6, "removed line output from output selector");
					} else if ( kGPIO_Selector_HeadphoneDetect == mPlatformInterface->getComboOutAssociation() ) {
						mOutputSelector->removeAvailableSelection ( kIOAudioOutputPortSubTypeInternalSpeaker );
						debugIOLog (6, "removed internal speaker from output selector **");
						mOutputSelector->removeAvailableSelection ( kIOAudioOutputPortSubTypeHeadphones );
						debugIOLog (6, "removed headphone from output selector");
					}
					mOutputSelector->addAvailableSelection (kIOAudioOutputPortSubTypeSPDIF, mDigitalOutputString);
					debugIOLog (6, "added SPDIF to output selector");
				}
			} else if (newValue == kRemoved) {
				mDetectCollection &= ~kSndHWDigitalOutput;
				mDetectCollection |= kSndHWInternalSpeaker;
				debugIOLog (6, "digital out removed.");
				//	[3514514]	If switching FROM an exclusive digital output then remove add other selectors 
				//				associated with the combo output jack supporting that digital output.
				if ( (kGPIO_Unknown != mPlatformInterface->getComboOutJackTypeConnected ()) ) {
					debugIOLog ( 5, "** AppleOnboardAudio[%ld]::updateDetectCollection invoking 'ConfigChangeHelper'", mInstanceIndex );
					ConfigChangeHelper theConfigeChangeHelper(mDriverDMAEngine);	
					if ( kGPIO_Selector_LineOutDetect == mPlatformInterface->getComboOutAssociation() ) {
						mOutputSelector->removeAvailableSelection ( kIOAudioOutputPortSubTypeLine );
						debugIOLog (6, "removed Line from output selector");
						mOutputSelector->addAvailableSelection (kIOAudioOutputPortSubTypeInternalSpeaker, mInternalSpeakerOutputString);
						debugIOLog (6, "added internal speaker to output selector");
					} else if ( kGPIO_Selector_HeadphoneDetect == mPlatformInterface->getComboOutAssociation() ) {
						mOutputSelector->removeAvailableSelection ( kIOAudioOutputPortSubTypeHeadphones );
						debugIOLog (6, "removed headphone from output selector");
						mOutputSelector->addAvailableSelection (kIOAudioOutputPortSubTypeInternalSpeaker, mInternalSpeakerOutputString);
						debugIOLog (6, "added internal speaker to output selector");
					}
					mOutputSelector->removeAvailableSelection ( kIOAudioOutputPortSubTypeSPDIF );
					debugIOLog (6, "removed SPDIF from output selector");
				}
			} else {
				debugIOLog (6, "Unknown digital out jack status.");
			}
			break;
		case kLineInStatus:
			debugIOLog (6,  "kLineInStatus mDetectCollection prior to modification %lX", mDetectCollection );
			if (newValue == kInserted) {
				mDetectCollection |= kSndHWLineInput;
			} else if (newValue == kRemoved) {
				mDetectCollection &= ~kSndHWLineInput;
			} else {
				debugIOLog (6, "Unknown line in status.");
			}
			break;
		case kExtSpeakersStatus:												//	[3398729]	begin	{
			debugIOLog (6,  "kExtSpeakersStatus mDetectCollection prior to modification %lX", mDetectCollection );
			if (newValue == kInserted) {
				mDetectCollection &= ~kSndHWInternalSpeaker;
				mDetectCollection |= kSndHWCPUExternalSpeaker;
				//	[3413551]	begin	{
				if ( ( NULL != mInternalSpeakerOutputString ) && ( NULL != mExternalSpeakerOutputString ) ) {
					debugIOLog ( 5, "** AppleOnboardAudio[%ld]::updateDetectCollection invoking 'ConfigChangeHelper'", mInstanceIndex );
					ConfigChangeHelper theConfigeChangeHelper(mDriverDMAEngine);	
					mOutputSelector->removeAvailableSelection ( kIOAudioOutputPortSubTypeInternalSpeaker );
					debugIOLog (6, "removed internal speaker from output selector ***");
					mOutputSelector->addAvailableSelection (kIOAudioOutputPortSubTypeExternalSpeaker, mExternalSpeakerOutputString);
				}
				//	}	end		[3413551]
				debugIOLog (6, "external speakers inserted, mDetectCollection = %lX", mDetectCollection);
			} else if (newValue == kRemoved) {
				mDetectCollection &= ~kSndHWCPUExternalSpeaker;
				mDetectCollection |= kSndHWInternalSpeaker;
				//	[3413551]	begin	{
				if ( ( NULL != mInternalSpeakerOutputString ) && ( NULL != mExternalSpeakerOutputString ) ) {
					debugIOLog ( 5, "** AppleOnboardAudio[%ld]::updateDetectCollection invoking 'ConfigChangeHelper'", mInstanceIndex );
					ConfigChangeHelper theConfigeChangeHelper(mDriverDMAEngine);	
					mOutputSelector->removeAvailableSelection ( kIOAudioOutputPortSubTypeExternalSpeaker );
					debugIOLog (6, "removed External Speaker from output selector");
					mOutputSelector->addAvailableSelection (kIOAudioOutputPortSubTypeInternalSpeaker, mInternalSpeakerOutputString);
				}
				//	}	end		[3413551]
				debugIOLog (6, "external speakers removed, mDetectCollection = %lX", mDetectCollection);
			} else {
				debugIOLog (6, "Unknown external speakers jack status, mDetectCollection = %lX", mDetectCollection);
			}
			break;																//	}	end	[3398729]
	}
	debugIOLog ( 5, "- AppleOnboardAudio[%ld]::updateOutputDetectCollection ( %ld, %ld ), mDetectCollection = %lX", mInstanceIndex, statusSelector, newValue, mDetectCollection );
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt32  AppleOnboardAudio::getSelectorCodeForOutputEvent (UInt32 eventSelector) {
	UInt32 								selectorCode;

	debugIOLog ( 5, "+ AppleOnboardAudio[%ld]::getSelectorCodeForOutputEvent ( %ld )", mInstanceIndex, eventSelector );
	
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
	debugIOLog ( 5, "- AppleOnboardAudio[%ld]::getSelectorCodeForOutputEvent ( %ld ), mDetectCollection %lX, selectorCode '%4s'", mInstanceIndex, eventSelector, mDetectCollection, (char*)&selectorCode );

	return selectorCode;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AppleOnboardAudio::selectOutputAmplifiers (const UInt32 inSelection, const bool inMuteState, const bool inUpdateAll)
{
	bool								needToWaitForAmps;
	
	debugIOLog (3,  "+ AppleOnboardAudio[%ld]::selectOutputAmplifiers %4s, inMuteState = %d", mInstanceIndex, (char *)&(inSelection), inMuteState );

	needToWaitForAmps = true;
	
	//	[3253678] mute analog outs if playing encoded content, unmute only if not playing 
	//	encoded content (eg. on jack inserts/removals)
	
	//	[3435307]	The amplifier mutes are associated with a port selection but the system mute
	//	state should not be applied to the amplifier mutes but only to the CODECs themselves.
	//	Amplifier mutes are only manipulated in response to a jack state change or power management
	//	tasks but are not manipulated in response to UI selections!!!	RBM
	
	//	[3513367]	If selecting a 'non-digital' output where a detect is available to indicate that
	//	no digital output cable is attached then mute the digital output (i.e. no digital out when
	//	analog out is active where analog v.s. digital output are mutually exclusive).	RBM

	//  [3558796]   Only touch amp mutes here, leave codec unmute separate.  This allows greater reuse of this method with less redundancy.
	//  This method now only unmutes to the current output.  If you want to mute all outputs, call muteAllAmps () explicitly.

	if (FALSE == inMuteState) {	
		switch (inSelection) {
			case kIOAudioOutputPortSubTypeHeadphones:
				debugIOLog (3,  "[AOA] switching amps to headphones.  mEncodedOutputFormat %d", mEncodedOutputFormat );
				mPlatformInterface->setSpeakerMuteState ( kGPIO_Muted );
				if (!mEncodedOutputFormat) {
					mPlatformInterface->setHeadphoneMuteState ( kGPIO_Unmuted );
				}
				if ((mDetectCollection & kSndHWLineOutput) && !mEncodedOutputFormat) {
					mPlatformInterface->setLineOutMuteState ( kGPIO_Unmuted );
				}
				break;
			case kIOAudioOutputPortSubTypeLine:
				debugIOLog (3,  "[AOA] switching amps to line output.  mEncodedOutputFormat %d", mEncodedOutputFormat );
				mPlatformInterface->setSpeakerMuteState ( kGPIO_Muted );
				if (!mEncodedOutputFormat) {
					mPlatformInterface->setLineOutMuteState ( kGPIO_Unmuted );
				}
				if ((mDetectCollection & kSndHWCPUHeadphone) && !mEncodedOutputFormat) {
					mPlatformInterface->setHeadphoneMuteState ( kGPIO_Unmuted );
				}	
				break;
			case kIOAudioOutputPortSubTypeExternalSpeaker:										//	[3398729]	fall through to kIOAudioOutputPortSubTypeInternalSpeaker
			case kIOAudioOutputPortSubTypeInternalSpeaker:
				debugIOLog (3,  "  [AOA] switching amps to speakers.  mEncodedOutputFormat %d", mEncodedOutputFormat );
				if (!mEncodedOutputFormat) {
					mPlatformInterface->setSpeakerMuteState ( kGPIO_Unmuted );
					// [3250195], don't want to get EQ on these outputs if we're on internal speaker.
					mPlatformInterface->setLineOutMuteState ( kGPIO_Muted );
					mPlatformInterface->setHeadphoneMuteState ( kGPIO_Muted );
				}
				break;
			case kIOAudioOutputPortSubTypeSPDIF:
				mPlatformInterface->setSpeakerMuteState ( kGPIO_Muted );
				if (mEncodedOutputFormat) {
					// [3656784] these should always be muted in this case, also the wait for amps being false that was here was wrong because we muted the speaker amp just above
					mPlatformInterface->setLineOutMuteState ( kGPIO_Muted );
					mPlatformInterface->setHeadphoneMuteState ( kGPIO_Muted );
				} else {
					if (inUpdateAll) {
						if ((mDetectCollection & kSndHWCPUHeadphone) && !inMuteState) {
							mPlatformInterface->setHeadphoneMuteState ( kGPIO_Unmuted );
							if ( mDetectCollection & kSndHWLineOutput ) {
								mPlatformInterface->setLineOutMuteState ( kGPIO_Unmuted );
							}
						} else if (mDetectCollection & kSndHWLineOutput && !inMuteState) {
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
	}
	
	if (needToWaitForAmps) {
		IOSleep (mAmpRecoveryMuteDuration);
	}
	
	//	[3581695]	Save the current output selection for waking from sleep.  Waking from
	//				sleep will invoke 'selectOutputAmplifiers' while passing the current output 
	//				selection to release the appropriate amplifier mutes for the current
	//				output selection.
	mCurrentOutputSelection = inSelection;													//	[3581695]	12 Mar 2004, rbm
	
	debugIOLog (3,  "- AppleOnboardAudio[%ld]::selectOutputAmplifiers %4s, inMuteState = %d", mInstanceIndex, (char *)&(inSelection), inMuteState );
    return;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AppleOnboardAudio::muteAllAmps() {
	mPlatformInterface->setHeadphoneMuteState ( kGPIO_Muted );
	mPlatformInterface->setLineOutMuteState ( kGPIO_Muted );
	mPlatformInterface->setSpeakerMuteState ( kGPIO_Muted );
	IOSleep (mAmpRecoveryMuteDuration);
	debugIOLog (3, "± AppleOnboardAudio::muteAllAmps ()");
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AppleOnboardAudio::free()
{
    debugIOLog (3, "+ AppleOnboardAudio[%ld]::free", mInstanceIndex);
	
	removeTimerEvent ( this );

    if (mDriverDMAEngine) {
		debugIOLog (3, "  AppleOnboardAudio[%ld]::free - mDriverDMAEngine retain count = %d", mInstanceIndex, mDriverDMAEngine->getRetainCount());
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
    debugIOLog (3, "- AppleOnboardAudio[%ld]::free, (void)", mInstanceIndex);
}

#pragma mark +PORT HANDLER FUNCTIONS
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
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
			inputState->release ();
		}
	}

}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
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

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
PlatformInterface * AppleOnboardAudio::getPlatformInterfaceObject () {
	return mPlatformInterface;
}

#pragma mark +INTERRUPTS
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AppleOnboardAudio::softwareInterruptHandler (OSObject *object, IOInterruptEventSource *source, int count) {
    UInt16								cachedProducerCount;
	AppleOnboardAudio *					aoa;

	aoa = OSDynamicCast (AppleOnboardAudio, object);
	FailIf ( NULL == aoa, Exit );
	
	for ( UInt32 index = 0; index < kNumberOfActionSelectors; index++ ) {
		cachedProducerCount = aoa->mInterruptProduced[index];
		if ( 0 != cachedProducerCount - aoa->mInterruptConsumed[index] ) {
			aoa->interruptEventHandler ( index, 0 );							//  [3515371]
			aoa->mInterruptConsumed[index] = cachedProducerCount;
			debugIOLog (2, "  Processed interrupt at index %d", index );
		}
	}
Exit:
	return;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AppleOnboardAudio::interruptEventHandler (UInt32 statusSelector, UInt32 newValue) {
	IOCommandGate *						cg;
	
	cg = getCommandGate ();
	if (NULL != cg) {
		cg->runAction (interruptEventHandlerAction, (void *)statusSelector, (void *)newValue);
	}

	return;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Must be called on the workloop via runAction()
IOReturn AppleOnboardAudio::interruptEventHandlerAction (OSObject * owner, void * arg1, void * arg2, void * arg3, void * arg4) {
	IOReturn 							result;
	AppleOnboardAudio * 				aoa;
	
	result = kIOReturnError;
	
	aoa = (AppleOnboardAudio *)owner;
	FailIf (NULL == aoa, Exit);
	debugIOLog (7, "+ AppleOnboardAudio[%ld]::interruptEventAction - (%p, %ld, %ld, %ld, %ld)", aoa->mInstanceIndex, owner, (UInt32)arg1, (UInt32)arg2, (UInt32)arg3, (UInt32)arg4);
	aoa->protectedInterruptEventHandler ((UInt32)arg1, (UInt32)arg2);

	result = kIOReturnSuccess;
	debugIOLog (7, "- AppleOnboardAudio[%ld]::interruptEventAction - (%p, %ld, %ld, %ld, %ld) returns %lX", aoa->mInstanceIndex, owner, (UInt32)arg1, (UInt32)arg2, (UInt32)arg3, (UInt32)arg4, result);
Exit:
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool AppleOnboardAudio::isTargetForMessage ( UInt32 actionSelector, AppleOnboardAudio * theAOA ) {
	OSArray *	swInterruptsArray;
	OSString *	theInterruptString;
	UInt32		interruptCount;
	bool		result = FALSE;
	
	debugIOLog (5, "+ AppleOnboardAudio[%ld]::isTargetForMessage ( 0x%0.8X, %p )", mInstanceIndex, actionSelector, theAOA);

	swInterruptsArray = OSDynamicCast ( OSArray, getLayoutEntry ( kSWInterrupts, theAOA ) );
	FailIf (NULL == swInterruptsArray, Exit);
	interruptCount = swInterruptsArray->getCount ();
	for ( UInt32 index = 0; index < interruptCount; index++ ) {
		theInterruptString = OSDynamicCast ( OSString, swInterruptsArray->getObject ( index ) );
		if ( NULL != theInterruptString ) {
			switch ( actionSelector ) {
				case kClockLockStatus:
					if ( !strcmp ( kClockLockIntMessage, theInterruptString->getCStringNoCopy() ) ) {
						debugIOLog (5, "  Target message is: kClockLockStatus");
						result = TRUE;
					}
					break;
				case kClockUnLockStatus:
					if ( !strcmp ( kClockUnLockIntMessage, theInterruptString->getCStringNoCopy() ) ) {
						debugIOLog (5, "  Target message is: kClockUnLockStatus");
						result = TRUE;
					}
					break;
				case kDigitalInInsertStatus:
					if ( !strcmp ( kDigitalInInsertIntMessage, theInterruptString->getCStringNoCopy() ) ) {
						debugIOLog (5, "  Target message is: kDigitalInInsertStatus");
						result = TRUE;
					}
					break;
				case kDigitalInRemoveStatus:
					if ( !strcmp ( kDigitalInRemoveIntMessage, theInterruptString->getCStringNoCopy() ) ) {
						debugIOLog (5, "  Target message is: kDigitalInRemoveStatus");
						result = TRUE;
					}
					break;
				case kRemoteActive:										//  [3515371]
					if ( !strcmp ( kRemoteActiveMessage, theInterruptString->getCStringNoCopy() ) ) {
						debugIOLog (5, "  Target message is: kRemoteWake");
						result = TRUE;
					}
					break;
				case kRemoteIdle:										//  [3515371]
					if ( !strcmp ( kRemoteIdleMessage, theInterruptString->getCStringNoCopy() ) ) {
						debugIOLog (5, "  Target message is: kRemoteIdle");
						result = TRUE;
					}
					break;
				case kRemoteSleep:										//  [3515371]
					if ( !strcmp ( kRemoteSleepMessage, theInterruptString->getCStringNoCopy() ) ) {
						debugIOLog (5, "  Target message is: kRemoteSleep");
						result = TRUE;
					}
					break;
				default:
					debugIOLog (5, "useless actionSelector = %ld", actionSelector);
					break;
			}
		}
	}

Exit:
	debugIOLog (5, "- AppleOnboardAudio[%ld]::isTargetForMessage ( 0x%0.8X, %p ) result = %d", mInstanceIndex, actionSelector, theAOA, result);
	return result;
}
	
//	--------------------------------------------------------------------------------
// Must run on the workloop because we might modify audio controls.
void AppleOnboardAudio::protectedInterruptEventHandler (UInt32 statusSelector, UInt32 newValue) {
	AudioHardwareObjectInterface *		thePlugin;
	OSNumber * 							connectionCodeNumber;
	char * 								pluginString;
	UInt32 								selectorCode;

	switch ( statusSelector ) {
		case kInternalSpeakerStatus:		debugIOLog (7, "+ AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kInternalSpeakerStatus, %ld )", mInstanceIndex, newValue);			break;
		case kHeadphoneStatus:				debugIOLog (6, "+ AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kHeadphoneStatus, %ld )", mInstanceIndex, newValue);					break;
		case kExtSpeakersStatus:			debugIOLog (6, "+ AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kExtSpeakersStatus, %ld )", mInstanceIndex, newValue);				break;
		case kLineOutStatus:				debugIOLog (6, "+ AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kLineOutStatus, %ld )", mInstanceIndex, newValue);					break;
		case kDigitalInStatus:				debugIOLog (6, "+ AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kDigitalInStatus, %ld )", mInstanceIndex, newValue);					break;
		case kDigitalOutStatus:				debugIOLog (6, "+ AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kDigitalOutStatus, %ld )", mInstanceIndex, newValue);				break;
		case kLineInStatus:					debugIOLog (6, "+ AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kLineInStatus, %ld )", mInstanceIndex, newValue);					break;
		case kInputMicStatus:				debugIOLog (7, "+ AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kInputMicStatus, %ld )", mInstanceIndex, newValue);					break;
		case kExternalMicInStatus:			debugIOLog (7, "+ AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kExternalMicInStatus, %ld )", mInstanceIndex, newValue);				break;
		case kDigitalInInsertStatus:		debugIOLog (6, "+ AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kDigitalInInsertStatus, %ld )", mInstanceIndex, newValue);			break;
		case kDigitalInRemoveStatus:		debugIOLog (6, "+ AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kDigitalInRemoveStatus, %ld )", mInstanceIndex, newValue);			break;
		case kRequestCodecRecoveryStatus:	debugIOLog (7, "+ AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kRequestCodecRecoveryStatus, %ld )", mInstanceIndex, newValue);		break;
		case kClockInterruptedRecovery:		debugIOLog (7, "+ AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kClockInterruptedRecovery, %ld )", mInstanceIndex, newValue);		break;
		case kClockUnLockStatus:			debugIOLog (6, "+ AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kClockUnLockStatus, %ld )", mInstanceIndex, newValue);				break;
		case kClockLockStatus:				debugIOLog (6, "+ AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kClockLockStatus, %ld )", mInstanceIndex, newValue);					break;
		case kAES3StreamErrorStatus:		debugIOLog (7, "+ AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kAES3StreamErrorStatus, %ld )", mInstanceIndex, newValue);			break;
		case kCodecErrorInterruptStatus:	debugIOLog (6, "+ AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kCodecErrorInterruptStatus, %ld )", mInstanceIndex, newValue);		break;
		case kCodecInterruptStatus:			debugIOLog (5, "+ AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kCodecInterruptStatus, %ld )", mInstanceIndex, newValue);			break;
		case kBreakClockSelect:				debugIOLog (7, "+ AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kBreakClockSelect, %ld )", mInstanceIndex, newValue);				break;
		case kMakeClockSelect:				debugIOLog (7, "+ AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kMakeClockSelect, %ld )", mInstanceIndex, newValue);					break;
		case kSetSampleRate:				debugIOLog (7, "+ AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kSetSampleRate, %ld )", mInstanceIndex, newValue);					break;
		case kSetSampleBitDepth:			debugIOLog (7, "+ AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kSetSampleBitDepth, %ld )", mInstanceIndex, newValue);				break;
		case kPowerStateChange:				debugIOLog (7, "+ AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kPreDMAEngineInit, %ld )", mInstanceIndex, newValue);				break;
		case kPreDMAEngineInit:				debugIOLog (7, "+ AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kPreDMAEngineInit, %ld )", mInstanceIndex, newValue);				break;
		case kPostDMAEngineInit:			debugIOLog (7, "+ AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kPostDMAEngineInit, %ld )", mInstanceIndex, newValue);				break;
		case kRestartTransport:				debugIOLog (5, "+ AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kRestartTransport, %ld )", mInstanceIndex, newValue);				break;
		case kRemoteActive:					debugIOLog (5, "+ AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kRemoteActive, %ld )", mInstanceIndex, newValue);					break;		//  [3515371]
		case kRemoteSleep:					debugIOLog (5, "+ AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kRemoteSleep, %ld )", mInstanceIndex, newValue);						break;		//  [3515371]
		case kRemoteIdle:					debugIOLog (5, "+ AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kRemoteIdle, %ld )", mInstanceIndex, newValue);						break;		//  [3515371]
		default:							debugIOLog (7, "+ AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( 0x%0.8X, %ld )", mInstanceIndex, statusSelector, newValue);			break;		//  [3515371]
	}

	if ( ourPowerState == kIOAudioDeviceSleep ) {
		if ( ( kRemoteActive != statusSelector ) && ( kRemoteSleep != statusSelector ) && ( kRemoteIdle != statusSelector ) ) {
			goto Exit;		// don't care about interrupts while we're powered off mpc - EXCEPT FOR POWER MESSAGES! [3515371]
		}
	}

	selectorCode = getCharCodeForIntCode (statusSelector);

	switch (statusSelector) {
		case kHeadphoneStatus:
		case kLineOutStatus:
		case kExtSpeakersStatus:													//	[3398729]
		case kDigitalOutStatus:
			updateOutputDetectCollection (statusSelector, newValue);

			// This method parses the detect collection to see if this was an insert or extraction, and returns the appropriate outut selection
			selectorCode = getSelectorCodeForOutputEvent (statusSelector);

			// [3656784] prevent redirection to analog outputs on machines with digital out but no detect if format is encoded.
			if (mCurrentOutputSelection == kIOAudioOutputPortSubTypeSPDIF && mEncodedOutputFormat && selectorCode != kIOAudioOutputPortSubTypeSPDIF)
				break;
				
			//	[3588678]	Prevent redundant redirection
			if ( mCurrentOutputSelection != selectorCode ) {
				connectionCodeNumber = OSNumber::withNumber (selectorCode, 32);
				debugIOLog ( 5, "  selectorCode = '%4s', connectionCodeNumber = %ld", &selectorCode, connectionCodeNumber );
				
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

				selectCodecOutputWithMuteState( mIsMute );
				selectOutputAmplifiers (selectorCode, mIsMute);
				
				//	[3513367]	begin	{
/*				if ( kGPIO_Unknown != mPlatformInterface->getDigitalOutConnected() ) {
					//	Unmute the digital output if a digital out detect exists and a digital out
					//	plug is inserted into the digital out jack.  Mute the digital out if a digital
					//	out detect exists and there is no digital out plug inserted into the digital
					//	out jack.
					if ( kGPIO_Connected == mPlatformInterface->getDigitalOutConnected() ) {
						callPluginsInOrder ( kSetDigitalMuteState, mIsMute );
					} else {
						callPluginsInOrder ( kSetDigitalMuteState, TRUE );
					}
				} else if ( kGPIO_Unknown != mPlatformInterface->getComboOutJackTypeConnected() ) {
					//	Unmute the digital output if a combo out detect exists and a digital out
					//	plug is inserted into the combo out jack.  Mute the digital out if a combo
					//	out detect exists and there is no digital out plug inserted into the combo
					//	out jack.
					if ( kGPIO_TypeIsDigital == mPlatformInterface->getComboOutJackTypeConnected() ) {
						callPluginsInOrder ( kSetDigitalMuteState, mIsMute );
					} else {
						callPluginsInOrder ( kSetDigitalMuteState, TRUE );
					}
				} else {
					callPluginsInOrder ( kSetDigitalMuteState, mIsMute );
				}
				//	}	end	[3513367]
*/
				
				if (NULL == mOutputSelector) debugIOLog (3, "\n!!!mOutputSelector = NULL!!!");
				FailIf (NULL == mOutputSelector, Exit);
				mOutputSelector->hardwareValueChanged (connectionCodeNumber);
				setSoftwareOutputDSP (pluginString);
	
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
				headphoneState->release ();
			}
			break;
		case kLineInStatus:
			updateOutputDetectCollection (statusSelector, newValue);

			// [3250612] don't do anything on insert events!
			pluginString = getConnectionKeyFromCharCode (selectorCode, kIOAudioStreamDirectionInput);
			thePlugin = getPluginObjectForConnection (pluginString);
			FailIf (NULL == thePlugin, Exit);

			FailIf (NULL == mInputSelector, Exit);
			selectorCode = mInputSelector->getIntValue ();

			if ((mDetectCollection & kSndHWLineInput) && (kIOAudioInputPortSubTypeLine == selectorCode)) {
				thePlugin->setInputMute (FALSE);
			} else if (!(mDetectCollection & kSndHWLineInput) && (kIOAudioInputPortSubTypeLine == selectorCode)) {
				thePlugin->setInputMute (TRUE);
			}
			break;
		case kExternalMicInStatus:
			updateOutputDetectCollection (statusSelector, newValue);

			break;
		case kDigitalInInsertStatus:
		case kDigitalInRemoveStatus:
			updateOutputDetectCollection (statusSelector, newValue);
			mDigitalInsertStatus = statusSelector;
			callPluginsInOrder ( kDigitalInStatus, kDigitalInInsertStatus == mDigitalInsertStatus ? kGPIO_Connected : kGPIO_Disconnected );
			break;
		case kRequestCodecRecoveryStatus:
			//	Walk through the available plugin objects and invoke the recoverFromFatalError on each object in the correct order.
			callPluginsInOrder (kRequestCodecRecoveryStatus, newValue);
			break;
		case kRestartTransport:
			//	This message is used to restart the transport hardware without invoking a general recovery or
			//	a recovery from clock interruption and is used to perform sequence sensitive initialization.
			if (NULL != mTransportInterface) {
				mTransportInterface->restartTransport ();   //  [3683602]   Dedicated restart method leaves transport hw registers unmodified
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
			//  Only respond to a change in clock lock status (some codecs broadcast redundant messages)
			//  by handling the interrupt locally if a clock selector control exists or by handling the
			//  interrupt remotely if no clock selector exists (assumes the clock selector exists in a
			//  remote AppleOnboardAudio instance).  22 Mar 2004 rbm
			if ( mCodecLockStatus != statusSelector ) {  //  [3628559]
				if ( kClockLockStatus == statusSelector ) {
					debugIOLog ( 5, "  ===== AppleOnboardAudio[%ld] CLOCK STATUS changed to kClockLockStatus, mClockSelectInProcessSemaphore = %d", mInstanceIndex, mClockSelectInProcessSemaphore );
					if ( mCodecLockStatus != statusSelector ) {																				//  [3684994]
						mTransportSampleRate.whole = mTransportInterface->transportGetSampleRate ();										//  [3684994]
						debugIOLog ( 4, "  *** about to mDriverDMAEngine->hardwareSampleRateChanged ( %d )", mTransportSampleRate.whole );	//  [3684994]
						mDriverDMAEngine->hardwareSampleRateChanged ( &mTransportSampleRate );												//  [3684994]
					}																														//  [3684994]
				} else if ( kClockUnLockStatus == statusSelector ) {
					debugIOLog ( 5, "  ===== AppleOnboardAudio[%ld] CLOCK STATUS changed to kClockUnLockStatus, mClockSelectInProcessSemaphore = %d", mInstanceIndex, mClockSelectInProcessSemaphore );
				}
				UInt32 transportType = mTransportInterface->transportGetTransportInterfaceType();
				if ( kTransportInterfaceType_I2S_Slave_Only != transportType ) {
					mCodecLockStatus = statusSelector;
					if ( NULL != mExternalClockSelector ) {
						//	Codec clock loss errors are to be ignored if in the process of switching clock sources.
						if ( !mClockSelectInProcessSemaphore ) {
							//	An 'kClockUnLockStatus' status selector requires that the clock source
							//	be redirected back to an internal source (i.e. the internal hardware is to act as a MASTER).
							if ( kClockUnLockStatus == statusSelector ) {
								//	Set the hardware to MASTER mode.
								clockSelectorChanged ( kClockSourceSelectionInternal );
								//	Flush the control value (i.e. MASTER = internal).
								OSNumber * clockSourceSelector = OSNumber::withNumber (kClockSourceSelectionInternal, 32);
								
								mExternalClockSelector->hardwareValueChanged (clockSourceSelector);
								clockSourceSelector->release ();
							} else {
								//	[3435307]	Dont touch amplifier mutes related to UI.	RBM
								//	[3253678]	successful lock detected, now safe to unmute analog part
								callPluginsInOrder ( kSetMuteState, mIsMute );
							}
						} else {
							if ( kClockLockStatus == statusSelector ) {
								debugIOLog (3,  "  Attempted to post kClockLockStatus blocked by 'mClockSelectInProcessSemaphore' semaphore" );
							} else if ( kClockUnLockStatus == statusSelector ) {
								debugIOLog (3,  "  Attempted to post kClockUnLockStatus blocked by 'mClockSelectInProcessSemaphore' semaphore" );
							}
						}
					}
				}
			} else {
				if ( kClockLockStatus == statusSelector ) {
					debugIOLog ( 5, "  ===== AppleOnboardAudio[%ld] CLOCK STATUS redundant post of kClockLockStatus, mClockSelectInProcessSemaphore = %d", mInstanceIndex, mClockSelectInProcessSemaphore );
				} else if ( kClockUnLockStatus == statusSelector ) {
					debugIOLog ( 5, "  ===== AppleOnboardAudio[%ld] CLOCK STATUS redundant post of kClockUnLockStatus, mClockSelectInProcessSemaphore = %d", mInstanceIndex, mClockSelectInProcessSemaphore );
				}
			}
			broadcastSoftwareInterruptMessage ( statusSelector );
			if ( mNeedsLockStatusUpdateToUnmute ) {					//  [3678605]
				debugIOLog ( 4, "  *** [%ld] about to unmute from mNeedsLockStatusUpdateToUnmute", mInstanceIndex );
				mNeedsLockStatusUpdateToUnmute = FALSE;
				selectCodecOutputWithMuteState (mIsMute);

				if ( NULL != mOutputSelector ) {
					selectOutputAmplifiers (mOutputSelector->getIntValue (), mIsMute);	//	Radar 3416318:	mOutMuteControl does not touch GPIOs so do so here!
				}
				if (NULL != mOutMuteControl) {
					mOutMuteControl->flushValue ();					// Restore hardware to the user's selected state
				}
			}
			break;
		case kAES3StreamErrorStatus:
			//	indicates that the V bit states data is invalid or may be compressed data
			debugIOLog (7,  "  ... kAES3StreamErrorStatus %d", (unsigned int)newValue );
			if ( newValue ) {
				//	As appropriate (TBD)...
			}
			break;
		case kRemoteActive:																	//  [3515371]
			performPowerStateChangeAction ( (void*)kIOAudioDeviceActive );					//  [3515371]
			break;
		case kRemoteIdle:																	//  [3515371]
			performPowerStateChangeAction ( (void*)kIOAudioDeviceIdle );					//  [3515371]
			break;
		case kRemoteSleep:																	//  [3515371]
			performPowerStateChangeAction ( (void*)kIOAudioDeviceSleep );					//  [3515371]
			break;
		default:
			break;
	}

Exit:
	switch ( statusSelector ) {
		case kInternalSpeakerStatus:		debugIOLog (7, "- AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kInternalSpeakerStatus, %ld )", mInstanceIndex, newValue);			break;
		case kHeadphoneStatus:				debugIOLog (6, "- AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kHeadphoneStatus, %ld )", mInstanceIndex, newValue);					break;
		case kExtSpeakersStatus:			debugIOLog (6, "- AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kExtSpeakersStatus, %ld )", mInstanceIndex, newValue);				break;
		case kLineOutStatus:				debugIOLog (6, "- AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kLineOutStatus, %ld )", mInstanceIndex, newValue);					break;
		case kDigitalInStatus:				debugIOLog (6, "- AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kDigitalInStatus, %ld )", mInstanceIndex, newValue);					break;
		case kDigitalOutStatus:				debugIOLog (6, "- AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kDigitalOutStatus, %ld )", mInstanceIndex, newValue);				break;
		case kLineInStatus:					debugIOLog (6, "- AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kLineInStatus, %ld )", mInstanceIndex, newValue);					break;
		case kInputMicStatus:				debugIOLog (7, "- AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kInputMicStatus, %ld )", mInstanceIndex, newValue);					break;
		case kExternalMicInStatus:			debugIOLog (7, "- AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kExternalMicInStatus, %ld )", mInstanceIndex, newValue);				break;
		case kDigitalInInsertStatus:		debugIOLog (6, "- AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kDigitalInInsertStatus, %ld )", mInstanceIndex, newValue);			break;
		case kDigitalInRemoveStatus:		debugIOLog (6, "- AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kDigitalInRemoveStatus, %ld )", mInstanceIndex, newValue);			break;
		case kRequestCodecRecoveryStatus:	debugIOLog (7, "- AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kRequestCodecRecoveryStatus, %ld )", mInstanceIndex, newValue);		break;
		case kClockInterruptedRecovery:		debugIOLog (7, "- AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kClockInterruptedRecovery, %ld )", mInstanceIndex, newValue);		break;
		case kClockUnLockStatus:			debugIOLog (6, "- AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kClockUnLockStatus, %ld )", mInstanceIndex, newValue);				break;
		case kClockLockStatus:				debugIOLog (6, "- AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kClockLockStatus, %ld )", mInstanceIndex, newValue);					break;
		case kAES3StreamErrorStatus:		debugIOLog (7, "- AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kAES3StreamErrorStatus, %ld )", mInstanceIndex, newValue);			break;
		case kCodecErrorInterruptStatus:	debugIOLog (6, "- AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kCodecErrorInterruptStatus, %ld )", mInstanceIndex, newValue);		break;
		case kCodecInterruptStatus:			debugIOLog (5, "- AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kCodecInterruptStatus, %ld )", mInstanceIndex, newValue);			break;
		case kBreakClockSelect:				debugIOLog (7, "- AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kBreakClockSelect, %ld )", mInstanceIndex, newValue);				break;
		case kMakeClockSelect:				debugIOLog (7, "- AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kMakeClockSelect, %ld )", mInstanceIndex, newValue);					break;
		case kSetSampleRate:				debugIOLog (7, "- AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kSetSampleRate, %ld )", mInstanceIndex, newValue);					break;
		case kSetSampleBitDepth:			debugIOLog (7, "- AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kSetSampleBitDepth, %ld )", mInstanceIndex, newValue);				break;
		case kPowerStateChange:				debugIOLog (7, "- AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kPreDMAEngineInit, %ld )", mInstanceIndex, newValue);				break;
		case kPreDMAEngineInit:				debugIOLog (7, "- AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kPreDMAEngineInit, %ld )", mInstanceIndex, newValue);				break;
		case kPostDMAEngineInit:			debugIOLog (7, "- AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kPostDMAEngineInit, %ld )", mInstanceIndex, newValue);				break;
		case kRestartTransport:				debugIOLog (5, "- AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kRestartTransport, %ld )", mInstanceIndex, newValue);				break;
		case kRemoteActive:					debugIOLog (5, "- AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kRemoteActive, %ld )", mInstanceIndex, newValue);					break;		//  [3515371]
		case kRemoteSleep:					debugIOLog (5, "- AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kRemoteSleep, %ld )", mInstanceIndex, newValue);						break;		//  [3515371]
		case kRemoteIdle:					debugIOLog (5, "- AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kRemoteIdle, %ld )", mInstanceIndex, newValue);						break;		//  [3515371]
		default:							debugIOLog (7, "- AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( 0x%0.8X, %ld )", mInstanceIndex, statusSelector, newValue);			break;		//  [3515371]
	}
	return;
}


//	--------------------------------------------------------------------------------
bool AppleOnboardAudio::broadcastSoftwareInterruptMessage ( UInt32 actionSelector ) {
	AppleOnboardAudio *		theAOA;
	OSObject *				theObject;
	UInt32					numInstances;
	bool					result = FALSE;

	switch ( actionSelector ) {
		case kDigitalInInsertStatus:	debugIOLog ( 5, "+ AppleOnboardAudio[%ld]::broadcastSoftwareInterruptMessage ( 0x%0.8X kDigitalInInsertStatus )", mInstanceIndex, actionSelector );		break;
		case kDigitalInRemoveStatus:	debugIOLog ( 5, "+ AppleOnboardAudio[%ld]::broadcastSoftwareInterruptMessage ( 0x%0.8X kDigitalInRemoveStatus )", mInstanceIndex, actionSelector );		break;
		case kClockLockStatus:			debugIOLog ( 5, "+ AppleOnboardAudio[%ld]::broadcastSoftwareInterruptMessage ( 0x%0.8X kClockLockStatus )", mInstanceIndex, actionSelector );			break;
		case kClockUnLockStatus:		debugIOLog ( 5, "+ AppleOnboardAudio[%ld]::broadcastSoftwareInterruptMessage ( 0x%0.8X kClockUnLockStatus )", mInstanceIndex, actionSelector );			break;
		case kRemoteActive:				debugIOLog ( 5, "+ AppleOnboardAudio[%ld]::broadcastSoftwareInterruptMessage ( 0x%0.8X kRemoteActive )", mInstanceIndex, actionSelector );				break;
		case kRemoteIdle:				debugIOLog ( 5, "+ AppleOnboardAudio[%ld]::broadcastSoftwareInterruptMessage ( 0x%0.8X kRemoteIdle )", mInstanceIndex, actionSelector );				break;
		case kRemoteSleep:				debugIOLog ( 5, "+ AppleOnboardAudio[%ld]::broadcastSoftwareInterruptMessage ( 0x%0.8X kRemoteSleep )", mInstanceIndex, actionSelector );				break;
		default:						debugIOLog ( 5, "+ AppleOnboardAudio[%ld]::broadcastSoftwareInterruptMessage ( 0x%0.8X UNKNOWN )", mInstanceIndex, actionSelector );					break;
	}

	if ( NULL != mAOAInstanceArray ) {
		numInstances = mAOAInstanceArray->getCount();
		for ( UInt32 index = 0; index < numInstances; index++ ) {
			theObject =  mAOAInstanceArray->getObject ( index );
			if ( NULL != theObject ) {
				theAOA = OSDynamicCast ( AppleOnboardAudio, theObject );
				debugIOLog ( 5, "  numInstances = %d, mAOAInstanceArray->getObject ( %d ) returns %p", numInstances, index, theAOA );
				if ( NULL != theAOA ) {
					if ( isTargetForMessage ( actionSelector, theAOA ) ) {
						switch ( actionSelector ) {
							case kRemoteActive:
							case kRemoteIdle:
							case kRemoteSleep:
								theAOA->interruptEventHandler ( actionSelector, 0 );
								result = TRUE;
								break;
							default:
								debugIOLog ( 5, "  theAOA %p is target for 0x%0.8X", theAOA, actionSelector );
								if (theAOA->mSoftwareInterruptHandler) {
									theAOA->mInterruptProduced[actionSelector]++;
									theAOA->mSoftwareInterruptHandler->interruptOccurred (NULL, NULL, 0);
									result = TRUE;
								}
								break;
						}
					}
				}
			}
		}
	}
	debugIOLog ( 5, "- AppleOnboardAudio[%ld]::broadcastSoftwareInterruptMessage ( 0x%0.8X ) returns %ld", mInstanceIndex, actionSelector, result );
	return result;
}


#pragma mark +IOAUDIO INIT
//	--------------------------------------------------------------------------------
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
#if 0
	bool *								dmaCanStallPtr;
#endif
	OSNumber *							transportIndexPtr;				//  [3648867]
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
	OSNumber *							theInputsBitmapNumber;
	OSNumber *							theOutputsBitmapNumber;
	OSNumber *							usesAOAPowerManagement;			//  [3515371]
	OSBoolean *							uiMutesAmpsBoolean;
	OSBoolean *							muteAmpWhenClockInterrupted;
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
	selectorCode = (UInt32)((SInt32)-1);

    debugIOLog (3, "+ AppleOnboardAudio[%ld]::protectedInitHardware", mInstanceIndex);
		
	debugIOLog (3, "provider's name is: %s", provider->getName ());
	
	tmpData = OSDynamicCast (OSData, provider->getProperty (kLayoutID));
	debugIOLog (3, "  provider layoutID data = %p", tmpData);
	
	workLoop = getWorkLoop();
	FailIf (NULL == workLoop, Exit);
		
	mSoftwareInterruptHandler = IOInterruptEventSource::interruptEventSource (this, softwareInterruptHandler);
	FailIf (NULL == mSoftwareInterruptHandler, Exit);
	workLoop->addEventSource (mSoftwareInterruptHandler);
	mSoftwareInterruptHandler->enable ();
	
	FailIf (NULL == tmpData, Exit);
	layoutID = (UInt32 *)tmpData->getBytesNoCopy ();
	debugIOLog (3, "  layoutID pointer = %p", layoutID);
	FailIf (NULL == layoutID, Exit)
	mLayoutID = *layoutID;
	mUCState.ucLayoutID = mLayoutID;
	
	mDigitalInsertStatus = kGPIO_Connected;
	
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
	debugIOLog (6,  "  AppleOnboardAudio[%ld]::mLayoutID 0x%lX (from provider node)", mInstanceIndex, mLayoutID);
	
	while (layoutsCount--) {
		layoutEntry = OSDynamicCast (OSDictionary, layouts->getObject (index));
		FailIf (NULL == layoutEntry, Exit);
		layoutIDNumber = OSDynamicCast (OSNumber, layoutEntry->getObject (kLayoutIDInfoPlist));
		FailIf (NULL == layoutIDNumber, Exit);
		layoutIDInt = layoutIDNumber->unsigned32BitValue ();
#ifdef REMOVE_LAYOUTS
		if (layoutIDInt != mLayoutID) {
			debugIOLog (6,  "  found layout id 0x%lX and deleted it", layoutIDInt);
			layouts->removeObject (index);			// Remove wrong entries from the IORegistry to save space
		} else {
			debugIOLog (6,  "  found matchine layout id 0x%lX @ index %ld", layoutIDInt, index);
			mMatchingIndex = index;
			index++;
		}
#else
		if (layoutIDInt == mLayoutID) {
			debugIOLog (6,  "  AppleOnboardAudio[%ld] found machine layout id 0x%lX @ index %ld", mInstanceIndex, layoutIDInt, index);
			mMatchingIndex = index;
			break;
		} else {
			index++;
		}
#endif
	}
	debugIOLog (6,  "  AppleOnboardAudio[%ld]::layoutIDInt 0x%lX", mInstanceIndex, layoutIDInt );

#ifdef REMOVE_LAYOUTS
	layoutEntry = OSDynamicCast (OSDictionary, layouts->getObject (0));
#else
	FailIf (0xFFFFFFFF == mMatchingIndex, Exit);	
	layoutEntry = OSDynamicCast (OSDictionary, layouts->getObject (mMatchingIndex));
#endif

	debugIOLog (6, "  layoutEntry = %p", layoutEntry);
	FailIf (NULL == layoutEntry, Exit);

	ampRecoveryNumber = OSDynamicCast ( OSNumber, layoutEntry->getObject (kAmpRecoveryTime) );
	FailIf (NULL == ampRecoveryNumber, Exit);
	mAmpRecoveryMuteDuration = ampRecoveryNumber->unsigned32BitValue();
	debugIOLog (6, "  AppleOnboardAudio[%ld]::protectedInitHardware - mAmpRecoveryMuteDuration = %ld", mInstanceIndex, mAmpRecoveryMuteDuration);

	// Find out what the correct platform object is and request the platform factory to build it for us.
	platformObjectString = OSDynamicCast ( OSString, layoutEntry->getObject ( kPlatformObject ) );
	FailIf (NULL == platformObjectString, Exit);
	debugIOLog (6, "  AppleOnboardAudio[%ld]::protectedInitHardware - platformObjectString = %s", mInstanceIndex, platformObjectString->getCStringNoCopy());

	mPlatformInterface = PlatformFactory::createPlatform (platformObjectString);
	FailIf (NULL == mPlatformInterface, Exit);
	debugIOLog (6, "  AppleOnboardAudio[%ld]::protectedInitHardware - mPlatformInterface = %p", mInstanceIndex, mPlatformInterface);
	FailIf (!mPlatformInterface->init(provider, this, AppleDBDMAAudio::kDBDMADeviceIndex), Exit);
	
	//  [3648867]   The driver <XML> dictionary will implement a <key>TransportIndex</key> <integer></integer> value pair 
	//				where values represent:
	//							0 = 'i2s-a'			1 = 'i2s-b'				2 = 'i2s-c'				3 = 'i2s-d'
	//							4 = 'i2s-e'			5 = 'i2s-f'				6 = 'i2s-g'				7 = 'i2s-h'
	//
	//  13 May 2004 rbm
	transportIndexPtr = OSDynamicCast ( OSNumber, layoutEntry->getObject ( kTransportIndex ) );
	if ( NULL != transportIndexPtr ) {
		debugIOLog (6, "  transportIndexPtr = %p", transportIndexPtr);
#if kBUILD_FOR_Q78_DEADBUG_CS8406
#warning	DO NOT USE THIS HALF OF THE CONDITIONAL COMPILE FOR MANUFACTURED BOARDS!!!!!  ONLY FOR DAN FREEMAN ON HAND BUILT PROTOTYPE!!!
		//  PLEASE SEE COMMENT IN 'DIAGNOSIS' SECTION OF RADAR 3652548 DATED 5/12/04 3:46 PM
		//  This section of conditional compiled code is NOT to be used in the final release.
		//  This section inverts the address relationships as a favor to Dan Feeman for hand
		//  built prototype as it was not possible to move the address of the CS8416 attached
		//  to 'i2s-c' which is already on the board.  The disabled code above in the other
		//  half of the conditional compile is to be used for manufactured boards!!!
		//  13 May 2004 rbm
		switch ( transportIndexPtr->unsigned32BitValue () ) {
			case 0:		mTransportInterfaceIndex = 2;													break;
			case 2:		mTransportInterfaceIndex = 0;													break;
			default:	mTransportInterfaceIndex = transportIndexPtr->unsigned32BitValue ();			break;
		}
#else
		mTransportInterfaceIndex = transportIndexPtr->unsigned32BitValue ();
#endif
	} else {
		debugIOLog ( 3, "  <key>TransportIndex</key> NOT FOUND!!!" );
	}
	debugIOLog (6, "  AppleOnboardAudio[%ld]::mTransportInterfaceIndex = %d", mInstanceIndex, mTransportInterfaceIndex);

	mUIMutesAmps = FALSE;
	uiMutesAmpsBoolean = OSDynamicCast ( OSBoolean, layoutEntry->getObject ( kUIMutesAmps ) );
	if ( NULL != uiMutesAmpsBoolean ) {
		mUIMutesAmps = uiMutesAmpsBoolean->getValue ();
	}	
	
	mMuteAmpWhenClockInterrupted = FALSE;   
	muteAmpWhenClockInterrupted = OSDynamicCast ( OSBoolean, layoutEntry->getObject ( kMuteAmpWhenClockInterrupted ) );
	if ( NULL != muteAmpWhenClockInterrupted ) {
		mMuteAmpWhenClockInterrupted = muteAmpWhenClockInterrupted->getValue ();
	}	
	
	//	[3515371]	begin	{
	//	There may be another instance of AppleOnboardAudio which is the next higher priority in servicing
	//	power management sleep requests.  Any such instance is discovered here and the 'layoutID' of that
	//	instance is saved.  When the first power managment request comes in, an AppleOnboardAudio instance
	//	of higher power management priority will be located and the power managment prioritization relationship
	//	will be established at that time (all AOA instances should have completed loading by then).
	usesAOAPowerManagement = OSDynamicCast ( OSNumber, layoutEntry->getObject ( kUsesAOAPowerManagement ) );			//  [3515371]
	if ( NULL != usesAOAPowerManagement ) {
		mUsesAOAPowerManagement = usesAOAPowerManagement->unsigned32BitValue();
	}

	//	}	end		[3515371]
	
	//	[3453799]	begin {
	comboInAssociationString = OSDynamicCast ( OSString, layoutEntry->getObject ( kComboInObject ) );
	if ( NULL != comboInAssociationString ) {
		debugIOLog (6, " comboInAssociationString = %s", comboInAssociationString->getCStringNoCopy ());
		if ( comboInAssociationString->isEqualTo ("LineInDetect") ) {
			mPlatformInterface->setAssociateComboInTo ( kGPIO_Selector_LineInDetect );
		} else if ( comboInAssociationString->isEqualTo ("ExternalMicDetect") ) {
			mPlatformInterface->setAssociateComboInTo ( kGPIO_Selector_ExternalMicDetect );
		}
	}
	
	comboOutAssociationString = OSDynamicCast ( OSString, layoutEntry->getObject ( kComboOutObject ) );
	if ( NULL != comboOutAssociationString ) {
		debugIOLog (6, " comboOutAssociationString = %s", comboOutAssociationString->getCStringNoCopy ());
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

	if (mMuteAmpWhenClockInterrupted) {
		muteAllAmps();
	}
	
	mPlatformInterface->setCodecReset ( kCODEC_RESET_Analog, kGPIO_Reset );

	// Find out what the correct transport object is and request the transport factory to build it for us.
	transportObjectString = OSDynamicCast ( OSString, layoutEntry->getObject ( kTransportObject ) );
	FailIf ( NULL == transportObjectString, Exit );
	debugIOLog (6, " AppleOnboardAudio[%ld]::protectedInitHardware - transportObjectString = %s", mInstanceIndex, transportObjectString->getCStringNoCopy());

	mTransportInterface = TransportFactory::createTransport ( transportObjectString );
	debugIOLog (6, " AppleOnboardAudio[%ld]::protectedInitHardware - mTransportInterface = %p", mInstanceIndex, mTransportInterface);
	FailIf (NULL == mTransportInterface, Exit);
	FailIf (!mTransportInterface->init ( mPlatformInterface ), Exit);

	// If we have the entry we were looking for then get the list of plugins that need to be loaded
	hardwareObjectsList = OSDynamicCast (OSArray, layoutEntry->getObject (kHardwareObjects));
	if ( NULL == hardwareObjectsList ) { debugIOLog (3,  "  NULL == hardwareObjectsList" ); }
	FailIf (NULL == hardwareObjectsList, Exit);

	// Set the IORegistry entries that will cause the plugins to load
	numPlugins = hardwareObjectsList->getCount ();
	debugIOLog (5,  "  AppleOnboardAudio[%ld] numPlugins to load = %ld", mInstanceIndex, numPlugins);

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

//#if DEBUGLOG
//	loadTimeNumber = OSNumber::withNumber ((unsigned long long)timeWaited * 10, 32);
//	setProperty ("Plugin load time (ms)", loadTimeNumber);
//	loadTimeNumber->release ();
//#endif
	volumeNumber = OSNumber::withNumber((long long unsigned int)0, 32);

    if (!super::initHardware (provider)) {
        goto Exit;
    }

	debugIOLog (3, "  A: about to set work loop");
	
//	workLoop = getWorkLoop();
//	FailIf (NULL == workLoop, Exit);

	// must occur in this order, and must be called in initHardware or later to have a valid workloop
	mPlatformInterface->setWorkLoop (workLoop);
	
	FailIf (NULL == mPluginObjects, Exit);

	count = mPluginObjects->getCount ();
	FailIf (0 == count, Exit);
	
	debugIOLog (3, "  AppleOnboardAudio[%ld] about to init %ld plugins", mInstanceIndex, count);

	for (index = 0; index < count; index++) {
		thePluginObject = getIndexedPluginObject (index);

		FailIf (NULL == thePluginObject, Exit);

		thePluginObject->setWorkLoop (workLoop);

		thePluginObject->initPlugin ( mPlatformInterface );

		// XXX FIX - this is a temporary init
		mCurrentOutputPlugin = thePluginObject;
		mCurrentInputPlugin = thePluginObject;
	}

	volumeNumber->release ();

	// FIX - check the result of this call and remove plugin if it fails!
	callPluginsInOrder (kPreDMAEngineInit, 0);

#if LOCALIZABLE
	sprintf ( deviceName, "%s", "DeviceName");
	if (mInstanceIndex > 1) {
		sprintf (num, "%d", mInstanceIndex);
		strcat (deviceName, num);
	}
	setDeviceName (deviceName);
	sprintf ( deviceName, "%s", "DeviceShortName");
	if (mInstanceIndex > 1) {
		sprintf (num, "%d", mInstanceIndex);
		strcat (deviceName, num);
	}
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
	debugIOLog (3, "  AppleOnboardAudio[%ld] finished configure DMA engine (%p) ", mInstanceIndex, mDriverDMAEngine);
	FailIf (NULL == mDriverDMAEngine, Exit);
	
	theOutputsBitmapNumber = OSDynamicCast (OSNumber, layoutEntry->getObject (kOutputsBitmap));
	if (theOutputsBitmapNumber) {
		mDriverDMAEngine->setProperty ("OutputsBitmap", theOutputsBitmapNumber);
	}
	theInputsBitmapNumber = OSDynamicCast (OSNumber, layoutEntry->getObject (kInputsBitmap));
	if (theInputsBitmapNumber) {
		mDriverDMAEngine->setProperty ("InputsBitmap", theInputsBitmapNumber);
	}

	// Have to create the audio controls before calling activateAudioEngine
	mAutoUpdatePRAM = FALSE;			// Don't update the PRAM value while we're initing from it
    result = createDefaultControls ();
	FailIf (kIOReturnSuccess != result, Exit);

	debugIOLog (3, "  AppleOnboardAudio[%ld]::initHardware - mDriverDMAEngine retain count before activateAudioEngine = %d", mInstanceIndex, mDriverDMAEngine->getRetainCount());
    if (kIOReturnSuccess != activateAudioEngine (mDriverDMAEngine)) {
		mDriverDMAEngine->release ();
		mDriverDMAEngine = NULL;
        goto Exit;
    }
	debugIOLog (3, "  AppleOnboardAudio[%ld]::initHardware - mDriverDMAEngine retain count after activateAudioEngine = %d", mInstanceIndex, mDriverDMAEngine->getRetainCount());

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

	if ( kGPIO_IsAlternate == mPlatformInterface->getInternalSpeakerID() ) {
		mInternalSpeakerID = 1;
	} else {
		mInternalSpeakerID = 0;
	}
	debugIOLog (6, "  Internal speaker ID is %ld", mInternalSpeakerID);

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
		headphoneState->release ();
	}

	//	[3513367, 3544877]	rbm		begin {	
	//	The digital status is only valid if the jack is connected.  If the jack
	//	is disconnected then an active 'digital' status should be ignored and the
	//	digital output should be muted.
	//
	//	ACTION TABLES - DIGITAL & ANALOG MUTE:
	//
	//		COMBO OUT TYPE		COMBO OUT DETECT		MUTE ACTION
	//		
	//			DIGITAL				REMOVED				MUTE DIGITAL, UNMUTE ANALOG
	//			DIGITAL				INSERTED			MUTE ANALOG, UNMUTE DIGITAL
	//			ANALOG				REMOVED				MUTE DIGITAL, UNMUTE ANALOG
	//			ANALOG				INSERTED			MUTE DIGITAL, UNMUTE ANALOG
	//
	{
		UInt32 comboOutJackTypeState = mPlatformInterface->getComboOutJackTypeConnected ();
		if ( kGPIO_Unknown != comboOutJackTypeState ) {
			GpioAttributes theAnalogState = kGPIO_Unknown;
			switch ( (UInt32)mPlatformInterface->getComboOutAssociation () ) {
				case kGPIO_Selector_HeadphoneDetect:	theAnalogState = mPlatformInterface->getHeadphoneConnected ();		break;
				case kGPIO_Selector_LineOutDetect:		theAnalogState = mPlatformInterface->getLineOutConnected ();		break;
				case kGPIO_Selector_SpeakerDetect:		theAnalogState = mPlatformInterface->getSpeakerConnected ();		break;
			}
			if ( kGPIO_Connected == theAnalogState && kGPIO_TypeIsDigital == comboOutJackTypeState ) {
				callPluginsInOrder ( kSetDigitalMuteState, FALSE );
			} else {
				callPluginsInOrder ( kSetDigitalMuteState, TRUE );
			}
		} else {
			callPluginsInOrder ( kSetDigitalMuteState, FALSE );
		}
	}
	//	} end	[3513367, 3544877]

	if ( NULL != mOutputSelector ) {
		mOutputSelector->hardwareValueChanged (connectionCodeNumber);
	}
	
	connectionCodeNumber->release();
		
	if ( NULL != mOutputSelector ) {
		selectorCode = mOutputSelector->getIntValue ();
		debugIOLog (6, "  mOutputSelector->getIntValue () returns %lX", selectorCode);
		if (0 != selectorCode) {
			connectionString = getConnectionKeyFromCharCode (selectorCode, kIOAudioStreamDirectionOutput);
			debugIOLog (6, "  mOutputSelector->getIntValue () char code is %s", connectionString);
			if (NULL != connectionString) {
				AudioHardwareObjectInterface* theHWObject;
				theHWObject = getPluginObjectForConnection (connectionString);
				if (NULL != theHWObject) {
					mCurrentOutputPlugin = theHWObject;
				} else {
					debugIOLog (1, "  *** Can't find hardware plugin for output %s", connectionString);
				}
			}
		}
		debugIOLog (6, "  AppleOnboardAudio[%ld] mCurrentOutputPlugin = %p", mInstanceIndex, mCurrentOutputPlugin);
	}
	
	if ( NULL != mInputSelector ) {
		selectorCode = mInputSelector->getIntValue ();
		debugIOLog (6, " mInputSelector->getIntValue () returns %4s", (char *)&selectorCode);
		if (0 != selectorCode) {
			connectionString = getConnectionKeyFromCharCode (selectorCode, kIOAudioStreamDirectionInput);
			debugIOLog (6, " mInputSelector->getIntValue () char code is %s", connectionString);
			if (NULL != connectionString) {
				AudioHardwareObjectInterface* theHWObject;
				theHWObject = getPluginObjectForConnection (connectionString);
				if (NULL != theHWObject) {
					mCurrentInputPlugin = theHWObject;
				} else {
					debugIOLog (1, "  *** Can't find hardware plugin for input %s", connectionString);
				}
			}
		}
	}
	debugIOLog (6, "  AppleOnboardAudio[%ld] mCurrentInputPlugin = %p", mInstanceIndex, mCurrentInputPlugin);
	
	mCurrentInputPlugin->setActiveInput (selectorCode);

	AOAprop = OSDynamicCast (OSDictionary, mCurrentInputPlugin->getProperty (kPluginPListAOAAttributes));
	FailIf (NULL == AOAprop, Exit);

	softwareInputGainBoolean = OSDynamicCast (OSBoolean, AOAprop->getObject (kPluginPListSoftwareInputGain));
	if (NULL != softwareInputGainBoolean) {
		mDriverDMAEngine->setUseSoftwareInputGain (softwareInputGainBoolean->getValue ());
		mCurrentPluginNeedsSoftwareInputGain = softwareInputGainBoolean->getValue ();
	} else {
		mDriverDMAEngine->setUseSoftwareInputGain (false);
		mCurrentPluginNeedsSoftwareInputGain = false;
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
		debugIOLog (6, "  devicesToLoad = %ld", devicesToLoad);
		
		for (deviceIndex = 0; deviceIndex < devicesToLoad; deviceIndex++) {
			OSDictionary *			deviceDict;
			OSString *				i2sNodeString;
			OSString *				soundNodePathString;
			OSString *				matchPropertyString;
			IOService *				i2sService;
			IORegistryEntry * 		soundRegEntry;
			IOService * 			sound;
			
			deviceDict = OSDynamicCast (OSDictionary, multipleDevicesArray->getObject(deviceIndex));
			debugIOLog (6, "  deviceDict = %p", deviceDict);
			FailIf (NULL == deviceDict, Exit);

			i2sNodeString = OSDynamicCast (OSString, deviceDict->getObject(kI2SNode));
			debugIOLog (6, "  i2sNodeString = %p", i2sNodeString);
			FailIf (NULL == i2sNodeString, Exit);

			soundNodePathString = OSDynamicCast (OSString, deviceDict->getObject(kSoundNodePath));
			debugIOLog (6, "  soundNodePathString = %p", soundNodePathString);
			FailIf (NULL == soundNodePathString, Exit);

			matchPropertyString = OSDynamicCast (OSString, deviceDict->getObject(kMatchProperty));
			debugIOLog (6, "  matchPropertyString = %p", matchPropertyString);
			FailIf (NULL == matchPropertyString, Exit);
		
			i2sService = IOService::waitForService (IOService::nameMatching(i2sNodeString->getCStringNoCopy()), &timeout);
			debugIOLog (6, "  i2sService = %p", deviceDict);
			FailIf (NULL == i2sService, Exit);			
			soundRegEntry = i2sService->childFromPath (soundNodePathString->getCStringNoCopy(), gIOServicePlane);
			debugIOLog (6, "  soundRegEntry = %p", soundRegEntry);
			FailIf (NULL == i2sService, Exit);
			
			sound = OSDynamicCast (IOService, soundRegEntry);
			FailIf (NULL == i2sService, Exit);
			debugIOLog (6, "  soundRegEntry = %p", soundRegEntry);

			sound->setProperty (matchPropertyString->getCStringNoCopy(), "YES");
			sound->registerService ();
		}
	} else {
		debugIOLog (3,  "  NULL == multipleDevicesArray on instance %ld", mInstanceIndex );
	}	
	
	debugIOLog ( 3, "  [%ld] addNotification (gIOPublishNotification, serviceMatching (AppleOnboardAudio), (IOServiceNotificationHandler)&aoaPublished=%p, this=%p )", mInstanceIndex, &aoaPublished, this ); 
	aoaNotifier = addNotification (gIOPublishNotification, serviceMatching ("AppleOnboardAudio"), (IOServiceNotificationHandler)&aoaPublished, this);
#endif

	mSignal->signal (kIOReturnSuccess, FALSE);

	result = kIOReturnSuccess;
	mAutoUpdatePRAM = TRUE;
	
	//	[3514709]	Switch to an external clock if this AppleOnboardAudio instance uses a slave only I2S transport interface object
	//  [3655075]   Switch to external clock on opaque slave only transport interface
	{
		UInt32 transportType = mTransportInterface->transportGetTransportInterfaceType();
		if ( ( kTransportInterfaceType_I2S_Slave_Only == transportType ) && ( kTransportInterfaceType_I2S_Opaque_Slave_Only == transportType ) ) {  //  [3655075]
			debugIOLog ( 3, "  Selecting external clock for 'SLAVE ONLY' device" );
			//  Used to call the change handler for the clock select here but that method is dependent on the 'LOCK'
			//  status.  The codec may be unlocked but if the codec is a 'slave only' device then the selection of
			//  the external clock is mandatory.  The blocking of clock selection on an 'unlock' status within the
			//  clock selector change control handler makes that method inappropriate for this use.
			mTransportInterface->transportBreakClockSelect ( kTRANSPORT_SLAVE_CLOCK );
			callPluginsInOrder ( kBreakClockSelect, kTRANSPORT_SLAVE_CLOCK );
			
			mTransportInterface->transportMakeClockSelect ( kTRANSPORT_SLAVE_CLOCK );
			callPluginsInOrder ( kMakeClockSelect, kTRANSPORT_SLAVE_CLOCK );
		}
	}

	// Give drivers a chance to do something after the DMA engine and IOAudioFamily have been created/started
	// FIX - check the result of this call and remove plugin if it fails!
	callPluginsInOrder (kPostDMAEngineInit, 0);
	callPluginsInOrder (kEndFormatChange, 0);		// force codec out of reset and power on dacs

	debugIOLog (1, "about to sleep after postDMAEngineInit"); IOSleep (3000); 

	mCurrentOutputSelection = 0x3F3F3F3F;	// force update of all mutes, etc. when interrupt handlers choose correct output 

	// Has to be after creating the controls so that interruptEventHandler isn't called before the selector controls exist.
	mPlatformInterface->enableAmplifierMuteRelease();								//	[3514762]
	mPlatformInterface->registerInterrupts ( (IOService*)mPlatformInterface );

    flushAudioControls ();
	if (NULL != mExternalClockSelector) { mExternalClockSelector->flushValue (); }		// Specifically flush the clock selector's values because flushAudioControls() doesn't seem to call it... ???
	
Exit:
	if (NULL != mInitHardwareThread) {
		thread_call_free (mInitHardwareThread);
	}

    debugIOLog (3, "- AppleOnboardAudio[%ld]::protectedInitHardware returns 0x%x", mInstanceIndex, result); 
	return (result);
}

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  NOTE:   This static function will manage an OSArray of AppleOnboardAudio object instances within the 
//			context of each AppleOnboardAudio instance.  The OSArray contains only those objects that are 
//			distinct from the AppleOnboardAudio instance owning the OSArray of AppleOnboardAudio
//			instances.  This allows the 'broadcastSoftwareInterruptMessage' method to examine only other
//			objects when attempting to locate an AppleOnboardAudio instance that is a recipient of a
//			specific software interrupt message.  When this method is registered (see 'protectedInitHardware')
//			this method will be invoked for each AppleOnboardAudio instance that has already been registered
//			and then once for each future registration.  When this method is invoked passing notification of
//			registration of an AppleOnboardAudion instance that is the same as the AppleOnboardAudio instance
//			owning the OSArray of AppleOnboardAudio instances then the AppleOnboardAudio instance for which
//			the notification is being performed is not added to the OSArray of AppleOnboardAudio instances
//			while all other AppleOnboardAudio service instances are added to the OSArray.
//
//  NOTE:   For unknown reasons, this notification will be invoked twice for each AppleOnboardAudio service
//			so it is necessary to iterate through the OSArray of objects to determine if the AppleOnboardAudio
//			service has been previously added to avoid redundantly adding an AppleOnboardAudio service to
//			the OSArray.		[3515372]   28 May 2004 rbm
bool AppleOnboardAudio::aoaPublished (AppleOnboardAudio * aoaObject, void * refCon, IOService * newService) {
	bool	result = FALSE;
	bool	found = FALSE;
	
	debugIOLog ( 3, "+ AppleOnboardAudio::aoaPublished ( %p, %p, %p )", aoaObject, refCon, newService );
	if ( newService != aoaObject ) {
		if ( NULL == aoaObject->mAOAInstanceArray ) {
			debugIOLog ( 3, "  aoaObject->mAOAInstanceArray = OSArray::withObjects ( (const OSObject**)&newService=%p, 1);", newService );
			aoaObject->mAOAInstanceArray = OSArray::withObjects ( (const OSObject**)&newService, 1 );
			FailIf ( NULL == aoaObject->mAOAInstanceArray, Exit );
			result = TRUE;
		} else {
			for ( UInt32 index = 0; index < aoaObject->mAOAInstanceArray->getCount() && !found; index++ ) {
				if ( newService == aoaObject->mAOAInstanceArray->getObject ( index ) ) {
					found = TRUE;
				}
			}
			if ( !found ) {
				debugIOLog ( 3, "  aoaObject->mAOAInstanceArray->setObject ( %p );", newService );
				aoaObject->mAOAInstanceArray->setObject ( newService );
				result = TRUE;
			}
		}
		
		for ( UInt32 index = 0; index < aoaObject->mAOAInstanceArray->getCount(); index++ ) {
			debugIOLog ( 3, "  %p = aoaObject->mAOAInstanceArray->getObject ( %d )", aoaObject->mAOAInstanceArray->getObject ( index ) );
		}
	}
	
Exit:
	debugIOLog ( 3, "- AppleOnboardAudio::aoaPublished ( %p, %p, %p ) returns %d", aoaObject, refCon, newService, result );
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

	debugIOLog (3, "± AppleOnboardAudio[%ld]::createInputSelectorControl - mInputSelector = %p", mInstanceIndex, mInputSelector);

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
	UInt32 temp;

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
					temp = kIOAudioOutputPortSubTypeInternalSpeaker;
					debugIOLog (3, "  add output selection of %4s", &temp);
					break;
				case kIOAudioOutputPortSubTypeExternalSpeaker:
					mExternalSpeakerOutputString = outputString;
					temp = kIOAudioOutputPortSubTypeExternalSpeaker;
					debugIOLog (3, "  add output selection of %4s", &temp);
					break;
				case kIOAudioOutputPortSubTypeLine:
					mLineOutputString = outputString;
					temp = kIOAudioOutputPortSubTypeLine;
					debugIOLog (3, "  add output selection of %4s", &temp);
					break;
				case kIOAudioOutputPortSubTypeSPDIF:
					mDigitalOutputString = outputString;
					temp = kIOAudioOutputPortSubTypeSPDIF;
					debugIOLog (3, "  add output selection of %4s", &temp);
					break;
				case kIOAudioOutputPortSubTypeHeadphones:
					mHeadphoneOutputString = outputString;
					temp = kIOAudioOutputPortSubTypeHeadphones;
					debugIOLog (3, "  add output selection of %4s", &temp);
					break;
				default:
					debugIOLog (2, "  AppleOnboardAudio[%ld]::createOutputSelectorControl: unknown output selection", mInstanceIndex);
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
			debugIOLog (3,  "  mOutputSelector->addAvailableSelection ( '%4s', %p )", (char*)&outputSelection, outputString );
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
				debugIOLog (6, "  removed internal speaker from output selector ****");
			} else {
				mOutputSelector->removeAvailableSelection ( kIOAudioOutputPortSubTypeExternalSpeaker );
				debugIOLog (6, "  removed External Speaker from output selector");
			}
		}
		if ( (kGPIO_Unknown != mPlatformInterface->getComboOutJackTypeConnected ()) ) {
			if ( kGPIO_Connected == mPlatformInterface->getLineOutConnected () ) {
				mOutputSelector->removeAvailableSelection ( kIOAudioOutputPortSubTypeSPDIF );
				debugIOLog (6, "  removed SPDIF from output selector");
			} else if ( kGPIO_Connected == mPlatformInterface->getDigitalOutConnected () ) {
				mOutputSelector->removeAvailableSelection ( kIOAudioOutputPortSubTypeLine );
				debugIOLog (6, "  removed Line from output selector");
			} else {
				mOutputSelector->removeAvailableSelection ( kIOAudioOutputPortSubTypeSPDIF );
				debugIOLog (6, "  removed SPDIF from output selector");
				mOutputSelector->removeAvailableSelection ( kIOAudioOutputPortSubTypeLine );
				debugIOLog (6, "  removed Line from output selector");
			}
		}
	
		mDriverDMAEngine->setProperty ("MappingDictionary", theDictionary);
	}
	
	debugIOLog (3, "AppleOnboardAudio[%ld]::createOutputSelectorControl - mOutputSelector = %p", mInstanceIndex, mOutputSelector);

	result = kIOReturnSuccess;

Exit:
	return result;
}

AudioHardwareObjectInterface * AppleOnboardAudio::getPluginObjectForConnection (const char * entry) {
	AudioHardwareObjectInterface *		thePluginObject;
	OSDictionary *						dictEntry;
	OSString *							pluginIDMatch;

	debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::getPluginObjectForConnection ( %s )", mInstanceIndex, entry );
	thePluginObject = NULL;
	pluginIDMatch = NULL;
	dictEntry = NULL;
	
	dictEntry = OSDynamicCast (OSDictionary, getLayoutEntry (entry, this));
	FailIf (NULL == dictEntry, Exit);

	pluginIDMatch = OSDynamicCast (OSString, dictEntry->getObject (kPluginID));
	FailIf (NULL == pluginIDMatch, Exit);

	thePluginObject = getPluginObjectWithName (pluginIDMatch);
	
	debugIOLog (3, "- AppleOnboardAudio[%ld]::getPluginObjectForConnection - pluginID = %s", mInstanceIndex, pluginIDMatch->getCStringNoCopy());

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
	
	debugIOLog (3, "AppleOnboardAudio[%ld]::getInputDataMuxForConnection - GpioAttributes result = %d", mInstanceIndex, result);

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
			debugIOLog (7, "AppleOnboardAudio[%ld] found matching plugin with ID %s", mInstanceIndex, thePluginID->getCStringNoCopy());
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
	SInt32								defaultInputGain = 0;	//	[3514617]

	debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::createInputGainControls()", mInstanceIndex );
	
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
		defaultInputGain = thePluginObject->getDefaultInputGain ();
	}
	if ( kStereoInputGainControls == mUseInputGainControls ) {
		debugIOLog ( 3, "  mInLeftGainControl = IOAudioLevelControl::createVolumeControl( %ld, %ld, %ld, %lX, %lX, %ld, %p, 0, %lX )", defaultInputGain, minGain, maxGain, mindBGain, maxdBGain, kIOAudioControlChannelIDDefaultLeft, kIOAudioControlChannelNameLeft, 0, kIOAudioControlUsageInput);
		mInLeftGainControl = IOAudioLevelControl::createVolumeControl (defaultInputGain, minGain, maxGain, mindBGain, maxdBGain, kIOAudioControlChannelIDDefaultLeft, kIOAudioControlChannelNameLeft, 0, kIOAudioControlUsageInput);
		if (NULL != mInLeftGainControl) {
			mDriverDMAEngine->addDefaultAudioControl (mInLeftGainControl);
			mInLeftGainControl->setValueChangeHandler ((IOAudioControl::IntValueChangeHandler)inputControlChangeHandler, this);
			// Don't release it because we might reference it later
		}
	
		debugIOLog ( 3, "  mInRightGainControl = IOAudioLevelControl::createVolumeControl( %ld, %ld, %ld, %lX, %lX, %ld, %p, 0, %lX )", defaultInputGain, minGain, maxGain, mindBGain, maxdBGain, kIOAudioControlChannelIDDefaultRight, kIOAudioControlChannelNameRight, 0, kIOAudioControlUsageInput);
		mInRightGainControl = IOAudioLevelControl::createVolumeControl (defaultInputGain, minGain, maxGain, mindBGain, maxdBGain, kIOAudioControlChannelIDDefaultRight, kIOAudioControlChannelNameRight, 0, kIOAudioControlUsageInput);
		if (NULL != mInRightGainControl) {
			mDriverDMAEngine->addDefaultAudioControl (mInRightGainControl);
			mInRightGainControl->setValueChangeHandler ((IOAudioControl::IntValueChangeHandler)inputControlChangeHandler, this);
			// Don't release it because we might reference it later
		}
	} else if ( kMonoInputGainControl == mUseInputGainControls ){
		debugIOLog ( 3, "  mInMasterGainControl = IOAudioLevelControl::createVolumeControl( %ld, %ld, %ld, %lX, %lX, %ld, %p, 0, %lX )", defaultInputGain, minGain, maxGain, mindBGain, maxdBGain, kIOAudioControlChannelIDAll, kIOAudioControlChannelNameAll, 0, kIOAudioControlUsageInput);
		mInMasterGainControl = IOAudioLevelControl::createVolumeControl (defaultInputGain, minGain, maxGain, mindBGain, maxdBGain, kIOAudioControlChannelIDAll, kIOAudioControlChannelNameAll, 0, kIOAudioControlUsageInput);
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
	debugIOLog ( 3, "- AppleOnboardAudio[%ld]::createInputGainControls() returns %lX", mInstanceIndex, result );
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
	OSDictionary *						theSpeakerIDDict;
	OSDictionary *						theSignalProcessingDict;
	OSDictionary *						theOutput;
	OSNumber *							theMaxVolumeNumber;
	OSString *							speakerIDString;
	char								speakerIDCString[32];
	UInt32								maxVolumeOffset;
	
	maxVolumeOffset = 0;
	
	theOutput = OSDynamicCast(OSDictionary, getLayoutEntry (inSelectedOutput, this));
	FailIf (NULL == theOutput, Exit);

	theSignalProcessingDict = OSDynamicCast (OSDictionary, theOutput->getObject (kSignalProcessing));
	if ( NULL != theSignalProcessingDict ) {
		sprintf (speakerIDCString, "%s_%ld", kSpeakerID, mInternalSpeakerID); 
		speakerIDString = OSString::withCString (speakerIDCString);
		FailIf (NULL == speakerIDString, Exit);
		
		theSpeakerIDDict = OSDynamicCast (OSDictionary, theSignalProcessingDict->getObject (speakerIDString));
		speakerIDString->release ();
		FailIf (NULL == theSpeakerIDDict, Exit);
	
		theMaxVolumeNumber = OSDynamicCast (OSNumber, theSpeakerIDDict->getObject (kMaxVolumeOffset));
		if (NULL != theMaxVolumeNumber) {
			debugIOLog (3, "getMaxVolumeOffsetForOutput: theMaxVolumeNumber value = %d", theMaxVolumeNumber->unsigned32BitValue ());
			maxVolumeOffset = theMaxVolumeNumber->unsigned32BitValue ();
		} 
	}

Exit:
	return maxVolumeOffset;
}

void AppleOnboardAudio::setSoftwareOutputDSP (const char * inSelectedOutput) {
	OSDictionary *						theSpeakerIDDict;
	OSDictionary *						theSignalProcessingDict;
	OSDictionary *						theOutput;
	OSDictionary *						theSoftwareDSPDict;
	OSString *							speakerIDString;
	char								speakerIDCString[32];
	
	debugIOLog (3, "+ AppleOnboardAudio[%ld]::setSoftwareOutputDSP (%s).", mInstanceIndex, inSelectedOutput);
	// check if we already have calculated coefficients for this output
	// this will NOT work for more than one output having processing on it
	if (mCurrentProcessingOutputString->isEqualTo (inSelectedOutput)) {
		
		debugIOLog (3, "Enabling DSP");
	
		mDriverDMAEngine->enableOutputProcessing ();
		
		debugIOLog (3, "mCurrentProcessingOutputString is '%s', coefficients not updated.", mCurrentProcessingOutputString->getCStringNoCopy ());
	} else {

		// commmon case is disabled, this is the safer fail scenario
		mDriverDMAEngine->disableOutputProcessing ();
		
		debugIOLog (3, "processing disabled.");
	
		theOutput = OSDynamicCast(OSDictionary, getLayoutEntry (inSelectedOutput, this));
		FailIf (NULL == theOutput, Exit);
	
		theSignalProcessingDict = OSDynamicCast (OSDictionary, theOutput->getObject (kSignalProcessing));
		if ( NULL != theSignalProcessingDict ) {
	
			sprintf (speakerIDCString, "%s_%ld", kSpeakerID, mInternalSpeakerID); 
			debugIOLog (3, "setSoftwareOutputDSP: speakerIDString = %s", speakerIDCString);
			speakerIDString = OSString::withCString (speakerIDCString);
			FailIf (NULL == speakerIDString, Exit);
			
			theSpeakerIDDict = OSDynamicCast (OSDictionary, theSignalProcessingDict->getObject (speakerIDString));
			speakerIDString->release ();
			FailIf (NULL == theSpeakerIDDict, Exit);
			debugIOLog (3, "setSoftwareOutputDSP: theSpeakerIDDict = %p", theSpeakerIDDict);
		
			theSoftwareDSPDict = OSDynamicCast (OSDictionary, theSpeakerIDDict->getObject (kSoftwareDSP));
			if (NULL != theSoftwareDSPDict) {
				debugIOLog (3, "setSoftwareOutputDSP: theSoftwareDSPDict = %p", theSoftwareDSPDict);
		
				mDriverDMAEngine->setOutputSignalProcessing (theSoftwareDSPDict);
				
				debugIOLog (3, "Processing set");
				
				// if we get here, we've found some DSP to perform for this output, so update the currently prepared output string
				debugIOLog (3, "mCurrentProcessingOutputString is '%s'", mCurrentProcessingOutputString->getCStringNoCopy ());
				mCurrentProcessingOutputString->initWithCString (inSelectedOutput);
				debugIOLog (3, "mCurrentProcessingOutputString set to '%s', coefficients will be updated.", mCurrentProcessingOutputString->getCStringNoCopy ());
			}
		}
	}	

Exit:
	debugIOLog (3, "- AppleOnboardAudio[%ld]::setSoftwareOutputDSP ('%4s' ).", mInstanceIndex, inSelectedOutput);
	return;
}

// [3306305]
void AppleOnboardAudio::setSoftwareInputDSP (const char * inSelectedInput) {
	OSDictionary *						theSignalProcessingDict;
	OSDictionary *						theMicrophoneIDDict;
	OSDictionary *						theSoftwareDSPDict;
	OSDictionary *						theInput;
	OSString *							microphoneIDString;
	char								microphoneIDCString[32];

	debugIOLog (3, "set input DSP for '%s'.", inSelectedInput);
	// check if we already have calculated coefficients for this input
	// this will NOT work for more than one input having processing on it
	if (mCurrentProcessingInputString->isEqualTo (inSelectedInput)) {
		
		debugIOLog (3, "Enabling input DSP");

		mDriverDMAEngine->enableInputProcessing ();

		debugIOLog (3, "mCurrentProcessingInputString is '%s', coefficients not updated.", mCurrentProcessingInputString->getCStringNoCopy ());

	} else {

		debugIOLog (3, "Disabling input DSP");

		// commmon case is disabled, this is the safer fail scenario
		mDriverDMAEngine->disableInputProcessing ();
		debugIOLog (3, "input processing disabled.");
	
		theInput = OSDynamicCast(OSDictionary, getLayoutEntry (inSelectedInput, this));
		FailIf (NULL == theInput, Exit);
	
		theSignalProcessingDict = OSDynamicCast (OSDictionary, theInput->getObject (kSignalProcessing));
		if (NULL != theSignalProcessingDict) {
	
			sprintf (microphoneIDCString, "%s_%ld", kMicrophoneID, mInternalMicrophoneID); 
			debugIOLog (3, "setSoftwareInputDSP: inputDeviceIDString = %s", microphoneIDCString);
			microphoneIDString = OSString::withCString (microphoneIDCString);
			FailIf (NULL == microphoneIDString, Exit);
			
			theMicrophoneIDDict = OSDynamicCast (OSDictionary, theSignalProcessingDict->getObject (microphoneIDString));
			microphoneIDString->release ();
			FailIf (NULL == theMicrophoneIDDict, Exit);
			debugIOLog (3, "setSoftwareInputDSP: theMicrophoneIDDict = %p", theMicrophoneIDDict);
	
			theSoftwareDSPDict = OSDynamicCast (OSDictionary, theMicrophoneIDDict->getObject (kSoftwareDSP));
			FailIf (NULL == theSoftwareDSPDict, Exit);
			debugIOLog (3, "setSoftwareInputDSP: theSoftwareDSPDict = %p", theSoftwareDSPDict);
			
			mDriverDMAEngine->setInputSignalProcessing (theSoftwareDSPDict);
			
			debugIOLog (3, "Input processing set");
			
			// if we get here, we've found some DSP to perform for this output, so update the currently prepared output string
			debugIOLog (3, "mCurrentProcessingInputString is '%s'", mCurrentProcessingInputString->getCStringNoCopy ());
			mCurrentProcessingInputString->initWithCString (inSelectedInput);
			debugIOLog (3, "mCurrentProcessingInputString set to '%s', coefficients will be updated.", mCurrentProcessingInputString->getCStringNoCopy ());
		}
	}	

Exit:
	return;
}

UInt32 AppleOnboardAudio::setClipRoutineForOutput (const char * inSelectedOutput) {
	OSDictionary *						theOutput;
	OSString *							clipRoutineString;
	OSArray *							theArray;
	IOReturn							result;
	UInt32								arrayCount;
	UInt32								index;
	
	debugIOLog (3,  "+ AppleOnboardAudio[%ld]::setClipRoutineForOutput ( '%4s' )", mInstanceIndex, inSelectedOutput );
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
	debugIOLog (3,  "- AppleOnboardAudio[%ld]::setClipRoutineForOutput ( '%4s' ) returns %X", mInstanceIndex, inSelectedOutput, result );
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

	debugIOLog (3, "+ AppleOnboardAudio[%ld]::createOutputVolumeControls", mInstanceIndex);

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

	debugIOLog (3, "- AppleOnboardAudio[%ld]::createOutputVolumeControls", mInstanceIndex);
	return result;
}

IOReturn AppleOnboardAudio::createDefaultControls () {
	AudioHardwareObjectInterface *		thePluginObject;
    OSDictionary *						AOAprop;
	OSBoolean *							clockSelectBoolean;
	OSArray *							inputListArray;
	UInt32								index;
	UInt32								count;
    IOReturn							result;
	Boolean								hasPlaythrough;
	Boolean								hasInput;
	
	debugIOLog (3, "+ AppleOnboardAudio[%ld]::createDefaultControls", mInstanceIndex);

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
    debugIOLog (3, "- %lX = AppleOnboardAudio[%ld]::createDefaultControls", mInstanceIndex, result);
    return result;
}

//	--------------------------------------------------------------------------------
//	[3515371]	begin	{
//	Searches array of AppleOnboardAudio instances and returns a pointer to the
//	AppleOnboardAudio instance with the specified layoutID or NULL.
//	
AppleOnboardAudio* AppleOnboardAudio::findAOAInstanceWithLayoutID ( UInt32 layoutID ) {
	AppleOnboardAudio *		theAOA;
	AppleOnboardAudio *		result = NULL;
	UInt32					numInstances;
	
	debugIOLog ( 3, "%p AppleOnboardAudio[%ld]::findAOAInstanceWithLayoutID ( %d )", mInstanceIndex, this, layoutID );
	if ( NULL != mAOAInstanceArray ) {
		numInstances = mAOAInstanceArray->getCount();
		for ( UInt32 index = 0; index <  numInstances && ( NULL == result ); index++ ) {
			theAOA = OSDynamicCast ( AppleOnboardAudio, mAOAInstanceArray->getObject ( index ) );
			if ( theAOA->getLayoutID() == layoutID ) {
				result = theAOA;
			}
		}
	}
	return result;
}
	//	}	end	[3515371]


//	--------------------------------------------------------------------------------
#pragma mark +IOAUDIO CONTROL HANDLERS
// --------------------------------------------------------------------------
//	You either have only a master volume control, or you have both volume controls.
//	[3514514]	With the addition of digital output being supported on a single
//				codec, it is necessary to remove the volume controls altogether
//				when streaming to an exclusive digital output.  A digital output
//				shall be considered as exclusive when a combo output jack exists
//				and the combo output jack has a digital plug inserted.  Under 
//				all other conditions the analog algorithms for publishing and 
//				removal of controls prevail.  (rbm 5 Feb 2004)
//
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

	debugIOLog (3, "+ AppleOnboardAudio[%ld]::AdjustOutputVolumeControls( %p, '%4s' ) - mindBVol %lX, maxdBVol %lX, minVolume %ld, maxVolume %ld", 
					mInstanceIndex, thePluginObject, (char*)&selectionCode, mindBVol, maxdBVol, minVolume, maxVolume);

	mDriverDMAEngine->pauseAudioEngine ();
	mDriverDMAEngine->beginConfigurationChange ();

	//	[3514514]	Exclusive digital outputs do not have discrete or master volume controls.  Allow
	//				enabling of software output volume scaling only for outputs other than an
	//				exclusive digital output.
	if ( ((kIOAudioOutputPortSubTypeSPDIF == selectionCode) && (kGPIO_Unknown == mPlatformInterface->getComboOutJackTypeConnected())) || (kIOAudioOutputPortSubTypeSPDIF != selectionCode) ) {
		debugIOLog ( 3, "  output is not exclusive digital output!" );

		hasMaster = hasMasterVolumeControl (selectionCode);
		hasLeft = hasLeftVolumeControl (selectionCode);
		hasRight = hasRightVolumeControl (selectionCode);
	
		// [3527440] we have a volume control, but needs software to implement it
		if ((hasMaster || hasLeft || hasRight) && (FALSE == thePluginObject->hasHardwareVolume ())) {
			debugIOLog (3, "have volume controls and need software implementation.");
			mCurrentPluginNeedsSoftwareOutputVolume = TRUE;
			mDriverDMAEngine->setUseSoftwareOutputVolume (TRUE, minVolume, maxVolume, mindBVol, maxdBVol);
		} else {
			debugIOLog (3, "no volume controls or don't need software implementation.");
			mCurrentPluginNeedsSoftwareOutputVolume = FALSE;
			mDriverDMAEngine->setUseSoftwareOutputVolume (FALSE);
		}
	
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

	} else {
		debugIOLog ( 3, "  output is exclusive digital output- removing all level controls!" );

		mCurrentPluginNeedsSoftwareOutputVolume = FALSE;
		mDriverDMAEngine->setUseSoftwareOutputVolume (FALSE);
		
		removeMasterVolumeControl();
		removeLeftVolumeControl();
		removeRightVolumeControl ();

	}
	

	mDriverDMAEngine->completeConfigurationChange ();
	mDriverDMAEngine->resumeAudioEngine ();

Exit:
	debugIOLog (3, "- AppleOnboardAudio[%ld]::AdjustOutputVolumeControls( %p, '%4s' )", mInstanceIndex, thePluginObject, (char*)&selectionCode );
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

	debugIOLog (3, "AppleOnboardAudio[%ld]::AdjustInputGainControls - mindBGain %lX, maxdBGain %lX, minGain %ld, maxGain %ld", mInstanceIndex, mindBGain, maxdBGain, minGain, maxGain);

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
	debugIOLog (3, "AppleOnboardAudio::createLeftVolumeControl - leftVol initially = %ld, theNumber = %p", leftVol, theNumber);

	if (NULL == theNumber || leftVol == 0) {
		theNumber = OSDynamicCast(OSNumber, mCurrentOutputPlugin->getProperty(kPluginPListMasterVol));
		if (NULL != theNumber) {
			leftVol = theNumber->unsigned32BitValue();
			debugIOLog (6, "createLeftVolumeControl - leftVol from master = %ld", leftVol);
			if (leftVol == -1) {
				leftVol = maxVolume / 2;
				debugIOLog (6, "createLeftVolumeControl - leftVol from max/2 = %ld", leftVol);
			}
		} else {
			leftVol = mCurrentOutputPlugin->getDefaultOutputVolume ();
			debugIOLog (6, "createLeftVolumeControl - leftVol from default = %ld", leftVol);
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
		} else {
			rightVol = mCurrentOutputPlugin->getDefaultOutputVolume ();
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
	debugIOLog (3, "AppleOnboardAudio::createMasterVolumeControl - masterVol initially = %ld, theNumber = %p", masterVol, theNumber);

	if (-1 == masterVol || NULL == theNumber) {
		OSNumber * 						theLeftNumber;
		OSNumber * 						theRightNumber;

		theLeftNumber = OSDynamicCast(OSNumber, mCurrentOutputPlugin->getProperty(kPluginPListLeftVol));
		theRightNumber = OSDynamicCast(OSNumber, mCurrentOutputPlugin->getProperty(kPluginPListRightVol));

		if (NULL == theLeftNumber && NULL == theRightNumber && NULL == theNumber) {
			masterVol = mCurrentOutputPlugin->getDefaultOutputVolume ();
			debugIOLog (6, "createMasterVolumeControl - masterVol from default = %ld", masterVol);
		} else {
			if (NULL == theLeftNumber) {
				masterVol = maxVolume;
				debugIOLog (6,"createMasterVolumeControl - masterVol from max = %ld", masterVol);
			} else {
				masterVol = theLeftNumber->unsigned32BitValue();
				debugIOLog (6,"createMasterVolumeControl - masterVol from left = %ld", masterVol);
			}
			if (NULL != theRightNumber) {
				masterVol += theRightNumber->unsigned32BitValue();
				debugIOLog (6,"createMasterVolumeControl - masterVol from right = %ld", masterVol);
			}
			masterVol >>= 1;
			debugIOLog (6,"createMasterVolumeControl - masterVol after shift = %ld", masterVol);
		}
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
	
	theNumber = OSDynamicCast(OSNumber, mCurrentInputPlugin->getProperty(kPluginPListLeftGain));
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
	
	theNumber = OSDynamicCast(OSNumber, mCurrentInputPlugin->getProperty(kPluginPListRightGain));
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

	result = kIOReturnError;
	audioDevice = OSDynamicCast (AppleOnboardAudio, target);
	wasPoweredDown = FALSE;
	FailIf (NULL == audioDevice, Exit);
	debugIOLog (3, "+ AppleOnboardAudio[%ld]::outputControlChangeHandler (%p, %p, %lX, %lX)", audioDevice->mInstanceIndex, target, control, oldValue, newValue);

	// We have to make sure the hardware is on before we can send it any control changes [2981190]
	debugIOLog ( 3, "  ourPowerState = %ld", audioDevice->ourPowerState );
	if ( kIOAudioDeviceSleep == audioDevice->ourPowerState ) {

		IOAudioDevicePowerState newState;
		newState = kIOAudioDeviceActive;

		//	Wake requires that the transport object go active prior
		//	to the hardware plugin object(s) going active.
		if ( NULL != audioDevice->mTransportInterface ) {
			result = audioDevice->mTransportInterface->performTransportWake ();
		}
		result = audioDevice->callPluginsInOrder (kPowerStateChange, kIOAudioDeviceActive);
		
		debugIOLog ( 5, "  AppleOnboardAudio[%ld]::outputControlChangeHandler setting 'ourPowerState' to kIOAudioDeviceActive", audioDevice->mInstanceIndex );

		audioDevice->mUCState.ucPowerState = audioDevice->ourPowerState = kIOAudioDeviceActive;
		
		if (NULL != audioDevice->mExternalClockSelector) {
			audioDevice->mExternalClockSelector->flushValue ();
		}
		if (NULL != audioDevice->mOutMuteControl) {
			audioDevice->mOutMuteControl->flushValue ();					// Restore hardware to the user's selected state
		}

		wasPoweredDown = TRUE;
		debugIOLog ( 3, "  about to restart the 'pollTimer'" );
		audioDevice->setPollTimer();										//  [3690065]
	}

	switch (control->getType ()) {
		case kIOAudioControlTypeLevel:
			debugIOLog ( 5, "  control->getType () is a kIOAudioControlTypeLevel" );
			switch (control->getSubType ()) {
				case kIOAudioLevelControlSubTypeVolume:
					debugIOLog ( 5, "  control->getSubType () is a kIOAudioLevelControlSubTypeVolume" );
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
									debugIOLog (3, "volume control change calling audioDevice->callPluginsInOrder ( kSetAnalogMuteState, TRUE )");
									result = audioDevice->callPluginsInOrder ( kSetAnalogMuteState, TRUE );
								}
							} else if (oldValue == levelControl->getMinValue () && FALSE == audioDevice->mIsMute) {
								OSNumber *			muteState;
								muteState = OSNumber::withNumber ((long long unsigned int)0, 32);
								if (NULL != audioDevice->mOutMuteControl) {
									audioDevice->mOutMuteControl->hardwareValueChanged (muteState);
									debugIOLog (3, "volume control change calling audioDevice->callPluginsInOrder ( kSetAnalogMuteState, FALSE )");
									result = audioDevice->callPluginsInOrder ( kSetAnalogMuteState, FALSE );
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
					debugIOLog ( 5, "  control->getSubType () is a kIOAudioLevelControlSubTypePRAMVolume" );
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
			debugIOLog ( 5, "  control->getType () is a kIOAudioControlTypeToggle" );
			subType = control->getSubType ();
			switch (control->getSubType ()) {
				case kIOAudioToggleControlSubTypeMute:
					debugIOLog ( 5, "  control->getSubType() is a kIOAudioToggleControlSubTypeMute" );
					if (FALSE == newValue) {	// [3578824] - need to pick up any changes to levels that happened while we were muted, and set the levels before unmuting
						audioDevice->volumeLeftChange(audioDevice->mVolLeft, TRUE);
						audioDevice->volumeRightChange(audioDevice->mVolRight, TRUE);
					}
					audioDevice->selectCodecOutputWithMuteState( newValue );
					audioDevice->selectOutputAmplifiers ( audioDevice->mOutputSelector->getIntValue (), newValue );
					audioDevice->mIsMute = newValue;
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
			break;
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
					debugIOLog (3, "  right volume control change updating mute state");
			}
				muteState->release ();
			} else if (newValue != levelControl->getMinValue () && oldValue == levelControl->getMinValue () && FALSE == audioDevice->mIsMute) {
				OSNumber *			muteState;
				muteState = OSNumber::withNumber ((long long unsigned int)0, 32);
				if (NULL != audioDevice->mOutMuteControl) {
					audioDevice->mOutMuteControl->hardwareValueChanged (muteState);
					debugIOLog (3, "  left volume control change updating mute state");
				}
				muteState->release ();
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
	debugIOLog (3, "- AppleOnboardAudio[%ld]::outputControlChangeHandler (%p, %p, %lX, %lX) returns %X", audioDevice->mInstanceIndex, target, control, oldValue, newValue, result);

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

	debugIOLog (3, "+ AppleOnboardAudio[%ld]::outputSelectorChanged( '%4s' )", mInstanceIndex, (char *)&newValue);

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
	
		setSoftwareOutputDSP (connectionString);
	
		AdjustOutputVolumeControls(mCurrentOutputPlugin, newValue);

		selectCodecOutputWithMuteState( mIsMute );	
		selectOutputAmplifiers(newValue, mIsMute, FALSE);		//	Tell selectOutput that this is an explicit selection
	} else {
		debugIOLog ( 3, "  AppleOnboardAudio[%ld]::outputSelectorChanged disallowing selection of '%4s'", mInstanceIndex, (char *)&newValue );
	}

Exit:
	debugIOLog (3, "- AppleOnboardAudio[%ld]::outputSelectorChanged( '%4s' ) returns %X", mInstanceIndex, (char *)&newValue, result);
	return result;
}

// This is called when we're on hardware that only has one active volume control (either right or left)
// otherwise the respective right or left volume handler will be called.
// This calls both volume handers becasue it doesn't know which one is really the active volume control.
IOReturn AppleOnboardAudio::volumeMasterChange (SInt32 newValue) {
	IOReturn						result = kIOReturnSuccess;

	debugIOLog (3, "+ AppleOnboardAudio[%ld]::volumeMasterChange (%ld)", mInstanceIndex, newValue);

	result = kIOReturnError;

	// Don't know which volume control really exists, so adjust both -- they'll ignore the change if they don't exist
	result = volumeLeftChange (newValue);
	result = volumeRightChange (newValue);

	result = kIOReturnSuccess;

	debugIOLog (3, "- AppleOnboardAudio[%ld]::volumeMasterChange, 0x%x", mInstanceIndex, result);
	return result;
}

IOReturn AppleOnboardAudio::volumeLeftChange (SInt32 newValue, bool ignoreMuteState) {
	IOReturn							result;
	UInt32 								count;
	UInt32 								index;
	AudioHardwareObjectInterface* 		thePluginObject;

	debugIOLog (3, "+ AppleOnboardAudio[%ld]::volumeLeftChange (%ld)", mInstanceIndex, newValue);

	if ( mIsMute && FALSE == ignoreMuteState) {		//	[3435307]
		if (NULL != mPluginObjects) {
			count = mPluginObjects->getCount ();
			for (index = 0; index < count; index++) {
				thePluginObject = getIndexedPluginObject (index);
				if((NULL != thePluginObject)) {
					thePluginObject->setMute (mIsMute);
				}
			}
		}
	} else {
		if (TRUE == mCurrentPluginNeedsSoftwareOutputVolume) {	// [3527440] aml
			// set software volume (will be scaled to hardware range)
			mDriverDMAEngine->setOutputVolumeLeft (newValue);
			// set hardware volume to 1.0
			if (NULL != mPluginObjects) {
				count = mPluginObjects->getCount ();
				for (index = 0; index < count; index++) {
					thePluginObject = getIndexedPluginObject (index);
					if((NULL != thePluginObject)) {
						thePluginObject->setVolume (thePluginObject->getMaximumVolume (), thePluginObject->getMaximumVolume ());
					}
				}
			}
		} else {
			// [3339273] set volume on all plugins
			if (NULL != mPluginObjects) {
				count = mPluginObjects->getCount ();
				for (index = 0; index < count; index++) {
					thePluginObject = getIndexedPluginObject (index);
					if((NULL != thePluginObject)) {
						thePluginObject->setVolume (newValue, mVolRight);
					}
				}
			}
		}
	}

	mVolLeft = newValue;

	result = kIOReturnSuccess;

	debugIOLog (3, "- AppleOnboardAudio[%ld]::volumeLeftChange, 0x%x", mInstanceIndex, result);

	return result;
}

IOReturn AppleOnboardAudio::volumeRightChange (SInt32 newValue, bool ignoreMuteState) {
	IOReturn							result;
	UInt32								count;
	UInt32 								index;
	AudioHardwareObjectInterface* 		thePluginObject;

	debugIOLog (3, "+ AppleOnboardAudio[%ld]::volumeRightChange (%ld)", mInstanceIndex, newValue);

	if ( mIsMute && FALSE == ignoreMuteState ) {		//	[3435307]
		if (NULL != mPluginObjects) {
			count = mPluginObjects->getCount ();
			for (index = 0; index < count; index++) {
				thePluginObject = getIndexedPluginObject (index);
				if((NULL != thePluginObject)) {
					thePluginObject->setMute (mIsMute);
				}
			}
		}
	} else {
		if (TRUE == mCurrentPluginNeedsSoftwareOutputVolume) {		// [3527440] aml
			// set software volume (will be scaled to hardware range)
			mDriverDMAEngine->setOutputVolumeRight (newValue);
			// set hardware volume to 1.0
			if (NULL != mPluginObjects) {
				count = mPluginObjects->getCount ();
				for (index = 0; index < count; index++) {
					thePluginObject = getIndexedPluginObject (index);
					if((NULL != thePluginObject)) {
						thePluginObject->setVolume (thePluginObject->getMaximumVolume (), thePluginObject->getMaximumVolume ());
					}
				}
			}
		} else {
			// [3339273] set volume on all plugins
			if (NULL != mPluginObjects) {
				count = mPluginObjects->getCount ();
				for (index = 0; index < count; index++) {
					thePluginObject = getIndexedPluginObject (index);
					if((NULL != thePluginObject)) {
						thePluginObject->setVolume (mVolLeft, newValue);
					}
				}
			}
		}
	}

	mVolRight = newValue;

	result = kIOReturnSuccess;

	debugIOLog (3, "- AppleOnboardAudio[%ld]::volumeRightChange, result = 0x%x", mInstanceIndex, result);

	return result;
}

IOReturn AppleOnboardAudio::selectCodecOutputWithMuteState (SInt32 newValue) {
	UInt32			comboOutJackTypeState;
	IOReturn		result = kIOReturnError;
	
	
	debugIOLog (3, "+ AppleOnboardAudio[%ld]::selectCodecOutputWithMuteState (%ld)", mInstanceIndex, newValue);

	FailIf ( NULL == mPlatformInterface, Exit );
	
	comboOutJackTypeState = mPlatformInterface->getComboOutJackTypeConnected ();
	if ( kGPIO_Unknown == comboOutJackTypeState ) {
		debugIOLog (3, "  comboOutJackTypeState is unknown");
		if ( !mEncodedOutputFormat ) {
			//	[3514514]	Digital out is not an exclusive digital output so apply the mute state
			//				to both the analog and digital paths (i.e. shared control).
			debugIOLog (3, "  selectCodecOutputWithMuteState calling callPluginsInOrder ( kSetMuteState, %ld ) non-exclusive dig", newValue);
			result = callPluginsInOrder ( kSetMuteState, newValue );
		} else {
			debugIOLog (3, "  selectCodecOutputWithMuteState calling callPluginsInOrder ( kSetAnalogMuteState, %ld ) exclusive dig", TRUE);
			result = callPluginsInOrder ( kSetAnalogMuteState, TRUE );
			if ( kIOReturnSuccess == result ) {
				debugIOLog (3, "  selectCodecOutputWithMuteState calling callPluginsInOrder ( kSetDigitalMuteState, %ld ) exclusive dig", newValue);
				result = callPluginsInOrder ( kSetDigitalMuteState, newValue );
			}
		}
	} else {
		debugIOLog (3, "  comboOutJackTypeState is %s", comboOutJackTypeState == kGPIO_TypeIsDigital ? "digital" : "analog");
		if ( !mEncodedOutputFormat ) {
			//	[3514514]	Digital out is  an exclusive digital output so apply the mute state
			//				to the active output (analog v.s. digital) and mute the inactive output.
			//	[3513367, 3544877]	rbm		begin {	
			//	The digital status is only valid if the jack is connected.  If the jack
			//	is disconnected then an active 'digital' status should be ignored and the
			//	digital output should be muted.
			//
			//	ACTION TABLES - DIGITAL & ANALOG MUTE:
			//
			//		COMBO OUT TYPE		COMBO OUT DETECT		MUTE ACTION
			//		
			//			DIGITAL				REMOVED				MUTE DIGITAL, UNMUTE ANALOG
			//			DIGITAL				INSERTED			MUTE ANALOG, UNMUTE DIGITAL
			//			ANALOG				REMOVED				MUTE DIGITAL, UNMUTE ANALOG
			//			ANALOG				INSERTED			MUTE DIGITAL, UNMUTE ANALOG
			//
			
			GpioAttributes theAnalogState = kGPIO_Unknown;
			debugIOLog (3, "(UInt32)mPlatformInterface->getComboOutAssociation () returns 0x%lx", (UInt32)mPlatformInterface->getComboOutAssociation ());
			switch ( (UInt32)mPlatformInterface->getComboOutAssociation () ) {
				case kGPIO_Selector_HeadphoneDetect:	theAnalogState = mPlatformInterface->getHeadphoneConnected ();		break;
				case kGPIO_Selector_LineOutDetect:		theAnalogState = mPlatformInterface->getLineOutConnected ();		break;
				case kGPIO_Selector_SpeakerDetect:		theAnalogState = mPlatformInterface->getSpeakerConnected ();		break;
			}
			debugIOLog (3, "theAnalogState = 0x%lx, comboOutJackTypeState = 0x%lx", theAnalogState, comboOutJackTypeState);
			if ( kGPIO_Connected == theAnalogState && kGPIO_TypeIsDigital == comboOutJackTypeState ) {
				//	[3514514]	Exclusive digital output is selected so apply control state to
				//				the digital section but mute the analog section.
				debugIOLog (3, "  selectCodecOutputWithMuteState calling callPluginsInOrder ( kSetAnalogMuteState, %ld ) exclusive dig", TRUE);
				result = callPluginsInOrder ( kSetAnalogMuteState, TRUE );
				if ( kIOReturnSuccess == result ) {
					debugIOLog (3, "  selectCodecOutputWithMuteState calling callPluginsInOrder ( kSetDigitalMuteState, %ld ) exclusive dig", newValue);
					result = callPluginsInOrder ( kSetDigitalMuteState, newValue );
				}
				result = callPluginsInOrder ( kSetEnableSPDIFOut, TRUE );
			} else {
				//	[3514514]	Exclusive digital output is not selected so apply control state to
				//				the analog section but mute the digital section.
				debugIOLog (3, "  selectCodecOutputWithMuteState calling callPluginsInOrder ( kSetAnalogMuteState, %ld ) non-exclusive dig", newValue);
				result = callPluginsInOrder ( kSetAnalogMuteState, newValue );
				if ( kIOReturnSuccess == result ) {
					debugIOLog (3, "  selectCodecOutputWithMuteState calling callPluginsInOrder ( kSetDigitalMuteState, %ld ) non-exclusive dig", TRUE);
					result = callPluginsInOrder ( kSetDigitalMuteState, TRUE );
				}
				result = callPluginsInOrder ( kSetEnableSPDIFOut, FALSE );
			}
			//	} end	[3513367, 3544877]
		} else {
			debugIOLog (3, "encoded format");
			GpioAttributes theAnalogState = kGPIO_Unknown;
			switch ( (UInt32)mPlatformInterface->getComboOutAssociation () ) {
				case kGPIO_Selector_HeadphoneDetect:	theAnalogState = mPlatformInterface->getHeadphoneConnected ();		break;
				case kGPIO_Selector_LineOutDetect:		theAnalogState = mPlatformInterface->getLineOutConnected ();		break;
				case kGPIO_Selector_SpeakerDetect:		theAnalogState = mPlatformInterface->getSpeakerConnected ();		break;
			}
			//	[3656784] mute analog always as we are in encoded format mode...
			debugIOLog (3, "  selectCodecOutputWithMuteState calling callPluginsInOrder ( kSetAnalogMuteState, TRUE ) for encoded format");
			result = callPluginsInOrder ( kSetAnalogMuteState, TRUE );
			if ( kIOReturnSuccess == result ) {
				//	simplified version of original fix for [3514514], exclusive digital output is not selected so mute the digital section
				if (!( kGPIO_Connected == theAnalogState && kGPIO_TypeIsDigital == comboOutJackTypeState )) {
					newValue = TRUE;
				}
				debugIOLog (3, "  selectCodecOutputWithMuteState calling callPluginsInOrder ( kSetDigitalMuteState, %ld ) exclusive dig", newValue);
				result = callPluginsInOrder ( kSetDigitalMuteState, newValue );
			}
		}
	}
Exit:
	debugIOLog (3, "- AppleOnboardAudio[%ld]::selectCodecOutputWithMuteState", mInstanceIndex);
    return result;
}

IOReturn AppleOnboardAudio::inputControlChangeHandler (IOService *target, IOAudioControl *control, SInt32 oldValue, SInt32 newValue) {
	IOReturn						result = kIOReturnError;
	AppleOnboardAudio *				audioDevice;
	IOAudioLevelControl *			levelControl;
	Boolean							wasPoweredDown;

	audioDevice = OSDynamicCast (AppleOnboardAudio, target);
	wasPoweredDown = FALSE;
	FailIf (NULL == audioDevice, Exit);
	debugIOLog (3, "+ AppleOnboardAudio[%ld]::inputControlChangeHandler (%p, %p, %lX, %lX)", audioDevice->mInstanceIndex, target, control, oldValue, newValue);

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
		if ( kIOAudioDeviceActive != audioDevice->ourPowerState ) {
			debugIOLog ( 5, "  AppleOnboardAudio[%ld]::inputControlChangeHandler setting 'ourPowerState' to kIOAudioDeviceActive", audioDevice->mInstanceIndex );
		}
		audioDevice->ourPowerState = kIOAudioDeviceActive;
		audioDevice->mUCState.ucPowerState = audioDevice->ourPowerState;
		if (NULL != audioDevice->mExternalClockSelector) {
			audioDevice->mExternalClockSelector->flushValue ();
		}
// MAKE INPUT MUTE CONTROL?
//		if (NULL != audioDevice->mOutMuteControl) {
//			audioDevice->mOutMuteControl->flushValue ();					// Restore hardware to the user's selected state
//		}

		wasPoweredDown = TRUE;
		debugIOLog ( 3, "  about to restart the 'pollTimer'" );
		audioDevice->setPollTimer();										//  [3690065]
	}

	switch (control->getType ()) {
		case kIOAudioControlTypeLevel:
			debugIOLog ( 3, "  control type = kIOAudioControlTypeLevel" );
			levelControl = OSDynamicCast (IOAudioLevelControl, control);

			switch (control->getChannelID ()) {
				case kIOAudioControlChannelIDDefaultLeft:
					debugIOLog ( 3, "  channel ID = kIOAudioControlChannelIDDefaultLeft" );
					result = audioDevice->gainLeftChanged (newValue);
					break;
				case kIOAudioControlChannelIDDefaultRight:
					debugIOLog ( 3, "  channel ID = kIOAudioControlChannelIDDefaultRight" );
					result = audioDevice->gainRightChanged (newValue);
					break;
				case kIOAudioControlChannelIDAll:
					debugIOLog ( 3, "  channel ID = kIOAudioControlChannelIDAll" );
					result = audioDevice->gainRightChanged (newValue);
					break;
			}
			break;
		case kIOAudioControlTypeToggle:
			debugIOLog ( 3, "  control type = kIOAudioControlTypeToggle" );
			result = audioDevice->passThruChanged (newValue);
			break;
		case kIOAudioControlTypeSelector:
			debugIOLog ( 3, "  control type = kIOAudioControlTypeSelector" );
			debugIOLog (3, "  input selector change handler");
			switch (control->getSubType ()) {
				case kIOAudioSelectorControlSubTypeInput:
					result = audioDevice->inputSelectorChanged (newValue);
					break;
				case kIOAudioSelectorControlSubTypeClockSource:
					result = audioDevice->clockSelectorChanged (newValue);
					break;
				default:
					debugIOLog (3, "  unknown control type in input selector change handler");
					break;
			}		
		default:
			break;
	}

Exit:
	// Second half of [2981190], put us back to sleep after the right amount of time if we were off
	if (TRUE == wasPoweredDown) {
		audioDevice->setTimerForSleep ();
	}
	if ( audioDevice ) {
		debugIOLog (3, "- AppleOnboardAudio[%ld]::inputControlChangeHandler (%p, %p, oldValue %lX, newValue %lX) returns %lX", audioDevice->mInstanceIndex, target, control, oldValue, newValue, result);
	} else {
		debugIOLog (3, "- AppleOnboardAudio::inputControlChangeHandler (%p, %p, oldValue %lX, newValue %lX) returns %lX", target, control, oldValue, newValue, result);
	}
	return result;
}

IOReturn AppleOnboardAudio::gainLeftChanged (SInt32 newValue) {
	IOReturn							result;

    debugIOLog (3, "+ AppleOnboardAudio[%ld]::gainLeftChanged", mInstanceIndex);

	if (mCurrentPluginNeedsSoftwareInputGain) {
		mDriverDMAEngine->setInputGainL (newValue);
	} else {
		mCurrentInputPlugin->setInputGain (newValue, mGainRight);
	}
    mGainLeft = newValue;

	result = kIOReturnSuccess;

    debugIOLog (3, "- AppleOnboardAudio[%ld]::gainLeftChanged, %d", mInstanceIndex, (result == kIOReturnSuccess));
    return result;
}

IOReturn AppleOnboardAudio::gainRightChanged (SInt32 newValue) {
	IOReturn							result;

    debugIOLog (3, "+ AppleOnboardAudio[%ld]::gainRightChanged", mInstanceIndex);

	if (mCurrentPluginNeedsSoftwareInputGain) {
		mDriverDMAEngine->setInputGainR (newValue);
	} else {
		mCurrentInputPlugin->setInputGain (newValue, mGainRight);
	}
    mGainRight = newValue;

	result = kIOReturnSuccess;

    debugIOLog (3, "- AppleOnboardAudio[%ld]::gainRightChanged, %d", mInstanceIndex, (result == kIOReturnSuccess));
    return result;
}

IOReturn AppleOnboardAudio::gainMasterChanged (SInt32 newValue) {
	IOReturn							result;

    debugIOLog (3, "+ AppleOnboardAudio[%ld]::gainMasterChanged", mInstanceIndex);

	if (mCurrentPluginNeedsSoftwareInputGain) {
		mDriverDMAEngine->setInputGainR (newValue);
		mDriverDMAEngine->setInputGainL (newValue);
	} else {
		mCurrentInputPlugin->setInputGain (newValue, newValue);
	}
	result = kIOReturnSuccess;

    debugIOLog (3, "- AppleOnboardAudio[%ld]::gainMasterChanged, %d", mInstanceIndex, (result == kIOReturnSuccess));
    return result;
}

IOReturn AppleOnboardAudio::passThruChanged (SInt32 newValue) {
	IOReturn							result;

    debugIOLog (3, "+ AppleOnboardAudio[%ld]::passThruChanged", mInstanceIndex);

    result = kIOReturnError;

	mCurrentInputPlugin->setPlayThrough (!newValue);

	result = kIOReturnSuccess;

    debugIOLog (3, "- AppleOnboardAudio[%ld]::passThruChanged", mInstanceIndex);
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

	debugIOLog (3, "+ AppleOnboardAudio[%ld]::inputSelectorChanged (%4s)", mInstanceIndex, (char *)&newValue);

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

	setSoftwareInputDSP (connectionString);	// [3306305]
	
	// [3250612], fix update logic regarding current input plugin	
	mCurrentInputPlugin->setInputMute (TRUE);

	if (mCurrentInputPlugin != thePluginObject) {

		thePluginObject->setInputMute (TRUE);

		// in future this may need to be on a per input basis (which would move this out of this if statement)
		cacheInputGainLevels (mCurrentInputPlugin);
		
		debugIOLog ( 5, "** AppleOnboardAudio[%ld]::inputSelectorChanged invoking 'ConfigChangeHelper'", mInstanceIndex );
		ConfigChangeHelper theConfigeChangeHelper(mDriverDMAEngine);	
	
		setInputDataMuxForConnection ( connectionString );
			
		// in future may need to update this based on individual inputs, not the part as a whole
		AOApropInput = OSDynamicCast (OSDictionary, thePluginObject->getProperty (kPluginPListAOAAttributes));
		if (NULL != AOApropInput) {
			softwareInputGainBoolean = OSDynamicCast (OSBoolean, AOApropInput->getObject (kPluginPListSoftwareInputGain));
			if (NULL != softwareInputGainBoolean) {
				mDriverDMAEngine->setUseSoftwareInputGain (softwareInputGainBoolean->getValue ());
				mCurrentPluginNeedsSoftwareInputGain = softwareInputGainBoolean->getValue ();
			} else {
				mDriverDMAEngine->setUseSoftwareInputGain (false);
				mCurrentPluginNeedsSoftwareInputGain = false;
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
		debugIOLog (3, "+ AppleOnboardAudio[%ld]::inputSelectorChanged - mCurrentInputPlugin updated to %p", mInstanceIndex, mCurrentInputPlugin);
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
	debugIOLog (3, "- AppleOnboardAudio[%ld]::inputSelectorChanged", mInstanceIndex);
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
	IOReturn			result;
	UInt32				transportType;  //  [3655075]

	debugIOLog (5, "+ AppleOnboardAudio[%ld]::clockSelectorChanged ('%4s')", mInstanceIndex, (char *)&newValue);

	result = kIOReturnError;

	mClockSelectInProcessSemaphore = true;	//	block 'UNLOCK' errors while switching clock sources
	
	FailIf ( NULL == mDriverDMAEngine, Exit );
	FailIf ( NULL == mTransportInterface, Exit );
	
	debugIOLog ( 5, "  mCodecLockStatus = %s", kClockLockStatus == mCodecLockStatus ? "kClockLockStatus" : "kClockUnlockStatus" );
	
	transportType = mTransportInterface->transportGetTransportInterfaceType();							//  [3655075]
	if ( kTransportInterfaceType_I2S_Slave_Only != transportType ) {									//  [3628559]   Dont attempt to change clocks on a slave only device
		if ( ( kClockSourceSelectionExternal == newValue ) || ( kClockSourceSelectionInternal == newValue ) ) {
		
			//  [3678605]   If the transport clock selection does not match the control then force an update!
			switch ( newValue ) {
				case kClockSourceSelectionExternal:
					if ( kTRANSPORT_SLAVE_CLOCK != mTransportInterface->transportGetClockSelect () ) {
						debugIOLog ( 4, "  *-*** updating mCurrentClockSelector from 0x%0.8X to 0x%0.8X to FORCE UPDATE to kClockSourceSelectionExternal", mCurrentClockSelector, kClockSourceSelectionInternal );
						mCurrentClockSelector = kClockSourceSelectionInternal;
					}
					break;
				case kClockSourceSelectionInternal:
					if ( kTRANSPORT_MASTER_CLOCK != mTransportInterface->transportGetClockSelect () ) {
						debugIOLog ( 4, "  ***-* updating mCurrentClockSelector from 0x%0.8X to 0x%0.8X to FORCE UPDATE to kClockSourceSelectionInternal", mCurrentClockSelector, kClockSourceSelectionExternal );
						mCurrentClockSelector = kClockSourceSelectionExternal;
					}
					break;
			}
			
			if ( mCurrentClockSelector != newValue ) {
				callPluginsInOrder ( kSetMuteState, TRUE );												//	[3435307],[3253678], mute outputs during clock selection
				muteAllAmps ();																			//  [3684994]
				if ( kClockSourceSelectionInternal == newValue ) {
					debugIOLog ( 5, "  ** AppleOnboardAudio[%ld]::clockSelectorChanged invoking 'ConfigChangeHelper' for kClockSourceSelectionInternal", mInstanceIndex );
					ConfigChangeHelper theConfigeChangeHelper(mDriverDMAEngine, 10);					//  pauses the DBDMA engine

					if ( kTransportInterfaceType_I2S_Opaque_Slave_Only == transportType ) {				//  [3655075]   begin {
						mTransportInterface->transportBreakClockSelect ( kTRANSPORT_SLAVE_CLOCK );
						callPluginsInOrder ( kBreakClockSelect, kTRANSPORT_MASTER_CLOCK );
						
						mTransportInterface->transportMakeClockSelect ( kTRANSPORT_SLAVE_CLOCK );
						callPluginsInOrder ( kMakeClockSelect, kTRANSPORT_MASTER_CLOCK );
					} else {
						mTransportInterface->transportBreakClockSelect ( kTRANSPORT_MASTER_CLOCK );
						callPluginsInOrder ( kBreakClockSelect, kTRANSPORT_MASTER_CLOCK );
						
						mTransportInterface->transportMakeClockSelect ( kTRANSPORT_MASTER_CLOCK );
						callPluginsInOrder ( kMakeClockSelect, kTRANSPORT_MASTER_CLOCK );
					}																					//  }   end		[3655075]
					
					selectCodecOutputWithMuteState (mIsMute);
					if ( NULL != mOutputSelector ) {
						selectOutputAmplifiers ( mOutputSelector->getIntValue (), mIsMute);
					}
					//  The 'ConfigChangeHelper' object goes out of scope here which resumes the DMA operation...
				} else if ( kClockSourceSelectionExternal == newValue ) {
					debugIOLog ( 5, "  ** AppleOnboardAudio[%ld]::clockSelectorChanged invoking 'ConfigChangeHelper' for kClockSourceSelectionExternal", mInstanceIndex );
					ConfigChangeHelper theConfigeChangeHelper(mDriverDMAEngine, 10);					//  pauses the DBDMA engine
					mTransportInterface->transportBreakClockSelect ( kTRANSPORT_SLAVE_CLOCK );
					callPluginsInOrder ( kBreakClockSelect, kTRANSPORT_SLAVE_CLOCK );
					
					mTransportInterface->transportMakeClockSelect ( kTRANSPORT_SLAVE_CLOCK );
					callPluginsInOrder ( kMakeClockSelect, kTRANSPORT_SLAVE_CLOCK );

					selectCodecOutputWithMuteState (mIsMute);
					if ( NULL != mOutputSelector ) {
						selectOutputAmplifiers ( mOutputSelector->getIntValue (), mIsMute);
					}
					//  The 'ConfigChangeHelper' object goes out of scope here which resumes the DMA operation...
				} else {
					debugIOLog (3,  "  ** Unknown clock source selection." );
					FailIf (TRUE, Exit);
				}
				debugIOLog ( 4, "  ***** updating mCurrentClockSelector from 0x%0.8X to 0x%0.8X", mCurrentClockSelector, newValue );
				mCurrentClockSelector = newValue;
				debugIOLog ( 4, "  *-* about to mDriverDMAEngine->hardwareSampleRateChanged ( %d )", mTransportSampleRate.whole );	//  [3686032]
				mDriverDMAEngine->hardwareSampleRateChanged ( &mTransportSampleRate );
			}
			result = kIOReturnSuccess;
		}
	}
Exit:
	mClockSelectInProcessSemaphore = false;	//	enable 'UNLOCK' errors after switching clock sources
	debugIOLog (5, "- AppleOnboardAudio[%ld]::clockSelectorChanged ('%4s')", mInstanceIndex, (char *)&newValue);
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
		debugIOLog (3,  "AppleOnboardAudio[%ld]::setInputDataMuxForConnection setting input data mux to %d", mInstanceIndex, (unsigned int)theMuxSelect );
		mPlatformInterface->setInputDataMux ( theMuxSelect );
	}
}


#pragma mark +POWER MANAGEMENT
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AppleOnboardAudio::setTimerForSleep () {
    AbsoluteTime				fireTime;
    UInt64						nanos;

	debugIOLog ( 3, "+ AppleOnboardAudio::setTimerForSleep ()" );
	if ( ( NULL != idleTimer ) && ( idleSleepDelayTime != kNoIdleAudioPowerDown ) ) {
		debugIOLog ( 3, "  Will set timer for sleep!" );
		clock_get_uptime (&fireTime);
		absolutetime_to_nanoseconds (fireTime, &nanos);
		nanos += idleSleepDelayTime;
		nanoseconds_to_absolutetime (nanos, &fireTime);
		idleTimer->wakeAtTime (fireTime);								// will call sleepHandlerTimer
	}
	debugIOLog ( 3, "- AppleOnboardAudio::setTimerForSleep ()" );
}

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AppleOnboardAudio::sleepHandlerTimer (OSObject *owner, IOTimerEventSource *sender) {
	AppleOnboardAudio *				audioDevice;
	UInt32							time = 0;
	IOAudioDevicePowerState			currentPowerState;

	debugIOLog ( 3, "+ AppleOnboardAudio::sleepHandlerTimer ( %p, %p )", owner, sender );
	audioDevice = OSDynamicCast (AppleOnboardAudio, owner);
	FailIf (NULL == audioDevice, Exit);
	currentPowerState = audioDevice->getPowerState ();
	debugIOLog ( 3, "  audioDevice->getPowerState () = %ld", currentPowerState );
	if ( currentPowerState == kIOAudioDeviceActive ) {		//  [3690065]   Need to idle sleep and not idle wake1
		audioDevice->performPowerStateChange (audioDevice->getPowerState (), kIOAudioDeviceIdle, &time);
	}

Exit:
	debugIOLog ( 3, "- AppleOnboardAudio::sleepHandlerTimer ( %p, %p )", owner, sender );
	return;
}


//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Have to call super::setAggressiveness to complete the function call
IOReturn AppleOnboardAudio::setAggressiveness (unsigned long type, unsigned long newLevel) {
	UInt32					time = 0;

	if (type == kPMPowerSource) {
		debugIOLog ( 3, "+ AppleOnboardAudio::setAggressiveness ( 0x%0.8X = kPMPowerSource, %ld )", type, newLevel );
		switch (newLevel) {
			case kIOPMInternalPower:								// Running on battery only
				debugIOLog ( 3, "  setting power aggressivness state to kIOPMInternalPower" );
#ifdef k3699908_FIX
				idleSleepDelayTime = kBatteryPowerDownDelayTime;
				debugIOLog ( 3, "  running on battery power, idleSleepDelayTime = %ld = kBatteryPowerDownDelayTime", idleSleepDelayTime );
				setIdleAudioSleepTime (idleSleepDelayTime);
				if (getPowerState () != kIOAudioDeviceActive) {
					performPowerStateChange (getPowerState (), kIOAudioDeviceIdle, &time);
				}
#else
				idleSleepDelayTime = kNoIdleAudioPowerDown;
				debugIOLog ( 3, "  running on battery power, idleSleepDelayTime = %ld = kNoIdleAudioPowerDown", idleSleepDelayTime );
				setIdleAudioSleepTime (idleSleepDelayTime);			// don't tell us about going to the idle state
				if (getPowerState () != kIOAudioDeviceActive) {
					performPowerStateChange (getPowerState (), kIOAudioDeviceActive, &time);
				}
#endif
				break;
			case kIOPMExternalPower:								// Running on AC power
				debugIOLog ( 3, "  setting power aggressivness state to kIOPMExternalPower" );
				// idleSleepDelayTime = kACPowerDownDelayTime;		// idle power down after 5 minutes
				idleSleepDelayTime = kNoIdleAudioPowerDown;
				debugIOLog ( 3, "  running on wall power, idleSleepDelayTime = %ld = kNoIdleAudioPowerDown", idleSleepDelayTime );
				setIdleAudioSleepTime (idleSleepDelayTime);			// don't tell us about going to the idle state
				if (getPowerState () != kIOAudioDeviceActive) {
					performPowerStateChange (getPowerState (), kIOAudioDeviceActive, &time);
				}
				break;
			default:
				debugIOLog ( 3, "  setting power aggressivness state to %ld", newLevel );
				break;
		}
		debugIOLog ( 3, "- AppleOnboardAudio::setAggressiveness ( 0x%0.8X = kPMPowerSource, %ld )", type, newLevel );
	}
	return super::setAggressiveness (type, newLevel);
}


//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleOnboardAudio::performPowerStateChange (IOAudioDevicePowerState oldPowerState, IOAudioDevicePowerState newPowerState, UInt32 *microsecondsUntilComplete)
{
	IOReturn					result;

	debugIOLog (3, "+ AppleOnboardAudio[%ld]::performPowerStateChange (%d, %d) -- ourPowerState = %d", mInstanceIndex, oldPowerState, newPowerState, ourPowerState);

	*microsecondsUntilComplete = 2000000;

	result = super::performPowerStateChange (oldPowerState, newPowerState, microsecondsUntilComplete);
    
	if ( kIOAudioDeviceIdle == ourPowerState && kIOAudioDeviceActive == newPowerState ) {
		result = performPowerStateChangeThreadAction ( this, (void*)newPowerState, 0, 0, 0 );
	} else {
		if (mPowerThread) {
			thread_call_enter1(mPowerThread, (thread_call_param_t)newPowerState);
		}
	}

	debugIOLog (3, "- AppleOnboardAudio[%ld]::performPowerStateChange -- ourPowerState = %d", mInstanceIndex, ourPowerState);

	return result; 
}


//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AppleOnboardAudio::performPowerStateChangeThread (AppleOnboardAudio * aoa, void * newPowerState) {
	IOCommandGate *						cg;
	IOReturn							result;

	FailIf (NULL == aoa, Exit);
	debugIOLog (3, "+ AppleOnboardAudio[%ld]::performPowerStateChangeThread (%p, %ld)", aoa->mInstanceIndex, aoa, (UInt32)newPowerState);

	aoa->mSignal->wait (FALSE);

	FailWithAction (TRUE == aoa->mTerminating, aoa->completePowerStateChange (), Exit);	
	cg = aoa->getCommandGate ();
	if (cg) {
		result = cg->runAction (aoa->performPowerStateChangeThreadAction, newPowerState, (void *)aoa);
	}

Exit:
	debugIOLog (3, "- AppleOnboardAudio[%ld]::performPowerStateChangeThread (%p, %ld)", aoa->mInstanceIndex, aoa, (UInt32)newPowerState);
	return;
}


//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  [3515371]   Changed 'performPowerStateChangeThreadAction' to only invoke power management on an
//				AppleOnboardAudio instance that is lowest in priority of multiple instances.  If this
//				instance is lowest in priority then the higher priority instances will have power
//				management requests invoked from this instance and the higher priority instances will
//				not apply power management to the underlying hardware as a result of power managment
//				requests invoked from IOAudioFamily.  This is to support power managment dependencies
//				between AppleOnboardAudio instances.  For example, if AppleOnboardAudio instance 1
//				can operate with a clock provided by AppleOnboardAudio instance 2 then AppleOnboardAudio
//				instance 1 will operate as the lower priority instance and will invoke a request to
//				perform sleep upon itself prior to invoking sleep upon the higher priority
//				AppleOnboardAudio instance 2 so that AppleOnboardAudio instance 1 can redirect to an
//				internal clock source prior to experiencing the loss of clock from AppleOnboardAudio
//				instance 2 when AppleOnboardAudio instance 2 goes to sleep.  Conversely AppleOnboardAudio
//				instance 1 will invoke wake upon the higher priority AppleOnboardAudio instance 2 prior
//				to performing wake upon AppleOnboardAudio instance 1 hardware in order to establish the
//				clock source from AppleOnboardAudio 2 so that AppleOnboardAudio instance 1 can restore
//				an external clock source selection upon wake.
IOReturn AppleOnboardAudio::performPowerStateChangeThreadAction (OSObject * owner, void * newPowerState, void * us, void * arg3, void * arg4) {
	AppleOnboardAudio *					aoa;
	IOReturn							result;

	result = kIOReturnError;

	aoa = OSDynamicCast (AppleOnboardAudio, (OSObject *)us);
	FailIf (NULL == aoa, Exit);

//	kprintf ("+AppleOnboardAudio::performPowerStateChangeThreadAction (%p, %ld), %d", owner, (UInt32)newPowerState, aoa->ourPowerState);
	debugIOLog (3, "+ AppleOnboardAudio[%ld]::performPowerStateChangeThreadAction (%p, %ld) -- ourPowerState = %d", aoa->mInstanceIndex, owner, (UInt32)newPowerState, aoa->ourPowerState);

	FailIf (NULL == aoa->mTransportInterface, Exit);
	FailIf (NULL == aoa->mPlatformInterface, Exit);														//	[3581695]	12 Mar 2004, rbm

	//  [3515371]   If this AOA instance power managment requests are received from another
	//				AOA instance then indicate success to IOAudioFamily and let the actual
	//				hardware manipulations associated with power management occur under the
	//				control of the controlling AOA instance.
	if ( 0 != aoa->mUsesAOAPowerManagement ) {
		debugIOLog ( 3, "  WARNING:  This AppleOnboardAudio[%ld] instance only applies power managment to hardware under direction of another AppleOnboardAudio instance.", aoa->mInstanceIndex );
		result = kIOReturnSuccess;
	} else {
		debugIOLog ( 3, "  This AppleOnboardAudio[%ld] instance  is about to invoke power managment under direction of IOAudioFamily", aoa->mInstanceIndex );
		aoa->performPowerStateChangeAction ( newPowerState );
	}
	
Exit:
	if (NULL != aoa) {
		aoa->protectedCompletePowerStateChange ();
	}
//	kprintf ("-AppleOnboardAudio::performPowerStateChangeThreadAction (%p, %ld), %d", owner, (UInt32)newPowerState, aoa->ourPowerState);
	debugIOLog (3, "- AppleOnboardAudio[%ld]::performPowerStateChangeThreadAction (%p, %ld) -- ourPowerState = %d", aoa->mInstanceIndex, owner, (UInt32)newPowerState, aoa->ourPowerState);
	return result;
}


//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  [3515371]   This AOA instance must determine if it is the power managment controller for
//				another AOA instance by testing the 'SleepsAOALayoutIDInstance' property value.
//				If this instance is a power managment controller for another instance then it
//				must issue sleep requests to the other instance after sleeping this instance 
//				and must issue wake requests to the other instance prior to waking this instance.   rbm
IOReturn AppleOnboardAudio::performPowerStateChangeAction ( void * newPowerState ) {
	IOReturn			result;

	switch ( (UInt32)newPowerState ) {
		case kIOAudioDeviceActive:
			switch ( ourPowerState ) {
				case kIOAudioDeviceActive:
					debugIOLog (3, "+ AppleOnboardAudio[%ld]::performPowerStateChangeAction ( %ld = kIOAudioDeviceActive ), ourPowerState = %d = kIOAudioDeviceActive", mInstanceIndex, (UInt32)newPowerState, ourPowerState);
					break;
				case kIOAudioDeviceIdle:
					debugIOLog (3, "+ AppleOnboardAudio[%ld]::performPowerStateChangeAction ( %ld = kIOAudioDeviceActive ), ourPowerState = %d = kIOAudioDeviceIdle", mInstanceIndex, (UInt32)newPowerState, ourPowerState);
					break;
				case kIOAudioDeviceSleep:
					debugIOLog (3, "+ AppleOnboardAudio[%ld]::performPowerStateChangeAction ( %ld = kIOAudioDeviceActive ), ourPowerState = %d = kIOAudioDeviceSleep", mInstanceIndex, (UInt32)newPowerState, ourPowerState);
					break;
				default:
					debugIOLog (3, "+ AppleOnboardAudio[%ld]::performPowerStateChangeAction ( %ld = kIOAudioDeviceActive ), ourPowerState = %d", mInstanceIndex, (UInt32)newPowerState, ourPowerState);
					break;
			}
			break;
		case kIOAudioDeviceIdle:
			switch ( ourPowerState ) {
				case kIOAudioDeviceActive:
					debugIOLog (3, "+ AppleOnboardAudio[%ld]::performPowerStateChangeAction ( %ld = kIOAudioDeviceIdle ), ourPowerState = %d = kIOAudioDeviceActive", mInstanceIndex, (UInt32)newPowerState, ourPowerState);
					break;
				case kIOAudioDeviceIdle:
					debugIOLog (3, "+ AppleOnboardAudio[%ld]::performPowerStateChangeAction ( %ld = kIOAudioDeviceIdle ), ourPowerState = %d = kIOAudioDeviceIdle", mInstanceIndex, (UInt32)newPowerState, ourPowerState);
					break;
				case kIOAudioDeviceSleep:
					debugIOLog (3, "+ AppleOnboardAudio[%ld]::performPowerStateChangeAction ( %ld = kIOAudioDeviceIdle ), ourPowerState = %d = kIOAudioDeviceSleep", mInstanceIndex, (UInt32)newPowerState, ourPowerState);
					break;
				default:
					debugIOLog (3, "+ AppleOnboardAudio[%ld]::performPowerStateChangeAction ( %ld = kIOAudioDeviceIdle ), ourPowerState = %d", mInstanceIndex, (UInt32)newPowerState, ourPowerState);
					break;
			}
			break;
		case kIOAudioDeviceSleep:
			switch ( ourPowerState ) {
				case kIOAudioDeviceActive:
					debugIOLog (3, "+ AppleOnboardAudio[%ld]::performPowerStateChangeAction ( %ld = kIOAudioDeviceSleep ), ourPowerState = %d = kIOAudioDeviceActive", mInstanceIndex, (UInt32)newPowerState, ourPowerState);
					break;
				case kIOAudioDeviceIdle:
					debugIOLog (3, "+ AppleOnboardAudio[%ld]::performPowerStateChangeAction ( %ld = kIOAudioDeviceSleep ), ourPowerState = %d = kIOAudioDeviceIdle", mInstanceIndex, (UInt32)newPowerState, ourPowerState);
					break;
				case kIOAudioDeviceSleep:
					debugIOLog (3, "+ AppleOnboardAudio[%ld]::performPowerStateChangeAction ( %ld = kIOAudioDeviceSleep ), ourPowerState = %d = kIOAudioDeviceSleep", mInstanceIndex, (UInt32)newPowerState, ourPowerState);
					break;
				default:
					debugIOLog (3, "+ AppleOnboardAudio[%ld]::performPowerStateChangeAction ( %ld = kIOAudioDeviceSleep ), ourPowerState = %d", mInstanceIndex, (UInt32)newPowerState, ourPowerState);
					break;
			}
			break;
		default:
			switch ( ourPowerState ) {
				case kIOAudioDeviceActive:
					debugIOLog (3, "+ AppleOnboardAudio[%ld]::performPowerStateChangeAction ( %ld ), ourPowerState = %d = kIOAudioDeviceActive", mInstanceIndex, (UInt32)newPowerState, ourPowerState);
					break;
				case kIOAudioDeviceIdle:
					debugIOLog (3, "+ AppleOnboardAudio[%ld]::performPowerStateChangeAction ( %ld ), ourPowerState = %d = kIOAudioDeviceIdle", mInstanceIndex, (UInt32)newPowerState, ourPowerState);
					break;
				case kIOAudioDeviceSleep:
					debugIOLog (3, "+ AppleOnboardAudio[%ld]::performPowerStateChangeAction ( %ld ), ourPowerState = %d = kIOAudioDeviceSleep", mInstanceIndex, (UInt32)newPowerState, ourPowerState);
					break;
				default:
					debugIOLog (3, "+ AppleOnboardAudio[%ld]::performPowerStateChangeAction ( %ld ), ourPowerState = %d", mInstanceIndex, (UInt32)newPowerState, ourPowerState);
					break;
			}
			break;
	}

	result = kIOReturnError;

	FailIf (NULL == mTransportInterface, Exit);
	FailIf (NULL == mPlatformInterface, Exit);												//	[3581695]	12 Mar 2004, rbm
	
	mNeedsLockStatusUpdateToUnmute = FALSE;													//  [3678605]

	switch ((UInt32)newPowerState) {
		case kIOAudioDeviceSleep:
			debugIOLog ( 3, "  about to stop the 'pollTimer'" );
			pollTimer->cancelTimeout ();													//  [3515371]
			//	Sleep requires that the hardware plugin object(s) go to sleep
			//	prior to the transport object going to sleep.
			if (kIOAudioDeviceActive == ourPowerState) {
				debugIOLog (3, "  active to sleep power change calling outputMuteChange(TRUE)");

				if (mMuteAmpWhenClockInterrupted) {
					muteAllAmps();
				}

				selectCodecOutputWithMuteState (TRUE);
				
				if (!mMuteAmpWhenClockInterrupted) {
					muteAllAmps();
				}
				
				//	[3678605]	begin	{
				//  Switch this instance to internal clock and then send request
				//  to sleep to other instance prior to puting this instance's
				//  transport object to sleep so that the MCLK signal from this
				//  instance's I2S IOM is present for any hardware attached to
				//  the other instance's I2S IOM.
				if ( NULL != mTransportInterface ) {
					if ( kClockSourceSelectionExternal == mCurrentClockSelector ) {
						clockSelectorChanged ( kClockSourceSelectionInternal );
#ifdef kDoNotForceInternalClockOnSleep													/*  [3686032]   */
						debugIOLog ( 4, "  *---* updating mCurrentClockSelector from 0x%0.8X to 0x%0.8X = kClockSourceSelectionExternal", mCurrentClockSelector, kClockSourceSelectionExternal );
						mCurrentClockSelector = kClockSourceSelectionExternal;
#else																					/*  [3686032]   */
					mExternalClockSelector->setValue ( OSNumber::withNumber (kClockSourceSelectionInternal, 32) );
#endif																					/*  [3686032]   */
					}
				}

				result = callPluginsInReverseOrder (kPowerStateChange, kIOAudioDeviceSleep);
				FailMessage ( kIOReturnSuccess != result );
				if ( kIOReturnSuccess != result ) {
					debugIOLog ( 3, "  callPluginsInReverseOrder (kPowerStateChange, kIOAudioDeviceSleep) FAILED" );
				}

				broadcastSoftwareInterruptMessage ( kRemoteSleep );							//  [3515371]
				//  }   end		[3678605]
				
				if ( NULL != mTransportInterface ) { 
					result = mTransportInterface->performTransportSleep ();
				}

				//	[3453989]	begin	{
				if ( NULL != mPlatformInterface ) { 
					mPlatformInterface->performPlatformSleep ();
				}
				//	}	end	[3453989]
				ourPowerState = kIOAudioDeviceSleep;
			}
			break;
		case kIOAudioDeviceIdle:
			debugIOLog ( 3, "  idleSleepDelayTime %ld", idleSleepDelayTime );
			if (kIOAudioDeviceActive == ourPowerState) {
				debugIOLog ( 3, "  about to stop the 'pollTimer'" );
				pollTimer->cancelTimeout ();												//  [3515371]
				debugIOLog (3, "  active to idle power change calling outputMuteChange(TRUE)");

				selectCodecOutputWithMuteState (TRUE);

				//	Going idle from wake requires that the hardware plugin object(s) go to sleep
				//	prior to the transport object going to sleep.
				result = callPluginsInReverseOrder (kPowerStateChange, kIOAudioDeviceSleep);
				FailMessage ( kIOReturnSuccess != result );
				if ( kIOReturnSuccess != result ) {
					debugIOLog ( 3, "  callPluginsInReverseOrder (kPowerStateChange, kIOAudioDeviceSleep) FAILED" );
				}
				
				//	[3678605]	begin	{
				//  Switch this instance to internal clock and then send request
				//  to sleep to other instance prior to puting this instance's
				//  transport object to sleep so that the MCLK signal from this
				//  instance's I2S IOM is present for any hardware attached to
				//  the other instance's I2S IOM.
				if ( NULL != mTransportInterface ) {
					if ( kClockSourceSelectionExternal == mCurrentClockSelector ) {
						clockSelectorChanged ( kClockSourceSelectionInternal );
#ifdef kDoNotForceInternalClockOnSleep													/*  [3686032]   */
						debugIOLog ( 4, "  **-** updating mCurrentClockSelector from 0x%0.8X to 0x%0.8X= kClockSourceSelectionExternal", mCurrentClockSelector, kClockSourceSelectionExternal );
						mCurrentClockSelector = kClockSourceSelectionExternal;
#else																					/*  [3686032]   */
					mExternalClockSelector->setValue ( OSNumber::withNumber (kClockSourceSelectionInternal, 32) );
#endif																					/*  [3686032]   */
					}
				}
				broadcastSoftwareInterruptMessage ( kRemoteSleep );							//  [3515371]
				//  }   end		[3678605]
				
				if ( NULL != mTransportInterface ) { 
					result = mTransportInterface->performTransportSleep ();
					FailMessage ( kIOReturnSuccess != result );
					if ( kIOReturnSuccess != result ) {
						debugIOLog ( 3, "  mTransportInterface->performTransportSleep () FAILED" );
					}
				}
				ourPowerState = kIOAudioDeviceSleep;
				//  [3515371]   When going 'Idle' 'Sleep' then perform remote sleep last so that an external clock source will be removed after it is no longer used
			} else if (kIOAudioDeviceSleep == ourPowerState && kNoIdleAudioPowerDown == idleSleepDelayTime) {
				//	Wake to idle from sleep requires that the transport object go active prior
				//	to the hardware plugin object(s) going active.
				if ( NULL != mTransportInterface ) {
					result = mTransportInterface->performTransportWake ();
					FailMessage ( kIOReturnSuccess != result );
					if ( kIOReturnSuccess != result ) {
						debugIOLog ( 3, "  mTransportInterface->performTransportWake () FAILED" );
					}
				}
				//  [3515371][3678605]   When going 'Idle' 'Active' then perform remote wakeup immediately 
				//  after waking this instance's transport interface object but prior to performing any
				//  other 'Active' tasks so that another AppleOnboardAudio instance can be made available
				//  as a clock source while making sure that the MCLK signal from this instance's I2S IOM
				//  is available to hardware associated with the remote AppleOnboardAudio instance.
				if ( broadcastSoftwareInterruptMessage ( kRemoteActive ) ) {				//  [3515371]
					//  If going active and slaved to external clock then defer unmuting the amplifiers
					//  until the AppleOnboardAudio instance that is providing the clock source has indicated
					//  that the clock source is either available or unavailable.  This is done to avoid pops
					//  and clicks during wake from sleep.  [3678605]
					if ( mCurrentClockSelector == kClockSourceSelectionExternal ) {
						mNeedsLockStatusUpdateToUnmute = TRUE;
					}
				}
				result = callPluginsInOrder (kPowerStateChange, kIOAudioDeviceActive);
				FailMessage ( kIOReturnSuccess != result );
				if ( kIOReturnSuccess != result ) {
					debugIOLog ( 3, "  callPluginsInOrder (kPowerStateChange, kIOAudioDeviceActive) FAILED" );
				}
				
				//  On transport objects that support 'Slave Only' then it is necessary to perform a post
				//  wake request so that the transport can be enabled.  This is necessary because the slave
				//  only instance will not respond to the flushing of the clock selector which implies
				//  enabling the transport.
				if ( NULL != mTransportInterface ) {
					mTransportInterface->performTransportPostWake ();						//  [3515371]
				}
				
				if ( NULL != mExternalClockSelector ) {
					debugIOLog ( 4, "  ... [%ld] about to mExternalClockSelector->flushValue ()", mInstanceIndex );
					mExternalClockSelector->flushValue ();
				}
				if ( kIOAudioDeviceActive != ourPowerState ) {
					debugIOLog ( 5, "  AppleOnboardAudio[%ld]::performPowerStateChangeThreadAction 1 setting 'ourPowerState' to kIOAudioDeviceActive", mInstanceIndex );
				}
				mDelayPollAfterWakeFromSleep = kDelayPollAfterWakeFromSleep;				//  [3686032]
				ourPowerState = kIOAudioDeviceActive;
				//	[3453989]	begin	{
				//		Unwind [3453989]:  'performPlatformSleep()' will unregister interrupts and that is not
				//							desired on idle sleep for portables.  Don't call performPlatformSleep
				//							when going idle.  Further, don't call performPlatformWake when going
				//							to wake state from idle as that would invoke redundant registration 
				//							of the platform interface interrupt handlers.
				if ( NULL != mPlatformInterface ) { 
					mPlatformInterface->performPlatformWake ( (IOService*)mPlatformInterface );
				}
				//	}	end	[3453989]

				if ( !mNeedsLockStatusUpdateToUnmute ) {									//  [3678605]
					selectCodecOutputWithMuteState (mIsMute);

					if ( NULL != mOutputSelector ) {
						selectOutputAmplifiers (mOutputSelector->getIntValue (), mIsMute);	//	Radar 3416318:	mOutMuteControl does not touch GPIOs so do so here!
					}
					if (NULL != mOutMuteControl) {
						mOutMuteControl->flushValue ();					// Restore hardware to the user's selected state
					}
				}
				debugIOLog ( 3, "  about to restart the 'pollTimer'" );
				setPollTimer();																//  [3515371]
			} else {
				debugIOLog ( 3, "  wake will be deferred because power cord is removed" );  //  [3690065]
			}
			break;
		case kIOAudioDeviceActive:
			if (kIOAudioDeviceActive != ourPowerState) {
				//	Wake requires that the transport object go active prior
				//	to the hardware plugin object(s) going active.
				if ( NULL != mTransportInterface ) {
					result = mTransportInterface->performTransportWake ();
					FailMessage ( kIOReturnSuccess != result );
					if ( kIOReturnSuccess != result ) {
						debugIOLog ( 3, "  mTransportInterface->performTransportWake () FAILED" );
					}
				}
				
				//  [3515371][3678605]   When going 'Idle' 'Active' then perform remote wakeup immediately 
				//  after waking this instance's transport interface object but prior to performing any
				//  other 'Active' tasks so that another AppleOnboardAudio instance can be made available
				//  as a clock source while making sure that the MCLK signal from this instance's I2S IOM
				//  is available to hardware associated with the remote AppleOnboardAudio instance.
				if ( broadcastSoftwareInterruptMessage ( kRemoteActive ) ) {				//  [3515371]
					//  If going active and slaved to external clock then defer unmuting the amplifiers
					//  until the AppleOnboardAudio instance that is providing the clock source has indicated
					//  that the clock source is either available or unavailable.  This is done to avoid pops
					//  and clicks during wake from sleep.  [3678605]
					if ( mCurrentClockSelector == kClockSourceSelectionExternal ) {
						mNeedsLockStatusUpdateToUnmute = TRUE;
					}
				}

				result = callPluginsInOrder (kPowerStateChange, kIOAudioDeviceActive);
				FailMessage ( kIOReturnSuccess != result );
				if ( kIOReturnSuccess != result ) {
					debugIOLog ( 3, "  callPluginsInOrder (kPowerStateChange, kIOAudioDeviceActive) FAILED" );
				}
				debugIOLog ( 5, "  AppleOnboardAudio::performPowerStateChangeThreadAction 2 setting 'ourPowerState' to kIOAudioDeviceActive" );
				//	[3581563]	begin	{
				//		Unwind [3453989]:  'performPlatformSleep()' will unregister interrupts and that is not
				//							desired on idle sleep for portables.  Don't call performPlatformSleep
				//							when going idle.  Further, don't call performPlatformWake when going
				//							to wake state from idle as that would invoke redundant registration 
				//							of the platform interface interrupt handlers.
				//		[3453989]	begin	{
				mDelayPollAfterWakeFromSleep = kDelayPollAfterWakeFromSleep;				//  [3686032]
				if ( NULL != mPlatformInterface && ( kIOAudioDeviceSleep == ourPowerState ) ) { 
					ourPowerState = kIOAudioDeviceActive;		// Have to go active here because otherwise we won't notice jack changes that happened while sleeping
					mPlatformInterface->performPlatformWake ( (IOService*)mPlatformInterface );					//	[3585556]	performPlatformWake requires passing the platform interface object!
				} else {
					debugIOLog ( 5, "  Did not perform platform wake, mPlatformInterface = %d, ourPowerState = %d, kIOAudioDeviceSleep = %d", mPlatformInterface, ourPowerState, kIOAudioDeviceSleep );
					ourPowerState = kIOAudioDeviceActive;
				}
				//		}	end	[3453989]
				//	}	end	[3581563]

				if (NULL != mExternalClockSelector) {
					debugIOLog ( 4, "  ... [%ld] about to mExternalClockSelector->flushValue ()", mInstanceIndex );
					mExternalClockSelector->flushValue ();
				}

				if ( !mNeedsLockStatusUpdateToUnmute ) {									//  [3678605]
					selectCodecOutputWithMuteState (mIsMute);

					if ( NULL != mOutputSelector ) {
						selectOutputAmplifiers (mOutputSelector->getIntValue (), mIsMute);	//	Radar 3416318:	mOutMuteControl does not touch GPIOs so do so here!
					}
					if (NULL != mOutMuteControl) {
						mOutMuteControl->flushValue ();					// Restore hardware to the user's selected state
					}
				}
				debugIOLog ( 3, "  about to restart the 'pollTimer'" );
				setPollTimer();																//  [3515371]
			} else {
				debugIOLog (3, "  trying to wake, but we're already awake");
			}
			break;
		default:
			FailMessage ( kIOReturnSuccess != result );
			debugIOLog ( 3, "  Unknown power management request!" );
			break;
	}

	setProperty ("IOAudioPowerState", ourPowerState, 32);
	mUCState.ucPowerState = ourPowerState;
Exit:
	protectedCompletePowerStateChange ();

	switch ( (UInt32)newPowerState ) {
		case kIOAudioDeviceActive:
			switch ( ourPowerState ) {
				case kIOAudioDeviceActive:
					debugIOLog (3, "- AppleOnboardAudio[%ld]::performPowerStateChangeAction ( %ld = kIOAudioDeviceActive ), ourPowerState = %d = kIOAudioDeviceActive, returns 0x%0.8X", mInstanceIndex, (UInt32)newPowerState, ourPowerState, result);
					break;
				case kIOAudioDeviceIdle:
					debugIOLog (3, "- AppleOnboardAudio[%ld]::performPowerStateChangeAction ( %ld = kIOAudioDeviceActive ), ourPowerState = %d = kIOAudioDeviceIdle, returns 0x%0.8X", mInstanceIndex, (UInt32)newPowerState, ourPowerState, result);
					break;
				case kIOAudioDeviceSleep:
					debugIOLog (3, "- AppleOnboardAudio[%ld]::performPowerStateChangeAction ( %ld = kIOAudioDeviceActive ), ourPowerState = %d = kIOAudioDeviceSleep, returns 0x%0.8X", mInstanceIndex, (UInt32)newPowerState, ourPowerState, result);
					break;
				default:
					debugIOLog (3, "- AppleOnboardAudio[%ld]::performPowerStateChangeAction ( %ld = kIOAudioDeviceActive ), ourPowerState = %d, returns 0x%0.8X", mInstanceIndex, (UInt32)newPowerState, ourPowerState, result);
					break;
			}
			break;
		case kIOAudioDeviceIdle:
			switch ( ourPowerState ) {
				case kIOAudioDeviceActive:
					debugIOLog (3, "- AppleOnboardAudio[%ld]::performPowerStateChangeAction ( %ld = kIOAudioDeviceIdle ), ourPowerState = %d = kIOAudioDeviceActive, returns 0x%0.8X", mInstanceIndex, (UInt32)newPowerState, ourPowerState, result);
					break;
				case kIOAudioDeviceIdle:
					debugIOLog (3, "- AppleOnboardAudio[%ld]::performPowerStateChangeAction ( %ld = kIOAudioDeviceIdle ), ourPowerState = %d = kIOAudioDeviceIdle, returns 0x%0.8X", mInstanceIndex, (UInt32)newPowerState, ourPowerState, result);
					break;
				case kIOAudioDeviceSleep:
					debugIOLog (3, "- AppleOnboardAudio[%ld]::performPowerStateChangeAction ( %ld = kIOAudioDeviceIdle ), ourPowerState = %d = kIOAudioDeviceSleep, returns 0x%0.8X", mInstanceIndex, (UInt32)newPowerState, ourPowerState, result);
					break;
				default:
					debugIOLog (3, "- AppleOnboardAudio[%ld]::performPowerStateChangeAction ( %ld = kIOAudioDeviceIdle ), ourPowerState = %d, returns 0x%0.8X", mInstanceIndex, (UInt32)newPowerState, ourPowerState, result);
					break;
			}
			break;
		case kIOAudioDeviceSleep:
			switch ( ourPowerState ) {
				case kIOAudioDeviceActive:
					debugIOLog (3, "- AppleOnboardAudio[%ld]::performPowerStateChangeAction ( %ld = kIOAudioDeviceSleep ), ourPowerState = %d = kIOAudioDeviceActive, returns 0x%0.8X", mInstanceIndex, (UInt32)newPowerState, ourPowerState, result);
					break;
				case kIOAudioDeviceIdle:
					debugIOLog (3, "- AppleOnboardAudio[%ld]::performPowerStateChangeAction ( %ld = kIOAudioDeviceSleep ), ourPowerState = %d = kIOAudioDeviceIdle, returns 0x%0.8X", mInstanceIndex, (UInt32)newPowerState, ourPowerState, result);
					break;
				case kIOAudioDeviceSleep:
					debugIOLog (3, "- AppleOnboardAudio[%ld]::performPowerStateChangeAction ( %ld = kIOAudioDeviceSleep ), ourPowerState = %d = kIOAudioDeviceSleep, returns 0x%0.8X", mInstanceIndex, (UInt32)newPowerState, ourPowerState, result);
					break;
				default:
					debugIOLog (3, "- AppleOnboardAudio[%ld]::performPowerStateChangeAction ( %ld = kIOAudioDeviceSleep ), ourPowerState = %d, returns 0x%0.8X", mInstanceIndex, (UInt32)newPowerState, ourPowerState, result);
					break;
			}
			break;
		default:
			switch ( ourPowerState ) {
				case kIOAudioDeviceActive:
					debugIOLog (3, "- AppleOnboardAudio[%ld]::performPowerStateChangeAction ( %ld ), ourPowerState = %d = kIOAudioDeviceActive, returns 0x%0.8X", mInstanceIndex, (UInt32)newPowerState, ourPowerState, result);
					break;
				case kIOAudioDeviceIdle:
					debugIOLog (3, "- AppleOnboardAudio[%ld]::performPowerStateChangeAction ( %ld ), ourPowerState = %d = kIOAudioDeviceIdle, returns 0x%0.8X", mInstanceIndex, (UInt32)newPowerState, ourPowerState, result);
					break;
				case kIOAudioDeviceSleep:
					debugIOLog (3, "- AppleOnboardAudio[%ld]::performPowerStateChangeAction ( %ld ), ourPowerState = %d = kIOAudioDeviceSleep, returns 0x%0.8X", mInstanceIndex, (UInt32)newPowerState, ourPowerState, result);
					break;
				default:
					debugIOLog (3, "- AppleOnboardAudio[%ld]::performPowerStateChangeAction ( %ld ), ourPowerState = %d, returns 0x%0.8X", mInstanceIndex, (UInt32)newPowerState, ourPowerState, result);
					break;
			}
			break;
	}

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

			appleOnboardAudio->performPowerStateChangeThread ( appleOnboardAudio, (void*)kIOAudioDeviceSleep );		//	[3581695]	12 Mar 2004, rbm
			
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

	debugIOLog (3,  "+ AppleOnboardAudio[%ld]::VolumeToPRAMValue ( 0x%X, 0x%X )", mInstanceIndex, (unsigned int)inLeftVol, (unsigned int)inRightVol );
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
		debugIOLog (3,  "  ... leftVol = 0x%X, rightVol = 0x%X", (unsigned int)leftVol, (unsigned int)rightVol );
	
		if (NULL != mOutMasterVolumeControl) {
			volumeRange = (mOutMasterVolumeControl->getMaxValue () - mOutMasterVolumeControl->getMinValue () + 1);
			debugIOLog (3,  "  ... mOutMasterVolumeControl volumeRange = 0x%X", (unsigned int)volumeRange );
		} else if (NULL != mOutLeftVolumeControl) {
			volumeRange = (mOutLeftVolumeControl->getMaxValue () - mOutLeftVolumeControl->getMinValue () + 1);
			debugIOLog (3,  "  ... mOutLeftVolumeControl volumeRange = 0x%X", (unsigned int)volumeRange );
		} else if (NULL != mOutRightVolumeControl) {
			volumeRange = (mOutRightVolumeControl->getMaxValue () - mOutRightVolumeControl->getMinValue () + 1);
			debugIOLog (3,  "  ... mOutRightVolumeControl volumeRange = 0x%X", (unsigned int)volumeRange );
		} else {
			volumeRange = kMaximumPRAMVolume;
			debugIOLog (3,  "  ... volumeRange = 0x%X **** NO AUDIO LEVEL CONTROLS!", (unsigned int)volumeRange );
		}

		averageVolume = (leftVol + rightVol) >> 1;		// sum the channel volumes and get an average
		debugIOLog (3,  "  ... averageVolume = 0x%X", (unsigned int)volumeRange );
		debugIOLog (3,  "  ... volumeRange %X, kMaximumPRAMVolume %X", (unsigned int)volumeRange, (unsigned int)kMaximumPRAMVolume );
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
	debugIOLog (3,  "- AppleOnboardAudio[%ld]::VolumeToPRAMValue returns 0x%X", mInstanceIndex, (unsigned int)pramVolume );
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
    
    debugIOLog (3, "+ AppleOnboardAudio[%ld]::WritePRAMVol leftVol=%lu, rightVol=%lu", mInstanceIndex, leftVol,  rightVol);
    
    if (platform) {
		debugIOLog (3,  "  ... platform 0x%X", (unsigned int)platform );
		pramVolume = VolumeToPRAMValue (leftVol, rightVol);
		// get the old value to compare it with
		err = platform->readXPRAM((IOByteCount)kPRamVolumeAddr, &curPRAMVol, (IOByteCount)1);
		if ( kIOReturnSuccess == err ) {
			debugIOLog (3,  "  ... curPRAMVol = 0x%X before write", (curPRAMVol & 0x07) );
			// Update only if there is a change
			if (pramVolume != (curPRAMVol & 0x07)) {
				// clear bottom 3 bits of volume control byte from PRAM low memory image
				curPRAMVol = (curPRAMVol & 0xF8) | pramVolume;
				debugIOLog (3, "  ... curPRAMVol = 0x%x",curPRAMVol);
				// write out the volume control byte to PRAM
				err = platform->writeXPRAM((IOByteCount)kPRamVolumeAddr, &curPRAMVol,(IOByteCount) 1);
				if ( kIOReturnSuccess != err ) {
					debugIOLog (3,  "  0x%X = platform->writeXPRAM( 0x%X, & 0x%X, 1 ), value = 0x%X", err, (unsigned int)kPRamVolumeAddr, (unsigned int)&curPRAMVol, (unsigned int)curPRAMVol );
				} else {
					mUCState.ucPramData = (UInt32)curPRAMVol;
					mUCState.ucPramVolume = mUCState.ucPramData & 0x00000007;
				}
			} else {
				debugIOLog (3,  "  PRAM write request is to current value: no I/O" );
			}
		} else {
			debugIOLog (3,  "  Could not readXPRAM prior to write! Error 0x%X", err );
		}
	} else {
		debugIOLog (3,  "  ... no platform" );
	}
    debugIOLog (3, "- AppleOnboardAudio[%ld]::WritePRAMVol", mInstanceIndex);
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
IOReturn AppleOnboardAudio::setDMAStateAndFormat ( UInt32 arg2, void * inState ) {
	IOReturn			result;
	
	debugIOLog ( 5, "+ AppleOnboardAudio::setDMAStateAndFormat( %d, %p )", arg2, inState );
	switch ( (DMA_STATE_SELECTOR)arg2 ) {
		case kSetInputChannelRegisters:		result = mDriverDMAEngine->setInputChannelRegisters ( inState );							break;
		case kSetOutputChannelRegisters:	result = mDriverDMAEngine->setOutputChannelRegisters ( inState );							break;
		case kSetDMAStateAndFormat:			result = mDriverDMAEngine->setDMAStateAndFormat ( (DBDMAUserClientStructPtr)inState );		break;
		default:							result = kIOReturnBadArgument;																break;
	}
	debugIOLog ( 5, "- AppleOnboardAudio::setDMAStateAndFormat( %d, %p ) returns %lX", arg2, inState, result );
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleOnboardAudio::getSoftwareProcessingState ( UInt32 arg2, void * outState ) {
#pragma unused ( arg2 )
	return kIOReturnError;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleOnboardAudio::getRealTimeCPUUsage ( UInt32 arg2, void * outState ) {
#pragma unused ( arg2 )
	return kIOReturnError;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleOnboardAudio::getAOAState ( UInt32 arg2, void * outState ) {
#pragma unused ( arg2 )
	IOReturn		result;
	
	result = kIOReturnError;
	FailIf ( 0 != arg2, Exit );
	FailIf (NULL == outState, Exit );
	
	debugIOLog (7,  "AppleOnboardAudio[%ld]::getAOAState for type %ld", mInstanceIndex, arg2 );

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
	return kIOReturnError;
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
		debugIOLog (4, "  ConfigChangeHelper (%p, %ld): calling pauseAudioEngine and beginConfigurationChange", inEngine, inSleep);
		mDriverDMAEngine->pauseAudioEngine ();
		if (0 != inSleep) {
			debugIOLog (4, "  waiting %d ms between pause and begin", inSleep);
			IOSleep ( 10 );
		}
		mDriverDMAEngine->beginConfigurationChange ();
	}
}

ConfigChangeHelper::~ConfigChangeHelper () {
	if (NULL != mDriverDMAEngine) {
		debugIOLog (4, "  ~ConfigChangeHelper: calling completeConfigurationChange and resumeAudioEngine");
		mDriverDMAEngine->completeConfigurationChange ();
		mDriverDMAEngine->resumeAudioEngine ();
	}
}


