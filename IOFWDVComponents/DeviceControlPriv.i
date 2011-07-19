/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
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
	File:		DeviceControlPriv.i

	Contains:	xxx put contents here xxx

	Version:	xxx put version here xxx

	Copyright:	© 1999 by Apple Computer, Inc., all rights reserved.

	File Ownership:

		DRI:				xxx put dri here xxx

		Other Contact:		xxx put other contact here xxx

		Technology:			xxx put technology here xxx

	Writers:

		(KW)	Kevin Williams
		(GDW)	George D. Wilson Jr.

	Change History (most recent first):

		 <3>	 6/15/99	KW		fix screwup
		 <2>	 6/15/99	KW		add subtype
		 <1>	 6/15/99	GDW		first checked in
*/


#include <DeviceControl.i>



typedef UInt32 DeviceConnectionID;

enum
{
	kDeviceControlComponentType = 'devc',			/* Component type */
	kDeviceControlSubtypeFWDV = 'fwdv'				/* Component subtype */
};




/* Private calls made by the Isoc component */


%TellEmitter "components" "prefix DeviceControl";

pascal <exportset=IDHLib_10>
ComponentResult DeviceControlEnableAVCTransactions(ComponentInstance instance) = ComponentCall(0x100);

pascal <exportset=IDHLib_10>
ComponentResult DeviceControlDisableAVCTransactions(ComponentInstance instance) = ComponentCall(0x101);

pascal <exportset=IDHLib_10>
ComponentResult DeviceControlSetDeviceConnectionID(ComponentInstance instance, DeviceConnectionID connectionID) = ComponentCall(0x102);

pascal <exportset=IDHLib_10>
 ComponentResult DeviceControlGetDeviceConnectionID(ComponentInstance instance, DeviceConnectionID *connectionID) = ComponentCall(0x103);

%TellEmitter "components" "emitProcInfos";
%TellEmitter "c" "emitComponentSelectors";
