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

#include "IOFireWireAVCUserClient.h"
#include <IOKit/IOLib.h>
#include <IOKit/IOMessage.h>
#include <IOKit/firewire/IOFireWireUnit.h>
#include <IOKit/firewire/IOFireWireController.h>

#define FWKLOG(arg)

OSDefineMetaClassAndStructors(IOFireWireAVCConnection, OSObject)
OSDefineMetaClassAndStructors(IOFireWireAVCUserClient, IOUserClient)

IOExternalMethod IOFireWireAVCUserClient::sMethods[kIOFWAVCUserClientNumCommands] =
{
    { //    kIOFWAVCUserClientOpen
        0,
        (IOMethod) &IOFireWireAVCUserClient::open,
        kIOUCScalarIScalarO,
        0,
        0
    },
    { //    kIOFWAVCUserClientClose
        0,
        (IOMethod) &IOFireWireAVCUserClient::close,
        kIOUCScalarIScalarO,
        0,
        0
    },
	{ //    kIOFWAVCUserClientOpenWithSessionRef
        0,
        (IOMethod) &IOFireWireAVCUserClient::openWithSessionRef,
        kIOUCScalarIScalarO,
        1,
        0
    },
	{ //    kIOFWAVCUserClientGetSessionRef
        0,
        (IOMethod) &IOFireWireAVCUserClient::getSessionRef,
        kIOUCScalarIScalarO,
        0,
        1
    },
    { //    kIOFWAVCUserClientAVCCommand
        0,
        (IOMethod) &IOFireWireAVCUserClient::AVCCommand,
        kIOUCStructIStructO,
        0xffffffff,
        0xffffffff
    },
    { //    kIOFWAVCUserClientAVCCommandInGeneration
        0,
        (IOMethod) &IOFireWireAVCUserClient::AVCCommandInGen,
        kIOUCStructIStructO,
        0xffffffff,
        0xffffffff
    },
    { //    kIOFWAVCUserClientUpdateAVCCommandTimeout
        0,
        (IOMethod) &IOFireWireAVCUserClient::updateAVCCommandTimeout,
        kIOUCScalarIScalarO,
        0,
        0
    },
	{ //    kIOFWAVCUserClientMakeP2PInputConnection
        0,
        (IOMethod) &IOFireWireAVCUserClient::makeP2PInputConnection,
        kIOUCScalarIScalarO,
        2,
        0
    },
	{ //    kIOFWAVCUserClientBreakP2PInputConnection
        0,
        (IOMethod) &IOFireWireAVCUserClient::breakP2PInputConnection,
        kIOUCScalarIScalarO,
        1,
        0
    },
	{ //    kIOFWAVCUserClientMakeP2POutputConnection
        0,
        (IOMethod) &IOFireWireAVCUserClient::makeP2POutputConnection,
        kIOUCScalarIScalarO,
        3,
        0
    },
	{ //    kIOFWAVCUserClientBreakP2POutputConnection
        0,
        (IOMethod) &IOFireWireAVCUserClient::breakP2POutputConnection,
        kIOUCScalarIScalarO,
        1,
        0
    }

};

//////////////////////////////////////////////////////////////////////////////////////////////////

// ctor
//
//

IOFireWireAVCUserClient *IOFireWireAVCUserClient::withTask(task_t owningTask)
{
    IOFireWireAVCUserClient* me = new IOFireWireAVCUserClient;

    if( me )
    {
        if( !me->init(NULL) )
        {
            me->release();
            return NULL;
        }
        me->fTask = owningTask;
    }

    return me;
}

bool IOFireWireAVCUserClient::init( OSDictionary * dictionary )
{
    bool res = IOService::init( dictionary );

    fOpened = false;
	fStarted = false;
    return res;
}

void IOFireWireAVCUserClient::free()
{
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
    IOService::free();
}

void IOFireWireAVCUserClient::stop( IOService * provider )
{
    //FireLog( "IOFireWireAVCUserClient::stop (0x%08X)\n",this);
    
    fStarted = false;
    
    IOService::stop(provider);
}


