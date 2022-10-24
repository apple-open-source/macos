/*
 * Copyright (c) 2017-2019, 2021 Apple Inc. All rights reserved.
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
 * InterfaceNamerControlPrefs.c
 * - definitions for accessing InterfaceNamer control preferences
 */

/*
 * Modification History
 *
 * January 12, 2017	Allan Nathanson (ajn@apple.com)
 * - created
 */

#include "InterfaceNamerControlPrefs.h"

/*
 * kInterfaceNamerControlPrefsID
 * - identifies the InterfaceNamer preferences file that contains 'AllowNewInterfaces'
 */
#define kInterfaceNamerControlPrefsIDStr	"com.apple.InterfaceNamer.control.plist"

/*
 * kAllowNewInterfaces
 * - indicates whether InterfaceNamer is allowed to create new interfaces
 *   while the screen is locked or not
 */
#define kAllowNewInterfaces			CFSTR("AllowNewInterfaces")

static _SCControlPrefsRef			S_control;

__private_extern__
_SCControlPrefsRef
InterfaceNamerControlPrefsInit(CFRunLoopRef				runloop,
			       InterfaceNamerControlPrefsCallBack	callback)
{
	_SCControlPrefsRef	control;

	control = _SCControlPrefsCreate(kInterfaceNamerControlPrefsIDStr, runloop, callback);
	return control;
}

/**
 ** Get
 **/
__private_extern__ Boolean
InterfaceNamerControlPrefsAllowNewInterfaces(void)
{
	Boolean	allow	= FALSE;

	if (S_control == NULL) {
		S_control = InterfaceNamerControlPrefsInit(NULL, NULL);
	}
	if (S_control != NULL) {
		allow = _SCControlPrefsGetBoolean(S_control, kAllowNewInterfaces);
	}
	return allow;
}

/**
 ** Set
 **/
__private_extern__ Boolean
InterfaceNamerControlPrefsSetAllowNewInterfaces(Boolean allow)
{
	Boolean	ok	= FALSE;

	if (S_control == NULL) {
		S_control = InterfaceNamerControlPrefsInit(NULL, NULL);
	}
	if (S_control != NULL) {
		ok = _SCControlPrefsSetBoolean(S_control, kAllowNewInterfaces, allow);
	}
	return ok;
}
