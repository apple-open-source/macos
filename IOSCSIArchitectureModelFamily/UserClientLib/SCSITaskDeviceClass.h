/*
 * Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
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

#ifndef __SCSI_TASK_DEVICE_USER_CLIENT_CLASS_H__
#define __SCSI_TASK_DEVICE_USER_CLIENT_CLASS_H__

#include <IOKit/IOCFPlugIn.h>
#include "SCSITaskLib.h"
#include "SCSITaskLibPriv.h"

struct MyConnectionAndPortContext
{
	io_connect_t	connection;
	mach_port_t		asyncPort;
};
typedef struct MyConnectionAndPortContext MyConnectionAndPortContext;


class SCSITaskDeviceClass
{
	
	public:
	
		struct InterfaceMap 
		{
			IUnknownVTbl *	pseudoVTable;
			SCSITaskDeviceClass *	obj;
		};
		
		// Default constructor
		SCSITaskDeviceClass ( void );
		
		// Destructor
		virtual ~SCSITaskDeviceClass ( void );
		
		// This is an internal method which the SCSITaskInterface calls
		// when it is released to remove it from the device's taskSet
		virtual void RemoveTaskFromTaskSet ( SCSITaskInterface ** task );
		
	protected:
		//////////////////////////////////////
		// cf plugin interfaces
		
		static IOCFPlugInInterface 				sIOCFPlugInInterface;
		InterfaceMap 			   				fIOCFPlugInInterface;
		static SCSITaskDeviceInterface			sSCSITaskDeviceInterface;
		InterfaceMap							fSCSITaskDeviceInterfaceMap;
	
		//////////////////////////////////////
		// CFPlugIn refcounting
		
		CFUUIDRef 	fFactoryId;	
		UInt32 		fRefCount;
	
		//////////////////////////////////////	
		// user client connection
		
		io_service_t 	fService;
		io_connect_t 	fConnection;
		bool			fAddedConnectRef;
		bool			fHasExclusiveAccess;
		bool			fIsServicesLayerInterface;
		CFMutableSetRef	fTaskSet;
		
		//////////////////////////////////////	
		// async callback
		mach_port_t 				fAsyncPort;
		CFRunLoopRef				fCFRunLoop;
		CFRunLoopSourceRef			fCFRunLoopSource;
		
		// utility function to get "this" pointer from interface
		static inline SCSITaskDeviceClass * getThis ( void * self )
			{ return ( SCSITaskDeviceClass * ) ( ( InterfaceMap * ) self)->obj; };
	
		//////////////////////////////////////	
		// IUnknown static methods
		
		static HRESULT staticQueryInterface ( void * self, REFIID iid, void **ppv );
		virtual HRESULT QueryInterface ( REFIID iid, void **ppv );
	
		static UInt32 staticAddRef ( void * self );
		virtual UInt32 AddRef ( void );
	
		static UInt32 staticRelease ( void * self );
		virtual UInt32 Release ( void );
		
		//////////////////////////////////////
		// CFPlugin methods
		
		static IOReturn staticProbe ( void * self, CFDictionaryRef propertyTable, 
									  io_service_t service, SInt32 * order );
		virtual IOReturn Probe ( CFDictionaryRef propertyTable, io_service_t service, SInt32 * order );
		
		static IOReturn staticStart ( void * self, CFDictionaryRef propertyTable, io_service_t service );
		virtual IOReturn Start ( CFDictionaryRef propertyTable, io_service_t service );
		
		static IOReturn staticStop ( void * self );
		virtual IOReturn Stop ( void );
		
		//////////////////////////////////////
		// SCSITaskDeviceInterface methods
		
		static Boolean		staticIsExclusiveAccessAvailable ( void * self );
		virtual Boolean		IsExclusiveAccessAvailable ( void );
		
		static IOReturn		staticAddCallbackDispatcherToRunLoop ( void * self, CFRunLoopRef cfRunLoopRef );
		virtual IOReturn 	AddCallbackDispatcherToRunLoop ( CFRunLoopRef cfRunLoopRef );
		
		static void 		staticRemoveCallbackDispatcherFromRunLoop ( void * self );
		virtual void 		RemoveCallbackDispatcherFromRunLoop ( void );
		
		static IOReturn		staticObtainExclusiveAccess ( void * self );
		virtual IOReturn 	ObtainExclusiveAccess ( void );

		static IOReturn		staticReleaseExclusiveAccess ( void * self );
		virtual IOReturn 	ReleaseExclusiveAccess ( void );

		static SCSITaskInterface **		staticCreateSCSITask ( void * self );
		virtual SCSITaskInterface ** 	CreateSCSITask ( void );
		
	private:
		
		// Disable Copying
		SCSITaskDeviceClass ( SCSITaskDeviceClass &src );
		void operator = ( SCSITaskDeviceClass &src );
		
	public:
		
		static IOCFPlugInInterface ** alloc ( void );
		static SCSITaskDeviceInterface ** alloc ( io_service_t service, io_connect_t connection );
		
		virtual IOReturn initWithConnection ( io_service_t service, io_connect_t connection );
		virtual IOReturn init ( void );
		
};


#endif /* __SCSI_TASK_DEVICE_USER_CLIENT_CLASS_H__ */