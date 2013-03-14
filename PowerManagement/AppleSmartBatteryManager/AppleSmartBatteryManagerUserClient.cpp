/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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

     /* 1. Only root processes may open a SmartBatteryManagerUserClient.
      * 2. Attempts to create exclusive UserClients will fail if an
      *     exclusive user client is attached.
      * 3. Non-exclusive clients will not be able to perform transactions
      *     while an exclusive client is attached.
      * 3a. Only battery firmware updaters should bother being exclusive.
      */
    if ( kIOReturnSuccess !=
            clientHasPrivilege(owningTask, kIOClientPrivilegeAdministrator))
    {
        return false;
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
            // requestExclusiveSMBusAccess will return false if there's already
            // an exclusive user client.
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
    
    // We only have one application client. If the app is closed,
    // we can terminate the user client.
    terminate();
    
    return kIOReturnSuccess;
}

IOReturn 
AppleSmartBatteryManagerUserClient::externalMethod( 
    uint32_t selector, 
    IOExternalMethodArguments * arguments,
    IOExternalMethodDispatch * dispatch __unused, 
    OSObject * target __unused, 
    void * reference __unused )
{
    if (selector >= kNumBattMethods) {
        // Invalid selector
        return kIOReturnBadArgument;
    }

    switch (selector)
    {
        case kSBInflowDisable:
            // 1 scalar in, 1 scalar out
            return this->secureInflowDisable((int)arguments->scalarInput[0],
                                            (int *)&arguments->scalarOutput[0]);
            break;

        case kSBChargeInhibit:
            // 1 scalar in, 1 scalar out
            return this->secureChargeInhibit((int)arguments->scalarInput[0],
                                            (int *)&arguments->scalarOutput[0]);
            break;
            
        case kSBSetPollingInterval:
            // 1 scalar in, no out
            return fOwner->setPollingInterval((int)arguments->scalarInput[0]);
            break;
            
        case kSBSMBusReadWriteWord:
            if ((kSBExclusiveSMBusAccessType != fUserClientType) && fOwner->hasExclusiveClient())
            {
                /* SmartBatteryManager should not perform this request if there's an exclusive client
                 * attached, and this client isn't the exclusive client. */
                return kIOReturnSuccess;
            }
            // Struct in, struct out
            return fOwner->performExternalTransaction(
                                            (void *)arguments->structureInput,
                                            (void *)arguments->structureOutput,
                                            (IOByteCount)arguments->structureInputSize,
                                            (IOByteCount *)&arguments->structureOutputSize);
            break;

        default:
            // Unknown selector.
            // With a very distinct return type.
            return kIOReturnMessageTooLarge;
    }
}

