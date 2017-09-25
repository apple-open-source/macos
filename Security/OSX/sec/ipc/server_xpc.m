/*
 * Copyright (c) 2017 Apple Inc.  All Rights Reserved.
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
 */

#import <Foundation/Foundation.h>

#include <ipc/securityd_client.h>
#include <ipc/server_security_helpers.h>
#include <ipc/server_endpoint.h>

#include <Security/SecEntitlements.h>
#include <Security/SecItemPriv.h>
#include <securityd/SecItemServer.h>
#include <securityd/SecItemSchema.h>
#include <securityd/SecItemDb.h>

#include "keychain/ckks/CKKSViewManager.h"

@implementation SecuritydXPCServer (SecuritydXPCProtocol)

- (void) SecItemAddAndNotifyOnSync:(NSDictionary*) attributes
                      syncCallback:(id<SecuritydXPCCallbackProtocol>) callback
                          complete:(void (^) (NSDictionary* opDictResult, NSArray* opArrayResult, NSError* operror)) complete
{
    CFErrorRef cferror = NULL;
    if([self clientHasBooleanEntitlement: (__bridge NSString*) kSecEntitlementKeychainDeny]) {
        SecError(errSecNotAvailable, &cferror, CFSTR("SecItemAddAndNotifyOnSync: %@ has entitlement %@"), _client.task, kSecEntitlementKeychainDeny);
        //TODO: ensure cferror can transit xpc
        complete(NULL, NULL, (__bridge NSError*) cferror);
        CFReleaseNull(cferror);
        return;
    }

    if(attributes[(id)kSecAttrDeriveSyncIDFromItemAttributes] ||
       attributes[(id)kSecAttrPCSPlaintextServiceIdentifier] ||
       attributes[(id)kSecAttrPCSPlaintextPublicKey] ||
       attributes[(id)kSecAttrPCSPlaintextPublicIdentity]) {

        if(![self clientHasBooleanEntitlement: (__bridge NSString*) kSecEntitlementPrivateCKKSPlaintextFields]) {
            SecError(errSecMissingEntitlement, &cferror, CFSTR("SecItemAddAndNotifyOnSync: %@ does not have entitlement %@, but is using SPI anyway"), _client.task, kSecEntitlementPrivateCKKSPlaintextFields);
            complete(NULL, NULL, (__bridge NSError*) cferror);
            CFReleaseNull(cferror);
            return;
        }
    }

    CFTypeRef cfresult = NULL;

    NSMutableDictionary* callbackQuery = [attributes mutableCopy];
    callbackQuery[@"f_ckkscallback"] = ^void (bool didSync, CFErrorRef syncerror) {
        [callback callCallback: didSync error: (__bridge NSError*)syncerror];
    };

    _SecItemAdd((__bridge CFDictionaryRef) callbackQuery, &_client, &cfresult, &cferror);

    //TODO: ensure cferror can transit xpc

    // SecItemAdd returns Some CF Object, but NSXPC is pretty adamant that everything be a specific NS type. Split it up here:
    if(!cfresult) {
        complete(NULL, NULL, (__bridge NSError *)(cferror));
    } else if( CFGetTypeID(cfresult) == CFDictionaryGetTypeID()) {
        complete((__bridge NSDictionary *)(cfresult), NULL, (__bridge NSError *)(cferror));
    } else if( CFGetTypeID(cfresult) == CFArrayGetTypeID()) {
        complete(NULL, (__bridge NSArray *)cfresult, (__bridge NSError *)(cferror));
    } else {
        // TODO: actually error here
        complete(NULL, NULL, NULL);
    }
    CFReleaseNull(cfresult);
    CFReleaseNull(cferror);
}

