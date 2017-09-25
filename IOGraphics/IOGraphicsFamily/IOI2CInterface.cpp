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

#include <IOKit/IOLib.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/IOLocks.h>
#include <IOKit/pwr_mgt/RootDomain.h>
#include <IOKit/assert.h>
#include <libkern/c++/OSContainers.h>

#include <IOKit/ndrvsupport/IONDRVFramebuffer.h>
#include <IOKit/i2c/IOI2CInterfacePrivate.h>
#include <IOKit/graphics/IOGraphicsPrivate.h>

#include "IOGraphicsKTrace.h"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef super
#define super IOService

OSDefineMetaClassAndAbstractStructors(IOI2CInterface, IOService)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool IOI2CInterface::registerI2C( UInt64 id )
{
    IOI2C_START(registerI2C,id,0,0);

    bool result = true;

    fID = id;

    setProperty(kIOI2CInterfaceIDKey, id, 64);

    registerService();

    IOI2C_END(registerI2C,result,0,0);
    return (result);
}

IOReturn IOI2CInterface::newUserClient( task_t          owningTask,
                                        void *          security_id,
                                        UInt32          type,
                                        IOUserClient ** handler )

{
    IOI2C_START(registerI2C,type,0,0);

    IOReturn            err = kIOReturnSuccess;
    IOUserClient *      newConnect = 0;

    if (type)
    {
        IOI2C_END(registerI2C,kIOReturnBadArgument,0,0);
        IOLog("IOI2CInterface::newUserClient failed, bad argument\n");
        return (kIOReturnBadArgument);
    }

    newConnect = IOI2CInterfaceUserClient::withTask(owningTask);

    if (newConnect)
    {
        if (!newConnect->attach(this)
                || !newConnect->start(this))
        {
            newConnect->detach( this );
            newConnect->release();
            newConnect = 0;

            err = kIOReturnNotAttached;
        }
    }
    else
        err = kIOReturnNoResources;

    *handler = newConnect;

    IOI2C_END(registerI2C,err,0,0);
    if (err)
        IOLog("IOI2CInterface::newUserClient failed 0x%08x\n", err);
    return (err);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef super
#define super IOUserClient

OSDefineMetaClassAndStructors(IOI2CInterfaceUserClient, IOUserClient)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOI2CInterfaceUserClient * IOI2CInterfaceUserClient::withTask( task_t owningTask )
{
    IOI2CUC_START(withTask,0,0,0);

    IOI2CInterfaceUserClient * inst = NULL;

    inst = new IOI2CInterfaceUserClient;
    if (inst && !inst->init())
    {
        inst->release();
        inst = NULL;
    }
    if (inst)
        inst->fTask = owningTask;

    IOI2CUC_END(withTask,0,0,0);
    return (inst);
}

bool IOI2CInterfaceUserClient::start( IOService * provider )
{
    IOI2CUC_START(start,0,0,0);

    if (!super::start(provider))
    {
        IOI2CUC_END(start,false,0,0);
        return (false);
    }

    IOI2CUC_END(start,true,0,0);
    return (true);
}

IOReturn IOI2CInterfaceUserClient::clientClose( void )
{
    IOI2CUC_START(clientClose,0,0,0);
    terminate();
    IOI2CUC_END(clientClose,kIOReturnSuccess,0,0);
    return (kIOReturnSuccess);
}

IOService * IOI2CInterfaceUserClient::getService( void )
{
    IOI2CUC_START(getService,0,0,0);
    IOService * srv = getProvider();
    IOI2CUC_END(getService,0,0,0);
    return (srv);
}

IOExternalMethod * IOI2CInterfaceUserClient::getTargetAndMethodForIndex(
    IOService ** targetP, UInt32 index )
{
    IOI2CUC_START(getTargetAndMethodForIndex,index,0,0);
    static const IOExternalMethod methodTemplate[] = {
                /* 0 */  { NULL, (IOMethod) &IOI2CInterfaceUserClient::extAcquireBus,
                           kIOUCScalarIScalarO, 0, 0 },
                /* 1 */  { NULL, (IOMethod) &IOI2CInterfaceUserClient::extReleaseBus,
                           kIOUCScalarIScalarO, 0, 0 },
                /* 2 */  { NULL, (IOMethod) &IOI2CInterfaceUserClient::extIO,
                           kIOUCStructIStructO, 0xffffffff, 0xffffffff },
            };

    if (index >= (sizeof(methodTemplate) / sizeof(methodTemplate[0])))
    {
        IOI2CUC_END(getTargetAndMethodForIndex,-1,0,0);
        return (NULL);
    }

    *targetP = this;

    IOI2CUC_END(getTargetAndMethodForIndex,0,0,0);
    return ((IOExternalMethod *)(methodTemplate + index));
}

IOReturn IOI2CInterfaceUserClient::setProperties( OSObject * properties )
{
    IOI2CUC_START(setProperties,0,0,0);
    IOI2CUC_END(setProperties,kIOReturnUnsupported,0,0);
    return (kIOReturnUnsupported);
}

IOReturn IOI2CInterfaceUserClient::extAcquireBus( void )
{
    IOI2CUC_START(extAcquireBus,0,0,0);
    IOReturn            ret = kIOReturnNotReady;
    IOI2CInterface *    provider;

    if ((provider = (IOI2CInterface *) copyParentEntry(gIOServicePlane)))
    {
        ret = provider->open( this ) ? kIOReturnSuccess : kIOReturnBusy;
        provider->release();
    }

    IOI2CUC_END(extAcquireBus,ret,0,0);
    return (ret);
}

IOReturn IOI2CInterfaceUserClient::extReleaseBus( void )
{
    IOI2CUC_START(extReleaseBus,0,0,0);
    IOReturn            ret = kIOReturnNotReady;
    IOI2CInterface *    provider;

    if ((provider = (IOI2CInterface *) copyParentEntry(gIOServicePlane)))
    {
        provider->close( this );
        provider->release();
        ret = kIOReturnSuccess;
    }

    IOI2CUC_END(extReleaseBus,ret,0,0);
    return (ret);
}

IOReturn IOI2CInterfaceUserClient::extIO(
    void * inStruct, void * outStruct,
    IOByteCount inSize, IOByteCount * outSize )
{
    IOI2CUC_START(extIO,0,0,0);
    IOReturn            err = kIOReturnNotReady;
    IOI2CInterface *    provider;
    IOI2CBuffer *       buffer;

    IOI2CRequest *                request;
    IOI2CRequest_10_5_0 * requestV1 = NULL;
    IOI2CRequest          requestV2;

    if (inSize < sizeof(IOI2CBuffer))
    {
        IOI2CUC_END(extIO,kIOReturnNoSpace,__LINE__,0);
        return (kIOReturnNoSpace);
    }
    if (*outSize < inSize)
    {
        IOI2CUC_END(extIO,kIOReturnNoSpace,__LINE__,0);
        return (kIOReturnNoSpace);
    }

    buffer = (IOI2CBuffer *) inStruct;
    request = &buffer->request;

    if (!request->sendTransactionType && !request->replyTransactionType)
    {
        requestV1 = (IOGRAPHICS_TYPEOF(requestV1)) &buffer->request;
        bzero(&requestV2, sizeof(requestV2));
        request = &requestV2;

        request->sendTransactionType  = requestV1->sendTransactionType;
        request->replyTransactionType = requestV1->replyTransactionType;
        request->sendAddress          = requestV1->sendAddress;
        request->replyAddress         = requestV1->replyAddress;
        request->sendBytes            = requestV1->sendBytes;
        request->replyBytes           = requestV1->replyBytes;
        request->sendSubAddress       = requestV1->sendSubAddress;
        request->replySubAddress      = requestV1->replySubAddress;
        request->commFlags            = requestV1->commFlags;
        request->minReplyDelay        = requestV1->minReplyDelay;
    }
    else
    {
        /*
         <rdar://problem/23955672> ZDI-CAN-3453: Apple OS X IOGraphicsFamily Untrusted Pointer Dereference Privilege Escalation Vulnerability IOI2CInterfaceUserClient::extIO
         <rdar://problem/24172232> ZDI-CAN-3453: Apple OS X IOGraphicsFamily Untrusted Pointer Dereference Privilege Escalation Vulnerability IOI2CInterfaceUserClient::extIO
         <rdar://problem/24065934> ZDI- CAN-3489: Apple OS X IOGraphicsFamily Untrusted Pointer Dereference Privilege Escalation Vulnerability
         <rdar://problem/24172270> ZDI- CAN-3489: Apple OS X IOGraphicsFamily Untrusted Pointer Dereference Privilege Escalation Vulnerability

         Force completion to NULL if passed in from user land (V1 only case), V2 case handled by the bzero of the V2 structure (above).
         */
        request->completion = NULL;
    }

    if ((provider = (IOI2CInterface *) copyParentEntry(gIOServicePlane)))
        do
        {
            if (!provider->isOpen(this))
            {
                err = kIOReturnNotOpen;
                continue;
            }

            if (request->sendBytes)
            {
                if (!request->sendBuffer)
                    request->sendBuffer = (vm_address_t)  &buffer->inlineBuffer[0];
                else
                {
                    err = kIOReturnMessageTooLarge;
                    continue;
                }
            }
            if (request->replyBytes)
            {
                if (!request->replyBuffer)
                    request->replyBuffer = (vm_address_t) &buffer->inlineBuffer[0];
                else
                {
                    err = kIOReturnMessageTooLarge;
                    continue;
                }
            }

            err = provider->startIO( request );

            if (requestV1)
                requestV1->result = request->result;
        }
    while (false);

    if (provider)
        provider->release();

    if (kIOReturnSuccess == err)
    {
        *outSize = inSize;
        bcopy(inStruct, outStruct, inSize);
    }
    else
        *outSize = 0;

    IOI2CUC_END(extIO,err,request->sendTransactionType,request->replyTransactionType);
    return (err);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool IOI2CInterfaceUserClient::willTerminate(IOService *provider, IOOptionBits options)
{
    IOI2CUC_START(willTerminate,0,0,0);
    DEBG1("I2C-UC", " provider=%p options=%#x\n", provider, (uint32_t)options);
    bool status = super::willTerminate(provider, options);
    IOI2CUC_END(willTerminate,status,0,0);
    return status;
}

bool IOI2CInterfaceUserClient::didTerminate(IOService *provider, IOOptionBits options, bool *defer)
{
    IOI2CUC_START(didTerminate,0,0,0);
    DEBG1("I2C-UC", " provider=%p options=%#x\n", provider, (uint32_t)options);
    bool status = super::didTerminate(provider, options, defer);
    IOI2CUC_END(didTerminate,status,0,0);
    return status;
}

bool IOI2CInterfaceUserClient::requestTerminate(IOService *provider, IOOptionBits options)
{
    IOI2CUC_START(requestTerminate,0,0,0);
    DEBG1("I2C-UC", " provider=%p options=%#x\n", provider, (uint32_t)options);
    bool status = super::requestTerminate(provider, options);
    IOI2CUC_END(requestTerminate,status,0,0);
    return status;
}

bool IOI2CInterfaceUserClient::terminate(IOOptionBits options)
{
    IOI2CUC_START(terminate,0,0,0);
    DEBG1("I2C-UC", "\n");
    bool status = super::terminate(options);
    IOI2CUC_END(terminate,status,0,0);
    return status;
}

bool IOI2CInterfaceUserClient::finalize(IOOptionBits options)
{
    IOI2CUC_START(finalize,0,0,0);
    DEBG1("I2C-UC", "(%#x)\n", options);
    bool status = super::finalize(options);
    IOI2CUC_END(finalize,status,0,0);
    return status;
}

void IOI2CInterfaceUserClient::stop(IOService *provider)
{
    IOI2CUC_START(stop,0,0,0);
    DEBG1("I2C-UC", "(%p)\n", provider);
    super::stop(provider);
    IOI2CUC_END(stop,0,0,0);
}

void IOI2CInterfaceUserClient::free()
{
    IOI2CUC_START(free,0,0,0);
    DEBG1("I2C-UC", "\n");
    super::free();
    IOI2CUC_END(free,0,0,0);
}

