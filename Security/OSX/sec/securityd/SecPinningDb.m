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
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 *
 */

/*
 *  SecPinningDb.m
 */

#include <AssertMacros.h>
#import <Foundation/Foundation.h>
#import <sys/stat.h>
#import <notify.h>

#if !TARGET_OS_BRIDGE
#import <MobileAsset/MAAsset.h>
#import <MobileAsset/MAAssetQuery.h>
#endif

#if TARGET_OS_OSX
#import <MobileAsset/MobileAsset.h>
#endif

#import <Security/SecInternalReleasePriv.h>

#import <securityd/OTATrustUtilities.h>
#import <securityd/SecPinningDb.h>

#include "utilities/debugging.h"
#include "utilities/sqlutils.h"
#include "utilities/iOSforOSX.h"
#include <utilities/SecCFError.h>
#include <utilities/SecCFRelease.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/SecDb.h>
#include <utilities/SecFileLocations.h>
#include "utilities/sec_action.h"

#define kSecPinningBasePath         "/Library/Keychains/"
#define kSecPinningDbFileName       "pinningrules.sqlite3"

const uint64_t PinningDbSchemaVersion = 2;
const NSString *PinningDbPolicyNameKey = @"policyName"; /* key for a string value */
const NSString *PinningDbDomainsKey = @"domains"; /* key for an array of dictionaries */
const NSString *PinningDbPoliciesKey = @"rules"; /* key for an array of dictionaries */
const NSString *PinningDbDomainSuffixKey = @"suffix"; /* key for a string */
const NSString *PinningDbLabelRegexKey = @"labelRegex"; /* key for a regex string */

const CFStringRef kSecPinningDbKeyHostname = CFSTR("PinningHostname");
const CFStringRef kSecPinningDbKeyPolicyName = CFSTR("PinningPolicyName");
const CFStringRef kSecPinningDbKeyRules = CFSTR("PinningRules");

#if !TARGET_OS_BRIDGE
const NSString *PinningDbMobileAssetType = @"com.apple.MobileAsset.CertificatePinning";
#define kSecPinningDbMobileAssetNotification "com.apple.MobileAsset.CertificatePinning.cached-metadata-updated"
#endif

#if TARGET_OS_OSX
const NSUInteger PinningDbMobileAssetCompatibilityVersion = 1;
#endif

@interface SecPinningDb : NSObject
@property (assign) SecDbRef db;
@property dispatch_queue_t queue;
@property NSURL *dbPath;
- (instancetype) init;
- ( NSDictionary * _Nullable ) queryForDomain:(NSString *)domain;
- ( NSDictionary * _Nullable ) queryForPolicyName:(NSString *)policyName;
@end

static bool isDbOwner() {
#ifdef NO_SERVER
    // Test app running as securityd
#elif TARGET_OS_IPHONE
    if (getuid() == 64) // _securityd
#else
    if (getuid() == 0)
#endif
        {
            return true;
        }
    return false;
}

static inline bool isNSNumber(id nsType) {
    return nsType && [nsType isKindOfClass:[NSNumber class]];
}

static inline bool isNSArray(id nsType) {
    return nsType && [nsType isKindOfClass:[NSArray class]];
}

static inline bool isNSDictionary(id nsType) {
    return nsType && [nsType isKindOfClass:[NSDictionary class]];
}

@implementation SecPinningDb
#define getSchemaVersionSQL CFSTR("PRAGMA user_version")
#define selectVersionSQL CFSTR("SELECT ival FROM admin WHERE key='version'")
#define insertAdminSQL CFSTR("INSERT OR REPLACE INTO admin (key,ival,value) VALUES (?,?,?)")
#define selectDomainSQL CFSTR("SELECT DISTINCT labelRegex,policyName,policies FROM rules WHERE domainSuffix=?")
#define selectPolicyNameSQL CFSTR("SELECT DISTINCT policies FROM rules WHERE policyName=?")
#define insertRuleSQL CFSTR("INSERT OR REPLACE INTO rules (policyName,domainSuffix,labelRegex,policies) VALUES (?,?,?,?) ")
#define removeAllRulesSQL CFSTR("DELETE FROM rules;")

- (NSNumber *)getSchemaVersion:(SecDbConnectionRef)dbconn error:(CFErrorRef *)error {
    __block bool ok = true;
    __block NSNumber *version = nil;
    ok &= SecDbWithSQL(dbconn, getSchemaVersionSQL, error, ^bool(sqlite3_stmt *selectVersion) {
        ok &= SecDbStep(dbconn, selectVersion, error, ^(bool *stop) {
            int ival = sqlite3_column_int(selectVersion, 0);
            version = [NSNumber numberWithInt:ival];
        });
        return ok;
    });
    return version;
}

