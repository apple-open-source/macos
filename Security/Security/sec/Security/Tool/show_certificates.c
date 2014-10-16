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
 * keychain_find.c
 */

#include <TargetConditionals.h>
#if TARGET_OS_EMBEDDED

#include "SecurityCommands.h"

#include "security.h"
#include "SecurityTool/print_cert.h"
#include "SecBase64.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <SecurityTool/tool_errors.h>

#include <Security/SecItem.h>

#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFDate.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFString.h>

#include <Security/SecCertificatePriv.h>
#include <Security/SecPolicyPriv.h>
#include <Security/SecTrustPriv.h>
#include <Security/SecInternal.h>

#include <SecurityTool/readline.h>

#include <utilities/SecCFWrappers.h>
#include "keychain_util.h"

typedef uint32_t SecProtocolType;
typedef uint32_t SecAuthenticationType;


static void show_cert_eval(CFArrayRef certs, bool verbose) {
    SecPolicyRef policy = SecPolicyCreateSSL(true, NULL);
    SecTrustRef trust = NULL;
    SecTrustCreateWithCertificates(certs, policy, &trust);
    SecTrustResultType trustResult;
    const char *trustResults[] = {
        "invalid",
        "proceed",
        "confirm",
        "deny",
        "unspecified",
        "recoverable trust failure",
        "fatal trust failure",
        "other error",
    };
    (void) SecTrustEvaluate(trust, &trustResult);
    printf("* trust: %s *\n", trustResults[trustResult]);
    CFArrayRef properties = SecTrustCopyProperties(trust);
    print_plist(properties);
    CFReleaseNull(properties);
    CFIndex ix, count = SecTrustGetCertificateCount(trust);
    for (ix = 0; ix < count; ++ix) {
        printf("* cert %ld summary properties *\n", ix);
        properties = SecTrustCopySummaryPropertiesAtIndex(trust, ix);
        print_plist(properties);
        CFReleaseNull(properties);
        if (verbose) {
            printf("* cert %ld detail properties *\n", ix);
            properties = SecTrustCopyDetailedPropertiesAtIndex(trust, ix);
            print_plist(properties);
            CFReleaseNull(properties);
        }
    }

    CFDictionaryRef info = SecTrustCopyInfo(trust);
    if (info) {
        printf("* info *\n");
        CFShow(info);
        CFReleaseNull(info);
    }
}

static size_t print_buffer_pem(FILE *stream, const char *pem_name, size_t length,
                            const uint8_t *bytes) {
    size_t pem_name_len = strlen(pem_name);
    size_t b64_len = SecBase64Encode2(NULL, length, NULL, 0,
                                      kSecB64_F_LINE_LEN_USE_PARAM, 64, NULL);
    char *buffer = malloc(33 + 2 * pem_name_len + b64_len);
    char *p = buffer;
    p += sprintf(buffer, "-----BEGIN %s-----\n", pem_name);
    SecBase64Result result;
    p += SecBase64Encode2(bytes, length, p, b64_len,\
                          kSecB64_F_LINE_LEN_USE_PARAM, 64, &result);
    if (result) {
        free(buffer);
        return result;
    }
    p += sprintf(p, "\n-----END %s-----\n", pem_name);
    size_t res = fwrite(buffer, 1, p - buffer, stream);
    fflush(stream);
    bzero(buffer, p - buffer);
    free(buffer);
    return res;
}

