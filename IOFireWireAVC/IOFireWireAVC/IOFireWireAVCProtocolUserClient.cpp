/*
 * Copyright (c) 1998-2001 Apple Computer, Inc. All rights reserved.
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
#include "IOFireWireAVCProtocolUserClient.h"
#include "IOFireWireAVCConsts.h"
#include <IOKit/firewire/IOFireWireNub.h>
#include <IOKit/firewire/IOFireWireBus.h>
#include <IOKit/firewire/IOFWAddressSpace.h>
#include "IOFireWirePCRSpace.h"
#include "IOFireWireAVCRequestSpace.h"

// Structure for keeping track of allocated plugs
struct PlugInfo {
    IOFireWireAVCProtocolUserClient *fClient;
    UInt32 fPlug;
    OSAsyncReference fCallbackInfo;
    void *fUserRefcon;	// Store here so CFPlugin doesn't have to track this
};

OSDefineMetaClassAndStructors(IOFireWireAVCProtocolUserClient, IOUserClient)

IOExternalMethod IOFireWireAVCProtocolUserClient::sMethods[kIOFWAVCProtocolUserClientNumCommands] =
{
    { //    kIOFWAVCProtocolUserClientSendAVCResponse
        0,
        (IOMethod) &IOFireWireAVCProtocolUserClient::sendAVCResponse,
        kIOUCScalarIStructI,
        2,
        0xFFFFFFFF	// variable
    },
    { //    kIOFWAVCProtocolUserClientFreeInputPlug
        0,
        (IOMethod) &IOFireWireAVCProtocolUserClient::freeInputPlug,
        kIOUCScalarIScalarO,
        1,
        0
    },
    { //    kIOFWAVCProtocolUserClientReadInputPlug
        0,
        (IOMethod) &IOFireWireAVCProtocolUserClient::readInputPlug,
        kIOUCScalarIScalarO,
        1,
        1
    },
    { //    kIOFWAVCProtocolUserClientUpdateInputPlug
        0,
        (IOMethod) &IOFireWireAVCProtocolUserClient::updateInputPlug,
        kIOUCScalarIScalarO,
        3,
        0
    },
    { //    kIOFWAVCProtocolUserClientFreeOutputPlug
        0,
        (IOMethod) &IOFireWireAVCProtocolUserClient::freeOutputPlug,
        kIOUCScalarIScalarO,
        1,
        0
    },
    { //    kIOFWAVCProtocolUserClientReadOutputPlug
        0,
        (IOMethod) &IOFireWireAVCProtocolUserClient::readOutputPlug,
        kIOUCScalarIScalarO,
        1,
        1
    },
    { //    kIOFWAVCProtocolUserClientUpdateOutputPlug
        0,
        (IOMethod) &IOFireWireAVCProtocolUserClient::updateOutputPlug,
        kIOUCScalarIScalarO,
        3,
        0
    },
    { //    kIOFWAVCProtocolUserClientReadOutputMasterPlug
        0,
        (IOMethod) &IOFireWireAVCProtocolUserClient::readOutputMasterPlug,
        kIOUCScalarIScalarO,
        0,
        1
    },
    { //    kIOFWAVCProtocolUserClientUpdateOutputMasterPlug
        0,
        (IOMethod) &IOFireWireAVCProtocolUserClient::updateOutputMasterPlug,
        kIOUCScalarIScalarO,
        2,
        0
    },
    { //    kIOFWAVCProtocolUserClientReadInputMasterPlug
        0,
        (IOMethod) &IOFireWireAVCProtocolUserClient::readInputMasterPlug,
        kIOUCScalarIScalarO,
        0,
        1
    },
    { //    kIOFWAVCProtocolUserClientUpdateInputMasterPlug
        0,
        (IOMethod) &IOFireWireAVCProtocolUserClient::updateInputMasterPlug,
        kIOUCScalarIScalarO,
        2,
        0
    }
};

IOExternalAsyncMethod IOFireWireAVCProtocolUserClient::sAsyncMethods[kIOFWAVCProtocolUserClientNumAsyncCommands] =
{
    {   //    kIOFWAVCProtocolUserClientSetAVCRequestCallback
        0,
        (IOAsyncMethod) &IOFireWireAVCProtocolUserClient::setAVCRequestCallback,
        kIOUCScalarIScalarO,
        2,
        0
    },
    {   //    kIOFWAVCProtocolUserClientAllocateInputPlug
        0,
        (IOAsyncMethod) &IOFireWireAVCProtocolUserClient::allocateInputPlug,
        kIOUCScalarIScalarO,
        1,
        1
    },
    {   //    kIOFWAVCProtocolUserClientAllocateOutputPlug
        0,
        (IOAsyncMethod) &IOFireWireAVCProtocolUserClient::allocateOutputPlug,
        kIOUCScalarIScalarO,
        1,
        1
    }
};

void IOFireWireAVCProtocolUserClient::forwardPlugWrite(void *refcon, UInt16 nodeID, UInt32 plug, UInt32 oldVal, UInt32 newVal)
{
    OSData *data = (OSData *)refcon;
    const PlugInfo *info = (const PlugInfo *)(data->getBytesNoCopy());
    void * args[kMaxAsyncArgs];
    UInt32 generation;
    UInt16 localID;
    
    // fill out return parameters
    info->fClient->fDevice->getNodeIDGeneration(generation, localID);
    args[0] = info->fUserRefcon;
    args[1] = (void*)generation;
    args[2] = (void*)nodeID;
    args[3] = (void*)plug;
    args[4] = (void*)oldVal;
    args[5] = (void*)newVal;
    info->fClient->sendAsyncResult( ((PlugInfo *)info)->fCallbackInfo, kIOReturnSuccess, args, 6 );
}

UInt32 IOFireWireAVCProtocolUserClient::avcRequestHandler(void *refcon, UInt16 nodeID, IOFWSpeed &speed,
                                  FWAddress addr, UInt32 len, const void *buf, IOFWRequestRefCon requestRefcon)
{
    IOFireWireAVCProtocolUserClient * me = (IOFireWireAVCProtocolUserClient *)refcon;
    
    // Send stuff off to user land
    if( me->fAVCRequestCallbackAsyncRef[0] != 0 ) {
        void * args[kMaxAsyncArgs];
        UInt32 generation;
        UInt16 localID;
        const UInt8 *src;
        UInt8 *dst;
        UInt32 copyLen;
        
        // fill out return parameters
        me->fDevice->getNodeIDGeneration(generation, localID);
        args[0] = 0;		// Initial call
        args[2] = (void*)generation;
        args[3] = (void*)nodeID;
        args[4] = (void*)len;

        src = (const UInt8*)buf;
        dst = (UInt8 *)(args+5);
        copyLen = len;
        if(copyLen > (kMaxAsyncArgs - 5)*sizeof(void *))
            copyLen =  (kMaxAsyncArgs - 5)*sizeof(void *);
        bcopy(src, dst, copyLen);
        args[1] = (void*)copyLen;
        me->sendAsyncResult( me->fAVCRequestCallbackAsyncRef, kIOReturnSuccess,
                                args, 5+(copyLen+sizeof(void *)-1)/sizeof(void *) );
        len -= copyLen;
        src += copyLen;
        // Send rest of packet if necessary
        while(len) {
            copyLen = len;
            if(copyLen > (kMaxAsyncArgs - 2)*sizeof(void *))
                copyLen =  (kMaxAsyncArgs - 2)*sizeof(void *);
            bcopy(src, dst, copyLen);
            args[0] = (void *)(src - (const UInt8*)buf);	// Bit being sent
            args[1] = (void*)copyLen;
            me->sendAsyncResult( me->fAVCRequestCallbackAsyncRef, kIOReturnSuccess,
                                args, 2+(copyLen+sizeof(void *)-1)/sizeof(void *) );
            len -= copyLen;
            src += copyLen;
        }
    }
    
    return kFWResponseComplete;
}

void IOFireWireAVCProtocolUserClient::free()
{
    if(fAVCSpace) {
        fAVCSpace->deactivate();
        fAVCSpace->release();
    }
    
    if(fInputPlugs) {
        OSData *data;
        while(data = OSDynamicCast(OSData, fInputPlugs->getAnyObject())) {
             const PlugInfo *info = (const PlugInfo *)(data->getBytesNoCopy());
             fPCRSpace->freeInputPlug(info->fPlug);
             fInputPlugs->removeObject(data);
        }
        fInputPlugs->release();
    }
    if(fOutputPlugs) {
        OSData *data;
        while(data = OSDynamicCast(OSData, fOutputPlugs->getAnyObject())) {
             const PlugInfo *info = (const PlugInfo *)(data->getBytesNoCopy());
             fPCRSpace->freeOutputPlug(info->fPlug);
             fOutputPlugs->removeObject(data);
        }
        fOutputPlugs->release();
    }
    if(fPCRSpace) {
        fPCRSpace->deactivate();
        fPCRSpace->release();
    }
    
    IOUserClient::free();
}

bool IOFireWireAVCProtocolUserClient::start(IOService *provider)
{
    OSObject *prop;
    IOFireWireNub *device;
    device = OSDynamicCast(IOFireWireNub, provider->getProvider());
    if(!device)
        return false;

    fBus = device->getBus();
    fDevice = device;
    
    // Copy over matching properties from FireWire Unit
    prop = device->getProperty(gFireWire_GUID);
    if(prop)
        setProperty(gFireWire_GUID, prop);

    fInputPlugs = OSSet::withCapacity(1);
    fOutputPlugs = OSSet::withCapacity(1);

    if(!fInputPlugs || !fOutputPlugs)
        return false;
    fPCRSpace = IOFireWirePCRSpace::getPCRAddressSpace(fDevice->getBus());
    if(!fPCRSpace)
        return false;
    fPCRSpace->activate();
        
    registerService();
    
    return true;
}

IOReturn IOFireWireAVCProtocolUserClient::newUserClient( task_t owningTask, void * securityID,
                                    UInt32 type,  OSDictionary * properties,
                                    IOUserClient ** handler )
{
    if(fStarted)
        return kIOReturnStillOpen;
    retain();
    *handler = this;
    fStarted = true;
    fTask = owningTask;
    return kIOReturnSuccess;
}

// clientClose / clientDied
//
//

IOReturn IOFireWireAVCProtocolUserClient::clientClose( void )
{
    IOLog( "IOFireWireAVCProtocolUserClient : clientClose\n" );

	fStarted = false;
	
	getProvider()->terminate( kIOServiceRequired );
	
	return kIOReturnSuccess;
}

IOReturn IOFireWireAVCProtocolUserClient::clientDied( void )
{
	IOLog( "IOFireWireAVCProtocolUserClient : clientDied\n" );

    return clientClose();
}

/**
 ** Matching methods
 **/
