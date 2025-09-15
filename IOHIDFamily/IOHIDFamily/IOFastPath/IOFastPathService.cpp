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

#include "IOFastPathService.h"
#include "IOFastPathUserClient.h"
#include <AssertMacros.h>
#include "../IOHIDDebug.h"

static void setDictNumber(OSSharedPtr<OSDictionary> dict, const char * key, unsigned long long num, unsigned int bits)
{
    OSSharedPtr<OSNumber> n = OSNumber::withNumber(num, bits);
    assert(n);

    bool ok = dict->setObject(key, n);
    assert(ok);
}

#pragma mark - IOFastPathField

OSDefineMetaClassAndStructors(IOFastPathField, OSObject);

OSSharedPtr<IOFastPathField>
IOFastPathField::create(IOFastPathFieldKey key, IOFastPathFieldType type, size_t offset, size_t size)
{
    OSSharedPtr<IOFastPathField> me = OSMakeShared<IOFastPathField>();
    assert(me);

    bool ok = me->init(key, type, offset, size);
    assert(ok);

    return me;
}

bool
IOFastPathField::init(IOFastPathFieldKey key, IOFastPathFieldType type, size_t offset, size_t size)
{
    bool ok = super::init();
    assert(ok);

    _key = key;
    _type = type;
    _offset = offset;
    _size = size;

    return ok;
}

bool
IOFastPathField::serialize(OSSerialize * serializer) const
{
    OSSharedPtr<OSDictionary> dict = OSDictionary::withCapacity(4);
    assert(dict);

    setDictNumber(dict, kIOFastPathFieldKeyKey, _key, 8 * sizeof(IOFastPathFieldKey));
    setDictNumber(dict, kIOFastPathFieldTypeKey, _type, 8 * sizeof(IOFastPathFieldType));
    setDictNumber(dict, kIOFastPathFieldOffsetKey, _offset, 8 * sizeof(size_t));
    setDictNumber(dict, kIOFastPathFieldSizeKey, _size, 8 * sizeof(size_t));

    return dict->serialize(serializer);
}


#pragma mark - IOFastPathDescriptor

OSDefineMetaClassAndStructors(IOFastPathDescriptor, OSObject);

OSSharedPtr<IOFastPathDescriptor>
IOFastPathDescriptor::create(OSArray * fields)
{
    OSSharedPtr<IOFastPathDescriptor> me = OSMakeShared<IOFastPathDescriptor>();
    assert(me);

    bool ok = me->init(fields);
    assert(ok);

    return me;
}

bool
IOFastPathDescriptor::init(OSArray * fields)
{
    bool ok = super::init();
    assert(ok);

    _fields = OSSharedPtr<OSArray>(fields, OSRetain);
    assert(_fields);

    _fields->iterateObjects(^bool(OSObject *object) {
        IOFastPathField * field = OSRequiredCast(IOFastPathField, object);
        _sampleSize += field->getSize();
        return false;
    });

    return ok;
}

bool
IOFastPathDescriptor::serialize(OSSerialize * serializer) const
{
    return _fields->serialize(serializer);
}

OSSharedPtr<OSArray>
IOFastPathDescriptor::copyFields() const
{
    return _fields;
}


#pragma mark - IOFastPathService

OSDefineMetaClassAndAbstractStructors(IOFastPathService, IOService);

bool
IOFastPathService::start(IOService * provider)
{
    bool started = false;
    bool success = false;
    IOReturn ret = kIOReturnInvalid;
    IOCircularDataQueueCreateOptions options;

    started = super::start(provider);
    require_action(started, exit, HIDServiceLogError("super::start failed"));

    _clients = OSSet::withCapacity(1);
    assert(_clients);

    _descriptor = createDescriptor();
    require_action(_descriptor, exit, HIDServiceLogError("createDescriptor failed"));

    setProperty(kIOFastPathDescriptorKey, _descriptor.get());

    options = isProducer() ? kIOCircularDataQueueCreateProducer : kIOCircularDataQueueCreateConsumer;
    ret = IOCircularDataQueueCreateWithEntries(options, 128, _descriptor->getSampleSize(), &_queue);
    require_noerr_action(ret, exit, HIDServiceLogError("IOCircularDataQueueCreateWithEntries:0x%x", ret));

    success = true;

exit:
    if (started && !success) {
        super::stop(provider);
    }
    return success;
}

void
IOFastPathService::stop(IOService * provider)
{
    if (_queue) {
        IOCircularDataQueueDestroy(&_queue);
    }
    super::stop(provider);
}

IOReturn
IOFastPathService::newUserClient(task_t owningTask, void * securityID, UInt32 type, OSDictionary * properties, IOUserClient ** handler)
{
    IOReturn ret = kIOReturnInvalid;
    bool ok = false;

    switch(type) {
        case kIOFastPathUserClientType:
        {
            OSSharedPtr<IOUserClient> client = OSMakeShared<IOFastPathUserClient>();
            require_action(client != nullptr, exit, HIDServiceLogError("failed to allocate user client"); ret = kIOReturnNoMemory);

            ok = client->initWithTask(owningTask, securityID, type, properties);
            require_action(ok, exit, HIDServiceLogError("initWithTask failed"); ret = kIOReturnDeviceError);

            ok = client->attach(this);
            require_action(ok, exit, HIDServiceLogError("attach failed"); ret = kIOReturnDeviceError);

            ok = client->start(this);
            require_action(ok, exit, HIDServiceLogError("start failed"); ret = kIOReturnDeviceError; client->detach(this));

            *handler = client.detach();
            ret = kIOReturnSuccess;
            break;
        }
        default:
        {
            ret = super::newUserClient(owningTask, securityID, type, properties, handler);
            break;
        }
    }

exit:
    return ret;
}

bool
IOFastPathService::handleOpen(IOService * client, IOOptionBits options, void * arg)
{
    HIDServiceLog("open by %s:0x%llx", client->getName(), client->getRegistryEntryID());
    if (!_clients->containsObject(client)) {
        _clients->setObject(client);
    }
    return true;
}

void
IOFastPathService::handleClose(IOService * client, IOOptionBits options)
{
    HIDServiceLog("close by %s:0x%llx", client->getName(), client->getRegistryEntryID());
    if (_clients->containsObject(client)) {
        _clients->removeObject(client);
    }
}

bool
IOFastPathService::handleIsOpen(const IOService * client) const
{
    return client ? (_clients->containsObject(client)) : (_clients->getCount() > 0);
}

OSSharedPtr<IOFastPathDescriptor>
IOFastPathService::copyDescriptor() const
{
    assert(_descriptor);
    return _descriptor;
}

IOCircularDataQueue *
IOFastPathService::getQueue() const
{
    assert(_queue);
    return _queue;
}
