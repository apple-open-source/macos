/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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
#include <IOKit/firewire/IOFireWireController.h>
#include <IOKit/firewire/IOLocalConfigDirectory.h>

#include <IOKit/avc/IOFireWireAVCConsts.h>
#include <IOKit/avc/IOFireWireAVCTargetSpace.h>

#define AVCTARGETMUTEX_LOCK fController->closeGate()
#define AVCTARGETMUTEX_UNLOCK fController->openGate()

// Local Prototypes
static void AVCTargetSendAVCResponseComplete(void *refcon, IOReturn status, IOFireWireNub *device, IOFWCommand *fwCmd);

OSDefineMetaClassAndStructors(AVCCommandHandlerInfo, OSObject)
OSDefineMetaClassAndStructors(AVCSubunitInfo, OSObject)
OSDefineMetaClassAndStructors(UCInfo, OSObject)
OSDefineMetaClassAndStructors(AVCConnectionRecord, OSObject)

//////////////////////////////////////////////////////
// AVCSubunitInfo::create
//////////////////////////////////////////////////////
AVCSubunitInfo *AVCSubunitInfo::create()
{
	AVCSubunitInfo * subUnitInfo;

	subUnitInfo = new AVCSubunitInfo;

    if( subUnitInfo != NULL && !subUnitInfo->init())
	{
        subUnitInfo->release();
        subUnitInfo = NULL;
    }

    return subUnitInfo;
}

//////////////////////////////////////////////////////
// AVCSubunitInfo::init
//////////////////////////////////////////////////////
bool AVCSubunitInfo::init()
{
	bool success = true;

	//IOLog( "AVCSubunitInfo::init (0x%08X)\n",(int) this);

	// init super
    if( !OSObject::init() )
        success = false;

	if( success )
	{
		// Initialize this object
	}

	return success;
}

//////////////////////////////////////////////////////
// AVCSubunitInfo::free
//////////////////////////////////////////////////////
void AVCSubunitInfo::free()
{
    //IOLog( "AVCSubunitInfo::free (0x%08X)\n",(int) this);

	if (sourcePlugRecords)
		delete[] sourcePlugRecords;

	if (destPlugRecords)
		delete[] destPlugRecords;

	OSObject::free();
}

OSDefineMetaClassAndStructors(IOFireWireAVCTargetSpace, IOFWPseudoAddressSpace)
OSMetaClassDefineReservedUnused(IOFireWireAVCTargetSpace, 0);
OSMetaClassDefineReservedUnused(IOFireWireAVCTargetSpace, 1);
OSMetaClassDefineReservedUnused(IOFireWireAVCTargetSpace, 2);
OSMetaClassDefineReservedUnused(IOFireWireAVCTargetSpace, 3);

//////////////////////////////////////////////////////
// AVCTargetSendAVCResponseComplete
//////////////////////////////////////////////////////
void AVCTargetSendAVCResponseComplete(void *refcon, IOReturn status, IOFireWireNub *device, IOFWCommand *fwCmd)
{
	//IOLog( "AVCTargetSendAVCResponseComplete Command=0x%08X  Status=0x%08X\n",(int)fwCmd,status);

	IOBufferMemoryDescriptor *pBufMemDesc = (IOBufferMemoryDescriptor*) refcon;

	// Free the command and the associated memory descriptor
    if(fwCmd)
        fwCmd->release();
	if(pBufMemDesc)
		pBufMemDesc->release();
}

//////////////////////////////////////////////////////
// IOFireWireAVCTargetSpace::init
//////////////////////////////////////////////////////
bool IOFireWireAVCTargetSpace::init(IOFireWireController *controller)
{
    //IOLog( "IOFireWireAVCTargetSpace::init (0x%08X)\n",(int) this);


    if(!IOFWPseudoAddressSpace::initFixed(controller,
										  FWAddress(kCSRRegisterSpaceBaseAddressHi, kFCPCommandAddress),
										  512, NULL, NULL, this))
		return false;

	// Save the pointer to the bus
	fController = controller;
	
    return true;
}

//////////////////////////////////////////////////////
// IOFireWireAVCTargetSpace::getAVCTargetSpace
//////////////////////////////////////////////////////
IOFireWireAVCTargetSpace * IOFireWireAVCTargetSpace::getAVCTargetSpace(IOFireWireController *controller)
{
    IOFWAddressSpace *existing;
    IOFireWireAVCTargetSpace *space;

	//IOLog( "IOFireWireAVCTargetSpace::getAVCTargetSpace\n");

    existing = controller->getAddressSpace(FWAddress(kCSRRegisterSpaceBaseAddressHi, kFCPCommandAddress));
    if(existing && OSDynamicCast(IOFireWireAVCTargetSpace, existing)) {
        existing->retain();
        return OSDynamicCast(IOFireWireAVCTargetSpace, existing);
    }
    space = new IOFireWireAVCTargetSpace;
    if(space) {
        if(!space->init(controller)) {
            space->release();
            space = NULL;
        }
    }
    return space;
}

//////////////////////////////////////////////////////
// IOFireWireAVCTargetSpace::doWrite
//////////////////////////////////////////////////////
UInt32 IOFireWireAVCTargetSpace::doWrite(UInt16 nodeID, IOFWSpeed &speed, FWAddress addr, UInt32 len,
								   const void *buf, IOFWRequestRefCon refcon)
{
	UInt32 generation = fController->getGeneration();
	UInt8 cts;
	UInt8 *pBuf = (UInt8*) buf;

	//IOLog( "IOFireWireAVCTargetSpace::doWrite (0x%08X)\n",(int) this);

	if(addr.addressHi != kCSRRegisterSpaceBaseAddressHi)
        return kFWResponseAddressError;
    if(addr.addressLo != kFCPCommandAddress)
        return kFWResponseAddressError;

	// Make sure this is an AVC command (CTS is 0)
	cts =  ((pBuf[0] & 0xF0) >> 4);
	if (cts != 0)
        return kFWResponseAddressError;

	// Find a handler for this command, or send not implemented response
	findAVCRequestHandler(NULL,generation,nodeID,speed,0xFFFFFFFF,(const char *) pBuf,len);

	return kFWResponseComplete;
}

//////////////////////////////////////////////////////
// IOFireWireAVCTargetSpace::findAVCRequestHandler
//////////////////////////////////////////////////////
IOReturn IOFireWireAVCTargetSpace::findAVCRequestHandler(IOFireWireAVCProtocolUserClient *userClient,
														UInt32 generation,
														UInt16 nodeID,
														IOFWSpeed speed,
														UInt32 handlerSearchIndex,
														const char *pCmdBuf,
														UInt32 cmdLen)
{
	UInt32 currentGeneration = fController->getGeneration();
	UInt8 subUnit;
	UInt8 opCode;
	UInt8 *pResponse;
	bool handled = false;
	IOBufferMemoryDescriptor *pBufMemDesc = NULL;
	int i;
	int firstIndex;

	// Parse the command a bit
	subUnit = pCmdBuf[1];
	opCode = pCmdBuf[2];

	//IOLog( "IOFireWireAVCTargetSpace::findAVCRequestHandler (0x%08X) subUnit=%02X  opCode=%02X\n",(int) this,subUnit,opCode);

	// See if we are still in the same bus generation as the command
	if (currentGeneration != generation)
		return kIOFireWireBusReset;

	AVCTARGETMUTEX_LOCK;
	
	if (handlerSearchIndex == 0xFFFFFFFF)
		firstIndex = (fCommandHandlers->getCount()-1);
	else
	{
		firstIndex = handlerSearchIndex - 1;

		// Make sure that we don't index to a command handler record that doesn't exist
		if (firstIndex >= (int) fCommandHandlers->getCount())
			firstIndex = (fCommandHandlers->getCount()-1);
	}

	// First, search the array of registered command handlers
	for (i=firstIndex;i>=0;i--)
	{
		AVCCommandHandlerInfo *cmdInfo;
		cmdInfo = (AVCCommandHandlerInfo*) fCommandHandlers->getObject(i);

		if ( ((cmdInfo->subUnitTypeAndID == subUnit) && (cmdInfo->opCode == opCode)) ||
	   ((cmdInfo->subUnitTypeAndID == subUnit) && (cmdInfo->opCode == kAVCAllOpcodes)) ||
	   ((cmdInfo->subUnitTypeAndID == kAVCAllSubunitsAndUnit) && (cmdInfo->opCode == opCode)) ||
	   ((cmdInfo->subUnitTypeAndID == kAVCAllSubunitsAndUnit) && (cmdInfo->opCode == kAVCAllOpcodes)))
		{
			// Call back the command's handler to pass the command up to user space
			cmdInfo->callBack(cmdInfo,generation,nodeID,pCmdBuf,cmdLen,speed,i);
			handled = true;
			break;
		}
	}
	
	// If not hadled by registered handler, try and Find an Internall Command Handler for this command
	if ((!handled) && (subUnit == 0xFF))
	{
		// See if we have an internal unit command handler for this opcode
		switch (opCode)
		{
			case kAVCUnitInfoOpcode:
				if (handleUnitInfoCommand(nodeID, generation, pCmdBuf, cmdLen) == kIOReturnSuccess)
					handled = true;
				break;

			case kAVCSubunitInfoOpcode:
				if (handleSubUnitInfoCommand(nodeID, generation, pCmdBuf, cmdLen) == kIOReturnSuccess)
					handled = true;
				break;

			case kAVCPowerOpcode:
				if (handlePowerCommand(nodeID, generation, pCmdBuf, cmdLen) == kIOReturnSuccess)
					handled = true;
				break;

			case kAVCConnectOpcode:
				if (handleConnectCommand(nodeID, generation, pCmdBuf, cmdLen) == kIOReturnSuccess)
					handled = true;
				break;

			case kAVCDisconnectOpcode:
				if (handleDisconnectCommand(nodeID, generation, pCmdBuf, cmdLen) == kIOReturnSuccess)
					handled = true;
				break;

			case kAVCInputPlugSignalFormatOpcode:
				if (handleInputPlugSignalFormatCommand(nodeID, generation, pCmdBuf, cmdLen) == kIOReturnSuccess)
					handled = true;
				break;

			case kAVCOutputPlugSignalFormatOpcode:
				if (handleOutputPlugSignalFormatCommand(nodeID, generation, pCmdBuf, cmdLen) == kIOReturnSuccess)
					handled = true;
				break;

			case kAVCConnectionsOpcode:
				if (handleConnectionsCommand(nodeID, generation, pCmdBuf, cmdLen) == kIOReturnSuccess)
					handled = true;
				break;

			case kAVCSignalSourceOpcode:
				if (handleSignalSourceCommand(nodeID, generation, pCmdBuf, cmdLen) == kIOReturnSuccess)
					handled = true;
				break;

			default:
				break;
		};
	}

	if ((!handled) && (opCode == kAVCPlugInfoOpcode))
	{
		// Internally handle the Plug Info command for the unit and all subunits
		if (handlePlugInfoCommand(nodeID, generation, pCmdBuf, cmdLen) == kIOReturnSuccess)
			handled = true;
	}

	// If not handled, send a Not Implemented Response
	if (!handled)
	{
		//IOLog("DEBUG: findAVCRequestHandler found no handler for command!\n");
        pBufMemDesc = IOBufferMemoryDescriptor::withCapacity(cmdLen, kIODirectionOutIn);
        if(!pBufMemDesc)
		{
			AVCTARGETMUTEX_UNLOCK;
			return kFWResponseDataError;
		}
		pResponse = (UInt8 *) pBufMemDesc->getBytesNoCopy();
		bcopy(pCmdBuf,pResponse,cmdLen);
		pResponse[kAVCCommandResponse] = kAVCNotImplementedStatus;	// Not Implemented
		targetSendAVCResponse(generation, nodeID, pBufMemDesc, cmdLen);
	}

	AVCTARGETMUTEX_UNLOCK;

	return kIOReturnSuccess;
}