bool IOFireWireAVCProtocolUserClient::matchPropertyTable(OSDictionary * table)
{
    //
    // If the service object wishes to compare some of its properties in its
    // property table against the supplied matching dictionary,
    // it should do so in this method and return truth on success.
    //
    if (!IOService::matchPropertyTable(table))  return false;

    // We return success if the following expression is true -- individual
    // comparisions evaluate to truth if the named property is not present
    // in the supplied matching dictionary.

    bool res = compareProperty(table, gFireWire_GUID);
    return res;
}

IOExternalMethod* IOFireWireAVCProtocolUserClient::getTargetAndMethodForIndex(IOService **target, UInt32 index)
{
    if( index >= kIOFWAVCProtocolUserClientNumCommands )
        return NULL;
    else
    {
        *target = this;
        return &sMethods[index];
    }
}

IOExternalAsyncMethod* 
IOFireWireAVCProtocolUserClient::getAsyncTargetAndMethodForIndex(IOService **target, UInt32 index)
{
    if( index >= kIOFWAVCProtocolUserClientNumAsyncCommands )
       return NULL;
   else
   {
       *target = this;
       return &sAsyncMethods[index];
   }
    return NULL;
}

IOReturn IOFireWireAVCProtocolUserClient::setAVCRequestCallback
	( OSAsyncReference asyncRef, UInt32 subUnitType, UInt32 subUnitID)
{
    bcopy( asyncRef, fAVCRequestCallbackAsyncRef, sizeof(OSAsyncReference) );

    if(fAVCSpace) {
        fAVCSpace->deactivate();
        fAVCSpace->release();
    }
    
    fAVCSpace = IOFireWireAVCRequestSpace::withSubUnit(fBus,
                subUnitType, subUnitID, avcRequestHandler, this);
    if(fAVCSpace)
        fAVCSpace->activate();
    
    return kIOReturnSuccess;
}

