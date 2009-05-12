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

int main (int argc, const char * argv[]) {
	OSStatus err = 0;
	void *krbHelper = NULL;

	CFStringRef inHostName = NULL, inAdvertisedPrincipal = NULL, outRealm = NULL;
	const char *inHostNameString = NULL, *inAdvertisedPrincipalString = NULL;

	if (argc > 1) {
		inHostNameString = argv[1];
		inHostName = CFStringCreateWithCString (NULL, inHostNameString, kCFStringEncodingASCII);
	}
	if (argc > 2) {
		inAdvertisedPrincipalString = argv[2];
		inAdvertisedPrincipal = CFStringCreateWithCString (NULL, inAdvertisedPrincipalString, kCFStringEncodingASCII);
	}
	
	err = KRBCreateSession (inHostName, inAdvertisedPrincipal, &krbHelper);
	if (noErr != err) { 
		printf ("Error = %d from KRBCreateSession (\"%s\",\"%s\" ... )\n", err, inHostNameString, inAdvertisedPrincipalString);
		return 1;
	}

	err = KRBCopyRealm (krbHelper, &outRealm);
	if (noErr != err) { 
		printf ("Error = %d from KRBCopyRealm ()\n", err);
		return 2;
	}

	char *outRealmString = NULL;
	
	if (NULL != outRealm) {
		int length = CFStringGetMaximumSizeForEncoding (CFStringGetLength (outRealm), kCFStringEncodingUTF8) + 1;
		outRealmString = malloc (length);
	
		if (NULL != outRealmString && !CFStringGetCString (outRealm, outRealmString, length, kCFStringEncodingUTF8)) {
			free (outRealmString);
			outRealmString = NULL;
			err = 32;
		}
	}

	printf ("REALM=%s\n", outRealmString);
	return err;
}
