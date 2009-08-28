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
#include <IOKit/avc/IOFireWireAVCUnit.h>
#include <IOKit/avc/IOFireWireAVCCommand.h>
#include <IOKit/avc/IOFireWireAVCConsts.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/firewire/IOFireWireUnit.h>
#include <IOKit/firewire/IOFireWireBus.h>
#include <IOKit/firewire/IOFWAddressSpace.h>
#include <IOKit/firewire/IOFireWireController.h>
#include <IOKit/avc/IOFireWirePCRSpace.h>

#if FIRELOG
#import <IOKit/firewire/FireLog.h>
#define FIRELOG_MSG(x) FireLog x
#else
#define FIRELOG_MSG(x) do {} while (0)
#endif

const OSSymbol *gIOFireWireAVCUnitType;
const OSSymbol *gIOFireWireAVCSubUnitType;
const OSSymbol *gIOFireWireAVCSubUnitCount[kAVCNumSubUnitTypes];

OSDefineMetaClassAndStructors(IOFireWireAVCAsynchronousCommand, IOCommand)
OSMetaClassDefineReservedUnused(IOFireWireAVCAsynchronousCommand, 0);
OSMetaClassDefineReservedUnused(IOFireWireAVCAsynchronousCommand, 1);
OSMetaClassDefineReservedUnused(IOFireWireAVCAsynchronousCommand, 2);
OSMetaClassDefineReservedUnused(IOFireWireAVCAsynchronousCommand, 3);

OSDefineMetaClass(IOFireWireAVCNub, IOService)
OSDefineAbstractStructors(IOFireWireAVCNub, IOService)
//OSMetaClassDefineReservedUnused(IOFireWireAVCNub, 0);
OSMetaClassDefineReservedUnused(IOFireWireAVCNub, 1);
OSMetaClassDefineReservedUnused(IOFireWireAVCNub, 2);
OSMetaClassDefineReservedUnused(IOFireWireAVCNub, 3);

OSDefineMetaClassAndStructors(IOFireWireAVCUnit, IOFireWireAVCNub)
OSMetaClassDefineReservedUnused(IOFireWireAVCUnit, 0);
OSMetaClassDefineReservedUnused(IOFireWireAVCUnit, 1);
OSMetaClassDefineReservedUnused(IOFireWireAVCUnit, 2);
OSMetaClassDefineReservedUnused(IOFireWireAVCUnit, 3);

//////////////////////////////////////////////////////
// IOFireWireAVCAsynchronousCommand::isPending
//////////////////////////////////////////////////////
bool IOFireWireAVCAsynchronousCommand::isPending(void)
{
	bool res;
		
	switch (cmdState)
	{
		case kAVCAsyncCommandStateRequestSent:				// Command has been submitted, but no write done yet
		case kAVCAsyncCommandStateWaitingForResponse:		// Received write done, but no first response
		case kAVCAsyncCommandStateReceivedInterimResponse:  // Received interim response, but no final response
			res = true;
			break;

		case kAVCAsyncCommandStatePendingRequest:			// Command created, but not yet submitted
		case kAVCAsyncCommandStateRequestFailed:			// Submitting the request failed
		case kAVCAsyncCommandStateReceivedFinalResponse:	// Received a final response
		case kAVCAsyncCommandStateTimeOutBeforeResponse:	// Timeout before first response
		case kAVCAsyncCommandStateBusReset:					// Bus reset before first response
		case kAVCAsyncCommandStateOutOfMemory:				// Ran out of memory
		case kAVCAsyncCommandStateCanceled:					// Command cancled
		default:
			res = false;
			break;
	};
	
	return res;
}

//////////////////////////////////////////////////////
// IOFireWireAVCAsynchronousCommand::free
//////////////////////////////////////////////////////
void IOFireWireAVCAsynchronousCommand::free()
{
    FIRELOG_MSG(("IOFireWireAVCAsynchronousCommand::free (this=0x%08X)\n",this));

	// Only allow a free if we're not pending.
	if ( isPending() )
	{
		cancel();
	}

	fWriteNodeID = kIOFWAVCAsyncCmdFreed;	// Special flag to indicate this command was cancled in this unit's free routine.
	// The command is now canceled
	cmdState	 = kAVCAsyncCommandStateCanceled;

	if ( fWriteCmd->Busy() )
	{
		fWriteCmd->cancel(kIOReturnOffline);
	}

	if ( fWriteCmd )
	{
		fWriteCmd->release();
		fWriteCmd = NULL;
	}
	
	if( fDelayCmd->Busy() )
	{
		fDelayCmd->cancel(kIOReturnOffline);
	}

	if (fDelayCmd)
	{
		fDelayCmd->release();
		fDelayCmd = NULL;
	}

	if (fMem)
	{
		fMem->release();
		fMem = NULL;
	}

	if (pCommandBuf)
	{
		delete[] pCommandBuf;
		pCommandBuf = NULL;
	}
	
	if (pInterimResponseBuf)
	{
		delete[] pInterimResponseBuf;
		pInterimResponseBuf = NULL;
	}
	
	if (pFinalResponseBuf)
	{
		delete[] pFinalResponseBuf;
		pFinalResponseBuf = NULL;
	}

	OSObject::free();
}

//////////////////////////////////////////////////////
// IOFireWireAVCAsynchronousCommand::init
//////////////////////////////////////////////////////
IOReturn IOFireWireAVCAsynchronousCommand::init(const UInt8 * command, 
												UInt32 len, 
												IOFireWireAVCAsynchronousCommandCallback completionCallback, 
												void *pClientRefCon)
{
	FIRELOG_MSG(("IOFireWireAVCAsynchronousCommand::init (this=0x%08X, opCode=0x%02X)\n",this,command[kAVCOpcode]));

	// Validate the length of the command buffer
    if(len == 0 || len > 512)
        return kIOReturnBadArgument;
	
	// Initialize async command object
	pCommandBuf = new UInt8[len];
	if (!pCommandBuf)
		return kIOReturnNoMemory;
	bcopy(command, pCommandBuf, len);
	cmdLen = len;
	cmdState = kAVCAsyncCommandStatePendingRequest;
	fCallback = completionCallback;
	pRefCon = pClientRefCon;
	pInterimResponseBuf = NULL;
	interimResponseLen = 0;
	pFinalResponseBuf = NULL;
	finalResponseLen = 0;
	fAVCUnit = NULL;
	fMem = NULL;
	fWriteCmd = NULL;
	fDelayCmd = NULL;
	fWriteNodeID = kFWBadNodeID;
	fWriteGen = 0xFFFFFFFF;
	
	return kIOReturnSuccess;
}

