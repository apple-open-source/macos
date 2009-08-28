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

#include <stdlib.h>
#include <stdio.h>

#include <IOKit/avc/IOFireWireAVCLib.h>
#include <IOKit/avc/IOFireWireAVCConsts.h>
#include "IOFireWireAVCLibConsumer.h"
#include "IOFireWireAVCUserClientCommon.h"
#include "IOFireWireAVCLibUnit.h"

#include <CoreFoundation/CFMachPort.h>
#include <IOKit/IOMessage.h>

#include <syslog.h>	// Debug messages
#include <pthread.h>	// for mutexes
#include <unistd.h>
#import <sys/mman.h>
#include <mach/mach_port.h>
#include <mach/vm_map.h>

#import <System/libkern/OSCrossEndian.h>

#if 0
#define FWLOG(x) printf x
#else
#define FWLOG(x) do {} while (0)
#endif

__BEGIN_DECLS
#include <IOKit/iokitmig.h>

void *IOFireWireAVCLibUnitFactory( CFAllocatorRef allocator, CFUUIDRef typeID );
__END_DECLS

struct _AVCUnit;
typedef struct _InterfaceMap 
{
    IUnknownVTbl *pseudoVTable;
    struct _AVCUnit *obj;
} InterfaceMap;

//
// UserLib AVCUnit Object
//
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
    CFMachPortRef			fCFAsyncPort;
    mach_port_t				fAsyncPort;
	
    //////////////////////////////////////	
	// async connection objects
	
    CFMutableArrayRef 	fACObjectArray;
	pthread_mutex_t 	fACObjectArrayLock;

	//////////////////////////////////////	
	// AVC async command objects
	
    CFMutableArrayRef 	fAVCAsyncCommandArray;
	pthread_mutex_t 	fAVCAsyncCommandArrayLock;
	
    //////////////////////////////////////	
	// notifications
    
    Boolean					fSuspended;
	Boolean					fHighPerfAVCCommands;

} AVCUnit;

//
// Structure for wrapper of user-lib initiated async AVC commands
//
typedef struct _AVCLibAsynchronousCommandPriv
{
	IOFireWireAVCLibAsynchronousCommand *pCmd;
	IOFireWireAVCLibAsynchronousCommandCallback clientCallback;
	UInt32 kernelAsyncAVCCommandHandle;
	UInt8 *pResponseBuf;
}AVCLibAsynchronousCommandPriv;

// utility function to get "this" pointer from interface
#define AVCUnit_getThis( self ) \
        (((InterfaceMap *) self)->obj)

AVCLibAsynchronousCommandPriv *FindPrivAVCAsyncCommand(AVCUnit *me, IOFireWireAVCLibAsynchronousCommand *pCommandObject);
static IOReturn stop( void * self );
static void removeIODispatcherFromRunLoop( void * self );

//////////////////////////////////////////////////////
// AVCAsyncCommandCallback
//////////////////////////////////////////////////////
static void AVCAsyncCommandCallback( void *refcon, IOReturn result, io_user_reference_t *args, int numArgs)
{
    AVCUnit *me = (AVCUnit*) refcon;
	UInt32 commandIdentifierHandle = (args[0] & 0xFFFFFFFF);
	UInt32 cmdState = (args[1] & 0xFFFFFFFF);
	UInt32 respLen = (args[2] & 0xFFFFFFFF);
	CFIndex count = 0;
	CFIndex i = 0;
	AVCLibAsynchronousCommandPriv *pPrivCmd = NULL;
	bool found = false;
	
	pthread_mutex_lock( &me->fAVCAsyncCommandArrayLock );
	count = CFArrayGetCount( me->fAVCAsyncCommandArray );
	for( i = 0; i < count; i++ )
	{
		pPrivCmd = (AVCLibAsynchronousCommandPriv*) CFArrayGetValueAtIndex( me->fAVCAsyncCommandArray, i);
		if (pPrivCmd->kernelAsyncAVCCommandHandle == commandIdentifierHandle)
		{
			found = true;
			break;
		}
	}
	pthread_mutex_unlock( &me->fAVCAsyncCommandArrayLock );
	
	// If we determined that the command object is valid, process it
	if (found == true)
	{
		// Update the command state
		pPrivCmd->pCmd->cmdState = (IOFWAVCAsyncCommandState) cmdState;
		
		// For response states, set the response buffer
		switch (cmdState)
		{
			case kAVCAsyncCommandStateReceivedInterimResponse:
				pPrivCmd->pCmd->pInterimResponseBuf = &pPrivCmd->pResponseBuf[kAsyncCmdSharedBufInterimRespOffset];
				pPrivCmd->pCmd->interimResponseLen = respLen;
				break;
				
			case kAVCAsyncCommandStateReceivedFinalResponse:
				pPrivCmd->pCmd->pFinalResponseBuf = &pPrivCmd->pResponseBuf[kAsyncCmdSharedBufFinalRespOffset];
				pPrivCmd->pCmd->finalResponseLen = respLen;
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
				break;
		};
		
		// Make the callback to the client
		if (pPrivCmd->clientCallback)
			pPrivCmd->clientCallback(pPrivCmd->pCmd->pRefCon,pPrivCmd->pCmd);
	}
}

