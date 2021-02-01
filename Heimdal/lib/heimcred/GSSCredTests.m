/*-
* Copyright (c) 2013 Kungliga Tekniska Högskolan
* (Royal Institute of Technology, Stockholm, Sweden).
* All rights reserved.
*
* Portions Copyright (c) 2013,2020 Apple Inc. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
* 1. Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
* 2. Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the
*    documentation and/or other materials provided with the distribution.
*
* THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
* OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
* LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
* OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
* SUCH DAMAGE.
*/

#import <Foundation/Foundation.h>
#import <TargetConditionals.h>
#import "GSSCredTestUtil.h"
#import <XCTest/XCTest.h>
#import "gsscred.h"
#import "hc_err.h"
#import "common.h"
#import "heimbase.h"
#import "heimcred.h"
#import "aks.h"
#import "mock_aks.h"

@interface GSSCredTests : XCTestCase
@property (nullable) struct peer * peer;
@property (nonatomic) MockManagedAppManager *mockManagedAppManager;
@end

@implementation GSSCredTests {
}
@synthesize peer;
@synthesize mockManagedAppManager;

- (void)setUp {
    
    self.mockManagedAppManager = [[MockManagedAppManager alloc] init];
    
    HeimCredGlobalCTX.isMultiUser = NO;
    HeimCredGlobalCTX.currentAltDSID = currentAltDSIDMock;
    HeimCredGlobalCTX.hasEntitlement = haveBooleanEntitlementMock;
    HeimCredGlobalCTX.getUid = getUidMock;
    HeimCredGlobalCTX.getAsid = getAsidMock;
    HeimCredGlobalCTX.encryptData = ksEncryptData;
    HeimCredGlobalCTX.decryptData = ksDecryptData;
    HeimCredGlobalCTX.managedAppManager = self.mockManagedAppManager;
    HeimCredGlobalCTX.useUidMatching = NO;
    HeimCredGlobalCTX.verifyAppleSigned = verifyAppleSignedMock;
    HeimCredGlobalCTX.sessionExists = sessionExistsMock;
    HeimCredGlobalCTX.saveToDiskIfNeeded = saveToDiskIfNeededMock;
    HeimCredGlobalCTX.getValueFromPreferences = getValueFromPreferencesMock;
    HeimCredGlobalCTX.notifyCaches = NULL;
    HeimCredGlobalCTX.gssCredHelperClientClass = nil;
    
    HeimCredCTX.mechanisms = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    heim_assert(HeimCredCTX.mechanisms != NULL, "out of memory");
    
    HeimCredCTX.schemas = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    heim_assert(HeimCredCTX.schemas != NULL, "out of memory");
    
    HeimCredCTX.globalSchema = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    heim_assert(HeimCredCTX.globalSchema != NULL, "out of memory");
    
    _HeimCredRegisterGeneric();
    _HeimCredRegisterConfiguration();
    _HeimCredRegisterKerberos();
    _HeimCredRegisterKerberosAcquireCred();
    _HeimCredRegisterNTLM();
    
    CFRELEASE_NULL(HeimCredCTX.globalSchema);
    
#if TARGET_OS_SIMULATOR
    archivePath = [[NSString alloc] initWithFormat:@"%@/Library/Caches/com.apple.GSSCred.simulator-archive.test", NSHomeDirectory()];
#else
    archivePath = @"/var/tmp/heim-credential-store.archive.test";
#endif
    HeimCredCTX.sessions = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    _HeimCredInitCommon();
    
    //always start clean
    NSError *error;
    [[NSFileManager defaultManager] removeItemAtPath:archivePath error:&error];
    
    readCredCache();
    
    //default test values
    _entitlements = @[];
    _currentUid = 501;
    _altDSID = NULL;
    _currentAsid = 10000;
}

- (void)tearDown {
    
    NSError *error;
    [[NSFileManager defaultManager] removeItemAtPath:archivePath error:&error];
    [GSSCredTestUtil freePeer:self.peer];
    self.peer = NULL;
    
    CFRELEASE_NULL(HeimCredCTX.sessions);
    HeimCredCTX.sessions = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
}

//pragma mark - Tests

//add credential and fetch it
- (void)testCreatingAndFetchingCredential {
    HeimCredGlobalCTX.isMultiUser = NO;
    HeimCredGlobalCTX.useUidMatching = NO;
    [GSSCredTestUtil freePeer:self.peer];
    self.peer = [GSSCredTestUtil createPeer:@"com.apple.fake" identifier:0];

    CFUUIDRef uuid = NULL;
    BOOL worked = [GSSCredTestUtil createCredentialAndCache:self.peer name:@"test@EXAMPLE.COM" returningCacheUuid:&uuid];
    
    XCTAssertTrue(worked, "Credential should be created successfully");
    
    CFDictionaryRef attributes;
    worked = [GSSCredTestUtil fetchCredential:self.peer uuid:uuid returningDictionary:&attributes];
    XCTAssertTrue(worked, "Credential should be fetched successfully using it's uuid");
    
}

//add credential and fetch it
- (void)testCreatingCredentialWithRemovingDuplicates {
    HeimCredGlobalCTX.isMultiUser = NO;
    [GSSCredTestUtil freePeer:self.peer];
    self.peer = [GSSCredTestUtil createPeer:@"com.apple.fake" identifier:0];

    CFUUIDRef uuid = NULL;
    BOOL worked = [GSSCredTestUtil createCredentialAndCache:self.peer name:@"test@EXAMPLE.COM" returningCacheUuid:&uuid];
    
    XCTAssertTrue(worked, "Credential should be created successfully");
    
    NSDictionary *childAttributes = @{ (id)kHEIMAttrParentCredential:(__bridge id)uuid,
				       (id)kHEIMAttrLeadCredential:@YES,
				       (id)kHEIMAttrAuthTime:[NSDate date],
				       (id)kHEIMAttrServerName:@"krbtgt/EXAMPLE.COM@EXAMPLE.COM",
				       (id)kHEIMAttrData:(id)[@"this is fake data" dataUsingEncoding:NSUTF8StringEncoding],
    };
    
    XCTAssertEqual([GSSCredTestUtil itemCount:self.peer], 2, "There should be one parent and one child");
    CFUUIDRef uuid1 = NULL;
    worked = [GSSCredTestUtil createCredential:self.peer name:@"test@EXAMPLE.COM" attributes:(__bridge CFDictionaryRef)childAttributes returningUuid:&uuid1];
    XCTAssertTrue(worked, "child credential with duplicate servername should be created successfully");
    
    XCTAssertEqual([GSSCredTestUtil itemCount:self.peer], 2, "the additional server name should not increase item count");
    
    CFUUIDRef uuid2 = NULL;
    worked = [GSSCredTestUtil createCredentialAndCache:self.peer name:@"test@EXAMPLE.COM" returningCacheUuid:&uuid2];
    XCTAssertTrue(worked, "Another Credential with the same name should be created successfully");
    
    XCTAssertEqual([GSSCredTestUtil itemCount:self.peer], 4, "There should be 2 entries and 2 child items");
    
    NSDictionary *nextChildAttributes = @{ (id)kHEIMAttrParentCredential:(__bridge id)uuid2,
				       (id)kHEIMAttrLeadCredential:@YES,
				       (id)kHEIMAttrAuthTime:[NSDate date],
				       (id)kHEIMAttrServerName:@"krbtgt/EXAMPLE.COM@EXAMPLE.COM",
				       (id)kHEIMAttrData:(id)[@"this is fake data" dataUsingEncoding:NSUTF8StringEncoding],
    };
    CFUUIDRef uuid3 = NULL;
    worked = [GSSCredTestUtil createCredential:self.peer name:@"test@EXAMPLE.COM" attributes:(__bridge CFDictionaryRef)nextChildAttributes returningUuid:&uuid3];
    XCTAssertTrue(worked, "Another child item for the same client name should be made correctly");
    
    XCTAssertEqual([GSSCredTestUtil itemCount:self.peer], 4, "There should still be 2 entries and 2 child items");

    uint64_t error = [GSSCredTestUtil delete:self.peer uuid:uuid];
    XCTAssertEqual(error, 0, "deleting the first credential should work too");
    XCTAssertEqual([GSSCredTestUtil itemCount:self.peer], 2, "deleting the first credential should leave 2 items");
    
    error = [GSSCredTestUtil delete:self.peer uuid:uuid2];
    XCTAssertEqual(error, 0, "deleting the second credential should work too");
    XCTAssertEqual([GSSCredTestUtil itemCount:self.peer], 0);
    
}

