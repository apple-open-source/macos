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

#import "SFKeychainControlManager.h"
#import "SecCFError.h"
#import "builtin_commands.h"
#import "debugging.h"
#import <Security/SecItem.h>
#import <Security/SecItemPriv.h>
#import <Foundation/NSXPCConnection_Private.h>
#import <Security/SecXPCHelper.h>

NSString* kSecEntitlementKeychainControl = @"com.apple.private.keychain.keychaincontrol";

XPC_RETURNS_RETAINED xpc_endpoint_t SecServerCreateKeychainControlEndpoint(void)
{
    return [[SFKeychainControlManager sharedManager] xpcControlEndpoint];
}

@implementation SFKeychainControlManager {
    NSXPCListener* _listener;
}

+ (instancetype)sharedManager
{
    static SFKeychainControlManager* manager = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        manager = [[SFKeychainControlManager alloc] _init];
    });

    return manager;
}

- (instancetype)_init
{
    if (self = [super init]) {
        _listener = [NSXPCListener anonymousListener];
        _listener.delegate = self;
        [_listener resume];
    }

    return self;
}

- (xpc_endpoint_t)xpcControlEndpoint
{
    return [_listener.endpoint _endpoint];
}

- (BOOL)listener:(NSXPCListener*)listener shouldAcceptNewConnection:(NSXPCConnection*)newConnection
{
    NSNumber* entitlementValue = [newConnection valueForEntitlement:kSecEntitlementKeychainControl];
    if (![entitlementValue isKindOfClass:[NSNumber class]] || !entitlementValue.boolValue) {
        secerror("SFKeychainControl: Client pid (%d) doesn't have entitlement: %@", newConnection.processIdentifier, kSecEntitlementKeychainControl);
        return NO;
    }

    NSSet<Class>* errorClasses = [SecXPCHelper safeErrorClasses];

    NSXPCInterface* interface = [NSXPCInterface interfaceWithProtocol:@protocol(SFKeychainControl)];
    [interface setClasses:errorClasses forSelector:@selector(rpcFindCorruptedItemsWithReply:) argumentIndex:1 ofReply:YES];
    [interface setClasses:errorClasses forSelector:@selector(rpcDeleteCorruptedItemsWithReply:) argumentIndex:1 ofReply:YES];
    newConnection.exportedInterface = interface;
    newConnection.exportedObject = self;
    [newConnection resume];
    return YES;
}

