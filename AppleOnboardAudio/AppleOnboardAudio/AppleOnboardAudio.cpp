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
#include <IOKit/IONVRAM.h>

#include "IOKit/audio/IOAudioDevice.h"

#include "AppleOnboardAudio.h"
#include "AudioHardwareUtilities.h"
#include "AudioHardwareObjectInterface.h"

#include "PlatformFactory.h"
#include "PlatformInterface.h"
#include "TransportFactory.h"

OSDefineMetaClassAndStructors(AppleOnboardAudio, IOAudioDevice)

#define super IOAudioDevice

#define LOCALIZABLE 1

#pragma mark +UNIX LIKE FUNCTIONS

bool AppleOnboardAudio::init (OSDictionary *properties)
{
    debug2IOLog("+ AppleOnboardAudio[%p]::init\n", this);

    if (!super::init(properties)) return false;
        
    currentDevices = 0xFFFF;
    
	mHasHardwareInputGain = true;	// aml 5.10.02
	
	mInternalMicDualMonoMode = e_Mode_Disabled;	// aml 6.17.02, turn off by default
	
	mSampleRateSelectInProcessSemaphore = false;
	mClockSelectInProcessSemaphore = false;

	mCurrentProcessingOutputString = OSString::withCString ("none");
	mUCState.ucPowerState = kIOAudioDeviceActive;

    debugIOLog("- AppleOnboardAudio::init\n");
    return true;
}

bool AppleOnboardAudio::start (IOService * provider) {
	bool								result;

    debug3IOLog ("+ AppleOnboardAudio[%p]::start (%p)\n", this, provider);
	result = FALSE;

	mProvider = provider;
	provider->open (this);

	mPowerThread = thread_call_allocate ((thread_call_func_t)AppleOnboardAudio::performPowerStateChangeThread, (thread_call_param_t)this);
	FailIf (NULL == mPowerThread, Exit);

	mInitHardwareThread = thread_call_allocate ((thread_call_func_t)AppleOnboardAudio::initHardwareThread, (thread_call_param_t)this);
	FailIf (NULL == mInitHardwareThread, Exit);

    debug3IOLog ("- AppleOnboardAudio[%p]::start (%p)\n", this, provider);

	result = super::start (provider);				// Causes our initHardware routine to be called.

Exit:
	return result;
}

bool AppleOnboardAudio::handleOpen (IOService * forClient, IOOptionBits options, void *	arg ) 
{
	bool							result;

	debug2IOLog ("AppleOnboardAudio::handleOpen(%p)\n", forClient);

	if (kFamilyOption_OpenMultiple == options) {
		result = true;
	} else {
		result = super::handleOpen ( forClient, options, arg );
		FailIf (!result, Exit);
	}

	registerPlugin ( (AudioHardwareObjectInterface *)forClient );

Exit:
	debug3IOLog ("AppleOnboardAudio::handleOpen(%p) returns %s\n", forClient, result == true ? "true" : "false");
	return result;
}

void AppleOnboardAudio::handleClose (IOService * forClient, IOOptionBits options ) 
{
	debug2IOLog ("AppleOnboardAudio::handleClose(%p)\n", forClient);

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
				debug2IOLog ("AppleOnboardAudio::willTerminate terminated  (%p)\n", thePluginObject);
			}
		}
	}
#endif
	
	bool result = super::willTerminate ( provider, options );

	provider->close(this);

	debug3IOLog("AppleOnboardAudio::willTerminate(%p) returns %d\n", provider, result);

	return result;
}

// Called by the plugin objects from their start() method so that AppleOnboardAudio knows about them and will call them as needed.
void AppleOnboardAudio::registerPlugin (AudioHardwareObjectInterface *thePlugin) {
//	IOCommandGate *				cg;

	debug2IOLog ("AppleOnboardAudio::registerPlugin (%p)\n", thePlugin);
/*	cg = getCommandGate ();
	if (NULL != cg) {
		cg->runAction (registerPluginAction, thePlugin);
	} else {
		debugIOLog ("no command gate for registering a plugin!\n");
	}*/
	
	FailIf ( NULL == mPluginObjects, Exit );
	mPluginObjects->setObject (thePlugin);

Exit:
	return;
}

void AppleOnboardAudio::stop (IOService * provider) {

	if (NULL != mSysPowerDownNotifier) {
		mSysPowerDownNotifier->remove ();
		mSysPowerDownNotifier = NULL;
	}

	mTerminating = TRUE;
	
	// [3253678]
	muteAnalogOuts ();
	
	if (mPowerThread) {
		thread_call_cancel (mPowerThread);
	}

	debug3IOLog ("AppleOnboardAudio::stop(), audioEngines = %p, isInactive() = %d\n", audioEngines, isInactive());
	super::stop (provider);
}

void AppleOnboardAudio::unRegisterPlugin (AudioHardwareObjectInterface *inPlugin) {
	AudioHardwareObjectInterface *		thePluginObject;
	UInt32								index;
	UInt32								count;

	debug2IOLog ("AppleOnboardAudio::unRegisterPlugin (%p)\n", inPlugin);

	if (NULL != mPluginObjects) {
		count = mPluginObjects->getCount ();
		for (index = 0; index < count; index++) {
			thePluginObject = getIndexedPluginObject (index);
			if ((NULL != thePluginObject) && (inPlugin == thePluginObject)) {
				mPluginObjects->removeObject(index);
				debug2IOLog ("removed  (%p)\n", inPlugin);
			}
		}
	}

	return;
}

IOReturn AppleOnboardAudio::registerPluginAction (OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4) {
	AppleOnboardAudio *			device;

	device = OSDynamicCast (AppleOnboardAudio, owner);
	debug2IOLog ("AppleOnboardAudio::plugin %p registering\n", arg1);

	if (NULL == device->mPluginObjects) {
		device->mPluginObjects = OSArray::withCapacity (1);
	}

	FailIf (NULL == device->mPluginObjects, Exit);
	device->mPluginObjects->setObject ((AudioHardwareObjectInterface *)arg1);

Exit:
	return kIOReturnSuccess;
}