//pragma mark - ACL Tests

//add credential no acl, check default
- (void)testCreatingAndFetchingWithDefaultACL {
    HeimCredGlobalCTX.isMultiUser = NO;
    [GSSCredTestUtil freePeer:self.peer];
    self.peer = [GSSCredTestUtil createPeer:@"com.apple.fake" identifier:0];

    CFUUIDRef uuid = NULL;
    BOOL worked = [GSSCredTestUtil createCredentialAndCache:self.peer name:@"test@EXAMPLE.COM" returningCacheUuid:&uuid];
    
    XCTAssertTrue(worked, "Credential should be created successfully");
    
    CFDictionaryRef attributes;
    worked = [GSSCredTestUtil fetchCredential:self.peer uuid:uuid returningDictionary:&attributes];
    XCTAssertTrue(worked, "Credential should be fetched successfully using it's uuid");
    NSArray *acl = CFDictionaryGetValue(attributes, kHEIMAttrBundleIdentifierACL);
    
    self.peer->bundleID = CFSTR("com.apple.foo");
    worked = [GSSCredTestUtil fetchCredential:self.peer uuid:uuid returningDictionary:&attributes];
    
#if (TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR)
    XCTAssertFalse(worked, "Credential should not be accessible from a different app");
    XCTAssertTrue([acl containsObject:@"com.apple.fake"], "The ACL should contain the calling app");
    XCTAssertFalse([acl containsObject:@"*"], "The ACL should not contain a wildcard");
    XCTAssertTrue(acl.count == 1, "The ACL should contain two entries");
#elif TARGET_OS_OSX
    XCTAssertTrue(worked, "Credentials should be available to all apps on MacOS");
    XCTAssertFalse([acl containsObject:@"com.apple.fake"], "The ACL should not contain the calling app");
    XCTAssertTrue([acl containsObject:@"*"], "The ACL should contain a wildcard");
    XCTAssertTrue(acl.count == 1, "The ACL should contain two entries");
#else
    XCTAssertFalse(worked, "Credential should not be accessible from a different app in sumulators");
    XCTAssertTrue([acl containsObject:@"com.apple.fake"], "The ACL should contain the calling app");
    XCTAssertFalse([acl containsObject:@"*"], "The ACL should not contain a wildcard");
    XCTAssertTrue(acl.count == 1, "The ACL should contain two entries");
#endif
   
}

//add credential with ACL, no entitlement
- (void)testCreatingWithWildcardACLNotCorrectSourceApp {
    HeimCredGlobalCTX.isMultiUser = NO;
    _entitlements = @[];
    [GSSCredTestUtil freePeer:self.peer];
    self.peer = [GSSCredTestUtil createPeer:@"com.apple.fake" identifier:0];

    CFUUIDRef uuid=NULL;
    BOOL worked = [GSSCredTestUtil createCredential:self.peer name:@"test@EXAMPLE.COM" attributes:NULL returningUuid:&uuid];
    XCTAssertTrue(worked, "Credential is created");
    
    NSDictionary *attributes = @{(id)kHEIMAttrBundleIdentifierACL:@[@"*"]};
    uint64_t result = [GSSCredTestUtil setAttributes:self.peer uuid:uuid attributes:(__bridge CFDictionaryRef)(attributes) returningDictionary:NULL];

#if (TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR)
    XCTAssertEqual(result, kHeimCredErrorUpdateNotAllowed, "Saving wildcard ACL should only be available from certain apps");
#elif TARGET_OS_OSX
    XCTAssertEqual(result, 0, "Saving wildcard ACL should be the default on MacOS");
#else
    XCTAssertEqual(result, 0, "Saving wildcard ACL should be the default in sumulators");
#endif
}

//add credential with ACL, unauthorized source app
- (void)testCreatingWithMismatchedBundleACLNotCorrectSourceApp {
    HeimCredGlobalCTX.isMultiUser = NO;
    _entitlements = @[];
    [GSSCredTestUtil freePeer:self.peer];
    self.peer = [GSSCredTestUtil createPeer:@"com.apple.fake" identifier:0];

    CFUUIDRef uuid=NULL;
    BOOL worked = [GSSCredTestUtil createCredential:self.peer name:@"test@EXAMPLE.COM"attributes:NULL returningUuid:&uuid];
    XCTAssertTrue(worked, "Credential is created");
    
    NSDictionary *attributes = @{(id)kHEIMAttrBundleIdentifierACL:@[@"com.apple.foo"]};
    uint64_t result = [GSSCredTestUtil setAttributes:self.peer uuid:uuid attributes:(__bridge CFDictionaryRef)(attributes) returningDictionary:NULL];
    
#if (TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR)
    XCTAssertEqual(result, kHeimCredErrorUpdateNotAllowed, "Saving ACL should only be available from certain apps");
#elif TARGET_OS_OSX
    XCTAssertEqual(result, 0, "Credentials should be available to all apps on MacOS");
#else
   XCTAssertEqual(result, 0, "Credentials should be available to all apps in sumulators");
#endif
}

//add credential with wildcard ACL, authorized source app
- (void)testCreatingandFetchingWithWildcardACLCorrectSourceApp {
    HeimCredGlobalCTX.isMultiUser = NO;
    _entitlements = @[@"com.apple.private.gssapi.allowwildcardacl"];
    [GSSCredTestUtil freePeer:self.peer];
    self.peer = [GSSCredTestUtil createPeer:@"com.apple.mobilesafari" callingBundleId:@"com.apple.AppSSOKerberos.KerberosExtension" identifier:0];

    CFUUIDRef uuid=NULL;
    BOOL worked = [GSSCredTestUtil createCredential:self.peer name:@"test@EXAMPLE.COM" attributes:NULL returningUuid:&uuid];
    XCTAssertTrue(worked, "Credential is created");
    
    NSDictionary *attributes = @{(id)kHEIMAttrBundleIdentifierACL:@[@"*"]};
    uint64_t result = [GSSCredTestUtil setAttributes:self.peer uuid:uuid attributes:(__bridge CFDictionaryRef)(attributes) returningDictionary:NULL];
    XCTAssertEqual(result, 0, "Kerberos Extension should be able to set wildcard acl");
    
    CFDictionaryRef returnAttributes;
    worked = [GSSCredTestUtil fetchCredential:self.peer uuid:uuid returningDictionary:&returnAttributes];
    NSArray *acl = CFDictionaryGetValue(returnAttributes, kHEIMAttrBundleIdentifierACL);
    XCTAssertTrue([acl containsObject:@"*"], "The ACL should contain a wildcard");
    XCTAssertTrue([acl containsObject:@"com.apple.mobilesafari"], "The ACL should contain the first app");
    XCTAssertTrue(acl.count == 2, "The ACL should contain two entries");
    CFRELEASE_NULL(returnAttributes);
    
    self.peer->callingAppBundleID = CFSTR("com.apple.accountsd");
    _entitlements = @[];
    CFUUIDRef uuid2=NULL;
    worked = [GSSCredTestUtil createCredential:self.peer name:@"test@EXAMPLE.COM"attributes:NULL returningUuid:&uuid2];
    XCTAssertTrue(worked, "Credential should be created");
    
    attributes = @{(id)kHEIMAttrBundleIdentifierACL:@[@"*"]};
    result = [GSSCredTestUtil setAttributes:self.peer uuid:uuid2 attributes:(__bridge CFDictionaryRef)(attributes) returningDictionary:NULL];
    XCTAssertEqual(result, 0, "Accountsd should be able to set wildcard acl");
    worked = [GSSCredTestUtil fetchCredential:self.peer uuid:uuid2 returningDictionary:&returnAttributes];
    acl = CFDictionaryGetValue(returnAttributes, kHEIMAttrBundleIdentifierACL);
    XCTAssertTrue([acl containsObject:@"*"], "The ACL should contain a wildcard");
    XCTAssertTrue([acl containsObject:@"com.apple.mobilesafari"], "The ACL should contain the first app");
    XCTAssertTrue(acl.count == 2, "The ACL should contain two entries");
    CFRELEASE_NULL(returnAttributes);
    
    [GSSCredTestUtil freePeer:self.peer];
    self.peer = [GSSCredTestUtil createPeer:@"com.apple.foo" identifier:0];
    NSArray *items;
    worked = [GSSCredTestUtil queryAllKerberos:self.peer returningArray:&items];
    XCTAssertTrue(worked, "Credential should be fetched successfully using it's bundleid");
    XCTAssertEqual(items.count, 2, "Two credentials should be added");
    
    
}
//add credential with non-wildcard ACL, authorized source app
- (void)testCreatingandFetchingWithBundleACLCorrectSourceApp {
    HeimCredGlobalCTX.isMultiUser = NO;
    _entitlements = @[@"com.apple.private.gssapi.allowwildcardacl"];
    [GSSCredTestUtil freePeer:self.peer];
    self.peer = [GSSCredTestUtil createPeer:@"com.apple.mobilesafari" callingBundleId:@"com.apple.AppSSOKerberos.KerberosExtension" identifier:0];

    CFUUIDRef uuid=NULL;
    BOOL worked = [GSSCredTestUtil createCredential:self.peer name:@"test@EXAMPLE.COM"attributes:NULL returningUuid:&uuid];
    XCTAssertTrue(worked, "Credential is created");
    
    NSDictionary *attributes = @{(id)kHEIMAttrBundleIdentifierACL:@[@"com.apple.foo"]};
    uint64_t result = [GSSCredTestUtil setAttributes:self.peer uuid:uuid attributes:(__bridge CFDictionaryRef)(attributes) returningDictionary:NULL];
    XCTAssertEqual(result, 0, "Kerberos Extension should be able to set non matching bundle acl");
    
    CFDictionaryRef returnAttributes;
    worked = [GSSCredTestUtil fetchCredential:self.peer uuid:uuid returningDictionary:&returnAttributes];
    NSArray *acl = CFDictionaryGetValue(returnAttributes, kHEIMAttrBundleIdentifierACL);
    XCTAssertTrue([acl containsObject:@"com.apple.mobilesafari"], "The ACL should contain the first app");
    XCTAssertTrue([acl containsObject:@"com.apple.foo"], "The ACL should contain the second app");
    XCTAssertTrue(acl.count == 2, "The ACL should contain two entries");
    CFRELEASE_NULL(returnAttributes);
    
    _entitlements = @[];
    [GSSCredTestUtil freePeer:self.peer];
    self.peer = [GSSCredTestUtil createPeer:@"com.apple.foo" identifier:0];
    NSArray *items;
    worked = [GSSCredTestUtil queryAllKerberos:self.peer returningArray:&items];
    XCTAssertTrue(worked, "Credential should be fetched successfully using it's bundleid");
    XCTAssertEqual(items.count, 1, "One credential should match the ACL");
    
    [GSSCredTestUtil freePeer:self.peer];
    self.peer = [GSSCredTestUtil createPeer:@"com.apple.bar" identifier:0];
    worked = [GSSCredTestUtil queryAllKerberos:self.peer returningArray:&items];
    XCTAssertTrue(worked, "Credential fetch for with results should not fail");
    XCTAssertEqual(items.count, 0, "This credential should not match the acl");
}