//////////////////////////////////////////////////////
// IOFireWireAVCTargetSpace::activateWithUserClient
//////////////////////////////////////////////////////
IOReturn IOFireWireAVCTargetSpace::activateWithUserClient(IOFireWireAVCProtocolUserClient *userClient)
{
    IOReturn res = kIOReturnSuccess;
	UCInfo *uc;
	UInt32 i;
	
	//IOLog( "IOFireWireAVCTargetSpace::activateWithUserClient (0x%08X)\n",(int) this);

	if(!fActivations++)
	{
		fUserClients = OSArray::withCapacity(1);
		fCommandHandlers = OSArray::withCapacity(1);
		fSubunits = OSArray::withCapacity(1);
		fConnectionRecords = OSArray::withCapacity(1); 
		
		// Setup Unit plug management
		fUnitPlugs.numIsochInPlugs = kAVCMaxNumPlugs;
		fUnitPlugs.numIsochOutPlugs = kAVCMaxNumPlugs;
		fUnitPlugs.numExternalInPlugs = kAVCMaxNumPlugs;
		fUnitPlugs.numExternalOutPlugs = kAVCMaxNumPlugs;
		for (i=0;i<kAVCMaxNumPlugs;i++)
		{
			fUnitPlugs.isochInPlugRecord[i].connectionCount = 0;
			fUnitPlugs.isochOutPlugRecord[i].connectionCount = 0;
			fUnitPlugs.externalInPlugRecord[i].connectionCount = 0;
			fUnitPlugs.externalOutPlugRecord[i].connectionCount = 0;
		}

#if 0		
		// Create the lock to protect data structures
		fLock = IORecursiveLockAlloc();
#endif
		
		res = IOFWAddressSpace::activate();
	}

	// Save User Client pointer in array
	uc = new UCInfo;
    if(!uc)
        return kIOReturnNoMemory;
	uc->fUserClient = userClient;
	if(!fUserClients->setObject(uc))
        return kIOReturnNoMemory;
	uc->release();
	//IOLog( "DEBUG: activateWithUserClient added client to array (0x%08X)\n",(int) userClient);

	
    return res;
}

//////////////////////////////////////////////////////
// IOFireWireAVCTargetSpace::deactivateWithUserClient
//////////////////////////////////////////////////////
void IOFireWireAVCTargetSpace::deactivateWithUserClient(IOFireWireAVCProtocolUserClient *userClient)
{
    IOReturn res = kIOReturnSuccess;
	int i,j;
	UCInfo *uc;
	AVCConnectionRecord *connection;
	AVCSubunitInfo *subUnitInfo;
	AVCSubunitInfo *connectedSubUnitInfo;
	bool avcResourcesFreed = false;
	UInt32 subunitTypeAndID;

	//IOLog( "IOFireWireAVCTargetSpace::deactivateWithUserClient (0x%08X)\n",(int) this);

	AVCTARGETMUTEX_LOCK;
	
	// Free any allocated command handlers for this user client
	for (i=(fCommandHandlers->getCount()-1);i>=0;i--)
	{
		AVCCommandHandlerInfo *cmdInfo;
		cmdInfo = (AVCCommandHandlerInfo*) fCommandHandlers->getObject(i);
        if(cmdInfo->userClient == userClient)
		{
			fCommandHandlers->removeObject(i);
			//IOLog( "DEBUG: deactivateWithUserClient found cmd handler to remove (0x%08X)\n",(int) userClient);
		}
	}

	// Free any allocated subunits for this user client
	for (i=(fSubunits->getCount()-1);i>=0;i--)
	{
		subUnitInfo = (AVCSubunitInfo*) fSubunits->getObject(i);
		if(subUnitInfo->userClient == userClient)
		{
			subunitTypeAndID = subUnitInfo->subunitTypeAndID;

			// Disconnect all plugs on this subunit
			for (j=(fConnectionRecords->getCount()-1);j>=0;j--)
			{
				connection = (AVCConnectionRecord*) fConnectionRecords->getObject(j);

				// See if this connection record is for this subunit
				if ((connection->sourceSubunitTypeAndID == subunitTypeAndID) ||
		(connection->destSubunitTypeAndID == subunitTypeAndID))
				{
					if (connection->sourceSubunitTypeAndID != subunitTypeAndID)
					{
						switch (connection->sourcePlugType)
						{
							case IOFWAVCPlugIsochInputType:
								fUnitPlugs.isochInPlugRecord[connection->sourcePlugNum].connectionCount--;
								break;
							case IOFWAVCPlugExternalInputType:
								fUnitPlugs.externalInPlugRecord[connection->sourcePlugNum - 0x80].connectionCount--;
								break;
							case IOFWAVCPlugSubunitSourceType:
								connectedSubUnitInfo = getSubunitInfo(connection->sourceSubunitTypeAndID);
								if (connectedSubUnitInfo)
								{
									connectedSubUnitInfo->sourcePlugRecords[connection->sourcePlugNum].connectionCount--;

									// If the connected plug is a subunit plug, and this
									// user client doesn't own that subunit, notify the owner
									// of the plug's disconnection
									if (connectedSubUnitInfo->userClient != userClient)
										connectedSubUnitInfo->callBack(connectedSubUnitInfo,
										 kIOFWAVCSubunitPlugMsgDisconnected,
										 connection->sourcePlugType,
										 connection->sourcePlugNum,
										 ((connection->destSubunitTypeAndID << 16) + (connection->destPlugType << 8) + connection->destPlugNum),
										 0,0);
								}
								break;
							default:
								break;
						};
						
					}

					if (connection->destSubunitTypeAndID != subunitTypeAndID)
					{
						switch (connection->destPlugType)
						{
							case IOFWAVCPlugIsochOutputType:
								fUnitPlugs.isochOutPlugRecord[connection->destPlugNum].connectionCount--;
								break;
							case IOFWAVCPlugExternalOutputType:
								fUnitPlugs.externalOutPlugRecord[connection->destPlugNum - 0x80].connectionCount--;
								break;
							case IOFWAVCPlugSubunitDestType:
								connectedSubUnitInfo = getSubunitInfo(connection->destSubunitTypeAndID);
								if (connectedSubUnitInfo)
								{
									connectedSubUnitInfo->destPlugRecords[connection->destPlugNum].connectionCount--;

									// If the connected plug is a subunit plug, and this
									// user client doesn't own that subunit, notify the owner
									// of the plug's disconnection
									if (connectedSubUnitInfo->userClient != userClient)
										connectedSubUnitInfo->callBack(connectedSubUnitInfo,
										 kIOFWAVCSubunitPlugMsgDisconnected,
										 connection->destPlugType,
										 connection->destPlugNum,
										 ((connection->sourceSubunitTypeAndID << 16) + (connection->sourcePlugType << 8) + connection->sourcePlugNum),
										 0,0);
								}
								break;
							default:
								break;
						};
					}
					
					// Remove this connection record
					fConnectionRecords->removeObject(j);
				}
			}
			
			fSubunits->removeObject(i);
			//IOLog( "DEBUG: deactivateWithUserClient found subunit to remove (0x%08X)\n",(int) userClient);

			avcResourcesFreed = true;
		}
	}
	
	// Find this user client in the user client array, and remove it
	for (i=(fUserClients->getCount()-1);i>=0;i--)
	{
        uc = (UCInfo *)fUserClients->getObject(i);
        if(uc->fUserClient == userClient)
		{
			fUserClients->removeObject(i);
			//IOLog( "DEBUG: deactivateWithUserClient found client to remove (0x%08X)\n",(int) userClient);
		}
	}

	// Decrement activations count
	fActivations -= 1;

	// If there is only one activation left, get the user-client info for it.
	if (fActivations == 1)
        uc = (UCInfo *)fUserClients->getObject(0);

	AVCTARGETMUTEX_UNLOCK;

	// If we are down to no activations, or if we have one remaining activation
	// and it is the IOFireWirePCRSpace's activation, remove the AVC unit
	// directory, if it exists.
	if ((fActivations == 0) || ((fActivations == 1) && (uc->fUserClient == (IOFireWireAVCProtocolUserClient*)0xFFFFFFFF)))
	{
		// If we've published an AVC Unit directory, now's the time to remove it
		if (fAVCLocalConfigDirectory)
		{
			res = fController->RemoveUnitDirectory(fAVCLocalConfigDirectory) ;

			// Release the fAVCLocalConfigDirectory object
			fAVCLocalConfigDirectory->release();
			fAVCLocalConfigDirectory = NULL;
		}
	}
	else if (avcResourcesFreed == true)
		fController->resetBus();

	if (fActivations == 0)
	{

#if 0		
		// Release the lock
		if (fLock)
			IORecursiveLockFree(fLock);
#endif
		
		// Release the OSArrays
		fUserClients->release();
		fCommandHandlers->release();
		fSubunits->release();
		fConnectionRecords->release();
		
		IOFWAddressSpace::deactivate();
	}
}

//////////////////////////////////////////////////////
// IOFireWireAVCTargetSpace::publishAVCUnitDirectory
//////////////////////////////////////////////////////
IOReturn IOFireWireAVCTargetSpace::publishAVCUnitDirectory(void)
{
	//IOLog( "IOFireWireAVCTargetSpace::publishAVCUnitDirectory (0x%08X)\n",(int) this);

    IOReturn res = kIOReturnSuccess;

	AVCTARGETMUTEX_LOCK;
	
	// Only do this once.
	if (!fAVCLocalConfigDirectory)
	{
		// Create entries for UnitSpecID and UnitSwVersion
		fAVCLocalConfigDirectory = IOLocalConfigDirectory::create();

		if (!fAVCLocalConfigDirectory)
		{
			res =  kIOReturnError;
		}

		AVCTARGETMUTEX_UNLOCK;

		if(res == kIOReturnSuccess)
			res = fAVCLocalConfigDirectory->addEntry(0x12,0xA02D) ;

		if(res == kIOReturnSuccess)
			res = fAVCLocalConfigDirectory->addEntry(0x13,0x10001) ;

		// lets publish it
		if(res == kIOReturnSuccess)
			res = fController->AddUnitDirectory(fAVCLocalConfigDirectory) ;
	}
	else
	{
		AVCTARGETMUTEX_UNLOCK;

		// The AVC Unit directory already exists, so just do a bus reset now.
		fController->resetBus();
	}
	
	// end of unit directory addition
    return res;
}

//////////////////////////////////////////////////////
// IOFireWireAVCTargetSpace::targetSendAVCResponse
//////////////////////////////////////////////////////
IOReturn
IOFireWireAVCTargetSpace::targetSendAVCResponse(UInt32 generation, UInt16 nodeID, IOBufferMemoryDescriptor *pBufMemDesc, UInt32 size)
{
	//IOLog( "IOFireWireAVCTargetSpace::targetSendAVCResponse (0x%08X)\n",(int) this);

    IOFWWriteCommand *cmd = NULL;
    IOReturn status;
    do {
        cmd = new IOFWWriteCommand;
        if(!cmd) {
            status = kIOReturnNoMemory;
            break;
        }
        if(!cmd->initAll(fController, generation,
						 FWAddress(kCSRRegisterSpaceBaseAddressHi, kFCPResponseAddress, nodeID),
						 pBufMemDesc, AVCTargetSendAVCResponseComplete, pBufMemDesc)) {
            status = kIOReturnNoMemory;
            break;
        }
        status = cmd->submit(true);
    } while (false);

    return status;
}

//////////////////////////////////////////////////////
// IOFireWireAVCTargetSpace::installAVCCommandHandler
//////////////////////////////////////////////////////
IOReturn IOFireWireAVCTargetSpace::installAVCCommandHandler(IOFireWireAVCProtocolUserClient *userClient,
															IOFireWireAVCTargetCommandHandlerCallback callBack,
															OSAsyncReference64 asyncRef,
															UInt32 subUnitTypeAndID,
															UInt32 opCode,
															uint64_t userCallBack,
															uint64_t userRefCon)
{
    IOReturn res = kIOReturnSuccess;
	AVCCommandHandlerInfo *cmdInfo;

	//IOLog( "IOFireWireAVCTargetSpace::installAVCCommandHandler (0x%08X)\n",(int) this);

	cmdInfo = new AVCCommandHandlerInfo;
    if(!cmdInfo)
        return kIOReturnNoMemory;
	
	cmdInfo->userClient = userClient;
	cmdInfo->callBack = callBack;
	bcopy(asyncRef, cmdInfo->asyncRef, sizeof(OSAsyncReference64));
	cmdInfo->subUnitTypeAndID = subUnitTypeAndID;
	cmdInfo->opCode = opCode;
	cmdInfo->userCallBack = userCallBack;
	cmdInfo->userRefCon = userRefCon;
	
	AVCTARGETMUTEX_LOCK;

	// Save command handler info in array
	if(!fCommandHandlers->setObject(cmdInfo))
        res = kIOReturnNoMemory;
	else
	{
		//IOLog( "DEBUG: installAVCCommandHandler added cmd handler to array (0x%08X)\n",(int) userClient);
	}

	AVCTARGETMUTEX_UNLOCK;
	
	cmdInfo->release();

	return res;
}

