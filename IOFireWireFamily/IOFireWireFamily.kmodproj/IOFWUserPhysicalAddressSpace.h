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
 *  IOFWUserClientPhysAddrSpace.h
 *  IOFireWireFamily
 *
 *  Created by NWG on Fri Dec 08 2000.
 *  Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 */

#ifndef _IOKIT_IOFWUserClientPhysAddrSpace_H_
#define _IOKIT_IOFWUserClientPhysAddrSpace_H_

#import <IOKit/firewire/IOFWAddressSpace.h>
#import <IOKit/IOMemoryCursor.h>

class IOFWUserPhysicalAddressSpace: public IOFWPhysicalAddressSpace
{
	OSDeclareDefaultStructors(IOFWUserPhysicalAddressSpace)

	protected:
	
		UInt32				fSegmentCount ;
		bool				fMemPrepared ;

	public:
	
		virtual void		free() ;
		void				exporterCleanup () ;

		virtual bool 		initWithDesc(
									IOFireWireBus *			bus,
									IOMemoryDescriptor*		mem);
	
		// getters
		IOReturn			getSegmentCount( UInt32* outSegmentCount ) ;
		IOReturn			getSegments(
									UInt32*					ioSegmentCount,
									IOMemoryCursor::IOPhysicalSegment outSegments[] ) ;
} ;

#endif //_IOKIT_IOFWUserClientPhysAddrSpace_H_
