/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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
 *  IOFireWireLibPriv.cpp
 *  IOFireWireLib
 *
 *  Created on Fri Apr 28 2000.
 *  Copyright (c) 2000-2002 Apple Computer, Inc. All rights reserved.
 *
 */
/*
	$Log: IOFireWireLibPriv.cpp,v $
	Revision 1.42  2003/07/21 06:53:11  niels
	merge isoch to TOT

	Revision 1.41.6.2  2003/07/21 06:44:48  niels
	*** empty log message ***
	
	Revision 1.41.6.1  2003/07/01 20:54:24  niels
	isoch merge
	
	Revision 1.41  2003/02/19 22:33:21  niels
	add skip cycle DCL
	
	Revision 1.40  2003/01/09 22:58:12  niels
	radar 3061582: change kCFRunLoopDefaultMode to kCFRunLoopCommonModes
	
	Revision 1.39  2002/12/12 22:44:03  niels
	fixed radar 3126316: panic with Hamamatsu driver
	
	Revision 1.38  2002/10/18 23:29:49  collin
	fix includes, fix cast which fails on new compiler
	
	Revision 1.37  2002/10/11 23:12:22  collin
	fix broken headerdoc, fix compiler warnings
	
	Revision 1.36  2002/09/25 00:27:34  niels
	flip your world upside-down
	
	Revision 1.35  2002/09/12 22:41:56  niels
	add GetIRMNodeID() to user client
	
*/

// private
#import "IOFireWireLibPriv.h"
#import "IOFireWireLibIOCFPlugIn.h"

// ============================================================
// factory function implementor
// ============================================================

extern "C" void*
IOFireWireLibFactory( CFAllocatorRef allocator, CFUUIDRef typeID )
{
	DebugLog( "IOFireWireLib debug build, built on " __DATE__ " " __TIME__ "\n" ) ;

	void* result	= nil;

	if ( CFEqual( typeID, kIOFireWireLibTypeID ) )
		result	= (void*) IOFireWireLib::IOCFPlugIn::Alloc() ;

	return (void*) result ;
}

