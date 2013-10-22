//
//  KDSecItems.m
//  Security
//
//  Created by J Osborne on 2/14/13.
//
//

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
