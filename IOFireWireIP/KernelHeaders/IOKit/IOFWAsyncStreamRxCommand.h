/*
 * Copyright (c) 1998-2001 Apple Computer, Inc. All rights reserved.
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
#ifndef _IOKIT_IOFWASYNCSTREAMRXCOMMAND_H
#define _IOKIT_IOFWASYNCSTREAMRXCOMMAND_H

#define FIREWIREPRIVATE

#include <IOKit/firewire/IOFireWireLink.h>
#include <IOKit/firewire/IOFWCommand.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/firewire/IOFWDCLProgram.h>

#include "IOIPPort.h"

#define MAX_BCAST_BUFFERS 9

typedef struct
{
	void	*obj;
    void	*thisObj;
	UInt8	*buffer;
    UInt16	index;
} RXProcData ;

// Structure variables for each segment
typedef struct IPRxSegment
{
	DCLLabelPtr pSegmentLabelDCL;
	DCLJumpPtr pSegmentJumpDCL;
}IPRxSegment, *IPRxSegmentPtr;
	
/*! @class IOFWIPAsyncWriteCommand
*/
class IOFWAsyncStreamRxCommand : public OSObject
{
    OSDeclareDefaultStructors(IOFWAsyncStreamRxCommand)

protected:
	IOBufferMemoryDescriptor	*fBufDesc;
	DCLCommandStruct			*fdclProgram;
	IOIPPort					*fAsynStreamPort;
	IOFWIsochChannel			*fAsyncStreamChan;
    IODCLProgram 				*fIODclProgram;
    IOFireWireController		*fControl;
	IOFireWireLink				*fFWIM;
	bool						bStarted;
    bool						fInitialized;
	UInt32						fChan;
    IOFWSpeed					fSpeed;
    
    UInt32						fSeg;
    IPRxSegmentPtr				receiveSegmentInfo;
	DCLLabel					*fDCLOverrunLabel;
	UInt16 						fIsoRxOverrun;
	
	// FIREWIRETODO : TBR when we have original Async stream packet receive
	enum
	{
		kMaxIsochPacketSize		= 4096,
	};
	
    
/*! @struct ExpansionData
    @discussion This structure will be used to expand the capablilties of the class in the future.
    */    
    struct ExpansionData { };

/*! @var reserved
    Reserved for future use.  (Internal use only)  */
    ExpansionData *reserved;
    
    virtual void		free();
    
	DCLCommandStruct *CreateAsyncStreamRxDCLProgram(DCLCallCommandProc* proc, UInt32 size, void *callbackObject);
	
	IOIPPort *CreateAsyncStreamPort(bool talking, DCLCommandStruct *opcodes, void *info,
									UInt32 startEvent, UInt32 startState, UInt32 startMask,
									UInt32 channel);
									
	void FreeAsyncStreamRxDCLProgram(DCLCommandStruct *dclProgram);
	
public:

	/*!
        @function initAll
		Initializes the Asynchronous write command object
        @result true if successfull.
    */
	bool initAll(UInt32					channel, 
				DCLCallCommandProc		*proc, 
				IOFireWireController	*control,
				UInt32					size,
				void					*callbackObject);
											
	IOReturn start(IOFWSpeed fBroadCastSpeed);	
	IOReturn stop();
    static void restart(DCLCommandStruct *callProc);
    void fixDCLJumps(bool bRestart);
    void modifyDCLJumps(DCLCommandStruct *callProc);
	UInt16 getOverrunCounter() {return fIsoRxOverrun;};

	
private:
    OSMetaClassDeclareReservedUnused(IOFWAsyncStreamRxCommand, 0);
    OSMetaClassDeclareReservedUnused(IOFWAsyncStreamRxCommand, 1);

};

#endif // _IOKIT_IOFWASYNCSTREAMRXCOMMAND_H

