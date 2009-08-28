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

#if FIRELOG
#import <IOKit/firewire/FireLog.h>
#define FIRELOG_MSG(x) FireLog x
#else
#define FIRELOG_MSG(x) do {} while (0)
#endif

// Structure for keeping track of allocated plugs
struct PlugInfo {
    IOFireWireAVCProtocolUserClient *fClient;
    UInt32 fPlug;
    OSAsyncReference64 fCallbackInfo;
    uint64_t fUserRefcon;	// Store here so CFPlugin doesn't have to track this
};

OSDefineMetaClassAndStructors(IOFireWireAVCProtocolUserClient, IOUserClient)

//////////////////////////////////////////////////////
// IOFireWireAVCUserClient::externalMethod
//////////////////////////////////////////////////////
IOReturn IOFireWireAVCProtocolUserClient::externalMethod( uint32_t selector, 
														IOExternalMethodArguments * arguments,
														IOExternalMethodDispatch * dispatch, 
														OSObject * target, 
														void * reference)
{
	IOReturn result = kIOReturnBadArgument;
	
	FIRELOG_MSG(("IOFireWireAVCProtocolUserClient::externalMethod (this=0x%08X), selector=0x%08X\n",this,selector));
	
	// Dispatch the method call
	switch (selector)
	{
		case kIOFWAVCProtocolUserClientSendAVCResponse:
			result = sendAVCResponse(arguments->scalarInput[0],arguments->scalarInput[1], (const char*)arguments->structureInput, arguments->structureInputSize);
			break;
	
		case kIOFWAVCProtocolUserClientFreeInputPlug:
			result = freeInputPlug(arguments->scalarInput[0]);
			break;
	
		case kIOFWAVCProtocolUserClientReadInputPlug:
			result = readInputPlug((UInt32) arguments->scalarInput[0], arguments->scalarOutput);
			break;
		
		case kIOFWAVCProtocolUserClientUpdateInputPlug:
			result = updateInputPlug(arguments->scalarInput[0],arguments->scalarInput[1],arguments->scalarInput[2]);
			break;
	
		case kIOFWAVCProtocolUserClientFreeOutputPlug:
			result = freeOutputPlug(arguments->scalarInput[0]);
			break;

		case kIOFWAVCProtocolUserClientReadOutputPlug:
			result = readOutputPlug((UInt32) arguments->scalarInput[0], arguments->scalarOutput);
			break;
		
		case kIOFWAVCProtocolUserClientUpdateOutputPlug:
			result = updateOutputPlug(arguments->scalarInput[0],arguments->scalarInput[1],arguments->scalarInput[2]);
			break;

		case kIOFWAVCProtocolUserClientReadOutputMasterPlug:
			result = readOutputMasterPlug(arguments->scalarOutput);
			break;

		case kIOFWAVCProtocolUserClientUpdateOutputMasterPlug:
			result = updateOutputMasterPlug(arguments->scalarInput[0],arguments->scalarInput[1]);
			break;
		
		case kIOFWAVCProtocolUserClientReadInputMasterPlug:
			result = readInputMasterPlug(arguments->scalarOutput);
			break;
		
		case kIOFWAVCProtocolUserClientUpdateInputMasterPlug:
			result = updateInputMasterPlug(arguments->scalarInput[0],arguments->scalarInput[1]);
			break;
		
		case kIOFWAVCProtocolUserClientPublishAVCUnitDirectory:
			result = publishAVCUnitDirectory();
			break;
		
		case kIOFWAVCProtocolUserClientSetSubunitPlugSignalFormat:
			result = setSubunitPlugSignalFormat(arguments->scalarInput[0],(IOFWAVCPlugTypes)arguments->scalarInput[1],arguments->scalarInput[2],arguments->scalarInput[3]);
			break;
		
		case kIOFWAVCProtocolUserClientGetSubunitPlugSignalFormat:
			result = getSubunitPlugSignalFormat(arguments->scalarInput[0],(IOFWAVCPlugTypes)arguments->scalarInput[1],arguments->scalarInput[2],arguments->scalarOutput);
			break;
		
		case kIOFWAVCProtocolUserClientConnectTargetPlugs:
			result = connectTargetPlugs((AVCConnectTargetPlugsInParams *) arguments->structureInput, (AVCConnectTargetPlugsOutParams *) arguments->structureOutput);
			break;
		
		case kIOFWAVCProtocolUserClientDisconnectTargetPlugs:
			result = disconnectTargetPlugs(arguments->scalarInput[0],
										(IOFWAVCPlugTypes)arguments->scalarInput[1],
										arguments->scalarInput[2],
										arguments->scalarInput[3],
										(IOFWAVCPlugTypes)arguments->scalarInput[4],
										arguments->scalarInput[5]);
			break;
	
		case kIOFWAVCProtocolUserClientGetTargetPlugConnection:
			result = getTargetPlugConnection((AVCGetTargetPlugConnectionInParams *) arguments->structureInput,
											(AVCGetTargetPlugConnectionOutParams *) arguments->structureOutput);
			break;
		
		case kIOFWAVCProtocolUserClientAVCRequestNotHandled:
			result = AVCRequestNotHandled(arguments->scalarInput[0],
										arguments->scalarInput[1],
										(IOFWSpeed)arguments->scalarInput[2],
										arguments->scalarInput[3],
										(const char *)arguments->structureInput,
										arguments->structureInputSize);
			break;

		case kIOFWAVCProtocolUserClientAllocateInputPlug:
			result = allocateInputPlug(arguments->asyncReference, arguments->scalarInput[0], arguments->scalarOutput);
			break;
	
		case kIOFWAVCProtocolUserClientAllocateOutputPlug:
			result = allocateOutputPlug(arguments->asyncReference, arguments->scalarInput[0], arguments->scalarOutput);
			break;

		case kIOFWAVCProtocolUserClientInstallAVCCommandHandler:
			result = installAVCCommandHandler(arguments->asyncReference, arguments->scalarInput[0], arguments->scalarInput[1],arguments->scalarInput[2],arguments->scalarInput[3]);
			break;
		
		case kIOFWAVCProtocolUserClientAddSubunit:
			result = addSubunit(arguments->asyncReference, 
								arguments->scalarInput[0],
								arguments->scalarInput[1],
								arguments->scalarInput[2],
								arguments->scalarInput[3],
								arguments->scalarInput[4],
								arguments->scalarOutput);		
			break;
	
		case kIOFWAVCProtocolUserClientSetAVCRequestCallback: // No longer supported!
			result = kIOReturnUnsupported;
			break;

		default:
			// None of the above!
			break;
	};
	
	return result;
}



