/*
 * Copyright (c) 2003-2007,2009-2010,2013-2023 Apple Inc. All Rights Reserved.
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
 * show_certificates.m
 */

#include <TargetConditionals.h>
#include <Foundation/Foundation.h>

#include "SecurityCommands.h"

#include "security.h"
#include "SecurityTool/sharedTool/print_cert.h"
#include "SecBase64.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "SecurityTool/sharedTool/tool_errors.h"

#include <Security/SecItem.h>

#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFDate.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFString.h>

#include <Security/SecCertificatePriv.h>
#include <Security/SecPolicyPriv.h>
#include <Security/SecTrustPriv.h>
#include <Security/SecInternal.h>
#include <Security/SecTrustStore.h>
#include <Security/SecTrustSettings.h>

#include "SecurityTool/sharedTool/readline.h"

#include <utilities/SecCFWrappers.h>
#include "keychain_util.h"

//typedef uint32_t SecProtocolType;
//typedef uint32_t SecAuthenticationType;

static const CFStringRef UNAVAILABLE_STR = CFSTR("unavailable");

#if TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR
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
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    CFArrayRef properties = SecTrustCopyProperties(trust);
#pragma clang diagnostic pop
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
    CFReleaseNull(policy);
}
#endif

static size_t print_buffer_pem(FILE *stream, const char *pem_name, size_t length,
                            const uint8_t *bytes) {
    size_t pem_name_len = strlen(pem_name);
    size_t b64_len = SecBase64Encode2(NULL, length, NULL, 0,
                                      kSecB64_F_LINE_LEN_USE_PARAM, 64, NULL);
    size_t buffer_len = 33 + 2 * pem_name_len + b64_len;
    char *buffer = malloc(buffer_len);
    char *p = buffer;
    p += snprintf(buffer, buffer_len, "-----BEGIN %s-----\n", pem_name);
    SecBase64Result result;
    p += SecBase64Encode2(bytes, length, p, b64_len,\
                          kSecB64_F_LINE_LEN_USE_PARAM, 64, &result);
    if (result) {
        free(buffer);
        return result;
    }
    p += snprintf(p, buffer_len - (p - buffer), "\n-----END %s-----\n", pem_name);
    size_t res = fwrite(buffer, 1, p - buffer, stream);
    fflush(stream);
    bzero(buffer, p - buffer);
    free(buffer);
    return res;
}

#if TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR
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
			return SHOW_USAGE_MESSAGE;
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
#endif

static bool isSettingWithResult(CFDictionaryRef setting, SecTrustSettingsResult result) {
    CFNumberRef value = CFDictionaryGetValue(setting, kSecTrustSettingsResult);
    if (!isNumberOfType(value, kCFNumberSInt64Type)) {
        return false;
    }
    int64_t setting_result = 0;
    if (!CFNumberGetValue(value, kCFNumberSInt64Type, &setting_result) ||
        (setting_result != result)) {
        return false;
    }
    return true;
}

static bool isUnconstrainedSettingWithResult(CFDictionaryRef setting, SecTrustSettingsResult result) {
    if (!isDictionary(setting) || (CFDictionaryGetCount(setting) != 1)) {
        return false;
    }

    return isSettingWithResult(setting, result);
}

static bool isDenyTrustSetting(CFArrayRef trust_settings) {
    if (CFArrayGetCount(trust_settings) != 1) {
        return false;
    }

    return isUnconstrainedSettingWithResult(CFArrayGetValueAtIndex(trust_settings, 0),
                                            kSecTrustSettingsResultDeny);
}

static bool isPartialSSLTrustSetting(CFArrayRef trust_settings) {
    if (CFArrayGetCount(trust_settings) != 2) {
        return false;
    }

    /* Second setting is a blanket "Trust" */
    if (!isUnconstrainedSettingWithResult(CFArrayGetValueAtIndex(trust_settings, 1),
                                          kSecTrustSettingsResultTrustRoot) &&
        !isUnconstrainedSettingWithResult(CFArrayGetValueAtIndex(trust_settings, 1),
                                          kSecTrustSettingsResultTrustAsRoot)) {
        return false;
    }

    /* First setting is "upspecified" for SSL policy */
    CFDictionaryRef setting = CFArrayGetValueAtIndex(trust_settings, 0);
    if (!isDictionary(setting) || (CFDictionaryGetCount(setting) < 2)) {
        return false;
    }
    if (!isSettingWithResult(setting, kSecTrustSettingsResultUnspecified)) {
        return false;
    }
    if (!CFEqualSafe(CFDictionaryGetValue(setting, kSecTrustSettingsPolicy), kSecPolicyAppleSSL)) {
        return false;
    }

    return true;
}

