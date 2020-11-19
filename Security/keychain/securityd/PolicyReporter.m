#include "PolicyReporter.h"

#import <CloudKit/CKContainer_Private.h>
#import <CloudKit/CloudKit.h>
#import <CoreFoundation/CFPriv.h>
#import <Foundation/Foundation.h>
#import <Foundation/NSURLSession.h>
#import <dispatch/dispatch.h>
#import <os/feature_private.h>
#import <os/variant_private.h>

#include <notify.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <xpc/private.h>
#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>
#include <Security/Security.h>
#include <keychain/SecureObjectSync/SOSCloudCircle.h>
#include <keychain/SecureObjectSync/SOSViews.h>
#include <utilities/SecAKSWrappers.h>
#import "utilities/debugging.h"
#import "TrustedPeers/TrustedPeers.h"

// Stolen from keychain/SecureObjectSync/SOSEngine.c

static NSString* getSOSView(id object, NSString* itemClass) {
    if (![object isKindOfClass:[NSDictionary class]]) {
        return nil;
    }

    NSString *viewHint = object[(NSString*)kSecAttrSyncViewHint];
    if (viewHint != nil) {
        return viewHint;
    } else {
        NSString *ag = object[(NSString*)kSecAttrAccessGroup];
        if ([itemClass isEqualToString: (NSString*)kSecClassKey] && [ag isEqualToString: @"com.apple.security.sos"]) {
            return (NSString*)kSOSViewiCloudIdentity;
        } else if ([ag isEqualToString: @"com.apple.cfnetwork"]) {
            return (NSString*)kSOSViewAutofillPasswords;
        } else if ([ag isEqualToString: @"com.apple.safari.credit-cards"]) {
            return (NSString*)kSOSViewSafariCreditCards;
        } else if ([itemClass isEqualToString: (NSString*)kSecClassGenericPassword]) {
            if ([ag isEqualToString: @"apple"] &&
                [object[(NSString*)kSecAttrService] isEqualToString: @"AirPort"]) {
                return (NSString*)kSOSViewWiFi;
            } else if ([ag isEqualToString: @"com.apple.sbd"]) {
                return (NSString*)kSOSViewBackupBagV0;
            } else {
                return (NSString*)kSOSViewOtherSyncable; // (genp)
            }
        } else {
            return (NSString*)kSOSViewOtherSyncable; // (inet || keys)
        }
    }
}

static inline NSNumber *now_msecs() {
    return @(((long)[[NSDate date] timeIntervalSince1970] * 1000));
}

static NSString* cloudKitDeviceID() {
  __block NSString* ret = nil;

  if (!os_variant_has_internal_diagnostics("com.apple.security")) {
    return nil;
  }
  CKContainer *container = [CKContainer containerWithIdentifier:@"com.apple.security.keychain"];
  dispatch_semaphore_t sem = dispatch_semaphore_create(0);
  [container fetchCurrentDeviceIDWithCompletionHandler:^(NSString* deviceID, NSError* error) {
      if (error != nil) {
	NSLog(@"failed to fetch CK deviceID: %@", error);
      } else {
	ret = deviceID;
      }
      dispatch_semaphore_signal(sem);
    }];
  dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);
  return ret;
}

