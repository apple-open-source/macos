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

#ifndef _IOKIT_IOFIREWIRESBP2LIBMGMTORB_H_
#define _IOKIT_IOFIREWIRESBP2LIBMGMTORB_H_

#include "IOFireWireSBP2LibLUN.h"
#include "IOFireWireSBP2UserClientCommon.h"

class IOFireWireSBP2LibMgmtORB
{

public:

	struct InterfaceMap 
	{
        IUnknownVTbl *pseudoVTable;
        IOFireWireSBP2LibMgmtORB *obj;
    };
	
	IOFireWireSBP2LibMgmtORB( void );
	virtual ~IOFireWireSBP2LibMgmtORB();
	
	virtual IOReturn init( io_connect_t connection, mach_port_t asyncPort );
	
protected:

	//////////////////////////////////////
	// cf plugin interfaces
	
	static IOFireWireSBP2LibMgmtORBInterface	sIOFireWireSBP2LibMgmtORBInterface;
	InterfaceMap								fIOFireWireSBP2LibMgmtORBInterface;

	//////////////////////////////////////
	// cf plugin ref counting
	
	UInt32 			fRefCount;
	
	//////////////////////////////////////
	// user client connection
	
	io_connect_t 	fConnection;	// connection to user client in kernel
	mach_port_t 	fAsyncPort;		// async port for callback from kernel
	uint64_t 		fMgmtORBRef;  	// reference to kernel orb object

	IOFWSBP2ORBAppendCallback		fORBCallbackRoutine;
	void *							fORBCallbackRefCon;

	void *			fRefCon;
	
	// utility function to get "this" pointer from interface
	static inline IOFireWireSBP2LibMgmtORB *getThis( void *self )
        { return (IOFireWireSBP2LibMgmtORB *) ((InterfaceMap *) self)->obj; };

	//////////////////////////////////////	
	// IUnknown static methods
	
	static HRESULT staticQueryInterface( void * self, REFIID iid, void **ppv );
	virtual HRESULT queryInterface( REFIID iid, void **ppv );

	static UInt32 staticAddRef( void * self );
	virtual UInt32 addRef( void );

	static UInt32 staticRelease( void * self );
	virtual UInt32 release( void );

	//////////////////////////////////////	
	// IOFireWireSBP2LibMgmtORB static methods
	static IOReturn staticSubmitORB( void * self );
	virtual IOReturn submitORB( void );

	static void staticSetORBCallback( void * self, void * refCon, 
												IOFWSBP2ORBAppendCallback callback );
	virtual void setORBCallback( void * refCon, IOFWSBP2ORBAppendCallback callback );

	static void staticSetRefCon( void * self, void * refCon );
	virtual void setRefCon( void * refCon );

	static void * staticGetRefCon( void * self );
	virtual void * getRefCon( void );

    static IOReturn staticSetCommandFunction( void * self, UInt32 function );
    virtual IOReturn setCommandFunction( UInt32 function );

	static IOReturn staticSetManageeORB( void * self, void * orb );
	virtual IOReturn setManageeORB( void * orb );

	static IOReturn staticSetManageeLogin( void * self, void * login );
	virtual IOReturn setManageeLogin( void * login );

	static IOReturn staticSetResponseBuffer( void * self, void * buf, UInt32 len );
	virtual IOReturn setResponseBuffer( void * buf, UInt32 len );

	//////////////////////////////////////
	// callback static methods
	
	static void staticORBCompletion( void *refcon, IOReturn result, io_user_reference_t *args, int numArgs );
	virtual void ORBCompletion( IOReturn result, io_user_reference_t *args, int numArgs );

public:
	
	static IUnknownVTbl **alloc( io_connect_t connection, mach_port_t asyncPort );

};
#endif
