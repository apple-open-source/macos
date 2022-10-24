/*
 * Copyright (c) 2021 Apple Inc. All rights reserved.
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

#ifndef _SCPREFSCONTROL_H
#define _SCPREFSCONTROL_H

/*
 * SCControlPrefs.h
 * - APIs for accessing control preferences and being notified
 *   when they change
 */

/*
 * Modification History
 *
 * Jun 10, 2021			Allan Nathanson (ajn@apple.com)
 * - created
 */

#include <sys/cdefs.h>
#include <CoreFoundation/CoreFoundation.h>

__BEGIN_DECLS

typedef const struct CF_BRIDGED_TYPE(id) __SCControlPrefs * _SCControlPrefsRef;

typedef void (*_SCControlPrefsCallBack)			(_SCControlPrefsRef		control);

_SCControlPrefsRef	_SCControlPrefsCreate		(const char			*prefsPlist,
							 CFRunLoopRef			runloop,
							 _SCControlPrefsCallBack	callback);

Boolean			_SCControlPrefsGetBoolean	(_SCControlPrefsRef		control,
							 CFStringRef			key);

Boolean			_SCControlPrefsSetBoolean	(_SCControlPrefsRef		control,
							 CFStringRef			key,
							 Boolean			enabled);

__END_DECLS

#endif	/* _SCPREFSCONTROL_H */
