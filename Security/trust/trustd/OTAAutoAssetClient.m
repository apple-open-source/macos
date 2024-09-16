/*
 * Copyright (c) 2023-2024 Apple Inc. All Rights Reserved.
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
 * OTAAutoAssetClient.m
 */

#import "OTAAutoAssetClient.h"
#import "trust/trustd/SecTrustLoggingServer.h"
#import "trust/trustd/trustdFileLocations.h"
#import "trust/trustd/trustdVariants.h"
#import "trust/trustd/OTATrustUtilities.h"
#import <utilities/SecCFWrappers.h>
#import <notify.h>

#if !TARGET_OS_BRIDGE
#import <MobileAsset/MobileAsset.h>
#import <MobileAsset/MAAutoAssetSelector.h>
#endif

static NSString * const OTAAutoAssetAssetType = @"com.apple.MobileAsset.PKITrustStore";
static NSString * const OTAAutoAssetAssetSpecifier = @"PKITrustStore";
static NSString * const OTAAutoAssetClientName = @"trustd";
static NSString * const OTAAutoAssetClientContentInterestReason = @"system trusted certificates";
static NSString * const OTAAutoAssetNotificationQueueName = @"com.apple.trustd.notifyQueue";
static NSString * const OTAAutoAssetSettingsPlist = @"AutoAsset.plist";
static NSString * const OTAAutoAssetPathKey = @"AssetPath";

static NSString * const OTAAssetsV2PathPrefix = @"/private/var/MobileAsset/AssetsV2/";
static NSString * const OTAMacOSAssetsV2PathPrefix = @"/System/Library/AssetsV2/";
static NSString * const OTACryptexesPathPrefix = @"/System/Cryptexes/OS/";

@interface OTAAutoAssetClient ()
@property (nonatomic) dispatch_queue_t notifyQueue;
@property (nonatomic, copy) void (^assetDidChangeHandler)(void);
@property (nonatomic) BOOL recheckAssetVersion;
@property (nonatomic) uint64_t lastCurrentVersion;
@property (nonatomic) uint64_t lastAvailableVersion;
@property (nonatomic) NSString *lastAssetPath;
#if !TARGET_OS_BRIDGE
@property (nonatomic) MAAutoAssetSelector *currentAssetSelector;
#endif
@end

@implementation OTAAutoAssetClient

- (nullable instancetype)initWithError:(NSError **)error {
    self = [super init];
#if !TARGET_OS_BRIDGE
    if (self) {
        // Indicate interest in PKITrustStore asset
        NSError *createInterestError = nil;
        BOOL didCreateInterest = [self _createInterestInAssetType:OTAAutoAssetAssetType withAssetSpecifier:OTAAutoAssetAssetSpecifier withError:&createInterestError];
        if (!didCreateInterest) {
            if (error) {
                *error = createInterestError;
            }
            return nil;
        }
    }
#endif
    return self;
}

#pragma mark - Asset Changed Notifications

- (void)_handleAssetChangedNotification {
#if !TARGET_OS_BRIDGE
    if (self.assetDidChangeHandler) {
        self.assetDidChangeHandler();
    }
#endif
}

- (void)_registerForNotificationsForAssetType:(NSString *)assetType andAssetSpecifier:(NSString *)assetSpecifier {
#if !TARGET_OS_BRIDGE
    self.notifyQueue = dispatch_queue_create(OTAAutoAssetNotificationQueueName.UTF8String, DISPATCH_QUEUE_SERIAL_WITH_AUTORELEASE_POOL);
    // Register for asset type + specifier notification
    NSString *notificationDownloadedSpecifierName = [MAAutoAssetNotifications
        notifyRegistrationName:MA_AUTO_ASSET_NOTIFICATION_ASSET_VERSION_DOWNLOADED
        forAssetType:assetType
        forAssetSpecifier:assetSpecifier];
    int versionDownloadedSpecifierNotifyToken = NOTIFY_TOKEN_INVALID;
    notify_register_dispatch([notificationDownloadedSpecifierName UTF8String], &versionDownloadedSpecifierNotifyToken, self.notifyQueue, ^(int token) {
        [self _handleAssetChangedNotification];
    });
#endif
}

