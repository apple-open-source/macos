/*
 * Copyright (c) 2003-2005 Apple Computer, Inc. All rights reserved.
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

#ifndef _SCDYNAMICSTORECOPYSPECIFICPRIVATE_H
#define _SCDYNAMICSTORECOPYSPECIFICPRIVATE_H

#include <sys/cdefs.h>
#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SCDynamicStore.h>


/*!
	@header SCDynamicStoreCopySpecificPrivate
	@discussion The following APIs allow an application to retrieve
		console information.
 */


__BEGIN_DECLS

/*
 * Predefined keys for the console session dictionaries
 */
extern const CFStringRef kSCConsoleSessionID;			/* value is CFNumber */
extern const CFStringRef kSCConsoleSessionUserName;		/* value is CFString */
extern const CFStringRef kSCConsoleSessionUID;			/* value is CFNumber (a uid_t) */
extern const CFStringRef kSCConsoleSessionConsoleSet;		/* value is CFNumber */
extern const CFStringRef kSCConsoleSessionOnConsole;		/* value is CFBoolean */
extern const CFStringRef kSCConsoleSessionLoginDone;		/* value is CFBoolean */
extern const CFStringRef kSCConsoleSessionSystemSafeBoot;	/* value is CFBoolean */
extern const CFStringRef kSCConsoleSessionLoginwindowSafeLogin;	/* value is CFBoolean */

/*!
	@function SCDynamicStoreCopyConsoleInformation
	@discussion Returns information about all console sessions on the system.
	@param store An SCDynamicStoreRef that should be used for communication
		with the server.
		If NULL, a temporary session will be used.
	@result An array of dictionaries containing information about each
		console session on the system; NULL if no sessions are defined
		or if an error was encountered.

		The contents of the returned array match that of the CoreGraphics
		CGSCopySessionList() SPI.

		You must release the returned value.
 */
CFArrayRef
SCDynamicStoreCopyConsoleInformation	(
					SCDynamicStoreRef	store
					);


__END_DECLS

#endif /* _SCDYNAMICSTORECOPYSPECIFICPRIVATE_H */
