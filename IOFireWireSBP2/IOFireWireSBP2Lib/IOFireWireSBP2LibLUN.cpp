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

#include <Carbon/Carbon.h>  // for printf

#include "FWDebugging.h"
#include "IOFireWireSBP2LibLUN.h"
#include "IOFireWireSBP2LibLogin.h"
#include "IOFireWireSBP2LibMgmtORB.h"
#include "IOFireWireSBP2UserClientCommon.h"

__BEGIN_DECLS
#include <IOKit/iokitmig.h>
#include <mach/mach.h>
__END_DECLS

//
// static interface table for IOCFPlugInInterface
//

IOCFPlugInInterface IOFireWireSBP2LibLUN::sIOCFPlugInInterface = 
{
    0,
	&IOFireWireSBP2LibLUN::staticQueryInterface,
	&IOFireWireSBP2LibLUN::staticAddRef,
	&IOFireWireSBP2LibLUN::staticRelease,
	1, 0, // version/revision
	&IOFireWireSBP2LibLUN::staticProbe,
	&IOFireWireSBP2LibLUN::staticStart,
	&IOFireWireSBP2LibLUN::staticStop
};

//
// static interface table for IOFireWireSBP2LibLUNInterface
//

IOFireWireSBP2LibLUNInterface IOFireWireSBP2LibLUN::sIOFireWireSBP2LibLUNInterface =
{
    0,
	&IOFireWireSBP2LibLUN::staticQueryInterface,
	&IOFireWireSBP2LibLUN::staticAddRef,
	&IOFireWireSBP2LibLUN::staticRelease,
	1, 0, // version/revision
	&IOFireWireSBP2LibLUN::staticOpen,
	&IOFireWireSBP2LibLUN::staticOpenWithSessionRef,
	&IOFireWireSBP2LibLUN::staticGetSessionRef,
	&IOFireWireSBP2LibLUN::staticClose,
	&IOFireWireSBP2LibLUN::staticAddIODispatcherToRunLoop,
	&IOFireWireSBP2LibLUN::staticRemoveIODispatcherFromRunLoop,
	&IOFireWireSBP2LibLUN::staticSetMessageCallback,
	&IOFireWireSBP2LibLUN::staticSetRefCon,
	&IOFireWireSBP2LibLUN::staticGetRefCon,
	&IOFireWireSBP2LibLUN::staticCreateLogin,
	&IOFireWireSBP2LibLUN::staticCreateMgmtORB,
};

// IOFireWireSBP2LibFactory
//
// factory method

void *IOFireWireSBP2LibFactory( CFAllocatorRef allocator, CFUUIDRef typeID )
{
	FWLOG(( "IOFireWireSBP2LibFactory called\n" ));
	
    if( CFEqual(typeID, kIOFireWireSBP2LibTypeID) )
        return (void *) IOFireWireSBP2LibLUN::alloc();
    else
        return NULL;
}

// alloc
//
// static allocator, called by factory method

IOCFPlugInInterface ** IOFireWireSBP2LibLUN::alloc()
{
    IOFireWireSBP2LibLUN *	me;
	IOCFPlugInInterface ** 	interface = NULL;
	
    me = new IOFireWireSBP2LibLUN;
    if( me )
	{
		// we return an interface here. queryInterface will not be called. call addRef here
		me->addRef();
        interface = (IOCFPlugInInterface **) &me->fIOCFPlugInInterface.pseudoVTable;
    }
	
	return interface;
}

// ctor
//
//

IOFireWireSBP2LibLUN::IOFireWireSBP2LibLUN( void )
{
	// init cf plugin ref counting
	fRefCount = 0;
	
	// init user client connection
	fConnection = MACH_PORT_NULL;
	fService = MACH_PORT_NULL;
	
	// init async callbacks
	fAsyncPort = MACH_PORT_NULL;
	fCFAsyncPort = NULL;
	fMessageCallbackRoutine = NULL;
	fMessageCallbackRefCon = NULL;
	
	// create plugin interface map
    fIOCFPlugInInterface.pseudoVTable = (IUnknownVTbl *) &sIOCFPlugInInterface;
    fIOCFPlugInInterface.obj = this;
	
	// create test driver interface map
	fIOFireWireSBP2LibLUNInterface.pseudoVTable 
								= (IUnknownVTbl *) &sIOFireWireSBP2LibLUNInterface;
	fIOFireWireSBP2LibLUNInterface.obj = this;
	
	fFactoryId = kIOFireWireSBP2LibFactoryID;
	CFRetain( fFactoryId );
	CFPlugInAddInstanceForFactory( fFactoryId );
}

