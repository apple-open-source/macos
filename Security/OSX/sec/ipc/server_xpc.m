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
#include <os/transaction_private.h>

#if (defined(TARGET_DARWINOS) && TARGET_DARWINOS) || (defined(SECURITYD_SYSTEM) && SECURITYD_SYSTEM)
#undef OCTAGON
#undef SECUREOBJECTSYNC
#undef SHAREDWEBCREDENTIALS
#endif

#if OCTAGON
#import "keychain/categories/NSError+UsefulConstructors.h"
#include <CloudKit/CloudKit_Private.h>
// If your callbacks might pass back a CK error, you should use the XPCSanitizeError() spi on all branches at this layer.
// Otherwise, XPC might crash on the other side if they haven't linked CloudKit.framework.
#define XPCSanitizeError CKXPCSuitableError
#else
// This is a no-op: XPCSanitizeError(error) turns into (error)
#define XPCSanitizeError
#endif // OCTAGON

#include <Security/SecEntitlements.h>
#include <Security/SecItemPriv.h>
#include <Security/SecItemFetchOutOfBandPriv.h>
#include "keychain/securityd/SecItemServer.h"
#include "keychain/securityd/SecItemSchema.h"
#include "keychain/securityd/SecItemDb.h"

#include "keychain/ckks/CKKSViewManager.h"

#import "keychain/ot/OTConstants.h"
#import "keychain/ot/Affordance_OTConstants.h"

@interface SecOSTransactionHolder : NSObject
@property os_transaction_t transaction;
- (instancetype)init:(os_transaction_t)transaction;
@end

@implementation SecOSTransactionHolder
- (instancetype)init:(os_transaction_t)transaction {
    if((self = [super init])) {
        _transaction = transaction;
    }
    return self;
}
@end

@implementation SecuritydXPCServer (SecuritydXPCProtocol)

- (void) SecItemAddAndNotifyOnSync:(NSDictionary*) attributes
                      syncCallback:(id<SecuritydXPCCallbackProtocol>) callback
                          complete:(void (^) (NSDictionary* opDictResult, NSArray* opArrayResult, NSError* operror))xpcComplete
{
    // The calling client might not handle CK types well. Sanitize!
    void (^complete)(NSDictionary*, NSArray*, NSError*) = ^(NSDictionary* opDictResult, NSArray* opArrayResult, NSError* operror){
        xpcComplete(opDictResult, opArrayResult, XPCSanitizeError(operror));
    };

    CFErrorRef cferror = NULL;
    if([self clientHasBooleanEntitlement: (__bridge NSString*) kSecEntitlementKeychainDeny]) {
        SecError(errSecNotAvailable, &cferror, CFSTR("SecItemAddAndNotifyOnSync: %@ has entitlement %@"), _client.task, kSecEntitlementKeychainDeny);
        //TODO: ensure cferror can transit xpc
        complete(NULL, NULL, (__bridge NSError*) cferror);
        CFReleaseNull(cferror);
        return;
    }

#if OCTAGON
    // Wait a bit for CKKS initialization in case of daemon start, but don't bail if it isn't up
    [[CKKSViewManager manager].completedSecCKKSInitialize wait:10];
#endif

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

    if(attributes[(id)kSecDataInetExtraNotes] ||
       attributes[(id)kSecDataInetExtraHistory] ||
       attributes[(id)kSecDataInetExtraClientDefined0] ||
       attributes[(id)kSecDataInetExtraClientDefined1] ||
       attributes[(id)kSecDataInetExtraClientDefined2] ||
       attributes[(id)kSecDataInetExtraClientDefined3]) {
        if(![self clientHasBooleanEntitlement:(__bridge NSString*)kSecEntitlementPrivateInetExpansionFields]) {
              SecError(errSecMissingEntitlement, &cferror, CFSTR("SecItemAddAndNotifyOnSync: %@ does not have entitlement %@"), _client.task, kSecEntitlementPrivateInetExpansionFields);
              complete(NULL, NULL, (__bridge NSError*) cferror);
              CFReleaseNull(cferror);
              return;
          }
    }

    CFTypeRef cfresult = NULL;

    NSMutableDictionary* callbackQuery = [attributes mutableCopy];

    // We probably need to figure out how to call os_transaction_needs_more_time on this transaction, but as this callback passes through C code, it's quite difficult
    SecOSTransactionHolder* callbackTransaction = [[SecOSTransactionHolder alloc] init:os_transaction_create("com.apple.securityd.SecItemAddAndNotifyOnSync-callback")];
    callbackQuery[@"f_ckkscallback"] = ^void (bool didSync, CFErrorRef syncerror) {
        [callback callCallback:didSync error:XPCSanitizeError((__bridge NSError*)syncerror)];
        callbackTransaction.transaction = nil;
    };

    SecurityClient client = {};

    if (OctagonSupportsPersonaMultiuser()) {
        SecSecurityFixUpClientWithPersona(&self->_client, &client);
    } else {
        client = self->_client;
    }
    _SecItemAdd((__bridge CFDictionaryRef) callbackQuery, &client, &cfresult, &cferror);

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
    if (OctagonSupportsPersonaMultiuser()) {
        CFReleaseNull(client.musr);
    }
}

