/*
 * Copyright (c) 2002-2004 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Copyright (c) 2002-2004 Apple Computer, Inc.  All rights reserved.
 *
 *
 */


#include "IOPlatformPluginSymbols.h"
//#include <ppc/machine_routines.h>
#include "PBG4_PlatformPlugin.h"

// Global pointer to our plugin for reference by sensors and control loops
PBG4_PlatformPlugin				*gPlatformPlugin;

const OSSymbol					*gIOPluginEnvStepperDataLoadRequest,
								*gIOPluginEnvStepControlState;

// Uncomment to enable listing contents of dictionaries
//#define PLUGIN_DEBUG_DICT

#ifdef PLUGIN_DEBUG_DICT
//*********************************************************************************
// printDictionaryKeys
//
// Print the keys for the given dictionary for diagnostic purposes
//*********************************************************************************
static void printDictionaryKeys (OSDictionary * inDictionary, char * inMsg)
{
#ifdef PLUGIN_DEBUG
	OSCollectionIterator * mcoll = OSCollectionIterator::withCollection (inDictionary);
	OSSymbol * mkey;
 
	mcoll->reset ();

	mkey = OSDynamicCast (OSSymbol, mcoll->getNextObject ());
	
	if (!mkey) {
		DLOG ("No objects found in dictionary '%s'\n",  inMsg);
		return;
	}
	
	DLOG ("Listing objects in dictionary '%s':\n", inMsg);
	
	while (mkey) {
		DLOG ("    '%s'\n", mkey->getCStringNoCopy());

		mkey = (OSSymbol *) mcoll->getNextObject ();

  }

  mcoll->release ();
  
#endif	// PLUGIN_DEBUG
  return;
}
#endif	// PLUGIN_DEBUG_DICT

#define super IOPlatformPlugin
OSDefineMetaClassAndStructors(PBG4_PlatformPlugin, IOPlatformPlugin)

bool PBG4_PlatformPlugin::start( IOService * provider )
{

	DLOG("PBG4_PlatformPlugin::start - entered\n");
	
	gPlatformPlugin = this;			// Let associated classes find us

	// See if we need to use PowerPlay for graphics
	fUsePowerPlay = (provider->getProperty ("UsePowerPlay") != NULL);
			
	if (!super::start(provider)) return(false);

	nub = provider;
	
	// set the platform ID
	if (gIOPPluginPlatformID)
		gIOPPluginPlatformID->release();

	gIOPPluginPlatformID = OSSymbol::withCString("PBG4");

	if ( thermalProfile != NULL ) {
		thermalProfile->adjustThermalProfile();

		// Tear down the nub (which also terminates thermalProfile)
		thermalNub->terminate();
		thermalNub = NULL;
		thermalProfile = NULL;
	}
		
	// These will get set correctly the first time we get an environmental interrupt
	setEnv(gIOPPluginEnvACPresent, kOSBooleanFalse);
	setEnv(gIOPPluginEnvBatteryPresent, kOSBooleanFalse);
	setEnv(gIOPPluginEnvBatteryOvercurrent, kOSBooleanFalse);  
	setEnv(gIOPPluginEnvClamshellClosed, kOSBooleanFalse);
	
	// Register for battery current interrupts
	// Consider adding a capability check here, if we ship any G4 Powerbooks without the Battery Current sensor
	// Right now, this plugin only supports  Q16b and Q41b. Both of these machines have the sensor, so no check
	//  is performed.
	IOService *PMUdriver = waitForService(serviceMatching("ApplePMU"));

	// Register for the interrupt:
	PMUdriver->callPlatformFunction("registerForPMUInterrupts", true, (void*) (kPMUEnvironmentIntBit), 
		(void*)handleEnvironmentalInterruptEvent, (void*)this, NULL);

	DLOG("PBG4_PlatformPlugin::start - done\n");
	return true;
}

/*******************************************************************************
 * Method:
 *	initCtrlLoops
 *
 ******************************************************************************/
