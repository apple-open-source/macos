/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
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
/*
 *  IOFWUserIsochPortProxy.h
 *  IOFireWireFamily
 *
 *  Created by NWG on Tue Mar 20 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 */

#ifndef _IOKIT_IOFWUserIsochPortProxy_H
#define _IOKIT_IOFWUserIsochPortProxy_H

#import "IOFireWireLibPriv.h"

// public
#import <IOKit/firewire/IOFWLocalIsochPort.h>
#import <IOKit/IOLocks.h>
#import <IOKit/OSMessageNotification.h>

#pragma mark -

class IODCLProgram ;
class IOBufferMemoryDescriptor ;
class IOFireWireUserClient ;
class IOFWDCLPool ;

class IOFWUserLocalIsochPort : public IOFWLocalIsochPort
{
	OSDeclareDefaultStructors( IOFWUserLocalIsochPort )
	
	typedef ::IOFireWireLib::LocalIsochPortAllocateParams AllocateParams ;
	
	protected:
		
		IORecursiveLock*			fLock ;
		mach_vm_address_t			fUserObj ;
		IOFireWireUserClient *		fUserClient ;

		unsigned					fProgramCount ;
		DCLCommand **				fDCLTable ;		// lookup table
		OSAsyncReference64			fStopTokenAsyncRef ;

		UInt8*						fProgramBuffer ; // for old style programs
		IOFWDCLPool *				fDCLPool ;		// for new style programs
		bool						fStarted ;
		
	public:

		// OSObject
		virtual void				free () ;
#if IOFIREWIREDEBUG > 0
		virtual bool				serialize( OSSerialize * s ) const ;
#endif

		// IOFWLocalIsochPort
		virtual IOReturn			start () ;
		virtual IOReturn			stop () ;

		// me
		bool						initWithUserDCLProgram (	
											AllocateParams * 		params,
											IOFireWireUserClient &	userclient,
											IOFireWireController &	controller ) ;
		IOReturn					importUserProgram (
											IOMemoryDescriptor *	userExportDesc,
											unsigned 				bufferRangeCount, 
											IOAddressRange			userBufferRanges [],
											IOMemoryMap *			bufferMap ) ;
		static	void				s_dclCallProcHandler (
											DCLCallProc * 			dcl ) ;
		IOReturn					setAsyncRef_DCLCallProc ( 
											OSAsyncReference64 		asyncRef ) ;
		IOReturn					modifyJumpDCL ( 
											UInt32 					jumpCompilerData, 
											UInt32 					labelCompilerData ) ;
		IOReturn					modifyDCLSize ( 
											UInt32 					compilerData, 
											IOByteCount 			newSize ) ;	

		inline void					lock ()				{ IORecursiveLockLock ( fLock ) ; }
		inline void					unlock ()			{ IORecursiveLockUnlock ( fLock ) ; }

		IOReturn					convertToKernelDCL ( UserExportDCLUpdateDCLList *pUserExportDCL, DCLUpdateDCLList * dcl ) ;
		IOReturn					convertToKernelDCL ( UserExportDCLJump *pUserExportDCL, DCLJump * dcl ) ;
		IOReturn					convertToKernelDCL ( UserExportDCLCallProc *pUserExportDCL, DCLCallProc * dcl ) ;

		static void					exporterCleanup( const OSObject * self );
		static void					s_nuDCLCallout( void * refcon ) ;
		IOReturn 					userNotify (
											UInt32			notificationType,
											UInt32			numDCLs,
											void *			data,
											IOByteCount		dataSize ) ;
		IOWorkLoop *				createRealtimeThread() ;
} ;

#endif //_IOKIT_IOFWUserIsochPortProxy_H
