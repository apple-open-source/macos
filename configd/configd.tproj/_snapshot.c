/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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

#include <fcntl.h>
#include <paths.h>
#include <unistd.h>

#include "configd.h"
#include "configd_server.h"
#include "session.h"


#define	SNAPSHOT_PATH_CACHE	_PATH_VARTMP "configd-cache.xml"
#define	SNAPSHOT_PATH_SESSION	_PATH_VARTMP "configd-session.xml"


SCDStatus
_SCDSnapshot(SCDSessionRef session)
{
	int			fd;
	CFDataRef		xmlData;

	SCDLog(LOG_DEBUG, CFSTR("_SCDSnapshot:"));

	/* Save a snapshot of the "cache" data */

	(void) unlink(SNAPSHOT_PATH_CACHE);
	fd = open(SNAPSHOT_PATH_CACHE, O_WRONLY|O_CREAT|O_TRUNC, 0644);
	if (fd < 0) {
		return SCD_FAILED;
	}

	xmlData = CFPropertyListCreateXMLData(NULL, cacheData);
	(void) write(fd, CFDataGetBytePtr(xmlData), CFDataGetLength(xmlData));
	(void) close(fd);
	CFRelease(xmlData);

	/* Save a snapshot of the "session" data */

	(void) unlink(SNAPSHOT_PATH_SESSION);
	fd = open(SNAPSHOT_PATH_SESSION, O_WRONLY|O_CREAT|O_TRUNC, 0644);
	if (fd < 0) {
		return SCD_FAILED;
	}

	/* Save a snapshot of the "session" data */

	xmlData = CFPropertyListCreateXMLData(NULL, sessionData);
	(void) write(fd, CFDataGetBytePtr(xmlData), CFDataGetLength(xmlData));
	(void) close(fd);
	CFRelease(xmlData);

	return SCD_OK;
}


kern_return_t
_snapshot(mach_port_t server, int *scd_status)
{
	serverSessionRef	mySession = getSession(server);

	SCDLog(LOG_DEBUG, CFSTR("Snapshot configuration database."));

	*scd_status = _SCDSnapshot(mySession->session);
	if (*scd_status != SCD_OK) {
		SCDLog(LOG_DEBUG, CFSTR("  SCDUnlock(): %s"), SCDError(*scd_status));
	}

	return KERN_SUCCESS;
}
