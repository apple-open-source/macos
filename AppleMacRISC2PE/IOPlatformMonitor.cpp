/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2002 Apple Computer, Inc.  All rights reserved.
 *
 */

#include <IOKit/IOLib.h>

#include "IOPlatformMonitor.h"

#define super IOService
OSDefineMetaClassAndStructors(IOPlatformMonitor, IOService)

bool IOPlatformMonitor::start(IOService *nub)
{
	if (!initSymbols())
		return false;
		
	// Make sure we have a commandGateCaller - must be provided by subclass
	if (!commandGateCaller) {
		IOLog ("IOPlatformMonitor::start - no commandGateCaller\n");
		return false;
	}
	
    // Creates the Workloop and attaches all the event handlers to it:
    // ---------------------------------------------------------------
    workLoop = IOWorkLoop::workLoop();
    if (workLoop == NULL) {
        return false;
    }

    // Creates the command gate for the events that need to be in the queue
    commandGate = IOCommandGate::commandGate(this, commandGateCaller);

    // and adds it to the workloop:
    if ((commandGate == NULL) ||
        (workLoop->addEventSource(commandGate) != kIOReturnSuccess))
    {
        return false;
    }
	
    // Before we go to sleep we wish to disable the napping mode so that the PMU
    // will not shutdown the system while going to sleep:
    pmRootDomain = OSDynamicCast(IOPMrootDomain, waitForService(serviceMatching("IOPMrootDomain")));
    if (pmRootDomain != 0)
    {
        kprintf("Register IOPlatformMonitor to acknowledge power changes\n");
        pmRootDomain->registerInterestedDriver(this);
        
        // Join the Power Management Tree to receive setAggressiveness calls.
        PMinit();
        nub->joinPMtree(this);
    }
	
	return super::start (nub);
}

// **********************************************************************************
// free
//
// **********************************************************************************
void IOPlatformMonitor::free()
{
	if (commandGate)
		commandGate->release();
	if (workLoop)
		workLoop->release();
	
	return;
}

// **********************************************************************************
// message
//
// **********************************************************************************
IOReturn IOPlatformMonitor::message( UInt32 type, IOService * provider, void * argument)
{
	OSDictionary		*dict;
	IOPMonEventData		event;
	
	dict = OSDynamicCast (OSDictionary, (OSObject *) argument);
	if (!dict)
		return kIOReturnBadArgument;
	
	switch (type) {
		case kIOPMonMessageRegister:
			return registerConSensor (dict, provider);
		
		case kIOPMonMessagePowerMonitor:
			return (monitorPower (dict, provider));
		
		case kIOPMonMessageLowThresholdHit:
		case kIOPMonMessageHighThresholdHit:
		case kIOPMonMessageCurrentValue:
		case kIOPMonMessageStateChanged:
			
			event.conSensor = provider;
			event.event = type;
			event.eventDict = dict;
			/*
			 * handleEvent posts the event to another thread so all handleEvent can
			 * tell us is that it successfully posted the event.  That's the only 
			 * status we can return to the caller.
			 */
			return handleEvent (&event);
		
		default:
			break;
	}
	
	return kIOReturnUnsupported;
}