// virtual
bool PBG4_PlatformPlugin::initCtrlLoops( const OSArray * ctrlLoopDicts )
{
	OSNumber			*flag;
	OSDictionary		*dict;
	int					count, i;
	bool				result;
	
	if (result = super::initCtrlLoops (ctrlLoopDicts)) {
		// Look for our own properties
		if (ctrlLoopDicts == NULL) {
			// this is not a fatal error
			DLOG("IOPlatformPlugin::initCtrlLoops no ctrlloop array\n");
		} else {
			count = ctrlLoopDicts->getCount();
			for (i = 0; i < count; i++) {
				// grab the array element
				if ((dict = OSDynamicCast(OSDictionary, ctrlLoopDicts->getObject(i))) == NULL) {
					DLOG("IOPlatformPlugin::initCtrlLoops error parsing ctrlLoopDicts element %d, skipping\n", i);
					continue;
				}
				
#ifdef PLUGIN_DEBUG_DICT
				char s[64];
				
				sprintf (s, "ctrlLoopDict[%d]", i);
				printDictionaryKeys (dict, s);
#endif
				if (flag = OSDynamicCast(OSNumber, dict->getObject(kCtrlLoopIsStateDrivenKey))) {
					//DLOG ("PBG4_PlatformPlugin::initCtrlLoops - got state driven flag '%s' for loop %d\n",
						//(flag->unsigned32BitValue() != 0) ? "true" : "false", i);
					fCtrlLoopStateFlagsArray[i] = (flag->unsigned32BitValue() != 0);
				}
			}
		}
	}
	return result;
}

UInt8 PBG4_PlatformPlugin::probeConfig( void )
{
	char											thermalProfilePrefix[ 128 ] = "ThermalProfile_";
	OSDictionary*									thermalNubDict;
	OSString*										modelString;
	OSString*										name;
	UInt8											config = 0;
	IOService										*service;

	DLOG("PBG4_PlatformPlugin::probeConfig - entered\n");
	if ( ( thermalNubDict = OSDictionary::withCapacity( 1 ) ) == NULL )
		return( 0 );

	if ( ( modelString = OSDynamicCast( OSString, getProvider()->getProperty( "model" ) ) ) == NULL )
		return( 0 );

	strcat( thermalProfilePrefix, modelString->getCStringNoCopy() );
	name = OSString::withCString( thermalProfilePrefix );

	// By using OSString or OSSymbol and setting the name as IOName, we avoid
	// overriding compareName.

	thermalNubDict->setObject( "IOName", name );

	if ( ( ( thermalNub = new IOService ) == NULL ) || ( !thermalNub->init( thermalNubDict ) ) )
		return( 0 );

	thermalNub->attach( this );
	thermalNub->start( this );
	thermalNub->registerService( kIOServiceSynchronous );

	// Get the dictionary from the nub.  What do we do if we couldn't find the thermal profile?
	if (!(service = thermalNub->getClient())) {
		DLOG("PBG4_PlatformPlugin::probeConfig - no thermal nub client\n");
	} else if ( ( thermalProfile = OSDynamicCast( IOPlatformPluginThermalProfile, thermalNub->getClient() ) ) != NULL ) {
		config = thermalProfile->getThermalConfig();
		removeProperty( kIOPPluginThermalProfileKey );
		setProperty( kIOPPluginThermalProfileKey, thermalProfile->copyProperty( kIOPPluginThermalProfileKey ) );
	} else
		DLOG("PBG4_PlatformPlugin::probeConfig - thermalProfile not found\n");


	thermalNubDict->release();
	name->release();

	DLOG("PBG4_PlatformPlugin::probeConfig - done, returning config %d\n", config);
	return( config );
}

// **********************************************************************************
// setAggressiveness
//
//			-- We override the superclass on this to filter out unwanted selectors
//
// **********************************************************************************
IOReturn PBG4_PlatformPlugin::setAggressiveness(unsigned long selector, unsigned long newLevel)
{
	/*
	 * Check here if this is a selector we care about - the only one of interest being kPMSetProcessorSpeed
	 * This is very important because the PBG4_DPSCtrlLoop issues a broadcast setAggressiveness through
	 * the pmRootDomain.  That call comes back here.  Without this check we attempt to dispatch
	 * a second setAggressiveness message while the first one is pending.  dispatchEvent does not 
	 * block this because it uses an IORecursiveLock and we are still on the same thread so dispatchEvent
	 * does not block and the CtrlLoop gets re-entered.
	 */
	if (selector != kPMSetProcessorSpeed)
		return IOPMNoErr;			// Filter out unwanted messages

	// Carry on...
    return super::setAggressiveness(selector, newLevel);
}

