/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include "IOFireWireIPUserClient.h"
#include <IOKit/IOLib.h>
#include <IOKit/firewire/IOFireWireUnit.h>
#include <IOKit/firewire/IOFireWireController.h>


OSDefineMetaClassAndStructors(IOFireWireIPUserClient, IOUserClient)

IOExternalMethod IOFireWireIPUserClient::sMethods[kIOFWIPUserClientNumCommands] =
{
    { //    kIOFWIPUserClientOpen
        0,
        (IOMethod) &IOFireWireIPUserClient::open,
        kIOUCScalarIScalarO,
        0,
        0
    },
    { //    kIOFWIPUserClientClose
        0,
        (IOMethod) &IOFireWireIPUserClient::close,
        kIOUCScalarIScalarO,
        0,
        0
    },
	{ //    kIOFWIPUserClientOpenWithSessionRef
        0,
        (IOMethod) &IOFireWireIPUserClient::openWithSessionRef,
        kIOUCScalarIScalarO,
        1,
        0
    },
	{ //    kIOFWIPUserClientGetSessionRef
        0,
        (IOMethod) &IOFireWireIPUserClient::getSessionRef,
        kIOUCScalarIScalarO,
        0,
        1
    },
    { //    kIOFWIPUserClientIPCommand
        0,
        (IOMethod) &IOFireWireIPUserClient::IPCommand,
        kIOUCStructIStructO,
        0xffffffff,
        0xffffffff
    },
    { //    kIOFWIPUserClientShowLcb
        0,
        (IOMethod) &IOFireWireIPUserClient::showLcb,
        kIOUCStructIStructO,
        0xffffffff,
        0xffffffff
    }
};

//////////////////////////////////////////////////////////////////////////////////////////////////

// ctor
//
//

IOFireWireIPUserClient *IOFireWireIPUserClient::withTask(task_t owningTask)
{
    IOFireWireIPUserClient* me = new IOFireWireIPUserClient;

    if( me )
    {
        if( !me->init() )
        {
            me->release();
            return NULL;
        }
        me->fTask = owningTask;
    }

    return me;
}

bool IOFireWireIPUserClient::init( OSDictionary * dictionary )
{
    bool res = IOService::init( dictionary );

	IOLog(( "IOFireWireIPUserClient : initing\n" ));

    fOpened = false;
	fStarted = false;
	
    return res;
}

bool IOFireWireIPUserClient::start( IOService * provider )
{
    IOLog(( "IOFireWireIPUserClient : starting\n" ));
    
    if( fStarted )
        return false;
		
    fFireWireIP = OSDynamicCast(IOFireWireIP, provider);
    if (fFireWireIP == NULL){
        IOLog(( "		OSDynamicCast\n" ));
        return false;
    }

    if( !IOUserClient::start(provider) ){
        IOLog(( "		IOUserClient::start\n" ));
        return false;
    }
  
    fStarted = true;
	
    IOLog(( "IOFireWireIPUserClient : started\n" ));
    
	return true;
}

// target/method lookups
//
//

IOExternalMethod* IOFireWireIPUserClient::getTargetAndMethodForIndex(IOService **target, UInt32 index)
{
    if( index >= kIOFWIPUserClientNumCommands )
        return NULL;
    else
    {
        *target = this;
        return &sMethods[index];
    }
}

IOExternalAsyncMethod* IOFireWireIPUserClient::getAsyncTargetAndMethodForIndex(IOService **target, UInt32 index)
{
#if 0
    if( index >= kIOFWIPUserClientNumAsyncCommands )
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

IOReturn IOFireWireIPUserClient::clientClose( void )
{
    IOLog(( "IOFireWireIPUserClient : clientClose\n" ));

    if( fOpened )
    {
        fOpened = false;
    }
    
	fStarted = false;
	
	terminate( kIOServiceRequired );
	
	return kIOReturnSuccess;
}

IOReturn IOFireWireIPUserClient::clientDied( void )
{
    IOLog(( "IOFireWireIPUserClient : clientDied\n" ));

    return clientClose();
}


IOReturn IOFireWireIPUserClient::open
	( void *, void *, void *, void *, void *, void * )
{
    IOReturn status = kIOReturnSuccess;

    IOLog(( "IOFireWireIPUserClient : open\n" ));

    if( fOpened )
        status = kIOReturnError;

    if( status == kIOReturnSuccess )
    {
        if( fFireWireIP->open(this) )
		{
            fOpened = true;
		}
		else
            status = kIOReturnExclusiveAccess;
    }
    
     return status;
}

IOReturn IOFireWireIPUserClient::openWithSessionRef( IOFireWireSessionRef sessionRef, void *, void *, void *, void *, void * )
{
    IOReturn status = kIOReturnSuccess;
	IOService * service;

    IOLog(( "IOFireWireIPUserClient : openWithSessionRef\n" ));

    if( fOpened || !fFireWireIP->isOpen() )
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
		while( fFireWireIP != service && service != NULL )
			service = service->getProvider();
		
		// were we found	
		if( service == NULL )
			status = kIOReturnBadArgument;
	}
	
	return status;
}

IOReturn IOFireWireIPUserClient::getSessionRef( IOFireWireSessionRef * sessionRef, void *, void *, void *, void *, void * )
{
    IOReturn status = kIOReturnSuccess;

    IOLog(( "IOFireWireIPUserClient : getSessionRef\n" ));

    if( !fOpened )
        status = kIOReturnError;

    if( status == kIOReturnSuccess )
    {
		*sessionRef = (IOFireWireSessionRef)this;
	}
    
	return status;
}

IOReturn IOFireWireIPUserClient::close
	( void *, void *, void *, void *, void *, void * )
{
    IOLog(( "IOFireWireIPUserClient : close\n" ));
    
    if( fOpened )
    {
        fFireWireIP->close(this);
        fOpened = false;
    }

    return kIOReturnSuccess;
}

IOReturn IOFireWireIPUserClient::IPCommand(UInt8 * cmd, UInt8 * response,
    UInt32 len, UInt32 *size)
{
    IOReturn res = 0xDEADBEEF;
    
    IOLog("IPUserClient called\n");
        
//  res = fFireWireIP->IPCommand(cmd,len,response,size);

    return res;
}

IOReturn IOFireWireIPUserClient::showLcb(UInt8 * cmd, UInt8 * response,
    UInt32 len, UInt32 *size)
{
    IOReturn res = 0;
    
    IOLog("IPUserClient showlcb called\n");
        
	fFireWireIP->showLcb();

    return res;
}



