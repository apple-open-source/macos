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

#include <Carbon/Carbon.h>  // for printf

#include "FWDebugging.h"
#include "IOFireWireSBP2LibORB.h"
#include "IOFireWireSBP2LibLogin.h"
#include "IOFireWireSBP2LibMgmtORB.h"

__BEGIN_DECLS
#include <IOKit/iokitmig.h>
__END_DECLS

//
// static interface table for IOFireWireSBP2LibMgmtORBInterface
//

IOFireWireSBP2LibMgmtORBInterface IOFireWireSBP2LibMgmtORB::sIOFireWireSBP2LibMgmtORBInterface =
{
    0,
	&IOFireWireSBP2LibMgmtORB::staticQueryInterface,
	&IOFireWireSBP2LibMgmtORB::staticAddRef,
	&IOFireWireSBP2LibMgmtORB::staticRelease,
	1, 0, // version/revision
	&IOFireWireSBP2LibMgmtORB::staticSubmitORB,
	&IOFireWireSBP2LibMgmtORB::staticSetORBCallback,
	&IOFireWireSBP2LibMgmtORB::staticSetRefCon,
	&IOFireWireSBP2LibMgmtORB::staticGetRefCon,
	&IOFireWireSBP2LibMgmtORB::staticSetCommandFunction,
	&IOFireWireSBP2LibMgmtORB::staticSetManageeORB,
	&IOFireWireSBP2LibMgmtORB::staticSetManageeLogin,
	&IOFireWireSBP2LibMgmtORB::staticSetResponseBuffer
	
};

// alloc
//
// static allocator, called by factory method

IUnknownVTbl ** IOFireWireSBP2LibMgmtORB::alloc( io_connect_t connection,
											     mach_port_t asyncPort )
{
    IOReturn					status = kIOReturnSuccess;
	IOFireWireSBP2LibMgmtORB *	me;
	IUnknownVTbl ** 			interface = NULL;
	
	if( status == kIOReturnSuccess )
	{
		me = new IOFireWireSBP2LibMgmtORB();
		if( me == NULL )
			status = kIOReturnError;
	}
		
	if( status == kIOReturnSuccess )
	{
		status = me->init( connection, asyncPort );
	}
	
	if( status != kIOReturnSuccess )
		delete me;

	if( status == kIOReturnSuccess )
	{
		// we return an interface here. queryInterface will not be called. call addRef here
		me->addRef();
		interface = (IUnknownVTbl **) &me->fIOFireWireSBP2LibMgmtORBInterface.pseudoVTable;
	}
	
	return interface;
}

// ctor
//
//

IOFireWireSBP2LibMgmtORB::IOFireWireSBP2LibMgmtORB( void )
{	
	// init cf plugin ref counting
	fRefCount = 0;
	fConnection = 0;
	fMgmtORBRef = 0;
	fRefCon = 0;
	fORBCallbackRoutine = NULL;
	fORBCallbackRefCon = NULL;

	// create test driver interface map
	fIOFireWireSBP2LibMgmtORBInterface.pseudoVTable 
								= (IUnknownVTbl *) &sIOFireWireSBP2LibMgmtORBInterface;
	fIOFireWireSBP2LibMgmtORBInterface.obj = this;

}

// init
//
//

IOReturn IOFireWireSBP2LibMgmtORB::init( io_connect_t connection, mach_port_t asyncPort )
{
	IOReturn status = kIOReturnSuccess;

	fConnection = connection;
	fAsyncPort = asyncPort;
	
	FWLOG(( "IOFireWireSBP2LibMgmtORB : fConnection %d, fAsyncPort %d\n", 
												fConnection, fAsyncPort ));
	
	if( !fConnection || !fAsyncPort )
		status = kIOReturnError;
		
	if( status == kIOReturnSuccess )
	{
		mach_msg_type_number_t len = 1;
		status = io_connect_method_scalarI_scalarO( connection, 					 
													kIOFWSBP2UserClientCreateMgmtORB, 
													NULL, 0, (int*)&fMgmtORBRef, &len );
		if( status != kIOReturnSuccess )
			fMgmtORBRef = 0; // just to make sure
													
		FWLOG(( "IOFireWireSBP2LibMgmtORB :  status = 0x%08x = fMgmtORBRef 0x%08lx\n",
																	status, fMgmtORBRef ));
	}
	
	if( status == kIOReturnSuccess )
	{
		io_async_ref_t 			asyncRef;
		io_scalar_inband_t		params;
		mach_msg_type_number_t	size = 0;
		
		asyncRef[0] = 0;
		params[0]	= (UInt32)fMgmtORBRef;
		params[1]	= (UInt32)this;
		params[2]	= (UInt32)(IOAsyncCallback1)&IOFireWireSBP2LibMgmtORB::staticORBCompletion;
	
		status = io_async_method_scalarI_scalarO( fConnection, fAsyncPort, 
												  asyncRef, 1, 
												  kIOFWSBP2UserClientSetMgmtORBCallback,
												  params, 3,
												  NULL, &size );	
	}
		
	return status;
}