/*
 * PBG4_PlatformPlugin::environmentChanged - this is a complete override of
 * IOPlatformPlugin::environmentChanged - see notes below.
 */
void PBG4_PlatformPlugin::environmentChanged( void )
{
	IOPlatformCtrlLoop *loop;
	int i, count;

	// let the control loops update their meta state and, if necessary, adjust their controls
	if (ctrlLoops)
	{
		count = ctrlLoops->getCount();
		for (i=0; i<count; i++)
		{
			if ((loop = OSDynamicCast(IOPlatformCtrlLoop, ctrlLoops->getObject(i))) != NULL)
			{
				/*
				 * This differs from IOPlatformPlugin::environmentChanged in that not all
				 * our control loops need both updateMetaState and adjustControls called.
				 * In particular, state driven control loops, like PBG4_DPSCtrlLoop just
				 * have their adjustControls() routine turn around and call updateMetaState().
				 * So, calling both is redundant.
				 */
				DLOG("PBG4_PlatformPlugin::enviromentChanged notifying ctrlLoop %d\n", i);
				loop->updateMetaState();
				// Only call adjustControls if loop is *not* state driven
				if (!fCtrlLoopStateFlagsArray[i]) {
					DLOG("PBG4_PlatformPlugin::enviromentChanged also calling adjustControls %d\n", i);
					loop->adjustControls();
				}
			}
		}
	}
	return;
}

// **********************************************************************************
// delEnv
//
//          -- delete an environmental object - this should be moved to superclass 
//
// **********************************************************************************
void PBG4_PlatformPlugin::delEnv (const OSSymbol *aKey)
{
	envInfo->removeObject (aKey);
	return;
}

// **********************************************************************************
// initSymbols
//
//          -- init symbols used by this plugin 
//
// **********************************************************************************
void PBG4_PlatformPlugin::initSymbols( void )
{
	gIOPluginEnvStepperDataLoadRequest		= OSSymbol::withCString (kIOPluginEnvStepperDataLoadRequest);
	gIOPluginEnvStepControlState			= OSSymbol::withCString (kIOPluginEnvStepControlState);
	
	super::initSymbols ();		// Let our parent do the same
	return;
}

IOReturn PBG4_PlatformPlugin::loadStepDataEventDispatch (UInt32 baseTableAddr, UInt32 baseTableLength, UInt32 secondaryTableAddr, UInt32 secondaryTableLength)
{
	LoadDataStruct		loadData;
	IOPPluginEventData	event;
	
	// Don't allow loading new data in a safe boot.  Stepping behavior is constrained and probably not what user expects
	if (safeBoot)
		return kIOReturnError;
	
	// Pass along user data
	loadData.baseTableAddr			= baseTableAddr;
	loadData.baseTableLength		= baseTableLength;
	loadData.secondaryTableAddr		= secondaryTableAddr;
	loadData.secondaryTableLength	= secondaryTableLength;
	
	// dispatch a (serialized) event
	event.eventType = IOPPluginEventMisc;
	event.param1 = (void *)this;				// Pass self
	event.param2 = (void *)&loadData;			// Pass user data
	event.param3 = (void *)NULL;				// Unused param
	event.param4 = (void *)PBG4_PlatformPlugin::loadStepDataHandler;

	DLOG("PBG4_PluginUserClient::loadStepDataEventDispatch - dispatching data\n");
	
	return (dispatchEvent (&event));
}

// **********************************************************************************
// loadStepDataHandler
//
//
// **********************************************************************************
/* static */
IOReturn PBG4_PlatformPlugin::loadStepDataHandler ( void * p1, void * p2, void * p3 )
{
	PBG4_PlatformPlugin		*localThis;

	DLOG("PBG4_PlatformPlugin::loadStepDataHandler - entered\n");
	
	localThis = OSDynamicCast(PBG4_PlatformPlugin, (OSObject *)p1);
    if (localThis == NULL)
        return kIOReturnBadArgument;
	
	return localThis->loadStepData ((LoadDataStruct *) p2);
}

