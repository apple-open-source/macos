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

#import "IOFWUserIsochPort.h"
#import "IOFWIsochPort.h"
#import "IOFireWireBus.h"
#import "IOFireWireUserClient.h"

class IOFWUserIsochPort: public IOFWIsochPort
{
	OSDeclareDefaultStructors(IOFWUserIsochPort)

 public:	
	virtual bool	init() ;

	// Return maximum speed and channels supported
	// (bit n set = chan n supported)
    virtual IOReturn getSupported(IOFWSpeed &maxSpeed, UInt64 &chanSupported) ;

	// Allocate hardware resources for port
    virtual IOReturn allocatePort(IOFWSpeed speed, UInt32 chan) ;
    virtual IOReturn releasePort() ;	// Free hardware resources
    virtual IOReturn start() ;			// Start port processing packets
    virtual IOReturn stop() ;			// Stop processing packets

 protected:
	IOFWSpeed	fMaxSpeed ;
	UInt32		fChanSupported ;
} ;

class IOFWUserIsochPortProxy: public OSObject
{
	OSDeclareDefaultStructors(IOFWUserIsochPortProxy)
	
 public:
	virtual Boolean				init(
										IOFireWireUserClient*	inUserClient) ;

    virtual IOReturn getSupported(IOFWSpeed &maxSpeed, UInt64 &chanSupported) ;

    virtual IOReturn allocatePort(IOFWSpeed speed, UInt32 chan) ;
    virtual IOReturn releasePort() ;	// Free hardware resources
    virtual IOReturn start() ;			// Start port processing packets
    virtual IOReturn stop() ;			// Stop processing packets

	const IOFWIsochPort* 	getPort() const { return fPort; }
	virtual IOReturn		createPort() ;
	
 protected:
	IOFireWireUserClient*	fUserClient ;
	IOFWIsochPort*			fPort ;
	Boolean					fPortStarted ;
	Boolean					fPortAllocated ;
    IOFireWireBus::DCLTaskInfo fTaskInfo;
	
	virtual void				free() ;

} ;

#pragma mark -
class IOFWUserLocalIsochPortProxy: public IOFWUserIsochPortProxy
{
	OSDeclareDefaultStructors(IOFWUserLocalIsochPortProxy)
	
 public:

	virtual Boolean				initWithUserDCLProgram(
										LocalIsochPortAllocateParams*	inParams,
										IOFireWireUserClient*		inUserClient) ;

    virtual IOReturn 			getSupported(
										IOFWSpeed&					maxSpeed, 
										UInt64&						chanSupported) ;
    virtual IOReturn 			allocatePort(
										IOFWSpeed 					speed, 
										UInt32 						chan) ;
	virtual	IOReturn			releasePort() ;
	virtual IOReturn			stop() ;

	// --- utility functions ----------
	static Boolean				getDCLDataBuffer(
										const DCLCommand*			dcl,
										IOVirtualAddress*			outDataBuffer,
										IOByteCount*				outDataLength) ;
	static void					setDCLDataBuffer(
										DCLCommand*					dcl,
										IOVirtualAddress			inDataBuffer,
										IOByteCount					inDataLength) ;
	static IOByteCount			getDCLSize(
										DCLCommand*					dcl) ;
	static void					printDCLProgram(
										const DCLCommand*			dcl,
										UInt32						inDCLCount) ;
	virtual IOReturn			convertToKernelDCL(
										DCLUpdateDCLListStruct*		inDCLCommand,
										DCLCommand*					inUserDCLTable[],
										DCLCommand*					inUserToKernelDCLLookupTable[],
										UInt32						inLookupTableLength,
										UInt32&						inOutHint ) ;
	virtual IOReturn			convertToKernelDCL(
										DCLJumpStruct*				inDCLCommand,
										DCLCommand*					inUserDCLTable[],
										DCLCommand*					inUserToKernelDCLLookupTable[],
										UInt32						inLookupTableLength,
										UInt32&						inOutHint ) ;
	virtual IOReturn			convertToKernelDCL(
										DCLCallProcStruct*			inDCLCommand,
										DCLCommand*					inUserDCL ) ;
	static	Boolean				findOffsetInRanges(
										IOVirtualAddress			inAddress,
										IOVirtualRange				inRanges[],
										UInt32						inRangeCount,
										IOByteCount*				outOffset) ;
	static Boolean				userToKernLookup( 
										DCLCommand*					inDCLCommand,
										DCLCommand*					inUserDCLList[],
										DCLCommand*					inKernDCLList[],
										UInt32						inTableLength,
										UInt32&						inOutHint,
										DCLCommand**				outDCLCommand ) ;
	static	void				dclCallProcHandler(
										DCLCommand*					pDCLCommand) ;
	virtual IOReturn			setAsyncRef_DCLCallProc( OSAsyncReference asyncRef, DCLCallCommandProc* proc) ;
	virtual IOReturn			modifyJumpDCL(
										UInt32						inJumpDCLCompilerData,
										UInt32						inLabelDCLCompilerData) ;
	virtual IOReturn			modifyJumpDCLSize( UInt32 inDCLCompilerData, IOByteCount newSize ) ;

protected:
	IOMemoryDescriptor*		fUserDCLProgramMem ;
	IOByteCount				fDCLProgramBytes ;
	IOMemoryDescriptor*		fUserBufferMem ;
	Boolean					fUserBufferMemPrepared ;
	IOMemoryMap*			fUserBufferMemMap ;
	Boolean					fUserDCLProgramMemPrepared ;
	
	UInt8*					fKernDCLProgramBuffer ;
	DCLCommand*				fKernDCLProgramStart ;
	
	Boolean					fTalking ;
	UInt32					fStartState ;
	UInt32					fStartMask ;
	UInt32					fStartEvent ;

	// lookup table
	UInt32					fUserToKernelDCLLookupTableLength ;
	DCLCommand**			fUserToKernelDCLLookupTable ;
	OSAsyncReference		fStopTokenAsyncRef ;
	void*					fUserObj ;
	
	virtual void			free() ;
	virtual IOReturn		createPort() ;
} ;


#endif //_IOKIT_IOFWUserIsochPortProxy_H
