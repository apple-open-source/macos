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

#include "IOFireWireAVCLib.h"
#include "IOFireWireAVCConsts.h"
#include "IOFireWireAVCLibConsumer.h"
#include "IOFireWireAVCUserClientCommon.h"
#include "IOFireWireAVCLibUnit.h"

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
void *IOFireWireAVCLibUnitFactory( CFAllocatorRef allocator, CFUUIDRef typeID );
__END_DECLS

struct _AVCUnit;
typedef struct _InterfaceMap 
{
    IUnknownVTbl *pseudoVTable;
    struct _AVCUnit *obj;
} InterfaceMap;

typedef struct _AVCUnit
{

	//////////////////////////////////////
	// cf plugin interfaces
	
	InterfaceMap 			   				fIOCFPlugInInterface;
	InterfaceMap							fIOFireWireAVCLibUnitInterface;

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
	IOFWAVCMessageCallback	fMessageCallbackRoutine;
	void *					fMessageCallbackRefCon;
	
    //////////////////////////////////////	
	// async connection objects
	
    CFMutableArrayRef 	fACObjectArray;
	pthread_mutex_t 	fACObjectArrayLock;

    //////////////////////////////////////	
	// notifications
    
    Boolean					fSuspended;

} AVCUnit;

	// utility function to get "this" pointer from interface
#define AVCUnit_getThis( self ) \
        (((InterfaceMap *) self)->obj)

static IOReturn stop( void * self );
static void removeIODispatcherFromRunLoop( void * self );

static UInt32 addRef( void * self )
{
    AVCUnit *me = AVCUnit_getThis(self);
	me->fRefCount++;
	return me->fRefCount;
}