//fetch credential matching is managed app ACL
- (void)testCreatingandFetchingWithManagedAppACL {
    HeimCredGlobalCTX.isMultiUser = NO;
    _entitlements = @[@"com.apple.private.gssapi.allowwildcardacl"];
    self.mockManagedAppManager.managedApps = @[@"com.apple.foo"];
    [GSSCredTestUtil freePeer:self.peer];
    self.peer = [GSSCredTestUtil createPeer:@"com.apple.mobilesafari" callingBundleId:@"com.apple.AppSSOKerberos.KerberosExtension" identifier:0];
   
    CFUUIDRef uuid=NULL;
    BOOL worked = [GSSCredTestUtil createCredential:self.peer name:@"test@EXAMPLE.COM"attributes:NULL returningUuid:&uuid];
    XCTAssertTrue(worked, "Credential is created");
    
    NSDictionary *attributes = @{(id)kHEIMAttrBundleIdentifierACL:@[@"com.apple.private.gssapi.allowmanagedapps"]};
    uint64_t result = [GSSCredTestUtil setAttributes:self.peer uuid:uuid attributes:(__bridge CFDictionaryRef)(attributes) returningDictionary:NULL];
    XCTAssertEqual(result, 0, "Kerberos Extension should be able to set managed app bundle acl");
    
    [GSSCredTestUtil freePeer:self.peer];
    self.peer = [GSSCredTestUtil createPeer:@"com.apple.foo" identifier:0];
    NSArray *items;
    worked = [GSSCredTestUtil queryAllKerberos:self.peer returningArray:&items];
    XCTAssertTrue(worked, "Credential should be fetched successfully as a managed app");
#if TARGET_OS_IOS
    XCTAssertEqual(items.count, 1, "The managed app should find the credential");
#else
    XCTAssertEqual(items.count, 0, "All apps should be considered unmanaged when not iOS");
#endif
    
    [GSSCredTestUtil freePeer:self.peer];
    self.peer = [GSSCredTestUtil createPeer:@"com.apple.bar" identifier:0];
    worked = [GSSCredTestUtil queryAllKerberos:self.peer returningArray:&items];
    XCTAssertTrue(worked, "Credential fetch for with results should not fail");
    XCTAssertEqual(items.count, 0, "The unmanaged app should not find the credential");
}

//fetch credential matching another bundle with is managed app ACL
- (void)testCreatingandFetchingWithAnotherAppWithManagedAppACL {
    HeimCredGlobalCTX.isMultiUser = NO;
    _entitlements = @[@"com.apple.private.gssapi.allowwildcardacl"];
    self.mockManagedAppManager.managedApps = @[@"com.apple.foo"];
    [GSSCredTestUtil freePeer:self.peer];
    self.peer = [GSSCredTestUtil createPeer:@"com.apple.mobilesafari" callingBundleId:@"com.apple.AppSSOKerberos.KerberosExtension" identifier:0];

    CFUUIDRef uuid=NULL;
    BOOL worked = [GSSCredTestUtil createCredential:self.peer name:@"test@EXAMPLE.COM"attributes:NULL returningUuid:&uuid];
    XCTAssertTrue(worked, "Credential is created");
    
    NSDictionary *attributes = @{(id)kHEIMAttrBundleIdentifierACL:@[@"com.apple.private.gssapi.allowmanagedapps", @"com.apple.bar"]};
    uint64_t result = [GSSCredTestUtil setAttributes:self.peer uuid:uuid attributes:(__bridge CFDictionaryRef)(attributes) returningDictionary:NULL];
    XCTAssertEqual(result, 0, "Kerberos Extension should be able to set managed app bundle acl");
    
    [GSSCredTestUtil freePeer:self.peer];
    _entitlements = @[];
    self.peer = [GSSCredTestUtil createPeer:@"com.apple.foo" identifier:0];
    NSArray *items;
    worked = [GSSCredTestUtil queryAllKerberos:self.peer returningArray:&items];
    XCTAssertTrue(worked, "Credential should be fetched successfully as a managed app");
#if TARGET_OS_IOS
    XCTAssertEqual(items.count, 1, "The managed app should find the credential");
#else
    XCTAssertEqual(items.count, 0, "All apps should be considered unmanaged when not iOS");
#endif
    
    [GSSCredTestUtil freePeer:self.peer];
    self.peer = [GSSCredTestUtil createPeer:@"com.apple.bar" identifier:0];
    worked = [GSSCredTestUtil queryAllKerberos:self.peer returningArray:&items];
    XCTAssertTrue(worked, "Credential fetch for non managed bundle id should not fail");
    XCTAssertEqual(items.count, 1, "the non managed bundleid in the ACL should work too");
    
    [GSSCredTestUtil freePeer:self.peer];
    self.peer = [GSSCredTestUtil createPeer:@"com.apple.notthere" identifier:0];
    worked = [GSSCredTestUtil queryAllKerberos:self.peer returningArray:&items];
    XCTAssertTrue(worked, "Credential fetch for wrong bundle should not fail");
    XCTAssertEqual(items.count, 0, "Apps not in the acl should find nothing");
}

