//
//  MyKeychain.h
//  KCSync
//
//  Created by John Hurley on 10/3/12.
//  Copyright (c) 2012 john. All rights reserved.
//

#import <Foundation/Foundation.h>

extern const NSString *kItemPasswordKey;
extern const NSString *kItemAccountKey;
extern const NSString *kItemNameKey;

@interface MyKeychain : NSObject
+ (MyKeychain *) sharedInstance;
- (void) setPassword: (NSString *) password;
- (NSString *) fetchPassword;
- (void)setItem:(NSDictionary *)newItem;
- (NSMutableArray *) fetchDictionaryWithQuery:(NSMutableDictionary *)query;
- (NSMutableArray *) fetchDictionaryAll;

- (void)setPasswordFull:(NSString *)account service:(NSString *)service password:(NSString *) thePassword;
- (void)clearAllKeychainItems;

@end
