/*
 *  IOFireWireLocalNode.h
 *  IOFireWireFamily
 *
 *  Created by Niels on Fri Aug 16 2002.
 *  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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
	$Log: IOFireWireLocalNode.h,v $
	Revision 1.3  2002/10/18 23:29:44  collin
	fix includes, fix cast which fails on new compiler
	
	Revision 1.2  2002/09/25 00:27:24  niels
	flip your world upside-down
	
*/

#import <IOKit/firewire/IOFireWireNub.h>

class IOFireWireLocalNode : public IOFireWireNub
{
    OSDeclareDefaultStructors(IOFireWireLocalNode)

	/*------------------Useful info about device (also available in the registry)--------*/
	protected:

	/*-----------Methods provided to FireWire device clients-------------*/
	public:
	
		// Set up properties affected by bus reset
		virtual void setNodeProperties(UInt32 generation, UInt16 nodeID, UInt32 *selfIDs, int numIDs);
		
		/*
		* Standard nub initialization
		*/
		virtual bool init(OSDictionary * propTable);
		virtual bool attach(IOService * provider );
	
		virtual void handleClose(   IOService *	  forClient,
								IOOptionBits	  options ) ;
		virtual bool handleOpen( 	IOService *	  forClient,
								IOOptionBits	  options,
								void *		  arg ) ;
		virtual bool handleIsOpen(  const IOService * forClient ) const;
	
		/*
		* Trick method to create protocol user clients
		*/
		virtual IOReturn setProperties( OSObject * properties );
	
	protected:
		UInt32	fOpenCount ;
};