// dtor
//
//

IOFireWireSBP2LibMgmtORB::~IOFireWireSBP2LibMgmtORB()
{
	if( fMgmtORBRef ) 
	{
		IOReturn status = kIOReturnSuccess;
		
		mach_msg_type_number_t len = 0;
		status = io_connect_method_scalarI_scalarO( fConnection, 	
													kIOFWSBP2UserClientReleaseMgmtORB, 
													(int*)&fMgmtORBRef, 1, NULL, &len );
		FWLOG(( "IOFireWireSBP2LibMgmtORB : release orb status = 0x%08x\n", status ));
	}
}


//////////////////////////////////////////////////////////////////
// IUnknown methods
//

// queryInterface
//
//

HRESULT IOFireWireSBP2LibMgmtORB::staticQueryInterface( void * self, REFIID iid, void **ppv )
{
	return getThis(self)->queryInterface( iid, ppv );
}

HRESULT IOFireWireSBP2LibMgmtORB::queryInterface( REFIID iid, void **ppv )
{
    CFUUIDRef uuid = CFUUIDCreateFromUUIDBytes(NULL, iid);
    HRESULT result = S_OK;

	if( CFEqual(uuid, IUnknownUUID) ||  CFEqual(uuid, kIOFireWireSBP2LibMgmtORBInterfaceID) ) 
	{
        *ppv = &fIOFireWireSBP2LibMgmtORBInterface;
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

UInt32 IOFireWireSBP2LibMgmtORB::staticAddRef( void * self )
{
	return getThis(self)->addRef();
}

UInt32 IOFireWireSBP2LibMgmtORB::addRef()
{
    fRefCount += 1;
    return fRefCount;
}

// release
//
//

UInt32 IOFireWireSBP2LibMgmtORB::staticRelease( void * self )
{
	return getThis(self)->release();
}

UInt32 IOFireWireSBP2LibMgmtORB::release( void )
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
// IOFireWireSBP2LibMgmtORB methods

// submitORB
//
//

IOReturn IOFireWireSBP2LibMgmtORB::staticSubmitORB( void * self )
{
	return getThis(self)->submitORB();
}

IOReturn IOFireWireSBP2LibMgmtORB::submitORB( void )
{
	IOReturn status = kIOReturnSuccess;
	
	FWLOG(( "IOFireWireSBP2LibMgmtORB : submitORB\n" ));
		
	mach_msg_type_number_t len = 0;
	status = io_connect_method_scalarI_scalarO( fConnection, 	
												kIOFWSBP2UserClientSubmitMgmtORB, 
												(int*)&fMgmtORBRef, 1, NULL, &len );
	return status;
}

// setORBCallback
//
//

void IOFireWireSBP2LibMgmtORB::staticSetORBCallback( void * self, void * refCon, 
													IOFWSBP2ORBAppendCallback callback )
{
	getThis(self)->setORBCallback( refCon, callback );
}

void IOFireWireSBP2LibMgmtORB::setORBCallback( void * refCon, 
													IOFWSBP2ORBAppendCallback callback )
{
	fORBCallbackRoutine = callback;
	fORBCallbackRefCon = refCon;
}

// setRefCon
//
//

void IOFireWireSBP2LibMgmtORB::staticSetRefCon( void * self, UInt32 refCon )
{
	getThis(self)->setRefCon( refCon );
}

void IOFireWireSBP2LibMgmtORB::setRefCon( UInt32 refCon )
{
	fRefCon = refCon;
}

// getRefCon
//
//

UInt32 IOFireWireSBP2LibMgmtORB::staticGetRefCon( void * self )
{
	return getThis(self)->getRefCon();
}

UInt32 IOFireWireSBP2LibMgmtORB::getRefCon( void )
{
	return fRefCon;
}

// setCommandFunction
//
//

IOReturn IOFireWireSBP2LibMgmtORB::staticSetCommandFunction
										( void * self, UInt32 function )
{
	return getThis(self)->setCommandFunction( function );
}

IOReturn IOFireWireSBP2LibMgmtORB::setCommandFunction( UInt32 function )
{
	IOReturn status = kIOReturnSuccess;

	if( fMgmtORBRef == NULL ) 
		status = kIOReturnError;
		
	if( status == kIOReturnSuccess )
	{
		mach_msg_type_number_t len = 0;
		UInt32 params[2];
	
		params[0] = fMgmtORBRef;
		params[1] = function;

		status = io_connect_method_scalarI_scalarO
						( fConnection, 	
						  kIOFWSBP2UserClientMgmtORBSetCommandFunction, 
						  (int*)&params, 2, NULL, &len );
						  
		FWLOG(( "IOFireWireSBP2LibMgmtORB : setCommandFunction = 0x%08x\n", function ));
	}
	
	return status;
}


// setManageeORB
//
//

IOReturn IOFireWireSBP2LibMgmtORB::staticSetManageeORB( void * self, void * orb )
{
	return getThis(self)->setManageeORB( orb );
}

IOReturn IOFireWireSBP2LibMgmtORB::setManageeORB( void * orb )
{
	IOReturn status = kIOReturnSuccess;

	if( fMgmtORBRef == NULL ) 
		status = kIOReturnError;
	
	UInt32 orbRef;

	if( orb == NULL )
		orbRef = NULL;
	else
		orbRef = IOFireWireSBP2LibORB::getThis(orb)->getORBRef();
		
	if( status == kIOReturnSuccess )
	{
		mach_msg_type_number_t len = 0;
		UInt32 params[2];
	
		params[0] = fMgmtORBRef;
		params[1] = orbRef;

		status = io_connect_method_scalarI_scalarO
						( fConnection, 	
						  kIOFWSBP2UserClientMgmtORBSetManageeORB, 
						  (int*)&params, 2, NULL, &len );
	}
	
	return status;
}

// setManageeLogin
//
//

IOReturn IOFireWireSBP2LibMgmtORB::staticSetManageeLogin( void * self, void * login )
{
	return getThis(self)->setManageeLogin( login );
}

IOReturn IOFireWireSBP2LibMgmtORB::setManageeLogin( void * login )
{
	IOReturn status = kIOReturnSuccess;

	if( fMgmtORBRef == NULL ) 
		status = kIOReturnError;
	
	UInt32 loginRef;

	if( login == NULL )
		loginRef = NULL;
	else
		loginRef = IOFireWireSBP2LibLogin::getThis(login)->getLoginRef();

	if( status == kIOReturnSuccess )
	{
		mach_msg_type_number_t len = 0;
		UInt32 params[2];
	
		params[0] = fMgmtORBRef;
		params[1] = loginRef;

		status = io_connect_method_scalarI_scalarO
						( fConnection, 	
						  kIOFWSBP2UserClientMgmtORBSetManageeLogin, 
						  (int*)&params, 2, NULL, &len );
	}
	
	return status;
}

// setResponseBuffer
//
//

IOReturn IOFireWireSBP2LibMgmtORB::staticSetResponseBuffer
								( void * self, void * buf, UInt32 len )
{
	return getThis(self)->setResponseBuffer( buf, len );
}

IOReturn IOFireWireSBP2LibMgmtORB::setResponseBuffer( void * buf, UInt32 len )
{
	IOReturn status = kIOReturnSuccess;

	if( fMgmtORBRef == NULL ) 
		status = kIOReturnError;
		
	if( status == kIOReturnSuccess )
	{
		mach_msg_type_number_t len = 0;
		UInt32 params[3];
	
		params[0] = fMgmtORBRef;
		params[1] = (UInt32)buf;
		params[2] = len;
		
		status = io_connect_method_scalarI_scalarO
						( fConnection, 	
						  kIOFWSBP2UserClientMgmtORBSetResponseBuffer, 
						  (int*)&params, 3, NULL, &len );
	}
	
	return status;
}

//////////////////////////////////////////////////////////////////
// callback methods

void IOFireWireSBP2LibMgmtORB::staticORBCompletion( void *refcon, IOReturn result, 
													void * arg0 )
{
	((IOFireWireSBP2LibMgmtORB*)refcon)->ORBCompletion( result, arg0 );
}

void IOFireWireSBP2LibMgmtORB::ORBCompletion( IOReturn result, void * arg0 )
{
	FWLOG(( "IOFireWireSBP2LibMgmtORB : ORBCompletion\n" ));
	
	if( fORBCallbackRoutine != NULL )
		(fORBCallbackRoutine)( fORBCallbackRefCon, (IOReturn)arg0, (void*)fRefCon );
}