// dtor
//
//

IOFireWireSBP2LibLUN::~IOFireWireSBP2LibLUN()
{
	CFPlugInRemoveInstanceForFactory( fFactoryId );
	CFRelease( fFactoryId );
}

//////////////////////////////////////////////////////////////////
// IUnknown methods
//

// queryInterface
//
//

HRESULT IOFireWireSBP2LibLUN::staticQueryInterface( void * self, REFIID iid, void **ppv )
{
	return getThis(self)->queryInterface( iid, ppv );
}

HRESULT IOFireWireSBP2LibLUN::queryInterface( REFIID iid, void **ppv )
{
    CFUUIDRef uuid = CFUUIDCreateFromUUIDBytes(NULL, iid);
    HRESULT result = S_OK;

	if( CFEqual(uuid, IUnknownUUID) ||  CFEqual(uuid, kIOCFPlugInInterfaceID) ) 
	{
        *ppv = &fIOCFPlugInInterface;
        addRef();
    }
	else if( CFEqual(uuid, kIOFireWireSBP2LibLUNInterfaceID) ) 
	{
        *ppv = &fIOFireWireSBP2LibLUNInterface;
        addRef();
    }
    else
        *ppv = 0;

    if( !*ppv )
        result = E_NOINTERFACE;

    CFRelease( uuid );
	
    return result;
}

// addRef
//
//

UInt32 IOFireWireSBP2LibLUN::staticAddRef( void * self )
{
	return getThis(self)->addRef();
}

UInt32 IOFireWireSBP2LibLUN::addRef()
{
    fRefCount += 1;
    return fRefCount;
}

// release
//
//

UInt32 IOFireWireSBP2LibLUN::staticRelease( void * self )
{
	return getThis(self)->release();
}

UInt32 IOFireWireSBP2LibLUN::release( void )
{
	UInt32 retVal = fRefCount;
	
	if( 1 == fRefCount-- ) 
	{
		delete this;
    }
    else if( fRefCount < 0 )
	{
        fRefCount = 0;
	}
	
	return retVal;
}

//////////////////////////////////////////////////////////////////
// IOCFPlugInInterface methods
//

// probe
//
//

IOReturn IOFireWireSBP2LibLUN::staticProbe( void * self, CFDictionaryRef propertyTable, 
											io_service_t service, SInt32 *order )
{
	return getThis(self)->probe( propertyTable, service, order );
}

IOReturn IOFireWireSBP2LibLUN::probe( CFDictionaryRef propertyTable, 
									  io_service_t service, SInt32 *order )
{
	// only load against LUN's
    if( !service || !IOObjectConformsTo(service, "IOFireWireSBP2LUN") )
        return kIOReturnBadArgument;
		
	return kIOReturnSuccess;
}

// start
//
//

IOReturn IOFireWireSBP2LibLUN::staticStart( void * self, CFDictionaryRef propertyTable, 
											io_service_t service )
{
	return getThis(self)->start( propertyTable, service );
}

IOReturn IOFireWireSBP2LibLUN::start( CFDictionaryRef propertyTable, io_service_t service )
{
	IOReturn status = kIOReturnSuccess;
	
	FWLOG(( "IOFireWireSBP2LibLUN : start\n" ));

	fService = service;
    status = IOServiceOpen( fService, mach_task_self(), 
							kIOFireWireSBP2LibConnection, &fConnection );
	if( !fConnection )
		status = kIOReturnNoDevice;

	if( status == kIOReturnSuccess )
	{
	    status = IOCreateReceivePort( kOSAsyncCompleteMessageID, &fAsyncPort );
	}

	FWLOG(( "IOFireWireSBP2LibLUN : IOServiceOpen status = 0x%08lx, connection = %d\n", (UInt32) status, fConnection ));
		
	return status;
}

