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
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All rights reserved.
 *
 * IOFWIsochPort is an abstract object that represents hardware on the bus
 * (locally or remotely) that sends or receives isochronous packets.
 * Local ports are implemented by the local device driver,
 * Remote ports are implemented by the driver for the remote device.
 *
 * HISTORY
 * $Log: not supported by cvs2svn $
 * Revision 1.3  2003/07/21 06:52:59  niels
 * merge isoch to TOT
 *
 * Revision 1.1.14.2  2003/07/21 06:44:45  niels
 * *** empty log message ***
 *
 * Revision 1.1.14.1  2003/07/01 20:54:07  niels
 * isoch merge
 *
 *
 */


#ifndef _IOKIT_IOFIREWIREIRM_H
#define _IOKIT_IOFIREWIREIRM_H

#include <libkern/c++/OSObject.h>

#include <IOKit/firewire/IOFireWireController.h>
#include <IOKit/firewire/IOFireWireFamilyCommon.h>

class IOFireWireIRM : public OSObject
{
	OSDeclareAbstractStructors(IOFireWireIRM)

protected:
	
	IOFireWireController * 	fControl;
	
	UInt16			fIRMNodeID;
	UInt16			fOurNodeID;
	UInt32			fGeneration;
		
	// channel allocation	
	UInt32			fNewChannelsAvailable31_0;
	UInt32			fOldChannelsAvailable31_0;

	IOFWCompareAndSwapCommand * fLockCmd;
	bool						fLockCmdInUse;
	UInt32						fLockRetries;
	
	// broadcast channel register
	UInt32						fBroadcastChannelBuffer;
	IOFWPseudoAddressSpace *	fBroadcastChannelAddressSpace;	
	
public:

	static IOFireWireIRM * create( IOFireWireController * controller );

    virtual bool initWithController( IOFireWireController * control );
    virtual void free( void );

	virtual bool isIRMActive( void );
	virtual void processBusReset( UInt16 ourNodeID, UInt16 irmNodeID, UInt32 generation );

protected:

	static void lockCompleteStatic( void *refcon, IOReturn status, IOFireWireNub *device, IOFWCommand *fwCmd );
	virtual void lockComplete( IOReturn status );
	virtual void allocateBroadcastChannel( void );
	
};

#endif
