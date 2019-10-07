/*
 * Copyright (c) 2018 Apple Inc. All Rights Reserved.
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
 * ct_exceptions.m
 */

#import <Foundation/Foundation.h>
#include <Security/SecTrustSettingsPriv.h>
#include <Security/SecCertificatePriv.h>
#include <utilities/fileIo.h>
#include <utilities/SecCFWrappers.h>

#include "SecurityCommands.h"

NSString* toolAppID = @"com.apple.security";

static int addCertFile(const char *fileName, NSMutableArray *array) {
    SecCertificateRef certRef = NULL;
    NSData *data = NULL;
    unsigned char *buf = NULL;
    size_t numBytes;
    int rtn = 0;

    if (readFileSizet(fileName, &buf, &numBytes)) {
        rtn = -1;
        goto errOut;
    }

    data = [NSData dataWithBytes:buf length:numBytes];
    certRef = SecCertificateCreateWithData(NULL, (__bridge CFDataRef)data);
    if (!certRef) {
        certRef = SecCertificateCreateWithPEM(NULL, (__bridge CFDataRef)data);
        if (!certRef) {
            rtn = -1;
            goto errOut;
        }
    }

    [array addObject:(__bridge id)certRef];

errOut:
    /* Cleanup */
    free(buf);
    CFReleaseNull(certRef);
    return rtn;
}

static int returnCFError(CFErrorRef CF_CONSUMED error) {
    CFStringRef errorString = CFErrorCopyDescription(error);
    CFStringPerformWithCString(errorString, ^(const char *utf8Str) {
        fprintf(stderr, "Failed to copy CT exceptions: %s\n", utf8Str);
    });
    CFIndex errCode = CFErrorGetCode(error);
    CFReleaseNull(error);
    return (int)errCode;
}

static int resetExceptions(bool resetCerts, bool resetDomains) {
    bool result = false;
    CFErrorRef error = NULL;
    if (resetCerts) {
        NSDictionary *resetCertsDict = @{ (__bridge NSString*)kSecCTExceptionsCAsKey: @[] };
        result = SecTrustStoreSetCTExceptions(NULL, (__bridge CFDictionaryRef)resetCertsDict, &error);
    }
    if (!error && resetDomains) {
        NSDictionary *resetDomainsDict = @{ (__bridge NSString*)kSecCTExceptionsDomainsKey : @[] };
        result = SecTrustStoreSetCTExceptions(NULL, (__bridge CFDictionaryRef)resetDomainsDict, &error);
    }
    if (!result) {
        return returnCFError(error);
    }
    return 0;
}

static int addExceptions(CFStringRef key, NSArray *newExceptions) {
    CFErrorRef error = NULL;
    NSDictionary *currentExceptions = CFBridgingRelease(SecTrustStoreCopyCTExceptions((__bridge CFStringRef)toolAppID, &error));
    if (!currentExceptions && error) {
        return returnCFError(error);
    }

    NSMutableArray *exceptionsForKey = nil;
    if (currentExceptions && currentExceptions[(__bridge NSString*)key]) {
        exceptionsForKey = [currentExceptions[(__bridge NSString*)key] mutableCopy];
        [exceptionsForKey addObjectsFromArray:newExceptions];
    } else {
        exceptionsForKey = [newExceptions copy];
    }

    NSDictionary *newExceptionsDict = @{ (__bridge NSString*)key: exceptionsForKey };
    bool result = SecTrustStoreSetCTExceptions(NULL, (__bridge CFDictionaryRef)newExceptionsDict, &error);
    if (!result) {
        return returnCFError(error);
    }

    return 0;
}

int add_ct_exceptions(int argc, char * const *argv) {
    int arg;

    bool resetDomains = false;
    bool resetCerts = false;

    NSMutableArray *domains = [NSMutableArray array];
    NSMutableArray *certs = [NSMutableArray array];
    NSDictionary *plist = nil;

    /* parse args */
    if (argc == 1) {
        return SHOW_USAGE_MESSAGE;
    }

    while ((arg = getopt(argc, argv, "d:c:r:p:")) != -1) {
        switch(arg) {
            case 'd': {
                NSString *domain = [NSString stringWithCString:optarg encoding:NSUTF8StringEncoding];
                [domains addObject:domain];
                break;
            }
            case 'c':
                if (addCertFile(optarg, certs)) {
                    fprintf(stderr, "Failed to read cert file\n");
                    return 1;
                }
                break;
            case 'r':
                if (!strcmp(optarg, "all")) {
                    resetDomains = true;
                    resetCerts = true;
                } else if (!strcmp(optarg, "domain")) {
                    resetDomains = true;
                } else if (!strcmp(optarg, "cert")) {
                    resetCerts = true;
                } else {
                    return SHOW_USAGE_MESSAGE;
                }
                break;
            case 'p':
                plist = [NSDictionary dictionaryWithContentsOfFile:[NSString stringWithCString:optarg encoding:NSUTF8StringEncoding]];
                break;
            case '?':
            default:
                return SHOW_USAGE_MESSAGE;
        }
    }

    /* handle reset operation */
    if (resetCerts || resetDomains) {
        return resetExceptions(resetCerts, resetDomains);
    }

    /* set plist */
    if (plist) {
        CFErrorRef error = NULL;
        bool result = SecTrustStoreSetCTExceptions(NULL, (__bridge CFDictionaryRef)plist, &error);
        if (!result) {
            return returnCFError(error);
        } else {
            return 0;
        }
    }

    /* add domains */
    int status = 0;
    if ([domains count]) {
        status = addExceptions(kSecCTExceptionsDomainsKey, domains);
    }
    if (status != 0) {
        fprintf(stderr, "failed to add domain exceptions\n");
        return status;
    }

    /* add certs */
    if ([certs count]) {
        NSMutableArray<NSDictionary *>*valuesForCAsKey = [NSMutableArray arrayWithCapacity:[certs count]];
        [certs enumerateObjectsUsingBlock:^(id  _Nonnull obj, NSUInteger idx, BOOL * _Nonnull stop) {
            SecCertificateRef cert = (__bridge SecCertificateRef)obj;
            NSData* hash = CFBridgingRelease(SecCertificateCopySubjectPublicKeyInfoSHA256Digest(cert));
            NSDictionary *value = @{ (__bridge NSString*)kSecCTExceptionsHashAlgorithmKey:@"sha256",
                                     (__bridge NSString*)kSecCTExceptionsSPKIHashKey:hash };
            [valuesForCAsKey addObject:value];
        }];
        status = addExceptions(kSecCTExceptionsCAsKey, valuesForCAsKey);
    }
    if (status != 0) {
        fprintf(stderr, "failed to add cert exceptions\n");
        return status;
    }

    return 0;
}

