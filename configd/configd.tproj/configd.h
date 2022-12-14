/*
 * Copyright (c) 2000, 2001, 2003, 2006, 2007, 2011, 2013, 2016, 2017, 2021-2022 Apple Inc. All rights reserved.
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
 * Modification History
 *
 * June 1, 2001			Allan Nathanson <ajn@apple.com>
 * - public API conversion
 *
 * March 24, 2000		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#ifndef _S_CONFIGD_H
#define _S_CONFIGD_H

#include <stdio.h>
#include <stdlib.h>
#include <TargetConditionals.h>

/* configd doesn't need the preference keys */
#define _SCSCHEMADEFINITIONS_H
#define _SCSCHEMADEFINITIONSPRIVATE_H

#define	SC_LOG_HANDLE	__configd_SCDynamicStore
#include "SCDynamicStoreInternal.h"
#include "config_types.h"
#include "_SCD.h"

extern Boolean		_configd_verbose;		/* TRUE if verbose logging enabled */
extern CFMutableSetRef	_plugins_exclude;		/* bundle identifiers to exclude from loading */
extern CFMutableSetRef	_plugins_verbose;		/* bundle identifiers to enable verbose logging */

extern Boolean	_should_log_path;

/*
 * PrivacyAccounting framework is only available on (non-simulator):
 * - iphoneos
 * - watchos
 */
#if (TARGET_OS_IOS || TARGET_OS_WATCH) && !TARGET_OS_SIMULATOR
#define _HAVE_PRIVACY_ACCOUNTING		1
#else
#define _HAVE_PRIVACY_ACCOUNTING		0
#endif

/*
 * BASupport library is only available on
 * - iphoneos
 * - macOS
 */
#if TARGET_OS_IOS || TARGET_OS_OSX
#define _HAVE_BASUPPORT				1
#else
#define _HAVE_BASUPPORT				0
#endif

#define SC_trace(__string, ...)	\
	os_log_debug(SC_LOG_HANDLE(), __string, ## __VA_ARGS__)


__BEGIN_DECLS

os_log_t
__configd_SCDynamicStore	(void);

#if _HAVE_PRIVACY_ACCOUNTING
Boolean
isSystemProcess(CFDictionaryRef entitlements);

Boolean
havePrivacyAccounting(void);

#endif /* _HAVE_PRIVACY_ACCOUNTING */

__END_DECLS

#endif	/* !_S_CONFIGD_H */
