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
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/ndrvsupport/IONDRVFramebuffer.h>
#include <IOKit/graphics/IOGraphicsInterfaceTypes.h>
#include <IOKit/assert.h>
#include <IOKit/i2c/IOI2CInterface.h>
#include <IOKit/i2c/PPCI2CInterface.h>

#include <libkern/c++/OSContainers.h>

#include "IONDRVI2CInterface.h"

#include <string.h>

#define IONDRVI2CLOG	0

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

class AppleOnboardI2CInterface : public IOI2CInterface
{
    OSDeclareDefaultStructors(AppleOnboardI2CInterface)

    class PPCI2CInterface * fInterface;
    SInt32		    fPort;

public:
    virtual bool start( IOService * provider );
    virtual IOReturn startIO( IOI2CRequest * request );

    static AppleOnboardI2CInterface * withInterface( PPCI2CInterface * interface, SInt32 port );
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef super
#define super IOI2CInterface

OSDefineMetaClassAndStructors(IONDRVI2CInterface, IOI2CInterface)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IONDRVI2CInterface * IONDRVI2CInterface::withNDRV(
    IONDRVFramebuffer * ndrv, SInt32 busID )
{
    IONDRVI2CInterface * interface;
    UInt64 id = (((UInt64) (UInt32) ndrv) << 32) | busID;

    interface = new IONDRVI2CInterface;
    if (interface)
    {
        interface->fNdrv = ndrv;
        interface->fBusID = busID;
        if (!interface->init()
                || !interface->attach(ndrv)
                || !interface->start(ndrv)
           )
        {
            interface->detach( ndrv );
            interface->release();
            interface = 0;
        }
        else
            interface->registerI2C(id);
    }
    return (interface);
}

bool IONDRVI2CInterface::start( IOService * provider )
{
    IOReturn			err;
    VDCommunicationInfoRec	commInfo;

    if (!super::start(provider))
        return (false);

    bzero( &commInfo, sizeof( commInfo));
    commInfo.csBusID = fBusID;

    err = fNdrv->doStatus( cscGetCommunicationInfo, &commInfo );
    if (kIOReturnSuccess != err)
        return (false);

    supportedTypes     = commInfo.csSupportedTypes;
    supportedCommFlags = commInfo.csSupportedCommFlags;

    setProperty(kIOI2CBusTypeKey, commInfo.csBusType, 32);
    setProperty(kIOI2CTransactionTypesKey, commInfo.csSupportedTypes, 32);
    setProperty(kIOI2CSupportedCommFlagsKey, commInfo.csSupportedCommFlags, 32);

    return (true);
}

IOReturn IONDRVFramebuffer::_iicAction( IONDRVFramebuffer * self, VDCommunicationRec * comm )
{
    return (self->doControl(cscDoCommunication, comm));
}

IOReturn IONDRVI2CInterface::startIO( IOI2CRequest * request )
{
    IOReturn	 	err;
    IOWorkLoop *	wl;
    VDCommunicationRec	comm;

    bzero( &comm, sizeof( comm));

    do
    {
        if (0 == ((1 << request->sendTransactionType) & supportedTypes))
        {
            err = kIOReturnUnsupportedMode;
            continue;
        }
        if (0 == ((1 << request->replyTransactionType) & supportedTypes))
        {
            err = kIOReturnUnsupportedMode;
            continue;
        }
        if (request->commFlags != (request->commFlags & supportedCommFlags))
        {
            err = kIOReturnUnsupportedMode;
            continue;
        }

        comm.csBusID		= fBusID;
        comm.csCommFlags	= request->commFlags;
        comm.csMinReplyDelay 	= 0;

        if (kIOI2CUseSubAddressCommFlag & request->commFlags)
            comm.csSendAddress	= (request->sendAddress << 8) | request->sendSubAddress;
        else
            comm.csSendAddress	= request->sendAddress;

        comm.csSendType		= request->sendTransactionType;
        comm.csSendBuffer	= (LogicalAddress) request->sendBuffer;
        comm.csSendSize		= request->sendBytes;

        if (kIOI2CUseSubAddressCommFlag & request->commFlags)
            comm.csReplyAddress	= (request->replyAddress << 8) | request->replySubAddress;
        else
            comm.csReplyAddress	= request->replyAddress;

        comm.csReplyType	= request->replyTransactionType;
        comm.csReplyBuffer	= (LogicalAddress) request->replyBuffer;
        comm.csReplySize	= request->replyBytes;

        if ((wl = getWorkLoop()))
            err = wl->runAction( (IOWorkLoop::Action) &fNdrv->_iicAction,
                                 fNdrv, (void *) &comm );
        else
            err = kIOReturnNotReady;
    }
    while (false);

    switch (err)
    {
        case kVideoI2CReplyPendingErr:
            err = kIOReturnNoCompletion;
            break;
        case kVideoI2CTransactionErr:
            err = kIOReturnNoDevice;
            break;
        case kVideoI2CBusyErr:
            err = kIOReturnBusy;
            break;
        case kVideoI2CTransactionTypeErr:
            err = kIOReturnUnsupportedMode;
            break;
        case kVideoBufferSizeErr:
            err = kIOReturnOverrun;
            break;
    }

    request->result = err;
    if (request->completion)
        (*request->completion)(request);

    err = kIOReturnSuccess;

    return (err);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef super
#define super IOI2CInterface

OSDefineMetaClassAndStructors(AppleOnboardI2CInterface, IOI2CInterface)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

AppleOnboardI2CInterface * AppleOnboardI2CInterface::withInterface(
    PPCI2CInterface * onboardInterface, SInt32 port )
{
    AppleOnboardI2CInterface * interface;
    UInt64 id = (((UInt64) (UInt32) onboardInterface) << 32) | port;

    interface = new AppleOnboardI2CInterface;
    if (interface)
    {
        interface->fInterface = onboardInterface;
        interface->fPort = port;
        if (!interface->init()
                || !interface->attach(onboardInterface)
                || !interface->start(onboardInterface)
           )
        {
            interface->detach( onboardInterface );
            interface->release();
            interface = 0;
        }
        else
            interface->registerI2C(id);
    }
    return (interface);
}

bool AppleOnboardI2CInterface::start( IOService * provider )
{
    if (!super::start(provider))
        return (false);

    setProperty(kIOI2CBusTypeKey,
                (UInt64) kIOI2CBusTypeI2C, 32);
    setProperty(kIOI2CTransactionTypesKey,
                (UInt64) ((1 << kIOI2CNoTransactionType)
                          | (1 << kIOI2CSimpleTransactionType)
                          | (1 << kIOI2CDDCciReplyTransactionType)
                          | (1 << kIOI2CCombinedTransactionType)), 32);
    setProperty(kIOI2CSupportedCommFlagsKey,
                (UInt64) kIOI2CUseSubAddressCommFlag, 32);

    return (true);
}

IOReturn AppleOnboardI2CInterface::startIO( IOI2CRequest * request )
{
    IOReturn err = kIOReturnSuccess;

    do
    {
        // Open the interface and sets it in the wanted mode:

        fInterface->openI2CBus(fPort);

        // the i2c driver does not support well read in interrupt mode
        // so it is better to "go polling" (read does not timeout on errors
        // in interrupt mode).
        fInterface->setPollingMode(true);

        if (request->sendBytes && (kIOI2CNoTransactionType != request->sendTransactionType))
        {
            if (kIOI2CCombinedTransactionType == request->sendTransactionType)
                fInterface->setCombinedMode();
            else if (kIOI2CUseSubAddressCommFlag & request->commFlags)
                fInterface->setStandardSubMode();
            else
                fInterface->setStandardMode();

            if (!fInterface->writeI2CBus(request->sendAddress >> 1, request->sendSubAddress,
                                         (UInt8 *) request->sendBuffer, request->sendBytes))
                err = kIOReturnNotWritable;
        }

        if (request->replyBytes && (kIOI2CNoTransactionType != request->replyTransactionType))
        {
            if (kIOI2CCombinedTransactionType == request->replyTransactionType)
                fInterface->setCombinedMode();
            else if (kIOI2CUseSubAddressCommFlag & request->commFlags)
                fInterface->setStandardSubMode();
            else
                fInterface->setStandardMode();

            if (!fInterface->readI2CBus(request->replyAddress >> 1, request->replySubAddress,
                                        (UInt8 *) request->replyBuffer, request->replyBytes))
                err = kIOReturnNotReadable;
        }

        fInterface->closeI2CBus();
    }
    while (false);

    request->result = err;

    err = kIOReturnSuccess;

    return (err);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOReturn IONDRVI2CInterface::create( IONDRVFramebuffer * ndrv )
{
    IOReturn			err;
    VDCommunicationInfoRec	commInfo;
    IOI2CInterface *		interface;
    SInt32			busID;
    OSObject *			num;
    bool			ok = true;

    OSArray * array = OSArray::withCapacity(1);
    if (!array)
        return (kIOReturnNoMemory);

    do
    {
        bzero( &commInfo, sizeof( commInfo));
        commInfo.csBusID = kVideoDefaultBus;

        err = ndrv->doStatus( cscGetCommunicationInfo, &commInfo );
#if IONDRVI2CLOG
        IOLog("%s: cscGetCommunicationInfo: ", ndrv->getName());
#endif
        if (kIOReturnSuccess != err)
        {
#if IONDRVI2CLOG
            IOLog("fails with %d\n", err);
#endif
            continue;
        }
#if IONDRVI2CLOG
        IOLog("csBusType %lx, csMinBus %lx, csMaxBus %lx\n"
              "csSupportedTypes %lx, csSupportedCommFlags %lx\n",
              commInfo.csBusType,
              commInfo.csMinBus, commInfo.csMaxBus,
              commInfo.csSupportedTypes, commInfo.csSupportedCommFlags);
#endif
        if (commInfo.csMaxBus < commInfo.csMinBus)
            continue;

        for (busID = commInfo.csMinBus;
                busID <= commInfo.csMaxBus;
                busID++)
        {
            interface = IONDRVI2CInterface::withNDRV( ndrv, busID );
            if (!interface)
                break;
            num = interface->getProperty(kIOI2CInterfaceIDKey);
            if (num)
                array->setObject( num );
            else
                break;
        }

        ok = (busID > commInfo.csMaxBus);
    }
    while (false);

    OSData * data = OSDynamicCast( OSData, ndrv->getProvider()->getProperty("iic-address"));
    if (data && (!ndrv->getProperty(kIOFBDependentIDKey))
            && (0x8c == *((UInt32 *) data->getBytesNoCopy())) /*iMac*/)
        do
        {
            PPCI2CInterface * onboardInterface =
                (PPCI2CInterface*) getResourceService()->getProperty("PPCI2CInterface.i2c-uni-n");
            if (!onboardInterface)
                continue;

            interface = AppleOnboardI2CInterface::withInterface( onboardInterface, 1 );
            if (!interface)
                break;
            num = interface->getProperty(kIOI2CInterfaceIDKey);
            if (num)
                array->setObject( num );
            else
                break;
        }
        while (false);

    if (ok)
        ndrv->setProperty(kIOFBI2CInterfaceIDsKey, array);

    array->release();

    return (kIOReturnSuccess);
}