//////////////////////////////////////////////////////
// addRef
//////////////////////////////////////////////////////
static UInt32 addRef( void * self )
{
    AVCUnit *me = AVCUnit_getThis(self);
	me->fRefCount++;
	return me->fRefCount;
}

//////////////////////////////////////////////////////
// release
//////////////////////////////////////////////////////
static UInt32 release( void * self )
{
    AVCUnit *me = AVCUnit_getThis(self);
	UInt32 retVal = me->fRefCount;
	AVCLibAsynchronousCommandPriv *pPrivCmd;
	uint32_t outputCnt = 0;
	
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

		pthread_mutex_lock( &me->fAVCAsyncCommandArrayLock );
        if( me->fAVCAsyncCommandArray )
        {
			while(CFArrayGetCount( me->fAVCAsyncCommandArray ))
			{
				pPrivCmd = (AVCLibAsynchronousCommandPriv*) CFArrayGetValueAtIndex( me->fAVCAsyncCommandArray, 0);
				if (pPrivCmd)
				{
					const uint64_t inArg = pPrivCmd->kernelAsyncAVCCommandHandle;
					IOConnectCallScalarMethod(me->fConnection,
											kIOFWAVCUserClientReleaseAsyncAVCCommand,
											&inArg,
											1,NULL,&outputCnt);

					// unmap the 1K response buffer
					if (pPrivCmd->pResponseBuf)
						//munmap( (void*)pPrivCmd->pResponseBuf, 1024 ) ;
						vm_deallocate(mach_task_self(), (vm_address_t) pPrivCmd->pResponseBuf,1024);

					// delete the command byte buffer, and the user command
					if (pPrivCmd->pCmd)
					{
						if (pPrivCmd->pCmd->pCommandBuf)
							delete pPrivCmd->pCmd->pCommandBuf;
						delete pPrivCmd->pCmd; 
					}
					
					// Remove from array
					CFArrayRemoveValueAtIndex(me->fAVCAsyncCommandArray, 0);
					
					// Delete the private command
					delete pPrivCmd;
				}
			}
			
            CFRelease( me->fAVCAsyncCommandArray );
            me->fACObjectArray = NULL;
        }
        pthread_mutex_unlock( &me->fAVCAsyncCommandArrayLock );
        pthread_mutex_destroy( &me->fAVCAsyncCommandArrayLock );
		
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

//////////////////////////////////////////////////////
// queryInterface
//////////////////////////////////////////////////////
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
		// Set flag to throttle AVC Commands
		me->fHighPerfAVCCommands = false;

        *ppv = &me->fIOFireWireAVCLibUnitInterface;
        addRef(self);
    }
	else if( CFEqual(uuid, kIOFireWireAVCLibUnitInterfaceID_v2) )
	{
		// Set flag to not throttle AVC Commands
		me->fHighPerfAVCCommands = true;

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

//////////////////////////////////////////////////////
// messageCallback
//////////////////////////////////////////////////////
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

	if (me->fACObjectArray)
	{
		//
		// put all consumers into a local array to avoid calling callback with lock held
		//

		CFMutableArrayRef array = CFArrayCreateMutable(kCFAllocatorDefault,
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
				IOFireWireAVCLibConsumer * consumer =
				( IOFireWireAVCLibConsumer*)CFArrayGetValueAtIndex( array, i );

				consumer->deviceInterestCallback( messageType, messageArgument );
			}

			CFRelease( array );
		}
	}

	if( me->fMessageCallbackRoutine != NULL )
		(me->fMessageCallbackRoutine)( me->fMessageCallbackRefCon, messageType, messageArgument );
}

//////////////////////////////////////////////////////
// isDeviceSuspended
//////////////////////////////////////////////////////
Boolean isDeviceSuspended( void * self )
{
    AVCUnit * me = AVCUnit_getThis(self);
    
    return me->fSuspended;
}

//////////////////////////////////////////////////////
// probe
//////////////////////////////////////////////////////
static IOReturn probe( void * self, CFDictionaryRef propertyTable, 
											io_service_t service, SInt32 *order )
{
	// only load against AVC Units and SubUnits
    if( !service || !IOObjectConformsTo(service, "IOFireWireAVCNub") )
        return kIOReturnBadArgument;
		
	return kIOReturnSuccess;
}

//////////////////////////////////////////////////////
// start
//////////////////////////////////////////////////////
static IOReturn start( void * self, CFDictionaryRef propertyTable, 
											io_service_t service )
{
	IOReturn status = kIOReturnSuccess;
    AVCUnit *me = AVCUnit_getThis(self);
	uint64_t returnVal;
	uint64_t refrncData[kOSAsyncRef64Count];
	uint32_t outputCnt = 1;
	
	me->fService = service;
    status = IOServiceOpen( me->fService, mach_task_self(), 
							kIOFireWireAVCLibConnection, &me->fConnection );
	if( !me->fConnection )
		status = kIOReturnNoDevice;

	if( status == kIOReturnSuccess )
	{
	    status = IOCreateReceivePort( kOSAsyncCompleteMessageID, &me->fAsyncPort );
	}
	
	// Setup the ref for the kernel user client to use for async AVC command callbacks 
	if( status == kIOReturnSuccess )
	{
		refrncData[kIOAsyncCalloutFuncIndex] = (uint64_t)AVCAsyncCommandCallback;
		refrncData[kIOAsyncCalloutRefconIndex] = (uint64_t)me;
		
		status = IOConnectCallAsyncScalarMethod(me->fConnection,
												kIOFWAVCUserClientInstallAsyncAVCCommandCallback,
												me->fAsyncPort,
												refrncData,kOSAsyncRef64Count,
												NULL,0,
												&returnVal,&outputCnt);
	}
	
	return status;
}

