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
#include <IOKit/avc/IOFireWireAVCConsts.h>
#include <IOKit/firewire/IOFireWireNub.h>
#include <IOKit/firewire/IOFireWireBus.h>
#include <IOKit/firewire/IOFWAddressSpace.h>
#include <IOKit/avc/IOFireWirePCRSpace.h>
#include <IOKit/avc/IOFireWireAVCTargetSpace.h>

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
    },
    { //    kIOFWAVCProtocolUserClientPublishAVCUnitDirectory
        0,
        (IOMethod) &IOFireWireAVCProtocolUserClient::publishAVCUnitDirectory,
        kIOUCScalarIScalarO,
        0,
        0
    },
    { //    kIOFWAVCProtocolUserClientSetSubunitPlugSignalFormat
        0,
        (IOMethod) &IOFireWireAVCProtocolUserClient::setSubunitPlugSignalFormat,
        kIOUCScalarIScalarO,
        4,
        0
    },
    { //    kIOFWAVCProtocolUserClientGetSubunitPlugSignalFormat, // kIOUCScalarIScalarO 3, 1
        0,
        (IOMethod) &IOFireWireAVCProtocolUserClient::getSubunitPlugSignalFormat,
        kIOUCScalarIScalarO,
        3,
        1
    },
    { //    kIOFWAVCProtocolUserClientConnectTargetPlugs,		// kIOUCStructIStructO
        0,
        (IOMethod) &IOFireWireAVCProtocolUserClient::connectTargetPlugs,
        kIOUCStructIStructO,
        sizeof(AVCConnectTargetPlugsInParams),
        sizeof(AVCConnectTargetPlugsOutParams)
    },
    { //    kIOFWAVCProtocolUserClientDisconnectTargetPlugs,	// kIOUCScalarIScalarO 6, 0
        0,
        (IOMethod) &IOFireWireAVCProtocolUserClient::disconnectTargetPlugs,
        kIOUCScalarIScalarO,
        6,
        0
    },
	{ //    kIOFWAVCProtocolUserClientGetTargetPlugConnection,	// kIOUCStructIStructO
        0,
        (IOMethod) &IOFireWireAVCProtocolUserClient::getTargetPlugConnection,
        kIOUCStructIStructO,
        sizeof(AVCGetTargetPlugConnectionInParams),
        sizeof(AVCGetTargetPlugConnectionOutParams)
    },
	{ //    kIOFWAVCProtocolUserClientAVCRequestNotHandled,  // kIOUCScalarIStructI 4, -1
        0,
        (IOMethod) &IOFireWireAVCProtocolUserClient::AVCRequestNotHandled,
        kIOUCScalarIStructI,
        4,
        0xFFFFFFFF	// variable
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
    },
	{
		//    kIOFWAVCProtocolUserClientInstallAVCCommandHandler
        0,
        (IOAsyncMethod) &IOFireWireAVCProtocolUserClient::installAVCCommandHandler,
        kIOUCScalarIScalarO,
        4,
        0
    },
	{
		//    kIOFWAVCProtocolUserClientAddSubunit
        0,
        (IOAsyncMethod) &IOFireWireAVCProtocolUserClient::addSubunit,
        kIOUCScalarIScalarO,
        5,
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

    //IOLog( "IOFireWireAVCProtocolUserClient::forwardPlugWrite\n");
	
    // fill out return parameters
    info->fClient->fDevice->getNodeIDGeneration(generation, localID);
    args[0] = info->fUserRefcon;
    args[1] = (void*)generation;
    args[2] = (void*)((UInt32)nodeID);
    args[3] = (void*)plug;
    args[4] = (void*)oldVal;
    args[5] = (void*)newVal;
    info->fClient->sendAsyncResult( ((PlugInfo *)info)->fCallbackInfo, kIOReturnSuccess, args, 6 );
}