//////////////////////////////////////////////////////
// IOFireWireAVCTargetSpace::addSubunit
//////////////////////////////////////////////////////
IOReturn IOFireWireAVCTargetSpace::addSubunit(IOFireWireAVCProtocolUserClient *userClient,
											  IOFireWireAVCSubunitPlugHandlerCallback callBack,
											  OSAsyncReference64 asyncRef,
											  UInt32 subunitType,
											  UInt32 numSourcePlugs,
											  UInt32 numDestPlugs,
											  uint64_t userCallBack,
											  uint64_t userRefCon,
											  UInt32 *subUnitID)
{
    IOReturn res = kIOReturnSuccess;
	AVCSubunitInfo *subUnitInfo;
	UInt32 count;
	UInt32 i;

	//IOLog( "IOFireWireAVCTargetSpace::addSubunit (0x%08X)\n",(int) this);

	AVCTARGETMUTEX_LOCK;
	
	// Make sure that we don't have too many
	// subunits already, or too many of the specified type
	if (fSubunits->getCount() >= 32)
		res = kIOReturnNoResources;
	count = subUnitOfTypeCount(subunitType);
	if (count >= 4)
		res = kIOReturnNoResources;

	// Create a new subunit info object
	if (res == kIOReturnSuccess)
	{	
		subUnitInfo = AVCSubunitInfo::create();
		if(!subUnitInfo)
			res = kIOReturnNoMemory;
		else
		{
			// Initialize the new object's parameters
			subUnitInfo->userClient = userClient;
			subUnitInfo->callBack = callBack;
			bcopy(asyncRef, subUnitInfo->asyncRef, sizeof(OSAsyncReference64));
			subUnitInfo->numSourcePlugs = numSourcePlugs;
			subUnitInfo->numDestPlugs = numDestPlugs;
			subUnitInfo->userCallBack = userCallBack;
			subUnitInfo->userRefCon = userRefCon;

			// Allocate the plug records for the source plugs
			if (numSourcePlugs)
			{
				subUnitInfo->sourcePlugRecords = new AVCSubunitPlugRecord[numSourcePlugs];
				for (i=0;i<numSourcePlugs;i++)
				{
					subUnitInfo->sourcePlugRecords[i].connectionCount = 0;
					subUnitInfo->sourcePlugRecords[i].plugSignalFormat = kAVCPlugSignalFormatNTSCDV;
				}
			}
			else
				subUnitInfo->sourcePlugRecords = NULL;

			// Allocate the plug records for the destination plugs
			if (numDestPlugs)
			{
				subUnitInfo->destPlugRecords = new AVCSubunitPlugRecord[numDestPlugs];
				for (i=0;i<numDestPlugs;i++)
				{
					subUnitInfo->destPlugRecords[i].connectionCount = 0;
					subUnitInfo->destPlugRecords[i].plugSignalFormat = kAVCPlugSignalFormatNTSCDV;
				}
			}
			else
				subUnitInfo->destPlugRecords = NULL;

			// Determine the ID for this subunit
			subUnitInfo->subunitTypeAndID = (subunitType << 3) + count;

			// Save command handler info in array
			if(!fSubunits->setObject(subUnitInfo))
				res = kIOReturnNoMemory;
			else
			{
				//IOLog( "DEBUG: addSubunit added subunit to array (0x%08X)\n",(int) userClient);
			}
		}

		*subUnitID  = subUnitInfo->subunitTypeAndID;
		subUnitInfo->release();
	}

	AVCTARGETMUTEX_UNLOCK;

	return res;
}

//////////////////////////////////////////////////////
// IOFireWireAVCTargetSpace::setSubunitPlugSignalFormat
//////////////////////////////////////////////////////
IOReturn IOFireWireAVCTargetSpace::setSubunitPlugSignalFormat(IOFireWireAVCProtocolUserClient *userClient,
															  UInt32 subunitTypeAndID,
															  IOFWAVCPlugTypes plugType,
															  UInt32 plugNum,
															  UInt32 signalFormat)
{
	IOReturn res = kIOReturnBadArgument;	// Preinitialize with an error
	AVCSubunitInfo *subUnitInfo;
	
	//IOLog( "IOFireWireAVCTargetSpace::setSubunitPlugSignalFormat (0x%08X)\n",(int) this);

	AVCTARGETMUTEX_LOCK;

	// See if this subunit exists
	subUnitInfo = getSubunitInfo(subunitTypeAndID);
	if (subUnitInfo)
		res = kIOReturnSuccess;
	
	// See if the caller owns this subunit
	if ((res == kIOReturnSuccess) && (userClient != subUnitInfo->userClient))
			res = kIOReturnBadArgument;

	// If this is a valid plug, set its signal format
	if (res == kIOReturnSuccess)
	{
		if ((plugType == IOFWAVCPlugSubunitSourceType) && (plugNum < subUnitInfo->numSourcePlugs))
			subUnitInfo->sourcePlugRecords[plugNum].plugSignalFormat = signalFormat;
		else if ((plugType == IOFWAVCPlugSubunitDestType) && (plugNum < subUnitInfo->numDestPlugs))
			subUnitInfo->destPlugRecords[plugNum].plugSignalFormat = signalFormat;
		else
			res = kIOReturnBadArgument;
	}

	AVCTARGETMUTEX_UNLOCK;
	
	return res;
}

//////////////////////////////////////////////////////
// IOFireWireAVCTargetSpace::getSubunitPlugSignalFormat
//////////////////////////////////////////////////////
IOReturn IOFireWireAVCTargetSpace::getSubunitPlugSignalFormat(IOFireWireAVCProtocolUserClient *userClient,
											UInt32 subunitTypeAndID,
											IOFWAVCPlugTypes plugType,
											UInt32 plugNum,
											UInt32 *pSignalFormat)
{
	IOReturn res = kIOReturnBadArgument;	// Preinitialize with an error
	AVCSubunitInfo *subUnitInfo;
	
	//IOLog( "IOFireWireAVCTargetSpace::getSubunitPlugSignalFormat (0x%08X)\n",(int) this);

	AVCTARGETMUTEX_LOCK;

	// Special Handling for unit plugs
	if (subunitTypeAndID == kAVCUnitAddress)
	{
		// TODO: For unit commands, find connected subunit plug (if exists)
		// and return its signal format. For now just return an error!
		res = kIOReturnBadArgument;
	}
	else
	{
		// See if this subunit exists
		subUnitInfo = getSubunitInfo(subunitTypeAndID);
		if (subUnitInfo)
			res = kIOReturnSuccess;
		
		// If this is a valid plug, get its signal format
		if (res == kIOReturnSuccess)
		{
			if ((plugType == IOFWAVCPlugSubunitSourceType) && (plugNum < subUnitInfo->numSourcePlugs))
				*pSignalFormat = subUnitInfo->sourcePlugRecords[plugNum].plugSignalFormat;
			else if ((plugType == IOFWAVCPlugSubunitDestType) && (plugNum < subUnitInfo->numDestPlugs))
				*pSignalFormat = subUnitInfo->destPlugRecords[plugNum].plugSignalFormat;
			else
				res = kIOReturnBadArgument;
		}
	}

	AVCTARGETMUTEX_UNLOCK;

	return res;
}

