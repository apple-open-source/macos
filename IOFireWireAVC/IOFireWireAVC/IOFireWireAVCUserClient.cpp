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

// This is for testing of the new async AVC command kernel stuff
//#define kUseAsyncAVCCommandForBlockingAVCCommand 1

#include "IOFireWireAVCUserClient.h"
#include <IOKit/IOLib.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOSyncer.h>
#include <IOKit/firewire/IOFireWireUnit.h>
#include <IOKit/firewire/IOFireWireController.h>

#if FIRELOG
#import <IOKit/firewire/FireLog.h>
#define FIRELOG_MSG(x) FireLog x
#else
#define FIRELOG_MSG(x) do {} while (0)
#endif

OSDefineMetaClassAndStructors(IOFireWireAVCConnection, OSObject)
OSDefineMetaClassAndStructors(IOFireWireAVCUserClientAsyncCommand, OSObject)
OSDefineMetaClassAndStructors(IOFireWireAVCUserClient, IOUserClient)

void AVCUserClientAsyncCommandCallback(void *pRefCon, IOFireWireAVCAsynchronousCommand *pCommandObject);

//////////////////////////////////////////////////////
// IOFireWireAVCUserClient::externalMethod
//////////////////////////////////////////////////////
IOReturn IOFireWireAVCUserClient::externalMethod( uint32_t selector, 
						IOExternalMethodArguments * arguments,
						IOExternalMethodDispatch * dispatch, 
						OSObject * target, 
						void * reference)
{
	IOReturn result = kIOReturnBadArgument;
	
	FIRELOG_MSG(("IOFireWireAVCUserClient::externalMethod (this=0x%08X), selector=0x%08X\n",this,selector));
	
	// Dispatch the method call
	switch (selector)
	{
	    case kIOFWAVCUserClientOpen:
			result = open(NULL,NULL,NULL,NULL,NULL,NULL);
			break;
		
		case kIOFWAVCUserClientClose:
			result = close(NULL,NULL,NULL,NULL,NULL,NULL);
			break;

		case kIOFWAVCUserClientGetSessionRef:
			result = getSessionRef( arguments->scalarOutput,NULL,NULL,NULL,NULL,NULL);
			break;

		case kIOFWAVCUserClientAVCCommand:
			result = AVCCommand((UInt8*)arguments->structureInput, 
								(UInt8*)arguments->structureOutput, 
								arguments->structureInputSize, 
								(UInt32*)&arguments->structureOutputSize);
			break;
		
		case kIOFWAVCUserClientOpenWithSessionRef:
			result = openWithSessionRef((IOFireWireLib::UserObjectHandle) arguments->scalarInput[0],NULL,NULL,NULL,NULL,NULL);
			break;
		
		case kIOFWAVCUserClientAVCCommandInGen:
			result = AVCCommandInGen((UInt8*) arguments->structureInput,
								(UInt8*)arguments->structureOutput, 
								arguments->structureInputSize, 
								(UInt32*)&arguments->structureOutputSize);
			break;
		
		case kIOFWAVCUserClientUpdateAVCCommandTimeout:
			result = updateAVCCommandTimeout(NULL,NULL,NULL,NULL,NULL,NULL);
			break;
		
		case kIOFWAVCUserClientMakeP2PInputConnection:
			result = makeP2PInputConnection(arguments->scalarInput[0],arguments->scalarInput[1],NULL,NULL,NULL,NULL);
			break;
		
		case kIOFWAVCUserClientBreakP2PInputConnection:
			result = breakP2PInputConnection(arguments->scalarInput[0],NULL,NULL,NULL,NULL,NULL);
			break;
		
		case kIOFWAVCUserClientMakeP2POutputConnection:
			result = makeP2POutputConnection(arguments->scalarInput[0],arguments->scalarInput[1],(IOFWSpeed)arguments->scalarInput[2],NULL,NULL,NULL);
			break;
		
		case kIOFWAVCUserClientBreakP2POutputConnection:
			result = breakP2POutputConnection(arguments->scalarInput[0],NULL,NULL,NULL,NULL,NULL);
			break;
		
		case kIOFWAVCUserClientCreateAsyncAVCCommand:
			result = CreateAVCAsyncCommand((UInt8*)arguments->structureInput, 
								(UInt8*)arguments->structureOutput, 
								arguments->structureInputSize, 
								(UInt32*)&arguments->structureOutputSize);
			break; 
		
		case kIOFWAVCUserClientSubmitAsyncAVCCommand:
			result = SubmitAVCAsyncCommand(arguments->scalarInput[0]);
			break;
		
		case kIOFWAVCUserClientCancelAsyncAVCCommand:
			result = CancelAVCAsyncCommand(arguments->scalarInput[0]);
			break;
		
		case kIOFWAVCUserClientReleaseAsyncAVCCommand:
			result = ReleaseAVCAsyncCommand(arguments->scalarInput[0]);
			break;
		
		case kIOFWAVCUserClientReinitAsyncAVCCommand:
			result = ReinitAVCAsyncCommand(arguments->scalarInput[0], (const UInt8*) arguments->structureInput, arguments->structureInputSize); 
			break;
		
		case kIOFWAVCUserClientInstallAsyncAVCCommandCallback:
			result = installUserLibAsyncAVCCommandCallback(arguments->asyncReference,arguments->scalarInput[0], arguments->scalarOutput);
			break;
		
		default:
			// None of the above!
			break;
	};

	return result;
}

