/*
 * Copyright (c) 2001-2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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

#ifndef __SCSI_TASK_DEVICE_USER_CLIENT_CLASS_H__
#define __SCSI_TASK_DEVICE_USER_CLIENT_CLASS_H__


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Includes
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

// IOKit includes
#include <IOKit/IOCFPlugIn.h>

// Private includes
#include "SCSITaskLib.h"
#include "SCSITaskLibPriv.h"
#include "SCSITaskIUnknown.h"


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Structures
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

struct MyConnectionAndPortContext
{
	io_connect_t	connection;
	mach_port_t		asyncPort;
};
typedef struct MyConnectionAndPortContext MyConnectionAndPortContext;


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Class Declarations
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

class SCSITaskDeviceClass : public SCSITaskIUnknown
{
	
	public:
		
		// Default constructor
		SCSITaskDeviceClass ( void );
		
		// Destructor
		virtual ~SCSITaskDeviceClass ( void );
		
		// This is an internal method which the SCSITaskInterface calls
		// when it is released to remove it from the device's taskSet
		virtual void RemoveTaskFromTaskSet ( SCSITaskInterface ** task );
		
		// Static allocation methods
		static IOCFPlugInInterface ** 		alloc ( void );
		static SCSITaskDeviceInterface ** 	alloc ( io_service_t service,
													io_connect_t connection );
		
		// Initialization methods
		virtual IOReturn InitWithConnection ( 	io_service_t	service,
												io_connect_t	connection );
		virtual IOReturn Init ( void );
		
	protected:
		
		static IOCFPlugInInterface				sIOCFPlugInInterface;
		static SCSITaskDeviceInterface			sSCSITaskDeviceInterface;
		struct InterfaceMap						fSCSITaskDeviceInterfaceMap;
		
		io_service_t 			fService;
		io_connect_t 			fConnection;
		bool					fHasExclusiveAccess;
		bool					fIsServicesLayerInterface;
		CFMutableSetRef			fTaskSet;
		
		mach_port_t 			fAsyncPort;
		CFRunLoopSourceRef		fCFRunLoopSource;
		CFRunLoopRef			fCFRunLoop;
		
		// utility function to get "this" pointer from interface
		static inline SCSITaskDeviceClass * getThis ( void * self )
			{ return ( SCSITaskDeviceClass * ) ( ( InterfaceMap * ) self )->obj; };
		
		// CFPlugIn/IOCFPlugIn stuff
		virtual HRESULT 	QueryInterface ( REFIID iid, void ** ppv );
		
		virtual IOReturn	Probe ( CFDictionaryRef propertyTable, io_service_t service, SInt32 * order );
		
		virtual IOReturn	Start ( CFDictionaryRef propertyTable, io_service_t service );
		
		virtual IOReturn	Stop ( void );
		
		virtual Boolean		IsExclusiveAccessAvailable ( void );
				
		virtual IOReturn 	AddCallbackDispatcherToRunLoop ( CFRunLoopRef cfRunLoopRef );
		
		virtual void 		RemoveCallbackDispatcherFromRunLoop ( void );
		
		virtual IOReturn 	ObtainExclusiveAccess ( void );
		
		virtual IOReturn 	ReleaseExclusiveAccess ( void );
		
		virtual SCSITaskInterface ** 	CreateSCSITask ( void );
		
		// New functions we havenÕt exported yet...
		virtual IOReturn			CreateDeviceAsyncEventSource ( CFRunLoopSourceRef * source );
		
		virtual CFRunLoopSourceRef  GetDeviceAsyncEventSource ( void );
		
		virtual IOReturn 			CreateDeviceAsyncPort ( mach_port_t * port );
		
		virtual mach_port_t 		GetDeviceAsyncPort ( void );
		
		// Static functions (C->C++ Glue Code)
		static IOReturn 			sProbe ( void * self, CFDictionaryRef propertyTable, io_service_t service, SInt32 * order );
		static IOReturn 			sStart ( void * self, CFDictionaryRef propertyTable, io_service_t service );
		static IOReturn 			sStop ( void * self );
		static Boolean				sIsExclusiveAccessAvailable ( void * self );
		static IOReturn				sCreateDeviceAsyncEventSource ( void * self, CFRunLoopSourceRef * source );
		static CFRunLoopSourceRef 	sGetDeviceAsyncEventSource ( void * self );
		static IOReturn 			sCreateDeviceAsyncPort ( void * self, mach_port_t * port );
		static mach_port_t 			sGetDeviceAsyncPort ( void * self );
		static IOReturn				sAddCallbackDispatcherToRunLoop ( void * self, CFRunLoopRef cfRunLoopRef );
		static void 				sRemoveCallbackDispatcherFromRunLoop ( void * self );
		static IOReturn				sObtainExclusiveAccess ( void * self );
		static IOReturn				sReleaseExclusiveAccess ( void * self );
		static SCSITaskInterface **	sCreateSCSITask ( void * self );

	private:
		
		// Disable Copying
		SCSITaskDeviceClass ( SCSITaskDeviceClass &src );
		void operator = ( SCSITaskDeviceClass &src );
				
};


#endif /* __SCSI_TASK_DEVICE_USER_CLIENT_CLASS_H__ */