void IOFireWireAVCProtocolUserClient::avcTargetCommandHandler(const AVCCommandHandlerInfo *pCmdInfo,
															  UInt32 generation,
															  UInt16 nodeID,
															  const void *command,
															  UInt32 cmdLen,
															  IOFWSpeed &speed,
															  UInt32 handlerSearchIndex)
{
    void * args[kMaxAsyncArgs];
	OSAsyncReference asyncRef;
	const UInt8 *src;
	UInt8 *dst;
	UInt32 copyLen;
	
    //IOLog( "IOFireWireAVCProtocolUserClient::avcTargetCommandHandler\n");

	bcopy(pCmdInfo->asyncRef, asyncRef, sizeof(OSAsyncReference));

	args[0] = 0;		// Initial call
	args[2] = (void*)generation;
	args[3] = (void*)((UInt32)nodeID);
	args[4] = (void*)cmdLen;
	args[5] = (void*)pCmdInfo->userCallBack;
	args[6] = (void*)pCmdInfo->userRefCon;
	args[7] = (void*)speed;
	args[8] = (void*)handlerSearchIndex;
	
	src = (const UInt8*)command;
	dst = (UInt8 *)(args+9);
	copyLen = cmdLen;
	if(copyLen > (kMaxAsyncArgs - 9)*sizeof(void *))
		copyLen =  (kMaxAsyncArgs - 9)*sizeof(void *);
	bcopy(src, dst, copyLen);
	args[1] = (void*)copyLen;
	pCmdInfo->userClient->sendAsyncResult( asyncRef, kIOReturnSuccess,
					  args, 9+(copyLen+sizeof(void *)-1)/sizeof(void *) );
	cmdLen -= copyLen;
	src += copyLen;
	// Send rest of packet if necessary
	while(cmdLen) {
		copyLen = cmdLen;
		if(copyLen > (kMaxAsyncArgs - 2)*sizeof(void *))
			copyLen =  (kMaxAsyncArgs - 2)*sizeof(void *);
		bcopy(src, dst, copyLen);
		args[0] = (void *)(src - (const UInt8*)command);	// Bit being sent
		args[1] = (void*)copyLen;
		pCmdInfo->userClient->sendAsyncResult( asyncRef, kIOReturnSuccess,
					   args, 2+(copyLen+sizeof(void *)-1)/sizeof(void *) );
		cmdLen -= copyLen;
		src += copyLen;
	}
}

void IOFireWireAVCProtocolUserClient::avcSubunitPlugHandler(const AVCSubunitInfo *pSubunitInfo,
															IOFWAVCSubunitPlugMessages plugMessage,
															IOFWAVCPlugTypes plugType,
															UInt32 plugNum,
															UInt32 messageParams,
															UInt32 generation,
															UInt16 nodeID)
{
    void * args[kMaxAsyncArgs];
	OSAsyncReference asyncRef;

	//IOLog( "IOFireWireAVCProtocolUserClient::avcSubunitPlugHandler\n");

	bcopy(pSubunitInfo->asyncRef, asyncRef, sizeof(OSAsyncReference));

	args[0] = (void*) pSubunitInfo->subunitTypeAndID;
	args[1] = (void*) plugType;
	args[2] = (void*) plugNum;
	args[3] = (void*) plugMessage;
	args[4] = (void*) messageParams;
	args[5] = (void*)pSubunitInfo->userCallBack;
	args[6] = (void*)pSubunitInfo->userRefCon;
	args[7] = (void*)generation;
	args[8] = (void*)((UInt32)nodeID);
	
	pSubunitInfo->userClient->sendAsyncResult( asyncRef, kIOReturnSuccess,
										args, 9);

}

void IOFireWireAVCProtocolUserClient::free()
{
    //IOLog( "IOFireWireAVCProtocolUserClient::free (0x%08X)\n",(int) this);

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
    if(fAVCTargetSpace) {
        fAVCTargetSpace->deactivateWithUserClient(this);
        fAVCTargetSpace->release();
    }
    
    IOUserClient::free();
}