//////////////////////////////////////////////////////
// IOFireWireAVCUserClient::initWithTask
//////////////////////////////////////////////////////
bool IOFireWireAVCUserClient::initWithTask(
				  task_t owningTask, void * securityToken, UInt32 type,
				  OSDictionary * properties)
{
	FIRELOG_MSG(("IOFireWireAVCUserClient::initWithTask (this=0x%08X)\n",this));
	
	fTask = owningTask;
	
	// Allow Rosetta based apps access to this user-client
	if (properties)
		properties->setObject("IOUserClientCrossEndianCompatible", kOSBooleanTrue);
	
	return IOUserClient::initWithTask(owningTask, securityToken, type,properties);
}

//////////////////////////////////////////////////////
// IOFireWireAVCUserClient::free
//////////////////////////////////////////////////////
void IOFireWireAVCUserClient::free()
{
    FIRELOG_MSG(("IOFireWireAVCUserClient::free (this=0x%08X)\n",this));
	
    if(fConnections) {
        UInt32 i;
        
        if(fUnit) {
            for(i=0; i<fConnections->getCount(); i++) {
                IOFireWireAVCConnection *connection;
                connection = (IOFireWireAVCConnection *)fConnections->getObject(i);
                //IOLog("Cleaning up connection %d %p\n", i, connection);
                updateP2PCount(connection->fPlugAddr, -1, false, 0xFFFFFFFF, kFWSpeedInvalid);
            }
		}
        fConnections->release();
	}

	// Free the async cmd lock
	if (fAsyncAVCCmdLock)
		IOLockFree(fAsyncAVCCmdLock);
	
#ifdef kUseAsyncAVCCommandForBlockingAVCCommand
    if (avcCmdLock)
	{
        if (IOLockTryLock(avcCmdLock) == false)
		{
			// The avcCmdLock is currently locked, meaning we are in the process of 
			// running IOFireWireAVCUserClient::AVCCommand on another thread. Cancel
			// that command now.
			if (pCommandObject)
				pCommandObject->cancel();
			IOTakeLock(avcCmdLock);
		}
		IOLockFree(avcCmdLock);
    }
#endif	
    
	IOService::free();

	// Release our retain on the IOFireWireAVCUnit!
	if (fUnit)
		fUnit->release();
}

//////////////////////////////////////////////////////
// IOFireWireAVCUserClient::stop
//////////////////////////////////////////////////////
void IOFireWireAVCUserClient::stop( IOService * provider )
{
	IOFireWireAVCUserClientAsyncCommand *pUCAsyncCommand;

    FIRELOG_MSG(("IOFireWireAVCUserClient::stop (this=0x%08X)\n",this));
    
    fStarted = false;
    
	// Deal with the fUCAsyncCommands array, deal with any outstanding commands and release it
	IOTakeLock(fAsyncAVCCmdLock);
	while (fUCAsyncCommands->getCount())
	{
		pUCAsyncCommand = (IOFireWireAVCUserClientAsyncCommand *)fUCAsyncCommands->getObject(0);
		if (pUCAsyncCommand)
		{
			pUCAsyncCommand->pAsyncCommand->cancel();	// Cancel, just in case it's still pending!
			pUCAsyncCommand->pAsyncCommand->release();
			
			// Get rid of the memory descriptor for the shared buf
			pUCAsyncCommand->fMem->complete();
			pUCAsyncCommand->fMem->release() ;
			
			// Remove this object from our array. This will release it.
			fUCAsyncCommands->removeObject(0);
		}
	}
	
	IOUnlock(fAsyncAVCCmdLock);
	
    IOService::stop(provider);
}


//////////////////////////////////////////////////////
// IOFireWireAVCUserClient::start
//////////////////////////////////////////////////////
bool IOFireWireAVCUserClient::start( IOService * provider )
{
    FIRELOG_MSG(("IOFireWireAVCUserClient::start (this=0x%08X)\n",this));
    
	if( fStarted )
		return false;
		
    fUnit = OSDynamicCast(IOFireWireAVCNub, provider);
    if (fUnit == NULL)
        return false;

	fConnections = OSArray::withCapacity(1);

	// Create array to hold outstanding async AVC commands for this UC, and a lock for protecting it
	fUCAsyncCommands = OSArray::withCapacity(1);
	fAsyncAVCCmdLock = IOLockAlloc();
    if (fAsyncAVCCmdLock == NULL)
        return false;
  	
#ifdef kUseAsyncAVCCommandForBlockingAVCCommand
	pCommandObject = NULL;
	avcCmdLock = IOLockAlloc();
    if (avcCmdLock == NULL) {
        return false;
    }
#endif
	
	if( !IOUserClient::start(provider) )
        return false;
	
    fStarted = true;

	// Retain the IOFireWireAVCUnit to prevent it from being terminated
	fUnit->retain();
	
     return true;
}