void IOFireWireAVCProtocolUserClient::forwardPlugWrite(void *refcon, UInt16 nodeID, UInt32 plug, UInt32 oldVal, UInt32 newVal)
{
    OSData *data = (OSData *)refcon;
    const PlugInfo *info = (const PlugInfo *)(data->getBytesNoCopy());
    io_user_reference_t args[kMaxAsyncArgs];
    UInt32 generation;
    UInt16 localID;

    FIRELOG_MSG(( "IOFireWireAVCProtocolUserClient::forwardPlugWrite\n"));
	
    // fill out return parameters
    info->fClient->fDevice->getNodeIDGeneration(generation, localID);
    args[0] = info->fUserRefcon;
    args[1] = generation;
    args[2] = ((UInt32)nodeID);
    args[3] = plug;
    args[4] = oldVal;
    args[5] = newVal;
    info->fClient->sendAsyncResult64( ((PlugInfo *)info)->fCallbackInfo, kIOReturnSuccess, args, 6 );
}

void IOFireWireAVCProtocolUserClient::avcTargetCommandHandler(const AVCCommandHandlerInfo *pCmdInfo,
															  UInt32 generation,
															  UInt16 nodeID,
															  const void *command,
															  UInt32 cmdLen,
															  IOFWSpeed &speed,
															  UInt32 handlerSearchIndex)
{
    io_user_reference_t args[kMaxAsyncArgs];
	OSAsyncReference64 asyncRef;
	UInt8 *src;
	UInt8 *dst;
	UInt32 copyLen;
	UInt32 i;
	UInt32 thisCopyLen;
	UInt8 *thisLoopStartSrc;
	
    FIRELOG_MSG(( "IOFireWireAVCProtocolUserClient::avcTargetCommandHandler\n"));

	bcopy(pCmdInfo->asyncRef, asyncRef, sizeof(OSAsyncReference64));

	args[0] = 0;		// Initial call
	args[2] = generation;
	args[3] = ((UInt32)nodeID);
	args[4] = cmdLen;
	args[5] = pCmdInfo->userCallBack;
	args[6] = pCmdInfo->userRefCon;
	args[7] = speed;
	args[8] = handlerSearchIndex;
	
	src = (UInt8*)command;
	dst = (UInt8 *)(args+9);

#if __BIG_ENDIAN__
	dst += 4;
#endif
	
	copyLen = cmdLen;
	if(copyLen > (kMaxAsyncArgs - 9)*4)
		copyLen =  (kMaxAsyncArgs - 9)*4;

	i = copyLen;
	while (i > 0)
	{
		thisCopyLen = (i < 4) ? i : 4;
		bcopy(src, dst, thisCopyLen);
		i -= thisCopyLen;
		src += 4;
		dst += 8;
	}
	
	args[1] = copyLen;
	pCmdInfo->userClient->sendAsyncResult64( asyncRef, kIOReturnSuccess,
					  args, 9+(copyLen+4-1)/4 );
	cmdLen -= copyLen;

	// Send rest of packet if necessary
	while(cmdLen) {
		copyLen = cmdLen;
		dst = (UInt8 *)(args+2);

#if __BIG_ENDIAN__
		dst += 4;
#endif
		
		if(copyLen > (kMaxAsyncArgs - 2)*4)
			copyLen =  (kMaxAsyncArgs - 2)*4;
		
		i = copyLen;
		thisLoopStartSrc = src;
		while (i > 0)
		{
			thisCopyLen = (i < 4) ? i : 4;
			bcopy(src, dst, thisCopyLen);
			i -= thisCopyLen;
			src += 4;
			dst += 8;
		}

		args[0] = (thisLoopStartSrc - (const UInt8*)command);	// Bit being sent
		args[1] = copyLen;
		pCmdInfo->userClient->sendAsyncResult64( asyncRef, kIOReturnSuccess,
					   args, 2+(copyLen+4-1)/4 );
		cmdLen -= copyLen;
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
    io_user_reference_t args[kMaxAsyncArgs];
	OSAsyncReference64 asyncRef;

	FIRELOG_MSG(( "IOFireWireAVCProtocolUserClient::avcSubunitPlugHandler\n"));

	bcopy(pSubunitInfo->asyncRef, asyncRef, sizeof(OSAsyncReference64));

	args[0] = pSubunitInfo->subunitTypeAndID;
	args[1] = plugType;
	args[2] = plugNum;
	args[3] = plugMessage;
	args[4] = messageParams;
	args[5] = pSubunitInfo->userCallBack;
	args[6] = pSubunitInfo->userRefCon;
	args[7] = generation;
	args[8] = ((UInt32)nodeID);
	
	pSubunitInfo->userClient->sendAsyncResult64( asyncRef, kIOReturnSuccess,
										args, 9);
}

void IOFireWireAVCProtocolUserClient::free()
{
    FIRELOG_MSG(( "IOFireWireAVCProtocolUserClient::free (%p)\n",this));

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

    FIRELOG_MSG(( "IOFireWireAVCProtocolUserClient::start (%p)\n", this));

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
    FIRELOG_MSG(( "IOFireWireAVCProtocolUserClient::newUserClient (%p)\n", this));

	if(fStarted)
        return kIOReturnStillOpen;
    retain();
    *handler = this;
    fStarted = true;
    fTask = owningTask;

	// Allow Rosetta based apps access to this user-client
	if (properties)
	{
		properties->setObject("IOUserClientCrossEndianCompatible", kOSBooleanTrue);
		setProperty("IOUserClientCrossEndianCompatible", kOSBooleanTrue);
	}
	
    return kIOReturnSuccess;
}

// clientClose / clientDied
//
//

IOReturn IOFireWireAVCProtocolUserClient::clientClose( void )
{
    FIRELOG_MSG(( "IOFireWireAVCProtocolUserClient::clientClose (%p)\n", this));

	fStarted = false;
	
	getProvider()->terminate( kIOServiceRequired );
	
	return kIOReturnSuccess;
}

IOReturn IOFireWireAVCProtocolUserClient::clientDied( void )
{
    FIRELOG_MSG(( "IOFireWireAVCProtocolUserClient::clientDied (%p)\n", this));

    return clientClose();
}

/**
 ** Matching methods
 **/
bool IOFireWireAVCProtocolUserClient::matchPropertyTable(OSDictionary * table)
{
    FIRELOG_MSG(( "IOFireWireAVCProtocolUserClient::matchPropertyTable (%p)\n", this));

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

IOReturn
IOFireWireAVCProtocolUserClient::sendAVCResponse(UInt32 generation, UInt16 nodeID, const char *buffer, UInt32 size)
{
	FIRELOG_MSG(( "IOFireWireAVCProtocolUserClient::sendAVCResponse (%p)\n", this));

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
IOFireWireAVCProtocolUserClient::allocateInputPlug(io_user_reference_t *asyncRef, uint64_t userRefcon, uint64_t *plugPtr)
{
    IOReturn status;
    PlugInfo info;
    OSData *data;

	FIRELOG_MSG(( "IOFireWireAVCProtocolUserClient::allocateInputPlug (%p)\n", this));
	
    data = OSData::withCapacity(sizeof(info));
    if(!data)
        return kIOReturnNoMemory;
    do {
        info.fPlug = 0x1234;	// Impossible plug
        status = fPCRSpace->allocateInputPlug(data, forwardPlugWrite, info.fPlug);
        if(status != kIOReturnSuccess)
            break;
        bcopy(asyncRef, info.fCallbackInfo, sizeof(OSAsyncReference64));
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
IOFireWireAVCProtocolUserClient::allocateOutputPlug(io_user_reference_t *asyncRef, uint64_t userRefcon, uint64_t *plugPtr)
{
    IOReturn status;
    PlugInfo info;
    OSData *data;

	FIRELOG_MSG(( "IOFireWireAVCProtocolUserClient::allocateOutputPlug (%p)\n", this));

    data = OSData::withCapacity(sizeof(info));
    if(!data)
        return kIOReturnNoMemory;
    do {
        info.fPlug = 0x1234;	// Impossible plug
        status = fPCRSpace->allocateOutputPlug(data, forwardPlugWrite, info.fPlug);
        if(status != kIOReturnSuccess)
            break;
        bcopy(asyncRef, info.fCallbackInfo, sizeof(OSAsyncReference64));
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

	FIRELOG_MSG(( "IOFireWireAVCProtocolUserClient::freeInputPlug (%p)\n", this));
	
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

	FIRELOG_MSG(( "IOFireWireAVCProtocolUserClient::freeOutputPlug (%p)\n", this));

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

IOReturn IOFireWireAVCProtocolUserClient::readInputPlug(UInt32 plug, uint64_t *valPtr)
{
	FIRELOG_MSG(( "IOFireWireAVCProtocolUserClient::readInputPlug (%p)\n", this));

	if(plug > 30)
        return kIOReturnBadArgument;
        
    *valPtr = fPCRSpace->readInputPlug(plug);
    return kIOReturnSuccess;
}

IOReturn IOFireWireAVCProtocolUserClient::updateInputPlug(UInt32 plug, UInt32 oldVal, UInt32 newVal)
{
	FIRELOG_MSG(( "IOFireWireAVCProtocolUserClient::updateInputPlug (%p)\n", this));

    if(plug > 30)
        return kIOReturnBadArgument;
        
    return fPCRSpace->updateInputPlug(plug, oldVal, newVal);
}

IOReturn IOFireWireAVCProtocolUserClient::readOutputPlug(UInt32 plug, uint64_t *valPtr)
{
	FIRELOG_MSG(( "IOFireWireAVCProtocolUserClient::readOutputPlug (%p)\n",this));

    if(plug > 30)
        return kIOReturnBadArgument;
        
    *valPtr = fPCRSpace->readOutputPlug(plug);
    return kIOReturnSuccess;
}

IOReturn IOFireWireAVCProtocolUserClient::updateOutputPlug(UInt32 plug, UInt32 oldVal, UInt32 newVal)
{
	FIRELOG_MSG(( "IOFireWireAVCProtocolUserClient::updateOutputPlug (%p)\n", this));

    if(plug > 30)
        return kIOReturnBadArgument;
        
    return fPCRSpace->updateOutputPlug(plug, oldVal, newVal);
}

IOReturn IOFireWireAVCProtocolUserClient::readOutputMasterPlug(uint64_t *valPtr)
{
	FIRELOG_MSG(( "IOFireWireAVCProtocolUserClient::readOutputMasterPlug (%p)\n",this));

    *valPtr = fPCRSpace->readOutputMasterPlug();
    return kIOReturnSuccess;
}

IOReturn IOFireWireAVCProtocolUserClient::updateOutputMasterPlug(UInt32 oldVal, UInt32 newVal)
{
	FIRELOG_MSG(( "IOFireWireAVCProtocolUserClient::updateOutputMasterPlug (%p)\n", this));

    return fPCRSpace->updateOutputMasterPlug(oldVal, newVal);
}

IOReturn IOFireWireAVCProtocolUserClient::readInputMasterPlug(uint64_t *valPtr)
{
	FIRELOG_MSG(( "IOFireWireAVCProtocolUserClient::readInputMasterPlug (%p)\n", this));

    *valPtr = fPCRSpace->readInputMasterPlug();
    return kIOReturnSuccess;
}

IOReturn IOFireWireAVCProtocolUserClient::updateInputMasterPlug(UInt32 oldVal, UInt32 newVal)
{
	FIRELOG_MSG(( "IOFireWireAVCProtocolUserClient::updateInputMasterPlug (%p)\n", this));

    return fPCRSpace->updateInputMasterPlug(oldVal, newVal);
}

IOReturn IOFireWireAVCProtocolUserClient::publishAVCUnitDirectory(void)
{
	FIRELOG_MSG(( "IOFireWireAVCProtocolUserClient::publishAVCUnitDirectory (%p)\n", this));

	return fAVCTargetSpace->publishAVCUnitDirectory();
}

IOReturn IOFireWireAVCProtocolUserClient::installAVCCommandHandler(io_user_reference_t *asyncRef, uint64_t subUnitTypeAndID, uint64_t opCode, uint64_t callback, uint64_t refCon)
{
	FIRELOG_MSG(( "IOFireWireAVCProtocolUserClient::installAVCCommandHandler (%p)\n", this));

	return fAVCTargetSpace->installAVCCommandHandler(this, avcTargetCommandHandler, asyncRef, subUnitTypeAndID, opCode, callback, refCon);
}

IOReturn IOFireWireAVCProtocolUserClient::addSubunit(io_user_reference_t *asyncRef,
													 uint64_t subunitType,
													 uint64_t numSourcePlugs,
													 uint64_t numDestPlugs,
													 uint64_t callBack,
													 uint64_t refCon,
													 uint64_t *subUnitTypeAndID)
{
    IOReturn res = kIOReturnSuccess;
	UInt32 subUnitID = 0x00;

	FIRELOG_MSG(( "IOFireWireAVCProtocolUserClient::addSubunit (%p)\n", this));

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
	FIRELOG_MSG(( "IOFireWireAVCProtocolUserClient::setSubunitPlugSignalFormat (%p)\n", this));

	return fAVCTargetSpace->setSubunitPlugSignalFormat(this,subunitTypeAndID, plugType, plugNum, signalFormat);
}

IOReturn IOFireWireAVCProtocolUserClient::getSubunitPlugSignalFormat(UInt32 subunitTypeAndID,
											IOFWAVCPlugTypes plugType,
											UInt32 plugNum,
											uint64_t *pSignalFormat)
{
	FIRELOG_MSG(( "IOFireWireAVCProtocolUserClient::getSubunitPlugSignalFormat (%p)\n", this));

	UInt32 signalFormat;
	IOReturn status;
	
	status = fAVCTargetSpace->getSubunitPlugSignalFormat(this,subunitTypeAndID, plugType, plugNum, &signalFormat);

	*pSignalFormat = signalFormat;
	
	return status;
}

IOReturn IOFireWireAVCProtocolUserClient::connectTargetPlugs(AVCConnectTargetPlugsInParams *inParams,
															 AVCConnectTargetPlugsOutParams *outParams)
{
	FIRELOG_MSG(( "IOFireWireAVCProtocolUserClient::connectTargetPlugs (%p)\n", this));

	return fAVCTargetSpace->connectTargetPlugs(this,inParams,outParams);
}

IOReturn IOFireWireAVCProtocolUserClient::disconnectTargetPlugs(UInt32 sourceSubunitTypeAndID,
									   IOFWAVCPlugTypes sourcePlugType,
									   UInt32 sourcePlugNum,
									   UInt32 destSubunitTypeAndID,
									   IOFWAVCPlugTypes destPlugType,
									   UInt32 destPlugNum)
{
	FIRELOG_MSG(( "IOFireWireAVCProtocolUserClient::disconnectTargetPlugs (%p)\n", this));

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
	FIRELOG_MSG(( "IOFireWireAVCProtocolUserClient::getTargetPlugConnection (%p)\n", this));

	return fAVCTargetSpace->getTargetPlugConnection(this,inParams,outParams);
}

IOReturn IOFireWireAVCProtocolUserClient::AVCRequestNotHandled(UInt32 generation,
									  UInt16 nodeID,
									  IOFWSpeed speed,
									  UInt32 handlerSearchIndex,
									  const char *pCmdBuf,
									  UInt32 cmdLen)
{
	FIRELOG_MSG(( "IOFireWireAVCProtocolUserClient::AVCRequestNotHandled (%p)\n", this));

	fAVCTargetSpace->findAVCRequestHandler(this,
							 generation,
							 nodeID,
							 speed,
							 handlerSearchIndex,
							 pCmdBuf,
							 cmdLen);

	return kIOReturnSuccess;
}


