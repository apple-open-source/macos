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
 * January 1, 2001		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#include <unistd.h>

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>


Boolean	apply	= TRUE;


void
usage(const char *command)
{
	SCPrint(TRUE, stderr, CFSTR("usage: %s [-n] new-set-name\n"), command);
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
	SCPreferencesRef	session;
	CFDictionaryRef		sets;
	CFIndex			nSets;
	const void		**setKeys	= NULL;
	const void		**setVals	= NULL;
	CFIndex			i;

	/* process any arguments */

	while ((opt = getopt(argc, argv, "dvn")) != -1)
		switch(opt) {
		case 'd':
			_sc_debug = TRUE;
			_sc_log   = FALSE;	/* enable framework logging */
			break;
		case 'v':
			_sc_verbose = TRUE;
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

	session = SCPreferencesCreate(NULL, CFSTR("Select Set Command"), NULL);
	if (!session) {
		SCPrint(TRUE, stderr, CFSTR("SCPreferencesCreate() failed\n"));
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
			SCPrint(TRUE, stderr, CFSTR("Set \"%@\" not available\n."), newSet);
			exit (1);
		}

		CFRelease(newSet);
		newSet = str;
	}

	sets = SCPreferencesGetValue(session, kSCPrefSets);
	if (!sets) {
		SCPrint(TRUE, stderr, CFSTR("SCPreferencesGetValue(...,%s,...) failed\n"));
		exit (1);
	}

	current = SCPreferencesGetValue(session, kSCPrefCurrentSet);
	if (current) {
		if (CFStringHasPrefix(current, prefix)) {
			CFMutableStringRef	tmp;

			tmp = CFStringCreateMutableCopy(NULL, 0, current);
			CFStringDelete(tmp, CFRangeMake(0, CFStringGetLength(prefix)));
			current = tmp;
		} else {
			currentMatched = -1;	/* not prefixed */
		}
	} else {
		current = CFSTR("");
		currentMatched = -2;	/* not defined */
	}

	nSets = CFDictionaryGetCount(sets);
	if (nSets > 0) {
		setKeys = CFAllocatorAllocate(NULL, nSets * sizeof(CFStringRef), 0);
		setVals = CFAllocatorAllocate(NULL, nSets * sizeof(CFDictionaryRef), 0);
		CFDictionaryGetKeysAndValues(sets, setKeys, setVals);
	}

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
		SCPrint(TRUE, stderr, CFSTR("Set \"%@\" not available.\n"), newSet);
	} else {
		usage(command);
	}

	SCPrint(TRUE, stderr, CFSTR("\n"));
	SCPrint(TRUE, stderr,
		CFSTR("Defined sets include:%s\n"),
		(currentMatched > 0) ? " (* == current set)" : "");

	for (i=0; i<nSets; i++) {
		CFStringRef	key  = (CFStringRef)    setKeys[i];
		CFDictionaryRef	dict = (CFDictionaryRef)setVals[i];
		CFStringRef	udn  = CFDictionaryGetValue(dict, kSCPropUserDefinedName);

		SCPrint(TRUE, stderr,
			CFSTR(" %s %@\t(%@)\n"),
			((currentMatched > 0) && CFEqual(key, current)) ? "*" : " ",
			key,
			udn ? udn : CFSTR(""));
	}

	switch (currentMatched) {
		case -2 :
			SCPrint(TRUE, stderr, CFSTR("\nCurrentSet not defined.\n"));
			break;
		case -1 :
			SCPrint(TRUE, stderr, CFSTR("\nCurrentSet \"%@\" may not be valid\n"), current);
			break;
		case  0 :
			SCPrint(TRUE, stderr, CFSTR("\nCurrentSet \"%@\" not valid\n"), current);
			break;
		default :
			break;
	}

	exit (1);

    found :

	if (!SCPreferencesSetValue(session, kSCPrefCurrentSet, current)) {
		SCPrint(TRUE, stderr,
			CFSTR("SCPreferencesSetValue(...,%@,%@) failed\n"),
			kSCPrefCurrentSet,
			current);
		exit (1);
	}

	if (!SCPreferencesCommitChanges(session)) {
		SCPrint(TRUE, stderr, CFSTR("SCPreferencesCommitChanges() failed\n"));
		exit (1);
	}

	if (apply) {
		if (!SCPreferencesApplyChanges(session)) {
			SCPrint(TRUE, stderr, CFSTR("SCPreferencesApplyChanges() failed\n"));
			exit (1);
		}
	}

	CFRelease(session);

	SCPrint(TRUE, stdout,
		CFSTR("%@ updated to %@ (%@)\n"),
		kSCPrefCurrentSet,
		newSet,
		newSetUDN ? newSetUDN : CFSTR(""));

	exit (0);
	return 0;
}