- (BOOL)setSchemaVersion:(SecDbConnectionRef)dbconn error:(CFErrorRef *)error {
    bool ok = true;
    NSString *setVersion = [NSString stringWithFormat:@"PRAGMA user_version = %llu", PinningDbSchemaVersion];
    ok &= SecDbExec(dbconn,
                    (__bridge CFStringRef)setVersion,
                    error);
    if (!ok) {
        secerror("SecPinningDb: failed to create admin table: %@", error ? *error : nil);
    }
    return ok;
}

- (NSNumber *)getContentVersion:(SecDbConnectionRef)dbconn error:(CFErrorRef *)error {
    __block bool ok = true;
    __block NSNumber *version = nil;
    ok &= SecDbWithSQL(dbconn, selectVersionSQL, error, ^bool(sqlite3_stmt *selectVersion) {
        ok &= SecDbStep(dbconn, selectVersion, error, ^(bool *stop) {
            uint64_t ival = sqlite3_column_int64(selectVersion, 0);
            version = [NSNumber numberWithUnsignedLongLong:ival];
        });
        return ok;
    });
    return version;
}

- (BOOL)setContentVersion:(NSNumber *)version dbConnection:(SecDbConnectionRef)dbconn error:(CFErrorRef *)error {
    __block BOOL ok = true;
    ok &= SecDbWithSQL(dbconn, insertAdminSQL, error, ^bool(sqlite3_stmt *insertAdmin) {
        const char *versionKey = "version";
        ok &= SecDbBindText(insertAdmin, 1, versionKey, strlen(versionKey), SQLITE_TRANSIENT, error);
        ok &= SecDbBindInt64(insertAdmin, 2, [version unsignedLongLongValue], error);
        ok &= SecDbStep(dbconn, insertAdmin, error, NULL);
        return ok;
    });
    if (!ok) {
        secerror("SecPinningDb: failed to set version %@ from pinning list: %@", version, error ? *error : nil);
    }
    return ok;
}

- (BOOL) shouldUpdateContent:(NSNumber *)new_version {
    __block CFErrorRef error = NULL;
    __block BOOL ok = YES;
    __block BOOL newer = NO;
    ok &= SecDbPerformRead(_db, &error, ^(SecDbConnectionRef dbconn) {
        NSNumber *db_version = [self getContentVersion:dbconn error:&error];
        if (!db_version || [new_version compare:db_version] == NSOrderedDescending) {
            newer = YES;
            secnotice("pinningDb", "Pinning database should update from version %@ to version %@", db_version, new_version);
        }
    });

    if (!ok || error) {
        secerror("SecPinningDb: error reading content version from database %@", error);
    }
    CFReleaseNull(error);
    return newer;
}

- (BOOL) insertRuleWithName:(NSString *)policyName
               domainSuffix:(NSString *)domainSuffix
                 labelRegex:(NSString *)labelRegex
                   policies:(NSArray *)policies
               dbConnection:(SecDbConnectionRef)dbconn
                      error:(CFErrorRef *)error{
    /* @@@ This insertion mechanism assumes that the input is trusted -- namely, that the new rules
     * are allowed to replace existing rules. For third-party inputs, this assumption isn't true. */

    secdebug("pinningDb", "inserting new rule: %@ for %@.%@", policyName, labelRegex, domainSuffix);

    __block bool ok = true;
    ok &= SecDbWithSQL(dbconn, insertRuleSQL, error, ^bool(sqlite3_stmt *insertRule) {
        ok &= SecDbBindText(insertRule, 1, [policyName UTF8String], [policyName length], SQLITE_TRANSIENT, error);
        ok &= SecDbBindText(insertRule, 2, [domainSuffix UTF8String], [domainSuffix length], SQLITE_TRANSIENT, error);
        ok &= SecDbBindText(insertRule, 3, [labelRegex UTF8String], [labelRegex length], SQLITE_TRANSIENT, error);
        NSData *xmlPolicies = [NSPropertyListSerialization dataWithPropertyList:policies
                                                                         format:NSPropertyListXMLFormat_v1_0
                                                                        options:0
                                                                          error:nil];
        if (!xmlPolicies) {
            secerror("SecPinningDb: failed to serialize policies");
            ok = false;
        }
        ok &= SecDbBindBlob(insertRule, 4, [xmlPolicies bytes], [xmlPolicies length], SQLITE_TRANSIENT, error);
        ok &= SecDbStep(dbconn, insertRule, error, NULL);
        return ok;
    });
    if (!ok) {
        secerror("SecPinningDb: failed to insert rule %@ for %@.%@ with error %@", policyName, labelRegex, domainSuffix, error ? *error : nil);
    }
    return ok;
}

