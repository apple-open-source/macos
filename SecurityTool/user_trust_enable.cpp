/*
 * Copyright (c) 2003-2004 Apple Computer, Inc. All Rights Reserved.
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
 *
 * user_trust_enable.cpp
 */

#include "user_trust_enable.h"
#include <errno.h>
#include <unistd.h>
#include <security_utilities/simpleprefs.h>
#include <Security/TrustSettingsSchema.h>		/* private SPI */
#include <CoreFoundation/CFNumber.h>

typedef enum {
	utoSet = 0,
	utoShow
} UserTrustOp;

int
user_trust_enable(int argc, char * const *argv)
{
	extern int optind;
	int arg;
	UserTrustOp op = utoShow;
	CFBooleanRef disabledBool = kCFBooleanFalse;	/* what we write to prefs */
	optind = 1;
	int ourRtn = 0;

	while ((arg = getopt(argc, argv, "deh")) != -1) {
		switch (arg) {
			case 'd':
				op = utoSet;
				disabledBool = kCFBooleanTrue;
				break;
			case 'e':
				op = utoSet;
				disabledBool = kCFBooleanFalse;
				break;
			default:
			case 'h':
				return 2; /* @@@ Return 2 triggers usage message. */
		}
	}
	if(optind != argc) {
		return 2; /* @@@ Return 2 triggers usage message. */
	}

	if(op == utoShow) {
		bool utDisable = false;

		try {
			Dictionary prefsDict(kSecTrustSettingsPrefsDomain, Dictionary::US_System);
		
			/* this returns false if the pref isn't there */
			utDisable = prefsDict.getBoolValue(kSecTrustSettingsDisableUserTrustSettings);
		}
		catch(...) {
			/* prefs not there, means disable = false */
		}
		fprintf(stdout, "User-level Trust Settings are %s\n",
			utDisable ? "Disabled" : "Enabled");
		return 0;
	}

	/*  set the pref... */
	if(geteuid() != 0) {
		fprintf(stderr, "You must be root to set this preference.\n");
		return 1;
	}

	/* get a mutable copy of the existing prefs, or a fresh empty one */
	MutableDictionary *prefsDict = NULL;
	try {
		prefsDict = new MutableDictionary(kSecTrustSettingsPrefsDomain, Dictionary::US_System);
	}
	catch(...) {
		/* not there, create empty */
		prefsDict = new MutableDictionary();
	}
	prefsDict->setValue(kSecTrustSettingsDisableUserTrustSettings, disabledBool);
	if(prefsDict->writePlistToPrefs(kSecTrustSettingsPrefsDomain, Dictionary::US_System)) {
		fprintf(stdout, "...User-level Trust Settings are %s\n",
			(disabledBool == kCFBooleanTrue) ? "Disabled" : "Enabled");
	}
	else {
		fprintf(stderr, "Could not write system preferences.\n");
		ourRtn = 1;
	}
	delete prefsDict;
	return ourRtn;
}