OSObject * AppleOnboardAudio::getLayoutEntry (const char * entryID) {
	OSArray *							layouts;
	OSDictionary *						layoutEntry;
	OSObject *							entry;

	entry = NULL;

	layouts = OSDynamicCast (OSArray, getProperty (kLayouts));
	FailIf (NULL == layouts, Exit);

	layoutEntry = OSDynamicCast (OSDictionary, layouts->getObject (0));
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

	dictEntry = OSDynamicCast (OSDictionary, getLayoutEntry (outputEntry));
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

	dictEntry = OSDynamicCast (OSDictionary, getLayoutEntry (outputEntry));
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

	dictEntry = OSDynamicCast (OSDictionary, getLayoutEntry (outputEntry));
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

	debug2IOLog("AppleOnboardAudio::setUseInputGainControls(%s)\n", inputEntry);
	mUseInputGainControls = FALSE;

	dictEntry = OSDynamicCast (OSDictionary, getLayoutEntry (inputEntry));
	FailIf (NULL == dictEntry, Exit);

	controlsArray = OSDynamicCast (OSArray, dictEntry->getObject (kControls));
	FailIf (NULL == controlsArray, Exit);

	controlsCount = controlsArray->getCount ();
	for (index = 0; index < controlsCount; index++) {
		controlString = OSDynamicCast (OSString, controlsArray->getObject (index));
		if ((NULL != controlString) && (controlString->isEqualTo (kLeftVolControlString) || controlString->isEqualTo (kRightVolControlString))) {
			mUseInputGainControls = TRUE;
			debugIOLog("mUseInputGainControls = true\n");
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

	debug2IOLog( "AppleOnboardAudio::setUsePlaythroughControl(%s)\n", inputEntry );
	mUsePlaythroughControl = FALSE;

	dictEntry = OSDynamicCast ( OSDictionary, getLayoutEntry ( inputEntry ) );
	FailIf ( NULL == dictEntry, Exit );

	playthroughOSBoolean = OSDynamicCast ( OSBoolean, dictEntry->getObject ( kPlaythroughControlString ) );
	FailIf ( NULL == playthroughOSBoolean, Exit );

	mUsePlaythroughControl = playthroughOSBoolean->getValue ();
Exit:
	debug2IOLog( "mUsePlaythroughControl = %d\n", mUsePlaythroughControl );
	return;
}
//	[3281535]	} end
			
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleOnboardAudio::formatChangeRequest (const IOAudioStreamFormat * inFormat, const IOAudioSampleRate * inRate) {
	IOReturn							result;
	OSNumber *							connectionCodeNumber;
	
	result = kIOReturnError;

	// [3253678]
	muteAnalogOuts ();

	if (NULL != inFormat) {
		debug2IOLog ("AppleOnboardAudio::formatChangeRequest with bit width %d\n", inFormat->fBitWidth);
		result = mTransportInterface->transportSetSampleWidth (inFormat->fBitDepth, inFormat->fBitWidth);
		callPluginsInOrder (kSetSampleBitDepth, inFormat->fBitDepth);
		if (kIOAudioStreamSampleFormat1937AC3 == inFormat->fSampleFormat) {
			FailIf (NULL == mOutputSelector, Exit);
			connectionCodeNumber = OSNumber::withNumber (kIOAudioOutputPortSubTypeSPDIF, 32);
			mOutputSelector->setValue (connectionCodeNumber);
			mEncodedOutputFormat = true;
		} else {
			mEncodedOutputFormat = false;		
		}
		result = callPluginsInOrder ( kSetSampleType, inFormat->fSampleFormat );
	}
	if (NULL != inRate) {
		mSampleRateSelectInProcessSemaphore = true;
		
		debug2IOLog ("AppleOnboardAudio::formatChangeRequest with rate %ld\n", inRate->whole);
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

	// [3253678]
	mCurrentOutputPlugin->prepareForOutputChange ();
	selectOutput (mOutputSelector->getIntValue ());

Exit:
	return result;
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
	
#ifdef kVERBOSE_LOG
	debug6IrqIOLog ("AppleOnboardAudio:: interruptEventAction - (%p, %ld, %ld, %ld, %ld)\n", owner, (UInt32)arg1, (UInt32)arg2, (UInt32)arg3, (UInt32)arg4);
#endif
	
	aoa = (AppleOnboardAudio *)owner;
	FailIf (NULL == aoa, Exit);
	aoa->protectedInterruptEventHandler ((UInt32)arg1, (UInt32)arg2);

	result = kIOReturnSuccess;
Exit:
	return result;
}

// Must run on the workloop because we might modify audio controls.
void AppleOnboardAudio::protectedInterruptEventHandler (UInt32 statusSelector, UInt32 newValue) {
	AudioHardwareObjectInterface *		thePlugin;
	OSNumber * 							connectionCodeNumber;
	char * 								pluginString;
	UInt32 								selectorCode;

	selectorCode = getCharCodeForIntCode (statusSelector);

#ifdef kVERBOSE_LOG
	switch ( statusSelector ) {
		case kInternalSpeakerStatus:		debug3IrqIOLog ("AppleOnboardAudio::protectedInterruptEventHandler ( kInternalSpeakerStatus, %lX, %ld )\n", selectorCode, newValue);		break;
		case kHeadphoneStatus:				debug3IrqIOLog ("AppleOnboardAudio::protectedInterruptEventHandler ( kHeadphoneStatus, %lX, %ld )\n", selectorCode, newValue);				break;
		case kExtSpeakersStatus:			debug3IrqIOLog ("AppleOnboardAudio::protectedInterruptEventHandler ( kExtSpeakersStatus, %lX, %ld )\n", selectorCode, newValue);			break;
		case kLineOutStatus:				debug3IrqIOLog ("AppleOnboardAudio::protectedInterruptEventHandler ( kLineOutStatus, %lX, %ld )\n", selectorCode, newValue);				break;
		case kDigitalOutStatus:				debug3IrqIOLog ("AppleOnboardAudio::protectedInterruptEventHandler ( kDigitalOutStatus, %lX, %ld )\n", selectorCode, newValue);				break;
		case kLineInStatus:					debug3IrqIOLog ("AppleOnboardAudio::protectedInterruptEventHandler ( kLineInStatus, %lX, %ld )\n", selectorCode, newValue);					break;
		case kInputMicStatus:				debug3IrqIOLog ("AppleOnboardAudio::protectedInterruptEventHandler ( kInputMicStatus, %lX, %ld )\n", selectorCode, newValue);				break;
		case kExternalMicInStatus:			debug3IrqIOLog ("AppleOnboardAudio::protectedInterruptEventHandler ( kExternalMicInStatus, %lX, %ld )\n", selectorCode, newValue);			break;
		case kDigitalInStatus:				debug3IrqIOLog ("AppleOnboardAudio::protectedInterruptEventHandler ( kDigitalInStatus, %lX, %ld )\n", selectorCode, newValue);				break;
		case kRequestCodecRecoveryStatus:	debug3IrqIOLog ("AppleOnboardAudio::protectedInterruptEventHandler ( kRequestCodecRecoveryStatus, %lX, %ld )\n", selectorCode, newValue);	break;
		case kClockInterruptedRecovery:		debug3IrqIOLog ("AppleOnboardAudio::protectedInterruptEventHandler ( kClockInterruptedRecovery, %lX, %ld )\n", selectorCode, newValue);		break;
		case kClockLockStatus:				debug3IrqIOLog ("AppleOnboardAudio::protectedInterruptEventHandler ( kClockLockStatus, %lX, %ld )\n", selectorCode, newValue);				break;
		case kAES3StreamErrorStatus:		debug3IrqIOLog ("AppleOnboardAudio::protectedInterruptEventHandler ( kAES3StreamErrorStatus, %lX, %ld )\n", selectorCode, newValue);		break;
		case kComboIsAnalogStatus:			debug3IrqIOLog ("AppleOnboardAudio::protectedInterruptEventHandler ( kComboIsAnalogStatus, %lX, %ld )\n", selectorCode, newValue);			break;
		case kComboIsDigitalStatus:			debug3IrqIOLog ("AppleOnboardAudio::protectedInterruptEventHandler ( kComboIsDigitalStatus, %lX, %ld )\n", selectorCode, newValue);			break;
		case kCodecErrorInterruptStatus:	debug3IrqIOLog ("AppleOnboardAudio::protectedInterruptEventHandler ( kCodecErrorInterruptStatus, %lX, %ld )\n", selectorCode, newValue);	break;
		case kCodecInterruptStatus:			debug3IrqIOLog ("AppleOnboardAudio::protectedInterruptEventHandler ( kCodecInterruptStatus, %lX, %ld )\n", selectorCode, newValue);			break;
		case kBreakClockSelect:				debug3IrqIOLog ("AppleOnboardAudio::protectedInterruptEventHandler ( kBreakClockSelect, %lX, %ld )\n", selectorCode, newValue);				break;
		case kMakeClockSelect:				debug3IrqIOLog ("AppleOnboardAudio::protectedInterruptEventHandler ( kMakeClockSelect, %lX, %ld )\n", selectorCode, newValue);				break;
		case kSetSampleRate:				debug3IrqIOLog ("AppleOnboardAudio::protectedInterruptEventHandler ( kSetSampleRate, %lX, %ld )\n", selectorCode, newValue);				break;
		case kSetSampleBitDepth:			debug3IrqIOLog ("AppleOnboardAudio::protectedInterruptEventHandler ( kSetSampleBitDepth, %lX, %ld )\n", selectorCode, newValue);			break;
		case kPowerStateChange:				debug3IrqIOLog ("AppleOnboardAudio::protectedInterruptEventHandler ( kPreDMAEngineInit, %lX, %ld )\n", selectorCode, newValue);				break;
		case kPreDMAEngineInit:				debug3IrqIOLog ("AppleOnboardAudio::protectedInterruptEventHandler ( kPreDMAEngineInit, %lX, %ld )\n", selectorCode, newValue);				break;
		case kPostDMAEngineInit:			debug3IrqIOLog ("AppleOnboardAudio::protectedInterruptEventHandler ( kPostDMAEngineInit, %lX, %ld )\n", selectorCode, newValue);			break;
		case kRestartTransport:				debug3IrqIOLog ("AppleOnboardAudio::protectedInterruptEventHandler ( kRestartTransport, %lX, %ld )\n", selectorCode, newValue);				break;
	}
#endif

	switch (statusSelector) {
		case kHeadphoneStatus:
			if (newValue == kInserted) {
				mDetectCollection |= kSndHWCPUHeadphone;
				debugIOLog ("headphones inserted.\n");
			} else if (newValue == kRemoved) {
				mDetectCollection &= ~kSndHWCPUHeadphone;
				debugIOLog ("headphones removed.\n");
			} else {
				debugIOLog ("Unknown headphone jack status.\n");
			}
			break;
		case kLineOutStatus:
			if (newValue == kInserted) {
				mDetectCollection |= kSndHWLineOutput;
				debugIOLog ("line out inserted.\n");
			} else if (newValue == kRemoved) {
				mDetectCollection &= ~kSndHWLineOutput;
				debugIOLog ("line out removed.\n");
			} else {
				debugIOLog ("Unknown line out jack status.\n");
			}
			break;
		case kLineInStatus:
			if (newValue == kInserted) {
				mDetectCollection |= kSndHWLineInput;
			} else if (newValue == kRemoved) {
				mDetectCollection &= ~kSndHWLineInput;
			} else {
				debugIOLog ("Unknown line in status.\n");
			}
			break;
	}

	switch (statusSelector) {
		case kHeadphoneStatus:
		case kLineOutStatus:
			//pluginString = getConnectionKeyFromCharCode (selectorCode, kIOAudioStreamDirectionOutput);
			//debug2IOLog ("AppleOnboardAudio::protectedInterruptEventHandler - pluginString = %s.\n", pluginString);
			//thePlugin = getPluginObjectForConnection (pluginString);
			//FailIf (NULL == thePlugin, Exit);
			//debug2IOLog ("AppleOnboardAudio::protectedInterruptEventHandler - thePlugin = %p.\n", thePlugin);

			//cacheOutputVolumeLevels (thePlugin);
			//thePlugin->prepareForOutputChange ();
			
			// [3279525] when exclusive, handle redirection
			if (kLineOutStatus == statusSelector) {
				if ( mDetectCollection & kSndHWLineOutput ) {
					selectorCode = kIOAudioOutputPortSubTypeLine;
				} else if ( mDetectCollection & kSndHWCPUHeadphone ) {
					selectorCode = kIOAudioOutputPortSubTypeHeadphones;
				} else {
					selectorCode = kIOAudioOutputPortSubTypeInternalSpeaker;
				}
			} else if (kHeadphoneStatus == statusSelector) {
				if ( mDetectCollection & kSndHWCPUHeadphone ) {
					selectorCode = kIOAudioOutputPortSubTypeHeadphones;
				} else if ( mDetectCollection & kSndHWLineOutput ) {
					selectorCode = kIOAudioOutputPortSubTypeLine;
				} else {
					selectorCode = kIOAudioOutputPortSubTypeInternalSpeaker;
				}
			}

			connectionCodeNumber = OSNumber::withNumber (selectorCode, 32);
			// [3250195], must update this too!  Otherwise you get the right output selected but with the wrong clip routine/EQ.
			pluginString = getConnectionKeyFromCharCode (selectorCode, kIOAudioStreamDirectionOutput);
			debug2IOLog ("pluginString updated to %s.\n", pluginString);
			// [3323073], move code from above, and now set the current output plugin if it's changing!
			thePlugin = getPluginObjectForConnection (pluginString);
			FailIf (NULL == thePlugin, Exit);
			
//			if (mCurrentOutputPlugin != thePlugin) {
				cacheOutputVolumeLevels (mCurrentOutputPlugin);
//			}
			//cacheOutputVolumeLevels (thePlugin);
			thePlugin->prepareForOutputChange ();
			
			mCurrentOutputPlugin = thePlugin;
			
			// [3250195], current plugin doesn't matter here, always need these updated
			setClipRoutineForOutput (pluginString);
			selectOutput (selectorCode);
			if (NULL == mOutputSelector) debugIOLog ("\n!!!mOutputSelector = NULL!!!\n");
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
			break;
		case kExtSpeakersStatus:
			break;
		case kDigitalOutStatus:
			break;
		case kLineInStatus:
			// [3250612] don't do anything on insert events!
			pluginString = getConnectionKeyFromCharCode (selectorCode, kIOAudioStreamDirectionInput);
			debug2IOLog ("AppleOnboardAudio::protectedInterruptEventHandler - pluginString = %s.\n", pluginString);
			thePlugin = getPluginObjectForConnection (pluginString);
			FailIf (NULL == thePlugin, Exit);
			debug2IOLog ("AppleOnboardAudio::protectedInterruptEventHandler - thePlugin = %p.\n", thePlugin);

			FailIf (NULL == mInputSelector, Exit);
			selectorCode = mInputSelector->getIntValue ();
			debug2IOLog("mInputSelector->getIntValue () returns %4s\n", (char *)&selectorCode);

			if ((mDetectCollection & kSndHWLineInput) && (kIOAudioInputPortSubTypeLine == selectorCode)) {
				thePlugin->setInputMute (FALSE);
			} else if (!(mDetectCollection & kSndHWLineInput) && (kIOAudioInputPortSubTypeLine == selectorCode)) {
				thePlugin->setInputMute (TRUE);
			}
			break;
		case kExternalMicInStatus:
			break;
		case kDigitalInStatus:
			break;
		case kRequestCodecRecoveryStatus:
			//	Walk through the available plugin objects and invoke the recoverFromFatalError on each object in the correct order.
			callPluginsInOrder (kRequestCodecRecoveryStatus, newValue);
			break;
		case kRestartTransport:
			//	This message is used to restart the transport hardware without invoking a general recovery or
			//	a recovery from clock interruption and is used to perform sequence sensitive initialization.
			mTransportInterface->transportSetSampleRate ( mTransportInterface->transportGetSampleRate() );
			break;
		case kCodecErrorInterruptStatus:
			callPluginsInOrder ( kCodecErrorInterruptStatus, 0 );
			break;
		case kCodecInterruptStatus:
			callPluginsInOrder ( kCodecInterruptStatus, 0 );
			break;
		case kClockLockStatus:
			//	Codec clock loss errors are to be ignored if in the process of switching clock sources.
			if ( !mClockSelectInProcessSemaphore ) {
				//	A non-zero 'newValue' indicates an 'UNLOCK' clock lock status and requires that the clock source
				//	be redirected back to an internal source (i.e. the internal hardware is to act as a MASTER).
				if ( newValue ) {
					//	Set the hardware to MASTER mode.
					clockSelectorChanged ( kClockSourceSelectionInternal );
					if ( NULL != mExternalClockSelector ) {
						//	Flush the control value (i.e. MASTER = internal).
						OSNumber *			clockSourceSelector;
						clockSourceSelector = OSNumber::withNumber (kClockSourceSelectionInternal, 32);
						
						mExternalClockSelector->hardwareValueChanged (clockSourceSelector);
					}
				} else {
					// [3253678] successful lock detected, now safe to unmute analog part
					mCurrentOutputPlugin->prepareForOutputChange ();
					selectOutput (mOutputSelector->getIntValue ());
				}
			} else {
				debugIOLog ( "Attempted to post kClockLockStatus blocked by semaphore\n" );
			}
			break;
		case kAES3StreamErrorStatus:
			//	indicates that the V bit states data is invalid or may be compressed data
			debug2IrqIOLog ( "... kAES3StreamErrorStatus %d\n", (unsigned int)newValue );
			if ( newValue ) {
				//	As appropriate (TBD)...
			}
			break;
		default:
			break;
	}
Exit:
	return;
}

IOReturn AppleOnboardAudio::callPluginsInReverseOrder (UInt32 inSelector, UInt32 newValue) {
	AudioHardwareObjectInterface *		thePluginObject;
	OSArray *							pluginOrderArray;
	OSString *							pluginName;
	SInt32								index;
	UInt32								pluginOrderArrayCount;
	IOReturn							result;

	debug3IOLog ("AppleOnboardAudio::callPluginsInReverseOrder (%lX, %lX)\n", inSelector, newValue);
	result = kIOReturnBadArgument;

	pluginOrderArray = OSDynamicCast (OSArray, getLayoutEntry (kPluginRecoveryOrder));
	FailIf (NULL == pluginOrderArray, Exit);

	pluginOrderArrayCount = pluginOrderArray->getCount ();

	for (index = pluginOrderArrayCount - 1; index >= 0; index--) {
		pluginName = OSDynamicCast(OSString, pluginOrderArray->getObject(index));
		if (NULL == pluginName) {
			debug2IOLog ("Corrupt %s entry in AppleOnboardAudio Info.plist\n", kPluginRecoveryOrder);
			continue;
		}
		thePluginObject = getPluginObjectWithName (pluginName);
		if (NULL == thePluginObject) {
			debug2IOLog ("Can't find required AppleOnboardAudio plugin from %s entry loaded\n", kPluginRecoveryOrder);
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
				if ( kIOReturnSuccess == result ) {
					mUCState.ucPowerState = newValue;
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
	
#ifdef kVERBOSE_LOG
	debug3IrqIOLog ("AppleOnboardAudio::callPluginsInOrder (%lX, %lX)\n", inSelector, newValue);
#endif
	result = kIOReturnBadArgument;

	pluginOrderArray = OSDynamicCast (OSArray, getLayoutEntry (kPluginRecoveryOrder));
	FailIf (NULL == pluginOrderArray, Exit);

	pluginOrderArrayCount = pluginOrderArray->getCount ();

	result = kIOReturnSuccess;
	for (index = 0; index < pluginOrderArrayCount; index++) {
		pluginName = OSDynamicCast(OSString, pluginOrderArray->getObject(index));
		if (NULL == pluginName) {
			debug2IOLog ("Corrupt %s entry in AppleOnboardAudio Info.plist\n", kPluginRecoveryOrder);
			continue;
		}
		thePluginObject = getPluginObjectWithName (pluginName);
		if (NULL == thePluginObject) {
			debug2IOLog ("Can't find required AppleOnboardAudio plugin from %s entry loaded\n", kPluginRecoveryOrder);
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
				tempResult = thePluginObject->preDMAEngineInit ();
				break;
			case kPostDMAEngineInit:
				thePluginObject->postDMAEngineInit ();
				break;
			case kPowerStateChange:
				if (kIOAudioDeviceSleep == newValue) {
					debug2IOLog("AppleOnboardAudio::callPluginsInOrder ### Telling %s to sleep\n", pluginName->getCStringNoCopy ());
					tempResult = thePluginObject->performDeviceSleep ();
				} else if (kIOAudioDeviceActive == newValue) {
					debug2IOLog("AppleOnboardAudio::callPluginsInOrder ### Telling %s to wake\n", pluginName->getCStringNoCopy ());
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
	debug4IOLog ("-AppleOnboardAudio::callPluginsInOrder (%lX, %lX) = %x\n", inSelector, newValue, result);
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
	
//	debug2IOLog ( "+ AppleOnboardAudio::findPluginForType (%d )\n", (unsigned int)pluginType );

	result = NULL;
	pluginOrderArray = OSDynamicCast (OSArray, getLayoutEntry (kPluginRecoveryOrder));
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
//	debug3IOLog ( "- AppleOnboardAudio::findPluginForType (%d ) returns %p\n", (unsigned int)pluginType, result );
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
			//	[3305011]	begin {
			//		If the DMA engine dies with no indication of a hardware error then
			//		recovery must be performed by stopping and starting the engine.
			if ( ( kTRANSPORT_MASTER_CLOCK != mTransportInterface->transportGetClockSelect() ) && mDriverDMAEngine->engineDied() ) {
				mDriverDMAEngine->pauseAudioEngine ();
				mDriverDMAEngine->beginConfigurationChange();
				
				UInt32 currentBitDepth = mTransportInterface->transportGetSampleWidth();
				mTransportInterface->transportSetSampleWidth ( currentBitDepth, mTransportInterface->transportGetDMAWidth() );
				callPluginsInOrder ( kSetSampleBitDepth, currentBitDepth );
				
				mDriverDMAEngine->completeConfigurationChange();
				mDriverDMAEngine->resumeAudioEngine ();
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
	
	debug2IOLog ( "+ AppleOnboardAudio::ParseDetectCollection() detectCollection 0x%lX\n", mDetectCollection );

	if ( mDetectCollection & kSndHWLineOutput ) {
		result = kIOAudioOutputPortSubTypeLine;
	} else if ( mDetectCollection & kSndHWCPUHeadphone ) {
		result = kIOAudioOutputPortSubTypeHeadphones;
	} else {
		result = kIOAudioOutputPortSubTypeInternalSpeaker;
	}

	debug2IOLog ( "- AppleOnboardAudio::ParseDetectCollection returns %lX\n", result );
	return result;
}

UInt32 AppleOnboardAudio::parseInputDetectCollection ( void ) {
	UInt32		result;
	
	debug2IOLog ( "+ AppleOnboardAudio::parseInputDetectCollection() detectCollection 0x%lX\n", mDetectCollection );

	if ( mDetectCollection & kSndHWLineInput ) {
		result = kIOAudioOutputPortSubTypeLine;
	} else {
		result = kIOAudioInputPortSubTypeInternalMicrophone;
	}

	debug2IOLog ( "- AppleOnboardAudio::parseInputDetectCollection returns %lX\n", result );
	return result;
}

void AppleOnboardAudio::initializeDetectCollection ( void ) {
	debugIOLog ( "+ AppleOnboardAudio::initializeDetectCollection() \n" );

	if ( kGPIO_Connected == mPlatformInterface->getHeadphoneConnected() ) {
		mDetectCollection |= kSndHWCPUHeadphone;
		debugIOLog ( "AppleOnboardAudio::initializeDetectCollection() - headphones are connected.\n" );
	} 
	if ( kGPIO_Connected == mPlatformInterface->getLineOutConnected() ) {
		mDetectCollection |= kSndHWLineOutput;
	} 
	if ( kGPIO_Connected == mPlatformInterface->getDigitalOutConnected() ) {
		mDetectCollection |= kSndHWDigitalOutput;
	} 
	if ( kGPIO_Connected == mPlatformInterface->getLineInConnected() ) {
		mDetectCollection |= kSndHWLineInput;
	} 
	if ( kGPIO_Connected == mPlatformInterface->getDigitalInConnected() ) {
		mDetectCollection |= kSndHWDigitalInput;
	} 
	
	debug2IOLog ( "- AppleOnboardAudio::initializeDetectCollection() %lX\n", mDetectCollection );
	return;
}

void AppleOnboardAudio::muteAnalogOuts () 	// [3253678]
{
	debugIOLog ( "muting all amps.\n" );
	mPlatformInterface->setHeadphoneMuteState ( kGPIO_Muted );
	mPlatformInterface->setLineOutMuteState ( kGPIO_Muted );
	mPlatformInterface->setSpeakerMuteState ( kGPIO_Muted );
	IOSleep (mAmpRecoveryMuteDuration);
	setAnalogCodecMute (1);
	return;
}

void AppleOnboardAudio::setAnalogCodecMute (UInt32 inValue)
{
	AudioHardwareObjectInterface *			thePlugin;
	thePlugin = findPluginForType (kCodec_TAS3004);
	if (NULL != thePlugin) {
		thePlugin->setMute (inValue);
    }
	return;
}

void AppleOnboardAudio::selectOutput (const UInt32 inSelection, const bool inUpdateAll)
{
	bool								needToWaitForAmps;
	
	debug2IOLog ( "Selecting output %4s.\n", (char *)&(inSelection) );

	needToWaitForAmps = true;
	
	// [3253678] mute analog outs if playing encoded content, unmute only if not playing 
	// encoded content (eg. on jack inserts/removals)
	
	switch (inSelection) {
		case kIOAudioOutputPortSubTypeHeadphones:
			debugIOLog ( "switching amps to headphones.\n" );
			mPlatformInterface->setSpeakerMuteState ( kGPIO_Muted );
			if (!mEncodedOutputFormat && !mIsMute) {
				setAnalogCodecMute (0);
				mPlatformInterface->setHeadphoneMuteState ( kGPIO_Unmuted );
			}
			if (mHeadLineDigExclusive) {
				mPlatformInterface->setLineOutMuteState ( kGPIO_Muted );
			} else {
				if ((mDetectCollection & kSndHWLineOutput) && !mEncodedOutputFormat && !mIsMute) {
					setAnalogCodecMute (0);
					mPlatformInterface->setLineOutMuteState ( kGPIO_Unmuted );
				}	
			}
			break;
		case kIOAudioOutputPortSubTypeLine:
			debugIOLog ( "switching amps to line out.\n" );
			mPlatformInterface->setSpeakerMuteState ( kGPIO_Muted );
			if (!mEncodedOutputFormat && !mIsMute) {
				setAnalogCodecMute (0);
				mPlatformInterface->setLineOutMuteState ( kGPIO_Unmuted );
			}
			if (!mHeadLineDigExclusive) {
				if ((mDetectCollection & kSndHWCPUHeadphone) && !mEncodedOutputFormat && !mIsMute) {
					setAnalogCodecMute (0);
					mPlatformInterface->setHeadphoneMuteState ( kGPIO_Unmuted );
				}	
			}
			break;
		case kIOAudioOutputPortSubTypeInternalSpeaker:
			if (!mEncodedOutputFormat && !mIsMute) {
				setAnalogCodecMute (0);
				mPlatformInterface->setSpeakerMuteState ( kGPIO_Unmuted );
				// [3250195], don't want to get EQ on these outputs if we're on internal speaker.
				mPlatformInterface->setLineOutMuteState ( kGPIO_Muted );
				mPlatformInterface->setHeadphoneMuteState ( kGPIO_Muted );
			}
			break;
		case kIOAudioOutputPortSubTypeSPDIF:
			mPlatformInterface->setSpeakerMuteState ( kGPIO_Muted );
			if (mEncodedOutputFormat) {
				muteAnalogOuts ();
 				needToWaitForAmps = false;
			} else {
				if (inUpdateAll) {
					if ((mDetectCollection & kSndHWCPUHeadphone)  && !mIsMute) {
						setAnalogCodecMute (0);
						mPlatformInterface->setHeadphoneMuteState ( kGPIO_Unmuted );
						if ( mDetectCollection & kSndHWLineOutput && !mHeadLineDigExclusive) {
							mPlatformInterface->setLineOutMuteState ( kGPIO_Unmuted );
						}
					} else if (mDetectCollection & kSndHWLineOutput && !mIsMute) {
						setAnalogCodecMute (0);
						mPlatformInterface->setLineOutMuteState ( kGPIO_Unmuted );
					}
				}
			}
			break;
		default:
			debug2IOLog("Amplifier not changed, selection = %ld\n", inSelection);
			needToWaitForAmps = false;
			break;
	}
	if (needToWaitForAmps) {
		IOSleep (mAmpRecoveryMuteDuration);
	}
    return;
}

void AppleOnboardAudio::free()
{
    debugIOLog("+ AppleOnboardAudio::free\n");
    
	removeTimerEvent ( this );

    if (mDriverDMAEngine) {
		debug2IOLog("  AppleOnboardAudio::free - mDriverDMAEngine retain count = %d\n", mDriverDMAEngine->getRetainCount());
        mDriverDMAEngine->release();
		mDriverDMAEngine = NULL;
	}
	if (idleTimer) {
		if (workLoop) {
			workLoop->removeEventSource (idleTimer);
		}

		idleTimer->release ();
		idleTimer = NULL;
	}
	if ( pollTimer ) {
		if ( workLoop ) {
			workLoop->removeEventSource ( pollTimer );
		}

		pollTimer->release ();
		pollTimer = NULL;
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
	CLEAN_RELEASE(mOutputSelector);
	CLEAN_RELEASE(mInputSelector);
	CLEAN_RELEASE(mPluginObjects);
	CLEAN_RELEASE(mTransportInterface);
	// must release last
	CLEAN_RELEASE(mPlatformInterface);

    super::free();
    debugIOLog("- AppleOnboardAudio::free, (void)\n");
}

#pragma mark +PORT HANDLER FUNCTIONS
void AppleOnboardAudio::setCurrentDevices(UInt32 devices){
    UInt32					odevice;

    if(devices != currentDevices) {
        odevice = currentDevices;
        currentDevices = devices;
//        changedDeviceHandler(odevice);
    }
    
	debug2IOLog ("currentDevices = %ld\n", currentDevices);
	debug2IOLog ("fCPUNeedsPhaseInversion = %d\n", fCPUNeedsPhaseInversion);
    // if this CPU has a phase inversion feature see if we need to enable phase inversion    
    if (fCPUNeedsPhaseInversion) {
        bool state;

        if (currentDevices == 0 || currentDevices & kSndHWInternalSpeaker) {			// may only need the kSndHWInternalSpeaker check
            state = true;
        } else {
            state = false;
		}

         mDriverDMAEngine->setPhaseInversion(state);
    }

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

#pragma mark +IOAUDIO INIT
bool AppleOnboardAudio::initHardware (IOService * provider) {
	bool								result;

	result = FALSE;

	FailIf (NULL == mInitHardwareThread, Exit);
	thread_call_enter1 (mInitHardwareThread, (void *)provider);

	result = TRUE;

Exit:
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
	OSDictionary *						layoutEntry;
	OSNumber *							layoutIDNumber;
	OSNumber *							ampRecoveryNumber;
	OSNumber *							headphoneState;
	OSString *							platformObjectString;
	OSString *							transportObjectString;
	OSData *							tmpData;
	UInt32 *							layoutID;
	UInt32								layoutIDInt;
	UInt32								timeout;
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
    bool								result;

    debugIOLog("+ AppleOnboardAudio::initHardware\n");

	tmpData = OSDynamicCast (OSData, provider->getProperty (kLayoutID));
	FailIf (NULL == tmpData, Exit);
	layoutID = (UInt32 *)tmpData->getBytesNoCopy ();
	FailIf (NULL == layoutID, Exit)
	mLayoutID = *layoutID;

	// Figure out which plugins need to be loaded for this machine.
	// Fix up the registry to get needed plugins to load using ourselves as a nub.
	layouts = OSDynamicCast (OSArray, getProperty (kLayouts));
	FailIf (NULL == layouts, Exit);

	// First thing to do is to find the array entry that describes the machine that we are on.
	layoutsCount = layouts->getCount ();
	layoutEntry = NULL;
	index = 0;
	while (layoutsCount--) {
		layoutEntry = OSDynamicCast (OSDictionary, layouts->getObject (index));
		FailIf (NULL == layoutEntry, Exit);
		layoutIDNumber = OSDynamicCast (OSNumber, layoutEntry->getObject (kLayoutIDInfoPlist));
		FailIf (NULL == layoutIDNumber, Exit);
		layoutIDInt = layoutIDNumber->unsigned32BitValue ();
		if (layoutIDInt != mLayoutID) {
			layouts->removeObject (index);			// Remove wrong entries from the IORegistry to save space
		} else {
			index++;
		}
	}
	debug2IOLog ( "><><>< mLayoutID %lX\n", mLayoutID);
	debug2IOLog ( "><><>< layoutIDInt %lX\n", layoutIDInt );

	layoutEntry = OSDynamicCast (OSDictionary, layouts->getObject (0));
	FailIf (NULL == layoutEntry, Exit);

	ampRecoveryNumber = OSDynamicCast ( OSNumber, layoutEntry->getObject (kAmpRecoveryTime) );
	FailIf (NULL == ampRecoveryNumber, Exit);
	mAmpRecoveryMuteDuration = ampRecoveryNumber->unsigned32BitValue();
	debug2IOLog ("AppleOnboardAudio::start - mAmpRecoveryMuteDuration = %ld\n", mAmpRecoveryMuteDuration);

	// Find out what the correct platform object is and request the platform factory to build it for us.
	platformObjectString = OSDynamicCast ( OSString, layoutEntry->getObject ( kPlatformObject ) );
	FailIf (NULL == platformObjectString, Exit);
	debug2IOLog ("AppleOnboardAudio::start - platformObjectString = %s\n", platformObjectString->getCStringNoCopy());

	mPlatformInterface = PlatformFactory::createPlatform (platformObjectString);
	FailIf (NULL == mPlatformInterface, Exit);
	debug2IOLog ("AppleOnboardAudio::start - mPlatformInterface = %p\n", mPlatformInterface);
	FailIf (!mPlatformInterface->init(provider, this, AppleDBDMAAudio::kDBDMADeviceIndex), Exit);

	debugIOLog ("AppleOnboardAudio::protectedInitHardware - about to mute all amps.\n");

	mPlatformInterface->setHeadphoneMuteState ( kGPIO_Muted );
	mPlatformInterface->setLineOutMuteState ( kGPIO_Muted );
	mPlatformInterface->setSpeakerMuteState ( kGPIO_Muted );
	IOSleep (mAmpRecoveryMuteDuration);

	// Find out what the correct transport object is and request the transport factory to build it for us.
	transportObjectString = OSDynamicCast ( OSString, layoutEntry->getObject ( kTransportObject ) );
	FailIf ( NULL == transportObjectString, Exit );
	debug2IOLog ("AppleOnboardAudio::start - transportObjectString = %s\n", transportObjectString->getCStringNoCopy());

	mTransportInterface = TransportFactory::createTransport ( transportObjectString );
	FailIf (NULL == mTransportInterface, Exit);
	debug2IOLog ("AppleOnboardAudio::start - mTransportInterface = %p\n", mTransportInterface);
	FailIf (!mTransportInterface->init ( mPlatformInterface ), Exit);

	// If we have the entry we were looking for then get the list of plugins that need to be loaded
	hardwareObjectsList = OSDynamicCast (OSArray, layoutEntry->getObject (kHardwareObjects));
	if ( NULL == hardwareObjectsList ) { debugIOLog ( "><><>< NULL == hardwareObjectsList\n" ); }
	FailIf (NULL == hardwareObjectsList, Exit);

	// Set the IORegistry entries that will cause the plugins to load
	numPlugins = hardwareObjectsList->getCount ();
	debug2IOLog ( "numPlugins to load = %ld\n", numPlugins);

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
#if DEBUGLOG
	timeout = 50000;
#else
	timeout = 1000;
#endif
	done = FALSE;
	while (!done && timeout) {
		if (NULL == mPluginObjects) {
			IOSleep (10);
		} else {
			if (mPluginObjects->getCount () != numPlugins) {
				IOSleep (10);
			} else {
				done = TRUE;
			}
		}
		timeout--;
	}
	
	if ((0 == timeout) && (FALSE == done)) {
		debugIOLog ("$$$$$$ timeout and not enough plugins $$$$$$\n");
		setProperty ("Plugin load failed", "TRUE");
	}

	volumeNumber = OSNumber::withNumber((long long unsigned int)0, 32);

	result = FALSE;
    if (!super::initHardware (provider)) {
        goto Exit;
    }

	mHeadLineDigExclusive = FALSE;		// [3276398], default to working like we always have.
	workLoop = getWorkLoop();
	FailIf (NULL == workLoop, Exit);

	// must occur in this order, and must be called in initHardware or later to have a valid workloop
	mPlatformInterface->setWorkLoop (workLoop);
	
	FailIf (NULL == mPluginObjects, Exit);

	count = mPluginObjects->getCount ();
	FailIf (0 == count, Exit);
	
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

#if LOCALIZABLE
    setDeviceName ("DeviceName");
    setDeviceShortName ("DeviceShortName");
    setManufacturerName ("ManufacturerName");
    setProperty (kIOAudioDeviceLocalizedBundleKey, "AppleOnboardAudio.kext");
#else
    setDeviceName ("Built-in Audio");
    setDeviceShortName ("Built-in");
    setManufacturerName ("Apple");
#endif
	setDeviceTransportType (kIOAudioDeviceTransportTypeBuiltIn);

	setProperty (kIOAudioEngineCoreAudioPlugInKey, "IOAudioFamily.kext/Contents/PlugIns/AOAHALPlugin.bundle");

    configureDMAEngines (provider);
	FailIf (NULL == mDriverDMAEngine, Exit);

	// Have to create the audio controls before calling activateAudioEngine
    createDefaultControls ();

	debug2IOLog ("  AppleOnboardAudio::initHardware - mDriverDMAEngine retain count before activateAudioEngine = %d\n", mDriverDMAEngine->getRetainCount());
    if (kIOReturnSuccess != activateAudioEngine (mDriverDMAEngine)) {
		mDriverDMAEngine->release ();
		mDriverDMAEngine = NULL;
        goto Exit;
    }
	debug2IOLog("  AppleOnboardAudio::initHardware - mDriverDMAEngine retain count after activateAudioEngine = %d\n", mDriverDMAEngine->getRetainCount());

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
	if (mDetectCollection & kSndHWCPUHeadphone) {
		headphoneState = OSNumber::withNumber (1, 32);
	} else {
		headphoneState = OSNumber::withNumber ((long long unsigned int)0, 32);
	}
	mHeadphoneConnected->hardwareValueChanged (headphoneState);
	
	FailIf (NULL == mOutputSelector, Exit);
	mOutputSelector->hardwareValueChanged (connectionCodeNumber);
	
	connectionCodeNumber->release();
		
	selectorCode = mOutputSelector->getIntValue ();
	debug2IOLog("mOutputSelector->getIntValue () returns %lX\n", selectorCode);
	if (0 != selectorCode) {
		connectionString = getConnectionKeyFromCharCode (selectorCode, kIOAudioStreamDirectionOutput);
		debug2IOLog("mOutputSelector->getIntValue () char code is %s\n", connectionString);
		if (NULL != connectionString) {
			mCurrentOutputPlugin = getPluginObjectForConnection (connectionString);
		}
	}
	debug2IOLog("mCurrentOutputPlugin = %p\n", mCurrentOutputPlugin);
	
	FailIf (NULL == mInputSelector, Exit);
	selectorCode = mInputSelector->getIntValue ();
	debug2IOLog("mInputSelector->getIntValue () returns %4s\n", (char *)&selectorCode);
	if (0 != selectorCode) {
		connectionString = getConnectionKeyFromCharCode (selectorCode, kIOAudioStreamDirectionInput);
		debug2IOLog("mInputSelector->getIntValue () char code is %s\n", connectionString);
		if (NULL != connectionString) {
			mCurrentInputPlugin = getPluginObjectForConnection (connectionString);
		}
	}
	debug2IOLog("mCurrentInputPlugin = %p\n", mCurrentInputPlugin);
	
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
	mPlatformInterface->registerInterrupts ( this );

#if 0
	IOLog ("about to flush all controls\n");
	
	OSSet *		theControls;
	theControls = mDriverDMAEngine->defaultAudioControls;
	IOLog ("theControls count = %d\n", mDriverDMAEngine->defaultAudioControls->getCount ());
#endif
    flushAudioControls ();
	if (NULL != mExternalClockSelector) mExternalClockSelector->flushValue ();		// Specifically flush the clock selector's values because flushAudioControls() doesn't seem to call it... ???

	mAutoUpdatePRAM = TRUE;

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

	mCurrentOutputPlugin->prepareForOutputChange ();
	selectOutput (connectionCode);

	result = TRUE;

Exit:
	if (NULL != mInitHardwareThread) {
		thread_call_free (mInitHardwareThread);
	}

    debug2IOLog ("- AppleOnboardAudio::initHardware returns %d\n", result); 
	return (result);
}

IOReturn AppleOnboardAudio::configureDMAEngines(IOService *provider){
    IOReturn 						result;
    bool							hasInput;
	OSArray *						formatsArray;
	OSArray *						inputListArray;
    
    result = kIOReturnError;

	inputListArray = OSDynamicCast (OSArray, getLayoutEntry (kInputsList));
	hasInput = (NULL != inputListArray);

    debug3IOLog("+ AppleOnboardAudio[%p]::configureDMAEngines (%p)\n", this, provider);

	// All this config should go in a single method
    mDriverDMAEngine = new AppleDBDMAAudio;
    // make sure we get an engine
    FailIf (NULL == mDriverDMAEngine, Exit);

	formatsArray = OSDynamicCast (OSArray, getLayoutEntry (kFormats));

    if (!mDriverDMAEngine->init (NULL, mPlatformInterface, (IOService *)provider->getParentEntry (gIODTPlane), hasInput, formatsArray)) {
        mDriverDMAEngine->release ();
		mDriverDMAEngine = NULL;
        goto Exit;
    }
   
	result = kIOReturnSuccess;

Exit:
    debug4IOLog("- AppleOnboardAudio[%p]::configureDMAEngines (%p) returns %x\n", this, provider, result);
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
		charCode = '????';
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
	} else if (kDigitalInStatus == inCode) {
		charCode = kIOAudioInputPortSubTypeSPDIF;
	} else if (kDigitalOutStatus == inCode) {
		charCode = kIOAudioOutputPortSubTypeSPDIF;
	} else {
		charCode = '????';
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
	inputsList = OSDynamicCast (OSArray, getLayoutEntry (kInputsList));
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

	debug2IOLog("AppleOnboardAudio::createInputSelectorControl - mInputSelector = %p\n", mInputSelector);

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
	outputsList = OSDynamicCast (OSArray, getLayoutEntry (kOutputsList));
	FailIf (NULL == outputsList, Exit);

	outputsCount = outputsList->getCount ();
	outputString = OSDynamicCast (OSString, outputsList->getObject (0));
	FailIf (NULL == outputString, Exit);

	theDictionary = OSDictionary::withCapacity (outputsCount);
	FailIf (NULL == theDictionary, Exit);

	outputSelection = getCharCodeForString (outputString);
	mOutputSelector = IOAudioSelectorControl::createOutputSelector (outputSelection, kIOAudioControlChannelIDAll);
	if (NULL != mOutputSelector) {
		mDriverDMAEngine->addDefaultAudioControl (mOutputSelector);
		mOutputSelector->setValueChangeHandler ((IOAudioControl::IntValueChangeHandler)outputControlChangeHandler, this);
		for (index = 0; index < outputsCount; index++) {
			outputString = OSDynamicCast (OSString, outputsList->getObject (index));
			FailIf (NULL == outputString, Exit);
			outputSelection = getCharCodeForString (outputString);
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
			mOutputSelector->addAvailableSelection (outputSelection, outputString);
#else
			selectionString = getStringForCharCode (outputSelection);
			mOutputSelector->addAvailableSelection (outputSelection, selectionString);
			selectionString->release ();
#endif
		}
	}

	mDriverDMAEngine->setProperty ("MappingDictionary", theDictionary);

	debug2IOLog ("AppleOnboardAudio::createOutputSelectorControl - mOutputSelector = %p\n", mOutputSelector);

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
	
	dictEntry = OSDynamicCast (OSDictionary, getLayoutEntry (entry));
	FailIf (NULL == dictEntry, Exit);

	pluginIDMatch = OSDynamicCast (OSString, dictEntry->getObject (kPluginID));
	FailIf (NULL == pluginIDMatch, Exit);

	thePluginObject = getPluginObjectWithName (pluginIDMatch);
	
	debug2IOLog ("AppleOnboardAudio::getPluginObjectForConnection - pluginID = %s\n", pluginIDMatch->getCStringNoCopy());

Exit:
	return thePluginObject;
}

GpioAttributes AppleOnboardAudio::getInputDataMuxForConnection (const char * entry) {
	OSDictionary *						dictEntry;
	OSNumber *							inputDataMuxOSNumber;
	GpioAttributes						result;

	dictEntry = NULL;
	result = kGPIO_Unknown;
	
	dictEntry = OSDynamicCast (OSDictionary, getLayoutEntry (entry));
	FailIf (NULL == dictEntry, Exit);

	inputDataMuxOSNumber = OSDynamicCast (OSNumber, dictEntry->getObject (kInputDataMux));
	FailIf (NULL == inputDataMuxOSNumber, Exit);

	if ( 0 == inputDataMuxOSNumber->unsigned32BitValue() ) {
		result = kGPIO_MuxSelectDefault;
	} else {
		result = kGPIO_MuxSelectAlternate;
	}
	
	debug2IOLog ("AppleOnboardAudio::getInputDataMuxForConnection - GpioAttributes result = %d\n", result);

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
#ifdef kVERBOSE_LOG
			debug2IOLog ("AppleOnboardAudio found matching plugin with ID %s\n", thePluginID->getCStringNoCopy());
#endif
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
	IOFixed								mindBGain;
	IOFixed								maxdBGain;
	UInt32								curSelection;
	SInt32								minGain;
	SInt32								maxGain;

	result = kIOReturnError;

	curSelection = mInputSelector->getIntValue ();
	
	selectedInput = getConnectionKeyFromCharCode (curSelection, kIOAudioStreamDirectionInput);

	setUseInputGainControls (selectedInput);

	if (mUseInputGainControls) {
		thePluginObject = getPluginObjectForConnection (selectedInput);
		FailIf (NULL == thePluginObject, Exit);

		debug2IOLog ("creating input gain controls for input %s\n", selectedInput);
	
		mindBGain = thePluginObject->getMinimumdBGain ();
		maxdBGain = thePluginObject->getMaximumdBGain ();
		minGain = thePluginObject->getMinimumGain ();
		maxGain = thePluginObject->getMaximumGain ();
	
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
	
	theOutput = OSDynamicCast(OSDictionary, getLayoutEntry(inSelectedOutput));
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
	
	theOutput = OSDynamicCast(OSDictionary, getLayoutEntry (inSelectedOutput));
	FailIf (NULL == theOutput, Exit);

	theSignalProcessingDict = OSDynamicCast (OSDictionary, theOutput->getObject (kSignalProcessing));
	FailIf (NULL == theSignalProcessingDict, Exit);

	sprintf (speakerIDCString, "%s_%ld", kSpeakerID, mSpeakerID); 
	speakerIDString = OSString::withCString (speakerIDCString);
	FailIf (NULL == speakerIDString, Exit);
	
	theSpeakerIDDict = OSDynamicCast (OSDictionary, theSignalProcessingDict->getObject (speakerIDString));
	speakerIDString->release ();
	FailIf (NULL == theSpeakerIDDict, Exit);

	theMaxVolumeNumber = OSDynamicCast (OSNumber, theSpeakerIDDict->getObject (kMaxVolumeOffset));
	if (NULL != theMaxVolumeNumber) {
		debug2IOLog ("getMaxVolumeOffsetForOutput: theMaxVolumeNumber value = %d\n", theMaxVolumeNumber->unsigned32BitValue ());
		maxVolumeOffset = theMaxVolumeNumber->unsigned32BitValue ();
	} 

Exit:
	return maxVolumeOffset;
}

void AppleOnboardAudio::setSoftwareOutputDSP (const char * inSelectedOutput) {
	OSDictionary *						theSpeakerIDDict;
	OSDictionary *						theSignalProcessingDict;
	OSDictionary *						theEQDict;
	OSDictionary *						theDynamicsDict;
	OSDictionary *						theCrossoverDict;
	OSDictionary *						theOutput;
	OSDictionary *						theSoftwareDSPDict;
	OSString *							speakerIDString;
	char								speakerIDCString[32];
	
	debug2IOLog ("set output DSP for '%s'.\n", inSelectedOutput);
	// check if we already have calculated coefficients for this output
	// this will NOT work for more than one output having processing on it
	if (mCurrentProcessingOutputString->isEqualTo (inSelectedOutput)) {
		
		debugIOLog ("Enabling DSP\n");
	
		mDriverDMAEngine->enableSoftwareEQ ();
		mDriverDMAEngine->enableSoftwareLimiter ();
		mCurrentOutputPlugin->enableEQ ();
		debug2IOLog ("mCurrentProcessingOutputString is '%s', coefficients not updated.\n", mCurrentProcessingOutputString->getCStringNoCopy ());
	} else {

		debugIOLog ("Disabling DSP\n");

		// commmon case is disabled, this is the safer fail senario
		mDriverDMAEngine->disableSoftwareEQ ();
		mDriverDMAEngine->disableSoftwareLimiter ();
		mCurrentOutputPlugin->disableEQ ();
		debugIOLog ("processing disabled.\n");
	
		theOutput = OSDynamicCast(OSDictionary, getLayoutEntry (inSelectedOutput));
		FailIf (NULL == theOutput, Exit);
	
		theSignalProcessingDict = OSDynamicCast (OSDictionary, theOutput->getObject (kSignalProcessing));
		FailIf (NULL == theSignalProcessingDict, Exit);
	
		sprintf (speakerIDCString, "%s_%ld", kSpeakerID, mSpeakerID); 
		debug2IOLog ("setSoftwareOutputDSP: speakerIDString = %s\n", speakerIDCString);
		speakerIDString = OSString::withCString (speakerIDCString);
		FailIf (NULL == speakerIDString, Exit);
		
		theSpeakerIDDict = OSDynamicCast (OSDictionary, theSignalProcessingDict->getObject (speakerIDString));
		speakerIDString->release ();
		FailIf (NULL == theSpeakerIDDict, Exit);
		debug2IOLog ("setSoftwareOutputDSP: theSpeakerIDDict = %p\n", theSpeakerIDDict);
	
		theSoftwareDSPDict = OSDynamicCast (OSDictionary, theSpeakerIDDict->getObject (kSoftwareDSP));
		FailIf (NULL == theSoftwareDSPDict, Exit);
		debug2IOLog ("setSoftwareOutputDSP: theSoftwareDSPDict = %p\n", theSoftwareDSPDict);
		
		theEQDict = OSDynamicCast(OSDictionary, theSoftwareDSPDict->getObject (kEqualization));
		if (NULL != theEQDict) {
			debug2IOLog ("setSoftwareOutputDSP: theEQDict = %p\n", theEQDict);
			mDriverDMAEngine->setEqualizationFromDictionary (theEQDict);
		}
		
		theDynamicsDict = OSDynamicCast(OSDictionary, theSoftwareDSPDict->getObject (kDynamicRange));
		if (NULL != theDynamicsDict) {
			debug2IOLog ("setSoftwareOutputDSP: theDynamicsDict = %p\n", theDynamicsDict);
			mDriverDMAEngine->setLimiterFromDictionary (theDynamicsDict);
			
			theCrossoverDict = OSDynamicCast(OSDictionary, theDynamicsDict->getObject (kCrossover));
			// this handles the case of theCrossoverDict = NULL, setting the crossover state to 1 band
			mDriverDMAEngine->setCrossoverFromDictionary (theCrossoverDict);
		}
		
		debugIOLog ("Processing set\n");
		
		// if we get here, we've found some DSP to perform for this output, so update the currently prepared output string
		debug2IOLog ("mCurrentProcessingOutputString is '%s'\n", mCurrentProcessingOutputString->getCStringNoCopy ());
		mCurrentProcessingOutputString->initWithCString (inSelectedOutput);
		debug2IOLog ("mCurrentProcessingOutputString set to '%s', coefficients will be updated.\n", mCurrentProcessingOutputString->getCStringNoCopy ());
	}	

Exit:
	return;
}

UInt32 AppleOnboardAudio::setClipRoutineForOutput (const char * inSelectedOutput) {
	OSDictionary *						theOutput;
	OSString *							clipRoutineString;
	OSArray *							theArray;
	OSData *							volumeData;
	IOReturn							result;
	UInt32								arrayCount;
	UInt32								index;
	UInt32								volume;
	
	result = kIOReturnSuccess;
	theArray = NULL;
	
	theOutput = OSDynamicCast(OSDictionary, getLayoutEntry(inSelectedOutput));
	FailIf (NULL == theOutput, Exit);
	
	theArray = OSDynamicCast(OSArray, theOutput->getObject(kClipRoutines));
	FailIf (NULL == theArray, Exit);
	
	arrayCount = theArray->getCount();

	mDriverDMAEngine->resetOutputClipOptions();
	
	for (index = 0; index < arrayCount; index++) {
		clipRoutineString = OSDynamicCast(OSString, theArray->getObject(index));
		debug3IOLog("getClipRoutineForOutput: clip routine[%ld] = %s\n", index, clipRoutineString->getCStringNoCopy());

		if (clipRoutineString->isEqualTo (kPhaseInversionClipString)) {
			mDriverDMAEngine->setPhaseInversion (true);
		} else if (clipRoutineString->isEqualTo (kStereoToRightChanClipString)) {
			mDriverDMAEngine->setRightChanMixed (true);
		} else if (clipRoutineString->isEqualTo (kDelayRightChan1SampleClipString)) {
			mDriverDMAEngine->setRightChanDelay (true);
		} else if (clipRoutineString->isEqualTo (kBalanceAdjustClipString)) {
			mDriverDMAEngine->setBalanceAdjust (true);
			volumeData = OSDynamicCast(OSData, theOutput->getObject(kLeftBalanceAdjust));
			if (NULL != volumeData) {
				memcpy (&(volume), volumeData->getBytesNoCopy (), 4);
				mDriverDMAEngine->setLeftSoftVolume ((float *)&volume);
			}
			volumeData = OSDynamicCast(OSData, theOutput->getObject(kRightBalanceAdjust));
			if (NULL != volumeData) {
				memcpy (&(volume), volumeData->getBytesNoCopy (), 4);
				mDriverDMAEngine->setRightSoftVolume ((float *)&volume);
			}
		}
	}	

	result = kIOReturnSuccess;
Exit:
	debug2IOLog("getClipRoutineForOutput returns %X\n", result);
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
	
	theInput = OSDynamicCast(OSDictionary, getLayoutEntry(inSelectedInput));
	FailIf (NULL == theInput, Exit);
	
	theArray = OSDynamicCast(OSArray, theInput->getObject(kClipRoutines));
	FailIf (NULL == theArray, Exit);
	
	arrayCount = theArray->getCount();

	mDriverDMAEngine->resetInputClipOptions();
	
	for (index = 0; index < arrayCount; index++) {
		clipRoutineString = OSDynamicCast(OSString, theArray->getObject(index));
		debug3IOLog("getClipRoutineForInput: clip routine[%ld] = %s\n", index, clipRoutineString->getCStringNoCopy());

		if (clipRoutineString->isEqualTo (kCopyLeftToRight)) {
			mDriverDMAEngine->setDualMonoMode (e_Mode_CopyLeftToRight);
		} else if (clipRoutineString->isEqualTo (kCopyRightToLeft)) {
			mDriverDMAEngine->setDualMonoMode (e_Mode_CopyRightToLeft);
		}	
	}	

	result = kIOReturnSuccess;
Exit:
	debug2IOLog("getClipRoutineForInput returns %X\n", result);
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

	debugIOLog ("+ AppleOnboardAudio::createOutputVolumeControls\n");

	result = kIOReturnError;
	FailIf (NULL == mOutputSelector, Exit);
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

Exit:

	debugIOLog ("- AppleOnboardAudio::createOutputVolumeControls\n");
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
	
	debugIOLog("+ AppleOnboardAudio::createDefaultControls\n");

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

	createOutputSelectorControl ();

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
		// no value change handler because this isn't a settable control
		// Don't release it because we might reference it later
	}

	// [3276398] only build control if we DON"T have a combo jack
	if (kGPIO_Unknown == mPlatformInterface->getComboOutJackTypeConnected ()) {
		outHeadLineDigExclusiveControl = IOAudioToggleControl::create (mHeadLineDigExclusive, kIOAudioControlChannelIDAll, kIOAudioControlChannelNameAll, 0, 'hpex', kIOAudioControlUsageOutput);
		if (NULL != outHeadLineDigExclusiveControl) {
			mDriverDMAEngine->addDefaultAudioControl (outHeadLineDigExclusiveControl);
			outHeadLineDigExclusiveControl->setValueChangeHandler ((IOAudioControl::IntValueChangeHandler)outputControlChangeHandler, this);
			outHeadLineDigExclusiveControl->release ();
		}
	}

	inputListArray = OSDynamicCast (OSArray, getLayoutEntry (kInputsList));
	hasInput = (NULL != inputListArray);
	
	if (hasInput) {
		createInputSelectorControl ();
	
		createInputGainControls ();
	}

	clockSelectBoolean = OSDynamicCast ( OSBoolean, getLayoutEntry (kExternalClockSelect) );
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
    debug2IOLog("- %d = AppleOnboardAudio::createDefaultControls\n", result);
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
	Boolean								headphonesOrLineOutConnected;

	FailIf (NULL == mDriverDMAEngine, Exit);

	mindBVol = thePluginObject->getMinimumdBVolume ();
	maxdBVol = thePluginObject->getMaximumdBVolume ();
	minVolume = thePluginObject->getMinimumVolume ();
	maxVolume = thePluginObject->getMaximumVolume ();

	maxVolume += getMaxVolumeOffsetForOutput (selectionCode);

	debug5IOLog ("AppleOnboardAudio::AdjustOutputVolumeControls - mindBVol %lX, maxdBVol %lX, minVolume %ld, maxVolume %ld\n", mindBVol, maxdBVol, minVolume, maxVolume);

	mDriverDMAEngine->pauseAudioEngine ();
	mDriverDMAEngine->beginConfigurationChange ();

	hasMaster = hasMasterVolumeControl (selectionCode);
	hasLeft = hasLeftVolumeControl (selectionCode);
	hasRight = hasRightVolumeControl (selectionCode);

	// [3339273], don't remove stereo controls if headphones or lineout are connected
	if ((kGPIO_Connected == mPlatformInterface->getHeadphoneConnected ()) || (kGPIO_Connected == mPlatformInterface->getLineOutConnected ())) {
		headphonesOrLineOutConnected = true;
	} else {
		headphonesOrLineOutConnected = false;
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
			if (!headphonesOrLineOutConnected) {
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
			if (!headphonesOrLineOutConnected) {
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

	debug5IOLog ("AppleOnboardAudio::AdjustInputGainControls - mindBGain %lX, maxdBGain %lX, minGain %ld, maxGain %ld\n", mindBGain, maxdBGain, minGain, maxGain);

	mDriverDMAEngine->pauseAudioEngine ();
	mDriverDMAEngine->beginConfigurationChange ();

	removePlayThruControl ();
	//	[3281535]	begin {
	if ( mUsePlaythroughControl ) {
		createPlayThruControl ();
	}
	//	[3281535]	} end

	if (mUseInputGainControls) {
		debugIOLog ("AdjustInputGainControls - creating input gain controls.\n");
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
	} else {
		debugIOLog ("AdjustInputGainControls - removing input gain controls.\n");
		removeLeftGainControl();
		removeRightGainControl();
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
	FAIL_IF (NULL == start, Exit);

	iterator = start->getChildIterator (gIOServicePlane);
	FAIL_IF (NULL == iterator, Exit);

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
	
	debug2IOLog("createLeftVolumeControl - leftVol = %ld\n", leftVol);

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

	debug2IOLog("createRightVolumeControl - rightVol = %ld\n", rightVol);

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
	
	debug2IOLog("createMasterVolumeControl - masterVol = %ld\n", masterVol);
	
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
	
	debug2IOLog("createLeftGainControl - leftVol = %ld\n", leftGain);

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
	
	debug2IOLog("createRightGainControl - rightVol = %ld\n", rightGain);

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

IOReturn AppleOnboardAudio::outputControlChangeHandler (IOService *target, IOAudioControl *control, SInt32 oldValue, SInt32 newValue) {
	IOReturn						result;
	AppleOnboardAudio *				audioDevice;
	IOAudioLevelControl *			levelControl;
	IODTPlatformExpert * 			platform;
	UInt32							leftVol;
	UInt32							rightVol;
	Boolean							wasPoweredDown;
	UInt32							subType;

	debug5IOLog ("+ AppleOnboardAudio::outputControlChangeHandler (%p, %p, %lX, %lX)\n", target, control, oldValue, newValue);

	result = kIOReturnError;
	audioDevice = OSDynamicCast (AppleOnboardAudio, target);
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
		if (NULL != audioDevice->mExternalClockSelector) {
			audioDevice->mExternalClockSelector->flushValue ();
		}
		if (NULL != audioDevice->mOutMuteControl) {
			audioDevice->mOutMuteControl->flushValue ();					// Restore hardware to the user's selected state
		}

		wasPoweredDown = TRUE;
	} else {
		wasPoweredDown = FALSE;
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
//									IOLog ("Muting because volume is 0\n");
									result = audioDevice->outputMuteChange (1);
								}
							} else if (oldValue == levelControl->getMinValue () && FALSE == audioDevice->mIsMute) {
								OSNumber *			muteState;
								muteState = OSNumber::withNumber ((long long unsigned int)0, 32);
								if (NULL != audioDevice->mOutMuteControl) {
									audioDevice->mOutMuteControl->hardwareValueChanged (muteState);
									result = audioDevice->outputMuteChange (0);
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
					result = audioDevice->outputMuteChange (newValue);
					break;
				case 'hpex':	
					audioDevice->mHeadLineDigExclusive = newValue;
					// [3279525] update the amps
					audioDevice->selectOutput (audioDevice->mOutputSelector->getIntValue ());
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
	debug6IOLog ("- AppleOnboardAudio::outputControlChangeHandler (%p, %p, %lX, %lX) returns %X\n", target, control, oldValue, newValue, result);

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

	debug2IOLog ("+ AppleOnboardAudio::outputSelectorChanged(%4s)\n", (char *)&newValue);

	result = kIOReturnError;
	inputLatency = 0;
	
	// [3318095], only allow selections to connected devices
	if (kIOAudioOutputPortSubTypeLine == newValue) {
		if ( !(mDetectCollection & kSndHWLineOutput) ) {
			return kIOReturnError;
		}	
	} else if (kIOAudioOutputPortSubTypeHeadphones == newValue) {
		if ( !(mDetectCollection & kSndHWCPUHeadphone) ) {
			return kIOReturnError;
		}	
	}
	// [3326566], don't allow internal speaker selection if headphones or line out are connected
	if (kIOAudioOutputPortSubTypeInternalSpeaker == newValue) {
		if ( (mDetectCollection & kSndHWLineOutput) || (mDetectCollection & kSndHWCPUHeadphone) ) {
			return kIOReturnError;
		}	
	}

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

	mCurrentOutputPlugin->prepareForOutputChange();

	selectOutput(newValue, FALSE);

	result = kIOReturnSuccess;

Exit:
	debug2IOLog ("- AppleOnboardAudio::outputSelectorChanged returns %X\n", result);
	return result;
}

// This is called when we're on hardware that only has one active volume control (either right or left)
// otherwise the respective right or left volume handler will be called.
// This calls both volume handers becasue it doesn't know which one is really the active volume control.
IOReturn AppleOnboardAudio::volumeMasterChange (SInt32 newValue) {
	IOReturn						result = kIOReturnSuccess;

	debug2IOLog("+ AppleOnboardAudio::volumeMasterChange (%ld)\n", newValue);

	result = kIOReturnError;

	// Don't know which volume control really exists, so adjust both -- they'll ignore the change if they don't exist
	result = volumeLeftChange (newValue);
	result = volumeRightChange (newValue);

	result = kIOReturnSuccess;

	debug2IOLog ("- AppleOnboardAudio::volumeMasterChange, 0x%x\n", result);
	return result;
}

IOReturn AppleOnboardAudio::volumeLeftChange (SInt32 newValue) {
	IOReturn							result;
	UInt32 								count;
	UInt32 								index;
	AudioHardwareObjectInterface* 		thePluginObject;

	debug2IOLog ("+ AppleOnboardAudio::volumeLeftChange (%ld)\n", newValue);

	// [3339273] set volume on all plugins
	//mCurrentOutputPlugin->setVolume (newValue, mVolRight);
	if (NULL != mPluginObjects) {
		count = mPluginObjects->getCount ();
		for (index = 0; index < count; index++) {
			thePluginObject = getIndexedPluginObject (index);
			if((NULL != thePluginObject)) {
				thePluginObject->setVolume (newValue, mVolRight);
				debug2IOLog ("AppleOnboardAudio::willTerminate terminated  (%p)\n", thePluginObject);
			}
		}
	}

	mVolLeft = newValue;

	result = kIOReturnSuccess;

	debug2IOLog("- AppleOnboardAudio::volumeLeftChange, 0x%x\n", result);

	return result;
}

IOReturn AppleOnboardAudio::volumeRightChange (SInt32 newValue) {
	IOReturn							result;
	UInt32 								count;
	UInt32 								index;
	AudioHardwareObjectInterface* 		thePluginObject;

	debug2IOLog ("+ AppleOnboardAudio::volumeRightChange (%ld)\n", newValue);

	// [3339273] set volume on all plugins
	//mCurrentOutputPlugin->setVolume (mVolLeft, newValue);
	if (NULL != mPluginObjects) {
		count = mPluginObjects->getCount ();
		for (index = 0; index < count; index++) {
			thePluginObject = getIndexedPluginObject (index);
			if((NULL != thePluginObject)) {
				thePluginObject->setVolume (mVolLeft, newValue);
				debug2IOLog ("AppleOnboardAudio::willTerminate terminated  (%p)\n", thePluginObject);
			}
		}
	}

	mVolRight = newValue;

	result = kIOReturnSuccess;

	debug2IOLog ("- AppleOnboardAudio::volumeRightChange, result = 0x%x\n", result);

	return result;
}

IOReturn AppleOnboardAudio::outputMuteChange (SInt32 newValue) {
	AudioHardwareObjectInterface *		thePluginObject;
	IOReturn							result;
	UInt32								index;
	UInt32								count;

	debug2IOLog ("+ AppleOnboardAudio::outputMuteChange (%ld)\n", newValue);

    result = kIOReturnError;

	// [3253678], mute amps, and mute them before muting the parts
	if (0 != newValue) {
		muteAnalogOuts ();
	} 
	FailIf (NULL == mPluginObjects, Exit);
	count = mPluginObjects->getCount ();
	for (index = 0; index < count; index++) {
		thePluginObject = getIndexedPluginObject (index);
		FailIf (NULL == thePluginObject, Exit);
		thePluginObject->setMute (newValue);
		debug2IOLog ("thePluginObject->setMute (%ld)\n", newValue);
	}

	mIsMute = newValue;

	// [3253678], unmute amps after unmuting the parts
	if (0 == newValue) {
		if (NULL != mOutputSelector) {
			(void)mCurrentOutputPlugin->prepareForOutputChange ();
			selectOutput (mOutputSelector->getIntValue ());
		}
	}

	result = kIOReturnSuccess;
Exit:
	debug2IOLog ("- AppleOnboardAudio::outputMuteChange, 0x%x\n", result);
    return result;
}

IOReturn AppleOnboardAudio::inputControlChangeHandler (IOService *target, IOAudioControl *control, SInt32 oldValue, SInt32 newValue) {
	IOReturn						result = kIOReturnError;
	AppleOnboardAudio *				audioDevice;
	IOAudioLevelControl *			levelControl;
	Boolean							wasPoweredDown;

	debug5IOLog ("+ AppleOnboardAudio::inputControlChangeHandler (%p, %p, %lX, %lX)\n", target, control, oldValue, newValue);

	audioDevice = OSDynamicCast (AppleOnboardAudio, target);
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
		if (NULL != audioDevice->mExternalClockSelector) {
			audioDevice->mExternalClockSelector->flushValue ();
		}
		if (NULL != audioDevice->mOutMuteControl) {
			audioDevice->mOutMuteControl->flushValue ();					// Restore hardware to the user's selected state
		}

		wasPoweredDown = TRUE;
	} else {
		wasPoweredDown = FALSE;
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
			debugIOLog ("input selector change handler\n");
			switch (control->getSubType ()) {
				case kIOAudioSelectorControlSubTypeInput:
					result = audioDevice->inputSelectorChanged (newValue);
					break;
				case kIOAudioSelectorControlSubTypeClockSource:
					result = audioDevice->clockSelectorChanged (newValue);
					break;
				default:
					debugIOLog ("unknown control type in input selector change handler\n");
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

    debugIOLog ("+ AppleOnboardAudio::gainLeftChanged\n");    

	if (mCurrentPluginHasSoftwareInputGain) {
		mDriverDMAEngine->setInputGainL (newValue);
	} else {
		mCurrentInputPlugin->setInputGain (newValue, mGainRight);
	}
    mGainLeft = newValue;

	result = kIOReturnSuccess;

    debug2IOLog ("- AppleOnboardAudio::gainLeftChanged, %d\n", (result == kIOReturnSuccess));
    return result;
}

IOReturn AppleOnboardAudio::gainRightChanged (SInt32 newValue) {
	IOReturn							result;

    debugIOLog ("+ AppleOnboardAudio::gainRightChanged\n");    

	if (mCurrentPluginHasSoftwareInputGain) {
		mDriverDMAEngine->setInputGainR (newValue);
	} else {
		mCurrentInputPlugin->setInputGain (newValue, mGainRight);
	}
    mGainRight = newValue;

	result = kIOReturnSuccess;

    debug2IOLog ("- AppleOnboardAudio::gainRightChanged, %d\n", (result == kIOReturnSuccess));
    return result;
}

IOReturn AppleOnboardAudio::passThruChanged (SInt32 newValue) {
	IOReturn							result;

    debugIOLog ("+ AppleOnboardAudio::passThruChanged\n");

    result = kIOReturnError;

	mCurrentInputPlugin->setPlayThrough (!newValue);

	result = kIOReturnSuccess;

    debugIOLog ("- AppleOnboardAudio::passThruChanged\n");
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

	debug2IOLog ("+ AppleOnboardAudio::inputSelectorChanged (%4s)\n", (char *)&newValue);

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
		
		mDriverDMAEngine->pauseAudioEngine ();
		mDriverDMAEngine->beginConfigurationChange();
	
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

		mDriverDMAEngine->completeConfigurationChange();
		mDriverDMAEngine->resumeAudioEngine ();

		mCurrentInputPlugin = thePluginObject;
		debug2IOLog ("+ AppleOnboardAudio::inputSelectorChanged - mCurrentInputPlugin updated to %p\n", mCurrentInputPlugin);
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
	debugIOLog ("- AppleOnboardAudio::inputSelectorChanged\n");
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

	debug2IrqIOLog ("+ AppleOnboardAudio::clockSelectorChanged (%4s)\n", (char *)&newValue);

	result = kIOReturnError;

	mClockSelectInProcessSemaphore = true;	//	block 'UNLOCK' errors while switching clock sources
	
	FailIf ( NULL == mDriverDMAEngine, Exit );
	FailIf ( NULL == mTransportInterface, Exit );
	if ( mCurrentClockSelector != newValue ) {

		muteAnalogOuts (); // [3253678], mute outputs during clock selection

		if ( kClockSourceSelectionInternal == newValue ) {
			mDriverDMAEngine->pauseAudioEngine ();
			IOSleep ( 10 );
			mDriverDMAEngine->beginConfigurationChange ();
			mTransportInterface->transportBreakClockSelect ( kTRANSPORT_MASTER_CLOCK );
			callPluginsInOrder ( kBreakClockSelect, kTRANSPORT_MASTER_CLOCK );
			
			mTransportInterface->transportMakeClockSelect ( kTRANSPORT_MASTER_CLOCK );
			callPluginsInOrder ( kMakeClockSelect, kTRANSPORT_MASTER_CLOCK );
			mDriverDMAEngine->completeConfigurationChange ();
			mDriverDMAEngine->resumeAudioEngine ();
			
			// [3253678], safe to unmute analog part when going to internal clock
			mCurrentOutputPlugin->prepareForOutputChange ();
			selectOutput (mOutputSelector->getIntValue ());
						
		} else if ( kClockSourceSelectionExternal == newValue ) {
			mDriverDMAEngine->pauseAudioEngine ();
			IOSleep ( 10 );
			mDriverDMAEngine->beginConfigurationChange ();
			mTransportInterface->transportBreakClockSelect ( kTRANSPORT_SLAVE_CLOCK );
			callPluginsInOrder ( kBreakClockSelect, kTRANSPORT_SLAVE_CLOCK );
			
			mTransportInterface->transportMakeClockSelect ( kTRANSPORT_SLAVE_CLOCK );
			callPluginsInOrder ( kMakeClockSelect, kTRANSPORT_SLAVE_CLOCK );
			mDriverDMAEngine->completeConfigurationChange ();
			mDriverDMAEngine->resumeAudioEngine ();
		} else {
			debugIOLog ( "Unknown clock source selection.\n" );
			FailIf (TRUE, Exit);
		}

		mCurrentClockSelector = newValue;
	}
	result = kIOReturnSuccess;
Exit:
	mClockSelectInProcessSemaphore = false;	//	enable 'UNLOCK' errors after switching clock sources
	debugIrqIOLog ("- AppleOnboardAudio::inputSelectorChanged\n");
	return result;
}

UInt32 AppleOnboardAudio::getCurrentSampleFrame (void) {
	return mCurrentOutputPlugin->getCurrentSampleFrame ();
}

void AppleOnboardAudio::setCurrentSampleFrame (UInt32 inValue) {
	return mCurrentOutputPlugin->setCurrentSampleFrame (inValue);
}

void AppleOnboardAudio::setInputDataMuxForConnection ( char * connectionString ) {
	GpioAttributes		theMuxSelect;
	
	theMuxSelect = getInputDataMuxForConnection ( connectionString );
	if ( kGPIO_Unknown != theMuxSelect ) {
		debug2IOLog ( "AppleOnboardAudio::setInputDataMuxForConnection setting input data mux to %d\n", (unsigned int)theMuxSelect );
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
		debugIOLog ("setting power aggressivness state to ");
		switch (newLevel) {
			case kIOPMInternalPower:								// Running on battery only
				debugIOLog ("battery power\n");
				idleSleepDelayTime = kBatteryPowerDownDelayTime;
				setIdleAudioSleepTime (idleSleepDelayTime);
				if (getPowerState () != kIOAudioDeviceActive) {
					performPowerStateChange (getPowerState (), kIOAudioDeviceIdle, &time);
				}
				break;
			case kIOPMExternalPower:								// Running on AC power
				debugIOLog ("wall power\n");
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

	debug4IOLog ("+ AppleOnboardAudio::performPowerStateChange (%d, %d) -- ourPowerState = %d\n", oldPowerState, newPowerState, ourPowerState);

	*microsecondsUntilComplete = 2000000;

	result = super::performPowerStateChange (oldPowerState, newPowerState, microsecondsUntilComplete);
    
	if (mPowerThread) {
		thread_call_enter1 (mPowerThread, (void *)newPowerState);
	}

	debug2IOLog ("- AppleOnboardAudio::performPowerStateChange -- ourPowerState = %d\n", ourPowerState);

	return result; 
}

void AppleOnboardAudio::performPowerStateChangeThread (AppleOnboardAudio * aoa, void * newPowerState) {
	IOCommandGate *						cg;
	IOReturn							result;

	debug3IOLog ("+ AppleOnboardAudio::performPowerStateChangeThread (%p, %ld)\n", aoa, (UInt32)newPowerState);

	FailIf (NULL == aoa, Exit);

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

	aoa = (AppleOnboardAudio *)us;
	FailIf (NULL == aoa, Exit);

//	kprintf ("+AppleOnboardAudio::performPowerStateChangeThreadAction (%p, %ld), %d\n", owner, (UInt32)newPowerState, aoa->ourPowerState);
	debug4IOLog ("+ AppleOnboardAudio::performPowerStateChangeThreadAction (%p, %ld) -- ourPowerState = %d\n", owner, (UInt32)newPowerState, aoa->ourPowerState);

	FailIf (NULL == aoa->mTransportInterface, Exit);

	switch ((UInt32)newPowerState) {
		case kIOAudioDeviceSleep:
			//	Sleep requires that the hardware plugin object(s) go to sleep
			//	prior to the transport object going to sleep.
			if (kIOAudioDeviceActive == aoa->ourPowerState) {
				aoa->outputMuteChange (TRUE);			// Mute before turning off power
				result = aoa->callPluginsInReverseOrder (kPowerStateChange, kIOAudioDeviceSleep);
				if ( NULL != aoa->mTransportInterface ) { 
					result = aoa->mTransportInterface->performTransportSleep ();
				}
				aoa->ourPowerState = kIOAudioDeviceSleep;
			}
			break;
		case kIOAudioDeviceIdle:
			if (kIOAudioDeviceActive == aoa->ourPowerState) {
				aoa->outputMuteChange (TRUE);			// Mute before turning off power
				//	Sleep requires that the hardware plugin object(s) go to sleep
				//	prior to the transport object going to sleep.
				result = aoa->callPluginsInReverseOrder (kPowerStateChange, kIOAudioDeviceSleep);
				if ( NULL != aoa->mTransportInterface ) { 
					result = aoa->mTransportInterface->performTransportSleep ();
				}
				aoa->ourPowerState = kIOAudioDeviceSleep;
			} else if (kIOAudioDeviceSleep == aoa->ourPowerState && kNoIdleAudioPowerDown == aoa->idleSleepDelayTime) {
				//	Wake requires that the transport object go active prior
				//	to the hardware plugin object(s) going active.
				if ( NULL != aoa->mTransportInterface ) {
					result = aoa->mTransportInterface->performTransportWake ();
				}
				result = aoa->callPluginsInOrder (kPowerStateChange, kIOAudioDeviceActive);
				if (NULL != aoa->mExternalClockSelector) {
					aoa->mExternalClockSelector->flushValue ();
				}
				aoa->ourPowerState = kIOAudioDeviceActive;
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
				if (NULL != aoa->mExternalClockSelector) {
					aoa->mExternalClockSelector->flushValue ();
				}
				if (NULL != aoa->mOutMuteControl) {
					aoa->mOutMuteControl->flushValue ();					// Restore hardware to the user's selected state
				}
			} else {
				debugIOLog ("trying to wake, but we're already awake\n");
			}
			break;
		default:
			break;
	}

	aoa->setProperty ("IOAudioPowerState", aoa->ourPowerState, 32);

Exit:
	aoa->protectedCompletePowerStateChange ();

//	kprintf ("-AppleOnboardAudio::performPowerStateChangeThreadAction (%p, %ld), %d\n", owner, (UInt32)newPowerState, aoa->ourPowerState);
	debug4IOLog ("- AppleOnboardAudio::performPowerStateChangeThreadAction (%p, %ld) -- ourPowerState = %d\n", owner, (UInt32)newPowerState, aoa->ourPowerState);
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

	debug3IOLog ( "+ AppleOnboardAudio::VolumeToPRAMValue ( 0x%X, 0x%X )\n", (unsigned int)inLeftVol, (unsigned int)inRightVol );
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
		debug3IOLog ( "... leftVol = 0x%X, rightVol = 0x%X\n", (unsigned int)leftVol, (unsigned int)rightVol );
	
		if (NULL != mOutMasterVolumeControl) {
			volumeRange = (mOutMasterVolumeControl->getMaxValue () - mOutMasterVolumeControl->getMinValue () + 1);
			debug2IOLog ( "... mOutMasterVolumeControl volumeRange = 0x%X\n", (unsigned int)volumeRange );
		} else if (NULL != mOutLeftVolumeControl) {
			volumeRange = (mOutLeftVolumeControl->getMaxValue () - mOutLeftVolumeControl->getMinValue () + 1);
			debug2IOLog ( "... mOutLeftVolumeControl volumeRange = 0x%X\n", (unsigned int)volumeRange );
		} else if (NULL != mOutRightVolumeControl) {
			volumeRange = (mOutRightVolumeControl->getMaxValue () - mOutRightVolumeControl->getMinValue () + 1);
			debug2IOLog ( "... mOutRightVolumeControl volumeRange = 0x%X\n", (unsigned int)volumeRange );
		} else {
			volumeRange = kMaximumPRAMVolume;
			debug2IOLog ( "... volumeRange = 0x%X **** NO AUDIO LEVEL CONTROLS!\n", (unsigned int)volumeRange );
		}

		averageVolume = (leftVol + rightVol) >> 1;		// sum the channel volumes and get an average
		debug2IOLog ( "... averageVolume = 0x%X\n", (unsigned int)volumeRange );
		debug3IOLog ( "... volumeRange %X, kMaximumPRAMVolume %X\n", (unsigned int)volumeRange, (unsigned int)kMaximumPRAMVolume );
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
	debug2IOLog ( "- AppleOnboardAudio::VolumeToPRAMValue returns 0x%X\n", (unsigned int)pramVolume );
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
    
    debug3IOLog("+ AppleOnboardAudio::WritePRAMVol leftVol=%lu, rightVol=%lu\n",leftVol,  rightVol);
    
    if (platform) {
		debug2IOLog ( "... platform 0x%X\n", (unsigned int)platform );
		pramVolume = VolumeToPRAMValue (leftVol, rightVol);
#if 0
		curPRAMVol = pramVolume ^ 0xFF;
		debug3IOLog ( "... target pramVolume = 0x%X, curPRAMVol = 0x%X\n", pramVolume, curPRAMVol );
#endif
		// get the old value to compare it with
		err = platform->readXPRAM((IOByteCount)kPRamVolumeAddr, &curPRAMVol, (IOByteCount)1);
		if ( kIOReturnSuccess == err ) {
			debug2IOLog ( "... curPRAMVol = 0x%X before write\n", (curPRAMVol & 0x07) );
			// Update only if there is a change
			if (pramVolume != (curPRAMVol & 0x07)) {
				// clear bottom 3 bits of volume control byte from PRAM low memory image
				curPRAMVol = (curPRAMVol & 0xF8) | pramVolume;
				debug2IOLog("... curPRAMVol = 0x%x\n",curPRAMVol);
				// write out the volume control byte to PRAM
				err = platform->writeXPRAM((IOByteCount)kPRamVolumeAddr, &curPRAMVol,(IOByteCount) 1);
				if ( kIOReturnSuccess != err ) {
					debug5IOLog ( "0x%X = platform->writeXPRAM( 0x%X, & 0x%X, 1 ), value = 0x%X\n", err, (unsigned int)kPRamVolumeAddr, (unsigned int)&curPRAMVol, (unsigned int)curPRAMVol );
				} else {
#if 0
					err = platform->readXPRAM((IOByteCount)kPRamVolumeAddr, &curPRAMVol, (IOByteCount)1);
					if ( kIOReturnSuccess == err ) {
						if ( ( 0x07 & curPRAMVol ) != pramVolume ) {
							debug3IOLog ( "PRAM Read after Write did not compare:  Write = 0x%X, Read = 0x%X\n", (unsigned int)pramVolume, (unsigned int)curPRAMVol );
						} else {
							debugIOLog ( "PRAM verified after write!\n" );
						}
					} else {
						debugIOLog ( "Could not readXPRAM to verify write!\n" );
					}
#endif
					mUCState.ucPramData = (UInt32)curPRAMVol;
					mUCState.ucPramVolume = mUCState.ucPramData & 0x00000007;
				}
			} else {
				debugIOLog ( "PRAM write request is to current value: no I/O\n" );
			}
		} else {
			debug2IOLog ( "Could not readXPRAM prior to write! Error 0x%X\n", err );
		}
	} else {
		debugIOLog ( "... no platform\n" );
	}
    debugIOLog("- AppleOnboardAudio::WritePRAMVol\n");
}
/*
void AppleOnboardAudio::WritePRAMVol (UInt32 leftVol, UInt32 rightVol) {
	OSData *					value;
	OSIterator *				iter;
	OSSymbol *					pramName;
	UInt8						pramValue;
	bool						result;

	iter = getMatchingServices (serviceMatching ("IODTNVRAM"));
	if (iter) {
		IODTNVRAM *			nvram;
		nvram = OSDynamicCast (IODTNVRAM, iter->getNextObject ());
		if (nvram) {
			pramName = (OSSymbol *)OSSymbol::withCStringNoCopy ("boot-volume");
			pramValue = VolumeToPRAMValue (leftVol, rightVol);
			IOLog ("pramValue = %d\n", pramValue);
			value = OSData::withBytes (&pramValue, 8);
			result = nvram->setProperty (pramName, value);
			pramName->release ();
			value->release ();
			IOLog ("result = %d\n", result);
		}
		iter->release ();
	}
}
*/
UInt8 AppleOnboardAudio::ReadPRAMVol (void) {
	UInt8 *						curPRAMVol;
	UInt8						volbackingstore;
	OSData *					value;
	OSIterator *				iter;

	curPRAMVol = &volbackingstore;
	*curPRAMVol = 0;

	iter = getMatchingServices (serviceMatching ("IODTNVRAM"));
	debug2IOLog ("iter = %p\n", iter);
	if (iter) {
		IODTNVRAM *			nvram;
		nvram = OSDynamicCast (IODTNVRAM, iter->getNextObject ());
		debug2IOLog ("nvram = %p\n", nvram);
		if (nvram) {
			value = (OSData *)nvram->getProperty ("boot-volume");
			debug2IOLog ("value = %p\n", value);
		}
		iter->release ();

		if (value) {
			curPRAMVol = (UInt8*)value->getBytesNoCopy ();
			debug2IOLog ("curPRAMVol = %p\n", curPRAMVol);
		}
		if (curPRAMVol) {
			debug2IOLog ("curPRAM = %d\n", *curPRAMVol);
			*curPRAMVol &= 0x07;
			debug2IOLog ("masked, curPRAM = %d\n", *curPRAMVol);
		}
	}

	IOLog ("returning curPRAMVol = %d\n", *curPRAMVol);
	return *curPRAMVol;
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
	
//	debug2IOLog( "[AppleOnboardAudio] creating user client for task 0x%08lX\n", ( UInt32 ) inOwningTask );
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
//	debug2IOLog( "[AppleOnboardAudio] newUserClient done (err=%d)\n", err );
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
	
	FailIf ( NULL == outState, Exit );
	thePluginObject = findPluginForType ( thePluginType );
	FailIf ( NULL == thePluginObject, Exit );
	result = thePluginObject->getPluginState ( (HardwarePluginDescriptorPtr)outState );
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
	((GetSoftProcUserClientStructPtr)outState)->numHardwareEQBands = getNumHardwareEQBandsForCurrentOutput ();
	return mDriverDMAEngine->copySoftwareProcessingState ( (GetSoftProcUserClientStructPtr)outState);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleOnboardAudio::getAOAState ( UInt32 arg2, void * outState ) {
#pragma unused ( arg2 )
	IOReturn		result;
	
	result = kIOReturnError;
	FailIf ( 0 != arg2, Exit );
	FailIf (NULL == outState, Exit );
	
	((AOAStateUserClientStructPtr)outState)->ucPramData = mUCState.ucPramData;
	((AOAStateUserClientStructPtr)outState)->ucPramVolume = mUCState.ucPramVolume;
	((AOAStateUserClientStructPtr)outState)->ucPowerState = mUCState.ucPowerState;
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
	return mDriverDMAEngine->applySoftwareProcessingState ((SetSoftProcUserClientStructPtr)inState);
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






