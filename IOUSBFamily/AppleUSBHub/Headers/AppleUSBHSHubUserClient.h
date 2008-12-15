/*
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) 1998-2007 Apple Inc.  All Rights Reserved.
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */


#include <IOKit/IOUserClient.h>
#include "AppleUSBHub.h"
#include "AppleUSBEHCI.h"

// Index for user client methods
//
enum
{
    kAppleUSBHSHubUserClientOpen = 0,
    kAppleUSBHSHubUserClientClose,
    kAppleUSBHSHubUserClientIsEHCIRootHub,
    kAppleUSBHSHubUserClientEnterTestMode,
    kAppleUSBHSHubUserClientLeaveTestMode,
    kAppleUSBHSHubUserClientGetNumberOfPorts,
    kAppleUSBHSHubUserClientPutPortIntoTestMode,
    kAppleUSBHSHubUserClientGetLocationID,
	kAppleUSBHSHubUserClientSupportsIndicators,
	kAppleUSBHSHubUserClientSetIndicatorForPort,
	kAppleUSBHSHubUserClientGetPortIndicatorControl,
	kAppleUSBHSHubUserClientSetIndicatorsToAutomatic,
	kAppleUSBHSHubUserClientGetPowerSwitchingMode,
	kAppleUSBHSHubUserClientSetPortPower,
	kAppleUSBHSHubUserClientGetPortPower,
    kNumUSBHSHubMethods
};


class AppleUSBHSHubUserClient : public IOUserClient
{
    OSDeclareDefaultStructors(AppleUSBHSHubUserClient)
    
private:

    AppleUSBHub *				fOwner;
    task_t						fTask;
    const IOExternalMethod *	fMethods;
    IOCommandGate *				fGate;
    UInt32						fNumMethods;
    mach_port_t					fWakePort;
    bool						fDead;

    static const IOExternalMethod	sMethods[kNumUSBHSHubMethods];
    static const IOItemCount 		sMethodCount;

    virtual void				SetExternalMethodVectors(void);

	//	IOService overrides
	//
    virtual IOReturn  			open(bool seize);
    virtual IOReturn  			close(void);
    virtual bool				start( IOService * provider );

    //	IOUserClient overrides
    //
    virtual bool				initWithTask( task_t owningTask, void * securityID, UInt32 type,  OSDictionary * properties );
    virtual IOExternalMethod *	getTargetAndMethodForIndex(IOService **target, UInt32 index);


    // Hub specific methods
    //
    IOReturn					IsEHCIRootHub(UInt32 *ret);
    IOReturn					EnterTestMode(void);
    IOReturn					LeaveTestMode(void);
    IOReturn					GetNumberOfPorts(UInt32 *numPorts);
    IOReturn					PutPortIntoTestMode(UInt32 port, UInt32 mode);
    IOReturn					GetLocationID(UInt32 *locID);
	IOReturn					SupportsIndicators(UInt32 *indicatorSupport);
	IOReturn					GetPortIndicatorControl(UInt32 portNumber, UInt32 *defaultColor );
	IOReturn					SetIndicatorForPort(UInt32 portNumber, UInt32 selector );
	IOReturn					SetIndicatorsToAutomatic();
	IOReturn					GetPowerSwitchingMode(UInt32 *mode);
	IOReturn					GetPortPower(UInt32 port, UInt32 *on);
	IOReturn					SetPortPower(UInt32 port, UInt32 on);
};
