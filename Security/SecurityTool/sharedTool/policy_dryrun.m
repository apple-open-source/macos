#import <CloudKit/CKContainer_Private.h>
#import <CloudKit/CloudKit.h>
#import <CoreFoundation/CFPriv.h>
#import <Foundation/Foundation.h>
#import <Foundation/NSURLSession.h>
#import <dispatch/dispatch.h>
#import <os/variant_private.h>

#include "policy_dryrun.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>
#include <Security/Security.h>
#include <keychain/SecureObjectSync/SOSCloudCircle.h>
#include <keychain/SecureObjectSync/SOSViews.h>
#import "TrustedPeers/TrustedPeers.h"

#include "tool_errors.h"

// Depends on a modern enough TrustedPeers framework.
// If the machine doesn't have one installed, set DYLD_FRAMEWORK_PATH

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
    NSError *error = nil;
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
    } else {
      NSLog(@"Unable to fetch CK device ID");
    }

    dict[@"mismatches"] = reportedMismatches;

    NSArray<NSDictionary*>* events = @[dict];
    NSDictionary *wrapper = @{
			      @"postTime": @([[NSDate date] timeIntervalSince1970] * 1000),
			      @"events": events,
    };
    NSData* data = [NSJSONSerialization dataWithJSONObject:wrapper options:0 error:&error];
    if (data == nil || error != nil) {
        NSLog(@"failed to encode data: %@", error);
        return;
    }
    NSLog(@"logging %@, %@", wrapper, data);
    req.HTTPBody = data;
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

