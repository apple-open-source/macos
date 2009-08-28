/*
 * Copyright (c) 2007 Apple Inc. All rights reserved.
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

#include <KerberosHelper/KerberosHelper.h>
#include <CoreFoundation/CoreFoundation.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include "utils.h"

int main (int argc, char **argv) {
	OSStatus err = 0;
	void *krbHelper = NULL;

	CFStringRef inHostName = NULL, inAdvertisedPrincipal = NULL;
	CFStringRef outRealm = NULL, outServer = NULL, outNoCanon = NULL;
	CFDictionaryRef outDict = NULL;
	const char *inHostNameString = NULL, *inAdvertisedPrincipalString = NULL;
	int no_lkdc = 0, ch;

	while ((ch = getopt(argc, argv, "n")) != -1) {
	    switch (ch) {
	    case 'n':
		no_lkdc = 0;
		break;
	    default:
		printf("usage");
		exit(1);
	    }
	}

	argc -= optind;
	argv += optind;

	if (argc > 0) {
		inHostNameString = argv[0];
		inHostName = CFStringCreateWithCString (NULL, inHostNameString, kCFStringEncodingASCII);
	}
	if (argc > 1) {
		inAdvertisedPrincipalString = argv[1];
		inAdvertisedPrincipal = CFStringCreateWithCString (NULL, inAdvertisedPrincipalString, kCFStringEncodingASCII);
	}
	
	err = KRBCreateSession (inHostName, inAdvertisedPrincipal, &krbHelper);
	if (noErr != err) { 
		printf ("ERROR=KRBCreateSession %d (\"%s\",\"%s\" ... )\n", (int)err, inHostNameString, inAdvertisedPrincipalString);
		return 1;
	}

	err = KRBCopyRealm (krbHelper, &outRealm);
	if (noErr != err) { 
		printf ("ERROR=%d from KRBCopyRealm ()\n", (int)err);
		return 2;
	}
	if (outRealm == NULL) {
		printf("ERROR=No realm from KRBCopyRealm\n");
                return 3;
        }

	err = KRBCopyServicePrincipalInfo (krbHelper, CFSTR("host"), &outDict);
	if (noErr != err) { 
		printf ("ERROR=%d from KRBCopyServicePrincipal ()\n", (int)err);
		return 2;
	}
	outServer = CFDictionaryGetValue(outDict, kKRBServicePrincipal);
	if (outServer == NULL) {
		printf("ERROR=No realm from KRBCopyServicePrincipal\n");
		return 3;
	}

	outNoCanon = CFDictionaryGetValue(outDict, kKRBNoCanon);

	char *outRealmString = NULL, *outServerString = NULL;
	__KRBCreateUTF8StringFromCFString(outRealm, &outRealmString);
	__KRBCreateUTF8StringFromCFString(outServer, &outServerString);

	printf ("REALM=%s\n", outRealmString);
	printf ("SERVER=%s\n", outServerString);
	printf ("DNS-CANON=%s\n", outNoCanon ? "NO" : "YES");

	__KRBReleaseUTF8String(outRealmString);
	__KRBReleaseUTF8String(outServerString);

	CFRelease(outRealm);
	CFRelease(outDict);

	KRBCloseSession(krbHelper);

	sleep(100);

	return err;
}