// **********************************************************************************
// loadStepData
//
//
// **********************************************************************************
IOReturn PBG4_PlatformPlugin::loadStepData (LoadDataStruct *userStepData)
{
	OSData				*baseTableData;
	UInt8				*baseTablePtr;
	UInt32				bytesRead;
	IOMemoryDescriptor	*baseTableMemDesc;
	//IOMemoryDescriptor	*secondaryTableMemDesc;		// currently unused
	IOReturn			status;

	DLOG("PBG4_PlatformPlugin::loadStepData - entered\n");
	
	status = kIOReturnBadArgument;
	if (userStepData && (userStepData->baseTableAddr != NULL)) {
		
		// Map in user memory
		baseTableMemDesc = IOMemoryDescriptor::withAddress((vm_address_t)userStepData->baseTableAddr, userStepData->baseTableLength, 
			kIODirectionOut, fTask);

		if (!baseTableMemDesc)
			return kIOReturnNoResources;
		
		baseTablePtr = (UInt8 *)IOMalloc (userStepData->baseTableLength);
		if (!baseTablePtr)
			return kIOReturnNoMemory;
		
		if (baseTableMemDesc->prepare() == kIOReturnSuccess) {
			// read the data from user buffer
			bytesRead = baseTableMemDesc->readBytes (0, (void *)baseTablePtr, userStepData->baseTableLength);
			
			if (bytesRead == userStepData->baseTableLength) {
				baseTableData = OSData::withBytes (baseTablePtr, userStepData->baseTableLength);
				if (baseTableData != NULL) {
					// Update property in registry
					setProperty(gIOPluginEnvStepperDataLoadRequest, (OSObject *)baseTableData);
					// And notify clients
					setEnv (gIOPluginEnvStepperDataLoadRequest, kOSBooleanTrue);
					
					status = kIOReturnSuccess;
				}
			}
			
			baseTableMemDesc->complete();
		}
		baseTableMemDesc->release();
		IOFree (baseTablePtr, userStepData->baseTableLength);
		
		// xxx repeat this process with secondary table data when available
		// secondaryTableMemDesc is unused until then
	}
	
	return status;
}

// **********************************************************************************
// stepperControlEventDispatch
//
//
// **********************************************************************************
IOReturn PBG4_PlatformPlugin::stepperControlEventDispatch (UInt32 stepLevel)
{
	IOPPluginEventData	event;
	
	// Don't allow changing state in a safe boot.
	if (safeBoot)
		return kIOReturnError;
	
	// dispatch a (serialized) event
	event.eventType = IOPPluginEventMisc;
	event.param1 = (void *)this;				// Pass self
	event.param2 = (void *)stepLevel;			// Pass user data
	event.param3 = (void *)NULL;				// Unused param
	event.param4 = (void *)PBG4_PlatformPlugin::stepperControlHandler;

	DLOG("PBG4_PluginUserClient::loadStepDataEventDispatch - dispatching data\n");
	
	return (dispatchEvent (&event));
}

// **********************************************************************************
// stepperControlHandler
//
//
// **********************************************************************************
/* static */
IOReturn PBG4_PlatformPlugin::stepperControlHandler ( void * p1, void * p2, void * p3 )
{
	PBG4_PlatformPlugin		*localThis;
	UInt32					stepLevel;
	OSNumber				*stepNum;

	DLOG("PBG4_PlatformPlugin::stepperControlHandler - entered\n");
	
	localThis = OSDynamicCast(PBG4_PlatformPlugin, (OSObject *)p1);
    if (localThis == NULL)
        return kIOReturnBadArgument;
	
	stepLevel = (UInt32)p2;
	
	stepNum = OSNumber::withNumber( (unsigned long long) stepLevel, 32);
	localThis->setEnv (gIOPluginEnvStepControlState, stepNum);
	
	return kIOReturnSuccess;
}