- (BOOL) populateDbFromBundle:(NSArray *)pinningList dbConnection:(SecDbConnectionRef)dbconn error:(CFErrorRef *)error {
    __block BOOL ok = true;
    [pinningList enumerateObjectsUsingBlock:^(id  _Nonnull obj, NSUInteger idx, BOOL * _Nonnull stop) {
        if (idx ==0) { return; } // Skip the first value which is the version
        if (!isNSDictionary(obj)) {
            secerror("SecPinningDb: rule entry in pinning plist is wrong class");
            ok = false;
            return;
        }
        NSDictionary *rule = obj;
        __block NSString *policyName = [rule objectForKey:PinningDbPolicyNameKey];
        NSArray *domains = [rule objectForKey:PinningDbDomainsKey];
        __block NSArray *policies = [rule objectForKey:PinningDbPoliciesKey];

        if (!policyName || !domains || !policies) {
            secerror("SecPinningDb: failed to get required fields from rule entry %lu", (unsigned long)idx);
            ok = false;
            return;
        }

        [domains enumerateObjectsUsingBlock:^(id  _Nonnull obj, NSUInteger idx, BOOL * _Nonnull stop) {
            if (!isNSDictionary(obj)) {
                secerror("SecPinningDb: domain entry %lu for %@ in pinning rule is wrong class", (unsigned long)idx, policyName);
                ok = false;
                return;
            }
            NSDictionary *domain = obj;
            NSString *suffix = [domain objectForKey:PinningDbDomainSuffixKey];
            NSString *labelRegex = [domain objectForKey:PinningDbLabelRegexKey];

            if (!suffix || !labelRegex) {
                secerror("SecPinningDb: failed to get required fields for entry %lu for %@", (unsigned long)idx, policyName);
                ok = false;
                return;
            }
            ok &= [self insertRuleWithName:policyName domainSuffix:suffix labelRegex:labelRegex policies:policies
                              dbConnection:dbconn error:error];
        }];
    }];
    if (!ok) {
        secerror("SecPinningDb: failed to populate DB from pinning list: %@", error ? *error : nil);
    }
    return ok;
}

- (BOOL) removeAllRulesFromDb:(SecDbConnectionRef)dbconn error:(CFErrorRef *)error {
    __block BOOL ok = true;
    ok &= SecDbWithSQL(dbconn, removeAllRulesSQL, error, ^bool(sqlite3_stmt *deleteRules) {
        ok &= SecDbStep(dbconn, deleteRules, error, NULL);
        return ok;
    });
    if (!ok) {
        secerror("SecPinningDb: failed to delete old values: %@", error ? *error :nil);
    }
    return ok;
}


- (BOOL) createOrAlterAdminTable:(SecDbConnectionRef)dbconn error:(CFErrorRef *)error {
    bool ok = true;
    ok &= SecDbExec(dbconn,
                    CFSTR("CREATE TABLE IF NOT EXISTS admin("
                          "key TEXT PRIMARY KEY NOT NULL,"
                          "ival INTEGER NOT NULL,"
                          "value BLOB"
                          ");"),
                    error);
    if (!ok) {
        secerror("SecPinningDb: failed to create admin table: %@", error ? *error : nil);
    }
    return ok;
}

- (BOOL) createOrAlterRulesTable:(SecDbConnectionRef)dbconn error:(CFErrorRef *)error {
    bool ok = true;
    ok &= SecDbExec(dbconn,
                    CFSTR("CREATE TABLE IF NOT EXISTS rules("
                          "policyName TEXT NOT NULL,"
                          "domainSuffix TEXT NOT NULL,"
                          "labelRegex TEXT NOT NULL,"
                          "policies BLOB NOT NULL,"
                          "UNIQUE(policyName, domainSuffix, labelRegex)"
                          ");"),
                    error);
    ok &= SecDbExec(dbconn, CFSTR("CREATE INDEX IF NOT EXISTS idomain ON rules(domainSuffix);"), error);
    ok &= SecDbExec(dbconn, CFSTR("CREATE INDEX IF NOT EXISTS ipolicy ON rules(policyName);"), error);
    if (!ok) {
        secerror("SecPinningDb: failed to create rules table: %@", error ? *error : nil);
    }
    return ok;
}

#if !TARGET_OS_BRIDGE
- (BOOL) installDbFromURL:(NSURL *)localURL {
    if (!localURL) {
        secerror("SecPinningDb: missing url for downloaded asset");
        return NO;
    }
    NSURL* basePath = nil, *fileLoc = nil;
    if (![localURL scheme]) {
        /* MobileAsset provides the URL without the scheme. Fix it up. */
        NSString *pathWithScheme = [[NSString alloc] initWithFormat:@"%@",localURL];
        basePath = [NSURL fileURLWithPath:pathWithScheme isDirectory:YES];
    } else {
        basePath = localURL;
    }
    fileLoc = [NSURL URLWithString:@"CertificatePinning.plist"
                       relativeToURL:basePath];
    __block NSArray *pinningList = [NSArray arrayWithContentsOfURL:fileLoc];
    if (!pinningList) {
        secerror("SecPinningDb: unable to create pinning list from asset file: %@", fileLoc);
        return NO;
    }

    NSNumber *plist_version = [pinningList objectAtIndex:0];
    if (![self shouldUpdateContent:plist_version]) {
        /* We got a new plist but we already have that version installed. */
        return YES;
    }

    /* Update Content */
    __block CFErrorRef error = NULL;
    __block BOOL ok = YES;
    ok &= SecDbPerformWrite(_db, &error, ^(SecDbConnectionRef dbconn) {
        ok &= [self updateDb:dbconn error:&error pinningList:pinningList updateSchema:NO updateContent:YES];
    });

    if (error) {
        secerror("SecPinningDb: error installing updated pinning list version %@: %@", [pinningList objectAtIndex:0], error);
        CFReleaseNull(error);
    }

    return ok;
}