//////////////////////////////////////////////////////
// IOFireWireAVCUserClient::clientClose
//////////////////////////////////////////////////////
IOReturn IOFireWireAVCUserClient::clientClose( void )
{
    FIRELOG_MSG(("IOFireWireAVCUserClient::clientClose (this=0x%08X)\n",this));

    if( fOpened )
    {
        fOpened = false;
    }
    
	fStarted = false;
	
	terminate( kIOServiceRequired );
	
	return kIOReturnSuccess;
}

//////////////////////////////////////////////////////
// IOFireWireAVCUserClient::clientDied
//////////////////////////////////////////////////////
IOReturn IOFireWireAVCUserClient::clientDied( void )
{
    FIRELOG_MSG(("IOFireWireAVCUserClient::clientDied (this=0x%08X)\n",this));

    return clientClose();
}

//////////////////////////////////////////////////////
// IOFireWireAVCUserClient::open
//////////////////////////////////////////////////////
IOReturn IOFireWireAVCUserClient::open
	( void *, void *, void *, void *, void *, void * )
{
	FIRELOG_MSG(("IOFireWireAVCUserClient::open (this=0x%08X)\n",this));

	IOReturn status = kIOReturnSuccess;

    if( fOpened )
        status = kIOReturnError;

    if( status == kIOReturnSuccess )
    {
        if( fUnit->open(this) )
		{
			IOFWUserObjectExporter * exporter = fUnit->getDevice()->getBus()->getSessionRefExporter();
			status = exporter->addObject( this, NULL, &fSessionRef );		
			if( status == kIOReturnSuccess )
			{
				fOpened = true;
			}
			else
			{
				fUnit->close(this);
			}
		}
		else
            status = kIOReturnExclusiveAccess;
    }
    
     return status;
}

//////////////////////////////////////////////////////
// IOFireWireAVCUserClient::openWithSessionRef
//////////////////////////////////////////////////////
IOReturn IOFireWireAVCUserClient::openWithSessionRef( IOFireWireLib::UserObjectHandle sessionRef, void *, void *, void *, void *, void * )
{
	FIRELOG_MSG(("IOFireWireAVCUserClient::openWithSessionRef (this=0x%08X)\n",this));

    IOReturn status = kIOReturnSuccess;
	IOService * service = NULL;
	IOService * original_service = NULL;
	
    if( fOpened || !fUnit->isOpen() )
        status = kIOReturnError;
	
	if( status == kIOReturnSuccess )
	{
		IOFWUserObjectExporter * exporter = fUnit->getDevice()->getBus()->getSessionRefExporter();
		original_service = (IOService*) exporter->lookupObjectForType( sessionRef, OSTypeID(IOService) );
		if( original_service == NULL )
			status = kIOReturnBadArgument;
	}
	
	if( status == kIOReturnSuccess )
	{
		// look for us in provider chain
		service = original_service;
		while( fUnit != service && service != NULL )
			service = service->getProvider();
		
		// were we found	
		if( service == NULL )
			status = kIOReturnBadArgument;
	}
	
	if( original_service )
	{
		original_service->release();
		original_service = NULL;
	}
	
	return status;
}

//////////////////////////////////////////////////////
// IOFireWireAVCUserClient::getSessionRef
//////////////////////////////////////////////////////
IOReturn IOFireWireAVCUserClient::getSessionRef( uint64_t * sessionRef, void *, void *, void *, void *, void * )
{
	FIRELOG_MSG(("IOFireWireAVCUserClient::getSessionRef (this=0x%08X)\n",this));
	
    IOReturn status = kIOReturnSuccess;

    if( !fOpened )
        status = kIOReturnError;

    if( status == kIOReturnSuccess )
    {
		*sessionRef = (uint64_t) fSessionRef;
	}
    
	return status;
}

//////////////////////////////////////////////////////
// IOFireWireAVCUserClient::close
//////////////////////////////////////////////////////
IOReturn IOFireWireAVCUserClient::close
	( void *, void *, void *, void *, void *, void * )
{
	FIRELOG_MSG(("IOFireWireAVCUserClient::close (this=0x%08X)\n",this));
    
    if( fOpened )
    {
		IOFWUserObjectExporter * exporter = fUnit->getDevice()->getBus()->getSessionRefExporter();
		exporter->removeObject( fSessionRef );	
		fSessionRef = 0;

		fUnit->close(this);
        fOpened = false;
	}

    return kIOReturnSuccess;
}

#ifdef kUseAsyncAVCCommandForBlockingAVCCommand
//////////////////////////////////////////////////////
// AVCAsyncCommandCallback
//////////////////////////////////////////////////////
void AVCAsyncCommandCallback(void *pRefCon, IOFireWireAVCAsynchronousCommand *pCommandObject)
{
	IOSyncer *fSyncWakeup = (IOSyncer*) pRefCon;

	FIRELOG_MSG(("IOFireWireAVCUserClient::AVCAsyncCommandCallback (cmd=0x%08X, state=%d)\n",pCommandObject,pCommandObject->cmdState));
	
	// If this command is no longer pending, release the blocking lock
	if (!pCommandObject->isPending())
        fSyncWakeup->signal(pCommandObject->cmdState);
}
#endif