//pragma mark - MultiUser Tests
#if TARGET_OS_IOS && HAVE_MOBILE_KEYBAG_SUPPORT
- (void)testMultiUserCreatingAndFetchingCredential {
    [self addTeardownBlock:^{
	NSError *error;
	[[NSFileManager defaultManager] removeItemAtPath:archivePath error:&error];
	readCredCache();
    }];
    
    _altDSID = @"abcd1234";
    HeimCredGlobalCTX.isMultiUser = YES;
    [GSSCredTestUtil freePeer:self.peer];
    self.peer = [GSSCredTestUtil createPeer:@"com.apple.fake" identifier:0];

    CFUUIDRef uuid = NULL;
    int64_t worked = [GSSCredTestUtil createCredentialAndCache:self.peer name:@"test@EXAMPLE.COM" returningCacheUuid:&uuid];
    
    XCTAssertTrue(worked, "Credential should be created successfully");
    
    CFDictionaryRef attributes;
    worked = [GSSCredTestUtil fetchCredential:self.peer uuid:uuid returningDictionary:&attributes];
    XCTAssertTrue(worked, "Credential should be fetched successfully using it's uuid");
    
    CFStringRef storedAltDSID = CFDictionaryGetValue(attributes, kHEIMAttrAltDSID);
    XCTAssertTrue([@"abcd1234" isEqualToString:(__bridge NSString *) storedAltDSID], "Stored altdsid should be the current value");
    
}

- (void)testMultiUserFetchRootOrNot {
    [self addTeardownBlock:^{
	NSError *error;
	[[NSFileManager defaultManager] removeItemAtPath:archivePath error:&error];
	readCredCache();
    }];
    
    HeimCredGlobalCTX.isMultiUser = YES;
    _currentUid = 501;
    _altDSID = @"abcd1234";
    [GSSCredTestUtil freePeer:self.peer];
    self.peer = [GSSCredTestUtil createPeer:@"com.apple.fake" identifier:0];

    CFUUIDRef uuid = NULL;
    BOOL worked = [GSSCredTestUtil createCredentialAndCache:self.peer name:@"test@EXAMPLE.COM" returningCacheUuid:&uuid];
    XCTAssertTrue(worked, "Credential should be created successfully");
    
    _altDSID = @"foo";
    CFRELEASE_NULL(self.peer->currentDSID);
    worked = [GSSCredTestUtil createCredentialAndCache:self.peer name:@"test@EXAMPLE.COM" returningCacheUuid:&uuid];
    XCTAssertTrue(worked, "Credential should be created successfully");
    
    XCTAssertEqual([GSSCredTestUtil itemCount:self.peer], 4);
    
    _altDSID = @"abcd1234";
    _currentUid = 501;
    [GSSCredTestUtil freePeer:self.peer];
    self.peer = [GSSCredTestUtil createPeer:@"com.apple.fake" identifier:0];
    worked = [GSSCredTestUtil fetchCredential:self.peer uuid:uuid];
    XCTAssertFalse(worked, "Credential for wrong alt DSID should only be available to root");
    
    _currentUid = 0;
    worked = [GSSCredTestUtil fetchCredential:self.peer uuid:uuid];
    XCTAssertTrue(worked, "Credential for wrong alt DSID should be available to root");
    
    
}

- (void)testUpdatingAltDSIDShouldFail {
    
    HeimCredGlobalCTX.isMultiUser = YES;
    HeimCredGlobalCTX.useUidMatching = YES;
    _currentUid = 501;
    _altDSID = @"abcd1234";
    [GSSCredTestUtil freePeer:self.peer];
    self.peer = [GSSCredTestUtil createPeer:@"com.apple.fake" identifier:0];
    
    CFUUIDRef uuid = NULL;
    BOOL worked = [GSSCredTestUtil createCredentialAndCache:self.peer name:@"test@EXAMPLE.COM" returningCacheUuid:&uuid];
    XCTAssertTrue(worked, "Credential was created successfully");
    
    NSDictionary *attributes = @{(id)kHEIMAttrAltDSID:@"foo"};
    uint64_t result = [GSSCredTestUtil setAttributes:self.peer uuid:uuid attributes:(__bridge CFDictionaryRef)(attributes) returningDictionary:NULL];
    XCTAssertEqual(result, kHeimCredErrorUpdateNotAllowed, "Updating the altDSID is not allowed.");
    
}
#endif

- (void)testMultiUserDeleteAll {

    [self addTeardownBlock:^{
	NSError *error;
	[[NSFileManager defaultManager] removeItemAtPath:archivePath error:&error];
	readCredCache();
    }];
    
    HeimCredGlobalCTX.isMultiUser = YES;
    _currentUid = 501;
    _altDSID = @"abcd1234";
    [GSSCredTestUtil freePeer:self.peer];
    self.peer = [GSSCredTestUtil createPeer:@"com.apple.fake" identifier:0];

    CFUUIDRef uuid = NULL;
    bool worked = [GSSCredTestUtil createCredentialAndCache:self.peer name:@"test@EXAMPLE.COM" returningCacheUuid:&uuid];
    XCTAssertTrue(worked, "Credential should be created successfully");
    
    _altDSID = @"foo";
    CFRELEASE_NULL(self.peer->currentDSID);
    worked = [GSSCredTestUtil createCredentialAndCache:self.peer name:@"test1@EXAMPLE.COM" returningCacheUuid:&uuid];
    XCTAssertTrue(worked, "Credential should be created successfully");
    
    worked = [GSSCredTestUtil createNTLMCredential:self.peer returningUuid:&uuid];
    XCTAssertTrue(worked, "Credential should be created successfully");
    
    XCTAssertEqual([GSSCredTestUtil itemCount:self.peer], 5); // 2 * 2 Kerberos + 1 NTLM
    
    _altDSID = @"abcd1234";
    CFRELEASE_NULL(self.peer->currentDSID);
    _currentUid = 0;
    _entitlements = @[];
    uint64_t error = [GSSCredTestUtil deleteAll:self.peer dsid:@"abcd1234"];
    
#if TARGET_OS_IPHONE
    XCTAssertEqual(error, kHeimCredErrorMissingEntitlement, "delete all should only be available with the entitlement for root user");
    
    XCTAssertEqual([GSSCredTestUtil itemCount:self.peer], 5, "there should be 5 entries");  //Kerberos 2 * 2 items, + 1 NTLM
    
    _entitlements = @[@"com.apple.private.gssapi.deleteall"];
    error = [GSSCredTestUtil deleteAll:self.peer dsid:@"abcd1234"];
    XCTAssertEqual(error, 0, "delete all should only be available with the entitlement to the root user");
    
    XCTAssertEqual([GSSCredTestUtil itemCount:self.peer], 3, "there should be 3 entries"); //Kerberos 1 * 2 items, + 1 NTLM
    
    worked = [GSSCredTestUtil deleteAll:self.peer dsid:@"foo"];
    XCTAssertEqual(error, 0, "delete all should only be available with the entitlement to root");
    
    XCTAssertEqual([GSSCredTestUtil itemCount:self.peer], 0);
#elif TARGET_OS_OSX
    XCTAssertEqual(error, kHeimCredErrorCommandUnavailable, "Delete all should not be available on MacOS");
#endif
    
}

