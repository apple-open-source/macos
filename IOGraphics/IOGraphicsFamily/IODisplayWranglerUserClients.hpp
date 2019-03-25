/*
 * Copyright (c) 2018 Apple Computer, Inc. All rights reserved.
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

#ifndef _IOKIT_IODISPLAYWRANGLERUSERCLIENTS_HPP
#define _IOKIT_IODISPLAYWRANGLERUSERCLIENTS_HPP

#include <stdatomic.h>

#include <IOKit/IOUserClient.h>
#include <IOKit/graphics/IOAccelTypes.h>

class IOAccelerationUserClient : public IOUserClient
{
    OSDeclareDefaultStructors(IOAccelerationUserClient);
    using super = IOUserClient;

private:
    OSData *fIDListData;  // list of allocated ids for this task

    IOReturn extCreate(IOOptionBits options,
                       IOAccelID requestedID, IOAccelID *idOutP);
    IOReturn extDestroy(IOOptionBits options, IOAccelID id);

public:
    // OSObject overrides
    virtual void free() APPLE_KEXT_OVERRIDE;

    // IOService overrides
    virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
    virtual void stop(IOService *provider) APPLE_KEXT_OVERRIDE;

    // IOUserClient overrides
    virtual bool initWithTask(task_t, void*, uint32_t,  OSDictionary*)
        APPLE_KEXT_OVERRIDE;
    virtual IOReturn clientClose() APPLE_KEXT_OVERRIDE;

    virtual IOExternalMethod *
        getTargetAndMethodForIndex(IOService **targetP, uint32_t index)
        APPLE_KEXT_OVERRIDE;
};

#endif // !_IOKIT_IODISPLAYWRANGLERUSERCLIENTS_HPP
