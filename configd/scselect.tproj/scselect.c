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

#include <SystemConfiguration/SystemConfiguration.h>
#include <unistd.h>


boolean_t	apply	= TRUE;


void
usage(const char *command)
{
	SCDLog(LOG_ERR, CFSTR("usage: %s [-n] new-set-name"), command);
	return;
}


int
main(int argc, char **argv)
{
	const char		*command	= argv[0];
	extern int		optind;
	int			opt;
	CFStringRef		current		= NULL;
	int			currentMatched	= 0;
	CFStringRef		newSet		= NULL;	/* set key */
	CFStringRef		newSetUDN	= NULL;	/* user defined name */
	CFStringRef		prefix;
	SCPStatus		status;
	SCPSessionRef		session;
	CFDictionaryRef		sets;
	CFIndex			nSets;
	void			**setKeys;
	void			**setVals;
	CFIndex			i;

	/* process any arguments */

	SCDOptionSet(NULL, kSCDOptionUseSyslog, FALSE);

	while ((opt = getopt(argc, argv, "dvn")) != -1)
		switch(opt) {
		case 'd':
			SCDOptionSet(NULL, kSCDOptionDebug, TRUE);
			break;
		case 'v':
			SCDOptionSet(NULL, kSCDOptionVerbose, TRUE);
			break;
		case 'n':
			apply = FALSE;
			break;
		case '?':
		default :
			usage(command);
	}
	argc -= optind;
	argv += optind;

	prefix = CFStringCreateWithFormat(NULL, NULL, CFSTR("/%@/"), kSCPrefSets);

	newSet = (argc == 1)
			? CFStringCreateWithCString(NULL, argv[0], kCFStringEncodingMacRoman)
			: CFSTR("");

	status = SCPOpen(&session, CFSTR("Select Set Command"), NULL, 0);
	if (status != SCP_OK) {
		SCDLog(LOG_ERR,
		       CFSTR("SCPOpen() failed: %s"),
		       SCPError(status));
		exit (1);
	}

	/* check if a full path to the new "set" was specified */
	if ((CFStringGetLength(newSet) > 0) && CFStringHasPrefix(newSet, prefix)) {
		CFRange			range;
		CFMutableStringRef	str;

		str = CFStringCreateMutableCopy(NULL, 0, newSet);
		CFStringDelete(str, CFRangeMake(0, CFStringGetLength(prefix)));

		range = CFStringFind(str, CFSTR("/"), 0);
		if (range.location != kCFNotFound) {
			SCDLog(LOG_ERR, CFSTR("Set \"%@\" not available."), newSet);
			exit (1);
		}

		CFRelease(newSet);
		newSet = str;
	}

	status = SCPGet(session, kSCPrefSets, (CFPropertyListRef *)&sets);
	if (status != SCP_OK) {
		SCDLog(LOG_ERR, CFSTR("SCDGet(...,%s,...) failed: %s"), SCPError(status));
		exit (1);
	}

	status = SCPGet(session, kSCPrefCurrentSet, (CFPropertyListRef *)&current);
	switch (status) {
		case SCP_OK :
			if (CFStringHasPrefix(current, prefix)) {
				CFMutableStringRef	tmp;

				tmp = CFStringCreateMutableCopy(NULL, 0, current);
				CFStringDelete(tmp, CFRangeMake(0, CFStringGetLength(prefix)));
				current = tmp;
			} else {
				currentMatched = -1;	/* not prefixed */
			}
			break;
		case SCP_NOKEY :
			current = CFSTR("");
			currentMatched = -2;	/* not defined */
			break;
		default :
			SCDLog(LOG_ERR, CFSTR("SCDGet(...,%s,...) failed: %s"), SCPError(status));
			exit (1);
	}

	nSets = CFDictionaryGetCount(sets);
	setKeys = CFAllocatorAllocate(NULL, nSets * sizeof(CFStringRef), 0);
	setVals = CFAllocatorAllocate(NULL, nSets * sizeof(CFDictionaryRef), 0);
	CFDictionaryGetKeysAndValues(sets, setKeys, setVals);

	/* check for set with matching name */
	for (i=0; i<nSets; i++) {
		CFStringRef	key  = (CFStringRef)    setKeys[i];
		CFDictionaryRef	dict = (CFDictionaryRef)setVals[i];

		if ((currentMatched >= 0) && CFEqual(key, current)) {
			currentMatched++;
		}

		if (CFEqual(newSet, key)) {
			newSetUDN = CFDictionaryGetValue(dict, kSCPropUserDefinedName);
			if (newSetUDN)	CFRetain(newSetUDN);
			current = CFStringCreateWithFormat(NULL, NULL, CFSTR("%@%@"), prefix, newSet);
			goto found;
		}
	}

	/* check for set with matching user-defined name */
	for (i=0; i<nSets; i++) {
		CFStringRef	key  = (CFStringRef)    setKeys[i];
		CFDictionaryRef	dict = (CFDictionaryRef)setVals[i];

		newSetUDN = CFDictionaryGetValue(dict, kSCPropUserDefinedName);
		if ((newSetUDN != NULL) && CFEqual(newSet, newSetUDN)) {
			CFRelease(newSet);
			newSet = CFRetain(key);
			CFRetain(newSetUDN);
			current = CFStringCreateWithFormat(NULL, NULL, CFSTR("%@%@"), prefix, newSet);
			goto found;
		}
	}

	if (argc == 2) {
		SCDLog(LOG_ERR, CFSTR("Set \"%@\" not available."), newSet);
	} else {
		usage(command);
	}

	SCDLog(LOG_ERR, CFSTR(""));
	SCDLog(LOG_ERR,
	       CFSTR("Defined sets include:%s"),
	       (currentMatched > 0) ? " (* == current set)" : "");

	for (i=0; i<nSets; i++) {
		CFStringRef	key  = (CFStringRef)    setKeys[i];
		CFDictionaryRef	dict = (CFDictionaryRef)setVals[i];
		CFStringRef	udn  = CFDictionaryGetValue(dict, kSCPropUserDefinedName);

		SCDLog(LOG_ERR,
			CFSTR(" %s %@\t(%@)"),
			((currentMatched > 0) && CFEqual(key, current)) ? "*" : " ",
			key,
			udn ? udn : CFSTR(""));
	}

	switch (currentMatched) {
		case -2 :
			SCDLog(LOG_ERR, CFSTR(""));
			SCDLog(LOG_ERR, CFSTR("CurrentSet not defined"));
			break;
		case -1 :
			SCDLog(LOG_ERR, CFSTR(""));
			SCDLog(LOG_ERR, CFSTR("CurrentSet \"%@\" may not be valid"), current);
			break;
		case  0 :
			SCDLog(LOG_ERR, CFSTR(""));
			SCDLog(LOG_ERR, CFSTR("CurrentSet \"%@\" not valid"), current);
			break;
		default :
			break;
	}

	exit (1);

    found :

	status = SCPSet(session, kSCPrefCurrentSet, current);
	if (status != SCP_OK) {
		SCDLog(LOG_ERR,
			CFSTR("SCDSet(...,%@,%@) failed: %s"),
			kSCPrefCurrentSet,
			current,
			SCPError(status));
		exit (1);
	}

	status = SCPCommit(session);
	if (status != SCP_OK) {
		SCDLog(LOG_ERR, CFSTR("SCPCommit() failed: %s"), SCPError(status));
		exit (1);
	}

	if (apply) {
		status = SCPApply(session);
		if (status != SCP_OK) {
			SCDLog(LOG_ERR, CFSTR("SCPApply() failed: %s"), SCPError(status));
			exit (1);
		}
	}

	status = SCPClose(&session);
	if (status != SCP_OK) {
		SCDLog(LOG_ERR, CFSTR("SCPClose() failed: %s"), SCPError(status));
		exit (1);
	}

	SCDLog(LOG_NOTICE,
		CFSTR("%@ updated to %@ (%@)"),
		kSCPrefCurrentSet,
		newSet,
		newSetUDN ? newSetUDN : CFSTR(""));

	exit (0);
	return 0;
}