static void reportStats(unsigned expected_mismatches, unsigned real_mismatches, NSArray<NSDictionary*>* reportedMismatches) {
    NSURLSessionConfiguration *defaultConfiguration = [NSURLSessionConfiguration ephemeralSessionConfiguration];
    NSURLSession *session = [NSURLSession sessionWithConfiguration:defaultConfiguration];
    NSURL *endpoint = [NSURL URLWithString:@"https://xp.apple.com/report/2/xp_sear_keysync"];
    NSMutableURLRequest *req = [[NSMutableURLRequest alloc] init];
    req.URL = endpoint;
    req.HTTPMethod = @"POST";
    NSMutableDictionary *dict = [@{
        @"expectedMismatches": @(expected_mismatches),
        @"realMismatches": @(real_mismatches),
        @"eventTime": now_msecs(),
        @"topic": @"xp_sear_keysync",
        @"eventType": @"policy-dryrun",
    } mutableCopy];
    NSDictionary *version = CFBridgingRelease(_CFCopySystemVersionDictionary());
    NSString *build = version[(__bridge NSString *)_kCFSystemVersionBuildVersionKey];
    if (build != nil) {
      dict[@"build"] = build;
    } else {
      NSLog(@"Unable to find out build version");
    }
    NSString *product = version[(__bridge NSString *)_kCFSystemVersionProductNameKey];
    if (product != nil) {
      dict[@"product"] = product;
    } else {
      NSLog(@"Unable to find out build product");
    }
    NSString* ckDeviceID = cloudKitDeviceID();
    if (ckDeviceID) {
      dict[@"SFAnalyticsDeviceID"] = ckDeviceID;
      dict[@"ckdeviceID"] = ckDeviceID;
    } else {
      NSLog(@"Unable to fetch CK device ID");
    }

    dict[@"mismatches"] = reportedMismatches;

    NSArray<NSDictionary*>* events = @[dict];
    NSDictionary *wrapper = @{
			      @"postTime": @([[NSDate date] timeIntervalSince1970] * 1000),
			      @"events": events,
    };
    NSError *encodeError = nil;
    NSData* post_data = [NSJSONSerialization dataWithJSONObject:wrapper options:0 error:&encodeError];
    if (post_data == nil || encodeError != nil) {
        NSLog(@"failed to encode data: %@", encodeError);
        return;
    }
    NSLog(@"logging %@, %@", wrapper, post_data);
    req.HTTPBody = post_data;
    dispatch_semaphore_t sem = dispatch_semaphore_create(0);
    NSURLSessionDataTask *task = [session dataTaskWithRequest:req completionHandler:^(NSData *data, NSURLResponse *response, NSError *error) {
            if (error != nil) {
                NSLog(@"splunk upload failed: %@", error);
                return;
            }
            NSHTTPURLResponse *httpResp = (NSHTTPURLResponse*)response;
            if (httpResp.statusCode != 200) {
                NSLog(@"HTTP Post error: %@", httpResp);
            }
	    NSLog(@"%@", httpResp);
	    if (data != nil) {
	      size_t length = [data length];
	      char *buf = malloc(length);
	      [data getBytes:buf length:length];
	      NSLog(@"%.*s", (int)length, buf);
	      free(buf);
	    }
            dispatch_semaphore_signal(sem);
        }];
    [task resume];
    dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);
}

