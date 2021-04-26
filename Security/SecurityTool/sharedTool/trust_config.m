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
 * trust_config.m
 */

#import <Foundation/Foundation.h>
#include <Security/SecTrustSettingsPriv.h>
#include <Security/SecCertificatePriv.h>
#include <utilities/fileIo.h>
#include <utilities/SecCFWrappers.h>

#include "SecurityCommands.h"

NSString* toolApplicationID = @"com.apple.security";

typedef enum {
    TrustConfigurationUnknown = 0,
    TrustConfigurationCTExceptions = 1,
    TrustConfigurationCARevocationAdditions = 2,
    TrustConfigurationTransparentConnectionPins = 3,
} TrustConfigurationType;

static TrustConfigurationType trustConfigurationTypeForOptarg(NSString *optarg)
{
    if ([optarg isEqualToString:@"ct-exceptions"]) {
        return TrustConfigurationCTExceptions;
    } else if ([optarg isEqualToString:@"ca-revocation-checking"]) {
        return TrustConfigurationCARevocationAdditions;
    } else if ([optarg isEqualToString:@"transparent-connection-pins"]) {
        return TrustConfigurationTransparentConnectionPins;
    }
    return TrustConfigurationUnknown;
}

static char *stringForTrustConfigurationType(TrustConfigurationType type)
{
    switch (type) {
        case TrustConfigurationCTExceptions:
            return "ct-exceptions";
        case TrustConfigurationCARevocationAdditions:
            return "ca-revocation-checking";
        case TrustConfigurationTransparentConnectionPins:
            return "transparent-connection-pins";
        default:
            return "unknown configuration type";
    }
}

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

static int returnCFError(TrustConfigurationType type,CFErrorRef CF_CONSUMED error) {
    CFStringRef errorString = CFErrorCopyDescription(error);
    CFStringPerformWithCString(errorString, ^(const char *utf8Str) {
        fprintf(stderr, "Trust Configuration error for %s: %s\n", stringForTrustConfigurationType(type), utf8Str);
    });
    CFIndex errCode = CFErrorGetCode(error);
    CFReleaseNull(error);
    return (int)errCode;
}

static int resetConfig(TrustConfigurationType type, bool resetCerts, bool resetDomains) {
    bool result = false;
    CFErrorRef error = NULL;
    switch(type) {
        case TrustConfigurationCTExceptions:
            if (resetCerts) {
                NSDictionary *resetCertsDict = @{ (__bridge NSString*)kSecCTExceptionsCAsKey: @[] };
                result = SecTrustStoreSetCTExceptions(NULL, (__bridge CFDictionaryRef)resetCertsDict, &error);
            }
            if (!error && resetDomains) {
                NSDictionary *resetDomainsDict = @{ (__bridge NSString*)kSecCTExceptionsDomainsKey : @[] };
                result = SecTrustStoreSetCTExceptions(NULL, (__bridge CFDictionaryRef)resetDomainsDict, &error);
            }
            break;
        case TrustConfigurationCARevocationAdditions: {
            NSDictionary *resetCertsDict = @{ (__bridge NSString*)kSecCARevocationAdditionsKey: @[] };
            result = SecTrustStoreSetCARevocationAdditions(NULL, (__bridge CFDictionaryRef)resetCertsDict, &error);
            break;
        }
        case TrustConfigurationTransparentConnectionPins:
            result = SecTrustStoreSetTransparentConnectionPins(NULL, NULL, &error);
            break;
        default:
            fprintf(stderr, "Unknown trust configuration type\n");
    }

    if (!result) {
        return returnCFError(type, error);
    }
    return 0;
}