- (void)registerForAssetChangedNotificationsWithBlock:(void (^)(void))assetDidChangeHandler {
#if !TARGET_OS_BRIDGE
    // Store the block if there is one
    if (assetDidChangeHandler) {
        self.assetDidChangeHandler = assetDidChangeHandler;
    }
    // Register for notifications for PKITrustStore asset
    [self _registerForNotificationsForAssetType:OTAAutoAssetAssetType andAssetSpecifier:OTAAutoAssetAssetSpecifier];
#endif
}

#pragma mark - AutoAsset SPI

#if !TARGET_OS_BRIDGE
- (MAAutoAsset *)_createAutoAssetWithType:(NSString *)assetType specifier:(NSString *)assetSpecifier error:(NSError **)outError {
    // Create an asset selector for the specified asset
    self.currentAssetSelector = [[MAAutoAssetSelector alloc] initForAssetType:assetType withAssetSpecifier:assetSpecifier];
    NSError *selectAssetError = nil;
    MAAutoAsset *autoAsset = [[MAAutoAsset alloc] initForClientName:OTAAutoAssetClientName
                                                     selectingAsset:self.currentAssetSelector
                                                              error:&selectAssetError];
    if (selectAssetError) {
        secerror("Unable to create auto-asset instance: %@", selectAssetError);
        TrustdHealthAnalyticsLogErrorCode(TAEventAssetBuiltIn, false, (OSStatus)CFErrorGetCode((__bridge CFErrorRef)selectAssetError));
        if (outError) {
            *outError = selectAssetError;
        }
        autoAsset = nil;
    }
    return autoAsset;
}
#endif

- (void)_recheckAssetVersion {
    if (!self.recheckAssetVersion) { return; }
    self.recheckAssetVersion = NO;
#if !TARGET_OS_BRIDGE
    CFErrorRef error = NULL;
    self.lastAvailableVersion = SecOTAPKIGetAvailableTrustStoreVersion((__bridge CFStringRef)self.lastAssetPath, &error);
    if (0 == self.lastAvailableVersion) {
        secerror("Unable to read trust store version from asset path, giving up");
        TrustdHealthAnalyticsLogErrorCode(TAEventAssetBuiltIn, false, (error) ? (OSStatus)CFErrorGetCode(error) : -1);
    } else if (0 != self.lastCurrentVersion) {
        secnotice("OTATrust", "Available version after recheck: %llu, our current version: %llu",
                  (unsigned long long)self.lastAvailableVersion, (unsigned long long)self.lastCurrentVersion);
        if (self.lastAvailableVersion > self.lastCurrentVersion) {
            // restart trustd to pick up new asset
            secnotice("OTATrust", "Will exit when clean to use updated asset");
            xpc_transaction_exit_clean();
        }
    }
    CFReleaseNull(error);
#endif
}

- (BOOL)_createInterestInAssetType:(NSString *)assetType withAssetSpecifier:(NSString *)assetSpecifier withError:(NSError **)outError {
#if !TARGET_OS_BRIDGE
    NSError *assetError = nil;
    MAAutoAsset *autoAsset = [self _createAutoAssetWithType:assetType specifier:assetSpecifier error:&assetError];
    if (!autoAsset) {
        secerror("Unable to create auto-asset instance for %@: %@", assetSpecifier, assetError);
        if (outError) {
            *outError = assetError;
        }
        return NO;
    }
    // Indicate our interest in the asset asynchronously
    [autoAsset interestInContent:OTAAutoAssetClientContentInterestReason
                   completion:^(MAAutoAssetSelector * _Nonnull assetSelector,
                                NSError * _Nullable operationError) {
        if (operationError) {
            secerror("Interest registration failed for %@ with error: %@", assetSpecifier, operationError);
        } else {
            secnotice("OTATrust", "Successfully registered interest for %@", assetSpecifier);
            [self _recheckAssetVersion];
        }
    }];
    return YES;
#else
    return NO;
#endif
}

- (BOOL)_removeInterestInAssetType:(NSString *)assetType withAssetSpecifier:(NSString *)assetSpecifier withError:(NSError **)outError {
#if !TARGET_OS_BRIDGE
    [MAAutoAsset eliminateAllForSelector:self.currentAssetSelector
                              completion:^(MAAutoAssetSelector * _Nonnull assetSelector,
                                           NSError * _Nullable operationError) {
        if (operationError) {
            secerror("Failed to eliminate asset: %@", operationError);
        } else {
            secnotice("OTATrust", "Successfully removed interest for %@", assetSpecifier);
        }
    }];
    return YES;
#else
    return NO;
#endif
}

