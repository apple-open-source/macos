/*
 * Copyright (c) 2000-2002 Apple Computer, Inc. All rights reserved.
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
 * Modification History
 *
 * June 1, 2001			Allan Nathanson <ajn@apple.com>
 * - public API conversion
 *
 * April 14, 2000		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#include <fcntl.h>
#include <paths.h>
#include <unistd.h>

#include "configd.h"
#include "configd_server.h"
#include "session.h"


#define	SNAPSHOT_PATH_STORE	_PATH_VARTMP "configd-store.xml"
#define	SNAPSHOT_PATH_SESSION	_PATH_VARTMP "configd-session.xml"


int
__SCDynamicStoreSnapshot(SCDynamicStoreRef store)
{
	int				fd;
	serverSessionRef		mySession;
	SCDynamicStorePrivateRef	storePrivate = (SCDynamicStorePrivateRef)store;
	CFDataRef			xmlData;

	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("__SCDynamicStoreSnapshot:"));

	/* check credentials */

	mySession = getSession(storePrivate->server);
	if (mySession->callerEUID != 0) {
		return kSCStatusAccessError;
	}

	/* Save a snapshot of the "store" data */

	(void) unlink(SNAPSHOT_PATH_STORE);
	fd = open(SNAPSHOT_PATH_STORE, O_WRONLY|O_CREAT|O_TRUNC, 0644);
	if (fd < 0) {
		return kSCStatusFailed;
	}

	xmlData = CFPropertyListCreateXMLData(NULL, storeData);
	if (!xmlData) {
		SCLog(TRUE, LOG_ERR, CFSTR("CFPropertyListCreateXMLData() failed"));
		close(fd);
		return kSCStatusFailed;
	}
	(void) write(fd, CFDataGetBytePtr(xmlData), CFDataGetLength(xmlData));
	(void) close(fd);
	CFRelease(xmlData);

	/* Save a snapshot of the "session" data */

	(void) unlink(SNAPSHOT_PATH_SESSION);
	fd = open(SNAPSHOT_PATH_SESSION, O_WRONLY|O_CREAT|O_TRUNC, 0644);
	if (fd < 0) {
		return kSCStatusFailed;
	}

	/* Save a snapshot of the "session" data */

	xmlData = CFPropertyListCreateXMLData(NULL, sessionData);
	if (!xmlData) {
		SCLog(TRUE, LOG_ERR, CFSTR("CFPropertyListCreateXMLData() failed"));
		close(fd);
		return kSCStatusFailed;
	}
	(void) write(fd, CFDataGetBytePtr(xmlData), CFDataGetLength(xmlData));
	(void) close(fd);
	CFRelease(xmlData);

	return kSCStatusOK;
}


kern_return_t
_snapshot(mach_port_t server, int *sc_status)
{
	serverSessionRef	mySession = getSession(server);

	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("Snapshot configuration database."));

	*sc_status = __SCDynamicStoreSnapshot(mySession->store);
	if (*sc_status != kSCStatusOK) {
		SCLog(_configd_verbose, LOG_DEBUG, CFSTR("  __SCDynamicStoreSnapshot(): %s"), SCErrorString(*sc_status));
	}

	return KERN_SUCCESS;
}
