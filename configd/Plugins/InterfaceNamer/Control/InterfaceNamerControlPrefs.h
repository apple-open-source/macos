/*
 * Copyright (c) 2017-2024 Apple Inc. All rights reserved.
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

#ifndef _INTERFACENAMERCONTROLPREFS_H
#define _INTERFACENAMERCONTROLPREFS_H

/*
 * InterfaceNamerControlPrefs.h
 * - definitions for accessing InterfaceNamer control preferences
 */

/*
 * Modification History
 *
 * January 12, 2017	Allan Nathanson (ajn@apple.com)
 * - created
 */

#include <sys/cdefs.h>
#include <CoreFoundation/CoreFoundation.h>
#include "SCControlPrefs.h"

__BEGIN_DECLS

typedef void (*InterfaceNamerControlPrefsCallBack)(_SCControlPrefsRef control);

_SCControlPrefsRef
InterfaceNamerControlPrefsInit(CFRunLoopRef runloop,
			       InterfaceNamerControlPrefsCallBack callback);

Boolean
InterfaceNamerControlPrefsAllowNewInterfaces(_SCControlPrefsRef control);

Boolean
InterfaceNamerControlPrefsSetAllowNewInterfaces(_SCControlPrefsRef control,
						Boolean allow);

Boolean
InterfaceNamerControlPrefsConfigureNewInterfaces(_SCControlPrefsRef control);

Boolean
InterfaceNamerControlPrefsSetConfigureNewInterfaces(_SCControlPrefsRef control,
						    Boolean configure);

__END_DECLS

#endif	/* _INTERFACENAMERCONTROLPREFS_H */
