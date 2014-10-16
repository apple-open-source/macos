//
//  MyKeychain.m
//  KCSync
//
//  Created by John Hurley on 10/3/12.
//  Copyright (c) 2012 john. All rights reserved.
//

#import "MyKeychain.h"
#import <Security/Security.h>
#import <Security/SecItemPriv.h>
#import <utilities/debugging.h>

const NSString *kItemPasswordKey = @"ItemPasswordKey";
const NSString *kItemAccountKey = @"ItemAccountKey";
const NSString *kItemNameKey = @"ItemNameKey";

#define KCSCOPE "mykeychain"

// secdebug(KCSCOPE,

@implementation MyKeychain

static MyKeychain *sharedInstance = nil;

+ (MyKeychain *) sharedInstance
{
    if (!sharedInstance)
		sharedInstance = [[self alloc] init];

    return sharedInstance;
}

// Translate status messages into return strings
- (NSString *) fetchStatus : (OSStatus) status
{
    switch (status)
    {
    case 0:
        return(@"Success!");
    case errSecNotAvailable:
        return(@"No trust results are available.!");
    case errSecItemNotFound:
        return(@"The item cannot be found.");
    case errSecParam:
        return(@"Parameter error.");
    case errSecAllocate:
        return(@"Memory allocation error. Failed to allocate memory.");
    case errSecInteractionNotAllowed:
        return(@"User interaction is not allowed.");
    case errSecUnimplemented:
        return(@"Function is not implemented");
    case errSecDuplicateItem:
        return(@"The item already exists.");
    case errSecDecode:
        return(@"Unable to decode the provided data.");

    default:
		return([NSString stringWithFormat:@"Function returned: %ld", (long)status]);
        break;
    }
    return @"can't happen...";
}

#define	ACCOUNT	@"Keychain Sync Test Account"
#define	SERVICE	@"Keychain Sync Test Service"
#define PWKEY	@"Keychain Sync Test Password Data"

// Return a base dictionary
- (NSMutableDictionary *) baseDictionary
{
	NSMutableDictionary *md = [[NSMutableDictionary alloc] init];
	
	// Password identification keys
	NSData *identifier = [PWKEY dataUsingEncoding:NSUTF8StringEncoding];
	[md setObject:identifier forKey:(__bridge id)kSecAttrGeneric];
	[md setObject:ACCOUNT forKey:(__bridge id)kSecAttrAccount];
    [md setObject:SERVICE forKey:(__bridge id)kSecAttrService];
	[md setObject:(__bridge id)kSecClassGenericPassword forKey:(__bridge id)kSecClass];
    [md setObject:(__bridge id)kCFBooleanTrue forKey:(__bridge id)kSecAttrSynchronizable];
    
	return md;
}

// Return a keychain-style dictionary populated with the password
- (NSMutableDictionary *) buildDictForPassword:(NSString *) password
{
	NSMutableDictionary *passwordDict = [self baseDictionary];
	
	// Add the password
	NSData *passwordData = [password dataUsingEncoding:NSUTF8StringEncoding];
    [passwordDict setObject:passwordData forKey:(__bridge id)kSecValueData]; // password 
	
	return passwordDict;
}

// Build a search query based
- (NSMutableDictionary *) buildSearchQuery
{
	NSMutableDictionary *genericPasswordQuery = [self baseDictionary];
	
	// Add the search constraints
	[genericPasswordQuery setObject:(__bridge id)kSecMatchLimitOne forKey:(__bridge id)kSecMatchLimit];
	[genericPasswordQuery setObject:(__bridge id)kCFBooleanTrue
							 forKey:(__bridge id)kSecReturnAttributes];
	[genericPasswordQuery setObject:(__bridge id)kCFBooleanTrue
							 forKey:(__bridge id)kSecReturnData];
	
	return genericPasswordQuery;
}

// retrieve data dictionary from the keychain
- (NSMutableArray *) fetchDictionaryWithQuery:(NSMutableDictionary *)query
{
	NSMutableDictionary *genericPasswordQuery = query;
	
//  secerror("Query: %@", query);
    CFTypeRef cfresult = nil;
	OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)genericPasswordQuery, &cfresult);

//  secerror( "FETCH: %s\n", [[self fetchStatus:status] UTF8String]);

	if (status == errSecItemNotFound)
        return NULL;
    
    if (CFGetTypeID(cfresult) == CFArrayGetTypeID())
    {
        NSMutableArray *result = [NSMutableArray arrayWithCapacity:0];
        [result addObjectsFromArray:CFBridgingRelease(cfresult)];
        return result;
    }
    
    // If it is a single result, embed it in an array because callers expect it
    if (CFGetTypeID(cfresult) == CFDictionaryGetTypeID())
        return [NSMutableArray arrayWithObject:CFBridgingRelease(cfresult)];

	return NULL;
}	

- (NSMutableArray *)fetchDictionary
{
	return [self fetchDictionaryWithQuery:[self buildSearchQuery]];
}	

