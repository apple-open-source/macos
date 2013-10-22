/*
 * Copyright (c) 2006 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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

#ifndef __AppleSmartBatteryManagerUserClient__
#define __AppleSmartBatteryManagerUserClient__

#include <IOKit/IOUserClient.h>
#include <IOKit/pwr_mgt/IOPMPowerSource.h>
#include "AppleSmartBatteryManager.h"

/*
 * Method index
 */
enum {
    kSBInflowDisable        = 0,
    kSBChargeInhibit        = 1,
    kSBSetPollingInterval   = 2,
    kSBSMBusReadWriteWord   = 3,
    kSBRequestPoll          = 4
};

#define kNumBattMethods     5

/*
 * user client types
 */
enum {
    kSBDefaultType = 0,
    kSBExclusiveSMBusAccessType = 1
};

class AppleSmartBatteryManager;

class AppleSmartBatteryManagerUserClient : public IOUserClient
{
    OSDeclareDefaultStructors(AppleSmartBatteryManagerUserClient)

    friend class AppleSmartBatteryManager;

private:
    AppleSmartBatteryManager *      fOwner;
    task_t                          fOwningTask;
    uint8_t                         fUserClientType;
    
    IOReturn    secureInflowDisable(int level, int *return_code);

    IOReturn    secureChargeInhibit(int level, int *return_code);

public:

    virtual IOReturn clientClose( void );
    
    virtual IOReturn externalMethod( uint32_t selector, 
                                IOExternalMethodArguments * arguments,
                                IOExternalMethodDispatch * dispatch = 0, 
                                OSObject * targe    = 0, void * reference = 0 );

    virtual bool start( IOService * provider );

    virtual bool initWithTask(task_t owningTask, void *security_id, 
                    UInt32 type, OSDictionary * properties);
};

#endif /* ! __AppleSmartBatteryManagerUserClient__ */

