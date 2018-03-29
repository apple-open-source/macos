/*
 * Copyright (c) 1998-2007 Apple Computer, Inc. All rights reserved.
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

#ifndef _IOKIT_IOFWUSERVECTORCOMMAND_H_
#define _IOKIT_IOFWUSERVECTORCOMMAND_H_

// private
#import "IOFireWireUserClient.h"

// system
#import <libkern/c++/OSObject.h>

#pragma mark -

class IOFWUserVectorCommand : public OSObject
{
	OSDeclareDefaultStructors( IOFWUserVectorCommand );
	
	protected:
		IOFireWireUserClient *		fUserClient;
		IOFireWireController *		fControl;
		
		IOMemoryDescriptor *		fSubmitDesc;
		IOMemoryDescriptor *		fResultDesc;
		
		int							fInflightCmds;
		mach_vm_size_t				fResultOffset;
		
		OSAsyncReference64			fAsyncRef;
		IOReturn					fVectorStatus;
		
	public:
	
		// factory
		static IOFWUserVectorCommand *		withUserClient( IOFireWireUserClient * inUserClient );
		
		// ctor/dtor
		virtual bool	initWithUserClient( IOFireWireUserClient * inUserClient );
		virtual void	free( void ) APPLE_KEXT_OVERRIDE;

		IOReturn		setBuffers(	mach_vm_address_t submit_buffer_address, mach_vm_size_t submit_buffer_size,
									mach_vm_address_t result_buffer_address, mach_vm_size_t result_buffer_size );
		IOReturn		submit( OSAsyncReference64 async_ref, mach_vm_address_t callback, io_user_reference_t refCon );

		void			asyncCompletion(	void *					refcon, 
											IOReturn 				status, 
											IOFireWireNub *			device, 
											IOFWCommand *			fwCmd );

		void			asyncPHYCompletion(	void *					refcon, 
											IOReturn 				status, 
											IOFireWireBus *			bus, 
											IOFWAsyncPHYCommand *	fwCmd );
	
	protected:
		IOReturn		submitOneCommand( CommandSubmitParams * params );
		
};

#endif