+ (nullable NSString *)validTrustStoreAssetPath:(NSString *)assetPath mustExist:(BOOL)mustExist {
#if !TARGET_OS_BRIDGE
    // given an asset path string, check it is not null,
    // resolves to an acceptable location, is on AuthAPFS, and the path exists.
    // returns the validated real path on success,
    // or null if the path could not be validated.
    __block bool validated = false;
    __block NSString *realPathString = nil;
    if (!assetPath) {
        secnotice("OTATrust", "invalid asset path: NULL");
        return nil;
    }
    CFStringPerformWithCString((__bridge CFStringRef)assetPath, ^(const char *utf8Str) {
        int result = ENOENT;
        char real_full_path[PATH_MAX];
        real_full_path[0] = '\0';
        // resolve the full path in case of symlinks, dots, etc.
        if (realpath(utf8Str, real_full_path) != NULL) {
            realPathString = [NSString stringWithCString:real_full_path encoding:NSUTF8StringEncoding];
            // make sure path prefix is what we expect from MobileAsset
            NSArray *knownPrefixes = @[ OTAAssetsV2PathPrefix, OTAMacOSAssetsV2PathPrefix, OTACryptexesPathPrefix ];
            for (id prefix in knownPrefixes) {
                if ([realPathString hasPrefix:prefix]) {
                    // check whether the path exists!
                    struct stat sb;
                    int ret = stat(real_full_path, &sb);
                    if (ret == 0) {
                        secnotice("OTATrust", "found valid asset path: %{public}s", real_full_path);
                        result = ret;
                        validated = true;
                    } else if (!mustExist) {
                        secnotice("OTATrust", "skipping existence check for %{public}s", real_full_path);
                        result = 0;
                        validated = true;
                    } else {
                        result = errno;
                        secnotice("OTATrust", "failed to stat %{public}s: %s", real_full_path, strerror(result));
                    }
                }
            }
            if (!validated) {
                secnotice("OTATrust", "invalid asset path: %{public}@", realPathString);
                TrustdHealthAnalyticsLogErrorCode(TAEventAssetBuiltIn, false, (OSStatus)result);
            }
        } else {
            secnotice("OTATrust", "unable to resolve asset path: %{public}s", utf8Str);
            TrustdHealthAnalyticsLogErrorCode(TAEventAssetBuiltIn, false, (OSStatus)result);
        }
    });
    if (validated) {
        if (!mustExist) {
            return realPathString; // we are testing the path string only, not opening a file
        }
        // one more thing. make sure this asset path is on an AuthAPFS volume (125399777)
        NSString *filename = @"AssetVersion.plist";
        NSString *pathString = [NSString stringWithFormat:@"%@/%@", realPathString, filename];
        if (!SecOTAPKIPathIsOnAuthAPFSVolume((__bridge CFStringRef)pathString)) {
            secnotice("OTATrust", "ignoring asset path (not on an AuthAPFS volume)");
            TrustdHealthAnalyticsLogErrorCode(TAEventAssetBuiltIn, false, EACCES);
            return nil;
        }
        return realPathString;
    }
#endif
    return nil;
}

+ (BOOL)saveTrustStoreAssetPath:(NSString *)assetPath {
#if !TARGET_OS_BRIDGE
    if (!TrustdVariantAllowsFileWrite()) {
        return NO;
    }
    __block NSString *plistPath = nil;
    WithPathInProtectedTrustdDirectory((__bridge CFStringRef)OTAAutoAssetSettingsPlist, ^(const char *utf8String) {
        plistPath = [NSString stringWithCString:utf8String encoding:NSUTF8StringEncoding];
    });
    if (!plistPath) { return NO; }

    NSError *error = nil;
    NSURL *plistURL = [NSURL fileURLWithPath:plistPath isDirectory:NO];
    NSMutableDictionary *autoAssetDict = [NSMutableDictionary dictionaryWithCapacity:0];
    autoAssetDict[OTAAutoAssetPathKey] = assetPath;
    if (![autoAssetDict writeToClassDURL:plistURL permissions:0666 error:&error]) {
        secerror("failed to write %{public}@: %@", plistPath, error);
    } else {
        return YES;
    }
#endif
    return NO;
}