static void oneReport(void) {
    NSError* error = nil;

    // From Swift.policy, policy v5
    TPPolicyDocument *tpd = [TPPolicyDocument policyDocWithHash:@"SHA256:O/ECQlWhvNlLmlDNh2+nal/yekUC87bXpV3k+6kznSo="
                                              data:[[NSData alloc] initWithBase64EncodedString:
                                                                       @"CAUSDgoGaVBob25lEgRmdWxsEgwKBGlQYWQSBGZ1bGwSDAoEaVBvZBIEZnVsbBILCgNNYWMSBGZ1bGwSDAoEaU1hYxIEZnVsbBINCgdBcHBsZVRWEgJ0dhIOCgVXYXRjaBIFd2F0Y2gSFwoOQXVkaW9BY2Nlc3NvcnkSBWF1ZGlvGhsKDEFwcGxpY2F0aW9ucxIEZnVsbBIFd2F0Y2gaHwoQU2VjdXJlT2JqZWN0U3luYxIEZnVsbBIFd2F0Y2gaHAoNRGV2aWNlUGFpcmluZxIEZnVsbBIFd2F0Y2gaGgoLQ3JlZGl0Q2FyZHMSBGZ1bGwSBXdhdGNoGhUKBkhlYWx0aBIEZnVsbBIFd2F0Y2gaLQoTTGltaXRlZFBlZXJzQWxsb3dlZBIEZnVsbBIFd2F0Y2gSAnR2EgVhdWRpbxokChVQcm90ZWN0ZWRDbG91ZFN0b3JhZ2USBGZ1bGwSBXdhdGNoGhcKCEFwcGxlUGF5EgRmdWxsEgV3YXRjaBoZCgpBdXRvVW5sb2NrEgRmdWxsEgV3YXRjaBoWCgdNYW5hdGVlEgRmdWxsEgV3YXRjaBoYCglQYXNzd29yZHMSBGZ1bGwSBXdhdGNoGhUKBkVuZ3JhbRIEZnVsbBIFd2F0Y2gaHgoEV2lGaRIEZnVsbBIFd2F0Y2gSAnR2EgVhdWRpbxoTCgRIb21lEgRmdWxsEgV3YXRjaCIbCgVhdWRpbxIEZnVsbBIFd2F0Y2gSBWF1ZGlvIhMKBGZ1bGwSBGZ1bGwSBXdhdGNoIhUKAnR2EgRmdWxsEgV3YXRjaBICdHYiFAoFd2F0Y2gSBGZ1bGwSBXdhdGNoMiIKFgAEIhICBHZ3aHQKCl5BcHBsZVBheSQSCEFwcGxlUGF5MiYKGAAEIhQCBHZ3aHQKDF5BdXRvVW5sb2NrJBIKQXV0b1VubG9jazIeChQABCIQAgR2d2h0CgheRW5ncmFtJBIGRW5ncmFtMh4KFAAEIhACBHZ3aHQKCF5IZWFsdGgkEgZIZWFsdGgyGgoSAAQiDgIEdndodAoGXkhvbWUkEgRIb21lMiAKFQAEIhECBHZ3aHQKCV5NYW5hdGVlJBIHTWFuYXRlZTI4CiEABCIdAgR2d2h0ChVeTGltaXRlZFBlZXJzQWxsb3dlZCQSE0xpbWl0ZWRQZWVyc0FsbG93ZWQyXQpQAAISHgAEIhoCBHZ3aHQKEl5Db250aW51aXR5VW5sb2NrJBIVAAQiEQIEdndodAoJXkhvbWVLaXQkEhUABCIRAgR2d2h0CgleQXBwbGVUViQSCU5vdFN5bmNlZDIrChsABCIXAgRhZ3JwCg9eWzAtOUEtWl17MTB9XC4SDEFwcGxpY2F0aW9uczLFAQqwAQACEjQAAQoTAAQiDwIFY2xhc3MKBl5nZW5wJAobAAQiFwIEYWdycAoPXmNvbS5hcHBsZS5zYmQkEj0AAQoTAAQiDwIFY2xhc3MKBl5rZXlzJAokAAQiIAIEYWdycAoYXmNvbS5hcHBsZS5zZWN1cml0eS5zb3MkEhkABCIVAgR2d2h0Cg1eQmFja3VwQmFnVjAkEhwABCIYAgR2d2h0ChBeaUNsb3VkSWRlbnRpdHkkEhBTZWN1cmVPYmplY3RTeW5jMmMKWwACEhIABCIOAgR2d2h0CgZeV2lGaSQSQwABChMABCIPAgVjbGFzcwoGXmdlbnAkChMABCIPAgRhZ3JwCgdeYXBwbGUkChUABCIRAgRzdmNlCgleQWlyUG9ydCQSBFdpRmkynQMKgwMAAhIYAAQiFAIEdndodAoMXlBDUy1CYWNrdXAkEhoABCIWAgR2d2h0Cg5eUENTLUNsb3VkS2l0JBIYAAQiFAIEdndodAoMXlBDUy1Fc2Nyb3ckEhUABCIRAgR2d2h0CgleUENTLUZERSQSGgAEIhYCBHZ3aHQKDl5QQ1MtRmVsZHNwYXIkEhoABCIWAgR2d2h0Cg5eUENTLU1haWxEcm9wJBIaAAQiFgIEdndodAoOXlBDUy1NYWlsZHJvcCQSGwAEIhcCBHZ3aHQKD15QQ1MtTWFzdGVyS2V5JBIXAAQiEwIEdndodAoLXlBDUy1Ob3RlcyQSGAAEIhQCBHZ3aHQKDF5QQ1MtUGhvdG9zJBIZAAQiFQIEdndodAoNXlBDUy1TaGFyaW5nJBIeAAQiGgIEdndodAoSXlBDUy1pQ2xvdWRCYWNrdXAkEh0ABCIZAgR2d2h0ChFeUENTLWlDbG91ZERyaXZlJBIaAAQiFgIEdndodAoOXlBDUy1pTWVzc2FnZSQSFVByb3RlY3RlZENsb3VkU3RvcmFnZTI6CisABCInAgRhZ3JwCh9eY29tLmFwcGxlLnNhZmFyaS5jcmVkaXQtY2FyZHMkEgtDcmVkaXRDYXJkczIuCiEABCIdAgRhZ3JwChVeY29tLmFwcGxlLmNmbmV0d29yayQSCVBhc3N3b3JkczJtClwAAhIeAAQiGgIEdndodAoSXkFjY2Vzc29yeVBhaXJpbmckEhoABCIWAgR2d2h0Cg5eTmFub1JlZ2lzdHJ5JBIcAAQiGAIEdndodAoQXldhdGNoTWlncmF0aW9uJBINRGV2aWNlUGFpcmluZzIOCgIABhIIQmFja3N0b3A=" options:0]];
    
    TPPolicy *policy = [tpd policyWithSecrets:@{} decrypter:nil error:&error];
    if (error != nil) {
        NSLog(@"policy error: %@", error);
        return;
    }
    if (policy == nil) {
        NSLog(@"policy is nil");
        return;
    }
    TPSyncingPolicy* syncingPolicy = [policy syncingPolicyForModel:@"iPhone"
                                         syncUserControllableViews:TPPBPeerStableInfo_UserControllableViewStatus_UNKNOWN
                                                             error:&error];
    if(syncingPolicy == nil || error != nil) {
        NSLog(@"syncing policy is nil: %@", error);
        return;
    }

    unsigned real_mismatches = 0;
    unsigned expected_mismatches = 0;
    NSMutableArray<NSDictionary*>* reportedMismatches = [[NSMutableArray<NSDictionary*> alloc] init];

    NSArray<NSString*>* keychainClasses = @[(id)kSecClassInternetPassword,
                                            (id)kSecClassGenericPassword,
                                            (id)kSecClassKey,
                                            (id)kSecClassCertificate];

    for(NSString* itemClass in keychainClasses) {
        NSDictionary *query = @{ (id)kSecMatchLimit : (id)kSecMatchLimitAll,
                                 (id)kSecClass : (id)itemClass,
                                 (id)kSecReturnAttributes : @YES,
                                 (id)kSecAttrSynchronizable: @YES,
                                 (id)kSecUseDataProtectionKeychain: @YES,
                                 (id)kSecUseAuthenticationUI : (id)kSecUseAuthenticationUISkip,
                                 };

        NSArray *result;

        OSStatus status = SecItemCopyMatching((CFDictionaryRef)query, (void*)&result);
        if (status) {
            if (status == errSecItemNotFound) {
                NSLog(@"no items found matching: %@", query);
                continue;
            } else {
                NSLog(@"SecItemCopyMatching(%@) failed: %d", query, (int)status);
                return;
            }
        }

        if (![result isKindOfClass:[NSArray class]]) {
            NSLog(@"expected NSArray result from SecItemCopyMatching");
            return;
        }

        for (id a in result) {
            NSLog(@"%@", a);
            NSString *oldView = getSOSView(a, itemClass);
            if (oldView != nil) {
                NSLog(@"old: %@", oldView);
            }

            NSMutableDictionary* mutA = [a mutableCopy];
            mutA[(id)kSecClass] = (id)itemClass;

            NSString* newView = [syncingPolicy mapDictionaryToView:mutA];
            if (newView != nil) {
                NSLog(@"new: %@", newView);
            }
            if(oldView == nil ^ newView == nil) {
                NSLog(@"real mismatch: old view (%@) != new view (%@)", oldView, newView);
                ++real_mismatches;
                [reportedMismatches addObject: a];

                } else if (oldView && newView && ![oldView isEqualToString: newView]) {
                    if ([oldView hasPrefix:@"PCS-"] && [newView isEqualToString: @"ProtectedCloudStorage"]) {
                        NSLog(@"(expected PCS mismatch): old view (%@) != new view (%@)", oldView, newView);
                        ++expected_mismatches;

                    } else if([oldView isEqualToString:@"OtherSyncable"] && [newView isEqualToString: @"Applications"]) {
                        NSLog(@"(expected 3rd party mismatch): old view (%@) != new view (%@)", oldView, newView);
                        ++expected_mismatches;

                    } else if([oldView isEqualToString:@"OtherSyncable"] && [newView isEqualToString: @"Backstop"]) {
                        NSLog(@"(expected backstop mismatch): old view (%@) != new view (%@)", oldView, newView);
                        ++expected_mismatches;

		    } else if([newView isEqualToString:@"NotSynced"]) {
			NSLog(@"(expected NotSynced mismatch): old view (%@) != new view (%@)", oldView, newView);
			++expected_mismatches;
			
		    } else if(([oldView isEqualToString:@"BackupBagV0"] || [oldView isEqualToString:@"iCloudIdentity"]) && [newView isEqualToString:@"SecureObjectSync"]) {
			NSLog(@"(expected BackupBag - SecureObjectSync mismatch): old view (%@) != new view (%@)", oldView, newView);
			++expected_mismatches;

		    } else if(([oldView isEqualToString:@"AccessoryPairing"]
			       || [oldView isEqualToString:@"NanoRegistry"]
			       || [oldView isEqualToString:@"WatchMigration"]) && [newView isEqualToString:@"DevicePairing"]) {
			NSLog(@"(expected DevicePairing mismatch): old view (%@) != new view (%@)", oldView, newView);
			++expected_mismatches;

                    } else {
                        NSLog(@"real mismatch: old view (%@) != new view (%@)", oldView, newView);
                        ++real_mismatches;
                        [reportedMismatches addObject: a];
                    }
                }
        }
    }

    NSLog(@"%d expected_mismatches", expected_mismatches);
    NSLog(@"%d real_mismatches", real_mismatches);
    reportStats(expected_mismatches, real_mismatches, reportedMismatches);
}