IOReturn
IOFireWireAVCProtocolUserClient::sendAVCResponse(UInt32 generation, UInt16 nodeID, const char *buffer, UInt32 size)
{
    IOMemoryDescriptor *desc = NULL;
    IOFWWriteCommand *cmd = NULL;
    IOReturn status;
    do {
        desc = IOMemoryDescriptor::withAddress((void *)buffer,size, kIODirectionOutIn);
        if(!desc) {
            status = kIOReturnNoMemory;
            break;
        }
        cmd = new IOFWWriteCommand;
        if(!cmd) {
            status = kIOReturnNoMemory;
            break;
        }
        if(!cmd->initAll(fDevice->getController(), generation,
            FWAddress(kCSRRegisterSpaceBaseAddressHi, kFCPResponseAddress, nodeID),
            desc, NULL, NULL)) {
            status = kIOReturnNoMemory;
            break;
        }
        status = cmd->submit();
    } while (false);
    if(cmd)
        cmd->release();
    if(desc)
        desc->release();
    return status;
}

IOReturn
IOFireWireAVCProtocolUserClient::allocateInputPlug(OSAsyncReference asyncRef, void *userRefcon, UInt32 *plugPtr)
{
    IOReturn status;
    PlugInfo info;
    OSData *data;
    
    data = OSData::withCapacity(sizeof(info));
    if(!data)
        return kIOReturnNoMemory;
    do {
        info.fPlug = 0x1234;	// Impossible plug
        status = fPCRSpace->allocateInputPlug(data, forwardPlugWrite, info.fPlug);
        if(status != kIOReturnSuccess)
            break;
        bcopy(asyncRef, info.fCallbackInfo, sizeof(OSAsyncReference));
        info.fUserRefcon = userRefcon;
        info.fClient = this;
        if(!data->appendBytes(&info, sizeof(info))) {
            status = kIOReturnNoMemory;
            break;
        }
        if(!fInputPlugs->setObject(data)) {
            status = kIOReturnNoMemory;
            break;
        }
        *plugPtr = info.fPlug;
        status = kIOReturnSuccess;
    } while (false);
    data->release();	// Either in set or need to clean up.
    if(status != kIOReturnSuccess && info.fPlug != 0x1234)
        fPCRSpace->freeInputPlug(info.fPlug);
    return status;
}

