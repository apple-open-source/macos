/*
 * Copyright (c) 2000-2003 Apple Computer, Inc. All rights reserved.
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
#define	SNAPSHOT_PATH_PATTERN	_PATH_VARTMP "configd-pattern.xml"
#define	SNAPSHOT_PATH_SESSION	_PATH_VARTMP "configd-session.xml"


#define N_QUICK	100

static CFDictionaryRef
_expandStore(CFDictionaryRef storeData)
{
	const void *		keys_q[N_QUICK];
	const void **		keys		= keys_q;
	CFIndex			nElements;
	CFDictionaryRef		newStoreData	= NULL;
	const void *		nValues_q[N_QUICK];
	const void **		nValues		= nValues_q;
	const void *		oValues_q[N_QUICK];
	const void **		oValues		= oValues_q;

	nElements = CFDictionaryGetCount(storeData);
	if (nElements > 0) {
		CFIndex	i;

		if (nElements > (CFIndex)(sizeof(keys_q) / sizeof(CFTypeRef))) {
			keys    = CFAllocatorAllocate(NULL, nElements * sizeof(CFTypeRef), 0);
			oValues = CFAllocatorAllocate(NULL, nElements * sizeof(CFTypeRef), 0);
			nValues = CFAllocatorAllocate(NULL, nElements * sizeof(CFTypeRef), 0);
		}
		bzero(nValues, nElements * sizeof(CFTypeRef));

		CFDictionaryGetKeysAndValues(storeData, keys, oValues);
		for (i = 0; i < nElements; i++) {
			CFDataRef		data;

			data = CFDictionaryGetValue(oValues[i], kSCDData);
			if (data) {
				CFPropertyListRef	plist;

				if (!_SCUnserialize(&plist, data, NULL, NULL)) {
					goto done;
				}

				nValues[i] = CFDictionaryCreateMutableCopy(NULL, 0, oValues[i]);
				CFDictionarySetValue((CFMutableDictionaryRef)nValues[i],
						     kSCDData,
						     plist);
				CFRelease(plist);
			} else {
				nValues[i] = CFRetain(oValues[i]);
			}
		}
	}

	newStoreData = CFDictionaryCreate(NULL,
				     keys,
				     nValues,
				     nElements,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);

    done :

	if (nElements > 0) {
		CFIndex	i;

		for (i = 0; i < nElements; i++) {
			if (nValues[i])	CFRelease(nValues[i]);
		}

		if (keys != keys_q) {
			CFAllocatorDeallocate(NULL, keys);
			CFAllocatorDeallocate(NULL, oValues);
			CFAllocatorDeallocate(NULL, nValues);
		}
	}

	return newStoreData;
}


__private_extern__
int
__SCDynamicStoreSnapshot(SCDynamicStoreRef store)
{
	CFDictionaryRef			expandedStoreData;
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

	expandedStoreData = _expandStore(storeData);
	xmlData = CFPropertyListCreateXMLData(NULL, expandedStoreData);
	CFRelease(expandedStoreData);
	if (!xmlData) {
		SCLog(TRUE, LOG_ERR, CFSTR("CFPropertyListCreateXMLData() failed"));
		close(fd);
		return kSCStatusFailed;
	}
	(void) write(fd, CFDataGetBytePtr(xmlData), CFDataGetLength(xmlData));
	(void) close(fd);
	CFRelease(xmlData);

	/* Save a snapshot of the "pattern" data */

	(void) unlink(SNAPSHOT_PATH_PATTERN);
	fd = open(SNAPSHOT_PATH_PATTERN, O_WRONLY|O_CREAT|O_TRUNC, 0644);
	if (fd < 0) {
		return kSCStatusFailed;
	}

	xmlData = CFPropertyListCreateXMLData(NULL, patternData);
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


__private_extern__
kern_return_t
_snapshot(mach_port_t server, int *sc_status)
{
	serverSessionRef	mySession = getSession(server);

	SCLog(_configd_verbose, LOG_DEBUG, CFSTR("Snapshot configuration database."));

	if (!mySession) {
		*sc_status = kSCStatusNoStoreSession;	/* you must have an open session to play */
		return KERN_SUCCESS;
	}

	*sc_status = __SCDynamicStoreSnapshot(mySession->store);
	if (*sc_status != kSCStatusOK) {
		SCLog(_configd_verbose, LOG_DEBUG, CFSTR("  __SCDynamicStoreSnapshot(): %s"), SCErrorString(*sc_status));
	}

	return KERN_SUCCESS;
}
