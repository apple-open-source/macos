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

#ifndef _IOKIT_IOFIREWIRESBP2LIBORB_H_
#define _IOKIT_IOFIREWIRESBP2LIBORB_H_

#include "IOFireWireSBP2LibLUN.h"
#include "IOFireWireSBP2UserClientCommon.h"

class IOFireWireSBP2LibORB
{

public:
	
	struct InterfaceMap 
	{
        IUnknownVTbl *pseudoVTable;
        IOFireWireSBP2LibORB *obj;
    };
	
	IOFireWireSBP2LibORB( void );
	virtual ~IOFireWireSBP2LibORB();
	
	virtual IOReturn init( io_connect_t connection, mach_port_t asyncPort );
	
protected:

	//////////////////////////////////////
	// cf plugin interfaces

	static IOFireWireSBP2LibORBInterface		sIOFireWireSBP2LibORBInterface;
	InterfaceMap								fIOFireWireSBP2LibORBInterface;

	//////////////////////////////////////
	// cf plugin ref counting
	
	UInt32 		fRefCount;
	
	//////////////////////////////////////
	// user client connection
	
	io_connect_t 	fConnection;	// connection to user client in kernel
	mach_port_t 	fAsyncPort;		// async port for callback from kernel
	UInt32 			fORBRef;  		// reference to kernel orb object

	UInt32			fRefCon;

	//////////////////////////////////////	
	// IUnknown static methods
	
	static HRESULT staticQueryInterface( void * self, REFIID iid, void **ppv );
	virtual HRESULT queryInterface( REFIID iid, void **ppv );

	static UInt32 staticAddRef( void * self );
	virtual UInt32 addRef( void );

	static UInt32 staticRelease( void * self );
	virtual UInt32 release( void );
	
	//////////////////////////////////////	
	// IOFireWireSBP2LibORB static methods

	static void staticSetRefCon( void * self, UInt32 refCon );
	virtual void setRefCon( UInt32 refCon );

	static UInt32 staticGetRefCon( void * self );
	virtual UInt32 getRefCon( void );

	static void staticSetCommandFlags( void * self, UInt32 flags );
	virtual void setCommandFlags( UInt32 flags );

	static void staticSetMaxORBPayloadSize( void * self, UInt32 size );
	virtual void setMaxORBPayloadSize( UInt32 size );

	static void staticSetCommandTimeout( void * self, UInt32 timeout );
	virtual void setCommandTimeout( UInt32 timeout );

	static void staticSetCommandGeneration( void * self, UInt32 generation );
	virtual void setCommandGeneration( UInt32 generation );

	static void staticSetToDummy( void * self );
	virtual void setToDummy( void );

    static IOReturn staticSetCommandBuffersAsRanges( void * self, FWSBP2VirtualRange * ranges, 
											UInt32 withCount, UInt32 withDirection, 
											UInt32 offset, UInt32 length );
    virtual IOReturn setCommandBuffersAsRanges( FWSBP2VirtualRange * ranges, UInt32 withCount,
											UInt32 withDirection, UInt32 offset, 
											UInt32 length );

    static IOReturn staticReleaseCommandBuffers( void * self );
    virtual IOReturn releaseCommandBuffers( void );

    static IOReturn staticSetCommandBlock( void * self, void * buffer, UInt32 length );
    virtual IOReturn setCommandBlock( void * buffer, UInt32 length );

    static IOReturn staticLSIWorkaroundSetCommandBuffersAsRanges
							( void * self, FWSBP2VirtualRange * ranges, UInt32 withCount,
									UInt32 withDirection, UInt32 offset, UInt32 length );
    virtual IOReturn LSIWorkaroundSetCommandBuffersAsRanges
								( FWSBP2VirtualRange * ranges, UInt32 withCount,
									UInt32 withDirection, UInt32 offset, UInt32 length );

	static IOReturn staticLSIWorkaroundSyncBuffersForOutput( void * self );
	virtual IOReturn LSIWorkaroundSyncBuffersForOutput( void );

    static IOReturn staticLSIWorkaroundSyncBuffersForInput( void * self );
    virtual IOReturn LSIWorkaroundSyncBuffersForInput( void );

public:

	// utility function to get "this" pointer from interface
	static inline IOFireWireSBP2LibORB *getThis( void *self )
        { return (IOFireWireSBP2LibORB *) ((InterfaceMap *) self)->obj; };

	static IUnknownVTbl **alloc( io_connect_t connection, mach_port_t asyncPort );

	virtual UInt32 getORBRef( void );
	
};

#endif