/*
 * Copyright (c) 2013-2014 Apple Inc. All Rights Reserved.
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


#import "KDSecItems.h"
#include <Security/Security.h>
#include <Security/SecItemPriv.h>


NSString *kKDSecItemsUpdated = @"KDSecItemsUpdated";

@interface KDSecItems ()
@property NSMutableArray *items;
@end

@implementation KDSecItems

-(NSInteger)numberOfRowsInTableView:(NSTableView*)t
{
    return [self.items count];
}

+(NSString*)nameOfItem:(NSDictionary*)item
{
    id name = item[(id)kSecAttrService];
    if (name) {
        return name;
    }
    
    NSString *path = item[(id)kSecAttrPath];
    if (!path) {
        path = @"/";
    }
    NSString *port = item[(id)kSecAttrPort];
    if ([@"0" isEqualToString:port] || [@0 isEqual:port]) {
        port = @"";
    } else {
        port = [NSString stringWithFormat:@":%@", port];
    }
    
    return [NSString stringWithFormat:@"%@://%@%@%@", item[(id)kSecAttrProtocol], item[(id)kSecAttrServer], port, path];
}

- (id)tableView:(NSTableView *)aTableView objectValueForTableColumn:(NSTableColumn *)aTableColumn row:(NSInteger)rowIndex
{
    NSString *identifier = [aTableColumn identifier];
    
    if ([@"account" isEqualToString:identifier]) {
        return self.items[rowIndex][(id)kSecAttrAccount];
    }
    if ([@"name" isEqualToString:identifier]) {
        return [KDSecItems nameOfItem:self.items[rowIndex]];
    }
    
    return [NSString stringWithFormat:@"*** c=%@ r%ld", [aTableColumn identifier], (long)rowIndex];
}

-(NSArray*)fetchItemsMatching:(NSDictionary *)query
{
    CFTypeRef raw_items = NULL;
    OSStatus result = SecItemCopyMatching((__bridge CFDictionaryRef)(query), &raw_items);
    if (result) {
        // XXX: UI
        NSLog(@"Error result %d - query: %@", result, query);
        return nil;
    }
    if (CFArrayGetTypeID() == CFGetTypeID(raw_items)) {
        return (__bridge NSArray*)raw_items;
    }
    
    NSLog(@"Unexpected result type from copyMatching: %@ (query=%@)", raw_items, query);
    CFRelease(raw_items);

    return nil;
}

-(void)loadItems
{
    NSDictionary *query_genp = @{(id)kSecClass: (id)kSecClassGenericPassword,
                                 (__bridge id)kSecAttrSynchronizable: @1,
                                 (id)kSecMatchLimit: (id)kSecMatchLimitAll,
                                 (id)kSecReturnAttributes: (id)kCFBooleanTrue};
    NSDictionary *query_inet = @{(id)kSecClass: (id)kSecClassInternetPassword,
                                 (__bridge id)kSecAttrSynchronizable: @1,
                                 (id)kSecMatchLimit: (id)kSecMatchLimitAll,
                                 (id)kSecReturnAttributes: (id)kCFBooleanTrue};
    NSArray *nextItems = [[self fetchItemsMatching:query_genp] arrayByAddingObjectsFromArray:[self fetchItemsMatching:query_inet]];
    self.items = [[nextItems sortedArrayUsingComparator:^NSComparisonResult(id a, id b) {
        NSDictionary *da = a, *db = b;
        return [da[(id)kSecAttrService] caseInsensitiveCompare:db[(id)kSecAttrService]];
    }] mutableCopy];
        
    dispatch_async(dispatch_get_main_queue(), ^{
        [[NSNotificationCenter defaultCenter] postNotificationName:kKDSecItemsUpdated object:self];
    });
}

-(id)init
{
    [self loadItems];
    return self;
}

@end
