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

#ifndef _IOKIT_IOFIREWIRESBP2LIBLUN_H_
#define _IOKIT_IOFIREWIRESBP2LIBLUN_H_

#include <IOKit/IOCFPlugIn.h>

#include <IOKit/sbp2/IOFireWireSBP2Lib.h>

__BEGIN_DECLS
void *IOFireWireSBP2LibFactory( CFAllocatorRef allocator, CFUUIDRef typeID );
__END_DECLS

class IOFireWireSBP2LibLUN
{

public:

	struct InterfaceMap 
	{
        IUnknownVTbl *pseudoVTable;
        IOFireWireSBP2LibLUN *obj;
    };
	
	IOFireWireSBP2LibLUN( void );
	virtual ~IOFireWireSBP2LibLUN();
	
protected:

	//////////////////////////////////////
	// cf plugin interfaces
	
	static IOCFPlugInInterface 				sIOCFPlugInInterface;
	InterfaceMap 			   				fIOCFPlugInInterface;
	static IOFireWireSBP2LibLUNInterface	sIOFireWireSBP2LibLUNInterface;
	InterfaceMap							fIOFireWireSBP2LibLUNInterface;

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
	
	mach_port_t 			fAsyncPort;
	CFMachPortRef			fCFAsyncPort;
	CFRunLoopRef			fCFRunLoop;
	CFRunLoopSourceRef		fCFRunLoopSource;
	IOFWSBP2MessageCallback	fMessageCallbackRoutine;
	void *					fMessageCallbackRefCon;
	IUnknownVTbl **			fLoginInterface;
	
	void *			fRefCon;

	// utility function to get "this" pointer from interface
	static inline IOFireWireSBP2LibLUN *getThis( void *self )
        { return (IOFireWireSBP2LibLUN *) ((InterfaceMap *) self)->obj; };

	//////////////////////////////////////	
	// IUnknown static methods
	
	static HRESULT staticQueryInterface( void * self, REFIID iid, void **ppv );
	virtual HRESULT queryInterface( REFIID iid, void **ppv );

	static UInt32 staticAddRef( void * self );
	virtual UInt32 addRef( void );

	static UInt32 staticRelease( void * self );
	virtual UInt32 release( void );
	
	//////////////////////////////////////
	// CFPlugin static methods
	
	static IOReturn staticProbe( void * self, CFDictionaryRef propertyTable, 
								 io_service_t service, SInt32 *order );
	virtual IOReturn probe( CFDictionaryRef propertyTable, io_service_t service, SInt32 *order );

    static IOReturn staticStart( void * self, CFDictionaryRef propertyTable, 
								 io_service_t service );
    virtual IOReturn start( CFDictionaryRef propertyTable, io_service_t service );

	static IOReturn staticStop( void * self );
	virtual IOReturn stop( void );

	//////////////////////////////////////
	// IOFireWireSBP2LUN static methods
	
	static IOReturn staticOpen( void * self );
	virtual IOReturn open( void );

	static IOReturn staticOpenWithSessionRef( void * self, IOFireWireSessionRef sessionRef );
	virtual IOReturn openWithSessionRef( IOFireWireSessionRef sessionRef );

	static IOFireWireSessionRef staticGetSessionRef(void * self);
	virtual IOFireWireSessionRef getSessionRef( void );

	static void staticClose( void * self );
	virtual void close( void );

	static IOReturn staticAddIODispatcherToRunLoop( void *self, CFRunLoopRef cfRunLoopRef );
	virtual IOReturn addIODispatcherToRunLoop( CFRunLoopRef cfRunLoopRef );

	static void staticRemoveIODispatcherFromRunLoop( void * self );
	virtual void removeIODispatcherFromRunLoop( void );

	static void staticSetMessageCallback( void * self, void * refCon, 
												IOFWSBP2MessageCallback callback );
	virtual void setMessageCallback( void * refCon, IOFWSBP2MessageCallback callback );

	static IUnknownVTbl ** staticCreateLogin( void * self, REFIID iid );
	virtual IUnknownVTbl ** createLogin( REFIID iid );

	static void staticSetRefCon( void * self, void * refCon );
	virtual void setRefCon( void * refCon );

	static void * staticGetRefCon( void * self );
	virtual void * getRefCon( void );

	static IUnknownVTbl ** staticCreateMgmtORB( void * self, REFIID iid );
	virtual IUnknownVTbl ** createMgmtORB( REFIID iid );

	//////////////////////////////////////											
	// callback static methods
	
	static void staticMessageCallback( void *refcon, IOReturn result, 
													io_user_reference_t *args, int numArgs );
	virtual void messageCallback( IOReturn result, io_user_reference_t *args, int numArgs );
	
public:

	static IOCFPlugInInterface **alloc( void );

};

#endif