// create a new keychain entry
- (BOOL) createKeychainValue:(NSString *) password
{
	NSMutableDictionary *md = [self buildDictForPassword:password];
	OSStatus status = SecItemAdd((__bridge CFDictionaryRef)md, NULL);

    secerror( "CREATE: %s\n", [[self fetchStatus:status] UTF8String]);
	
	if (status == errSecSuccess) return YES; else return NO;
}

// remove a keychain entry
- (void) clearKeychain
{
	NSMutableDictionary *genericPasswordQuery = [self baseDictionary];
	
	OSStatus status = SecItemDelete((__bridge CFDictionaryRef) genericPasswordQuery);
    secerror( "DELETE: %s\n", [[self fetchStatus:status] UTF8String]);
}

// update a keychain entry
- (BOOL) updateKeychainValue:(NSString *)password
{
	NSMutableDictionary *genericPasswordQuery = [self baseDictionary];
	
	NSMutableDictionary *attributesToUpdate = [[NSMutableDictionary alloc] init];
	NSData *passwordData = [password dataUsingEncoding:NSUTF8StringEncoding];
	[attributesToUpdate setObject:passwordData forKey:(__bridge id)kSecValueData];
	
	OSStatus status = SecItemUpdate((__bridge CFDictionaryRef)genericPasswordQuery, (__bridge CFDictionaryRef)attributesToUpdate);
    secerror( "UPDATE: %s\n", [[self fetchStatus:status] UTF8String]);
	
	if (status == 0) return YES; else return NO;
}

// fetch a keychain value
- (NSString *) fetchPassword
{
    NSMutableArray *allItems = [self fetchDictionary];
    if (!allItems)
        return NULL;
    
    // This is used for a single item, so take first item in array
    NSData *passData = [allItems[0] objectForKey:(__bridge id)kSecValueData];
    if (passData)
        return [[NSString alloc] initWithData:passData encoding:NSUTF8StringEncoding];
	
    return NULL;
}

- (void)setPassword: (NSString *) thePassword
{
	if (![self createKeychainValue:thePassword]) 
		[self updateKeychainValue:thePassword];
}

- (void)setItem:(NSDictionary *)newItem
{
    OSStatus status = errSecSuccess;
    NSMutableDictionary *passwordDict = [self baseDictionary];

	// Add the password
	NSData *passwordData = [[newItem objectForKey:kItemPasswordKey] dataUsingEncoding:NSUTF8StringEncoding];
    [passwordDict setObject:passwordData forKey:(__bridge id)kSecValueData]; // password 

    [passwordDict setObject:[newItem objectForKey:kItemAccountKey] forKey:(__bridge id)kSecAttrAccount];
    [passwordDict setObject:[newItem objectForKey:kItemNameKey] forKey:(__bridge id)kSecAttrService];

    // Try to add first; if error, then try to update
	status = SecItemAdd((__bridge CFDictionaryRef)passwordDict, NULL);
    secerror( "SecItemAdd result: %@ (%ld)", [self fetchStatus:status], (long)status);
    NSLog(@"SecItemAdd result: %@ (%ld)", [self fetchStatus:status], (long)status);

    if (status)
    {
        // Try update
        // We only want to update the password data, so delete the other keys
        
//        NSArray *keysToRemove = [NSArray arrayWithObjects:kItemAccountKey, kItemNameKey, nil];
        [passwordDict removeObjectsForKeys:@[(__bridge id)kSecValueData]];

        NSDictionary *itemsToUpdate = @{ (__bridge id)kSecValueData : passwordData };
//        [genericPasswordQuery setObject:[newItem objectForKey:kItemAccountKey] forKey:(__bridge id)kSecAttrAccount];
//        [genericPasswordQuery setObject:[newItem objectForKey:kItemNameKey] forKey:(__bridge id)kSecAttrService];
        status = SecItemUpdate((__bridge CFDictionaryRef)passwordDict, (__bridge CFDictionaryRef)(itemsToUpdate));
        secerror( "SecItemUpdate result: %@ (%ld)", [self fetchStatus:status], (long)status);
        NSLog(@"SecItemUpdate result: %@ (%ld)", [self fetchStatus:status], (long)status);
    }
}

- (NSMutableArray *)fetchDictionaryAll
{
	NSMutableDictionary *query = [[NSMutableDictionary alloc] init];
	[query setObject:(__bridge id)kSecClassGenericPassword forKey:(__bridge id)kSecClass];
    [query setObject:(__bridge id)kCFBooleanTrue forKey:(__bridge id)kSecAttrSynchronizable];

	// Add the search constraints
	[query setObject:(__bridge id)kSecMatchLimitAll forKey:(__bridge id)kSecMatchLimit];
	[query setObject:(__bridge id)kCFBooleanTrue
							 forKey:(__bridge id)kSecReturnAttributes];
	[query setObject:(__bridge id)kCFBooleanTrue
							 forKey:(__bridge id)kSecReturnData];

    NSMutableArray *genericItems = [self fetchDictionaryWithQuery:query];
    
    // Now look for internet items
    [query setObject:(__bridge id)kSecClassInternetPassword forKey:(__bridge id)kSecClass];
    NSMutableArray *internetItems = [self fetchDictionaryWithQuery:query];
    if (internetItems)
        [genericItems addObjectsFromArray:internetItems];
	return genericItems;
}	

