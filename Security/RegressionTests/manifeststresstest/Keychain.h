//
//  Keychain.h
//  Security
//
//  Created by Ben Williamson on 6/2/17.
//
//

#import <Foundation/Foundation.h>
#import <Security/Security.h>

@interface Keychain : NSObject

// Should return errSecSuccess or errSecDuplicateItem
- (OSStatus)addItem:(NSString *)name value:(NSString *)value view:(NSString *)view;
- (OSStatus)addItem:(NSString *)name value:(NSString *)value view:(NSString *)view pRef:(NSArray **)result;

// Should return errSecSuccess or errSecItemNotFound
- (OSStatus)updateItemWithName:(NSString *)name newValue:(NSString *)newValue;
- (OSStatus)updateItem:(id)pRef newValue:(NSString *)newValue;
- (OSStatus)updateItem:(id)pRef newName:(NSString *)newName;
- (OSStatus)updateItem:(id)pRef newName:(NSString *)newName newValue:(NSString *)newValue;
- (OSStatus)deleteItem:(id)pRef;
- (OSStatus)deleteItemWithName:(NSString *)name;

// Should return errSecSuccess
- (OSStatus)deleteAllItems;

- (NSDictionary<NSString *, NSArray *> *)getAllItems;

@end