//////////////////////////////////////////////////////
// stop
//////////////////////////////////////////////////////
static IOReturn stop( void * self )
{
    AVCUnit *me = AVCUnit_getThis(self);
	if( me->fConnection ) 
	{
        IOServiceClose( me->fConnection );
        me->fConnection = MACH_PORT_NULL;
    }
	
	if( me->fAsyncPort != MACH_PORT_NULL )
	{
        mach_port_destroy( mach_task_self(), me->fAsyncPort);
		me->fAsyncPort = MACH_PORT_NULL;
	}

	return kIOReturnSuccess;
}

//////////////////////////////////////////////////////////////////
// IOFireWireAVCLibUnit methods
//

//////////////////////////////////////////////////////
// open
//////////////////////////////////////////////////////
static IOReturn open( void * self )
{
    AVCUnit *me = AVCUnit_getThis(self);
	IOReturn status = kIOReturnSuccess;
	uint32_t outputCnt = 0;
	
    if( !me->fConnection )		    
		return kIOReturnNoDevice; 

	status = IOConnectCallScalarMethod(me->fConnection,kIOFWAVCUserClientOpen,NULL,0,NULL,&outputCnt);

	return status;
}

//////////////////////////////////////////////////////
// openWithSessionRef
//////////////////////////////////////////////////////
static IOReturn openWithSessionRef( void * self, IOFireWireSessionRef sessionRef )
{
    AVCUnit *me = AVCUnit_getThis(self);
	IOReturn status = kIOReturnSuccess;
	uint32_t outputCnt = 0;
	
    if( !me->fConnection )		    
		return kIOReturnNoDevice; 
	
	const uint64_t inputs[1]={(const uint64_t)sessionRef};
	status = IOConnectCallScalarMethod(me->fConnection,kIOFWAVCUserClientOpenWithSessionRef,inputs,1,NULL,&outputCnt);
	
	return status;
}

//////////////////////////////////////////////////////
// getSessionRef
//////////////////////////////////////////////////////
static IOFireWireSessionRef getSessionRef(void * self)
{
    AVCUnit *me = AVCUnit_getThis(self);
	IOReturn status = kIOReturnSuccess;
	IOFireWireSessionRef sessionRef = 0;
	uint32_t outputCnt = 1;
	uint64_t outputVal;
	
    if( !me->fConnection )		    
		return sessionRef; 

	status = IOConnectCallScalarMethod(me->fConnection,kIOFWAVCUserClientGetSessionRef,NULL,0,&outputVal,&outputCnt);
	
	if( status != kIOReturnSuccess )
		sessionRef = 0; // just to make sure
	else 
		sessionRef = (IOFireWireSessionRef) outputVal;
	
	return sessionRef;
}

//////////////////////////////////////////////////////
// close
//////////////////////////////////////////////////////
static void close( void * self )
{
    AVCUnit *me = AVCUnit_getThis(self);
	uint32_t outputCnt = 0;

	if( !me->fConnection )		    
        return; 
		
	IOConnectCallScalarMethod(me->fConnection,kIOFWAVCUserClientClose,NULL,0,NULL,&outputCnt);
}

//////////////////////////////////////////////////////
// addIODispatcherToRunLoop
//////////////////////////////////////////////////////
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
    
	if( status == kIOReturnSuccess )
	{
		CFMachPortContext context;
		Boolean	shouldFreeInfo; // zzz what's this for? I think it's set to true if the create failed.
		
		context.version = 1;
		context.info = me;
		context.retain = NULL;
		context.release = NULL;
		context.copyDescription = NULL;
		
		me->fCFAsyncPort = CFMachPortCreateWithPort( kCFAllocatorDefault, me->fAsyncPort, 
													 (CFMachPortCallBack) IODispatchCalloutFromMessage, 
													 &context, &shouldFreeInfo );
		if( !me->fCFAsyncPort )
			status = kIOReturnNoMemory;
	}
	
	if( status == kIOReturnSuccess )
	{
		me->fCFRunLoopSource = CFMachPortCreateRunLoopSource( kCFAllocatorDefault, me->fCFAsyncPort, 0 );
		if( !me->fCFRunLoopSource )
			status = kIOReturnNoMemory;
	}
	
	if( status == kIOReturnSuccess )
	{
		CFRunLoopAddSource(cfRunLoopRef, me->fCFRunLoopSource, kCFRunLoopDefaultMode );
	}
	
	return status;
}

//////////////////////////////////////////////////////
// removeIODispatcherFromRunLoop
//////////////////////////////////////////////////////
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
	
	if(me->fCFRunLoopSource) {
		CFRunLoopRemoveSource( me->fCFRunLoop,
							   me->fCFRunLoopSource, kCFRunLoopDefaultMode );
        CFRelease(me->fCFRunLoopSource);
        me->fCFRunLoopSource = NULL;
    }
	
	if( me->fCFAsyncPort != NULL ) {
        CFMachPortInvalidate(me->fCFAsyncPort);
		CFRelease( me->fCFAsyncPort );
        me->fCFAsyncPort = NULL;
    }
}