int trust_store_show_certificates(int argc, char * const *argv)
{
    int ch, result = 0;
    bool output_subject = false;
    bool verbose = false;
    bool trust_settings = false;
    bool output_pem = false;
    bool output_finger_print = false;
    bool output_keyid = false;
    CFArrayRef certs = NULL;

    while ((ch = getopt(argc, argv, "fpstvk")) != -1)
    {
        switch  (ch)
        {
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
                trust_settings = true;
                break;
            case 'f':
                output_finger_print = true;
                break;
            case 'k':
                output_keyid = true;
                break;
            case '?':
            default:
                return SHOW_USAGE_MESSAGE;
        }
    }

    if(SecTrustStoreCopyAll(SecTrustStoreForDomain(kSecTrustStoreDomainUser),
                             &certs) || !certs) {
        fprintf(stderr, "failed to get trust store contents for user\n");
        return 1;
    }

    CFIndex ix, count = CFArrayGetCount(certs);
    if (count) printf("*******************************************************\n");
    for (ix = 0; ix < count; ix++) {
        CFArrayRef certSettingsPair = NULL;
        CFDataRef certData = NULL;
        SecCertificateRef cert = NULL;

        certSettingsPair = CFArrayGetValueAtIndex(certs, ix);
        certData = (CFDataRef)CFArrayGetValueAtIndex(certSettingsPair, 0);
        cert = SecCertificateCreateWithData(kCFAllocatorDefault, certData);
        if (!cert) {
            fprintf(stderr, "failed to get cert at %ld\n",ix);
            return 1;
        }
        if (verbose) {
            print_cert(cert, verbose);
        } else if (output_subject) {
            CFStringRef subject = SecCertificateCopySubjectString(cert);
            if (subject) {
                CFStringWriteToFileWithNewline(subject, stdout);
                CFRelease(subject);
            }
        } else if (output_pem) {
            print_buffer_pem(stdout, "CERTIFICATE",
                             SecCertificateGetLength(cert),
                             SecCertificateGetBytePtr(cert));
        } else {
            print_cert(cert, verbose);
        }
        if (output_keyid) {
            CFDataRef key_fingerprint = SecCertificateCopyPublicKeySHA1Digest(cert);
            if (key_fingerprint) {
                int i;
                CFIndex j = CFDataGetLength(key_fingerprint);
                const uint8_t *byte = CFDataGetBytePtr(key_fingerprint);

                fprintf(stdout, "Keyid:");
                for (i = 0; i < j; i++) {
                    fprintf(stdout, " %02X", byte[i]);
                }
                fprintf(stdout, "\n");
            }
            CFReleaseSafe(key_fingerprint);
        }
        if (output_finger_print) {
            CFDataRef fingerprint = SecCertificateGetSHA1Digest(cert);
            if (fingerprint) {
                int i;
                CFIndex j = CFDataGetLength(fingerprint);
                const uint8_t *byte = CFDataGetBytePtr(fingerprint);

                fprintf(stdout, "Fingerprint:");
                for (i = 0; i < j; i++) {
                    fprintf(stdout, " %02X", byte[i]);
                }
                fprintf(stdout, "\n");
            }
        }
        if (trust_settings) {
            CFPropertyListRef trust_settings = NULL;
            trust_settings = CFArrayGetValueAtIndex(certSettingsPair, 1);
            if (trust_settings && CFGetTypeID(trust_settings) != CFArrayGetTypeID()) {
                fprintf(stderr, "failed to get trust settings for cert %ld\n", ix);
                CFReleaseNull(cert);
                return 1;
            }

            /* These are some trust settings configs used by ManagedConfiguration on iOS */
            if (CFArrayGetCount(trust_settings) == 0) {
                /* Full trust */
                fprintf(stdout, "Full trust enabled\n");
            } else if (isDenyTrustSetting(trust_settings)) {
                fprintf(stdout, "Administrator blacklisted\n");
            } else if (isPartialSSLTrustSetting(trust_settings)) {
                fprintf(stdout, "Partial trust enabled\n");
            } else {
                CFStringRef settings = CFStringCreateWithFormat(NULL, NULL, CFSTR("%@"), trust_settings);
                if (settings) {
                    char *settingsStr = CFStringToCString(settings);
                    if (settingsStr) {
                        fprintf(stdout, "Unknown trust settings:\n%s\n", settingsStr);
                        free(settingsStr);
                    }
                    CFRelease(settings);
                }
            }


        }
        printf("*******************************************************\n");
        CFReleaseNull(cert);
    }

    CFRelease(certs);
    return result;
}