IOReturn
IOFireWireAVCProtocolUserClient::allocateOutputPlug(OSAsyncReference asyncRef, void *userRefcon, UInt32 *plugPtr)
{
    IOReturn status;
    PlugInfo info;
    OSData *data;
    
    data = OSData::withCapacity(sizeof(info));
    if(!data)
        return kIOReturnNoMemory;
    do {
        info.fPlug = 0x1234;	// Impossible plug
        status = fPCRSpace->allocateOutputPlug(data, forwardPlugWrite, info.fPlug);
        if(status != kIOReturnSuccess)
            break;
        bcopy(asyncRef, info.fCallbackInfo, sizeof(OSAsyncReference));
        info.fUserRefcon = userRefcon;
        info.fClient = this;
        if(!data->appendBytes(&info, sizeof(info))) {
            status = kIOReturnNoMemory;
            break;
        }
        if(!fOutputPlugs->setObject(data)) {
            status = kIOReturnNoMemory;
            break;
        }
        *plugPtr = info.fPlug;
        status = kIOReturnSuccess;
    } while (false);
    data->release();	// Either in set or need to clean up.
    if(status != kIOReturnSuccess && info.fPlug != 0x1234)
        fPCRSpace->freeOutputPlug(info.fPlug);
    return status;
}

IOReturn IOFireWireAVCProtocolUserClient::freeInputPlug(UInt32 plug)
{
    OSIterator *plugIterator;
    plugIterator = OSCollectionIterator::withCollection(fInputPlugs);
    if( plugIterator) {
        OSData * data;
        while( (data = (OSData *)plugIterator->getNextObject())) {
            const PlugInfo *info = (const PlugInfo *)(data->getBytesNoCopy());
            if(info->fPlug == plug) {
                fPCRSpace->freeInputPlug(info->fPlug);
                fInputPlugs->removeObject(data);
            }
        }
        plugIterator->release();
    }
    
    return kIOReturnSuccess;
}