//////////////////////////////////////////////////////
// IOFireWireAVCTargetSpace::connectTargetPlugs
//////////////////////////////////////////////////////
IOReturn IOFireWireAVCTargetSpace::connectTargetPlugs(IOFireWireAVCProtocolUserClient *userClient,
													  AVCConnectTargetPlugsInParams *inParams,
													  AVCConnectTargetPlugsOutParams *outParams)
{
	IOReturn res = kIOReturnSuccess;
	AVCSubunitInfo *sourceSubUnitInfo = NULL;
	AVCSubunitInfo *destSubUnitInfo = NULL;
	UInt32 sourcePlugIndex;
	UInt32 destPlugIndex;
	UInt32 sourcePlugNum;
	UInt32 destPlugNum;
	int i;
	bool found;
	bool plugFound;
	AVCConnectionRecord *connection;
	UInt32 actualPlug;
	
	//IOLog( "IOFireWireAVCTargetSpace::connectTargetPlugs (0x%08X)\n",(int) this);

	AVCTARGETMUTEX_LOCK;
	
	// See if this connection already exists
	for (i=(fConnectionRecords->getCount()-1);i>=0;i--)
	{
		connection = (AVCConnectionRecord *) fConnectionRecords->getObject(i);
		if ((inParams->sourceSubunitTypeAndID == connection->sourceSubunitTypeAndID) &&
	  (inParams->sourcePlugType == connection->sourcePlugType) &&
	  (inParams->sourcePlugNum == connection->sourcePlugNum) &&
	  (inParams->destSubunitTypeAndID == connection->destSubunitTypeAndID) &&
	  (inParams->destPlugType == connection->destPlugType) &&
	  (inParams->destPlugNum == connection->destPlugNum))
		{
			// Connection exists. Set lock and/or perm
			// if specified, and return success
			
			if (inParams->lockConnection == true)
				connection->lockConnection = true;

			if (inParams->permConnection == true)
				connection->permConnection = true;

			outParams->sourcePlugNum = connection->sourcePlugNum;
			outParams->destPlugNum = connection->destPlugNum;
			
			AVCTARGETMUTEX_UNLOCK;
			return res;
		}
	}
	
	// Identify Source Plug for Connection
	if (inParams->sourceSubunitTypeAndID == kAVCUnitAddress)
	{
		switch (inParams->sourcePlugType)
		{
			case IOFWAVCPlugIsochInputType:
				if (inParams->sourcePlugNum == kAVCAnyAvailableIsochPlug)
				{
					// Search for an unconnected plug first,
					// then, if all connected, overlay a connection.
					plugFound = false;
					for (i=0;i<kAVCMaxNumPlugs;i++)
					{
						if (fUnitPlugs.isochInPlugRecord[i].connectionCount == 0)
						{
							sourcePlugIndex=i;
							sourcePlugNum=i;
							plugFound = true;
							break;
						}
					}
					if (!plugFound)
					{
						// No available plugs, just overlay on first plug.
						sourcePlugIndex=0;
						sourcePlugNum=0;
					}
				}
				else if (inParams->sourcePlugNum < kAVCMaxNumPlugs)
				{
					sourcePlugIndex = inParams->sourcePlugNum;
					sourcePlugNum = sourcePlugIndex;
				}
				else
					res = kIOReturnBadArgument;
				break;

			case IOFWAVCPlugExternalInputType:
				if (inParams->sourcePlugNum == kAVCAnyAvailableExternalPlug)
				{
					// Search for an unconnected plug first,
					// then, if all connected, overlay a connection.
					plugFound = false;
					for (i=0;i<kAVCMaxNumPlugs;i++)
					{
						if (fUnitPlugs.externalInPlugRecord[i].connectionCount == 0)
						{
							sourcePlugIndex=i;
							sourcePlugNum= 0x80 + i;
							plugFound = true;
							break;
						}
					}
					if (!plugFound)
					{
						// No available plugs, just overlay on first plug.
						sourcePlugIndex=0;
						sourcePlugNum = 0x80;
					}
				}
				else if ((inParams->sourcePlugNum >= 0x80) && (inParams->sourcePlugNum < 0x9F))
				{
					sourcePlugIndex = inParams->sourcePlugNum - 0x80;
					sourcePlugNum = inParams->sourcePlugNum;
				}
				else
					res = kIOReturnBadArgument;
				break;
				
			default:
				res = kIOReturnBadArgument;
				break;
		};
	}
	else
	{
		found = false;
		for (i=(fSubunits->getCount()-1);i>=0;i--)
		{
			sourceSubUnitInfo = (AVCSubunitInfo *) fSubunits->getObject(i);
			if (inParams->sourceSubunitTypeAndID == sourceSubUnitInfo->subunitTypeAndID)
			{
				found = true;
				break;
			}
		}
		if (found == false)
			res = kIOReturnBadArgument;
		
		// For internally initiated connection, verify that the userClient owns this subunit
		if ((res == kIOReturnSuccess) && (userClient != NULL) && (sourceSubUnitInfo->userClient != userClient))
			res = kIOReturnBadArgument;
		
		if (res == kIOReturnSuccess)
		{
			switch (inParams->sourcePlugType)
			{
				case IOFWAVCPlugSubunitSourceType:
					if ((inParams->sourcePlugNum == kAVCAnyAvailableSubunitPlug) && (sourceSubUnitInfo->numSourcePlugs > 0))
					{
						// Search for an unconnected plug first,
						// then, if all connected, overlay a connection.
						plugFound = false;
						for (i=0;i<(int)sourceSubUnitInfo->numSourcePlugs;i++)
						{
							if (sourceSubUnitInfo->sourcePlugRecords[i].connectionCount == 0)
							{
								sourcePlugIndex=i;
								sourcePlugNum=i;
								plugFound = true;
								break;
							}
						}
						if (!plugFound)
						{
							// No available plugs, just overlay on first plug.
							sourcePlugIndex=0;
							sourcePlugNum=0;
						}
					}
					else if (inParams->sourcePlugNum < sourceSubUnitInfo->numSourcePlugs)
					{
						sourcePlugIndex = inParams->sourcePlugNum;
						sourcePlugNum = sourcePlugIndex;
					}
					else
						res = kIOReturnBadArgument;
					break;

				default:
					res = kIOReturnBadArgument;
					break;
			};
		}
	}

	// Identify Destination Plug for Connection
	if (res == kIOReturnSuccess)
	{
		if (inParams->destSubunitTypeAndID == kAVCUnitAddress)
		{
			switch (inParams->destPlugType)
			{
				case IOFWAVCPlugIsochOutputType:
					if (inParams->destPlugNum == kAVCAnyAvailableIsochPlug)
					{
						// Search for an unconnected plug first
						plugFound = false;
						for (i=0;i<kAVCMaxNumPlugs;i++)
						{
							if (fUnitPlugs.isochOutPlugRecord[i].connectionCount == 0)
							{
								destPlugIndex=i;
								destPlugNum=i;
								plugFound = true;
								break;
							}
						}
						if (!plugFound)
						{
							// We had no unconnected plugs available, so
							// next, we need to see if we can find a non-locked,
							// non-permanent connection to an isoch output plug,
							// and if so, disconnect it and use that isoch out
							// plug for this new connection.
							actualPlug = kAVCInvalidPlug;
							if (canConnectDestPlug(kAVCUnitAddress,
							  IOFWAVCPlugIsochOutputType,
							  &actualPlug))
							{
								destPlugIndex = actualPlug;
								destPlugNum = destPlugIndex;
							}
							else
								res = kIOReturnBadArgument;
						}
					}
					else if (inParams->destPlugNum < kAVCMaxNumPlugs)
					{
						if (canConnectDestPlug(kAVCUnitAddress,
							 IOFWAVCPlugIsochOutputType,
							 &inParams->destPlugNum))
						{
							destPlugIndex = inParams->destPlugNum;
							destPlugNum = destPlugIndex;
						}
						else
							res = kIOReturnBadArgument;
					}
					else
						res = kIOReturnBadArgument;
					break;

				case IOFWAVCPlugExternalOutputType:
					if (inParams->destPlugNum == kAVCAnyAvailableExternalPlug)
					{
						// Search for an unconnected plug first
						plugFound = false;
						for (i=0;i<kAVCMaxNumPlugs;i++)
						{
							if (fUnitPlugs.externalOutPlugRecord[i].connectionCount == 0)
							{
								destPlugIndex=i;
								destPlugNum= 0x80 + i;
								plugFound = true;
								break;
							}
						}
						if (!plugFound)
						{
							// We had no unconnected plugs available, so
							// next, we need to see if we can find a non-locked,
							// non-permanent connection to an external output plug,
							// and if so, disconnect it and use that extern out
							// plug for this new connection.
							actualPlug = kAVCInvalidPlug;
							if (canConnectDestPlug(kAVCUnitAddress,
							  IOFWAVCPlugExternalOutputType,
							  &actualPlug))
							{
								destPlugIndex = actualPlug - 0x80;
								destPlugNum = actualPlug;
							}
							else
								res = kIOReturnBadArgument;
						}
					}
					else if ((inParams->destPlugNum >= 0x80) && (inParams->destPlugNum < 0x9F))
					{
						if (canConnectDestPlug(kAVCUnitAddress,
							 IOFWAVCPlugExternalOutputType,
							 &inParams->destPlugNum))
						{
							destPlugIndex = inParams->destPlugNum - 0x80;
							destPlugNum = inParams->destPlugNum;
						}
						else
							res = kIOReturnBadArgument;
					}
					else
						res = kIOReturnBadArgument;
					break;
					
				default:
					res = kIOReturnBadArgument;
					break;
			};
		}
		else
		{
			found = false;
			for (i=(fSubunits->getCount()-1);i>=0;i--)
			{
				destSubUnitInfo = (AVCSubunitInfo *) fSubunits->getObject(i);
				if (inParams->destSubunitTypeAndID == destSubUnitInfo->subunitTypeAndID)
				{
					found = true;
					break;
				}
			}
			if (found == false)
				res = kIOReturnBadArgument;

			// For internally initiated connection, verify that the userClient owns this subunit
			if ((res == kIOReturnSuccess) && (userClient != NULL) && (destSubUnitInfo->userClient != userClient))
				res = kIOReturnBadArgument;
			
			if (res == kIOReturnSuccess)
			{
				switch (inParams->destPlugType)
				{
					case IOFWAVCPlugSubunitDestType:
						if ((inParams->destPlugNum == kAVCAnyAvailableSubunitPlug) && (destSubUnitInfo->numDestPlugs > 0))
						{
							// Search for an unconnected plug first
							plugFound = false;
							for (i=0;i<(int)destSubUnitInfo->numDestPlugs;i++)
							{
								if (destSubUnitInfo->destPlugRecords[i].connectionCount == 0)
								{
									destPlugIndex =i;
									destPlugNum =i;
									plugFound = true;
									break;
								}
							}
							if (!plugFound)
							{
								// We had no unconnected plugs available, so
								// next, we need to see if we can find a non-locked,
								// non-permanent connection to one of this subunits dest plugs,
								// and if so, disconnect it and use that
								// plug for this new connection.
								actualPlug = kAVCInvalidPlug;
								if (canConnectDestPlug(destSubUnitInfo->subunitTypeAndID,
							   IOFWAVCPlugSubunitDestType,
							   &actualPlug))
								{
									destPlugIndex = actualPlug;
									destPlugNum = destPlugIndex;
								}
								else
									res = kIOReturnBadArgument;
							}
						}
						else if (inParams->destPlugNum < destSubUnitInfo->numDestPlugs)
						{
							if (canConnectDestPlug(destSubUnitInfo->subunitTypeAndID,
							  IOFWAVCPlugSubunitDestType,
							  &inParams->destPlugNum))
							{
								destPlugIndex = inParams->destPlugNum;
								destPlugNum = destPlugIndex;
							}
							else
								res = kIOReturnBadArgument;
						}
						else
							res = kIOReturnBadArgument;
						break;

					default:
						res = kIOReturnBadArgument;
						break;
				};
			}
		}
	}

	// Here, we know that we have a valid source and
	// destination plug, so make the connection
	if (res == kIOReturnSuccess)
	{
		connection = new AVCConnectionRecord;
		if(!connection)
			res = kIOReturnNoMemory;

		if (res == kIOReturnSuccess)
		{
			// Update connection record parameters
			connection->sourceSubunitTypeAndID = inParams->sourceSubunitTypeAndID;
			connection->sourcePlugType = inParams->sourcePlugType;
			connection->sourcePlugNum = sourcePlugNum;
			connection->destSubunitTypeAndID = inParams->destSubunitTypeAndID;
			connection->destPlugType = inParams->destPlugType;
			connection->destPlugNum = destPlugNum;
			connection->lockConnection = inParams->lockConnection;
			connection->permConnection = inParams->permConnection;

			if(!fConnectionRecords->setObject(connection))
				res = kIOReturnNoMemory;
			connection->release();
			//IOLog( "DEBUG: connectTargetPlugs added connection record to array (0x%08X)\n",(int) userClient);

			// Update plug records in subunit/unit
			if (res == kIOReturnSuccess)
			{
				switch (inParams->sourcePlugType)
				{
					case IOFWAVCPlugIsochInputType:
						fUnitPlugs.isochInPlugRecord[sourcePlugIndex].connectionCount++;
						break;
					case IOFWAVCPlugExternalInputType:
						fUnitPlugs.externalInPlugRecord[sourcePlugIndex].connectionCount++;
						break;
					case IOFWAVCPlugSubunitSourceType:
						sourceSubUnitInfo->sourcePlugRecords[sourcePlugIndex].connectionCount++;
						break;
					default:
						break;
				};
				switch (inParams->destPlugType)
				{
					case IOFWAVCPlugIsochOutputType:
						fUnitPlugs.isochOutPlugRecord[destPlugIndex].connectionCount++;
						break;
					case IOFWAVCPlugExternalOutputType:
						fUnitPlugs.externalOutPlugRecord[destPlugIndex].connectionCount++;
						break;
					case IOFWAVCPlugSubunitDestType:
						destSubUnitInfo->destPlugRecords[destPlugIndex].connectionCount++;
						break;
					default:
						break;
				};
			}
			
			// If source is a subunit, notify source plug owner of new connection
			if ((res == kIOReturnSuccess) && (sourceSubUnitInfo))
				sourceSubUnitInfo->callBack(sourceSubUnitInfo,
								kIOFWAVCSubunitPlugMsgConnected,
								inParams->sourcePlugType,
								sourcePlugNum,
								((inParams->destSubunitTypeAndID << 16) + (inParams->destPlugType << 8) + destPlugNum),
								0,0);

			// If dest is a subunit, notify dest plug owner of new connection
			if ((res == kIOReturnSuccess) && (destSubUnitInfo))
				destSubUnitInfo->callBack(destSubUnitInfo,
							  kIOFWAVCSubunitPlugMsgConnected,
							  inParams->destPlugType,
							  destPlugNum,
							  ((inParams->sourceSubunitTypeAndID << 16) + (inParams->sourcePlugType << 8) + sourcePlugNum),
							  0,0);

			// Update the out Params
			if (res == kIOReturnSuccess)
			{
				outParams->sourcePlugNum = sourcePlugNum;
				outParams->destPlugNum = destPlugNum;
			}
		}
	}	

	AVCTARGETMUTEX_UNLOCK;
	
	return res;
}

//////////////////////////////////////////////////////
// IOFireWireAVCTargetSpace::disconnectTargetPlugs
//////////////////////////////////////////////////////
IOReturn IOFireWireAVCTargetSpace::disconnectTargetPlugs(IOFireWireAVCProtocolUserClient *userClient,
									   UInt32 sourceSubunitTypeAndID,
									   IOFWAVCPlugTypes sourcePlugType,
									   UInt32 sourcePlugNum,
									   UInt32 destSubunitTypeAndID,
									   IOFWAVCPlugTypes destPlugType,
									   UInt32 destPlugNum)
{
	int i;
	AVCConnectionRecord *connection;
	AVCSubunitInfo *sourceSubUnitInfo = NULL;
	AVCSubunitInfo *destSubUnitInfo = NULL;
	bool found = false;
	
	//IOLog( "IOFireWireAVCTargetSpace::disconnectTargetPlugs (0x%08X)\n",(int) this);

	AVCTARGETMUTEX_LOCK;
	
	// Search connection records for this source plug
	for (i=(fConnectionRecords->getCount()-1);i>=0;i--)
	{
		connection = (AVCConnectionRecord *) fConnectionRecords->getObject(i);
		if ((sourceSubunitTypeAndID == connection->sourceSubunitTypeAndID) &&
	  (sourcePlugType == connection->sourcePlugType) &&
	  (sourcePlugNum == connection->sourcePlugNum) &&
	  (connection->permConnection == false))
		{
			// Find the info for the source and dest subunits (if not a unit)
			if (connection->sourceSubunitTypeAndID != kAVCUnitAddress)
				sourceSubUnitInfo = getSubunitInfo(connection->sourceSubunitTypeAndID);
			if (connection->destSubunitTypeAndID != kAVCUnitAddress)
				destSubUnitInfo = getSubunitInfo(connection->destSubunitTypeAndID);

			// Update connection counts in plug records in subunit/unit
			switch (connection->sourcePlugType)
			{
				case IOFWAVCPlugIsochInputType:
					fUnitPlugs.isochInPlugRecord[connection->sourcePlugNum].connectionCount--;
					break;
				case IOFWAVCPlugExternalInputType:
					fUnitPlugs.externalInPlugRecord[connection->sourcePlugNum - 0x80].connectionCount--;
					break;
				case IOFWAVCPlugSubunitSourceType:
					if (sourceSubUnitInfo)
						sourceSubUnitInfo->sourcePlugRecords[connection->sourcePlugNum].connectionCount--;
					break;
				default:
					break;
			};
			switch (connection->destPlugType)
			{
				case IOFWAVCPlugIsochOutputType:
					fUnitPlugs.isochOutPlugRecord[connection->destPlugNum].connectionCount--;
					break;
				case IOFWAVCPlugExternalOutputType:
					fUnitPlugs.externalOutPlugRecord[connection->destPlugNum - 0x80].connectionCount--;
					break;
				case IOFWAVCPlugSubunitDestType:
					if (destSubUnitInfo)
						destSubUnitInfo->destPlugRecords[connection->destPlugNum].connectionCount--;
					break;
				default:
					break;
			};

			// If source is a subunit, notify source plug owner of disconnection
			if (sourceSubUnitInfo)
				sourceSubUnitInfo->callBack(sourceSubUnitInfo,
								kIOFWAVCSubunitPlugMsgDisconnected,
								connection->sourcePlugType,
								connection->sourcePlugNum,
								((connection->destSubunitTypeAndID << 16) + (connection->destPlugType << 8) + connection->destPlugNum),
								0,0);

			// If dest is a subunit, notify dest plug owner of disconnection
			if (destSubUnitInfo)
				destSubUnitInfo->callBack(destSubUnitInfo,
							  kIOFWAVCSubunitPlugMsgDisconnected,
							  connection->destPlugType,
							  connection->destPlugNum,
							  ((connection->sourceSubunitTypeAndID << 16) + (connection->sourcePlugType << 8) + connection->sourcePlugNum),
							  0,0);

			// Remove the plug connection record from the array
			fConnectionRecords->removeObject(i);
			found = true;
		}
	}

	AVCTARGETMUTEX_UNLOCK;
	
	if (found)
		return kIOReturnSuccess;
	else
		return kIOReturnBadArgument;
}

