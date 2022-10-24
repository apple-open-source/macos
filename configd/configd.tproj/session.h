/*
 * Copyright (c) 2000, 2001, 2003, 2005-2007, 2009-2012, 2014, 2016-2022 Apple Inc. All rights reserved.
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

#ifndef _S_SESSION_H
#define _S_SESSION_H

#include <sys/cdefs.h>
#include <os/availability.h>

#define DISPATCH_MACH_SPI 1
#include <dispatch/private.h>

/*
 * SCDynamicStore read no-fault entitlement
 *
 *   Key   : "com.apple.SystemConfiguration.SCDynamicStore-read-no-fault"
 *   Value : Boolean
 *             TRUE == do not issue a fault (or simulated crash) if a read
 *		       operation is denied due to a missing entitlement
 */
#define kSCReadNoFaultEntitlementName	CFSTR("com.apple.SystemConfiguration.SCDynamicStore-read-no-fault")

/*
 * SCDynamicStore write access entitlement
 *
 *   Key   : "com.apple.SystemConfiguration.SCDynamicStore-write-access"
 *   Value : Boolean
 *             TRUE == allow SCDynamicStore write access for this process
 *
 *           Dictionary
 *             Key   : "keys"
 *             Value : <array> of CFString with write access allowed for
 *                     each SCDynamicStore key matching the string(s)
 *
 *             Key   : "patterns"
 *             Value : <array> of CFString with write access allowed for
 *                     each SCDynamicStore key matching the regex pattern(s)
 */
#define	kSCWriteEntitlementName	CFSTR("com.apple.SystemConfiguration.SCDynamicStore-write-access")

/*
 * SCDynamicStore write no-fault entitlement
 *
 *   Key   : "com.apple.SystemConfiguration.SCDynamicStore-write-no-fault"
 *   Value : Boolean
 *             TRUE == do not issue a fault (or simulated crash) if a write
 *		       operation is denied due to a missing entitlement
 */
#define kSCWriteNoFaultEntitlementName	CFSTR("com.apple.SystemConfiguration.SCDynamicStore-write-no-fault")

/* Per client server state */
typedef struct {

	// base CFType information
	CFRuntimeBase           cfBase;

	/* mach port used as the key to this session */
	mach_port_t		key;

	/* mach channel associated with this session */
	dispatch_mach_t		serverChannel;

	/* data associated with this "open" session */
	CFMutableArrayRef	changedKeys;
	CFStringRef		name;
	CFMutableArrayRef	sessionKeys;
	SCDynamicStoreRef	store;

	/* credentials associated with this "open" session */
	uid_t			callerEUID;

	/* Mach security audit trailer for evaluating credentials */
	audit_token_t		auditToken;

	/*
	 * entitlements associated with this "open" session
	 *
	 * Note: the dictionary key is the entitlement name.  A
	 *       value of kCFNull indicates that the entitlement
	 *       does not exist for the session.
	 */
	CFMutableDictionaryRef	entitlements;

	/*
	 * isBackgroundAssetExtension
	 * - NULL means we haven't checked yet
	 * - kCFBooleanTrue/kCFBooleanFalse otherwise
	 * - not retained
	 */
	CFBooleanRef		isBackgroundAssetExtension;

	/*
	 * isPlatformBinary
	 * - NULL means we haven't checked yet
	 * - kCFBooleanTrue/kCFBooleanFalse otherwise
	 * - not retained
	 */
	CFBooleanRef		isPlatformBinary;

	/*
	 * isSystemProcess
	 * - NULL means we haven't checked yet
	 * - kCFBooleanTrue/kCFBooleanFalse otherwise
	 * - not retained
	 */
	CFBooleanRef		isSystemProcess;
} serverSession, *serverSessionRef;

__BEGIN_DECLS

serverSessionRef	addClient	(mach_port_t	server,
					 audit_token_t	audit_token);

serverSessionRef	addServer	(mach_port_t	server);

serverSessionRef	getSession	(mach_port_t	server);

serverSessionRef	getSessionNum	(CFNumberRef	serverKey);

serverSessionRef	getSessionStr	(CFStringRef	serverKey);

void			cleanupSession	(serverSessionRef	session);

void			closeSession	(serverSessionRef	session);

void			listSessions	(FILE		*f);

Boolean			hasRootAccess	(serverSessionRef	session);

int			checkReadAccess	(serverSessionRef	session,
					 CFStringRef		key,
					 CFDictionaryRef	controls);

int			checkWriteAccess(serverSessionRef	session,
					 CFStringRef		key);

__END_DECLS

#endif	/* !_S_SESSION_H */