- (void)testCreatingAndFetchingCredentialWithUidMatching {
    
    /*
     
     uid matching enables the credentails in the table below to be shared.
     
     session 1
     uid           root
     ----------------------------
     | kinit A   | kinit B      |
     | klist ->  | klist -> A,B |
     |   A,B,C   |              |
     ----------------------------

     session 2
     uid           root
     ----------------------------
     | kinit C   | kinit D      |
     | klist ->  | klist -> C,D |
     |   A,C,D   |              |
     ----------------------------

     */
    
    HeimCredGlobalCTX.useUidMatching = YES;
    NSArray *items;
    
    _currentUid = 501;
    _currentAsid = 10001;
    //the first app connection should create, find credetials, and access it
    struct peer *firstPeer = [GSSCredTestUtil createPeer:@"com.apple.fake" identifier:-1];
    CFUUIDRef firstUuid = NULL;
    BOOL worked = [GSSCredTestUtil createCredentialAndCache:firstPeer name:@"test1@EXAMPLE.COM" returningCacheUuid:&firstUuid];
    XCTAssertTrue(worked, "Credential was created successfully");
    
    worked = [GSSCredTestUtil queryAllCredentials:firstPeer returningArray:&items];
    XCTAssertTrue(worked, "Searching should not fail");
    XCTAssertTrue([items containsObject:@"test1@EXAMPLE.COM"], "Credential should be found");
    
    worked = [GSSCredTestUtil fetchCredential:firstPeer uuid:firstUuid];
    XCTAssertTrue(worked, "Credential should be accessible");
    
    // *****
    //sudo to root with same asid as first peer should provide access to the credential
    // *****
    _currentUid = 0;
    _currentAsid = 10001;
    struct peer *secondPeer = [GSSCredTestUtil createPeer:@"com.apple.fake" identifier:-1];
    worked = [GSSCredTestUtil fetchCredential:secondPeer uuid:firstUuid];
    XCTAssertTrue(worked, "Credential from same asid should be found.");

    worked = [GSSCredTestUtil queryAllCredentials:secondPeer returningArray:&items];
    XCTAssertTrue(worked, "Searching should not fail");
    XCTAssertTrue([items containsObject:@"test1@EXAMPLE.COM"], "First credential should be found by sudo");
    
    //credential created with sudo should be found
    CFUUIDRef secondUuid = NULL;
    worked = [GSSCredTestUtil createCredentialAndCache:secondPeer name:@"test2@EXAMPLE.COM" returningCacheUuid:&secondUuid];
    XCTAssertTrue(worked, "Credential from sudo created successfully");
    
    worked = [GSSCredTestUtil queryAllCredentials:secondPeer returningArray:&items];
    XCTAssertTrue(worked, "Searching should not fail");
    XCTAssertTrue([items containsObject:@"test2@EXAMPLE.COM"], "Second credential should be found");
    
    worked = [GSSCredTestUtil fetchCredential:secondPeer uuid:secondUuid];
    XCTAssertTrue(worked, "Credential from sudo should be accessbile");
    
    //credential created with sudo should be found via non sudo because it is using the same asid
    XCTAssertTrue([items containsObject:@"test2@EXAMPLE.COM"], "Credential created by sudo should be found by non-sudo");
    worked = [GSSCredTestUtil fetchCredential:firstPeer uuid:secondUuid];
    XCTAssertTrue(worked, "Credential from sudo should be accessible");
    
    // *****
    // credentials created by the same user should be found by a different asid for the same user.
    // *****
    _currentUid = 501;
    _currentAsid = 10002;
    struct peer *thirdPeer = [GSSCredTestUtil createPeer:@"com.apple.fake" identifier:-1];
    
    worked = [GSSCredTestUtil queryAllCredentials:thirdPeer returningArray:&items];
    XCTAssertTrue(worked, "Searching should not fail");
    XCTAssertTrue([items containsObject:@"test1@EXAMPLE.COM"], "Credential from first connection should be found");
    
    worked = [GSSCredTestUtil fetchCredential:thirdPeer uuid:firstUuid];
    XCTAssertTrue(worked, "Credential from first connection should be accessible");
    
    XCTAssertFalse([items containsObject:@"test2@EXAMPLE.COM"], "Credential from second connection should not be found (different asid, different uid)");
    
    worked = [GSSCredTestUtil fetchCredential:thirdPeer uuid:secondUuid];
    XCTAssertFalse(worked, "Credential from second connection (sudo) should not be accessible");
    
    //credentials from third connection should be created, found and accessbile
    CFUUIDRef thirdUuid = NULL;
    worked = [GSSCredTestUtil createCredentialAndCache:thirdPeer name:@"test3@EXAMPLE.COM" returningCacheUuid:&thirdUuid];
    XCTAssertTrue(worked, "Credential was created successfully");
    
    worked = [GSSCredTestUtil queryAllCredentials:thirdPeer returningArray:&items];
    XCTAssertTrue(worked, "Searching should not fail");
    XCTAssertTrue([items containsObject:@"test3@EXAMPLE.COM"], "Credential from third connection should be found");
    
    worked = [GSSCredTestUtil fetchCredential:thirdPeer uuid:thirdUuid];
    XCTAssertTrue(worked, "Credential from third connection should be accessible");
    
    
    // *****
    // credentials created by the same user should be not found by a different asid for the same user as when using sudo. (different asid)
    // *****
    _currentUid = 0;
    _currentAsid = 10002;
    struct peer *fourthPeer = [GSSCredTestUtil createPeer:@"com.apple.fake" identifier:-1];
    
    worked = [GSSCredTestUtil queryAllCredentials:fourthPeer returningArray:&items];
    XCTAssertTrue(worked, "Searching should not fail");
    XCTAssertFalse([items containsObject:@"test1@EXAMPLE.COM"], "Credential from first connection should be found (different uid, different asid)");
    worked = [GSSCredTestUtil fetchCredential:fourthPeer uuid:firstUuid];
    XCTAssertFalse(worked, "Credential from first connection should be not accessible (different asid)");

    XCTAssertFalse([items containsObject:@"test2@EXAMPLE.COM"], "Credential from second connection should not be found (different asid)");
    worked = [GSSCredTestUtil fetchCredential:fourthPeer uuid:secondUuid];
    XCTAssertFalse(worked, "Credential from second connection should not be accessible");
    
    XCTAssertTrue([items containsObject:@"test3@EXAMPLE.COM"], "Credential from third connection should be found (same asid)");
    worked = [GSSCredTestUtil fetchCredential:fourthPeer uuid:thirdUuid];
    XCTAssertTrue(worked, "Credential from third connection should be accessible as sudo");
    
    
    //credentials from fourth connection should be created, found and accessbile
    CFUUIDRef fourthUuid = NULL;
    worked = [GSSCredTestUtil createCredentialAndCache:fourthPeer name:@"test4@EXAMPLE.COM" returningCacheUuid:&fourthUuid];
    XCTAssertTrue(worked, "Credential was created successfully");
    
    worked = [GSSCredTestUtil queryAllCredentials:fourthPeer returningArray:&items];
    XCTAssertTrue(worked, "Searching should not fail");
    XCTAssertTrue([items containsObject:@"test4@EXAMPLE.COM"], "Credential from fourth connection should be found");
    
    worked = [GSSCredTestUtil fetchCredential:fourthPeer uuid:fourthUuid];
    XCTAssertTrue(worked, "Credential from fourth connection should be accessible");
    
    // creadentials created by first asid should be deleted when first asid is removed
    [GSSCredTestUtil freePeer:firstPeer];
    [GSSCredTestUtil freePeer:secondPeer];
    RemoveSession(10001);
    
    worked = [GSSCredTestUtil queryAllCredentials:thirdPeer returningArray:&items];
    XCTAssertTrue(worked, "Searching should not fail");
    XCTAssertFalse([items containsObject:@"test1@EXAMPLE.COM"], "Credential from first connection should not be found");
    XCTAssertFalse([items containsObject:@"test2@EXAMPLE.COM"], "Credential from second connection should not be found");
    XCTAssertTrue([items containsObject:@"test3@EXAMPLE.COM"], "Credential from third connection should be found");
    XCTAssertTrue([items containsObject:@"test4@EXAMPLE.COM"], "Credential from fourth connection should be found");
    
    //credentails from third and fourth sessions should be deleted when second asid is removed
    [GSSCredTestUtil freePeer:thirdPeer];
    [GSSCredTestUtil freePeer:fourthPeer];
    RemoveSession(10002);
    
    struct peer *queryPeer = [GSSCredTestUtil createPeer:@"com.apple.fake" identifier:-1];
    worked = [GSSCredTestUtil queryAllCredentials:queryPeer returningArray:&items];
    XCTAssertFalse([items containsObject:@"test1@EXAMPLE.COM"], "Credential from first connection should not be found");
    XCTAssertFalse([items containsObject:@"test2@EXAMPLE.COM"], "Credential from second connection should not be found");
    XCTAssertFalse([items containsObject:@"test3@EXAMPLE.COM"], "Credential from third connection should not be found");
    XCTAssertFalse([items containsObject:@"test4@EXAMPLE.COM"], "Credential from fourth connection should not be found");
    
    [GSSCredTestUtil freePeer:queryPeer];
    
}

- (void)testUpdatingUidShouldFail {
    
    HeimCredGlobalCTX.isMultiUser = NO;
    HeimCredGlobalCTX.useUidMatching = YES;
    _currentUid = 501;
    [GSSCredTestUtil freePeer:self.peer];
    self.peer = [GSSCredTestUtil createPeer:@"com.apple.fake" identifier:0];
    
    CFUUIDRef uuid = NULL;
    BOOL worked = [GSSCredTestUtil createCredentialAndCache:self.peer name:@"test@EXAMPLE.COM" returningCacheUuid:&uuid];
    XCTAssertTrue(worked, "Credential was created successfully");
    
    NSDictionary *attributes = @{(id)kHEIMAttrUserID:@0};
    uint64_t result = [GSSCredTestUtil setAttributes:self.peer uuid:uuid attributes:(__bridge CFDictionaryRef)(attributes) returningDictionary:NULL];
    XCTAssertEqual(result, kHeimCredErrorUpdateNotAllowed, "Updating the Uid is not allowed.");

}