//////////////////////////////////////////////////////
// setMessageCallback
//////////////////////////////////////////////////////
static void setMessageCallback( void * self, void * refCon, 
													IOFWAVCMessageCallback callback )
{
    AVCUnit *me = AVCUnit_getThis(self);
	me->fMessageCallbackRoutine = callback;
	me->fMessageCallbackRefCon = refCon;
}

//////////////////////////////////////////////////////
// AVCCommand
//////////////////////////////////////////////////////
static IOReturn AVCCommand(void *self, const UInt8 * command, UInt32 cmdLen, UInt8 * response, UInt32 *responseLen)
{
    AVCUnit *me = AVCUnit_getThis(self);
    IOReturn status;
    size_t outputCnt = *responseLen;
	if( !me->fConnection )		    
        return kIOReturnNotOpen; 
		
	status = IOConnectCallStructMethod(me->fConnection, kIOFWAVCUserClientAVCCommand, command, cmdLen, response,&outputCnt);
		
    if(status == kIOReturnSuccess)
        *responseLen = outputCnt;

	if (me->fHighPerfAVCCommands == false)
	{
		// sleep for 8 milliseconds to throttle back iMovie
		usleep( 8 * 1000 );
	}
	
    return status;
}

//////////////////////////////////////////////////////
// AVCCommandInGeneration
//////////////////////////////////////////////////////
static IOReturn AVCCommandInGeneration(void *self, UInt32 generation,
            const UInt8 * command, UInt32 cmdLen, UInt8 * response, UInt32 *responseLen)
{
    UInt8 annoying[sizeof(UInt32) + 512];
    AVCUnit *me = AVCUnit_getThis(self);
    IOReturn status;
    size_t outputCnt = *responseLen;
	if( !me->fConnection )		    
        return kIOReturnNotOpen; 

	ROSETTA_ONLY(
		{
			generation = OSSwapInt32(generation);
		}
	);
		
    // Have to stick the generation in with the command bytes.
    *(UInt32 *)annoying = generation;
    bcopy(command, annoying+sizeof(UInt32), cmdLen);
 
 	status = IOConnectCallStructMethod(me->fConnection, kIOFWAVCUserClientAVCCommandInGen, annoying, cmdLen+sizeof(UInt32), response,&outputCnt);

	if(status == kIOReturnSuccess)
        *responseLen = outputCnt;

	if (me->fHighPerfAVCCommands == false)
	{
		// sleep for 8 milliseconds to throttle back iMovie
		usleep( 8 * 1000 );
	}
	
    return status;
}

//////////////////////////////////////////////////////
// GetAncestorInterface
//////////////////////////////////////////////////////
static void *GetAncestorInterface( void * self, char * object_class, REFIID pluginType, REFIID iid)
{
    io_registry_entry_t 	parent = NULL;
    io_registry_entry_t 	notTheDesiredParent = NULL;
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
		{
			notTheDesiredParent = parent;
            err = IORegistryEntryGetParentEntry(notTheDesiredParent, kIOServicePlane, &parent);
			IOObjectRelease(notTheDesiredParent);
		}
		
        if(err)
		{
			parent = NULL;
			break;
		}
		
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
	
	if ((!resultInterface) && (parent))
		IOObjectRelease(parent);
        
    return resultInterface;
}

