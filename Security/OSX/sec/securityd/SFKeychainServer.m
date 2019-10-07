/*
 * Copyright (c) 2017 Apple Inc. All Rights Reserved.
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

#import "SFKeychainServer.h"
#import <TargetConditionals.h>

#if !TARGET_OS_BRIDGE
#if __OBJC2__

#import "SecCDKeychain.h"
#import "SecFileLocations.h"
#import "debugging.h"
#import "CloudKitCategories.h"
#import "SecAKSWrappers.h"
#include "securityd_client.h"
#import "server_entitlement_helpers.h"
#import "SecTask.h"
#import "keychain/categories/NSError+UsefulConstructors.h"
#import "SecEntitlements.h"
#import <SecurityFoundation/SFKeychain.h>
#import <SecurityFoundation/SFCredential_Private.h>
#import <SecurityFoundation/SFCredentialStore_Private.h>
#import <Foundation/NSKeyedArchiver_Private.h>
#import <Foundation/NSXPCConnection_Private.h>

static NSString* const SFKeychainItemAttributeLocalizedLabel = @"label";
static NSString* const SFKeychainItemAttributeLocalizedDescription = @"description";

static NSString* const SFCredentialAttributeUsername = @"username";
static NSString* const SFCredentialAttributePrimaryServiceIdentifier = @"primaryServiceID";
static NSString* const SFCredentialAttributeSupplementaryServiceIdentifiers = @"supplementaryServiceIDs";
static NSString* const SFCredentialAttributeCreationDate = @"creationDate";
static NSString* const SFCredentialAttributeModificationDate = @"modificationDate";
static NSString* const SFCredentialAttributeCustom = @"customAttributes";
static NSString* const SFCredentialSecretPassword = @"password";

@interface SFCredential (securityd_only)

- (instancetype)_initWithUsername:(NSString*)username primaryServiceIdentifier:(SFServiceIdentifier*)primaryServiceIdentifier supplementaryServiceIdentifiers:(nullable NSArray<SFServiceIdentifier*>*)supplementaryServiceIdentifiers;

@end

@interface SFKeychainServerConnection ()

- (instancetype)initWithKeychain:(SecCDKeychain*)keychain xpcConnection:(NSXPCConnection*)connection;

@end

@implementation SecCDKeychainItemTypeCredential

+ (instancetype)itemType
{
    static SecCDKeychainItemTypeCredential* itemType = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        itemType = [[self alloc] _initWithName:@"Credential" version:1 primaryKeys:@[SFCredentialAttributeUsername, SFCredentialAttributePrimaryServiceIdentifier] syncableKeys:nil];
    });

    return itemType;
}

@end

@implementation SFKeychainServer {
    SecCDKeychain* _keychain;
}

- (instancetype)initWithStorageURL:(NSURL*)persistentStoreURL modelURL:(NSURL*)managedObjectURL encryptDatabase:(bool)encryptDatabase
{
    if (self = [super init]) {
        _keychain = [[SecCDKeychain alloc] initWithStorageURL:persistentStoreURL modelURL:managedObjectURL encryptDatabase:encryptDatabase];
    }

    return self;
}

- (BOOL)listener:(NSXPCListener*)listener shouldAcceptNewConnection:(NSXPCConnection*)newConnection
{
    NSNumber* keychainDenyEntitlement = [newConnection valueForEntitlement:(__bridge NSString*)kSecEntitlementKeychainDeny];
    if ([keychainDenyEntitlement isKindOfClass:[NSNumber class]] && keychainDenyEntitlement.boolValue == YES) {
        secerror("SFKeychainServer: connection denied due to entitlement %@", kSecEntitlementKeychainDeny);
        return NO;
    }

    // wait a bit for shared function from SecurityFoundation to get to SDK, then addopt that
    NSXPCInterface* interface = [NSXPCInterface interfaceWithProtocol:@protocol(SFKeychainServerProtocol)];
    [interface setClasses:[NSSet setWithObjects:[NSArray class], [SFServiceIdentifier class], nil] forSelector:@selector(rpcLookupCredentialsForServiceIdentifiers:reply:) argumentIndex:0 ofReply:NO];
    [interface setClasses:[NSSet setWithObjects:[NSArray class], [SFPasswordCredential class], nil] forSelector:@selector(rpcLookupCredentialsForServiceIdentifiers:reply:) argumentIndex:0 ofReply:YES];
    newConnection.exportedInterface = interface;
    newConnection.exportedObject = [[SFKeychainServerConnection alloc] initWithKeychain:_keychain xpcConnection:newConnection];
    [newConnection resume];
    return YES;
}

- (SecCDKeychain*)_keychain
{
    return _keychain;
}

@end

@implementation SFKeychainServerConnection {
    SecCDKeychain* _keychain;
    NSArray* _clientAccessGroups;
}

@synthesize clientAccessGroups = _clientAccessGroups;

- (instancetype)initWithKeychain:(SecCDKeychain*)keychain xpcConnection:(NSXPCConnection*)connection
{
    if (self = [super init]) {
        _keychain = keychain;
        
        SecTaskRef task = SecTaskCreateWithAuditToken(NULL, connection.auditToken);
        if (task) {
            _clientAccessGroups = (__bridge_transfer NSArray*)SecTaskCopyAccessGroups(task);
        }
        CFReleaseNull(task);
    }
    
    return self;
}

- (keyclass_t)keyclassForAccessPolicy:(SFAccessPolicy*)accessPolicy
{
    if (accessPolicy.accessibility.mode == SFAccessibleAfterFirstUnlock) {
        if (accessPolicy.sharingPolicy == SFSharingPolicyThisDeviceOnly) {
            return key_class_cku;
        }
        else {
            return key_class_ck;
        }
    }
    else {
        if (accessPolicy.sharingPolicy == SFSharingPolicyThisDeviceOnly) {
            return key_class_aku;
        }
        else {
            return key_class_ak;
        }
    }
}

- (void)rpcAddCredential:(SFCredential*)credential withAccessPolicy:(SFAccessPolicy*)accessPolicy reply:(void (^)(NSString* persistentIdentifier, NSError* error))reply
{
    if (![credential isKindOfClass:[SFPasswordCredential class]]) {
        reply(nil, [NSError errorWithDomain:SFKeychainErrorDomain code:SFKeychainErrorInvalidParameter userInfo:@{NSLocalizedDescriptionKey : [NSString stringWithFormat:@"attempt to add credential to SFCredentialStore that is not a password credential: %@", credential]}]);
        return;
    }

    NSString* accessGroup = accessPolicy.accessGroup;
    if (!accessGroup) {
        NSError* error = nil;
        accessGroup = self.clientAccessGroups.firstObject;
        if (!accessGroup) {
            error = [NSError errorWithDomain:SFKeychainErrorDomain code:SFKeychainErrorMissingAccessGroup userInfo:@{NSLocalizedDescriptionKey : @"no keychain access group found; ensure that your process has the keychain-access-groups entitlement"}];
            reply(nil, error);
            return;
        }
    }

    SFPasswordCredential* passwordCredential = (SFPasswordCredential*)credential;

    NSError* error = nil;
    NSData* primaryServiceIdentifierData = [NSKeyedArchiver archivedDataWithRootObject:passwordCredential.primaryServiceIdentifier requiringSecureCoding:YES error:&error];
    if (!primaryServiceIdentifierData) {
        dispatch_async(dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
            reply(nil, [NSError errorWithDomain:SFKeychainErrorDomain code:SFKeychainErrorSaveFailed userInfo:@{ NSLocalizedDescriptionKey : @"failed to serialize primary service identifier", NSUnderlyingErrorKey : error }]);
        });
        return;
    }

    NSMutableArray* serializedSupplementaryServiceIdentifiers = [[NSMutableArray alloc] initWithCapacity:passwordCredential.supplementaryServiceIdentifiers.count];
    for (SFServiceIdentifier* serviceIdentifier in passwordCredential.supplementaryServiceIdentifiers) {
        NSData* serviceIdentifierData = [NSKeyedArchiver archivedDataWithRootObject:serviceIdentifier requiringSecureCoding:YES error:&error];
        if (serviceIdentifierData) {
            [serializedSupplementaryServiceIdentifiers addObject:serviceIdentifierData];
        }
        else {
            dispatch_async(dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
                reply(nil, [NSError errorWithDomain:SFKeychainErrorDomain code:SFKeychainErrorSaveFailed userInfo:@{ NSLocalizedDescriptionKey : @"failed to serialize supplementary service identifier", NSUnderlyingErrorKey : error }]);
            });
            return;
        }
    }

    NSDictionary* attributes = @{ SFCredentialAttributeUsername : passwordCredential.username,
                                  SFCredentialAttributePrimaryServiceIdentifier : primaryServiceIdentifierData,
                                  SFCredentialAttributeSupplementaryServiceIdentifiers : serializedSupplementaryServiceIdentifiers,
                                  SFCredentialAttributeCreationDate : [NSDate date],
                                  SFCredentialAttributeModificationDate : [NSDate date],
                                  SFKeychainItemAttributeLocalizedLabel : passwordCredential.localizedLabel,
                                  SFKeychainItemAttributeLocalizedDescription : passwordCredential.localizedDescription,
                                  SFCredentialAttributeCustom : passwordCredential.customAttributes ?: [NSDictionary dictionary] };
    
    NSDictionary* secrets = @{ SFCredentialSecretPassword : passwordCredential.password };
    NSUUID* persistentID = [NSUUID UUID];

    // lookup attributes:
    // 1. primaryServiceIdentifier (always)
    // 2. username (always)
    // 3. label (if present)
    // 4. description (if present)
    // 5. each of the service identifiers by type, e.g. "domain"
    // 6. any custom attributes that fit the requirements (key is string, and value is plist type)

    SecCDKeychainLookupTuple* primaryServiceIdentifierLookup = [SecCDKeychainLookupTuple lookupTupleWithKey:SFCredentialAttributePrimaryServiceIdentifier value:primaryServiceIdentifierData];
    SecCDKeychainLookupTuple* usernameLookup = [SecCDKeychainLookupTuple lookupTupleWithKey:SFCredentialAttributeUsername value:passwordCredential.username];
    SecCDKeychainLookupTuple* labelLookup = [SecCDKeychainLookupTuple lookupTupleWithKey:SFKeychainItemAttributeLocalizedLabel value:passwordCredential.localizedLabel];
    SecCDKeychainLookupTuple* descriptionLookup = [SecCDKeychainLookupTuple lookupTupleWithKey:SFKeychainItemAttributeLocalizedDescription value:passwordCredential.localizedDescription];
    NSMutableArray* lookupAttributes = [[NSMutableArray alloc] initWithObjects:primaryServiceIdentifierLookup, usernameLookup, nil];
    if (labelLookup) {
        [lookupAttributes addObject:labelLookup];
    }
    if (descriptionLookup) {
        [lookupAttributes addObject:descriptionLookup];
    }

    SFServiceIdentifier* primaryServiceIdentifier = credential.primaryServiceIdentifier;
    [lookupAttributes addObject:[SecCDKeychainLookupTuple lookupTupleWithKey:primaryServiceIdentifier.lookupKey value:primaryServiceIdentifier.serviceID]];
    for (SFServiceIdentifier* serviceIdentifier in credential.supplementaryServiceIdentifiers) {
        [lookupAttributes addObject:[SecCDKeychainLookupTuple lookupTupleWithKey:serviceIdentifier.lookupKey value:serviceIdentifier.serviceID]];
    }

    [passwordCredential.customAttributes enumerateKeysAndObjectsUsingBlock:^(NSString* customKey, id value, BOOL* stop) {
        if ([customKey isKindOfClass:[NSString class]]) {
            SecCDKeychainLookupTuple* lookupTuple = [SecCDKeychainLookupTuple lookupTupleWithKey:customKey value:value];
            if (lookupTuple) {
                [lookupAttributes addObject:lookupTuple];
            }
            else {
                // TODO: an error here?
            }
        }
    }];

    SecCDKeychainAccessControlEntity* owner = [SecCDKeychainAccessControlEntity accessControlEntityWithType:SecCDKeychainAccessControlEntityTypeAccessGroup stringRepresentation:accessGroup];
    keyclass_t keyclass = [self keyclassForAccessPolicy:accessPolicy];
    SecCDKeychainItem* item = [[SecCDKeychainItem alloc] initItemType:[SecCDKeychainItemTypeCredential itemType] withPersistentID:persistentID attributes:attributes lookupAttributes:lookupAttributes secrets:secrets owner:owner keyclass:keyclass];
    [_keychain insertItems:@[item] withConnection:self completionHandler:^(bool success, NSError* insertError) {
        if (success && !insertError) {
            reply(persistentID.UUIDString, nil);
        }
        else {
            reply(nil, insertError);
        }
    }];
}

- (void)rpcFetchPasswordCredentialForPersistentIdentifier:(NSString*)persistentIdentifier reply:(void (^)(SFPasswordCredential* credential, NSString* password, NSError* error))reply
{
    // TODO: negative testing
    NSUUID* persistentID = [[NSUUID alloc] initWithUUIDString:persistentIdentifier];
    if (!persistentID) {
        secerror("SFKeychainServer: attempt to fetch credential with invalid persistent identifier; %@", persistentIdentifier);
        reply(nil, nil, [NSError errorWithDomain:SFKeychainErrorDomain code:SFKeychainErrorInvalidPersistentIdentifier userInfo:@{NSLocalizedDescriptionKey : [NSString stringWithFormat:@"invalid persistent identifier: %@", persistentIdentifier]}]);
        return;
    }

    [_keychain fetchItemForPersistentID:persistentID withConnection:self completionHandler:^(SecCDKeychainItem* item, NSError* error) {
        NSError* localError = error;
        SFPasswordCredential* credential = nil;
        if (item && !error) {
            credential = [self passwordCredentialForItem:item error:&localError];
        }
        
        if (credential) {
            reply(credential, credential.password, nil);
        }
        else {
            reply(nil, nil, localError);
        }
    }];
}

- (void)rpcLookupCredentialsForServiceIdentifiers:(nullable NSArray<SFServiceIdentifier*>*)serviceIdentifiers reply:(void (^)(NSArray<SFCredential*>* _Nullable results, NSError* _Nullable error))reply
{
    __block NSMutableDictionary* resultsDict = [[NSMutableDictionary alloc] init];
    __block NSError* resultError = nil;
    
    void (^processFetchedItems)(NSArray*) = ^(NSArray* fetchedItems) {
        for (SecCDKeychainItemMetadata* item in fetchedItems) {
            if ([item.itemType isKindOfClass:[SecCDKeychainItemTypeCredential class]]) {
                SFPasswordCredential* credential = [self passwordCredentialForItemMetadata:item error:&resultError];
                if (credential) {
                    resultsDict[item.persistentID] = credential;
                }
                else {
                    resultsDict = nil; // got an error
                }
            }
        }
    };

    if (!serviceIdentifiers) {
        // TODO: lookup everything
    }
    else {
        for (SFServiceIdentifier* serviceIdentifier in serviceIdentifiers) {
            dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
            // TODO: this is lamé; make fetchItemsWithValue take an array and get rid of the semaphore crap
            [_keychain fetchItemsWithValue:serviceIdentifier.serviceID forLookupKey:serviceIdentifier.lookupKey ofType:SecCDKeychainLookupValueTypeString withConnection:self completionHandler:^(NSArray<SecCDKeychainItemMetadata*>* items, NSError* error) {
                if (items && !error) {
                    processFetchedItems(items);
                }
                else {
                    resultsDict = nil;
                    resultError = error;
                }

                dispatch_semaphore_signal(semaphore);
            }];
            dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
        }
    }

    reply(resultsDict.allValues, resultError);
}

- (void)rpcRemoveCredentialWithPersistentIdentifier:(NSString*)persistentIdentifier reply:(void (^)(BOOL success, NSError* _Nullable error))reply
{
    NSUUID* persistentID = [[NSUUID alloc] initWithUUIDString:persistentIdentifier];
    if (!persistentID) {
        secerror("SFKeychainServer: attempt to remove credential with invalid persistent identifier; %@", persistentIdentifier);
        reply(false, [NSError errorWithDomain:SFKeychainErrorDomain code:SFKeychainErrorInvalidPersistentIdentifier userInfo:@{NSLocalizedDescriptionKey : [NSString stringWithFormat:@"invalid persistent identifier: %@", persistentIdentifier]}]);
        return;
    }
    
    [_keychain deleteItemWithPersistentID:persistentID withConnection:self completionHandler:^(bool success, NSError* error) {
        reply(success, error);
    }];
}

- (void)rpcReplaceOldCredential:(SFCredential*)oldCredential withNewCredential:(SFCredential*)newCredential reply:(void (^)(NSString* newPersistentIdentifier, NSError* _Nullable error))reply
{
    // TODO: implement
    reply(nil, nil);
}

- (SFPasswordCredential*)passwordCredentialForItem:(SecCDKeychainItem*)item error:(NSError**)error
{
    SFPasswordCredential* credential = [self passwordCredentialForItemMetadata:item.metadata error:error];
    if (credential) {
        credential.password = item.secrets[SFCredentialSecretPassword];
        if (!credential.password) {
            if (error) {
                *error = [NSError errorWithDomain:SFKeychainErrorDomain code:SFKeychainErrorSecureDecodeFailed userInfo:@{NSLocalizedDescriptionKey : @"failed to get password for SFCredential"}];
            }
            return nil;
        }
    }

    return credential;
}

- (SFPasswordCredential*)passwordCredentialForItemMetadata:(SecCDKeychainItemMetadata*)metadata error:(NSError**)error
{
    NSDictionary* attributes = metadata.attributes;
    NSString* username = attributes[SFCredentialAttributeUsername];
    
    NSError* localError = nil;
    SFServiceIdentifier* primaryServiceIdentifier = [NSKeyedUnarchiver unarchivedObjectOfClass:[SFServiceIdentifier class] fromData:attributes[SFCredentialAttributePrimaryServiceIdentifier] error:&localError];
    
    NSArray* serializedSupplementaryServiceIdentifiers = attributes[SFCredentialAttributeSupplementaryServiceIdentifiers];
    NSMutableArray* supplementaryServiceIdentifiers = [[NSMutableArray alloc] initWithCapacity:serializedSupplementaryServiceIdentifiers.count];
    for (NSData* serializedServiceIdentifier in serializedSupplementaryServiceIdentifiers) {
        if ([serializedServiceIdentifier isKindOfClass:[NSData class]]) {
            SFServiceIdentifier* serviceIdentifier = [NSKeyedUnarchiver unarchivedObjectOfClass:[SFServiceIdentifier class] fromData:serializedServiceIdentifier error:&localError];
            if (serviceIdentifier) {
                [supplementaryServiceIdentifiers addObject:serviceIdentifier];
            }
            else {
                supplementaryServiceIdentifiers = nil;
                break;
            }
        }
        else {
            supplementaryServiceIdentifiers = nil;
            localError = [NSError errorWithDomain:SFKeychainErrorDomain code:SFKeychainErrorSecureDecodeFailed userInfo:@{NSLocalizedDescriptionKey : @"malformed supplementary service identifiers array in SecCDKeychainItem"}];
            break;
        }
    }

    if (username && primaryServiceIdentifier && supplementaryServiceIdentifiers) {
        SFPasswordCredential* credential = [[SFPasswordCredential alloc] _initWithUsername:username primaryServiceIdentifier:primaryServiceIdentifier supplementaryServiceIdentifiers:supplementaryServiceIdentifiers];
        credential.creationDate = attributes[SFCredentialAttributeCreationDate];
        credential.modificationDate = attributes[SFCredentialAttributeModificationDate];
        credential.localizedLabel = attributes[SFKeychainItemAttributeLocalizedLabel];
        credential.localizedDescription = attributes[SFKeychainItemAttributeLocalizedDescription];
        credential.persistentIdentifier = metadata.persistentID.UUIDString;
        credential.customAttributes = attributes[SFCredentialAttributeCustom];
        return credential;
    }
    else {
        if (error) {
            *error = [NSError errorWithDomain:SFKeychainErrorDomain code:SFKeychainErrorSecureDecodeFailed userInfo:@{ NSLocalizedDescriptionKey : @"failed to deserialize SFCredential", NSUnderlyingErrorKey : localError }];
        }
        return nil;
    }
}

@end

#endif // ___OBJC2__

void SFKeychainServerInitialize(void)
{
    static dispatch_once_t once;
    static SFKeychainServer* server;
    static NSXPCListener* listener;

    dispatch_once(&once, ^{
        @autoreleasepool {
            NSURL* persistentStoreURL = (__bridge_transfer NSURL*)SecCopyURLForFileInKeychainDirectory((__bridge CFStringRef)@"CDKeychain");
            NSBundle* resourcesBundle = [NSBundle bundleWithPath:@"/System/Library/Keychain/KeychainResources.bundle"];
            NSURL* managedObjectModelURL = [resourcesBundle URLForResource:@"KeychainModel" withExtension:@"momd"];
            server = [[SFKeychainServer alloc] initWithStorageURL:persistentStoreURL modelURL:managedObjectModelURL encryptDatabase:true];
            listener = [[NSXPCListener alloc] initWithMachServiceName:@(kSFKeychainServerServiceName)];
            listener.delegate = server;
            [listener resume];
        }
    });
}

#else // !TARGET_OS_BRIDGE

void SFKeychainServerInitialize(void) {}

#endif

