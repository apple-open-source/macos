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

#ifndef __SCSI_TASK_IUNKNOWN__H__
#define __SCSI_TASK_IUNKNOWN__H__


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Includes
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

// IOKit includes
#include <IOKit/IOCFPlugIn.h>


#ifdef __cplusplus
extern "C" {
#endif

extern void * SCSITaskLibFactory ( CFAllocatorRef allocator, CFUUIDRef typeID );

#ifdef __cplusplus
}
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Class Declarations
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ


class SCSITaskIUnknown
{
	
	public:
		
		struct InterfaceMap
		{
			IUnknownVTbl *		pseudoVTable;
			SCSITaskIUnknown *	obj;
		};
		
	private:
		
		// Disable copy constructor
		SCSITaskIUnknown ( SCSITaskIUnknown& src );
		void operator = ( SCSITaskIUnknown& src );
		
		// Default constructor
		SCSITaskIUnknown ( ) : fRefCount ( 1 ) { };
		
		UInt32			fRefCount;
		
	protected:
		
		// These maintain the SCSITaskLib factory.
		static UInt32	sFactoryRefCount;
		static void		sFactoryAddRef ( void );
		static void		sFactoryRelease ( void );
		
		// Constructor used by subclasses
		SCSITaskIUnknown ( void * unknownVTable );
		
		// Destructor
		virtual ~SCSITaskIUnknown ( void );
		
		// Generic functions used by every subclass, but implemented here
		// to reduce the number of times it is written.
		static HRESULT	sQueryInterface ( void * self, REFIID iid, void ** ppv );
		static UInt32	sAddRef ( void * self );
		static UInt32	sRelease ( void * self );
		
		InterfaceMap	fInterfaceMap;
		
	public:
		
		// Subclasses must add this method.
		virtual HRESULT	QueryInterface ( REFIID iid, void ** ppv ) = 0;
		
		virtual UInt32	AddRef ( void );
		virtual UInt32	Release ( void );
		
};


#endif	/* __SCSI_TASK_IUNKNOWN__H__ */