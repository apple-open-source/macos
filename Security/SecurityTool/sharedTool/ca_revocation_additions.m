/*
 * Copyright (c) 2020 Apple Inc. All Rights Reserved.
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
 * ca_revocation_additions.m
 */

#import <Foundation/Foundation.h>
#include <Security/SecTrustSettingsPriv.h>
#include <Security/SecCertificatePriv.h>
#include <utilities/fileIo.h>
#include <utilities/SecCFWrappers.h>

#include "SecurityCommands.h"

NSString* secToolAppID = @"com.apple.security";

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
        fprintf(stderr, "Failed to copy CA revocation additions: %s\n", utf8Str);
    });
    CFIndex errCode = CFErrorGetCode(error);
    CFReleaseNull(error);
    return (int)errCode;
}

static int resetRevocationAdditions(bool resetCerts) {
    bool result = false;
    CFErrorRef error = NULL;
    if (resetCerts) {
        NSDictionary *resetCertsDict = @{ (__bridge NSString*)kSecCARevocationAdditionsKey: @[] };
        result = SecTrustStoreSetCARevocationAdditions(NULL, (__bridge CFDictionaryRef)resetCertsDict, &error);
    }
    if (!result) {
        return returnCFError(error);
    }
    return 0;
}

static int addRevocationAdditions(CFStringRef key, NSArray *newAdditions) {
    CFErrorRef error = NULL;
    NSDictionary *currentAdditions = CFBridgingRelease(SecTrustStoreCopyCARevocationAdditions((__bridge CFStringRef)secToolAppID, &error));
    if (!currentAdditions && error) {
        return returnCFError(error);
    }

    NSMutableArray *additionsForKey = nil;
    if (currentAdditions && currentAdditions[(__bridge NSString*)key]) {
        additionsForKey = [currentAdditions[(__bridge NSString*)key] mutableCopy];
        [additionsForKey addObjectsFromArray:newAdditions];
    } else {
        additionsForKey = [newAdditions copy];
    }

    NSDictionary *newAdditionsDict = @{ (__bridge NSString*)key: additionsForKey };
    bool result = SecTrustStoreSetCARevocationAdditions(NULL, (__bridge CFDictionaryRef)newAdditionsDict, &error);
    if (!result) {
        return returnCFError(error);
    }

    return 0;
}