int trust_store_show_pki_certificates(int argc, char * const *argv)
{
    int ch, result = 0;
    bool output_subject = false;
    bool verbose = false;
    bool trust_settings = false;
    bool output_pem = false;
    bool output_finger_print = false;
    bool output_keyid = false;
    bool output_json = false;
    CFArrayRef certs = NULL;

    while ((ch = getopt(argc, argv, "fpstvkj")) != -1)
    {
        switch  (ch)
        {
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
                trust_settings = true;
                break;
            case 'f':
                output_finger_print = true;
                break;
            case 'j':
                output_json = true;
                break;
            case 'k':
                output_keyid = true;
                break;
            case '?':
            default:
                return SHOW_USAGE_MESSAGE;
        }
    }

    if(SecTrustStoreCopyAll(SecTrustStoreForDomain(kSecTrustStoreDomainSystem),
                             &certs) || !certs) {
        fprintf(stderr, "failed to get system trust store contents\n");
        return 1;
    }

    uint64_t tsVersion = SecTrustGetTrustStoreVersionNumber(NULL);
    CFStringRef tsDigest = SecTrustCopyTrustStoreContentDigest(NULL);
    CFStringRef tsAssetVersion = SecTrustCopyTrustStoreAssetVersion(NULL);
    if (!tsDigest) { tsDigest = UNAVAILABLE_STR; }
    if (!tsAssetVersion) { tsAssetVersion = UNAVAILABLE_STR; }

    NSMutableDictionary *dict = [@{
        @"AssetVersion": [NSString stringWithString:(__bridge NSString*)tsAssetVersion],
        @"ContentDigest": [NSString stringWithString:(__bridge NSString*)tsDigest],
        @"ContentVersion": [[NSNumber numberWithUnsignedLongLong:(unsigned long long)tsVersion] stringValue],
    } mutableCopy];
    CFReleaseNull(tsDigest);
    CFReleaseNull(tsAssetVersion);

    if (certs) {
        CFIndex ix, count = (certs) ? CFArrayGetCount(certs) : 0;
        NSMutableArray *digestsArray = [NSMutableArray arrayWithCapacity:count];
        for (ix = 0; ix < count; ix++) {
            //CFArrayRef certSettingsPair = NULL;
            CFDataRef certData = NULL;
            SecCertificateRef cert = NULL;

            //certSettingsPair = CFArrayGetValueAtIndex(certs, ix);
            //certData = (CFDataRef)CFArrayGetValueAtIndex(certSettingsPair, 0);
            //cert = SecCertificateCreateWithData(kCFAllocatorDefault, certData);
            //%%% currently this is just the certificates; fix to include settings pair
            certData = (CFDataRef)CFArrayGetValueAtIndex(certs, ix);
            cert = SecCertificateCreateWithData(kCFAllocatorDefault, certData);

            CFDataRef fingerprint = (cert) ? SecCertificateCopySHA256Digest(cert) : NULL;
            CFStringRef digest = (fingerprint) ? CFDataCopyHexString(fingerprint) : NULL;
            if (digest) {
                [digestsArray addObject:(__bridge NSString*)digest];
            }
            CFReleaseNull(digest);
            CFReleaseNull(fingerprint);
            CFReleaseNull(cert);
        }
        dict[@"Digests"] = digestsArray;
    }
    if (output_json) {
        NSError *error = nil;
        NSJSONWritingOptions opts = NSJSONWritingWithoutEscapingSlashes | NSJSONWritingSortedKeys;
        NSData *data = [NSJSONSerialization dataWithJSONObject:dict options:opts error:&error];
        NSString *string = [[NSString alloc] initWithBytes:data.bytes length:data.length encoding:NSUTF8StringEncoding];
        const char *p = [string UTF8String];
        if (p) { printf("%s\n", p); }
        string = nil;
    } else {
        printf("AssetVersion: %s\n", [dict[@"AssetVersion"] UTF8String]);
        printf("ContentDigest: %s\n", [dict[@"ContentDigest"] UTF8String]);
        printf("ContentVersion: %s\n", [dict[@"ContentVersion"] UTF8String]);
    }

    CFIndex ix, count = (certs) ? CFArrayGetCount(certs) : 0;
    if (count && !output_json) printf("*******************************************************\n");
    for (ix = 0; ix < count && !output_json; ix++) {
        CFArrayRef certSettingsPair = NULL;
        CFDataRef certData = NULL;
        SecCertificateRef cert = NULL;

        //certSettingsPair = CFArrayGetValueAtIndex(certs, ix);
        //certData = (CFDataRef)CFArrayGetValueAtIndex(certSettingsPair, 0);
        //cert = SecCertificateCreateWithData(kCFAllocatorDefault, certData);
        //%%% currently this is just the certificates; fix to include settings pair
        certData = (CFDataRef)CFArrayGetValueAtIndex(certs, ix);
        cert = SecCertificateCreateWithData(kCFAllocatorDefault, certData);
        if (!cert) {
            fprintf(stderr, "failed to get cert at %ld\n",ix);
            return 1;
        }
        if (verbose) {
            print_cert(cert, verbose);
        } else if (output_subject) {
            CFStringRef subject = SecCertificateCopySubjectString(cert);
            if (subject) {
                CFStringWriteToFileWithNewline(subject, stdout);
                CFRelease(subject);
            }
        } else if (output_pem) {
            print_buffer_pem(stdout, "CERTIFICATE",
                             SecCertificateGetLength(cert),
                             SecCertificateGetBytePtr(cert));
        } else {
            print_cert(cert, verbose);
        }
        if (output_keyid) {
            CFDataRef key_fingerprint = SecCertificateCopyPublicKeySHA256Digest(cert);
            if (key_fingerprint) {
                int i;
                CFIndex j = CFDataGetLength(key_fingerprint);
                const uint8_t *byte = CFDataGetBytePtr(key_fingerprint);

                fprintf(stdout, "Keyid:");
                for (i = 0; i < j; i++) {
                    fprintf(stdout, " %02X", byte[i]);
                }
                fprintf(stdout, "\n");
            }
            CFReleaseSafe(key_fingerprint);
        }
        if (output_finger_print) {
            CFDataRef fingerprint = SecCertificateCopySHA256Digest(cert);
            if (fingerprint) {
                int i;
                CFIndex j = CFDataGetLength(fingerprint);
                const uint8_t *byte = CFDataGetBytePtr(fingerprint);

                fprintf(stdout, "Fingerprint:");
                for (i = 0; i < j; i++) {
                    fprintf(stdout, " %02X", byte[i]);
                }
                fprintf(stdout, "\n");
            }
            CFReleaseSafe(fingerprint);
        }
        if (trust_settings) {
            CFPropertyListRef trust_settings = NULL;
            trust_settings = CFArrayGetValueAtIndex(certSettingsPair, 1);
            if (trust_settings && CFGetTypeID(trust_settings) != CFArrayGetTypeID()) {
                fprintf(stderr, "failed to get trust settings for cert %ld\n", ix);
                CFReleaseNull(cert);
                return 1;
            }

            /* These are some trust settings configs used by ManagedConfiguration on iOS */
            if (CFArrayGetCount(trust_settings) == 0) {
                /* Full trust */
                fprintf(stdout, "Full trust enabled\n");
            } else if (isDenyTrustSetting(trust_settings)) {
                fprintf(stdout, "Administrator blacklisted\n");
            } else if (isPartialSSLTrustSetting(trust_settings)) {
                fprintf(stdout, "Partial trust enabled\n");
            } else {
                CFStringRef settings = CFStringCreateWithFormat(NULL, NULL, CFSTR("%@"), trust_settings);
                if (settings) {
                    char *settingsStr = CFStringToCString(settings);
                    if (settingsStr) {
                        fprintf(stdout, "Unknown trust settings:\n%s\n", settingsStr);
                        free(settingsStr);
                    }
                    CFRelease(settings);
                }
            }


        }
        printf("*******************************************************\n");
        CFReleaseNull(cert);
    }

    CFReleaseNull(certs);
    return result;
}