//////////////////////////////////////////////////////
// IOFireWireAVCAsynchronousCommand::submit
//////////////////////////////////////////////////////
IOReturn IOFireWireAVCAsynchronousCommand::submit(IOFireWireAVCNub *pAVCNub)
{
	IOReturn res;
	FWAddress addr;
	IOFireWireAVCUnit *pAVCUnit;
	IOFireWireAVCSubUnit *pAVCSubunit;
	
    FIRELOG_MSG(("IOFireWireAVCAsynchronousCommand::submit (this=0x%08X, opCode=0x%02X)\n",this,pCommandBuf[kAVCOpcode]));

	retain();
	
	// If fWriteNodeID is kIOFWAVCAsyncCmdFreed, this command was cancled in the AVCUnit's free routine,
	// so, we will not let this command be reinited or submitted again!
	if (fWriteNodeID == kIOFWAVCAsyncCmdFreed)
	{
		release();
		return kIOReturnNotPermitted;
	}
	
	// Figure out if the nub is a unit or subunit
	if (OSDynamicCast(IOFireWireAVCUnit, pAVCNub) != NULL)
	{
		pAVCUnit = (IOFireWireAVCUnit*) pAVCNub;
	}
	else if (OSDynamicCast(IOFireWireAVCSubUnit, pAVCNub) != NULL)
	{
		pAVCSubunit = (IOFireWireAVCSubUnit*) pAVCNub;
		pAVCUnit = pAVCSubunit->fAVCUnit;
	}
	else
	{
		release();
		return kIOReturnBadArgument; 
	}
	
    // setup AVC Request address
    addr.addressHi   = kCSRRegisterSpaceBaseAddressHi;
    addr.addressLo   = kFCPCommandAddress;

	// Only submit the write command, if we are pending request
	if (cmdState != kAVCAsyncCommandStatePendingRequest)
	{
		release();
		return kIOReturnNotPermitted;
	}
	
	// Save a pointer to the unit
	fAVCUnit = pAVCUnit;
	
	if ( not fAVCUnit->available() )
	{
		release();
		return kIOReturnNotPermitted;
	}
	
	// Create a memory descriptor for the request bytes
	fMem = IOMemoryDescriptor::withAddress((void *)pCommandBuf,
										   cmdLen,
										   kIODirectionOutIn);
	if(!fMem)
	{
		release();
		return kIOReturnNoMemory;
	}
	
	// Prepare the memory descriptor
	IOReturn err = fMem->prepare();
	if( err != kIOReturnSuccess )
	{
		release();
		return kIOReturnNoMemory;
	}
		
	// Create a write command
	fWriteCmd = fAVCUnit->fDevice->createWriteCommand(addr,
											fMem,
											IOFireWireAVCUnit::AVCAsynchRequestWriteDone,
											this);
	if(!fWriteCmd)
	{
		release();
		return kIOReturnNoMemory;
	}
	
	// Create a delay command for providing a timeout when waiting for AVC response
	fDelayCmd = fAVCUnit->fIOFireWireAVCUnitExpansion->fControl->createDelayedCmd(250000, 
																					IOFireWireAVCUnit::AVCAsynchDelayDone, 
																					this);
	if (!fDelayCmd)
	{
		release();
		return kIOReturnNoMemory;
	}
	
	// Get the async command lock
	fAVCUnit->lockAVCAsynchronousCommandLock();

	// Add this command to the unit's array of pending async commands
	// Try to add the new command to our array of outstanding commands
	if(!fAVCUnit->fIOFireWireAVCUnitExpansion->fAVCAsyncCommands->setObject(this))
	{
		res = kIOReturnNoMemory;
	}
	else
	{
		// Submit the write command
		res = fWriteCmd->submit();
		// Note that the write done routine may have already been called during the submit
		// Only change the command state here if it hasn't already changed
		if (cmdState == kAVCAsyncCommandStatePendingRequest)
		{
			if (res == kIOReturnSuccess)
				cmdState = kAVCAsyncCommandStateRequestSent;
			else
				cmdState = kAVCAsyncCommandStateRequestFailed;
		}
	}
	
	// Free the async command lock
	fAVCUnit->unlockAVCAsynchronousCommandLock();

	release();

	return res;
}

//////////////////////////////////////////////////////
// IOFireWireAVCAsynchronousCommand::reinit
//////////////////////////////////////////////////////
IOReturn IOFireWireAVCAsynchronousCommand::reinit(const UInt8 * command, UInt32 len)
{
    FIRELOG_MSG(("IOFireWireAVCAsynchronousCommand::reinit (this=0x%08X)\n",this));

	// If fWriteNodeID is kIOFWAVCAsyncCmdFreed, this command was cancled in the AVCUnit's free routine,
	// so, we will not let this command be reinited or submitted again!
	if (fWriteNodeID == kIOFWAVCAsyncCmdFreed)
		return kIOReturnNotPermitted;

	// Only allow a reinit if we're not pending.
	if (isPending())
		return kIOReturnNotPermitted;

	// Validate the length of the command buffer
    if(len == 0 || len > 512)
        return kIOReturnBadArgument;
	
	if (fWriteCmd)
		fWriteCmd->release();
	
	if (fDelayCmd)
		fDelayCmd->release();
	
	if (fMem)
		fMem->release();
	
	if (pCommandBuf)
		delete pCommandBuf;
	
	if (pInterimResponseBuf)
		delete pInterimResponseBuf;
	
	if (pFinalResponseBuf)
		delete pFinalResponseBuf;

	// Initialize async command object
	pCommandBuf = new UInt8[len];
	if (!pCommandBuf)
		return kIOReturnNoMemory;
	bcopy(command, pCommandBuf, len);

	cmdLen = len;
	cmdState = kAVCAsyncCommandStatePendingRequest;
	pInterimResponseBuf = NULL;
	interimResponseLen = 0;
	pFinalResponseBuf = NULL;
	finalResponseLen = 0;
	fAVCUnit = NULL;
	fMem = NULL;
	fWriteCmd = NULL;
	fDelayCmd = NULL;
	fWriteNodeID = kFWBadNodeID;
	fWriteGen = 0xFFFFFFFF;
	
	return kIOReturnSuccess;
}

//////////////////////////////////////////////////////
// IOFireWireAVCAsynchronousCommand::cancel
//////////////////////////////////////////////////////
IOReturn IOFireWireAVCAsynchronousCommand::cancel(void)
{
	// Local Vars
	UInt32 cmdIndex;
	
    FIRELOG_MSG(("IOFireWireAVCAsynchronousCommand::cancel (this=0x%08X)\n",this));

	// TODO: What if the AVCUnit is already been freed?
	
	// TODO: Do some state checking before continuing
	
	// Get the async command lock
	fAVCUnit->lockAVCAsynchronousCommandLock();
	
	// Cancel the delay command, and write command (if needed?)
	if ((cmdState == kAVCAsyncCommandStateRequestSent) && (fWriteCmd))
		fWriteCmd->cancel(kIOReturnAborted);
	else if ((cmdState == kAVCAsyncCommandStateWaitingForResponse) && (fDelayCmd))
			fDelayCmd->cancel(kIOReturnAborted);
				
	// The command is now canceled
	cmdState = kAVCAsyncCommandStateCanceled;

	// Remove this object from the unit's array
	cmdIndex = fAVCUnit->indexOfAVCAsynchronousCommandObject(this);
	if (cmdIndex != 0xFFFFFFFF)
	{
		fAVCUnit->removeAVCAsynchronousCommandObjectAtIndex(cmdIndex);
	}
	
	// Free the async command lock
	fAVCUnit->unlockAVCAsynchronousCommandLock();

	// We do a client callback here
	if (fCallback != NULL)
		fCallback(pRefCon,this);
		
	return kIOReturnSuccess;
}	

//////////////////////////////////////////////////////
// IOFireWireAVCUnit::setProperties
//////////////////////////////////////////////////////
IOReturn IOFireWireAVCUnit::setProperties (OSObject * properties )
{
	IOReturn result = kIOReturnSuccess ;
	
	//IOLog(IOFireWireAVCUnit::setProperties\n");
	
	OSDictionary*	dict = OSDynamicCast( OSDictionary, properties ) ;
	
	if ( dict )
	{
		OSObject*	value = dict->getObject( "RobustAVCResponseMatching" ) ;
		
		if ( value )
		{
			// Disable robust AV/C command/response matching
			//IOLog("Disabling RobustAVCResponseMatching for AV/C device 0x%08X\n",(unsigned int) this);
			fIOFireWireAVCUnitExpansion->enableRobustAVCCommandResponseMatching = false;
		}
		else
		{
			result = IOFireWireAVCNub::setProperties ( properties ) ;
		}
	}
	else
		result = IOFireWireAVCNub::setProperties ( properties ) ;

	return result ;
}

