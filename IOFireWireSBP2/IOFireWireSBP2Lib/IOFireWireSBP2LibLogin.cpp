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

#include <CoreFoundation/CoreFoundation.h>

#include "FWDebugging.h"
#include "IOFireWireSBP2LibLogin.h"
#include "IOFireWireSBP2LibORB.h"

#include <System/libkern/OSCrossEndian.h>

__BEGIN_DECLS
#include <IOKit/iokitmig.h>
__END_DECLS

//
// static interface table for IOFireWireSBP2LibLoginInterface
//

IOFireWireSBP2LibLoginInterface IOFireWireSBP2LibLogin::sIOFireWireSBP2LibLoginInterface =
{
    0,
	&IOFireWireSBP2LibLogin::staticQueryInterface,
	&IOFireWireSBP2LibLogin::staticAddRef,
	&IOFireWireSBP2LibLogin::staticRelease,
	1, 0, // version/revision
	&IOFireWireSBP2LibLogin::staticSubmitLogin,
	&IOFireWireSBP2LibLogin::staticSubmitLogout,
	&IOFireWireSBP2LibLogin::staticSetLoginFlags,
	&IOFireWireSBP2LibLogin::staticSetLoginCallback,
	&IOFireWireSBP2LibLogin::staticSetLogoutCallback,
	&IOFireWireSBP2LibLogin::staticSetRefCon,
	&IOFireWireSBP2LibLogin::staticGetRefCon,
	&IOFireWireSBP2LibLogin::staticGetMaxCommandBlockSize,
	&IOFireWireSBP2LibLogin::staticGetLoginID,
	&IOFireWireSBP2LibLogin::staticSetMaxPayloadSize,
	&IOFireWireSBP2LibLogin::staticSetReconnectTime,
	&IOFireWireSBP2LibLogin::staticCreateORB,
	&IOFireWireSBP2LibLogin::staticSubmitORB,
	&IOFireWireSBP2LibLogin::staticSetUnsolicitedStatusNotify,
	&IOFireWireSBP2LibLogin::staticSetStatusNotify,
	&IOFireWireSBP2LibLogin::staticSetFetchAgentResetCallback,
	&IOFireWireSBP2LibLogin::staticSubmitFetchAgentReset,
	&IOFireWireSBP2LibLogin::staticSetFetchAgentWriteCallback,
	&IOFireWireSBP2LibLogin::staticRingDoorbell,
	&IOFireWireSBP2LibLogin::staticEnableUnsolicitedStatus,
	&IOFireWireSBP2LibLogin::staticSetBusyTimeoutRegisterValue,
	&IOFireWireSBP2LibLogin::staticSetPassword
};

// alloc
//
// static allocator, called by factory method

IUnknownVTbl ** IOFireWireSBP2LibLogin::alloc( io_connect_t connection,
											   mach_port_t asyncPort )
{
    IOReturn					status = kIOReturnSuccess;
	IOFireWireSBP2LibLogin *	me;
	IUnknownVTbl ** 			interface = NULL;
	
	if( status == kIOReturnSuccess )
	{
		me = new IOFireWireSBP2LibLogin();
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
		interface = (IUnknownVTbl **) &me->fIOFireWireSBP2LibLoginInterface.pseudoVTable;
	}
	
	return interface;
}

// ctor
//
//

IOFireWireSBP2LibLogin::IOFireWireSBP2LibLogin( void )
{	
	// init cf plugin ref counting
	fRefCount = 0;
	fConnection = 0;
	fLoginRef = 0;
	fRefCon = 0;
	
	fLoginCallbackRoutine = NULL;
	fLoginCallbackRefCon = NULL;

	fLogoutCallbackRoutine = NULL;
	fLogoutCallbackRefCon = NULL;

	fUnsolicitedStatusNotifyRoutine = NULL;
	fUnsolicitedStatusNotifyRefCon = NULL;

	fStatusNotifyRoutine = NULL;
	fStatusNotifyRefCon = NULL;

	fFetchAgentResetCallback = NULL;
	fFetchAgentResetRefCon = NULL;
	
	// create test driver interface map
	fIOFireWireSBP2LibLoginInterface.pseudoVTable 
								= (IUnknownVTbl *) &sIOFireWireSBP2LibLoginInterface;
	fIOFireWireSBP2LibLoginInterface.obj = this;

}