// stop
//
//

IOReturn IOFireWireSBP2LibLUN::staticStop( void * self )
{
	return getThis(self)->stop();
}

IOReturn IOFireWireSBP2LibLUN::stop( void )
{
	FWLOG(( "IOFireWireSBP2LibLUN : stop\n" ));
	
	removeIODispatcherFromRunLoop();
	
	if( fConnection ) 
	{
		FWLOG(( "IOFireWireSBP2LibLUN : IOServiceClose connection = %d\n", fConnection ));
        IOServiceClose( fConnection );
        fConnection = MACH_PORT_NULL;
    }
	
	if( fAsyncPort != MACH_PORT_NULL )
	{
		FWLOG(( "IOFireWireSBP2LibLUN : release fAsyncPort\n" ));
		mach_port_destroy( mach_task_self(), fAsyncPort );
		fAsyncPort = MACH_PORT_NULL;
	}
	
	return kIOReturnSuccess;
}

//////////////////////////////////////////////////////////////////
// IOFireWireSBP2LUN methods
//

// open
//
//

IOReturn IOFireWireSBP2LibLUN::staticOpen( void * self )
{
	return getThis(self)->open();
}

IOReturn IOFireWireSBP2LibLUN::open( void )
{
	IOReturn status = kIOReturnSuccess;
	
    if( !fConnection )		    
		return kIOReturnNoDevice; 

	FWLOG(( "IOFireWireSBP2LUN : open\n" ));

    mach_msg_type_number_t len = 0;
    status = io_connect_method_scalarI_scalarO( fConnection, kIOFWSBP2UserClientOpen, 
												NULL, 0, NULL, &len );	
	return status;
}

// openWithSessionRef
//
//

IOReturn IOFireWireSBP2LibLUN::staticOpenWithSessionRef( void * self, IOFireWireSessionRef sessionRef )
{
	return getThis(self)->openWithSessionRef( sessionRef );
}

IOReturn IOFireWireSBP2LibLUN::openWithSessionRef( IOFireWireSessionRef sessionRef )
{
		IOReturn status = kIOReturnSuccess;
	
    if( !fConnection )		    
		return kIOReturnNoDevice; 

	FWLOG(( "IOFireWireSBP2LUN : openWithSessionRef\n" ));

    mach_msg_type_number_t len = 0;
    status = io_connect_method_scalarI_scalarO( fConnection, kIOFWSBP2UserClientOpenWithSessionRef, 
												(int*)&sessionRef, 1, NULL, &len );
	
	return status;
}

// getSessionRef
//
//

IOFireWireSessionRef IOFireWireSBP2LibLUN::staticGetSessionRef(void * self)
{
	return getThis(self)->getSessionRef();
}

IOFireWireSessionRef IOFireWireSBP2LibLUN::getSessionRef( void )
{
	IOReturn status = kIOReturnSuccess;
	IOFireWireSessionRef sessionRef = 0;
	
    if( !fConnection )		    
		return sessionRef; 

	FWLOG(( "IOFireWireSBP2LUN : getSessionRef\n" ));

    mach_msg_type_number_t len = 1;
    status = io_connect_method_scalarI_scalarO( fConnection, kIOFWSBP2UserClientGetSessionRef, 
												NULL, 0, (int*)&sessionRef, &len );	

	if( status != kIOReturnSuccess )
		sessionRef = 0; // just to make sure

	return sessionRef;
}

// close
//
//

void IOFireWireSBP2LibLUN::staticClose( void * self )
{
	getThis(self)->close();
}

void IOFireWireSBP2LibLUN::close( void )
{
   if( !fConnection )		    
		return; 
		
	FWLOG(( "IOFireWireSBP2LUN : close\n" ));

    mach_msg_type_number_t len = 0;
	io_connect_method_scalarI_scalarO( fConnection, kIOFWSBP2UserClientClose, 
									   NULL, 0, NULL, &len );	

}

// addIODispatcherToRunLoop
//
//

IOReturn IOFireWireSBP2LibLUN::staticAddIODispatcherToRunLoop( void *self, 
												CFRunLoopRef cfRunLoopRef )
{
	return getThis(self)->addIODispatcherToRunLoop( cfRunLoopRef );
}