//////////////////////////////////////////////////////
// IOFireWireAVCTargetSpace::getTargetPlugConnection
//////////////////////////////////////////////////////
IOReturn IOFireWireAVCTargetSpace::getTargetPlugConnection(IOFireWireAVCProtocolUserClient *userClient,
														   AVCGetTargetPlugConnectionInParams *inParams,
														   AVCGetTargetPlugConnectionOutParams *outParams)
{
	IOReturn res;
	int i;
	AVCConnectionRecord *connection;
	UInt32 count = 0;
	
	//IOLog( "IOFireWireAVCTargetSpace::getTargetPlugConnection (0x%08X)\n",(int) this);

	AVCTARGETMUTEX_LOCK;
	
	// Search connection records for this source plug
	for (i=(fConnectionRecords->getCount()-1);i>=0;i--)
	{
		connection = (AVCConnectionRecord *) fConnectionRecords->getObject(i);

		switch (inParams->plugType)
		{
			case IOFWAVCPlugSubunitSourceType:
			case IOFWAVCPlugIsochOutputType:
			case IOFWAVCPlugExternalOutputType:
				if ((inParams->subunitTypeAndID == connection->sourceSubunitTypeAndID) &&
		(inParams->plugType == connection->sourcePlugType) &&
		(inParams->plugNum == connection->sourcePlugNum))
				{
					outParams->connectedSubunitTypeAndID = connection->destSubunitTypeAndID;
					outParams->connectedPlugType = connection->destPlugType;
					outParams->connectedPlugNum = connection->destPlugNum;
					outParams->lockConnection = connection->lockConnection;
					outParams->permConnection = connection->permConnection;
					count += 1;
				}
				break;

			case IOFWAVCPlugSubunitDestType:
			case IOFWAVCPlugIsochInputType:
			case IOFWAVCPlugExternalInputType:
				if ((inParams->subunitTypeAndID == connection->destSubunitTypeAndID) &&
		(inParams->plugType == connection->destPlugType) &&
		(inParams->plugNum == connection->destPlugNum))
				{
					outParams->connectedSubunitTypeAndID = connection->sourceSubunitTypeAndID;
					outParams->connectedPlugType = connection->sourcePlugType;
					outParams->connectedPlugNum = connection->sourcePlugNum;
					outParams->lockConnection = connection->lockConnection;
					outParams->permConnection = connection->permConnection;
					count += 1;
				}
				break;
				
			default:
				break;
		};
	}

	if (count == 0)
		res = kIOReturnBadArgument;
	else
	{
		res = kIOReturnSuccess;
		if (count > 1)
			outParams->connectedPlugNum = kAVCMultiplePlugs;
	}

	AVCTARGETMUTEX_UNLOCK;
	
	return res;
}

//////////////////////////////////////////////////////
// IOFireWireAVCTargetSpace::handleUnitInfoCommand
//////////////////////////////////////////////////////
IOReturn IOFireWireAVCTargetSpace::handleUnitInfoCommand(UInt16 nodeID, UInt32 generation, const char *buf, UInt32 len)
{
	UInt8 *pResponse;
	UInt8 *pBuf = (UInt8*) buf;
	UInt8 cType;
	IOBufferMemoryDescriptor *pBufMemDesc = NULL;
	AVCSubunitInfo *subUnitInfo;
	
	//IOLog( "IOFireWireAVCTargetSpace::handleUnitInfoCommand (0x%08X)\n",(int) this);

	// Check the length of the command. Don't handle command if wrong.
	if (len != 8)
		return kIOReturnError;

	// Parse the command buf	
	cType = pBuf[0] & 0x0F;

	// Check the cType.  Don't handle command if not a status type.
	if (cType != kAVCStatusInquiryCommand)
		return kIOReturnError;

	// Check the operands.  Don't handle command if not properly initialized.
	if ((pBuf[kAVCOperand0] != 0xFF) ||
	 (pBuf[kAVCOperand1] != 0xFF) ||
	 (pBuf[kAVCOperand2] != 0xFF) ||
	 (pBuf[kAVCOperand3] != 0xFF) ||
	 (pBuf[kAVCOperand4] != 0xFF))
		return kIOReturnError;

	// All tests passed. Handle the command
	pBufMemDesc = IOBufferMemoryDescriptor::withCapacity(len, kIODirectionOutIn);
	if(!pBufMemDesc)
		return kFWResponseDataError;
	pResponse = (UInt8 *) pBufMemDesc->getBytesNoCopy();

	// Initialize the response
	pResponse[kAVCCommandResponse] = kAVCImplementedStatus;
	pResponse[kAVCAddress] = pBuf[kAVCAddress];
	pResponse[kAVCOpcode] = pBuf[kAVCOpcode];
	pResponse[kAVCOperand0] = 0x07;

	AVCTARGETMUTEX_LOCK;

	// This is the unit type field. Set to value of first subunit 
	// in subunit info list if it exists
	if (fSubunits->getCount() > 0)
	{
		subUnitInfo = (AVCSubunitInfo*) fSubunits->getObject(0);
		pResponse[kAVCOperand1] = subUnitInfo->subunitTypeAndID;
	}
	else
		pResponse[kAVCOperand1] = 0xFF;

	// Add Apple's OUI to the company_ID field of the response
	pResponse[kAVCOperand2] = 0x00;
	pResponse[kAVCOperand3] = 0x03;
	pResponse[kAVCOperand4] = 0x93;
	
	targetSendAVCResponse(generation, nodeID, pBufMemDesc, len);

	AVCTARGETMUTEX_UNLOCK;
	
	return kIOReturnSuccess;
}

//////////////////////////////////////////////////////
// IOFireWireAVCTargetSpace::handleSubUnitInfoCommand
//////////////////////////////////////////////////////
IOReturn IOFireWireAVCTargetSpace::handleSubUnitInfoCommand(UInt16 nodeID, UInt32 generation, const char *buf, UInt32 len)
{
	UInt8 *pResponse;
	UInt8 *pBuf = (UInt8*) buf;
	UInt8 cType;
	UInt8 page;
	IOBufferMemoryDescriptor *pBufMemDesc = NULL;
	int i;
	AVCSubunitInfo *subUnitInfo;
	UInt32 subUnitType;
	UInt8 count[32];
	UInt32 uniqueSubUnitCount = 0;
	UInt32 countArrayIndex;
	int skipped;

	//IOLog( "IOFireWireAVCTargetSpace::handleSubUnitInfoCommand (0x%08X)\n",(int) this);
	
	// Check the length of the command. Don't handle command if wrong.
	if (len != 8)
		return kIOReturnError;

	// Parse the command buf
	cType = pBuf[0] & 0x0F;

	// Check the cType.  Don't handle command if not a status type.
	if (cType != kAVCStatusInquiryCommand)
		return kIOReturnError;

	// Check the operands.  Don't handle command if not properly initialized.
	if (((pBuf[kAVCOperand0] & 0x8F) != 0x07) ||
	 (pBuf[kAVCOperand1] != 0xFF) ||
	 (pBuf[kAVCOperand2] != 0xFF) ||
	 (pBuf[kAVCOperand3] != 0xFF) ||
	 (pBuf[kAVCOperand4] != 0xFF))
		return kIOReturnError;

	AVCTARGETMUTEX_LOCK;

	// Initialize the count array
	for (i=0;i<32;i++) 
		count[i] = 0;

	// Parse the subunit list
	for (i=(fSubunits->getCount()-1);i>=0;i--)
	{
		subUnitInfo = (AVCSubunitInfo *) fSubunits->getObject(i);
		subUnitType = ((subUnitInfo->subunitTypeAndID & 0xF8) >> 3);
		if (count[subUnitType] == 0)
			uniqueSubUnitCount += 1;
		count[subUnitType] += 1;
    }

	// Check the page to see if valid
	page = ((pBuf[kAVCOperand0] & 0x70) >> 4);
	if (page > uniqueSubUnitCount/4)
	{
		AVCTARGETMUTEX_UNLOCK;
		return kIOReturnError;	// Spec says empty page should result in NOT_IMPLEMENTED response
	}
	
	// All tests passed. Handle the command
	pBufMemDesc = IOBufferMemoryDescriptor::withCapacity(len, kIODirectionOutIn);
	if(!pBufMemDesc)
	{
		AVCTARGETMUTEX_UNLOCK;
		return kFWResponseDataError;
	}
	pResponse = (UInt8 *) pBufMemDesc->getBytesNoCopy();
	
	// Initialize the response
	pResponse[kAVCCommandResponse] = kAVCImplementedStatus;
	pResponse[kAVCAddress] = pBuf[kAVCAddress];
	pResponse[kAVCOpcode] = pBuf[kAVCOpcode];
	pResponse[kAVCOperand0] = pBuf[kAVCOperand0];

	// Fill in subunit info page data
	for (i=0;i<4;i++)
	{
		if (((page*4)+i) < (int) uniqueSubUnitCount)
		{
			// Reset the skipped count
			skipped = 0;
			
			// Find the ((page*4)+i) non-zero entry in the count array. It's index is the subunit type, and it's value is the number 
			// of that type of subunit. Note: We've already confirmed that this entry does indeed exist, so no failsafe code needed here.
			for (countArrayIndex = 0; countArrayIndex < 32; countArrayIndex++)
			{
				if (count[countArrayIndex] != 0)
				{
					// Found a non-zero entry, is this the one we're looking for?
					if (skipped != ((page*4)+i))
					{
						// This is not the one we're looking for
						skipped += 1;
					}
					else
					{
						// This is the one we're looking for. The subunit's max ID for the response packet is the count - 1
						pResponse[kAVCOperand1+i]  = ((countArrayIndex << 3) | (count[countArrayIndex] > 8 ? 7 : (count[countArrayIndex]-1)));
						break;
					}
				}
			}
		}
		else
			pResponse[kAVCOperand1+i]  = 0xFF;
	}

	targetSendAVCResponse(generation, nodeID, pBufMemDesc, len);

	AVCTARGETMUTEX_UNLOCK;

	return kIOReturnSuccess;
}