IOReturn IOFireWireAVCProtocolUserClient::freeOutputPlug(UInt32 plug)
{
    OSIterator *plugIterator;
    plugIterator = OSCollectionIterator::withCollection(fOutputPlugs);
    if( plugIterator) {
        OSData * data;
        while( (data = (OSData *)plugIterator->getNextObject())) {
            const PlugInfo *info = (const PlugInfo *)(data->getBytesNoCopy());
            if(info->fPlug == plug) {
                fPCRSpace->freeOutputPlug(info->fPlug);
                fInputPlugs->removeObject(data);
            }
        }
        plugIterator->release();
    }
    
    return kIOReturnSuccess;
}

IOReturn IOFireWireAVCProtocolUserClient::readInputPlug(UInt32 plug, UInt32 *valPtr)
{
    if(plug > 30)
        return kIOReturnBadArgument;
        
    *valPtr = fPCRSpace->readInputPlug(plug);
    return kIOReturnSuccess;
}

IOReturn IOFireWireAVCProtocolUserClient::updateInputPlug(UInt32 plug, UInt32 oldVal, UInt32 newVal)
{
    if(plug > 30)
        return kIOReturnBadArgument;
        
    return fPCRSpace->updateInputPlug(plug, oldVal, newVal);
}

IOReturn IOFireWireAVCProtocolUserClient::readOutputPlug(UInt32 plug, UInt32 *valPtr)
{
    if(plug > 30)
        return kIOReturnBadArgument;
        
    *valPtr = fPCRSpace->readOutputPlug(plug);
    return kIOReturnSuccess;
}

IOReturn IOFireWireAVCProtocolUserClient::updateOutputPlug(UInt32 plug, UInt32 oldVal, UInt32 newVal)
{
    if(plug > 30)
        return kIOReturnBadArgument;
        
    return fPCRSpace->updateOutputPlug(plug, oldVal, newVal);
}

IOReturn IOFireWireAVCProtocolUserClient::readOutputMasterPlug(UInt32 *valPtr)
{
    *valPtr = fPCRSpace->readOutputMasterPlug();
    return kIOReturnSuccess;
}

IOReturn IOFireWireAVCProtocolUserClient::updateOutputMasterPlug(UInt32 oldVal, UInt32 newVal)
{
    return fPCRSpace->updateOutputMasterPlug(oldVal, newVal);
}

IOReturn IOFireWireAVCProtocolUserClient::readInputMasterPlug(UInt32 *valPtr)
{
    *valPtr = fPCRSpace->readInputMasterPlug();
    return kIOReturnSuccess;
}

IOReturn IOFireWireAVCProtocolUserClient::updateInputMasterPlug(UInt32 oldVal, UInt32 newVal)
{
    return fPCRSpace->updateInputMasterPlug(oldVal, newVal);
}