bool IOFireWireAVCProtocolUserClient::start(IOService *provider)
{
	OSObject *prop;
    IOFireWireNub *device;

    //IOLog( "IOFireWireAVCProtocolUserClient::start (0x%08X)\n",(int) this);

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

    fAVCTargetSpace = IOFireWireAVCTargetSpace::getAVCTargetSpace(fDevice->getController());
    if(!fAVCTargetSpace)
        return false;
    fAVCTargetSpace->activateWithUserClient(this);

	// Enable the communication between the PCR space and the Target space objects
	fPCRSpace->setAVCTargetSpacePointer(fAVCTargetSpace);
	
    registerService();
    
    return true;
}

IOReturn IOFireWireAVCProtocolUserClient::newUserClient( task_t owningTask, void * securityID,
                                    UInt32 type,  OSDictionary * properties,
                                    IOUserClient ** handler )
{
    //IOLog( "IOFireWireAVCProtocolUserClient::newUserClient (0x%08X)\n",(int) this);

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
    //IOLog( "IOFireWireAVCProtocolUserClient::clientClose (0x%08X)\n",(int) this);

	fStarted = false;
	
	getProvider()->terminate( kIOServiceRequired );
	
	return kIOReturnSuccess;
}

IOReturn IOFireWireAVCProtocolUserClient::clientDied( void )
{
    //IOLog( "IOFireWireAVCProtocolUserClient::clientDied (0x%08X)\n",(int) this);

    return clientClose();
}

/**
 ** Matching methods
 **/
bool IOFireWireAVCProtocolUserClient::matchPropertyTable(OSDictionary * table)
{
    //IOLog( "IOFireWireAVCProtocolUserClient::matchPropertyTable (0x%08X)\n",(int) this);

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
    //IOLog( "IOFireWireAVCProtocolUserClient::getTargetAndMethodForIndex (0x%08X)\n",(int) this);

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
    //IOLog( "IOFireWireAVCProtocolUserClient::getAsyncTargetAndMethodForIndex (0x%08X)\n",(int) this);

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
	//IOLog( "IOFireWireAVCProtocolUserClient::setAVCRequestCallback (0x%08X)\n",(int) this);

	// This function has been deprecated!	
	return kIOReturnUnsupported;
}