// init
//
//

IOReturn IOFireWireSBP2LibLogin::init( io_connect_t connection, mach_port_t asyncPort )
{
	IOReturn status = kIOReturnSuccess;

	fConnection = connection;
	fAsyncPort = asyncPort;
	
	FWLOG(( "IOFireWireSBP2LibLogin : fConnection %d, fAsyncPort %d\n",
													fConnection, fAsyncPort ));
	
	if( !fConnection || !fAsyncPort )
		status = kIOReturnError;
		
	if( status == kIOReturnSuccess )
	{
		uint32_t len = 1;
		status = IOConnectCallScalarMethod( connection, kIOFWSBP2UserClientCreateLogin, NULL, 0, &fLoginRef, &len );
		if( status != kIOReturnSuccess )
			fLoginRef = 0; // just to make sure
													
		FWLOG(( "IOFireWireSBP2LibLogin :  status = 0x%08x = fLoginRef 0x%08lx\n",
																	status, fLoginRef ));
	}
	
	if( status == kIOReturnSuccess )
	{
		io_async_ref64_t asyncRef;
		mach_msg_type_number_t	size = 0;
		
		asyncRef[kIOAsyncCalloutFuncIndex] = (uint64_t)&IOFireWireSBP2LibLogin::staticLoginCompletion;
		asyncRef[kIOAsyncCalloutRefconIndex] = (uint64_t)this;
			  
		status = IOConnectCallAsyncScalarMethod( fConnection, kIOFWSBP2UserClientSetLoginCallback, fAsyncPort, asyncRef, kOSAsyncRef64Count, NULL, 0, NULL, &size  );
	}

	if( status == kIOReturnSuccess )
	{
		io_async_ref64_t asyncRef;
		mach_msg_type_number_t	size = 0;

		asyncRef[kIOAsyncCalloutFuncIndex] = (uint64_t)&IOFireWireSBP2LibLogin::staticLogoutCompletion;
		asyncRef[kIOAsyncCalloutRefconIndex] = (uint64_t)this;
			  
		status = IOConnectCallAsyncScalarMethod( fConnection, kIOFWSBP2UserClientSetLogoutCallback, fAsyncPort, asyncRef, kOSAsyncRef64Count, NULL, 0, NULL, &size  );
	}
	
	if( status == kIOReturnSuccess )
	{
		io_async_ref64_t asyncRef;
		mach_msg_type_number_t	size = 0;

		asyncRef[kIOAsyncCalloutFuncIndex] = (uint64_t)&IOFireWireSBP2LibLogin::staticUnsolicitedStatusNotify;
		asyncRef[kIOAsyncCalloutRefconIndex] = (uint64_t)this;
					  
		status = IOConnectCallAsyncScalarMethod( fConnection, kIOFWSBP2UserClientSetUnsolicitedStatusNotify, fAsyncPort, asyncRef, kOSAsyncRef64Count, NULL, 0, NULL, &size  );
	}
	
	if( status == kIOReturnSuccess )
	{
		io_async_ref64_t asyncRef;
		mach_msg_type_number_t	size = 0;

		asyncRef[kIOAsyncCalloutFuncIndex] = (uint64_t)&IOFireWireSBP2LibLogin::staticStatusNotify;
		asyncRef[kIOAsyncCalloutRefconIndex] = (uint64_t)this;
					  
		status = IOConnectCallAsyncScalarMethod( fConnection, kIOFWSBP2UserClientSetStatusNotify, fAsyncPort, asyncRef, kOSAsyncRef64Count, NULL, 0, NULL, &size  );
	}

	if( status == kIOReturnSuccess )
	{
		io_async_ref64_t asyncRef;
		mach_msg_type_number_t	size = 0;

		asyncRef[kIOAsyncCalloutFuncIndex] = (uint64_t)&IOFireWireSBP2LibLogin::staticFetchAgentWriteCompletion;
		asyncRef[kIOAsyncCalloutRefconIndex] = (uint64_t)this;
					  
		status = IOConnectCallAsyncScalarMethod( fConnection, kIOFWSBP2UserClientSetFetchAgentWriteCompletion, fAsyncPort, asyncRef, kOSAsyncRef64Count, NULL, 0, NULL, &size  );
	}
	
	return status;
}

