/*
* Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
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
/*
*  IOFWUserCommand.h
*  IOFireWireFamily
*
*  Created by noggin on Tue May 08 2001.
*  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
*
*/

// public
#import <IOKit/firewire/IOFWCommand.h>

// private
#import "IOFireWireUserClient.h"

// system
#import <libkern/c++/OSObject.h>

class IOFWUserCommand: public OSObject
{
	OSDeclareAbstractStructors(IOFWUserCommand)

public:
	// --- IOKit constructors --------------------
	// --- with's --------------------------------
	static	IOFWUserCommand*	withSubmitParams(
										const CommandSubmitParams*	inParams,
										const IOFireWireUserClient*			inUserClient) ;

	// --- init's --------------------------------
	virtual bool				initWithSubmitParams(
										const CommandSubmitParams*	inParams,
										const IOFireWireUserClient*			inUserClient ) ;

	// --- free ----------------------------------										
	virtual void				free() ;
	virtual void				setAsyncReference(
										OSAsyncReference		inAsyncRef) ;	
	static void					asyncReadWriteCommandCompletion(
										void *					refcon, 
										IOReturn 				status, 
										IOFireWireNub *			device, 
										IOFWCommand *			fwCmd) ;
	static void					asyncReadQuadletCommandCompletion(
										void *					refcon, 
										IOReturn 				status, 
										IOFireWireNub *			device, 
										IOFWCommand *			fwCmd) ;
	virtual IOReturn			submit(
										CommandSubmitParams*	inParams,
										CommandSubmitResult*	outResult) = 0 ;

protected:
	OSAsyncReference				fAsyncRef ;
	IOFWAsyncCommand*				fCommand ;
	const IOFireWireUserClient*		fUserClient ;

	IOMemoryDescriptor*				fMem ;
	UInt32*							fQuads ;
	UInt32							fNumQuads ;
	Boolean							fCopyFlag ;
} ;

class IOFWUserReadCommand: public IOFWUserCommand
{
	OSDeclareDefaultStructors(IOFWUserReadCommand)

public:
	// --- init's --------------------------------
	virtual bool				initWithSubmitParams(
										const CommandSubmitParams*	inParams,
										const IOFireWireUserClient*			inUserClient ) ;

	// --- IOFWCommand methods -------------------
	virtual IOReturn			submit(
										CommandSubmitParams*	inParams,
										CommandSubmitResult*	outResult) ;
} ;

class IOFWUserWriteCommand: public IOFWUserCommand
{
	OSDeclareDefaultStructors(IOFWUserWriteCommand)

public:
	// --- init's --------------------------------
	virtual bool				initWithSubmitParams(
										const CommandSubmitParams*	inParams,
										const IOFireWireUserClient*			inUserClient ) ;

	// --- IOFWCommand methods -------------------
	virtual IOReturn			submit(
										CommandSubmitParams*		inParams,
										CommandSubmitResult*		outResult) ;

} ;

class IOFWUserCompareSwapCommand: public IOFWUserCommand
{
	OSDeclareDefaultStructors(IOFWUserCompareSwapCommand)

	public:
		// --- init's --------------------------------
		virtual bool				initWithSubmitParams(
											const CommandSubmitParams*	inParams,
											const IOFireWireUserClient*			inUserClient ) ;
		
		// --- IOFWCommand methods -------------------
		virtual IOReturn			submit( CommandSubmitParams* params, CommandSubmitResult* result ) ;
		IOReturn					submit( CommandSubmitParams* params, CompareSwapSubmitResult* result ) ;
		static void					asyncCompletion( void* refcon, IOReturn status, IOFireWireNub* device, 
											IOFWCommand* fwCmd) ;
	
	protected:
		IOByteCount		fSize ;
} ;