- (void)secItemSetCurrentItemAcrossAllDevices:(NSData* _Nonnull)newItemPersistentRef
                           newCurrentItemHash:(NSData* _Nonnull)newItemSHA1
                                  accessGroup:(NSString* _Nonnull)accessGroup
                                   identifier:(NSString* _Nonnull)identifier
                                     viewHint:(NSString* _Nonnull)viewHint
                      oldCurrentItemReference:(NSData* _Nullable)oldCurrentItemPersistentRef
                           oldCurrentItemHash:(NSData* _Nullable)oldItemSHA1
                                     complete:(void (^) (NSError* _Nullable operror))xpcComplete
{
#if OCTAGON
    // The calling client might not handle CK types well. Sanitize!
    void (^complete)(NSError*) = ^(NSError* error){
        xpcComplete(XPCSanitizeError(error));
    };

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

#if OCTAGON
    // Wait a bit for CKKS initialization in case of daemon start, and bail it doesn't come up
    if([[CKKSViewManager manager].completedSecCKKSInitialize wait:10] != 0) {
        secerror("SecItemSetCurrentItemAcrossAllDevices: CKKSViewManager not initialized?");
        complete([NSError errorWithDomain:CKKSErrorDomain code:CKKSNotInitialized description:@"CKKS not yet initialized"]);
        return;
    }
#endif

    CKKSViewManager* manager = [CKKSViewManager manager];
    if(!manager) {
        secerror("SecItemSetCurrentItemAcrossAllDevices: no view manager?");
        complete([NSError errorWithDomain:CKKSErrorDomain
                                     code:CKKSNotInitialized
                              description:@"No view manager, cannot forward request"]);
        return;
    }

    [manager setCurrentItemForAccessGroup:newItemPersistentRef
                                     hash:newItemSHA1
                              accessGroup:accessGroup
                               identifier:identifier
                                 viewHint:viewHint
                                replacing:oldCurrentItemPersistentRef
                                     hash:oldItemSHA1
                                 complete:complete];
    return;
#else // ! OCTAGON
    xpcComplete([NSError errorWithDomain:@"securityd" code:errSecParam userInfo:@{NSLocalizedDescriptionKey: @"SecItemSetCurrentItemAcrossAllDevices not implemented on this platform"}]);
#endif // OCTAGON
}