- (void)testDefaultCredential {
    HeimCredGlobalCTX.isMultiUser = NO;
    struct peer *firstPeer = [GSSCredTestUtil createPeer:@"com.apple.fake" identifier:0];
    
    CFUUIDRef defCred = [GSSCredTestUtil getDefaultCredential:firstPeer];
    XCTAssertNotEqual(defCred, NULL, "The default credential exists.");
    
    CFUUIDRef uuid = NULL;
    BOOL worked = [GSSCredTestUtil createCredentialAndCache:firstPeer name:@"test@EXAMPLE.COM" returningCacheUuid:&uuid];
    XCTAssertTrue(worked, "Credential should be created successfully.");
    
    defCred = [GSSCredTestUtil getDefaultCredential:firstPeer];
    XCTAssertTrue(CFEqual(uuid, defCred), "The only credential should be the default.");
    
#if TARGET_OS_OSX
    struct peer *secondPeer = [GSSCredTestUtil createPeer:@"com.apple.foo" identifier:0];
#else
    //on embedded, there is not a wildcard ACL, so this test needs to use the same bundleid to pass
    struct peer *secondPeer = [GSSCredTestUtil createPeer:@"com.apple.fake" identifier:0];
#endif
    defCred = [GSSCredTestUtil getDefaultCredential:secondPeer];
    XCTAssertTrue(CFEqual(uuid, defCred), "The credential should be the default for another app too.");
    
    CFUUIDRef uuid1 = NULL;
    worked = [GSSCredTestUtil createCredentialAndCache:firstPeer name:@"test1@EXAMPLE.COM" returningCacheUuid:&uuid1];
    XCTAssertTrue(worked, "Credential should be created successfully.");
    
    defCred = [GSSCredTestUtil getDefaultCredential:firstPeer];
    XCTAssertTrue(CFEqual(uuid, defCred), "The first credential should still be the default.");
    XCTAssertFalse(CFEqual(uuid1, defCred), "The second credential should not be the default.");
    
    defCred = [GSSCredTestUtil getDefaultCredential:secondPeer];
    XCTAssertTrue(CFEqual(uuid, defCred), "The first credential should still be the default for the other app.");
    XCTAssertFalse(CFEqual(uuid1, defCred), "The second credential should not be the default for the other app.");
    
    NSDictionary *attributes = @{(id)kHEIMAttrDefaultCredential:@YES};
    uint64_t result = [GSSCredTestUtil setAttributes:firstPeer uuid:uuid1 attributes:(__bridge CFDictionaryRef)attributes returningDictionary:NULL];
    XCTAssertEqual(result, 0, "Changing the default credential should work");
    
    defCred = [GSSCredTestUtil getDefaultCredential:firstPeer];
    XCTAssertFalse(CFEqual(uuid, defCred), "The first credential should not be the default.");
    XCTAssertTrue(CFEqual(uuid1, defCred), "The second credential should be the default.");
    
    defCred = [GSSCredTestUtil getDefaultCredential:secondPeer];
    XCTAssertFalse(CFEqual(uuid, defCred), "The first credential should not be the default for the other app.");
    XCTAssertTrue(CFEqual(uuid1, defCred), "The second credential should be the default for the other app.");
    
    result = [GSSCredTestUtil delete:firstPeer uuid:uuid1];
    XCTAssertEqual(result, 0, "Deleting the second credentials should work.");
    
    defCred = [GSSCredTestUtil getDefaultCredential:firstPeer];
    XCTAssertTrue(CFEqual(uuid, defCred), "The first credential should be the default.");
    XCTAssertFalse(CFEqual(uuid1, defCred), "The second credential should not be the default.");
    
    defCred = [GSSCredTestUtil getDefaultCredential:secondPeer];
    XCTAssertTrue(CFEqual(uuid, defCred), "The first credential should be the default for the other app.");
    XCTAssertFalse(CFEqual(uuid1, defCred), "The second credential should not be the default for the other app.");
    
    [GSSCredTestUtil freePeer:firstPeer];
    
    defCred = [GSSCredTestUtil getDefaultCredential:secondPeer];
    XCTAssertTrue(CFEqual(uuid, defCred), "The first credential should be the default for the other app.");
    XCTAssertFalse(CFEqual(uuid1, defCred), "The second credential should not be the default for the other app.");
    
    result = [GSSCredTestUtil delete:secondPeer uuid:uuid];
    XCTAssertEqual(result, 0, "Deleting the first credential should work.");
    
    defCred = [GSSCredTestUtil getDefaultCredential:secondPeer];
    XCTAssertNotEqual(defCred, NULL, "The default credential should exist.");
    XCTAssertFalse(CFEqual(uuid, defCred), "The first credential should not be the default.");
    XCTAssertFalse(CFEqual(uuid1, defCred), "The second credential should not be the default.");
    
    CFUUIDRef uuid2 = NULL;
    worked = [GSSCredTestUtil createCredentialAndCache:secondPeer name:@"test2@EXAMPLE.COM" returningCacheUuid:&uuid2];
    XCTAssertTrue(worked, "Credential should be created successfully.");
    
    defCred = [GSSCredTestUtil getDefaultCredential:secondPeer];
    XCTAssertTrue(CFEqual(uuid2, defCred), "The third credential should be the default.");
    
    [GSSCredTestUtil freePeer:secondPeer];
    
}

- (void)testDefaultCredentialLoadOrderedCredFirst {
    // this file is a specific ordered file where the first entry is a cred rather than a cache.
    // to remake it, run a test and copy the archive file from /var/tmp.
    // use plutil -convert xml1 {the file} to convert it to XML.  modify the entries so that the first value loaded is a cred and not a cache.
    // use plutil -convert binary1 {the file} to convert it back to binary and add it to the test bundle.
    
    //the order of load here is TGT (not eligible), Cache (elected as default)

    HeimCredGlobalCTX.encryptData = encryptDataMock;
    HeimCredGlobalCTX.decryptData = decryptDataMock;

    NSString *path = [[NSBundle bundleForClass:[self class]] pathForResource:@"heim-ordered-test-1" ofType:@"plist"];
    archivePath = path;
    
    readCredCache();
        
    self.peer = [GSSCredTestUtil createPeer:@"com.apple.fake" identifier:0];

    CFUUIDRef cacheUUID = CFUUIDCreateFromString(NULL, CFSTR("4FAD3654-D2ED-4A6A-A6E4-5227315D3DBE"));
    
    CFUUIDRef defCred = [GSSCredTestUtil getDefaultCredential:self.peer];
    XCTAssertTrue(CFEqual(cacheUUID, defCred), "The default credential must be the cache, even if not loaded first.");
    
    CFRELEASE_NULL(cacheUUID);
    
}

- (void)testDefaultCredentialLoadOrderedElectedCredThenDefaultTagged {
    // this file is a specific ordered file where the first entry is a cred rather than a cache.
    // to remake it, run a test and copy the archive file from /var/tmp.
    // use plutil -convert xml1 {the file} to convert it to XML.  modify the entries so that the first value loaded is a cred and not a cache.
    // use plutil -convert binary1 {the file} to convert it back to binary and add it to the test bundle.
    
    //the order of load here is TGT, Cache (elected as default), Cache(tagged as default), TGT

    //use mock encryption
    HeimCredGlobalCTX.encryptData = encryptDataMock;
    HeimCredGlobalCTX.decryptData = decryptDataMock;

    NSString *path = [[NSBundle bundleForClass:[self class]] pathForResource:@"heim-ordered-test-2" ofType:@"plist"];
    archivePath = path;
    
    readCredCache();
    
    self.peer = [GSSCredTestUtil createPeer:@"com.apple.fake" identifier:0];
    
    CFUUIDRef cacheUUID = CFUUIDCreateFromString(NULL, CFSTR("04741D17-8DF3-46BD-95D1-EABD36D93277"));
    
    CFUUIDRef defCred = [GSSCredTestUtil getDefaultCredential:self.peer];
    XCTAssertTrue(CFEqual(cacheUUID, defCred), "The default credential must be the cache, even if not loaded first.");
    
    CFRELEASE_NULL(cacheUUID);
    
}

