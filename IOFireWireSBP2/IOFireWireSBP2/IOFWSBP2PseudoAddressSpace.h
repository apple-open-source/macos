/*
 * Copyright (c) 1998-2003 Apple Computer, Inc. All rights reserved.
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
 
#ifndef _IOKIT_IOFWSBP2PSEUDOADDRESSSPACE_H
#define _IOKIT_IOFWSBP2PSEUDOADDRESSSPACE_H

#include <IOKit/firewire/IOFWPseudoAddressSpace.h>

#pragma mark -

class IOFireWireUnit;

/*! 
	@class IOFWSBP2PseudoAddressSpace
*/

class IOFWSBP2PseudoAddressSpace : public IOFWPseudoAddressSpace
{
    OSDeclareDefaultStructors(IOFWSBP2PseudoAddressSpace)
	
protected:
	
	/*! 
		@struct ExpansionData
		@discussion This structure will be used to expand the capablilties of the class in the future.
    */  
	  
    struct ExpansionData { };

	/*! 
		@var reserved
		Reserved for future use.  (Internal use only)  
	*/
    
	ExpansionData *reserved;

public:

	virtual void setAddressLo( UInt32 addressLo );

	static IOFWSBP2PseudoAddressSpace * simpleRead(	IOFireWireBus *	control,
													FWAddress *		addr, 
													UInt32 			len, 
													const void *	data );

	static IOFWSBP2PseudoAddressSpace * simpleRW(	IOFireWireBus *	control,
													FWAddress *		addr, 
													UInt32 			len, 
													void *			data );
																											
	static IOFWSBP2PseudoAddressSpace * createPseudoAddressSpace(	IOFireWireBus * control,
																	IOFireWireUnit * unit,
																	FWAddress *		addr, 
																	UInt32 			len, 
																	FWReadCallback 	reader, 
																	FWWriteCallback	writer, 
																	void *			refcon );
															
private:
    OSMetaClassDeclareReservedUnused(IOFWSBP2PseudoAddressSpace, 0);
    OSMetaClassDeclareReservedUnused(IOFWSBP2PseudoAddressSpace, 1);
    OSMetaClassDeclareReservedUnused(IOFWSBP2PseudoAddressSpace, 2);
    OSMetaClassDeclareReservedUnused(IOFWSBP2PseudoAddressSpace, 3);
    OSMetaClassDeclareReservedUnused(IOFWSBP2PseudoAddressSpace, 4);
    OSMetaClassDeclareReservedUnused(IOFWSBP2PseudoAddressSpace, 5);
    OSMetaClassDeclareReservedUnused(IOFWSBP2PseudoAddressSpace, 6);
    OSMetaClassDeclareReservedUnused(IOFWSBP2PseudoAddressSpace, 7);
    OSMetaClassDeclareReservedUnused(IOFWSBP2PseudoAddressSpace, 8);
    OSMetaClassDeclareReservedUnused(IOFWSBP2PseudoAddressSpace, 9);
	
};

#endif
