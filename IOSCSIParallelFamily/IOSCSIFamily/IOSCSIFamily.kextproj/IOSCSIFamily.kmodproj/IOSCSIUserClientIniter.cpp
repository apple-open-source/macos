/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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

#include <libkern/OSAtomic.h>
#include <libkern/c++/OSDictionary.h>

#include <IOKit/IOService.h>

class IOSCSIUserClientIniter : public IOService 
{
    OSDeclareDefaultStructors(IOSCSIUserClientIniter);

private:
    static UInt32 hasUCIniter;
    static IOSCSIUserClientIniter *ucIniter;
    static OSDictionary *kProviderMergeProperties;

    virtual IOService *probe(IOService *provider, SInt32 * /* score */);
};

OSDefineMetaClassAndStructors(IOSCSIUserClientIniter, IOService);

UInt32 IOSCSIUserClientIniter::hasUCIniter = 0;
IOSCSIUserClientIniter *IOSCSIUserClientIniter::ucIniter = 0;
OSDictionary *IOSCSIUserClientIniter::kProviderMergeProperties = 0;

IOService * IOSCSIUserClientIniter::
probe(IOService *provider, SInt32 * /* score */)
{
    if (OSCompareAndSwap(false, true, &hasUCIniter)) {
        ucIniter = this;

        OSObject *dictObj = getProperty("IOProviderMergeProperties");
        kProviderMergeProperties = OSDynamicCast(OSDictionary, dictObj);
        
        if (!kProviderMergeProperties) {
            hasUCIniter = false;
            return 0;
        }

        const OSSymbol *userClientClass;
        OSObject *temp = kProviderMergeProperties->getObject(gIOUserClientClassKey);
        if (OSDynamicCast(OSSymbol, temp))
            userClientClass = 0;
        else if (OSDynamicCast(OSString, temp))
            userClientClass = OSSymbol::withString((const OSString *) temp);
        else {
            userClientClass = 0;
            kProviderMergeProperties->removeObject(gIOUserClientClassKey);
        }

        if (userClientClass)
            kProviderMergeProperties->
                setObject(gIOUserClientClassKey, (OSObject *) userClientClass);
    }

    provider->getPropertyTable()->merge(kProviderMergeProperties);

    return ucIniter;
}