//////////////////////////////////////////////////////
// IOFireWireAVCUnit::AVCResponse
//////////////////////////////////////////////////////
UInt32 IOFireWireAVCUnit::AVCResponse(void *refcon, UInt16 nodeID, IOFWSpeed &speed,
                    FWAddress addr, UInt32 len, const void *buf, IOFWRequestRefCon requestRefcon)
{
	// Local Vars
    IOFireWireAVCUnit *me = (IOFireWireAVCUnit *)refcon;
	UInt8 *pResponseBytes = (UInt8*) buf;
	UInt32 res = kFWResponseAddressError;
	UInt32 i;
	IOFireWireAVCAsynchronousCommand *pCmd;
	bool foundOutstandingAVCAsynchCommandForNode = false;
	bool matchFound = false;
	UInt32 matchedCommandIndex;
	bool doCallback = false;

    FIRELOG_MSG(("IOFireWireAVCUnit::AVCResponse (this=0x%08X)\n",me));
    FIRELOG_MSG(("AVCResponse Info: nodeID=0x%04X, opCode=0x%02X, avcAddress=0x%02X respLen=0x%08X\n",nodeID, pResponseBytes[kAVCOpcode], pResponseBytes[kAVCAddress],len));

	// Check this packet for validity
	if ((addr.addressLo != kFCPResponseAddress) || (len < 3))
		return res;

	// Get the async command lock
	me->lockAVCAsynchronousCommandLock();

	// Look through all the pending AVCAsynch commands to find a match
	for (i = 0; i < me->fIOFireWireAVCUnitExpansion->fAVCAsyncCommands->getCount(); i++)
	{
		pCmd = (IOFireWireAVCAsynchronousCommand*) me->fIOFireWireAVCUnitExpansion->fAVCAsyncCommands->getObject(i);
		FIRELOG_MSG(("Evaluating outstanding AVC async cmd %d (%d total): cmd = 0x%08X, nodeID = 0x%04X, opCode=0x%02X, avcAddress=0x%02X pending=%s\n",
					 i,
					 me->fIOFireWireAVCUnitExpansion->fAVCAsyncCommands->getCount(),
					 pCmd,
					 pCmd->fWriteNodeID,
					 pCmd->pCommandBuf[kAVCOpcode] ,
					 pCmd->pCommandBuf[kAVCAddress],
					 (pCmd->isPending() ? "YES" : "NO")
					 ));
	
		// Does the nodeID match?
		if (pCmd->fWriteNodeID == nodeID)
		{
			// Mark that we found at least one pending AVCAsync command for this node
			foundOutstandingAVCAsynchCommandForNode = true;
			
			// Evaluate the AVCAddress, and Opcode, looking for a match
			if ((pCmd->pCommandBuf[kAVCAddress] == pResponseBytes[kAVCAddress]) && (pCmd->pCommandBuf[kAVCOpcode] == pResponseBytes[kAVCOpcode]))
			{
				if ((pCmd->pCommandBuf[kAVCCommandResponse] == kAVCNotifyCommand) && 
						((pResponseBytes[kAVCCommandResponse] == kAVCAcceptedStatus) || (pResponseBytes[kAVCCommandResponse] == kAVCInTransitionStatus) || (pResponseBytes[kAVCCommandResponse] == kAVCImplementedStatus)))
				{
					// This is not a match because notify commands cannot have this type of response!
				}
				else
				{
					// This is a match
					matchFound = true;
					matchedCommandIndex = i;
					break;
				}
			}
		}
	}
	
	// If we didn't match, yet we have an oustanding command for this node, and the response is from a tape-subunit,
	// see if this is the special-case of the tape-subunit transport-state command, which overwrites the opcode
	// in the response packet.
	if ((!matchFound) && (foundOutstandingAVCAsynchCommandForNode) && ((pResponseBytes[kAVCAddress] & 0xF8) == 0x20))
	{
		// Look again through all the pending AVCAsynch commands to find a match
		for (i = 0; i < me->fIOFireWireAVCUnitExpansion->fAVCAsyncCommands->getCount(); i++)
		{
			pCmd = (IOFireWireAVCAsynchronousCommand*) me->fIOFireWireAVCUnitExpansion->fAVCAsyncCommands->getObject(i);

			// Does the nodeID match?
			if (pCmd->fWriteNodeID == nodeID)
			{
				// Evaluate the AVCAddress for a match, and see if this command is the tape subunit transport-state opcode
				if ((pCmd->pCommandBuf[kAVCAddress] == pResponseBytes[kAVCAddress]) && (pCmd->pCommandBuf[kAVCOpcode] == 0xD0))
				{
					// It is a tape-subunit transport state command. See if the response packet look like it could indeed 
					// be the one we're looking for
					if (((pResponseBytes[kAVCOpcode] == 0xC1) || 
						 (pResponseBytes[kAVCOpcode] == 0xC2) || 
						 (pResponseBytes[kAVCOpcode] == 0xC3) ||
						 (pResponseBytes[kAVCOpcode] == 0xC4)) && (len == 4))
					{
						if ((pCmd->pCommandBuf[kAVCCommandResponse] == kAVCNotifyCommand) && 
							((pResponseBytes[kAVCCommandResponse] == kAVCAcceptedStatus) || (pResponseBytes[kAVCCommandResponse] == kAVCInTransitionStatus) || (pResponseBytes[kAVCCommandResponse] == kAVCImplementedStatus)))
						{
							// This is not a match because notify commands cannot have this type of response!
						}
						else
						{
							// This is a match
							matchFound = true;
							matchedCommandIndex = i;
							break;
						}
					}
				}
			}
		}
	}
	
	// We found a match, so deal with it
	if (matchFound)
	{
		FIRELOG_MSG(("AVC Async Request/Response Match Found: %d\n",matchedCommandIndex));

		// At this point, if the interim response buffer is NULL, then this is the
		// first response for this command, so now is the time to cancle the timer
		if (pCmd->pInterimResponseBuf == NULL)
		{
			// Abort the timeOut delay command
			if (pCmd->fDelayCmd)
				pCmd->fDelayCmd->cancel(kIOReturnAborted);
		}
		
		// Is this an Interim, or Final Response
		if (pResponseBytes[kAVCCommandResponse] == 0x0F)
		{
			// Interim Response

			// Allocate the command's interim response buffer, and copy response bytes
			pCmd->pInterimResponseBuf = new UInt8[len];
			if (pCmd->pInterimResponseBuf)
			{
				pCmd->interimResponseLen = len;
				bcopy(pResponseBytes, pCmd->pInterimResponseBuf, len);

				// Set the command' state
				pCmd->cmdState = kAVCAsyncCommandStateReceivedInterimResponse;
			}
			else
			{
				pCmd->cmdState = kAVCAsyncCommandStateOutOfMemory;

				// Remove this command from the unit's pending async command list
				me->removeAVCAsynchronousCommandObjectAtIndex(matchedCommandIndex);
			}
		}
		else
		{
			// Final Response
			
			// Allocate the command's final response buffer, and copy response bytes
			pCmd->pFinalResponseBuf = new UInt8[len];
			if (pCmd->pFinalResponseBuf)
			{
				pCmd->finalResponseLen = len;
				bcopy(pResponseBytes, pCmd->pFinalResponseBuf, len);

				// Set the command' state
				pCmd->cmdState = kAVCAsyncCommandStateReceivedFinalResponse;
			}
			else
				pCmd->cmdState = kAVCAsyncCommandStateOutOfMemory;
			
			// Remove this command from the unit's pending async command list
			me->removeAVCAsynchronousCommandObjectAtIndex(matchedCommandIndex);
		}
		
		// We need to do a callback after we release the lock
		doCallback = true;

		res = kFWResponseComplete;
	}
	
	// Free the async command lock
	me->unlockAVCAsynchronousCommandLock();

	// Se if we need to do a callback to the client
	if (doCallback == true)
	{
		// Notify the client
		if (pCmd->fCallback != NULL)
			pCmd->fCallback(pCmd->pRefCon,pCmd);
	}
	
	// If we don't have a match, see if there is a pending blocking-AVC command for this node
	if (!matchFound)
	{
		// if this is for us, copy the status bytes from fPseudoSpace 
		if(me->fCommand)
		{
			if (me->fIOFireWireAVCUnitExpansion->enableRobustAVCCommandResponseMatching)
				res = me->fCommand->handleResponse(nodeID, len, buf);
			else
				res = me->fCommand->handleResponseWithSimpleMatching(nodeID, len, buf);
		}
	}

    return res;
}