static int setConfigPlist(TrustConfigurationType type, id plist) {
    CFErrorRef error = NULL;
    bool result = false;
    switch(type) {
        case TrustConfigurationCTExceptions:
            if (![plist isKindOfClass:[NSDictionary class]]) {
                fprintf(stderr, "Plist should be a dictionary type for %s\n",
                        stringForTrustConfigurationType(type));
                return -1;
            }
            result = SecTrustStoreSetCTExceptions(NULL, (__bridge CFDictionaryRef)plist, &error);
            break;
        case TrustConfigurationCARevocationAdditions:
            if (![plist isKindOfClass:[NSDictionary class]]) {
                fprintf(stderr, "Plist should be a dictionary type for %s\n",
                        stringForTrustConfigurationType(type));
                return -1;
            }
            result = SecTrustStoreSetCARevocationAdditions(NULL, (__bridge CFDictionaryRef)plist, &error);
            break;
        case TrustConfigurationTransparentConnectionPins:
            if (![plist isKindOfClass:[NSArray class]]) {
                fprintf(stderr, "Plist should be a array type for %s\n",
                        stringForTrustConfigurationType(type));
                return -1;
            }
            result = SecTrustStoreSetTransparentConnectionPins(NULL, (__bridge CFArrayRef)plist, &error);
            break;
        default:
            fprintf(stderr, "Unknown trust configuration type\n");
    }

    if (!result) {
        return returnCFError(type, error);
    }
    return 0;
}

static int addDictionaryConfig(TrustConfigurationType type, bool certsValues, NSArray *values) {
    CFErrorRef error = NULL;
    NSDictionary *currentConfig = NULL;
    NSString *key = @"unknown";
    switch (type) {
        case TrustConfigurationCTExceptions:
            currentConfig = CFBridgingRelease(SecTrustStoreCopyCTExceptions((__bridge CFStringRef)toolApplicationID, &error));
            key = certsValues ? (__bridge NSString *)kSecCTExceptionsCAsKey :
                                (__bridge NSString *)kSecCTExceptionsDomainsKey;
            break;
        case TrustConfigurationCARevocationAdditions:
            currentConfig = CFBridgingRelease(SecTrustStoreCopyCARevocationAdditions((__bridge CFStringRef)toolApplicationID, &error));
            key = (__bridge NSString *)kSecCARevocationAdditionsKey;
            break;
        default:
            fprintf(stderr, "Unknown trust configuration type\n");
            return SHOW_USAGE_MESSAGE;
    }
    if (!currentConfig && error) {
        return returnCFError(type, error);
    }

    NSMutableArray *configForKey = nil;
    if (currentConfig && currentConfig[key]) {
        configForKey = [currentConfig[key] mutableCopy];
        [configForKey addObjectsFromArray:values];
    } else {
        configForKey = [values copy];
    }

    NSDictionary *newConfigDict = @{key: configForKey };
    bool result = false;
    switch (type) {
        case TrustConfigurationCTExceptions:
            result = SecTrustStoreSetCTExceptions(NULL, (__bridge CFDictionaryRef)newConfigDict, &error);
            break;
        case TrustConfigurationCARevocationAdditions:
            result = SecTrustStoreSetCARevocationAdditions(NULL, (__bridge CFDictionaryRef)newConfigDict, &error);
            break;
        default:
            fprintf(stderr, "Unknown trust configuration type\n");
            return SHOW_USAGE_MESSAGE;
    }

    if (!result) {
        return returnCFError(type, error);
    }

    return 0;
}

static int addArrayConfig(TrustConfigurationType type, NSArray *values) {
    CFErrorRef error = NULL;
    NSArray *currentConfig = NULL;
    switch (type) {
        case TrustConfigurationTransparentConnectionPins:
            currentConfig = CFBridgingRelease(SecTrustStoreCopyTransparentConnectionPins((__bridge CFStringRef)toolApplicationID, &error));
            break;
        default:
            fprintf(stderr, "Unknown trust configuration type\n");
            return SHOW_USAGE_MESSAGE;
    }
    if (!currentConfig && error) {
        return returnCFError(type, error);
    }

    NSMutableArray *newConfig = nil;
    if (currentConfig) {
        newConfig = [currentConfig mutableCopy];
        [newConfig addObjectsFromArray:values];
    } else {
        newConfig = [values copy];
    }

    bool result = false;
    switch (type) {
        case TrustConfigurationTransparentConnectionPins:
            result = SecTrustStoreSetTransparentConnectionPins(NULL, (__bridge CFArrayRef)newConfig, &error);
            break;
        default:
            fprintf(stderr, "Unknown trust configuration type\n");
            return SHOW_USAGE_MESSAGE;
    }

    if (!result) {
        return returnCFError(type, error);
    }

    return 0;
}

