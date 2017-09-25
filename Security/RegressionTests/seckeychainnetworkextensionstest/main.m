//
//  main.m
//  seckeychainnetworkextensionstest
//
//  Created by Luke Hiesterman on 2/22/17.
//

#import <Foundation/Foundation.h>
#import <Security/Security.h>
#import <Security/SecItemPriv.h>
#import <err.h>

static NSString* NetworkExtensionPersistentRefSharingAccessGroup = @"com.apple.NetworkExtensionPersistentRefSharingAccessGroup";
static NSString* NetworkExtensionAccessGroup = @"FakeAppPrefix.com.apple.networkextensionsharing";
static NSString* TestAccount = @"MyTestAccount";
static NSString* TestPassword = @"MyTestPassword";

static void cleanupKeychain()
{
    NSMutableDictionary* attributes = [NSMutableDictionary dictionary];
    attributes[(__bridge NSString*)kSecClass] = (__bridge NSString*)kSecClassGenericPassword;
    attributes[(__bridge NSString*)kSecAttrAccessGroup] = NetworkExtensionAccessGroup;
    attributes[(__bridge NSString*)kSecAttrAccount] = TestAccount;
    attributes[(__bridge NSString*)kSecAttrNoLegacy] = @YES;
    SecItemDelete((__bridge CFDictionaryRef)attributes);
    
    attributes = [NSMutableDictionary dictionary];
    attributes[(__bridge NSString*)kSecClass] = (__bridge NSString*)kSecClassGenericPassword;
    attributes[(__bridge NSString*)kSecAttrAccessGroup] = NetworkExtensionPersistentRefSharingAccessGroup;
    attributes[(__bridge NSString*)kSecAttrAccount] = TestAccount;
    attributes[(__bridge NSString*)kSecAttrNoLegacy] = @YES;
    SecItemDelete((__bridge CFDictionaryRef)attributes);
    
}

int main(int argc, const char * argv[])
{
    @autoreleasepool {
        cleanupKeychain();
        
        NSMutableDictionary* attributes = [NSMutableDictionary dictionary];
        attributes[(__bridge NSString*)kSecClass] = (__bridge NSString*)kSecClassGenericPassword;
        attributes[(__bridge NSString*)kSecAttrAccessGroup] = NetworkExtensionAccessGroup;
        attributes[(__bridge NSString*)kSecAttrAccount] = TestAccount;
        attributes[(__bridge NSString*)kSecValueData] = [NSData dataWithBytes:TestPassword.UTF8String length:TestPassword.length];
        attributes[(__bridge NSString*)kSecReturnPersistentRef] = @YES;
        attributes[(__bridge NSString*)kSecAttrNoLegacy] = @YES;

        CFTypeRef returnData = NULL;
        OSStatus result = SecItemAdd((__bridge CFDictionaryRef)attributes, &returnData);
        if (result != 0) {
            NSLog(@"got an error: %d", (int)result);
            errx(1, "failed to add item to keychain");
        }
        
        if (returnData) {
            attributes = [NSMutableDictionary dictionary];
            attributes[(__bridge NSString*)kSecClass] = (__bridge NSString*)kSecClassGenericPassword;
            attributes[(__bridge NSString*)kSecAttrAccessGroup] = NetworkExtensionPersistentRefSharingAccessGroup;
            attributes[(__bridge NSString*)kSecAttrAccount] = TestAccount;
            attributes[(__bridge NSString*)kSecValueData] = (__bridge NSData*)returnData;
            attributes[(__bridge NSString*)kSecAttrNoLegacy] = @YES;

            result = SecItemAdd((__bridge CFDictionaryRef)attributes, &returnData);
            if (result == 0) {
                NSLog(@"successfully stored persistent ref for shared network extension item to keychain");
            }
            else {
                errx(1, "failed to add persistent ref to keychain");
            }
        }
        else {
            errx(1, "failed to get persistent ref from item added to keychain");
        }
    }
    return 0;
}