- (void)secItemSetCurrentItemAcrossAllDevices:(NSData* _Nonnull)newItemPersistentRef
                           newCurrentItemHash:(NSData* _Nonnull)newItemSHA1
                                  accessGroup:(NSString* _Nonnull)accessGroup
                                   identifier:(NSString* _Nonnull)identifier
                                     viewHint:(NSString* _Nonnull)viewHint
                      oldCurrentItemReference:(NSData* _Nullable)oldCurrentItemPersistentRef
                           oldCurrentItemHash:(NSData* _Nullable)oldItemSHA1
                                     complete:(void (^) (NSError* _Nullable operror)) complete
{
#if OCTAGON
    __block CFErrorRef cferror = NULL;
    if([self clientHasBooleanEntitlement: (__bridge NSString*) kSecEntitlementKeychainDeny]) {
        SecError(errSecNotAvailable, &cferror, CFSTR("SecItemSetCurrentItemAcrossAllDevices: %@ has entitlement %@"), _client.task, kSecEntitlementKeychainDeny);
        complete((__bridge NSError*) cferror);
        CFReleaseNull(cferror);
        return;
    }

    if(![self clientHasBooleanEntitlement: (__bridge NSString*) kSecEntitlementPrivateCKKSWriteCurrentItemPointers]) {
        SecError(errSecMissingEntitlement, &cferror, CFSTR("SecItemSetCurrentItemAcrossAllDevices: %@ does not have entitlement %@"), _client.task, kSecEntitlementPrivateCKKSWriteCurrentItemPointers);
        complete((__bridge NSError*) cferror);
        CFReleaseNull(cferror);
        return;
    }

    if (!accessGroupsAllows(self->_client.accessGroups, (__bridge CFStringRef)accessGroup, &_client)) {
        SecError(errSecMissingEntitlement, &cferror, CFSTR("SecItemSetCurrentItemAcrossAllDevices: client is missing access-group %@: %@"), accessGroup, _client.task);
        complete((__bridge NSError*)cferror);
        CFReleaseNull(cferror);
        return;
    }

    __block SecDbItemRef newItem = NULL;
    __block SecDbItemRef oldItem = NULL;

    bool ok = kc_with_dbt(false, &cferror, ^bool (SecDbConnectionRef dbt) {
        Query *q = query_create_with_limit( (__bridge CFDictionaryRef) @{
                                                                         (__bridge NSString *)kSecValuePersistentRef : newItemPersistentRef,
                                                                         (__bridge NSString *)kSecAttrAccessGroup : accessGroup,
                                                                         },
                                           NULL,
                                           1,
                                           &cferror);
        if(cferror) {
            secerror("couldn't create query: %@", cferror);
            return false;
        }

        if(!SecDbItemQuery(q, NULL, dbt, &cferror, ^(SecDbItemRef item, bool *stop) {
            newItem = CFRetainSafe(item);
        })) {
            query_destroy(q, NULL);
            return false;
        }

        if(!query_destroy(q, &cferror)) {
            return false;
        };

        if(oldCurrentItemPersistentRef) {
            q = query_create_with_limit( (__bridge CFDictionaryRef) @{
                                                                      (__bridge NSString *)kSecValuePersistentRef : oldCurrentItemPersistentRef,
                                                                      (__bridge NSString *)kSecAttrAccessGroup : accessGroup,
                                                                      },
                                        NULL,
                                        1,
                                        &cferror);
            if(cferror) {
                secerror("couldn't create query: %@", cferror);
                return false;
            }

            if(!SecDbItemQuery(q, NULL, dbt, &cferror, ^(SecDbItemRef item, bool *stop) {
                oldItem = CFRetainSafe(item);
            })) {
                query_destroy(q, NULL);
                return false;
            }

            if(!query_destroy(q, &cferror)) {
                return false;
            };
        }

        CKKSViewManager* manager = [CKKSViewManager manager];
        if(!manager) {
            secerror("SecItemSetCurrentItemAcrossAllDevices: no view manager?");
            cferror = (CFErrorRef) CFBridgingRetain([NSError errorWithDomain:@"securityd" code:errSecInternalError userInfo:@{NSLocalizedDescriptionKey: @"No view manager, cannot forward request"}]);
            return false;
        }
        [manager setCurrentItemForAccessGroup:newItem
                                         hash:newItemSHA1
                                  accessGroup:accessGroup
                                   identifier:identifier
                                     viewHint:viewHint
                                    replacing:oldItem
                                         hash:oldItemSHA1
                                     complete:complete];
        return true;
    });

    CFReleaseNull(newItem);
    CFReleaseNull(oldItem);

    if(!ok) {
        secnotice("ckks", "SecItemSetCurrentItemAcrossAllDevices failed due to: %@", cferror);
        complete((__bridge NSError*) cferror);
    }
    CFReleaseNull(cferror);
#else // ! OCTAGON
    complete([NSError errorWithDomain:@"securityd" code:errSecParam userInfo:@{NSLocalizedDescriptionKey: @"SecItemSetCurrentItemAcrossAllDevices not implemented on this platform"}]);
#endif // OCTAGON
}

