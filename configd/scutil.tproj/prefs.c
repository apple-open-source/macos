/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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
 * May 29, 2003			Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#include "scutil.h"
#include "prefs.h"

#include <SCPreferencesSetSpecific.h>


static SCPreferencesRef
_open()
{
	SCPreferencesRef	prefs;

	prefs = SCPreferencesCreate(NULL, CFSTR("scutil"), NULL);
	if (!prefs) {
		SCPrint(TRUE,
			stdout,
			CFSTR("SCPreferencesCreate() failed: %s\n"),
			SCErrorString(SCError()));
		exit (1);
	}

	return prefs;
}


static void
_save(SCPreferencesRef prefs)
{
	if (!SCPreferencesCommitChanges(prefs)) {
		switch (SCError()) {
			case kSCStatusAccessError :
				SCPrint(TRUE, stderr, CFSTR("Permission denied.\n"));
				break;
			default :
				SCPrint(TRUE,
					stdout,
					CFSTR("SCPreferencesCommitChanges() failed: %s\n"),
					SCErrorString(SCError()));
				break;
		}
		exit (1);
	}

	if (!SCPreferencesApplyChanges(prefs)) {
		SCPrint(TRUE,
			stdout,
			CFSTR("SCPreferencesApplyChanges() failed: %s\n"),
			SCErrorString(SCError()));
		exit (1);
	}

	return;
}


static CFStringRef
_copyStringFromSTDIN()
{
	char		buf[1024];
	size_t		len;
	CFStringRef	utf8;

	if (fgets(buf, sizeof(buf), stdin) == NULL) {
		return NULL;
	}

	len = strlen(buf);
	if (buf[len-1] == '\n') {
		buf[--len] = '\0';
	}

	utf8 = CFStringCreateWithBytes(NULL, buf, len, kCFStringEncodingUTF8, TRUE);
	return utf8;
}


static void
get_ComputerName(int argc, char **argv)
{
	CFStringEncoding	encoding;
	CFStringRef		hostname;
	CFDataRef		utf8;

	hostname = SCDynamicStoreCopyComputerName(NULL, &encoding);
	if (!hostname) {
		int	sc_status	= SCError();

		switch (sc_status) {
			case kSCStatusNoKey :
				SCPrint(TRUE,
					stderr,
					CFSTR("ComputerName: not set\n"));
				break;
			default :
				SCPrint(TRUE,
					stderr,
					CFSTR("SCDynamicStoreCopyComputerName() failed: %s\n"),
					SCErrorString(SCError()));
				break;
		}
		exit (1);
	}

	utf8 = CFStringCreateExternalRepresentation(NULL, hostname, kCFStringEncodingUTF8, 0);
	if (!utf8) {
		SCPrint(TRUE,
			stderr,
			CFSTR("ComputerName: could not convert to external representation\n"));
		exit(1);
	}
	SCPrint(TRUE, stdout, CFSTR("%.*s\n"), CFDataGetLength(utf8), CFDataGetBytePtr(utf8));

	CFRelease(utf8);
	CFRelease(hostname);
	exit(0);
}


static void
set_ComputerName(int argc, char **argv)
{
	CFStringEncoding	encoding;
	CFStringRef		hostname;
	SCPreferencesRef	prefs;

	if (argc == 0) {
		hostname = _copyStringFromSTDIN();
		encoding = kCFStringEncodingUTF8;
	} else {
		hostname = CFStringCreateWithCString(NULL, argv[0], kCFStringEncodingASCII);
		encoding = kCFStringEncodingASCII;
	}

	prefs = _open();
	if (!SCPreferencesSetComputerName(prefs, hostname, encoding)) {
		SCPrint(TRUE,
			stdout,
			CFSTR("SCPreferencesSetComputerName() failed: %s\n"),
			SCErrorString(SCError()));
		exit (1);
	}
	_save(prefs);
	CFRelease(prefs);
	CFRelease(hostname);
	exit(0);
}


static void
get_LocalHostName(int argc, char **argv)
{
	CFStringRef	hostname;

	hostname = SCDynamicStoreCopyLocalHostName(NULL);
	if (!hostname) {
		int	sc_status	= SCError();

		switch (sc_status) {
			case kSCStatusNoKey :
				SCPrint(TRUE,
					stderr,
					CFSTR("LocalHostName: not set\n"));
				break;
			default :
				SCPrint(TRUE,
					stderr,
					CFSTR("SCDynamicStoreCopyLocalHostName() failed: %s\n"),
					SCErrorString(SCError()));
				break;
		}
		exit (1);
	}

	SCPrint(TRUE, stdout, CFSTR("%@\n"), hostname);
	CFRelease(hostname);
	exit(0);
}


static void
set_LocalHostName(int argc, char **argv)
{
	CFStringRef		hostname = NULL;
	SCPreferencesRef	prefs;

	if (argc == 0) {
		hostname = _copyStringFromSTDIN();
	} else {
		hostname = CFStringCreateWithCString(NULL, argv[0], kCFStringEncodingASCII);
	}

	prefs = _open();
	if (!SCPreferencesSetLocalHostName(prefs, hostname)) {
		SCPrint(TRUE,
			stderr,
			CFSTR("SCPreferencesSetLocalHostName() failed: %s\n"),
			SCErrorString(SCError()));
		exit (1);
	}
	_save(prefs);
	CFRelease(prefs);
	CFRelease(hostname);
	exit(0);
}


typedef void (*pref_func) (int argc, char **argv);

static struct {
	char		*pref;
	pref_func	get;
	pref_func	set;
} prefs[] = {
	{ "ComputerName",	get_ComputerName,	set_ComputerName	},
	{ "LocalHostName",	get_LocalHostName,	set_LocalHostName	}
};
#define	N_PREFS	(sizeof(prefs) / sizeof(prefs[0]))


int
findPref(char *pref)
{
	int	i;

	for (i = 0; i < (int)N_PREFS; i++) {
		if (strcmp(pref, prefs[i].pref) == 0) {
			return i;
		}
	}

	return -1;
}


void
do_getPref(char *pref, int argc, char **argv)
{
	int	i;

	i = findPref(pref);
	if (i >= 0) {
		(*prefs[i].get)(argc, argv);
	}
	return;
}


void
do_setPref(char *pref, int argc, char **argv)
{
	int	i;

	i = findPref(pref);
	if (i >= 0) {
		(*prefs[i].set)(argc, argv);
	}
	return;
}