- (void)testDefaultCredentialLoadOrderedDefaultTaggedCredThenAnotherCache {
    // this file is a specific ordered file where the first entry is a cred rather than a cache.
    // to remake it, run a test and copy the archive file from /var/tmp.
    // use plutil -convert xml1 {the file} to convert it to XML.  modify the entries so that the first value loaded is a cred and not a cache.
    // use plutil -convert binary1 {the file} to convert it back to binary and add it to the test bundle.
    
    //the order of load here is TGT, Cache(tagged as default), Cache (could be default) , TGT

    //use mock encryption
    HeimCredGlobalCTX.encryptData = encryptDataMock;
    HeimCredGlobalCTX.decryptData = decryptDataMock;

    NSString *path = [[NSBundle bundleForClass:[self class]] pathForResource:@"heim-ordered-test-3" ofType:@"plist"];
    archivePath = path;
    
    readCredCache();
    
    self.peer = [GSSCredTestUtil createPeer:@"com.apple.fake" identifier:0];
    
    CFUUIDRef cacheUUID = CFUUIDCreateFromString(NULL, CFSTR("E4E26D78-2B0A-4F32-8E70-761D5649D275"));
    
    CFUUIDRef defCred = [GSSCredTestUtil getDefaultCredential:self.peer];
    XCTAssertTrue(CFEqual(cacheUUID, defCred), "The default credential must be the cache, even if not loaded first.");
    
    CFRELEASE_NULL(cacheUUID);
    
}

- (void)testDefaultCredentialSaveLoad {
    HeimCredGlobalCTX.isMultiUser = NO;
    self.peer = [GSSCredTestUtil createPeer:@"com.apple.fake" identifier:0];

    //add a bunch of extra creds
    for (int i=10; i<20; i++) {
	CFUUIDRef junkUUID = NULL;
	NSString *name = [NSString stringWithFormat:@"test%d@EXAMPLE.COM", i];
	[GSSCredTestUtil createCredentialAndCache:self.peer name:name returningCacheUuid:&junkUUID];

    }
    
    CFUUIDRef defCred = NULL;
    CFUUIDRef uuid = NULL;
    BOOL worked = [GSSCredTestUtil createCredentialAndCache:self.peer name:@"test@EXAMPLE.COM" returningCacheUuid:&uuid];
    XCTAssertTrue(worked, "Credential should be created successfully.");
    
    //tag first main cred as default
    NSDictionary *attributes = @{(id)kHEIMAttrDefaultCredential:@YES};
    uint64_t result = [GSSCredTestUtil setAttributes:self.peer uuid:uuid attributes:(__bridge CFDictionaryRef)attributes returningDictionary:NULL];
    XCTAssertEqual(result, 0, "Changing the default credential should work");
    
    CFUUIDRef uuid1 = NULL;
    worked = [GSSCredTestUtil createCredentialAndCache:self.peer name:@"test1@EXAMPLE.COM" returningCacheUuid:&uuid1];
    XCTAssertTrue(worked, "Credential should be created successfully.");
    
    //add some more extra creds
    for (int i=20; i<30; i++) {
	CFUUIDRef junkUUID = NULL;
	NSString *name = [NSString stringWithFormat:@"test%d@EXAMPLE.COM", i];
	[GSSCredTestUtil createCredentialAndCache:self.peer name:name returningCacheUuid:&junkUUID];

    }
    
    //verify default before reading file
    defCred = [GSSCredTestUtil getDefaultCredential:self.peer];
    XCTAssertTrue(CFEqual(uuid, defCred), "The first credential should still be the default.");
    defCred = NULL;
    
    [GSSCredTestUtil freePeer:self.peer];
    
    CFRELEASE_NULL(HeimCredCTX.sessions);
    HeimCredCTX.sessions = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    
    readCredCache();
    
    self.peer = [GSSCredTestUtil createPeer:@"com.apple.fake" identifier:0];
    
    //UUIDs are not the same across loads, so we compare with the cred name
    
    //verify the same cred is default after loading.
    NSString *defaultCredName;
    worked = [GSSCredTestUtil fetchDefaultCredential:self.peer returningName:&defaultCredName];
    XCTAssertTrue(worked, "The default cred should exist");
    XCTAssertTrue([defaultCredName isEqualToString:@"test@EXAMPLE.COM"], "The first credential should still be the default.");
    defaultCredName = nil;
    
    defCred = [GSSCredTestUtil getDefaultCredential:self.peer];
    result = [GSSCredTestUtil delete:self.peer uuid:defCred];
    XCTAssertEqual(result, 0, "Deleting the default credential should work.");
    
    //verify the elected cred is the default
    NSString *defaultCredNameBeforeSave;
    worked = [GSSCredTestUtil fetchDefaultCredential:self.peer returningName:&defaultCredNameBeforeSave];
    XCTAssertTrue(worked, "The default cred should exist");
    //check default cred attributes here
    
    
    [GSSCredTestUtil freePeer:self.peer];
    
    CFRELEASE_NULL(HeimCredCTX.sessions);
    HeimCredCTX.sessions = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    
    readCredCache();
    
    self.peer = [GSSCredTestUtil createPeer:@"com.apple.fake" identifier:0];
    
    worked = [GSSCredTestUtil fetchDefaultCredential:self.peer returningName:&defaultCredName];
    XCTAssertTrue(worked, "The default cred should exist");
    XCTAssertTrue([defaultCredName isEqualToString:defaultCredNameBeforeSave], "The default credential before saving still be the default.");
    defaultCredName = nil;
    
    //if a new cred is made and selected as default, it should also persist across save / loads
    CFUUIDRef uuid2 = NULL;
    worked = [GSSCredTestUtil createCredentialAndCache:self.peer name:@"test2@EXAMPLE.COM" returningCacheUuid:&uuid2];
    XCTAssertTrue(worked, "Credential should be created successfully.");
    
    attributes = @{(id)kHEIMAttrDefaultCredential:@YES};
    result = [GSSCredTestUtil setAttributes:self.peer uuid:uuid2 attributes:(__bridge CFDictionaryRef)attributes returningDictionary:NULL];
    XCTAssertEqual(result, 0, "Changing the default credential should work");
    
    [GSSCredTestUtil freePeer:self.peer];

    CFRELEASE_NULL(HeimCredCTX.sessions);
    HeimCredCTX.sessions = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    
    readCredCache();
    
    self.peer = [GSSCredTestUtil createPeer:@"com.apple.fake" identifier:0];
    
    worked = [GSSCredTestUtil fetchDefaultCredential:self.peer returningName:&defaultCredName];
    XCTAssertTrue(worked, "The default cred should exist");
    XCTAssertTrue([defaultCredName isEqualToString:@"test2@EXAMPLE.COM"], "The third credential should be the default.");
    defaultCredName = nil;
    
}