// MARK: ----- Full routines -----

// Return a base dictionary
- (NSMutableDictionary *) baseDictionaryFull:(NSString *)account service:(NSString *)service
{
	NSMutableDictionary *md = [[NSMutableDictionary alloc] init];
	
	// Password identification keys
	NSData *identifier = [PWKEY dataUsingEncoding:NSUTF8StringEncoding];
	[md setObject:identifier forKey:(__bridge id)kSecAttrGeneric];
	[md setObject:account forKey:(__bridge id)kSecAttrAccount];
    [md setObject:service forKey:(__bridge id)kSecAttrService];
	[md setObject:(__bridge id)kSecClassGenericPassword forKey:(__bridge id)kSecClass];
    [md setObject:(__bridge id)kCFBooleanTrue forKey:(__bridge id)kSecAttrSynchronizable];

//	return [md autorelease];
	return md;
}

- (BOOL) createKeychainValueFull:(NSString *)account service:(NSString *)service password:(NSString *)password
{
	NSMutableDictionary *md = [self buildDictForPasswordFull:account service:service password:password];
	OSStatus status = SecItemAdd((__bridge CFDictionaryRef)md, NULL);
    secerror( "CREATE: %s\n", [[self fetchStatus:status] UTF8String]);
	
	if (status == errSecSuccess) return YES; else return NO;
}

- (BOOL) updateKeychainValueFull:(NSString *)account service:(NSString *)service password:(NSString *)password
{
	NSMutableDictionary *genericPasswordQuery = [self baseDictionaryFull:account service:service];
	
	NSMutableDictionary *attributesToUpdate = [[NSMutableDictionary alloc] init];
	NSData *passwordData = [password dataUsingEncoding:NSUTF8StringEncoding];
	[attributesToUpdate setObject:passwordData forKey:(__bridge id)kSecValueData];
	
	OSStatus status = SecItemUpdate((__bridge CFDictionaryRef)genericPasswordQuery, (__bridge CFDictionaryRef)attributesToUpdate);
    secerror( "UPDATE: %s\n", [[self fetchStatus:status] UTF8String]);
	
	if (status == 0) return YES; else return NO;
}

- (void)setPasswordFull:(NSString *)account service:(NSString *)service password:(NSString *) thePassword
{
    secerror( "setPasswordFull account: %@, service: %@, password: %@", account, service, thePassword);
	if (![self createKeychainValueFull:account service:service password:thePassword])
		[self updateKeychainValueFull:account service:service password:thePassword];
}

// Return a keychain-style dictionary populated with the password
- (NSMutableDictionary *) buildDictForPasswordFull:(NSString *)account service:(NSString *)service password:(NSString *)password
{
	NSMutableDictionary *passwordDict = [self baseDictionaryFull:account service:service];
	
	// Add the password
	NSData *passwordData = [password dataUsingEncoding:NSUTF8StringEncoding];
    [passwordDict setObject:passwordData forKey:(__bridge id)kSecValueData]; // password 
	
	return passwordDict;
}

- (void)clearAllKeychainItems
{
    CFIndex ix, top;
    OSStatus status = errSecSuccess;
    
    NSArray *allItems = (NSArray *)[[MyKeychain sharedInstance] fetchDictionaryAll];
    top = [allItems count];
    secerror( "Deleting %ld items", (long)top);
    
    for (ix=0; ix<top && !status; ix++)
    {
        NSDictionary *item = [allItems objectAtIndex:ix];
        NSMutableDictionary *query = [[NSMutableDictionary alloc] init];
        
        NSData *identifier = [PWKEY dataUsingEncoding:NSUTF8StringEncoding];
        [query setObject:identifier forKey:(__bridge id)kSecAttrGeneric];
        [query setObject:(__bridge id)kSecClassGenericPassword forKey:(__bridge id)kSecClass];
        [query setObject:(__bridge id)kCFBooleanTrue forKey:(__bridge id)kSecAttrSynchronizable];
        
        [query setObject:[item objectForKey:(__bridge id)(kSecAttrAccount)] forKey:(__bridge id)kSecAttrAccount];
        [query setObject:[item objectForKey:(__bridge id)(kSecAttrService)] forKey:(__bridge id)kSecAttrService];
        
        status = SecItemDelete((__bridge CFDictionaryRef) query);
        secerror( "DELETE: %s\n", [[self fetchStatus:status] UTF8String]);
    }
}


@end