//////////////////////////////////////////////////////
// IOFireWireAVCUnit::rescanSubUnits
//////////////////////////////////////////////////////
void IOFireWireAVCUnit::rescanSubUnits(void *arg)
{

    IOFireWireAVCUnit *me = (IOFireWireAVCUnit *)arg;

	FIRELOG_MSG(("IOFireWireAVCUnit::rescanSubUnits (this=0x%08X)\n",me));
    
    me->updateSubUnits(false);
}

//////////////////////////////////////////////////////
// IOFireWireAVCUnit::updateSubUnits
//////////////////////////////////////////////////////
void IOFireWireAVCUnit::updateSubUnits(bool firstTime)
{
	FIRELOG_MSG(("IOFireWireAVCUnit::updateSubUnits (this=0x%08X)\n",this));
	
    IOReturn res;
    UInt32 size;
    UInt8 cmd[8],response[8];
    OSObject *prop;
    bool hasFCP = true;
// Get SubUnit info
    cmd[kAVCCommandResponse] = kAVCStatusInquiryCommand;
    cmd[kAVCAddress] = kAVCUnitAddress;
    cmd[kAVCOpcode] = kAVCSubunitInfoOpcode;
    cmd[kAVCOperand0] = 7;
    cmd[4] = cmd[5] = cmd[6] = cmd[7] = 0xff;
    size = 8;

    for(int i = 0; i<10; i++) {
        res = AVCCommand(cmd, 8, response, &size);
        if(res == (kIOFireWireResponseBase + kFWResponseConflictError)) {
            IOSleep(10);
            continue;	// Try again
        }
        else if(res == kIOReturnSuccess && response[kAVCOperand1] == 0xff) {
            // Some devices initially say they have no subunits.
            IOSleep(10);
            continue;	// Try again
        }
		else if(res == kIOReturnOffline){
			// Bus-reset occurred.
			FIRELOG_MSG(("IOFireWireAVCUnit %p, bus-reset during subunit scan! firstTime=%s\n",this,firstTime == true ? "true" : "false"));
			IOSleep(10);
			continue;	// Try again
		}
        else
            break;		// Got a final result code
    }
    if(res != kIOReturnSuccess || response[kAVCCommandResponse] != kAVCImplementedStatus) {
        if(firstTime) {
            // Sony convertor box doesn't do AVC, make it look like a camcorder.
            // Panasonic NV-C5 doesn't support SubunitInfo query but does support VCR commands
            if(res != kIOReturnSuccess)
                hasFCP = false;
            
            response[kAVCOperand1] = 0x20;	// One VCR
            response[kAVCOperand2] = 0xff;
            response[kAVCOperand3] = 0xff;
            response[kAVCOperand4] = 0xff;
        }
        else
		{
			this->release();	// If this is not the first-time we need to release before returning
			return;	// No update necessary
		}
    }
    else if(size == 5) {
        // some mLAN devices don't report their subunit info correctly,
        // set it up here
        size = 8;
        response[kAVCOperand1] = 0x08;	// One Audio subunit
        response[kAVCOperand2] = 0xff;
        response[kAVCOperand3] = 0xff;
        response[kAVCOperand4] = 0xff;
    }
    if(firstTime)
        setProperty("supportsFCP", hasFCP);
    
    // Zero count of subunits before updating with new counts
    bzero(fSubUnitCount, sizeof(fSubUnitCount));
    for(int i=0; i<kAVCNumSubUnitTypes; i++) {
        removeProperty(gIOFireWireAVCSubUnitCount[i]);
    }
    
    for(int i=0; i<4; i++) {
        UInt8 val = response[kAVCOperand1+i];
        if(val != 0xff) {
            UInt8 type, num;
            type = val >> 3;
            num = (val & 0x7)+1;
            fSubUnitCount[type] = num;
            //IOLog("Subunit type %x, num %d\n", type, num);
            setProperty(gIOFireWireAVCSubUnitCount[type]->getCStringNoCopy(), num, 8);
            
            // Create sub unit nub if it doesn't exist
            IOFireWireAVCSubUnit *sub = NULL;
            OSDictionary * propTable = 0;
            do {
                propTable = OSDictionary::withCapacity(6);
                if(!propTable)
                    break;
                prop = OSNumber::withNumber(type, 32);
                propTable->setObject(gIOFireWireAVCSubUnitType, prop);
                prop->release();
                if(!firstTime) {
                    OSIterator *childIterator;
                    IOFireWireAVCSubUnit * found = NULL;
                    childIterator = getClientIterator();
                    if(childIterator) {
                        OSObject *child;
                        while( (child = childIterator->getNextObject())) {
                            found = OSDynamicCast(IOFireWireAVCSubUnit, child);
                            if(found && found->matchPropertyTable(propTable)) {
                                break;
                            }
                            else
                                found = NULL;
                        }
                        childIterator->release();
                        if(found) {
                            break;
                        }
                    }
                }
                sub = new IOFireWireAVCSubUnit;
                if(!sub)
                    break;

                if (!sub->init(propTable, this))
                    break;
                if (!sub->attach(this))	
                    break;
                sub->setProperty("supportsFCP", hasFCP);

                sub->registerService();
                
				// Special handling for Sony TVs - make them root!
				if (type == 0)
				{
					OSObject *prop;
					OSNumber *deviceGUID;
					unsigned long long guidVal;

					prop = getProperty(gFireWire_GUID);
					deviceGUID = OSDynamicCast( OSNumber, prop );
					guidVal = deviceGUID->unsigned64BitValue();
					
					if ((guidVal & 0xFFFFFF0000000000LL) == 0x0800460000000000LL) // Sony
					{
						fDevice->setNodeFlags(kIOFWMustBeRoot);
					}
				}
				
            } while (0);
            if(sub)
                sub->release();
            if(propTable)
                propTable->release();
        }
    }
    
    // Prune sub units that have gone away.
    if(!firstTime) {
        OSIterator *childIterator;
        IOFireWireAVCSubUnit * sub = NULL;
        childIterator = getClientIterator();
        if(childIterator) {
            OSObject *child;
            while( (child = childIterator->getNextObject())) {
                sub = OSDynamicCast(IOFireWireAVCSubUnit, child);
                if(sub) {
                    OSNumber *type;
                    type = OSDynamicCast(OSNumber, sub->getProperty(gIOFireWireAVCSubUnitType));
                    if(type && !fSubUnitCount[type->unsigned32BitValue()])
                        sub->terminate();
                }
            }
            childIterator->release();
        }
    }
	
	if (!firstTime)
		this->release(); // If this is not the first-time we need to release before returning
}