- (NSArray<NSDictionary*>*)findCorruptedItemsWithError:(NSError**)error
{
    NSMutableArray<NSDictionary*>* corruptedItems = [[NSMutableArray alloc] init];
    NSMutableArray* underlyingErrors = [[NSMutableArray alloc] init];

    CFTypeRef genericPasswords = NULL;
    NSDictionary* genericPasswordsQuery = @{ (id)kSecClass : (id)kSecClassGenericPassword,
                                             (id)kSecReturnPersistentRef : @(YES),
                                             (id)kSecUseDataProtectionKeychain : @(YES),
                                             (id)kSecMatchLimit : (id)kSecMatchLimitAll };
    OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)genericPasswordsQuery, &genericPasswords);
    CFErrorRef genericPasswordError = NULL;
    if (status != errSecItemNotFound) {
        SecError(status, &genericPasswordError, CFSTR("generic password query failed"));
        if (genericPasswordError) {
            [underlyingErrors addObject:CFBridgingRelease(genericPasswordError)];
        }
    }

    CFTypeRef internetPasswords = NULL;
    NSDictionary* internetPasswordsQuery = @{ (id)kSecClass : (id)kSecClassInternetPassword,
                                              (id)kSecReturnPersistentRef : @(YES),
                                              (id)kSecUseDataProtectionKeychain : @(YES),
                                              (id)kSecMatchLimit : (id)kSecMatchLimitAll };
    status = SecItemCopyMatching((__bridge CFDictionaryRef)internetPasswordsQuery, &internetPasswords);
    CFErrorRef internetPasswordError = NULL;
    if (status != errSecItemNotFound) {
        SecError(status, &internetPasswordError, CFSTR("internet password query failed"));
        if (internetPasswordError) {
            [underlyingErrors addObject:CFBridgingRelease(internetPasswordError)];
        }
    }

    CFTypeRef keys = NULL;
    NSDictionary* keysQuery = @{ (id)kSecClass : (id)kSecClassKey,
                                 (id)kSecReturnPersistentRef : @(YES),
                                 (id)kSecUseDataProtectionKeychain : @(YES),
                                 (id)kSecMatchLimit : (id)kSecMatchLimitAll };
    status = SecItemCopyMatching((__bridge CFDictionaryRef)keysQuery, &keys);
    CFErrorRef keyError = NULL;
    if (status != errSecItemNotFound) {
        if (keyError) {
            [underlyingErrors addObject:CFBridgingRelease(keyError)];
        }
    }

    CFTypeRef certificates = NULL;
    NSDictionary* certificateQuery = @{ (id)kSecClass : (id)kSecClassCertificate,
                                        (id)kSecReturnPersistentRef : @(YES),
                                        (id)kSecUseDataProtectionKeychain : @(YES),
                                        (id)kSecMatchLimit : (id)kSecMatchLimitAll };
    status = SecItemCopyMatching((__bridge CFDictionaryRef)certificateQuery, &certificates);
    CFErrorRef certificateError = NULL;
    if (status != errSecItemNotFound) {
        SecError(status, &certificateError, CFSTR("certificate query failed"));
        if (certificateError) {
            [underlyingErrors addObject:CFBridgingRelease(certificateError)];
        }
    }

    void (^scanArrayForCorruptedItem)(CFTypeRef, NSString*) = ^(CFTypeRef items, NSString* class) {
        if ([(__bridge NSArray*)items isKindOfClass:[NSArray class]]) {
            NSLog(@"scanning %d %@", (int)CFArrayGetCount(items), class);
            for (NSData* persistentRef in (__bridge NSArray*)items) {
                NSDictionary* itemQuery = @{ (id)kSecClass : class,
                                             (id)kSecValuePersistentRef : persistentRef,
                                             (id)kSecReturnAttributes : @(YES),
                                             (id)kSecUseDataProtectionKeychain : @(YES) };
                CFTypeRef itemAttributes = NULL;
                OSStatus copyStatus = SecItemCopyMatching((__bridge CFDictionaryRef)itemQuery, &itemAttributes);
                if (copyStatus != errSecSuccess && status != errSecInteractionNotAllowed) {
                    [corruptedItems addObject:itemQuery];
                }
            }
        }
    };

    scanArrayForCorruptedItem(genericPasswords, (id)kSecClassGenericPassword);
    scanArrayForCorruptedItem(internetPasswords, (id)kSecClassInternetPassword);
    scanArrayForCorruptedItem(keys, (id)kSecClassKey);
    scanArrayForCorruptedItem(certificates, (id)kSecClassCertificate);

    if (underlyingErrors.count > 0 && error) {
        *error = [NSError errorWithDomain:@"com.apple.security.keychainhealth" code:1 userInfo:@{ NSLocalizedDescriptionKey : [NSString stringWithFormat:@"encountered %d errors searching for corrupted items", (int)underlyingErrors.count], NSUnderlyingErrorKey : underlyingErrors.firstObject, @"searchingErrorCount" : @(underlyingErrors.count) }];
    }

    return corruptedItems;
}

- (bool)deleteCorruptedItemsWithError:(NSError**)error
{
    NSError* findError = nil;
    NSArray* corruptedItems = [self findCorruptedItemsWithError:&findError];
    bool success = findError == nil;

    NSMutableArray* deleteErrors = [[NSMutableArray alloc] init];
    for (NSDictionary* corruptedItem in corruptedItems) {
        OSStatus status = SecItemDelete((__bridge CFDictionaryRef)corruptedItem);
        if (status != errSecSuccess) {
            success = false;
            CFErrorRef deleteError = NULL;
            SecError(status, &deleteError, CFSTR("failed to delete corrupted item"));
            [deleteErrors addObject:CFBridgingRelease(deleteError)];
        }
    }

    if (error && (findError || deleteErrors.count > 0)) {
        *error = [NSError errorWithDomain:@"com.apple.security.keychainhealth" code:2 userInfo:@{ NSLocalizedDescriptionKey : [NSString stringWithFormat:@"encountered %@ errors searching for corrupted items and %d errors attempting to delete corrupted items", findError.userInfo[@"searchingErrorCount"], (int)deleteErrors.count]}];
    }

    return success;
}

- (void)rpcFindCorruptedItemsWithReply:(void (^)(NSArray* corruptedItems, NSError* error))reply
{
    NSError* error = nil;
    NSArray* corruptedItems = [self findCorruptedItemsWithError:&error];
    reply(corruptedItems, error);
}

- (void)rpcDeleteCorruptedItemsWithReply:(void (^)(bool success, NSError* error))reply
{
    NSError* error = nil;
    bool success = [self deleteCorruptedItemsWithError:&error];
    reply(success, error);
}

@end