static int addDomains(TrustConfigurationType type, NSArray *domains) {
    int status = ERR_SUCCESS;
    if ([domains count]) {
        status = addDictionaryConfig(type, false, domains);
    }
    if (status != 0) {
        fprintf(stderr, "failed to add domain configuration\n");
    }
    return status;
}

static int addCerts(TrustConfigurationType type, NSArray *certs) {
    int status = ERR_SUCCESS;
    if ([certs count]) {
        NSMutableArray<NSDictionary *>*valuesForCAsKey = [NSMutableArray arrayWithCapacity:[certs count]];
        [certs enumerateObjectsUsingBlock:^(id  _Nonnull obj, NSUInteger idx, BOOL * _Nonnull stop) {
            SecCertificateRef cert = (__bridge SecCertificateRef)obj;
            NSData* hash = CFBridgingRelease(SecCertificateCopySubjectPublicKeyInfoSHA256Digest(cert));
            NSDictionary *value = @{ (__bridge NSString*)kSecTrustStoreHashAlgorithmKey:@"sha256",
                                     (__bridge NSString*)kSecTrustStoreSPKIHashKey:hash };
            [valuesForCAsKey addObject:value];
        }];
        switch(type) {
            case TrustConfigurationCTExceptions:
            case TrustConfigurationCARevocationAdditions:
                status = addDictionaryConfig(type, true, valuesForCAsKey);
                break;
            case TrustConfigurationTransparentConnectionPins:
                status = addArrayConfig(type, valuesForCAsKey);
                break;
            default:
                fprintf(stderr, "Unknown trust configuration type\n");
                return SHOW_USAGE_MESSAGE;
        }

    }
    if (status != 0) {
        fprintf(stderr, "failed to add cert configuration\n");
    }
    return status;
}

int add_trust_config(int argc, char * const *argv) {
    int arg;

    bool resetDomains = false;
    bool resetCerts = false;
    TrustConfigurationType type = TrustConfigurationUnknown;

    NSMutableArray *domains = [NSMutableArray array];
    NSMutableArray *certs = [NSMutableArray array];
    id plist = nil;

    /* parse args */
    if (argc == 2) {
        return SHOW_USAGE_MESSAGE;
    }

    while ((arg = getopt(argc, argv, "t:d:c:r:p:")) != -1) {
        switch(arg) {
            case 't':
                type = trustConfigurationTypeForOptarg([NSString stringWithCString:optarg encoding:NSUTF8StringEncoding]);
                break;
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
                if (!plist) {
                    plist = [NSArray arrayWithContentsOfFile:[NSString stringWithCString:optarg encoding:NSUTF8StringEncoding]];
                }
                break;
            case '?':
            default:
                return SHOW_USAGE_MESSAGE;
        }
    }

    if (type == TrustConfigurationUnknown) {
        fprintf(stderr, "Unknown trust configuration type\n");
        return SHOW_USAGE_MESSAGE;
    }

    if ((domains.count > 0 || (resetDomains && !resetCerts)) && (type != TrustConfigurationCTExceptions)) {
        fprintf(stderr, "-d and domain resets not supported by %s configuration type\n",
                stringForTrustConfigurationType(type));
        return SHOW_USAGE_MESSAGE;
    }

    /* handle reset operation */
    if (resetCerts || resetDomains) {
        return resetConfig(type, resetCerts, resetDomains);
    }

    /* set plist */
    if (plist) {
        return setConfigPlist(type, plist);
    }

    /* add domains */
    int status = addDomains(type, domains);
    if (status != 0) {
        return status;
    }

    /* add certs */
    return addCerts(type, certs);
}