+ (nullable NSString *)savedTrustStoreAssetPath {
#if !TARGET_OS_BRIDGE
    __block NSString *plistPath = nil;
    WithPathInProtectedTrustdDirectory((__bridge CFStringRef)OTAAutoAssetSettingsPlist, ^(const char *utf8String) {
        plistPath = [NSString stringWithCString:utf8String encoding:NSUTF8StringEncoding];
    });
    if (!plistPath) {
        secnotice("OTATrust", "unable to resolve location of %{public}@", OTAAutoAssetSettingsPlist);
        return NULL;
    }
    NSDictionary *tsDict = [NSDictionary dictionaryWithContentsOfFile:plistPath];
    if (!tsDict) {
        secnotice("OTATrust", "unable to read from %{public}@", plistPath);
        return NULL;
    }
    NSString *value = [tsDict objectForKey:OTAAutoAssetPathKey];
    if (!value || !isString((__bridge CFStringRef) value)) {
        secnotice("OTATrust", "could not read OTAAutoAssetPathKey from %{public}@", plistPath);
        return NULL;
    }
    if (value) {
        return value;
    }
#endif
    return NULL;
}

- (nullable NSString *)startUsingLocalAsset {
#if !TARGET_OS_BRIDGE
    // IMPORTANT:
    // We cannot call MobileAsset APIs synchronously, since that causes hangs and/or deadlocks.
    // To avoid that, we start up in the following order:
    // 1. If we were previously given a path that we saved, check that it is valid and exists.
    // 2. If we have a valid asset path, return it so it will be used.
    // 3. If we don't have a valid asset path, then return NULL to use the built-in asset from the OS.
    //
    // We call MobileAsset asynchronously to retrieve and lock the current asset path.
    // Inside the _lockLocalAsset completion block, we can determine if it's the same path we're already using
    // (and if it is, whether the content version has changed.)
    // Then we can save the new path and quit trustd if needed, which will proceed with step 1 again.

    NSString *assetPath = [OTAAutoAssetClient savedTrustStoreAssetPath];
    self.lastAssetPath = assetPath;
    [self _lockLocalAsset:assetPath]; // this is done asynchronously and we do NOT wait for it.

    // Resolve and validate the saved asset path.
    // This will return nil if the asset path is invalid or does not exist,
    // causing us to use the built-in asset instead.
    return [OTAAutoAssetClient validTrustStoreAssetPath:assetPath mustExist:YES];
#else
    return nil;
#endif
}

- (void)stopUsingLocalAsset {
    [self _endLocalAssetLocks];
}

static uint64_t CurrentlyAvailableTrustStoreVersion(NSString *assetPath) {
    uint64_t tsVersion = SecOTAPKIGetAvailableTrustStoreVersion((__bridge CFStringRef)assetPath, NULL);
    if (0 == tsVersion) {
        tsVersion = SecOTAPKIGetSystemTrustStoreVersion(NULL);
    }
    return tsVersion;
}