//////////////////////////////////////////////////////
// IOFireWireAVCUnit::start
//////////////////////////////////////////////////////
bool IOFireWireAVCUnit::start(IOService *provider)
{
	FIRELOG_MSG(("IOFireWireAVCUnit::start (this=0x%08X)\n",this));

    OSObject *prop;
    UInt32 type;
	OSNumber *deviceGUID;
	unsigned long long guidVal;
	UInt8 series;

    fDevice = OSDynamicCast(IOFireWireNub, provider);
    if(!fDevice)
        return false;
	
	// Retain our provider, the IOFireWireUnit object
	fDevice->retain();

	// create/clear expansion data
	fIOFireWireAVCUnitExpansion = (ExpansionData*) IOMalloc( sizeof(ExpansionData) );
	if( fIOFireWireAVCUnitExpansion == NULL )
		return false;
	else
		bzero( fIOFireWireAVCUnitExpansion, sizeof(ExpansionData) );

	// Get the controller
	fIOFireWireAVCUnitExpansion->fControl = fDevice->getController();
    if(!fIOFireWireAVCUnitExpansion->fControl)
		return false;
	
	// Enable robust AV/C Command/Response Matching
	fIOFireWireAVCUnitExpansion->enableRobustAVCCommandResponseMatching = true;
	
	// Create array to hold outstanding async AVC commands
	fIOFireWireAVCUnitExpansion->fAVCAsyncCommands = OSArray::withCapacity(1);

    if(!gIOFireWireAVCUnitType)
        gIOFireWireAVCUnitType = OSSymbol::withCString("Unit_Type");
    if(!gIOFireWireAVCUnitType)
		return false;

    if(!gIOFireWireAVCSubUnitType)
        gIOFireWireAVCSubUnitType = OSSymbol::withCString("SubUnit_Type");
    if(!gIOFireWireAVCSubUnitType)
		return false;

    for(int i=0; i<kAVCNumSubUnitTypes; i++) {
        char buff[16];
        if(!gIOFireWireAVCSubUnitCount[i]) {
            snprintf(buff, sizeof(buff), "AVCSubUnit_%x", i);
            gIOFireWireAVCSubUnitCount[i] = OSSymbol::withCString(buff);
            if(!gIOFireWireAVCSubUnitCount[i])
				return false;
        }
    }
    
    if( !IOService::start(provider))
		return false;

    fFCPResponseSpace = fDevice->getBus()->createInitialAddressSpace(kFCPResponseAddress, 512,
                                                                        NULL, AVCResponse, this);
    if(!fFCPResponseSpace)
		return false;

    fFCPResponseSpace->activate();
    
    avcLock = IOLockAlloc();
    if (avcLock == NULL) {
        IOLog("IOAVCUnit::start avcLock failed\n");
		return false;
    }
    
    cmdLock = IOLockAlloc();
    if (cmdLock == NULL) {
        IOLog("IOAVCUnit::start cmdLock failed\n");
        return false;
    }
    
// Get Unit type
    IOReturn res;
    UInt32 size;
    UInt8 cmd[8],response[8];
	UInt32 unitInfoRetryCount = 0;

    cmd[kAVCCommandResponse] = kAVCStatusInquiryCommand;
    cmd[kAVCAddress] = kAVCUnitAddress;
    cmd[kAVCOpcode] = kAVCUnitInfoOpcode;
    cmd[3] = cmd[4] = cmd[5] = cmd[6] = cmd[7] = 0xff;
    size = 8;
    res = AVCCommand(cmd, 8, response, &size);
	if(kIOReturnSuccess != res)
	{
		do
		{
			unitInfoRetryCount++;
			IOSleep(2000);	// two seconds, give device time to get it's act together
			size = 8;
			res = AVCCommand(cmd, 8, response, &size);
		}while((kIOReturnSuccess != res) && (unitInfoRetryCount <= 4));
    }

	if(kIOReturnSuccess != res || response[kAVCCommandResponse] != kAVCImplementedStatus)
        type = kAVCVideoCamera;	// Anything that doesn't implement AVC properly is probably a camcorder!
    else
        type = IOAVCType(response[kAVCOperand1]);

    // Copy over matching properties from FireWire Unit
    prop = provider->getProperty(gFireWireVendor_ID);
    if(prop)
        setProperty(gFireWireVendor_ID, prop);


	prop = provider->getProperty(gFireWire_GUID);
    if(prop)
	{
        setProperty(gFireWire_GUID, prop);

		// Check the guid to see if this device requires special asynch throttling
		deviceGUID = OSDynamicCast( OSNumber, prop );
		guidVal = deviceGUID->unsigned64BitValue();
		if ((guidVal & 0xFFFFFFFFFF000000LL) == 0x0000850000000000LL)
		{
			series = (UInt8) ((guidVal & 0x0000000000FF0000LL) >> 16);
			if ((series <= 0x13) || ((series >= 0x18) && (series <= 0x23)))
				fDevice->setNodeFlags( kIOFWLimitAsyncPacketSize );
			
			series = (UInt8) (((guidVal & 0x00000000FFFFFFFFLL) >> 18) & 0x3f); // GL-2
			if(series == 0x19) // GL-2
				fDevice->setNodeFlags(kIOFWMustNotBeRoot);
		}
		
		if ((guidVal & 0xFFFFFF0000000000LL) == 0x0080450000000000LL) // panasonic
		{
			series = (UInt8) ((guidVal & 0x0000000000FF0000LL) >> 16);
			
			prop = provider->getProperty(gFireWireProduct_Name);
			if(prop)
			{
				OSString * string = OSDynamicCast ( OSString, prop ) ;
				if (string->isEqualTo("PV-GS15"))
				{
					fDevice->setNodeFlags(kIOFWMustNotBeRoot);
					fDevice->setNodeFlags(kIOFWMustHaveGap63);
					IOLog("Panasonic guid=%lld series=%x model=%s\n", guidVal, series, string->getCStringNoCopy()); // node flags happens here
				}
				else if (string->isEqualTo("PV-GS120 "))
				{
					fDevice->setNodeFlags(kIOFWMustBeRoot);
					IOLog("Panasonic guid=%lld series=%x model=%s\n", guidVal, series, string->getCStringNoCopy()); // node flags happens here
				}
				else
				{
					FIRELOG_MSG(( "Unknown Panasonic series\n" ));
				}
			}
		}
	}
	
	prop = provider->getProperty(gFireWireProduct_Name);
    if(prop)
        setProperty(gFireWireProduct_Name, prop);
    
    setProperty("Unit_Type", type, 32);
    
	// mark ourselves as started, this allows us to service resumed messages
	// resumed messages after this point should be safe.
	fStarted = true;
	
    updateSubUnits(true);
    
    // Finally enable matching on this object.
    registerService();

    return true;
}

bool IOFireWireAVCUnit::available()
{
	return fStarted;
}

//////////////////////////////////////////////////////
// IOFireWireAVCUnit::free
//////////////////////////////////////////////////////
void IOFireWireAVCUnit::free(void)
{
	// Local Vars
	IOFireWireAVCAsynchronousCommand *pCmd;
	
	FIRELOG_MSG(("IOFireWireAVCUnit::free (this=0x%08X)\n",this));

	if ((fIOFireWireAVCUnitExpansion) && (fIOFireWireAVCUnitExpansion->fControl))
	{
		lockAVCAsynchronousCommandLock();
		
		fStarted = false;
		
		unlockAVCAsynchronousCommandLock();
	}

    if (fFCPResponseSpace) {
        fFCPResponseSpace->deactivate();
        fFCPResponseSpace->release();
		fFCPResponseSpace = NULL;
    }
    if (avcLock) {
        IOLockFree(avcLock);
		avcLock = NULL;
    }

	if ((fIOFireWireAVCUnitExpansion) && (fIOFireWireAVCUnitExpansion->fControl) && (fIOFireWireAVCUnitExpansion->fAVCAsyncCommands))
	{
		// Get the unit's async command lock
		lockAVCAsynchronousCommandLock();
		
		// Cancel any remaining pending AVC async commands in the AVC command array
		while (fIOFireWireAVCUnitExpansion->fAVCAsyncCommands->getCount())
		{
			pCmd = (IOFireWireAVCAsynchronousCommand*) fIOFireWireAVCUnitExpansion->fAVCAsyncCommands->getObject(0);
			pCmd->fWriteNodeID = kIOFWAVCAsyncCmdFreed;	// Special flag to indicate this command was cancled in this unit's free routine.
			pCmd->cancel();
		}
		
		// Free the async command lock
		unlockAVCAsynchronousCommandLock();
		
		// Release the async AVC command array
		fIOFireWireAVCUnitExpansion->fAVCAsyncCommands->release();
	}
	
    if (cmdLock)
	{
        IOLockFree(cmdLock);
		cmdLock = NULL;
    }
	
	// Release our provider, the IOFireWireUnit object
	if( fDevice )
	{
		fDevice->release();
		fDevice = NULL;
	}

	// free expansion data
	if (fIOFireWireAVCUnitExpansion)
	{
		IOFree ( fIOFireWireAVCUnitExpansion, sizeof(ExpansionData) );
		fIOFireWireAVCUnitExpansion = NULL;
	}
	
    IOService::free();
}