static int printArrayConfig(TrustConfigurationType type, NSArray *configArray) {
    if (!configArray || [configArray count] == 0) {
        fprintf(stdout, "No configuration for %s\n", stringForTrustConfigurationType(type));
        return 0;
    }

    NSMutableString *resultString = [NSMutableString stringWithFormat:@"["];
    [configArray enumerateObjectsUsingBlock:^(id  _Nonnull obj, NSUInteger idx, BOOL * _Nonnull stop) {
        if ([obj isKindOfClass:[NSString class]]) {
            if (idx == 0) {
                [resultString appendFormat:@"\"%@\"",obj];
            } else {
                [resultString appendFormat:@", \"%@\"", obj];
            }
        } else if ([obj isKindOfClass:[NSDictionary class]]) {
            if (idx == 0) {
                [resultString appendString:@"\n\t    "];
            } else {
                [resultString appendString:@"\t    "];
            }
            [resultString appendFormat:@"\"%@:", obj[(__bridge NSString*)kSecTrustStoreHashAlgorithmKey]];
            NSString *hashHex = CFBridgingRelease(CFDataCopyHexString((__bridge CFDataRef)obj[(__bridge NSString*)kSecTrustStoreSPKIHashKey]));
            [resultString appendString:hashHex];
            if ([configArray count] == idx + 1) { // last entry
                [resultString appendString:@"\"\n"];
            } else {
                [resultString appendString:@"\",\n"];
            }
        }
    }];
    [resultString appendString:@"]\n"];
    CFStringPerformWithCString((__bridge CFStringRef)resultString, ^(const char *utf8Str) {
        fprintf(stdout, "\n%s\n", utf8Str);
    });

    return 0;
}

static int printDictionaryConfig(TrustConfigurationType type, CFStringRef key, NSDictionary *allConfig) {
    if (!allConfig || !allConfig[(__bridge NSString*)key] ||
        [allConfig[(__bridge NSString*)key] count] == 0) {
        CFStringPerformWithCString(key, ^(const char *utf8Str) {
            fprintf(stdout, "No configuration for %s\n", utf8Str);
        });
        return 0;
    }

    NSArray *exceptionsForKey = allConfig[(__bridge NSString*)key];
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
            [exceptionsString appendFormat:@"\"%@:", obj[(__bridge NSString*)kSecTrustStoreHashAlgorithmKey]];
            NSString *hashHex = CFBridgingRelease(CFDataCopyHexString((__bridge CFDataRef)obj[(__bridge NSString*)kSecTrustStoreSPKIHashKey]));
            [exceptionsString appendString:hashHex];
            if ([exceptionsForKey count] == idx + 1) { // last entry
                [exceptionsString appendString:@"\"\n"];
            } else {
                [exceptionsString appendString:@"\",\n"];
            }
        }
    }];
    [exceptionsString appendString:@"\t]\n"];
    CFStringPerformWithCString((__bridge CFStringRef)exceptionsString, ^(const char *utf8Str) {
        fprintf(stdout, "\n%s\n", utf8Str);
    });

    return 0;
}

static int printDomainConfig(TrustConfigurationType type, id config) {
    if (type != TrustConfigurationCTExceptions) {
        fprintf(stderr, "Unknown trust configuration type\n");
        return SHOW_USAGE_MESSAGE;
    }
    int status = printDictionaryConfig(type, kSecCTExceptionsDomainsKey, config);
    if (status != 0) {
        fprintf(stderr, "failed to print domain config\n");
    }
    return status;
}

