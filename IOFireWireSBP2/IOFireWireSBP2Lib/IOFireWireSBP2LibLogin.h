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

#ifndef _IOKIT_IOFIREWIRESBP2LIBLOGIN_H_
#define _IOKIT_IOFIREWIRESBP2LIBLOGIN_H_

#include "IOFireWireSBP2LibLUN.h"
#include "IOFireWireSBP2UserClientCommon.h"

class IOFireWireSBP2LibLogin
{

public:

	struct InterfaceMap 
	{
        IUnknownVTbl *pseudoVTable;
        IOFireWireSBP2LibLogin *obj;
    };
	
	IOFireWireSBP2LibLogin( void );
	virtual ~IOFireWireSBP2LibLogin();
	
	virtual IOReturn init( io_connect_t connection, mach_port_t asyncPort );
	
protected:

	//////////////////////////////////////											
	// cf plugin interfaces
	
	static IOFireWireSBP2LibLoginInterface		sIOFireWireSBP2LibLoginInterface;
	InterfaceMap								fIOFireWireSBP2LibLoginInterface;

	//////////////////////////////////////											
	// cf plugin ref counting
	
	UInt32 			fRefCount;

	//////////////////////////////////////												
	// user client connection
	
	io_connect_t 	fConnection;	// connection to user client in kernel
	mach_port_t 	fAsyncPort;		// async port for callback from kernel
	UInt32 			fLoginRef;  	// reference to kernel login object

	IOFWSBP2LoginCallback			fLoginCallbackRoutine;
	void *							fLoginCallbackRefCon;

	IOFWSBP2LogoutCallback			fLogoutCallbackRoutine;
	void *							fLogoutCallbackRefCon;

	IOFWSBP2NotifyCallback			fUnsolicitedStatusNotifyRoutine;
	void * 							fUnsolicitedStatusNotifyRefCon;

	IOFWSBP2NotifyCallback			fStatusNotifyRoutine;
	void * 							fStatusNotifyRefCon;
	
	IOFWSBP2StatusCallback 			fFetchAgentResetCallback;
	void * 							fFetchAgentResetRefCon;

	IOFWSBP2FetchAgentWriteCallback fFetchAgentWriteCallback;
	void * 							fFetchAgentWriteRefCon;
	
	UInt32							fRefCon;
	
	//////////////////////////////////////												
	// IUnknown static methods
	
	static HRESULT staticQueryInterface( void * self, REFIID iid, void **ppv );
	virtual HRESULT queryInterface( REFIID iid, void **ppv );

	static UInt32 staticAddRef( void * self );
	virtual UInt32 addRef( void );

	static UInt32 staticRelease( void * self );
	virtual UInt32 release( void );

	//////////////////////////////////////												
	// IOFireWireSBP2Login static methods
	
	static IOReturn staticSubmitLogin( void * self );
	virtual IOReturn submitLogin( void );

	static IOReturn staticSubmitLogout( void * self );
	virtual IOReturn submitLogout( void );

	static void staticSetLoginFlags( void * self, UInt32 flags );
	virtual void setLoginFlags( UInt32 flags );

	static void staticSetLoginCallback( void * self, void * refCon, IOFWSBP2LoginCallback callback );
	virtual void setLoginCallback( void * refCon, IOFWSBP2LoginCallback callback );

	static void staticSetLogoutCallback( void * self, void * refCon, IOFWSBP2LogoutCallback callback );
	virtual void setLogoutCallback( void * refCon, IOFWSBP2LogoutCallback callback );

	static void staticSetRefCon( void * self, UInt32 refCon );
	virtual void setRefCon( UInt32 refCon );

	static UInt32 staticGetRefCon( void * self );
	virtual UInt32 getRefCon( void );

	static UInt32 staticGetMaxCommandBlockSize( void * self );
	virtual UInt32 getMaxCommandBlockSize( void );