-(void)secItemFetchCurrentItemAcrossAllDevices:(NSString*)accessGroup
                                    identifier:(NSString*)identifier
                                      viewHint:(NSString*)viewHint
                               fetchCloudValue:(bool)fetchCloudValue
                                      complete:(void (^) (NSData* persistentref, NSError* operror)) complete
{
#if OCTAGON
    CFErrorRef cferror = NULL;
    if([self clientHasBooleanEntitlement: (__bridge NSString*) kSecEntitlementKeychainDeny]) {
        SecError(errSecNotAvailable, &cferror, CFSTR("SecItemFetchCurrentItemAcrossAllDevices: %@ has entitlement %@"), _client.task, kSecEntitlementKeychainDeny);
        complete(NULL, (__bridge NSError*) cferror);
        CFReleaseNull(cferror);
        return;
    }

    if(![self clientHasBooleanEntitlement: (__bridge NSString*) kSecEntitlementPrivateCKKSReadCurrentItemPointers]) {
        SecError(errSecNotAvailable, &cferror, CFSTR("SecItemFetchCurrentItemAcrossAllDevices: %@ does not have entitlement %@"), _client.task, kSecEntitlementPrivateCKKSReadCurrentItemPointers);
        complete(NULL, (__bridge NSError*) cferror);
        CFReleaseNull(cferror);
        return;
    }

    if (!accessGroupsAllows(self->_client.accessGroups, (__bridge CFStringRef)accessGroup, &_client)) {
        SecError(errSecMissingEntitlement, &cferror, CFSTR("SecItemFetchCurrentItemAcrossAllDevices: client is missing access-group %@: %@"), accessGroup, _client.task);
        complete(NULL, (__bridge NSError*)cferror);
        CFReleaseNull(cferror);
        return;
    }

    [[CKKSViewManager manager] getCurrentItemForAccessGroup:accessGroup
                                                 identifier:identifier
                                                   viewHint:viewHint
                                            fetchCloudValue:fetchCloudValue
                                                   complete:^(NSString* uuid, NSError* error) {
                                                       if(error || !uuid) {
                                                           complete(NULL, error);
                                                           return;
                                                       }

                                                       // Find the persisent ref and return it.
                                                       [self findItemPersistentRefByUUID:uuid complete:complete];
                                                   }];
#else // ! OCTAGON
    complete(NULL, [NSError errorWithDomain:@"securityd" code:errSecParam userInfo:@{NSLocalizedDescriptionKey: @"SecItemFetchCurrentItemAcrossAllDevices not implemented on this platform"}]);
#endif // OCTAGON
}