static int printCertConfig(TrustConfigurationType type, id config) {
    switch (type) {
        case TrustConfigurationCTExceptions:
            if (config && ![config isKindOfClass:[NSDictionary class]]) {
                fprintf(stderr, "Internal error: configuration should be a dictionary type for %s\n",
                        stringForTrustConfigurationType(type));
                return -1;
            }
            return printDictionaryConfig(type, kSecCTExceptionsCAsKey, config);
        case TrustConfigurationCARevocationAdditions:
            if (config && ![config isKindOfClass:[NSDictionary class]]) {
                fprintf(stderr, "Internal error: configuration should be a dictionary type for %s\n",
                        stringForTrustConfigurationType(type));
                return -1;
            }
            return printDictionaryConfig(type, kSecCARevocationAdditionsKey, config);
        case TrustConfigurationTransparentConnectionPins:
            if (config && ![config isKindOfClass:[NSArray class]]) {
                fprintf(stderr, "Internal error: configuration should be a array type for %s\n",
                        stringForTrustConfigurationType(type));
                return -1;
            }
            return printArrayConfig(type, config);
        default:
            fprintf(stderr, "Unknown trust configuration type\n");
            return SHOW_USAGE_MESSAGE;
    }
}

static id getConfig(TrustConfigurationType type, NSString *identifier, CFErrorRef *error) {
    id results = NULL;
    switch(type) {
        case TrustConfigurationCTExceptions:
            results = CFBridgingRelease(SecTrustStoreCopyCTExceptions((__bridge CFStringRef)identifier, error));
            break;
        case TrustConfigurationCARevocationAdditions:
            results = CFBridgingRelease(SecTrustStoreCopyCARevocationAdditions((__bridge CFStringRef)identifier, error));
            break;
        case TrustConfigurationTransparentConnectionPins:
            results = CFBridgingRelease(SecTrustStoreCopyTransparentConnectionPins((__bridge CFStringRef)identifier, error));
            break;
        default:
            fprintf(stderr, "Unknown trust configuration type\n");
    }
    return results;
}

int show_trust_config(int argc, char * const *argv) {
    int arg;
    bool allConfig = false;
    NSString *identifier = nil;
    bool domainConfig = false;
    bool certConfig = false;
    TrustConfigurationType type = TrustConfigurationUnknown;

    /* parse args */
    while ((arg = getopt(argc, argv, "t:ai:dc")) != -1) {
        switch(arg) {
            case 't':
                type = trustConfigurationTypeForOptarg([NSString stringWithCString:optarg encoding:NSUTF8StringEncoding]);
                break;
            case 'a':
                allConfig = true;
                break;
            case 'i':
                identifier = [NSString stringWithCString:optarg encoding:NSUTF8StringEncoding];
                break;
            case 'd':
                domainConfig = true;
                break;
            case 'c':
                certConfig = true;
                break;
            case '?':
            default:
                return SHOW_USAGE_MESSAGE;
        }
    }

    if (type == TrustConfigurationUnknown) {
        fprintf(stderr, "Unknown trust configuration type\n");
        return SHOW_USAGE_MESSAGE;
    }

    if (!domainConfig && !certConfig) {
        /* Nothing specified, show cert config and domain config if it exists*/
        certConfig = true;
        domainConfig = (type == TrustConfigurationCTExceptions) ? true : false;
    }

    if (!certConfig && (type != TrustConfigurationCTExceptions)) {
        fprintf(stderr, "-d not supported by %s configuration type\n",
                stringForTrustConfigurationType(type));
        return SHOW_USAGE_MESSAGE;
    }

    if (allConfig) {
        identifier = nil;
        fprintf(stdout, "Showing %s configuration for all apps\n", stringForTrustConfigurationType(type));
    } else if (!identifier) {
        identifier = toolApplicationID;
    }

    if (identifier) {
        CFStringPerformWithCString((__bridge CFStringRef)identifier, ^(const char *utf8Str) {
            fprintf(stdout, "Showing %s configuration for %s\n", stringForTrustConfigurationType(type), utf8Str);
        });
    }

    CFErrorRef error = NULL;
    id results = getConfig(type, identifier, &error);

    /* Copy failed, return error */
    if (!results && error) {
        return returnCFError(type, error);
    }

    /* print domain exceptions */
    int status = 0;
    if (domainConfig) {
        status = printDomainConfig(type, results);
    }
    if (status != 0) {
        return status;
    }

    /* print cert exceptions */
    if (certConfig) {
        status = printCertConfig(type, results);
    }

    return status;
}