//////////////////////////////////////////////////////
// GetProtocolInterface
//////////////////////////////////////////////////////
static void *GetProtocolInterface( void * self, REFIID pluginType, REFIID iid)
{
    io_registry_entry_t 	parent = NULL;
    io_registry_entry_t 	notTheDesiredParent = NULL;
    io_registry_entry_t 	child = NULL;
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
		{
			notTheDesiredParent = parent;
            err = IORegistryEntryGetParentEntry(notTheDesiredParent, kIOServicePlane, &parent);
			IOObjectRelease(notTheDesiredParent);
		}
		
        if(err)
		{
			parent = NULL;
			break;
		}
		
        // Now search for an IOFireWireLocalNode.
        err = IORegistryEntryGetChildIterator(parent, kIOServicePlane, &iterator );
        if(err)
            break;
        
        while(child = IOIteratorNext(iterator)) {
            if(IOObjectConformsTo(child, "IOFireWireLocalNode"))
                break;
            IOObjectRelease(child);
			child = NULL;
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
	
	if (parent)
		IOObjectRelease(parent);

	if ((!resultInterface) && (child))
		IOObjectRelease(child);

    return resultInterface;
}

//////////////////////////////////////////////////////
// getAsyncConnectionPlugCounts
//////////////////////////////////////////////////////
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

//////////////////////////////////////////////////////
// createConsumerPlug
//////////////////////////////////////////////////////
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

//////////////////////////////////////////////////////
// consumerPlugDestroyed
//////////////////////////////////////////////////////
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

//////////////////////////////////////////////////////
// updateAVCCommandTimeout
//////////////////////////////////////////////////////
static IOReturn updateAVCCommandTimeout( void * self )
{
    AVCUnit *me = AVCUnit_getThis(self);
	uint32_t outputCnt = 0;

	if( !me->fConnection )		    
        return kIOReturnNotOpen; 
		
	return IOConnectCallScalarMethod(me->fConnection,kIOFWAVCUserClientUpdateAVCCommandTimeout,NULL,0,NULL,&outputCnt);
}

//////////////////////////////////////////////////////
// makeP2PInputConnection
//////////////////////////////////////////////////////
static IOReturn makeP2PInputConnection(void * self, UInt32 inputPlug, UInt32 chan)
{
    AVCUnit *me = AVCUnit_getThis(self);
	uint32_t outputCnt = 0;
	const uint64_t inputs[2] = {inputPlug,chan};
	
	return IOConnectCallScalarMethod(me->fConnection,kIOFWAVCUserClientMakeP2PInputConnection,inputs,2,NULL,&outputCnt);

}

//////////////////////////////////////////////////////
// breakP2PInputConnection
//////////////////////////////////////////////////////
static IOReturn breakP2PInputConnection(void * self, UInt32 inputPlug)
{
    AVCUnit *me = AVCUnit_getThis(self);
	uint32_t outputCnt = 0;
	const uint64_t inputs[1]={(const uint64_t)inputPlug};
	return IOConnectCallScalarMethod(me->fConnection,kIOFWAVCUserClientBreakP2PInputConnection,inputs,1,NULL,&outputCnt);
}

//////////////////////////////////////////////////////
// makeP2POutputConnection
//////////////////////////////////////////////////////
static IOReturn makeP2POutputConnection(void * self, UInt32 outputPlug, UInt32 chan, IOFWSpeed speed)
{
    AVCUnit *me = AVCUnit_getThis(self);
	uint32_t outputCnt = 0;
	const uint64_t inputs[3] = {outputPlug,chan,speed};

	return IOConnectCallScalarMethod(me->fConnection,kIOFWAVCUserClientMakeP2POutputConnection,inputs,3,NULL,&outputCnt);
}

//////////////////////////////////////////////////////
// breakP2POutputConnection
//////////////////////////////////////////////////////
static IOReturn breakP2POutputConnection(void * self, UInt32 outputPlug)
{
    AVCUnit *me = AVCUnit_getThis(self);
	uint32_t outputCnt = 0;
	const uint64_t inputs[1]={(const uint64_t)outputPlug};

	return IOConnectCallScalarMethod(me->fConnection,kIOFWAVCUserClientBreakP2POutputConnection,inputs,1,NULL,&outputCnt);
}

//////////////////////////////////////////////////////
// createAVCAsynchronousCommand
//////////////////////////////////////////////////////
static IOReturn createAVCAsynchronousCommand(void * self,
										 const UInt8 * command,
										 UInt32 cmdLen,
										 IOFireWireAVCLibAsynchronousCommandCallback completionCallback,
										 void *pRefCon,
										 IOFireWireAVCLibAsynchronousCommand **ppCommandObject)
{
	AVCUnit *me = AVCUnit_getThis(self);
	AVCLibAsynchronousCommandPriv *pPrivCmd;
    IOReturn status = kIOReturnNoMemory;
	size_t outputCnt = sizeof(UInt32);
	//UInt8 **ppSharedBufAddress;
	mach_vm_address_t *pSharedBufAddress;
	mach_vm_address_t sharedBuf;
	
	// Do some parameter validation
	if(cmdLen == 0 || cmdLen > 512)
        return kIOReturnBadArgument;
	
	do
	{
		// Create a private async command object
		pPrivCmd = new AVCLibAsynchronousCommandPriv;
		if (!pPrivCmd)
			break;
		
		// Create the client async command object
		pPrivCmd->pCmd = new IOFireWireAVCLibAsynchronousCommand;
		if (!pPrivCmd->pCmd)
			break;

		// Create the client command buf, and copy the passed in command bytes
		// Note, add room at the end of this buffer for passing the address of the
		// shared kernel/user response buffer down to the kernel
		//pPrivCmd->pCmd->pCommandBuf = (UInt8*) malloc(cmdLen+sizeof(UInt8*));
		pPrivCmd->pCmd->pCommandBuf = (UInt8*) malloc(cmdLen+sizeof(mach_vm_address_t));
		if (!pPrivCmd->pCmd->pCommandBuf)
			break;

		// Copy the passed in command bytes into the command buffer
		bcopy(command, pPrivCmd->pCmd->pCommandBuf, cmdLen);

		// Create a 1 KByte memory buffer for the kernel/user shared memory
		// The first 512 bytes is the interim response buffer.
		// The second 512 bytes is the final response buffer
		//pPrivCmd->pResponseBuf = (UInt8*) mmap( NULL, 1024, PROT_READ | PROT_WRITE, MAP_ANON, -1, 0 );
		//if ((pPrivCmd->pResponseBuf == (UInt8*) -1 ) || (!pPrivCmd->pResponseBuf))
		vm_allocate(mach_task_self(), (vm_address_t *)&pPrivCmd->pResponseBuf,1024, VM_FLAGS_ANYWHERE);
		if (!pPrivCmd->pResponseBuf)
			break;
			
		// Put the address of the response buffer into the array of bytes we will send to the kernel
		pSharedBufAddress = (mach_vm_address_t *) &(pPrivCmd->pCmd->pCommandBuf[cmdLen]);
		sharedBuf = (mach_vm_address_t) pPrivCmd->pResponseBuf;
		*pSharedBufAddress = sharedBuf;
		
		ROSETTA_ONLY(
			{
				*pSharedBufAddress = (mach_vm_address_t) OSSwapInt64(sharedBuf);
			}
		);
		
		// Initialize the command object
		pPrivCmd->pCmd->cmdLen = cmdLen;
		pPrivCmd->pCmd->cmdState = kAVCAsyncCommandStatePendingRequest;
		pPrivCmd->pCmd->pRefCon = pRefCon;
		pPrivCmd->pCmd->pInterimResponseBuf = nil;
		pPrivCmd->pCmd->interimResponseLen = 0;
		pPrivCmd->pCmd->pFinalResponseBuf = nil;
		pPrivCmd->pCmd->finalResponseLen = 0;
		pPrivCmd->clientCallback = completionCallback;

		// Have the user-client create a in-kernel AVC async command object
		status = IOConnectCallStructMethod(me->fConnection,
										kIOFWAVCUserClientCreateAsyncAVCCommand,
										pPrivCmd->pCmd->pCommandBuf,
										cmdLen+sizeof(mach_vm_address_t),
										&(pPrivCmd->kernelAsyncAVCCommandHandle),
										&outputCnt);

		ROSETTA_ONLY(
			{
				pPrivCmd->kernelAsyncAVCCommandHandle = OSSwapInt32(pPrivCmd->kernelAsyncAVCCommandHandle);
			}
		);
		
		if (status != kIOReturnSuccess)
			*ppCommandObject = nil;
		else
			*ppCommandObject = pPrivCmd->pCmd;
	}while(0);
	
	// If success, add this command to our array, or, if something went wrong, clean up the allocated memory
	if (status == kIOReturnSuccess)
	{
		pthread_mutex_lock( &me->fAVCAsyncCommandArrayLock );
        CFArrayAppendValue(me->fAVCAsyncCommandArray, pPrivCmd);
        pthread_mutex_unlock( &me->fAVCAsyncCommandArrayLock );
	}
	else
	{
		if (pPrivCmd)
		{
			if (pPrivCmd->pResponseBuf)
				//munmap( (void*)pPrivCmd->pResponseBuf, 1024 ) ;
				vm_deallocate(mach_task_self(), (vm_address_t) pPrivCmd->pResponseBuf,1024);

			if (pPrivCmd->pCmd)
			{
				if (pPrivCmd->pCmd->pCommandBuf)
					delete pPrivCmd->pCmd->pCommandBuf;
				delete pPrivCmd->pCmd; 
			}
		
			delete pPrivCmd;
		}
	}

	return status;
}

//////////////////////////////////////////////////////
// AVCAsynchronousCommandSubmit
//////////////////////////////////////////////////////
static IOReturn AVCAsynchronousCommandSubmit(void * self, IOFireWireAVCLibAsynchronousCommand *pCommandObject)
{
	AVCUnit *me = AVCUnit_getThis(self);
	AVCLibAsynchronousCommandPriv *pPrivCmd;
	IOReturn res = kIOReturnBadArgument;
	uint32_t outputCnt = 0;

	// Look up this command to see if it is valid
	pPrivCmd = FindPrivAVCAsyncCommand(me,pCommandObject);
	
	// If we determined that the command object is valid, release it
	if (pPrivCmd)
	{
		if (pPrivCmd->pCmd->cmdState != kAVCAsyncCommandStatePendingRequest)
		{
			res = kIOReturnNotPermitted;
		}
		else
		{
			const uint64_t inArg = pPrivCmd->kernelAsyncAVCCommandHandle;
			res =  IOConnectCallScalarMethod(me->fConnection,
										kIOFWAVCUserClientSubmitAsyncAVCCommand,
										&inArg,
										1,NULL,&outputCnt);
			if (res == kIOReturnSuccess)
			{
				// We need to check the command state here, because the callback may have already happened
				if (pPrivCmd->pCmd->cmdState == kAVCAsyncCommandStatePendingRequest)
					pPrivCmd->pCmd->cmdState = kAVCAsyncCommandStateRequestSent;
			}
			else
			{
				pPrivCmd->pCmd->cmdState = kAVCAsyncCommandStateRequestFailed;
			}
		}
	}
	
	return res;
}

//////////////////////////////////////////////////////
// AVCAsynchronousCommandReinit
//////////////////////////////////////////////////////
static IOReturn AVCAsynchronousCommandReinit(void * self, IOFireWireAVCLibAsynchronousCommand *pCommandObject)
{
	AVCUnit *me = AVCUnit_getThis(self);
	AVCLibAsynchronousCommandPriv *pPrivCmd;
	IOReturn res = kIOReturnBadArgument;
	uint32_t outputCnt = 0;
	size_t outputStructCnt = 0;
	
	
	// Look up this command to see if it is valid
	pPrivCmd = FindPrivAVCAsyncCommand(me,pCommandObject);
	
	// If we determined that the command object is valid, reinit it
	if (pPrivCmd)
	{
		// Don't allow if the command is in one of these "pending" states
		if ( (pPrivCmd->pCmd->cmdState == kAVCAsyncCommandStateRequestSent) || 
			 (pPrivCmd->pCmd->cmdState == kAVCAsyncCommandStateWaitingForResponse) || 
			 (pPrivCmd->pCmd->cmdState == kAVCAsyncCommandStateReceivedInterimResponse) )
		{
			res = kIOReturnNotPermitted;
		}
		else
		{
			const uint64_t inArg = pPrivCmd->kernelAsyncAVCCommandHandle;
			res = IOConnectCallMethod(me->fConnection, 
									kIOFWAVCUserClientReinitAsyncAVCCommand,
									&inArg,
									1,
									pPrivCmd->pCmd->pCommandBuf,
									pPrivCmd->pCmd->cmdLen,
									NULL,
									&outputCnt,
									NULL,
									&outputStructCnt);
				
			if (res == kIOReturnSuccess)
			{
				// Update the user parts of the lib command object
				pPrivCmd->pCmd->cmdState = kAVCAsyncCommandStatePendingRequest;
				pPrivCmd->pCmd->pInterimResponseBuf = nil;
				pPrivCmd->pCmd->interimResponseLen = 0;
				pPrivCmd->pCmd->pFinalResponseBuf = nil;
				pPrivCmd->pCmd->finalResponseLen = 0;
			}
		}
	}
	
	return res;
}

//////////////////////////////////////////////////////
// AVCAsynchronousCommandReinitWithCommandBytes
//////////////////////////////////////////////////////
static IOReturn AVCAsynchronousCommandReinitWithCommandBytes(void * self, 
															 IOFireWireAVCLibAsynchronousCommand *pCommandObject, 
															 const UInt8 * command,
															 UInt32 cmdLen)
{
	AVCUnit *me = AVCUnit_getThis(self);
	AVCLibAsynchronousCommandPriv *pPrivCmd;
	IOReturn res = kIOReturnBadArgument;
	UInt8 *pNewCommandBuf;
	uint32_t outputCnt = 0;
	size_t outputStructCnt = 0;
	
	// Do some parameter validation
	if(cmdLen == 0 || cmdLen > 512)
        return kIOReturnBadArgument;
	
	// Look up this command to see if it is valid
	pPrivCmd = FindPrivAVCAsyncCommand(me,pCommandObject);
	
	// If we determined that the command object is valid
	if (pPrivCmd)
	{
		// Malloc space for the new command buffer
		pNewCommandBuf = (UInt8*) malloc(cmdLen);
		if (pNewCommandBuf)
		{
			res = kIOReturnBadArgument;
		}
		else
		{
			// Don't allow if the command is in one of these "pending" states
			if ( (pPrivCmd->pCmd->cmdState == kAVCAsyncCommandStateRequestSent) || 
				 (pPrivCmd->pCmd->cmdState == kAVCAsyncCommandStateWaitingForResponse) || 
				 (pPrivCmd->pCmd->cmdState == kAVCAsyncCommandStateReceivedInterimResponse) )
			{
				delete pNewCommandBuf;	// We won't be needing this after all!
				res = kIOReturnNotPermitted;
			}
			else
			{
				// Save a copy of the new command bytes
				delete pPrivCmd->pCmd->pCommandBuf;
				pPrivCmd->pCmd->pCommandBuf = pNewCommandBuf;
				bcopy(command, pPrivCmd->pCmd->pCommandBuf, cmdLen);
				
				const uint64_t inArg = pPrivCmd->kernelAsyncAVCCommandHandle;
				res = IOConnectCallMethod(me->fConnection, 
										kIOFWAVCUserClientReinitAsyncAVCCommand,
										&inArg,
										1,
										pPrivCmd->pCmd->pCommandBuf,
										pPrivCmd->pCmd->cmdLen,
										NULL,
										&outputCnt,
										NULL,
										&outputStructCnt);

				if (res == kIOReturnSuccess)
				{
					// Update the user parts of the lib command object
					pPrivCmd->pCmd->cmdState = kAVCAsyncCommandStatePendingRequest;
					pPrivCmd->pCmd->pInterimResponseBuf = nil;
					pPrivCmd->pCmd->interimResponseLen = 0;
					pPrivCmd->pCmd->pFinalResponseBuf = nil;
					pPrivCmd->pCmd->finalResponseLen = 0;
				}
			}
		}
	}
	
	return res;
}

//////////////////////////////////////////////////////
// AVCAsynchronousCommandCancel
//////////////////////////////////////////////////////
static IOReturn AVCAsynchronousCommandCancel(void * self, IOFireWireAVCLibAsynchronousCommand *pCommandObject)
{
	AVCUnit *me = AVCUnit_getThis(self);
	AVCLibAsynchronousCommandPriv *pPrivCmd;
	IOReturn res = kIOReturnBadArgument;
	uint32_t outputCnt = 0;
	
	// Look up this command to see if it is valid
	pPrivCmd = FindPrivAVCAsyncCommand(me,pCommandObject);
	
	// If we determined that the command object is valid, release it
	if (pPrivCmd)
	{
		const uint64_t inArg = pPrivCmd->kernelAsyncAVCCommandHandle;
		res =  IOConnectCallScalarMethod(me->fConnection,
									kIOFWAVCUserClientCancelAsyncAVCCommand,
									&inArg,
									1,NULL,&outputCnt);
		
		pPrivCmd->pCmd->cmdState = kAVCAsyncCommandStateCanceled;
	}
	
	return res;
}

//////////////////////////////////////////////////////
// AVCAsynchronousCommandRelease
//////////////////////////////////////////////////////
static IOReturn AVCAsynchronousCommandRelease(void * self, IOFireWireAVCLibAsynchronousCommand *pCommandObject)
{
	AVCUnit *me = AVCUnit_getThis(self);
	AVCLibAsynchronousCommandPriv *pPrivCmd;
	IOReturn res = kIOReturnBadArgument;
	CFIndex count = 0;
	CFIndex i = 0;
	bool found = false;
	uint32_t outputCnt = 0;
	
	// First, see if this is a valid command object passed in by the client
	pthread_mutex_lock( &me->fAVCAsyncCommandArrayLock );
	count = CFArrayGetCount( me->fAVCAsyncCommandArray );
	for( i = 0; i < count; i++ )
	{
		pPrivCmd = (AVCLibAsynchronousCommandPriv*) CFArrayGetValueAtIndex( me->fAVCAsyncCommandArray, i);
		if (pCommandObject == pPrivCmd->pCmd)
		{
			found = true;
			break;
		}
	}
	
	// If we determined that the command object is valid, cancel it
	if (found == true)
	{
		const uint64_t inArg = pPrivCmd->kernelAsyncAVCCommandHandle;
		IOConnectCallScalarMethod(me->fConnection,
								kIOFWAVCUserClientReleaseAsyncAVCCommand,
								&inArg,
								1,NULL,&outputCnt);
		
		// unmap the 1K response buffer
		if (pPrivCmd->pResponseBuf)
			//munmap( (void*)pPrivCmd->pResponseBuf, 1024 ) ;
			vm_deallocate(mach_task_self(), (vm_address_t) pPrivCmd->pResponseBuf,1024);

		// delete the command byte buffer, and the user command
		if (pPrivCmd->pCmd)
		{
			if (pPrivCmd->pCmd->pCommandBuf)
				delete pPrivCmd->pCmd->pCommandBuf;
			delete pPrivCmd->pCmd; 
		}

		// Remove from array
		CFArrayRemoveValueAtIndex(me->fAVCAsyncCommandArray, i);
		
		// Delete the private command
		delete pPrivCmd;
		
		res = kIOReturnSuccess;
	}

	pthread_mutex_unlock( &me->fAVCAsyncCommandArrayLock );

	return res;
}

//////////////////////////////////////////////////////
// FindPrivAVCAsyncCommand
//////////////////////////////////////////////////////
AVCLibAsynchronousCommandPriv *FindPrivAVCAsyncCommand(AVCUnit *me, IOFireWireAVCLibAsynchronousCommand *pCommandObject)
{
	CFIndex count = 0;
	CFIndex i = 0;
	AVCLibAsynchronousCommandPriv *pPrivCmd = NULL;
	bool found = false;

	// First, see if this is a valid command object passed in by the client
	pthread_mutex_lock( &me->fAVCAsyncCommandArrayLock );
	count = CFArrayGetCount( me->fAVCAsyncCommandArray );
	for( i = 0; i < count; i++ )
	{
		pPrivCmd = (AVCLibAsynchronousCommandPriv*) CFArrayGetValueAtIndex( me->fAVCAsyncCommandArray, i);
		if (pCommandObject == pPrivCmd->pCmd)
		{
			found = true;
			break;
		}
	}
	pthread_mutex_unlock( &me->fAVCAsyncCommandArrayLock );
	
	// If we determined that the command object is valid, cancel it
	if (found == true)
		return pPrivCmd;
	else
		return nil;
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
	4, 0, // version/revision
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
    &updateAVCCommandTimeout,
    &makeP2PInputConnection,
    &breakP2PInputConnection,
    &makeP2POutputConnection,
    &breakP2POutputConnection,
	&createAVCAsynchronousCommand,
	&AVCAsynchronousCommandSubmit,
	&AVCAsynchronousCommandReinit,
	&AVCAsynchronousCommandCancel,
	&AVCAsynchronousCommandRelease,
	&AVCAsynchronousCommandReinitWithCommandBytes
};

// IOFireWireAVCLibUnitFactory

//////////////////////////////////////////////////////
// alloc
// static allocator, called by factory method
//////////////////////////////////////////////////////
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
		me->fHighPerfAVCCommands = false;
	
      	pthread_mutex_init( &me->fACObjectArrayLock, NULL );
	    me->fACObjectArray = CFArrayCreateMutable(	kCFAllocatorDefault, 
                                                    2, // capacity 
                                                    IOFireWireAVCLibConsumer::getCFArrayCallbacks() );

		// Create the array to hold avc async commands, and the lock to protect it
      	pthread_mutex_init( &me->fAVCAsyncCommandArrayLock, NULL );
		me->fAVCAsyncCommandArray = CFArrayCreateMutable(kCFAllocatorDefault, 
															 0, // capacity 
															 NULL);
		
        me->fFactoryId = kIOFireWireAVCLibUnitFactoryID;
        CFRetain( me->fFactoryId );
        CFPlugInAddInstanceForFactory( me->fFactoryId );
        interface = (IOCFPlugInInterface **) &me->fIOCFPlugInInterface.pseudoVTable;
    }
	
	return interface;
}

//////////////////////////////////////////////////////
// IOFireWireAVCLibUnitFactory
// factory method (only exported symbol)
//////////////////////////////////////////////////////
void *IOFireWireAVCLibUnitFactory( CFAllocatorRef allocator, CFUUIDRef typeID )
{
    if( CFEqual(typeID, kIOFireWireAVCLibUnitTypeID) )
        return (void *) alloc();
    else
        return NULL;
}