bool IOPlatformMonitor::initSymbols ()
{
	//	Create common symbols
	if (!gIOPMonTypeKey)			gIOPMonTypeKey 				= OSSymbol::withCString (kIOPMonTypeKey);
	if (!gIOPMonConTypeKey)			gIOPMonConTypeKey 			= OSSymbol::withCString (kIOPMonControlTypeKey);
	if (!gIOPMonTypePowerSens)		gIOPMonTypePowerSens		= OSSymbol::withCString (kIOPMonTypePowerSens);
	if (!gIOPMonTypeThermalSens)	gIOPMonTypeThermalSens		= OSSymbol::withCString (kIOPMonTypeThermalSens);
	if (!gIOPMonTypeClamshellSens)	gIOPMonTypeClamshellSens	= OSSymbol::withCString (kIOPMonTypeClamshellSens);
	if (!gIOPMonTypeCPUCon)			gIOPMonTypeCPUCon			= OSSymbol::withCString (kIOPMonTypeCPUCon);
	if (!gIOPMonTypeGPUCon)			gIOPMonTypeGPUCon			= OSSymbol::withCString (kIOPMonTypeGPUCon);
	if (!gIOPMonTypeSlewCon)		gIOPMonTypeSlewCon			= OSSymbol::withCString (kIOPMonTypeSlewCon);
	if (!gIOPMonTypeFanCon)			gIOPMonTypeFanCon			= OSSymbol::withCString (kIOPMonTypeFanCon);
	if (!gIOPMonIDKey)				gIOPMonIDKey				= OSSymbol::withCString (kIOPMonIDKey);
	if (!gIOPMonCPUIDKey)			gIOPMonCPUIDKey				= OSSymbol::withCString (kIOPMonCPUIDKey);
	if (!gIOPMonLowThresholdKey)	gIOPMonLowThresholdKey		= OSSymbol::withCString (kIOPMonLowThresholdKey);
	if (!gIOPMonHighThresholdKey)	gIOPMonHighThresholdKey		= OSSymbol::withCString (kIOPMonHighThresholdKey);
	if (!gIOPMonThresholdValueKey)	gIOPMonThresholdValueKey	= OSSymbol::withCString (kIOPMonThresholdValueKey);
	if (!gIOPMonCurrentValueKey)	gIOPMonCurrentValueKey		= OSSymbol::withCString (kIOPMonCurrentValueKey);
		
	return true;
}

// **********************************************************************************
// powerStateWillChangeTo
//
// **********************************************************************************
IOReturn IOPlatformMonitor::powerStateWillChangeTo (IOPMPowerFlags, unsigned long, IOService*)
{
    return IOPMAckImplied;
}

// **********************************************************************************
// setAggressiveness
//
// **********************************************************************************
IOReturn IOPlatformMonitor::setAggressiveness(unsigned long selector, unsigned long newLevel)
{
    return super::setAggressiveness(selector, newLevel);
}

// **********************************************************************************
// monitorPower
//
// **********************************************************************************
IOReturn IOPlatformMonitor::monitorPower (OSDictionary *dict, IOService *provider)
{
	return kIOReturnSuccess;
}

// **********************************************************************************
// executeEventThread - thread created to send a runCommand to the commandGate
//
// **********************************************************************************
/* static */
void IOPlatformMonitor::executeCommandThread (IOPMonCommandThreadSet *threadSet)
{
	thread_call_t	workThreadCopy;
	
	// run the command - single threaded
	threadSet->me->commandGate->runCommand (threadSet);
	workThreadCopy = threadSet->workThread;					// remember our thread
	if (threadSet->eventData.eventDict)
		threadSet->eventData.eventDict->release();
	IOFree (threadSet, sizeof (IOPMonCommandThreadSet));	// free the data
	thread_call_free (workThreadCopy);						// kill ourself off
	
	return;
}

// **********************************************************************************
// initPlatformState
//
// **********************************************************************************
bool IOPlatformMonitor::initPlatformState ()
{
	return true;
}


// **********************************************************************************
// savePlatformState
//
// **********************************************************************************
void IOPlatformMonitor::savePlatformState ()
{
	IOPMonCommandThreadSet		*threadSet;
		
	// create the thread data - freed in executeCommandThread
	if ((threadSet = (IOPMonCommandThreadSet *)IOMalloc (sizeof (IOPMonCommandThreadSet))) == NULL)
		return;

	threadSet->command = kIOPMonCommandSaveState;			// command to execute
	threadCommon (threadSet);
	
	return;
}

// **********************************************************************************
// restorePlatformState
//
// **********************************************************************************
void IOPlatformMonitor::restorePlatformState ()
{
	IOPMonCommandThreadSet		*threadSet;
		
	// create the thread data - freed in executeCommandThread
	if ((threadSet = (IOPMonCommandThreadSet *)IOMalloc (sizeof (IOPMonCommandThreadSet))) == NULL)
		return;

	threadSet->command = kIOPMonCommandRestoreState;		// command to execute
	threadCommon (threadSet);
	
	return;
}

// **********************************************************************************
// adjustPlatformState
//
// **********************************************************************************
bool IOPlatformMonitor::adjustPlatformState ()
{
	return true;
}

// **********************************************************************************
// registerConSensor
//
// **********************************************************************************
IOReturn IOPlatformMonitor::registerConSensor (OSDictionary *dict, IOService *conSensor)
{
	return true;
}