int add_ca_revocation_checking(int argc, char * const *argv) {
    int arg;

    bool resetCerts = false;

    NSMutableArray *certs = [NSMutableArray array];
    NSDictionary *plist = nil;

    /* parse args */
    if (argc == 1) {
        return SHOW_USAGE_MESSAGE;
    }

    while ((arg = getopt(argc, argv, "c:r:p:")) != -1) {
        switch(arg) {
            case 'c':
                if (addCertFile(optarg, certs)) {
                    fprintf(stderr, "Failed to read cert file\n");
                    return 1;
                }
                break;
            case 'r':
                if (!strcmp(optarg, "all")) {
                    resetCerts = true;
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
    if (resetCerts) {
        return resetRevocationAdditions(resetCerts);
    }

    /* set plist */
    if (plist) {
        CFErrorRef error = NULL;
        bool result = SecTrustStoreSetCARevocationAdditions(NULL, (__bridge CFDictionaryRef)plist, &error);
        if (!result) {
            return returnCFError(error);
        } else {
            return 0;
        }
    }

    /* add certs */
    int status = 0;
    if ([certs count]) {
        NSMutableArray<NSDictionary *>*valuesForCAsKey = [NSMutableArray arrayWithCapacity:[certs count]];
        [certs enumerateObjectsUsingBlock:^(id  _Nonnull obj, NSUInteger idx, BOOL * _Nonnull stop) {
            SecCertificateRef cert = (__bridge SecCertificateRef)obj;
            NSData* hash = CFBridgingRelease(SecCertificateCopySubjectPublicKeyInfoSHA256Digest(cert));
            NSDictionary *value = @{ (__bridge NSString*)kSecCARevocationHashAlgorithmKey:@"sha256",
                                     (__bridge NSString*)kSecCARevocationSPKIHashKey:hash };
            [valuesForCAsKey addObject:value];
        }];
        status = addRevocationAdditions(kSecCARevocationAdditionsKey, valuesForCAsKey);
    }
    if (status != 0) {
        fprintf(stderr, "failed to add cert revocation additions\n");
        return status;
    }

    return 0;
}

static int printRevocationAdditions(CFStringRef key, NSDictionary *allAdditions) {
    if (!allAdditions || !allAdditions[(__bridge NSString*)key] ||
        [allAdditions[(__bridge NSString*)key] count] == 0) {
        CFStringPerformWithCString(key, ^(const char *utf8Str) {
            fprintf(stdout, "No revocation additions for %s\n", utf8Str);
        });
        return 0;
    }

    NSArray *additionsForKey = allAdditions[(__bridge NSString*)key];
    NSMutableString *additionsString = [NSMutableString stringWithFormat:@"\t%@ : [",key];
    [additionsForKey enumerateObjectsUsingBlock:^(id  _Nonnull obj, NSUInteger idx, BOOL * _Nonnull stop) {
        if ([obj isKindOfClass:[NSString class]]) {
            if (idx == 0) {
                [additionsString appendFormat:@"\"%@\"",obj];
            } else {
                [additionsString appendFormat:@", \"%@\"", obj];
            }
        } else if ([obj isKindOfClass:[NSDictionary class]]) {
            if (idx == 0) {
                [additionsString appendString:@"\n\t    "];
            } else {
                [additionsString appendString:@"\t    "];
            }
            [additionsString appendFormat:@"\"%@:", obj[(__bridge NSString*)kSecCARevocationHashAlgorithmKey]];
            NSString *hashHex = CFBridgingRelease(CFDataCopyHexString((__bridge CFDataRef)obj[(__bridge NSString*)kSecCARevocationSPKIHashKey]));
            [additionsString appendString:hashHex];
            if ([additionsForKey count] == idx + 1) { // last entry
                [additionsString appendString:@"\"\n"];
            } else {
                [additionsString appendString:@"\",\n"];
            }
        }
    }];
    [additionsString appendString:@"]\n"];
    CFStringPerformWithCString((__bridge CFStringRef)additionsString, ^(const char *utf8Str) {
        fprintf(stdout, "\n%s\n", utf8Str);
    });

    return 0;
}

int show_ca_revocation_checking(int argc, char * const *argv) {
    int arg;
    bool allAdditions = false;
    NSString *identifier = nil;
    bool certAdditions = false;

    /* parse args */
    while ((arg = getopt(argc, argv, "ai:c")) != -1) {
        switch(arg) {
            case 'a':
                allAdditions = true;
                break;
            case 'i':
                identifier = [NSString stringWithCString:optarg encoding:NSUTF8StringEncoding];
                break;
            case 'c':
                certAdditions = true;
                break;
            case '?':
            default:
                return SHOW_USAGE_MESSAGE;
        }
    }

    if (allAdditions) {
        identifier = nil;
        fprintf(stdout, "Showing revocation additions for all apps\n");
    } else if (!identifier) {
        identifier = secToolAppID;
    }

    if (identifier) {
        CFStringPerformWithCString((__bridge CFStringRef)identifier, ^(const char *utf8Str) {
            fprintf(stdout, "Showing revocation additions for %s\n", utf8Str);
        });
    }

    CFErrorRef error = NULL;
    NSDictionary *results = CFBridgingRelease(SecTrustStoreCopyCARevocationAdditions((__bridge CFStringRef)identifier, &error));

    /* Copy failed, return error */
    if (!results && error) {
        return returnCFError(error);
    }

    /* print cert revocation additions */
    int status = 0;
    if (certAdditions) {
        status = printRevocationAdditions(kSecCARevocationAdditionsKey, results);
    }
    if (status != 0) {
        fprintf(stderr, "failed to print revocation additions\n");
        return status;
    }


    return 0;
}