#if TARGET_OS_OSX
const CFStringRef kSecSUPrefDomain = CFSTR("com.apple.SoftwareUpdate");
const CFStringRef kSecSUScanPrefConfigDataInstallKey = CFSTR("ConfigDataInstall");
#endif

static BOOL PinningDbCanCheckMobileAsset(void) {
    BOOL result = YES;
#if TARGET_OS_OSX
    /* Check the user's SU preferences to determine if "Install system data files" is off */
    if (!CFPreferencesSynchronize(kSecSUPrefDomain, kCFPreferencesAnyUser, kCFPreferencesCurrentHost)) {
        secerror("SecPinningDb: unable to synchronize SoftwareUpdate prefs");
        return NO;
    }

    id value = nil;
    if (CFPreferencesAppValueIsForced(kSecSUScanPrefConfigDataInstallKey, kSecSUPrefDomain)) {
        value = CFBridgingRelease(CFPreferencesCopyAppValue(kSecSUScanPrefConfigDataInstallKey, kSecSUPrefDomain));
    } else {
        value = CFBridgingRelease(CFPreferencesCopyValue(kSecSUScanPrefConfigDataInstallKey, kSecSUPrefDomain,
                                                         kCFPreferencesAnyUser, kCFPreferencesCurrentHost));
    }
    if (isNSNumber(value)) {
        result = [value boolValue];
    }

    if (!result) { secnotice("pinningDb", "User has disabled system data installation."); }

    /* MobileAsset.framework isn't mastered into the BaseSystem. Check that the MA classes are linked. */
    if (![ASAssetQuery class] || ![ASAsset class] || ![MAAssetQuery class] || ![MAAsset class]) {
        secnotice("PinningDb", "Weak linked MobileAsset framework missing.");
        result = NO;
    }
#endif
    return result;
}