// **********************************************************************************
// handleEnvironmentalInterruptEvent
//
//          -- Currently just called as an interrupt handler
//				- this call is unsynchronized
//
// **********************************************************************************
/* static */
void PBG4_PlatformPlugin::handleEnvironmentalInterruptEvent(IOService *client, UInt8 interruptMask, UInt32 length, UInt8 *buffer)
{
    PBG4_PlatformPlugin		*localThis;
	UInt32					changedIntBits, envIntData, envRawData;
	
    // Verify that we are receiving the correct interrupt (probably unnecessary(?)):
    if (!(interruptMask & kPMUEnvironmentIntBit))
		return;
	
    // Was this one meant for us?
	localThis = OSDynamicCast(PBG4_PlatformPlugin, client);
    if (localThis == NULL)
        return;
	
	// Retrieve the data (little-endian) and mask off bits we don't care about
	envRawData = OSReadSwapInt16(buffer, 0);
    envIntData = envRawData & kPMUEnvIntMask;
	
	if (!localThis->fEnvDataIsValid) {
		changedIntBits = kPMUEnvIntMask;		// First time through, mark everything as changed
		localThis->fEnvDataIsValid = true;
		DLOG("PBG4_PlatformPlugin::handleEnvironmentalInterruptEvent - initial raw data 0x%x, masked data 0x%x\n",
			envRawData, envIntData);
	} else
		changedIntBits = envIntData ^ localThis->fLastPMUEnvIntData;
	
	if (changedIntBits != 0) {
		// Only dispatch the event if something changed
		IOPPluginEventData event;
	
		// dispatch a (serialized) event
		event.eventType = IOPPluginEventMisc;
		event.param1 = (void *)localThis;			// Pass object reference
		event.param2 = (void *)envIntData;			// Pass current data
		event.param3 = (void *)changedIntBits;		// Pass change mask
		event.param4 = (void *)PBG4_PlatformPlugin::environmentalIntSyncHandler;
	
		DLOG("PBG4_PlatformPlugin::handleEnvironmentalInterruptEvent - dispatching data 0x%x, changed 0x%x\n",
			envIntData, changedIntBits);
		localThis->dispatchEvent(&event);
		
		localThis->fLastPMUEnvIntData = envIntData;		// Save current value
	}
	
	//DLOG("PBG4_PlatformPlugin::handleEnvironmentalInterruptEvent - done\n");
	return;
}

// **********************************************************************************
// environmentalIntSyncHandler
//
//          -- Synchronized handler to set change environment status
//			-- Should only be called if status changes as it automatically
//				triggers updates in the control loop(s)
//
// **********************************************************************************
/* static */
IOReturn PBG4_PlatformPlugin::environmentalIntSyncHandler ( void * p1, void * p2, void * p3 )
{
    PBG4_PlatformPlugin		*localThis;
	UInt32					intData, changeMask;
	
	
	localThis = OSDynamicCast(PBG4_PlatformPlugin, (OSMetaClassBase *)p1);
    if (localThis == NULL)
        return  kIOReturnBadArgument;

	// retrieve the data
	intData = (UInt32) p2;
	changeMask = (UInt32) p3;
	
	// Examine the change mask to see what we need to update
	if (changeMask & kACPlugEventMask) {
		localThis->setEnv(gIOPPluginEnvACPresent, 
			((intData & kACPlugEventMask) != 0) ? kOSBooleanTrue : kOSBooleanFalse);
		DLOG("PBG4_PlatformPlugin::environmentalIntSyncHandler - sending A/C present '%s'\n",
			((intData & kACPlugEventMask) != 0) ? "true" : "false");
	}

	if (changeMask & kBatteryStatusEventMask) {
		localThis->setEnv(gIOPPluginEnvBatteryPresent, 
			((intData & kBatteryStatusEventMask) != 0) ? kOSBooleanTrue : kOSBooleanFalse);
		DLOG("PBG4_PlatformPlugin::environmentalIntSyncHandler - sending battery present '%s'\n",
			((intData & kBatteryStatusEventMask) != 0) ? "true" : "false");
	}

	if (changeMask & kPMUBatteryOvercurrentIntMask) {
		localThis->setEnv(gIOPPluginEnvBatteryOvercurrent, 
			((intData & kPMUBatteryOvercurrentIntMask) != 0) ? kOSBooleanTrue : kOSBooleanFalse);
		DLOG("PBG4_PlatformPlugin::environmentalIntSyncHandler - sending overcurrent '%s'\n",
			((intData & kPMUBatteryOvercurrentIntMask) != 0) ? "true" : "false");
	}

	if (changeMask & kClamshellClosedEventMask) {
		localThis->setEnv(gIOPPluginEnvClamshellClosed, 
			((intData & kClamshellClosedEventMask) != 0) ? kOSBooleanTrue : kOSBooleanFalse);
		DLOG("PBG4_PlatformPlugin::environmentalIntSyncHandler - sending clamshell closed '%s'\n",
			((intData & kClamshellClosedEventMask) != 0) ? "true" : "false");
	}

	return kIOReturnSuccess;
}

