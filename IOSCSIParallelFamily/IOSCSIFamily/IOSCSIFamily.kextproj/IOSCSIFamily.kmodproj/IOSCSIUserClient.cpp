/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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

#include <libkern/c++/OSNumber.h>

#include <IOKit/scsi/IOSCSIDeviceInterface.h>

#include "IOSCSIUserClient.h"

#define super IOCDBUserClient
OSDefineMetaClassAndStructorsWithInit
    (IOSCSIUserClient, IOCDBUserClient, IOSCSIUserClient::initialize());

IOExternalMethod IOSCSIUserClient::sMethods[kIOSCSIUserClientNumCommands];

const IOExternalMethod IOSCSIUserClient::
sSCSIOnlyMethods[kIOSCSIUserClientNumOnlySCSICommands] = {
    { //    kIOSCSIUserClientSetTargetParms
	0,
	(IOMethod) &IOSCSIUserClient::setTargetParms,
	kIOUCStructIStructO,
	sizeof(SCSITargetParms),
	0
    },
    { //    kIOSCSIUserClientGetTargetParms,
	0,
	(IOMethod) &IOSCSIUserClient::getTargetParms,
	kIOUCStructIStructO,
	0,
	sizeof(SCSITargetParms)
    },
    { //    kIOSCSIUserClientSetLunParms,
	0,
	(IOMethod) &IOSCSIUserClient::setLunParms,
	kIOUCStructIStructO,
	sizeof(SCSILunParms),
	0
    },
    { //    kIOSCSIUserClientGetLunParms,
	0,
	(IOMethod) &IOSCSIUserClient::getLunParms,
	kIOUCStructIStructO,
	0,
	sizeof(SCSILunParms)
    },
    { //    kIOSCSIUserClientHoldQueue,
	0,
	(IOMethod) &IOSCSIUserClient::holdQueue,
	kIOUCScalarIScalarO,
	1,
	0
    },
    { //    kIOSCSIUserClientReleaseQueue,
	0,
	(IOMethod) &IOSCSIUserClient::releaseQueue,
	kIOUCScalarIScalarO,
	1,
	0
    },
    { //    kIOSCSIUserClientFlushQueue,
	0,
	(IOMethod) &IOSCSIUserClient::flushQueue,
	kIOUCScalarIScalarO,
	2,
	0
    },
    { //    kIOSCSIUserClientNotifyIdle
	0,
	(IOMethod) &IOSCSIUserClient::notifyIdle,
	kIOUCScalarIScalarO,
	3,
	0
    }
};

void IOSCSIUserClient::initialize()
{
    // Copy over cdb methods structure into our local method structure
    bcopy(&IOCDBUserClient::sMethods[0],
            &IOSCSIUserClient::sMethods[0],
            sizeof(IOCDBUserClient::sMethods));
    // Now append the CDBOnly method structure.
    bcopy(&sSCSIOnlyMethods[0],
            &IOSCSIUserClient::sMethods[kIOCDBUserClientNumCommands],
            sizeof(sSCSIOnlyMethods));
}

void IOSCSIUserClient::setExternalMethodVectors()
{
    super::setExternalMethodVectors();
    fMethods = sMethods;
    fNumMethods = kIOSCSIUserClientNumCommands;
}

bool IOSCSIUserClient::start(IOService *provider)
{
    if (!super::start(provider))
	return false;

    if (!OSDynamicCast(IOSCSIDevice, fNub))
        return false;

    return true;
}

IOReturn IOSCSIUserClient::
setTargetParms(void *vInParms, void *vInSize,
               void *, void *, void *, void *)
{
    IOSCSIDevice *nub = (IOSCSIDevice *) fNub;
    OSNumber *lun;
    bool res;

    // Only allowed to set target parameters on Logical unit zero devices
    lun = (OSNumber *) nub->getProperty(kSCSIPropertyLun);
    if (!OSDynamicCast(OSNumber, lun) || lun->unsigned32BitValue())
        return kIOReturnNotPrivileged;

    if ((size_t) vInSize < sizeof(SCSITargetParms))
        return kIOReturnBadArgument;

    res = nub->setTargetParms((SCSITargetParms *) vInParms);
    return (res)? kIOReturnSuccess : kIOReturnError;
}