//////////////////////////////////////////////////////
// IOFireWireAVCUserClient::AVCCommand
//////////////////////////////////////////////////////
IOReturn IOFireWireAVCUserClient::AVCCommand(UInt8 * cmd, UInt8 * response,
    UInt32 len, UInt32 *size)
{
	FIRELOG_MSG(("IOFireWireAVCUserClient::AVCCommand (this=0x%08X)\n",this));
	
    IOReturn res;

    if(!fStarted )
	return kIOReturnNoDevice;

#ifndef kUseAsyncAVCCommandForBlockingAVCCommand

    res = fUnit->AVCCommand(cmd,len,response,size);
    return res;

#else

	// Local Vars
    IOSyncer *fSyncWakeup;
	UInt32 responseLen;
	
	IOTakeLock(avcCmdLock);
	
	// TODO: Remove (Just a sanity check. This should never happen!)
	if (pCommandObject)
	{
		FIRELOG_MSG(("IOFireWireAVCUserClient::AVCCommand ERROR: pCommandObject is not NULL!\n"));
		return kIOReturnError;
	}
	
	// Allocate a syncer
	fSyncWakeup = IOSyncer::create();
	if(!fSyncWakeup)
	{
		IOUnlock(avcCmdLock);
        return kIOReturnNoMemory;
	}

	pCommandObject = new IOFireWireAVCAsynchronousCommand;
	if (pCommandObject)
	{
		res = pCommandObject->init(cmd,len,AVCAsyncCommandCallback,fSyncWakeup);
		if (res == kIOReturnSuccess)
		{
			FIRELOG_MSG(("IOFireWireAVCUserClient::AVCCommand createAVCAsynchronousCommand successful, cmd=0x%08X\n",pCommandObject));
			
			// Submit it 
			res = pCommandObject->submit(fUnit);
			if (res == kIOReturnSuccess)
			{
				// Wait for the async command callback to signal us.
				res = fSyncWakeup->wait();
				FIRELOG_MSG(("IOFireWireAVCUserClient::AVCCommand continuing after receiving async command complete notification(this=0x%08X)\n",this));
				
				// Copy the async command final response, if it exists
				if ((pCommandObject->cmdState == kAVCAsyncCommandStateReceivedFinalResponse) && 
					(pCommandObject->pFinalResponseBuf != NULL))
				{
					// Copy as much of the response as will fit into the caller's response buf
					if (pCommandObject->finalResponseLen > *size)
						responseLen = *size;
					else
						responseLen = pCommandObject->finalResponseLen;
					bcopy(pCommandObject->pFinalResponseBuf, response, responseLen);
					*size = responseLen;
					
					// Set the return value to success
					res = kIOReturnSuccess;
				}
				else
				{
					// This is a failure, set the return value correctly
					res = kIOReturnError;   // TODO: further refine error codes based on command state
				}
			}
		}
		else
		{
			// TODO: Since the init failed, we need to manually release the syncer
		}
		
		// Release the command object
		pCommandObject->release();
		pCommandObject = NULL;
	}

	// Release the syncer
	//fSyncWakeup->release();
	
	IOUnlock(avcCmdLock);

	return res;									
#endif
}

//////////////////////////////////////////////////////
// IOFireWireAVCUserClient::AVCCommandInGen
//////////////////////////////////////////////////////
IOReturn IOFireWireAVCUserClient::AVCCommandInGen(UInt8 * cmd, UInt8 * response,
    UInt32 len, UInt32 *size)
{
	FIRELOG_MSG(("IOFireWireAVCUserClient::AVCCommandInGen (this=0x%08X)\n",this));

    IOReturn res;
    UInt32 generation;
    generation = *(UInt32 *)cmd;
    cmd += sizeof(UInt32);
    len -= sizeof(UInt32);
    
    if(!fStarted )
	return kIOReturnNoDevice;

    res = fUnit->AVCCommandInGeneration(generation,cmd,len,response,size);

	return res;
}

//////////////////////////////////////////////////////
// IOFireWireAVCUserClient::updateAVCCommandTimeout
//////////////////////////////////////////////////////
IOReturn IOFireWireAVCUserClient::updateAVCCommandTimeout
	( void *, void *, void *, void *, void *, void * )
{
	FIRELOG_MSG(("IOFireWireAVCUserClient::updateAVCCommandTimeout (this=0x%08X)\n",this));
    
    if(!fStarted )
	return kIOReturnNoDevice;
    
    fUnit->updateAVCCommandTimeout();

    return kIOReturnSuccess;
}