// dtor
//
//

IOFireWireSBP2LibLogin::~IOFireWireSBP2LibLogin()
{
	if( fLoginRef ) 
	{
		IOReturn status = kIOReturnSuccess;		
		
		uint32_t len = 0;
		status = IOConnectCallScalarMethod( fConnection, 	
											kIOFWSBP2UserClientReleaseLogin, 
											&fLoginRef, 1, NULL, &len );
		FWLOG(( "IOFireWireSBP2LibLogin : release login status = 0x%08x\n", status ));
	}
}


//////////////////////////////////////////////////////////////////
// IUnknown methods
//

// queryInterface
//
//

HRESULT IOFireWireSBP2LibLogin::staticQueryInterface( void * self, REFIID iid, void **ppv )
{
	return getThis(self)->queryInterface( iid, ppv );
}

HRESULT IOFireWireSBP2LibLogin::queryInterface( REFIID iid, void **ppv )
{
    CFUUIDRef uuid = CFUUIDCreateFromUUIDBytes(NULL, iid);
    HRESULT result = S_OK;

	if( CFEqual(uuid, IUnknownUUID) ||  CFEqual(uuid, kIOFireWireSBP2LibLoginInterfaceID) ) 
	{
        *ppv = &fIOFireWireSBP2LibLoginInterface;
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

UInt32 IOFireWireSBP2LibLogin::staticAddRef( void * self )
{
	return getThis(self)->addRef();
}

UInt32 IOFireWireSBP2LibLogin::addRef()
{
    fRefCount += 1;
    return fRefCount;
}

// release
//
//

UInt32 IOFireWireSBP2LibLogin::staticRelease( void * self )
{
	return getThis(self)->release();
}

UInt32 IOFireWireSBP2LibLogin::release( void )
{
	UInt32 retVal = fRefCount;
	
	if( 1 == fRefCount-- ) 
	{
		delete this;
    }
	
	return retVal;
}

//////////////////////////////////////////////////////////////////
// IOFireWireSBP2Login methods

// submitLogin
//
//

IOReturn IOFireWireSBP2LibLogin::staticSubmitLogin( void * self )
{
	return getThis(self)->submitLogin();
}

IOReturn IOFireWireSBP2LibLogin::submitLogin( void )
{
	IOReturn status = kIOReturnSuccess;
	
	FWLOG(( "IOFireWireSBP2LibLogin : submitLogin\n" ));
		
	uint32_t len = 0;
	status = IOConnectCallScalarMethod( fConnection, 	
										kIOFWSBP2UserClientSubmitLogin, 
										&fLoginRef, 1, NULL, &len );
	return status;
}

// submitLogout
//
//

IOReturn IOFireWireSBP2LibLogin::staticSubmitLogout( void * self )
{
	return getThis(self)->submitLogout();
}

IOReturn IOFireWireSBP2LibLogin::submitLogout( void )
{
	IOReturn status = kIOReturnSuccess;
	
	FWLOG(( "IOFireWireSBP2LibLogin : submitLogout\n" ));
		
	uint32_t len = 0;
	status = IOConnectCallScalarMethod( fConnection, 	
										kIOFWSBP2UserClientSubmitLogout, 
										&fLoginRef, 1, NULL, &len );
	return status;
}

// setLoginFlags
//
//

void IOFireWireSBP2LibLogin::staticSetLoginFlags( void * self, UInt32 flags )
{
	getThis(self)->setLoginFlags( flags );
}

void IOFireWireSBP2LibLogin::setLoginFlags( UInt32 flags )
{
	FWLOG(( "IOFireWireSBP2LibLogin : setLoginFlags: 0x%08lx\n", flags ));
		
	uint32_t len = 0;
	uint64_t params[2];
	
	params[0] = fLoginRef;
	params[1] = flags;
	
	IOConnectCallScalarMethod( fConnection, 	
								kIOFWSBP2UserClientSetLoginFlags, 
								params, 2, NULL, &len );
}

// setLoginCallback
//
//

void IOFireWireSBP2LibLogin::staticSetLoginCallback( void * self, void * refCon, 
													IOFWSBP2LoginCallback callback )
{
	getThis(self)->setLoginCallback( refCon, callback );
}

void IOFireWireSBP2LibLogin::setLoginCallback( void * refCon, 
													IOFWSBP2LoginCallback callback )
{
	fLoginCallbackRoutine = callback;
	fLoginCallbackRefCon = refCon;
}

// setLogoutCallback
//
//

void IOFireWireSBP2LibLogin::staticSetLogoutCallback( void * self, void * refCon, 
													IOFWSBP2LogoutCallback callback )
{
	getThis(self)->setLogoutCallback( refCon, callback );
}

void IOFireWireSBP2LibLogin::setLogoutCallback( void * refCon, 
													IOFWSBP2LogoutCallback callback )
{
	fLogoutCallbackRoutine = callback;
	fLogoutCallbackRefCon = refCon;
}

// setRefCon
//
//

void IOFireWireSBP2LibLogin::staticSetRefCon( void * self, void * refCon )
{
	getThis(self)->setRefCon( refCon );
}

void IOFireWireSBP2LibLogin::setRefCon( void * refCon )
{
	fRefCon = refCon;
}

// getRefCon
//
//

void * IOFireWireSBP2LibLogin::staticGetRefCon( void * self )
{
	return getThis(self)->getRefCon();
}

void * IOFireWireSBP2LibLogin::getRefCon( void )
{
	return fRefCon;
}

// getMaxCommandBlockSize
//
//

UInt32 IOFireWireSBP2LibLogin::staticGetMaxCommandBlockSize( void * self )
{
	return getThis(self)->getMaxCommandBlockSize();
}

UInt32 IOFireWireSBP2LibLogin::getMaxCommandBlockSize( void )
{
	IOReturn status = kIOReturnSuccess;
	uint32_t len = 1;
	uint64_t blockSize;
		
	status = IOConnectCallScalarMethod( fConnection, 	
									    kIOFWSBP2UserClientGetMaxCommandBlockSize, 
									    &fLoginRef, 1, &blockSize, &len );
	if( status != kIOReturnSuccess )
		blockSize = 0;
	
	return (UInt32)blockSize;
}

// getLoginID
//
//

UInt32 IOFireWireSBP2LibLogin::staticGetLoginID( void * self )
{
	return getThis(self)->getLoginID();
}

UInt32 IOFireWireSBP2LibLogin::getLoginID( void )
{
	IOReturn status = kIOReturnSuccess;
	uint32_t len = 1;
	uint64_t loginID;
	
	IOConnectCallScalarMethod(	fConnection, 	
								kIOFWSBP2UserClientGetLoginID, 
								&fLoginRef, 1, &loginID, &len );
	if( status != kIOReturnSuccess )
		loginID = 0;
		
	return loginID;
}

// setMaxPayloadSize
//
//

void IOFireWireSBP2LibLogin::staticSetMaxPayloadSize( void * self, UInt32 size )
{
	getThis(self)->setMaxPayloadSize( size );
}

void IOFireWireSBP2LibLogin::setMaxPayloadSize( UInt32 size )
{
	FWLOG(( "IOFireWireSBP2LibLogin : setReconnectTime = %ld\n", size ));
		
	uint32_t len = 0;
	uint64_t params[2];
	
	params[0] = fLoginRef;
	params[1] = size;
	
	IOConnectCallScalarMethod(	fConnection, 	
								kIOFWSBP2UserClientSetMaxPayloadSize, 
								params, 2, NULL, &len );

}

// setReconnectTime
//
//

void IOFireWireSBP2LibLogin::staticSetReconnectTime( void * self, UInt32 time )
{
	getThis(self)->setReconnectTime( time );
}

void IOFireWireSBP2LibLogin::setReconnectTime( UInt32 time )
{
	FWLOG(( "IOFireWireSBP2LibLogin : setReconnectTime = %ld\n", time ));
		
	uint32_t len = 0;
	uint64_t params[2];
	
	params[0] = fLoginRef;
	params[1] = time;
	
	IOConnectCallScalarMethod(	fConnection, 	
								kIOFWSBP2UserClientSetReconnectTime, 
								params, 2, NULL, &len );

}

// createORB
//
//

IUnknownVTbl ** IOFireWireSBP2LibLogin::staticCreateORB( void * self, REFIID iid )
{
	return getThis(self)->createORB(iid);
}

IUnknownVTbl ** IOFireWireSBP2LibLogin::createORB( REFIID iid )
{
	IOReturn status = kIOReturnSuccess;
	IUnknownVTbl ** iunknown = NULL;
	IUnknownVTbl ** interface = NULL;
	
	if( !fConnection )
		status = kIOReturnError;

	if( status == kIOReturnSuccess )
	{
		iunknown = IOFireWireSBP2LibORB::alloc( fConnection, fAsyncPort );
		if( iunknown == NULL )
			status = kIOReturnNoMemory;
	}
	
	if( status == kIOReturnSuccess )
	{	
		HRESULT res;
		res = (*iunknown)->QueryInterface( iunknown, iid,
										   (void **) &interface );
		
		if( res != S_OK )
			status = kIOReturnError;
	}
		
	if( iunknown != NULL )
	{
		(*iunknown)->Release(iunknown);
	}

	if( status == kIOReturnSuccess )
		return interface;
	else
		return NULL;
}

// submitORB
//
//

IOReturn IOFireWireSBP2LibLogin::staticSubmitORB( void * self, IOFireWireSBP2LibORBInterface ** orb )
{
	return getThis(self)->submitORB( orb );
}

IOReturn IOFireWireSBP2LibLogin::submitORB( IOFireWireSBP2LibORBInterface ** orb )
{
	IOReturn status = kIOReturnSuccess;
	
	FWLOG(( "IOFireWireSBP2LibLogin : submitORB\n" ));
	
	uint64_t ref = IOFireWireSBP2LibORB::getThis(orb)->getORBRef();

	uint32_t len = 0;
	status = IOConnectCallScalarMethod( fConnection, 	
										kIOFWSBP2UserClientSubmitORB, 
										&ref, 1, NULL, &len );
	return status;
}

// setUnsolicitedStatusNotify
//
//

void IOFireWireSBP2LibLogin::staticSetUnsolicitedStatusNotify( void * self, void * refCon, 
														IOFWSBP2NotifyCallback callback )
{
	getThis(self)->setUnsolicitedStatusNotify( refCon, callback );
}

void IOFireWireSBP2LibLogin::setUnsolicitedStatusNotify( void * refCon, 																		 IOFWSBP2NotifyCallback callback )
{
	fUnsolicitedStatusNotifyRoutine = callback;
	fUnsolicitedStatusNotifyRefCon = refCon;
}

// setStatusNotify
//
//

void IOFireWireSBP2LibLogin::staticSetStatusNotify( void * self, void * refCon, 
														IOFWSBP2NotifyCallback callback )
{
	getThis(self)->setStatusNotify( refCon, callback );
}

void IOFireWireSBP2LibLogin::setStatusNotify( void * refCon, 
													IOFWSBP2NotifyCallback callback )
{
	fStatusNotifyRoutine = callback;
	fStatusNotifyRefCon = refCon;
}

//////////////////////////////////////////////////////////////////
// callback methods

// loginCompletion
//
//

void IOFireWireSBP2LibLogin::staticLoginCompletion( void *refcon, IOReturn result, 
													io_user_reference_t *args, int numArgs )
{
	((IOFireWireSBP2LibLogin*)refcon)->loginCompletion( result, args, numArgs );
}

void IOFireWireSBP2LibLogin::loginCompletion( IOReturn result, io_user_reference_t *args, int numArgs )
{
	FWLOG(( "IOFireWireSBP2LibLogin : loginCompletion numArgs = %d\n",  numArgs));
	
	FWSBP2LoginCompleteParams params;
	
	UInt32 loginResponse[2];

	{
		int i;

		for( i = 0; i < 2; i++ )
		{
			IF_ROSETTA()
			{
				loginResponse[i] = OSSwapInt32( (UInt32)args[2+i] );
			}
			else
			{
				loginResponse[i] = (UInt32)args[2+i];
			}
		}
	}

	UInt32 statusBlock[4];
	
	{
		int i;

		for( i = 0; i < 4; i++ )
		{
			IF_ROSETTA()
			{
				statusBlock[i] = OSSwapInt32( (UInt32)args[7+i] );
			}
			else
			{
				statusBlock[i] = (UInt32)args[7+i];
			}
		}
	}
			
	params.refCon = (void*)fRefCon;
	params.generation = (UInt32)args[0];
	params.status = (IOReturn)args[1];
	params.loginResponse = (FWSBP2LoginResponse*)&loginResponse;
	params.statusBlock = (FWSBP2StatusBlock*)&statusBlock;
	params.statusBlockLength = (UInt32)args[6];
	
	if( fLoginCallbackRoutine != NULL )
		(fLoginCallbackRoutine)( fLoginCallbackRefCon, &params );
}

// logoutCompletion
//
//

void IOFireWireSBP2LibLogin::staticLogoutCompletion( void *refcon, IOReturn result, 
													io_user_reference_t *args, int numArgs )
{
	((IOFireWireSBP2LibLogin*)refcon)->logoutCompletion( result, args, numArgs );
}

void IOFireWireSBP2LibLogin::logoutCompletion( IOReturn result, io_user_reference_t *args, int numArgs )
{
	FWLOG(( "IOFireWireSBP2LibLogin : logoutCompletion numArgs = %d\n", numArgs ));

	FWSBP2LogoutCompleteParams params;
	
	UInt32 statusBlock[4];
	{
		int i;

		for( i = 0; i < 4; i++ )
		{
			IF_ROSETTA()
			{
				statusBlock[i] = OSSwapInt32( (UInt32)args[3+i] );
			}
			else
			{
				statusBlock[i] = (UInt32)args[3+i];
			}
		}
	}
			
	params.refCon = (void*)fRefCon;
	params.generation = (UInt32)args[0];
	params.status = (IOReturn)args[1];
	params.statusBlock = (FWSBP2StatusBlock*)&statusBlock;
	params.statusBlockLength = (UInt32)args[2];

	if( fLogoutCallbackRoutine != NULL )
		(fLogoutCallbackRoutine)( fLogoutCallbackRefCon, &params );
}

// unsolicitedNotify
//
//

void IOFireWireSBP2LibLogin::staticUnsolicitedStatusNotify( void *refcon, IOReturn result, 
													io_user_reference_t *args, int numArgs )
{
	((IOFireWireSBP2LibLogin*)refcon)->unsolicitedStatusNotify( result, args, numArgs );
}

void IOFireWireSBP2LibLogin::unsolicitedStatusNotify( IOReturn result, io_user_reference_t *args, 
																		int numArgs )
{
	FWLOG(( "IOFireWireSBP2LibLogin : unsolicitedStatusNotify numArgs = %d\n", numArgs ));

	FWSBP2NotifyParams params;

	UInt32 statusBlock[8];
	{
		int i;

		for( i = 0; i < 8; i++ )
		{
			IF_ROSETTA()
			{
				statusBlock[i] = OSSwapInt32( (UInt32)args[3+i] );
			}
			else
			{
				statusBlock[i] = (UInt32)args[3+i];
			}
		}
	}
		
	params.refCon = (void*)fRefCon;
	params.notificationEvent = (UInt32)args[0];
	params.generation = (IOReturn)args[1];
	params.length = (UInt32)args[2];
	params.message = (FWSBP2StatusBlock*)&statusBlock;

	if( fUnsolicitedStatusNotifyRoutine != NULL )
		(fUnsolicitedStatusNotifyRoutine)( fUnsolicitedStatusNotifyRefCon, &params );
}

// statusNotify
//
//

void IOFireWireSBP2LibLogin::staticStatusNotify( void *refcon, IOReturn result, 
													io_user_reference_t *args, int numArgs )
{
	((IOFireWireSBP2LibLogin*)refcon)->statusNotify( result, args, numArgs );
}

void IOFireWireSBP2LibLogin::statusNotify( IOReturn result, io_user_reference_t *args, int numArgs )
{
	FWLOG(( "IOFireWireSBP2LibLogin : statusNotify numArgs = %d\n", numArgs ));

//	printf( "IOFireWireSBP2LibLogin : statusNotify numArgs = %d\n", numArgs );

	FWSBP2NotifyParams params;

	UInt32 statusBlock[8];
	{
		int i;

		for( i = 0; i < 8; i++ )
		{
			IF_ROSETTA()
			{
				statusBlock[i] = OSSwapInt32( (UInt32)args[4+i] );
			}
			else
			{
				statusBlock[i] = (UInt32)args[4+i];
			}
		}
	}

#if 0		
	{
		int i;

		for( i = 0; i < 8; i++ )
		{
			printf( "IOFireWireSBP2LibLogin : statusNotify params[%d] 0x%08lx\n", i, statusBlock[i] ); 
		}
	}
#endif

	params.notificationEvent = (UInt32)args[0];
	params.generation = (IOReturn)args[1];
	params.length = (UInt32)args[2];
	params.refCon = (void*)args[3];
	params.message = (FWSBP2StatusBlock*)&statusBlock;

#if 0
	printf( "IOFireWireSBP2LibLogin : statusNotify args[0] 0x%08llx\n", (UInt32)args[0] ); 
	printf( "IOFireWireSBP2LibLogin : statusNotify args[1] 0x%08llx\n", (IOReturn)args[1] ); 
	printf( "IOFireWireSBP2LibLogin : statusNotify args[2] 0x%08llx\n", (UInt32)args[2] ); 
	#if __LP64__
		printf( "IOFireWireSBP2LibLogin : statusNotify args[3] 0x%016llx\n", (void*)args[3] ); 
	#else
		printf( "IOFireWireSBP2LibLogin : statusNotify args[3] 0x%08lx\n", (void*)args[3] ); 
	#endif
#endif

	if( fStatusNotifyRoutine != NULL )
		(fStatusNotifyRoutine)( fStatusNotifyRefCon, &params );
}

// setFetchAgentResetCallback
//
//

void IOFireWireSBP2LibLogin::staticSetFetchAgentResetCallback( void * self, void * refCon, IOFWSBP2StatusCallback callback )
{
	getThis(self)->setFetchAgentResetCallback( refCon, callback );
}

void IOFireWireSBP2LibLogin::setFetchAgentResetCallback( void * refCon, IOFWSBP2StatusCallback callback )
{
	fFetchAgentResetCallback = callback;
	fFetchAgentResetRefCon = refCon;
}

// submitFetchAgentReset
//
//

IOReturn IOFireWireSBP2LibLogin::staticSubmitFetchAgentReset( void * self )
{
	return getThis(self)->submitFetchAgentReset();
}

IOReturn IOFireWireSBP2LibLogin::submitFetchAgentReset( void )
{
	IOReturn status = kIOReturnSuccess;
	
	if( status == kIOReturnSuccess )
	{
		io_async_ref64_t asyncRef;
		mach_msg_type_number_t	size = 0;
		uint64_t	params[1];

		params[0] = fLoginRef;

		asyncRef[kIOAsyncCalloutFuncIndex] = (uint64_t)&IOFireWireSBP2LibLogin::staticFetchAgentResetCompletion;
		asyncRef[kIOAsyncCalloutRefconIndex] = (uint64_t)this;
		
		status = IOConnectCallAsyncScalarMethod( fConnection, kIOFWSBP2UserClientSubmitFetchAgentReset, fAsyncPort, asyncRef, kOSAsyncRef64Count, params, 1, NULL, &size  );
	}
									
	return status;

}

// fetchAgentResetCompletion
//
//

void IOFireWireSBP2LibLogin::staticFetchAgentResetCompletion( void *refcon, IOReturn result, io_user_reference_t *args,
												int numArgs )
{
	((IOFireWireSBP2LibLogin*)refcon)->fetchAgentResetCompletion( result, args, numArgs );
}

void IOFireWireSBP2LibLogin::fetchAgentResetCompletion( IOReturn result, io_user_reference_t *args, int numArgs )
{
	if( fFetchAgentResetCallback != NULL )
		(fFetchAgentResetCallback)( fFetchAgentResetRefCon, result );
}

// setFetchAgentWriteCallback
//
//

void IOFireWireSBP2LibLogin::staticSetFetchAgentWriteCallback( void * self, void * refCon, IOFWSBP2FetchAgentWriteCallback callback )
{
	getThis(self)->setFetchAgentWriteCallback( refCon, callback );
}

void IOFireWireSBP2LibLogin::setFetchAgentWriteCallback( void * refCon, IOFWSBP2FetchAgentWriteCallback callback )
{
	fFetchAgentWriteCallback = callback;
	fFetchAgentWriteRefCon = refCon;
}

// fetchAgentWriteCompletion
//
//

void IOFireWireSBP2LibLogin::staticFetchAgentWriteCompletion( void *refcon, IOReturn result, io_user_reference_t *args,
												int numArgs )
{
	((IOFireWireSBP2LibLogin*)refcon)->fetchAgentWriteCompletion( result, args, numArgs );
}

void IOFireWireSBP2LibLogin::fetchAgentWriteCompletion( IOReturn result, io_user_reference_t *args, int numArgs )
{
	if( fFetchAgentWriteCallback != NULL )
		(fFetchAgentWriteCallback)( fFetchAgentWriteRefCon, result, NULL );
}

// ringDoorbell
//
//

IOReturn IOFireWireSBP2LibLogin::staticRingDoorbell( void * self )
{
	return getThis(self)->ringDoorbell();
}

IOReturn IOFireWireSBP2LibLogin::ringDoorbell( void )
{
	IOReturn status = kIOReturnSuccess;
	
	if( status == kIOReturnSuccess )
	{
		uint32_t	size = 0;
		status = IOConnectCallScalarMethod( fConnection,
											kIOFWSBP2UserClientRingDoorbell,
											&fLoginRef, 1,
											NULL, &size );	
	}											
	
	return status;
}

// enableUnsolicitedStatus
//
//

IOReturn IOFireWireSBP2LibLogin::staticEnableUnsolicitedStatus( void * self )
{
	return getThis(self)->enableUnsolicitedStatus();
}

IOReturn IOFireWireSBP2LibLogin::enableUnsolicitedStatus( void )
{
	IOReturn status = kIOReturnSuccess;
	
	if( status == kIOReturnSuccess )
	{
		uint32_t	size = 0;
		status = IOConnectCallScalarMethod( fConnection,
											kIOFWSBP2UserClientEnableUnsolicitedStatus,
											&fLoginRef, 1,
											NULL, &size );	
	}											
	
	return status;
}

// setBusyTimeoutRegisterValue
//
//

IOReturn IOFireWireSBP2LibLogin::staticSetBusyTimeoutRegisterValue( void * self, UInt32 timeout )
{
	return getThis(self)->setBusyTimeoutRegisterValue( timeout );
}

IOReturn IOFireWireSBP2LibLogin::setBusyTimeoutRegisterValue( UInt32 timeout )
{
	IOReturn status = kIOReturnSuccess;
	
	if( status == kIOReturnSuccess )
	{
		uint64_t	params[2];
		uint32_t	size = 0;
		
		params[0]	= fLoginRef;
		params[1] 	= timeout;
		status = IOConnectCallScalarMethod( fConnection,
											kIOFWSBP2UserClientSetBusyTimeoutRegisterValue,
											params, 2,
											NULL, &size );	
	}											
	
	return status;
}

// setPassword
//
//

IOReturn IOFireWireSBP2LibLogin::staticSetPassword( void * self, void * buffer, UInt32 length )
{
	return getThis(self)->setPassword( buffer, length );
}

IOReturn IOFireWireSBP2LibLogin::setPassword( void * buffer, UInt32 length )
{
	IOReturn status = kIOReturnSuccess;

	FWLOG(( "IOFireWireSBP2LibORB : setPassword\n" ));
		
	uint32_t len = 0;
	uint64_t params[3];
	
	params[0] = fLoginRef;
	params[1] = (UInt64)buffer;
	params[2] = length;

	status = IOConnectCallScalarMethod( fConnection, 	
										kIOFWSBP2UserClientSetPassword, 
										params, 3, NULL, &len );
	return status;	
}


// getLoginRef
//
//

UInt32 IOFireWireSBP2LibLogin::getLoginRef( void )
{
	return fLoginRef;
}