IOReturn IOSCSIUserClient::
getTargetParms(void *vOutParms, void *vOutSize,
               void *, void *, void *, void *)
{
    IOSCSIDevice *nub = (IOSCSIDevice *) fNub;

    if ((size_t) vOutSize < sizeof(SCSITargetParms))
        return kIOReturnBadArgument;

    nub->getTargetParms((SCSITargetParms *) vOutParms);

    return kIOReturnSuccess;
}

IOReturn IOSCSIUserClient::
setLunParms(void *vInParms, void *vInSize, void *, void *, void *, void *)
{
    IOSCSIDevice *nub = (IOSCSIDevice *) fNub;
    bool res;

    if ((size_t) vInSize < sizeof(SCSILunParms))
        return kIOReturnBadArgument;

    res = nub->setLunParms((SCSILunParms *) vInParms);
    return (res)? kIOReturnSuccess : kIOReturnError;
}

IOReturn IOSCSIUserClient::
getLunParms(void *vOutParms, void *vOutSize, void *, void *, void *, void *)
{
    IOSCSIDevice *nub = (IOSCSIDevice *) fNub;

    if ((size_t) vOutSize < sizeof(SCSILunParms))
        return kIOReturnBadArgument;

    nub->getLunParms((SCSILunParms *) vOutParms);

    return kIOReturnSuccess;
}

IOReturn IOSCSIUserClient::
holdQueue(void *vInQType, void *, void *, void *, void *, void *)
{
    SCSIQueueType queueType = (SCSIQueueType) (int) vInQType;
    IOSCSIDevice *nub = (IOSCSIDevice *) fNub;

    switch((UInt32) vInQType) {
    case kQTypeNormalQ:
    case kQTypeBypassQ:
        nub->holdQueue(queueType);
        return kIOReturnSuccess;
    default:
        return kIOReturnBadArgument;
    }
}

IOReturn IOSCSIUserClient::
releaseQueue(void *vInQType, void *, void *, void *, void *, void *)
{
    SCSIQueueType queueType = (SCSIQueueType) (int) vInQType;
    IOSCSIDevice *nub = (IOSCSIDevice *) fNub;

    switch(queueType) {
    case kQTypeNormalQ:
    case kQTypeBypassQ:
        nub->releaseQueue(queueType);
        return kIOReturnSuccess;
    default:
        return kIOReturnBadArgument;
    }
}

IOReturn IOSCSIUserClient::
flushQueue(void *vInQType, void *vInResultCode,
           void *, void *, void *, void *)
{
    SCSIQueueType queueType = (SCSIQueueType) (int) vInQType;
    IOReturn rc = (UInt32) vInResultCode;
    IOSCSIDevice *nub = (IOSCSIDevice *) fNub;

    switch(queueType) {
    case kQTypeNormalQ:
    case kQTypeBypassQ:
        nub->flushQueue(queueType, rc);
        return kIOReturnSuccess;
    default:
        return kIOReturnBadArgument;
    }
}

IOReturn IOSCSIUserClient::
notifyIdle(void *target, void *callback, void *refcon, void *sender,
           void *, void *)
{
    IOSCSIDevice *nub = (IOSCSIDevice *) fNub;
    int i = 0;

    fIdleArgs[i++] = refcon;
    fIdleArgs[i++] = sender;
    assert(i == (sizeof(fIdleArgs) / sizeof(fIdleArgs[0])));
    IOUserClient::setAsyncReference(fIdleAsyncRef,
                                    fWakePort, callback, target);

    if (!target && !callback && !refcon)
        nub->notifyIdle(0, 0, 0);
    else
        nub->notifyIdle(this, &IOSCSIUserClient::notifyIdleCallBack, 0);

    return kIOReturnSuccess;
}

void IOSCSIUserClient::notifyIdleCallBack(void *vSelf, void *refcon)
{
    IOSCSIUserClient *me = (IOSCSIUserClient *) vSelf;
    sendAsyncResult(me->fIdleAsyncRef,
                    kIOReturnSuccess,
                    me->fIdleArgs,
                    sizeof(me->fIdleArgs) / sizeof(me->fIdleArgs[0]));
}
