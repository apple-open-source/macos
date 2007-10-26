/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 1999 Apple Computer, Inc.  All rights reserved.
 *
 */

#include <sys/systm.h>
#include <sys/proc.h>
#include <kern/task.h>
#include "AppleSmartBatteryManagerUserClient.h"

#define super IOUserClient

enum {
    kCallOnOwner = 0,
    kCallOnSelf = 1
};


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSDefineMetaClassAndStructors(AppleSmartBatteryManagerUserClient, IOUserClient)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool AppleSmartBatteryManagerUserClient::initWithTask(task_t owningTask, 
                    void *security_id, UInt32 type, OSDictionary * properties)
{    
    uint32_t            _pid;

     /* Only root processes may grab exclusive OS access of the SMBus
     * To be clear; 'exclusive' in this context only means that 
     * SmartBattery traffic from the OS is temporarily suspended while
     * a client with exclusive access is open.
     * Our one and only expected exclusive access client is the Battery Updater.
     */
    if (kSBExclusiveSMBusAccessType == type)
    {
        if ( kIOReturnSuccess != 
                clientHasPrivilege(owningTask, kIOClientPrivilegeAdministrator))
        {
            return false;     
        }
    }
    
    if (!super::initWithTask(owningTask, security_id, type, properties)) {    
        return false;
    }

    fUserClientType = type;

	_pid = proc_selfpid();
	setProperty("pid", _pid, 32);

    fOwningTask = owningTask;
    task_reference (fOwningTask);    
    return true;
}


bool AppleSmartBatteryManagerUserClient::start( IOService * provider )
{
    fOwner = (AppleSmartBatteryManager *)provider;
    
    /*
     * exclusive access user client?
     * shut up the AppleSmartBattery from doing ongoing polls
     *
     */
    if (kSBExclusiveSMBusAccessType == fUserClientType)
    {
        if(!fOwner->requestExclusiveSMBusAccess(true)) {
            // Could not obtain exclusive access to SmartBattery
            return false;
        }
    }
    
    if(!super::start(provider))
        return false;

    return true;
}

IOReturn AppleSmartBatteryManagerUserClient::secureInflowDisable(
    int level,
    int *return_code)
{
    int             admin_priv = 0;
    IOReturn        ret = kIOReturnNotPrivileged;

    if( !(level == 0 || level == 1))
    {
        *return_code = kIOReturnBadArgument;
        return kIOReturnSuccess;
    }

    ret = clientHasPrivilege(fOwningTask, kIOClientPrivilegeAdministrator);
    admin_priv = (kIOReturnSuccess == ret);

    if(admin_priv && fOwner) {
        *return_code = fOwner->disableInflow( level );
        return kIOReturnSuccess;
    } else {
        *return_code = kIOReturnNotPrivileged;
        return kIOReturnSuccess;
    }

}

IOReturn AppleSmartBatteryManagerUserClient::secureChargeInhibit( 
    int level,
    int *return_code)
{
    int             admin_priv = 0;
    IOReturn        ret = kIOReturnNotPrivileged;

    if( !(level == 0 || level == 1))
    {
        *return_code = kIOReturnBadArgument;
        return kIOReturnSuccess;
    }

    ret = clientHasPrivilege(fOwningTask, kIOClientPrivilegeAdministrator);
    admin_priv = (kIOReturnSuccess == ret);

    if(admin_priv && fOwner) {
        *return_code = fOwner->inhibitCharging(level);
        return kIOReturnSuccess;
    } else {
        *return_code = kIOReturnNotPrivileged;
        return kIOReturnSuccess;
    }

}


IOReturn AppleSmartBatteryManagerUserClient::clientClose( void )
{
    /* remove our request for exclusive SMBus access */
    if (kSBExclusiveSMBusAccessType == fUserClientType) {    
        fOwner->requestExclusiveSMBusAccess(false);
    }

    detach(fOwner);
    
    if(fOwningTask) {
        task_deallocate(fOwningTask);
        fOwningTask = 0;
    }   
    
    return kIOReturnSuccess;
}

IOExternalMethod *
AppleSmartBatteryManagerUserClient::getTargetAndMethodForIndex( 
    IOService ** targetP, 
    UInt32 index )
{
    static IOExternalMethod sMethods[] = {
        { // kSBInflowDisable = 0
            (IOService *)kCallOnSelf, 
            (IOMethod)&AppleSmartBatteryManagerUserClient::secureInflowDisable, 
            kIOUCScalarIScalarO, 1, 1
        },
        { // kSBChargeInhibit = 1
            (IOService *)kCallOnSelf, 
            (IOMethod)&AppleSmartBatteryManagerUserClient::secureChargeInhibit, 
            kIOUCScalarIScalarO, 1, 1
        },
        { // kSBSetPollingInterval = 2
            (IOService *)kCallOnOwner, 
            (IOMethod)&AppleSmartBatteryManager::setPollingInterval, 
            kIOUCScalarIScalarO, 1, 0
        },
        { // kSBSMBusReadWriteWord = 3
            (IOService *)kCallOnOwner,
            (IOMethod)&AppleSmartBatteryManager::performExternalTransaction,
            kIOUCStructIStructO, sizeof(EXSMBUSInputStruct), sizeof(EXSMBUSOutputStruct)
        }
    };
        
    if(index >= kNumBattMethods) {
        return NULL;
    } else {
        if (kCallOnSelf == (int)sMethods[index].object)
            *targetP = this;
        else
            *targetP = fOwner;

        return &sMethods[index];
    }
}


