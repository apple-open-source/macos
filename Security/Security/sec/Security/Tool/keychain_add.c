/*
 * Copyright (c) 2003-2007,2009-2010,2013-2014 Apple Inc. All Rights Reserved.
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
 * keychain_add.c
 */

#include <TargetConditionals.h>
#if TARGET_OS_EMBEDDED

#include "Securitycommands.h"

#include "security.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFString.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecTrustStore.h>

#include <SecurityTool/readline.h>
#include <SecurityTool/tool_errors.h>

static int
do_add_certificates(const char *keychainName, bool trustSettings,
	int argc, char * const *argv)
{
	int ix, result = 0;
	OSStatus status;

	CFMutableDictionaryRef attributes =
		CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
	CFDictionarySetValue(attributes, kSecClass, kSecClassCertificate);

	for (ix = 0; ix < argc; ++ix) {
        CFDataRef data = copyFileContents(argv[ix]);
        if (data) {
            SecCertificateRef cert = SecCertificateCreateWithData(
                kCFAllocatorDefault, data);
            if (!cert) {
                cert = SecCertificateCreateWithPEM(kCFAllocatorDefault, data);
            }
            CFRelease(data);
            if (cert) {
				if (trustSettings) {
					SecTrustStoreSetTrustSettings(
						SecTrustStoreForDomain(kSecTrustStoreDomainUser),
						cert, NULL);
				} else {
					CFDictionarySetValue(attributes, kSecValueRef, cert);
					status = SecItemAdd(attributes, NULL);
					CFRelease(cert);
					if (status) {
						fprintf(stderr, "file %s: SecItemAdd %s",
							argv[ix], sec_errstr(status));
						result = 1;
					}
				}
            } else {
                result = 1;
                fprintf(stderr, "file %s: does not contain a valid certificate",
                    argv[ix]);
            }
        } else {
            result = 1;
        }
    }

    CFRelease(attributes);

	return result;
}


int
keychain_add_certificates(int argc, char * const *argv)
{
	int ch, result = 0;
	const char *keychainName = NULL;
	bool trustSettings = false;
	while ((ch = getopt(argc, argv, "hk:t")) != -1)
	{
		switch  (ch)
		{
        case 'k':
            keychainName = optarg;
			if (*keychainName == '\0')
				return 2;
            break;
        case 't':
            trustSettings = true;
            break;
		case '?':
		default:
			return 2; /* Return 2 triggers usage message. */
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 0)
		return 2;

	result = do_add_certificates(keychainName, trustSettings, argc, argv);

	return result;
}

#endif // TARGET_OS_EMBEDDED