- (void)_lockLocalAsset:(nullable NSString *)assetPath {
#if !TARGET_OS_BRIDGE
    // note: input assetPath may be:
    // 1. NULL, which means there was no saved asset path and we should restart when we get a new path.
    // 2. a non-empty string which means there was a saved asset path (though not necessarily valid).
    // In the second case, we will need to verify that the new path differs from this path,
    // or has later content than we are currently using.
    __block NSString *localAssetPath = assetPath;
    __block uint64_t localVersion = CurrentlyAvailableTrustStoreVersion(assetPath);
    self.lastCurrentVersion = localVersion;

    // Create an asset selector for our client name
    NSError *selectAssetError = nil;
    MAAutoAsset *autoAsset = [[MAAutoAsset alloc] initForClientName:OTAAutoAssetClientName selectingAsset:self.currentAssetSelector error:&selectAssetError];
    if (selectAssetError) {
        secerror("Unable to create auto-asset instance: %@", selectAssetError.description);
        return;
    }
    // Check lock status asynchronously (this is informational only)
    [autoAsset currentStatus:^(MAAutoAssetStatus *assetStatus, NSError *statusError) {
        if (statusError) {
            secerror("Unable to get asset status: %@", statusError.description);
        }
        NSDictionary *lockUsage = [assetStatus currentLockUsage];
        secnotice("OTATrust", "Current %@ asset usage: %@", OTAAutoAssetAssetSpecifier, lockUsage);
        bool isLocked = [lockUsage objectForKey:OTAAutoAssetClientContentInterestReason] != nil;
        secnotice("OTATrust", "Current %@ status: %@", OTAAutoAssetAssetSpecifier, isLocked ? @"locked" : @"unlocked");
    }];

    // Set lock usage policy (user did not initiate this action)
    MAAutoAssetPolicy *lockUsagePolicy = [MAAutoAssetPolicy new];
    lockUsagePolicy.userInitiated = NO;

    // Lock the asset asynchronously
    [autoAsset lockContent:OTAAutoAssetClientContentInterestReason
           withUsagePolicy:lockUsagePolicy
               withTimeout:MA_LOCK_CONTENT_SECS_NO_WAIT
         reportingProgress:nil
                completion:^(MAAutoAssetSelector * _Nonnull assetSelector,
                             BOOL contentLocked,
                             NSURL * _Nullable localContentURL,
                             MAAutoAssetStatus * _Nullable newerInProgress,
                             NSError * _Nullable lockForUseError) {
        // completion callback should give us an existing version of the asset, or nil.
        if (lockForUseError) {
            secerror("Unable to lock any version of auto-asset content: %@", lockForUseError.description);
            TrustdHealthAnalyticsLogErrorCode(TAEventAssetBuiltIn, false, (OSStatus)CFErrorGetCode((__bridge CFErrorRef)lockForUseError));
            return;
        }
        if (localContentURL.path) {
            bool hasNewContent = false;
            secnotice("OTATrust", "Locked %@ asset at path: %@", OTAAutoAssetAssetSpecifier, localContentURL.path);
            secnotice("OTATrust", "Previous %@ asset path: %@", OTAAutoAssetAssetSpecifier, localAssetPath);
            if (localAssetPath == nil || ![localAssetPath isEqualToString:localContentURL.path]) {
                // we have a non-nil path that is different from what we had when we started up
                secnotice("OTATrust", "New %@ asset path: %@", OTAAutoAssetAssetSpecifier, localContentURL.path);
                hasNewContent = true;
            } else {
                // we have the same path, but its contents may only become available after the lock.
                uint64_t availableVersion = SecOTAPKIGetAvailableTrustStoreVersion((__bridge CFStringRef)localContentURL.path, NULL);
                secnotice("OTATrust", "Available version: %llu, our current version: %llu, content locked: %s",
                          (unsigned long long)availableVersion, (unsigned long long)localVersion,
                          (contentLocked) ? "true" : "false");
                self.lastAvailableVersion = availableVersion;
                self.lastAssetPath = localContentURL.path;
                if (0 == availableVersion) {
                    // this is unexpected since MA told us we had the asset lock, but if this is prior to
                    // first unlock, we may be running into rdar://126777531 and will need to retry later.
                    self.recheckAssetVersion = YES;
                    secerror("Unable to read trust store version from locked asset path, will retry later");
                    return;
                }
                if (availableVersion > localVersion && localVersion > 0) {
                    hasNewContent = true;
                }
            }
            if (hasNewContent) {
                // save new asset path
                if ([OTAAutoAssetClient saveTrustStoreAssetPath:localContentURL.path] == YES) {
                    secnotice("OTATrust", "--- New asset path obtained from MobileAsset ---");
                    // restart trustd to pick up new asset
                    secnotice("OTATrust", "Will exit when clean to use updated asset");
                    TrustdHealthAnalyticsLogSuccess(TAEventAssetUpdate);
                    xpc_transaction_exit_clean();
                }
            }
        }
    }];
#endif
}

- (void)_endLocalAssetLocks {
#if !TARGET_OS_BRIDGE
    // Create an asset selector for our client name
    [MAAutoAsset endAllPreviousLocksOfSelector:self.currentAssetSelector
                                    completion:^(MAAutoAssetSelector * _Nonnull assetSelector,
                                                 NSError * _Nullable operationError) {
          if (operationError) {
              secerror("Failed to end asset locks for %@: %@", OTAAutoAssetClientName, operationError);
          } else {
              secnotice("OTATrust", "Ended local asset locks for %@", OTAAutoAssetClientName);
          }
    }];
#endif
}

@end