//////////////////////////////////////////////////////
// IOFireWireAVCTargetSpace::handlePlugInfoCommand
//////////////////////////////////////////////////////
IOReturn IOFireWireAVCTargetSpace::handlePlugInfoCommand(UInt16 nodeID, UInt32 generation, const char *buf, UInt32 len)
{
	UInt8 *pResponse;
	UInt8 *pBuf = (UInt8*) buf;
	UInt8 cType;
	IOBufferMemoryDescriptor *pBufMemDesc = NULL;
	int i;
	AVCSubunitInfo *subUnitInfo;
	bool found = false;
	
	//IOLog( "IOFireWireAVCTargetSpace::handlePlugInfoCommand (0x%08X)\n",(int) this);

	// Check the length of the command. Don't handle command if wrong.
	if (len != 8)
		return kIOReturnError;

	// Parse the command buf
	cType = pBuf[0] & 0x0F;

	// Check the cType.  Don't handle command if not a status type.
	if (cType != kAVCStatusInquiryCommand)
		return kIOReturnError;

	// Check the operands.  Don't handle command if not properly initialized.
	if ((pBuf[kAVCOperand1] != 0xFF) ||
	 (pBuf[kAVCOperand2] != 0xFF) ||
	 (pBuf[kAVCOperand3] != 0xFF) ||
	 (pBuf[kAVCOperand4] != 0xFF))
		return kIOReturnError;

	// Currently, we only support subfunction 0
	if (pBuf[kAVCOperand0] != 0x00)
		return kIOReturnError;
	
	// All tests passed. Handle the command
	pBufMemDesc = IOBufferMemoryDescriptor::withCapacity(len, kIODirectionOutIn);
	if(!pBufMemDesc)
		return kFWResponseDataError;
	pResponse = (UInt8 *) pBufMemDesc->getBytesNoCopy();
	bcopy(buf,pResponse,len);

	if (pBuf[kAVCAddress] == kAVCUnitAddress)
	{
		// Fill in the response values
		pResponse[kAVCCommandResponse] = kAVCImplementedStatus;
		pResponse[kAVCOperand1] = 31;  // Currently this matches the iMPR/oMPR. Changes will be
		pResponse[kAVCOperand2] = 31;  // needed here if we reduce the number of allocated plugs
	}
	else
	{
		// This command is addressed to a subunit. See if it's a valid subunit address,
		// and, if so, report its dest and source plug count.

		AVCTARGETMUTEX_LOCK;

		for (i=(fSubunits->getCount()-1);i>=0;i--)
		{
			subUnitInfo = (AVCSubunitInfo *) fSubunits->getObject(i);
			if (subUnitInfo->subunitTypeAndID == pBuf[kAVCAddress])
			{
				pResponse[kAVCCommandResponse] = kAVCImplementedStatus;
				pResponse[kAVCOperand1] = subUnitInfo->numDestPlugs;
				pResponse[kAVCOperand2] = subUnitInfo->numSourcePlugs; 
				found = true;
				break;
			}
		}
		if (!found)
		{
			pResponse[kAVCCommandResponse] = kAVCNotImplementedStatus;
		}
		
		AVCTARGETMUTEX_UNLOCK;
	}

	targetSendAVCResponse(generation, nodeID, pBufMemDesc, len);

	return kIOReturnSuccess;
}

//////////////////////////////////////////////////////
// IOFireWireAVCTargetSpace::handlePowerCommand
//////////////////////////////////////////////////////
IOReturn IOFireWireAVCTargetSpace::handlePowerCommand(UInt16 nodeID, UInt32 generation, const char *buf, UInt32 len)
{
	UInt8 *pResponse;
	UInt8 *pBuf = (UInt8*) buf;
	UInt8 cType;
	IOBufferMemoryDescriptor *pBufMemDesc = NULL;

	//IOLog( "IOFireWireAVCTargetSpace::handlePowerCommand (0x%08X)\n",(int) this);

	// Check the length of the command. Don't handle command if wrong.
	if (len != 4)
		return kIOReturnError;

	// Parse the command buf
	cType = pBuf[0] & 0x0F;

	// Further check the request packet parameters
	switch (cType)
	{
		case kAVCStatusInquiryCommand:
			if (pBuf[kAVCOperand0] != 0x7F)
				return kIOReturnError;
			break;

		case kAVCControlCommand:
			if ((pBuf[kAVCOperand0] != 0x60) && (pBuf[kAVCOperand0] != 0x70))
				return kIOReturnError;
			break;

		default:
			return kIOReturnError;
	};
	
	// All tests passed. Handle the command
	pBufMemDesc = IOBufferMemoryDescriptor::withCapacity(len, kIODirectionOutIn);
	if(!pBufMemDesc)
		return kFWResponseDataError;
	pResponse = (UInt8 *) pBufMemDesc->getBytesNoCopy();
	bcopy(buf,pResponse,len);

	// Fill in the response values
	pResponse[kAVCCommandResponse] = (cType == kAVCStatusInquiryCommand) ? kAVCImplementedStatus : kAVCAcceptedStatus;
	pResponse[kAVCOperand0] = (cType == kAVCStatusInquiryCommand) ? 0x70 : pBuf[kAVCOperand0];

	targetSendAVCResponse(generation, nodeID, pBufMemDesc, len);

	return kIOReturnSuccess;
}

//////////////////////////////////////////////////////
// IOFireWireAVCTargetSpace::handleConnectCommand
//////////////////////////////////////////////////////
IOReturn IOFireWireAVCTargetSpace::handleConnectCommand(UInt16 nodeID, UInt32 generation, const char *buf, UInt32 len)
{
	UInt8 *pResponse;
	UInt8 *pBuf = (UInt8*) buf;
	UInt8 cType;
	IOBufferMemoryDescriptor *pBufMemDesc = NULL;
	int i;
	AVCConnectTargetPlugsInParams inParams;
	AVCConnectTargetPlugsOutParams outParams;
	AVCConnectionRecord *pConnection;
	UInt32 connCount = 0;

	//IOLog( "IOFireWireAVCTargetSpace::handleConnectCommand (0x%08X)\n",(int) this);

	// Check the length of the command. Don't handle command if wrong.
	if (len != 8)
		return kIOReturnError;

	// Parse the command buf
	cType = pBuf[0] & 0x0F;

	// Further check the request packet parameters
	switch (cType)
	{
		case kAVCStatusInquiryCommand:
			if ((pBuf[kAVCOperand0] & 0xFF) != 0xFF)
				return kIOReturnError;
			break;

		case kAVCControlCommand:
			if ((pBuf[kAVCOperand0] & 0xFC) != 0xFC)
				return kIOReturnError;
			break;

		default:
			return kIOReturnError;
	};

	// Prepare the connect parameters struct
	inParams.sourceSubunitTypeAndID = pBuf[kAVCOperand1];
	inParams.sourcePlugNum = pBuf[kAVCOperand2];
	inParams.destSubunitTypeAndID = pBuf[kAVCOperand3];
	inParams.destPlugNum = pBuf[kAVCOperand4];
	inParams.lockConnection = ((pBuf[kAVCOperand0] & 0x02) == 0x02) ? true : false;
	inParams.permConnection = false;

	// Determine the type of each the source plug
	if (inParams.sourceSubunitTypeAndID == kAVCUnitAddress)
	{
		if ((inParams.sourcePlugNum < kAVCMaxNumPlugs) || (inParams.sourcePlugNum == kAVCAnyAvailableIsochPlug))
			inParams.sourcePlugType = IOFWAVCPlugIsochInputType;
		else if (((inParams.sourcePlugNum >= 0x80) && (inParams.sourcePlugNum <= 0x9E)) || (inParams.sourcePlugNum == kAVCAnyAvailableExternalPlug))
			inParams.sourcePlugType = IOFWAVCPlugExternalInputType;
		else if ((cType == kAVCStatusInquiryCommand) && (inParams.sourcePlugNum = kAVCInvalidPlug))
			inParams.sourcePlugType = IOFWAVCPlugIsochInputType;
		else
			return kIOReturnError;
	}
	else
		inParams.sourcePlugType = IOFWAVCPlugSubunitSourceType;

	// Determine the type of each the dest plug
	if (inParams.destSubunitTypeAndID == kAVCUnitAddress)
	{
		if ((inParams.destPlugNum < kAVCMaxNumPlugs) || (inParams.destPlugNum == kAVCAnyAvailableIsochPlug))
			inParams.destPlugType = IOFWAVCPlugIsochOutputType;
		else if (((inParams.destPlugNum >= 0x80) && (inParams.destPlugNum <= 0x9E)) || (inParams.destPlugNum == kAVCAnyAvailableExternalPlug))
			inParams.destPlugType = IOFWAVCPlugExternalOutputType;
		else if ((cType == kAVCStatusInquiryCommand) && (inParams.destPlugNum = kAVCInvalidPlug))
			inParams.destPlugType = IOFWAVCPlugIsochOutputType;
		else
			return kIOReturnError;
	}
	else
		inParams.destPlugType = IOFWAVCPlugSubunitDestType;
	
	// All tests passed. Handle the command
	pBufMemDesc = IOBufferMemoryDescriptor::withCapacity(len, kIODirectionOutIn);
	if(!pBufMemDesc)
		return kFWResponseDataError;
	pResponse = (UInt8 *) pBufMemDesc->getBytesNoCopy();
	bcopy(buf,pResponse,len);
	
	if (cType == kAVCStatusInquiryCommand)
	{
		AVCTARGETMUTEX_LOCK;

		// Initialize response
		pResponse[kAVCCommandResponse] = kAVCImplementedStatus;

		if ((inParams.sourceSubunitTypeAndID == 0xFF) && (inParams.sourcePlugNum == 0xFE))
		{
			// We're searching for a destination plug's connections
			for (i=(fConnectionRecords->getCount()-1);i>=0;i--)
			{
				pConnection = (AVCConnectionRecord *) fConnectionRecords->getObject(i);
				if ((pConnection->destSubunitTypeAndID == inParams.destSubunitTypeAndID) &&
		(pConnection->destPlugNum == inParams.destPlugNum))
				{
					connCount++;
					pResponse[kAVCOperand1] = pConnection->sourceSubunitTypeAndID;
					pResponse[kAVCOperand2] = pConnection->sourcePlugNum;
					if (pConnection->lockConnection == false)
					{
						pResponse[kAVCOperand0] &= 0xFD;	// Clear bit 1
					}
					if (pConnection->permConnection == false)
					{
						pResponse[kAVCOperand0] &= 0xFE;	// Clear bit 0
					}
				}
			}
			if (connCount == 0)
			{
				pResponse[kAVCOperand2] = kAVCInvalidPlug;
			}
			else if (connCount > 1)
			{
				pResponse[kAVCOperand1] = kAVCUnitAddress;
				pResponse[kAVCOperand2] = kAVCMultiplePlugs;
				pResponse[kAVCOperand0] |= 0x03;	// Set bits 0 and 1

			}
		}
		else if ((inParams.destSubunitTypeAndID == 0xFF) && (inParams.destPlugNum == 0xFE))

		{
			// We're searching for a source plug's connections
			for (i=(fConnectionRecords->getCount()-1);i>=0;i--)
			{
				pConnection = (AVCConnectionRecord *) fConnectionRecords->getObject(i);
				if ((pConnection->sourceSubunitTypeAndID == inParams.sourceSubunitTypeAndID) &&
		(pConnection->sourcePlugNum == inParams.sourcePlugNum))
				{
					connCount++;
					pResponse[kAVCOperand3] = pConnection->destSubunitTypeAndID;
					pResponse[kAVCOperand4] = pConnection->destPlugNum;
					if (pConnection->lockConnection == false)
					{
						pResponse[kAVCOperand0] &= 0xFD;	// Clear bit 1
					}
					if (pConnection->permConnection == false)
					{
						pResponse[kAVCOperand0] &= 0xFE;	// Clear bit 0
					}
				}
			}
			if (connCount == 0)
			{
				pResponse[kAVCOperand4] = kAVCInvalidPlug;
			}
			else if (connCount > 1)
			{
				pResponse[kAVCOperand3] = kAVCUnitAddress;
				pResponse[kAVCOperand4] = kAVCMultiplePlugs;
				pResponse[kAVCOperand0] |= 0x03;	// Set bits 0 and 1
			}
		}
		else
		{
			// Parameter error, return not implemented response
			pResponse[kAVCCommandResponse] = kAVCNotImplementedStatus;
		}
	}
	else	
	{
		// Initialize response
		pResponse[kAVCCommandResponse] = kAVCAcceptedStatus;

		// Try and connect the plugs
		if (connectTargetPlugs(NULL,&inParams,&outParams) == kIOReturnSuccess)
		{
			// Update the plug num fields in the response
			pResponse[kAVCOperand2] = outParams.sourcePlugNum;
			pResponse[kAVCOperand4] = outParams.destPlugNum;
		}
		else
		{
			// Set rejected status in response
			pResponse[kAVCCommandResponse] = kAVCRejectedStatus;
		}
	}

	AVCTARGETMUTEX_UNLOCK;

	// Send the response
	targetSendAVCResponse(generation, nodeID, pBufMemDesc, len);

	return kIOReturnSuccess;
}