//////////////////////////////////////////////////////
// IOFireWireAVCUserClient::updateP2PCount
//////////////////////////////////////////////////////
IOReturn IOFireWireAVCUserClient::updateP2PCount(UInt32 addr, SInt32 inc, bool failOnBusReset, UInt32 chan, IOFWSpeed speed)
{
	FIRELOG_MSG(("IOFireWireAVCUserClient::updateP2PCount (this=0x%08X)\n",this));
	
    if(!fStarted )
	return kIOReturnNoDevice;

    IOFireWireNub *device = fUnit->getDevice();
    FWAddress plugAddr(kCSRRegisterSpaceBaseAddressHi, addr);
    IOFWReadQuadCommand *readCmd;
    IOFWCompareAndSwapCommand *lockCmd;
    UInt32 plugVal, newVal;
	UInt32 plugValHost, newValHost;
	UInt32 curCount;
	UInt32 curChan;
	IOFWSpeed curSpeed;
    IOReturn res;
    
    readCmd = device->createReadQuadCommand(plugAddr, &plugVal, 1, NULL, NULL, failOnBusReset);
    res = readCmd->submit();
    readCmd->release();
    if(res != kIOReturnSuccess)
        return res;
    
	plugValHost = OSSwapBigToHostInt32( plugVal );
		    
    for(int i=0; i<4; i++) {
        bool success;

		// Parse current plug value
		curCount = ((plugValHost & kIOFWPCRP2PCount) >> 24);
		curChan = ((plugValHost & kIOFWPCRChannel) >> 16);
		curSpeed = (IOFWSpeed)((plugValHost & kIOFWPCROutputDataRate) >> 14);
		newValHost = plugValHost;

		// If requested, modify channel
		if (chan != 0xFFFFFFFF)
		{
			if ((curCount != 0) && (chan != curChan))
				return kIOReturnError;

			newValHost &= ~kIOFWPCRChannel;
			newValHost |= ((chan & 0x3F) << 16);
		}

		// If requested, modify speed
		if (speed != kFWSpeedInvalid)
		{
			if ((curCount != 0) && (speed != curSpeed))
				return kIOReturnError;

			newValHost &= ~kIOFWPCROutputDataRate;
			newValHost |= ((speed & 0x03) << 14);
		}

		// Modify P2P count
		newValHost &= ~kIOFWPCRP2PCount;
		if (inc > 0)
		{
			if (curCount == 0x3F)
				return kIOReturnError;
			newValHost |= ((curCount+1) << 24);
		}
		else
		{
			if (curCount == 0)
				return kIOReturnError;
			newValHost |= ((curCount-1) << 24);
		}
		
		newVal = OSSwapHostToBigInt32( newValHost );
        lockCmd = device->createCompareAndSwapCommand(plugAddr, &plugVal, &newVal, 1);
        res = lockCmd->submit();
        success = lockCmd->locked(&plugVal);
		plugValHost = OSSwapBigToHostInt32( plugVal );
        lockCmd->release();
        if(res != kIOReturnSuccess)
            break;
        if(success)
            break;
    }
    return res;
}

//////////////////////////////////////////////////////
// IOFireWireAVCUserClient::makeConnection
//////////////////////////////////////////////////////
IOReturn IOFireWireAVCUserClient::makeConnection(UInt32 addr, UInt32 chan, IOFWSpeed speed)
{
	FIRELOG_MSG(("IOFireWireAVCUserClient::makeConnection (this=0x%08X)\n",this));
	
    IOReturn err;
    IOFireWireAVCConnection *connection;
    connection = new IOFireWireAVCConnection;
    if(!connection)
        return kIOReturnNoMemory;
        
    err = updateP2PCount(addr, 1, false, chan, speed);
    if(kIOReturnSuccess == err) {
        connection->fPlugAddr = addr;
        connection->fChannel = chan;
        fConnections->setObject(connection);
    }
    connection->release();
    return err;
}

//////////////////////////////////////////////////////
// IOFireWireAVCUserClient::breakConnection
//////////////////////////////////////////////////////
void IOFireWireAVCUserClient::breakConnection(UInt32 addr)
{
	FIRELOG_MSG(("IOFireWireAVCUserClient::breakConnection (this=0x%08X)\n",this));

    UInt32 i;
    
    for(i=0; i<fConnections->getCount(); i++) {
        IOFireWireAVCConnection *connection;
        connection = (IOFireWireAVCConnection *)fConnections->getObject(i);
         if(connection->fPlugAddr == addr) {
            updateP2PCount(addr, -1, false, 0xFFFFFFFF, kFWSpeedInvalid);
            fConnections->removeObject(i);
            break;
        }
    }
}

//////////////////////////////////////////////////////
// IOFireWireAVCUserClient::makeP2PInputConnection
//////////////////////////////////////////////////////
IOReturn IOFireWireAVCUserClient::makeP2PInputConnection( UInt32 plugNo, UInt32 chan, void *, void *, void *, void *)
{
	FIRELOG_MSG(("IOFireWireAVCUserClient::makeP2PInputConnection (this=0x%08X)\n",this));
	
    return makeConnection(kPCRBaseAddress+0x84+4*plugNo, chan, kFWSpeedInvalid);
}

//////////////////////////////////////////////////////
// IOFireWireAVCUserClient::breakP2PInputConnection
//////////////////////////////////////////////////////
IOReturn IOFireWireAVCUserClient::breakP2PInputConnection( UInt32 plugNo, void *, void *, void *, void *, void *)
{
	FIRELOG_MSG(("IOFireWireAVCUserClient::breakP2PInputConnection (this=0x%08X)\n",this));

    breakConnection(kPCRBaseAddress+0x84+4*plugNo);
	return kIOReturnSuccess;
}

