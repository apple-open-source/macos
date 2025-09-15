/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 *
 * This document is the property of Apple Inc.
 * It is considered confidential and proprietary.
 *
 * This document may not be reproduced or transmitted in any form,
 * in whole or in part, without the express written permission of
 * Apple Inc.
 */

#define IOKIT_ENABLE_SHARED_PTR

#include "IOFastPathUserClient.h"
#include "IOFastPathService.h"
#include <IOKit/IOKitKeys.h>
#include <IOKit/IOCommandGate.h>
#include <AssertMacros.h>

OSDefineMetaClassAndStructors(IOFastPathUserClient, IOUserClient2022);

bool
IOFastPathUserClient::start(IOService * provider)
{
    bool ok = super::start(provider);
    require(ok, exit);

    _service = OSSharedPtr<IOFastPathService>(OSRequiredCast(IOFastPathService, provider), OSRetain);

    _gate = IOCommandGate::commandGate(this);
    assert(_gate);

    getWorkLoop()->addEventSource(_gate.get());

    ok = setProperty(kIOUserClientDefaultLockingKey, kOSBooleanTrue);
    assert(ok);

    ok = setProperty(kIOUserClientDefaultLockingSetPropertiesKey, kOSBooleanTrue);
    assert(ok);

    ok = setProperty(kIOUserClientDefaultLockingSingleThreadExternalMethodKey, kOSBooleanTrue);
    assert(ok);

    ok = setProperty(kIOUserClientEntitlementsKey, kOSBooleanFalse);
    assert(ok);

    ok = provider->open(this);
    require(ok, exit);

exit:
    if (!ok && _gate) {
        getWorkLoop()->removeEventSource(_gate.get());
    }
    return ok;
}

void
IOFastPathUserClient::stop(IOService * provider)
{
    getWorkLoop()->removeEventSource(_gate.get());
    super::stop(provider);
}

bool
IOFastPathUserClient::willTerminate(IOService * provider, IOOptionBits options)
{
    if (provider->isOpen(this)) {
        provider->close(this);
    }
    return super::willTerminate(provider, options);
}

IOReturn
IOFastPathUserClient::clientClose()
{
    if (!isInactive()) {
        terminate();
    }
    return kIOReturnSuccess;
}

IOReturn
IOFastPathUserClient::clientMemoryForType(UInt32 type, IOOptionBits * options, IOMemoryDescriptor ** memory)
{
    return dispatchWorkloopSync(^IOReturn{
        return clientMemoryForTypeGated(type, options, memory);
    });
}

IOReturn
IOFastPathUserClient::clientMemoryForTypeGated(UInt32 type, IOOptionBits * options, IOMemoryDescriptor ** memory)
{
    *memory = IOCircularDataQueueCopyMemoryDescriptor(_service->getQueue());
    if (_service->isProducer()) {
        *options = kIOMapReadOnly; // enforce read-only mapping
    }
    return kIOReturnSuccess;
}

IOReturn
IOFastPathUserClient::externalMethod(uint32_t selector, IOExternalMethodArgumentsOpaque * args)
{
    return dispatchWorkloopSync(^IOReturn{
        return externalMethodGated(selector, args);
    });
}

IOReturn
IOFastPathUserClient::externalMethodGated(uint32_t selector, IOExternalMethodArgumentsOpaque * args)
{
    static const IOExternalMethodDispatch2022 dispatchArray[] = {
        /* Uncomment the formula for count below if/when an external method is added. */
    };
    static const size_t count = 0; /* sizeof(dispatchArray) / sizeof(dispatchArray[0]) */

    return dispatchExternalMethod(selector, args, dispatchArray, count, this, NULL);
}

IOReturn
IOFastPathUserClient::dispatchWorkloopSync(IOEventSource::ActionBlock action)
{
    IOReturn ret = kIOReturnOffline;
    if (!isInactive()) {
        ret = _gate->runActionBlock(^IOReturn{
            return isInactive() ? kIOReturnOffline : action();
        });
    }
    return ret;
}