//////////////////////////////////////////////////////
// IOFireWireAVCUnit::matchPropertyTable
//////////////////////////////////////////////////////
bool IOFireWireAVCUnit::matchPropertyTable(OSDictionary * table)
{
	//FIRELOG_MSG(("IOFireWireAVCUnit::matchPropertyTable (this=0x%08X)\n",this));

    //
    // If the service object wishes to compare some of its properties in its
    // property table against the supplied matching dictionary,
    // it should do so in this method and return truth on success.
    //
    if (!IOService::matchPropertyTable(table))  return false;

    // We return success if the following expression is true -- individual
    // comparisions evaluate to truth if the named property is not present
    // in the supplied matching dictionary.
    

    bool res = compareProperty(table, gIOFireWireAVCUnitType) &&
        compareProperty(table, gFireWireVendor_ID) &&
        compareProperty(table, gFireWire_GUID);
        
    if(res) {
        // Also see if requested subunits are available.
        int i;
        //OLog("Checking subunit foo\n");
        for(i=0; i<kAVCNumSubUnitTypes; i++) {
            OSNumber *	value;
            value = OSDynamicCast(OSNumber, table->getObject( gIOFireWireAVCSubUnitCount[i] ));
            if(value) {
                // make sure we have at least the requested number of subunits of the requested type
                //IOLog("Want %d AVCSubUnit_%x, got %d\n", value->unsigned8BitValue(), i, fSubUnitCount[i]);
                res = value->unsigned8BitValue() <= fSubUnitCount[i];
                if(!res)
                    break;
            }
        }
        //IOLog("After Checking subunit foo, match is %d\n", res);
    }
    return res;
}

//////////////////////////////////////////////////////
// IOFireWireAVCUnit::AVCCommand
//////////////////////////////////////////////////////
IOReturn IOFireWireAVCUnit::AVCCommand(const UInt8 * in, UInt32 len, UInt8 * out, UInt32 *size)
{
	FIRELOG_MSG(("IOFireWireAVCUnit::AVCCommand (this=0x%08X, opCode=0x%02X)\n",this,in[2]));

    IOReturn res;
    IOFireWireAVCCommand *cmd;
    if(len == 0 || len > 512) {
        IOLog("Loopy AVCCmd, len %d, respLen %d\n", (uint32_t)len, (uint32_t)*size);
        return kIOReturnBadArgument;
    }

	// Retain the AVCUnit object while processing the command
	this->retain();
	
    cmd = IOFireWireAVCCommand::withNub(fDevice, in, len, out, size);
    if(!cmd)
	{
		// Remove the extra retain we made above.
		this->release();
		
		return kIOReturnNoMemory;
	}
	
    // lock avc space
    IOTakeLock(avcLock);

    fCommand = cmd;
    
    res = fCommand->submit();
    if(res != kIOReturnSuccess) {
        //IOLog("AVCCommand returning 0x%x\n", res);
        //IOLog("command %x\n", *(UInt32 *)in);
    }
    IOTakeLock(cmdLock);
    fCommand = NULL;
    IOUnlock(cmdLock);
    cmd->release();
    IOUnlock(avcLock);

	// Remove the extra retain we made above.
	this->release();

    return res;
}

//////////////////////////////////////////////////////
// IOFireWireAVCUnit::AVCCommandInGeneration
//////////////////////////////////////////////////////
IOReturn IOFireWireAVCUnit::AVCCommandInGeneration(UInt32 generation, const UInt8 * in, UInt32 len, UInt8 * out, UInt32 *size)
{
	FIRELOG_MSG(("IOFireWireAVCUnit::AVCCommandInGeneration (this=0x%08X)\n",this));

    IOReturn res;
    IOFireWireAVCCommand *cmd;
    if(len == 0 || len > 512) {
        IOLog("Loopy AVCCmd, len %d, respLen %d\n", (uint32_t)len, (uint32_t)*size);
        return kIOReturnBadArgument;
    }

    cmd = IOFireWireAVCCommand::withNub(fDevice, generation, in, len, out, size);
    if(!cmd)
        return kIOReturnNoMemory;

    // lock avc space
    IOTakeLock(avcLock);
    fCommand = cmd;
    
    res = fCommand->submit();
    if(res != kIOReturnSuccess) {
        //IOLog("AVCCommand returning 0x%x\n", res);
        //IOLog("command %x\n", *(UInt32 *)in);
    }
    IOTakeLock(cmdLock);
    fCommand = NULL;
    IOUnlock(cmdLock);
    cmd->release();
    IOUnlock(avcLock);

    return res;
}

//////////////////////////////////////////////////////
// IOFireWireAVCUnit::AVCAsynchRequestWriteDone
//////////////////////////////////////////////////////
void IOFireWireAVCUnit::AVCAsynchRequestWriteDone(void *refcon, IOReturn status, IOFireWireNub *device, IOFWCommand *fwCmd)
{
	IOFireWireAVCAsynchronousCommand *pCmdObject = OSDynamicCast(IOFireWireAVCAsynchronousCommand, (IOFireWireAVCAsynchronousCommand*)refcon);

	if(!pCmdObject)
		return;
		
	IOFireWireAVCUnit *pAVCUnit = OSDynamicCast(IOFireWireAVCUnit, pCmdObject->fAVCUnit);

	if(!pAVCUnit)
		return;

	UInt32 cmdIndex;
	bool doCallback = false;

	FIRELOG_MSG(("IOFireWireAVCUnit::AVCAsynchRequestWriteDone (cmd=0x%08X, status=0x%08X)\n",pCmdObject,status));

	// Get the async command lock
	pAVCUnit->lockAVCAsynchronousCommandLock();

	// If this is due to a cancel, don't process further
	if(status == kIOReturnAborted)
	{
		pAVCUnit->unlockAVCAsynchronousCommandLock();
		return;
	}
	
	// Verify the async command object is still on our list of pending commands
	cmdIndex = pAVCUnit->indexOfAVCAsynchronousCommandObject(pCmdObject);
	if (cmdIndex == 0xFFFFFFFF)
	{
		// The AVC async command must have already been terminated. Free the lock, and return.
		pAVCUnit->unlockAVCAsynchronousCommandLock();
		return;
	}
	
	if(status == kIOReturnSuccess)
	{
        // Store current node and generation
        if(device)
            device->getNodeIDGeneration(pCmdObject->fWriteGen, pCmdObject->fWriteNodeID);

		// Start the delay
		pCmdObject->fDelayCmd->submit();
		
		// Change the state of this command
		pCmdObject->cmdState = kAVCAsyncCommandStateWaitingForResponse;
    }
    else
	{
		// Change the state of this command
		pCmdObject->cmdState = kAVCAsyncCommandStateRequestFailed;

		// We need to do a callback after we release the lock
		doCallback = true;

		// Remove this command from the unit's pending async command list
		pAVCUnit->removeAVCAsynchronousCommandObjectAtIndex(cmdIndex);
	}
	
	// Free the async command lock
	pAVCUnit->unlockAVCAsynchronousCommandLock();

	// Se if we need to do a callback to the client
	if (doCallback == true)
	{
		// Notify the client
		if (pCmdObject->fCallback != NULL)
			pCmdObject->fCallback(pCmdObject->pRefCon,pCmdObject);
	}
}

