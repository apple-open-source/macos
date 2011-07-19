/*
 * Copyright (c) 1998-2008 Apple Inc. All rights reserved.
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

#include <IOKit/assert.h>
#include <IOKit/network/IONetworkInterface.h>
#include "IONetworkUserClient.h"
#include <IOKit/network/IONetworkData.h>

//------------------------------------------------------------------------

#define super IOUserClient
OSDefineMetaClassAndStructors( IONetworkUserClient, IOUserClient )

#ifdef  DEBUG
#define DLOG(fmt, args...)  IOLog(fmt, ## args)
#else
#define DLOG(fmt, args...)
#endif

//---------------------------------------------------------------------------
// Factory method that performs allocation and initialization
// of an IONetworkUserClient instance.

IONetworkUserClient * IONetworkUserClient::withTask(task_t owningTask)
{
    IONetworkUserClient * me;

    me = new IONetworkUserClient;
    if (me)
    {
        if (!me->init())
        {
            me->release();
            return 0;
        }
        me->_task = owningTask;
    }
    return me;
}

//---------------------------------------------------------------------------
// Start the IONetworkUserClient.

bool IONetworkUserClient::start(IOService * provider)
{
    _owner = OSDynamicCast(IONetworkInterface, provider);
    assert(_owner);

    _handleArray = OSArray::withCapacity(4);
    if (!_handleArray)
        return false;

    _handleLock = IOLockAlloc();
    if (!_handleLock)
        return false;

    if (!super::start(_owner))
        return false;

    if (!_owner->open(this))
        return false;

    return true;
}

//---------------------------------------------------------------------------
// Free the IONetworkUserClient instance.

void IONetworkUserClient::free(void)
{
    if (_handleArray)
    {
        _handleArray->release();
        _handleArray = 0;
    }
    if (_handleLock)
    {
        IOLockFree(_handleLock);
        _handleLock = 0;
    }
    super::free();
}

//---------------------------------------------------------------------------
// Handle a client close. Close and detach from our owner (provider).

IOReturn IONetworkUserClient::clientClose(void)
{
    if (_owner) {
        _owner->close(this);
        detach(_owner);
    }

    return kIOReturnSuccess;
}

//---------------------------------------------------------------------------
// Handle client death. Close and detach from our owner (provider).

IOReturn IONetworkUserClient::clientDied(void)
{
    return clientClose();
}

//---------------------------------------------------------------------------

IOReturn IONetworkUserClient::externalMethod(
            uint32_t selector, IOExternalMethodArguments * arguments,
            IOExternalMethodDispatch * dispatch, OSObject * target,
            void * reference )
{
    IOReturn    ret = kIOReturnBadArgument;

    if (!arguments)
        return kIOReturnBadArgument;

    switch (selector)
    {
        case kIONUCResetNetworkDataIndex:
            if (arguments->scalarInputCount == 1)
                ret = resetNetworkData(
                        (uint32_t) arguments->scalarInput[0]);
            break;
        
        case kIONUCWriteNetworkDataIndex:
            if ((arguments->scalarInputCount == 1) &&
                (arguments->structureInputSize > 0))
                ret = writeNetworkData(
                        (uint32_t) arguments->scalarInput[0],
                        (void *) arguments->structureInput,
                        arguments->structureInputSize);
            break;

        case kIONUCReadNetworkDataIndex:
            if ((arguments->scalarInputCount == 1) &&
                (arguments->structureOutputSize > 0))
                ret = readNetworkData(
                        (uint32_t) arguments->scalarInput[0],
                        arguments->structureOutput,
                        &arguments->structureOutputSize);
            break;

        case kIONUCGetNetworkDataCapacityIndex:
            if ((arguments->scalarInputCount  == 1) &&
                (arguments->scalarOutputCount == 1))
                ret = getNetworkDataCapacity(
                        (uint32_t) arguments->scalarInput[0],
                        &arguments->scalarOutput[0]);
            break;

        case kIONUCGetNetworkDataHandleIndex:
            ret = getNetworkDataHandle(
                    (const char *) arguments->structureInput,
                    (uint32_t *) arguments->structureOutput,
                    arguments->structureInputSize,
                    &arguments->structureOutputSize);
            break;
   
    }
    
    return ret;
}

//---------------------------------------------------------------------------
// Fill the data buffer in an IONetworkData object with zeroes.

IOReturn IONetworkUserClient::resetNetworkData(uint32_t  dataHandle)
{
    IONetworkData *  data;
    const OSSymbol * key;
    IOReturn         ret;

    IOLockLock(_handleLock);
    key = (const OSSymbol *) _handleArray->getObject(dataHandle);
    IOLockUnlock(_handleLock);

    if (!key)
        return kIOReturnBadArgument;

    data = _owner->getNetworkData(key);
    ret = data ? data->reset() : kIOReturnBadArgument;

    return ret;
}

//---------------------------------------------------------------------------
// Write to the data buffer in an IONetworkData object with data from a
// source buffer provided by the caller.

IOReturn
IONetworkUserClient::writeNetworkData(uint32_t  dataHandle,
                                      void *    srcBuffer,
                                      uint32_t  srcBufferSize)
{
    IONetworkData *  data;
    const OSSymbol * key;
    IOReturn         ret;

    IOLockLock(_handleLock);
    key = (const OSSymbol *) _handleArray->getObject(dataHandle);
    IOLockUnlock(_handleLock);

    if (!key || !srcBuffer || !srcBufferSize)
        return kIOReturnBadArgument;

    data = _owner->getNetworkData(key);
    ret = data ? data->write(srcBuffer, srcBufferSize) : kIOReturnBadArgument;

    return ret;
}

//---------------------------------------------------------------------------
// Read the data buffer in an IONetworkData object and copy
// this data to a destination buffer provided by the caller.

IOReturn
IONetworkUserClient::readNetworkData(uint32_t   dataHandle,
                                     void *     dstBuffer,
                                     uint32_t * dstBufferSize)
{
    IONetworkData *  data;
    const OSSymbol * key;
    IOReturn         ret;

    IOLockLock(_handleLock);
    key = (const OSSymbol *) _handleArray->getObject(dataHandle);
    IOLockUnlock(_handleLock);

    if (!key || !dstBuffer || !dstBufferSize)
        return kIOReturnBadArgument;

    data = _owner->getNetworkData(key);
    ret = data ? data->read(dstBuffer, (UInt32 *) dstBufferSize) : 
                 kIOReturnBadArgument;

    return ret;
}

//---------------------------------------------------------------------------
// Get the capacity of an IONetworkData object.

IOReturn
IONetworkUserClient::getNetworkDataCapacity(uint32_t   dataHandle,
                                            uint64_t * capacity)
{
    const OSSymbol * key;
    IONetworkData *  data;
    IOReturn         ret = kIOReturnBadArgument;

    IOLockLock(_handleLock);
    key = (const OSSymbol *) _handleArray->getObject(dataHandle);
    IOLockUnlock(_handleLock);

    if (key)
    {
        data = _owner->getNetworkData(key);
        if (data) {
            *capacity = (uint64_t) data->getSize();
            ret = kIOReturnSuccess;
        }
    }

    return ret;
}

//---------------------------------------------------------------------------
// Called to obtain a handle that maps to an IONetworkData object.
// This handle can be later passed to other methods in this class
// to refer to the same object.

IOReturn
IONetworkUserClient::getNetworkDataHandle(const char * name,
                                          uint32_t *   handle,
                                          uint32_t     nameSize,
                                          uint32_t *   handleSizeP)
{
    IOReturn         ret = kIOReturnBadArgument;
    const OSSymbol * key;
    int              index;

    if (!name || !nameSize || (name[nameSize - 1] != '\0') ||
        (*handleSizeP != sizeof(*handle)))
        return kIOReturnBadArgument;

    key = OSSymbol::withCStringNoCopy(name);
    if (!key)
        return kIOReturnNoMemory;

    if (_owner->getNetworkData(key))
    {
        IOLockLock(_handleLock);
        index = _handleArray->getNextIndexOfObject(key, 0);
        if (index < 0)
        {
            _handleArray->setObject(key);
            index = _handleArray->getNextIndexOfObject(key, 0);
        }
        IOLockUnlock(_handleLock);

        if (index >= 0)
        {
            *handle = index;
            ret = kIOReturnSuccess;
        }
    }

    if (key)
        key->release();

    return ret;
}

//---------------------------------------------------------------------------
// Route setProperties() to our provider.

IOReturn
IONetworkUserClient::setProperties(OSObject * properties)
{
    return _owner->setProperties(properties);
}

//---------------------------------------------------------------------------
// Return our provider. This is called by IOConnectGetService().

IOService * IONetworkUserClient::getService()
{
    return _owner;
}
