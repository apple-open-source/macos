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

#include "PlatformInterface.h"
#include "TransportFactory.h"

OSDefineMetaClassAndStructors(AppleOnboardAudio, IOAudioDevice)

UInt32 AppleOnboardAudio::sInstanceCount = 0;
UInt32 AppleOnboardAudio::sTotalNumAOAEnginesRunning = 0;												//	[3935620],[3942561]

#define super IOAudioDevice

// DualKeyLargo is the first unit with dual codecs on a Keylargo system
// The on KeyLargo systems the service plane by default is flattened so our sound nodes appear
// under the mac-io node. This workaround will search the mac-io 
// node and find the sound node with the layout id not equal to the parent id
// if the sound node is not found by the multidevice array lookup
// There is now a property you can add to the mac-io node to get the service
// plane unflattened and then our sound node look like they would on a K2 system
// The property is include-k2-support any value is fine.
// in open firmware: dev mac-io 1 encode-int " include-k2-support" property
// will add that property You also need AppleKeylargo.kext 1.5.5 or later.

#define DualKeyLargo_WORKAROUND 1

#pragma mark +UNIX LIKE FUNCTIONS

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool AppleOnboardAudio::init (OSDictionary *properties)
{
    debugIOLog ( 3, "+ AppleOnboardAudio[%p]::init", this);

    if (!super::init(properties)) return false;
        
    currentDevices = 0xFFFF;
    
	mHasHardwareInputGain = true;	// aml 5.10.02
	
	mInternalMicDualMonoMode = e_Mode_Disabled;	// aml 6.17.02, turn off by default
	
	mNumberOfAOAPowerParents = 0;				//	AppleOnboardAudio always has at least one power parent
	
	mSampleRateSelectInProcessSemaphore = false;
	mClockSelectInProcessSemaphore = false;

	mCurrentProcessingOutputString = OSString::withCString ("none");
	mCurrentProcessingInputString = OSString::withCString ("none");

	mUCState.ucPowerState = kIOAudioDeviceActive;

	debugIOLog ( 6, "  setting mCurrentAggressivenessLevel to %ld", kIOPMExternalPower );
	mCurrentAggressivenessLevel = kIOPMExternalPower;		//	[3933529]

	mOutputSelectorLastValue=0;		// [3639956]
	mHasSPDIFControl=false;			// [3639956]

    debugIOLog ( 3, "- AppleOnboardAudio[%p]::init", this);
    return true;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool AppleOnboardAudio::start (IOService * provider) {
	OSArray *							layouts;
	OSArray *							multipleDevicesArray;
	OSDictionary *						layoutEntry;
	OSNumber *							layoutIDNumber;
	UInt32 *							layoutID;
	UInt32								layoutIDInt;
	UInt32								layoutsCount;
	UInt32								index;
	OSData *							tmpData;
	bool								result;

    debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::start (%p)", mInstanceIndex, provider);
	result = FALSE;

	AppleOnboardAudio::sInstanceCount++;
	
	mInstanceIndex = AppleOnboardAudio::sInstanceCount;

	mProvider = provider;
	provider->open (this);

#ifdef THREAD_POWER_MANAGEMENT
	mPowerThread = thread_call_allocate ((thread_call_func_t)AppleOnboardAudio::performPowerStateChangeThread, (thread_call_param_t)this);
	FailIf (0 == mPowerThread, Exit);
#endif

	mInitHardwareThread = thread_call_allocate ((thread_call_func_t)AppleOnboardAudio::initHardwareThread, (thread_call_param_t)this);
	FailIf (0 == mInitHardwareThread, Exit);

	//	[3938771]	begin	{
	tmpData = OSDynamicCast (OSData, provider->getProperty ( kLayoutID ) );
	layoutID = (UInt32 *)tmpData->getBytesNoCopy ();
	FailIf ( 0 == layoutID, Exit )
	
	mLayoutID = *layoutID;
	
	// Figure out which plugins need to be loaded for this machine.
	layouts = OSDynamicCast ( OSArray, getProperty ( kLayouts ) );
	debugIOLog ( 3, "  layout array = %p", layouts );
	FailIf ( 0 == layouts, Exit );

	// First thing to do is to find the array entry that describes the machine that we are on.
	layoutsCount = layouts->getCount ();
	debugIOLog ( 3, "  layouts->getCount () returns %ld", layoutsCount );
	layoutEntry = 0;
	index = 0;
	
	mMatchingIndex = 0xFFFFFFFF;
	layoutIDInt = 0;
	debugIOLog ( 6,  "  AppleOnboardAudio[%ld]::mLayoutID 0x%lX = #%ld (from provider node)", mInstanceIndex, mLayoutID, mLayoutID);
	
	while ( layoutsCount-- )
	{
		layoutEntry = OSDynamicCast (OSDictionary, layouts->getObject (index));
		FailIf (0 == layoutEntry, Exit);
		
		layoutIDNumber = OSDynamicCast (OSNumber, layoutEntry->getObject (kLayoutIDInfoPlist));
		FailIf (0 == layoutIDNumber, Exit);
		
		layoutIDInt = layoutIDNumber->unsigned32BitValue ();

		if ( layoutIDInt == mLayoutID )
		{
			debugIOLog ( 6,  "  AppleOnboardAudio[%ld] found machine layout id 0x%lX @ index %ld", mInstanceIndex, layoutIDInt, index);
			mMatchingIndex = index;
			break;
		}
		else
		{
			index++;
		}
	}

	FailIf (0xFFFFFFFF == mMatchingIndex, Exit);	
	layoutEntry = OSDynamicCast (OSDictionary, layouts->getObject (mMatchingIndex));

	debugIOLog ( 6, "  layoutEntry = %p", layoutEntry);
	FailIf (0 == layoutEntry, Exit);
	multipleDevicesArray = OSDynamicCast (OSArray, layoutEntry->getObject (kMultipleDevices));
	debugIOLog ( 3, "  AOA[%d] is %p->multipleDevicesArray = %p", mInstanceIndex, this, multipleDevicesArray);
	
	//	[3938771]
	//	If there is a multiple devices array then don't allow super::start to join the provider power management tree
	if ( multipleDevicesArray )
	{
		mJoinAOAPMTree = TRUE;
	}
	//	}	end	[3938771]
	
	debugIOLog ( 5, "  about to setFamilyManagePower ( FALSE ) due to mJoinAOAPMTree %d", mInstanceIndex, mJoinAOAPMTree );
	setFamilyManagePower ( FALSE );

	result = super::start (provider);				// Causes our initHardware routine to be called.
	FailIf ( !result, Exit );
	
	debugIOLog ( 5, "  mNumberOfAOAPowerParents %ld", mNumberOfAOAPowerParents );


	if ( !mJoinAOAPMTree )	//	[3938771]
	{
		static IOPMPowerState aoaPowerParentPowerStates[2] = {{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},{1, IOPMDeviceUsable, IOPMPowerOn, IOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0}};

		PMinit ();
		debugIOLog ( 6, "  %p newService->about to joinPMtree ( %p )", provider, this );
		provider->joinPMtree ( this );
		
		if ( pm_vars != NULL )
		{
			duringStartup = TRUE;
			debugIOLog ( 6, "  about to registerPowerDriver ( %p, %p, Td)", this, aoaPowerParentPowerStates, 2 );
			registerPowerDriver ( this, aoaPowerParentPowerStates, 2 );
			changePowerStateTo ( 1 );
			duringStartup = FALSE;
		}
		
		//	NOTE:	PMInit() is invoked by super::start after completion of initHardware ( %p ).  
		mNumberOfAOAPowerParents = 1;
		//	Since this instance may be a controlling driver for power management, lets register 
		//	to indicate that this instance can be a power management parent.
		
	}
	
	//	[3938771]  This now needs to occur after super start so that the notifications of AppleOnboardAudio service
	//	will occur after AppleOnboardAudio is prepared to join an AppleOnboardAudio power parent as appropriate.  This
	//	is needed because super::start will invoke PMInit and PMStart after invoking initHardware if this AppleOnboardAudio
	//	instance is not to join another AppleOnboardAudio instance power tree.    The number of power parents needs to
	//	be initialized prior to receiving notification and the initialization of the number of power parents cannot be
	//	performed until completion of super::start.  The notification to 'aoaPublished' will allow the mAOAInstanceArray 
	//	of AppleOnboardAudio objects to be built up which will exclude this AppleOnboardAudio object but will include 
	//	any other AppleOnboardAudio instance.
	debugIOLog ( 3, "  [%ld] addNotification (gIOPublishNotification, serviceMatching (AppleOnboardAudio), (IOServiceNotificationHandler)&aoaPublished=%p, this=%p )", mInstanceIndex, &aoaPublished, this ); 
	aoaNotifier = addNotification ( gIOPublishNotification, serviceMatching ( "AppleOnboardAudio" ), (IOServiceNotificationHandler)&aoaPublished, this );

Exit:
    debugIOLog ( 3, "- AppleOnboardAudio[%ld]::start (%p) returns %d while mNumberOfAOAPowerParents = %d", mInstanceIndex, provider, result, mNumberOfAOAPowerParents );
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool AppleOnboardAudio::handleOpen (IOService * forClient, IOOptionBits options, void *	arg ) 
{
	bool							result;

	debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::handleOpen(%p)", mInstanceIndex, forClient);

	if (kFamilyOption_OpenMultiple == options) {
		result = true;
	} else {
		result = super::handleOpen ( forClient, options, arg );
		FailIf (!result, Exit);
	}

	registerPlugin ( (AudioHardwareObjectInterface *)forClient );

Exit:
	debugIOLog ( 3, "- AppleOnboardAudio[%ld]::handleOpen(%p) returns %s", mInstanceIndex, forClient, result == true ? "true" : "false");
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AppleOnboardAudio::handleClose (IOService * forClient, IOOptionBits options ) 
{
	debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::handleClose(%p)", mInstanceIndex, forClient);

	unRegisterPlugin ( (AudioHardwareObjectInterface *)forClient );

	if (options != kFamilyOption_OpenMultiple) {
		super::handleClose ( forClient, options );
	}
	debugIOLog ( 3, "- AppleOnboardAudio[%ld]::handleClose(%p)", mInstanceIndex, forClient);
	return;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool AppleOnboardAudio::willTerminate ( IOService * provider, IOOptionBits options )
{
	bool			result;
	
	debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::willTerminate(%p)", mInstanceIndex, provider);
	result = super::willTerminate ( provider, options );

	provider->close(this);

	debugIOLog ( 3, "- AppleOnboardAudio[%ld]::willTerminate(%p) returns %d", mInstanceIndex, provider, result);
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Called by the plugin objects from their start() method so that AppleOnboardAudio knows about them and will call them as needed.
void AppleOnboardAudio::registerPlugin (AudioHardwareObjectInterface *thePlugin) {

	debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::registerPlugin (%p)", mInstanceIndex, thePlugin);
	
	FailIf ( 0 == mPluginObjects, Exit );
	mPluginObjects->setObject (thePlugin);

Exit:
	debugIOLog ( 3, "- AppleOnboardAudio[%ld]::registerPlugin (%p)", mInstanceIndex, thePlugin);
	return;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AppleOnboardAudio::stop (IOService * provider) {

	if ( mJoinAOAPMTree )	//	[3938771]
	{
        PMstop();
	}
	
	if ( 0 != aoaNotifier ) {
		aoaNotifier->remove();
	}
	
	FailIf ( mProvider != provider, Exit );
	if (0 != mSysPowerDownNotifier) {
		mSysPowerDownNotifier->remove ();
		mSysPowerDownNotifier = 0;
	}

	mTerminating = TRUE;
	
	if (idleTimer) {
		if (workLoop) {
			workLoop->removeEventSource (idleTimer);
		}

		idleTimer->release ();
		idleTimer = 0;
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
		pollTimer = 0;
	}

	// [3253678]
	callPluginsInOrder ( kSetMuteState, TRUE );						//	[3435307]	rbm
	
#ifdef THREAD_POWER_MANAGEMENT
	if (mPowerThread) {
		thread_call_cancel (mPowerThread);
	}
#endif

	debugIOLog ( 3, "AppleOnboardAudio[%ld]::stop(), audioEngines = %p, isInactive() = %d", mInstanceIndex, audioEngines, isInactive());
Exit:
	super::stop (provider);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AppleOnboardAudio::unRegisterPlugin (AudioHardwareObjectInterface *inPlugin) {
	AudioHardwareObjectInterface *		thePluginObject;
	UInt32								index;
	UInt32								count;

	debugIOLog ( 3, "AppleOnboardAudio[%ld]::unRegisterPlugin (%p)", mInstanceIndex, inPlugin);

	if (0 != mPluginObjects) {
		count = mPluginObjects->getCount ();
		for (index = 0; index < count; index++) {
			thePluginObject = getIndexedPluginObject (index);
			if ((0 != thePluginObject) && (inPlugin == thePluginObject)) {
				mPluginObjects->removeObject(index);
				debugIOLog ( 3, "  removed  plugin (%p)", inPlugin);
			}
		}
	}

	return;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleOnboardAudio::registerPluginAction (OSObject *owner, void *arg1, void *arg2, void *arg3, void *arg4) {
	AppleOnboardAudio *			device;
	IOReturn					result = kIOReturnError;

	device = OSDynamicCast (AppleOnboardAudio, owner);
	FailIf ( 0 == device, Exit );

	debugIOLog ( 3, "AppleOnboardAudio[%ld]::plugin %p registering", device->mInstanceIndex, arg1);

	if (0 == device->mPluginObjects) {
		device->mPluginObjects = OSArray::withCapacity (1);
	}

	FailIf (0 == device->mPluginObjects, Exit);
	device->mPluginObjects->setObject ((AudioHardwareObjectInterface *)arg1);

	result = kIOReturnSuccess;
Exit:
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSObject * AppleOnboardAudio::getLayoutEntry (const char * entryID, AppleOnboardAudio * theAOA) {
	OSArray *							layouts;
	OSDictionary *						layoutEntry;
	OSObject *							entry;

	entry = 0;

	layouts = OSDynamicCast (OSArray, theAOA->getProperty (kLayouts));
	FailIf (0 == layouts, Exit);

	layoutEntry = OSDynamicCast (OSDictionary, layouts->getObject (theAOA->mMatchingIndex));

	FailIf (0 == layoutEntry, Exit);

	entry = OSDynamicCast (OSObject, layoutEntry->getObject (entryID));

Exit:
	return entry;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool AppleOnboardAudio::hasMasterVolumeControl (const UInt32 inCode) {
	char *			connectionString;
	
	connectionString = getConnectionKeyFromCharCode (inCode, kIOAudioStreamDirectionOutput);
	return hasMasterVolumeControl (connectionString);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool AppleOnboardAudio::hasMasterVolumeControl (const char * outputEntry) {
	OSDictionary *					dictEntry;
	OSArray *						controlsArray;
	OSString *						controlString;
	UInt32							controlsCount;
	UInt32							index;
	bool							hasMasterVolControl = FALSE;

	if ( 0 != outputEntry ) {
		debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::hasMasterVolumeControl( %p->'%4s' )", mInstanceIndex, outputEntry, outputEntry );

		dictEntry = OSDynamicCast (OSDictionary, getLayoutEntry (outputEntry, this));
		FailIf (0 == dictEntry, Exit);
		controlsArray = OSDynamicCast (OSArray, dictEntry->getObject (kControls));
		FailIf (0 == controlsArray, Exit);
		controlsCount = controlsArray->getCount ();
		for (index = 0; index < controlsCount; index++) {
			controlString = OSDynamicCast (OSString, controlsArray->getObject (index));
			if ((0 != controlString) && controlString->isEqualTo (kMasterVolControlString)) {
				hasMasterVolControl = TRUE;
			}
		}
	} else {
		debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::hasMasterVolumeControl(%p)", mInstanceIndex, outputEntry );
	}
Exit:
	debugIOLog ( 3, "- AppleOnboardAudio[%ld]::hasMasterVolumeControl(%p) returns %d", mInstanceIndex, outputEntry, hasMasterVolControl );
	return hasMasterVolControl;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool AppleOnboardAudio::hasLeftVolumeControl (const UInt32 inCode) {
	char *			connectionString;
	
	connectionString = getConnectionKeyFromCharCode (inCode, kIOAudioStreamDirectionOutput);
	return hasLeftVolumeControl (connectionString);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool AppleOnboardAudio::hasLeftVolumeControl (const char * outputEntry) {
	OSDictionary *					dictEntry;
	OSArray *						controlsArray;
	OSString *						controlString;
	UInt32							controlsCount;
	UInt32							index;
	bool							hasLeftVolControl = FALSE;

	if ( 0 != outputEntry ) {
		debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::hasLeftVolumeControl( %p->'%4s' )", mInstanceIndex, outputEntry, outputEntry );

		dictEntry = OSDynamicCast (OSDictionary, getLayoutEntry (outputEntry, this));
		FailIf (0 == dictEntry, Exit);
		controlsArray = OSDynamicCast (OSArray, dictEntry->getObject (kControls));
		FailIf (0 == controlsArray, Exit);
		controlsCount = controlsArray->getCount ();
		for (index = 0; index < controlsCount; index++) {
			controlString = OSDynamicCast (OSString, controlsArray->getObject (index));
			if ((0 != controlString) && controlString->isEqualTo (kLeftVolControlString)) {
				hasLeftVolControl = TRUE;
			}
		}
	} else {
		debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::hasLeftVolumeControl(%p)", mInstanceIndex, outputEntry );
	}

Exit:
	debugIOLog ( 3, "- AppleOnboardAudio[%ld]::hasLeftVolumeControl(%p) returns %d", mInstanceIndex, outputEntry, hasLeftVolControl );
	return hasLeftVolControl;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool AppleOnboardAudio::hasRightVolumeControl (const UInt32 inCode) {
	char *			connectionString;
	
	connectionString = getConnectionKeyFromCharCode (inCode, kIOAudioStreamDirectionOutput);
	return hasRightVolumeControl (connectionString);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool AppleOnboardAudio::hasRightVolumeControl (const char * outputEntry) {
	OSDictionary *					dictEntry;
	OSArray *						controlsArray;
	OSString *						controlString;
	UInt32							controlsCount;
	UInt32							index;
	bool							hasRightVolControl = FALSE;

	if ( 0 != outputEntry ) {
		debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::hasRightVolumeControl( %p->'%4s' )", mInstanceIndex, outputEntry, outputEntry );

		dictEntry = OSDynamicCast (OSDictionary, getLayoutEntry (outputEntry, this));
		FailIf (0 == dictEntry, Exit);
		controlsArray = OSDynamicCast (OSArray, dictEntry->getObject (kControls));
		FailIf (0 == controlsArray, Exit);
		controlsCount = controlsArray->getCount ();
		for (index = 0; index < controlsCount; index++) {
			controlString = OSDynamicCast (OSString, controlsArray->getObject (index));
			if ((0 != controlString) && controlString->isEqualTo (kRightVolControlString)) {
				hasRightVolControl = TRUE;
			}
		}
	} else {
		debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::hasRightVolumeControl(%p)", mInstanceIndex, outputEntry );
	}

Exit:
	debugIOLog ( 3, "- AppleOnboardAudio[%ld]::hasRightVolumeControl(%p) returns %d", mInstanceIndex, outputEntry, hasRightVolControl );
	return hasRightVolControl;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AppleOnboardAudio::setUseInputGainControls (const char * inputEntry) {
	OSDictionary *					dictEntry;
	OSArray *						controlsArray;
	OSString *						controlString;
	UInt32							controlsCount;
	UInt32							index;

	if ( 0 != inputEntry ) {
		debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::setUseInputGainControls( %p->'%4s' )", mInstanceIndex, inputEntry, inputEntry);
		mUseInputGainControls = kNoInputGainControls;
	
		dictEntry = OSDynamicCast (OSDictionary, getLayoutEntry (inputEntry, this));
		FailIf (0 == dictEntry, Exit);
	
		controlsArray = OSDynamicCast (OSArray, dictEntry->getObject (kControls));
		
		if (NULL != controlsArray) {
			controlsCount = controlsArray->getCount ();
			for (index = 0; index < controlsCount; index++) {
				controlString = OSDynamicCast (OSString, controlsArray->getObject (index));
				if ((0 != controlString) && (controlString->isEqualTo (kLeftVolControlString) || controlString->isEqualTo (kRightVolControlString))) {
					mUseInputGainControls = kStereoInputGainControls;
					debugIOLog ( 3, "  mUseInputGainControls = kStereoInputGainControls");
				} else if ((0 != controlString) && (controlString->isEqualTo (kMasterVolControlString))) {
					mUseInputGainControls = kMonoInputGainControl;
					debugIOLog ( 3, "  mUseInputGainControls = kMonoInputGainControl");
				}
			}
		}
	}
Exit:
		debugIOLog ( 3, "- AppleOnboardAudio[%ld]::setUseInputGainControls( %p )", mInstanceIndex, inputEntry);
	return;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	[3281535]	begin {
void AppleOnboardAudio::setUsePlaythroughControl (const char * inputEntry) {
	OSDictionary *					dictEntry;
	OSBoolean *						playthroughOSBoolean;

	if ( 0 != inputEntry ) {
		debugIOLog ( 3,  "+ AppleOnboardAudio[%ld]::setUsePlaythroughControl( %p->'%4s' )", mInstanceIndex, inputEntry, inputEntry );
		mUsePlaythroughControl = FALSE;
	
		dictEntry = OSDynamicCast ( OSDictionary, getLayoutEntry ( inputEntry, this) );
		FailIf ( 0 == dictEntry, Exit );
	
		playthroughOSBoolean = OSDynamicCast ( OSBoolean, dictEntry->getObject ( kPlaythroughControlString ) );
		FailIf ( 0 == playthroughOSBoolean, Exit );
	
		mUsePlaythroughControl = playthroughOSBoolean->getValue ();
	} else {
		debugIOLog ( 3,  "+ AppleOnboardAudio[%ld]::setUsePlaythroughControl( %p )", mInstanceIndex, inputEntry);
	}
Exit:
	debugIOLog ( 3,  "- AppleOnboardAudio[%ld]::setUsePlaythroughControl(%p), mUsePlaythroughControl = %d", mInstanceIndex, inputEntry, mUsePlaythroughControl );
	return;
}
//	[3281535]	} end
			
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleOnboardAudio::validateOutputFormatChangeRequest (const IOAudioStreamFormat * inFormat, const IOAudioSampleRate * inRate) {
	IOReturn							result;

	result = kIOReturnSuccess;

	if (kIOAudioStreamSampleFormat1937AC3 == inFormat->fSampleFormat) {
		if (0 != mOutputSelector) {
			// [3656784] must check if digital output is available, otherwise this encoded format selection is denied - aml
			// should add a safety check for machines with no digital out even though DVD player doesn't allow ac3 selection - check output bitmap?
			if (!(kGPIO_Unknown == mPlatformInterface->getComboOutJackTypeConnected() || kGPIO_Connected == mPlatformInterface->getDigitalOutConnected())) {
				debugIOLog ( 3, "~ AppleOnboardAudio::validateOutputFormatChangeRequest found invalid encoded format request");
				result = kIOReturnError;
			}
		}
	}
	
	if ((NULL != inRate) && (kClockSourceSelectionExternal == mCurrentClockSelector)) {
		debugIOLog(3, "AppleOnboardAudio::validateOutputFormatChangeRequest - attempt to change sampling rate while external clock selected...returning error");
		result = kIOReturnNotPermitted;
	}
	
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleOnboardAudio::validateInputFormatChangeRequest (const IOAudioStreamFormat * inFormat, const IOAudioSampleRate * inRate) {
	IOReturn result = kIOReturnSuccess;
	
	if ((NULL != inRate) && (kClockSourceSelectionExternal == mCurrentClockSelector)) {
		debugIOLog(3, "AppleOnboardAudio::validateInputFormatChangeRequest - attempt to change sampling rate while external clock selected...returning error");
		result = kIOReturnNotPermitted;
	}
	
	return result;
}
			
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleOnboardAudio::formatChangeRequest (const IOAudioStreamFormat * inFormat, const IOAudioSampleRate * inRate) {
	IOReturn							result;
	OSNumber *							connectionCodeNumber;
	bool								needsUpdate = FALSE;	//  [3628559]   (22 April 2004) rbm
	
	result = kIOReturnSuccess;																//	[4030727]

	debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::formatChangeRequest (%p, %p)", mInstanceIndex, inFormat, inRate);
	FailIf ( 0 == mPlatformInterface, Exit );

	//  [3628559]   Format changes (i.e. bit depth or encoding) should be applied to any audio hardware.
	//  Sample rate changes should only be applied to hardware that supports sample rate changes.  Since
	//  'Slave Only' hardware determines sample rate from recovery of an external clock source, the 
	//  request to change the sample rate on a 'Slave Only' device should be blocked.  This will avoid
	//  pausing and resuming the audio engine on devices that cannot apply the request to the hardware.

	//  [3628559]   begin   {   (22 April 2004) rbm
	if ( 0 != inFormat ) {
		needsUpdate = TRUE;
	}
	if ( 0 != inRate ) {
		if ( kTransportInterfaceType_I2S_Slave_Only != mTransportInterface->transportGetTransportInterfaceType() ) {	//  [3628559]   Dont attempt to change sample rate on a slave only device
			needsUpdate = TRUE;
		}
	}
	//  }   end		[3628559]   (22 April 2004) rbm
	
	if ( needsUpdate ) {																		//  [3628559]   (22 April 2004) rbm
		result = kIOReturnError;																//	[4030727]
		if (mMuteAmpWhenClockInterrupted) {														// aml, this was checking the UI mutes amps flag which is wrong
			muteAllAmps ();
		}

		callPluginsInOrder (kBeginFormatChange, 0);											//	[3558796]	aml

		if (0 != inFormat) {
			debugIOLog ( 3, "  AppleOnboardAudio[%ld]::formatChangeRequest with bit width %d", mInstanceIndex, inFormat->fBitWidth);
			result = mTransportInterface->transportSetSampleWidth (inFormat->fBitDepth, inFormat->fBitWidth);
			callPluginsInOrder (kSetSampleBitDepth, inFormat->fBitDepth);
			if (kIOAudioStreamSampleFormat1937AC3 == inFormat->fSampleFormat) {
				if (0 != mOutputSelector) {
				
					// [3656784] must check if digital output is available, otherwise this encoded format selection is denied - aml
					// should add a safety check for machines with no digital out even though DVD player doesn't allow ac3 selection - check output bitmap?
					debugIOLog ( 3, "  mPlatformInterface->getComboOutJackTypeConnected() = %ld", mPlatformInterface->getComboOutJackTypeConnected());
					debugIOLog ( 3, "  mPlatformInterface->getDigitalOutConnected() = %ld", mPlatformInterface->getDigitalOutConnected());
					if (kGPIO_Unknown == mPlatformInterface->getComboOutJackTypeConnected() || kGPIO_Connected == mPlatformInterface->getDigitalOutConnected()) {
						connectionCodeNumber = OSNumber::withNumber (kIOAudioOutputPortSubTypeSPDIF, 32);
						mOutputSelector->setValue (connectionCodeNumber);
						connectionCodeNumber->release ();
						mEncodedOutputFormat = true;
						debugIOLog ( 3, "  encoded format request honored");
					} else {
						debugIOLog ( 3, "  encoded format requested, but no digital hardware is connected");
						callPluginsInOrder (kEndFormatChange, 0);
						selectCodecOutputWithMuteState (mIsMute);
						result = kIOReturnError;
						goto Exit;
					}
				}
			} else {
				mEncodedOutputFormat = false;		
				debugIOLog ( 5, "  non-encoded format");
			}
			result = callPluginsInOrder ( kSetSampleType, inFormat->fSampleFormat );
		}
		if (0 != inRate) {
			mSampleRateSelectInProcessSemaphore = true;
			
			debugIOLog ( 3, "  AppleOnboardAudio[%ld]::formatChangeRequest with rate %ld", mInstanceIndex, inRate->whole);
			result = mTransportInterface->transportSetSampleRate (inRate->whole);
			FailIf ( kIOReturnSuccess != result, Exit );					//	[3886091]
			result = callPluginsInOrder (kSetSampleRate, inRate->whole);
			FailIf ( kIOReturnSuccess != result, Exit );					//	[3886091]
			
			//	Maintain a member copy of the sample rate so that changes in sample rate
			//	when running on an externally clocked sample rate can be detected so that
			//	the HAL can be notified of sample rate changes.
			
			if ( kIOReturnSuccess == result ) {
				mTransportSampleRate.whole = inRate->whole;
				mTransportSampleRate.fraction = inRate->fraction;
				mSampleRateSelectInProcessSemaphore = false;
			}
		}

		callPluginsInOrder (kEndFormatChange, 0);											//	[3558796]	aml
		selectCodecOutputWithMuteState (mIsMute);
		if ( 0 != mOutputSelector ) {
			selectOutputAmplifiers (mOutputSelector->getIntValue (), mIsMute);
		}
	}

Exit:
	debugIOLog ( 3, "- AppleOnboardAudio[%ld]::formatChangeRequest returns %d", mInstanceIndex, result);
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleOnboardAudio::callPluginsInReverseOrder (UInt32 inSelector, UInt32 newValue) {
	AudioHardwareObjectInterface *		thePluginObject;
	OSArray *							pluginOrderArray;
	OSString *							pluginName;
	SInt32								index;
	UInt32								pluginOrderArrayCount;
	IOReturn							result;

	debugIOLog ( 7, "+ AppleOnboardAudio[%ld]::callPluginsInReverseOrder (%lX, %lX)", mInstanceIndex, inSelector, newValue);
	result = kIOReturnBadArgument;

	pluginOrderArray = OSDynamicCast (OSArray, getLayoutEntry (kPluginRecoveryOrder, this));
	FailIf (0 == pluginOrderArray, Exit);

	pluginOrderArrayCount = pluginOrderArray->getCount ();

	for (index = pluginOrderArrayCount - 1; index >= 0; index--) {
		pluginName = OSDynamicCast(OSString, pluginOrderArray->getObject(index));
		if (0 == pluginName) {
			debugIOLog ( 3, "  Corrupt %s entry in AppleOnboardAudio[%ld] Info.plist", kPluginRecoveryOrder, mInstanceIndex);
			continue;
		}
		thePluginObject = getPluginObjectWithName (pluginName);
		if (0 == thePluginObject) {
			debugIOLog ( 3, "  Can't find required AppleOnboardAudio plugin from %s entry loaded", kPluginRecoveryOrder);
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
				//	[3933529]	synthesize two arguments from one where:	pendingPowerState = ( newValue & 0x0000FFFF );
				//															currentPowerState = ( newValue >> 16 );
				result = thePluginObject->performSetPowerState ( ( newValue >> 16 ), ( newValue & 0x0000FFFF ) );
				break;
			default:
				break;
		}		
	}

Exit:
	debugIOLog ( 7, "- AppleOnboardAudio[%ld]::callPluginsInReverseOrder (%lX, %lX) returns %lX", mInstanceIndex, inSelector, newValue, result);
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleOnboardAudio::callPluginsInOrder (UInt32 inSelector, UInt32 newValue) {
	AudioHardwareObjectInterface *		thePluginObject;
	OSArray *							pluginOrderArray;
	OSString *							pluginName;
	UInt32								index;
	UInt32								pluginOrderArrayCount;
	IOReturn							tempResult;
	IOReturn							result;
	bool								boolResult;
	
	debugIOLog ( 7, "+ AppleOnboardAudio[%ld]::callPluginsInOrder (%lX, %lX)", mInstanceIndex, inSelector, newValue);

	result = kIOReturnBadArgument;

	pluginOrderArray = OSDynamicCast (OSArray, getLayoutEntry (kPluginRecoveryOrder, this));
	FailIf (0 == pluginOrderArray, Exit);

	pluginOrderArrayCount = pluginOrderArray->getCount ();

	result = kIOReturnSuccess;
	for (index = 0; index < pluginOrderArrayCount; index++) {
		pluginName = OSDynamicCast(OSString, pluginOrderArray->getObject(index));
		if (0 == pluginName) {
			debugIOLog ( 3, "  Corrupt %s entry in AppleOnboardAudio[%ld] Info.plist", kPluginRecoveryOrder, mInstanceIndex);
			continue;
		}
		thePluginObject = getPluginObjectWithName (pluginName);
		if (0 == thePluginObject) {
			debugIOLog ( 3, "  Can't find required AppleOnboardAudio plugin from %s entry loaded", kPluginRecoveryOrder);
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
				boolResult = thePluginObject->preDMAEngineInit ( newValue );
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
				{
					//	[3933529]	synthesize two arguments from one where:
					//				pendingPowerState = ( newValue & 0x0000FFFF );
					//				currentPowerState = ( newValue >> 16 );
					tempResult = thePluginObject->performSetPowerState ( ( newValue >> 16 ), ( newValue & 0x0000FFFF ) );
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
				debugIOLog ( 3,  "  callPluginsInOrder calls thePluginObject->setMute ( %ld )", newValue );
				tempResult = thePluginObject->setMute ( newValue );
				break;
			case kSetAnalogMuteState:			//	[3435307]	
				debugIOLog ( 3,  "  callPluginsInOrder calls thePluginObject->setMute ( %ld, kAnalogAudioSelector )", newValue );
				tempResult = thePluginObject->setMute ( newValue, kAnalogAudioSelector );
				break;
			case kSetDigitalMuteState:			//	[3435307]	
				debugIOLog ( 3,  "  callPluginsInOrder calls thePluginObject->setMute ( %ld, kDigitalAudioSelector )", newValue );
				tempResult = thePluginObject->setMute ( newValue, kDigitalAudioSelector );
				break;
			case kBeginFormatChange:		//	[3558796]
				debugIOLog ( 3,  "  callPluginsInOrder calls thePluginObject->beginFormatChange ()");
				tempResult = thePluginObject->beginFormatChange ();
				break;
			case kEndFormatChange:			//	[3558796]	
				debugIOLog ( 3,  "  callPluginsInOrder calls thePluginObject->endFormatChange ()");
				tempResult = thePluginObject->endFormatChange ();
				break;
			case kRequestSleepTime:
				//	[3787193]
				//	each hardware plugin will add the hardware plugin power management delay time to the current
				//	time pointed to by newValue and expressed in microseconds.  Upon exit, the delay time will
				//	be the delay time for all hardware plugins currently being used by this AppleOnboardAudio instance.
				debugIOLog ( 6, "  kRequestSleepTime before plugin: %p->%ld", (UInt32*)newValue, *(UInt32*)newValue );
				tempResult = thePluginObject->requestSleepTime ( (UInt32*)newValue );
				debugIOLog ( 6, "  kRequestSleepTime after plugin:  %p->%ld", (UInt32*)newValue, *(UInt32*)newValue );
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
	debugIOLog ( 7, "- AppleOnboardAudio[%ld]::callPluginsInOrder (%lX, %lX) returns %lX", mInstanceIndex, inSelector, newValue, result);
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
	
	result = 0;
	pluginOrderArray = OSDynamicCast (OSArray, getLayoutEntry (kPluginRecoveryOrder, this));
	FailIf (0 == pluginOrderArray, Exit);

	pluginOrderArrayCount = pluginOrderArray->getCount ();

	for (index = 0; index < pluginOrderArrayCount && 0 == result; index++) {
		thePluginObject = 0;
		pluginName = OSDynamicCast(OSString, pluginOrderArray->getObject(index));
		if ( 0 != pluginName ) {
			thePluginObject = getPluginObjectWithName (pluginName);
			if ( 0 != thePluginObject ) {
				if ( pluginType == thePluginObject->getPluginType() ) {
					result = thePluginObject;
				}
			}
		}
	}
Exit:
	return result;
}


//	--------------------------------------------------------------------------------
void AppleOnboardAudio::setPollTimer () {
    AbsoluteTime				fireTime;
    UInt64						nanos;

	if ( pollTimer ) {
		debugIOLog ( 6, "± AppleOnboardAudio::setPollTimer () starting timer" );
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
	FailIf ( 0 == audioDevice, Exit );
	audioDevice->runPollTasksEventHandler ();
Exit:
	return;
}


//	--------------------------------------------------------------------------------
//	[3515371]
void	AppleOnboardAudio::runPollTasksEventHandler ( void ) {
	IOCommandGate *				cg;
	
	cg = getCommandGate ();
	if (0 != cg) {
		cg->runAction ( runPolledTasks, (void*)0, (void*)0 );
	}
	return;
}


//	--------------------------------------------------------------------------------
//	[3515371]
IOReturn AppleOnboardAudio::runPolledTasks (OSObject * owner, void * arg1, void * arg2, void * arg3, void * arg4) {
	FailIf ( 0 == owner, Exit );
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
		if ( kIOAudioDeviceActive == getPowerState () ) {
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
							debugIOLog ( 4, "  *#* about to [%ld]mDriverDMAEngine->hardwareSampleRateChanged ( %d )", mInstanceIndex, mTransportSampleRate.whole );	//  [3684994]
							mDriverDMAEngine->hardwareSampleRateChanged ( &mTransportSampleRate );
							mDriverDMAEngine->updateDSPForSampleRate(mTransportSampleRate.whole);  // [4220086]
						} else {
							if ( ( kTransportInterfaceType_I2S_Slave_Only != transportType ) && !mAutoSelectClock ) {  //  [3628559,4073140]
																							  //	Set the hardware to MASTER mode.
								debugIOLog ( 5, "  ** AppleOnboardAudio[%ld]::runPolledTasks switching to INTERNAL CLOCK", mInstanceIndex );
								mCodecLockStatus = kClockUnLockStatus;		//	[4073140]
								clockSelectorChanged ( kClockSourceSelectionInternal );
								if ( 0 != mExternalClockSelector ) {
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
			
			// [4216970]
			// Move call to callPluginsInOrder(kRunPollTask) above mPlatformInterface->poll() and mTransportInterface->poll() so that plugins 
			// which require decrementing of a delay poll after wake counter can decrement this counter from their poll routine prior to
			// executing their notifyHardwareEvent() routine whose execution may depend on this counter having expired (e.g. Topaz CS8416).  
			callPluginsInOrder ( kRunPollTask, 0 );
			mPlatformInterface->poll ();
			mTransportInterface->poll ();
		}	//	} end	[3326541]
	} else {	//  [3686032]   only invoke hardware plugin poll as no polled interrupt notifications are desired until full polling is enabled
		if ( kIOAudioDeviceActive == getPowerState () ) {
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
	
	debugIOLog ( 3,  "+ AppleOnboardAudio[%ld]::ParseDetectCollection() detectCollection 0x%lX", mInstanceIndex, mDetectCollection );

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

	debugIOLog ( 3,  "- AppleOnboardAudio[%ld]::ParseDetectCollection returns %4s", mInstanceIndex, (char*)&result );
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt32 AppleOnboardAudio::parseInputDetectCollection ( void ) {
	UInt32		result;
	
	if ( mDetectCollection & kSndHWLineInput ) {
		result = kIOAudioOutputPortSubTypeLine;
	} else {
		result = kIOAudioInputPortSubTypeInternalMicrophone;
	}

	debugIOLog ( 3,  "± AppleOnboardAudio[%ld]::parseInputDetectCollection returns'%4s' from mDetectCollection = 0x%lX", mInstanceIndex, (char*)&result, mDetectCollection );
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	[3514514]	Updated for combo output jacks:
//
//		Line out is to be excluded if a digital output plug is inserted in a
//		combo output jack associated with the line output detect.
//
void AppleOnboardAudio::initializeDetectCollection ( void ) {
	debugIOLog ( 3,  "+ AppleOnboardAudio[%ld]::initializeDetectCollection() ", mInstanceIndex);

	mDetectCollection = getValueForDetectCollection ( mDetectCollection );
	
	debugIOLog ( 3,  "- AppleOnboardAudio[%ld]::initializeDetectCollection(), mDetectCollection = 0x%lX", mInstanceIndex, mDetectCollection );
	return;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt32	AppleOnboardAudio::getValueForDetectCollection ( UInt32 currentDetectCollection )
{
	UInt32			result = currentDetectCollection;
	
	debugIOLog ( 3,  "+ AppleOnboardAudio[%ld]::getValueForDetectCollection ( 0x%X )", mInstanceIndex, currentDetectCollection );

	if ( kGPIO_Connected == mPlatformInterface->getHeadphoneConnected() ) {
		//	[3514514]	begin {
		if ( kGPIO_Selector_HeadphoneDetect == mPlatformInterface->getComboOutAssociation() ) {
			if ( kGPIO_Unknown == mPlatformInterface->getComboOutJackTypeConnected () ) {
				result |= kSndHWCPUHeadphone;
				debugIOLog ( 3,  "  AppleOnboardAudio[%ld]::getValueForDetectCollection() - headphones are connected.", mInstanceIndex);
			} else {
				if ( kGPIO_TypeIsAnalog == mPlatformInterface->getComboOutJackTypeConnected () ) {
					result |= kSndHWCPUHeadphone;
					debugIOLog ( 3,  "  AppleOnboardAudio[%ld]::getValueForDetectCollection() - headphones are connected.", mInstanceIndex);
				} else {
					result |= kSndHWDigitalOutput;
					debugIOLog ( 3,  "  AppleOnboardAudio[%ld]::getValueForDetectCollection() - digital output is connected.", mInstanceIndex);
				}
			}
		} else {
			result |= kSndHWCPUHeadphone;
			debugIOLog ( 3,  "  AppleOnboardAudio[%ld]::getValueForDetectCollection() - headphones are connected.", mInstanceIndex);
		}
		//	} end	[3514514]
	} 
	if ( kGPIO_Connected == mPlatformInterface->getLineOutConnected() ) {
		//	[3514514]	begin {
		if ( kGPIO_Selector_LineOutDetect == mPlatformInterface->getComboOutAssociation() ) {
			if ( kGPIO_Unknown == mPlatformInterface->getComboOutJackTypeConnected () ) {
				result |= kSndHWLineOutput;
				debugIOLog ( 3,  "  AppleOnboardAudio[%ld]::getValueForDetectCollection() - line output is connected.", mInstanceIndex);
			} else {
				if ( kGPIO_TypeIsAnalog == mPlatformInterface->getComboOutJackTypeConnected () ) {
					result |= kSndHWLineOutput;
					debugIOLog ( 3,  "  AppleOnboardAudio[%ld]::getValueForDetectCollection() - line output is connected.", mInstanceIndex);
				} else {
					result |= kSndHWDigitalOutput;
					debugIOLog ( 3,  "  AppleOnboardAudio[%ld]::getValueForDetectCollection() - digital output is connected.", mInstanceIndex);
				}
			}
		} else {
			result |= kSndHWLineOutput;
			debugIOLog ( 3,  "  AppleOnboardAudio[%ld]::getValueForDetectCollection() - line output is connected.", mInstanceIndex);
		}
		//	} end	[3514514]
	} 
	if ( kGPIO_Connected == mPlatformInterface->getDigitalOutConnected() ) {
		debugIOLog ( 3,  "  AppleOnboardAudio[%ld]::getValueForDetectCollection() - digital out is connected.", mInstanceIndex);
		result |= kSndHWDigitalOutput;
	} 
	if ( kGPIO_Connected == mPlatformInterface->getLineInConnected() ) {
		if ( kGPIO_Selector_LineInDetect == mPlatformInterface->getComboInAssociation() ) {
			if ( kGPIO_Unknown == mPlatformInterface->getComboInJackTypeConnected () ) {
				result |= kSndHWLineInput;
			}
			else {
				if ( kGPIO_TypeIsAnalog == mPlatformInterface->getComboInJackTypeConnected () ) {
					result |= kSndHWLineInput;
				}
				else {
					result |= kSndHWDigitalInput;
				}
			}
		}
		else {
			result |= kSndHWLineInput;
		}
	}
	if ( kGPIO_Connected == mPlatformInterface->getDigitalInConnected() ) {
		result |= kSndHWDigitalInput;
	} 
	if ( kGPIO_Connected == mPlatformInterface->getSpeakerConnected() ) {	//	[3398729]	start	{
		debugIOLog ( 3,  "  AppleOnboardAudio[%ld]::getValueForDetectCollection() - external speakers are connected.", mInstanceIndex);
		result |= kSndHWCPUExternalSpeaker;
		result &= ~kSndHWInternalSpeaker;
	} else/* if ( kGPIO_Disconnected == mPlatformInterface->getSpeakerConnected() )*/ {
		debugIOLog ( 3,  "  AppleOnboardAudio[%ld]::getValueForDetectCollection() - in internal speakers are connected.", mInstanceIndex);
		result |= kSndHWInternalSpeaker;
		result &= ~kSndHWCPUExternalSpeaker;
	}																		//	}	end	[3398729]
	
	debugIOLog ( 3,  "- AppleOnboardAudio[%ld]::getValueForDetectCollection ( 0x%X ) returns 0x%lX", mInstanceIndex, currentDetectCollection );
	return result;
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
void AppleOnboardAudio::updateAllDetectCollection (UInt32 statusSelector, UInt32 newValue) {

	debugIOLog ( 5, "+ AppleOnboardAudio[%ld]::updateAllDetectCollection ( %ld, %ld )", mInstanceIndex, statusSelector, newValue );
	
	switch (statusSelector)
	{
		case kHeadphoneStatus:
			debugIOLog ( 6,  "  kHeadphoneStatus mDetectCollection prior to modification %lX", mDetectCollection );
			if (newValue == kInserted)
			{
				mDetectCollection |= kSndHWCPUHeadphone;
				mDetectCollection &= ~kSndHWInternalSpeaker;
				debugIOLog ( 6, "  headphones inserted, mDetectCollection = %lX", mDetectCollection);
				ConfigChangeHelper theConfigeChangeHelper(mDriverDMAEngine);	
				//	[3514514]	If the line out is IN and is associated with a combo out jack then remove
				//				the S/PDIF selector and add the Line Out selector.
				if ( kGPIO_Selector_HeadphoneDetect == mPlatformInterface->getComboOutAssociation() )
				{
					mOutputSelector->removeAvailableSelection ( kIOAudioOutputPortSubTypeSPDIF );
					debugIOLog ( 6, "  removed SPDIF from output selector due to headphone insert **");
					mDetectCollection &= ~kSndHWDigitalOutput;
				}
				debugIOLog ( 6, "  add headphones to output selector *");
				mOutputSelector->addAvailableSelection ( kIOAudioOutputPortSubTypeHeadphones, mHeadphoneOutputString );
				debugIOLog ( 6, "  remove internal speaker from output selector *");
				mOutputSelector->removeAvailableSelection ( kIOAudioOutputPortSubTypeInternalSpeaker );
			}
			else if (newValue == kRemoved)
			{
				mDetectCollection &= ~kSndHWCPUHeadphone;
				mDetectCollection |= kSndHWInternalSpeaker;
				debugIOLog ( 6, "  headphones removed, mDetectCollection = %lX", mDetectCollection);
				ConfigChangeHelper theConfigeChangeHelper(mDriverDMAEngine);	
				mOutputSelector->removeAvailableSelection ( kIOAudioOutputPortSubTypeHeadphones );
				debugIOLog ( 6, "  headphones removed, mDetectCollection = %lX", mDetectCollection);
				//  If headphones are removed and line out is removed then add internal speakers
				if ( 0 == ( mDetectCollection & kSndHWLineOutput ) )
				{
					mOutputSelector->addAvailableSelection ( kIOAudioOutputPortSubTypeInternalSpeaker, mInternalSpeakerOutputString );
					debugIOLog ( 6, "  add internal speaker to output selector *");
				}
			}
			else
			{
				debugIOLog ( 6, "  Unknown headphone jack status, mDetectCollection = %lX", mDetectCollection);
			}
			break;
		case kLineOutStatus:
			debugIOLog ( 6,  "  kLineOutStatus mDetectCollection prior to modification %lX", mDetectCollection );
			if (newValue == kInserted)
			{
				mDetectCollection |= kSndHWLineOutput;
				mDetectCollection &= ~kSndHWInternalSpeaker;
				debugIOLog ( 6, "  line out inserted.");
				ConfigChangeHelper theConfigeChangeHelper(mDriverDMAEngine);	
				mOutputSelector->removeAvailableSelection ( kIOAudioOutputPortSubTypeInternalSpeaker );
				debugIOLog ( 6, "  removed internal speaker from output selector **");
				//	[3514514]	If the line out is IN and is associated with a combo out jack then remove
				//				the S/PDIF selector and add the Line Out selector.
				if ( kGPIO_Selector_LineOutDetect == mPlatformInterface->getComboOutAssociation() )
				{
					mOutputSelector->removeAvailableSelection ( kIOAudioOutputPortSubTypeSPDIF );
					debugIOLog ( 6, "  removed SPDIF from output selector **");
					mDetectCollection &= ~kSndHWDigitalOutput;
				}
				debugIOLog ( 6, "  add LineOut from output selector");
				mOutputSelector->addAvailableSelection (kIOAudioOutputPortSubTypeLine, mLineOutputString);
			}
			else if (newValue == kRemoved)
			{
				mDetectCollection &= ~kSndHWLineOutput;
				mDetectCollection |= kSndHWInternalSpeaker;
				debugIOLog ( 6, "  line out removed.");
				ConfigChangeHelper theConfigeChangeHelper(mDriverDMAEngine);	
				mOutputSelector->removeAvailableSelection ( kIOAudioOutputPortSubTypeLine );
				debugIOLog ( 6, "  removed Line from output selector");
				//  If headphones are removed and line out is removed then add internal speakers
				if ( 0 == ( mDetectCollection & kSndHWCPUHeadphone ) )
				{
					mOutputSelector->addAvailableSelection ( kIOAudioOutputPortSubTypeInternalSpeaker, mInternalSpeakerOutputString );
					debugIOLog ( 6, "  added internal speaker to output selector *");
				}
			}
			else
			{
				debugIOLog ( 6, "  Unknown line out jack status.");
			}
			break;
		case kDigitalOutStatus:
			debugIOLog ( 6,  "  kDigitalOutStatus mDetectCollection prior to modification %lX", mDetectCollection );
			if (newValue == kInserted)
			{
				mDetectCollection |= kSndHWDigitalOutput;
				mDetectCollection &= ~kSndHWInternalSpeaker;
				debugIOLog ( 6, "  digital out inserted, mDigitalOutputString = %p, getComboOutJackTypeConnected = %ld", mDigitalOutputString, mPlatformInterface->getComboOutJackTypeConnected ());
				//	[3514514]	If switching TO an exclusive digital output then remove all other selectors 
				//				associated with the combo output jack supporting that digital output.
				debugIOLog ( 6, "  mPlatformInterface->getComboOutJackTypeConnected () returns 0x%0.2X", mPlatformInterface->getComboOutJackTypeConnected () );
				if ( kGPIO_Unknown != mPlatformInterface->getComboOutJackTypeConnected () )
				{
					ConfigChangeHelper theConfigeChangeHelper(mDriverDMAEngine);	
					if ( kGPIO_Selector_LineOutDetect == mPlatformInterface->getComboOutAssociation() )
					{
						mOutputSelector->removeAvailableSelection ( kIOAudioOutputPortSubTypeInternalSpeaker );
						debugIOLog ( 6, "  removed internal speaker from output selector *");
						mOutputSelector->removeAvailableSelection ( kIOAudioOutputPortSubTypeLine );
						debugIOLog ( 6, "  removed line output from output selector");
						mDetectCollection &= ~kSndHWLineOutput;
					}
					else if ( kGPIO_Selector_HeadphoneDetect == mPlatformInterface->getComboOutAssociation() )
					{
						mOutputSelector->removeAvailableSelection ( kIOAudioOutputPortSubTypeInternalSpeaker );
						debugIOLog ( 6, "  removed internal speaker from output selector **");
						mOutputSelector->removeAvailableSelection ( kIOAudioOutputPortSubTypeHeadphones );
						debugIOLog ( 6, "  removed headphone from output selector");
						mDetectCollection &= ~kSndHWCPUHeadphone;
					}
					mOutputSelector->addAvailableSelection (kIOAudioOutputPortSubTypeSPDIF, mDigitalOutputString);
					debugIOLog ( 6, "  added SPDIF to output selector");
				}
			}
			else if (newValue == kRemoved)
			{
				mDetectCollection &= ~kSndHWDigitalOutput;
				mDetectCollection |= kSndHWInternalSpeaker;
				debugIOLog ( 6, "digital out removed.");
				//	[3514514]	If switching FROM an exclusive digital output then remove add other selectors 
				//				associated with the combo output jack supporting that digital output.
				if ( (kGPIO_Unknown != mPlatformInterface->getComboOutJackTypeConnected ()) )
				{
					debugIOLog ( 5, "  ** AppleOnboardAudio[%ld]::updateDetectCollection invoking 'ConfigChangeHelper'", mInstanceIndex );
					ConfigChangeHelper theConfigeChangeHelper(mDriverDMAEngine);	
					if ( kGPIO_Selector_LineOutDetect == mPlatformInterface->getComboOutAssociation() )
					{
						mOutputSelector->addAvailableSelection (kIOAudioOutputPortSubTypeInternalSpeaker, mInternalSpeakerOutputString);
						debugIOLog ( 6, "  added internal speaker to output selector");
						if ( kGPIO_Connected == mPlatformInterface->getLineOutConnected () )
						{
							debugIOLog ( 6, "  add LineOut from output selector");
							mOutputSelector->addAvailableSelection (kIOAudioOutputPortSubTypeLine, mLineOutputString);
							mDetectCollection |= kSndHWLineOutput;
						}
						else
						{
							mOutputSelector->removeAvailableSelection ( kIOAudioOutputPortSubTypeLine );
							debugIOLog ( 6, "  removed Line from output selector");
						}
					}
					else if ( kGPIO_Selector_HeadphoneDetect == mPlatformInterface->getComboOutAssociation() )
					{
						mOutputSelector->addAvailableSelection (kIOAudioOutputPortSubTypeInternalSpeaker, mInternalSpeakerOutputString);
						debugIOLog ( 6, "  added internal speaker to output selector");
						if ( kGPIO_Connected == mPlatformInterface->getHeadphoneConnected () )
						{
							debugIOLog ( 6, "  added headphones to output selector *");
							mOutputSelector->addAvailableSelection ( kIOAudioOutputPortSubTypeHeadphones, mHeadphoneOutputString );
							mDetectCollection |= kSndHWCPUHeadphone;
						}
						else
						{
							mOutputSelector->removeAvailableSelection ( kIOAudioOutputPortSubTypeHeadphones );
							debugIOLog ( 6, "  removed headphone from output selector");
						}
					}
					mOutputSelector->removeAvailableSelection ( kIOAudioOutputPortSubTypeSPDIF );
					debugIOLog ( 6, "  removed SPDIF from output selector");
				}
			}
			else
			{
				debugIOLog ( 6, "  Unknown digital out jack status.");
			}
			break;
		case kLineInStatus:
			debugIOLog ( 6,  "  kLineInStatus mDetectCollection prior to modification %lX", mDetectCollection );
			if (newValue == kInserted)
			{
				mDetectCollection |= kSndHWLineInput;
				
				// [4148027]
				if (TRUE == mAutoSelectInput) {
					ConfigChangeHelper theConfigChangeHelper(mDriverDMAEngine);
					
					if (kGPIO_Unknown != mPlatformInterface->getComboInJackTypeConnected()) {
						if (TRUE == mInputSelector->valueExists(kIOAudioInputPortSubTypeSPDIF)) {
							mInputSelector->removeAvailableSelection(kIOAudioInputPortSubTypeSPDIF);
							debugIOLog ( 6, "  AppleOnboardAudio[%ld]::updateAllDetectCollection() - kLineInStatus-kInserted - Removed digital input from input selector due to auto input selection", mInstanceIndex);
						}
						mDetectCollection &= ~kSndHWDigitalInput;
					}
					
					if (NULL != mInternalMicrophoneInputString) {
						if (FALSE == mInputSelector->valueExists(kIOAudioInputPortSubTypeInternalMicrophone)) {
							mInputSelector->addAvailableSelection (kIOAudioInputPortSubTypeInternalMicrophone, mInternalMicrophoneInputString);
							debugIOLog ( 6, "  AppleOnboardAudio[%ld]::updateAllDetectCollection() - kLineInStatus-kInserted - Added internal microphone to input selector due to auto input selection", mInstanceIndex);
						}
						mDetectCollection |= kSndHWInternalMicrophone;
					}
					
					if (NULL != mLineInputString) {
						if (FALSE == mInputSelector->valueExists(kIOAudioInputPortSubTypeLine)) {
							mInputSelector->addAvailableSelection (kIOAudioInputPortSubTypeLine, mLineInputString);
							debugIOLog ( 6, "  AppleOnboardAudio[%ld]::updateAllDetectCollection() - kLineInStatus-kInserted - Added line input to input selector due to auto input selection", mInstanceIndex);
						}
					}
				}
			}
			else if (newValue == kRemoved)
			{
				mDetectCollection &= ~kSndHWLineInput;
				
				// [4208860]
				// Don't remove line in from the selector since we're going to keep it selected and make the user manually choose internal mic if desired
				// rather than automatically switching to it as specified in radar 4148027.  This is done to prevent accidental mic feecback.  
			}
			else
			{
				debugIOLog ( 6, "  Unknown line in status.");
			}
			break;
		case kDigitalInStatus:
			debugIOLog ( 6,  "  kDigitalInStatus mDetectCollection prior to modification %lX", mDetectCollection );
			if (newValue == kInserted)
			{
				mDetectCollection |= kSndHWDigitalInput;
				
				// [4148027]
				if (TRUE == mAutoSelectInput) {
					ConfigChangeHelper theConfigChangeHelper(mDriverDMAEngine);
					
					if (kGPIO_Unknown != mPlatformInterface->getComboInJackTypeConnected()) {
						if (TRUE == mInputSelector->valueExists(kIOAudioInputPortSubTypeLine)) {
							mInputSelector->removeAvailableSelection(kIOAudioInputPortSubTypeLine);
							debugIOLog ( 6, "  AppleOnboardAudio[%ld]::updateAllDetectCollection() - kDigitalInStatus-kInserted - Removed line input from input selector due to auto input selection", mInstanceIndex);
						}
						mDetectCollection &= ~kSndHWLineInput;
					}
					
					if (TRUE == mInputSelector->valueExists(kIOAudioInputPortSubTypeInternalMicrophone)) {
						mInputSelector->removeAvailableSelection ( kIOAudioInputPortSubTypeInternalMicrophone );
						debugIOLog ( 6, "  AppleOnboardAudio[%ld]::updateAllDetectCollection() - kDigitalInStatus-kInserted - Removed internal microphone from input selector due to auto input selection", mInstanceIndex);
					}
					mDetectCollection &= ~kSndHWInternalMicrophone;
					
					if (NULL != mDigitalInputString) {
						if (FALSE == mInputSelector->valueExists(kIOAudioInputPortSubTypeSPDIF)) {
							mInputSelector->addAvailableSelection (kIOAudioInputPortSubTypeSPDIF, mDigitalInputString);
							debugIOLog ( 6, "  AppleOnboardAudio[%ld]::updateAllDetectCollection() - kDigitalInStatus-kInserted - Added digital input to input selector due to auto input selection", mInstanceIndex);
						}
					}
				}
			}
			else if (newValue == kRemoved)
			{
				mDetectCollection &= ~kSndHWDigitalInput;
				
				// [4148027]
				if (TRUE == mAutoSelectInput) {
					ConfigChangeHelper theConfigChangeHelper(mDriverDMAEngine);
					
					if (TRUE == mInputSelector->valueExists(kIOAudioInputPortSubTypeSPDIF)) {
						mInputSelector->removeAvailableSelection ( kIOAudioInputPortSubTypeSPDIF );
						debugIOLog ( 6, "  AppleOnboardAudio[%ld]::updateAllDetectCollection() - kDigitalInStatus-kRemoved - Removed digital input from input selector due to auto input selection", mInstanceIndex);
					}
					
					if (NULL != mInternalMicrophoneInputString) {
						if (FALSE == mInputSelector->valueExists(kIOAudioInputPortSubTypeInternalMicrophone)) {
							mInputSelector->addAvailableSelection ( kIOAudioInputPortSubTypeInternalMicrophone, mInternalMicrophoneInputString );
							debugIOLog ( 6, "  AppleOnboardAudio[%ld]::updateAllDetectCollection() - kDigitalInStatus-kRemoved - Added internal microphone to input selector due to auto input selection", mInstanceIndex);
						}
						mDetectCollection |= kSndHWInternalMicrophone;
					}
					
					// [4208860]
					// Add line in to the selector since we're going to select it and make the user manually choose internal mic if desired
					// rather than automatically switching to it as specified in radar 4148027.  This is done to prevent accidental mic feecback.
					if (NULL != mLineInputString) {
						if (FALSE == mInputSelector->valueExists(kIOAudioInputPortSubTypeLine)) {
							mInputSelector->addAvailableSelection (kIOAudioInputPortSubTypeLine, mLineInputString);
							debugIOLog ( 6, "  AppleOnboardAudio[%ld]::updateAllDetectCollection() - kLineInStatus-kInserted - Added line input to input selector due to auto input selection", mInstanceIndex);
						}
						// don't update mDetectCollection here since line-in is not actually plugged in
					}
				}
			}
			else
			{
				debugIOLog ( 6, "  Unknown digital in status.");
			}
			break;
		case kExtSpeakersStatus:												//	[3398729]	begin	{
			debugIOLog ( 6,  "  kExtSpeakersStatus mDetectCollection prior to modification %lX", mDetectCollection );
			if (newValue == kInserted)
			{
				mDetectCollection &= ~kSndHWInternalSpeaker;
				mDetectCollection |= kSndHWCPUExternalSpeaker;
				//	[3413551]	begin	{
				if ( ( 0 != mInternalSpeakerOutputString ) && ( 0 != mExternalSpeakerOutputString ) )
				{
					debugIOLog ( 5, "  ** AppleOnboardAudio[%ld]::updateDetectCollection invoking 'ConfigChangeHelper'", mInstanceIndex );
					ConfigChangeHelper theConfigeChangeHelper(mDriverDMAEngine);	
					mOutputSelector->removeAvailableSelection ( kIOAudioOutputPortSubTypeInternalSpeaker );
					debugIOLog ( 6, "  removed internal speaker from output selector ***");
					mOutputSelector->addAvailableSelection (kIOAudioOutputPortSubTypeExternalSpeaker, mExternalSpeakerOutputString);
				}
				//	}	end		[3413551]
				debugIOLog ( 6, "  external speakers inserted, mDetectCollection = %lX", mDetectCollection);
			}
			else if (newValue == kRemoved)
			{
				mDetectCollection &= ~kSndHWCPUExternalSpeaker;
				mDetectCollection |= kSndHWInternalSpeaker;
				//	[3413551]	begin	{
				if ( ( 0 != mInternalSpeakerOutputString ) && ( 0 != mExternalSpeakerOutputString ) )
				{
					debugIOLog ( 5, "  ** AppleOnboardAudio[%ld]::updateDetectCollection invoking 'ConfigChangeHelper'", mInstanceIndex );
					ConfigChangeHelper theConfigeChangeHelper(mDriverDMAEngine);	
					mOutputSelector->removeAvailableSelection ( kIOAudioOutputPortSubTypeExternalSpeaker );
					debugIOLog ( 6, "  removed External Speaker from output selector");
					mOutputSelector->addAvailableSelection (kIOAudioOutputPortSubTypeInternalSpeaker, mInternalSpeakerOutputString);
				}
				//	}	end		[3413551]
				debugIOLog ( 6, "  external speakers removed, mDetectCollection = %lX", mDetectCollection);
			}
			else
			{
				debugIOLog ( 6, "  Unknown external speakers jack status, mDetectCollection = %lX", mDetectCollection);
			}
			break;																//	}	end	[3398729]
	}

	debugIOLog ( 5, "- AppleOnboardAudio[%ld]::updateAllDetectCollection ( %ld, %ld ), mDetectCollection = %lX", mInstanceIndex, statusSelector, newValue, mDetectCollection );
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
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
			debugIOLog ( 3,  "  UNKNOWN device so selecting INTERNAL SPEAKER" );
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
			debugIOLog ( 3,  "  UNKNOWN device so selecting INTERNAL SPEAKER" );
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
			debugIOLog ( 3,  "  UNKNOWN device so selecting INTERNAL SPEAKER" );
			selectorCode = kIOAudioOutputPortSubTypeInternalSpeaker;
			mDetectCollection |= kSndHWInternalSpeaker;
		}
	} else if (kDigitalOutStatus == eventSelector) {
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
			debugIOLog ( 3,  "UNKNOWN device so selecting INTERNAL SPEAKER" );
			selectorCode = kIOAudioOutputPortSubTypeInternalSpeaker;
			mDetectCollection |= kSndHWInternalSpeaker;
		}
    }
	       
	switch ( eventSelector ) {
		case kLineOutStatus:			debugIOLog ( 5, "± AppleOnboardAudio[%ld]::getSelectorCodeForOutputEvent ( %ld kLineOutStatus ), mDetectCollection %lX, returns '%4s'", mInstanceIndex, eventSelector, mDetectCollection, (char*)&selectorCode );			break;
		case kHeadphoneStatus:			debugIOLog ( 5, "± AppleOnboardAudio[%ld]::getSelectorCodeForOutputEvent ( %ld kHeadphoneStatus ), mDetectCollection %lX, returns '%4s'", mInstanceIndex, eventSelector, mDetectCollection, (char*)&selectorCode );			break;
		case kExtSpeakersStatus:		debugIOLog ( 5, "± AppleOnboardAudio[%ld]::getSelectorCodeForOutputEvent ( %ld kExtSpeakersStatus ), mDetectCollection %lX, returns '%4s'", mInstanceIndex, eventSelector, mDetectCollection, (char*)&selectorCode );		break;
		case kDigitalOutStatus:			debugIOLog ( 5, "± AppleOnboardAudio[%ld]::getSelectorCodeForOutputEvent ( %ld kDigitalOutStatus ), mDetectCollection %lX, returns '%4s'", mInstanceIndex, eventSelector, mDetectCollection, (char*)&selectorCode );		break;
		case kInternalSpeakerStatus:	debugIOLog ( 5, "± AppleOnboardAudio[%ld]::getSelectorCodeForOutputEvent ( %ld kInternalSpeakerStatus ), mDetectCollection %lX, returns '%4s'", mInstanceIndex, eventSelector, mDetectCollection, (char*)&selectorCode );   break;
		default:						debugIOLog ( 5, "± AppleOnboardAudio[%ld]::getSelectorCodeForOutputEvent ( %ld ), mDetectCollection %lX, returns '%4s'", mInstanceIndex, eventSelector, mDetectCollection, (char*)&selectorCode );							break;
	}

	return selectorCode;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AppleOnboardAudio::selectOutputAmplifiers (const UInt32 inSelection, const bool inMuteState, const bool inUpdateAll)
{
	bool								needToWaitForAmps;
	
	debugIOLog ( 3,  "+ AppleOnboardAudio[%ld]::selectOutputAmplifiers '%4s', inMuteState = %d", mInstanceIndex, (char *)&(inSelection), inMuteState );

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
				debugIOLog ( 3,  "  [AOA] switching amps to headphones.  mEncodedOutputFormat %d", mEncodedOutputFormat );
				mPlatformInterface->setSpeakerMuteState ( kGPIO_Muted );
				if (!mEncodedOutputFormat) {
					mPlatformInterface->setHeadphoneMuteState ( kGPIO_Unmuted );
				}
				if ((mDetectCollection & kSndHWLineOutput) && !mEncodedOutputFormat) {
					mPlatformInterface->setLineOutMuteState ( kGPIO_Unmuted );
				}
				break;
			case kIOAudioOutputPortSubTypeLine:
				debugIOLog ( 3,  "  [AOA] switching amps to line output.  mEncodedOutputFormat %d", mEncodedOutputFormat );
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
				debugIOLog ( 3,  "  [AOA] switching amps to speakers.  mEncodedOutputFormat %d", mEncodedOutputFormat );
				if (!mEncodedOutputFormat) {
					mPlatformInterface->setSpeakerMuteState ( kGPIO_Unmuted );
					// [3250195], don't want to get EQ on these outputs if we're on internal speaker.
					mPlatformInterface->setLineOutMuteState ( kGPIO_Muted );
					mPlatformInterface->setHeadphoneMuteState ( kGPIO_Muted );
				}
				break;
			case kIOAudioOutputPortSubTypeSPDIF:
				debugIOLog ( 3,  "  [AOA] switching to S/PDIF Digital Output.  mEncodedOutputFormat %d", mEncodedOutputFormat );
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
				debugIOLog ( 3, "  Amplifier not changed, selection = %ld", inSelection);
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
	
	debugIOLog ( 3,  "- AppleOnboardAudio[%ld]::selectOutputAmplifiers '%4s', inMuteState = %d, mCurrentOutputSelection '%4s'", mInstanceIndex, (char *)&(inSelection), inMuteState, &mCurrentOutputSelection );
    return;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AppleOnboardAudio::muteAllAmps() {
	mPlatformInterface->setHeadphoneMuteState ( kGPIO_Muted );
	mPlatformInterface->setLineOutMuteState ( kGPIO_Muted );
	mPlatformInterface->setSpeakerMuteState ( kGPIO_Muted );
	IOSleep (mAmpRecoveryMuteDuration);
	debugIOLog ( 3, "± AppleOnboardAudio::muteAllAmps ()");
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AppleOnboardAudio::free()
{
    debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::free", mInstanceIndex);
	
	removeTimerEvent ( this );

    if (mDriverDMAEngine) {
		debugIOLog ( 3, "  AppleOnboardAudio[%ld]::free - mDriverDMAEngine retain count = %d", mInstanceIndex, mDriverDMAEngine->getRetainCount());
        mDriverDMAEngine->release();
		mDriverDMAEngine = 0;
	}

#ifdef THREAD_POWER_MANAGEMENT
	if (0 != mPowerThread) {
		thread_call_free (mPowerThread);
	}
#endif

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
    debugIOLog ( 3, "- AppleOnboardAudio[%ld]::free, (void)", mInstanceIndex);
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
    
	debugIOLog ( 3, "± AppleOnboardAudio[%ld]::setCurrentDevices ( %ld ), currentDevices = %ld", mInstanceIndex, devices, currentDevices);

	if (devices & kSndHWInputDevices || odevice & kSndHWInputDevices) {
		if (0 != mInputConnectionControl) {
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
	
	thePluginObject = 0;
	theObject = mPluginObjects->getObject(index);
	
	if (0 != theObject) {
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
	AppleOnboardAudio *					aoa;
    UInt16								cachedProducerCount;

	aoa = OSDynamicCast (AppleOnboardAudio, object);
	FailIf ( 0 == aoa, Exit );
	
	for ( UInt32 index = 0; index < kNumberOfActionSelectors; index++ ) {
		cachedProducerCount = aoa->mInterruptProduced[index];
		if ( 0 != cachedProducerCount - aoa->mInterruptConsumed[index] ) {
			aoa->interruptEventHandler ( index, 0 );							//  [3515371]
			aoa->mInterruptConsumed[index] = cachedProducerCount;
		}
	}
Exit:
	return;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AppleOnboardAudio::interruptEventHandler (UInt32 statusSelector, UInt32 newValue) {
	IOCommandGate *						cg;
	
	debugIOLog ( 6, "+ AppleOnboardAudio::interruptEventHandler ( %d, %d )", statusSelector, newValue );
	cg = getCommandGate ();
	debugIOLog ( 6, "  cg %p", cg );
	if (0 != cg) {
		cg->runAction (interruptEventHandlerAction, (void *)statusSelector, (void *)newValue);
	}
	debugIOLog ( 6, "- AppleOnboardAudio::interruptEventHandler ( %d, %d )", statusSelector, newValue );
	return;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Must be called on the workloop via runAction()
IOReturn AppleOnboardAudio::interruptEventHandlerAction (OSObject * owner, void * arg1, void * arg2, void * arg3, void * arg4) {
	IOReturn 							result;
	AppleOnboardAudio * 				aoa;
	
	result = kIOReturnError;
	
	aoa = (AppleOnboardAudio *)owner;
	FailIf (0 == aoa, Exit);
	aoa->protectedInterruptEventHandler ((UInt32)arg1, (UInt32)arg2);

	result = kIOReturnSuccess;
Exit:
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool AppleOnboardAudio::isTargetForMessage ( UInt32 actionSelector, AppleOnboardAudio * theAOA ) {
	OSArray *	swInterruptsArray;
	OSString *	theInterruptString;
	UInt32		interruptCount;
	bool		result = FALSE;
	
	swInterruptsArray = OSDynamicCast ( OSArray, getLayoutEntry ( kSWInterrupts, theAOA ) );
	FailIf (0 == swInterruptsArray, Exit);
	interruptCount = swInterruptsArray->getCount ();
	for ( UInt32 index = 0; index < interruptCount; index++ ) {
		theInterruptString = OSDynamicCast ( OSString, swInterruptsArray->getObject ( index ) );
		if ( 0 != theInterruptString ) {
			switch ( actionSelector ) {
				case kClockLockStatus:
					if ( !strcmp ( kClockLockIntMessage, theInterruptString->getCStringNoCopy() ) ) {
						debugIOLog ( 5, "± AppleOnboardAudio[%ld]::isTargetForMessage ( 0x%0.8X, %p ) is for kClockLockStatus", mInstanceIndex, actionSelector, theAOA);
						result = TRUE;
					}
					break;
				case kClockUnLockStatus:
					if ( !strcmp ( kClockUnLockIntMessage, theInterruptString->getCStringNoCopy() ) ) {
						debugIOLog ( 5, "± AppleOnboardAudio[%ld]::isTargetForMessage ( 0x%0.8X, %p ) is for kClockUnLockStatus", mInstanceIndex, actionSelector, theAOA);
						result = TRUE;
					}
					break;
				case kDigitalInInsertStatus:
					if ( !strcmp ( kDigitalInInsertIntMessage, theInterruptString->getCStringNoCopy() ) ) {
						debugIOLog ( 5, "± AppleOnboardAudio[%ld]::isTargetForMessage ( 0x%0.8X, %p ) is for kDigitalInInsertStatus", mInstanceIndex, actionSelector, theAOA);
						result = TRUE;
					}
					break;
				case kDigitalInRemoveStatus:
					if ( !strcmp ( kDigitalInRemoveIntMessage, theInterruptString->getCStringNoCopy() ) ) {
						debugIOLog ( 5, "± AppleOnboardAudio[%ld]::isTargetForMessage ( 0x%0.8X, %p ) is for kDigitalInRemoveStatus", mInstanceIndex, actionSelector, theAOA);
						result = TRUE;
					}
					break;
				case kRemoteActive:										//  [3515371]
					if ( !strcmp ( kRemoteActiveMessage, theInterruptString->getCStringNoCopy() ) ) {
						debugIOLog ( 5, "± AppleOnboardAudio[%ld]::isTargetForMessage ( 0x%0.8X, %p ) is for kRemoteActive", mInstanceIndex, actionSelector, theAOA);
						result = TRUE;
					}
					break;
				case kRemoteIdle:										//  [3515371]
					if ( !strcmp ( kRemoteIdleMessage, theInterruptString->getCStringNoCopy() ) ) {
						debugIOLog ( 5, "± AppleOnboardAudio[%ld]::isTargetForMessage ( 0x%0.8X, %p ) is for kRemoteIdle", mInstanceIndex, actionSelector, theAOA);
						result = TRUE;
					}
					break;
				case kRemoteSleep:										//  [3515371]
					if ( !strcmp ( kRemoteSleepMessage, theInterruptString->getCStringNoCopy() ) ) {
						debugIOLog ( 5, "± AppleOnboardAudio[%ld]::isTargetForMessage ( 0x%0.8X, %p ) is for kRemoteSleep", mInstanceIndex, actionSelector, theAOA);
						result = TRUE;
					}
					break;
				case kRemoteChildSleep:									//  [3938771]
					if ( !strcmp ( kRemoteChildSleepMessage, theInterruptString->getCStringNoCopy() ) ) {
						debugIOLog ( 5, "± AppleOnboardAudio[%ld]::isTargetForMessage ( 0x%0.8X, %p ) is for kRemoteChildSleep", mInstanceIndex, actionSelector, theAOA);
						result = TRUE;
					}
					break;
				case kRemoteChildIdle:									//  [3938771]
					if ( !strcmp ( kRemoteChildIdleMessage, theInterruptString->getCStringNoCopy() ) ) {
						debugIOLog ( 5, "± AppleOnboardAudio[%ld]::isTargetForMessage ( 0x%0.8X, %p ) is for kRemoteChildIdle", mInstanceIndex, actionSelector, theAOA);
						result = TRUE;
					}
					break;
				case kRemoteChildActive:								//  [3938771]
					if ( !strcmp ( kRemoteChildActiveMessage, theInterruptString->getCStringNoCopy() ) ) {
						debugIOLog ( 5, "± AppleOnboardAudio[%ld]::isTargetForMessage ( 0x%0.8X, %p ) is for kRemoteChildActive", mInstanceIndex, actionSelector, theAOA);
						result = TRUE;
					}
					break;
				case kAOAPowerParent:									//  [3938771]
					if ( !strcmp ( kAOAPowerParentMessage, theInterruptString->getCStringNoCopy() ) ) {
						debugIOLog ( 5, "± AppleOnboardAudio[%ld]::isTargetForMessage ( 0x%0.8X, %p ) is for kAOAPowerParent", mInstanceIndex, actionSelector, theAOA);
						result = TRUE;
					}
					break;
				default:
					debugIOLog ( 5, "± AppleOnboardAudio[%ld]::isTargetForMessage ( 0x%0.8X, %p ) is USELESS!", mInstanceIndex, actionSelector, theAOA);
					FailMessage ( TRUE );
					break;
			}
		}
	}

Exit:
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
		case kInternalSpeakerStatus:		debugIOLog ( 7, "+ AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kInternalSpeakerStatus, %ld )", mInstanceIndex, newValue);			break;
		case kHeadphoneStatus:				debugIOLog ( 6, "+ AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kHeadphoneStatus, %ld )", mInstanceIndex, newValue);				break;
		case kExtSpeakersStatus:			debugIOLog ( 6, "+ AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kExtSpeakersStatus, %ld )", mInstanceIndex, newValue);				break;
		case kLineOutStatus:				debugIOLog ( 6, "+ AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kLineOutStatus, %ld )", mInstanceIndex, newValue);					break;
		case kDigitalInStatus:				debugIOLog ( 6, "+ AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kDigitalInStatus, %ld )", mInstanceIndex, newValue);				break;
		case kDigitalOutStatus:				debugIOLog ( 6, "+ AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kDigitalOutStatus, %ld )", mInstanceIndex, newValue);				break;
		case kLineInStatus:					debugIOLog ( 6, "+ AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kLineInStatus, %ld )", mInstanceIndex, newValue);					break;
		case kInputMicStatus:				debugIOLog ( 7, "+ AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kInputMicStatus, %ld )", mInstanceIndex, newValue);					break;
		case kExternalMicInStatus:			debugIOLog ( 7, "+ AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kExternalMicInStatus, %ld )", mInstanceIndex, newValue);			break;
		case kDigitalInInsertStatus:		debugIOLog ( 6, "+ AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kDigitalInInsertStatus, %ld )", mInstanceIndex, newValue);			break;
		case kDigitalInRemoveStatus:		debugIOLog ( 6, "+ AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kDigitalInRemoveStatus, %ld )", mInstanceIndex, newValue);			break;
		case kRequestCodecRecoveryStatus:	debugIOLog ( 7, "+ AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kRequestCodecRecoveryStatus, %ld )", mInstanceIndex, newValue);		break;
		case kClockInterruptedRecovery:		debugIOLog ( 7, "+ AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kClockInterruptedRecovery, %ld )", mInstanceIndex, newValue);		break;
		case kClockUnLockStatus:			debugIOLog ( 6, "+ AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kClockUnLockStatus, %ld )", mInstanceIndex, newValue);				break;
		case kClockLockStatus:				debugIOLog ( 6, "+ AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kClockLockStatus, %ld )", mInstanceIndex, newValue);				break;
		case kAES3StreamErrorStatus:		debugIOLog ( 7, "+ AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kAES3StreamErrorStatus, %ld )", mInstanceIndex, newValue);			break;
		case kCodecErrorInterruptStatus:	debugIOLog ( 6, "+ AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kCodecErrorInterruptStatus, %ld )", mInstanceIndex, newValue);		break;
		case kCodecInterruptStatus:			debugIOLog ( 5, "+ AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kCodecInterruptStatus, %ld )", mInstanceIndex, newValue);			break;
		case kBreakClockSelect:				debugIOLog ( 7, "+ AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kBreakClockSelect, %ld )", mInstanceIndex, newValue);				break;
		case kMakeClockSelect:				debugIOLog ( 7, "+ AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kMakeClockSelect, %ld )", mInstanceIndex, newValue);				break;
		case kSetSampleRate:				debugIOLog ( 7, "+ AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kSetSampleRate, %ld )", mInstanceIndex, newValue);					break;
		case kSetSampleBitDepth:			debugIOLog ( 7, "+ AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kSetSampleBitDepth, %ld )", mInstanceIndex, newValue);				break;
		case kPowerStateChange:				debugIOLog ( 7, "+ AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kPreDMAEngineInit, %ld )", mInstanceIndex, newValue);				break;
		case kPreDMAEngineInit:				debugIOLog ( 7, "+ AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kPreDMAEngineInit, %ld )", mInstanceIndex, newValue);				break;
		case kPostDMAEngineInit:			debugIOLog ( 7, "+ AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kPostDMAEngineInit, %ld )", mInstanceIndex, newValue);				break;
		case kRestartTransport:				debugIOLog ( 5, "+ AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kRestartTransport, %ld )", mInstanceIndex, newValue);				break;
		case kRemoteActive:					debugIOLog ( 5, "+ AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kRemoteActive, %ld )", mInstanceIndex, newValue);					break;		//  [3515371]
		case kRemoteSleep:					debugIOLog ( 5, "+ AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kRemoteSleep, %ld )", mInstanceIndex, newValue);					break;		//  [3515371]
		case kRemoteIdle:					debugIOLog ( 5, "+ AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kRemoteIdle, %ld )", mInstanceIndex, newValue);						break;		//  [3515371]
		case kRemoteChildSleep:				debugIOLog ( 5, "+ AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kRemoteChildSleep, %ld )", mInstanceIndex, newValue);				break;		//  [3515371]
		case kRemoteChildIdle:				debugIOLog ( 5, "+ AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kRemoteChildIdle, %ld )", mInstanceIndex, newValue);				break;		//  [3515371]
		case kRemoteChildActive:			debugIOLog ( 5, "+ AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kRemoteChildActive, %ld )", mInstanceIndex, newValue);				break;		//  [3515371]
		default:							debugIOLog ( 7, "+ AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( 0x%0.8X, %ld )", mInstanceIndex, statusSelector, newValue);			break;		//  [3515371]
	}

	if ( !executeProtectedInterruptEventHandlerWhenInactive ( statusSelector ) )
	{
		goto Exit;		// don't care about interrupts while we're powered off mpc - EXCEPT FOR POWER MESSAGES! [3515371]
	}

	selectorCode = getCharCodeForIntCode (statusSelector);

	switch (statusSelector) {
		case kDigitalOutStatus:
			startDetectInterruptService ();											//	[3933529]
			mDriverDMAEngine->updateOutputStreamFormats ();
			if (TRUE == mEncodedOutputFormat && TRUE == mDriverDMAEngine->isMixable ()) {
				mEncodedOutputFormat = FALSE;		
			}	
			// fall through intentionally
		case kInternalSpeakerStatus:
		case kHeadphoneStatus:
		case kLineOutStatus:
		case kExtSpeakersStatus:													//	[3398729]
			if ( kDigitalOutStatus != statusSelector )
			{
				startDetectInterruptService ();										//	[3933529]
			}
			updateAllDetectCollection (statusSelector, newValue);

			// This method parses the detect collection to see if this was an insert or extraction, and returns the appropriate output selection
			selectorCode = getSelectorCodeForOutputEvent (statusSelector);

			// [3656784] prevent redirection to analog outputs on machines with digital out but no detect if format is encoded.
			if (kGPIO_Unknown == mPlatformInterface->getComboOutJackTypeConnected()) {
				if (mCurrentOutputSelection == kIOAudioOutputPortSubTypeSPDIF && mEncodedOutputFormat && selectorCode != kIOAudioOutputPortSubTypeSPDIF) {
					goto Exit;
				}	
			}
			
			//	[3588678]	Prevent redundant redirection
			if ( mCurrentOutputSelection != selectorCode ) {
				connectionCodeNumber = OSNumber::withNumber (selectorCode, 32);
				debugIOLog ( 5, "  selectorCode = '%4s', connectionCodeNumber = %ld", &selectorCode, connectionCodeNumber );
				
				// [3250195], must update this too!  Otherwise you get the right output selected but with the wrong clip routine/EQ.
				pluginString = getConnectionKeyFromCharCode (selectorCode, kIOAudioStreamDirectionOutput);
				debugIOLog ( 3, "  pluginString updated to '%s'.", pluginString);
				
				// [3323073], move code from above, and now set the current output plugin if it's changing!
				thePlugin = getPluginObjectForConnection (pluginString);
				FailWithAction (0 == thePlugin, endDetectInterruptService (), Exit);	//	[3933529]
				
				cacheOutputVolumeLevels (mCurrentOutputPlugin);
				mCurrentOutputPlugin = thePlugin;
				
				// [3250195], current plugin doesn't matter here, always need these updated
				setClipRoutineForOutput (pluginString);

				selectCodecOutputWithMuteState( mIsMute );
				selectOutputAmplifiers (selectorCode, mIsMute);
				
				if (0 == mOutputSelector) debugIOLog ( 3, "\n!!!  mOutputSelector = 0!!!");
				FailWithAction (0 == mOutputSelector, endDetectInterruptService (), Exit);	//	[3933529]
				
				//  [3745129]   IMPORTANT:  Invoking 'hardwareValueChanged' will wake the hardware if it is not
				//				already awake.  This will set the analog control register in the TAS3004 to enable
				//				audio output.  This code can be re-ordered but invoking 'hardwareValueChanged' must
				//				be retained in any code change applied here.
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
			endDetectInterruptService ();											//	[3933529]
			break;
		case kExternalMicInStatus:
			startDetectInterruptService ();											//	[3933529]
			updateAllDetectCollection (statusSelector, newValue);
			endDetectInterruptService ();											//	[3933529]
			break;
		case kLineInStatus:
			startDetectInterruptService ();											//	[3933529]
			updateAllDetectCollection (statusSelector, newValue);

			// [3250612] don't do anything on insert events!
			pluginString = getConnectionKeyFromCharCode (selectorCode, kIOAudioStreamDirectionInput);
			thePlugin = getPluginObjectForConnection (pluginString);
			FailIf (0 == thePlugin, Exit);

			FailIf (0 == mInputSelector, Exit);			
			selectorCode = mInputSelector->getIntValue ();
			
			//	Mute the line input if nothing is connected to Line In
			if ((mDetectCollection & kSndHWLineInput) && (kIOAudioInputPortSubTypeLine == selectorCode)) {
				thePlugin->setInputMute (FALSE);
			} else if (!(mDetectCollection & kSndHWLineInput) && (kIOAudioInputPortSubTypeLine == selectorCode)) {
				thePlugin->setInputMute (TRUE);
			}
			
			if ( mAutoSelectInput ) {  // [4148027], [4208860]
				//	If line input attached and analog is line input device then make
				//	sure the analog selectors are published and select line input.  If
				//	line input is detached and no digital source is attached then publish
				//	the internal microphone selector and the line input selector and select 
				//  line input (to prevent accidental mic feedback).  If digital is attached then
				//	publish only the digital input and select digital input.
				
				// If unplugging line input while line input was selected, keep line input selected rather than switching to internal mic to prevent
				// accidental mic feedback [4208860]
				if (!(mDetectCollection & kSndHWLineInput) && (kIOAudioInputPortSubTypeLine == selectorCode)) {
					debugIOLog(6, "AppleOnboardAudio[%ld]::protectedInterruptEventHandler - kLineInStatus posted...line in disconnected while selected (mAutoSelectInput = %d)", mInstanceIndex, mAutoSelectInput);
				}
				else if (mDetectCollection & kSndHWLineInput) {
					debugIOLog(6, "AppleOnboardAudio[%ld]::protectedInterruptEventHandler - kLineInStatus posted...line in connected (mAutoSelectInput = %d)", mInstanceIndex, mAutoSelectInput);
					if (kIOReturnSuccess == inputSelectorChanged(kIOAudioInputPortSubTypeLine)) {
						connectionCodeNumber = OSNumber::withNumber (kIOAudioInputPortSubTypeLine, 32);
						mInputSelector->hardwareValueChanged (connectionCodeNumber);
						connectionCodeNumber->release();
					}
				}
				else {
					debugIOLog(6, "AppleOnboardAudio[%ld]::protectedInterruptEventHandler - kLineInStatus posted...line in disconnected while not selected (mAutoSelectInput = %d)", mInstanceIndex, mAutoSelectInput);
				}
			}
			
			endDetectInterruptService ();											//	[3933529]
			break;
		case kDigitalInStatus:
			startDetectInterruptService ();											//	[3933529]
			
			// [4073140]
			if ( mAutoSelectClock && ( kClockLockStatus == mCodecLockStatus ) && ( kClockSourceSelectionExternal == mCurrentClockSelector ) && ( kRemoved == newValue ) ) {
				muteAllAmps();
			}
			else if ( mAutoSelectClock && ( kClockLockStatus == mCodecLockStatus ) && ( kInserted == newValue ) ) {		// [4189622]
				selectCodecOutputWithMuteState (mIsMute);
				
				if ( 0 != mOutputSelector ) {
					selectOutputAmplifiers (mOutputSelector->getIntValue (), mIsMute);	
				}
			}
			
			updateAllDetectCollection (statusSelector, newValue);
			mDigitalInsertStatus = ( kInserted == newValue ) ? kGPIO_Connected : kGPIO_Disconnected;
			
			// [4267209, 4244167]
			// It may be the case that the digital-in codec reports lock to external clock even if there is nothing plugged in, e.g. if the PLL
			// locks on noise.  So we should only allow auto clock selection if digital-in is actually present.
			if ( mAutoSelectClock ) {
				if ( kInserted == newValue ) {
					debugIOLog( 5, "AppleOnboardAudio[%ld]::protectedInterruptEventHandler - digital-in inserted...enabling auto clock selection", mInstanceIndex );
					mDisableAutoSelectClock = FALSE;
				}
				else if ( kRemoved == newValue ) {
					debugIOLog( 5, "AppleOnboardAudio[%ld]::protectedInterruptEventHandler - digital-in removed...disabling auto clock selection", mInstanceIndex );
					mDisableAutoSelectClock = TRUE;
				}
				else {
					debugIOLog( 5, "AppleOnboardAudio[%ld]::protectedInterruptEventHandler - unknown digital-in status", mInstanceIndex );
				}
			}
			
			if ( mAutoSelectInput ) {	// [4148027], [4208860]
				//	If line input attached and analog is line input device then make
				//	sure the analog selectors are published and select line input.  If
				//	line input is detached and no digital source is attached then publish
				//	the internal microphone selector and the line input selector and select 
				//  line input (to prevent accidental mic feedback).  If digital is attached then
				//	publish only the digital input and select digital input.
				
				if ( kInserted == newValue ) {
					debugIOLog(6, "AppleOnboardAudio[%ld]::protectedInterruptEventHandler - kDigitalInStatus posted...digital in connected (mAutoSelectInput = %d)", mInstanceIndex, mAutoSelectInput);
					if (kIOReturnSuccess == inputSelectorChanged(kIOAudioInputPortSubTypeSPDIF)) {
						connectionCodeNumber = OSNumber::withNumber (kIOAudioInputPortSubTypeSPDIF, 32);
						mInputSelector->hardwareValueChanged (connectionCodeNumber);
						connectionCodeNumber->release();
					}
				}
				else {
					debugIOLog(6, "AppleOnboardAudio[%ld]::protectedInterruptEventHandler - kDigitalInStatus posted...digital in disconnected (mAutoSelectInput = %d)", mInstanceIndex, mAutoSelectInput);
					if (kIOReturnSuccess == inputSelectorChanged(kIOAudioInputPortSubTypeLine)) {
						connectionCodeNumber = OSNumber::withNumber (kIOAudioInputPortSubTypeLine, 32);
						mInputSelector->hardwareValueChanged (connectionCodeNumber);
						connectionCodeNumber->release();
					}
				}
			}
			
			endDetectInterruptService ();											//	[3933529]
			break;
		case kDigitalInInsertStatus:
		case kDigitalInRemoveStatus:
			startDetectInterruptService ();											//	[3933529]
			updateAllDetectCollection (kDigitalInStatus, newValue);
			mDigitalInsertStatus = ( kDigitalInInsertStatus == statusSelector ) ? kGPIO_Connected : kGPIO_Disconnected;
			callPluginsInOrder ( kDigitalInStatus, mDigitalInsertStatus );
			endDetectInterruptService ();											//	[3933529]
			break;
		case kRequestCodecRecoveryStatus:
			//	Walk through the available plugin objects and invoke the recoverFromFatalError on each object in the correct order.
			callPluginsInOrder (kRequestCodecRecoveryStatus, newValue);
			break;
		case kRestartTransport:
			//	This message is used to restart the transport hardware without invoking a general recovery or
			//	a recovery from clock interruption and is used to perform sequence sensitive initialization.
			if (0 != mTransportInterface) {
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
			if ( mRemoteNonDetectInterruptEnabled )			//	[3935620],[3942561]
			{
				//  Only respond to a change in clock lock status (some codecs broadcast redundant messages)
				//  by handling the interrupt locally if a clock selector control exists or by handling the
				//  interrupt remotely if no clock selector exists (assumes the clock selector exists in a
				//  remote AppleOnboardAudio instance).  22 Mar 2004 rbm
				if ( mCodecLockStatus != statusSelector ) {  //  [3628559]	a change in status occurred
					if ( kClockLockStatus == statusSelector ) {		//	and the change is to a lock state
						//	[4073140] if auto clock select and on internal clock then switch to external
						//	[4196870] We need to disable auto clock selection when going idle or going to sleep
						//	since we force redirect to internal clock and we don't want a poll to come in after
						//	the force redirect and revert to external clock.  
						if ( mAutoSelectClock && ( FALSE == mDisableAutoSelectClock ) )	{
							if ( !mClockSelectInProcessSemaphore ) {
								if ( kClockSourceSelectionExternal != mCurrentClockSelector ) {
									muteAllAmps ();
									callPluginsInOrder ( kSetMuteState, TRUE );
									
									if (NULL != mExternalClockSelector) {
										ConfigChangeHelper theConfigChangeHelper(mDriverDMAEngine);
										OSNumber * clockSourceSelector = OSNumber::withNumber ( kClockSourceSelectionExternal, 32 );
										
										debugIOLog(5, "AppleOnboardAudio::protectedInterruptEventHandler - Auto Clock Select - changing clock source selection to external");
										
										mExternalClockSelector->removeAvailableSelection(kClockSourceSelectionInternal);
										mExternalClockSelector->addAvailableSelection(kClockSourceSelectionExternal, kExternalClockString);
										mExternalClockSelector->hardwareValueChanged (clockSourceSelector);
										clockSourceSelector->release ();
									}
									
									//	Set the hardware to SLAVE mode.
									clockSelectorChanged ( kClockSourceSelectionExternal );
								}
							}
						}
						
						debugIOLog ( 5, "  ===== AppleOnboardAudio[%ld] CLOCK STATUS changed to kClockLockStatus, mClockSelectInProcessSemaphore = %d", mInstanceIndex, mClockSelectInProcessSemaphore );
						UInt32 tempSampleRateWhole = mTransportInterface->transportGetSampleRate ();
						//	Only notify core audio of a rate change if the rate actually changed.
						if ( mTransportSampleRate.whole != tempSampleRateWhole ) {
							mTransportSampleRate.whole = mTransportInterface->transportGetSampleRate ();										//  [3684994]
							debugIOLog ( 4, "  *** about to [%ld]mDriverDMAEngine->hardwareSampleRateChanged ( %d )", mInstanceIndex, mTransportSampleRate.whole );	//  [3684994]
							mDriverDMAEngine->hardwareSampleRateChanged ( &mTransportSampleRate );												//  [3684994]
							mDriverDMAEngine->updateDSPForSampleRate(mTransportSampleRate.whole);  // [4220086]
						}
					} else if ( kClockUnLockStatus == statusSelector ) {
						debugIOLog ( 5, "  ===== AppleOnboardAudio[%ld] CLOCK STATUS changed to kClockUnLockStatus, mClockSelectInProcessSemaphore = %d", mInstanceIndex, mClockSelectInProcessSemaphore );
					}
					
					UInt32 transportType = mTransportInterface->transportGetTransportInterfaceType();
					if ( kTransportInterfaceType_I2S_Slave_Only != transportType ) {
						mCodecLockStatus = statusSelector;
						if ( 0 != mExternalClockSelector ) {
							//	Codec clock loss errors are to be ignored if in the process of switching clock sources.
							if ( mAutoSelectClock ) {  //	[4073140] if auto clock select and on external clock then switch to internal
								if ( !mClockSelectInProcessSemaphore ) {
									//	An 'kClockUnLockStatus' status selector requires that the clock source
									//	be redirected back to an internal source (i.e. the internal hardware is to act as a MASTER).
									if ( kClockUnLockStatus == statusSelector ) {
										if ( kClockSourceSelectionInternal != mCurrentClockSelector ) {
											ConfigChangeHelper theConfigChangeHelper(mDriverDMAEngine);
											OSNumber * clockSourceSelector = OSNumber::withNumber ( kClockSourceSelectionInternal, 32 );
											
											muteAllAmps ();
											callPluginsInOrder ( kSetMuteState, TRUE );
											
											debugIOLog(5, "AppleOnboardAudio::protectedInterruptEventHandler - Auto Clock Select - changing clock source selection to internal");
											
											// [4189050]
											// The hardware plugin passes a pending relock message to the interrupt event handler if it detects a change
											// in sampling rate or bit depth.  This serves as a notification that we have not truly lost lock,
											// but rather are force unlocking to subsequently relock with the new sampling rate or bit depth
											// settings.  This enables us to mute until the relock in order to minimize artifacts.
											if ( kAutoClockLockStatePendingRelock == newValue ) {
												mRelockToExternalClockInProgress = TRUE;
											}
											
											mExternalClockSelector->removeAvailableSelection(kClockSourceSelectionExternal);
											mExternalClockSelector->addAvailableSelection(kClockSourceSelectionInternal, kInternalClockString);
											mExternalClockSelector->hardwareValueChanged (clockSourceSelector);
											clockSourceSelector->release ();
											
											//	Set the hardware to MASTER mode.
											clockSelectorChanged ( kClockSourceSelectionInternal );
										}
									} else {
										//	[3435307]	Dont touch amplifier mutes related to UI.	RBM
										//	[3253678]	successful lock detected, now safe to unmute analog part
										callPluginsInOrder ( kSetMuteState, mIsMute );
									}
								} else {
									if ( kClockLockStatus == statusSelector ) {
										debugIOLog ( 3,  "  Attempted to post kClockLockStatus blocked by 'mClockSelectInProcessSemaphore' semaphore" );
									} else if ( kClockUnLockStatus == statusSelector ) {
										debugIOLog ( 3,  "  Attempted to post kClockUnLockStatus blocked by 'mClockSelectInProcessSemaphore' semaphore" );
									}
								}
							}
						}
					}
				} else {
					if ( kClockLockStatus == statusSelector ) {
						debugIOLog ( 5, "  ===== AppleOnboardAudio[%ld] CLOCK STATUS redundant post of kClockLockStatus, mClockSelectInProcessSemaphore = %d", mInstanceIndex, mClockSelectInProcessSemaphore );
						
						// [4244167, 4267209]
						// If the codec is reporting lock but digital-in isn't plugged in, it indicates that the codec has locked to a false
						// clock and we should force re-direct to internal clock if we're on external.  
						if ( mAutoSelectClock && ( 0 == ( kSndHWDigitalInput & mDetectCollection ) ) && ( kClockSourceSelectionExternal == mCurrentClockSelector ) && ( !mClockSelectInProcessSemaphore ) ) {
							ConfigChangeHelper theConfigChangeHelper(mDriverDMAEngine);
							OSNumber * clockSourceSelector = OSNumber::withNumber ( kClockSourceSelectionInternal, 32 );
							
							mDisableAutoSelectClock = TRUE;
							
							muteAllAmps ();
							callPluginsInOrder ( kSetMuteState, TRUE );
							
							debugIOLog(5, "AppleOnboardAudio::protectedInterruptEventHandler - Auto Clock Select - changing clock source selection to internal due to lock message with non-existent digital-in");
							
							mExternalClockSelector->removeAvailableSelection(kClockSourceSelectionExternal);
							mExternalClockSelector->addAvailableSelection(kClockSourceSelectionInternal, kInternalClockString);
							mExternalClockSelector->hardwareValueChanged (clockSourceSelector);
							clockSourceSelector->release ();
							
							//	Set the hardware to MASTER mode.
							clockSelectorChanged ( kClockSourceSelectionInternal );
						}
					} else if ( kClockUnLockStatus == statusSelector ) {
						debugIOLog ( 5, "  ===== AppleOnboardAudio[%ld] CLOCK STATUS redundant post of kClockUnLockStatus, mClockSelectInProcessSemaphore = %d", mInstanceIndex, mClockSelectInProcessSemaphore );
						
						// [4189050]
						// If we think we're relocking but never actually do, we'll remain muted, so after a certain number of polls,
						// assume that relock isn't going to happen and that we're remaining unlocked, and unmute.  An example of this 
						// situation is when we think we're going to relock, say across a sampling rate change, and external clock is
						// lost before relock ever happens.  Also unmute if we're told that we've returned to a normal unlock state.
						if ( mAutoSelectClock && mRelockToExternalClockInProgress ) {
							mRelockToExternalClockPollCount++;
							
							if ( ( kRelockToExternalClockMaxNumPolls == mRelockToExternalClockPollCount ) || ( kAutoClockLockStateNormal == newValue ) ) {
								mRelockToExternalClockInProgress = FALSE;
								mRelockToExternalClockPollCount = 0;
								
								selectCodecOutputWithMuteState (mIsMute);
								if ( 0 != mOutputSelector ) {
									selectOutputAmplifiers ( mOutputSelector->getIntValue (), mIsMute);
								}
							}
						}
					}
				}
				
				//	Lock and unlock messages are always broadcast by the source.  Filtering of redundant
				//	messages always occurs at the receiver.  This is necessary as the initial broadcast
				//	may not have a receiver present if the receiver instance of AppleOnboardAudio has
				//	not yet been loaded or initialized.
				broadcastSoftwareInterruptMessage ( statusSelector );
				if ( mNeedsLockStatusUpdateToUnmute ) {					//  [3678605]
					debugIOLog ( 4, "  *** [%ld] about to unmute from mNeedsLockStatusUpdateToUnmute", mInstanceIndex );
					mNeedsLockStatusUpdateToUnmute = FALSE;
					selectCodecOutputWithMuteState (mIsMute);

					if ( 0 != mOutputSelector ) {
						selectOutputAmplifiers (mOutputSelector->getIntValue (), mIsMute);	//	Radar 3416318:	mOutMuteControl does not touch GPIOs so do so here!
					}
					if (0 != mOutMuteControl) {
						mOutMuteControl->flushValue ();					// Restore hardware to the user's selected state
					}
				}
			}
			else
			{
				switch ( statusSelector )
				{
					case kClockUnLockStatus:
						debugIOLog ( 6, "  ##### kClockUnLockStatus IGNORED - INTERRUPT DISABLED" );
						break;
					case kClockLockStatus:
						debugIOLog ( 6, "  ##### kClockLockStatus IGNORED - INTERRUPT DISABLED" );
						break;
				}
			}
			break;
		case kAES3StreamErrorStatus:
			//	indicates that the V bit states data is invalid or may be compressed data
			debugIOLog ( 7,  "  ... kAES3StreamErrorStatus %d", (unsigned int)newValue );
			if ( newValue ) {
				//	As appropriate (TBD)...
			}
			break;
		case kRemoteActive:																	//  [3515371]
			pendingPowerState = kIOAudioDeviceActive;
			performPowerStateChangeAction ( (void*)kIOAudioDeviceActive );					//  [3515371]
			break;
		case kRemoteIdle:																	//  [3515371]
			pendingPowerState = kIOAudioDeviceIdle;
			performPowerStateChangeAction ( (void*)kIOAudioDeviceIdle );					//  [3515371]
			break;
		case kRemoteSleep:																	//  [3515371]
			pendingPowerState = kIOAudioDeviceSleep;
			performPowerStateChangeAction ( (void*)kIOAudioDeviceSleep );					//  [3515371]
			break;
		case kRemoteChildSleep:
			pendingPowerState = kIOAudioDeviceSleep;
			performPowerStateChangeAction ( (void*)kIOAudioDeviceSleep );					//  [3938771]
			break;
		case kRemoteChildIdle:
			pendingPowerState = kIOAudioDeviceIdle;
			performPowerStateChangeAction ( (void*)kIOAudioDeviceIdle );					//  [3938771]
			break;
		case kRemoteChildActive:
			pendingPowerState = kIOAudioDeviceActive;
			performPowerStateChangeAction ( (void*)kIOAudioDeviceActive );					//  [3938771]
			break;
		default:
			break;
	}

Exit:
	switch ( statusSelector ) {
		case kInternalSpeakerStatus:		debugIOLog ( 7, "- AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kInternalSpeakerStatus, %ld )", mInstanceIndex, newValue);			break;
		case kHeadphoneStatus:				debugIOLog ( 6, "- AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kHeadphoneStatus, %ld )", mInstanceIndex, newValue);				break;
		case kExtSpeakersStatus:			debugIOLog ( 6, "- AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kExtSpeakersStatus, %ld )", mInstanceIndex, newValue);				break;
		case kLineOutStatus:				debugIOLog ( 6, "- AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kLineOutStatus, %ld )", mInstanceIndex, newValue);					break;
		case kDigitalInStatus:				debugIOLog ( 6, "- AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kDigitalInStatus, %ld )", mInstanceIndex, newValue);				break;
		case kDigitalOutStatus:				debugIOLog ( 6, "- AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kDigitalOutStatus, %ld )", mInstanceIndex, newValue);				break;
		case kLineInStatus:					debugIOLog ( 6, "- AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kLineInStatus, %ld )", mInstanceIndex, newValue);					break;
		case kInputMicStatus:				debugIOLog ( 7, "- AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kInputMicStatus, %ld )", mInstanceIndex, newValue);					break;
		case kExternalMicInStatus:			debugIOLog ( 7, "- AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kExternalMicInStatus, %ld )", mInstanceIndex, newValue);			break;
		case kDigitalInInsertStatus:		debugIOLog ( 6, "- AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kDigitalInInsertStatus, %ld )", mInstanceIndex, newValue);			break;
		case kDigitalInRemoveStatus:		debugIOLog ( 6, "- AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kDigitalInRemoveStatus, %ld )", mInstanceIndex, newValue);			break;
		case kRequestCodecRecoveryStatus:	debugIOLog ( 7, "- AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kRequestCodecRecoveryStatus, %ld )", mInstanceIndex, newValue);		break;
		case kClockInterruptedRecovery:		debugIOLog ( 7, "- AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kClockInterruptedRecovery, %ld )", mInstanceIndex, newValue);		break;
		case kClockUnLockStatus:			debugIOLog ( 6, "- AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kClockUnLockStatus, %ld )", mInstanceIndex, newValue);				break;
		case kClockLockStatus:				debugIOLog ( 6, "- AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kClockLockStatus, %ld )", mInstanceIndex, newValue);				break;
		case kAES3StreamErrorStatus:		debugIOLog ( 7, "- AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kAES3StreamErrorStatus, %ld )", mInstanceIndex, newValue);			break;
		case kCodecErrorInterruptStatus:	debugIOLog ( 6, "- AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kCodecErrorInterruptStatus, %ld )", mInstanceIndex, newValue);		break;
		case kCodecInterruptStatus:			debugIOLog ( 5, "- AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kCodecInterruptStatus, %ld )", mInstanceIndex, newValue);			break;
		case kBreakClockSelect:				debugIOLog ( 7, "- AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kBreakClockSelect, %ld )", mInstanceIndex, newValue);				break;
		case kMakeClockSelect:				debugIOLog ( 7, "- AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kMakeClockSelect, %ld )", mInstanceIndex, newValue);				break;
		case kSetSampleRate:				debugIOLog ( 7, "- AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kSetSampleRate, %ld )", mInstanceIndex, newValue);					break;
		case kSetSampleBitDepth:			debugIOLog ( 7, "- AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kSetSampleBitDepth, %ld )", mInstanceIndex, newValue);				break;
		case kPowerStateChange:				debugIOLog ( 7, "- AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kPreDMAEngineInit, %ld )", mInstanceIndex, newValue);				break;
		case kPreDMAEngineInit:				debugIOLog ( 7, "- AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kPreDMAEngineInit, %ld )", mInstanceIndex, newValue);				break;
		case kPostDMAEngineInit:			debugIOLog ( 7, "- AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kPostDMAEngineInit, %ld )", mInstanceIndex, newValue);				break;
		case kRestartTransport:				debugIOLog ( 5, "- AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kRestartTransport, %ld )", mInstanceIndex, newValue);				break;
		case kRemoteActive:					debugIOLog ( 5, "- AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kRemoteActive, %ld )", mInstanceIndex, newValue);					break;		//  [3515371]
		case kRemoteSleep:					debugIOLog ( 5, "- AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kRemoteSleep, %ld )", mInstanceIndex, newValue);					break;		//  [3515371]
		case kRemoteIdle:					debugIOLog ( 5, "- AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kRemoteIdle, %ld )", mInstanceIndex, newValue);						break;		//  [3515371]
		case kRemoteChildSleep:				debugIOLog ( 5, "- AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kRemoteChildSleep, %ld )", mInstanceIndex, newValue);				break;		//  [3515371]
		case kRemoteChildIdle:				debugIOLog ( 5, "- AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kRemoteChildIdle, %ld )", mInstanceIndex, newValue);				break;		//  [3515371]
		case kRemoteChildActive:			debugIOLog ( 5, "- AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( kRemoteChildActive, %ld )", mInstanceIndex, newValue);				break;		//  [3515371]
		default:							debugIOLog ( 7, "- AppleOnboardAudio[%ld]::protectedInterruptEventHandler ( 0x%0.8X, %ld )", mInstanceIndex, statusSelector, newValue);			break;		//  [3515371]
	}
	return;
}


//	--------------------------------------------------------------------------------
bool AppleOnboardAudio::executeProtectedInterruptEventHandlerWhenInactive ( UInt32 selector )
{
	bool			result = FALSE;
	
	debugIOLog ( 6, "+ AppleOnboardAudio::executeProtectedInterruptEventHandlerWhenInactive (%lx)", selector );

	debugIOLog ( 6, "   getPowerState () = %ld, pendingPowerState = %ld", getPowerState (), pendingPowerState );
	
	if ( kIOAudioDeviceSleep == getPowerState () && kIOAudioDeviceSleep == pendingPowerState )
	{
		debugIOLog ( 6, "   power state = SLEEP" );
		switch ( selector )
		{
			case kRemoteActive:					//	fall through
			case kRemoteSleep:					//	fall through
			case kRemoteIdle:					//	fall through
			case kRemoteChildSleep:				//	fall through
			case kRemoteChildIdle:				//	fall through
			case kRemoteChildActive:
				result = TRUE;
				break;
		}
	}
	else
	{
		result = TRUE;
	}

	debugIOLog ( 6, "- AppleOnboardAudio::executeProtectedInterruptEventHandlerWhenInactive () returns %d", result );

	return result;
}


#pragma mark +INTERRUPT POWER MANAGMENT SUPPORT
//	--------------------------------------------------------------------------------
void AppleOnboardAudio::startDetectInterruptService ( void )
{
	debugIOLog ( 6, "+ AppleOnboardAudio::startDetectInterruptService" );
	//	[3933529]
	//	A detect interrupt handler is starting.  If currently in the kIOAudioDeviceIdle state then
	//	switch to the kIOAudioDeviceActiveState.  Entry to active should not dispatch to the
	//	interrupt handlers to avoid recursion.  It is only necessary to wake if a change in output port is being posted.
	if ( kIOAudioDeviceActive != getPowerState () )
	{
		if ( mDetectCollection != getValueForDetectCollection ( mDetectCollection ) )	// read hardware state
		{
			//	THE FOLLOWING 'IOLog' IS REQUIRED AND SHOULD NOT BE MOVED.  POWER MANAGEMENT
			//	VERIFICATION CAN ONLY BE PERFORMED USING THE SYSTEM LOG!  AOA Viewer can be 
			//	used to enable or disable kprintf power management logging messages.
			if ( mDoKPrintfPowerState )
			{
				IOLog ( "AppleOnboardAudio[%ld]::protectedInterruptEventHandler setting power state to ACTIVE\n", mInstanceIndex );
			}
			FailMessage ( kIOReturnSuccess != doLocalChangeToActiveState ( FALSE, 0 ) );
		}
	}
	debugIOLog ( 6, "- AppleOnboardAudio::startDetectInterruptService" );
}


//	--------------------------------------------------------------------------------
void AppleOnboardAudio::endDetectInterruptService ( void )
{
	debugIOLog ( 6, "+ AppleOnboardAudio::endDetectInterruptService" );
	//	[3933529]
	//	A detect interrupt is completing.  If running on battery power then scheduled a deferred
	//	request to enter the kIOAudioDeviceIdle state.
	debugIOLog ( 6, "  mCurrentAggressivenessLevel %ld", mCurrentAggressivenessLevel );
	if ( ( 0 == sTotalNumAOAEnginesRunning ) && ( kIOPMInternalPower == mCurrentAggressivenessLevel ) )	//	[3942561]
	{
		//	THE FOLLOWING 'IOLog' IS REQUIRED AND SHOULD NOT BE MOVED.  POWER MANAGEMENT
		//	VERIFICATION CAN ONLY BE PERFORMED USING THE SYSTEM LOG!  AOA Viewer can be 
		//	used to enable or disable kprintf power management logging messages.
		if ( mDoKPrintfPowerState )
		{
			IOLog ( "AppleOnboardAudio[%ld]::endDetectInterruptService scheduling deferred IDLE\n", mInstanceIndex );
		}
		setIdleAudioSleepTime ( kBatteryPowerDownDelayTime );
		debugIOLog ( 6, "  asyncPowerStateChangeInProgress %d", asyncPowerStateChangeInProgress );
#ifdef THREAD_POWER_MANAGEMENT
		if ( asyncPowerStateChangeInProgress )
		{
			waitForPendingPowerStateChange ();
		}
#endif
		debugIOLog ( 6, "  setting pendingPowerState to kIOAudioDeviceIdle" );
		pendingPowerState = kIOAudioDeviceIdle;
	}
	debugIOLog ( 6, "- AppleOnboardAudio::endDetectInterruptService" );
}


//	--------------------------------------------------------------------------------
//	Returns TRUE if the a recipient of the actionSelector was located and the 
//	actionSelector was broadcast to a recipient.
bool AppleOnboardAudio::broadcastSoftwareInterruptMessage ( UInt32 actionSelector )
{
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
		case kRemoteChildActive:		debugIOLog ( 5, "+ AppleOnboardAudio[%ld]::broadcastSoftwareInterruptMessage ( 0x%0.8X kRemoteChildActive )", mInstanceIndex, actionSelector );			break;
		case kRemoteChildIdle:			debugIOLog ( 5, "+ AppleOnboardAudio[%ld]::broadcastSoftwareInterruptMessage ( 0x%0.8X kRemoteChildIdle )", mInstanceIndex, actionSelector );			break;
		case kRemoteChildSleep:			debugIOLog ( 5, "+ AppleOnboardAudio[%ld]::broadcastSoftwareInterruptMessage ( 0x%0.8X kRemoteChildSleep )", mInstanceIndex, actionSelector );			break;
		default:						debugIOLog ( 5, "+ AppleOnboardAudio[%ld]::broadcastSoftwareInterruptMessage ( 0x%0.8X UNKNOWN )", mInstanceIndex, actionSelector );					break;
	}

	if ( 0 != mAOAInstanceArray ) {
		numInstances = mAOAInstanceArray->getCount();
		for ( UInt32 index = 0; index < numInstances; index++ ) {
			theObject =  mAOAInstanceArray->getObject ( index );
			if ( 0 != theObject ) {
				theAOA = OSDynamicCast ( AppleOnboardAudio, theObject );
				debugIOLog ( 5, "  numInstances = %d, mAOAInstanceArray->getObject ( %d ) returns %p", numInstances, index, theAOA );
				if ( 0 != theAOA ) {
					if ( isTargetForMessage ( actionSelector, theAOA ) ) {
						switch ( actionSelector ) {
							case kRemoteActive:
							case kRemoteIdle:
							case kRemoteSleep:
							case kRemoteChildActive:
							case kRemoteChildIdle:
							case kRemoteChildSleep:
								debugIOLog ( 6, "  %p AOA[%ld] calls %p->interruptEventHandler ( %d, 0 )", this, getAOAInstanceIndex (), theAOA, actionSelector );
								theAOA->interruptEventHandler ( actionSelector, 0 );
								debugIOLog ( 6, "  %p AOA[%ld] returns from %p->interruptEventHandler ( %d, 0 )", this, getAOAInstanceIndex (), theAOA, actionSelector );
								result = TRUE;
								break;
							default:
								debugIOLog ( 5, "  theAOA %p is target for 0x%0.8X", theAOA, actionSelector );
								if (theAOA->mSoftwareInterruptHandler) {
									theAOA->mInterruptProduced[actionSelector]++;
									theAOA->mSoftwareInterruptHandler->interruptOccurred (0, 0, 0);
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
bool AppleOnboardAudio::initHardware ( IOService * provider ) {
	bool			result = FALSE;

    debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::initHardware", mInstanceIndex);

	mSignal = IOSyncer::create (FALSE);
	FailIf (0 == mInitHardwareThread, Exit);
	
	// this version allows unloading of the kext if initHW fails
	if ( kIOReturnSuccess == protectedInitHardware ( provider ) )
	{
		result = TRUE;
	}
	else
	{
		AppleOnboardAudio::sInstanceCount--;
		mInstanceIndex = AppleOnboardAudio::sInstanceCount;
		debugIOLog ( 3, "  AppleOnboardAudio[%ld]::initHardware FAILED !!!! bumping down instance count now to %d", mInstanceIndex, mInstanceIndex );
	}
	
Exit:
    debugIOLog ( 3, "- AppleOnboardAudio[%ld]::initHardware returns %d", mInstanceIndex, result );
	return result;
}

//	--------------------------------------------------------------------------------
void AppleOnboardAudio::initHardwareThread (AppleOnboardAudio * aoa, void * provider) {
	IOCommandGate *						cg;
	IOReturn							result;

	FailIf (0 == aoa, Exit);
	FailIf (TRUE == aoa->mTerminating, Exit);	

	cg = aoa->getCommandGate ();
	if (cg) {
		result = cg->runAction (aoa->initHardwareThreadAction, provider);
	}

Exit:
	return;
}

//	--------------------------------------------------------------------------------
IOReturn AppleOnboardAudio::initHardwareThreadAction (OSObject * owner, void * provider, void * arg2, void * arg3, void * arg4) {
	AppleOnboardAudio *					aoa;
	IOReturn							result;

	result = kIOReturnError;

	aoa = (AppleOnboardAudio *)owner;
	FailIf (0 == aoa, Exit);

	result = aoa->protectedInitHardware ((IOService *)provider);

Exit:
	return result;
}

//	--------------------------------------------------------------------------------
IOReturn AppleOnboardAudio::protectedInitHardware (IOService * provider) {
	OSArray *							layouts;
	OSArray *							hardwareObjectsList;
	OSArray *							multipleDevicesArray;
	OSDictionary *						layoutEntry;
	OSNumber *							layoutIDNumber;
	OSNumber *							ampRecoveryNumber;
	OSNumber *							microsecsToSleepNumber;
	OSNumber *							headphoneState;
	OSString *							transportObjectString;
	OSString *							comboInAssociationString;
	OSString *							comboOutAssociationString;
	OSData *							tmpData;
	UInt32 *							layoutID;
	UInt32								layoutIDInt;
	OSNumber *							transportIndexPtr;				//  [3648867]
	OSNumber *							platformInterfaceSupportPtr;
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
	OSNumber *							usesAOAPowerManagement;			//  [3515371]
	OSNumber *							theInputsBitmap;
	OSNumber *							theOutputsBitmap;
	OSBoolean *							uiMutesAmpsBoolean;
	OSBoolean *							comboInNoIrqProperty;
	OSBoolean *							comboOutNoIrqProperty;
	OSBoolean *							autoSelectInputBoolean;
	OSBoolean *							autoSelectClockBoolean;
	OSBoolean *							muteAmpWhenClockInterrupted;
	OSBoolean *							supressBootChimeLevelControl;   //  [3730863]
	bool								comboInNoIrq;					//	[4073140,4079688]
	bool								comboOutNoIrq;					//	[4073140,4079688]
	UInt32								irqEnableMask;					//	[4073140,4079688]
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

    debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::protectedInitHardware", mInstanceIndex );
		
	debugIOLog ( 3, "  provider's name is: '%s'", provider->getName () );
	
	connectionString = 0;
	
	tmpData = OSDynamicCast (OSData, provider->getProperty ( kLayoutID ) );
	
	workLoop = getWorkLoop();
	FailIf (0 == workLoop, Exit);
		
	//	Prepare to message other AppleOnboardAudio instances...
	mSoftwareInterruptHandler = IOInterruptEventSource::interruptEventSource ( this, softwareInterruptHandler );
	FailIf ( 0 == mSoftwareInterruptHandler, Exit );
	
	workLoop->addEventSource ( mSoftwareInterruptHandler );
	mSoftwareInterruptHandler->enable ();
	FailIf ( 0 == tmpData, Exit );
	
	layoutID = (UInt32 *)tmpData->getBytesNoCopy ();
	FailIf ( 0 == layoutID, Exit )
	
	mCodecLockStatus = kClockUnLockStatus;
	mRelockToExternalClockInProgress = FALSE;	// [4189050]
    mRelockToExternalClockPollCount = 0;		// [4189050]
	
	mLayoutID = *layoutID;
	mUCState.ucLayoutID = mLayoutID;
	debugIOLog ( 3, "  ...:%s:layout-id = 0x%lX = #%d", provider->getName (), mLayoutID, mLayoutID );
	
	mDigitalInsertStatus = kGPIO_Connected;
	
	// Figure out which plugins need to be loaded for this machine.
	// Fix up the registry to get needed plugins to load using ourselves as a nub.
	layouts = OSDynamicCast ( OSArray, getProperty ( kLayouts ) );
	debugIOLog ( 3, "  layout array = %p", layouts );
	FailIf ( 0 == layouts, Exit );

	// First thing to do is to find the array entry that describes the machine that we are on.
	layoutsCount = layouts->getCount ();
	debugIOLog ( 3, "  layouts->getCount () returns %ld", layoutsCount );
	layoutEntry = 0;
	index = 0;
	
	mMatchingIndex = 0xFFFFFFFF;
	layoutIDInt = 0;
	debugIOLog ( 6,  "  AppleOnboardAudio[%ld]::mLayoutID 0x%lX = #%ld (from provider node)", mInstanceIndex, mLayoutID, mLayoutID);
	
	while ( layoutsCount-- )
	{
		layoutEntry = OSDynamicCast (OSDictionary, layouts->getObject (index));
		FailIf (0 == layoutEntry, Exit);
		
		layoutIDNumber = OSDynamicCast (OSNumber, layoutEntry->getObject (kLayoutIDInfoPlist));
		FailIf (0 == layoutIDNumber, Exit);
		
		layoutIDInt = layoutIDNumber->unsigned32BitValue ();
		debugIOLog ( 6, "  found layoutIDInt %d in XML layout entry dictionary", layoutIDInt );

		if ( layoutIDInt == mLayoutID )
		{
			debugIOLog ( 6,  "  AppleOnboardAudio[%ld] found machine layout id 0x%lX @ index %ld", mInstanceIndex, layoutIDInt, index);
			mMatchingIndex = index;
			break;
		}
		else
		{
			index++;
		}
	}

	FailIf (0xFFFFFFFF == mMatchingIndex, Exit);	
	layoutEntry = OSDynamicCast (OSDictionary, layouts->getObject (mMatchingIndex));

	debugIOLog ( 6, "  layoutEntry = %p", layoutEntry);
	FailIf (0 == layoutEntry, Exit);

	mAmpRecoveryMuteDuration = 1;
	ampRecoveryNumber = OSDynamicCast ( OSNumber, layoutEntry->getObject (kAmpRecoveryTime) );
	if ( 0 != ampRecoveryNumber )
	{
		mAmpRecoveryMuteDuration = ampRecoveryNumber->unsigned32BitValue();
	}
	debugIOLog ( 6, "  AppleOnboardAudio[%ld]::protectedInitHardware - mAmpRecoveryMuteDuration = %ld", mInstanceIndex, mAmpRecoveryMuteDuration);
	
	//  [4148027]
	autoSelectInputBoolean = OSDynamicCast ( OSBoolean, layoutEntry->getObject ( kInputAutoSelect ) );
	if ( 0 != autoSelectInputBoolean ) {
		mAutoSelectInput = autoSelectInputBoolean->getValue ();
	}	
	else {
		mAutoSelectInput = FALSE;
	}

	//  [4073140]
	autoSelectClockBoolean = OSDynamicCast ( OSBoolean, layoutEntry->getObject ( kExternalClockAutoSelect ) );
	if ( 0 != autoSelectClockBoolean ) {
		mAutoSelectClock = autoSelectClockBoolean->getValue ();
	}	
	else {
		mAutoSelectClock = FALSE;
	}
	
	mDisableAutoSelectClock = TRUE;  // let digital-in jack detect interrupt set this if necessary
	
	//	[3938771]
	microsecsToSleepNumber = OSDynamicCast ( OSNumber, layoutEntry->getObject ( kMicrosecsToSleep ) );
	if ( 0 != microsecsToSleepNumber )
	{
		mMicrosecsToSleep = microsecsToSleepNumber->unsigned32BitValue();
	}
	debugIOLog ( 6, "  AppleOnboardAudio[%ld]::protectedInitHardware - mMicrosecsToSleep = %ld", mInstanceIndex, mMicrosecsToSleep);

	//	[4073140,4079688]  Need to avoid registration of combo jacks if newer combo jacks where there is no
	//	platform interface support for the combo jack interrupt specified in the ROM / device 
	//	tree as the platform function will not return if dispatched to register the interrupt
	//	when no interrupt services are available.
	comboInNoIrq = FALSE;
	comboInNoIrqProperty = OSDynamicCast ( OSBoolean, layoutEntry->getObject ( kComboInNoIrq ) );
	if ( 0 != comboInNoIrqProperty ) {
		comboInNoIrq = comboInNoIrqProperty->getValue ();
	}	
	
	//	[4073140,4079688]  Need to avoid registration of combo jacks if newer combo jacks where there is no
	//	platform interface support for the combo jack interrupt specified in the ROM / device 
	//	tree as the platform function will not return if dispatched to register the interrupt
	//	when no interrupt services are available.
	comboOutNoIrq = FALSE;
	comboOutNoIrqProperty = OSDynamicCast ( OSBoolean, layoutEntry->getObject ( kComboOutNoIrq ) );
	if ( 0 != comboOutNoIrqProperty ) {
		comboOutNoIrq = comboOutNoIrqProperty->getValue ();
	}	
	
	//	The <key>PlatformInterfaceSupport</key> property is a <integer> value consisting of a bit mapped
	//	array of selector values passed to the PlatformInterface::init method whereby the bit mapped
	//	selectors are used to indicate which derived classes are to be instantiated in support of the
	//	DBDMA, FCR, GPIO, I2C and I2S I/O Modules.  This property value is a REQUIRED value.  If not found
	//	then an XML encoding error exists and a fatal assert is thrown.		3 Nov 2004 - RBM
	platformInterfaceSupportPtr = OSDynamicCast ( OSNumber, layoutEntry->getObject ( kPlatformInterfaceSupport ) );
	FailIf ( 0 == platformInterfaceSupportPtr, Exit );
	mPlatformInterfaceSupport = platformInterfaceSupportPtr->unsigned32BitValue ();

	mPlatformInterface = new PlatformInterface ();
	
	FailIf (0 == mPlatformInterface, Exit);
	debugIOLog ( 6, "  AppleOnboardAudio[%ld]::protectedInitHardware - mPlatformInterface = %p", mInstanceIndex, mPlatformInterface);
	
	// [4073140,4079688]
	irqEnableMask = 0xFFFFFFFF;
	if ( comboInNoIrq )
	{
		irqEnableMask &= ~( 1 << gpioMessage_ComboInJackType_bitAddress );
	}
	if ( comboOutNoIrq )
	{
		irqEnableMask &= ~( 1 << gpioMessage_ComboOutJackType_bitAddress );
	}
	
	FailIf (!mPlatformInterface->init ( provider, this, AppleDBDMAAudio::kDBDMADeviceIndex, mPlatformInterfaceSupport, irqEnableMask ), Exit);
	
	// [4083703] Read the internal microphone selector GPIO for state. This must occur before the input selector is created.
	
	if ( kGPIO_IsAlternate == mPlatformInterface->getInternalMicrophoneID() ) {
		mInternalMicrophoneID = 1;
	} else {
		mInternalMicrophoneID = 0;
	}
	
	//  [3648867]   The driver <XML> dictionary will implement a <key>TransportIndex</key> <integer></integer> value pair 
	//				where values represent:
	//							0 = 'i2s-a'			1 = 'i2s-b'				2 = 'i2s-c'				3 = 'i2s-d'
	//							4 = 'i2s-e'			5 = 'i2s-f'				6 = 'i2s-g'				7 = 'i2s-h'
	//
	//  13 May 2004 rbm
	transportIndexPtr = OSDynamicCast ( OSNumber, layoutEntry->getObject ( kTransportIndex ) );
	if ( 0 != transportIndexPtr ) {
		debugIOLog ( 6, "  transportIndexPtr = %p", transportIndexPtr);
		mTransportInterfaceIndex = transportIndexPtr->unsigned32BitValue ();
	} else {
		debugIOLog ( 3, "  <key>TransportIndex</key> NOT FOUND!!!" );
	}
	debugIOLog ( 6, "  AppleOnboardAudio[%ld]::mTransportInterfaceIndex = %d", mInstanceIndex, mTransportInterfaceIndex);
	
	mUIMutesAmps = FALSE;
	uiMutesAmpsBoolean = OSDynamicCast ( OSBoolean, layoutEntry->getObject ( kUIMutesAmps ) );
	if ( 0 != uiMutesAmpsBoolean ) {
		mUIMutesAmps = uiMutesAmpsBoolean->getValue ();
	}	
	
	mMuteAmpWhenClockInterrupted = FALSE;   
	muteAmpWhenClockInterrupted = OSDynamicCast ( OSBoolean, layoutEntry->getObject ( kMuteAmpWhenClockInterrupted ) );
	if ( 0 != muteAmpWhenClockInterrupted ) {
		mMuteAmpWhenClockInterrupted = muteAmpWhenClockInterrupted->getValue ();
	}
	
	//	[3515371]	begin	{
	//	There may be another instance of AppleOnboardAudio which is the next higher priority in servicing
	//	power management sleep requests.  Any such instance is discovered here and the 'layoutID' of that
	//	instance is saved.  When the first power managment request comes in, an AppleOnboardAudio instance
	//	of higher power management priority will be located and the power managment prioritization relationship
	//	will be established at that time (all AOA instances should have completed loading by then).
	usesAOAPowerManagement = OSDynamicCast ( OSNumber, layoutEntry->getObject ( kUsesAOAPowerManagement ) );			//  [3515371]
	if ( 0 != usesAOAPowerManagement ) {
		mUsesAOAPowerManagement = usesAOAPowerManagement->unsigned32BitValue();
	}

	//	}	end		[3515371]
	
	//  [3730863]   begin {
	supressBootChimeLevelControl = OSDynamicCast ( OSBoolean, layoutEntry->getObject ( kSuppressBootChimeLevelCtrl ) );			//  [3515371]
	if ( 0 != supressBootChimeLevelControl ) {
		mSpressBootChimeLevelControl = supressBootChimeLevelControl->getValue();
	}
	//  } end		[3730863]
	
	//	[3453799]	begin {
	comboInAssociationString = OSDynamicCast ( OSString, layoutEntry->getObject ( kComboInObject ) );
	if ( 0 != comboInAssociationString ) {
		debugIOLog ( 6, "  comboInAssociationString = %s", comboInAssociationString->getCStringNoCopy ());
		if ( comboInAssociationString->isEqualTo ("LineInDetect") ) {
			mPlatformInterface->setAssociateComboInTo ( kGPIO_Selector_LineInDetect );
		} else if ( comboInAssociationString->isEqualTo ("ExternalMicDetect") ) {
			mPlatformInterface->setAssociateComboInTo ( kGPIO_Selector_ExternalMicDetect );
		}
	}
	
	comboOutAssociationString = OSDynamicCast ( OSString, layoutEntry->getObject ( kComboOutObject ) );
	if ( 0 != comboOutAssociationString ) {
		debugIOLog ( 6, "  comboOutAssociationString = %s", comboOutAssociationString->getCStringNoCopy ());
		if ( comboOutAssociationString->isEqualTo ("LineOutDetect") ) {
			mPlatformInterface->setAssociateComboOutTo ( kGPIO_Selector_LineOutDetect );
		} else if ( comboOutAssociationString->isEqualTo ("HeadphonesDetect") ) {
			mPlatformInterface->setAssociateComboOutTo ( kGPIO_Selector_HeadphoneDetect );
		} else if ( comboOutAssociationString->isEqualTo ("ExtSpeakersDetect") ) {
			mPlatformInterface->setAssociateComboOutTo ( kGPIO_Selector_SpeakerDetect );
		}
	}
	//	} end	[3453799]
			
	debugIOLog ( 3, "  AppleOnboardAudio[%ld]::protectedInitHardware - about to mute all amps.", mInstanceIndex );

	if (mMuteAmpWhenClockInterrupted) {
		muteAllAmps();
	}
	
	if ( kGPIO_Unknown != mPlatformInterface->getCodecReset ( kCODEC_RESET_Analog ) )
	{
		mPlatformInterface->setCodecReset ( kCODEC_RESET_Analog, kGPIO_Reset );
	}

	// Find out what the correct transport object is and request the transport factory to build it for us.
	transportObjectString = OSDynamicCast ( OSString, layoutEntry->getObject ( kTransportObject ) );
	FailIf ( 0 == transportObjectString, Exit );
	debugIOLog ( 6, "  AppleOnboardAudio[%ld]::protectedInitHardware - transportObjectString = %s", mInstanceIndex, transportObjectString->getCStringNoCopy());

	mTransportInterface = TransportFactory::createTransport ( transportObjectString );
	debugIOLog ( 6, "  AppleOnboardAudio[%ld]::protectedInitHardware - mTransportInterface = %p", mInstanceIndex, mTransportInterface);
	FailIf (0 == mTransportInterface, Exit);
	FailIf (!mTransportInterface->init ( mPlatformInterface ), Exit);

	// If we have the entry we were looking for then get the list of plugins that need to be loaded
	hardwareObjectsList = OSDynamicCast (OSArray, layoutEntry->getObject (kHardwareObjects));
	if ( 0 == hardwareObjectsList ) { debugIOLog ( 3,  "  0 == hardwareObjectsList" ); }
	FailIf (0 == hardwareObjectsList, Exit);

	// Set the IORegistry entries that will cause the plugins to load
	numPlugins = hardwareObjectsList->getCount ();
	debugIOLog ( 5,  "  AppleOnboardAudio[%ld] numPlugins to load = %ld", mInstanceIndex, numPlugins);

	if (0 == mPluginObjects) {
		mPluginObjects = OSArray::withCapacity (0);
	}
	FailIf (0 == mPluginObjects, Exit);

	for (index = 0; index < numPlugins; index++) {
		setProperty (((OSString *)hardwareObjectsList->getObject(index))->getCStringNoCopy(), "YES");
		debugIOLog ( 5,  "  set property %s", ((OSString *)hardwareObjectsList->getObject(index))->getCStringNoCopy());
	}
	
	registerService ();														// Gets the plugins to load.

	// Wait for the plugins to load so that when we get to initHardware everything will be up and ready to go.
	timeWaited = 0;
	done = FALSE;
	while (!done) {
		if (0 == mPluginObjects) {
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

#if 0		//  begin   {   This section was commented out.  It may have broken the DEBUG build???

	if ((0 == timeout) && (FALSE == done)) {
		debugIOLog ( 3, "$$$$$$ timeout and not enough plugins $$$$$$");
		setProperty ("Plugin load failed", "TRUE");
	}

#if DEBUGLOG
	loadTimeNumber = OSNumber::withNumber ((unsigned long long)timeWaited * 10, 32);
	setProperty ("Plugin load time (ms)", loadTimeNumber);
	loadTimeNumber->release ();
#endif

#endif		//  }   end

	volumeNumber = OSNumber::withNumber((long long unsigned int)0, 32);

	FailIf ( !super::initHardware (provider), Exit);

	debugIOLog ( 3, "  A: about to set work loop");
	
	// must occur in this order, and must be called in initHardware or later to have a valid workloop
	mPlatformInterface->setWorkLoop (workLoop);
	
	FailIf (0 == mPluginObjects, Exit);

	count = mPluginObjects->getCount ();
	FailIf (0 == count, Exit);
	
	debugIOLog ( 3, "  AppleOnboardAudio[%ld] about to init %ld plugins", mInstanceIndex, count);

	for (index = 0; index < count; index++) {
		thePluginObject = getIndexedPluginObject (index);

		FailIf (0 == thePluginObject, Exit);

		thePluginObject->setWorkLoop (workLoop);

		thePluginObject->initPlugin ( mPlatformInterface );

		// XXX FIX - this is a temporary init
		mCurrentOutputPlugin = thePluginObject;
		mCurrentInputPlugin = thePluginObject;
	}

	volumeNumber->release ();

	// FIX - check the result of this call and remove plugin if it fails!
	callPluginsInOrder (kPreDMAEngineInit, (UInt32) mAutoSelectClock);

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

	setDeviceTransportType (kIOAudioDeviceTransportTypeBuiltIn);
	
	setProperty (kIOAudioEngineCoreAudioPlugInKey, "IOAudioFamily.kext/Contents/PlugIns/AOAHALPlugin.bundle");

	configureDMAEngines (provider);
	debugIOLog ( 6, "  AppleOnboardAudio[%ld] finished configure DMA engine (%p) ", mInstanceIndex, mDriverDMAEngine);
	FailIf (0 == mDriverDMAEngine, Exit);

	theInputsBitmap = OSDynamicCast (OSNumber, layoutEntry->getObject (kInputsBitmap));
	if (theInputsBitmap) {
		mDriverDMAEngine->setProperty ("InputsBitmap", theInputsBitmap);
	}
	theOutputsBitmap = OSDynamicCast (OSNumber, layoutEntry->getObject (kOutputsBitmap));
	if (theOutputsBitmap) {
		mDriverDMAEngine->setProperty ("OutputsBitmap", theOutputsBitmap);
	}
	
	// Have to create the audio controls before calling activateAudioEngine
	mAutoUpdatePRAM = FALSE;			// Don't update the PRAM value while we're initing from it
    result = createDefaultControls ();
	FailIf (kIOReturnSuccess != result, Exit);

	debugIOLog ( 3, "  AppleOnboardAudio[%ld]::initHardware - mDriverDMAEngine retain count before activateAudioEngine = %d", mInstanceIndex, mDriverDMAEngine->getRetainCount());
    if (kIOReturnSuccess != activateAudioEngine (mDriverDMAEngine)) {
		mDriverDMAEngine->release ();
		mDriverDMAEngine = 0;
        goto Exit;
    }
	debugIOLog ( 3, "  AppleOnboardAudio[%ld]::initHardware - mDriverDMAEngine retain count after activateAudioEngine = %d", mInstanceIndex, mDriverDMAEngine->getRetainCount());

	pollTimer = IOTimerEventSource::timerEventSource ( this, pollTimerCallback );
	if ( pollTimer ) {
		workLoop->addEventSource ( pollTimer );
	}

	// Set this to a default for desktop machines (portables will get a setAggressiveness call later in the boot sequence).
	mUCState.ucPowerState = getPowerState ();
	setProperty ("IOAudioPowerState", getPowerState (), 32);
	// [3107909] Turn the hardware off because IOAudioFamily defaults to the off state, so make sure the hardware is off or we get out of synch with the family.
	setIdleAudioSleepTime ( kNoIdleAudioPowerDown );

	// Set the default volume to that stored in the PRAM in case we don't get a setValue call from the Sound prefs before being activated.
	mAutoUpdatePRAM = FALSE;			// Don't update the PRAM value while we're initing from it
	if (0 != mOutMasterVolumeControl) {
		UInt32			volume;
		volume = (mOutMasterVolumeControl->getMaxValue () - mOutMasterVolumeControl->getMinValue () + 1) * 90 / 100;
		mOutMasterVolumeControl->setValue (volume);
	}
	if (0 != mOutLeftVolumeControl) {
		UInt32			volume;
		volume = (mOutLeftVolumeControl->getMaxValue () - mOutLeftVolumeControl->getMinValue () + 1) * 65 / 100;
		mOutLeftVolumeControl->setValue (volume);
	}
	if (0 != mOutRightVolumeControl) {
		UInt32			volume;
		volume = (mOutRightVolumeControl->getMaxValue () - mOutRightVolumeControl->getMinValue () + 1) * 65 / 100;
		mOutRightVolumeControl->setValue (volume);
	}
	
	debugIOLog (6, "  Internal microphone ID is %ld", mInternalMicrophoneID);
	if ( kGPIO_IsAlternate == mPlatformInterface->getInternalSpeakerID() ) {
		mInternalSpeakerID = 1;
	} else {
		mInternalSpeakerID = 0;
	}
	debugIOLog ( 6, "  Internal speaker ID is %ld", mInternalSpeakerID);
	
	mSiliconVersion = mCurrentInputPlugin->getSiliconVersion();
	debugIOLog ( 3, "  Silicon version is %ld", mSiliconVersion);
	
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

	if ( 0 != mOutputSelector ) {
		mOutputSelector->hardwareValueChanged (connectionCodeNumber);
	}
	
	connectionCodeNumber->release();
		
	if ( 0 != mOutputSelector ) {
		selectorCode = mOutputSelector->getIntValue ();
		debugIOLog ( 6, "  mOutputSelector->getIntValue () returns %lX", selectorCode);
		if (0 != selectorCode) {
			connectionString = getConnectionKeyFromCharCode (selectorCode, kIOAudioStreamDirectionOutput);
			debugIOLog ( 6, "  mOutputSelector->getIntValue () char code is %s", connectionString);
			if (0 != connectionString) {
				AudioHardwareObjectInterface* theHWObject;
				theHWObject = getPluginObjectForConnection (connectionString);
				if (0 != theHWObject) {
					mCurrentOutputPlugin = theHWObject;
				} else {
					debugIOLog (1, "  *** Can't find hardware plugin for output %s", connectionString);
				}
			}
		}
		debugIOLog ( 6, "  AppleOnboardAudio[%ld] mCurrentOutputPlugin = %p", mInstanceIndex, mCurrentOutputPlugin);
	}
	
	connectionString = 0;
	if ( 0 != mInputSelector ) {
		selectorCode = mInputSelector->getIntValue ();
		debugIOLog ( 6, "  mInputSelector->getIntValue () returns '%4s'", (char *)&selectorCode);
		if (0 != selectorCode) {
			connectionString = getConnectionKeyFromCharCode (selectorCode, kIOAudioStreamDirectionInput);
			debugIOLog ( 6, "  mInputSelector->getIntValue () char code is %s", connectionString);
			if (0 != connectionString) {
				AudioHardwareObjectInterface* theHWObject;
				theHWObject = getPluginObjectForConnection (connectionString);
				if (0 != theHWObject) {
					mCurrentInputPlugin = theHWObject;
				} else {
					debugIOLog (1, "  *** Can't find hardware plugin for input %s", connectionString);
				}
			}
		}
	}
	debugIOLog ( 6, "  AppleOnboardAudio[%ld] mCurrentInputPlugin = %p", mInstanceIndex, mCurrentInputPlugin);
	
	mCurrentInputPlugin->setActiveInput (selectorCode);

	AOAprop = OSDynamicCast (OSDictionary, mCurrentInputPlugin->getProperty (kPluginPListAOAAttributes));
	FailIf (0 == AOAprop, Exit);

	softwareInputGainBoolean = OSDynamicCast (OSBoolean, AOAprop->getObject (kPluginPListSoftwareInputGain));
	if (0 != softwareInputGainBoolean) {
		mDriverDMAEngine->setUseSoftwareInputGain (softwareInputGainBoolean->getValue ());
		mCurrentPluginNeedsSoftwareInputGain = softwareInputGainBoolean->getValue ();
	} else {
		mDriverDMAEngine->setUseSoftwareInputGain (false);
		mCurrentPluginNeedsSoftwareInputGain = false;
	}

	if (0 != connectionString) {
		setSoftwareInputDSP (connectionString);
	}
	
	inputLatency = 0;			// init them to safe default values, a bit high, but safe.
	inputLatencyNumber = OSDynamicCast (OSNumber, AOAprop->getObject (kPluginPListInputLatency));
	if (0 != inputLatencyNumber) {
		inputLatency = inputLatencyNumber->unsigned32BitValue();
	}

// [3277271], find, store and use the largest output latency - this doesn't change anymore since we can't keep track of 
// which hardware is receiving audio in all cases
	mOutputLatency = 0;
	
	count = mPluginObjects->getCount ();
	for (index = 0; index < count; index++) {
		thePluginObject = getIndexedPluginObject (index);

		FailIf (0 == thePluginObject, Exit); 
		
		AOAprop = OSDynamicCast (OSDictionary, thePluginObject->getProperty (kPluginPListAOAAttributes));
		FailIf (0 == AOAprop, Exit);
	
		outputLatencyNumber = OSDynamicCast (OSNumber, AOAprop->getObject (kPluginPListOutputLatency));
		if (0 != outputLatencyNumber) {
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

	// if we are the first instance around and we need to fire up another AOA, find the other i2s node
	// we want and set a property that will force our other AOA personality to load
	multipleDevicesArray = OSDynamicCast (OSArray, layoutEntry->getObject (kMultipleDevices));
	debugIOLog ( 3, "  multipleDevicesArray = %p", multipleDevicesArray);

	if ( 0 != multipleDevicesArray )
	{
		if ( 1 == mInstanceIndex )
		{
			UInt32						deviceIndex;
			UInt32						devicesToLoad;
			mach_timespec_t				timeout;
			
			timeout.tv_sec = 5;
			timeout.tv_nsec = 0;
			devicesToLoad = 0;
			
			devicesToLoad = multipleDevicesArray->getCount();
			debugIOLog ( 6, "  devicesToLoad = %ld", devicesToLoad);
			
			//	The array of MultipleDevices contains dictionary entries as follows:
			//
			//	<dict>
			//		<key>i2sNode</key>
			//		<string>¥¥¥ TRANSPORT IDENTIFIER ¥¥¥</string>
			//		<key>matchingProperty</key>
			//		<string>¥¥¥ LOAD IDENTIFIER ¥¥¥</string>
			//		<key>soundNodePath</key>
			//		<string>¥¥¥ THE PATH ¥¥¥</string>
			//	</dict>
			//	
			//	where:
			//
			//		¥¥¥ TRANSPORT IDENTIFIER ¥¥¥	=	'i2s-a', 'i2s-b', 'i2s-c', etc.
			//		¥¥¥ LOAD IDENTIFIER ¥¥¥			=	'Load-i2s-a', 'Load-i2s-b', 'Load-i2s-c', etc.
			//		¥¥¥ THE PATH ¥¥¥				=	'AppleKeyLargo/sound', 'AppleK2Driver/sound', etc.
			//
			//	Once all services associated with the 'sound' node referenced in the MultipleDevices array
			//	dictionary element have been detected then the service is published which allows the 
			//	AppleOnboardAudio instance associated with the MultipleDevices array dictionary element to
			//	match and load.
			
			for (deviceIndex = 0; deviceIndex < devicesToLoad; deviceIndex++) {
				OSDictionary *			deviceDict;
				OSString *				i2sNodeString;
				OSString *				soundNodePathString;
				OSString *				matchPropertyString;
				IOService *				i2sService;
				IORegistryEntry * 		soundRegEntry;
				IOService * 			sound;
				
				deviceDict = OSDynamicCast (OSDictionary, multipleDevicesArray->getObject(deviceIndex));
				debugIOLog ( 6, "  deviceDict = %p", deviceDict);
				FailIf (0 == deviceDict, Exit);

				i2sNodeString = OSDynamicCast (OSString, deviceDict->getObject(kI2SNode));				//	One of: 'i2s-a', 'i2s-b' or 'i2s-c'
				debugIOLog ( 6, "  i2sNodeString = %p", i2sNodeString);
				FailIf (0 == i2sNodeString, Exit);
				debugIOLog ( 6, "  i2sNodeString = %s", i2sNodeString->getCStringNoCopy());

				soundNodePathString = OSDynamicCast (OSString, deviceDict->getObject(kSoundNodePath));	//	One of:	'AppleK2Driver/sound' or 'AppleKeyLargo/sound'
				debugIOLog ( 6, "  soundNodePathString = %p", soundNodePathString);
				FailIf (0 == soundNodePathString, Exit);
				debugIOLog ( 6, "  soundNodePathString = %s", soundNodePathString->getCStringNoCopy());

				matchPropertyString = OSDynamicCast (OSString, deviceDict->getObject(kMatchProperty));	//	One of:	'Load-i2s-b' or 'Load-i2s-c'
				debugIOLog ( 6, "  matchPropertyString = %p", matchPropertyString);
				FailIf (0 == matchPropertyString, Exit);
				debugIOLog ( 6, "  matchPropertyString = %s", matchPropertyString->getCStringNoCopy());

				i2sService = IOService::waitForService (IOService::nameMatching(i2sNodeString->getCStringNoCopy()), &timeout);
				debugIOLog ( 6, "  i2sService = %p i2sNodeString=%s", deviceDict,i2sNodeString->getCStringNoCopy());
	#ifndef DualKeyLargo_WORKAROUND
				FailIf (0 == i2sService, Exit);	
	#endif
				
				if (0 != i2sService) {
					soundRegEntry = i2sService->childFromPath (soundNodePathString->getCStringNoCopy(), gIOServicePlane);
					debugIOLog ( 6, "  soundRegEntry = %p soundNodePathString=%s", soundRegEntry,soundNodePathString->getCStringNoCopy());
				} else {
					// set this to 0 to fall into the brute force search
					soundRegEntry = 0;
				}

	#ifdef DualKeyLargo_WORKAROUND
				// !!! DualKeyLargo workaround code to get digital in loading
				// if we still do not find the second instance then do a brute force search
				if (soundRegEntry == 0) {
					OSIterator* iter;
					IOService* parent = (IOService*)i2sService->getParentEntry(gIOServicePlane);
					if (parent) {
						debugIOLog ( 6, "  soundRegEntry = %p ", parent);

						iter = parent->getChildIterator(gIOServicePlane);
						if (iter) {
							IORegistryEntry* node;
							while ((node =  OSDynamicCast (IORegistryEntry,iter->getNextObject())) != 0 ) {
								
								tmpData = OSDynamicCast (OSData, node->getProperty (kLayoutID));
								if (tmpData) {
									UInt32* layoutID = (UInt32 *)tmpData->getBytesNoCopy ();
									UInt32 tempLayout = *layoutID;
									debugIOLog ( 3, "  provider tempLayout(%lu) mLayoutID (%lu)", tempLayout, mLayoutID);
								
									if (tempLayout != mLayoutID) {
										soundRegEntry = node;
										soundRegEntry->retain();
										break;
									}
								}
							}
						}
					}
				}
	#endif				
				FailIf (0 == soundRegEntry, Exit);
				
				sound = OSDynamicCast (IOService, soundRegEntry);
				FailIf (0 == sound, Exit);
				debugIOLog ( 6, "  sound = %p", sound);

				sound->setProperty (matchPropertyString->getCStringNoCopy(), "YES");
				sound->registerService ();
			}
		} else {
			debugIOLog ( 3,  "  0 == multipleDevicesArray on instance %ld", mInstanceIndex );
		}	
	}
	
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

	debugIOLog (1, "  about to sleep after postDMAEngineInit"); IOSleep (3000); 

	mCurrentOutputSelection = 0x3F3F3F3F;	// force update of all mutes, etc. when interrupt handlers choose correct output 

	// Has to be after creating the controls so that interruptEventHandler isn't called before the selector controls exist.
	mPlatformInterface->enableAmplifierMuteRelease();								//	[3514762]
	mAllowDetectIrqDispatchesOnWake = TRUE;						//	don't do power management tasks until PMInit has executed

	// <rdar://6144120> Flush the output selector's values here because it creates volume controls that corrupts iterator of
	// the engine's defaultAudioControls list while it is being iterated over in flushAudioControls().
	if (0 != mOutputSelector)
	{
		mOutputSelector->flushValue ();
	}

	if (0 != mInputSelector)
	{
		mInputSelector->flushValue ();
	}

    flushAudioControls ();

	if (0 != mExternalClockSelector)
	{
		mExternalClockSelector->flushValue ();		// Specifically flush the clock selector's values because flushAudioControls() doesn't seem to call it... ???
	}

	// <rdar://6144120> Move the interrupt registration after flushAudioControls() so that the interrupt doesn't fired 
	// while it is iterating the defaultAudioControls list. The defaultAudioControls list could get corrupted if the 
	// interrupt fired as the volume controls are removed for SPDIF.
	// Note: This is a workaround the actual issue where the PlatformInterface::comboDelayTimerCallback() doesn't
	// do runAction to run PlatformInterface::platformRunComboDelayTasks(), which means it can be running at the
	// same time as AppleOnboardAudio::protectedInitHardware(), leading the race condition that corrupts the
	// defaultAudioControl list. We do this workaround to minimize impact of the fix for <rdar://6144120>.
	mPlatformInterface->registerDetectInterrupts ( (IOService*)mPlatformInterface );
	mRemoteDetectInterruptEnabled = TRUE;
	mPlatformInterface->registerNonDetectInterrupts ( (IOService*)mPlatformInterface );
	mRemoteNonDetectInterruptEnabled = TRUE;
	debugIOLog ( 6, "  AOA[%ld] where mRemoteDetectInterruptEnabled = %d, mRemoteNonDetectInterruptEnabled = %d", mInstanceIndex, mRemoteDetectInterruptEnabled, mRemoteNonDetectInterruptEnabled );

Exit:
	if (0 != mInitHardwareThread) {
		thread_call_free (mInitHardwareThread);
	}

    debugIOLog ( 3, "- AppleOnboardAudio[%ld]::protectedInitHardware returns 0x%x", mInstanceIndex, result); 
	return (result);
}

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	Recipient of notifications of publication of AppleOnboardAudio instances.  Only other AppleOnboardAudio
//	instances are added to the mAOAInstanceArray while 'this' AppleOnboardAudio instance is excluded
//	from the mAOAInstanceArray.  When adding an AppleOnboardAudio instance to the mAOAInstanceArray
//	check to see if 'this' instance needs to detach from the provider power tree and attach to the
//	aoaObject power tree.	[3938771]
//
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
bool AppleOnboardAudio::aoaPublished ( AppleOnboardAudio * aoaObject, void * refCon, IOService * newService )
{
	bool		result = FALSE;
	
	debugIOLog ( 3, "+ AppleOnboardAudio::aoaPublished ( %p, %p, %p )", aoaObject, refCon, newService );
	if ( aoaObject )
	{
		if ( aoaObject != newService )
		{
			result = aoaObject->aoaPublishedAction ( refCon, newService );
		}
	}
	debugIOLog ( 3, "- AppleOnboardAudio::aoaPublished ( %p, %p, %p ) returns %d", aoaObject, refCon, newService, result );
	return result;
}

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool AppleOnboardAudio::aoaPublishedAction ( void * refCon, IOService * newService )
{
	bool	result = FALSE;
	bool	found = FALSE;
	
	debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::aoaPublishedAction ( %p, %p )", mInstanceIndex, refCon, newService );
	
	if ( 0 == mAOAInstanceArray )
	{
		debugIOLog ( 3, "  aoaObject->mAOAInstanceArray = OSArray::withObjects ( (const OSObject**)&newService=%p, 1);", newService );
		mAOAInstanceArray = OSArray::withObjects ( (const OSObject**)&newService, 1 );
		FailIf ( 0 == mAOAInstanceArray, Exit );
		result = TRUE;
	}
	else
	{
		for ( UInt32 index = 0; index < mAOAInstanceArray->getCount() && !found; index++ )
		{
			if ( newService == mAOAInstanceArray->getObject ( index ) )
			{
				found = TRUE;
			}
		}
		if ( !found )
		{
			debugIOLog ( 3, "  mAOAInstanceArray->setObject ( %p );", newService );
			mAOAInstanceArray->setObject ( newService );
			result = TRUE;
		}
	}
	
	//	[3938771]	If adding an instance to the mAOAInstanceArray then check to see if 'this' instance
	//				should detach from the provider power tree and attach to the instance being added
	//				to mAOAInstanceArray.
	if ( result )
	{
		debugIOLog ( 6, "  AOA[%ld] where mJoinAOAPMTree %d, mNumberOfAOAPowerParents %ld", mInstanceIndex, mJoinAOAPMTree, mNumberOfAOAPowerParents );
		if ( mJoinAOAPMTree && ( 0 == mNumberOfAOAPowerParents ) )
		{
			static IOPMPowerState aoaPowerStates[2] = {{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},{1, IOPMDeviceUsable, IOPMPowerOn, IOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0}};

			PMinit ();
			debugIOLog ( 6, "  %p newService->about to joinPMtree ( %p )", newService, this );
			newService->joinPMtree ( this );
			
			if ( pm_vars != NULL )
			{
				duringStartup = TRUE;
				debugIOLog ( 6, "  about to registerPowerDriver ( %p, %p, Td)", this, aoaPowerStates, 2 );
				registerPowerDriver ( this, aoaPowerStates, 2 );
				changePowerStateTo ( 1 );
				duringStartup = FALSE;
			}
			mNumberOfAOAPowerParents++;
		}
	}
	
	for ( UInt32 index = 0; index < mAOAInstanceArray->getCount(); index++ )
	{
		debugIOLog ( 3, "  %p = mAOAInstanceArray->getObject ( %d )", mAOAInstanceArray->getObject ( index ) );
	}
	
Exit:
	debugIOLog ( 3, "- AppleOnboardAudio[%ld]::aoaPublishedAction ( %p, %p ) returns %d", mInstanceIndex, refCon, newService, result );
	return result;
}

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleOnboardAudio::configureDMAEngines(IOService *provider) {
    IOReturn 						result;
    bool							hasInput;
    bool							hasOutput;
	OSArray *						formatsArray;
	OSArray *						inputListArray;
	OSArray *						outputListArray;
    
    result = kIOReturnError;

	inputListArray = OSDynamicCast (OSArray, getLayoutEntry (kInputsList, this));
	hasInput = (0 != inputListArray);
	
	outputListArray = OSDynamicCast (OSArray, getLayoutEntry (kOutputsList, this));
	hasOutput = ( 0 != outputListArray );
	
    debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::configureDMAEngines (%p)", mInstanceIndex, provider);

	// All this config should go in a single method
    mDriverDMAEngine = new AppleDBDMAAudio;
    // make sure we get an engine
    FailIf (0 == mDriverDMAEngine, Exit);

	formatsArray = OSDynamicCast (OSArray, getLayoutEntry (kFormats, this));

    if (!mDriverDMAEngine->init (0, mPlatformInterface, (IOService *)provider->getParentEntry (gIODTPlane), hasInput, hasOutput, formatsArray)) {
        mDriverDMAEngine->release ();
		mDriverDMAEngine = 0;
        goto Exit;
    }
   
	result = kIOReturnSuccess;

Exit:
    debugIOLog ( 3, "- AppleOnboardAudio[%ld]::configureDMAEngines (%p) returns %x", mInstanceIndex, provider, result);
    return result;
}

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
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

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
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
	} else if (string->isEqualTo (kDigitalOut)) {
		charCode = kIOAudioOutputPortSubTypeSPDIF;
	} else if (string->isEqualTo (kInternalMic)) {
		charCode = kIOAudioInputPortSubTypeInternalMicrophone;
	} else if (string->isEqualTo (kExternalMic)) {
		charCode = kIOAudioInputPortSubTypeExternalMicrophone;
	} else if (string->isEqualTo (kLineIn)) {
		charCode = kIOAudioInputPortSubTypeLine;
	} else if (string->isEqualTo (kDigitalIn)) {
		charCode = kIOAudioInputPortSubTypeSPDIF;
	} else {
		charCode = 0x3F3F3F3F; 			// because '????' is a trigraph....
	}

	return charCode;
}

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
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

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
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
			theString = 0;
			break;
	}

	return theString;
}

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
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

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleOnboardAudio::createInputSelectorControl (void) {
	OSString *							inputString;
	OSArray *							inputsList;
	IOReturn							result;
	UInt32								inputsCount;
	UInt32								inputSelection;
	UInt32								index;
	UInt32								temp;
	
	debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::createInputSelectorControl()", mInstanceIndex );
	
    mInternalMicrophoneInputString = NULL;
    mExternalMicrophoneInputString = NULL;
    mLineInputString = NULL;
    mDigitalInputString = NULL;
	
	result = kIOReturnError;
	inputsList = OSDynamicCast (OSArray, getLayoutEntry (kInputsList, this));
	
	if ( 0 != inputsList ) {
		inputsCount = inputsList->getCount ();
		inputString = OSDynamicCast (OSString, inputsList->getObject (0));
		FailIf (0 == inputString, Exit);
		
		inputSelection = getCharCodeForString (inputString);
		mInputSelector = IOAudioSelectorControl::createInputSelector (inputSelection, kIOAudioControlChannelIDAll);
		if ( 0 == mInputSelector ) { debugIOLog ( 3, "createInputSelector for '%4s' FAILED", (char*)&inputSelection ); }
		FailIf (0 == mInputSelector, Exit);
		
		mDriverDMAEngine->addDefaultAudioControl (mInputSelector);
		mInputSelector->setValueChangeHandler ((IOAudioControl::IntValueChangeHandler)inputControlChangeHandler, this);
		
		for (index = 0; index < inputsCount; index++) {
			inputString = OSDynamicCast (OSString, inputsList->getObject (index));
			FailIf (0 == inputString, Exit);
			inputSelection = getCharCodeForString (inputString);
			
			switch (inputSelection) {
				case kIOAudioInputPortSubTypeInternalMicrophone:
					mInternalMicrophoneInputString = inputString;
					temp = kIOAudioInputPortSubTypeInternalMicrophone;
					debugIOLog ( 3, "  add input selection of '%4s'", &temp);
					break;
				case kIOAudioInputPortSubTypeExternalMicrophone:
					mExternalMicrophoneInputString = inputString;
					temp = kIOAudioInputPortSubTypeExternalMicrophone;
					debugIOLog ( 3, "  add input selection of '%4s'", &temp);
					break;
				case kIOAudioInputPortSubTypeLine:
					mLineInputString = inputString;
					temp = kIOAudioInputPortSubTypeLine;
					debugIOLog ( 3, "  add input selection of '%4s'", &temp);
					break;
				case kIOAudioInputPortSubTypeSPDIF:
					mDigitalInputString = inputString;
					temp = kIOAudioInputPortSubTypeSPDIF;
					debugIOLog ( 3, "  add input selection of '%4s'", &temp);
					break;
				default:
					debugIOLog (2, "  AppleOnboardAudio[%ld]::createInputSelectorControl: unknown input selection", mInstanceIndex);
			}
			
			debugIOLog ( 3,  "  mInputSelector->addAvailableSelection ( '%4s', %p )", (char*)&inputSelection, inputString );
			mInputSelector->addAvailableSelection (inputSelection, inputString);
		}
		
		// [4148027], [4208860]
		//	If auto input select is enabled, remove all input selections except for internal microphone and
		//	line-in.  If something else is actually plugged-in, protectedInterruptEventHandler will fire and 
		//	take care of it.
		if (mAutoSelectInput) {
			if (TRUE == mInputSelector->valueExists(kIOAudioInputPortSubTypeSPDIF)) {
				mInputSelector->removeAvailableSelection(kIOAudioInputPortSubTypeSPDIF);
			}
		}
	}
	
	debugIOLog ( 3, "  AppleOnboardAudio[%ld]::createInputSelectorControl - mInputSelector = %p cur selector=%lx", mInstanceIndex, mInputSelector, mInputSelector->getIntValue());
	
	result = kIOReturnSuccess;
	
Exit:
	debugIOLog ( 3, "- AppleOnboardAudio[%ld]::createInputSelectorControl() returns %lX", mInstanceIndex, result );
	
	return result;
}

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleOnboardAudio::createOutputSelectorControl (void) {
	char								outputSelectionCString[5];
	OSDictionary *						theDictionary;
	OSNumber *							terminalTypeNum;
	OSString *							outputString;
	OSString *							outputSelectionString;
	OSArray *							outputsList;
	IOReturn							result;
	UInt32								outputsCount;
	UInt32								outputSelection;
	UInt32								index;
	UInt16								terminalType;
	UInt32 temp;

	result = kIOReturnError;
	outputsList = OSDynamicCast (OSArray, getLayoutEntry (kOutputsList, this));
	if ( 0 != outputsList ) {
		outputsCount = outputsList->getCount ();
		outputString = OSDynamicCast (OSString, outputsList->getObject (0));
		FailIf (0 == outputString, Exit);
	
		theDictionary = OSDictionary::withCapacity (outputsCount);
		FailIf (0 == theDictionary, Exit);
	
		outputSelection = getCharCodeForString (outputString);
		mOutputSelector = IOAudioSelectorControl::createOutputSelector (outputSelection, kIOAudioControlChannelIDAll);
		if ( 0 == mOutputSelector ) { debugIOLog ( 3, "createOutputSelector for '%4s' FAILED", (char*)&outputSelection ); }
		FailIf (0 == mOutputSelector, Exit);
		
		mDriverDMAEngine->addDefaultAudioControl (mOutputSelector);
		mOutputSelector->setValueChangeHandler ((IOAudioControl::IntValueChangeHandler)outputControlChangeHandler, this);
		for (index = 0; index < outputsCount; index++) {
			outputString = OSDynamicCast (OSString, outputsList->getObject (index));
			FailIf (0 == outputString, Exit);
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
					debugIOLog ( 3, "  add output selection of '%4s'", &temp);
					break;
				case kIOAudioOutputPortSubTypeExternalSpeaker:
					mExternalSpeakerOutputString = outputString;
					temp = kIOAudioOutputPortSubTypeExternalSpeaker;
					debugIOLog ( 3, "  add output selection of '%4s'", &temp);
					break;
				case kIOAudioOutputPortSubTypeLine:
					mLineOutputString = outputString;
					temp = kIOAudioOutputPortSubTypeLine;
					debugIOLog ( 3, "  add output selection of '%4s'", &temp);
					break;
				case kIOAudioOutputPortSubTypeSPDIF:
					mHasSPDIFControl = true;	// [3639956] Remember we have a selection of SPDIF, we need to know that after wakeup
					mDigitalOutputString = outputString;
					temp = kIOAudioOutputPortSubTypeSPDIF;
					debugIOLog ( 3, "  add output selection of '%4s'", &temp);
					break;
				case kIOAudioOutputPortSubTypeHeadphones:
					mHeadphoneOutputString = outputString;
					temp = kIOAudioOutputPortSubTypeHeadphones;
					debugIOLog ( 3, "  add output selection of '%4s'", &temp);
					break;
				default:
					debugIOLog (2, "  AppleOnboardAudio[%ld]::createOutputSelectorControl: unknown output selection", mInstanceIndex);
			}
			//	}	end		[3413551]
			terminalType = getTerminalTypeForCharCode (outputSelection);
			terminalTypeNum = OSNumber::withNumber (terminalType, 16);
			FailIf (0 == terminalTypeNum, Exit);
			*(UInt32 *)outputSelectionCString = outputSelection;
			outputSelectionCString[4] = 0;
			outputSelectionString = OSString::withCString (outputSelectionCString);
			FailIf (0 == outputSelectionString, Exit);
			theDictionary->setObject (outputSelectionString, terminalTypeNum);
			terminalTypeNum->release ();
			outputSelectionString->release ();
			debugIOLog ( 3,  "  mOutputSelector->addAvailableSelection ( '%4s', %p )", (char*)&outputSelection, outputString );
			mOutputSelector->addAvailableSelection (outputSelection, outputString);
		}
		//	[3413551]	If both an internal and external speaker selector were added then these selectors must be
		//	removed here.  The protectedInterruptEventHandler() will dynamically add these selectors as appropriate
		//	for the jack state.  Removal of these selectors will prevent adding redundant selectors as there is 
		//	no test to determine if the selector has already been added in order to avoid redundantly adding a selector.
		if ( ( 0 != mInternalSpeakerOutputString ) && ( 0 != mExternalSpeakerOutputString ) ) {
			if ( kGPIO_Connected == mPlatformInterface->getSpeakerConnected() ) {
				mOutputSelector->removeAvailableSelection ( kIOAudioOutputPortSubTypeInternalSpeaker );
				debugIOLog ( 6, "  removed internal speaker from output selector ****");
			} else {
				mOutputSelector->removeAvailableSelection ( kIOAudioOutputPortSubTypeExternalSpeaker );
				debugIOLog ( 6, "  removed External Speaker from output selector");
			}
		}
		if ( (kGPIO_Unknown != mPlatformInterface->getComboOutJackTypeConnected ()) ) {
			if ( kGPIO_Connected == mPlatformInterface->getLineOutConnected () ) {
				mOutputSelector->removeAvailableSelection ( kIOAudioOutputPortSubTypeSPDIF );
				debugIOLog ( 6, "  removed SPDIF from output selector");
			} else if ( kGPIO_Connected == mPlatformInterface->getDigitalOutConnected () ) {
				mOutputSelector->removeAvailableSelection ( kIOAudioOutputPortSubTypeLine );
				debugIOLog ( 6, "  removed Line from output selector");
			} else {
				mOutputSelector->removeAvailableSelection ( kIOAudioOutputPortSubTypeSPDIF );
				debugIOLog ( 6, "  removed SPDIF from output selector");
				mOutputSelector->removeAvailableSelection ( kIOAudioOutputPortSubTypeLine );
				debugIOLog ( 6, "  removed Line from output selector");
			}
		}
	
		mDriverDMAEngine->setProperty ("MappingDictionary", theDictionary);
		debugIOLog ( 3, "  AppleOnboardAudio[%ld]::createOutputSelectorControl - mOutputSelector = %p cur selector=%lx", mInstanceIndex, mOutputSelector,mOutputSelector->getIntValue());
	}
	

	result = kIOReturnSuccess;

Exit:
	return result;
}

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AudioHardwareObjectInterface * AppleOnboardAudio::getPluginObjectForConnection (const char * entry) {
	AudioHardwareObjectInterface *		thePluginObject;
	OSDictionary *						dictEntry;
	OSString *							pluginIDMatch;

	if ( 0 != entry ) {
		debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::getPluginObjectForConnection ( %p->'%s' )", mInstanceIndex, entry, entry );
	} else {
		debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::getPluginObjectForConnection ( %p )", mInstanceIndex, entry );
	}

	thePluginObject = 0;
	pluginIDMatch = 0;
	dictEntry = 0;
	
	dictEntry = OSDynamicCast (OSDictionary, getLayoutEntry (entry, this));
	FailIf (0 == dictEntry, Exit);

	pluginIDMatch = OSDynamicCast (OSString, dictEntry->getObject (kPluginID));
	FailIf (0 == pluginIDMatch, Exit);

	thePluginObject = getPluginObjectWithName (pluginIDMatch);
	
	debugIOLog ( 3, "  pluginID = %s", pluginIDMatch->getCStringNoCopy());

Exit:
	debugIOLog ( 3, "- AppleOnboardAudio[%ld]::getPluginObjectForConnection ( %p ) returns %p", mInstanceIndex, entry, thePluginObject );
	return thePluginObject;
}

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
GpioAttributes AppleOnboardAudio::getInputDataMuxForConnection (const char * entry)
{
	OSDictionary *						dictEntry;
	OSNumber *							inputDataMuxOSNumber;
	GpioAttributes						result;

	dictEntry = 0;
	result = kGPIO_Unknown;
	
	dictEntry = OSDynamicCast (OSDictionary, getLayoutEntry (entry, this));
	FailIf (0 == dictEntry, Exit);

	inputDataMuxOSNumber = OSDynamicCast ( OSNumber, dictEntry->getObject ( kInputDataMux ) );
	if ( 0 != inputDataMuxOSNumber )
	{
		if ( 0 == inputDataMuxOSNumber->unsigned32BitValue() )
		{
			result = kGPIO_MuxSelectDefault;
		}
		else
		{
			result = kGPIO_MuxSelectAlternate;
		}
	}
	
Exit:
	debugIOLog ( 3, "± AppleOnboardAudio[%ld]::getInputDataMuxForConnection returns GpioAttributes = %d", mInstanceIndex, result);
	return result;
}

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
AudioHardwareObjectInterface * AppleOnboardAudio::getPluginObjectWithName (OSString * inName) {
	AudioHardwareObjectInterface *		thePluginObject;
    OSDictionary *						AOAprop;
	OSString *							thePluginID;
	UInt32								index;
	UInt32								count;
	Boolean								found;

	thePluginObject = 0;

	FailIf (NULL == mPluginObjects, Exit); // [4011988]

	count = mPluginObjects->getCount ();
	found = FALSE;
	index = 0;
	while (!found && index < count) {
		thePluginObject = getIndexedPluginObject (index);
		FailIf (0 == thePluginObject, Exit);
		AOAprop = OSDynamicCast (OSDictionary, thePluginObject->getProperty (kPluginPListAOAAttributes));
		FailIf (0 == AOAprop, Exit);
		thePluginID = OSDynamicCast (OSString, AOAprop->getObject (kPluginID));
		FailIf (0 == thePluginID, Exit);

		if (thePluginID->isEqualTo (inName)) {
			debugIOLog ( 7, "± AppleOnboardAudio[%ld]::getPluginObjectWithName ( %p ) found matching plugin with ID '%s'", mInstanceIndex, inName, thePluginID->getCStringNoCopy());
			found = TRUE;
		}
		index++;
	}
	
Exit:	
	return thePluginObject;
}

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
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
		FailIf (0 == thePluginObject, Exit);

		debugIOLog ( 3, "  creating input gain controls for input %s", selectedInput);
	
		mindBGain = thePluginObject->getMinimumdBGain ();
		maxdBGain = thePluginObject->getMaximumdBGain ();
		minGain = thePluginObject->getMinimumGain ();
		maxGain = thePluginObject->getMaximumGain ();
		defaultInputGain = thePluginObject->getDefaultInputGain ();
	}
	if ( kStereoInputGainControls == mUseInputGainControls ) {
		debugIOLog ( 3, "  mInLeftGainControl = IOAudioLevelControl::createVolumeControl( %ld, %ld, %ld, %lX, %lX, %ld, %p, 0, %lX )", defaultInputGain, minGain, maxGain, mindBGain, maxdBGain, kIOAudioControlChannelIDDefaultLeft, kIOAudioControlChannelNameLeft, 0, kIOAudioControlUsageInput);
		mInLeftGainControl = IOAudioLevelControl::createVolumeControl (defaultInputGain, minGain, maxGain, mindBGain, maxdBGain, kIOAudioControlChannelIDDefaultLeft, kIOAudioControlChannelNameLeft, 0, kIOAudioControlUsageInput);
		if (0 != mInLeftGainControl) {
			mDriverDMAEngine->addDefaultAudioControl (mInLeftGainControl);
			mInLeftGainControl->setValueChangeHandler ((IOAudioControl::IntValueChangeHandler)inputControlChangeHandler, this);
			// Don't release it because we might reference it later
		}
	
		debugIOLog ( 3, "  mInRightGainControl = IOAudioLevelControl::createVolumeControl( %ld, %ld, %ld, %lX, %lX, %ld, %p, 0, %lX )", defaultInputGain, minGain, maxGain, mindBGain, maxdBGain, kIOAudioControlChannelIDDefaultRight, kIOAudioControlChannelNameRight, 0, kIOAudioControlUsageInput);
		mInRightGainControl = IOAudioLevelControl::createVolumeControl (defaultInputGain, minGain, maxGain, mindBGain, maxdBGain, kIOAudioControlChannelIDDefaultRight, kIOAudioControlChannelNameRight, 0, kIOAudioControlUsageInput);
		if (0 != mInRightGainControl) {
			mDriverDMAEngine->addDefaultAudioControl (mInRightGainControl);
			mInRightGainControl->setValueChangeHandler ((IOAudioControl::IntValueChangeHandler)inputControlChangeHandler, this);
			// Don't release it because we might reference it later
		}
	} else if ( kMonoInputGainControl == mUseInputGainControls ){
		debugIOLog ( 3, "  mInMasterGainControl = IOAudioLevelControl::createVolumeControl( %ld, %ld, %ld, %lX, %lX, %ld, %p, 0, %lX )", defaultInputGain, minGain, maxGain, mindBGain, maxdBGain, kIOAudioControlChannelIDAll, kIOAudioControlChannelNameAll, 0, kIOAudioControlUsageInput);
		mInMasterGainControl = IOAudioLevelControl::createVolumeControl (defaultInputGain, minGain, maxGain, mindBGain, maxdBGain, kIOAudioControlChannelIDAll, kIOAudioControlChannelNameAll, 0, kIOAudioControlUsageInput);
		if (0 != mInMasterGainControl) {
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

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
OSArray * AppleOnboardAudio::getControlsArray (const char * inSelectedOutput) {
	OSArray *							theArray;
	OSDictionary *						theOutput;
	
	theArray = 0;
	
	theOutput = OSDynamicCast(OSDictionary, getLayoutEntry(inSelectedOutput, this));
	FailIf (0 == theOutput, Exit);
	theArray = OSDynamicCast(OSArray, theOutput->getObject(kControls));
	
Exit:
	return theArray;
}

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt32 AppleOnboardAudio::getNumHardwareEQBandsForCurrentOutput () {
	OSDictionary *						AOApropOutput;
	OSNumber *							numBandsNumber;
	UInt32								numBands;
	
	numBands = 0;
	AOApropOutput = OSDynamicCast (OSDictionary, mCurrentOutputPlugin->getProperty (kPluginPListAOAAttributes));
	if (0 != AOApropOutput) {
		numBandsNumber = OSDynamicCast (OSNumber, AOApropOutput->getObject (kPluginPListNumHardwareEQBands));
		if (0 != numBandsNumber) {
			numBands = numBandsNumber->unsigned32BitValue();
		}
	}
	return numBands;
}

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt32 AppleOnboardAudio::getMaxVolumeOffsetForOutput (const UInt32 inCode) {
	char *			connectionString;
	
	connectionString = getConnectionKeyFromCharCode (inCode, kIOAudioStreamDirectionOutput);
	return getMaxVolumeOffsetForOutput (connectionString);
}

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
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
	FailIf (0 == theOutput, Exit);

	theSignalProcessingDict = OSDynamicCast (OSDictionary, theOutput->getObject (kSignalProcessing));
	if ( 0 != theSignalProcessingDict ) {
		sprintf (speakerIDCString, "%s_%ld", kSpeakerID, mInternalSpeakerID); 
		speakerIDString = OSString::withCString (speakerIDCString);
		FailIf (0 == speakerIDString, Exit);
		
		theSpeakerIDDict = OSDynamicCast (OSDictionary, theSignalProcessingDict->getObject (speakerIDString));
		speakerIDString->release ();
		FailIf (0 == theSpeakerIDDict, Exit);
	
		theMaxVolumeNumber = OSDynamicCast (OSNumber, theSpeakerIDDict->getObject (kMaxVolumeOffset));
		if (0 != theMaxVolumeNumber) {
			maxVolumeOffset = theMaxVolumeNumber->unsigned32BitValue ();
		} 
	}

Exit:
	return maxVolumeOffset;
}

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AppleOnboardAudio::setSoftwareOutputDSP (const char * inSelectedOutput) {
	OSDictionary *						theSpeakerIDDict;
	OSDictionary *						theSignalProcessingDict;
	OSDictionary *						theOutput;
	OSDictionary *						theSoftwareDSPDict;
	OSString *							speakerIDString;
	char								speakerIDCString[32];
	
	if ( 0 == inSelectedOutput ) {
		debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::setSoftwareOutputDSP (%p).", mInstanceIndex, inSelectedOutput);
	} else {
		debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::setSoftwareOutputDSP ('%s').", mInstanceIndex, inSelectedOutput);
	}
	FailIf ( 0 == inSelectedOutput, Exit );
	
	// check if we already have calculated coefficients for this output
	// this will NOT work for more than one output having processing on it
	if (mCurrentProcessingOutputString->isEqualTo (inSelectedOutput)) {
		
		debugIOLog ( 3, "  Enabling DSP");
	
		mDriverDMAEngine->enableOutputProcessing ();
		
		debugIOLog ( 3, "  mCurrentProcessingOutputString is '%s', coefficients not updated.", mCurrentProcessingOutputString->getCStringNoCopy ());
	} else {

		// commmon case is disabled, this is the safer fail scenario
		mDriverDMAEngine->disableOutputProcessing ();
		
		debugIOLog ( 3, "  processing disabled.");
	
		theOutput = OSDynamicCast(OSDictionary, getLayoutEntry (inSelectedOutput, this));
		FailIf (0 == theOutput, Exit);
	
		theSignalProcessingDict = OSDynamicCast (OSDictionary, theOutput->getObject (kSignalProcessing));
		if ( 0 != theSignalProcessingDict ) {
	
			sprintf (speakerIDCString, "%s_%ld", kSpeakerID, mInternalSpeakerID); 
			debugIOLog ( 3, "  setSoftwareOutputDSP: speakerIDString = '%s'", speakerIDCString);
			speakerIDString = OSString::withCString (speakerIDCString);
			FailIf (0 == speakerIDString, Exit);
			
			theSpeakerIDDict = OSDynamicCast (OSDictionary, theSignalProcessingDict->getObject (speakerIDString));
			speakerIDString->release ();
			FailIf (0 == theSpeakerIDDict, Exit);
			debugIOLog ( 3, "  setSoftwareOutputDSP: theSpeakerIDDict = %p", theSpeakerIDDict);
		
			theSoftwareDSPDict = OSDynamicCast (OSDictionary, theSpeakerIDDict->getObject (kSoftwareDSP));
			if (0 != theSoftwareDSPDict) {
				debugIOLog ( 3, "  setSoftwareOutputDSP: theSoftwareDSPDict = %p", theSoftwareDSPDict);
		
				mDriverDMAEngine->setOutputSignalProcessing (theSoftwareDSPDict);
				
				debugIOLog ( 3, "  Processing set");
				
				// if we get here, we've found some DSP to perform for this output, so update the currently prepared output string
				debugIOLog ( 3, "  mCurrentProcessingOutputString is '%s'", mCurrentProcessingOutputString->getCStringNoCopy ());
				mCurrentProcessingOutputString->initWithCString (inSelectedOutput);
				debugIOLog ( 3, "  mCurrentProcessingOutputString set to '%s', coefficients will be updated.", mCurrentProcessingOutputString->getCStringNoCopy ());
			}
		}
	}	

Exit:
	if ( 0 == inSelectedOutput ) {
		debugIOLog ( 3, "- AppleOnboardAudio[%ld]::setSoftwareOutputDSP (%p).", mInstanceIndex, inSelectedOutput);
	} else {
		debugIOLog ( 3, "- AppleOnboardAudio[%ld]::setSoftwareOutputDSP ('%s').", mInstanceIndex, inSelectedOutput);
	}
	return;
}

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// [3306305]
void AppleOnboardAudio::setSoftwareInputDSP (const char * inSelectedInput) {
	OSDictionary *						theSignalProcessingDict;
	OSDictionary *						theMicrophoneIDDict;
	OSDictionary *                      theSiliconVersionDict;
	OSDictionary *						theSoftwareDSPDict;
	OSDictionary *						theInput;
	OSString *							microphoneIDString;
	char								microphoneIDCString[32];
	OSString *                          siliconVersionString;
	char                                siliconVersionCString[32];

	if ( 0 == inSelectedInput ) {
		debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::setSoftwareInputDSP ( %p )", mInstanceIndex, inSelectedInput );
	} else {
		debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::setSoftwareInputDSP ( %p->'%s' )", mInstanceIndex, inSelectedInput, inSelectedInput );
	}
	
	FailIf ( 0 == inSelectedInput, Exit );
	
	// check if we already have calculated coefficients for this input
	// this will NOT work for more than one input having processing on it
	if (mCurrentProcessingInputString->isEqualTo (inSelectedInput)) {
		
		debugIOLog ( 3, "  Enabling input DSP");

		mDriverDMAEngine->enableInputProcessing ();

		debugIOLog ( 3, "  mCurrentProcessingInputString is '%s', coefficients not updated.", mCurrentProcessingInputString->getCStringNoCopy ());

	} else {

		debugIOLog ( 3, "  Disabling input DSP");

		// commmon case is disabled, this is the safer fail scenario
		mDriverDMAEngine->disableInputProcessing ();
		debugIOLog ( 3, "  input processing disabled.");
	
		theInput = OSDynamicCast(OSDictionary, getLayoutEntry (inSelectedInput, this));
		FailIf (0 == theInput, Exit);
	
		theSignalProcessingDict = OSDynamicCast (OSDictionary, theInput->getObject (kSignalProcessing));
		if (0 != theSignalProcessingDict) {
	
			sprintf (microphoneIDCString, "%s_%ld", kMicrophoneID, mInternalMicrophoneID); 
			debugIOLog ( 3, "  setSoftwareInputDSP: inputDeviceIDString = %s", microphoneIDCString);
			microphoneIDString = OSString::withCString (microphoneIDCString);
			FailIf (0 == microphoneIDString, Exit);
			
			theMicrophoneIDDict = OSDynamicCast (OSDictionary, theSignalProcessingDict->getObject (microphoneIDString));
			microphoneIDString->release ();
			FailIf (0 == theMicrophoneIDDict, Exit);
			debugIOLog ( 3, "  setSoftwareInputDSP: theMicrophoneIDDict = %p", theMicrophoneIDDict);
			
			sprintf (siliconVersionCString, "%s_%ld", kSiliconVersion, mSiliconVersion);
			debugIOLog ( 3, "  setSoftwareInputDSP: siliconVersionString = %s", siliconVersionCString);
			siliconVersionString = OSString::withCString (siliconVersionCString);
			FailIf (0 == siliconVersionString, Exit);
			
			theSiliconVersionDict = OSDynamicCast (OSDictionary, theMicrophoneIDDict->getObject (siliconVersionString));
			siliconVersionString->release ();
			FailIf (0 == theSiliconVersionDict, Exit);
			debugIOLog ( 3, "  setSoftwareInputDSP: theSiliconVersionDict = %p", theSiliconVersionDict);
			
			theSoftwareDSPDict = OSDynamicCast (OSDictionary, theSiliconVersionDict->getObject (kSoftwareDSP));
			FailIf (0 == theSoftwareDSPDict, Exit);
			debugIOLog ( 3, "  setSoftwareInputDSP: theSoftwareDSPDict = %p", theSoftwareDSPDict);
			
			mDriverDMAEngine->setInputSignalProcessing (theSoftwareDSPDict);
			
			debugIOLog ( 3, "  Input processing set");
			
			// if we get here, we've found some DSP to perform for this output, so update the currently prepared output string
			debugIOLog ( 3, "  mCurrentProcessingInputString is '%s'", mCurrentProcessingInputString->getCStringNoCopy ());
			mCurrentProcessingInputString->initWithCString (inSelectedInput);
			debugIOLog ( 3, "  mCurrentProcessingInputString set to '%s', coefficients will be updated.", mCurrentProcessingInputString->getCStringNoCopy ());
		}
	}	

Exit:
	if ( 0 == inSelectedInput ) {
		debugIOLog ( 3, "- AppleOnboardAudio[%ld]::setSoftwareInputDSP ( %p )", mInstanceIndex, inSelectedInput );
	} else {
		debugIOLog ( 3, "- AppleOnboardAudio[%ld]::setSoftwareInputDSP ( %p->'%s' )", mInstanceIndex, inSelectedInput, inSelectedInput );
	}
	return;
}

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt32 AppleOnboardAudio::setClipRoutineForOutput (const char * inSelectedOutput) {
	OSDictionary *						theOutput;
	OSString *							clipRoutineString;
	OSArray *							theArray;
	IOReturn							result = kIOReturnError;
	UInt32								arrayCount;
	UInt32								index;
	
	if ( 0 == inSelectedOutput ) {
		debugIOLog ( 3,  "+ AppleOnboardAudio[%ld]::setClipRoutineForOutput ( %p )", mInstanceIndex, inSelectedOutput);
	} else {
		debugIOLog ( 3,  "+ AppleOnboardAudio[%ld]::setClipRoutineForOutput ( %p->'%4s' )", mInstanceIndex, inSelectedOutput, inSelectedOutput );
	}
	FailIf ( 0 == inSelectedOutput, Exit );
	
	theArray = 0;
	
	theOutput = OSDynamicCast(OSDictionary, getLayoutEntry(inSelectedOutput, this));
	FailIf (0 == theOutput, Exit);
	
	theArray = OSDynamicCast(OSArray, theOutput->getObject(kClipRoutines));
	FailIf (0 == theArray, Exit);
	
	arrayCount = theArray->getCount();

	mDriverDMAEngine->resetOutputClipOptions();
	
	for (index = 0; index < arrayCount; index++) {
		clipRoutineString = OSDynamicCast(OSString, theArray->getObject(index));
		debugIOLog ( 3, "  clip routine[%ld] = %s", index, clipRoutineString->getCStringNoCopy());

		if (clipRoutineString->isEqualTo (kStereoToRightChanClipString)) {
			mDriverDMAEngine->setRightChanMixed (true);
		} else {
			mDriverDMAEngine->resetOutputClipOptions();
		}
	}	

	result = kIOReturnSuccess;
Exit:
	if ( 0 == inSelectedOutput ) {
		debugIOLog ( 3,  "+ AppleOnboardAudio[%ld]::setClipRoutineForOutput ( %p ) returns 0x%lX", mInstanceIndex, inSelectedOutput, result);
	} else {
		debugIOLog ( 3,  "+ AppleOnboardAudio[%ld]::setClipRoutineForOutput ( %p->'%4s' ) returns 0x%lX", mInstanceIndex, inSelectedOutput, inSelectedOutput, result );
	}
	return result;
}

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt32 AppleOnboardAudio::setClipRoutineForInput (const char * inSelectedInput) {
	OSDictionary *						theInput;
	OSString *							clipRoutineString;
	OSArray *							theArray;
	IOReturn							result = kIOReturnError;
	UInt32								arrayCount;
	UInt32								index;
	
	if ( 0 == inSelectedInput ) {
		debugIOLog ( 3,  "+ AppleOnboardAudio[%ld]::setClipRoutineForInput ( %p )", mInstanceIndex, inSelectedInput);
	} else {
		debugIOLog ( 3,  "+ AppleOnboardAudio[%ld]::setClipRoutineForInput ( %p->'%4s' )", mInstanceIndex, inSelectedInput, inSelectedInput );
	}
	FailIf ( 0 == inSelectedInput, Exit );
	
	theArray = 0;
	
	theInput = OSDynamicCast(OSDictionary, getLayoutEntry(inSelectedInput, this));
	FailIf (0 == theInput, Exit);
	
	theArray = OSDynamicCast(OSArray, theInput->getObject(kClipRoutines));
	FailIf (0 == theArray, Exit);
	
	arrayCount = theArray->getCount();

	mDriverDMAEngine->resetInputClipOptions();
	
	for (index = 0; index < arrayCount; index++) {
		clipRoutineString = OSDynamicCast(OSString, theArray->getObject(index));
		debugIOLog ( 3, "  clip routine[%ld] = %s", index, clipRoutineString->getCStringNoCopy());

		if (clipRoutineString->isEqualTo (kCopyLeftToRight)) {
			mDriverDMAEngine->setDualMonoMode (e_Mode_CopyLeftToRight);
		} else if (clipRoutineString->isEqualTo (kCopyRightToLeft)) {
			mDriverDMAEngine->setDualMonoMode (e_Mode_CopyRightToLeft);
		}
	}	

	result = kIOReturnSuccess;
Exit:
	if ( 0 == inSelectedInput ) {
		debugIOLog ( 3,  "+ AppleOnboardAudio[%ld]::setClipRoutineForInput ( %p ) returns 0x%lX", mInstanceIndex, inSelectedInput, result);
	} else {
		debugIOLog ( 3,  "+ AppleOnboardAudio[%ld]::setClipRoutineForInput ( %p->'%4s' ) returns 0x%lX", mInstanceIndex, inSelectedInput, inSelectedInput, result);
	}
	return result;
}

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AppleOnboardAudio::cacheOutputVolumeLevels (AudioHardwareObjectInterface * thePluginObject) {
	if (0 != mOutMasterVolumeControl) {
		mCurrentOutputPlugin->setProperty(kPluginPListMasterVol, mOutMasterVolumeControl->getValue ());
	} else {
		OSNumber *				theNumber;
		
		theNumber = OSNumber::withNumber ((unsigned long long)-1, 32);
		if (0 != mOutLeftVolumeControl) {
			mCurrentOutputPlugin->setProperty(kPluginPListLeftVol, mOutLeftVolumeControl->getValue ());
			mCurrentOutputPlugin->setProperty(kPluginPListMasterVol, theNumber);
		}
		if (0 != mOutRightVolumeControl) {
			mCurrentOutputPlugin->setProperty(kPluginPListRightVol, mOutRightVolumeControl->getValue ());
			mCurrentOutputPlugin->setProperty(kPluginPListMasterVol, theNumber);
		}
		theNumber->release ();
	}	
	return;
}

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AppleOnboardAudio::cacheInputGainLevels (AudioHardwareObjectInterface * thePluginObject) {
	if (0 != mInLeftGainControl) {
		mCurrentInputPlugin->setProperty(kPluginPListLeftGain, mInLeftGainControl->getValue ());
	}
	if (0 != mInRightGainControl) {
		mCurrentInputPlugin->setProperty(kPluginPListRightGain, mInRightGainControl->getValue ());
	}
	if ( 0 != mInMasterGainControl ) {
		mCurrentInputPlugin->setProperty ( kPluginPListMasterGain, mInMasterGainControl->getValue () );
	}
	return;
}

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleOnboardAudio::createOutputVolumeControls (void) {
	AudioHardwareObjectInterface *		thePluginObject;
	OSArray *							controlsArray;
	OSString *							controlString;
	char *								selectedOutput;
	IOReturn							result;
	UInt32								curSelection;
	UInt32								count;
	UInt32								index;

	debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::createOutputVolumeControls()", mInstanceIndex);

	result = kIOReturnError;
	if ( 0 != mOutputSelector ) {
		curSelection = mOutputSelector->getIntValue ();
		
		selectedOutput = getConnectionKeyFromCharCode(curSelection, kIOAudioStreamDirectionOutput);
	
		thePluginObject = getPluginObjectForConnection (selectedOutput);
		FailIf (0 == thePluginObject, Exit);
	
		AdjustOutputVolumeControls (thePluginObject, curSelection);
	
		controlsArray = getControlsArray (selectedOutput);
		FailIf (0 == controlsArray, Exit);
		count = controlsArray->getCount ();
		for (index = 0; index < count; index++) {
			controlString = OSDynamicCast (OSString, controlsArray->getObject (index));
			if (controlString->isEqualTo (kMuteControlString)) {
				mOutMuteControl = IOAudioToggleControl::createMuteControl (false, kIOAudioControlChannelIDAll, kIOAudioControlChannelNameAll, 0, kIOAudioControlUsageOutput);
				if (0 != mOutMuteControl) {
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

	debugIOLog ( 3, "- AppleOnboardAudio[%ld]::createOutputVolumeControls()", mInstanceIndex);
	return result;
}

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
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
	
	debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::createDefaultControls()", mInstanceIndex);

	hasPlaythrough = FALSE;
	result = kIOReturnError;
	FailIf (0 == mDriverDMAEngine, Exit);

	count = mPluginObjects->getCount ();
	for (index = 0; index < count; index++) {
		thePluginObject = getIndexedPluginObject (index);
		FailIf (0 == thePluginObject, Exit);
		AOAprop = OSDynamicCast (OSDictionary, thePluginObject->getProperty (kPluginPListAOAAttributes));
		FailIf (0 == AOAprop, Exit);
	}

	result = createOutputSelectorControl ();
	FailIf (kIOReturnSuccess != result, Exit);

	// The call to createOutputVolumeControls has to come after the creation of the selector control because
	// it references the current value of the selector control to know what controls to create.
	createOutputVolumeControls ();

	if ( !mSpressBootChimeLevelControl ) {		//  [3730863]
		mPRAMVolumeControl = IOAudioLevelControl::create (PRAMToVolumeValue (), 0, 7, 0x00120000, 0, kIOAudioControlChannelIDAll, "BootBeepVolume", 0, kIOAudioLevelControlSubTypePRAMVolume, kIOAudioControlUsageOutput);
		if (0 != mPRAMVolumeControl) {
			mDriverDMAEngine->addDefaultAudioControl (mPRAMVolumeControl);
			mPRAMVolumeControl->setValueChangeHandler ((IOAudioControl::IntValueChangeHandler)outputControlChangeHandler, this);
			mPRAMVolumeControl->release ();
			mPRAMVolumeControl = 0;
		}
	}

	// Create a toggle control for reporting the status of the headphone jack
	mHeadphoneConnected = IOAudioToggleControl::create (FALSE, kIOAudioControlChannelIDAll, kIOAudioControlChannelNameAll, 0, kIOAudioControlTypeJack, kIOAudioControlUsageOutput);

	if (0 != mHeadphoneConnected) {
		mDriverDMAEngine->addDefaultAudioControl (mHeadphoneConnected);
		mHeadphoneConnected->setReadOnlyFlag ();		// 3292105
		// no value change handler because this isn't a settable control
		// Don't release it because we might reference it later
	}

	inputListArray = OSDynamicCast (OSArray, getLayoutEntry (kInputsList, this));
	hasInput = (0 != inputListArray);
	
	if (hasInput) {
		createInputSelectorControl ();
	
		createInputGainControls ();
	}

	clockSelectBoolean = OSDynamicCast ( OSBoolean, getLayoutEntry (kExternalClockSelect, this) );
	if (0 != clockSelectBoolean) {
		if (TRUE == clockSelectBoolean->getValue ()) {
			mExternalClockSelector = IOAudioSelectorControl::create (kClockSourceSelectionInternal, kIOAudioControlChannelIDAll, kIOAudioControlChannelNameAll, 0, kIOAudioSelectorControlSubTypeClockSource, kIOAudioControlUsageInput);		
			FailIf (0 == mExternalClockSelector, Exit);
			mDriverDMAEngine->addDefaultAudioControl (mExternalClockSelector);
			mExternalClockSelector->setValueChangeHandler ((IOAudioControl::IntValueChangeHandler)inputControlChangeHandler, this);
			mExternalClockSelector->addAvailableSelection (kClockSourceSelectionInternal, kInternalClockString);
			mExternalClockSelector->addAvailableSelection (kClockSourceSelectionExternal, kExternalClockString);
			// don't release, may be used in event of loss of clock lock
		}
	}
	else {  // [4073140] - check for external clock auto select
		if ( mAutoSelectClock ) {
			mExternalClockSelector = IOAudioSelectorControl::create (kClockSourceSelectionInternal, kIOAudioControlChannelIDAll, kIOAudioControlChannelNameAll, 0, kIOAudioSelectorControlSubTypeClockSource, kIOAudioControlUsageInput);		
			FailIf (0 == mExternalClockSelector, Exit);
			mDriverDMAEngine->addDefaultAudioControl (mExternalClockSelector);
			mExternalClockSelector->setValueChangeHandler ((IOAudioControl::IntValueChangeHandler)inputControlChangeHandler, this);
			mExternalClockSelector->addAvailableSelection (kClockSourceSelectionInternal, kInternalClockString);
			// don't release, may be used in event of loss of clock lock
		}
	}
  	
	result = kIOReturnSuccess;
Exit:    
    debugIOLog ( 3, "- AppleOnboardAudio[%ld]::createDefaultControls() returns 0x%lX", mInstanceIndex, result);
    return result;
}

//	--------------------------------------------------------------------------------
//	[3515371]	begin	{
//	Searches array of AppleOnboardAudio instances and returns a pointer to the
//	AppleOnboardAudio instance with the specified layoutID or 0.
//	
AppleOnboardAudio* AppleOnboardAudio::findAOAInstanceWithLayoutID ( UInt32 layoutID ) {
	AppleOnboardAudio *		theAOA;
	AppleOnboardAudio *		result = 0;
	UInt32					numInstances;
	
	if ( 0 != mAOAInstanceArray ) {
		numInstances = mAOAInstanceArray->getCount();
		for ( UInt32 index = 0; index <  numInstances && ( 0 == result ); index++ ) {
			theAOA = OSDynamicCast ( AppleOnboardAudio, mAOAInstanceArray->getObject ( index ) );
			if ( theAOA->getLayoutID() == layoutID ) {
				result = theAOA;
			}
		}
	}
	debugIOLog ( 3, "± AppleOnboardAudio[%ld]::findAOAInstanceWithLayoutID ( %d ), this = %p, returns %p", mInstanceIndex, layoutID, this, result );
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
	UInt32								engineState;
	Boolean								hasMaster;
	Boolean								hasLeft;
	Boolean								hasRight;
	Boolean								stereoOutputConnected;
	
	FailIf (0 == mDriverDMAEngine, Exit);

	mindBVol = thePluginObject->getMinimumdBVolume ();
	maxdBVol = thePluginObject->getMaximumdBVolume ();
	minVolume = thePluginObject->getMinimumVolume ();
	maxVolume = thePluginObject->getMaximumVolume ();

	maxVolume += getMaxVolumeOffsetForOutput (selectionCode);

	debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::AdjustOutputVolumeControls( %p, '%4s' ) - mindBVol %lX, maxdBVol %lX, minVolume %ld, maxVolume %ld", 
					mInstanceIndex, thePluginObject, (char*)&selectionCode, mindBVol, maxdBVol, minVolume, maxVolume);
	
    engineState = mDriverDMAEngine->getState();
    debugIOLog (5, "AppleOnboardAudio[%ld]::AdjustOutputVolumeControls - about to try to mDriverDMAEngine->pauseAudioEngine...engine state = %lu", mInstanceIndex, engineState);
	if ( ( kIOAudioEngineRunning == engineState ) || ( kIOAudioEngineResumed == engineState ) ) {
        mDriverDMAEngine->pauseAudioEngine ();
    }
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
			debugIOLog ( 3, "  have volume controls and need software implementation.");
			mCurrentPluginNeedsSoftwareOutputVolume = TRUE;
			mDriverDMAEngine->setUseSoftwareOutputVolume (TRUE, minVolume, maxVolume, mindBVol, maxdBVol);
		} else {
			debugIOLog ( 3, "  no volume controls or don't need software implementation.");
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
			
		if ((TRUE == hasMaster && 0 == mOutMasterVolumeControl) || (FALSE == hasMaster && 0 != mOutMasterVolumeControl) ||
			(0 != mOutMasterVolumeControl && mOutMasterVolumeControl->getMinValue () != minVolume) ||
			(0 != mOutMasterVolumeControl && mOutMasterVolumeControl->getMaxValue () != maxVolume)) {
	
			if (TRUE == hasMaster) {
				// We have only the master volume control (possibly not created yet) and have to remove the other volume controls (possibly don't exist)
				if (0 != mOutMasterVolumeControl) {
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
	
		if ((TRUE == hasLeft && 0 == mOutLeftVolumeControl) || (FALSE == hasLeft && 0 != mOutLeftVolumeControl) ||
			(0 != mOutLeftVolumeControl && mOutLeftVolumeControl->getMinValue () != minVolume) ||
			(0 != mOutLeftVolumeControl && mOutLeftVolumeControl->getMaxValue () != maxVolume)) {
			if (TRUE == hasLeft) {
				if (0 != mOutLeftVolumeControl) {
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
	
		if ((TRUE == hasRight && 0 == mOutRightVolumeControl) || (FALSE == hasRight && 0 != mOutRightVolumeControl) ||
			(0 != mOutRightVolumeControl && mOutRightVolumeControl->getMinValue () != minVolume) ||
			(0 != mOutRightVolumeControl && mOutRightVolumeControl->getMaxValue () != maxVolume)) {
			if (TRUE == hasRight) {
				if (0 != mOutRightVolumeControl) {
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
	engineState = mDriverDMAEngine->getState();
    debugIOLog (5, "AppleOnboardAudio[%ld]::AdjustOutputVolumeControls - about to try to mDriverDMAEngine->resumeAudioEngine...engine state = %lu", mInstanceIndex, engineState);
	if ( kIOAudioEnginePaused == engineState ) {
		mDriverDMAEngine->resumeAudioEngine ();
		// [4238699]
		// resumeAudioEngine alone only puts IOAudioEngine in its kIOAudioEngineResumed state.  If an engine was running prior to this pause-resume
		// sequence, it might be possible to keep the engine in a resumed state indefinitely.  This can prevent audioEngineStopped from being called, even
		// if a sound has finished playing.  This is dangerous when running on battery power as it can prevent us from going idle.  By calling startAudioEngine,
		// we force the engine to issue a performAudioEngineStart, and the engine's state is set to running.  This allows audioEngineStopped to be called.
		//
		// Before calling startAudioEngine, check to make sure that the engine is in the resumed state.  This ensures that the audio engine will be started on only
		// the last resume call in cases where pause-resume sequences are nested.
		if ( kIOAudioEngineResumed == mDriverDMAEngine->getState() ) {
			mDriverDMAEngine->startAudioEngine ();
		}
	}
	
Exit:
	debugIOLog ( 3, "- AppleOnboardAudio[%ld]::AdjustOutputVolumeControls( %p, '%4s' )", mInstanceIndex, thePluginObject, (char*)&selectionCode );
	return kIOReturnSuccess;
}

// --------------------------------------------------------------------------
// You either have only a master volume control, or you have both volume controls.
IOReturn AppleOnboardAudio::AdjustInputGainControls (AudioHardwareObjectInterface * thePluginObject) {
	IOFixed								mindBGain;
	IOFixed								maxdBGain;
	SInt32								minGain;
	SInt32								maxGain;
	UInt32								engineState;
	
	FailIf (0 == mDriverDMAEngine, Exit);

	mindBGain = thePluginObject->getMinimumdBGain ();
	maxdBGain = thePluginObject->getMaximumdBGain ();
	minGain = thePluginObject->getMinimumGain ();
	maxGain = thePluginObject->getMaximumGain ();

	debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::AdjustInputGainControls ( %p )", mInstanceIndex, thePluginObject );
	debugIOLog ( 3, "  mindBGain %lX, maxdBGain %lX, minGain %ld, maxGain %ld", mInstanceIndex, mindBGain, maxdBGain, minGain, maxGain);
	
    engineState = mDriverDMAEngine->getState();
    debugIOLog (5, "AppleOnboardAudio[%ld]::AdjustInputGainControls - about to try to mDriverDMAEngine->pauseAudioEngine...engine state = %lu", mInstanceIndex, engineState);
	if ( ( kIOAudioEngineRunning == engineState ) || ( kIOAudioEngineResumed == engineState ) ) {
        mDriverDMAEngine->pauseAudioEngine ();
    }
	mDriverDMAEngine->beginConfigurationChange ();
	
	removePlayThruControl ();
	//	[3281535]	begin {
	if ( mUsePlaythroughControl ) {
		createPlayThruControl ();
	}
	//	[3281535]	} end

	if ( kStereoInputGainControls == mUseInputGainControls ) {
		debugIOLog ( 3, "  AdjustInputGainControls - creating input gain controls.");
		// or we have both controls (possibly not created yet) and we have to remove the master volume control (possibly doesn't exist)
		if (0 != mInLeftGainControl) {
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
	
		if (0 != mInRightGainControl) {
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
		if (0 != mInMasterGainControl) {
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
		debugIOLog ( 3, "  AdjustInputGainControls - removing input gain controls.");
		removeLeftGainControl();
		removeRightGainControl();
		removeMasterGainControl();
	}
	
	mDriverDMAEngine->completeConfigurationChange ();
	engineState = mDriverDMAEngine->getState();
    debugIOLog (5, "AppleOnboardAudio[%ld]::AdjustInputGainControls - about to try to mDriverDMAEngine->resumeAudioEngine...engine state = %lu", mInstanceIndex, engineState);
	if ( kIOAudioEnginePaused == engineState ) {
		mDriverDMAEngine->resumeAudioEngine ();
		// [4238699]
		// resumeAudioEngine alone only puts IOAudioEngine in its kIOAudioEngineResumed state.  If an engine was running prior to this pause-resume
		// sequence, it might be possible to keep the engine in a resumed state indefinitely.  This can prevent audioEngineStopped from being called, even
		// if a sound has finished playing.  This is dangerous when running on battery power as it can prevent us from going idle.  By calling startAudioEngine,
		// we force the engine to issue a performAudioEngineStart, and the engine's state is set to running.  This allows audioEngineStopped to be called.
		//
		// Before calling startAudioEngine, check to make sure that the engine is in the resumed state.  This ensures that the audio engine will be started on only
		// the last resume call in cases where pause-resume sequences are nested.
		if ( kIOAudioEngineResumed == mDriverDMAEngine->getState() ) {
			mDriverDMAEngine->startAudioEngine ();
		}
	}
	
Exit:
	debugIOLog ( 3, "- AppleOnboardAudio[%ld]::AdjustInputGainControls ( %p ) returns kIOReturnSuccess", mInstanceIndex, thePluginObject );
	return kIOReturnSuccess;
}

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IORegistryEntry * AppleOnboardAudio::FindEntryByNameAndProperty (const IORegistryEntry * start, const char * name, const char * key, UInt32 value) {
	OSIterator				*iterator;
	IORegistryEntry			*theEntry;
	IORegistryEntry			*tmpReg;
	OSNumber				*tmpNumber;

	theEntry = 0;
	iterator = 0;
	FailIf (0 == start, Exit);

	iterator = start->getChildIterator (gIOServicePlane);
	FailIf (0 == iterator, Exit);

	while (0 == theEntry && (tmpReg = OSDynamicCast (IORegistryEntry, iterator->getNextObject ())) != 0) {
		if (strcmp (tmpReg->getName (), name) == 0) {
			tmpNumber = OSDynamicCast (OSNumber, tmpReg->getProperty (key));
			if (0 != tmpNumber && tmpNumber->unsigned32BitValue () == value) {
				theEntry = tmpReg;
				// remove retain?
				//theEntry->retain();
			}
		}
	}

Exit:
	if (0 != iterator) {
		iterator->release ();
	}
	return theEntry;
}

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AppleOnboardAudio::createLeftVolumeControl (IOFixed mindBVol, IOFixed maxdBVol, SInt32 minVolume, SInt32 maxVolume) {
	OSNumber * 						theNumber;
	SInt32							leftVol;
	
	leftVol = 0;
	theNumber = OSDynamicCast(OSNumber, mCurrentOutputPlugin->getProperty(kPluginPListLeftVol));
	if (0 != theNumber) {
		leftVol = theNumber->unsigned32BitValue();
	}
	debugIOLog ( 3, "  AppleOnboardAudio::createLeftVolumeControl - leftVol initially = %ld, theNumber = %p", leftVol, theNumber);

	if (0 == theNumber || leftVol == 0) {
		theNumber = OSDynamicCast(OSNumber, mCurrentOutputPlugin->getProperty(kPluginPListMasterVol));
		if (0 != theNumber) {
			leftVol = theNumber->unsigned32BitValue();
			debugIOLog ( 6, "  createLeftVolumeControl - leftVol from master = %ld", leftVol);
			if (leftVol == -1) {
				leftVol = maxVolume / 2;
				debugIOLog ( 6, "  createLeftVolumeControl - leftVol from max/2 = %ld", leftVol);
			}
		} else {
			leftVol = mCurrentOutputPlugin->getDefaultOutputVolume ();
			debugIOLog ( 6, "  createLeftVolumeControl - leftVol from default = %ld", leftVol);
		}
	}
	
	debugIOLog ( 3, "  createLeftVolumeControl - leftVol = %ld", leftVol);

	mOutLeftVolumeControl = IOAudioLevelControl::createVolumeControl (leftVol, minVolume, maxVolume, mindBVol, maxdBVol,
										kIOAudioControlChannelIDDefaultLeft,
										kIOAudioControlChannelNameLeft,
										0,
										kIOAudioControlUsageOutput);
	if (0 != mOutLeftVolumeControl) {
		mDriverDMAEngine->addDefaultAudioControl (mOutLeftVolumeControl);
		mOutLeftVolumeControl->setValueChangeHandler ((IOAudioControl::IntValueChangeHandler)outputControlChangeHandler, this);
		mOutLeftVolumeControl->flushValue ();
	}
}

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AppleOnboardAudio::createRightVolumeControl (IOFixed mindBVol, IOFixed maxdBVol, SInt32 minVolume, SInt32 maxVolume) {
	OSNumber * 						theNumber;
	SInt32							rightVol;
	
	rightVol = 0;
	theNumber = OSDynamicCast(OSNumber, mCurrentOutputPlugin->getProperty(kPluginPListRightVol));
	if (0 != theNumber) {
		rightVol = theNumber->unsigned32BitValue();
	}

	if (0 == theNumber || rightVol == 0) {
		theNumber = OSDynamicCast(OSNumber, mCurrentOutputPlugin->getProperty(kPluginPListMasterVol));
		if (0 != theNumber) {
			rightVol = theNumber->unsigned32BitValue();
			if (rightVol == -1) {
				rightVol = maxVolume / 2;
			}
		} else {
			rightVol = mCurrentOutputPlugin->getDefaultOutputVolume ();
		}
	}

	debugIOLog ( 3, "  createRightVolumeControl - rightVol = %ld", rightVol);

	mOutRightVolumeControl = IOAudioLevelControl::createVolumeControl (rightVol, minVolume, maxVolume, mindBVol, maxdBVol,
										kIOAudioControlChannelIDDefaultRight,
										kIOAudioControlChannelNameRight,
										0,
										kIOAudioControlUsageOutput);
	if (0 != mOutRightVolumeControl) {
		mDriverDMAEngine->addDefaultAudioControl (mOutRightVolumeControl);
		mOutRightVolumeControl->setValueChangeHandler ((IOAudioControl::IntValueChangeHandler)outputControlChangeHandler, this);
		mOutRightVolumeControl->flushValue ();
	}
}

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AppleOnboardAudio::createMasterVolumeControl (IOFixed mindBVol, IOFixed maxdBVol, SInt32 minVolume, SInt32 maxVolume) {
	OSNumber * 						theNumber;
	SInt32							masterVol;
	
	masterVol = 0;
	theNumber = OSDynamicCast(OSNumber, mCurrentOutputPlugin->getProperty(kPluginPListMasterVol));
	if (0 != theNumber) {
		masterVol = theNumber->unsigned32BitValue();
	}
	debugIOLog ( 3, "AppleOnboardAudio::createMasterVolumeControl - masterVol initially = %ld, theNumber = %p", masterVol, theNumber);

	if (-1 == masterVol || 0 == theNumber) {
		OSNumber * 						theLeftNumber;
		OSNumber * 						theRightNumber;

		theLeftNumber = OSDynamicCast(OSNumber, mCurrentOutputPlugin->getProperty(kPluginPListLeftVol));
		theRightNumber = OSDynamicCast(OSNumber, mCurrentOutputPlugin->getProperty(kPluginPListRightVol));

		if (0 == theLeftNumber && 0 == theRightNumber && 0 == theNumber) {
			masterVol = mCurrentOutputPlugin->getDefaultOutputVolume ();
			debugIOLog ( 6, "createMasterVolumeControl - masterVol from default = %ld", masterVol);
		} else {
			if (0 == theLeftNumber) {
				masterVol = maxVolume;
				debugIOLog ( 6,"createMasterVolumeControl - masterVol from max = %ld", masterVol);
			} else {
				masterVol = theLeftNumber->unsigned32BitValue();
				debugIOLog ( 6,"createMasterVolumeControl - masterVol from left = %ld", masterVol);
			}
			if (0 != theRightNumber) {
				masterVol += theRightNumber->unsigned32BitValue();
				debugIOLog ( 6,"createMasterVolumeControl - masterVol from right = %ld", masterVol);
			}
			masterVol >>= 1;
			debugIOLog ( 6,"createMasterVolumeControl - masterVol after shift = %ld", masterVol);
		}
	}
	
	debugIOLog ( 3, "createMasterVolumeControl - masterVol = %ld", masterVol);
	
	mOutMasterVolumeControl = IOAudioLevelControl::createVolumeControl (masterVol, minVolume, maxVolume, mindBVol, maxdBVol,
																		kIOAudioControlChannelIDAll,
																		kIOAudioControlChannelNameAll,
																		0, 
																		kIOAudioControlUsageOutput);

	if (0 != mOutMasterVolumeControl) {
		mDriverDMAEngine->addDefaultAudioControl(mOutMasterVolumeControl);
		mOutMasterVolumeControl->setValueChangeHandler((IOAudioControl::IntValueChangeHandler)outputControlChangeHandler, this);
		mOutMasterVolumeControl->flushValue ();
	}

}


//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AppleOnboardAudio::createLeftGainControl (IOFixed mindBGain, IOFixed maxdBGain, SInt32 minGain, SInt32 maxGain) {
	OSNumber * 						theNumber;
	SInt32							leftGain;
	
	theNumber = OSDynamicCast(OSNumber, mCurrentInputPlugin->getProperty(kPluginPListLeftGain));
	if (0 == theNumber) {
		leftGain = 0;
	} else {
		leftGain = theNumber->unsigned32BitValue();
	}
	
	debugIOLog ( 3, "createLeftGainControl - leftVol = %ld", leftGain);

	mInLeftGainControl = IOAudioLevelControl::createVolumeControl (leftGain, minGain, maxGain, mindBGain, maxdBGain,
										kIOAudioControlChannelIDDefaultLeft,
										kIOAudioControlChannelNameLeft,
										0,
										kIOAudioControlUsageInput);
	if (0 != mInLeftGainControl) {
		mDriverDMAEngine->addDefaultAudioControl (mInLeftGainControl);
		mInLeftGainControl->setValueChangeHandler ((IOAudioControl::IntValueChangeHandler)inputControlChangeHandler, this);
		mInLeftGainControl->flushValue ();
	}
}

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AppleOnboardAudio::createRightGainControl (IOFixed mindBGain, IOFixed maxdBGain, SInt32 minGain, SInt32 maxGain) {
	OSNumber * 						theNumber;
	SInt32							rightGain;
	
	theNumber = OSDynamicCast(OSNumber, mCurrentInputPlugin->getProperty(kPluginPListRightGain));
	if (0 == theNumber) {
		rightGain = 0;
	} else {
		rightGain = theNumber->unsigned32BitValue();
	}
	
	debugIOLog ( 3, "createRightGainControl - rightVol = %ld", rightGain);

	mInRightGainControl = IOAudioLevelControl::createVolumeControl (rightGain, minGain, maxGain, mindBGain, maxdBGain,
										kIOAudioControlChannelIDDefaultRight,
										kIOAudioControlChannelNameRight,
										0,
										kIOAudioControlUsageInput);
	if (0 != mInRightGainControl) {
		mDriverDMAEngine->addDefaultAudioControl (mInRightGainControl);
		mInRightGainControl->setValueChangeHandler ((IOAudioControl::IntValueChangeHandler)inputControlChangeHandler, this);
		mInRightGainControl->flushValue ();
	}
}

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AppleOnboardAudio::createMasterGainControl (IOFixed mindBGain, IOFixed maxdBGain, SInt32 minGain, SInt32 maxGain) {
	OSNumber * 						theNumber;
	SInt32							masterGain;
	
	theNumber = OSDynamicCast(OSNumber, mCurrentInputPlugin->getProperty("master-gain"));
	if (0 == theNumber) {
		masterGain = 0;
	} else {
		masterGain = theNumber->unsigned32BitValue();
	}
	
	debugIOLog ( 3, "createMasterGainControl - masterVol = %ld", masterGain);

	mInMasterGainControl = IOAudioLevelControl::createVolumeControl (masterGain, minGain, maxGain, mindBGain, maxdBGain,
										kIOAudioControlChannelIDAll,
										kIOAudioControlChannelNameAll,
										0,
										kIOAudioControlUsageInput);
	if (0 != mInMasterGainControl) {
		mDriverDMAEngine->addDefaultAudioControl (mInMasterGainControl);
		mInMasterGainControl->setValueChangeHandler ((IOAudioControl::IntValueChangeHandler)inputControlChangeHandler, this);
		mInMasterGainControl->flushValue ();
	}
}

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AppleOnboardAudio::removeLeftVolumeControl() {
	if (0 != mOutLeftVolumeControl) {
		mDriverDMAEngine->removeDefaultAudioControl (mOutLeftVolumeControl);
		mOutLeftVolumeControl = 0;
	}
}

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AppleOnboardAudio::removeRightVolumeControl() {
	if (0 != mOutRightVolumeControl) {
		mDriverDMAEngine->removeDefaultAudioControl (mOutRightVolumeControl);
		mOutRightVolumeControl = 0;
	}
}

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AppleOnboardAudio::removeMasterVolumeControl() {
	if (0 != mOutMasterVolumeControl) {
		mDriverDMAEngine->removeDefaultAudioControl (mOutMasterVolumeControl);
		mOutMasterVolumeControl = 0;
	}
}

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AppleOnboardAudio::removeLeftGainControl() {
	if (0 != mInLeftGainControl) {
		mDriverDMAEngine->removeDefaultAudioControl (mInLeftGainControl);
		mInLeftGainControl = 0;
	}
}

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AppleOnboardAudio::removeRightGainControl() {
	if (0 != mInRightGainControl) {
		mDriverDMAEngine->removeDefaultAudioControl (mInRightGainControl);
		mInRightGainControl = 0;
	}
}

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AppleOnboardAudio::removeMasterGainControl() {
	if ( 0 != mInMasterGainControl ) {
		mDriverDMAEngine->removeDefaultAudioControl ( mInMasterGainControl );
		mInMasterGainControl = 0;
	}
}


//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	[3922678]	Dispatch to virtual space to enable access to protected member variables in the base class.
IOReturn AppleOnboardAudio::outputControlChangeHandler (IOService *target, IOAudioControl *control, SInt32 oldValue, SInt32 newValue)
{
	IOReturn						result = kIOReturnError;
	AppleOnboardAudio *				audioDevice;
	
	audioDevice = OSDynamicCast (AppleOnboardAudio, target);
	FailIf ( 0 == audioDevice, Exit );
	
	result = audioDevice->outputControlChangeHandlerAction ( target, control, oldValue, newValue );
Exit:
	return result;
}


//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  outputControlChangeHandler:		A call-back method for the IOAudioControl that is invoked when the
//									IOAudioControl changes value.
//	[3922678]	All member variables and methods now directly accessed rather than using a pointer to
//				an AppleOnboardAudio object allows access to protected members of base class.
IOReturn AppleOnboardAudio::outputControlChangeHandlerAction ( IOService *target, IOAudioControl *control, SInt32 oldValue, SInt32 newValue )
{
	IOReturn						result;
	IOAudioLevelControl *			levelControl;
	IODTPlatformExpert * 			platform;
	UInt32							leftVol;
	UInt32							rightVol;
	Boolean							wasPoweredDown;
	UInt32							subType;

	debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::outputControlChangeHandlerAction (%p, %p, %lX, %lX), currentPowerState %ld", mInstanceIndex, target, control, oldValue, newValue, getPowerState ());

	result = kIOReturnError;
	wasPoweredDown = FALSE;
	// We have to make sure the hardware is on before we can send it any control changes [2981190]
	debugIOLog ( 6, "  getPowerState () is %d", getPowerState () );
	if ( kIOAudioDeviceActive != getPowerState () )
	{
		if ( mDoKPrintfPowerState )
		{
			IOLog ( "AppleOnboardAudio[%ld]::outputControlChangeHandlerAction setting ACTIVE\n", mInstanceIndex );
		}
		FailIf ( kIOReturnSuccess != doLocalChangeToActiveState ( TRUE, &wasPoweredDown ), Exit );
	}

	switch ( control->getType () )
	{
		case kIOAudioControlTypeLevel:
			debugIOLog ( 5, "  control->getType () is a kIOAudioControlTypeLevel" );
			switch ( control->getSubType () )
			{
				case kIOAudioLevelControlSubTypeVolume:
					debugIOLog ( 5, "  control->getSubType () is a kIOAudioLevelControlSubTypeVolume" );
					levelControl = OSDynamicCast (IOAudioLevelControl, control);
		
					switch ( control->getChannelID () )
					{
						case kIOAudioControlChannelIDAll:
							result = volumeMasterChange (newValue);
							if ( newValue == levelControl->getMinValue () )
							{
								// If it's set to it's min, then it's mute, so tell the HAL it's muted
								OSNumber *			muteState;
								muteState = OSNumber::withNumber ((long long unsigned int)1, 32);
								if (0 != mOutMuteControl) {
									mOutMuteControl->hardwareValueChanged (muteState);
									debugIOLog ( 3, "  volume control change calling callPluginsInOrder ( kSetAnalogMuteState, TRUE )");
									result = callPluginsInOrder ( kSetAnalogMuteState, TRUE );
								}
								muteState->release();
							}
							else if (oldValue == levelControl->getMinValue () && FALSE == mIsMute)
							{
								OSNumber *			muteState;
								muteState = OSNumber::withNumber ((long long unsigned int)0, 32);
								if (0 != mOutMuteControl) {
									mOutMuteControl->hardwareValueChanged (muteState);
									debugIOLog ( 3, "  volume control change calling callPluginsInOrder ( kSetAnalogMuteState, FALSE )");
									result = callPluginsInOrder ( kSetAnalogMuteState, FALSE );
								}
								muteState->release();
							}
							break;
						case kIOAudioControlChannelIDDefaultLeft:
							result = volumeLeftChange (newValue);
							break;
						case kIOAudioControlChannelIDDefaultRight:
							result = volumeRightChange (newValue);
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
						mUCState.ucPramData = (UInt32)curPRAMVol;
						mUCState.ucPramVolume = mUCState.ucPramData & 0x00000007;
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
						volumeLeftChange(mVolLeft, TRUE);
						volumeRightChange(mVolRight, TRUE);
					}
					selectCodecOutputWithMuteState( newValue );
					selectOutputAmplifiers ( mOutputSelector->getIntValue (), newValue );
					if ( mUIMutesAmps ) {				//  [3707147]
						if ( FALSE != newValue ) {					//  [3707147]
							muteAllAmps();				//  [3707147]
						}											//  [3707147]
					}												//  [3707147]
					mIsMute = newValue;
					result = kIOReturnSuccess;
					break;
				default:
					result = kIOReturnBadArgument;
					break;
			}
			break;
		case kIOAudioControlTypeSelector:
			result = outputSelectorChanged (newValue);
			break;
		default:
			break;
	}

	if (control->getSubType () == kIOAudioLevelControlSubTypeVolume) {
		levelControl = OSDynamicCast (IOAudioLevelControl, control);
		if (mOutRightVolumeControl && mOutLeftVolumeControl) {
			if (mOutRightVolumeControl->getMinValue () == mVolRight &&
				mOutLeftVolumeControl->getMinValue () == mVolLeft) {
				// If it's set to it's min, then it's mute, so tell the HAL it's muted
				OSNumber *			muteState;
				muteState = OSNumber::withNumber ((long long unsigned int)1, 32);
				if (0 != mOutMuteControl) {
					mOutMuteControl->hardwareValueChanged (muteState);
					debugIOLog ( 3, "  right volume control change updating mute state");
			}
				muteState->release ();
			} else if (newValue != levelControl->getMinValue () && oldValue == levelControl->getMinValue () && FALSE == mIsMute) {
				OSNumber *			muteState;
				muteState = OSNumber::withNumber ((long long unsigned int)0, 32);
				if (0 != mOutMuteControl) {
					mOutMuteControl->hardwareValueChanged (muteState);
					debugIOLog ( 3, "  left volume control change updating mute state");
				}
				muteState->release ();
			}
		}
	}

	if (mIsMute) {
		leftVol = 0;
		rightVol = 0;
	} else {
		leftVol = mVolLeft;
		rightVol = mVolRight;
	}

	if (TRUE == mAutoUpdatePRAM) {				// We do that only if we are on a OS 9 like UI guideline
		WritePRAMVol (leftVol, rightVol);
	}
Exit:
	// Second half of [2981190], put us back to sleep after the right amount of time if we were off.
	// NOTE:	The 'initialized' is initialized to TRUE by PMInit() which is executed from super::start.
	//			The 'initialized' is a protected member of IOService.
	result = doLocalChangeScheduleIdle ( wasPoweredDown );
	FailMessage ( kIOReturnSuccess != result );

	debugIOLog ( 3, "- AppleOnboardAudio[%ld]::outputControlChangeHandlerAction (%p, %p, %lX, %lX) returns %X", mInstanceIndex, target, control, oldValue, newValue, result);

	return result;
}

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AppleOnboardAudio::createPlayThruControl (void) {
	OSDictionary *						AOAprop;

	AOAprop = OSDynamicCast (OSDictionary, mCurrentInputPlugin->getProperty (kPluginPListAOAAttributes));
	FailIf (0 == AOAprop, Exit);

	if (kOSBooleanTrue == OSDynamicCast (OSBoolean, AOAprop->getObject ("Playthrough"))) {
		mPlaythruToggleControl = IOAudioToggleControl::createMuteControl (TRUE, kIOAudioControlChannelIDAll, kIOAudioControlChannelNameAll, 0, kIOAudioControlUsagePassThru);

		if (0 != mPlaythruToggleControl) {
			mDriverDMAEngine->addDefaultAudioControl (mPlaythruToggleControl);
			mPlaythruToggleControl->setValueChangeHandler ((IOAudioControl::IntValueChangeHandler)inputControlChangeHandler, this);
			// Don't release it because we might reference it later
		}
	}

Exit:
	return;
}

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AppleOnboardAudio::removePlayThruControl (void) {
	if (0 != mPlaythruToggleControl) {
		mDriverDMAEngine->removeDefaultAudioControl (mPlaythruToggleControl);
		mPlaythruToggleControl->release ();
		mPlaythruToggleControl = 0;
	}
}

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  outputSelectorChanged:		Invoked on the driver's work loop when the hardware has changed state 
//								and updates the IOAudioControl and I/O Registry to the new value before
//								notifying registered clients of a change.
IOReturn AppleOnboardAudio::outputSelectorChanged (SInt32 newValue) {
	AudioHardwareObjectInterface *		thePluginObject;
	IOAudioStream *						outputStream;
	const IOAudioStreamFormat *			theFormat;
	char *								connectionString;
	IOReturn							result;
	UInt32								inputLatency;

	debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::outputSelectorChanged( '%4s' )", mInstanceIndex, (char *)&newValue);

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
		FailIf (0 == thePluginObject, Exit);
	
		outputStream = mDriverDMAEngine->getAudioStream (kIOAudioStreamDirectionOutput, 1);
		FailIf (0 == outputStream, Exit);
		
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
		
		// [3639956] Remember the user's selection, so we can restore SPDIF output after sleep.
		mOutputSelectorLastValue=newValue;
		
	} else {
		debugIOLog ( 3, "  AppleOnboardAudio[%ld]::outputSelectorChanged disallowing selection of '%4s'", mInstanceIndex, (char *)&newValue );
	}

Exit:
	debugIOLog ( 3, "- AppleOnboardAudio[%ld]::outputSelectorChanged( '%4s' ) returns %X", mInstanceIndex, (char *)&newValue, result);
	return result;
}

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// This is called when we're on hardware that only has one active volume control (either right or left)
// otherwise the respective right or left volume handler will be called.
// This calls both volume handers becasue it doesn't know which one is really the active volume control.
IOReturn AppleOnboardAudio::volumeMasterChange (SInt32 newValue) {
	IOReturn		result;

	// Don't know which volume control really exists, so adjust both -- they'll ignore the change if they don't exist
	result = volumeLeftChange (newValue);
	result = volumeRightChange (newValue);

	result = kIOReturnSuccess;

	debugIOLog ( 3, "± AppleOnboardAudio[%ld]::volumeMasterChange ( %ld )0x%x", mInstanceIndex, newValue, result);
	return result;
}


//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleOnboardAudio::volumeLeftChange (SInt32 newValue, bool ignoreMuteState) {
	UInt32 								count;
	UInt32 								index;
	AudioHardwareObjectInterface* 		thePluginObject;

	debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::volumeLeftChange ( %ld, %d )", mInstanceIndex, newValue, ignoreMuteState);

	if ( mIsMute && FALSE == ignoreMuteState) {		//	[3435307]
		if (0 != mPluginObjects) {
			count = mPluginObjects->getCount ();
			for (index = 0; index < count; index++) {
				thePluginObject = getIndexedPluginObject (index);
				if((0 != thePluginObject)) {
					thePluginObject->setMute (mIsMute);
				}
			}
		}
	} else {
		if (TRUE == mCurrentPluginNeedsSoftwareOutputVolume) {	// [3527440] aml
			// set software volume (will be scaled to hardware range)
			mDriverDMAEngine->setOutputVolumeLeft (newValue);
			// set hardware volume to 1.0
			if (0 != mPluginObjects) {
				count = mPluginObjects->getCount ();
				for (index = 0; index < count; index++) {
					thePluginObject = getIndexedPluginObject (index);
					if((0 != thePluginObject)) {
						thePluginObject->setVolume (thePluginObject->getMaximumVolume (), thePluginObject->getMaximumVolume ());
					}
				}
			}
		} else {
			// [3339273] set volume on all plugins
			if (0 != mPluginObjects) {
				count = mPluginObjects->getCount ();
				for (index = 0; index < count; index++) {
					thePluginObject = getIndexedPluginObject (index);
					if((0 != thePluginObject)) {
						thePluginObject->setVolume (newValue, mVolRight);
					}
				}
			}
		}
	}

	mVolLeft = newValue;

	debugIOLog ( 3, "- AppleOnboardAudio[%ld]::volumeLeftChange ( %ld, %d )", mInstanceIndex, newValue, ignoreMuteState);
	return kIOReturnSuccess;
}


//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleOnboardAudio::volumeRightChange (SInt32 newValue, bool ignoreMuteState) {
	UInt32								count;
	UInt32 								index;
	AudioHardwareObjectInterface* 		thePluginObject;

	debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::volumeRightChange ( %ld, %d )", mInstanceIndex, newValue, ignoreMuteState);

	if ( mIsMute && FALSE == ignoreMuteState ) {		//	[3435307]
		if (0 != mPluginObjects) {
			count = mPluginObjects->getCount ();
			for (index = 0; index < count; index++) {
				thePluginObject = getIndexedPluginObject (index);
				if((0 != thePluginObject)) {
					thePluginObject->setMute (mIsMute);
				}
			}
		}
	} else {
		if (TRUE == mCurrentPluginNeedsSoftwareOutputVolume) {		// [3527440] aml
			// set software volume (will be scaled to hardware range)
			mDriverDMAEngine->setOutputVolumeRight (newValue);
			// set hardware volume to 1.0
			if (0 != mPluginObjects) {
				count = mPluginObjects->getCount ();
				for (index = 0; index < count; index++) {
					thePluginObject = getIndexedPluginObject (index);
					if((0 != thePluginObject)) {
						thePluginObject->setVolume (thePluginObject->getMaximumVolume (), thePluginObject->getMaximumVolume ());
					}
				}
			}
		} else {
			// [3339273] set volume on all plugins
			if (0 != mPluginObjects) {
				count = mPluginObjects->getCount ();
				for (index = 0; index < count; index++) {
					thePluginObject = getIndexedPluginObject (index);
					if((0 != thePluginObject)) {
						thePluginObject->setVolume (mVolLeft, newValue);
					}
				}
			}
		}
	}

	mVolRight = newValue;

	debugIOLog ( 3, "- AppleOnboardAudio[%ld]::volumeRightChange ( %ld, %d )", mInstanceIndex, newValue, ignoreMuteState);
	return kIOReturnSuccess;
}


//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleOnboardAudio::selectCodecOutputWithMuteState ( SInt32 newValue ) {
	UInt32			comboOutJackTypeState;
	IOReturn		result = kIOReturnError;
	
	
	debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::selectCodecOutputWithMuteState (%ld)", mInstanceIndex, newValue);

	FailIf ( 0 == mPlatformInterface, Exit );
	
	comboOutJackTypeState = mPlatformInterface->getComboOutJackTypeConnected ();
	
	if ( kGPIO_Unknown == comboOutJackTypeState ) {
		//  NOTE:   Non-exclusive output selection may be in play here!
		debugIOLog ( 3, "  comboOutJackTypeState is unknown");
		if ( !mEncodedOutputFormat ) {
			//	[3514514]	Digital out is not an exclusive digital output so apply the mute state
			//				to both the analog and digital paths (i.e. shared control).
			debugIOLog ( 3, "  selectCodecOutputWithMuteState calling callPluginsInOrder ( kSetMuteState, %ld ) non-exclusive dig", newValue);
			result = callPluginsInOrder ( kSetMuteState, newValue );
			FailMessage ( kIOReturnSuccess != result );
		} else {
			debugIOLog ( 3, "  selectCodecOutputWithMuteState calling callPluginsInOrder ( kSetAnalogMuteState, %ld ) exclusive dig", TRUE);
			result = callPluginsInOrder ( kSetAnalogMuteState, TRUE );
			FailMessage ( kIOReturnSuccess != result );
			if ( kIOReturnSuccess == result ) {
				debugIOLog ( 3, "  selectCodecOutputWithMuteState calling callPluginsInOrder ( kSetDigitalMuteState, %ld ) exclusive dig", newValue);
				result = callPluginsInOrder ( kSetDigitalMuteState, newValue );
			}
		}
		result = callPluginsInOrder ( kSetEnableSPDIFOut, TRUE );			//  [3750927]
	} else {
		//  NOTE:   Exclusive output selection is in play here!
		debugIOLog ( 3, "  comboOutJackTypeState is %s", comboOutJackTypeState == kGPIO_TypeIsDigital ? "digital" : "analog");

		GpioAttributes theAnalogState = kGPIO_Unknown;
		switch ( (UInt32)mPlatformInterface->getComboOutAssociation () ) {
			case kGPIO_Selector_HeadphoneDetect:	theAnalogState = mPlatformInterface->getHeadphoneConnected ();		break;
			case kGPIO_Selector_LineOutDetect:		theAnalogState = mPlatformInterface->getLineOutConnected ();		break;
			case kGPIO_Selector_SpeakerDetect:		theAnalogState = mPlatformInterface->getSpeakerConnected ();		break;
		}
		debugIOLog ( 3, "  (UInt32)mPlatformInterface->getComboOutAssociation () returns 0x%lx", (UInt32)mPlatformInterface->getComboOutAssociation ());
		debugIOLog ( 3, "  theAnalogState = 0x%lx, comboOutJackTypeState = 0x%lx", theAnalogState, comboOutJackTypeState);

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
			//		COMBO OUT TYPE		COMBO OUT DETECT		MUTE ACTION						SPDIF INTERFACE
			//		
			//			DIGITAL				REMOVED				MUTE DIGITAL, UNMUTE ANALOG		DISABLE
			//			DIGITAL				INSERTED			MUTE ANALOG, UNMUTE DIGITAL		ENABLE
			//			ANALOG				REMOVED				MUTE DIGITAL, UNMUTE ANALOG		DISABLE
			//			ANALOG				INSERTED			MUTE DIGITAL, UNMUTE ANALOG		DISABLE
			//			
			if ( kGPIO_Connected == theAnalogState && kGPIO_TypeIsDigital == comboOutJackTypeState ) {
				//	[3514514]	Exclusive digital output is selected so apply control state to
				//				the digital section but mute the analog section.
				debugIOLog ( 3, "  selectCodecOutputWithMuteState calling callPluginsInOrder ( kSetAnalogMuteState, %ld ) exclusive dig", TRUE);
				result = callPluginsInOrder ( kSetAnalogMuteState, TRUE );
				if ( kIOReturnSuccess == result ) {
					debugIOLog ( 3, "  selectCodecOutputWithMuteState calling callPluginsInOrder ( kSetDigitalMuteState, %ld ) exclusive dig", newValue);
					result = callPluginsInOrder ( kSetDigitalMuteState, newValue );
				}
				result = callPluginsInOrder ( kSetEnableSPDIFOut, TRUE );
			} else {
				//	[3514514]	Exclusive digital output is not selected so apply control state to
				//				the analog section but mute the digital section.
				debugIOLog ( 3, "  selectCodecOutputWithMuteState calling callPluginsInOrder ( kSetAnalogMuteState, %ld ) non-exclusive dig", newValue);
				result = callPluginsInOrder ( kSetAnalogMuteState, newValue );
				if ( kIOReturnSuccess == result ) {
					debugIOLog ( 3, "  selectCodecOutputWithMuteState calling callPluginsInOrder ( kSetDigitalMuteState, %ld ) non-exclusive dig", TRUE);
					result = callPluginsInOrder ( kSetDigitalMuteState, TRUE );
				}
				result = callPluginsInOrder ( kSetEnableSPDIFOut, FALSE );
			}
			//	} end	[3513367, 3544877]
		} else {
			debugIOLog ( 3, "encoded format");

			result = callPluginsInOrder ( kSetEnableSPDIFOut, TRUE );

			//	[3656784] mute analog always as we are in encoded format mode...
			debugIOLog ( 3, "  selectCodecOutputWithMuteState calling callPluginsInOrder ( kSetAnalogMuteState, TRUE ) for encoded format");
			result = callPluginsInOrder ( kSetAnalogMuteState, TRUE );
			if ( kIOReturnSuccess == result ) {
				//	simplified version of original fix for [3514514], exclusive digital output is not selected so mute the digital section
				if (!( kGPIO_Connected == theAnalogState && kGPIO_TypeIsDigital == comboOutJackTypeState )) {
					newValue = TRUE;
				}
				debugIOLog ( 3, "  selectCodecOutputWithMuteState calling callPluginsInOrder ( kSetDigitalMuteState, %ld ) exclusive dig", newValue);
				result = callPluginsInOrder ( kSetDigitalMuteState, newValue );
			}
		}
	}
Exit:
	debugIOLog ( 3, "- AppleOnboardAudio[%ld]::selectCodecOutputWithMuteState ( %ld ) returns 0x%lX", mInstanceIndex, newValue, result);
    return result;
}

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	[3922678]	Dispatch to virtual space to enable access to protected member variables in the base class.
IOReturn AppleOnboardAudio::inputControlChangeHandler (IOService *target, IOAudioControl *control, SInt32 oldValue, SInt32 newValue)
{
	IOReturn						result = kIOReturnError;
	AppleOnboardAudio *				audioDevice;
	
	audioDevice = OSDynamicCast (AppleOnboardAudio, target);
	FailIf ( 0 == audioDevice, Exit );
	
	result = audioDevice->inputControlChangeHandlerAction ( target, control, oldValue, newValue );
Exit:
	return result;
}


//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	[3922678]	All member variables and methods now directly accessed rather than using a pointer to
//				an AppleOnboardAudio object allows access to protected members of base class.
IOReturn AppleOnboardAudio::inputControlChangeHandlerAction ( IOService *target, IOAudioControl *control, SInt32 oldValue, SInt32 newValue )
{
	IOReturn						result = kIOReturnError;
	IOAudioLevelControl *			levelControl;
	Boolean							wasPoweredDown;

	debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::inputControlChangeHandlerAction (%p, %p, %lX, %lX)", mInstanceIndex, target, control, oldValue, newValue);

	wasPoweredDown = FALSE;
	// We have to make sure the hardware is on before we can send it any control changes [2981190]
	debugIOLog ( 6, "  getPowerState () is %d, pendingPowerState = %d", getPowerState (), pendingPowerState );
	if ( kIOAudioDeviceActive != getPowerState () )
	{
		if ( mDoKPrintfPowerState )
		{
			IOLog ( "AppleOnboardAudio[%ld]::inputControlChangeHandlerAction setting ACTIVE\n", mInstanceIndex );
		}
		FailIf ( kIOReturnSuccess != doLocalChangeToActiveState ( TRUE, &wasPoweredDown ), Exit );
	}

	switch ( control->getType () )
	{
		case kIOAudioControlTypeLevel:
			debugIOLog ( 3, "  control type = kIOAudioControlTypeLevel" );
			levelControl = OSDynamicCast (IOAudioLevelControl, control);

			switch (control->getChannelID ()) {
				case kIOAudioControlChannelIDDefaultLeft:
					debugIOLog ( 3, "  channel ID = kIOAudioControlChannelIDDefaultLeft" );
					result = gainLeftChanged (newValue);
					break;
				case kIOAudioControlChannelIDDefaultRight:
					debugIOLog ( 3, "  channel ID = kIOAudioControlChannelIDDefaultRight" );
					result = gainRightChanged (newValue);
					break;
				case kIOAudioControlChannelIDAll:
					debugIOLog ( 3, "  channel ID = kIOAudioControlChannelIDAll" );
					result = gainRightChanged (newValue);
					break;
			}
			break;
		case kIOAudioControlTypeToggle:
			debugIOLog ( 3, "  control type = kIOAudioControlTypeToggle" );
			result = passThruChanged (newValue);
			break;
		case kIOAudioControlTypeSelector:
			debugIOLog ( 3, "  control->getType() = kIOAudioControlTypeSelector" );
			switch ( control->getSubType () )
			{
				case kIOAudioSelectorControlSubTypeInput:
					debugIOLog ( 3, "  control->getSubType() = kIOAudioSelectorControlSubTypeInput" );
					result = inputSelectorChanged (newValue);
					break;
				case kIOAudioSelectorControlSubTypeClockSource:
					debugIOLog ( 3, "  control->getSubType() = kIOAudioSelectorControlSubTypeClockSource" );
					if ( kClockSourceSelectionExternal == newValue  )
					{
						broadcastSoftwareInterruptMessage ( kRemoteActive ); 
					}
					result = clockSelectorChanged (newValue);
					break;
				default:
					debugIOLog ( 3, "  unknown control type in input selector change handler");
					break;
			}
		default:
			break;
	}
Exit:
	// Second half of [2981190], put us back to sleep after the right amount of time if we were off
	// NOTE:	The 'initialized' is initialized to TRUE by PMInit() which is executed from super::start.
	//			The 'initialized' is a protected member of IOService.
	result = doLocalChangeScheduleIdle ( wasPoweredDown );
	FailMessage ( kIOReturnSuccess != result );

	debugIOLog ( 3, "- AppleOnboardAudio[%ld]::inputControlChangeHandlerAction (%p, %p, oldValue %lX, newValue %lX) returns %lX", mInstanceIndex, target, control, oldValue, newValue, result);
	return result;
}

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleOnboardAudio::gainLeftChanged (SInt32 newValue) {

    debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::gainLeftChanged ( %ld )", mInstanceIndex, newValue);

	if (mCurrentPluginNeedsSoftwareInputGain) {
		mDriverDMAEngine->setInputGainL (newValue);
	} else {
		mCurrentInputPlugin->setInputGain (newValue, mGainRight);
	}
    mGainLeft = newValue;

    debugIOLog ( 3, "- AppleOnboardAudio[%ld]::gainLeftChanged ( %ld )", mInstanceIndex, newValue );
    return kIOReturnSuccess;
}

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleOnboardAudio::gainRightChanged (SInt32 newValue) {

    debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::gainRightChanged ( %ld )", mInstanceIndex, newValue);

	if (mCurrentPluginNeedsSoftwareInputGain) {
		mDriverDMAEngine->setInputGainR (newValue);
	} else {
		mCurrentInputPlugin->setInputGain (newValue, mGainRight);
	}
    mGainRight = newValue;

    debugIOLog ( 3, "- AppleOnboardAudio[%ld]::gainRightChanged ( %ld )", mInstanceIndex, newValue );
    return kIOReturnSuccess;
}

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleOnboardAudio::gainMasterChanged (SInt32 newValue) {

    debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::gainMasterChanged ( %ld )", mInstanceIndex, newValue);

	if (mCurrentPluginNeedsSoftwareInputGain) {
		mDriverDMAEngine->setInputGainR (newValue);
		mDriverDMAEngine->setInputGainL (newValue);
	} else {
		mCurrentInputPlugin->setInputGain (newValue, newValue);
	}

    debugIOLog ( 3, "- AppleOnboardAudio[%ld]::gainMasterChanged ( %ld )", mInstanceIndex, newValue );
    return kIOReturnSuccess;
}

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleOnboardAudio::passThruChanged (SInt32 newValue) {
	IOReturn							result;

    debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::passThruChanged ( %ld )", mInstanceIndex, newValue);

    result = kIOReturnError;

	mCurrentInputPlugin->setPlayThrough (!newValue);

	result = kIOReturnSuccess;

    debugIOLog ( 3, "- AppleOnboardAudio[%ld]::passThruChanged ( %ld )", mInstanceIndex, newValue );
    return kIOReturnSuccess;
}

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleOnboardAudio::inputSelectorChanged (SInt32 newValue) {
	AudioHardwareObjectInterface *		thePluginObject;
	IOAudioStream *						inputStream;
	OSDictionary *						AOApropInput;
	OSBoolean *							softwareInputGainBoolean;
	OSNumber *							inputLatencyNumber;
	char *								connectionString;
	IOReturn							result;
	UInt32								inputLatency;

	debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::inputSelectorChanged (%4s)", mInstanceIndex, (char *)&newValue);

	result = kIOReturnError;
	inputLatency = 0;
	
	connectionString = getConnectionKeyFromCharCode (newValue, kIOAudioStreamDirectionInput);
	FailIf (0 == connectionString, Exit);

	thePluginObject = getPluginObjectForConnection (connectionString);
	//	Dont use the FailIf macro here because an input only instance, such as digital input only, may
	//	not have an input selector control.  Abort without an assertion if no control exists.  (13 Oct 2004 - rbm)
	if ( 0 != thePluginObject )
	{
		// FIX  Make this change as the selection changes.
		inputStream = mDriverDMAEngine->getAudioStream (kIOAudioStreamDirectionInput, 1);
		FailIf (0 == inputStream, Exit);
		inputStream->setTerminalType (getTerminalTypeForCharCode (newValue));

		setUseInputGainControls (connectionString);
		
		setUsePlaythroughControl (connectionString);	// [3250612], this was missing

		setClipRoutineForInput (connectionString);

		setSoftwareInputDSP (connectionString);	// [3306305]
		
		// [3250612], fix update logic regarding current input plugin	
		mCurrentInputPlugin->setInputMute (TRUE);

		setInputDataMuxForConnection ( connectionString );		//  [3743041]
			
		if (mCurrentInputPlugin != thePluginObject) {

			thePluginObject->setInputMute (TRUE);

			// in future this may need to be on a per input basis (which would move this out of this if statement)
			cacheInputGainLevels (mCurrentInputPlugin);
			
			ConfigChangeHelper theConfigeChangeHelper(mDriverDMAEngine);	
		
			// in future may need to update this based on individual inputs, not the part as a whole
			AOApropInput = OSDynamicCast (OSDictionary, thePluginObject->getProperty (kPluginPListAOAAttributes));
			if (0 != AOApropInput) {
				softwareInputGainBoolean = OSDynamicCast (OSBoolean, AOApropInput->getObject (kPluginPListSoftwareInputGain));
				if (0 != softwareInputGainBoolean) {
					mDriverDMAEngine->setUseSoftwareInputGain (softwareInputGainBoolean->getValue ());
					mCurrentPluginNeedsSoftwareInputGain = softwareInputGainBoolean->getValue ();
				} else {
					mDriverDMAEngine->setUseSoftwareInputGain (false);
					mCurrentPluginNeedsSoftwareInputGain = false;
				}
			}

			if (0 != AOApropInput) {
				inputLatencyNumber = OSDynamicCast (OSNumber, AOApropInput->getObject (kPluginPListInputLatency));
				if (0 != inputLatencyNumber) {
					inputLatency = inputLatencyNumber->unsigned32BitValue();
				}
			}
			// [3277271], output latency doesn't change
			mDriverDMAEngine->setSampleLatencies (mOutputLatency, inputLatency);

			mCurrentInputPlugin = thePluginObject;
			debugIOLog ( 3, "  AppleOnboardAudio[%ld]::inputSelectorChanged - mCurrentInputPlugin updated to %p", mInstanceIndex, mCurrentInputPlugin);
		}
		
		mCurrentInputPlugin->setActiveInput (newValue);
		
		AdjustInputGainControls (mCurrentInputPlugin);

		if ((mDetectCollection & kSndHWLineInput) && (kIOAudioInputPortSubTypeLine == newValue)) {
			mCurrentInputPlugin->setInputMute (FALSE);
		} else if (!(mDetectCollection & kSndHWLineInput) && (kIOAudioInputPortSubTypeLine == newValue)) {
			mCurrentInputPlugin->setInputMute (TRUE);
		}

		result = kIOReturnSuccess;
	}

Exit:
	debugIOLog ( 3, "- AppleOnboardAudio[%ld]::inputSelectorChanged (%4s) returns 0x%lX", mInstanceIndex, (char *)&newValue, result);
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

	debugIOLog ( 5, "+ AppleOnboardAudio[%ld]::clockSelectorChanged ('%4s')", mInstanceIndex, (char *)&newValue);

	result = kIOReturnError;

	mClockSelectInProcessSemaphore = true;	//	block 'UNLOCK' errors while switching clock sources
	
	FailIf ( 0 == mDriverDMAEngine, Exit );
	FailIf ( 0 == mTransportInterface, Exit );
	
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
				muteAllAmps ();																			//  [3684994]
				callPluginsInOrder ( kSetMuteState, TRUE );												//	[3435307],[3253678], mute outputs during clock selection
				if ( kClockSourceSelectionInternal == newValue ) {
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
					
					// [4189050]
					// Don't unmute if we think we're going to relock to external clock.  This helps minimize
					// artifacts.
					if ( ( FALSE == mAutoSelectClock ) || ( FALSE == mRelockToExternalClockInProgress ) ) {
						selectCodecOutputWithMuteState (mIsMute);
						if ( 0 != mOutputSelector ) {
							selectOutputAmplifiers ( mOutputSelector->getIntValue (), mIsMute);
						}
					}
					
					//  The 'ConfigChangeHelper' object goes out of scope here which resumes the DMA operation...
				} else if ( kClockSourceSelectionExternal == newValue ) {
					ConfigChangeHelper theConfigeChangeHelper(mDriverDMAEngine, 10);					//  pauses the DBDMA engine
					mTransportInterface->transportBreakClockSelect ( kTRANSPORT_SLAVE_CLOCK );
					callPluginsInOrder ( kBreakClockSelect, kTRANSPORT_SLAVE_CLOCK );
					
					mTransportInterface->transportMakeClockSelect ( kTRANSPORT_SLAVE_CLOCK );
					callPluginsInOrder ( kMakeClockSelect, kTRANSPORT_SLAVE_CLOCK );
					
					selectCodecOutputWithMuteState (mIsMute);
					if ( 0 != mOutputSelector ) {
						selectOutputAmplifiers ( mOutputSelector->getIntValue (), mIsMute);
					}
					
					// [4189050]
					// If a relock to external clock was in progress, the switch to external clock here signifies the
					// completion of relock, so reset the relock flag and poll counter.
					if ( mAutoSelectClock && mRelockToExternalClockInProgress ) {
						mRelockToExternalClockInProgress = FALSE;
						mRelockToExternalClockPollCount = 0;
					}
					
					//  The 'ConfigChangeHelper' object goes out of scope here which resumes the DMA operation...
				} else {
					debugIOLog ( 3,  "  ** Unknown clock source selection." );
					FailIf (TRUE, Exit);
				}
				debugIOLog ( 4, "  ***** updating mCurrentClockSelector from 0x%0.8X to 0x%0.8X", mCurrentClockSelector, newValue );
				mCurrentClockSelector = newValue;
				
				if (kClockSourceSelectionInternal == newValue) {
					mTransportSampleRate.whole = mTransportInterface->transportGetSampleRate ();
					mTransportSampleRate.fraction = 0;
				}
				
				debugIOLog ( 4, "  *-* about to mDriverDMAEngine->hardwareSampleRateChanged ( %d )", mTransportSampleRate.whole );	//  [3686032]
				mDriverDMAEngine->hardwareSampleRateChanged ( &mTransportSampleRate );
				mDriverDMAEngine->updateDSPForSampleRate(mTransportSampleRate.whole);  // [4220086]
			}
			result = kIOReturnSuccess;
		}
	}
Exit:
	mClockSelectInProcessSemaphore = false;	//	enable 'UNLOCK' errors after switching clock sources
	debugIOLog ( 5, "- AppleOnboardAudio[%ld]::clockSelectorChanged ('%4s') returns 0x%lX", mInstanceIndex, (char *)&newValue, result);
	return result;
}

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt32 AppleOnboardAudio::getCurrentSampleFrame (void) {
	return mPlatformInterface->getFrameCount ();
}

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AppleOnboardAudio::setCurrentSampleFrame (UInt32 inValue) {
	mPlatformInterface->setFrameCount (inValue);
	return;
}

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AppleOnboardAudio::setInputDataMuxForConnection ( char * connectionString ) {
	GpioAttributes		theMuxSelect;
	
	theMuxSelect = getInputDataMuxForConnection ( connectionString );
	if ( kGPIO_Unknown != theMuxSelect ) {
		debugIOLog ( 3,  "± AppleOnboardAudio[%ld]::setInputDataMuxForConnection ( %p ) setting input data mux to %d", mInstanceIndex, connectionString, (unsigned int)theMuxSelect );
		mPlatformInterface->setInputDataMux ( theMuxSelect );
	}
}


//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//  [3743041]
void AppleOnboardAudio::notifyStreamFormatsPublished ( void ) {
	IOReturn			result = kIOReturnSuccess;
	
	//  [3743041]   Write the value of the control to the mux GPIO.
	if ( 0 != mInputSelector ) {
		result = mInputSelector->flushValue ();
		debugIOLog ( 3, "  mInputSelector->flushValue () returns 0x%lX", result );
	}
}


#pragma mark +POWER MANAGEMENT
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Have to call super::setAggressiveness to complete the function call
IOReturn AppleOnboardAudio::setAggressiveness ( unsigned long type, unsigned long newLevel )
{
	UInt32					time = 0;
	IOReturn				result = kIOReturnSuccess;

	if (type == kPMPowerSource)
	{
		debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::setAggressiveness ( 0x%0.8X = kPMPowerSource, %ld )", mInstanceIndex, type, newLevel );
		switch ( newLevel ) {
			case kIOPMInternalPower:								// Running on battery only
				mCurrentAggressivenessLevel = newLevel;		//	[3933529]
				//	THE FOLLOWING 'IOLog' IS REQUIRED AND SHOULD NOT BE MOVED.  POWER MANAGEMENT
				//	VERIFICATION CAN ONLY BE PERFORMED USING THE SYSTEM LOG!  AOA Viewer can be 
				//	used to enable or disable kprintf power management logging messages.
				if ( mDoKPrintfPowerState )
				{
					IOLog ( "AppleOnboardAudio[%ld]::setAggressiveness now on BATTERY POWER\n", mInstanceIndex );
				}
				//	Don't schedule deferred idle if playing or recording from any instance!
				if ( 0 == sTotalNumAOAEnginesRunning )		//	[3935620],[3942561]
				{
					debugIOLog ( 3, "  setting power aggressivness state to kIOPMInternalPower" );
					setIdleAudioSleepTime ( kBatteryPowerDownDelayTime );									// tell us about going to the idle state
					debugIOLog ( 6, "  asyncPowerStateChangeInProgress %d", asyncPowerStateChangeInProgress );
					debugIOLog ( 6, "  setting pendingPowerState to kIOAudioDeviceIdle" );
					pendingPowerState = kIOAudioDeviceIdle;
				}
				debugIOLog ( 6, "  setting mCurrentAggressivenessLevel to %ld", newLevel );
				break;
			case kIOPMExternalPower:								// Running on AC power
				mCurrentAggressivenessLevel = newLevel;		//	[3933529]
				//	THE FOLLOWING 'IOLog' IS REQUIRED AND SHOULD NOT BE MOVED.  POWER MANAGEMENT
				//	VERIFICATION CAN ONLY BE PERFORMED USING THE SYSTEM LOG!  AOA Viewer can be 
				//	used to enable or disable kprintf power management logging messages.
				if ( mDoKPrintfPowerState )
				{
					IOLog ( "AppleOnboardAudio[%ld]::setAggressiveness now on EXTERNAL POWER\n", mInstanceIndex );
				}
				debugIOLog ( 3, "  setting power aggressivness state to kIOPMExternalPower" );
				setIdleAudioSleepTime ( kNoIdleAudioPowerDown );										// don't tell us about going to the idle state
				debugIOLog ( 6, "  asyncPowerStateChangeInProgress %d", asyncPowerStateChangeInProgress );
#ifdef THREAD_POWER_MANAGEMENT
				if ( asyncPowerStateChangeInProgress )
				{
					waitForPendingPowerStateChange ();
				}
#endif
				debugIOLog ( 6, "  setting pendingPowerState to kIOAudioDeviceActive" );
				pendingPowerState = kIOAudioDeviceActive;
				FailMessage ( kIOReturnSuccess != performPowerStateChange ( getPowerState (), kIOAudioDeviceActive, &time ) );
				debugIOLog ( 6, "  setting mCurrentAggressivenessLevel to %ld", newLevel );
				break;
			default:
				debugIOLog ( 3, "  setting power aggressivness state to %ld", newLevel );
				break;
		}
		debugIOLog ( 6, "  about to super::setAggressiveness ( %d, %d )", type, newLevel );
		result = super::setAggressiveness ( type, newLevel );
		debugIOLog ( 3, "- AppleOnboardAudio[%ld]::setAggressiveness ( 0x%0.8X = kPMPowerSource, %ld ) returns 0x%lX", mInstanceIndex, type, newLevel, result );
	}
	else
	{
		result = super::setAggressiveness (type, newLevel);
	}
	return result;
}


//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleOnboardAudio::performPowerStateChange ( IOAudioDevicePowerState oldPowerState, IOAudioDevicePowerState newPowerState, UInt32 *microsecondsUntilComplete )
{
	IOReturn					result = kIOReturnError;

	debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::performPowerStateChange ( oldPowerState %d, newPowerState %d, microsecondsUntilComplete %p )", mInstanceIndex, oldPowerState, newPowerState, microsecondsUntilComplete );
	debugIOLog ( 3, "  currentPowerState = %d", getPowerState ());

	FailIf ( 0 == microsecondsUntilComplete, Exit );
	
	//	[3787193]
	//	Initialize to amplifier recovery time as expressed in XML or a minimum delay if no
	//	amplifier recovery time is expressed within the XML dictionary.
	
	if ( 0 == mMicrosecsToSleep )
	{
		if ( 0 != mAmpRecoveryMuteDuration )
		{
			*microsecondsUntilComplete = ( mAmpRecoveryMuteDuration * 1000 );	//	convert mS to µS
		}
		else
		{
			*microsecondsUntilComplete = 1000;
		}
		
		//	Call the plugins to acquire a cumulative time required to complete a power management request.
		//	Each plugin will add its delay to the current delay already expressed in microsecondsUntilComplete.
		
		debugIOLog ( 6, "  seed microsecondsUntilComplete = %ld", *microsecondsUntilComplete );
		callPluginsInOrder ( kRequestSleepTime, (UInt32)microsecondsUntilComplete );
		
		*microsecondsUntilComplete = ( *microsecondsUntilComplete * 3 );		//	Add single instance margin

		//	'mAOAInstanceArray' only exists if there are multiple instances.  If there are multiple instances
		//	then increase the time to include the cumulative time of both instances.
		if ( 0 != mAOAInstanceArray )
		{
			*microsecondsUntilComplete = ( *microsecondsUntilComplete * 3 );	//	Add dual instance margin
		}
	}
	else
	{
		*microsecondsUntilComplete = mMicrosecsToSleep;
	}
	debugIOLog ( 6, "  total microsecondsUntilComplete = %ld", *microsecondsUntilComplete );

	result = super::performPowerStateChange ( oldPowerState, newPowerState, microsecondsUntilComplete );
	FailMessage ( kIOReturnSuccess != result );
    
#ifdef THREAD_POWER_MANAGEMENT
	if ( ( kIOAudioDeviceIdle == getPowerState () ) && ( kIOAudioDeviceActive == newPowerState ) )
	{
		//	This transition is always only in our own AOA / IOAudioFamily context (i.e. we're managing it)
		result = performPowerStateChangeThreadAction ( this, (void*)newPowerState, 0, 0, 0 );
		FailMessage ( kIOReturnSuccess != result );
	}
	else
	{
		//	We're doing threaded calls for system managed power.
		if ( mPowerThread )
		{
			thread_call_enter1 ( mPowerThread, (thread_call_param_t)newPowerState );
		}
	}
#else
	performPowerStateChangeThread ( this, (void*)newPowerState );
#endif
	
Exit:
	debugIOLog ( 3, "  currentPowerState = %d", getPowerState ());
	debugIOLog ( 3, "- AppleOnboardAudio[%ld]::performPowerStateChange ( oldPowerState %d, newPowerState %d, microsecondsUntilComplete %p ) returns 0x%lX", mInstanceIndex, oldPowerState, newPowerState, microsecondsUntilComplete, result );
	return result; 
}

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AppleOnboardAudio::performPowerStateChangeThread (AppleOnboardAudio * aoa, void * newPowerState) {
	IOCommandGate *						cg;
	IOReturn							result;

	FailIf (0 == aoa, Exit);
	debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::performPowerStateChangeThread (%p, %ld)", aoa->mInstanceIndex, aoa, (UInt32)newPowerState);

	aoa->mSignal->wait (FALSE);

	FailWithAction ( TRUE == aoa->mTerminating, aoa->completePowerStateChange (), Exit );	
	cg = aoa->getCommandGate ();
	if (cg)
	{
		result = cg->runAction ( aoa->performPowerStateChangeThreadAction, newPowerState, (void *)aoa );
		FailMessage ( kIOReturnSuccess != result );
	}

	debugIOLog ( 3, "- AppleOnboardAudio[%ld]::performPowerStateChangeThread (%p, %ld)", aoa->mInstanceIndex, aoa, (UInt32)newPowerState );
Exit:
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
IOReturn AppleOnboardAudio::performPowerStateChangeThreadAction ( OSObject * owner, void * newPowerState, void * arg2, void * arg3, void * arg4 )
{
	AppleOnboardAudio *			aoa;
	IOReturn					result = kIOReturnError;

	debugIOLog ( 6, "+ AppleOnboardAudio::performPowerStateChangeThreadAction ( %p, %p, %d, %d, %d )", owner, newPowerState, arg2, arg3, arg4 );
	
	aoa = OSDynamicCast ( AppleOnboardAudio, (OSObject *)owner );
	FailIf ( 0 == aoa, Exit );
	
	result = aoa->performPowerStateAction ( owner, newPowerState, arg2, arg3, arg4 );

Exit:
	debugIOLog ( 6, "- AppleOnboardAudio::performPowerStateChangeThreadAction ( %p, %p, %d, %d, %d ) returns 0x%X", owner, newPowerState, arg2, arg3, arg4, result );
	return result;
}

	
//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleOnboardAudio::performPowerStateAction ( OSObject * owner, void * newPowerState, void * arg2, void * arg3, void * arg4 )
{
	IOReturn					result = kIOReturnError;
	unsigned long				currentAggressivenessLevel = 0;

	debugIOLog ( 6, "+ AppleOnboardAudio[%ld]::performPowerStateAction ( %p, %p, %d, %d, %d )", mInstanceIndex, owner, newPowerState, arg2, arg3, arg4 );
	debugIOLog ( 6, "  AOA[%ld] getPowerState () = %d, pendingPowerState = %d, newPowerState = %lu, sTotalNumAOAEnginesRunning = %lu", mInstanceIndex, getPowerState (), pendingPowerState, (UInt32) newPowerState, sTotalNumAOAEnginesRunning );
	
	FailIf ( 0 == mTransportInterface, Exit );
	FailIf ( 0 == mPlatformInterface, Exit );														//	[3581695]	12 Mar 2004, rbm

	if ( kIOAudioDeviceIdle == (UInt32)newPowerState )
	{
		//	[4048245]	When there are multiple AppleOnboardAudio instances where any instance has
		//	a running engine then no instance should wake to idle.
		if ( 0 != sTotalNumAOAEnginesRunning )
		{
			newPowerState = (void*)kIOAudioDeviceActive;
			if (1 == sInstanceCount) {
				pendingPowerState = kIOAudioDeviceActive;  // see [4151129] comment below
				debugIOLog(3, "AppleOnboardAudio[%ld]::performPowerStateAction - forcing pending power state from idle to active due to 0 != sTotalNumAOAEnginesRunning", mInstanceIndex);
			}
		}
		else if ( kIOReturnSuccess == getAggressiveness ( kPMPowerSource, &currentAggressivenessLevel ) )
		{
			//	[3935596] If waking to idle on external power then convert the request to wake to active
			if ( kIOPMExternalPower == currentAggressivenessLevel )
			{
				newPowerState = (void*)kIOAudioDeviceActive;
				
				// [4151129]
				// By forcing the newPowerState to kIOAudioDeviceActive, AppleOnboardAudio and IOAudioFamily may now have a different
				// notion of the pending power state.  If this happens, i.e. pendingPowerState in IOAudioFamily is not kIOAudioDeviceActive itself,
				// then IOAudioFamily's protectedCompletePowerStateChange routine will set the current power state to the incorrect state.  
				//
				// In the kIOAudioDeviceActive section of AppleOnboardAudio's performPowerStateChangeAction, after IOAudioFamily's protectedCompletePowerStateChange 
				// routine is called, input and output selector values are potentially flushed, which will cause the corresponding change handler routines to fire.  
				// Some of these handlers perform a check to make sure that we are active by calling IOAudioFamily's getPowerState routine.  If IOAudioFamily's
				// notion of the current power state is incorrect, i.e. not active, as previously described, then AppleOnboardAudio's doLocalChangeToActiveState will
				// be called.  This "nested" power state change call to active will cause a potential codec reset which will now occur AFTER we have already unmuted output 
				// (in performPowerStateChangeAction_requestActive), which will cause a pop.  
				//
				// IOAudioFamily's pendingPowerState is a protected member variable, so we can ensure that AppleOnboardAudio and IOAudioFamily have the same notion
				// of the pending power state by setting IOAudioFamily's pendingPowerState to kIOAudioDeviceActive explicitly here.

				/*	[4237314] TEMPORARY FIX - 9/2/2005 - TJG
					Forcing IOAudioFamily's pendingPowerState to active causes rdar://4237314 to occur.  However, not forcing it to active
					here causes rdar://4151129.  Examination of Q78 wake log reveals that forcing of the pending power state to active is not
					the root of the problem.  A summary of the wake log follows:
					  1) Second instance gets performPowerStateChange with IDLE as the new power state.
					  2) Second instance forces the request to ACTIVE since it's on external power.  pendingPowerState is also forced to ACTIVE.
					  3) Second instance issues kRemoteChildActive message via broadcastSoftwareInterruptMessage, which returns FALSE indicating that 
					     no target was found for the message.  This is the culprit.  The other instance should be responding to this.
					  4) Since broadcastSoftwareInterruptMessage didn't do anything, control falls through and protectedCompletePowerStateChange is called 
					     which sets the current power state to the pending power state.  In other words, the second instance now thinks it's active even though 
					     it never executed any of it's code to go active.  This explains why digital in wasn't working after wake from sleep.
					  5) First instance gets performPowerStateChange with IDLE as the new power state.
					  6) First instance forces request to ACTIVE since it's on external power.  pendingPowerState is also forced to ACTIVE.
					  7) First instance calls performPowerStateChangeAction directly.
					  8) First instance goes active.
					  9) First instance issues kRemoteActive message via broadcastSoftwareInterruptMessage, which successfully sends the message to the 
					     second instance.
					 10) Second instance makes a request to go active, which returns immediately because it already thinks it's active.  Here's where forcing 
					     the pending power state makes a difference.  If we don't force the pending power state to active, the protectedCompletePowerStateChange 
					     in step 4 sets the current power state of the second instance to IDLE.  Then in this step, the request to go active would actually go 
					     through.

					Also interesting to note is that the instance index of the second instance is shown as 3 rather than 2 in the log.

					[rdar://4256135] Changed the following conditional to use (1 == sInstanceCount) instead of (mAutoSelectClock) since rdar://4151129 would 
					happen with any single instance PCM3052 machine. /thw

				*/
				if (1 == sInstanceCount) {
					pendingPowerState = kIOAudioDeviceActive;
					debugIOLog(3, "AppleOnboardAudio[%ld]::performPowerStateAction - forcing pending power state from idle to active due to presence of external power", mInstanceIndex);
				}
			}
		}
	}
	
	//  [3515371]   If this AOA instance power managment requests are received from another
	//				AOA instance then indicate success to IOAudioFamily and let the actual
	//				hardware manipulations associated with power management occur under the
	//				control of the controlling AOA instance.
	if ( 0 != mUsesAOAPowerManagement )
	{
		//	[3938771]	IOAudioDevice power management notifications include notifications that fall
		//	outside of the system power states and system power management tree.  These notifications
		//	need to be properly serialized between AppleOnboardAudio instances so that any hardware
		//	sequence dependencies can be properly managed.  If a parent AppleOnboardAudio instance 
		//	receives a sleep or idle request prior to the child AppleOnboardAudio instance then the
		//	child must be notified to sleep or idle prior to the parent entering sleep or idle.
		//	Similarly, if a child receives an active request prior to the parent then the child
		//	must notify the parent to activate (see performPowerStateChangeAction_requestActive).
		switch ( (UInt32)newPowerState )
		{
			case kIOAudioDeviceSleep:		broadcastSoftwareInterruptMessage ( kRemoteChildSleep );		break;
			case kIOAudioDeviceIdle:		broadcastSoftwareInterruptMessage ( kRemoteChildIdle );			break;
			case kIOAudioDeviceActive:		broadcastSoftwareInterruptMessage ( kRemoteChildActive );		break;
		}
		result = kIOReturnSuccess;
	}
	else
	{
		result = performPowerStateChangeAction ( newPowerState );	//	[3752748]	now invokes pre-action for support of multiple power parents
		FailMessage ( kIOReturnSuccess != result );
	}
Exit:
	debugIOLog ( 6, "  AOA[%ld] getPowerState () = %d, pendingPowerState = %d, about to protectedCompletePowerStateChange ()", mInstanceIndex, getPowerState (), pendingPowerState );
	
	protectedCompletePowerStateChange ();
	acknowledgeSetPowerState ();				//	[3931723]
	
	debugIOLog ( 6, "  AOA[%ld] getPowerState () = %d, pendingPowerState = %d", mInstanceIndex, getPowerState (), pendingPowerState );
	debugIOLog ( 6, "- AppleOnboardAudio[%ld]::performPowerStateAction ( %p, %p, %d, %d, %d ) returns 0x%X", mInstanceIndex, owner, newPowerState, arg2, arg3, arg4, result );
	return result;
}


//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleOnboardAudio::performPowerStateChangeAction ( void * newPowerState )
{
	IOReturn			result = kIOReturnError;

	logPerformPowerStateChangeAction ( mInstanceIndex, (UInt32)newPowerState, getPowerState (), kLOG_ENTRY_TO_AOA_METHOD, 0 );

	FailIf ( 0 == mTransportInterface, Exit );
	FailIf ( 0 == mPlatformInterface, Exit );													//	[3581695]	12 Mar 2004, rbm
	
	mNeedsLockStatusUpdateToUnmute = FALSE;														//  [3678605]

	switch ( (UInt32)newPowerState )	//	[3922678]
	{
		case kIOAudioDeviceActive:
			result = performPowerStateChangeAction_requestActive ( TRUE );
			FailIf ( kIOReturnSuccess != result, Exit );
			
			//	Need to complete power state change before flushing controls to prevent
			//	the control change handler from requesting kIOAudioDeviceActive from
			//	within the transition to kIOAudioDeviceActive.
			//   moved above all change handlers for [3942561]
			protectedCompletePowerStateChange ();
			setProperty ( "IOAudioPowerState", kIOAudioDeviceActive, 32 );
			mUCState.ucPowerState = getPowerState ();
			
			FailMessage(kIOAudioDeviceActive != getPowerState());
			
			if ( 0 != mExternalClockSelector )
			{
				debugIOLog ( 4, "  ... [%ld] about to mExternalClockSelector->flushValue ()", mInstanceIndex );
				mExternalClockSelector->flushValue ();
			}

			if ( !mNeedsLockStatusUpdateToUnmute )
			{									//  [3678605]
				FailMessage ( kIOReturnSuccess != selectCodecOutputWithMuteState ( mIsMute ) );

				if ( 0 != mOutputSelector )
				{
					selectOutputAmplifiers ( mOutputSelector->getIntValue (), mIsMute );	//	Radar 3416318:	mOutMuteControl does not touch GPIOs so do so here!
				}

				if ( 0 != mOutMuteControl )
				{
					mOutMuteControl->flushValue ();											// Restore hardware to the user's selected state
				}
			}
			//	THE FOLLOWING 'IOLog' IS REQUIRED AND SHOULD NOT BE MOVED.  POWER MANAGEMENT
			//	VERIFICATION CAN ONLY BE PERFORMED USING THE SYSTEM LOG!  AOA Viewer can be 
			//	used to enable or disable kprintf power management logging messages.
			if ( mDoKPrintfPowerState )
			{
				IOLog ( "AppleOnboardAudio[%ld] power state is ACTIVE\n", mInstanceIndex );
			}
			break;
		case kIOAudioDeviceIdle:
			debugIOLog ( 6, "  AOA[%ld] about to performPowerStateChangeAction_requestIdle", mInstanceIndex );
			result = performPowerStateChangeAction_requestIdle ();
			debugIOLog ( 6, "  AOA[%ld] completed performPowerStateChangeAction_requestIdle, pending %d, current %d", mInstanceIndex, pendingPowerState, getPowerState () );
			FailIf ( kIOReturnSuccess != result, Exit );
			//	THE FOLLOWING 'IOLog' IS REQUIRED AND SHOULD NOT BE MOVED.  POWER MANAGEMENT
			//	VERIFICATION CAN ONLY BE PERFORMED USING THE SYSTEM LOG!  AOA Viewer can be 
			//	used to enable or disable kprintf power management logging messages.
			if ( mDoKPrintfPowerState )
			{
				IOLog ( "AppleOnboardAudio[%ld] power state is IDLE\n", mInstanceIndex );
			}
			protectedCompletePowerStateChange ();
			setProperty ( "IOAudioPowerState", kIOAudioDeviceIdle, 32 );
			mUCState.ucPowerState = getPowerState ();

			break;
		case kIOAudioDeviceSleep:
			result = performPowerStateChangeAction_requestSleep ();
			FailIf ( kIOReturnSuccess != result, Exit );
			//	THE FOLLOWING 'IOLog' IS REQUIRED AND SHOULD NOT BE MOVED.  POWER MANAGEMENT
			//	VERIFICATION CAN ONLY BE PERFORMED USING THE SYSTEM LOG!  AOA Viewer can be 
			//	used to enable or disable kprintf power management logging messages.
			if ( mDoKPrintfPowerState )
			{
				IOLog ( "AppleOnboardAudio[%ld] power state is SLEEP\n", mInstanceIndex );
			}
			protectedCompletePowerStateChange ();
			setProperty ( "IOAudioPowerState", kIOAudioDeviceSleep, 32 );
			mUCState.ucPowerState = getPowerState ();

			break;
	}
	FailIf ( kIOReturnSuccess != result, Exit );

Exit:
	logPerformPowerStateChangeAction ( mInstanceIndex, (UInt32)newPowerState, getPowerState (), kLOG_EXIT_FROM_AOA_METHOD, result );
	return result;
}


//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleOnboardAudio::performPowerStateChangeAction_requestActive ( bool allowDetectIRQDispatch )
{
	IOReturn			result = kIOReturnError;
	
	debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::performPowerStateChangeAction_requestActive () - currentPowerState = %d, pendingPowerState = %d", mInstanceIndex, getPowerState(), pendingPowerState );

	FailIf ( 0 == mTransportInterface, Exit );
	FailIf ( 0 == mPlatformInterface, Exit );
	
	if ( kIOAudioDeviceActive != getPowerState () )
	{
		//	Wake requires that the transport object go active prior
		//	to the hardware plugin object(s) going active.
		result = mTransportInterface->performTransportWake ();
		FailIf ( kIOReturnSuccess != result, Exit );
		
		//  [3515371][3678605]   When going 'Idle' 'Active' then perform remote wakeup immediately 
		//  after waking this instance's transport interface object but prior to performing any
		//  other 'Active' tasks so that another AppleOnboardAudio instance can be made available
		//  as a clock source while making sure that the MCLK signal from this instance's I2S IOM
		//  is available to hardware associated with the remote AppleOnboardAudio instance.
		if ( broadcastSoftwareInterruptMessage ( kRemoteActive ) )
		{				//  [3515371]
			//  If going active and slaved to external clock then defer unmuting the amplifiers
			//  until the AppleOnboardAudio instance that is providing the clock source has indicated
			//  that the clock source is either available or unavailable.  This is done to avoid pops
			//  and clicks during wake from sleep.  [3678605]
			if ( mCurrentClockSelector == kClockSourceSelectionExternal )
			{
				mNeedsLockStatusUpdateToUnmute = TRUE;
			}
		}

		//	[3933529]	combine two arguments into one where:	pendingPowerState = ( newValue & 0x0000FFFF );
		//														currentPowerState = ( newValue >> 16 );
		result = callPluginsInOrder ( kPowerStateChange, ( ( getPowerState () << 16 ) | kIOAudioDeviceActive ) );
		FailIf ( kIOReturnSuccess != result, Exit );

		selectOutputAmplifiers ( mCurrentOutputSelection, mIsMute );
		FailMessage ( kIOReturnSuccess != selectCodecOutputWithMuteState ( mIsMute ) );
		
		// [4216970]
		// If we're running on a single AOA instance and we're automatically locking to external clock, shorten the
		// delay in executing polled interrupts so that codec lock/unlock messages can be received sooner.
		if ((1 == sInstanceCount) && (TRUE == mAutoSelectClock)) {
			mDelayPollAfterWakeFromSleep = kDelayPollAfterWakeFromSleep >> 1;
			debugIOLog(4, "AppleOnboardAudio::performPowerStateChangeAction_requestActive() - number of AOA instances = 1, auto clock select = 1...setting delay poll after wake counter to %lu", mDelayPollAfterWakeFromSleep);
		}
		else {
			mDelayPollAfterWakeFromSleep = kDelayPollAfterWakeFromSleep;  // [3686032]
			debugIOLog(4, "AppleOnboardAudio::performPowerStateChangeAction_requestActive() - number of AOA instances = %lu, auto clock select = %d...setting delay poll after wake counter to %lu", sInstanceCount, mAutoSelectClock, mDelayPollAfterWakeFromSleep);
		}
		
		if ( mAllowDetectIrqDispatchesOnWake && allowDetectIRQDispatch )
		{
			mPlatformInterface->performPowerStateChange ( (IOService*)mPlatformInterface, getPowerState (), pendingPowerState );
		}
		
		if (TRUE == mAutoSelectClock) {
			mDisableAutoSelectClock = TRUE;  // let digital-in jack detect interrupt set this if necessary
		}
		mCodecLockStatus = kClockUnLockStatus;
		
		// [3639956] hp: On machines that can't sense their digital output, output is reset to analog on every wake.
		// Problem is that the state of the connector system is being destroyed on wakeup by calling the jack sense int handlers.
		// Workaround: set output selection to last value set by user.
		if(	(kGPIO_Unknown == mPlatformInterface->getDigitalOutConnected ())	// no sense line for Digital Out
			&& mHasSPDIFControl)												// but there's a Control that knows whether the user liked to do SPDIF
			mOutputSelector->setValue(mOutputSelectorLastValue);
		
		setPollTimer();																	//  [3515371]
		setIdleAudioSleepTime ( kNoIdleAudioPowerDown );
		
		mRemoteDetectInterruptEnabled = TRUE;											//	[3935620],[3942561]
		mRemoteNonDetectInterruptEnabled = TRUE;										//	[3935620],[3942561]
		debugIOLog ( 3, "  AOA[%ld] where mRemoteDetectInterruptEnabled = %d, mRemoteNonDetectInterruptEnabled = %d", mInstanceIndex, mRemoteDetectInterruptEnabled, mRemoteNonDetectInterruptEnabled );
	}
	else
	{
		result = kIOReturnSuccess;
	}
Exit:
	debugIOLog ( 4, "- AppleOnboardAudio[%ld]::performPowerStateChangeAction_requestActive () returns 0x%lX", mInstanceIndex, result );
	return result;
}


//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleOnboardAudio::performPowerStateChangeAction_requestIdle ( void )
{
	IOReturn			result = kIOReturnError;
	
	debugIOLog ( 5, "+ AppleOnboardAudio[%ld]::performPowerStateChangeAction_requestIdle ()", mInstanceIndex );

	FailIf ( 0 == mTransportInterface, Exit );
	FailIf ( 0 == mPlatformInterface, Exit );
	
	mRemoteNonDetectInterruptEnabled = FALSE;										//	[3935620],[3942561]
	debugIOLog ( 6, "  AOA[%ld] where mRemoteDetectInterruptEnabled = %d, mRemoteNonDetectInterruptEnabled = %d", mInstanceIndex, mRemoteDetectInterruptEnabled, mRemoteNonDetectInterruptEnabled );
	if ( kIOAudioDeviceActive ==  getPowerState () )
	{
		//	Transition from kIOAudioDeviceActive to kIOAudioDeviceIdle requires setting the
		//	hardware to the kIOAudioDeviceSleep state while maintaining the interrupt support 
		//	in the kIOAudioDeviceActive state.

		debugIOLog ( 3, "  going from kIOAudioDeviceActive to kAudioDeviceIdle about to stop the 'pollTimer'" );
		pollTimer->cancelTimeout ();												//  [3515371]

		if ( mMuteAmpWhenClockInterrupted )					//	mute the amps
		{
			muteAllAmps();
		}

		FailMessage ( kIOReturnSuccess != selectCodecOutputWithMuteState ( TRUE ) );
		
		if ( !mMuteAmpWhenClockInterrupted )
		{
			muteAllAmps();
		}
		
		//  Switch this instance to internal clock and then send request to sleep to other 
		//	instance prior to puting this instance's transport object to sleep so that the 
		//	MCLK signal from this instance's I2S IOM is present for any hardware attached to
		//  the other instance's I2S IOM.
		if ( kClockSourceSelectionExternal == mCurrentClockSelector )
		{
			if (NULL != mExternalClockSelector) {
				OSNumber * clockSourceSelector = OSNumber::withNumber ( kClockSourceSelectionInternal, 32 );
				
				if (NULL != clockSourceSelector) {
					ConfigChangeHelper theConfigChangeHelper(mDriverDMAEngine);
					
					if (TRUE == mAutoSelectClock) {
						mExternalClockSelector->removeAvailableSelection(kClockSourceSelectionExternal);
						mExternalClockSelector->addAvailableSelection(kClockSourceSelectionInternal, kInternalClockString);
						mExternalClockSelector->hardwareValueChanged(clockSourceSelector);
					}
					else {
						mExternalClockSelector->setValue(clockSourceSelector);
					}
					
					clockSourceSelector->release();
				}
			}
			
			clockSelectorChanged ( kClockSourceSelectionInternal );
			
			// [4196870]
			// Since we're force redirecting to internal clock upon going idle, we must update our notion of the
			// codec lock status to be unlocked so that we properly automatically relock to external clock
			// if a valid external clock is present when waking.
			if (TRUE == mAutoSelectClock) {
				mDisableAutoSelectClock = TRUE;
				mCodecLockStatus = kClockUnLockStatus;
			}
		}

		//	[3933529]	combine two arguments into one where:	pendingPowerState = ( newValue & 0x0000FFFF );
		//														currentPowerState = ( newValue >> 16 );
		result = callPluginsInReverseOrder ( kPowerStateChange, ( ( getPowerState () << 16 ) | kIOAudioDeviceIdle ) );
		FailIf ( kIOReturnSuccess != result, Exit );

		broadcastSoftwareInterruptMessage ( kRemoteIdle );
		
		result = mTransportInterface->performTransportSleep ();
		FailIf ( kIOReturnSuccess != result, Exit );
	}
	else
	{
		//	[3954187]	Give the codec an opportunity to reset when going to idle from sleep
		result = callPluginsInReverseOrder ( kPowerStateChange, ( ( getPowerState () << 16 ) | kIOAudioDeviceIdle ) );
		FailIf ( kIOReturnSuccess != result, Exit );

		broadcastSoftwareInterruptMessage ( kRemoteIdle );
	}
	
	//	Transition from kIOAudioDeviceSleep to kIOAudioDeviceIdle requires setting the
	//	hardware to the kIOAudioDeviceSleep state while setting the interrupt 
	//	support in the kIOAudioDeviceActive state.  Since the hardware is already in
	//	the kIOAudioDeviceSleep state then it is only necessary to enable the
	//	interrupt support.  The detectCollection should be updated and if a change to
	//	the detect collection occurs then the interrupt handler(s) should be invoked
	//	to support redirection of the output port.
	
	result = mPlatformInterface->performPowerStateChange ( (IOService*)mPlatformInterface, getPowerState (), pendingPowerState );
	FailIf ( kIOReturnSuccess != result, Exit );

	mRemoteDetectInterruptEnabled = TRUE;											//	[3935620],[3942561]
	debugIOLog ( 6, "  AOA[%ld] where mRemoteDetectInterruptEnabled = %d, mRemoteNonDetectInterruptEnabled = %d", mInstanceIndex, mRemoteDetectInterruptEnabled, mRemoteNonDetectInterruptEnabled );
Exit:
	debugIOLog ( 5, "- AppleOnboardAudio[%ld]::performPowerStateChangeAction_requestIdle () returns 0x%lX", mInstanceIndex, result );
	return result;
}


//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleOnboardAudio::performPowerStateChangeAction_requestSleep ( void )
{
	IOReturn			result = kIOReturnError;
	
	debugIOLog ( 5, "+ AppleOnboardAudio[%ld]::performPowerStateChangeAction_requestSleep ()", mInstanceIndex );
	debugIOLog ( 3, "  about to stop the 'pollTimer'" );

	FailIf ( 0 == mTransportInterface, Exit );
	FailIf ( 0 == mPlatformInterface, Exit );
	
	mRemoteDetectInterruptEnabled = FALSE;											//	[3935620],[3942561]
	mRemoteNonDetectInterruptEnabled = FALSE;										//	[3935620],[3942561]
	debugIOLog ( 6, "  AOA[%ld] where mRemoteDetectInterruptEnabled = %d, mRemoteNonDetectInterruptEnabled = %d", mInstanceIndex, mRemoteDetectInterruptEnabled, mRemoteNonDetectInterruptEnabled );
	pollTimer->cancelTimeout ();													//  [3515371]

	result = mPlatformInterface->performPowerStateChange ( (IOService*)mPlatformInterface, getPowerState (), pendingPowerState );	//	disable/unregister GPIO interrupts as appropriate
	FailIf ( kIOReturnSuccess != result, Exit );

	//	Sleep requires that the hardware plugin object(s) go to sleep prior to the transport object going to sleep.
	if ( kIOAudioDeviceActive == getPowerState () ) {

		if ( mMuteAmpWhenClockInterrupted )
		{
			muteAllAmps();
		}

		FailMessage ( kIOReturnSuccess != selectCodecOutputWithMuteState ( TRUE ) );
		
		if ( !mMuteAmpWhenClockInterrupted )
		{
			muteAllAmps();
		}
				
		//	[3678605]	begin	{
		//  Switch this instance to internal clock and then send request to sleep to other 
		//  instance prior to puting this instance's transport object to sleep so that the 
		//  MCLK signal from this instance's I2S IOM is present for any hardware attached to
		//  the other instance's I2S IOM.
		if ( kClockSourceSelectionExternal == mCurrentClockSelector )
		{
			if (NULL != mExternalClockSelector) {
				OSNumber * clockSourceSelector = OSNumber::withNumber ( kClockSourceSelectionInternal, 32 );
				
				if (NULL != clockSourceSelector) {
					ConfigChangeHelper theConfigChangeHelper(mDriverDMAEngine);
					
					if (TRUE == mAutoSelectClock) {
						mExternalClockSelector->removeAvailableSelection(kClockSourceSelectionExternal);
						mExternalClockSelector->addAvailableSelection(kClockSourceSelectionInternal, kInternalClockString);
						mExternalClockSelector->hardwareValueChanged(clockSourceSelector);
					}
					else {
						mExternalClockSelector->setValue(clockSourceSelector);
					}
					
					clockSourceSelector->release();
				}
			}
			
			clockSelectorChanged ( kClockSourceSelectionInternal );
			
			// [4196870]
			// Since we're force redirecting to internal clock upon sleep, we must update our notion of the
			// codec lock status to be unlocked so that we properly automatically relock to external clock
			// if a valid external clock is present when waking.
			if (TRUE == mAutoSelectClock) {
				mDisableAutoSelectClock = TRUE;
				mCodecLockStatus = kClockUnLockStatus;
			}
			
			// [4180911]  Need to re-mute here since clockSelectorChanged un-mutes after break-before-make
			// sequence and we don't want to go to sleep un-muted.
			muteAllAmps();
			selectCodecOutputWithMuteState ( TRUE );
		}
	}
	//	[3954187]	Give the codec an opportunity to reset when going to sleep from idle
	//	[3933529]	combine two arguments into one where:	pendingPowerState = ( newValue & 0x0000FFFF );
	//														currentPowerState = ( newValue >> 16 );
	result = callPluginsInReverseOrder ( kPowerStateChange, ( ( getPowerState () << 16 ) | kIOAudioDeviceSleep ) );
	FailIf ( kIOReturnSuccess != result, Exit );

	broadcastSoftwareInterruptMessage ( kRemoteSleep );							//  [3515371]
	//  }   end		[3678605]
	
	if ( kIOAudioDeviceActive == getPowerState () ) {
		result = mTransportInterface->performTransportSleep ();
		FailIf ( kIOReturnSuccess != result, Exit );
	}

Exit:
	debugIOLog ( 5, "- AppleOnboardAudio[%ld]::performPowerStateChangeAction_requestSleep () returns 0x%lX", mInstanceIndex, result );
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
IOReturn AppleOnboardAudio::sysPowerDownHandler (void * target, void * refCon, UInt32 messageType, IOService * provider, void * messageArgument, vm_size_t argSize)
{
	AppleOnboardAudio *				appleOnboardAudio;
	IOReturn						result = kIOReturnUnsupported;

	appleOnboardAudio = OSDynamicCast (AppleOnboardAudio, (OSObject *)target);
	FailIf (0 == appleOnboardAudio, Exit);

	result = appleOnboardAudio->sysPowerDownHandlerAction ( messageType );
	
Exit:
	return result;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn	AppleOnboardAudio::sysPowerDownHandlerAction ( UInt32 messageType )
{
	IOReturn			result = kIOReturnSuccess;
	
	debugIOLog ( 6, "+ AppleOnboardAudio[%ld]::sysPowerDownHandlerAction ( %d )", mInstanceIndex, messageType );
	switch ( messageType ) {
		case kIOMessageSystemWillPowerOff:
		case kIOMessageSystemWillRestart:
			// Interested applications have been notified of an impending power
			// change and have acked (when applicable).
			// This is our chance to save whatever state we can before powering
			// down.
			//	[3950579]
			//	THE FOLLOWING 'IOLog' IS REQUIRED AND SHOULD NOT BE MOVED.  POWER MANAGEMENT
			//	VERIFICATION CAN ONLY BE PERFORMED USING THE SYSTEM LOG!  AOA Viewer can be 
			//	used to enable or disable kprintf power management logging messages.
			if ( mDoKPrintfPowerState )
			{
				IOLog ( "AppleOnboardAudio[%ld]::sysPowerDownHandlerAction setting SLEEP\n", mInstanceIndex );
			}
			pendingPowerState = kIOAudioDeviceSleep;
			result = performPowerStateChangeAction_requestSleep ();
			FailIf ( kIOReturnSuccess != result, Exit );

			protectedCompletePowerStateChange ();
			setProperty ( "IOAudioPowerState", getPowerState (), 32 );
			mUCState.ucPowerState = getPowerState ();

			break;
		default:
			break;
	}
Exit:
	debugIOLog ( 6, "- AppleOnboardAudio[%ld]::sysPowerDownHandlerAction ( %d ) return 0x%X", mInstanceIndex, messageType, result );
	return result;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	[3933529]
IOReturn AppleOnboardAudio::doLocalChangeToActiveState ( bool allowDetectIRQDispatch, Boolean * wasPoweredDown )
{
	IOReturn			result;
	
	debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::doLocalChangeToActiveState ( %d, %p )", mInstanceIndex, wasPoweredDown );

	if (kIOAudioDeviceActive != pendingPowerState) {	// [4046140]

		debugIOLog ( 3, "   about to call performPowerStateChangeAction_requestActive" );

		pendingPowerState = kIOAudioDeviceActive;
		result = performPowerStateChangeAction_requestActive ( allowDetectIRQDispatch );	//	[3922678]
		FailIf ( kIOReturnSuccess != result, Exit );

		protectedCompletePowerStateChange ();
		setProperty ( "IOAudioPowerState", getPowerState (), 32 );
		mUCState.ucPowerState = getPowerState ();

		FailMessage ( kIOReturnSuccess != selectCodecOutputWithMuteState ( mIsMute ) );
		if ( wasPoweredDown )
		{
			*wasPoweredDown = TRUE;
		}
		setPollTimer();												//  [3690065]
	} else {
		debugIOLog ( 3, "   pendingPowerState already = active" );
		result = kIOReturnSuccess;
	}
Exit:

	debugIOLog ( 3, "- AppleOnboardAudio[%ld]::doLocalChangeToActiveState ( ) return 0x%X", mInstanceIndex, result );

	return result;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	[3935202]
IOReturn AppleOnboardAudio::doLocalChangeScheduleIdle ( Boolean wasPoweredDown )
{
	IOReturn			result = kIOReturnSuccess;
	
	if ( TRUE == wasPoweredDown )
	{
		if ( ( kIOPMInternalPower == mCurrentAggressivenessLevel ) && ( 0 == sTotalNumAOAEnginesRunning ) )
		{
			//	THE FOLLOWING 'IOLog' IS REQUIRED AND SHOULD NOT BE MOVED.  POWER MANAGEMENT
			//	VERIFICATION CAN ONLY BE PERFORMED USING THE SYSTEM LOG!  AOA Viewer can be 
			//	used to enable or disable kprintf power management logging messages.
			if ( mDoKPrintfPowerState )
			{
				IOLog ( "AppleOnboardAudio[%ld]::doLocalChangeScheduleIdle ( %d ) priming timer for IDLE\n", mInstanceIndex, wasPoweredDown );
			}

			setIdleAudioSleepTime ( kBatteryPowerDownDelayTime );											// tell us about going to the idle state
			debugIOLog ( 6, "  asyncPowerStateChangeInProgress %d", asyncPowerStateChangeInProgress );
#ifdef THREAD_POWER_MANAGEMENT
			if ( asyncPowerStateChangeInProgress )		//	[3699908]
			{
				waitForPendingPowerStateChange ();
			}
#endif
			debugIOLog ( 6, "  setting pendingPowerState to kIOAudioDeviceIdle" );
			pendingPowerState = kIOAudioDeviceIdle;
		}
	}
	return result;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AppleOnboardAudio::audioEngineStopped ()
{
	debugIOLog ( 6, "+ AppleOnboardAudio[%ld]::audioEngineStopped (), sTotalNumAOAEnginesRunning %d", mInstanceIndex, sTotalNumAOAEnginesRunning );
	if ( ( kIOPMInternalPower == mCurrentAggressivenessLevel ) && ( 1 == sTotalNumAOAEnginesRunning ) )	//	[3935620],[3942561]
	{
		//	THE FOLLOWING 'IOLog' IS REQUIRED AND SHOULD NOT BE MOVED.  POWER MANAGEMENT
		//	VERIFICATION CAN ONLY BE PERFORMED USING THE SYSTEM LOG!  AOA Viewer can be 
		//	used to enable or disable kprintf power management logging messages.
		if ( mDoKPrintfPowerState )
		{
			IOLog ( "AppleOnboardAudio[%ld]::audioEngineStopped scheduling deferred IDLE\n", mInstanceIndex );
		}
		setIdleAudioSleepTime ( kBatteryPowerDownDelayTime );
		debugIOLog ( 6, "  asyncPowerStateChangeInProgress %d", asyncPowerStateChangeInProgress );
#ifdef THREAD_POWER_MANAGEMENT
		if ( asyncPowerStateChangeInProgress )
		{
			waitForPendingPowerStateChange ();
		}
#endif
		debugIOLog ( 6, "  setting pendingPowerState to kIOAudioDeviceIdle" );
		pendingPowerState = kIOAudioDeviceIdle;
	}
	super::audioEngineStopped ();
	if ( 0 != sTotalNumAOAEnginesRunning )
	{
		sTotalNumAOAEnginesRunning--;		//	[3935620],[3942561]
		
	}
	debugIOLog ( 6, "- AppleOnboardAudio[%ld]::audioEngineStopped (), sTotalNumAOAEnginesRunning %d", mInstanceIndex, sTotalNumAOAEnginesRunning );
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//	[3935620],[3942561]
void AppleOnboardAudio::audioEngineStarting ()
{
	debugIOLog ( 6, "+ AppleOnboardAudio[%ld]::audioEngineStarting (), sTotalNumAOAEnginesRunning %d", mInstanceIndex, sTotalNumAOAEnginesRunning );
	sTotalNumAOAEnginesRunning++;
	if ( 1 == sTotalNumAOAEnginesRunning )
	{
		pendingPowerState = kIOAudioDeviceActive;
		
		if (2 == mInstanceIndex) {
			broadcastSoftwareInterruptMessage ( kRemoteChildActive );
		}
		else {
			broadcastSoftwareInterruptMessage ( kRemoteActive );
		}
	}
	super::audioEngineStarting ();
	debugIOLog ( 6, "- AppleOnboardAudio[%ld]::audioEngineStarting (), sTotalNumAOAEnginesRunning %d", mInstanceIndex, sTotalNumAOAEnginesRunning );
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

	debugIOLog ( 3,  "+ AppleOnboardAudio[%ld]::VolumeToPRAMValue ( 0x%X, 0x%X )", mInstanceIndex, (unsigned int)inLeftVol, (unsigned int)inRightVol );
	pramVolume = 0;											//	[2886446]	Always pass zero as a result when muting!!!
	if ( ( 0 != inLeftVol ) || ( 0 != inRightVol ) ) {		//	[2886446]
		leftVol = inLeftVol;
		rightVol = inRightVol;
		if ( 0 != mOutLeftVolumeControl) {
			leftVol -= mOutLeftVolumeControl->getMinValue ();
		}
	
		if (0 != mOutRightVolumeControl) {
			rightVol -= mOutRightVolumeControl->getMinValue ();
		}
		debugIOLog ( 3,  "  ... leftVol = 0x%X, rightVol = 0x%X", (unsigned int)leftVol, (unsigned int)rightVol );
	
		if (0 != mOutMasterVolumeControl) {
			volumeRange = (mOutMasterVolumeControl->getMaxValue () - mOutMasterVolumeControl->getMinValue () + 1);
			debugIOLog ( 3,  "  ... mOutMasterVolumeControl volumeRange = 0x%X", (unsigned int)volumeRange );
		} else if (0 != mOutLeftVolumeControl) {
			volumeRange = (mOutLeftVolumeControl->getMaxValue () - mOutLeftVolumeControl->getMinValue () + 1);
			debugIOLog ( 3,  "  ... mOutLeftVolumeControl volumeRange = 0x%X", (unsigned int)volumeRange );
		} else if (0 != mOutRightVolumeControl) {
			volumeRange = (mOutRightVolumeControl->getMaxValue () - mOutRightVolumeControl->getMinValue () + 1);
			debugIOLog ( 3,  "  ... mOutRightVolumeControl volumeRange = 0x%X", (unsigned int)volumeRange );
		} else {
			volumeRange = kMaximumPRAMVolume;
			debugIOLog ( 3,  "  ... volumeRange = 0x%X **** NO AUDIO LEVEL CONTROLS!", (unsigned int)volumeRange );
		}

		averageVolume = (leftVol + rightVol) >> 1;		// sum the channel volumes and get an average
		debugIOLog ( 3,  "  ... averageVolume = 0x%X", (unsigned int)volumeRange );
		debugIOLog ( 3,  "  ... volumeRange %X, kMaximumPRAMVolume %X", (unsigned int)volumeRange, (unsigned int)kMaximumPRAMVolume );
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
	debugIOLog ( 3,  "- AppleOnboardAudio[%ld]::VolumeToPRAMValue ( 0x%X, 0x%X ) returns 0x%X", mInstanceIndex, (unsigned int)inLeftVol, (unsigned int)inRightVol, (unsigned int)(pramVolume & 0x07) );
	return (pramVolume & 0x07);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
UInt32 AppleOnboardAudio::PRAMToVolumeValue (void) {
	UInt32		 	volumeRange;
	UInt32 			volumeSteps;

	if ( 0 != mOutMasterVolumeControl ) {
		volumeRange = (mOutMasterVolumeControl->getMaxValue () - mOutMasterVolumeControl->getMinValue () + 1);
	} else if ( 0 != mOutLeftVolumeControl ) {
		volumeRange = (mOutLeftVolumeControl->getMaxValue () - mOutLeftVolumeControl->getMinValue () + 1);
	} else if ( 0 != mOutRightVolumeControl ) {
		volumeRange = (mOutRightVolumeControl->getMaxValue () - mOutRightVolumeControl->getMinValue () + 1);
	} else {
		volumeRange = kMaximumPRAMVolume;
	}

	volumeSteps = volumeRange / KNumPramVolumeSteps;	// divide the range by the range of the pramVolume

	return (volumeSteps * ReadPRAMVol ());
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AppleOnboardAudio::WritePRAMVol (UInt32 leftVol, UInt32 rightVol) {
	UInt8						pramVolume;
	UInt8 						curPRAMVol;
	IODTPlatformExpert * 		platform;
	IOReturn					err;
		
	platform = OSDynamicCast(IODTPlatformExpert,getPlatform());
    
    debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::WritePRAMVol ( leftVol=%lu, rightVol=%lu )", mInstanceIndex, leftVol,  rightVol);
    
    if (platform) {
		pramVolume = VolumeToPRAMValue (leftVol, rightVol);
		// get the old value to compare it with
		err = platform->readXPRAM((IOByteCount)kPRamVolumeAddr, &curPRAMVol, (IOByteCount)1);
		if ( kIOReturnSuccess == err ) {
			// Update only if there is a change
			if (pramVolume != (curPRAMVol & 0x07)) {
				// clear bottom 3 bits of volume control byte from PRAM low memory image
				curPRAMVol = (curPRAMVol & 0xF8) | pramVolume;
				// write out the volume control byte to PRAM
				err = platform->writeXPRAM((IOByteCount)kPRamVolumeAddr, &curPRAMVol,(IOByteCount) 1);
				if ( kIOReturnSuccess != err ) {
					debugIOLog ( 3,  "  0x%X = platform->writeXPRAM( 0x%X, & 0x%X, 1 ), value = 0x%X", err, (unsigned int)kPRamVolumeAddr, (unsigned int)&curPRAMVol, (unsigned int)curPRAMVol );
				} else {
					mUCState.ucPramData = (UInt32)curPRAMVol;
					mUCState.ucPramVolume = mUCState.ucPramData & 0x00000007;
				}
			} else {
				debugIOLog ( 3,  "  PRAM write request is to current value: no I/O" );
			}
		} else {
			debugIOLog ( 3,  "  Could not readXPRAM prior to write! Error 0x%X", err );
		}
	} else {
		debugIOLog ( 3,  "  ... no platform" );
	}
    debugIOLog ( 3, "- AppleOnboardAudio[%ld]::WritePRAMVol ( leftVol=%lu, rightVol=%lu )", mInstanceIndex, leftVol,  rightVol);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
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
	
	err = kIOReturnError;
	result = false;
	
	// Create the user client object
	userClientPtr = AppleOnboardAudioUserClient::Create( this, inOwningTask );
	FailIf ( 0 == userClientPtr, Exit );
    
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
	
	FailIf ( 0 == outState, Exit );
	FailIf ( 0 == mPlatformInterface, Exit );
	result = mPlatformInterface->getPlatformState ( (PlatformStateStructPtr)outState );
Exit:
	return result;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleOnboardAudio::getPluginState ( HardwarePluginType thePluginType, void * outState ) {
	AudioHardwareObjectInterface *		thePluginObject;
	IOReturn							result = kIOReturnError;

	debugIOLog ( 7,  "+ AppleOnboardAudio[%ld]::getPluginState ( %d, %p )", mInstanceIndex, thePluginType, outState);
	
	FailIf ( 0 == outState, Exit );
	thePluginObject = findPluginForType ( thePluginType );
	//	No fail messages here please!  The AOA Viewer always queries for plugin modules that may not
	//	be loaded and a fail message will overflow the message log!
	if ( 0 != thePluginObject ) {
		result = thePluginObject->getPluginState ( (HardwarePluginDescriptorPtr)outState );
	}
Exit:
	debugIOLog ( 7,  "- AppleOnboardAudio[%ld]::getPluginState ( %d, %p ) returns 0x%lX", mInstanceIndex, thePluginType, outState, result );
	return result;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleOnboardAudio::getDMAStateAndFormat ( UInt32 arg2, void * outState ) {
	IOReturn			result;
	
	switch ( (DMA_STATE_SELECTOR)arg2 ) {
		case kGetDMAStateAndFormat:			result = mDriverDMAEngine->copyDMAStateAndFormat ( (DBDMAUserClientStructPtr)outState );	break;
		case kGetDMAInputChannelCommands:	result = mDriverDMAEngine->copyInputChannelCommands ( outState );							break;
		case kGetDMAInputChannelCommands1:	result = mDriverDMAEngine->copyInputChannelCommands1 ( outState );							break;
		case kGetDMAOutputChannelCommands:	result = mDriverDMAEngine->copyOutputChannelCommands ( outState );							break;
		case kGetDMAOutputChannelCommands1:	result = mDriverDMAEngine->copyOutputChannelCommands1 ( outState );							break;
		case kGetInputChannelRegisters:		result = mDriverDMAEngine->copyInputChannelRegisters ( outState );							break;
		case kGetOutputChannelRegisters:	result = mDriverDMAEngine->copyOutputChannelRegisters ( outState );							break;
		default:							result = kIOReturnBadArgument;																break;
	}
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleOnboardAudio::setDMAStateAndFormat ( UInt32 arg2, void * inState ) {
	IOReturn			result;
	
	debugIOLog ( 5, "+ AppleOnboardAudio[%ld]::setDMAStateAndFormat( %d, %p )", mInstanceIndex, arg2, inState );
	switch ( (DMA_STATE_SELECTOR)arg2 ) {
		case kSetInputChannelRegisters:		result = mDriverDMAEngine->setInputChannelRegisters ( inState );							break;
		case kSetOutputChannelRegisters:	result = mDriverDMAEngine->setOutputChannelRegisters ( inState );							break;
		case kSetDMAStateAndFormat:			result = mDriverDMAEngine->setDMAStateAndFormat ( (DBDMAUserClientStructPtr)inState );		break;
		default:							result = kIOReturnBadArgument;																break;
	}
	debugIOLog ( 5, "- AppleOnboardAudio[%ld]::setDMAStateAndFormat( %d, %p ) returns %lX", mInstanceIndex, arg2, inState, result );
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleOnboardAudio::getSoftwareProcessingState ( UInt32 arg2, void * outState ) {
#pragma unused ( arg2 )
	IOReturn		result = kIOReturnError;
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleOnboardAudio::getRealTimeCPUUsage ( UInt32 arg2, void * outState ) {
#pragma unused ( arg2 )
	return kIOReturnError;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleOnboardAudio::getAOAState ( UInt32 arg2, void * outState ) {
#pragma unused ( arg2 )
	IOReturn					result;
	const IOAudioEngineStatus *	engineStatusPtr;
	
	result = kIOReturnError;
	FailIf ( 0 != arg2, Exit );
	FailIf (0 == outState, Exit );
	
	debugIOLog ( 7,  "AppleOnboardAudio[%ld]::getAOAState for type %ld", mInstanceIndex, arg2 );

	engineStatusPtr = mDriverDMAEngine->getStatus();
	if ( engineStatusPtr ) {
		((AOAStateUserClientStructPtr)outState)->uc_fVersion = engineStatusPtr->fVersion;
		((AOAStateUserClientStructPtr)outState)->uc_IOAudioEngineStatus_fCurrentLoopCount = engineStatusPtr->fCurrentLoopCount;
		((AOAStateUserClientStructPtr)outState)->uc_IOAudioEngineStatus_fLastLoopTime_hi = engineStatusPtr->fLastLoopTime.hi;
		((AOAStateUserClientStructPtr)outState)->uc_IOAudioEngineStatus_fLastLoopTime_lo = engineStatusPtr->fLastLoopTime.lo;
		((AOAStateUserClientStructPtr)outState)->uc_IOAudioEngineStatus_fEraseHeadSampleFrame = engineStatusPtr->fEraseHeadSampleFrame;
		((AOAStateUserClientStructPtr)outState)->uc_IOAudioEngineState = mDriverDMAEngine->state;
		((AOAStateUserClientStructPtr)outState)->uc_IOAudioEngine_sampleOffset = mDriverDMAEngine->sampleOffset;
		((AOAStateUserClientStructPtr)outState)->uc_IOAudioStreamFormatExtension_fVersion = 0;
		((AOAStateUserClientStructPtr)outState)->uc_IOAudioStreamFormatExtension_fFlags = 0;
		((AOAStateUserClientStructPtr)outState)->uc_IOAudioStreamFormatExtension_fFramesPerPacket = 0;
		((AOAStateUserClientStructPtr)outState)->uc_IOAudioStreamFormatExtension_fBytesPerPacket = 0;
		((AOAStateUserClientStructPtr)outState)->uc_IOAudioEngine_sampleOffset = mDriverDMAEngine->sampleOffset;
	} else {
		((AOAStateUserClientStructPtr)outState)->uc_fVersion = 0;
		((AOAStateUserClientStructPtr)outState)->uc_IOAudioEngineStatus_fCurrentLoopCount = 0;
		((AOAStateUserClientStructPtr)outState)->uc_IOAudioEngineStatus_fLastLoopTime_hi = 0;
		((AOAStateUserClientStructPtr)outState)->uc_IOAudioEngineStatus_fLastLoopTime_lo = 0;
		((AOAStateUserClientStructPtr)outState)->uc_IOAudioEngineStatus_fEraseHeadSampleFrame = 0;
		((AOAStateUserClientStructPtr)outState)->uc_IOAudioEngineState = 0;
		((AOAStateUserClientStructPtr)outState)->uc_IOAudioEngine_sampleOffset = 0;
		((AOAStateUserClientStructPtr)outState)->uc_IOAudioStreamFormatExtension_fVersion = 0;
		((AOAStateUserClientStructPtr)outState)->uc_IOAudioStreamFormatExtension_fFlags = 0;
		((AOAStateUserClientStructPtr)outState)->uc_IOAudioStreamFormatExtension_fFramesPerPacket = 0;
		((AOAStateUserClientStructPtr)outState)->uc_IOAudioStreamFormatExtension_fBytesPerPacket = 0;
		((AOAStateUserClientStructPtr)outState)->uc_IOAudioEngine_sampleOffset = 0;
	}
	((AOAStateUserClientStructPtr)outState)->ucPramData = mUCState.ucPramData;
	((AOAStateUserClientStructPtr)outState)->ucPramVolume = mUCState.ucPramVolume;
	((AOAStateUserClientStructPtr)outState)->ucPowerState = mUCState.ucPowerState;
	((AOAStateUserClientStructPtr)outState)->ucLayoutID = mLayoutID;
	((AOAStateUserClientStructPtr)outState)->uc_mDoKPrintfPowerState = (UInt32)mDoKPrintfPowerState;
	((AOAStateUserClientStructPtr)outState)->uc_sTotalNumAOAEnginesRunning = sTotalNumAOAEnginesRunning;
	((AOAStateUserClientStructPtr)outState)->uc_numRunningAudioEngines = numRunningAudioEngines;
	((AOAStateUserClientStructPtr)outState)->uc_currentAggressivenessLevel = mCurrentAggressivenessLevel;
	result = kIOReturnSuccess;
Exit:
	return result;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleOnboardAudio::getTransportInterfaceState ( UInt32 arg2, void * outState ) {
#pragma unused ( arg2 )
	IOReturn		result = kIOReturnError;
	
	if ( 0 != mTransportInterface ) {
		result = mTransportInterface->getTransportInterfaceState ( (TransportStateStructPtr)outState );
	}
	return result;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleOnboardAudio::setPlatformState ( UInt32 arg2, void * inState ) {
#pragma unused ( arg2 )
	IOReturn		result = kIOReturnError;
	
	FailIf ( 0 == inState, Exit );
	FailIf ( 0 == mPlatformInterface, Exit );
	result = mPlatformInterface->setPlatformState ( (PlatformStateStructPtr)inState );
Exit:
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleOnboardAudio::setPluginState ( HardwarePluginType thePluginType, void * inState ) {
	AudioHardwareObjectInterface *		thePluginObject;
	IOReturn							result = kIOReturnError;
	
	FailIf ( 0 == inState, Exit );
	thePluginObject = findPluginForType ( thePluginType );
	FailIf ( 0 == thePluginObject, Exit );
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
	mDoKPrintfPowerState = (bool)(((AOAStateUserClientStructPtr)inState)->uc_mDoKPrintfPowerState);
	if ( mDoKPrintfPowerState )
	{
		switch ( getPowerState () )
		{
			case kIOAudioDeviceSleep:	IOLog ( "AppleOnboardAudio[%ld] is currently SLEEP\n", mInstanceIndex );		break;
			case kIOAudioDeviceIdle:	IOLog ( "AppleOnboardAudio[%ld] is currently IDLE\n", mInstanceIndex );			break;
			case kIOAudioDeviceActive:	IOLog ( "AppleOnboardAudio[%ld] is currently ACTIVE\n", mInstanceIndex );		break;
		}
	}
	return kIOReturnSuccess;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
IOReturn AppleOnboardAudio::setTransportInterfaceState ( UInt32 arg2, void * inState ) {
#pragma unused ( arg2 )
	IOReturn		result = kIOReturnError;
	
	if ( 0 != mTransportInterface ) {
		result = mTransportInterface->setTransportInterfaceState ( (TransportStateStructPtr)inState );
	}
	return result;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ConfigChangeHelper::ConfigChangeHelper (IOAudioEngine * inEngine, UInt32 inSleep) {
	mDriverDMAEngine = inEngine;
	if (0 != mDriverDMAEngine) {
		UInt32 engineState = mDriverDMAEngine->getState();
		debugIOLog (5, "  ConfigChangeHelper (%p, %ld) - about to try to mDriverDMAEngine->pauseAudioEngine...engine state = %lu", inEngine, inSleep, engineState);
		if ( ( kIOAudioEngineRunning == engineState ) || ( kIOAudioEngineResumed == engineState ) ) {
			mDriverDMAEngine->pauseAudioEngine ();
		}
		
		if (0 != inSleep) {
			debugIOLog ( 4, "  waiting %d ms between pause and begin", inSleep);
			IOSleep ( 10 );
		}
		mDriverDMAEngine->beginConfigurationChange ();
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ConfigChangeHelper::~ConfigChangeHelper () {
	if (0 != mDriverDMAEngine) {
		UInt32 engineState;
		
		mDriverDMAEngine->completeConfigurationChange ();
		engineState = mDriverDMAEngine->getState();
		debugIOLog (5, "  ~ConfigChangeHelper - about to try to mDriverDMAEngine->resumeAudioEngine...engine state = %lu", engineState);
		if ( kIOAudioEnginePaused == engineState ) {
			mDriverDMAEngine->resumeAudioEngine ();
			// [4238699]
			// resumeAudioEngine alone only puts IOAudioEngine in its kIOAudioEngineResumed state.  If an engine was running prior to this pause-resume
			// sequence, it might be possible to keep the engine in a resumed state indefinitely.  This can prevent audioEngineStopped from being called, even
			// if a sound has finished playing.  This is dangerous when running on battery power as it can prevent us from going idle.  By calling startAudioEngine,
			// we force the engine to issue a performAudioEngineStart, and the engine's state is set to running.  This allows audioEngineStopped to be called.
			//
			// Before calling startAudioEngine, check to make sure that the engine is in the resumed state.  This ensures that the audio engine will be started on only
			// the last resume call in cases where pause-resume sequences are nested.
			if ( kIOAudioEngineResumed == mDriverDMAEngine->getState() ) {
				mDriverDMAEngine->startAudioEngine ();
			}
		}
	}
}


#pragma mark +LOGGING UTILITIES

//  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void AppleOnboardAudio::logPerformPowerStateChangeAction ( UInt32 mInstanceIndex, UInt32 newPowerState, UInt32 curPowerState, bool flag, IOReturn resultCode )
{
	if ( kLOG_ENTRY_TO_AOA_METHOD == flag )
	{
		switch ( (UInt32)newPowerState ) {
			case kIOAudioDeviceActive:
				switch ( curPowerState ) {
					case kIOAudioDeviceActive:
						debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::performPowerStateChangeAction ( %ld = kIOAudioDeviceActive ), currentPowerState = %d kIOAudioDeviceActive", mInstanceIndex, (UInt32)newPowerState, getPowerState () );
						break;
					case kIOAudioDeviceIdle:
						debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::performPowerStateChangeAction ( %ld = kIOAudioDeviceActive ), currentPowerState = %d kIOAudioDeviceIdle", mInstanceIndex, (UInt32)newPowerState, getPowerState () );
						break;
					case kIOAudioDeviceSleep:
						debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::performPowerStateChangeAction ( %ld = kIOAudioDeviceActive ), currentPowerState = %d kIOAudioDeviceSleep", mInstanceIndex, (UInt32)newPowerState, getPowerState () );
						break;
					default:
						debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::performPowerStateChangeAction ( %ld = kIOAudioDeviceActive ), currentPowerState = %d", mInstanceIndex, (UInt32)newPowerState, getPowerState () );
						break;
				}
				break;
			case kIOAudioDeviceIdle:
				switch ( getPowerState ()  ) {
					case kIOAudioDeviceActive:
						debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::performPowerStateChangeAction ( %ld = kIOAudioDeviceIdle ), currentPowerState = %d kIOAudioDeviceActive", mInstanceIndex, (UInt32)newPowerState, getPowerState () );
						break;
					case kIOAudioDeviceIdle:
						debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::performPowerStateChangeAction ( %ld = kIOAudioDeviceIdle ), currentPowerState = %d kIOAudioDeviceIdle", mInstanceIndex, (UInt32)newPowerState, getPowerState () );
						break;
					case kIOAudioDeviceSleep:
						debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::performPowerStateChangeAction ( %ld = kIOAudioDeviceIdle ), currentPowerState = %d kIOAudioDeviceSleep", mInstanceIndex, (UInt32)newPowerState, getPowerState () );
						break;
					default:
						debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::performPowerStateChangeAction ( %ld = kIOAudioDeviceIdle ), currentPowerState = %d", mInstanceIndex, (UInt32)newPowerState, getPowerState () );
						break;
				}
				break;
			case kIOAudioDeviceSleep:
				switch ( getPowerState ()  ) {
					case kIOAudioDeviceActive:
						debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::performPowerStateChangeAction ( %ld = kIOAudioDeviceSleep ), currentPowerState = %d kIOAudioDeviceActive", mInstanceIndex, (UInt32)newPowerState, getPowerState () );
						break;
					case kIOAudioDeviceIdle:
						debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::performPowerStateChangeAction ( %ld = kIOAudioDeviceSleep ), currentPowerState = %d kIOAudioDeviceIdle", mInstanceIndex, (UInt32)newPowerState, getPowerState () );
						break;
					case kIOAudioDeviceSleep:
						debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::performPowerStateChangeAction ( %ld = kIOAudioDeviceSleep ), currentPowerState = %d kIOAudioDeviceSleep", mInstanceIndex, (UInt32)newPowerState, getPowerState () );
						break;
					default:
						debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::performPowerStateChangeAction ( %ld = kIOAudioDeviceSleep ), currentPowerState = %d", mInstanceIndex, (UInt32)newPowerState, getPowerState () );
						break;
				}
				break;
			default:
				switch ( currentPowerState ) {
					case kIOAudioDeviceActive:
						debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::performPowerStateChangeAction ( %ld ), currentPowerState = %d kIOAudioDeviceActive", mInstanceIndex, (UInt32)newPowerState, currentPowerState);
						break;
					case kIOAudioDeviceIdle:
						debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::performPowerStateChangeAction ( %ld ), currentPowerState = %d kIOAudioDeviceIdle", mInstanceIndex, (UInt32)newPowerState, currentPowerState);
						break;
					case kIOAudioDeviceSleep:
						debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::performPowerStateChangeAction ( %ld ), currentPowerState = %d kIOAudioDeviceSleep", mInstanceIndex, (UInt32)newPowerState, currentPowerState);
						break;
					default:
						debugIOLog ( 3, "+ AppleOnboardAudio[%ld]::performPowerStateChangeAction ( %ld ), currentPowerState = %d", mInstanceIndex, (UInt32)newPowerState, currentPowerState);
						break;
				}
				break;
		}
	}
	else if ( kLOG_EXIT_FROM_AOA_METHOD == flag )
	{
		switch ( (UInt32)newPowerState ) {
			case kIOAudioDeviceActive:
				switch ( currentPowerState ) {
					case kIOAudioDeviceActive:
						debugIOLog ( 3, "- AppleOnboardAudio[%ld]::performPowerStateChangeAction ( %ld = kIOAudioDeviceActive ), currentPowerState = %d kIOAudioDeviceActive, returns 0x%0.8X", mInstanceIndex, (UInt32)newPowerState, currentPowerState, resultCode);
						break;
					case kIOAudioDeviceIdle:
						debugIOLog ( 3, "- AppleOnboardAudio[%ld]::performPowerStateChangeAction ( %ld = kIOAudioDeviceActive ), currentPowerState = %d kIOAudioDeviceIdle, returns 0x%0.8X", mInstanceIndex, (UInt32)newPowerState, currentPowerState, resultCode);
						break;
					case kIOAudioDeviceSleep:
						debugIOLog ( 3, "- AppleOnboardAudio[%ld]::performPowerStateChangeAction ( %ld = kIOAudioDeviceActive ), currentPowerState = %d kIOAudioDeviceSleep, returns 0x%0.8X", mInstanceIndex, (UInt32)newPowerState, currentPowerState, resultCode);
						break;
					default:
						debugIOLog ( 3, "- AppleOnboardAudio[%ld]::performPowerStateChangeAction ( %ld = kIOAudioDeviceActive ), currentPowerState = %d, returns 0x%0.8X", mInstanceIndex, (UInt32)newPowerState, currentPowerState, resultCode);
						break;
				}
				break;
			case kIOAudioDeviceIdle:
				switch ( currentPowerState ) {
					case kIOAudioDeviceActive:
						debugIOLog ( 3, "- AppleOnboardAudio[%ld]::performPowerStateChangeAction ( %ld = kIOAudioDeviceIdle ), currentPowerState = %d kIOAudioDeviceActive, returns 0x%0.8X", mInstanceIndex, (UInt32)newPowerState, currentPowerState, resultCode);
						break;
					case kIOAudioDeviceIdle:
						debugIOLog ( 3, "- AppleOnboardAudio[%ld]::performPowerStateChangeAction ( %ld = kIOAudioDeviceIdle ), currentPowerState = %d kIOAudioDeviceIdle, returns 0x%0.8X", mInstanceIndex, (UInt32)newPowerState, currentPowerState, resultCode);
						break;
					case kIOAudioDeviceSleep:
						debugIOLog ( 3, "- AppleOnboardAudio[%ld]::performPowerStateChangeAction ( %ld = kIOAudioDeviceIdle ), currentPowerState = %d kIOAudioDeviceSleep, returns 0x%0.8X", mInstanceIndex, (UInt32)newPowerState, currentPowerState, resultCode);
						break;
					default:
						debugIOLog ( 3, "- AppleOnboardAudio[%ld]::performPowerStateChangeAction ( %ld = kIOAudioDeviceIdle ), currentPowerState = %d, returns 0x%0.8X", mInstanceIndex, (UInt32)newPowerState, currentPowerState, resultCode);
						break;
				}
				break;
			case kIOAudioDeviceSleep:
				switch ( currentPowerState ) {
					case kIOAudioDeviceActive:
						debugIOLog ( 3, "- AppleOnboardAudio[%ld]::performPowerStateChangeAction ( %ld = kIOAudioDeviceSleep ), currentPowerState = %d kIOAudioDeviceActive, returns 0x%0.8X", mInstanceIndex, (UInt32)newPowerState, currentPowerState, resultCode);
						break;
					case kIOAudioDeviceIdle:
						debugIOLog ( 3, "- AppleOnboardAudio[%ld]::performPowerStateChangeAction ( %ld = kIOAudioDeviceSleep ), currentPowerState = %d kIOAudioDeviceIdle, returns 0x%0.8X", mInstanceIndex, (UInt32)newPowerState, currentPowerState, resultCode);
						break;
					case kIOAudioDeviceSleep:
						debugIOLog ( 3, "- AppleOnboardAudio[%ld]::performPowerStateChangeAction ( %ld = kIOAudioDeviceSleep ), currentPowerState = %d kIOAudioDeviceSleep, returns 0x%0.8X", mInstanceIndex, (UInt32)newPowerState, currentPowerState, resultCode);
						break;
					default:
						debugIOLog ( 3, "- AppleOnboardAudio[%ld]::performPowerStateChangeAction ( %ld = kIOAudioDeviceSleep ), currentPowerState = %d, returns 0x%0.8X", mInstanceIndex, (UInt32)newPowerState, currentPowerState, resultCode);
						break;
				}
				break;
			default:
				switch ( currentPowerState ) {
					case kIOAudioDeviceActive:
						debugIOLog ( 3, "- AppleOnboardAudio[%ld]::performPowerStateChangeAction ( %ld ), currentPowerState = %d kIOAudioDeviceActive, returns 0x%0.8X", mInstanceIndex, (UInt32)newPowerState, currentPowerState, resultCode);
						break;
					case kIOAudioDeviceIdle:
						debugIOLog ( 3, "- AppleOnboardAudio[%ld]::performPowerStateChangeAction ( %ld ), currentPowerState = %d kIOAudioDeviceIdle, returns 0x%0.8X", mInstanceIndex, (UInt32)newPowerState, currentPowerState, resultCode);
						break;
					case kIOAudioDeviceSleep:
						debugIOLog ( 3, "- AppleOnboardAudio[%ld]::performPowerStateChangeAction ( %ld ), currentPowerState = %d kIOAudioDeviceSleep, returns 0x%0.8X", mInstanceIndex, (UInt32)newPowerState, currentPowerState, resultCode);
						break;
					default:
						debugIOLog ( 3, "- AppleOnboardAudio[%ld]::performPowerStateChangeAction ( %ld ), currentPowerState = %d, returns 0x%0.8X", mInstanceIndex, (UInt32)newPowerState, currentPowerState, resultCode);
						break;
				}
				break;
		}
	}
}