#if TARGET_OS_IPHONE
- (void) downloadPinningAsset:(BOOL __unused)isLocalOnly {
    if (!PinningDbCanCheckMobileAsset()) {
        secnotice("pinningDb", "MobileAsset disabled, skipping check.");
        return;
    }

    secnotice("pinningDb", "begin MobileAsset query for catalog");
    [MAAsset startCatalogDownload:(NSString *)PinningDbMobileAssetType then:^(MADownLoadResult result) {
        if (result != MADownloadSucceesful) {
            secerror("SecPinningDb: failed to download catalog: %ld", (long)result);
            return;
        }
        MAAssetQuery *query = [[MAAssetQuery alloc] initWithType:(NSString *)PinningDbMobileAssetType];
        [query augmentResultsWithState:true];

        secnotice("pinningDb", "begin MobileAsset metadata sync request");
        MAQueryResult queryResult = [query queryMetaDataSync];
        if (queryResult != MAQuerySucceesful) {
            secerror("SecPinningDb: failed to query MobileAsset metadata: %ld", (long)queryResult);
            return;
        }

        if (!query.results) {
            secerror("SecPinningDb: no results in MobileAsset query");
            return;
        }

        for (MAAsset *asset in query.results) {
            NSNumber *asset_version = [asset assetProperty:@"_ContentVersion"];
            if (![self shouldUpdateContent:asset_version]) {
                secdebug("pinningDb", "skipping asset because we already have _ContentVersion %@", asset_version);
                continue;
            }
            switch(asset.state) {
                default:
                    secerror("SecPinningDb: unknown asset state %ld", (long)asset.state);
                    continue;
                case MAInstalled:
                    /* The asset is already in the cache, get it from disk. */
                    secdebug("pinningDb", "CertificatePinning asset already installed");
                    if([self installDbFromURL:[asset getLocalUrl]]) {
                        secnotice("pinningDb", "finished db update from installed asset. purging asset.");
                        [asset purge:^(MAPurgeResult purge_result) {
                            if (purge_result != MAPurgeSucceeded) {
                                secerror("SecPinningDb: purge failed: %ld", (long)purge_result);
                            }
                        }];
                    }
                    break;
                case MAUnknown:
                    secerror("SecPinningDb: pinning asset is unknown");
                    continue;
                case MADownloading:
                    secnotice("pinningDb", "pinning asset is downloading");
                    /* fall through */
                case MANotPresent:
                    secnotice("pinningDb", "begin download of CertificatePinning asset");
                    [asset startDownload:^(MADownLoadResult downloadResult) {
                        if (downloadResult != MADownloadSucceesful) {
                            secerror("SecPinningDb: failed to download pinning asset: %ld", (long)downloadResult);
                            return;
                        }
                        if([self installDbFromURL:[asset getLocalUrl]]) {
                            secnotice("pinningDb", "finished db update from installed asset. purging asset.");
                            [asset purge:^(MAPurgeResult purge_result) {
                                if (purge_result != MAPurgeSucceeded) {
                                    secerror("SecPinningDb: purge failed: %ld", (long)purge_result);
                                }
                            }];
                        }
                    }];
                    break;
            }
        }
    }];
}
#else /* !TARGET_OS_IPHONE */
/* <rdar://problem/30879827> MobileAssetV2 fails on macOS, so use V1 */
- (void) downloadPinningAsset:(BOOL)isLocalOnly {
    if (!PinningDbCanCheckMobileAsset()) {
        secnotice("pinningDb", "MobileAsset disabled, skipping check.");
        return;
    }

    ASAssetQuery *query = [[ASAssetQuery alloc] initWithAssetType:(NSString *)PinningDbMobileAssetType];
    [query setQueriesLocalAssetInformationOnly:isLocalOnly]; // Omitting this leads to a notifcation loop.
    NSError *error = nil;
    NSArray<ASAsset *>*query_results = [query runQueryAndReturnError:&error];
    if (!query_results) {
        secerror("SecPinningDb: asset query failed: %@", error);
        return;
    }

    for (ASAsset *asset in query_results) {
        NSDictionary *attributes = [asset attributes];

        NSNumber *compatibilityVersion = [attributes objectForKey:ASAttributeCompatibilityVersion];
        if (!isNSNumber(compatibilityVersion) ||
            [compatibilityVersion unsignedIntegerValue] != PinningDbMobileAssetCompatibilityVersion) {
            secnotice("pinningDb", "Skipping asset with compatibility version %@", compatibilityVersion);
            continue;
        }

        NSNumber *contentVersion = [attributes objectForKey:ASAttributeContentVersion];
        if (!isNSNumber(contentVersion) || ![self shouldUpdateContent:contentVersion]) {
            secnotice("pinningDb", "Skipping asset with content version %@", contentVersion);
            continue;
        }

        ASProgressHandler pinningHandler = ^(NSDictionary *state, NSError *progressError){
            if (progressError) {
                secerror("SecPinningDb: asset download error: %@", progressError);
                return;
            }

            if (!state) {
                secerror("SecPinningDb: no asset state in progress handler");
                return;
            }

            NSString *operationState = [state objectForKey:ASStateOperation];
            secdebug("pinningDb", "Asset state is %@", operationState);

            if (operationState && [operationState isEqualToString:ASOperationCompleted]) {
                if ([self installDbFromURL:[asset localURL]]) {
                    secnotice("pinningDb", "finished db update from installed asset. purging asset.");
                    [asset purge:^(NSError *error) {
                        if (error) {
                            secerror("SecPinningDb: purge failed %@", error);
                        }
                    }];
                }
            }
        };

        switch ([asset state]) {
            case ASAssetStateNotPresent:
                secdebug("pinningDb", "CertificatePinning asset needs to be downloaded");
                asset.progressHandler= pinningHandler;
                asset.userInitiatedDownload = YES;
                [asset beginDownloadWithOptions:@{ASDownloadOptionPriority : ASDownloadPriorityNormal}];
                break;
            case ASAssetStateInstalled:
                /* The asset is already in the cache, get it from disk. */
                secdebug("pinningDb", "CertificatePinning asset already installed");
                if([self installDbFromURL:[asset localURL]]) {
                    secnotice("pinningDb", "finished db update from installed asset. purging asset.");
                    [asset purge:^(NSError *error) {
                        if (error) {
                            secerror("SecPinningDb: purge failed %@", error);
                        }
                    }];
                }
                break;
            case ASAssetStatePaused:
                secdebug("pinningDb", "CertificatePinning asset download paused");
                asset.progressHandler = pinningHandler;
                asset.userInitiatedDownload = YES;
                if (![asset resumeDownloadAndReturnError:&error]) {
                    secerror("SecPinningDb: failed to resume download of asset: %@", error);
                }
                break;
            case ASAssetStateDownloading:
                secdebug("pinningDb", "CertificatePinning asset downloading");
                asset.progressHandler = pinningHandler;
                asset.userInitiatedDownload = YES;
                break;
            default:
                secerror("SecPinningDb: unhandled asset state %ld", (long)asset.state);
                continue;
        }
    }
}
#endif /* !TARGET_OS_IPHONE */

- (void) downloadPinningAsset {
    [self downloadPinningAsset:NO];
}
#endif /* !TARGET_OS_BRIDGE */