static dispatch_queue_t queue;

static void report(void);

static void maybeReport(void) {
    CFErrorRef error = NULL;
    bool locked = true;
    if (!SecAKSGetIsLocked(&locked, &error)) {
        secerror("PolicyReporter: %@", error);
        CFReleaseNull(error);
        xpc_object_t options = xpc_dictionary_create(NULL, NULL, 0);
        xpc_dictionary_set_uint64(options, XPC_ACTIVITY_DELAY, random() % 60);
        xpc_dictionary_set_uint64(options, XPC_ACTIVITY_GRACE_PERIOD, 30);
        xpc_dictionary_set_bool(options, XPC_ACTIVITY_REPEATING, false);
        xpc_dictionary_set_bool(options, XPC_ACTIVITY_ALLOW_BATTERY, true);
        xpc_dictionary_set_string(options, XPC_ACTIVITY_PRIORITY, XPC_ACTIVITY_PRIORITY_UTILITY);
        xpc_dictionary_set_bool(options, XPC_ACTIVITY_REQUIRE_NETWORK_CONNECTIVITY, true);
#if TARGET_OS_IPHONE
        xpc_dictionary_set_bool(options, XPC_ACTIVITY_REQUIRES_CLASS_A, true);
#endif
        xpc_activity_register("com.apple.security.securityd.policy-reporting2",
                              options, ^(xpc_activity_t activity) {
                                  report();
                              });
        return;
    }

    if (locked) {
        int token = 0;
        notify_register_dispatch(kUserKeybagStateChangeNotification, &token, queue, ^(int t) {
                report();
            });
        return;
    }
    oneReport();
}