static UInt32 release( void * self )
{
    AVCUnit *me = AVCUnit_getThis(self);
	UInt32 retVal = me->fRefCount;
	
	if( 1 == me->fRefCount-- ) 
	{
        // First disconnect from kernel before deleting things accessed by kernel callbacks
        removeIODispatcherFromRunLoop(self);
        stop(self);
        
		pthread_mutex_lock( &me->fACObjectArrayLock );
    
        if( me->fACObjectArray )
        {
            CFRelease( me->fACObjectArray );  // release array and consumers
            me->fACObjectArray = NULL;
        }
        
        pthread_mutex_unlock( &me->fACObjectArrayLock );
        
        pthread_mutex_destroy( &me->fACObjectArrayLock );

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
    AVCUnit *me = AVCUnit_getThis(self);

	if( CFEqual(uuid, IUnknownUUID) ||  CFEqual(uuid, kIOCFPlugInInterfaceID) ) 
	{
        *ppv = &me->fIOCFPlugInInterface;
        addRef(self);
    }
	else if( CFEqual(uuid, kIOFireWireAVCLibUnitInterfaceID) ) 
	{
        *ppv = &me->fIOFireWireAVCLibUnitInterface;
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

// messageCallback
//
//

static void messageCallback(void * refcon, io_service_t service,
                          natural_t messageType, void *messageArgument)
{
    AVCUnit *me = (AVCUnit *)refcon;
    IOReturn status = kIOReturnSuccess;
    
    CFIndex count = 0;
    CFIndex i = 0;

	//FWLOG(( "IOFireWireAVCLibUnit : messageCallback numArgs = %d\n", numArgs ));

	switch( messageType )
	{
        case kIOMessageServiceIsSuspended:
            me->fSuspended = true;
            break;
            
        case kIOMessageServiceIsResumed:
            me->fSuspended = false;
            break;
            
        default:
			break;
	}
    
    //
    // put all consumers into a local array to avoid calling callback with lock held
    //
    
    CFMutableArrayRef array = CFArrayCreateMutable(	kCFAllocatorDefault, 
                                                    2, // max capacity 
                                                    IOFireWireAVCLibConsumer::getCFArrayCallbacks() );
    if( array == NULL )
        status = kIOReturnNoMemory;       
    
    if( status == kIOReturnSuccess )
    {
        pthread_mutex_lock( &me->fACObjectArrayLock );
                
        count = CFArrayGetCount( me->fACObjectArray );
        for( i = 0; i < count; i++ )
        {
            CFArrayAppendValue( array, CFArrayGetValueAtIndex( me->fACObjectArray, i ) );
        }
        
        pthread_mutex_unlock( &me->fACObjectArrayLock );
 
        for( i = 0; i < count; i++ )
        {
            IOFireWireAVCLibConsumer * consumer = ( IOFireWireAVCLibConsumer*)CFArrayGetValueAtIndex( array, i );
            consumer->deviceInterestCallback( messageType, messageArgument );
        }
        
        CFRelease( array );
	}
	
	if( me->fMessageCallbackRoutine != NULL )
		(me->fMessageCallbackRoutine)( me->fMessageCallbackRefCon, messageType, messageArgument );
}

// isDeviceSuspended
//
//

Boolean isDeviceSuspended( void * self )
{
    AVCUnit * me = AVCUnit_getThis(self);
    
    return me->fSuspended;
}

// probe
//
//

static IOReturn probe( void * self, CFDictionaryRef propertyTable, 
											io_service_t service, SInt32 *order )
{
	// only load against AVC Units and SubUnits
    if( !service || !IOObjectConformsTo(service, "IOFireWireAVCNub") )
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
    AVCUnit *me = AVCUnit_getThis(self);
	
	me->fService = service;
    status = IOServiceOpen( me->fService, mach_task_self(), 
							kIOFireWireAVCLibConnection, &me->fConnection );
	if( !me->fConnection )
		status = kIOReturnNoDevice;

	return status;
}

// stop
//
//

static IOReturn stop( void * self )
{
    AVCUnit *me = AVCUnit_getThis(self);
	if( me->fConnection ) 
	{
        IOServiceClose( me->fConnection );
        me->fConnection = MACH_PORT_NULL;
    }

	return kIOReturnSuccess;
}

//////////////////////////////////////////////////////////////////
// IOFireWireAVCLibUnit methods
//

// open
//
//

static IOReturn open( void * self )
{
    AVCUnit *me = AVCUnit_getThis(self);
	IOReturn status = kIOReturnSuccess;
	
    if( !me->fConnection )		    
		return kIOReturnNoDevice; 

    status = IOConnectMethodScalarIScalarO( me->fConnection, kIOFWAVCUserClientOpen, 0, 0);
	return status;
}

// openWithSessionRef
//
//

static IOReturn openWithSessionRef( void * self, IOFireWireSessionRef sessionRef )
{
    AVCUnit *me = AVCUnit_getThis(self);
	IOReturn status = kIOReturnSuccess;
	
    if( !me->fConnection )		    
		return kIOReturnNoDevice; 

    status = IOConnectMethodScalarIScalarO( me->fConnection, kIOFWAVCUserClientOpenWithSessionRef, 
												1, 0, sessionRef);
	
	return status;
}

// getSessionRef
//
//

static IOFireWireSessionRef getSessionRef(void * self)
{
    AVCUnit *me = AVCUnit_getThis(self);
	IOReturn status = kIOReturnSuccess;
	IOFireWireSessionRef sessionRef = 0;
	
    if( !me->fConnection )		    
		return sessionRef; 

    status = IOConnectMethodScalarIScalarO( me->fConnection, kIOFWAVCUserClientGetSessionRef, 
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
    AVCUnit *me = AVCUnit_getThis(self);
	if( !me->fConnection )		    
        return; 
		
	IOConnectMethodScalarIScalarO( me->fConnection, kIOFWAVCUserClientClose, 0, 0);
}

// addIODispatcherToRunLoop
//
//

static IOReturn addIODispatcherToRunLoop( void *self, CFRunLoopRef cfRunLoopRef )
{
    AVCUnit *me = AVCUnit_getThis(self);
	IOReturn 				status = kIOReturnSuccess;
    mach_port_t masterDevicePort;
	IONotificationPortRef notifyPort;
	CFRunLoopSourceRef cfSource;
	//FWLOG(( "IOFireWireAVCLibUnit : addIODispatcherToRunLoop\n" ));
	
	if( !me->fConnection )		    
		return kIOReturnNoDevice; 

	// get mach master port
	status = IOMasterPort(bootstrap_port, &masterDevicePort) ;

    notifyPort = IONotificationPortCreate(masterDevicePort);
    cfSource = IONotificationPortGetRunLoopSource(notifyPort);
    CFRunLoopAddSource(cfRunLoopRef, cfSource, kCFRunLoopDefaultMode);
// Get messages from device
    status = IOServiceAddInterestNotification(notifyPort, me->fService,
                                kIOGeneralInterest, messageCallback, me,
                                &me->fNotification);
	
    me->fCFRunLoop = cfRunLoopRef;
    me->fNotifyPort = notifyPort;
    
	return status;
}

// removeIODispatcherFromRunLoop
//
//

static void removeIODispatcherFromRunLoop( void * self )
{
    AVCUnit *me = AVCUnit_getThis(self);
    if( me->fNotification )
    {
        IOObjectRelease(me->fNotification);
        me->fNotification = NULL;
    }
	if( me->fNotifyPort )
	{
		CFRunLoopRemoveSource( me->fCFRunLoop,
            IONotificationPortGetRunLoopSource(me->fNotifyPort), kCFRunLoopDefaultMode );
        IONotificationPortDestroy(me->fNotifyPort);
		me->fNotifyPort = NULL;
	}
}

// setMessageCallback
//
//

static void setMessageCallback( void * self, void * refCon, 
													IOFWAVCMessageCallback callback )
{
    AVCUnit *me = AVCUnit_getThis(self);
	me->fMessageCallbackRoutine = callback;
	me->fMessageCallbackRefCon = refCon;
}


static IOReturn AVCCommand(void *self, const UInt8 * command, UInt32 cmdLen, UInt8 * response, UInt32 *responseLen)
{
    AVCUnit *me = AVCUnit_getThis(self);
    IOReturn status;
    IOByteCount outputCnt = *responseLen;
	if( !me->fConnection )		    
        return kIOReturnNotOpen; 
		
    status = IOConnectMethodStructureIStructureO(
        me->fConnection, kIOFWAVCUserClientAVCCommand,
        cmdLen, &outputCnt, (UInt8 *)command, response);
    if(status == kIOReturnSuccess)
        *responseLen = outputCnt;
    
    return status;
}

static IOReturn AVCCommandInGeneration(void *self, UInt32 generation,
            const UInt8 * command, UInt32 cmdLen, UInt8 * response, UInt32 *responseLen)
{
    UInt8 annoying[sizeof(UInt32) + 512];
    AVCUnit *me = AVCUnit_getThis(self);
    IOReturn status;
    IOByteCount outputCnt = *responseLen;
	if( !me->fConnection )		    
        return kIOReturnNotOpen; 
		
    // Have to stick the generation in with the command bytes.
    *(UInt32 *)annoying = generation;
    bcopy(command, annoying+sizeof(UInt32), cmdLen);
    status = IOConnectMethodStructureIStructureO(
        me->fConnection, kIOFWAVCUserClientAVCCommandInGen,
        cmdLen+sizeof(UInt32), &outputCnt, annoying, response);
    if(status == kIOReturnSuccess)
        *responseLen = outputCnt;
    
    return status;
}

static void *GetAncestorInterface( void * self, char * object_class, REFIID pluginType, REFIID iid)
{
    io_registry_entry_t 	parent;
    IOCFPlugInInterface** 	theCFPlugInInterface = 0;
    void *					resultInterface = 0 ;
    SInt32					theScore ;
    IOReturn				err;
    HRESULT					comErr;
    AVCUnit *				me = AVCUnit_getThis(self);
    CFUUIDRef 				type_id = CFUUIDCreateFromUUIDBytes(NULL, pluginType);

    do {
        err = IORegistryEntryGetParentEntry(me->fService, kIOServicePlane, &parent);
        
        while(!err && !IOObjectConformsTo(parent, object_class) )
            err = IORegistryEntryGetParentEntry(parent, kIOServicePlane, &parent);
            
        if(err)
            break;

        err = IOCreatePlugInInterfaceForService(
                        parent,
                        type_id,
                        kIOCFPlugInInterfaceID,		//interfaceType,
                        & theCFPlugInInterface, 
                        & theScore);
        if(err)
            break;
            
        comErr = (*theCFPlugInInterface)->QueryInterface(
                                            theCFPlugInInterface, 
                                            iid, 
                                            (void**) & resultInterface);
        if (comErr != S_OK) {
            err = comErr;
            break;
        }
    } while (false);
    
    if(theCFPlugInInterface) {
        UInt32 ref;
        ref = (*theCFPlugInInterface)->Release(theCFPlugInInterface);	// Leave just one reference.
    }

    CFRelease( type_id );
        
    return resultInterface;
}

static void *GetProtocolInterface( void * self, REFIID pluginType, REFIID iid)
{
    io_registry_entry_t 	parent;
    io_registry_entry_t 	child;
    io_iterator_t			iterator = NULL;
    IOCFPlugInInterface** 	theCFPlugInInterface = 0;
    void *					resultInterface = 0 ;
    SInt32					theScore ;
    IOReturn				err;
    HRESULT					comErr;
    AVCUnit *				me = AVCUnit_getThis(self);
    CFUUIDRef 				type_id = CFUUIDCreateFromUUIDBytes(NULL, pluginType);

    do {
        err = IORegistryEntryGetParentEntry(me->fService, kIOServicePlane, &parent);
        
        while(!err && !IOObjectConformsTo(parent, "IOFireWireController") )
            err = IORegistryEntryGetParentEntry(parent, kIOServicePlane, &parent);
            
        if(err)
            break;

        // Now search for an IOFireWireLocalNode.
        err = IORegistryEntryGetChildIterator(parent, kIOServicePlane, &iterator );
        if(err)
            break;
        
        while(child = IOIteratorNext(iterator)) {
            if(IOObjectConformsTo(child, "IOFireWireLocalNode"))
                break;
            IOObjectRelease(child);
        }

        if(!child)
            break;
            
        err = IOCreatePlugInInterfaceForService(
                        child,
                        type_id,
                        kIOCFPlugInInterfaceID,		//interfaceType,
                        & theCFPlugInInterface, 
                        & theScore);
        if(err)
            break;
            
        comErr = (*theCFPlugInInterface)->QueryInterface(
                                            theCFPlugInInterface, 
                                            iid, 
                                            (void**) & resultInterface);
        if (comErr != S_OK) {
            err = comErr;
            break;
        }
    } while (false);
    
    if(theCFPlugInInterface) {
        UInt32 ref;
        ref = (*theCFPlugInInterface)->Release(theCFPlugInInterface);	// Leave just one reference.
    }

    if(iterator)
        IOObjectRelease(iterator);

    CFRelease( type_id );
        
    return resultInterface;
}

// getAsyncConnectionPlugCounts
//
//

static IOReturn getAsyncConnectionPlugCounts( void *self, UInt8 * inputPlugCount, UInt8 * outputPlugCount )
{
    IOReturn status;
    UInt8 command[8];
    UInt8 response[8];
    UInt32 responseLength = 0;
    
    command[0] = 0x01;
    command[1] = kAVCUnitAddress;
    command[2] = 0x02;
    command[3] = 0x01;
    command[4] = 0xFF;
    command[5] = 0xFF;
    command[6] = 0XFF;
    command[7] = 0XFF;
    
    status = AVCCommand( self, command, 8, response, &responseLength );
    
    if( status == kIOReturnSuccess && responseLength == 8 )
    {
        *inputPlugCount = response[4];
        *outputPlugCount = response[5];
        
        return kIOReturnSuccess;
    }
    else
        return kIOReturnError;
}

// createConsumerPlug
//
//

static IUnknownVTbl ** createConsumerPlug( void *self, UInt8 plugNumber, REFIID iid )
{
	IOReturn status = kIOReturnSuccess;
	IUnknownVTbl ** iunknown = NULL;
	IUnknownVTbl ** consumer = NULL;
    AVCUnit * me = AVCUnit_getThis(self);
    
    pthread_mutex_lock( &me->fACObjectArrayLock );
	
	if( status == kIOReturnSuccess )
	{
		iunknown = IOFireWireAVCLibConsumer::alloc( (IOFireWireAVCLibUnitInterface **)self, me->fCFRunLoop, plugNumber);
		if( iunknown == NULL )
			status = kIOReturnNoMemory;
	}
	
	if( status == kIOReturnSuccess )
	{	
		HRESULT res;
		res = (*iunknown)->QueryInterface( iunknown, iid,
										   (void **) &consumer );
		
		if( res != S_OK )
			status = kIOReturnError;
	}

    FWLOG(( "IOFireWireAVCLibUnit : about to CFArrayAppendValue\n" ));
    
    if( status == kIOReturnSuccess )
    {
        IOFireWireAVCLibConsumer * consumerObject;
        
        consumerObject = IOFireWireAVCLibConsumer::getThis( consumer );

        CFArrayAppendValue( me->fACObjectArray, (void*)consumerObject );
    }

	if( iunknown != NULL )
	{
		(*iunknown)->Release(iunknown);
	}
    
	FWLOG(( "IOFireWireAVCLibUnit : just CFArrayAppendValue\n" ));
    
    pthread_mutex_unlock( &me->fACObjectArrayLock );
    
	if( status == kIOReturnSuccess )
		return consumer;
	else
		return NULL;
}

void consumerPlugDestroyed( void * self, IOFireWireAVCLibConsumer * consumer )
{
    CFIndex 	count = 0;
    CFIndex 	index = 0;
    AVCUnit *	 me = AVCUnit_getThis(self);

    FWLOG(( "IOFireWireAVCLibUnit : consumerPlugDestroyed\n" ));
    
    pthread_mutex_lock( &me->fACObjectArrayLock );
    
    count = CFArrayGetCount( me->fACObjectArray );
    index = CFArrayGetFirstIndexOfValue( me->fACObjectArray, 
                                         CFRangeMake(0, count), 
                                         (void *)consumer );
    if( index != -1 )
    {												
        CFArrayRemoveValueAtIndex( me->fACObjectArray, index );
    }
        
    pthread_mutex_unlock( &me->fACObjectArrayLock );    
}

static IOReturn updateAVCCommandTimeout( void * self )
{
    AVCUnit *me = AVCUnit_getThis(self);
	if( !me->fConnection )		    
        return kIOReturnNotOpen; 
		
	return IOConnectMethodScalarIScalarO( me->fConnection, kIOFWAVCUserClientUpdateAVCCommandTimeout, 0, 0);
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
// static interface table for IOFireWireAVCLibUnitInterface
//

static IOFireWireAVCLibUnitInterface sUnitInterface =
{
    0,
	&queryInterface,
	&addRef,
	&release,
	3, 0, // version/revision
	&open,
	&openWithSessionRef,
	&getSessionRef,
	&close,
	&addIODispatcherToRunLoop,
	&removeIODispatcherFromRunLoop,
	&setMessageCallback,
    &AVCCommand,
    &AVCCommandInGeneration,
    &GetAncestorInterface,
    &GetProtocolInterface,
	&getAsyncConnectionPlugCounts,
	&createConsumerPlug,
    &updateAVCCommandTimeout
};

// IOFireWireAVCLibUnitFactory

// alloc
//
// static allocator, called by factory method

static IOCFPlugInInterface ** alloc()
{
    AVCUnit *	me;
	IOCFPlugInInterface ** 	interface = NULL;
	
    me = (AVCUnit *)malloc(sizeof(AVCUnit));
    if( me )
	{
        bzero(me, sizeof(AVCUnit));
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
        me->fIOFireWireAVCLibUnitInterface.pseudoVTable 
                                    = (IUnknownVTbl *) &sUnitInterface;
        me->fIOFireWireAVCLibUnitInterface.obj = me;

		me->fSuspended = false;
		
      	pthread_mutex_init( &me->fACObjectArrayLock, NULL );
	    me->fACObjectArray = CFArrayCreateMutable(	kCFAllocatorDefault, 
                                                    2, // capacity 
                                                    IOFireWireAVCLibConsumer::getCFArrayCallbacks() );

        me->fFactoryId = kIOFireWireAVCLibUnitFactoryID;
        CFRetain( me->fFactoryId );
        CFPlugInAddInstanceForFactory( me->fFactoryId );
        interface = (IOCFPlugInInterface **) &me->fIOCFPlugInInterface.pseudoVTable;
    }
	
	return interface;
}

//
// factory method (only exported symbol)

void *IOFireWireAVCLibUnitFactory( CFAllocatorRef allocator, CFUUIDRef typeID )
{
    if( CFEqual(typeID, kIOFireWireAVCLibUnitTypeID) )
        return (void *) alloc();
    else
        return NULL;
}