- (NSArray *) copyCurrentPinningList {
    NSArray *pinningList = nil;
    /* Get the pinning list shipped with the OS */
    SecOTAPKIRef otapkiref = SecOTAPKICopyCurrentOTAPKIRef();
    if (otapkiref) {
        pinningList = CFBridgingRelease(SecOTAPKICopyPinningList(otapkiref));
        CFReleaseNull(otapkiref);
        if (!pinningList) {
            secerror("SecPinningDb: failed to read pinning plist from bundle");
        }
    }

#if !TARGET_OS_BRIDGE
    /* Asynchronously ask MobileAsset for most recent pinning list. */
    dispatch_async(_queue, ^{
        secnotice("pinningDb", "Initial check with MobileAsset for newer pinning asset");
        [self downloadPinningAsset];
    });

    /* Register for changes in our asset */
    if (PinningDbCanCheckMobileAsset()) {
        int out_token = 0;
        notify_register_dispatch(kSecPinningDbMobileAssetNotification, &out_token, self->_queue, ^(int __unused token) {
            secnotice("pinningDb", "Got a notification about a new pinning asset.");
            [self downloadPinningAsset:YES];
        });
    }
#endif

    return pinningList;
}

- (BOOL) updateDb:(SecDbConnectionRef)dbconn error:(CFErrorRef *)error pinningList:(NSArray *)pinningList
     updateSchema:(BOOL)updateSchema updateContent:(BOOL)updateContent
{
    if (!isDbOwner()) { return false; }
    secdebug("pinningDb", "updating or creating database");

    __block bool ok = true;
    ok &= SecDbTransaction(dbconn, kSecDbExclusiveTransactionType, error, ^(bool *commit) {
        if (updateSchema) {
            /* update the tables */
            ok &= [self createOrAlterAdminTable:dbconn error:error];
            ok &= [self createOrAlterRulesTable:dbconn error:error];
            ok &= [self setSchemaVersion:dbconn error:error];
        }

        if (updateContent) {
            /* remove the old data */
            /* @@@ This behavior assumes that we have all the rules we want to populate
             * elsewhere on disk and that the DB doesn't contain the sole copy of that data. */
            ok &= [self removeAllRulesFromDb:dbconn error:error];

            /* read the new data */
            NSNumber *version = [pinningList objectAtIndex:0];

            /* populate the tables */
            ok &= [self populateDbFromBundle:pinningList dbConnection:dbconn error:error];
            ok &= [self setContentVersion:version dbConnection:dbconn error:error];
        }

        *commit = ok;
    });

    return ok;
}

- (SecDbRef) createAtPath {
    bool readWrite = isDbOwner();
    mode_t mode = 0644;

    CFStringRef path = CFStringCreateWithCString(NULL, [_dbPath fileSystemRepresentation], kCFStringEncodingUTF8);
    SecDbRef result = SecDbCreateWithOptions(path, mode, readWrite, false, false,
         ^bool (SecDbRef db, SecDbConnectionRef dbconn, bool didCreate, bool *callMeAgainForNextConnection, CFErrorRef *error) {
             if (!isDbOwner()) {
                 /* Non-owner process can't update the db, but it should get a db connection.
                  * @@@ Revisit if new schema version is needed by reader processes. */
                 return true;
             }

             __block BOOL ok = true;
             dispatch_sync(self->_queue, ^{
                 bool updateSchema = false;
                 bool updateContent = false;

                 /* Get the pinning plist */
                 NSArray *pinningList = [self copyCurrentPinningList];
                 if (!pinningList) {
                     secerror("SecPinningDb: failed to find pinning plist in bundle");
                     ok = false;
                     return;
                 }

                 /* Check latest data and schema versions against existing table. */
                 if (!isNSNumber([pinningList objectAtIndex:0])) {
                     secerror("SecPinningDb: pinning plist in wrong format");
                     return; // Don't change status. We can continue to use old DB.
                 }
                 NSNumber *plist_version = [pinningList objectAtIndex:0];
                 NSNumber *db_version = [self getContentVersion:dbconn error:error];
                 if (!db_version || [plist_version compare:db_version] == NSOrderedDescending) {
                     secnotice("pinningDb", "Updating pinning database content from version %@ to version %@",
                               db_version ? db_version : 0, plist_version);
                     updateContent = true;
                 }
                 NSNumber *schema_version = [self getSchemaVersion:dbconn error:error];
                 NSNumber *current_version = [NSNumber numberWithUnsignedLongLong:PinningDbSchemaVersion];
                 if (!schema_version || ![schema_version isEqualToNumber:current_version]) {
                     secnotice("pinningDb", "Updating pinning database schema from version %@ to version %@",
                               schema_version, current_version);
                     updateSchema = true;
                 }

                 if (updateContent || updateSchema) {
                     ok &= [self updateDb:dbconn error:error pinningList:pinningList updateSchema:updateSchema updateContent:updateContent];
                 }
                 if (!ok) {
                     secerror("SecPinningDb: %s failed: %@", didCreate ? "Create" : "Open", error ? *error : NULL);
                 }
             });
             return ok;
         });

    CFReleaseNull(path);
    return result;
}

static void verify_create_path(const char *path)
{
    int ret = mkpath_np(path, 0755);
    if (!(ret == 0 || ret ==  EEXIST)) {
        secerror("could not create path: %s (%s)", path, strerror(ret));
    }
}