static void report() {
    @autoreleasepool {
        maybeReport();
    }
}

void InitPolicyReporter(void) {
    if (!os_feature_enabled(Security, securitydReportPolicy)) {
        secnotice("securityd-PolicyReporter", "not enabled by feature flag");
        return;
    }

    srandom(getpid() ^ (int)time(NULL));
    queue = dispatch_queue_create("com.apple.security.securityd_reporting", DISPATCH_QUEUE_SERIAL_WITH_AUTORELEASE_POOL);
    xpc_object_t options = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_uint64(options, XPC_ACTIVITY_DELAY, random() % 3600);
    xpc_dictionary_set_uint64(options, XPC_ACTIVITY_GRACE_PERIOD, 1800);
    xpc_dictionary_set_uint64(options, XPC_ACTIVITY_INTERVAL, 3600);
    xpc_dictionary_set_bool(options, XPC_ACTIVITY_REPEATING, true);
    xpc_dictionary_set_bool(options, XPC_ACTIVITY_ALLOW_BATTERY, true);
    xpc_dictionary_set_string(options, XPC_ACTIVITY_PRIORITY, XPC_ACTIVITY_PRIORITY_UTILITY);
    xpc_dictionary_set_bool(options, XPC_ACTIVITY_REQUIRE_NETWORK_CONNECTIVITY, true);
#if TARGET_OS_IPHONE
    xpc_dictionary_set_bool(options, XPC_ACTIVITY_REQUIRES_CLASS_A, true);
#endif
    
    xpc_activity_register("com.apple.security.securityd.policy-reporting",
                          options, ^(xpc_activity_t activity) {
                              report();
                          });
}