- (void)secItemUnsetCurrentItemsAcrossAllDevices:(NSString*)accessGroup
                                      identifiers:(NSArray<NSString*>*)identifiers
                                         viewHint:(NSString*)viewHint
                                         complete:(void (^)(NSError* operror))xpcComplete
{
#if OCTAGON
    // The calling client might not handle CK types well. Sanitize!
    void (^complete)(NSError*) = ^(NSError* error){
        xpcComplete(XPCSanitizeError(error));
    };

    __block CFErrorRef cferror = NULL;
    if([self clientHasBooleanEntitlement: (__bridge NSString*) kSecEntitlementKeychainDeny]) {
        SecError(errSecNotAvailable, &cferror, CFSTR("SecItemUnsetCurrentItemsAcrossAllDevices: %@ has entitlement %@"), _client.task, kSecEntitlementKeychainDeny);
        complete((__bridge NSError*) cferror);
        CFReleaseNull(cferror);
        return;
    }

    if(![self clientHasBooleanEntitlement: (__bridge NSString*) kSecEntitlementPrivateCKKSWriteCurrentItemPointers]) {
        SecError(errSecMissingEntitlement, &cferror, CFSTR("SecItemUnsetCurrentItemsAcrossAllDevices: %@ does not have entitlement %@"), _client.task, kSecEntitlementPrivateCKKSWriteCurrentItemPointers);
        complete((__bridge NSError*) cferror);
        CFReleaseNull(cferror);
        return;
    }

    if (!accessGroupsAllows(self->_client.accessGroups, (__bridge CFStringRef)accessGroup, &_client)) {
        SecError(errSecMissingEntitlement, &cferror, CFSTR("SecItemUnsetCurrentItemsAcrossAllDevices: client is missing access-group %@: %@"), accessGroup, _client.task);
        complete((__bridge NSError*)cferror);
        CFReleaseNull(cferror);
        return;
    }

    CKKSViewManager* manager = [CKKSViewManager manager];
    if(!manager) {
        secerror("SecItemUnsetCurrentItemsAcrossAllDevices: no view manager?");
        complete([NSError errorWithDomain:CKKSErrorDomain
                                     code:CKKSNotInitialized
                              description:@"No view manager, cannot forward request"]);
        return;
    }

    // Wait a bit for CKKS initialization in case of daemon start, and bail it doesn't come up
    if([manager.completedSecCKKSInitialize wait:10] != 0) {
        secerror("SecItemUnsetCurrentItemsAcrossAllDevices: CKKSViewManager not initialized?");
        complete([NSError errorWithDomain:CKKSErrorDomain code:CKKSNotInitialized description:@"CKKS not yet initialized"]);
        return;
    }

    [manager unsetCurrentItemsForAccessGroup:accessGroup
                                 identifiers:identifiers
                                    viewHint:viewHint
                                    complete:complete];
    return;
#else // ! OCTAGON
    xpcComplete([NSError errorWithDomain:@"securityd" code:errSecParam userInfo:@{NSLocalizedDescriptionKey: @"SecItemUnsetCurrentItemsAcrossAllDevices not implemented on this platform"}]);
#endif // OCTAGON
}