static int printExceptions(CFStringRef key, NSDictionary *allExceptions) {
    if (!allExceptions || !allExceptions[(__bridge NSString*)key] ||
        [allExceptions[(__bridge NSString*)key] count] == 0) {
        CFStringPerformWithCString(key, ^(const char *utf8Str) {
            fprintf(stdout, "No CT Exceptions for %s\n", utf8Str);
        });
        return 0;
    }

    NSArray *exceptionsForKey = allExceptions[(__bridge NSString*)key];
    NSMutableString *exceptionsString = [NSMutableString stringWithFormat:@"\t%@ : [",key];
    [exceptionsForKey enumerateObjectsUsingBlock:^(id  _Nonnull obj, NSUInteger idx, BOOL * _Nonnull stop) {
        if ([obj isKindOfClass:[NSString class]]) {
            if (idx == 0) {
                [exceptionsString appendFormat:@"\"%@\"",obj];
            } else {
                [exceptionsString appendFormat:@", \"%@\"", obj];
            }
        } else if ([obj isKindOfClass:[NSDictionary class]]) {
            if (idx == 0) {
                [exceptionsString appendString:@"\n\t    "];
            } else {
                [exceptionsString appendString:@"\t    "];
            }
            [exceptionsString appendFormat:@"\"%@:", obj[(__bridge NSString*)kSecCTExceptionsHashAlgorithmKey]];
            NSString *hashHex = CFBridgingRelease(CFDataCopyHexString((__bridge CFDataRef)obj[(__bridge NSString*)kSecCTExceptionsSPKIHashKey]));
            [exceptionsString appendString:hashHex];
            if ([exceptionsForKey count] == idx + 1) { // last entry
                [exceptionsString appendString:@"\"\n"];
            } else {
                [exceptionsString appendString:@"\",\n"];
            }
        }
    }];
    [exceptionsString appendString:@"]\n"];
    CFStringPerformWithCString((__bridge CFStringRef)exceptionsString, ^(const char *utf8Str) {
        fprintf(stdout, "\n%s\n", utf8Str);
    });

    return 0;
}

int show_ct_exceptions(int argc, char * const *argv) {
    int arg;
    bool allExceptions = false;
    NSString *identifier = nil;
    bool domainExceptions = false;
    bool certExceptions = false;

    /* parse args */
    while ((arg = getopt(argc, argv, "ai:dc")) != -1) {
        switch(arg) {
            case 'a':
                allExceptions = true;
                break;
            case 'i':
                identifier = [NSString stringWithCString:optarg encoding:NSUTF8StringEncoding];
                break;
            case 'd':
                domainExceptions = true;
                break;
            case 'c':
                certExceptions = true;
                break;
            case '?':
            default:
                return SHOW_USAGE_MESSAGE;
        }
    }

    if (!domainExceptions && !certExceptions) {
        /* Nothing specified, show both */
        domainExceptions = true;
        certExceptions = true;
    }

    if (allExceptions) {
        identifier = nil;
        fprintf(stdout, "Showing exceptions for all apps\n");
    } else if (!identifier) {
        identifier = toolAppID;
    }

    if (identifier) {
        CFStringPerformWithCString((__bridge CFStringRef)identifier, ^(const char *utf8Str) {
            fprintf(stdout, "Showing exceptions for %s\n", utf8Str);
        });
    }

    CFErrorRef error = NULL;
    NSDictionary *results = CFBridgingRelease(SecTrustStoreCopyCTExceptions((__bridge CFStringRef)identifier, &error));

    /* Copy failed, return error */
    if (!results && error) {
        return returnCFError(error);
    }

    /* print domain exceptions */
    int status = 0;
    if (domainExceptions) {
        status = printExceptions(kSecCTExceptionsDomainsKey, results);
    }
    if (status != 0) {
        fprintf(stderr, "failed to print domain exceptions\n");
        return status;
    }

    /* print cert exceptions */
    if (certExceptions) {
        status = printExceptions(kSecCTExceptionsCAsKey, results);
    }
    if (status != 0) {
        fprintf(stderr, "failed to print cert exceptions\n");
        return status;
    }


    return 0;
}