//////////////////////////////////////////////////////
// IOFireWireAVCUserClient::makeP2POutputConnection
//////////////////////////////////////////////////////
IOReturn IOFireWireAVCUserClient::makeP2POutputConnection( UInt32 plugNo, UInt32 chan, IOFWSpeed speed, void *, void *, void *)
{
	FIRELOG_MSG(("IOFireWireAVCUserClient::makeP2POutputConnection (this=0x%08X)\n",this));

    return makeConnection(kPCRBaseAddress+4+4*plugNo, chan, speed);
}

//////////////////////////////////////////////////////
// IOFireWireAVCUserClient::breakP2POutputConnection
//////////////////////////////////////////////////////
IOReturn IOFireWireAVCUserClient::breakP2POutputConnection( UInt32 plugNo, void *, void *, void *, void *, void *)
{
	FIRELOG_MSG(("IOFireWireAVCUserClient::breakP2POutputConnection (this=0x%08X)\n",this));

    breakConnection(kPCRBaseAddress+4+4*plugNo);
	return kIOReturnSuccess;
}

//////////////////////////////////////////////////////
// IOFireWireAVCUserClient::message
//////////////////////////////////////////////////////
IOReturn IOFireWireAVCUserClient::message(UInt32 type, IOService *provider, void *argument)
{
	//FIRELOG_MSG(("IOFireWireAVCUserClient::message (this=0x%08X)\n",this));
	
	if( fStarted == true && type == kIOMessageServiceIsResumed ) {
        retain();	// Make sure we don't get deleted with the thread running
		thread_t		thread;
		if( kernel_thread_start((thread_continue_t)remakeConnections, this, &thread ) == KERN_SUCCESS )
		{
			thread_deallocate(thread);
		}
    }
    
    return kIOReturnSuccess;
}

//////////////////////////////////////////////////////
// IOFireWireAVCUserClient::remakeConnections
//////////////////////////////////////////////////////
void IOFireWireAVCUserClient::remakeConnections(void *arg)
{	
    IOFireWireAVCUserClient *me = (IOFireWireAVCUserClient *)arg;

	FIRELOG_MSG(("IOFireWireAVCUserClient::remakeConnections (this=0x%08X)\n",me));

    UInt32 i;
    IOReturn res;
    for(i=0; i<me->fConnections->getCount(); i++) {
        IOFireWireAVCConnection *connection;
        connection = (IOFireWireAVCConnection *)me->fConnections->getObject(i);
        //IOLog("Remaking connection %d %p\n", i, connection);
        res = me->updateP2PCount(connection->fPlugAddr, 1, true, connection->fChannel, kFWSpeedInvalid);
        if(res == kIOFireWireBusReset)
            break;
    }
    
    me->release();
}

//////////////////////////////////////////////////////
// IOFireWireAVCUserClient::installUserLibAsyncAVCCommandCallback
//////////////////////////////////////////////////////
IOReturn IOFireWireAVCUserClient::installUserLibAsyncAVCCommandCallback(io_user_reference_t *asyncRef, uint64_t userRefcon, uint64_t *returnParam)
{
	FIRELOG_MSG(("IOFireWireAVCUserClient::installUserLibAsyncAVCCommandCallback (this=0x%08X, userRefcon=0x%08X)\n",this,userRefcon));

	bcopy(asyncRef,fAsyncAVCCmdCallbackInfo,sizeof(OSAsyncReference64));
	*returnParam = 0x12345678;
	
	return kIOReturnSuccess;
}

//////////////////////////////////////////////////////
// IOFireWireAVCUserClient::CreateAVCAsyncCommand
//////////////////////////////////////////////////////
IOReturn IOFireWireAVCUserClient::CreateAVCAsyncCommand(UInt8 * cmd, UInt8 * asyncAVCCommandHandle, UInt32 len, UInt32 *refSize)
{
	IOReturn res = kIOReturnNoMemory;
	UInt32 *pReturnedCommandHandle = (UInt32*) asyncAVCCommandHandle;
	UInt32 cmdLen = len - sizeof(mach_vm_address_t);
	mach_vm_address_t *ppSharedBufAddress = (mach_vm_address_t*) &cmd[cmdLen];
	IOFireWireAVCUserClientAsyncCommand *pUCAsyncCommand;
	bool memDescPrepared = false;
	
	FIRELOG_MSG(("IOFireWireAVCUserClient::CreateAVCAsyncCommand (this=0x%08X)\n",this));

	if(!fStarted )
		return kIOReturnNoDevice;
	
	do
	{
		// Create the wrapper object for the async command
		pUCAsyncCommand = new IOFireWireAVCUserClientAsyncCommand;
		if (!pUCAsyncCommand)
			break;
		
		// Initialize the wrapper object
		pUCAsyncCommand->pUserClient = this;
		
		// Create the memory descriptor for the user/kernel shared response buffer
		pUCAsyncCommand->fMem = IOMemoryDescriptor::withAddressRange( *ppSharedBufAddress, 1024, kIODirectionInOut, fTask ) ;
		if (!pUCAsyncCommand->fMem)
			break;
		
		// Prepare the memory descriptor
		res = pUCAsyncCommand->fMem->prepare() ;
		if (res != kIOReturnSuccess)
			break;
		else
			memDescPrepared = true;

		// Create the Async command object
		pUCAsyncCommand->pAsyncCommand = new IOFireWireAVCAsynchronousCommand;
		if (!pUCAsyncCommand->pAsyncCommand)
		{
			res = kIOReturnNoMemory;
			break;
		}		

		// Init the async command object
		res = pUCAsyncCommand->pAsyncCommand->init(cmd,cmdLen,AVCUserClientAsyncCommandCallback,pUCAsyncCommand);
		if (res != kIOReturnSuccess)
			break;

	}while(0);
	
	if (res == kIOReturnSuccess)
	{
		// Everything created successfully. Add this to the array of created async commands
		IOTakeLock(fAsyncAVCCmdLock);
		pUCAsyncCommand->commandIdentifierHandle = fNextAVCAsyncCommandHandle++; 
		fUCAsyncCommands->setObject(pUCAsyncCommand);
		IOUnlock(fAsyncAVCCmdLock);

		// Now that it's retained by the array, remove the extra retain count
		pUCAsyncCommand->release();

		// Set the return handle for this new command to the user-side lib
		*pReturnedCommandHandle = pUCAsyncCommand->commandIdentifierHandle;
	}
	else
	{
		// Something went wrong. Cleanup the mess.
		*pReturnedCommandHandle = 0xFFFFFFFF;
		if (pUCAsyncCommand)
		{
			if (pUCAsyncCommand->fMem)
			{
				if (memDescPrepared == true)
					pUCAsyncCommand->fMem->complete();
				pUCAsyncCommand->fMem->release() ;
			}
			
			if (pUCAsyncCommand->pAsyncCommand)
				pUCAsyncCommand->pAsyncCommand->release();
			
			pUCAsyncCommand->release();
		}
	}
	
	return res;
}