IOReturn
IOFireWireAVCProtocolUserClient::sendAVCResponse(UInt32 generation, UInt16 nodeID, const char *buffer, UInt32 size)
{
	//IOLog( "IOFireWireAVCProtocolUserClient::sendAVCResponse (0x%08X)\n",(int) this);

	IOMemoryDescriptor *desc = NULL;
    IOFWWriteCommand *cmd = NULL;
    IOReturn status;
    do {
        desc = IOMemoryDescriptor::withAddress((void *)buffer,size, kIODirectionOutIn);
        if(!desc) {
            status = kIOReturnNoMemory;
            break;
        }
		
		status = desc->prepare();
		if( status != kIOReturnSuccess )
		{
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

	//IOLog( "IOFireWireAVCProtocolUserClient::allocateInputPlug (0x%08X)\n",(int) this);
	
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

	//IOLog( "IOFireWireAVCProtocolUserClient::allocateOutputPlug (0x%08X)\n",(int) this);

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

	//IOLog( "IOFireWireAVCProtocolUserClient::freeInputPlug (0x%08X)\n",(int) this);
	
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

	//IOLog( "IOFireWireAVCProtocolUserClient::freeOutputPlug (0x%08X)\n",(int) this);

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
	//IOLog( "IOFireWireAVCProtocolUserClient::readInputPlug (0x%08X)\n",(int) this);

	if(plug > 30)
        return kIOReturnBadArgument;
        
    *valPtr = fPCRSpace->readInputPlug(plug);
    return kIOReturnSuccess;
}

IOReturn IOFireWireAVCProtocolUserClient::updateInputPlug(UInt32 plug, UInt32 oldVal, UInt32 newVal)
{
	//IOLog( "IOFireWireAVCProtocolUserClient::updateInputPlug (0x%08X)\n",(int) this);

    if(plug > 30)
        return kIOReturnBadArgument;
        
    return fPCRSpace->updateInputPlug(plug, oldVal, newVal);
}

IOReturn IOFireWireAVCProtocolUserClient::readOutputPlug(UInt32 plug, UInt32 *valPtr)
{
	//IOLog( "IOFireWireAVCProtocolUserClient::readOutputPlug (0x%08X)\n",(int) this);

    if(plug > 30)
        return kIOReturnBadArgument;
        
    *valPtr = fPCRSpace->readOutputPlug(plug);
    return kIOReturnSuccess;
}

IOReturn IOFireWireAVCProtocolUserClient::updateOutputPlug(UInt32 plug, UInt32 oldVal, UInt32 newVal)
{
	//IOLog( "IOFireWireAVCProtocolUserClient::updateOutputPlug (0x%08X)\n",(int) this);

    if(plug > 30)
        return kIOReturnBadArgument;
        
    return fPCRSpace->updateOutputPlug(plug, oldVal, newVal);
}

IOReturn IOFireWireAVCProtocolUserClient::readOutputMasterPlug(UInt32 *valPtr)
{
	//IOLog( "IOFireWireAVCProtocolUserClient::readOutputMasterPlug (0x%08X)\n",(int) this);

    *valPtr = fPCRSpace->readOutputMasterPlug();
    return kIOReturnSuccess;
}

IOReturn IOFireWireAVCProtocolUserClient::updateOutputMasterPlug(UInt32 oldVal, UInt32 newVal)
{
	//IOLog( "IOFireWireAVCProtocolUserClient::updateOutputMasterPlug (0x%08X)\n",(int) this);

    return fPCRSpace->updateOutputMasterPlug(oldVal, newVal);
}

IOReturn IOFireWireAVCProtocolUserClient::readInputMasterPlug(UInt32 *valPtr)
{
	//IOLog( "IOFireWireAVCProtocolUserClient::readInputMasterPlug (0x%08X)\n",(int) this);

    *valPtr = fPCRSpace->readInputMasterPlug();
    return kIOReturnSuccess;
}

IOReturn IOFireWireAVCProtocolUserClient::updateInputMasterPlug(UInt32 oldVal, UInt32 newVal)
{
	//IOLog( "IOFireWireAVCProtocolUserClient::updateInputMasterPlug (0x%08X)\n",(int) this);

    return fPCRSpace->updateInputMasterPlug(oldVal, newVal);
}

IOReturn IOFireWireAVCProtocolUserClient::publishAVCUnitDirectory(void)
{
	//IOLog( "IOFireWireAVCProtocolUserClient::publishAVCUnitDirectory (0x%08X)\n",(int) this);

	return fAVCTargetSpace->publishAVCUnitDirectory();
}

IOReturn IOFireWireAVCProtocolUserClient::installAVCCommandHandler(OSAsyncReference asyncRef, UInt32 subUnitTypeAndID, UInt32 opCode, UInt32 callback, UInt32 refCon)
{
	//IOLog( "IOFireWireAVCProtocolUserClient::installAVCCommandHandler (0x%08X)\n",(int) this);

	return fAVCTargetSpace->installAVCCommandHandler(this, avcTargetCommandHandler, asyncRef, subUnitTypeAndID, opCode, callback, refCon);
}

IOReturn IOFireWireAVCProtocolUserClient::addSubunit(OSAsyncReference asyncRef,
													 UInt32 subunitType,
													 UInt32 numSourcePlugs,
													 UInt32 numDestPlugs,
													 UInt32 callBack,
													 UInt32 refCon,
													 UInt32 *subUnitTypeAndID)
{
    IOReturn res = kIOReturnSuccess;
	UInt32 subUnitID = 0x00;

	//IOLog( "IOFireWireAVCProtocolUserClient::addSubunit (0x%08X)\n",(int) this);

	res = fAVCTargetSpace->addSubunit(this,
								   avcSubunitPlugHandler,
								   asyncRef,
								   subunitType,
								   numSourcePlugs,
								   numDestPlugs,
								   callBack,
								   refCon,
								   &subUnitID);
	if (res == kIOReturnSuccess)
		*subUnitTypeAndID = subUnitID; 
	
    return res;
}

IOReturn IOFireWireAVCProtocolUserClient::setSubunitPlugSignalFormat(UInt32 subunitTypeAndID,
																	 IOFWAVCPlugTypes plugType,
																	 UInt32 plugNum,
																	 UInt32 signalFormat)
{
	//IOLog( "IOFireWireAVCProtocolUserClient::setSubunitPlugSignalFormat (0x%08X)\n",(int) this);

	return fAVCTargetSpace->setSubunitPlugSignalFormat(this,subunitTypeAndID, plugType, plugNum, signalFormat);
}

IOReturn IOFireWireAVCProtocolUserClient::getSubunitPlugSignalFormat(UInt32 subunitTypeAndID,
											IOFWAVCPlugTypes plugType,
											UInt32 plugNum,
											UInt32 *pSignalFormat)
{
	//IOLog( "IOFireWireAVCProtocolUserClient::getSubunitPlugSignalFormat (0x%08X)\n",(int) this);

	return fAVCTargetSpace->getSubunitPlugSignalFormat(this,subunitTypeAndID, plugType, plugNum, pSignalFormat);
}

IOReturn IOFireWireAVCProtocolUserClient::connectTargetPlugs(AVCConnectTargetPlugsInParams *inParams,
															 AVCConnectTargetPlugsOutParams *outParams)
{
	//IOLog( "IOFireWireAVCProtocolUserClient::connectTargetPlugs (0x%08X)\n",(int) this);

	return fAVCTargetSpace->connectTargetPlugs(this,inParams,outParams);
}

IOReturn IOFireWireAVCProtocolUserClient::disconnectTargetPlugs(UInt32 sourceSubunitTypeAndID,
									   IOFWAVCPlugTypes sourcePlugType,
									   UInt32 sourcePlugNum,
									   UInt32 destSubunitTypeAndID,
									   IOFWAVCPlugTypes destPlugType,
									   UInt32 destPlugNum)
{
	//IOLog( "IOFireWireAVCProtocolUserClient::disconnectTargetPlugs (0x%08X)\n",(int) this);

	return fAVCTargetSpace->disconnectTargetPlugs(this,
											   sourceSubunitTypeAndID,
											   sourcePlugType,
											   sourcePlugNum,
											   destSubunitTypeAndID,
											   destPlugType,
											   destPlugNum);
}

IOReturn IOFireWireAVCProtocolUserClient::getTargetPlugConnection(AVCGetTargetPlugConnectionInParams *inParams,
																  AVCGetTargetPlugConnectionOutParams *outParams)
{
	//IOLog( "IOFireWireAVCProtocolUserClient::getTargetPlugConnection (0x%08X)\n",(int) this);

	return fAVCTargetSpace->getTargetPlugConnection(this,inParams,outParams);
}

IOReturn IOFireWireAVCProtocolUserClient::AVCRequestNotHandled(UInt32 generation,
									  UInt16 nodeID,
									  IOFWSpeed speed,
									  UInt32 handlerSearchIndex,
									  const char *pCmdBuf,
									  UInt32 cmdLen)
{
	//IOLog( "IOFireWireAVCProtocolUserClient::AVCRequestNotHandled (0x%08X)\n",(int) this);

	fAVCTargetSpace->findAVCRequestHandler(this,
							 generation,
							 nodeID,
							 speed,
							 handlerSearchIndex,
							 pCmdBuf,
							 cmdLen);

	return kIOReturnSuccess;
}