-(void)secItemFetchCurrentItemAcrossAllDevices:(NSString*)accessGroup
                                    identifier:(NSString*)identifier
                                      viewHint:(NSString*)viewHint
                               fetchCloudValue:(bool)fetchCloudValue
                                      complete:(void (^) (NSData* persistentref, NSDate *cipModificationTime, NSError* operror))xpcComplete
{
#if OCTAGON
    // The calling client might not handle CK types well. Sanitize!
    void (^complete)(NSData*, NSDate*, NSError*) = ^(NSData* persistentref, NSDate* cipModificationTime, NSError* error){
        xpcComplete(persistentref, cipModificationTime, XPCSanitizeError(error));
    };

    CFErrorRef cferror = NULL;
    if([self clientHasBooleanEntitlement: (__bridge NSString*) kSecEntitlementKeychainDeny]) {
        SecError(errSecNotAvailable, &cferror, CFSTR("SecItemFetchCurrentItemAcrossAllDevices: %@ has entitlement %@"), _client.task, kSecEntitlementKeychainDeny);
        complete(NULL, NULL, (__bridge NSError*) cferror);
        CFReleaseNull(cferror);
        return;
    }

    if(![self clientHasBooleanEntitlement: (__bridge NSString*) kSecEntitlementPrivateCKKSReadCurrentItemPointers]) {
        SecError(errSecNotAvailable, &cferror, CFSTR("SecItemFetchCurrentItemAcrossAllDevices: %@ does not have entitlement %@"), _client.task, kSecEntitlementPrivateCKKSReadCurrentItemPointers);
        complete(NULL, NULL, (__bridge NSError*) cferror);
        CFReleaseNull(cferror);
        return;
    }

    if (!accessGroupsAllows(self->_client.accessGroups, (__bridge CFStringRef)accessGroup, &_client)) {
        SecError(errSecMissingEntitlement, &cferror, CFSTR("SecItemFetchCurrentItemAcrossAllDevices: client is missing access-group %@: %@"), accessGroup, _client.task);
        complete(NULL, NULL, (__bridge NSError*)cferror);
        CFReleaseNull(cferror);
        return;
    }

    // Wait a bit for CKKS initialization in case of daemon start, and bail it doesn't come up
    if([[CKKSViewManager manager].completedSecCKKSInitialize wait:10] != 0) {
        secerror("SecItemFetchCurrentItemAcrossAllDevices: CKKSViewManager not initialized?");
        complete(NULL, NULL, [NSError errorWithDomain:CKKSErrorDomain code:CKKSNotInitialized description:@"CKKS not yet initialized"]);
        return;
    }

    SecurityClient* client = nil;
    if (OctagonSupportsPersonaMultiuser()) {
        client = malloc(sizeof(struct SecurityClient));
        SecSecurityFixUpClientWithPersona(&_client, client);
    } else {
        client = &self->_client;
    }

    [[CKKSViewManager manager] getCurrentItemForAccessGroup:accessGroup
                                                 identifier:identifier
                                                   viewHint:viewHint
                                            fetchCloudValue:fetchCloudValue
                                                   complete:^(CKKSCurrentItemData* data, NSError* error) {
                                                       if(error || !data) {
                                                           secnotice("ckkscurrent", "CKKS didn't find a current item for (%@,%@): %@ %@", accessGroup, identifier, data.uuid, error);
                                                           complete(NULL, NULL, error);
                                                           if (OctagonSupportsPersonaMultiuser()) {
                                                               CFReleaseNull(client->musr);
                                                               free(client);
                                                           }
                                                           return;
                                                       }

                                                       // Find the persistent ref and return it.
                                                       secinfo("ckkscurrent", "CKKS believes current item UUID for (%@,%@) is %@. Looking up persistent ref...", accessGroup, identifier, data.uuid);
                                                       [self findItemPersistentRefByUUID:data.uuid
                                                                      extraLoggingString:[NSString stringWithFormat:@"%@,%@", accessGroup, identifier]
                                                                                  client:client
                                                                                complete:^(NSData *persistentref, NSError *operror) {

                                                           complete(persistentref, data.modificationDate, operror);
                                                           if (OctagonSupportsPersonaMultiuser()) {
                                                               CFReleaseNull(client->musr);
                                                               free(client);
                                                           }
                                                       }];
                                                   }];
#else // ! OCTAGON
    xpcComplete(NULL, NULL, [NSError errorWithDomain:@"securityd" code:errSecParam userInfo:@{NSLocalizedDescriptionKey: @"SecItemFetchCurrentItemAcrossAllDevices not implemented on this platform"}]);
#endif // OCTAGON
}