//////////////////////////////////////////////////////
// IOFireWireAVCUserClient::SubmitAVCAsyncCommand
//////////////////////////////////////////////////////
IOReturn IOFireWireAVCUserClient::SubmitAVCAsyncCommand(UInt32 commandHandle)
{
	IOReturn res = kIOReturnBadArgument;
	IOFireWireAVCUserClientAsyncCommand *pUCAsyncCommand;
	
	FIRELOG_MSG(("IOFireWireAVCUserClient::SubmitAVCAsyncCommand (this=0x%08X)\n",this));

	pUCAsyncCommand = FindUCAsyncCommandWithHandle(commandHandle);
	
	if (pUCAsyncCommand)
	{
		// Submit it
		res = pUCAsyncCommand->pAsyncCommand->submit(fUnit);
	}
	return res;
}

//////////////////////////////////////////////////////
// IOFireWireAVCUserClient::CancelAVCAsyncCommand
//////////////////////////////////////////////////////
IOReturn IOFireWireAVCUserClient::CancelAVCAsyncCommand(UInt32 commandHandle)
{
	IOReturn res = kIOReturnBadArgument;
	IOFireWireAVCUserClientAsyncCommand *pUCAsyncCommand;
	
	FIRELOG_MSG(("IOFireWireAVCUserClient::CancelAVCAsyncCommand (this=0x%08X)\n",this));
	
	pUCAsyncCommand = FindUCAsyncCommandWithHandle(commandHandle);
	
	if (pUCAsyncCommand)
			res = pUCAsyncCommand->pAsyncCommand->cancel();

	return res;
}

//////////////////////////////////////////////////////
// IOFireWireAVCUserClient::ReleaseAVCAsyncCommand
//////////////////////////////////////////////////////
IOReturn IOFireWireAVCUserClient::ReleaseAVCAsyncCommand(UInt32 commandHandle)
{
	IOReturn res = kIOReturnBadArgument;
	IOFireWireAVCUserClientAsyncCommand *pUCAsyncCommand;
	bool found = false;
	UInt32 i;
	
	FIRELOG_MSG(("IOFireWireAVCUserClient::ReleaseAVCAsyncCommand (this=0x%08X)\n",this));
	
	// Look for an command in our array with the specified command handle
	IOTakeLock(fAsyncAVCCmdLock);
	for(i=0; i<fUCAsyncCommands->getCount(); i++)
	{
		pUCAsyncCommand = (IOFireWireAVCUserClientAsyncCommand *)fUCAsyncCommands->getObject(i);
		if (pUCAsyncCommand->commandIdentifierHandle == commandHandle)
		{
			found = true;
			break;
		}
	}
	
	if (found == true)
	{
		pUCAsyncCommand->pAsyncCommand->cancel();	// Cancel, just in case it's still pending!
		pUCAsyncCommand->pAsyncCommand->release();
		
		// Get rid of the memory descriptor for the shared buf
		pUCAsyncCommand->fMem->complete();
		pUCAsyncCommand->fMem->release() ;
		
		// Remove this object from our array. This will release it.
		fUCAsyncCommands->removeObject(i);
		
		res = kIOReturnSuccess;
	}
		
	IOUnlock(fAsyncAVCCmdLock);
	return res;
}	

