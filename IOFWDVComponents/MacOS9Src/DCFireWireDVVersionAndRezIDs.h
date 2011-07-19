/*
 * Copyright (c) 2009 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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
	File:		DCFireWireDVVersionAndRezIDs.h

	Contains:	Contants that define the version of the Crush Isochronous Data Hanlder
				and any PRIVATE ResourceIDs it might need.

	Copyright:	© 1997-1999 by Apple Computer, Inc., all rights reserved.

	File Ownership:

		DRI:				Sean Williams

	Writers:


	Change History (most recent first):

	   <1>	 6/15/99		KW		Created
*/


#ifndef __DCFIREWIREDVVERSIONANDREZIDS__
#define __DCFIREWIREDVVERSIONANDREZIDS__



//
// IDH Crush Version Constants
//	When a component is queried with with the kComponentVersionSelect selector, it is
//	supposed to return the 'interface version' in the high word of the ComponentResult
//	and the 'code revision' in the low word of the Component result.

enum { kDCFireWireDVInterfaceVersion = 1 };


//
// kDCFireWireDVCodeVersion
// 1.0.0 - Initial release

#define	kMajorRevisionNumber		0x01
#define	kMinorRevisionNumber		0x00
#define kDevelopmentStage			development
#define kBuildVersionNumber			0x00
#define	kShortVersionString			"1.0.0d0"
#define	kLongVersionString			"1.0.0d0 © Apple Computer, Inc., 1999"


#define kDCFireWireDVCodeVersion (kMajorRevisionNumber << 8 | kMinorRevisionNumber)



#endif // __DCFIREWIREDVVERSIONANDREZIDS__ //