- (void)secItemFetchCurrentItemOutOfBand:(NSArray<CKKSCurrentItemQuery*>*)currentItemQueries
                              forceFetch:(bool)forceFetch
                                complete:(void (^)(NSArray<CKKSCurrentItemQueryResult*>* currentItems, NSError* operror))xpcComplete
{
#if OCTAGON
    // Sanitization
    void (^complete)(NSArray<CKKSCurrentItemQueryResult*>*, NSError*) = ^(NSArray<CKKSCurrentItemQueryResult*>* currentItems, NSError* error) {
        xpcComplete(currentItems, XPCSanitizeError(error));
    };

    CFErrorRef cferror = NULL;
    if([self clientHasBooleanEntitlement: (__bridge NSString*) kSecEntitlementKeychainDeny]) {
        SecError(errSecNotAvailable, &cferror, CFSTR("secItemFetchCurrentItemOutOfBand: %@ has entitlement %@"), _client.task, kSecEntitlementKeychainDeny);
        complete(NULL, (__bridge NSError*) cferror);
        CFReleaseNull(cferror);
        return;
    }

    if(![self clientHasBooleanEntitlement: (__bridge NSString*) kSecEntitlementPrivateCKKSReadCurrentItemPointers]) {
        SecError(errSecNotAvailable, &cferror, CFSTR("secItemFetchCurrentItemOutOfBand: %@ does not have entitlement %@"), _client.task, kSecEntitlementPrivateCKKSReadCurrentItemPointers);
        complete(NULL, (__bridge NSError*) cferror);
        CFReleaseNull(cferror);
        return;
    }

    NSString* accessGroup = currentItemQueries.count > 0 ? currentItemQueries[0].accessGroup : nil;
    if (accessGroup && !accessGroupsAllows(self->_client.accessGroups, (__bridge CFStringRef)accessGroup, &_client)) {
        SecError(errSecMissingEntitlement, &cferror, CFSTR("secItemFetchCurrentItemOutOfBand: client is missing access-group %@: %@"), accessGroup, _client.task);
        complete(NULL, (__bridge NSError*)cferror);
        CFReleaseNull(cferror);
        return;
    }

    // Wait a bit for CKKS initialization in case of daemon start, and bail it doesn't come up
    if([[CKKSViewManager manager].completedSecCKKSInitialize wait:10] != 0) {
        secerror("secItemFetchCurrentItemOutOfBand: CKKSViewManager not initialized?");
        complete(NULL, [NSError errorWithDomain:CKKSErrorDomain code:CKKSNotInitialized description:@"CKKS not yet initialized"]);
        return;
    }

    SecurityClient* client = nil;
    if (OctagonSupportsPersonaMultiuser()) {
        client = malloc(sizeof(struct SecurityClient));
        SecSecurityFixUpClientWithPersona(&_client, client);
    } else {
        client = &self->_client;
    }

    [[CKKSViewManager manager] getCurrentItemOutOfBand:currentItemQueries
                                            forceFetch:forceFetch
                                              complete:^(NSArray<CKKSCurrentItemQueryResult*>* data, NSError* error) {
        if (error || !data) {
            secnotice("ckkscurrent", "CKKS didn't find a current item for (%@): %@", currentItemQueries, error);
            complete(NULL, error);
        } else {
            complete(data, error);
        }
        
        if (OctagonSupportsPersonaMultiuser()) {
            CFReleaseNull(client->musr);
            free(client);
        }
    }];
    
#else // ! OCTAGON
    xpcComplete(NULL, [NSError errorWithDomain:@"securityd" code:errSecParam userInfo:@{NSLocalizedDescriptionKey: @"secItemFetchCurrentItemOutOfBand not implemented on this platform"}]);
#endif // OCTAGON
    
}

- (void)secItemFetchPCSIdentityByKeyOutOfBand:(NSArray<CKKSPCSIdentityQuery*>*)pcsIdentityQueries
                                   forceFetch:(bool)forceFetch
                                     complete:(void (^)(NSArray<CKKSPCSIdentityQueryResult*>* pcsIdentities, NSError* operror))xpcComplete
{
#if OCTAGON
    // Sanitization
    void (^complete)(NSArray<CKKSPCSIdentityQueryResult*>*, NSError*) = ^(NSArray<CKKSPCSIdentityQueryResult*>* pcsIdentities, NSError* error) {
        xpcComplete(pcsIdentities, XPCSanitizeError(error));
    };

    CFErrorRef cferror = NULL;
    if([self clientHasBooleanEntitlement: (__bridge NSString*) kSecEntitlementKeychainDeny]) {
        SecError(errSecNotAvailable, &cferror, CFSTR("secItemFetchPCSIdentityByKeyOutOfBand: %@ has entitlement %@"), _client.task, kSecEntitlementKeychainDeny);
        complete(NULL, (__bridge NSError*) cferror);
        CFReleaseNull(cferror);
        return;
    }

    NSString* accessGroup = pcsIdentityQueries.count > 0 ? pcsIdentityQueries[0].accessGroup : nil;
    if (accessGroup && !accessGroupsAllows(self->_client.accessGroups, (__bridge CFStringRef)accessGroup, &_client)) {
        SecError(errSecMissingEntitlement, &cferror, CFSTR("secItemFetchPCSIdentityByKeyOutOfBand: client is missing access-group %@: %@"), accessGroup, _client.task);
        complete(NULL, (__bridge NSError*)cferror);
        CFReleaseNull(cferror);
        return;
    }

    // Wait a bit for CKKS initialization in case of daemon start, and bail it doesn't come up
    if([[CKKSViewManager manager].completedSecCKKSInitialize wait:10] != 0) {
        secerror("secItemFetchPCSIdentityByKeyOutOfBand: CKKSViewManager not initialized?");
        complete(NULL, [NSError errorWithDomain:CKKSErrorDomain code:CKKSNotInitialized description:@"CKKS not yet initialized"]);
        return;
    }

    SecurityClient* client = nil;
    if (OctagonSupportsPersonaMultiuser()) {
        client = malloc(sizeof(struct SecurityClient));
        SecSecurityFixUpClientWithPersona(&_client, client);
    } else {
        client = &self->_client;
    }

    [[CKKSViewManager manager] fetchPCSIdentityOutOfBand:pcsIdentityQueries
                                              forceFetch:forceFetch
                                                complete:^(NSArray<CKKSPCSIdentityQueryResult*>* data, NSError* error) {
        if (error || !data) {
            secnotice("ckkscurrent", "CKKS didn't find a PCS Identity for (%@): %@", pcsIdentityQueries, error);
            complete(NULL, error);
        } else {
            complete(data, error);
        }
        
        if (OctagonSupportsPersonaMultiuser()) {
            CFReleaseNull(client->musr);
            free(client);
        }
    }];
    
#else // ! OCTAGON
    xpcComplete(NULL, [NSError errorWithDomain:@"securityd" code:errSecParam userInfo:@{NSLocalizedDescriptionKey: @"secItemFetchPCSIdentityByKeyOutOfBand not implemented on this platform"}]);
#endif // OCTAGON
    
}