// **********************************************************************************
// unregisterSensor
//
// **********************************************************************************
bool IOPlatformMonitor::unregisterSensor (UInt32 sensorID)
{
	return true;
}

	/* Dictionary access utility functions */
// **********************************************************************************
// retrieveValueByKey
//
// **********************************************************************************
bool IOPlatformMonitor::retrieveValueByKey (const OSSymbol *key, OSDictionary *dict, UInt32 *value)
{
	OSNumber		*num;
	
	if (num = OSDynamicCast (OSNumber, dict->getObject (key))) {
		*value = num->unsigned32BitValue();
		return true;
	}
	
	return false;	
}

// **********************************************************************************
// retrieveSensorIndex
//
// **********************************************************************************
bool IOPlatformMonitor::retrieveSensorIndex (OSDictionary *dict, UInt32 *value)
{	
	return (retrieveValueByKey (gIOPMonIDKey, dict, value));
}

// **********************************************************************************
// retrieveThreshold
//
// **********************************************************************************
bool IOPlatformMonitor::retrieveThreshold (OSDictionary *dict, ThermalValue *value)
{
	return (retrieveValueByKey (gIOPMonThresholdValueKey, dict, value));
}

// **********************************************************************************
// retrieveCurrentValue
//
// **********************************************************************************
bool IOPlatformMonitor::retrieveCurrentValue (OSDictionary *dict, UInt32 *value)
{
	return (retrieveValueByKey (gIOPMonCurrentValueKey, dict, value));
}

// **********************************************************************************
// handleEvent
//
// **********************************************************************************
IOReturn IOPlatformMonitor::handleEvent (IOPMonEventData *event)
{
	IOPMonCommandThreadSet		*threadSet;
		
	// create the thread data - freed in executeCommandThread
	if ((threadSet = (IOPMonCommandThreadSet *)IOMalloc (sizeof (IOPMonCommandThreadSet))) == NULL)
		return kIOReturnNoMemory;

	threadSet->command = kIOPMonCommandHandleEvent;		// command to execute
	threadSet->eventData = *event;						// private copy of event data
	threadCommon (threadSet);

	return kIOReturnSuccess;
}

// **********************************************************************************
// threadCommon
//
// **********************************************************************************
bool IOPlatformMonitor::threadCommon (IOPMonCommandThreadSet *threadSet)
{
	// Remember me
	threadSet->me = this;								// object reference for static functions
	threadSet->commandFunction = &executeCommandThread;	// function to execute
	if (threadSet->eventData.eventDict)
		threadSet->eventData.eventDict->retain();
	threadSet->workThread = thread_call_allocate((thread_call_func_t)executeCommandThread, 
		(thread_call_param_t)threadSet);				// create the thread
	
	thread_call_enter(threadSet->workThread);			// invoke it
	
	return true;
}

// **********************************************************************************
// initPowerState
//
// **********************************************************************************
bool IOPlatformMonitor::initPowerState ()
{
	return true;
}

// **********************************************************************************
// savePowerState
//
// **********************************************************************************
void IOPlatformMonitor::savePowerState ()
{
	return;
}

// **********************************************************************************
// restorePowerState
//
// **********************************************************************************
bool IOPlatformMonitor::restorePowerState ()
{
	// false indicates state did not change
	return false;
}
	
// **********************************************************************************
// initThermalState
//
// **********************************************************************************
bool IOPlatformMonitor::initThermalState ()
{
	return true;
}

// **********************************************************************************
// saveThermalState
//
// **********************************************************************************
void IOPlatformMonitor::saveThermalState ()
{
	return;
}

// **********************************************************************************
// restoreThermalState
//
// **********************************************************************************
bool IOPlatformMonitor::restoreThermalState ()
{
	// false indicates state did not change
	return false;
}

// **********************************************************************************
// initClamshellState
//
// **********************************************************************************
bool IOPlatformMonitor::initClamshellState ()
{
	return true;
}

// **********************************************************************************
// saveClamshellState
//
// **********************************************************************************
void IOPlatformMonitor::saveClamshellState ()
{
	return;
}

// **********************************************************************************
// restoreClamshellState
//
// **********************************************************************************
bool IOPlatformMonitor::restoreClamshellState ()
{
	// false indicates state did not change
	return false;
}