- (void)testPrivilidgedAccessToPassword {
    HeimCredGlobalCTX.isMultiUser = NO;
    [GSSCredTestUtil freePeer:self.peer];
    struct peer *gssdPeer = [GSSCredTestUtil createPeer:@"com.apple.gssd" identifier:0];
    
    CFUUIDRef uuid = NULL;
    BOOL worked = [GSSCredTestUtil createCredentialAndCache:gssdPeer name:@"test@EXAMPLE.COM" returningCacheUuid:&uuid];
    
    XCTAssertTrue(worked, "Credential should be created successfully");
    
    NSDictionary *childAttributes = @{ (id)kHEIMAttrParentCredential:(__bridge id)uuid,
				       (id)kHEIMAttrLeadCredential:@YES,
				       (id)kHEIMAttrAuthTime:[NSDate date],
				       (id)kHEIMAttrServerName:@"krb5_ccache_conf_data/password@X-CACHECONF:",
				       (id)kHEIMAttrData:(id)[@"this is fake password data" dataUsingEncoding:NSUTF8StringEncoding],
    };
    
    XCTAssertEqual([GSSCredTestUtil itemCount:gssdPeer], 2, "There should be one parent and one child");
    CFUUIDRef uuid1 = NULL;
    worked = [GSSCredTestUtil createCredential:gssdPeer name:@"test@EXAMPLE.COM" attributes:(__bridge CFDictionaryRef)childAttributes returningUuid:&uuid1];
    XCTAssertTrue(worked, "adding password config entry should be created successfully");
    
    XCTAssertEqual([GSSCredTestUtil itemCount:gssdPeer], 3, "there should be 3 entries now");
    
    NSArray *items;
#if TARGET_OS_OSX
    _currentSignedIdentifier = @"com.apple.gssd";
    worked = [GSSCredTestUtil queryAll:gssdPeer parentUUID:uuid returningArray:&items];
    XCTAssertTrue(worked, "Searching should not fail");
    XCTAssertTrue([items containsObject:@"krbtgt/EXAMPLE.COM@EXAMPLE.COM"], "Credential from first connection should be found");
    XCTAssertTrue([items containsObject:@"krb5_ccache_conf_data/password@X-CACHECONF:"], "password data should be found by gssd");
    
    worked = [GSSCredTestUtil queryAll:gssdPeer parentUUID:uuid returningArray:&items];
    XCTAssertTrue([items containsObject:@"krb5_ccache_conf_data/password@X-CACHECONF:"], "password data should be found by gssd on subsequent calls");
    
    _currentSignedIdentifier = @"com.apple.NOTgssd";
    gssdPeer->access_status = IAKERB_NOT_CHECKED;  //reset this for testing
    worked = [GSSCredTestUtil queryAll:gssdPeer parentUUID:uuid returningArray:&items];
    XCTAssertTrue(worked, "Searching should not fail");
    XCTAssertFalse([items containsObject:@"krb5_ccache_conf_data/password@X-CACHECONF:"], "password data should not be found by an app pretending to be gssd");
    
    [GSSCredTestUtil freePeer:gssdPeer];
    
    struct peer *nasaPeer = [GSSCredTestUtil createPeer:@"com.apple.NetAuthSysAgent" identifier:0];
    _currentSignedIdentifier = @"com.apple.NetAuthSysAgent";
    worked = [GSSCredTestUtil queryAll:nasaPeer parentUUID:uuid returningArray:&items];
    XCTAssertTrue(worked, "Searching should not fail");
    XCTAssertTrue([items containsObject:@"krbtgt/EXAMPLE.COM@EXAMPLE.COM"], "Credential from first connection should be found");
    XCTAssertTrue([items containsObject:@"krb5_ccache_conf_data/password@X-CACHECONF:"], "password data should be found by NetAuthSysAgent");
    
    worked = [GSSCredTestUtil queryAll:nasaPeer parentUUID:uuid returningArray:&items];
    XCTAssertTrue([items containsObject:@"krb5_ccache_conf_data/password@X-CACHECONF:"], "password data should be found by NetAuthSysAgent on subsequent calls");
    
    _currentSignedIdentifier = @"com.apple.NOTNetAuthSysAgent";
    nasaPeer->access_status = IAKERB_NOT_CHECKED;  //reset this for testing
    worked = [GSSCredTestUtil queryAll:nasaPeer parentUUID:uuid returningArray:&items];
    XCTAssertTrue(worked, "Searching should not fail");
    XCTAssertFalse([items containsObject:@"krb5_ccache_conf_data/password@X-CACHECONF:"], "password data should be found by an app that is pretending to be NetAuthSysAgent");

    [GSSCredTestUtil freePeer:nasaPeer];
    
    struct peer *anyAppPeer = [GSSCredTestUtil createPeer:@"com.apple.anything" identifier:0];
    _entitlements = @[@"com.apple.private.gssapi.iakerb-data-access"];
    worked = [GSSCredTestUtil queryAll:anyAppPeer parentUUID:uuid returningArray:&items];
    XCTAssertTrue(worked, "Searching should not fail");
    XCTAssertTrue([items containsObject:@"krbtgt/EXAMPLE.COM@EXAMPLE.COM"], "Credential from first connection should be found");
    XCTAssertTrue([items containsObject:@"krb5_ccache_conf_data/password@X-CACHECONF:"], "password data should be found by app with the entitlement");
    
    worked = [GSSCredTestUtil queryAll:anyAppPeer parentUUID:uuid returningArray:&items];
    XCTAssertTrue([items containsObject:@"krb5_ccache_conf_data/password@X-CACHECONF:"], "password data should be found by app with the entitlement on subsequent calls");
    
    
    _entitlements = @[];
    anyAppPeer->access_status = IAKERB_NOT_CHECKED;  //reset this for testing
    worked = [GSSCredTestUtil queryAll:anyAppPeer parentUUID:uuid returningArray:&items];
    XCTAssertTrue(worked, "Searching should not fail");
    XCTAssertTrue([items containsObject:@"krbtgt/EXAMPLE.COM@EXAMPLE.COM"], "Credential from first connection should be found");
    XCTAssertFalse([items containsObject:@"krb5_ccache_conf_data/password@X-CACHECONF:"], "password data should not be found by app without the entitlement");
    
    worked = [GSSCredTestUtil queryAll:anyAppPeer parentUUID:uuid returningArray:&items];
    XCTAssertTrue(worked, "Searching should not fail");
    XCTAssertFalse([items containsObject:@"krb5_ccache_conf_data/password@X-CACHECONF:"], "password data should not be found by app without the entitlement on subsequent calls");
    
    [GSSCredTestUtil freePeer:anyAppPeer];

#else
    // iOS access to the entry is entitlement only. This test is using gssd becaue it matches the ACL for the credential
    _entitlements = @[@"com.apple.private.gssapi.iakerb-data-access"];
    worked = [GSSCredTestUtil queryAll:gssdPeer parentUUID:uuid returningArray:&items];
    XCTAssertTrue(worked, "Searching should not fail");
    XCTAssertTrue([items containsObject:@"krbtgt/EXAMPLE.COM@EXAMPLE.COM"], "Credential from first connection should be found");
    XCTAssertTrue([items containsObject:@"krb5_ccache_conf_data/password@X-CACHECONF:"], "password data should be found by app with the entitlement");
    
    [GSSCredTestUtil freePeer:gssdPeer];

    _entitlements = @[];
    struct peer *notGssdPeer = [GSSCredTestUtil createPeer:@"com.apple.NOTgssd" identifier:0];
    worked = [GSSCredTestUtil queryAll:notGssdPeer parentUUID:uuid returningArray:&items];
    XCTAssertTrue(worked, "Searching should not fail");
    XCTAssertFalse([items containsObject:@"krbtgt/EXAMPLE.COM@EXAMPLE.COM"], "Credential from first connection should not be found");
    XCTAssertFalse([items containsObject:@"krb5_ccache_conf_data/password@X-CACHECONF:"], "password data should not be found by app that didnt create it");
    
    [GSSCredTestUtil freePeer:notGssdPeer];
#endif
}

- (void)testArchiveFilePermissions
{
    HeimCredGlobalCTX.isMultiUser = NO;
    self.peer = [GSSCredTestUtil createPeer:@"com.apple.fake" identifier:0];

    CFUUIDRef uuid = NULL;
    BOOL worked = [GSSCredTestUtil createCredentialAndCache:self.peer name:@"test@EXAMPLE.COM" returningCacheUuid:&uuid];
    XCTAssertTrue(worked, "Credential should be created successfully.");
    CFRELEASE_NULL(uuid);

    NSDictionary *attributes = [[NSFileManager defaultManager] attributesOfItemAtPath:archivePath error:nil];

    NSNumber *perm = attributes[NSFilePosixPermissions];
    XCTAssertEqualObjects(perm, @(S_IRUSR|S_IWUSR), "Archive file permissions should be readable and writable by owner only");

}

// mocks

static NSArray<NSString*> *_entitlements;
static NSString *_altDSID;
static int _currentUid;
static int _currentAsid;
static NSString *_currentSignedIdentifier;

static NSString * currentAltDSIDMock(void)
{
    return _altDSID;
}

static bool haveBooleanEntitlementMock(struct peer *peer, const char *entitlement)
{
    NSString *ent = @(entitlement);
    return [_entitlements containsObject:ent];
}

static bool verifyAppleSignedMock(struct peer *peer, NSString *identifer)
{
    return ([identifer isEqualToString:_currentSignedIdentifier]);
}

static bool sessionExistsMock(pid_t asid) {
    return true;
}

//xpc mock

static uid_t getUidMock(xpc_connection_t connection) {
    return _currentUid;
}

static au_asid_t getAsidMock(xpc_connection_t connection) {
    return _currentAsid;
}

static void saveToDiskIfNeededMock(void)
{
    
}

static CFPropertyListRef getValueFromPreferencesMock(CFStringRef key)
{
    return NULL;
}
@end