int policy_dryrun(int argc, char * const *argv) {
    @autoreleasepool {
        NSError* error = nil;
        // From Swift.policy, policy v7

        TPPolicyDocument *tpd = [TPPolicyDocument policyDocWithHash:@"SHA256:dL8Qujqzprhp6FdH5GzNMtPlnZtLWMwfiiF7aykr8WU="
                                                  data:[[NSData alloc] initWithBase64EncodedString:
                                                                           @"CAcSDgoGaVBob25lEgRmdWxsEgwKBGlQYWQSBGZ1bGwSDAoEaVBvZBIEZnVsbBILCgNNYWMSBGZ1bGwSDAoEaU1hYxIEZnVsbBINCgdBcHBsZVRWEgJ0dhIOCgVXYXRjaBIFd2F0Y2gSFwoOQXVkaW9BY2Nlc3NvcnkSBWF1ZGlvGh4KBEhvbWUSBGZ1bGwSBXdhdGNoEgJ0dhIFYXVkaW8aJAoVUHJvdGVjdGVkQ2xvdWRTdG9yYWdlEgRmdWxsEgV3YXRjaBoYCglQYXNzd29yZHMSBGZ1bGwSBXdhdGNoGh8KEFNlY3VyZU9iamVjdFN5bmMSBGZ1bGwSBXdhdGNoGh4KBFdpRmkSBGZ1bGwSBXdhdGNoEgJ0dhIFYXVkaW8aGgoLQ3JlZGl0Q2FyZHMSBGZ1bGwSBXdhdGNoGhcKCEFwcGxlUGF5EgRmdWxsEgV3YXRjaBoVCgZIZWFsdGgSBGZ1bGwSBXdhdGNoGhkKCkF1dG9VbmxvY2sSBGZ1bGwSBXdhdGNoGi0KE0xpbWl0ZWRQZWVyc0FsbG93ZWQSBGZ1bGwSBXdhdGNoEgJ0dhIFYXVkaW8aHAoNRGV2aWNlUGFpcmluZxIEZnVsbBIFd2F0Y2gaFgoHTWFuYXRlZRIEZnVsbBIFd2F0Y2gaFQoGRW5ncmFtEgRmdWxsEgV3YXRjaBoXCghCYWNrc3RvcBIEZnVsbBIFd2F0Y2gaGwoMQXBwbGljYXRpb25zEgRmdWxsEgV3YXRjaCITCgRmdWxsEgRmdWxsEgV3YXRjaCIVCgJ0dhIEZnVsbBIFd2F0Y2gSAnR2IhQKBXdhdGNoEgRmdWxsEgV3YXRjaCIbCgVhdWRpbxIEZnVsbBIFd2F0Y2gSBWF1ZGlvMiIKFgAEIhICBHZ3aHQKCl5BcHBsZVBheSQSCEFwcGxlUGF5MiYKGAAEIhQCBHZ3aHQKDF5BdXRvVW5sb2NrJBIKQXV0b1VubG9jazIeChQABCIQAgR2d2h0CgheRW5ncmFtJBIGRW5ncmFtMh4KFAAEIhACBHZ3aHQKCF5IZWFsdGgkEgZIZWFsdGgyGgoSAAQiDgIEdndodAoGXkhvbWUkEgRIb21lMiAKFQAEIhECBHZ3aHQKCV5NYW5hdGVlJBIHTWFuYXRlZTI4CiEABCIdAgR2d2h0ChVeTGltaXRlZFBlZXJzQWxsb3dlZCQSE0xpbWl0ZWRQZWVyc0FsbG93ZWQyXQpQAAISHgAEIhoCBHZ3aHQKEl5Db250aW51aXR5VW5sb2NrJBIVAAQiEQIEdndodAoJXkhvbWVLaXQkEhUABCIRAgR2d2h0CgleQXBwbGVUViQSCU5vdFN5bmNlZDIrChsABCIXAgRhZ3JwCg9eWzAtOUEtWl17MTB9XC4SDEFwcGxpY2F0aW9uczLKAQq1AQACEjYAAQoTAAQiDwIFY2xhc3MKBl5nZW5wJAodAAQiGQIEYWdycAoRXmNvbVwuYXBwbGVcLnNiZCQSQAABChMABCIPAgVjbGFzcwoGXmtleXMkCicABCIjAgRhZ3JwChteY29tXC5hcHBsZVwuc2VjdXJpdHlcLnNvcyQSGQAEIhUCBHZ3aHQKDV5CYWNrdXBCYWdWMCQSHAAEIhgCBHZ3aHQKEF5pQ2xvdWRJZGVudGl0eSQSEFNlY3VyZU9iamVjdFN5bmMyYwpbAAISEgAEIg4CBHZ3aHQKBl5XaUZpJBJDAAEKEwAEIg8CBWNsYXNzCgZeZ2VucCQKEwAEIg8CBGFncnAKB15hcHBsZSQKFQAEIhECBHN2Y2UKCV5BaXJQb3J0JBIEV2lGaTKdAwqDAwACEhgABCIUAgR2d2h0CgxeUENTLUJhY2t1cCQSGgAEIhYCBHZ3aHQKDl5QQ1MtQ2xvdWRLaXQkEhgABCIUAgR2d2h0CgxeUENTLUVzY3JvdyQSFQAEIhECBHZ3aHQKCV5QQ1MtRkRFJBIaAAQiFgIEdndodAoOXlBDUy1GZWxkc3BhciQSGgAEIhYCBHZ3aHQKDl5QQ1MtTWFpbERyb3AkEhoABCIWAgR2d2h0Cg5eUENTLU1haWxkcm9wJBIbAAQiFwIEdndodAoPXlBDUy1NYXN0ZXJLZXkkEhcABCITAgR2d2h0CgteUENTLU5vdGVzJBIYAAQiFAIEdndodAoMXlBDUy1QaG90b3MkEhkABCIVAgR2d2h0Cg1eUENTLVNoYXJpbmckEh4ABCIaAgR2d2h0ChJeUENTLWlDbG91ZEJhY2t1cCQSHQAEIhkCBHZ3aHQKEV5QQ1MtaUNsb3VkRHJpdmUkEhoABCIWAgR2d2h0Cg5eUENTLWlNZXNzYWdlJBIVUHJvdGVjdGVkQ2xvdWRTdG9yYWdlMj0KLgAEIioCBGFncnAKIl5jb21cLmFwcGxlXC5zYWZhcmlcLmNyZWRpdC1jYXJkcyQSC0NyZWRpdENhcmRzMjAKIwAEIh8CBGFncnAKF15jb21cLmFwcGxlXC5jZm5ldHdvcmskEglQYXNzd29yZHMybQpcAAISHgAEIhoCBHZ3aHQKEl5BY2Nlc3NvcnlQYWlyaW5nJBIaAAQiFgIEdndodAoOXk5hbm9SZWdpc3RyeSQSHAAEIhgCBHZ3aHQKEF5XYXRjaE1pZ3JhdGlvbiQSDURldmljZVBhaXJpbmcyDgoCAAYSCEJhY2tzdG9w" options:0]];
        TPPolicy *policy = [tpd policyWithSecrets:@{} decrypter:nil error:&error];
        if (error != nil) {
            NSLog(@"policy error: %@", error);
            return 1;
        }
        if (policy == nil) {
            NSLog(@"policy is nil");
            return 1;
        }

        TPSyncingPolicy* syncingPolicy = [policy syncingPolicyForModel:@"iPhone"
                                             syncUserControllableViews:TPPBPeerStableInfo_UserControllableViewStatus_UNKNOWN
                                                                 error:&error];
        if(syncingPolicy == nil || error != nil) {
            NSLog(@"syncing policy is nil: %@", error);
            return 1;
        }

        unsigned real_mismatches = 0;
        unsigned expected_mismatches = 0;
        NSMutableArray<NSDictionary*>* reportedMismatches = [[NSMutableArray<NSDictionary*> alloc] init];

        NSArray<NSString*>* keychainClasses = @[(id)kSecClassInternetPassword,
                                                (id)kSecClassGenericPassword,
                                                (id)kSecClassKey,
                                                (id)kSecClassCertificate];

        for(NSString* itemClass in keychainClasses) {
	    NSLog(@"itemClass: %@", itemClass);
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
                    return status;
                }
            }

            if (![result isKindOfClass:[NSArray class]]) {
                NSLog(@"expected NSArray result from SecItemCopyMatching");
                return -1;
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
    return 0;
}
