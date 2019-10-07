//
//  Keychain.m
//  Security
//
//  Created by Ben Williamson on 6/2/17.
//
//

#import "Keychain.h"

/*
 * This is to fool os services to not provide the Keychain manager
 * interface that doens't work since we don't have unified headers
 * between iOS and OS X. rdar://23405418/
 */
#define __KEYCHAINCORE__ 1

#import <Security/Security.h>
#import <Security/SecItemPriv.h>
#import <Security/SecBasePriv.h>
#import <Security/SecIdentityPriv.h>

#if SEC_OS_OSX_INCLUDES
#import <Security/SecKeychain.h>
#endif

#import <stdio.h>
#import <stdlib.h>

static NSString *kAccessGroup = @"manifeststresstest";
static NSString *kService = @"manifeststresstest";



@implementation Keychain

- (OSStatus)addItem:(NSString *)name value:(NSString *)value view:(NSString *)view pRef:(NSArray **)result
{
    NSDictionary *query = @{
                            (id)kSecClass : (id)kSecClassGenericPassword,
                            (id)kSecAttrAccessGroup : kAccessGroup,
                            (id)kSecAttrService : kService,
                            (id)kSecAttrAccount : name,
                            (id)kSecValueData : [value dataUsingEncoding:NSUTF8StringEncoding],
                            (id)kSecAttrSynchronizable: (id)kCFBooleanTrue,
                            (id)kSecUseDataProtectionKeychain : (id)kCFBooleanTrue,
                            (id)kSecAttrSyncViewHint : view,
                            (id)kSecReturnPersistentRef: @YES,
                            };
    CFArrayRef itemRef = NULL;
    OSStatus status = SecItemAdd((__bridge CFDictionaryRef)query, (void *)&itemRef);
    if (result) {
        *result = CFBridgingRelease(itemRef);
    }

    return status;
}

- (OSStatus)addItem:(NSString *)name value:(NSString *)value view:(NSString *)view
{
    return [self addItem:name value:value view:view pRef:nil];
}

- (OSStatus)updateItemWithName:(NSString *)name newValue:(NSString *)newValue
{

    NSDictionary *query = @{
                            (id)kSecClass : (id)kSecClassGenericPassword,
                            (id)kSecAttrAccessGroup : kAccessGroup,
                            (id)kSecAttrService : kService,
                            (id)kSecAttrAccount : name,
                            (id)kSecAttrSynchronizable: (id)kCFBooleanTrue,
                            };
    NSDictionary *modifications  = @{
                                     (id)kSecValueData : [newValue dataUsingEncoding:NSUTF8StringEncoding],
                                     };
    return SecItemUpdate((__bridge CFDictionaryRef)query, (__bridge CFDictionaryRef)modifications);
}

- (OSStatus)updateItem:(id)pRef newValue:(NSString *)newValue
{
    return [self updateItem:pRef modifications:@{
                                                 (id)kSecValueData : [newValue dataUsingEncoding:NSUTF8StringEncoding],
                                                 }];
}

- (OSStatus)updateItem:(id)pRef newName:(NSString *)newName
{
    return [self updateItem:pRef modifications:@{
                                                 (id)kSecAttrAccount : newName,
                                                 }];
}

- (OSStatus)updateItem:(id)pRef newName:(NSString *)newName newValue:(NSString *)newValue
{
    return [self updateItem:pRef modifications:@{
                                                 (id)kSecAttrAccount : newName,
                                                 (id)kSecValueData : [newValue dataUsingEncoding:NSUTF8StringEncoding],
                                                 }];
}

- (OSStatus)updateItem:(id)pRef modifications:(NSDictionary *)modifications
{
    NSDictionary *query = @{
                            (id)kSecClass : (id)kSecClassGenericPassword,
                            (id)kSecValuePersistentRef: ([pRef isKindOfClass:[NSData class]] ? pRef : pRef[0]),
                            };
    return SecItemUpdate((__bridge CFDictionaryRef)query, (__bridge CFDictionaryRef)modifications);
}

- (OSStatus)deleteItem:(id)pRef
{
    NSDictionary *query = @{
                            (id)kSecClass : (id)kSecClassGenericPassword,
                            (id)kSecValuePersistentRef: ([pRef isKindOfClass:[NSData class]] ? pRef : pRef[0]),
                            };
    return SecItemDelete((__bridge CFDictionaryRef)query);
}

- (OSStatus)deleteItemWithName:(NSString *)name
{
    NSDictionary *query = @{
                            (id)kSecClass : (id)kSecClassGenericPassword,
                            (id)kSecAttrAccessGroup : kAccessGroup,
                            (id)kSecAttrService : kService,
                            (id)kSecAttrSynchronizable: (id)kCFBooleanTrue,
                            (id)kSecAttrAccount : name,
                            };
    return SecItemDelete((__bridge CFDictionaryRef)query);
}

- (OSStatus)deleteAllItems
{
    NSDictionary *query = @{
                            (id)kSecClass : (id)kSecClassGenericPassword,
                            (id)kSecAttrAccessGroup : kAccessGroup,
                            (id)kSecAttrSynchronizable: (id)kCFBooleanTrue,
                            };
    return SecItemDelete((__bridge CFDictionaryRef)query);
}

- (NSDictionary<NSString *, NSArray *> *)getAllItems
{
    CFArrayRef result = NULL;
    NSDictionary *query = @{
                            (id)kSecMatchLimit : (id)kSecMatchLimitAll,
                            (id)kSecReturnData : (id)kCFBooleanTrue,
                            (id)kSecReturnAttributes : (id)kCFBooleanTrue,
                            (id)kSecReturnPersistentRef : (id)kCFBooleanTrue,
                            (id)kSecClass : (id)kSecClassGenericPassword,
                            (id)kSecAttrAccessGroup : kAccessGroup,
                            (id)kSecAttrService : kService,
                            (id)kSecUseDataProtectionKeychain : (id)kCFBooleanTrue,
                            (id)kSecAttrSynchronizable: (id)kCFBooleanTrue,
                            };
    OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)query, (CFTypeRef *)&result);
    if (status == errSecItemNotFound) {
        return @{};
    }
    if (status != errSecSuccess) {
        printf("Error reading items to verify: %d\n", (int)status);
        exit(1);
    }
    NSArray *arr = CFBridgingRelease(result);
    
    NSMutableDictionary *items = [NSMutableDictionary dictionary];
    for (NSDictionary *dict in arr) {
        NSString *name = dict[(id)kSecAttrAccount];
        NSData *data = dict[(id)kSecValueData];
        NSData *ref = dict[(id)kSecValuePersistentRef];
        NSString *value = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
        if (!value) {
            printf("Item %s has data that is not valid UTF-8.\n", [name UTF8String]);
            exit(1);
        }
        items[name] = @[ref, value];
    }
    return items;
}

@end