//////////////////////////////////////////////////////
// IOFireWireAVCTargetSpace::handleDisconnectCommand
//////////////////////////////////////////////////////
IOReturn IOFireWireAVCTargetSpace::handleDisconnectCommand(UInt16 nodeID, UInt32 generation, const char *buf, UInt32 len)
{
	UInt8 *pResponse;
	UInt8 *pBuf = (UInt8*) buf;
	UInt8 cType;
	IOBufferMemoryDescriptor *pBufMemDesc = NULL;
	IOReturn res;
	IOFWAVCPlugTypes sourcePlugType;
	IOFWAVCPlugTypes destPlugType;
	
	//IOLog( "IOFireWireAVCTargetSpace::handleDisconnectCommand (0x%08X)\n",(int) this);

	// Check the length of the command. Don't handle command if wrong.
	if (len != 8)
		return kIOReturnError;

	// Get the cType. Only control type accepted
	cType = pBuf[kAVCCommandResponse] & 0x0F;
	if (cType != kAVCControlCommand)
		return kIOReturnError;

	// Further check the request packet parameters
	if (pBuf[kAVCOperand0] != 0xFF)
		return kIOReturnError;

	// Determine type of source plug
	if (pBuf[kAVCOperand1] != kAVCUnitAddress)
	{
		if (pBuf[kAVCOperand2] < kAVCMaxNumPlugs)
			sourcePlugType = IOFWAVCPlugSubunitSourceType;
		else
			return kIOReturnError;
	}
	else
	{
		if (pBuf[kAVCOperand2] < kAVCMaxNumPlugs)
			sourcePlugType = IOFWAVCPlugIsochInputType;
		else if ((pBuf[kAVCOperand2] >= 0x80) && (pBuf[kAVCOperand2] <= 0x9E))
				sourcePlugType = IOFWAVCPlugExternalInputType;
		else
			return kIOReturnError;
	}

	// Determine type of dest plug
	if (pBuf[kAVCOperand3] != kAVCUnitAddress)
	{
		if ((pBuf[kAVCOperand4] < kAVCMaxNumPlugs) || (pBuf[kAVCOperand4] == kAVCAnyAvailableSubunitPlug))
			destPlugType = IOFWAVCPlugSubunitDestType;
		else
			return kIOReturnError;
	}
	else
	{
		if ((pBuf[kAVCOperand4] < kAVCMaxNumPlugs) || (pBuf[kAVCOperand4] == kAVCAnyAvailableIsochPlug))
			destPlugType = IOFWAVCPlugIsochOutputType;
		else if (((pBuf[kAVCOperand4] >= 0x80) && (pBuf[kAVCOperand4] <= 0x9E)) || (pBuf[kAVCOperand4] == kAVCAnyAvailableExternalPlug))
			destPlugType = IOFWAVCPlugExternalOutputType;
		else
			return kIOReturnError;
	}

	// All tests passed. Handle the command
	pBufMemDesc = IOBufferMemoryDescriptor::withCapacity(len, kIODirectionOutIn);
	if(!pBufMemDesc)
		return kFWResponseDataError;
	pResponse = (UInt8 *) pBufMemDesc->getBytesNoCopy();
	bcopy(buf,pResponse,len);

	AVCTARGETMUTEX_LOCK;
	
	// Attempt to disconnect the plugs
	res = disconnectTargetPlugs(NULL,
							 pBuf[kAVCOperand1],
							 sourcePlugType,
							 pBuf[kAVCOperand2],
							 pBuf[kAVCOperand3],
							 destPlugType,
							 pBuf[kAVCOperand4]);

	if (res == kIOReturnSuccess)
		pResponse[kAVCCommandResponse] = kAVCAcceptedStatus;
	else
		pResponse[kAVCCommandResponse] = kAVCRejectedStatus;

	AVCTARGETMUTEX_UNLOCK;

	targetSendAVCResponse(generation, nodeID, pBufMemDesc, len);

	return kIOReturnSuccess;
}

//////////////////////////////////////////////////////
// IOFireWireAVCTargetSpace::handleInputPlugSignalFormatCommand
//////////////////////////////////////////////////////
IOReturn IOFireWireAVCTargetSpace::handleInputPlugSignalFormatCommand(UInt16 nodeID, UInt32 generation, const char *buf, UInt32 len)
{
	UInt8 *pResponse;
	IOBufferMemoryDescriptor *pBufMemDesc = NULL;
	UInt8 *pBuf = (UInt8*) buf;
	UInt8 cType;
	UInt32 plug;
	int i;
	AVCConnectionRecord *pConnection;
	bool found = false;
	AVCSubunitInfo *subUnitInfo;
	UInt32 signalFormat;

	//IOLog( "IOFireWireAVCTargetSpace::handleInputPlugSignalFormatCommand (0x%08X)\n",(int) this);

	// Check the length of the command. Don't handle command if wrong.
	if (len != 8)
		return kIOReturnError;

	// Parse the command buf
	cType = pBuf[0] & 0x0F;
	plug = pBuf[kAVCOperand0];
	signalFormat = (((UInt32)pBuf[kAVCOperand1] << 24) +
				 ((UInt32)pBuf[kAVCOperand2] << 16) +
				 ((UInt32)pBuf[kAVCOperand3] << 8) +
				 (UInt32)pBuf[kAVCOperand4]);

	// Further check the request packet parameters
	if (plug > (kAVCMaxNumPlugs-1))
		return kIOReturnError;
	switch (cType)
	{
		case kAVCStatusInquiryCommand:
			if ((pBuf[kAVCOperand1] != 0xFF) ||
	   (pBuf[kAVCOperand2] != 0xFF) ||
	   (pBuf[kAVCOperand3] != 0xFF) ||
	   (pBuf[kAVCOperand4] != 0xFF))
				return kIOReturnError;
			break;

		case kAVCControlCommand:
			if ((pBuf[kAVCOperand1] & 0xC0) != 0x80)
				return kIOReturnError;
			break;

		default:
			return kIOReturnError;
	};

	AVCTARGETMUTEX_LOCK;
	
	// Search connection records for a subunit dest plug connection to this plug
	for (i=(fConnectionRecords->getCount()-1);i>=0;i--)
	{
		pConnection = (AVCConnectionRecord *) fConnectionRecords->getObject(i);
		if ((pConnection->sourceSubunitTypeAndID == kAVCUnitAddress) &&
	  (pConnection->sourcePlugNum == plug) &&
	  (pConnection->destSubunitTypeAndID != kAVCUnitAddress))
		{
			subUnitInfo = getSubunitInfo(pConnection->destSubunitTypeAndID);
			if (subUnitInfo)
			{
				found = true;
				break;
			}
		}
	}

	if ((found == true) && (cType == kAVCControlCommand))
	{
		// Send callback to user client
		subUnitInfo->callBack(subUnitInfo,
						kIOFWAVCSubunitPlugMsgSignalFormatModified,
						IOFWAVCPlugSubunitDestType,
						pConnection->destPlugNum,
						signalFormat,
						generation,
						nodeID);

		// Were done for now. User client will send response packet
		AVCTARGETMUTEX_UNLOCK;
		return kIOReturnSuccess;
	}

	// We will send a response to handle the command
	pBufMemDesc = IOBufferMemoryDescriptor::withCapacity(len, kIODirectionOutIn);
	if(!pBufMemDesc)
	{
		AVCTARGETMUTEX_UNLOCK;
		return kFWResponseDataError;
	}
	pResponse = (UInt8 *) pBufMemDesc->getBytesNoCopy();
	bcopy(buf,pResponse,len);

	if (cType == kAVCStatusInquiryCommand)
	{
		pResponse[kAVCCommandResponse] = kAVCImplementedStatus;

		// See if this plug has a connection
		if (found == true)
		{
			signalFormat = subUnitInfo->destPlugRecords[pConnection->destPlugNum].plugSignalFormat;
			pResponse[kAVCOperand1] = ((signalFormat & 0xFF000000) >> 24);
			pResponse[kAVCOperand2] = ((signalFormat & 0x00FF0000) >> 16);
			pResponse[kAVCOperand3] = ((signalFormat & 0x0000FF00) >> 8);
			pResponse[kAVCOperand4] = (signalFormat & 0x000000FF);
		}
		else
		{
			// If no connections, default plug signal type to NTSC-DV
			pResponse[kAVCOperand1] = ((kAVCPlugSignalFormatNTSCDV & 0xFF000000) >> 24);
			pResponse[kAVCOperand2] = ((kAVCPlugSignalFormatNTSCDV & 0x00FF0000) >> 16);
			pResponse[kAVCOperand3] = ((kAVCPlugSignalFormatNTSCDV & 0x0000FF00) >> 8);
			pResponse[kAVCOperand4] = (kAVCPlugSignalFormatNTSCDV & 0x000000FF);
		}
	}
	else
	{
		// This is a control type command to
		// a isoch output plug with no internal connection.
		// TODO: Today we just accept and ignore. But should we
		// maintain signal format for unconnected plugs?
		pResponse[kAVCCommandResponse] = kAVCAcceptedStatus;
	}

	AVCTARGETMUTEX_UNLOCK;
	
	targetSendAVCResponse(generation, nodeID, pBufMemDesc, len);

	return kIOReturnSuccess;
}

//////////////////////////////////////////////////////
// IOFireWireAVCTargetSpace::handleOutputPlugSignalFormatCommand
//////////////////////////////////////////////////////
IOReturn IOFireWireAVCTargetSpace::handleOutputPlugSignalFormatCommand(UInt16 nodeID, UInt32 generation, const char *buf, UInt32 len)
{
	UInt8 *pResponse;
	IOBufferMemoryDescriptor *pBufMemDesc = NULL;
	UInt8 *pBuf = (UInt8*) buf;
	UInt8 cType;
	UInt32 plug;
	int i;
	AVCConnectionRecord *pConnection;
	bool found = false;
	AVCSubunitInfo *subUnitInfo;
	UInt32 signalFormat;

	//IOLog( "IOFireWireAVCTargetSpace::handleOutputPlugSignalFormatCommand (0x%08X)\n",(int) this);

	// Check the length of the command. Don't handle command if wrong.
	if (len != 8)
		return kIOReturnError;

	// Parse the command buf
	cType = pBuf[0] & 0x0F;
	plug = pBuf[kAVCOperand0];
	signalFormat = (((UInt32)pBuf[kAVCOperand1] << 24) +
				 ((UInt32)pBuf[kAVCOperand2] << 16) +
				 ((UInt32)pBuf[kAVCOperand3] << 8) +
				 (UInt32)pBuf[kAVCOperand4]);

	// Further check the request packet parameters
	if (plug > (kAVCMaxNumPlugs-1))
	 return kIOReturnError;
	switch (cType)
	{
		case kAVCStatusInquiryCommand:
			if ((pBuf[kAVCOperand1] != 0xFF) ||
	   (pBuf[kAVCOperand2] != 0xFF) ||
	   (pBuf[kAVCOperand3] != 0xFF) ||
	   (pBuf[kAVCOperand4] != 0xFF))
				return kIOReturnError;
			break;
			
		case kAVCControlCommand:
			if ((pBuf[kAVCOperand1] & 0xC0) != 0x80)
				return kIOReturnError;
			break;
			
		default:
			return kIOReturnError;
	};

	AVCTARGETMUTEX_LOCK;
	
	// Search connection records for a subunit source plug connection to this plug
	for (i=(fConnectionRecords->getCount()-1);i>=0;i--)
	{
		pConnection = (AVCConnectionRecord *) fConnectionRecords->getObject(i);
		if ((pConnection->destSubunitTypeAndID == kAVCUnitAddress) &&
	  (pConnection->destPlugNum == plug) &&
	  (pConnection->sourceSubunitTypeAndID != kAVCUnitAddress))
		{
			subUnitInfo = getSubunitInfo(pConnection->sourceSubunitTypeAndID);
			if (subUnitInfo)
			{
				found = true;
				break;
			}
		}
	}

	if ((found == true) && (cType == kAVCControlCommand))
	{
		// Send callback to user client
		subUnitInfo->callBack(subUnitInfo,
						kIOFWAVCSubunitPlugMsgSignalFormatModified,
						IOFWAVCPlugSubunitSourceType,
						pConnection->sourcePlugNum,
						signalFormat,
						generation,
						nodeID);

		// Were done for now. User client will send response packet
		AVCTARGETMUTEX_UNLOCK;
		return kIOReturnSuccess;
	}

	// We will send a response to handle the command
	pBufMemDesc = IOBufferMemoryDescriptor::withCapacity(len, kIODirectionOutIn);
	if(!pBufMemDesc)
	{
		AVCTARGETMUTEX_UNLOCK;
		return kFWResponseDataError;
	}
	pResponse = (UInt8 *) pBufMemDesc->getBytesNoCopy();
	bcopy(buf,pResponse,len);

	if (cType == kAVCStatusInquiryCommand)
	{
		pResponse[kAVCCommandResponse] = kAVCImplementedStatus;

		// See if this plug has a connection
		if (found == true)
		{
			signalFormat = subUnitInfo->sourcePlugRecords[pConnection->sourcePlugNum].plugSignalFormat;
			pResponse[kAVCOperand1] = ((signalFormat & 0xFF000000) >> 24);
			pResponse[kAVCOperand2] = ((signalFormat & 0x00FF0000) >> 16);
			pResponse[kAVCOperand3] = ((signalFormat & 0x0000FF00) >> 8);
			pResponse[kAVCOperand4] = (signalFormat & 0x000000FF);
		}
		else
		{
			// If no connections, default plug signal type to NTSC-DV
			pResponse[kAVCOperand1] = ((kAVCPlugSignalFormatNTSCDV & 0xFF000000) >> 24);
			pResponse[kAVCOperand2] = ((kAVCPlugSignalFormatNTSCDV & 0x00FF0000) >> 16);
			pResponse[kAVCOperand3] = ((kAVCPlugSignalFormatNTSCDV & 0x0000FF00) >> 8);
			pResponse[kAVCOperand4] = (kAVCPlugSignalFormatNTSCDV & 0x000000FF);
		}
	}
	else
	{
		// This is a control type command to
		// a isoch output plug with no internal connection.
		// TODO: Today we just accept and ignore. But should we
		// maintain signal format for unconnected plugs?
		pResponse[kAVCCommandResponse] = kAVCAcceptedStatus;
	}

	AVCTARGETMUTEX_UNLOCK;
	
	targetSendAVCResponse(generation, nodeID, pBufMemDesc, len);

	return kIOReturnSuccess;
}