	static UInt32 staticGetLoginID( void * self );
	virtual UInt32 getLoginID( void );
	
	static void staticSetMaxPayloadSize( void * self, UInt32 size );
	virtual void setMaxPayloadSize( UInt32 size );
	
	static void staticSetReconnectTime( void * self, UInt32 time );
	virtual void setReconnectTime( UInt32 time );

	static IUnknownVTbl ** staticCreateORB( void * self, REFIID iid );
	virtual IUnknownVTbl ** createORB( REFIID iid );

	static IOReturn staticSubmitORB( void * self, IOFireWireSBP2LibORBInterface ** orb );
	virtual IOReturn submitORB( IOFireWireSBP2LibORBInterface ** orb );

	static void staticSetUnsolicitedStatusNotify( void * self, void * refCon, IOFWSBP2NotifyCallback callback );
	virtual void setUnsolicitedStatusNotify( void * refCon, IOFWSBP2NotifyCallback callback );

	static void staticSetStatusNotify( void * self, void * refCon, IOFWSBP2NotifyCallback callback );
	virtual void setStatusNotify( void * refCon, IOFWSBP2NotifyCallback callback );

	static void staticSetFetchAgentResetCallback( void * self, void * refCon, IOFWSBP2StatusCallback callback );
	virtual void setFetchAgentResetCallback( void * refCon, IOFWSBP2StatusCallback callback );

	static IOReturn staticSubmitFetchAgentReset( void * self );
	virtual IOReturn submitFetchAgentReset( void );

	static void staticSetFetchAgentWriteCallback( void * self, void * refCon, IOFWSBP2FetchAgentWriteCallback callback );
	virtual void setFetchAgentWriteCallback( void * refCon, IOFWSBP2FetchAgentWriteCallback callback );

	static IOReturn staticRingDoorbell( void * self );
	virtual IOReturn ringDoorbell( void );

	static IOReturn staticEnableUnsolicitedStatus( void * self );
	virtual IOReturn enableUnsolicitedStatus( void );

	static IOReturn staticSetBusyTimeoutRegisterValue( void * self, UInt32 timeout );
	virtual IOReturn setBusyTimeoutRegisterValue( UInt32 timeout );

	static IOReturn staticSetPassword( void * self, void * buffer, UInt32 length );
	virtual IOReturn setPassword( void * buffer, UInt32 length );

	//////////////////////////////////////												
	// callback static methods
	
	static void staticLoginCompletion( void *refcon, IOReturn result, void **args, int numArgs );
	virtual void loginCompletion( IOReturn result, void **args, int numArgs );

	static void staticLogoutCompletion( void *refcon, IOReturn result, void **args, int numArgs );
	virtual void logoutCompletion( IOReturn result, void **args, int numArgs );

	static void staticUnsolicitedStatusNotify( void *refcon, IOReturn result, void **args, int numArgs );
	virtual void unsolicitedStatusNotify( IOReturn result, void **args, int numArgs );

	static void staticStatusNotify( void *refcon, IOReturn result, void **args, int numArgs );
	virtual void statusNotify( IOReturn result, void **args, int numArgs );

	static void staticFetchAgentResetCompletion( void *refcon, IOReturn result, void **args, int numArgs );
	virtual void fetchAgentResetCompletion( IOReturn result, void **args, int numArgs );

	static void staticFetchAgentWriteCompletion( void *refcon, IOReturn result, void **args, int numArgs );
	virtual void fetchAgentWriteCompletion( IOReturn result, void **args, int numArgs );

public:

	// utility function to get "this" pointer from interface
	static inline IOFireWireSBP2LibLogin *getThis( void *self )
        { return (IOFireWireSBP2LibLogin *) ((InterfaceMap *) self)->obj; };

	static IUnknownVTbl **alloc( io_connect_t connection, mach_port_t asyncPort );

	virtual UInt32 getLoginRef( void );
	
};
#endif