bool IOFireWireAVCUserClient::start( IOService * provider )
{
    FWKLOG(( "IOFireWireAVCUserClient : starting\n" ));
    
	if( fStarted )
		return false;
		
    fUnit = OSDynamicCast(IOFireWireAVCNub, provider);
    if (fUnit == NULL)
        return false;

	fConnections = OSArray::withCapacity(1);

    if( !IOUserClient::start(provider) )
        return false;

    fStarted = true;

    do {
    
    // If computer uses the old Lucent Phy, make the AVC device root to avoid
    // "Old Lucent Phy sends iso packets too quickly after cycle start for old TI 100mb Phy" bug
        IOService *fwim = fUnit->getDevice()->getBus()->getProvider();
        OSNumber *num;
        UInt32 device;
        UInt32 generation;
        UInt16 nodeID;
        num = OSDynamicCast(OSNumber, fwim->getProperty("PHY Vendor_ID"));
        if(!num)
            break;
        if(num->unsigned32BitValue() != 0x601d)
            break;
        num = OSDynamicCast(OSNumber, fwim->getProperty("PHY Device_ID"));
        if(!num)
            break;
        device = num->unsigned32BitValue();
        if(device != 0x81400 && device != 0x81401)
            break;
            
        // Set device to root if we are.
        IOFireWireController *control = fUnit->getDevice()->getController();
        control->getIRMNodeID(generation, nodeID);
        
        if(nodeID == control->getLocalNodeID()) {
            FWKLOG(("Setting AVC device to root\n"));
            fUnit->getDevice()->getNodeIDGeneration(generation, nodeID);
            control->makeRoot(generation, nodeID);
            IOSleep(1000);	// Give UniN time to get its act together?
        }
    } while (false);
	 
     return true;
}

// target/method lookups
//
//

IOExternalMethod* IOFireWireAVCUserClient::getTargetAndMethodForIndex(IOService **target, UInt32 index)
{
    if( index >= kIOFWAVCUserClientNumCommands )
        return NULL;
    else
    {
        *target = this;
        return &sMethods[index];
    }
}

IOExternalAsyncMethod* IOFireWireAVCUserClient::getAsyncTargetAndMethodForIndex(IOService **target, UInt32 index)
{
#if 0
    if( index >= kIOFWAVCUserClientNumAsyncCommands )
       return NULL;
    else
    {
        *target = this;
        return &sAsyncMethods[index];
    }
#else
    return NULL;
#endif
}

// clientClose / clientDied
//
//

IOReturn IOFireWireAVCUserClient::clientClose( void )
{
    FWKLOG(( "IOFireWireAVCUserClient : clientClose\n" ));

    if( fOpened )
    {
        fOpened = false;
    }
    
	fStarted = false;
	
	terminate( kIOServiceRequired );
	
	return kIOReturnSuccess;
}

IOReturn IOFireWireAVCUserClient::clientDied( void )
{
    FWKLOG(( "IOFireWireAVCUserClient : clientDied\n" ));

    return clientClose();
}


IOReturn IOFireWireAVCUserClient::open
	( void *, void *, void *, void *, void *, void * )
{
    IOReturn status = kIOReturnSuccess;

    FWKLOG(( "IOFireWireAVCUserClient : open\n" ));

    if( fOpened )
        status = kIOReturnError;

    if( status == kIOReturnSuccess )
    {
        if( fUnit->open(this) )
		{
            fOpened = true;
		}
		else
            status = kIOReturnExclusiveAccess;
    }
    
     return status;
}

IOReturn IOFireWireAVCUserClient::openWithSessionRef( IOFireWireSessionRef sessionRef, void *, void *, void *, void *, void * )
{
    IOReturn status = kIOReturnSuccess;
	IOService * service;

    FWKLOG(( "IOFireWireAVCUserClient : openWithSessionRef\n" ));

    if( fOpened || !fUnit->isOpen() )
        status = kIOReturnError;
	
	if( status == kIOReturnSuccess )
	{
		service = OSDynamicCast( IOService, (OSObject*)sessionRef );
		if( service == NULL )
			status = kIOReturnBadArgument;
	}
	
	if( status == kIOReturnSuccess )
	{
		// look for us in provider chain
		while( fUnit != service && service != NULL )
			service = service->getProvider();
		
		// were we found	
		if( service == NULL )
			status = kIOReturnBadArgument;
	}
	
	return status;
}

IOReturn IOFireWireAVCUserClient::getSessionRef( IOFireWireSessionRef * sessionRef, void *, void *, void *, void *, void * )
{
    IOReturn status = kIOReturnSuccess;

    FWKLOG(( "IOFireWireAVCUserClient : getSessionRef\n" ));

    if( !fOpened )
        status = kIOReturnError;

    if( status == kIOReturnSuccess )
    {
		*sessionRef = (IOFireWireSessionRef)this;
	}
    
	return status;
}

IOReturn IOFireWireAVCUserClient::close
	( void *, void *, void *, void *, void *, void * )
{
    FWKLOG(( "IOFireWireAVCUserClient : close\n" ));
    
    if( fOpened )
    {
        fUnit->close(this);
        fOpened = false;
    }

    return kIOReturnSuccess;
}

IOReturn IOFireWireAVCUserClient::AVCCommand(UInt8 * cmd, UInt8 * response,
    UInt32 len, UInt32 *size)
{
    IOReturn res;

    if(!fStarted )
	return kIOReturnNoDevice;

    res = fUnit->AVCCommand(cmd,len,response,size);
    return res;
}