//////////////////////////////////////////////////////
// IOFireWireAVCTargetSpace::handleConnectionsCommand
//////////////////////////////////////////////////////
IOReturn IOFireWireAVCTargetSpace::handleConnectionsCommand(UInt16 nodeID, UInt32 generation, const char *buf, UInt32 len)
{
	UInt8 cType;
	UInt8 connectionCount;
	UInt32 responseLen;
	UInt8 *pBuf = (UInt8*) buf;
	UInt8 *pResponse;
	IOBufferMemoryDescriptor *pBufMemDesc = NULL;
	int i;
	AVCConnectionRecord *pConnection;
	UInt8 respIndex;
	UInt8 connectionType;

	//IOLog( "IOFireWireAVCTargetSpace::handleConnectionsCommand (0x%08X)\n",(int) this);

	// Check the length of the command. Don't handle command if wrong.
	if (len != 4)
		return kIOReturnError;

	// Parse the command buf
	cType = pBuf[0] & 0x0F;

	// Check the cType.  Don't handle command if not a status type.
	if (cType != kAVCStatusInquiryCommand)
		return kIOReturnError;

	if (pBuf[kAVCOperand0] != 0xFF)
		return kIOReturnError;
	
	AVCTARGETMUTEX_LOCK;

	connectionCount = (UInt8) (fConnectionRecords->getCount() & 0x000000FF);

	// Limit this command to only include up to 100 connection records in
	// the response, to prevent going beyond the max AVC response packet size
	if (connectionCount > 100)
		connectionCount = 100;

	responseLen = 4 + (connectionCount*5);

	// We will send a response to handle the command
	pBufMemDesc = IOBufferMemoryDescriptor::withCapacity(responseLen, kIODirectionOutIn);
	if(!pBufMemDesc)
	{
		AVCTARGETMUTEX_UNLOCK;
		return kFWResponseDataError;
	}
	pResponse = (UInt8 *) pBufMemDesc->getBytesNoCopy();

	pResponse[kAVCCommandResponse] = kAVCImplementedStatus;
	pResponse[kAVCAddress] = 0xFF;
	pResponse[kAVCOpcode] = 0x22;
	pResponse[kAVCOperand0] = connectionCount;

	respIndex = 4;

	for (i=(connectionCount-1);i>=0;i--)
	{
		pConnection = (AVCConnectionRecord *) fConnectionRecords->getObject(i);

		connectionType = 0xFC;
		if (pConnection->lockConnection)
			connectionType += 2;
		if (pConnection->permConnection)
			connectionType += 1;

		pResponse[respIndex++] = connectionType;
		pResponse[respIndex++] = pConnection->sourceSubunitTypeAndID;
		pResponse[respIndex++] = pConnection->sourcePlugNum;
		pResponse[respIndex++] = pConnection->destSubunitTypeAndID;
		pResponse[respIndex++] = pConnection->destPlugNum;
	}

	AVCTARGETMUTEX_UNLOCK;

	targetSendAVCResponse(generation, nodeID, pBufMemDesc, responseLen);

	return kIOReturnSuccess;
}

//////////////////////////////////////////////////////
// IOFireWireAVCTargetSpace::handleSignalSourceCommand
//////////////////////////////////////////////////////
IOReturn IOFireWireAVCTargetSpace::handleSignalSourceCommand(UInt16 nodeID, UInt32 generation, const char *buf, UInt32 len)
{
	//IOLog( "IOFireWireAVCTargetSpace::handleSignalSourceCommand (0x%08X)\n",(int) this);

	// TODO: Not yet Handled!
	return kIOReturnError;
}

//////////////////////////////////////////////////////
// IOFireWireAVCTargetSpace::pcrModified
//////////////////////////////////////////////////////
void IOFireWireAVCTargetSpace::pcrModified(IOFWAVCPlugTypes plugType,
										   UInt32 plugNum,
										   UInt32 newValue)
{
	AVCConnectionRecord *pConnection;
	AVCSubunitInfo *subUnitInfo;
	int i;
	
	//IOLog( "IOFireWireAVCTargetSpace::pcrModified (0x%08X)\n",(int) this);

	AVCTARGETMUTEX_LOCK;
	
	// Search through all the connection records to find subunit(s) connected
	// to this plug, and alert the subunit owner of the plug value modification
	for (i=(fConnectionRecords->getCount()-1);i>=0;i--)
	{
		pConnection = (AVCConnectionRecord *) fConnectionRecords->getObject(i);

		switch (plugType)
		{
			case IOFWAVCPlugIsochInputType:
				if ((pConnection->sourceSubunitTypeAndID == kAVCUnitAddress) &&
		(pConnection->sourcePlugNum == plugNum) &&
		(pConnection->destSubunitTypeAndID != kAVCUnitAddress) )
				{
					subUnitInfo = getSubunitInfo(pConnection->destSubunitTypeAndID);
					if (subUnitInfo)
						subUnitInfo->callBack(subUnitInfo,
							kIOFWAVCSubunitPlugMsgConnectedPlugModified,
							IOFWAVCPlugSubunitDestType,
							pConnection->destPlugNum,
							newValue,
							0,0);
				}
				break;

			case IOFWAVCPlugIsochOutputType:
				if ((pConnection->destSubunitTypeAndID == kAVCUnitAddress) &&
		(pConnection->destPlugNum == plugNum) &&
		(pConnection->sourceSubunitTypeAndID != kAVCUnitAddress))
				{
					subUnitInfo = getSubunitInfo(pConnection->sourceSubunitTypeAndID);
					if (subUnitInfo)
						subUnitInfo->callBack(subUnitInfo,
							kIOFWAVCSubunitPlugMsgConnectedPlugModified,
							IOFWAVCPlugSubunitSourceType,
							pConnection->sourcePlugNum,
							newValue,
							0,0);
				}
				break;

			default:
				break;
		};
	}

	AVCTARGETMUTEX_UNLOCK;
	
	return;
}

//////////////////////////////////////////////////////
// IOFireWireAVCTargetSpace::getSubunitInfo
//////////////////////////////////////////////////////
AVCSubunitInfo *IOFireWireAVCTargetSpace::getSubunitInfo(UInt32 subunitTypeAndID)
{
	AVCSubunitInfo *subunitInfo = NULL;
	int i;
	bool found = false;
	
	//IOLog( "IOFireWireAVCTargetSpace::getSubunitInfo (0x%08X)\n",(int) this);

	AVCTARGETMUTEX_LOCK;
	
	// Find the subunit in the list
	for (i=(fSubunits->getCount()-1);i>=0;i--)
	{
		subunitInfo = (AVCSubunitInfo *) fSubunits->getObject(i);
		if (subunitInfo->subunitTypeAndID == subunitTypeAndID)
		{
			found = true;
			break;
		}
	}

	AVCTARGETMUTEX_UNLOCK;

	if (found == true)
		return subunitInfo;
	else
		return NULL;
}

//////////////////////////////////////////////////////
// IOFireWireAVCTargetSpace::subUnitOfTypeCount
//////////////////////////////////////////////////////
UInt32 IOFireWireAVCTargetSpace::subUnitOfTypeCount(UInt32 type)
{
	UInt32 cnt = 0;
	UInt32 subUnitType;
	AVCSubunitInfo *subUnitInfo;
	int i;
	
	//IOLog( "IOFireWireAVCTargetSpace::subUnitOfTypeCount (0x%08X)\n",(int) this);

	AVCTARGETMUTEX_LOCK;
	
	for (i=(fSubunits->getCount()-1);i>=0;i--)
	{
		subUnitInfo = (AVCSubunitInfo *) fSubunits->getObject(i);
		subUnitType = ((subUnitInfo->subunitTypeAndID & 0xF8) >> 3);
		if (subUnitType == type)
			cnt+= 1;
    }

	AVCTARGETMUTEX_UNLOCK;

	return cnt;
}

//////////////////////////////////////////////////////
// IOFireWireAVCTargetSpace::canConnectDestPlug
//////////////////////////////////////////////////////
bool IOFireWireAVCTargetSpace::canConnectDestPlug(UInt32 destSubunitTypeAndID,
													 IOFWAVCPlugTypes destPlugType,
													 UInt32 *destPlugNum)
{
	bool res=true;
	int i;
	AVCConnectionRecord *connection;
	UInt32 actualPlugNumber = *destPlugNum;
	
	// Search the connection records for a connection with this dest plug.
	// If we find one (and there can be at most one), check the lock and
	// perm bits. If both clear, disconnect the connection,
	// then return true, if either is set return false.
	// If no connection is found, return true.
	// If this dest plug is kAVCInvalidPlug, look for any connection, with
	// a dest plug matching this subunit/type, that can be disconnected.
	// AVCTARGETMUTEX_LOCK assumed already set!!!

	for (i=(fConnectionRecords->getCount()-1);i>=0;i--)
	{
		connection = (AVCConnectionRecord *) fConnectionRecords->getObject(i);

		if ((connection->destSubunitTypeAndID == destSubunitTypeAndID) &&
			(connection->destPlugType == destPlugType))
		{
			if ((actualPlugNumber == kAVCInvalidPlug) || (connection->destPlugNum == *destPlugNum))
			{
				if ((connection->lockConnection == false) && (connection->permConnection == false))
				{
					actualPlugNumber = connection->destPlugNum;

					// Disconnect this connection
					disconnectTargetPlugs(NULL,
						   connection->sourceSubunitTypeAndID,
						   connection->sourcePlugType,
						   connection->sourcePlugNum,
						   connection->destSubunitTypeAndID,
						   connection->destPlugType,
						   connection->destPlugNum);

					break;	// No need to look anymore
				}
				else if (connection->destPlugNum == *destPlugNum)
				{
					res = false;
					break;	// No need to look anymore
				}
			}
		}
	}

	if (actualPlugNumber == kAVCInvalidPlug)
	{
		res = false;
	}
	else
	{
		*destPlugNum = actualPlugNumber;
	}
	
	return res;
}
	