- (NSURL *)pinningDbPath {
    /* Make sure the /Library/Keychains directory is there */
#if TARGET_OS_IPHONE
    NSURL *directory = CFBridgingRelease(SecCopyURLForFileInKeychainDirectory(nil));
#else
    NSURL *directory = [NSURL fileURLWithFileSystemRepresentation:"/Library/Keychains/" isDirectory:YES relativeToURL:nil];
#endif
    verify_create_path([directory fileSystemRepresentation]);

    /* Get the full path of the pinning DB */
    return [directory URLByAppendingPathComponent:@"pinningrules.sqlite3"];
}

- (void) initializedDb {
    dispatch_sync(_queue, ^{
        if (!self->_db) {
            self->_dbPath = [self pinningDbPath];
            self->_db = [self createAtPath];
        }
    });
}

- (instancetype) init {
    if (self = [super init]) {
        _queue = dispatch_queue_create("Pinning DB Queue", DISPATCH_QUEUE_SERIAL_WITH_AUTORELEASE_POOL);
        [self initializedDb];
    }
    return self;
}

- (void) dealloc {
    CFReleaseNull(_db);
}

- (BOOL) isPinningDisabled:(NSString * _Nullable)policy {
    static dispatch_once_t once;
    static sec_action_t action;

    BOOL pinningDisabled = NO;
    if (SecIsInternalRelease()) {
        NSUserDefaults *defaults = [[NSUserDefaults alloc] initWithSuiteName:@"com.apple.security"];
        pinningDisabled = [defaults boolForKey:@"AppleServerAuthenticationNoPinning"];
        if (!pinningDisabled && policy) {
            NSMutableString *policySpecificKey = [NSMutableString stringWithString:@"AppleServerAuthenticationNoPinning"];
            [policySpecificKey appendString:policy];
            pinningDisabled = [defaults boolForKey:policySpecificKey];
            secinfo("pinningQA", "%@ disable pinning = %d", policy, pinningDisabled);
        }
    }

    dispatch_once(&once, ^{
        /* Only log system-wide pinning status once a minute */
        action = sec_action_create("pinning logging charles", 60.0);
        sec_action_set_handler(action, ^{
            if (!SecIsInternalRelease()) {
                secnotice("pinningQA", "could not disable pinning: not an internal release");
            } else {
                NSUserDefaults *defaults = [[NSUserDefaults alloc] initWithSuiteName:@"com.apple.security"];
                secnotice("pinningQA", "generic pinning disable = %d", [defaults boolForKey:@"AppleServerAuthenticationNoPinning"]);
            }
        });
    });
    sec_action_perform(action);

    return pinningDisabled;
}

- ( NSDictionary * _Nullable ) queryForDomain:(NSString *)domain {
    if (!_queue) { (void)[self init]; }
    if (!_db) { [self initializedDb]; }

    /* Check for general no-pinning setting */
    if ([self isPinningDisabled:nil]) {
        return nil;
    }

    /* parse the domain into suffix and 1st label */
    NSRange firstDot = [domain rangeOfString:@"."];
    if (firstDot.location == NSNotFound) { return nil; } // Probably not a legitimate domain name
    __block NSString *firstLabel = [domain substringToIndex:firstDot.location];
    __block NSString *suffix = [domain substringFromIndex:(firstDot.location + 1)];

    /* Perform SELECT */
    __block bool ok = true;
    __block CFErrorRef error = NULL;
    __block NSMutableArray *resultRules = [NSMutableArray array];
    __block NSString *resultName = nil;
    ok &= SecDbPerformRead(_db, &error, ^(SecDbConnectionRef dbconn) {
        ok &= SecDbWithSQL(dbconn, selectDomainSQL, &error, ^bool(sqlite3_stmt *selectDomain) {
            ok &= SecDbBindText(selectDomain, 1, [suffix UTF8String], [suffix length], SQLITE_TRANSIENT, &error);
            ok &= SecDbStep(dbconn, selectDomain, &error, ^(bool *stop) {
                /* Match the labelRegex */
                const uint8_t *regex = sqlite3_column_text(selectDomain, 0);
                if (!regex) { return; }
                NSString *regexStr = [NSString stringWithUTF8String:(const char *)regex];
                if (!regexStr) { return; }
                NSRegularExpression *regularExpression = [NSRegularExpression regularExpressionWithPattern:regexStr
                                                                                                  options:NSRegularExpressionCaseInsensitive
                                                                                                    error:nil];
                if (!regularExpression) { return; }
                NSUInteger numMatches = [regularExpression numberOfMatchesInString:firstLabel
                                                                           options:0
                                                                             range:NSMakeRange(0, [firstLabel length])];
                if (numMatches == 0) {
                    return;
                }
                secdebug("SecPinningDb", "found matching rule for %@.%@", firstLabel, suffix);

                /* Check the policyName for no-pinning settings */
                const uint8_t *policyName = sqlite3_column_text(selectDomain, 1);
                NSString *policyNameStr = [NSString stringWithUTF8String:(const char *)policyName];
                if ([self isPinningDisabled:policyNameStr]) {
                    return;
                }

                /* Deserialize the policies and return.
                 * @@@ Assumes there is only one rule with matching suffix/label pairs. */
                NSData *xmlPolicies = [NSData dataWithBytes:sqlite3_column_blob(selectDomain, 2) length:sqlite3_column_bytes(selectDomain, 2)];
                if (!xmlPolicies) { return; }
                id policies = [NSPropertyListSerialization propertyListWithData:xmlPolicies options:0 format:nil error:nil];
                if (!isNSArray(policies)) {
                    return;
                }
                [resultRules addObjectsFromArray:(NSArray *)policies];
                resultName = policyNameStr;
            });
            return ok;
        });
    });

    if (error) {
        secerror("SecPinningDb: error querying DB for hostname: %@", error);
        CFReleaseNull(error);
    }

    if ([resultRules count] > 0) {
        NSDictionary *results = @{(__bridge NSString*)kSecPinningDbKeyRules:resultRules,
                                  (__bridge NSString*)kSecPinningDbKeyPolicyName:resultName};
        return results;
    }
    return nil;
}