//////////////////////////////////////////////////////
// IOFireWireAVCUnit::AVCAsynchDelayDone
//////////////////////////////////////////////////////
void IOFireWireAVCUnit::AVCAsynchDelayDone(void *refcon, IOReturn status, IOFireWireBus *bus, IOFWBusCommand *fwCmd)
{
	IOFireWireAVCAsynchronousCommand *pCmdObject = OSDynamicCast( IOFireWireAVCAsynchronousCommand, (IOFireWireAVCAsynchronousCommand*)refcon );

	if(!pCmdObject)
		return;
		
	IOFireWireAVCUnit *pAVCUnit = OSDynamicCast(IOFireWireAVCUnit, (IOFireWireAVCUnit*)pCmdObject->fAVCUnit);

	if(!pAVCUnit)
		return;
	
	UInt32 cmdIndex;

	FIRELOG_MSG(("IOFireWireAVCUnit::AVCAsynchDelayDone, cmd=0x%08X, status = 0x%08X\n",pCmdObject,status));

	// only proceed if status is time-out!
	if (status != kIOReturnTimeout)
		return;

	// Get the async command lock
	pAVCUnit->lockAVCAsynchronousCommandLock();

	// Verify the async command object is still on our list of pending commands
	cmdIndex = pAVCUnit->indexOfAVCAsynchronousCommandObject(pCmdObject);
	if (cmdIndex == 0xFFFFFFFF)
	{
		// The AVC async command must have already been terminated. Free the lock, and return.
		pAVCUnit->unlockAVCAsynchronousCommandLock();
		return;
	}

	// Change the state of this command
	pCmdObject->cmdState = kAVCAsyncCommandStateTimeOutBeforeResponse;

	// Remove this command from the unit's pending async command list
	pAVCUnit->removeAVCAsynchronousCommandObjectAtIndex(cmdIndex);
	
	// Free the async command lock
	pAVCUnit->unlockAVCAsynchronousCommandLock();

	// Notify the client
	if (pCmdObject->fCallback != NULL)
		pCmdObject->fCallback(pCmdObject->pRefCon,pCmdObject);
}

//////////////////////////////////////////////////////
// IOFireWireAVCUnit::indexOfAVCAsynchronousCommandObject
//////////////////////////////////////////////////////
UInt32 IOFireWireAVCUnit::indexOfAVCAsynchronousCommandObject(IOFireWireAVCAsynchronousCommand *pCommandObject)
{
	UInt32 res = 0xFFFFFFFF;
	int i;

	// NOTE: Assume that the AVCAsynchronousCommandLock has already
	// been taken before this function was called!

	for (i=(fIOFireWireAVCUnitExpansion->fAVCAsyncCommands->getCount()-1);i>=0;i--)
	{
		IOFireWireAVCAsynchronousCommand *pCmd;
		pCmd = (IOFireWireAVCAsynchronousCommand*) fIOFireWireAVCUnitExpansion->fAVCAsyncCommands->getObject(i);
        if(pCommandObject == pCmd)
		{
			res = i;
			break;
		}
	}
	
	return res;
}

//////////////////////////////////////////////////////
// IOFireWireAVCUnit::removeAVCAsynchronousCommandObjectAtIndex
//////////////////////////////////////////////////////
void IOFireWireAVCUnit::removeAVCAsynchronousCommandObjectAtIndex(UInt32 index)
{
	// NOTE: Assume that the AVCAsynchronousCommandLock has already
	// been taken before this function was called!

	fIOFireWireAVCUnitExpansion->fAVCAsyncCommands->removeObject(index);
}

//////////////////////////////////////////////////////
// IOFireWireAVCUnit::lockAVCAsynchronousCommandLock
//////////////////////////////////////////////////////
void IOFireWireAVCUnit::lockAVCAsynchronousCommandLock()
{
	FIRELOG_MSG(("IOFireWireAVCUnit::lockAVCAsynchronousCommandLock (this=0x%08X)\n",this));
	fIOFireWireAVCUnitExpansion->fControl->closeGate();
}

//////////////////////////////////////////////////////
// IOFireWireAVCUnit::unlockAVCAsynchronousCommandLock
//////////////////////////////////////////////////////
void IOFireWireAVCUnit::unlockAVCAsynchronousCommandLock()
{
	FIRELOG_MSG(("IOFireWireAVCUnit::unlockAVCAsynchronousCommandLock (this=0x%08X)\n",this));
	fIOFireWireAVCUnitExpansion->fControl->openGate();
}

//////////////////////////////////////////////////////
// IOFireWireAVCUnit::handleOpen
//////////////////////////////////////////////////////
bool IOFireWireAVCUnit::handleOpen( IOService * forClient, IOOptionBits options, void * arg )
{
	FIRELOG_MSG(("IOFireWireAVCUnit::handleOpen (this=0x%08X)\n",this));

	bool ok = false;
	
	if( !isOpen() )
	{
		ok = fDevice->open(this, options, arg);
		if(ok)
			ok = IOService::handleOpen(forClient, options, arg);
	}
	
	return ok;
}

//////////////////////////////////////////////////////
// IOFireWireAVCUnit::handleClose
//////////////////////////////////////////////////////
void IOFireWireAVCUnit::handleClose( IOService * forClient, IOOptionBits options )
{
	FIRELOG_MSG(("IOFireWireAVCUnit::handleClose (this=0x%08X)\n",this));

	if( isOpen( forClient ) )
	{
		IOService::handleClose(forClient, options);
		fDevice->close(this, options);
	}
}

//////////////////////////////////////////////////////
// IOFireWireAVCUnit::message
//////////////////////////////////////////////////////
IOReturn IOFireWireAVCUnit::message(UInt32 type, IOService *provider, void *argument)
{
	// Local Vars
	UInt32 i;
	IOFireWireAVCAsynchronousCommand *pCmd;
	OSArray *pTerminatedCommandsArray = NULL;
	
	FIRELOG_MSG(("IOFireWireAVCUnit::message (type = 0x%08X, this=0x%08X)\n",type,this));

	// If we have outstanding Async AVC commands, process them here for bus-reset command termination.
	if( fStarted == true && 
		type == kIOMessageServiceIsSuspended && 
		(fIOFireWireAVCUnitExpansion->fAVCAsyncCommands->getCount() > 0))
	{
		// Get the unit's async command lock
		lockAVCAsynchronousCommandLock();
		
		for (i = 0; i < fIOFireWireAVCUnitExpansion->fAVCAsyncCommands->getCount(); i++)
		{
			pCmd = (IOFireWireAVCAsynchronousCommand*) fIOFireWireAVCUnitExpansion->fAVCAsyncCommands->getObject(i);
			
			// If the write command has been submitted, but no write done yet, cancel it now
			if (pCmd->cmdState == kAVCAsyncCommandStateRequestSent)
				pCmd->fWriteCmd->cancel(kIOReturnAborted);
			
			// If the delay command has been submitted, but not completed, cancel it now
			if (pCmd->cmdState == kAVCAsyncCommandStateWaitingForResponse)
				pCmd->fDelayCmd->cancel(kIOReturnAborted);
			
			FIRELOG_MSG(("IOFireWireAVCUnit::message setting pending async AVC command (0x%08X) to bus-reset state\n",pCmd));
			pCmd->cmdState = kAVCAsyncCommandStateBusReset;
			
			// Remove this command from the unit's pending async command list
			removeAVCAsynchronousCommandObjectAtIndex(i);
			
			// Add this command to the array of commands which we need to do client callbacks for
			// Note - this will add an extra retain to the command, which will be released when the array is released
			if (pTerminatedCommandsArray == NULL)
				pTerminatedCommandsArray = OSArray::withCapacity(1);
			if (pTerminatedCommandsArray != NULL)
				pTerminatedCommandsArray->setObject(pCmd);
		}

		// Free the async command lock
		unlockAVCAsynchronousCommandLock();
		
		// Do we have any terminated commands which we should do client callbacks for?
		if (pTerminatedCommandsArray != NULL)
		{
			for (i = 0; i < pTerminatedCommandsArray->getCount(); i++)
			{
				pCmd = (IOFireWireAVCAsynchronousCommand*) pTerminatedCommandsArray->getObject(i);
				
				// Notify the client
				if (pCmd->fCallback != NULL)
					pCmd->fCallback(pCmd->pRefCon,pCmd);			
			}
			
			// Release the array - note that this will release all the objects from the array
			// to remove the extra retain that was done when the command was added to the array
			pTerminatedCommandsArray->release();
		}
	}
	
	// If this is a bus-reset complete, then rescan subunits on the device
	// on another thread
	if( fStarted == true && type == kIOMessageServiceIsResumed )
	{
		this->retain(); // Retain this object before starting the rescan thread!
		thread_t		thread;
		if( kernel_thread_start((thread_continue_t)rescanSubUnits, this, &thread ) == KERN_SUCCESS )
		{
			thread_deallocate(thread);
		}
    }
    messageClients(type);
    
    return kIOReturnSuccess;
}