IOReturn IOFireWireAVCUserClient::AVCCommandInGen(UInt8 * cmd, UInt8 * response,
    UInt32 len, UInt32 *size)
{
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

IOReturn IOFireWireAVCUserClient::updateAVCCommandTimeout
	( void *, void *, void *, void *, void *, void * )
{
    //IOLog("IOFireWireAVCUserClient : updateAVCCommandTimeout\n");
    
    if(!fStarted )
	return kIOReturnNoDevice;
    
    fUnit->updateAVCCommandTimeout();

    return kIOReturnSuccess;
}

IOReturn IOFireWireAVCUserClient::updateP2PCount(UInt32 addr, SInt32 inc, bool failOnBusReset, UInt32 chan, IOFWSpeed speed)
{
    if(!fStarted )
	return kIOReturnNoDevice;

    IOFireWireNub *device = fUnit->getDevice();
    FWAddress plugAddr(kCSRRegisterSpaceBaseAddressHi, addr);
    IOFWReadQuadCommand *readCmd;
    IOFWCompareAndSwapCommand *lockCmd;
    UInt32 plugVal, newVal;
	UInt32 curCount;
	UInt32 curChan;
	IOFWSpeed curSpeed;
    IOReturn res;
    
    readCmd = device->createReadQuadCommand(plugAddr, &plugVal, 1, NULL, NULL, failOnBusReset);
    res = readCmd->submit();
    readCmd->release();
    if(res != kIOReturnSuccess)
        return res;
        
    for(int i=0; i<4; i++) {
        bool success;

		// Parse current plug value
		curCount = ((plugVal & kIOFWPCRP2PCount) >> 24);
		curChan = ((plugVal & kIOFWPCRChannel) >> 16);
		curSpeed = (IOFWSpeed)((plugVal & kIOFWPCROutputDataRate) >> 14);
		newVal = plugVal;

		// If requested, modify channel
		if (chan != 0xFFFFFFFF)
		{
			if ((curCount != 0) && (chan != curChan))
				return kIOReturnError;

			newVal &= ~kIOFWPCRChannel;
			newVal |= ((chan & 0x3F) << 16);
		}

		// If requested, modify speed
		if (speed != kFWSpeedInvalid)
		{
			if ((curCount != 0) && (speed != curSpeed))
				return kIOReturnError;

			newVal &= ~kIOFWPCROutputDataRate;
			newVal |= ((speed & 0x03) << 14);
		}

		// Modify P2P count
		newVal &= ~kIOFWPCRP2PCount;
		if (inc > 0)
		{
			if (curCount == 0x3F)
				return kIOReturnError;
			newVal |= ((curCount+1) << 24);
		}
		else
		{
			if (curCount == 0)
				return kIOReturnError;
			newVal |= ((curCount-1) << 24);
		}
		
        lockCmd = device->createCompareAndSwapCommand(plugAddr, &plugVal, &newVal, 1);
        res = lockCmd->submit();
        success = lockCmd->locked(&plugVal);
        lockCmd->release();
        if(res != kIOReturnSuccess)
            break;
        if(success)
            break;
    }
    return res;
}

IOReturn IOFireWireAVCUserClient::makeConnection(UInt32 addr, UInt32 chan, IOFWSpeed speed)
{
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

void IOFireWireAVCUserClient::breakConnection(UInt32 addr)
{
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

IOReturn IOFireWireAVCUserClient::makeP2PInputConnection( UInt32 plugNo, UInt32 chan, void *, void *, void *, void *)
{
    return makeConnection(kPCRBaseAddress+0x84+4*plugNo, chan, kFWSpeedInvalid);
}

IOReturn IOFireWireAVCUserClient::breakP2PInputConnection( UInt32 plugNo, void *, void *, void *, void *, void *)
{
    breakConnection(kPCRBaseAddress+0x84+4*plugNo);
	return kIOReturnSuccess;
}

IOReturn IOFireWireAVCUserClient::makeP2POutputConnection( UInt32 plugNo, UInt32 chan, IOFWSpeed speed, void *, void *, void *)
{
    return makeConnection(kPCRBaseAddress+4+4*plugNo, chan, speed);
}

IOReturn IOFireWireAVCUserClient::breakP2POutputConnection( UInt32 plugNo, void *, void *, void *, void *, void *)
{
    breakConnection(kPCRBaseAddress+4+4*plugNo);
	return kIOReturnSuccess;
}

IOReturn IOFireWireAVCUserClient::message(UInt32 type, IOService *provider, void *argument)
{
    if( fStarted == true && type == kIOMessageServiceIsResumed ) {
        retain();	// Make sure we don't get deleted with the thread running
        IOCreateThread(remakeConnections, this);
    }
    
    return kIOReturnSuccess;
}

void IOFireWireAVCUserClient::remakeConnections(void *arg)
{
    IOFireWireAVCUserClient *me = (IOFireWireAVCUserClient *)arg;
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