IOReturn IOFireWireSBP2LibLUN::addIODispatcherToRunLoop( CFRunLoopRef cfRunLoopRef )
{
	IOReturn 				status = kIOReturnSuccess;

	FWLOG(( "IOFireWireSBP2LibLUN : addIODispatcherToRunLoop\n" ));
	
	if( !fConnection )		    
		return kIOReturnNoDevice; 

	if( status == kIOReturnSuccess )
	{
		CFMachPortContext context;
		Boolean	shouldFreeInfo; // zzz what's this for?
	
		context.version = 1;
		context.info = this;
		context.retain = NULL;
		context.release = NULL;
		context.copyDescription = NULL;

		fCFAsyncPort = CFMachPortCreateWithPort( kCFAllocatorDefault, fAsyncPort, 
							(CFMachPortCallBack) IODispatchCalloutFromMessage, 
							&context, &shouldFreeInfo );
		if( !fCFAsyncPort )
			status = kIOReturnNoMemory;
	}
		
	if( status == kIOReturnSuccess )
	{
		fCFRunLoopSource = CFMachPortCreateRunLoopSource( kCFAllocatorDefault, fCFAsyncPort, 0 );
		if( !fCFRunLoopSource )
			status = kIOReturnNoMemory;
	}
		
	if( status == kIOReturnSuccess )
	{
		fCFRunLoop = cfRunLoopRef;
		CFRunLoopAddSource( fCFRunLoop, fCFRunLoopSource, kCFRunLoopCommonModes );
	}

	if( status == kIOReturnSuccess )
	{
		io_async_ref_t 			asyncRef;
		mach_msg_type_number_t	size = 0;
		io_scalar_inband_t		params;
		
		asyncRef[0] = 0;
		params[0]	= (UInt32)this;
		params[1]	= (UInt32)(IOAsyncCallback1)&IOFireWireSBP2LibLUN::staticMessageCallback;
	
		status = io_async_method_scalarI_scalarO( fConnection, fAsyncPort, 
												  asyncRef, 1, 
												  kIOFWSBP2UserClientSetMessageCallback,
												  params, 2,
												  NULL, &size );	
	}
	
	return status;
}

// removeIODispatcherFromRunLoop
//
//

void IOFireWireSBP2LibLUN::staticRemoveIODispatcherFromRunLoop( void * self )
{
	return getThis(self)->removeIODispatcherFromRunLoop();
}

void IOFireWireSBP2LibLUN::removeIODispatcherFromRunLoop( void )
{
	if( fCFRunLoopSource != NULL )
	{
		CFRunLoopRemoveSource( fCFRunLoop, fCFRunLoopSource, kCFRunLoopCommonModes );
		CFRelease( fCFRunLoopSource );
		fCFRunLoopSource = NULL;
	}

	if( fCFAsyncPort != NULL )
	{
		FWLOG(( "IOFireWireSBP2LibLUN : release fCFAsyncPort\n" ));
		CFMachPortInvalidate( fCFAsyncPort );
		CFRelease( fCFAsyncPort );
		fCFAsyncPort = NULL;
	}
}

// setMessageCallback
//
//

void IOFireWireSBP2LibLUN::staticSetMessageCallback( void * self, void * refCon, 
													IOFWSBP2MessageCallback callback )
{
	getThis(self)->setMessageCallback( refCon, callback );
}

void IOFireWireSBP2LibLUN::setMessageCallback( void * refCon, 
													IOFWSBP2MessageCallback callback )
{
	fMessageCallbackRoutine = callback;
	fMessageCallbackRefCon = refCon;
}

// createLogin
//
// create a login object

IUnknownVTbl ** IOFireWireSBP2LibLUN::staticCreateLogin( void * self, REFIID iid )
{
	return getThis(self)->createLogin( iid );
}