//////////////////////////////////////////////////////
// IOFireWireAVCUnit::updateAVCCommandTimeout
//////////////////////////////////////////////////////
IOReturn IOFireWireAVCUnit::updateAVCCommandTimeout()
{
	FIRELOG_MSG(("IOFireWireAVCUnit::updateAVCCommandTimeout (this=0x%08X)\n",this));

    IOTakeLock(cmdLock);
    if(fCommand != NULL)
        fCommand->resetInterimTimeout();
    IOUnlock(cmdLock);

    return kIOReturnSuccess;    
}

/* -------------------------------------------- AVC SubUnit -------------------------------------------- */

OSDefineMetaClassAndStructors(IOFireWireAVCSubUnit, IOFireWireAVCNub)
OSMetaClassDefineReservedUnused(IOFireWireAVCSubUnit, 0);
OSMetaClassDefineReservedUnused(IOFireWireAVCSubUnit, 1);
OSMetaClassDefineReservedUnused(IOFireWireAVCSubUnit, 2);
OSMetaClassDefineReservedUnused(IOFireWireAVCSubUnit, 3);

//////////////////////////////////////////////////////
// IOFireWireAVCSubUnit::init
//////////////////////////////////////////////////////
bool IOFireWireAVCSubUnit::init(OSDictionary *propTable, IOFireWireAVCUnit *provider)
{
	FIRELOG_MSG(("IOFireWireAVCSubUnit::init (this=0x%08X)\n",this));
	
    OSObject *prop;

    if(!IOFireWireAVCNub::init(propTable))
        return false;
    fAVCUnit = provider;
    if(!fAVCUnit)
        return false;
    fDevice = fAVCUnit->getDevice();
    if(!fDevice)
        return false;
    
    // Copy over matching properties from AVC Unit
    prop = provider->getProperty(gFireWireVendor_ID);
    if(prop)
        setProperty(gFireWireVendor_ID, prop);
    prop = provider->getProperty(gFireWire_GUID);
    if(prop)
        setProperty(gFireWire_GUID, prop);
    prop = provider->getProperty(gFireWireProduct_Name);
    if(prop)
        setProperty(gFireWireProduct_Name, prop);

    // Copy over user client properties
    prop = provider->getProperty(gIOUserClientClassKey);
    if(prop)
        setProperty(gIOUserClientClassKey, prop);
    prop = provider->getProperty(kIOCFPlugInTypesKey);
    if(prop)
        setProperty(kIOCFPlugInTypesKey, prop);
    
    return true;
}

/**
 ** Matching methods
 **/
//////////////////////////////////////////////////////
// IOFireWireAVCSubUnit::matchPropertyTable
//////////////////////////////////////////////////////
bool IOFireWireAVCSubUnit::matchPropertyTable(OSDictionary * table)
{
	//FIRELOG_MSG(("IOFireWireAVCSubUnit::matchPropertyTable (this=0x%08X)\n",this));

    //
    // If the service object wishes to compare some of its properties in its
    // property table against the supplied matching dictionary,
    // it should do so in this method and return truth on success.
    //
    if (!IOService::matchPropertyTable(table))  return false;

    // We return success if the following expression is true -- individual
    // comparisions evaluate to truth if the named property is not present
    // in the supplied matching dictionary.
    

    return compareProperty(table, gIOFireWireAVCSubUnitType) &&
        compareProperty(table, gFireWireVendor_ID) &&
        compareProperty(table, gFireWire_GUID);
}

//////////////////////////////////////////////////////
// IOFireWireAVCSubUnit::AVCCommand
//////////////////////////////////////////////////////
IOReturn IOFireWireAVCSubUnit::AVCCommand(const UInt8 * in, UInt32 len, UInt8 * out, UInt32 *size)
{
	FIRELOG_MSG(("IOFireWireAVCSubUnit::AVCCommand (this=0x%08X)\n",this));

    return fAVCUnit->AVCCommand(in, len, out, size);
}

//////////////////////////////////////////////////////
// IOFireWireAVCSubUnit::AVCCommandInGeneration
//////////////////////////////////////////////////////
IOReturn IOFireWireAVCSubUnit::AVCCommandInGeneration(UInt32 generation, const UInt8 * in, UInt32 len, UInt8 * out, UInt32 *size)
{
	FIRELOG_MSG(("IOFireWireAVCSubUnit::AVCCommandInGeneration (this=0x%08X)\n",this));

    return fAVCUnit->AVCCommandInGeneration(generation, in, len, out, size);
}

//////////////////////////////////////////////////////
// IOFireWireAVCSubUnit::updateAVCCommandTimeout
//////////////////////////////////////////////////////
IOReturn IOFireWireAVCSubUnit::updateAVCCommandTimeout()
{
	FIRELOG_MSG(("IOFireWireAVCSubUnit::updateAVCCommandTimeout (this=0x%08X)\n",this));

    return fAVCUnit->updateAVCCommandTimeout();
}

//////////////////////////////////////////////////////
// IOFireWireAVCSubUnit::handleOpen
//////////////////////////////////////////////////////
bool IOFireWireAVCSubUnit::handleOpen( IOService * forClient, IOOptionBits options, void * arg )
{
	FIRELOG_MSG(("IOFireWireAVCSubUnit::handleOpen (this=0x%08X)\n",this));
	
	bool ok = false;
	
	if( !isOpen() )
	{
		ok = fAVCUnit->open(this, options, arg);
		if(ok)
			ok = IOService::handleOpen(forClient, options, arg);
	}
	
	return ok;
}

//////////////////////////////////////////////////////
// IOFireWireAVCSubUnit::handleClose
//////////////////////////////////////////////////////
void IOFireWireAVCSubUnit::handleClose( IOService * forClient, IOOptionBits options )
{
	FIRELOG_MSG(("IOFireWireAVCSubUnit::handleClose (this=0x%08X)\n",this));

	if( isOpen( forClient ) )
	{
		IOService::handleClose(forClient, options);
		fAVCUnit->close(this, options);
	}
}

//////////////////////////////////////////////////////
// IOFireWireAVCSubUnit::message
//////////////////////////////////////////////////////
IOReturn IOFireWireAVCSubUnit::message(UInt32 type, IOService *provider, void *argument)
{
	//FIRELOG_MSG(("IOFireWireAVCSubUnit::message (this=0x%08X)\n",this));

    messageClients(type);
    
    return kIOReturnSuccess;
}

