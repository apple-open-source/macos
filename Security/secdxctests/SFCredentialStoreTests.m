/*
 * Copyright (c) 2018 Apple Inc. All Rights Reserved.
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

#import "KeychainXCTest.h"
#import "SFKeychainServer.h"
#import "SecCDKeychain.h"
#import "SecFileLocations.h"
#import <Foundation/Foundation.h>
#import <Foundation/NSXPCConnection_Private.h>
#import <XCTest/XCTest.h>
#import <SecurityFoundation/SFKeychain.h>
#import <OCMock/OCMock.h>

#if USE_KEYSTORE

@interface SFCredentialStore (UnitTestingForwardDeclarations)

- (instancetype)_init;

- (id<NSXPCProxyCreating>)_serverConnectionWithError:(NSError**)error;

@end

@interface SFKeychainServer (UnitTestingForwardDeclarations)

@property (readonly, getter=_keychain) SecCDKeychain* keychain;

@end

@interface SFKeychainServerConnection (UnitTestingRedeclarations)

- (instancetype)initWithKeychain:(SecCDKeychain*)keychain xpcConnection:(NSXPCConnection*)connection;

@end

@interface SecCDKeychain (UnitTestingRedeclarations)

- (NSData*)_onQueueGetDatabaseKeyDataWithError:(NSError**)error;

@end

@interface KeychainNoXPCServerProxy : NSObject <NSXPCProxyCreating>

@property (readonly) SFKeychainServer* server;

@end

@implementation KeychainNoXPCServerProxy {
    SFKeychainServer* _server;
    SFKeychainServerFakeConnection* _connection;
}

@synthesize server = _server;

- (instancetype)init
{
    if (self = [super init]) {
        NSURL* persistentStoreURL = (__bridge_transfer NSURL*)SecCopyURLForFileInKeychainDirectory((__bridge CFStringRef)@"CDKeychain");
        NSBundle* resourcesBundle = [NSBundle bundleWithPath:@"/System/Library/Keychain/KeychainResources.bundle"];
        NSURL* managedObjectModelURL = [resourcesBundle URLForResource:@"KeychainModel" withExtension:@"momd"];
        _server = [[SFKeychainServer alloc] initWithStorageURL:persistentStoreURL modelURL:managedObjectModelURL encryptDatabase:false];
        _connection = [[SFKeychainServerFakeConnection alloc] initWithKeychain:_server.keychain xpcConnection:nil];
    }
    
    return self;
}

- (id)remoteObjectProxy
{
    return _server;
}

- (id)remoteObjectProxyWithErrorHandler:(void (^)(NSError*))handler
{
    return _connection;
}

@end

@interface SFCredentialStoreTests : KeychainXCTest
@end

@implementation SFCredentialStoreTests {
    SFCredentialStore* _credentialStore;
}

+ (void)setUp
{
    [super setUp];
    
    id credentialStoreMock = OCMClassMock([SFCredentialStore class]);
    [[[[credentialStoreMock stub] andCall:@selector(serverProxyWithError:) onObject:self] ignoringNonObjectArgs] _serverConnectionWithError:NULL];
}

+ (id)serverProxyWithError:(NSError**)error
{
    return [[KeychainNoXPCServerProxy alloc] init];
}

- (void)setUp
{
    [super setUp];
    self.keychainPartialMock = OCMPartialMock([(SFKeychainServer*)[[self.class serverProxyWithError:nil] server] _keychain]);
    [[[[self.keychainPartialMock stub] andCall:@selector(getDatabaseKeyDataWithError:) onObject:self] ignoringNonObjectArgs] _onQueueGetDatabaseKeyDataWithError:NULL];

    _credentialStore = [[SFCredentialStore alloc] _init];
}

- (BOOL)passwordCredential:(SFPasswordCredential*)firstCredential matchesCredential:(SFPasswordCredential*)secondCredential
{
    return [firstCredential.primaryServiceIdentifier isEqual:secondCredential.primaryServiceIdentifier] &&
           [[NSSet setWithArray:firstCredential.supplementaryServiceIdentifiers] isEqualToSet:[NSSet setWithArray:secondCredential.supplementaryServiceIdentifiers]] &&
           [firstCredential.localizedLabel isEqualToString:secondCredential.localizedLabel] &&
           [firstCredential.localizedDescription isEqualToString:secondCredential.localizedDescription] &&
           [firstCredential.customAttributes isEqualToDictionary:secondCredential.customAttributes];
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Warc-retain-cycles"
// we don't care about creating retain cycles inside our testing blocks (they get broken properly anyway)

- (void)testAddAndFetchCredential
{
    SFPasswordCredential* credential = [[SFPasswordCredential alloc] initWithUsername:@"TestUser" password:@"TestPass" primaryServiceIdentifier:[SFServiceIdentifier serviceIdentifierForDomain:@"testdomain.com"]];
    __block NSString* credentialIdentifier = nil;
    
    XCTestExpectation* addExpectation = [self expectationWithDescription:@"add credential"];
    [_credentialStore addCredential:credential withAccessPolicy:[[SFAccessPolicy alloc] initWithAccessibility:SFAccessibilityMakeWithMode(SFAccessibleWhenUnlocked) sharingPolicy:SFSharingPolicyWithTrustedDevices] resultHandler:^(NSString* persistentIdentifier, NSError* error) {
        credentialIdentifier = persistentIdentifier;
        XCTAssertNotNil(persistentIdentifier, @"failed to get persistent identifier for added credential");
        XCTAssertNil(error, @"received unexpected error attempting to add credential to store: %@", error);
        [addExpectation fulfill];
    }];
    [self waitForExpectations:@[addExpectation] timeout:5.0];

    XCTestExpectation* fetchExpecation = [self expectationWithDescription:@"fetch credential"];
    [_credentialStore fetchPasswordCredentialForPersistentIdentifier:credentialIdentifier withResultHandler:^(SFPasswordCredential* fetchedCredential, NSString* password, NSError* error) {
        XCTAssertNotNil(fetchedCredential, @"failed to fetch credential just added to store");
        XCTAssertNil(error, @"received unexpected error fetching credential from store: %@", error);
        XCTAssertTrue([self passwordCredential:credential matchesCredential:fetchedCredential], @"the credential we fetched from the store does not match the one we added");
        XCTAssertEqualObjects(password, @"TestPass", @"the password we fetched from the store does not match the one we added");
        [fetchExpecation fulfill];
    }];
    [self waitForExpectations:@[fetchExpecation] timeout:5.0];
}

- (void)testLookupCredential
{
    SFPasswordCredential* credential = [[SFPasswordCredential alloc] initWithUsername:@"TestUser" password:@"TestPass" primaryServiceIdentifier:[SFServiceIdentifier serviceIdentifierForDomain:@"testdomain.com"]];
    
    XCTestExpectation* addExpectation = [self expectationWithDescription:@"add credential"];
    [_credentialStore addCredential:credential withAccessPolicy:[[SFAccessPolicy alloc] initWithAccessibility:SFAccessibilityMakeWithMode(SFAccessibleWhenUnlocked) sharingPolicy:SFSharingPolicyWithTrustedDevices] resultHandler:^(NSString* persistentIdentifier, NSError* error) {
        XCTAssertNotNil(persistentIdentifier, @"failed to get persistent identifier for added credential");
        XCTAssertNil(error, @"received unexpected error attempting to add credential to store: %@", error);
        [addExpectation fulfill];
    }];
    [self waitForExpectations:@[addExpectation] timeout:5.0];
    
    SFServiceIdentifier* serviceIdentifier = [SFServiceIdentifier serviceIdentifierForDomain:@"testdomain.com"];
    if (!serviceIdentifier) {
        XCTAssertTrue(false, @"Failed to create a service identifier; aborting test");
        return;
    }

    XCTestExpectation* lookupExpecation = [self expectationWithDescription:@"lookup credential"];
    [_credentialStore lookupCredentialsForServiceIdentifiers:@[serviceIdentifier] withResultHandler:^(NSArray<SFCredential*>* results, NSError* error) {
        XCTAssertEqual((int)results.count, 1, @"error looking up credentials with service identifiers; expected 1 result but got %d", (int)results.count);
        XCTAssertTrue([self passwordCredential:credential matchesCredential:(SFPasswordCredential*)results.firstObject], @"the credential we looked up does not match the one we added");
        [lookupExpecation fulfill];
    }];
    [self waitForExpectations:@[lookupExpecation] timeout:5.0];
}

- (void)testAddDuplicateCredential
{
    SFPasswordCredential* credential = [[SFPasswordCredential alloc] initWithUsername:@"TestUser" password:@"TestPass" primaryServiceIdentifier:[SFServiceIdentifier serviceIdentifierForDomain:@"testdomain.com"]];

    XCTestExpectation* addExpectation = [self expectationWithDescription:@"add credential"];
    [_credentialStore addCredential:credential withAccessPolicy:[[SFAccessPolicy alloc] initWithAccessibility:SFAccessibilityMakeWithMode(SFAccessibleWhenUnlocked) sharingPolicy:SFSharingPolicyWithTrustedDevices] resultHandler:^(NSString* persistentIdentifier, NSError* error) {
        XCTAssertNotNil(persistentIdentifier, @"failed to get persistent identifier for added credential");
        XCTAssertNil(error, @"received unexpected error attempting to add credential to store: %@", error);
        [addExpectation fulfill];
    }];
    [self waitForExpectations:@[addExpectation] timeout:5.0];

    XCTestExpectation* conflictingAddExpectation = [self expectationWithDescription:@"add conflicting item"];
    SFCredential* conflictingCredential = [[SFPasswordCredential alloc] initWithUsername:@"TestUser" password:@"DifferentPassword" primaryServiceIdentifier:[SFServiceIdentifier serviceIdentifierForDomain:@"testdomain.com"]];
    [_credentialStore addCredential:conflictingCredential withAccessPolicy:[[SFAccessPolicy alloc] initWithAccessibility:SFAccessibilityMakeWithMode(SFAccessibleWhenUnlocked) sharingPolicy:SFSharingPolicyWithTrustedDevices] resultHandler:^(NSString* persistentIdentifier, NSError* error) {
        XCTAssertNil(persistentIdentifier, @"adding a credential seems to have succeeded when we expected it to fail");
        XCTAssertNotNil(error, @"failed to get error when adding a credential that should be rejected as a duplicate entry");
        XCTAssertEqualObjects(error.domain, SFKeychainErrorDomain, @"duplicate error domain is not SFKeychainErrorDomain");
        XCTAssertEqual(error.code, SFKeychainErrorDuplicateItem, @"duplicate error is not SFKeychainErrorDuplicateItem");
        [conflictingAddExpectation fulfill];
    }];
    [self waitForExpectations:@[conflictingAddExpectation] timeout:5.0];
}

- (void)testRemoveCredential
{
    SFPasswordCredential* credential = [[SFPasswordCredential alloc] initWithUsername:@"TestUser" password:@"TestPass" primaryServiceIdentifier:[SFServiceIdentifier serviceIdentifierForDomain:@"testdomain.com"]];
    
    __block NSString* newItemPersistentIdentifier = nil;
    XCTestExpectation* addExpectation = [self expectationWithDescription:@"add credential"];
    [_credentialStore addCredential:credential withAccessPolicy:[[SFAccessPolicy alloc] initWithAccessibility:SFAccessibilityMakeWithMode(SFAccessibleWhenUnlocked) sharingPolicy:SFSharingPolicyWithTrustedDevices] resultHandler:^(NSString* persistentIdentifier, NSError* error) {
        XCTAssertNotNil(persistentIdentifier, @"failed to get persistent identifier for added credential");
        XCTAssertNil(error, @"received unexpected error attempting to add credential to store: %@", error);
        
        newItemPersistentIdentifier = persistentIdentifier;
        [addExpectation fulfill];
    }];
    [self waitForExpectations:@[addExpectation] timeout:5.0];

    XCTestExpectation* removeExpectation = [self expectationWithDescription:@"remove credential"];
    [_credentialStore removeCredentialWithPersistentIdentifier:newItemPersistentIdentifier withResultHandler:^(BOOL success, NSError* error) {
        XCTAssertTrue(success, @"failed to remove credential from store");
        XCTAssertNil(error, @"encountered error attempting to remove credential from store: %@", error);
        [removeExpectation fulfill];
    }];
    [self waitForExpectations:@[removeExpectation] timeout:5.0];

    XCTestExpectation* removeAgainExpectation = [self expectationWithDescription:@"remove credential gain"];
    [_credentialStore removeCredentialWithPersistentIdentifier:newItemPersistentIdentifier withResultHandler:^(BOOL success, NSError* error) {
        XCTAssertFalse(success, @"somehow succeeded at removing a credential that we'd already deleted");
        XCTAssertNotNil(error, @"failed to get an error attempting to remove credential from store when there should not be a credential to delete");
        [removeAgainExpectation fulfill];
    }];

    XCTestExpectation* fetchExpectation = [self expectationWithDescription:@"fetch credential"];
    [_credentialStore fetchPasswordCredentialForPersistentIdentifier:newItemPersistentIdentifier withResultHandler:^(SFPasswordCredential* fetchedCredential, NSString* password, NSError* error) {
        XCTAssertNil(fetchedCredential, @"found credential that we expected to be deleted");
        XCTAssertNil(password, @"found password when credential was supposed to be deleted");
        XCTAssertNotNil(error, "failed to get an error when fetching deleted credential");
        [fetchExpectation fulfill];
    }];
    [self waitForExpectations:@[removeAgainExpectation, fetchExpectation] timeout:5.0];
    
    // now try adding the thing again to make sure that works
    XCTestExpectation* addAgainExpectation = [self expectationWithDescription:@"add credential again"];
    [_credentialStore addCredential:credential withAccessPolicy:[[SFAccessPolicy alloc] initWithAccessibility:SFAccessibilityMakeWithMode(SFAccessibleWhenUnlocked) sharingPolicy:SFSharingPolicyWithTrustedDevices] resultHandler:^(NSString* persistentIdentifier, NSError* error) {
        XCTAssertNotNil(persistentIdentifier, @"failed to get persistent identifier for added credential");
        XCTAssertNil(error, @"received unexpected error attempting to add credential to store: %@", error);
        XCTAssertNotEqual(persistentIdentifier, newItemPersistentIdentifier, @"the added credential has the same persistent identifier as the item we already deleted");
        
        newItemPersistentIdentifier = persistentIdentifier;
        [addAgainExpectation fulfill];
    }];
    [self waitForExpectations:@[addAgainExpectation] timeout:5.0];

    XCTestExpectation* fetchAgainExpectation = [self expectationWithDescription:@"fetch credential again"];
    [_credentialStore fetchPasswordCredentialForPersistentIdentifier:newItemPersistentIdentifier withResultHandler:^(SFPasswordCredential* fetchedCredential, NSString* password, NSError* error) {
        XCTAssertNotNil(fetchedCredential, @"failed to fetch credential just added to store");
        XCTAssertNil(error, @"received unexpected error fetching credential from store: %@", error);
        XCTAssertTrue([self passwordCredential:credential matchesCredential:fetchedCredential], @"the credential we fetched from the store does not match the one we added");
        XCTAssertEqualObjects(password, @"TestPass", @"the password we fetched from the store does not match the one we added");
        [fetchAgainExpectation fulfill];
    }];
    [self waitForExpectations:@[fetchAgainExpectation] timeout:5.0];
}

- (void)testRemoveCredentialWithBadPersistentIdentifier
{
    SFPasswordCredential* credential = [[SFPasswordCredential alloc] initWithUsername:@"TestUser" password:@"TestPass" primaryServiceIdentifier:[SFServiceIdentifier serviceIdentifierForDomain:@"testdomain.com"]];
    
    __block NSString* newItemPersistentIdentifier = nil;
    XCTestExpectation* addExpectation = [self expectationWithDescription:@"add credential"];
    [_credentialStore addCredential:credential withAccessPolicy:[[SFAccessPolicy alloc] initWithAccessibility:SFAccessibilityMakeWithMode(SFAccessibleWhenUnlocked) sharingPolicy:SFSharingPolicyWithTrustedDevices] resultHandler:^(NSString* persistentIdentifier, NSError* error) {
        XCTAssertNotNil(persistentIdentifier, @"failed to get persistent identifier for added credential");
        XCTAssertNil(error, @"received unexpected error attempting to add credential to store: %@", error);
        
        newItemPersistentIdentifier = persistentIdentifier;
        [addExpectation fulfill];
    }];
    [self waitForExpectations:@[addExpectation] timeout:5.0];
    
    NSString* wrongPersistentIdentifier = [[NSUUID UUID] UUIDString];
    XCTestExpectation* removeWrongIdentifierEsxpectation = [self expectationWithDescription:@"remove wrong persistent identifier"];
    [_credentialStore removeCredentialWithPersistentIdentifier:wrongPersistentIdentifier withResultHandler:^(BOOL success, NSError* error) {
        XCTAssertFalse(success, @"reported success deleting a credential that was never there");
        XCTAssertNotNil(error, @"failed to get error when attempting to delete an item with an erroneous persistent identifier");
        [removeWrongIdentifierEsxpectation fulfill];
    }];
    
    NSString* notEvenAUUIDString = @"badstring";
    XCTestExpectation* removeNonUUIDIdentifierExpectation = [self expectationWithDescription:@"remove non-uuid string identifier"];
    [_credentialStore removeCredentialWithPersistentIdentifier:notEvenAUUIDString withResultHandler:^(BOOL success, NSError* error) {
        XCTAssertFalse(success, @"reported success deleting a credential with a malformed persistent identifier");
        XCTAssertNotNil(error, @"failed to get error when attempting to delete an item with a malformed persistent identifier");
        XCTAssertEqualObjects(error.domain, SFKeychainErrorDomain);
        XCTAssertEqual(error.code, SFKeychainErrorInvalidPersistentIdentifier);
        [removeNonUUIDIdentifierExpectation fulfill];
    }];
    [self waitForExpectations:@[removeWrongIdentifierEsxpectation, removeNonUUIDIdentifierExpectation] timeout:5.0];
}

#pragma clang diagnostic pop

@end

#endif
