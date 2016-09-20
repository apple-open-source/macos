/*
 * Copyright (c) 2016 Apple Inc. All Rights Reserved.
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
 * limitations under the xLicense.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#import <Security/Security.h>
#import <Security/SecCertificatePriv.h>

#include "keychain_regressions.h"
#include "kc-helpers.h"
#include "kc-identity-helpers.h"


//	Example of looking up a SecIdentityRef in the keychain,
//	then getting the attributes of its private key.
//

#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#include <Security/Security.h>

#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <time.h>
#include <sys/param.h>

static void PrintPrivateKeyAttributes(SecKeyRef keyRef)
{
	CFMutableDictionaryRef query = CFDictionaryCreateMutable(NULL, 0,
			&kCFTypeDictionaryKeyCallBacks,
			&kCFTypeDictionaryValueCallBacks);

	/* set up the query: find specified item, return attributes */
	//CFDictionaryAddValue( query, kSecClass, kSecClassKey );
	CFDictionaryAddValue( query, kSecValueRef, keyRef );
	CFDictionaryAddValue( query, kSecReturnAttributes, kCFBooleanTrue );

	CFTypeRef result = NULL;
    OSStatus status = SecItemCopyMatching(query, &result);
    ok_status(status, "%s: SecItemCopyMatching", testName);

	if (query)
		CFRelease(query);

    if(result) {
        CFShow(result);
    }
}

static void tests(SecKeychainRef kc)
{
	SecIdentityRef identity=NULL;
	SecKeyRef privateKeyRef=NULL;
	OSStatus status;

	identity = copyFirstIdentity(kc);
	status = SecIdentityCopyPrivateKey(identity, &privateKeyRef);
    ok_status(status, "%s: SecIdentityCopyPrivateKey", testName);

	if (privateKeyRef) {
		PrintPrivateKeyAttributes(privateKeyRef);
		CFRelease(privateKeyRef);
	}
    CFReleaseNull(identity);
}

int kc_20_identity_key_attributes(int argc, char *const *argv)
{
    plan_tests(6);
    initializeKeychainTests(__FUNCTION__);

    SecKeychainRef kc = getPopulatedTestKeychain();

	tests(kc);

    ok_status(SecKeychainDelete(kc), "%s: SecKeychainDelete", testName);
    CFReleaseNull(kc);

    deleteTestFiles();
    return 0;
}