int keychain_show_certificates(int argc, char * const *argv)
{
	int ch, result = 0;
	bool output_subject = false;
	bool verbose = false;
    bool trust_eval = false;
    bool keychain_certs = false;
    bool output_pem = false;
    bool output_finger_print = false;
    CFMutableArrayRef certs = NULL;
    CFMutableDictionaryRef query = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

	while ((ch = getopt(argc, argv, "kfq:pstv")) != -1)
	{
		switch  (ch)
		{
        case 'k':
            keychain_certs = true;
            break;
        case 'p':
            output_pem = true;
            break;
		case 's':
			output_subject = true;
			break;
        case 'v':
            verbose = true;
            break;
		case 't':
			trust_eval = true;
			break;
        case 'f':
            output_finger_print = true;
            break;
        case 'q':
            if (!keychain_query_parse_cstring(query, optarg)) {
                CFReleaseNull(query);
                return 1;
            }
            keychain_certs = true;
            break;
        case '?':
		default:
			return 2; /* @@@ Return 2 triggers usage message. */
		}
	}
  
	argc -= optind;
	argv += optind;

    if ((keychain_certs && argc > 0) || (!keychain_certs && argc < 1))
        result = 2;

    if (trust_eval)
        certs = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);

    CFArrayRef kc_certs = NULL;
    int arg;
    if (keychain_certs) {
        for (arg = 0; arg < argc; ++arg) {
            if (!keychain_query_parse_cstring(query, argv[arg])) {
                CFReleaseSafe(query);
                CFReleaseSafe(certs);
                return 1;
            }
        }
        CFDictionarySetValue(query, kSecClass, kSecClassCertificate);
        CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitAll);
        CFDictionarySetValue(query, kSecReturnRef, kCFBooleanTrue);
        CFTypeRef results;
        if (!SecItemCopyMatching(query, &results)) {
            kc_certs = results;
            argc = (int) CFArrayGetCount(kc_certs);
        }
    }

    for (arg = 0; arg < argc; ++arg) {
        SecCertificateRef cert = NULL;
        if (keychain_certs) {
            cert = (SecCertificateRef)CFArrayGetValueAtIndex(kc_certs, arg);
        } else {
            CFDataRef data = copyFileContents(argv[arg]);
            if (data) {
                cert = SecCertificateCreateWithData(
                    kCFAllocatorDefault, data);
                if (!cert) {
                    /* DER failed, try PEM. */
                    cert = SecCertificateCreateWithPEM(kCFAllocatorDefault, data);
                }
                CFRelease(data);
            } else {
                result = 1;
            }
        }

        if (cert) {
            if (!keychain_certs) {
                printf(
                       "*******************************************************\n"
                       "%s\n"
                       "*******************************************************\n"
                       , argv[arg]);
            }
            if (trust_eval) {
                if (keychain_certs) {
                    CFArraySetValueAtIndex(certs, 0, cert);
                    show_cert_eval(certs, verbose);
                } else {
                    CFArrayAppendValue(certs, cert);
                }
            } else {
                if (verbose) {
                    print_cert(cert, verbose);
                } else if (output_subject) {
                    CFStringRef subject = SecCertificateCopySubjectString(cert);
                    if (subject) {
                        CFStringWriteToFileWithNewline(subject, stdout);
                        CFRelease(subject);
                    }
                } else if (!output_pem) {
                    print_cert(cert, verbose);
                }
            }
            if (output_finger_print) {
                CFDataRef key_fingerprint = SecCertificateCopyPublicKeySHA1Digest(cert);
                if (key_fingerprint) {
                    int i;
                    CFIndex j = CFDataGetLength(key_fingerprint);
                    const uint8_t *byte = CFDataGetBytePtr(key_fingerprint);

                    fprintf(stdout, "Key fingerprint:");
                    for (i = 0; i < j; i++) {
                        fprintf(stdout, " %02X", byte[i]);
                    }
                    fprintf(stdout, "\n");
                }
                CFReleaseSafe(key_fingerprint);
            }
            if (output_pem) {
                print_buffer_pem(stdout, "CERTIFICATE",
                                 SecCertificateGetLength(cert),
                                 SecCertificateGetBytePtr(cert));
            }
            if (!keychain_certs) {
                CFRelease(cert);
            }
        } else {
            result = 1;
            fprintf(stderr, "file %s: does not contain a valid certificate",
                argv[arg]);
        }
    }

    if (trust_eval && !keychain_certs)
        show_cert_eval(certs, verbose);

    CFReleaseSafe(kc_certs);
    CFReleaseSafe(certs);

	return result;
}

#endif // TARGET_OS_EMBEDDED