//////////////////////////////////////////////////////
// IOFireWireAVCUserClient::ReinitAVCAsyncCommand
//////////////////////////////////////////////////////
IOReturn IOFireWireAVCUserClient::ReinitAVCAsyncCommand(UInt32 commandHandle, const UInt8 *pCommandBytes, UInt32 len)
{
	IOReturn res = kIOReturnBadArgument;
	IOFireWireAVCUserClientAsyncCommand *pUCAsyncCommand;
	
	FIRELOG_MSG(("IOFireWireAVCUserClient::CancelAVCAsyncCommand (this=0x%08X)\n",this));
	
	pUCAsyncCommand = FindUCAsyncCommandWithHandle(commandHandle);
	
	if (pUCAsyncCommand)
		res = pUCAsyncCommand->pAsyncCommand->reinit(pCommandBytes, len);
	
	return res;
}	

//////////////////////////////////////////////////////
// IOFireWireAVCUserClient::FindUCAsyncCommandWithHandle
//////////////////////////////////////////////////////
IOFireWireAVCUserClientAsyncCommand *IOFireWireAVCUserClient::FindUCAsyncCommandWithHandle(UInt32 commandHandle)
{
	IOFireWireAVCUserClientAsyncCommand *pUCAsyncCommand;
	bool found = false;
	UInt32 i;
	
	FIRELOG_MSG(("IOFireWireAVCUserClient::FindUCAsyncCommandWithHandle (this=0x%08X)\n",this));
	
	// Look for an command in our array with the specified command handle
	IOTakeLock(fAsyncAVCCmdLock);
	for(i=0; i<fUCAsyncCommands->getCount(); i++)
	{
		pUCAsyncCommand = (IOFireWireAVCUserClientAsyncCommand *)fUCAsyncCommands->getObject(i);
		if (pUCAsyncCommand->commandIdentifierHandle == commandHandle)
		{
			found = true;
			break;
		}
	}
	IOUnlock(fAsyncAVCCmdLock);
	
	if (found == true)
		return pUCAsyncCommand;
	else
		return NULL;
}

//////////////////////////////////////////////////////
// IOFireWireAVCUserClient::HandleUCAsyncCommandCallback
//////////////////////////////////////////////////////
void IOFireWireAVCUserClient::HandleUCAsyncCommandCallback(IOFireWireAVCUserClientAsyncCommand *pUCAsyncCommand)
{
	UInt32 respLen;
	//void * args[kMaxAsyncArgs];
	io_user_reference_t args[kMaxAsyncArgs];
	OSAsyncReference64 asyncRef;
	
	FIRELOG_MSG(("IOFireWireAVCUserClient::HandleUCAsyncCommandCallback (this=0x%08X)\n",this));

	bcopy(fAsyncAVCCmdCallbackInfo, asyncRef, kOSAsyncRef64Size);

	// If we just got a response, copy it into the shared user/kernel response memory buffer for this command 
	switch(pUCAsyncCommand->pAsyncCommand->cmdState)
	{
		case kAVCAsyncCommandStateReceivedInterimResponse:
			pUCAsyncCommand->fMem->writeBytes(kAsyncCmdSharedBufInterimRespOffset,
											  pUCAsyncCommand->pAsyncCommand->pInterimResponseBuf,
											  pUCAsyncCommand->pAsyncCommand->interimResponseLen);
			respLen = pUCAsyncCommand->pAsyncCommand->interimResponseLen;
			break;
			
		case kAVCAsyncCommandStateReceivedFinalResponse:
			pUCAsyncCommand->fMem->writeBytes(kAsyncCmdSharedBufFinalRespOffset,
											  pUCAsyncCommand->pAsyncCommand->pFinalResponseBuf,
											  pUCAsyncCommand->pAsyncCommand->finalResponseLen);
			respLen = pUCAsyncCommand->pAsyncCommand->finalResponseLen;
			break;
		
		case kAVCAsyncCommandStatePendingRequest:
		case kAVCAsyncCommandStateRequestSent:
		case kAVCAsyncCommandStateRequestFailed:
		case kAVCAsyncCommandStateWaitingForResponse:
		case kAVCAsyncCommandStateTimeOutBeforeResponse:
		case kAVCAsyncCommandStateBusReset:
		case kAVCAsyncCommandStateOutOfMemory:
		case kAVCAsyncCommandStateCanceled:
		default:
			respLen = 0;
			break;
	}
	
	// Send the results to user space
	args[0] = (io_user_reference_t) pUCAsyncCommand->commandIdentifierHandle;
	args[1] = (io_user_reference_t) pUCAsyncCommand->pAsyncCommand->cmdState;
	args[2] = (io_user_reference_t) respLen;
	sendAsyncResult64(asyncRef, kIOReturnSuccess, args, 3);
}

//////////////////////////////////////////////////////
// AVCUserClientAsyncCommandCallback
//////////////////////////////////////////////////////
void AVCUserClientAsyncCommandCallback(void *pRefCon, IOFireWireAVCAsynchronousCommand *pCommandObject)
{
	IOFireWireAVCUserClientAsyncCommand *pUCAsyncCommand = (IOFireWireAVCUserClientAsyncCommand*) pRefCon;
	
	FIRELOG_MSG(("AVCUserClientAsyncCommandCallback (pCommandObject=0x%08X)\n",pCommandObject));

	pUCAsyncCommand->pUserClient->HandleUCAsyncCommandCallback(pUCAsyncCommand);
}