-(void)findItemPersistentRefByUUID:(NSString*)uuid
                          complete:(void (^) (NSData* persistentref, NSError* operror)) complete
{
    CFErrorRef cferror = NULL;
    CFTypeRef result = NULL;

    // Must query per-class, so:
    const SecDbSchema *newSchema = current_schema();
    for (const SecDbClass *const *class = newSchema->classes; *class != NULL; class++) {
        CFReleaseNull(result);
        CFReleaseNull(cferror);

        if(!((*class)->itemclass)) {
            //Don't try to search non-item 'classes'
            continue;
        }

        _SecItemCopyMatching((__bridge CFDictionaryRef) @{
                                                          (__bridge NSString*) kSecClass: (__bridge NSString*) (*class)->name,
                                                          (id)kSecAttrSynchronizable: (id)kSecAttrSynchronizableAny,
                                                          (id)kSecMatchLimit : (id)kSecMatchLimitOne,
                                                          (id)kSecAttrUUID: uuid,
                                                          (id)kSecReturnPersistentRef: @YES,
                                                          },
                             &self->_client,
                             &result,
                             &cferror);

        if(cferror && CFErrorGetCode(cferror) != errSecItemNotFound) {
            break;
        }

        if(result) {
            // Found the persistent ref! Quit searching.
            break;
        }
    }

    complete((__bridge NSData*) result, (__bridge NSError*) cferror);
    CFReleaseNull(result);
    CFReleaseNull(cferror);
}

- (void) secItemDigest:(NSString *)itemClass
           accessGroup:(NSString *)accessGroup
              complete:(void (^)(NSArray *digest, NSError* error))complete
{
    CFArrayRef accessGroups = self->_client.accessGroups;
    __block CFErrorRef cferror = NULL;
    __block CFArrayRef result = NULL;

    if (itemClass == NULL || accessGroup == NULL) {
        SecError(errSecParam, &cferror, CFSTR("parameter missing: %@"), _client.task);
        complete(NULL, (__bridge NSError*) cferror);
        CFReleaseNull(cferror);
        return;
    }

    if (![itemClass isEqualToString:@"inet"] && ![itemClass isEqualToString:@"genp"]) {
        SecError(errSecParam, &cferror, CFSTR("class %@ is not supported: %@"), itemClass, _client.task);
        complete(NULL, (__bridge NSError*) cferror);
        CFReleaseNull(cferror);
        return;
    }

    if (!accessGroupsAllows(accessGroups, (__bridge CFStringRef)accessGroup, &_client)) {
        SecError(errSecMissingEntitlement, &cferror, CFSTR("Client is missing access-group %@: %@"), accessGroup, _client.task);
        complete(NULL, (__bridge NSError*) cferror);
        CFReleaseNull(cferror);
        return;
    }

    if (CFArrayContainsValue(accessGroups, CFRangeMake(0, CFArrayGetCount(accessGroups)), CFSTR("*"))) {
        /* Having the special accessGroup "*" allows access to all accessGroups. */
        accessGroups = NULL;
    }

    NSDictionary *attributes  = @{
       (__bridge NSString *)kSecClass : itemClass,
       (__bridge NSString *)kSecAttrAccessGroup : accessGroup,
       (__bridge NSString *)kSecAttrSynchronizable : (__bridge NSString *)kSecAttrSynchronizableAny,
    };

    Query *q = query_create_with_limit((__bridge CFDictionaryRef)attributes, _client.musr, 0, &cferror);
    if (q == NULL) {
        SecError(errSecParam, &cferror, CFSTR("failed to build query: %@"), _client.task);
        complete(NULL, (__bridge NSError*) cferror);
        CFReleaseNull(cferror);
        return;
    }

    bool ok = kc_with_dbt(false, &cferror, ^(SecDbConnectionRef dbt) {
        return (bool)s3dl_copy_digest(dbt, q, &result, accessGroups, &cferror);
    });

    (void)ok;

    complete((__bridge NSArray *)result, (__bridge NSError *)cferror);

    (void)query_destroy(q, &cferror);

    CFReleaseNull(result);
    CFReleaseNull(cferror);
}


@end