IUnknownVTbl ** IOFireWireSBP2LibLUN::createLogin( REFIID iid )
{
	IOReturn status = kIOReturnSuccess;
	IUnknownVTbl ** iunknown = NULL;

	if( !fConnection )
		status = kIOReturnError;

	if( fAsyncPort == MACH_PORT_NULL )
		status = kIOReturnError;
		
	if( status == kIOReturnSuccess )
	{
		iunknown = IOFireWireSBP2LibLogin::alloc( fConnection, fAsyncPort );
		if( iunknown == NULL )
			status = kIOReturnNoMemory;
	}
	
	if( status == kIOReturnSuccess )
	{	
		HRESULT res;
		res = (*iunknown)->QueryInterface( iunknown, iid,
										   (void **) &fLoginInterface );
		
		if( res != S_OK )
			status = kIOReturnError;
	}
	
	if( iunknown != NULL )
	{
		(*iunknown)->Release(iunknown);
	}

	if( status == kIOReturnSuccess )
		return fLoginInterface;
	else
		return NULL;
}
	
// setRefCon
//
//

void IOFireWireSBP2LibLUN::staticSetRefCon( void * self, UInt32 refCon )
{
	getThis(self)->setRefCon( refCon );
}

void IOFireWireSBP2LibLUN::setRefCon( UInt32 refCon )
{
	fRefCon = refCon;
}

// getRefCon
//
//

UInt32 IOFireWireSBP2LibLUN::staticGetRefCon( void * self )
{
	return getThis(self)->getRefCon();
}

UInt32 IOFireWireSBP2LibLUN::getRefCon( void )
{
	return fRefCon;
}	

// createMgmtORB
//
//

IUnknownVTbl ** IOFireWireSBP2LibLUN::staticCreateMgmtORB( void * self, REFIID iid )
{
	return getThis(self)->createMgmtORB( iid );
}

IUnknownVTbl ** IOFireWireSBP2LibLUN::createMgmtORB( REFIID iid )
{
	IOReturn status = kIOReturnSuccess;
	IUnknownVTbl ** iunknown = NULL;
	IUnknownVTbl ** mgmtORB = NULL;

	if( !fConnection )
		status = kIOReturnError;

	if( fAsyncPort == MACH_PORT_NULL )
		status = kIOReturnError;

	if( status == kIOReturnSuccess )
	{
		iunknown = IOFireWireSBP2LibMgmtORB::alloc( fConnection, fAsyncPort );
		if( iunknown == NULL )
			status = kIOReturnNoMemory;
	}
	
	if( status == kIOReturnSuccess )
	{	
		HRESULT res;
		res = (*iunknown)->QueryInterface( iunknown, iid,
										   (void **) &mgmtORB );
		
		if( res != S_OK )
			status = kIOReturnError;
	}
		
	if( iunknown != NULL )
	{
		(*iunknown)->Release(iunknown);
	}

	if( status == kIOReturnSuccess )
		return mgmtORB;
	else
		return NULL;
}


//////////////////////////////////////////////////////////////////
// callback static methods
//

// messageCallback
//
//

void IOFireWireSBP2LibLUN::staticMessageCallback( void *refcon, IOReturn result, 
													void **args, int numArgs )
{
	((IOFireWireSBP2LibLUN*)refcon)->messageCallback( result, args, numArgs );
}

void IOFireWireSBP2LibLUN::messageCallback( IOReturn result, void **args, int numArgs )
{
	FWLOG(( "IOFireWireSBP2LibLUN : messageCallback numArgs = %d\n", numArgs ));

	FWSBP2ReconnectParams params;
	void * outArgs = NULL;

	UInt32 type = (UInt32)args[11];
	if( type == kIOMessageFWSBP2ReconnectComplete ||
		type == kIOMessageFWSBP2ReconnectFailed )
	{
		UInt32 statusBlock[8];
		bcopy( &args[3], statusBlock, 8 * sizeof(UInt32) );
		
		params.refCon = (void*)fRefCon;
		params.generation = (UInt32)args[0];
		params.status = (IOReturn)args[1];
		params.reconnectStatusBlock = (FWSBP2StatusBlock*)&statusBlock;
		params.reconnectStatusBlockLength = (UInt32)args[2];
		
		outArgs = &params;
	}	
	
	if( fMessageCallbackRoutine != NULL )
		(fMessageCallbackRoutine)( fMessageCallbackRefCon, type, outArgs );

}