- (NSDictionary * _Nullable) queryForPolicyName:(NSString *)policyName {
    if (!_queue) { (void)[self init]; }
    if (!_db) { [self initializedDb]; }

    /* Skip the "sslServer" policyName, which is not a pinning policy */
    if ([policyName isEqualToString:@"sslServer"]) {
        return nil;
    }

    /* Check for general no-pinning setting */
    if ([self isPinningDisabled:nil] || [self isPinningDisabled:policyName]) {
        return nil;
    }

    /* Perform SELECT */
    __block bool ok = true;
    __block CFErrorRef error = NULL;
    __block NSMutableArray *resultRules = [NSMutableArray array];
    ok &= SecDbPerformRead(_db, &error, ^(SecDbConnectionRef dbconn) {
        ok &= SecDbWithSQL(dbconn, selectPolicyNameSQL, &error, ^bool(sqlite3_stmt *selectPolicyName) {
            ok &= SecDbBindText(selectPolicyName, 1, [policyName UTF8String], [policyName length], SQLITE_TRANSIENT, &error);
            ok &= SecDbStep(dbconn, selectPolicyName, &error, ^(bool *stop) {
                secdebug("SecPinningDb", "found matching rule for %@ policy", policyName);

                /* Deserialize the policies and return */
                NSData *xmlPolicies = [NSData dataWithBytes:sqlite3_column_blob(selectPolicyName, 0) length:sqlite3_column_bytes(selectPolicyName, 0)];
                if (!xmlPolicies) { return; }
                id policies = [NSPropertyListSerialization propertyListWithData:xmlPolicies options:0 format:nil error:nil];
                if (!isNSArray(policies)) {
                    return;
                }
                [resultRules addObjectsFromArray:(NSArray *)policies];
            });
            return ok;
        });
    });

    if (error) {
        secerror("SecPinningDb: error querying DB for policyName: %@", error);
        CFReleaseNull(error);
    }

    if ([resultRules count] > 0) {
        NSDictionary *results = @{(__bridge NSString*)kSecPinningDbKeyRules:resultRules,
                                  (__bridge NSString*)kSecPinningDbKeyPolicyName:policyName};
        return results;
    }
    return nil;
}

@end

static SecPinningDb *pinningDb = nil;
void SecPinningDbInitialize(void) {
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        pinningDb = [[SecPinningDb alloc] init];
        __block CFErrorRef error = NULL;
        BOOL ok = SecDbPerformRead([pinningDb db], &error, ^(SecDbConnectionRef dbconn) {
            NSNumber *contentVersion = [pinningDb getContentVersion:dbconn error:&error];
            NSNumber *schemaVersion = [pinningDb getSchemaVersion:dbconn error:&error];
            secinfo("pinningDb", "Database Schema: %@ Content: %@", schemaVersion, contentVersion);
        });
        if (!ok || error) {
            secerror("SecPinningDb: unable to initialize db: %@", error);
        }
        CFReleaseNull(error);
    });
}

CFDictionaryRef _Nullable SecPinningDbCopyMatching(CFDictionaryRef query) {
    SecPinningDbInitialize();

    NSDictionary *nsQuery = (__bridge NSDictionary*)query;
    NSString *hostname = [nsQuery objectForKey:(__bridge NSString*)kSecPinningDbKeyHostname];

    NSDictionary *results = [pinningDb queryForDomain:hostname];
    if (results) { return CFBridgingRetain(results); }
    NSString *policyName = [nsQuery objectForKey:(__bridge NSString*)kSecPinningDbKeyPolicyName];
    results = [pinningDb queryForPolicyName:policyName];
    if (!results) { return nil; }
    return CFBridgingRetain(results);
}
