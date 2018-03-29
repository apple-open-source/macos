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

class IOFWUserVectorCommand;

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
	virtual void				free(void) APPLE_KEXT_OVERRIDE;
	
	virtual void				setAsyncReference64(
										OSAsyncReference64		inAsyncRef) ;	
	OSAsyncReference64 *		getAsyncReference64( void ) { return &fAsyncRef; }
	
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

	void						setFlush( bool flush ) 
										{ fFlush = flush; }
										
	void						setRefCon( mach_vm_address_t refcon )		
										{ fRefCon = refcon; }
	mach_vm_address_t			getRefCon( void )		
										{ return fRefCon; }
	
	void						setVectorCommand( IOFWUserVectorCommand * vector ) 
										{ fVectorCommand = vector; }
	
	virtual IOFWAsyncCommand *		getAsyncCommand( void ) { return fCommand;  }
										
protected:
	OSAsyncReference64				fAsyncRef ;
	IOFWAsyncCommand*				fCommand ;
	const IOFireWireUserClient*		fUserClient ;

	IOMemoryDescriptor*				fMem ;
	UInt32 *						fOutputArgs;
	UInt32							fOutputArgsSize;
	UInt32 *						fQuads ;
	UInt32							fNumQuads ;
	Boolean							fCopyFlag ;
	bool							fFlush;
	mach_vm_address_t				fRefCon;
	IOFWUserVectorCommand *			fVectorCommand;
} ;

class IOFWUserReadCommand: public IOFWUserCommand
{
	OSDeclareDefaultStructors(IOFWUserReadCommand)

public:
	// --- init's --------------------------------
	virtual bool				initWithSubmitParams(
										const CommandSubmitParams*	inParams,
										const IOFireWireUserClient*			inUserClient ) APPLE_KEXT_OVERRIDE;

	// --- IOFWCommand methods -------------------
	virtual IOReturn			submit(
										CommandSubmitParams*	inParams,
										CommandSubmitResult*	outResult) APPLE_KEXT_OVERRIDE;
} ;

class IOFWUserWriteCommand: public IOFWUserCommand
{
	OSDeclareDefaultStructors(IOFWUserWriteCommand)

public:
	// --- init's --------------------------------
	virtual bool				initWithSubmitParams(
										const CommandSubmitParams*	inParams,
										const IOFireWireUserClient*			inUserClient ) APPLE_KEXT_OVERRIDE;

	// --- IOFWCommand methods -------------------
	virtual IOReturn			submit(
										CommandSubmitParams*		inParams,
										CommandSubmitResult*		outResult) APPLE_KEXT_OVERRIDE;

} ;

class IOFWUserPHYCommand: public IOFWUserCommand
{
	OSDeclareDefaultStructors(IOFWUserPHYCommand)

protected:
	IOFWAsyncPHYCommand *		fPHYCommand;

	static void		asyncPHYCommandCompletion(	void *					refcon, 
												IOReturn 				status, 
												IOFireWireBus *			device, 
												IOFWAsyncPHYCommand *	fwCmd );
		
public:
	// --- init's --------------------------------
	virtual bool				initWithSubmitParams(
										const CommandSubmitParams*	inParams,
										const IOFireWireUserClient*			inUserClient ) APPLE_KEXT_OVERRIDE;

	virtual void				free(void) APPLE_KEXT_OVERRIDE;
	
	// --- IOFWCommand methods -------------------
	virtual IOReturn			submit(
										CommandSubmitParams*		inParams,
										CommandSubmitResult*		outResult) APPLE_KEXT_OVERRIDE;

	virtual IOFWAsyncPHYCommand *		getAsyncPHYCommand( void ) { return fPHYCommand;  }
};

class IOFWUserCompareSwapCommand: public IOFWUserCommand
{
	OSDeclareDefaultStructors(IOFWUserCompareSwapCommand)

	public:
		// --- init's --------------------------------
		virtual bool				initWithSubmitParams(
											const CommandSubmitParams*	inParams,
											const IOFireWireUserClient*			inUserClient ) APPLE_KEXT_OVERRIDE;
		
		// --- IOFWCommand methods -------------------
		virtual IOReturn			submit( CommandSubmitParams* params, CommandSubmitResult* result ) APPLE_KEXT_OVERRIDE;
		IOReturn					submit( CommandSubmitParams* params, CompareSwapSubmitResult* result ) ;
		static void					asyncCompletion( void* refcon, IOReturn status, IOFireWireNub* device, 
											IOFWCommand* fwCmd) ;
	
	protected:
		IOByteCount		fSize ;
} ;

class IOFWUserAsyncStreamCommand: public IOFWUserCommand
{
	OSDeclareDefaultStructors(IOFWUserAsyncStreamCommand)

protected:
	IOFWAsyncStreamCommand *		fAsyncStreamCommand;

	static void 	asyncStreamCommandCompletion(void						*refcon, 
												 IOReturn					status, 
												 IOFireWireBus				*bus,
												 IOFWAsyncStreamCommand		*fwCmd );
		
public:
	// --- init's --------------------------------
	virtual bool				initWithSubmitParams(
										const CommandSubmitParams*	inParams,
										const IOFireWireUserClient*	inUserClient ) APPLE_KEXT_OVERRIDE;

	virtual void				free(void) APPLE_KEXT_OVERRIDE;
	
	// --- IOFWCommand methods -------------------
	virtual IOReturn			submit(
										CommandSubmitParams*		inParams,
										CommandSubmitResult*		outResult) APPLE_KEXT_OVERRIDE;

	virtual IOFWAsyncStreamCommand *		getAsyncStreamCommand( void ) { return fAsyncStreamCommand;  }
};