-(void)findItemPersistentRefByUUID:(NSString*)uuid
                extraLoggingString:(NSString*)loggingStr
                            client:(SecurityClient*)client
                          complete:(void (^) (NSData* persistentref, NSError* operror))xpcComplete
{
    // The calling client might not handle CK types well. Sanitize!
    void (^complete)(NSData*, NSError*) = ^(NSData* persistentref, NSError* error){
        xpcComplete(persistentref, XPCSanitizeError(error));
    };

    CFErrorRef cferror = NULL;
    CFTypeRef result = NULL;

    // Must query per-class, so:
    const SecDbSchema *newSchema = current_schema();
    for (const SecDbClass *const *class = newSchema->classes; *class != NULL; class++) {
        if(!((*class)->itemclass)) {
            //Don't try to search non-item 'classes'
            continue;
        }

        // Now that we're in an item class, reset any errSecItemNotFound errors from the last item class
        CFReleaseNull(result);
        CFReleaseNull(cferror);

        _SecItemCopyMatching((__bridge CFDictionaryRef) @{
                                                          (__bridge NSString*) kSecClass: (__bridge NSString*) (*class)->name,
                                                          (id)kSecAttrSynchronizable: (id)kSecAttrSynchronizableAny,
                                                          (id)kSecMatchLimit : (id)kSecMatchLimitOne,
                                                          (id)kSecAttrUUID: uuid,
                                                          (id)kSecReturnPersistentRef: @YES,
                                                          },
                             client,
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

    if(result && !cferror) {
        secinfo("ckkscurrent", "Found current item for (%@: %@)", loggingStr, uuid);
    } else {
        secerror("ckkscurrent: No current item for (%@,%@): %@ %@", loggingStr, uuid, result, cferror);
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

    NSDictionary *attributes = NULL;
    if (SecKeychainIsStaticPersistentRefsEnabled()) {
        //<rdar://problem/72194459> Fix Copy Digest UUID Persistent Refs
        attributes = @{
            (__bridge NSString *)kSecClass : itemClass,
            (__bridge NSString *)kSecAttrAccessGroup : accessGroup,
            (__bridge NSString *)kSecAttrSynchronizable : (__bridge NSString *)kSecAttrSynchronizableAny,
            (__bridge NSString *)kSecReturnPersistentRef : @YES,
        };
    } else {
        attributes = @{
            (__bridge NSString *)kSecClass : itemClass,
            (__bridge NSString *)kSecAttrAccessGroup : accessGroup,
            (__bridge NSString *)kSecAttrSynchronizable : (__bridge NSString *)kSecAttrSynchronizableAny,
        };
    }

    SecurityClient client = {};
    if (OctagonSupportsPersonaMultiuser()) {
        SecSecurityFixUpClientWithPersona(&self->_client, &client);
    } else {
        client = self->_client;
    }
    
    Query *q = query_create_with_limit((__bridge CFDictionaryRef)attributes, client.musr, 0, &(client), &cferror);
    if (q == NULL) {
        SecError(errSecParam, &cferror, CFSTR("failed to build query: %@"), client.task);
        complete(NULL, (__bridge NSError*) cferror);
        CFReleaseNull(cferror);
        if (OctagonSupportsPersonaMultiuser()) {
            CFReleaseNull(client.musr);
        }
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
    if (OctagonSupportsPersonaMultiuser()) {
        CFReleaseNull(client.musr);
    }
}


- (void) secKeychainDeleteMultiuser:(NSData *)uuid
                           complete:(void(^)(bool status, NSError* error))complete
{
    __block CFErrorRef cferror = NULL;

#define SKDMUEntitlement @"com.apple.keychain.multiuser-admin"

    if([self clientHasBooleanEntitlement: SKDMUEntitlement]) {
        SecError(errSecNotAvailable, &cferror, CFSTR("secKeychainDeleteMultiuser: %@ need entitlement %@"), _client.task, SKDMUEntitlement);
        complete(false, (__bridge NSError *)cferror);
        CFReleaseNull(cferror);
        return;
    }
    if ([uuid length] != 16) {
        SecError(errSecNotAvailable, &cferror, CFSTR("secKeychainDeleteMultiuser: %@ uuid have wrong length: %d"), _client.task, (int)[uuid length]);
        complete(false, (__bridge NSError *)cferror);
        CFReleaseNull(cferror);
        return;

    }

#if KEYCHAIN_SUPPORTS_PERSONA_MULTIUSER
    bool status = kc_with_dbt(true, &cferror, ^(SecDbConnectionRef dbt) {
        return SecServerDeleteAllForUser(dbt, (__bridge CFDataRef)uuid, false, &cferror);
    });
#else
    bool status = false;
#endif

    complete(status, (__bridge NSError *)cferror);
    CFReleaseNull(cferror);
}


- (void)secItemDeleteForAppClipApplicationIdentifier:(NSString*)identifier
                                              completion:(void (^)(OSStatus))completion
{
    if (![self clientHasBooleanEntitlement:(__bridge NSString*)kSecEntitlementPrivateAppClipDeletion]) {
        completion(errSecMissingEntitlement);
        return;
    }

    completion(SecServerDeleteForAppClipApplicationIdentifier((__bridge CFStringRef)identifier));
}


- (void)secItemPersistKeychainWritesAtHighPerformanceCost:(void (^)(OSStatus status, NSError* error))completion
{
    if (![self clientHasBooleanEntitlement:(__bridge NSString*)kSecEntitlementPrivatePerformanceImpactingAPI]) {
        completion(errSecMissingEntitlement, [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecMissingEntitlement userInfo:nil]);
        return;
    }

    __block CFErrorRef cferror = NULL;
    secnotice("item", "Performing keychain database checkpoint");

    bool status = kc_with_dbt(true, &cferror, ^bool(SecDbConnectionRef dbt) {
        return SecDbCheckpoint(dbt, &cferror);
    });

    if(!status) {
        secerror("item: keychain database checkpoint failed: %@", cferror);
    } else {
        secnotice("item", "Keychain database checkpoint succeeded");
    }

    completion(status ? errSecSuccess : errSecInternal, (__bridge NSError*)cferror);

    CFReleaseNull(cferror);
}

- (void)secItemPromoteItemsForAppClip:(NSString*)appClipAppID
                          toParentApp:(NSString*)parentAppID
                           completion:(void (^)(OSStatus))completion
{
    if (![self clientHasBooleanEntitlement:(__bridge NSString*)kSecEntitlementPrivateAppClipDeletion]) {
        completion(errSecMissingEntitlement);
        return;
    }

    completion(SecServerPromoteAppClipItemsToParentApp((__bridge CFStringRef)appClipAppID, (__bridge CFStringRef)parentAppID));
}

- (void)secKeychainForceUpgradeIfNeeded:(void (^)(OSStatus))completion
{
    __block CFErrorRef error = NULL;
    secnotice("secKeychainForceUpgradeIfNeeded", "Performing keychain database upgrade if needed");

    bool status = kc_with_dbt(false, &error, ^bool(SecDbConnectionRef dbt) {
        // no-op will do the needful
        return true;
    });

    if (!status) {
        secerror("secKeychainForceUpgradeIfNeeded: failed: %@", error);
    } else {
        secnotice("secKeychainForceUpgradeIfNeeded", "succeeded");
    }
    completion(status ? errSecSuccess : errSecInternal);
}

@end
