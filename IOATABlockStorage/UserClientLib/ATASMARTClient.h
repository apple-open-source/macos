/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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

#ifndef __ATA_SMART_CLIENT_H__
#define __ATA_SMART_CLIENT_H__

//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Includes
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

// IOKit includes
#include <IOKit/IOCFPlugIn.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void * ATASMARTLibFactory ( CFAllocatorRef allocator, CFUUIDRef typeID );

#ifdef __cplusplus
}
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Class Declarations
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

class ATASMARTClient
{
	
	public:
		
		// Default constructor
		ATASMARTClient ( );
		
		typedef struct InterfaceMap
		{
			IUnknownVTbl *		pseudoVTable;
			ATASMARTClient *	obj;
		} InterfaceMap;
		
	private:
		
		// Disable copy constructor
		ATASMARTClient ( ATASMARTClient& src );
		void operator = ( ATASMARTClient& src );
			
	protected:

		// utility function to get "this" pointer from interface
		static inline ATASMARTClient * getThis ( void * self )
			{ return ( ATASMARTClient * ) ( ( InterfaceMap * ) self )->obj; };
		
		// Static functions (C->C++ Glue Code)
		static UInt32		sFactoryRefCount;
		static void			sFactoryAddRef ( void );
		static void			sFactoryRelease ( void );
		
		static HRESULT		sQueryInterface ( void * self, REFIID iid, void ** ppv );
		static UInt32		sAddRef ( void * self );
		static UInt32		sRelease ( void * self );
		
		static IOReturn 	sProbe ( void * self, CFDictionaryRef propertyTable, io_service_t service, SInt32 * order );
		static IOReturn 	sStart ( void * self, CFDictionaryRef propertyTable, io_service_t service );
		static IOReturn 	sStop ( void * self );
				
		// Destructor
		virtual ~ATASMARTClient ( void );
		
		virtual IOReturn	Probe ( CFDictionaryRef propertyTable, io_service_t service, SInt32 * order );
		virtual IOReturn	Start ( CFDictionaryRef propertyTable, io_service_t service );
		virtual IOReturn	Stop ( void );
		
		static 	IOReturn	sSMARTEnableDisableOperations ( void * interface, Boolean enable );
		static 	IOReturn	sSMARTEnableDisableAutosave ( void * interface, Boolean enable );
		static 	IOReturn	sSMARTReturnStatus ( void * interface, Boolean * exceededCondition );
		static 	IOReturn 	sSMARTExecuteOffLineImmediate ( void * interface, Boolean extendedTest );
		static 	IOReturn 	sSMARTReadData ( void * interface, ATASMARTData * data );
		static 	IOReturn 	sSMARTValidateReadData ( void * interface, const ATASMARTData * data );
		static 	IOReturn 	sSMARTReadDataThresholds ( void * interface, ATASMARTDataThresholds * data );
		static 	IOReturn	sSMARTReadLogDirectory ( void * interface, ATASMARTLogDirectory * logData );
		static 	IOReturn	sSMARTReadLogAtAddress ( void * interface, UInt32 address, void * buffer, UInt32 size );
		static 	IOReturn	sSMARTWriteLogAtAddress ( void * interface, UInt32 address, const void * buffer, UInt32 size );

		virtual IOReturn	SMARTEnableDisableOperations ( Boolean enable );
		virtual IOReturn	SMARTEnableDisableAutosave ( Boolean enable );
		virtual IOReturn	SMARTReturnStatus ( Boolean * exceededCondition );
		virtual IOReturn 	SMARTExecuteOffLineImmediate ( Boolean extendedTest );
		virtual IOReturn 	SMARTReadData ( ATASMARTData * data );
		virtual IOReturn 	SMARTReadDataThresholds ( ATASMARTDataThresholds * data );
		virtual IOReturn	SMARTReadLogDirectory ( ATASMARTLogDirectory * logData );
		virtual IOReturn	SMARTReadLogAtAddress ( UInt32 address, void * buffer, UInt32 size );
		virtual IOReturn	SMARTWriteLogAtAddress ( UInt32 address, const void * buffer, UInt32 size );
		
		static IOCFPlugInInterface			sIOCFPlugInInterface;
		static IOATASMARTInterface			sATASMARTInterface;
		
		UInt32								fRefCount;
		InterfaceMap						fCFPlugInInterfaceMap;
		InterfaceMap						fATASMARTInterfaceMap;
		io_service_t						fService;
		io_connect_t						fConnection;
		
	public:
		
		// Static allocation methods
		static IOCFPlugInInterface ** 		alloc ( void );
				
		// Subclasses must add this method.
		virtual HRESULT	QueryInterface ( REFIID iid, void ** ppv );
		virtual UInt32	AddRef ( void );
		virtual UInt32	Release ( void );
		
};

#endif	/* __ATA_SMART_CLIENT_H__ */