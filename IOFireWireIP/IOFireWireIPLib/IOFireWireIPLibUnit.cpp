/*
 * Copyright (c) 1998-2001 Apple Computer, Inc. All rights reserved.
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "IOFireWireIPLib.h"
#include "IOFireWireIPUserClientCommon.h"
#include "IOFireWireIPLibUnit.h"

#include <CoreFoundation/CFMachPort.h>
#include <IOKit/IOMessage.h>

#include <syslog.h>	// Debug messages
#include <pthread.h>	// for mutexes

#if 0
#define FWLOG(x) printf x
#else
#define FWLOG(x) do {} while (0)
#endif

__BEGIN_DECLS
void *IOFireWireIPLibUnitFactory( CFAllocatorRef allocator, CFUUIDRef typeID );
__END_DECLS

struct _IPUnit;
typedef struct _InterfaceMap 
{
    IUnknownVTbl *pseudoVTable;
    struct _IPUnit *obj;
} InterfaceMap;

typedef struct _IPUnit
{

	//////////////////////////////////////
	// cf plugin interfaces
	
	InterfaceMap 			   				fIOCFPlugInInterface;
	InterfaceMap							fIOFireWireIPLibUnitInterface;

	//////////////////////////////////////
	// cf plugin ref counting
	
	CFUUIDRef 	fFactoryId;	
	UInt32 		fRefCount;

	//////////////////////////////////////	
	// user client connection
	
	io_service_t 	fService;
	io_connect_t 	fConnection;

	//////////////////////////////////////	
	// async callbacks
	
	CFRunLoopRef			fCFRunLoop;
	CFRunLoopSourceRef		fCFRunLoopSource;
    IONotificationPortRef	fNotifyPort;
    io_object_t				fNotification;
	IOFWIPMessageCallback	fMessageCallbackRoutine;
	void *					fMessageCallbackRefCon;
	
    //////////////////////////////////////	
	// async connection objects
	
    CFMutableArrayRef 	fACObjectArray;
	pthread_mutex_t 	fACObjectArrayLock;

    //////////////////////////////////////	
	// notifications
    
    Boolean					fSuspended;

} IPUnit;

	// utility function to get "this" pointer from interface
#define IPUnit_getThis( self ) \
        (((InterfaceMap *) self)->obj)

static IOReturn stop( void * self );

static UInt32 addRef( void * self )
{
    IPUnit *me = IPUnit_getThis(self);
	me->fRefCount++;
	return me->fRefCount;
}

static UInt32 release( void * self )
{
    IPUnit *me = IPUnit_getThis(self);
	UInt32 retVal = me->fRefCount;
	
	if( 1 == me->fRefCount-- ) 
	{
		pthread_mutex_lock( &me->fACObjectArrayLock );
    
        if( me->fACObjectArray )
        {
            CFRelease( me->fACObjectArray );  // release array and consumers
            me->fACObjectArray = NULL;
        }
        
        pthread_mutex_unlock( &me->fACObjectArrayLock );
        
        pthread_mutex_destroy( &me->fACObjectArrayLock );

        
        stop(self);
        CFPlugInRemoveInstanceForFactory( me->fFactoryId );
        CFRelease( me->fFactoryId );
		free(me);
    }
    else if( me->fRefCount < 0 )
	{
        me->fRefCount = 0;
	}
	
	return retVal;
}


static HRESULT queryInterface( void * self, REFIID iid, void **ppv )
{
    CFUUIDRef uuid = CFUUIDCreateFromUUIDBytes(NULL, iid);
    HRESULT result = S_OK;
    IPUnit *me = IPUnit_getThis(self);

	if( CFEqual(uuid, IUnknownUUID) ||  CFEqual(uuid, kIOCFPlugInInterfaceID) ) 
	{
        *ppv = &me->fIOCFPlugInInterface;
        addRef(self);
    }
	else if( CFEqual(uuid, kIOFireWireIPLibUnitInterfaceID) ) 
	{
        *ppv = &me->fIOFireWireIPLibUnitInterface;
        addRef(self);
    }
    else
        *ppv = 0;

    if( !*ppv )
        result = E_NOINTERFACE;

    CFRelease( uuid );
	
    return result;
}

//////////////////////////////////////////////////////////////////
// IOCFPlugInInterface methods
//

//////////////////////////////////////////////////////////////////
// callback static methods
//

// isDeviceSuspended
//
//

Boolean isDeviceSuspended( void * self )
{
    IPUnit * me = IPUnit_getThis(self);
    
    return me->fSuspended;
}

// probe
//
//

static IOReturn probe( void * self, CFDictionaryRef propertyTable, 
											io_service_t service, SInt32 *order )
{
	fprintf(stderr, "+ IOFireWireLibUnit probe\n") ;

	// only load against IP Units
    if( !service || !IOObjectConformsTo(service, "IOFireWireIP") )
        return kIOReturnBadArgument;
		
	return kIOReturnSuccess;
}

// start
//
//

static IOReturn start( void * self, CFDictionaryRef propertyTable, 
											io_service_t service )
{
	IOReturn status = kIOReturnSuccess;
    IPUnit *me = IPUnit_getThis(self);
	
	me->fService = service;
    status = IOServiceOpen( me->fService, mach_task_self(), 
							kIOFireWireIPLibConnection, &me->fConnection );
	
	fprintf(stderr, "+ IOFireWireLibUnit start %x \n", status) ;
	if( !me->fConnection )
		status = kIOReturnNoDevice;

	return status;
}

// stop
//
//

static IOReturn stop( void * self )
{
    IPUnit *me = IPUnit_getThis(self);
	if( me->fConnection ) 
	{
        IOServiceClose( me->fConnection );
        me->fConnection = MACH_PORT_NULL;
    }

	return kIOReturnSuccess;
}

//////////////////////////////////////////////////////////////////
// IOFireWireIPLibUnit methods
//

// open
//
//

static IOReturn open( void * self )
{
    IPUnit *me = IPUnit_getThis(self);
	IOReturn status = kIOReturnSuccess;
	
    if( !me->fConnection )		    
		return kIOReturnNoDevice; 

    status = IOConnectMethodScalarIScalarO( me->fConnection, kIOFWIPUserClientOpen, 0, 0);
	return status;
}

// openWithSessionRef
//
//

static IOReturn openWithSessionRef( void * self, IOFireWireSessionRef sessionRef )
{
    IPUnit *me = IPUnit_getThis(self);
	IOReturn status = kIOReturnSuccess;
	
    if( !me->fConnection )		    
		return kIOReturnNoDevice; 

    status = IOConnectMethodScalarIScalarO( me->fConnection, kIOFWIPUserClientOpenWithSessionRef, 
												1, 0, sessionRef);
	
	return status;
}

// getSessionRef
//
//

static IOFireWireSessionRef getSessionRef(void * self)
{
    IPUnit *me = IPUnit_getThis(self);
	IOReturn status = kIOReturnSuccess;
	IOFireWireSessionRef sessionRef = 0;
	
    if( !me->fConnection )		    
		return sessionRef; 

    status = IOConnectMethodScalarIScalarO( me->fConnection, kIOFWIPUserClientGetSessionRef, 
												0, 1, (int*)&sessionRef);	

	if( status != kIOReturnSuccess )
		sessionRef = 0; // just to make sure

	return sessionRef;
}

// close
//
//

static void close( void * self )
{
    IPUnit *me = IPUnit_getThis(self);
	if( !me->fConnection )		    
        return; 
		
	IOConnectMethodScalarIScalarO( me->fConnection, kIOFWIPUserClientClose, 0, 0);
}

// setMessageCallback
//
//

static void setMessageCallback( void * self, void * refCon, 
													IOFWIPMessageCallback callback )
{
    IPUnit *me = IPUnit_getThis(self);
	me->fMessageCallbackRoutine = callback;
	me->fMessageCallbackRefCon = refCon;
}


static IOReturn showLcb(void *self, const UInt8 * command, UInt32 cmdLen, UInt8 * response, UInt32 *responseLen)
{
    IPUnit *me = IPUnit_getThis(self);
    IOReturn status;
    IOByteCount outputCnt = *responseLen;
    if( !me->fConnection )		    
        return kIOReturnNotOpen; 
		
    status = IOConnectMethodStructureIStructureO(
        me->fConnection, kIOFWIPUserClientShowLcb,
        cmdLen, &outputCnt, (UInt8 *)command, response);
    if(status == kIOReturnSuccess)
        *responseLen = outputCnt;
    
    return status;
}

static IOReturn IPCommand(void *self, const UInt8 * command, UInt32 cmdLen, UInt8 * response, UInt32 *responseLen)
{
    IPUnit *me = IPUnit_getThis(self);
    IOReturn status;
    IOByteCount outputCnt = *responseLen;
    if( !me->fConnection )		    
        return kIOReturnNotOpen; 
		
    status = IOConnectMethodStructureIStructureO(
        me->fConnection, kIOFWIPUserClientIPCommand,
        cmdLen, &outputCnt, (UInt8 *)command, response);
    if(status == kIOReturnSuccess)
        *responseLen = outputCnt;
    
    return status;
}

//
// static interface table for IOCFPlugInInterface
//

static IOCFPlugInInterface sIOCFPlugInInterface = 
{
    0,
	&queryInterface,
	&addRef,
	&release,
	1, 0, // version/revision
	&probe,
	&start,
	&stop
};

//
// static interface table for IOFireWireIPLibUnitInterface
//

static IOFireWireIPLibUnitInterface sUnitInterface =
{
    0,
	&queryInterface,
	&addRef,
	&release,
	2, 0, // version/revision
	&open,
	&openWithSessionRef,
	&getSessionRef,
	&close,
	&setMessageCallback,
    &IPCommand,
	&showLcb
};

// IOFireWireIPLibUnitFactory

// alloc
//
// static allocator, called by factory method

static IOCFPlugInInterface ** alloc()
{
    IPUnit *	me;
	IOCFPlugInInterface ** 	interface = NULL;
	
    me = (IPUnit *)malloc(sizeof(IPUnit));
    if( me )
	{
        memset(me, 0, sizeof(IPUnit));
		// we return an interface here. queryInterface will not be called. set refs to 1
        // init cf plugin ref counting
        me->fRefCount = 1;
        
        // init user client connection
        me->fConnection = MACH_PORT_NULL;
        me->fService = MACH_PORT_NULL;
        
        // create plugin interface map
        me->fIOCFPlugInInterface.pseudoVTable = (IUnknownVTbl *) &sIOCFPlugInInterface;
        me->fIOCFPlugInInterface.obj = me;
        
        // create unit driver interface map
        me->fIOFireWireIPLibUnitInterface.pseudoVTable 
                                    = (IUnknownVTbl *) &sUnitInterface;
        me->fIOFireWireIPLibUnitInterface.obj = me;

		me->fSuspended = false;
		
      	pthread_mutex_init( &me->fACObjectArrayLock, NULL );
	    me->fACObjectArray = NULL;
        
        me->fFactoryId = kIOFireWireIPLibUnitFactoryID;
        CFRetain( me->fFactoryId );
        CFPlugInAddInstanceForFactory( me->fFactoryId );
        interface = (IOCFPlugInInterface **) &me->fIOCFPlugInInterface.pseudoVTable;
    }
	
	return interface;
}

//
// factory method (only exported symbol)

void *IOFireWireIPLibUnitFactory( CFAllocatorRef allocator, CFUUIDRef typeID )
{
    if( CFEqual(typeID, kIOFireWireIPLibUnitTypeID) )
        return (void *) alloc();
    else
        return NULL;
}

