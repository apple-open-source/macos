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
#import "mock_aks.h"
#import "aks.h"
#import "acquirecred.h"
#import "GSSCredMockHelperClient.h"

@interface GSSCredKerberosAcquireCredTests : XCTestCase

@property (nullable) struct peer * peer;
@property (nonatomic) MockManagedAppManager *mockManagedAppManager;
@property (nonnull) GSSCredMockHelperClient *mockHelperClient;

@end

@implementation GSSCredKerberosAcquireCredTests {
}
@synthesize peer;
@synthesize mockManagedAppManager;
@synthesize mockHelperClient;

- (void)setUp {

    self.mockManagedAppManager = [[MockManagedAppManager alloc] init];
    self.mockHelperClient = [[GSSCredMockHelperClient alloc] init];
    
    __weak typeof(self) weakSelf = self;
    GSSCredMockHelperClient.expireBlock = ^krb5_error_code(HeimCredRef _Nonnull cred, time_t * _Nonnull expire) {
	*expire = [[NSDate dateWithTimeIntervalSinceNow:3600] timeIntervalSince1970];
	NSString *uuidString = CFBridgingRelease(CFUUIDCreateString(NULL, cred->uuid));
	if (weakSelf.mockHelperClient.expireExpectations[uuidString]) {
	    [weakSelf.mockHelperClient.expireExpectations[uuidString] fulfill];
	}
	return 0;
    };
    
    GSSCredMockHelperClient.renewBlock = ^krb5_error_code(HeimCredRef _Nonnull cred, time_t * _Nonnull expire) {
	NSString *uuidString = CFBridgingRelease(CFUUIDCreateString(NULL, cred->uuid));
	if (weakSelf.mockHelperClient.renewExpectations[uuidString]) {
	    [weakSelf.mockHelperClient.renewExpectations[uuidString] fulfill];
	}
	return 0;
    };
    
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
    HeimCredGlobalCTX.expireFunction = expire_func;
    HeimCredGlobalCTX.renewFunction = renew_func;
    HeimCredGlobalCTX.finalFunction = final_func;
    HeimCredGlobalCTX.notifyCaches = notifyCachesMock;
    HeimCredGlobalCTX.gssCredHelperClientClass = GSSCredMockHelperClient.class;
    HeimCredGlobalCTX.executeOnRunQueue = executeOnRunQueueMock;

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
    CFRELEASE_NULL(HeimCredCTX.sessions);
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

    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
	heim_ipc_init_globals();
	heim_ipc_resume_events();
    });
}

-(void)tearDown {
    NSError *error;
    [[NSFileManager defaultManager] removeItemAtPath:archivePath error:&error];
    [GSSCredTestUtil freePeer:self.peer];
    self.peer = NULL;
    
    CFRELEASE_NULL(HeimCredCTX.sessions);
    
    [self.mockHelperClient.expireExpectations removeAllObjects];
    [self.mockHelperClient.renewExpectations removeAllObjects];
    
}

//pragma mark - Tests

- (void)testKerberosAcquireCredCreateAndFetch {
    HeimCredGlobalCTX.isMultiUser = NO;
    self.peer = [GSSCredTestUtil createPeer:@"com.apple.test" identifier:0];
    
    //create an empty cache
    CFUUIDRef uuid = NULL;
    BOOL worked = [GSSCredTestUtil createCredential:self.peer name:@"test@EXAMPLE.COM" attributes:NULL returningUuid:&uuid];
    
    XCTAssertTrue(worked, "cache should be created successfully");
    
    NSDictionary *acquireAttributes = @{ (id)kHEIMObjectType: (id)kHEIMObjectKerberosAcquireCred,
					 (id)kHEIMAttrType:(id)kHEIMTypeKerberosAcquireCred,
					 (id)kHEIMAttrParentCredential:(__bridge id)uuid,
					 (id)kHEIMAttrClientName:@"test@EXAMPLE.COM",
					 (id)kHEIMAttrData:(id)[@"this is fake password data" dataUsingEncoding:NSUTF8StringEncoding],
					 (id)kHEIMAttrCredential:(id)kCFBooleanTrue,
    };
    
    CFDictionaryRef attrs;
    worked = [GSSCredTestUtil executeCreateCred:self.peer forAttributes:(__bridge CFDictionaryRef _Nonnull)(acquireAttributes) returningDictionary:&attrs];
    XCTAssertTrue(worked, "Kerberos Acquire Cred item should be created successfully");
    XCTAssertEqual([GSSCredTestUtil itemCount:self.peer], 2, "There should be one parent and one child");
    
    CFUUIDRef credUUID = CFDictionaryGetValue(attrs, kHEIMAttrUUID);
    CFRELEASE_NULL(attrs);
    worked = [GSSCredTestUtil fetchCredential:self.peer uuid:credUUID returningDictionary:&attrs];
    XCTAssertFalse(worked, "Searching should fail");
    XCTAssertTrue(attrs == NULL, "Kerberos Acquire Cred attributes should be NULL");
    
    NSArray *items;
    worked = [GSSCredTestUtil queryAll:self.peer type:kHEIMTypeKerberosAcquireCred returningArray:&items];
    XCTAssertTrue(worked, "Searching should not fail");
    XCTAssertTrue(items.count == 0, "Kerberos Acquire Cred should be never be returned");
    
    uint64_t error = [GSSCredTestUtil delete:self.peer uuid:uuid];
    XCTAssertEqual(error, 0, "deleting Kerberos Acquire Cred cache should work");
    XCTAssertEqual([GSSCredTestUtil itemCount:self.peer], 0, "deleting should remove both the parent and the Kerberos Acquire Cred item");
    
    CFRELEASE_NULL(uuid);
    CFRELEASE_NULL(attrs);
}

#if TARGET_OS_OSX
- (void)testKerberosAcquireCredTriggerExpire {
    HeimCredGlobalCTX.isMultiUser = NO;
    self.peer = [GSSCredTestUtil createPeer:@"com.apple.test" identifier:0];
    
    //create an empty cache
    CFUUIDRef uuid = NULL;
    BOOL worked = [GSSCredTestUtil createCredential:self.peer name:@"test@EXAMPLE.COM" attributes:NULL returningUuid:&uuid];
    
    XCTAssertTrue(worked, "cache should be created successfully");
    
    NSDictionary *acquireAttributes = @{ (id)kHEIMObjectType: (id)kHEIMObjectKerberosAcquireCred,
					 (id)kHEIMAttrType:(id)kHEIMTypeKerberosAcquireCred,
					 (id)kHEIMAttrParentCredential:(__bridge id)uuid,
					 (id)kHEIMAttrClientName:@"test@EXAMPLE.COM",
					 (id)kHEIMAttrData:(id)[@"this is fake password data" dataUsingEncoding:NSUTF8StringEncoding],
					 (id)kHEIMAttrCredential:(id)kCFBooleanTrue,
    };
    

    XCTestExpectation *expectation = [[XCTestExpectation alloc] initWithDescription:@"acquire credential should trigger expire event"];
    CFDictionaryRef attrs;
    worked = [GSSCredTestUtil executeCreateCred:self.peer forAttributes:(__bridge CFDictionaryRef _Nonnull)(acquireAttributes) returningDictionary:&attrs];
    CFUUIDRef credUUID = CFDictionaryGetValue(attrs, kHEIMAttrUUID);
    if (credUUID) {
	NSString *uuidString = CFBridgingRelease(CFUUIDCreateString(NULL, credUUID));
	self.mockHelperClient.expireExpectations[uuidString] = expectation;
    }
    
    XCTAssertTrue(worked, "store onlhy item should be created successfully");
    XCTAssertEqual([GSSCredTestUtil itemCount:self.peer], 2, "There should be one parent and one child");
    
    [self waitForExpectations:@[expectation] timeout:15];
  
    [GSSCredTestUtil delete:self.peer uuid:uuid];
    
    CFRELEASE_NULL(uuid);
    CFRELEASE_NULL(attrs);
}
#endif

- (void)testKerberosAcquireCredSetAttrs {
    HeimCredGlobalCTX.isMultiUser = NO;
    self.peer = [GSSCredTestUtil createPeer:@"com.apple.test" identifier:0];
        
    //create an empty cache
    CFUUIDRef uuid = NULL;
    BOOL worked = [GSSCredTestUtil createCredential:self.peer name:@"test@EXAMPLE.COM" attributes:NULL returningUuid:&uuid];
    
    XCTAssertTrue(worked, "cache should be created successfully");
    
    NSDictionary *acquireAttributes = @{ (id)kHEIMObjectType: (id)kHEIMObjectKerberosAcquireCred,
					 (id)kHEIMAttrType:(id)kHEIMTypeKerberosAcquireCred,
					 (id)kHEIMAttrParentCredential:(__bridge id)uuid,
					 (id)kHEIMAttrClientName:@"test@EXAMPLE.COM",
					 (id)kHEIMAttrData:(id)[@"this is fake password data" dataUsingEncoding:NSUTF8StringEncoding],
					 (id)kHEIMAttrCredential:(id)kCFBooleanTrue,
    };
    
    CFDictionaryRef attrs;
    worked = [GSSCredTestUtil executeCreateCred:self.peer forAttributes:(__bridge CFDictionaryRef _Nonnull)(acquireAttributes) returningDictionary:&attrs];
    XCTAssertTrue(worked, "Kerberos Acquire Cred item should be created successfully");
    XCTAssertEqual([GSSCredTestUtil itemCount:self.peer], 2, "There should be one parent and one child");
    
    CFUUIDRef credUUID = CFDictionaryGetValue(attrs, kHEIMAttrUUID);
    CFRELEASE_NULL(attrs);
    
    NSDictionary *attributes = @{(id)kHEIMAttrType:(id)kHEIMTypeKerberos};
    uint64_t result = [GSSCredTestUtil setAttributes:self.peer uuid:credUUID attributes:(__bridge CFDictionaryRef)(attributes) returningDictionary:NULL];

    XCTAssertEqual(result, kHeimCredErrorUpdateNotAllowed, "should not be able to change attribute type");
    
    attributes = @{(id)kHEIMObjectType:(id)kHEIMObjectKerberos};
    result = [GSSCredTestUtil setAttributes:self.peer uuid:credUUID attributes:(__bridge CFDictionaryRef)(attributes) returningDictionary:NULL];
    
    XCTAssertEqual(result, kHeimCredErrorUpdateNotAllowed, "Should not be able to change object type");
    
    [GSSCredTestUtil delete:self.peer uuid:uuid];
    CFRELEASE_NULL(uuid);
}

- (void)testKerberosTriggerExpire {
    HeimCredGlobalCTX.isMultiUser = NO;
    self.peer = [GSSCredTestUtil createPeer:@"com.apple.test" identifier:0];
    
    //create an empty cache
    CFUUIDRef uuid = NULL;
    BOOL worked = [GSSCredTestUtil createCredential:self.peer name:@"test@EXAMPLE.COM" attributes:NULL returningUuid:&uuid];
    
    XCTAssertTrue(worked, "cache should be created successfully");
   
    XCTestExpectation *expectation = [[XCTestExpectation alloc] initWithDescription:@"An expired credential should trigger the event"];
    CFUUIDRef credUUID = NULL;
    NSDictionary *attributes = @{ (id)kHEIMAttrParentCredential:(__bridge id)uuid,
				  (id)kHEIMAttrLeadCredential:@YES,
				  (id)kHEIMAttrAuthTime:[NSDate date],
				  (id)kHEIMAttrServerName:@"krbtgt/EXAMPLE.COM@EXAMPLE.COM",
				  (id)kHEIMAttrData:(id)[@"this is fake data" dataUsingEncoding:NSUTF8StringEncoding],
				  (id)kHEIMAttrExpire:[NSDate dateWithTimeIntervalSinceNow:2]
    };
    worked = [GSSCredTestUtil createCredential:self.peer name:@"test@EXAMPLE.COM" attributes:(__bridge CFDictionaryRef _Nullable)(attributes) returningUuid:&credUUID];
    if (credUUID) {
	_notifyExpectation = expectation;
    }
    
    XCTAssertTrue(worked, "Kerberos Acquire Cred item should be created successfully");
    XCTAssertEqual([GSSCredTestUtil itemCount:self.peer], 2, "There should be one parent and one child");
    
    [self waitForExpectations:@[expectation] timeout:15];
    
    [GSSCredTestUtil delete:self.peer uuid:uuid];
    CFRELEASE_NULL(uuid);
}

- (void)testKerberosTriggerExpireStressTest {
    HeimCredGlobalCTX.isMultiUser = NO;
    self.peer = [GSSCredTestUtil createPeer:@"com.apple.test" identifier:0];
    
    //stop event processing until we load it up with work
    heim_ipc_suspend_events();
#if TARGET_OS_OSX
    uint testInterations = 500;
#else
    uint testInterations = 200;  //ios is slower
#endif
    XCTestExpectation *expectation = [[XCTestExpectation alloc] initWithDescription:@"There should be one notification for each cred (in this test)"];
    expectation.expectedFulfillmentCount = testInterations;
    _notifyExpectation = expectation;
    for (uint i = 0; i < testInterations; i++) {
	//create an empty cache
	CFUUIDRef uuid = NULL;
	BOOL worked = [GSSCredTestUtil createCredential:self.peer name:@"test@EXAMPLE.COM" attributes:NULL returningUuid:&uuid];
	
	XCTAssertTrue(worked, "cache should be created successfully");
	
	CFUUIDRef credUUID = NULL;
	NSDictionary *attributes = @{ (id)kHEIMAttrParentCredential:(__bridge id)uuid,
				      (id)kHEIMAttrLeadCredential:@YES,
				      (id)kHEIMAttrAuthTime:[NSDate date],
				      (id)kHEIMAttrServerName:@"krbtgt/EXAMPLE.COM@EXAMPLE.COM",
				      (id)kHEIMAttrData:(id)[@"this is fake data" dataUsingEncoding:NSUTF8StringEncoding],
				      (id)kHEIMAttrExpire:[NSDate dateWithTimeIntervalSinceNow:2]
	};
	worked = [GSSCredTestUtil createCredential:self.peer name:@"test@EXAMPLE.COM" attributes:(__bridge CFDictionaryRef _Nullable)(attributes) returningUuid:&credUUID];
	CFRELEASE_NULL(uuid);
	
    }
    
    XCTAssertEqual([GSSCredTestUtil itemCount:self.peer], testInterations * 2, "There should be one parent and one child for each cred");
    //resume work
    heim_ipc_resume_events();
    
    [self waitForExpectations:@[expectation] timeout:30];
    
    _notifyExpectation = nil;
}
#if TARGET_OS_OSX

- (void)testKerberosTriggerAcquireStressTest {
    HeimCredGlobalCTX.isMultiUser = NO;
    self.peer = [GSSCredTestUtil createPeer:@"com.apple.test" identifier:0];
    
    NSMutableArray *allExpectations = [@[] mutableCopy];
    
    //stop event processing until we load it up with work
    heim_ipc_suspend_events();
    uint testInterations = 500;
    
    for (uint i = 0; i < testInterations; i++) {
	//create an empty cache
	CFUUIDRef uuid = NULL;
	BOOL worked = [GSSCredTestUtil createCredential:self.peer name:@"test@EXAMPLE.COM" attributes:NULL returningUuid:&uuid];
	
	XCTAssertTrue(worked, "cache should be created successfully");
	
	NSDictionary *acquireAttributes = @{ (id)kHEIMObjectType: (id)kHEIMObjectKerberosAcquireCred,
					     (id)kHEIMAttrType:(id)kHEIMTypeKerberosAcquireCred,
					     (id)kHEIMAttrParentCredential:(__bridge id)uuid,
					     (id)kHEIMAttrClientName:@"test@EXAMPLE.COM",
					     (id)kHEIMAttrData:(id)[@"this is fake password data" dataUsingEncoding:NSUTF8StringEncoding],
					     (id)kHEIMAttrCredential:(id)kCFBooleanTrue,
	};
	CFDictionaryRef attrs;
	worked = [GSSCredTestUtil executeCreateCred:self.peer forAttributes:(__bridge CFDictionaryRef _Nonnull)(acquireAttributes) returningDictionary:&attrs];
	CFUUIDRef credUUID = CFDictionaryGetValue(attrs, kHEIMAttrUUID);
	if (credUUID) {
	    NSString *uuidString = CFBridgingRelease(CFUUIDCreateString(NULL, credUUID));
	    NSString *expectationDescription = [NSString stringWithFormat:@"An acquire credential should trigger the event: %@", uuidString];
	    XCTestExpectation *expectation = [[XCTestExpectation alloc] initWithDescription:expectationDescription];
	    self.mockHelperClient.expireExpectations[uuidString] = expectation;
	    [allExpectations addObject:expectation];
	}
	CFRELEASE_NULL(uuid);
	CFRELEASE_NULL(attrs);
    }
    
    XCTAssertEqual([GSSCredTestUtil itemCount:self.peer], testInterations * 2, "There should be one parent and one child for each cred");
    XCTAssertEqual(allExpectations.count, testInterations, "There should be one expectation for each cred");
    //resume work
    heim_ipc_resume_events();
    
    [self waitForExpectations:allExpectations timeout:30];
    
    //run the run loop to let saves to disk complete before tear down
    [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:3]];
    
}


- (void)testKerberosTriggerRenew {
    HeimCredGlobalCTX.isMultiUser = NO;
    
    HeimCredGlobalCTX.renewInterval = 3;  //three seconds
    
    self.peer = [GSSCredTestUtil createPeer:@"com.apple.test" identifier:0];
    
    //create an empty cache
    CFUUIDRef uuid = NULL;
    BOOL worked = [GSSCredTestUtil createCredential:self.peer name:@"test@EXAMPLE.COM" attributes:NULL returningUuid:&uuid];
    
    XCTAssertTrue(worked, "cache should be created successfully");
    
    XCTestExpectation *expectation = [[XCTestExpectation alloc] initWithDescription:@"A renewable credential should be renewed"];
    CFUUIDRef credUUID = NULL;
    NSDictionary *attributes = @{ (id)kHEIMAttrParentCredential:(__bridge id)uuid,
				  (id)kHEIMAttrLeadCredential:@YES,
				  (id)kHEIMAttrAuthTime:[NSDate date],
				  (id)kHEIMAttrServerName:@"krbtgt/EXAMPLE.COM@EXAMPLE.COM",
				  (id)kHEIMAttrData:(id)[@"this is fake data" dataUsingEncoding:NSUTF8StringEncoding],
				  (id)kHEIMAttrExpire:[NSDate dateWithTimeIntervalSinceNow:60*60],
				  (id)kHEIMAttrRenewTill:[NSDate dateWithTimeIntervalSinceNow:4*60*60]
    };
    worked = [GSSCredTestUtil createCredential:self.peer name:@"test@EXAMPLE.COM" attributes:(__bridge CFDictionaryRef _Nullable)(attributes) returningUuid:&credUUID];
    if (credUUID) {
	NSString *uuidString = CFBridgingRelease(CFUUIDCreateString(NULL, credUUID));
	self.mockHelperClient.renewExpectations[uuidString] = expectation;
    }
    XCTAssertTrue(worked, "renewable cred should be created successfully");
    XCTAssertEqual([GSSCredTestUtil itemCount:self.peer], 2, "There should be one parent and one child");
    
    [self waitForExpectations:@[expectation] timeout:5];
    
    [GSSCredTestUtil delete:self.peer uuid:uuid];
    CFRELEASE_NULL(uuid);
}

- (void)testKerberosTriggerRenewStressTest {
    HeimCredGlobalCTX.isMultiUser = NO;
    
    HeimCredGlobalCTX.renewInterval = 3;  //three seconds
    
    self.peer = [GSSCredTestUtil createPeer:@"com.apple.test" identifier:0];
    
    NSMutableArray *allExpectations = [@[] mutableCopy];
    
    //stop event processing until we load it up with work
    heim_ipc_suspend_events();
    uint testInterations = 500;
    
    for (uint i = 0; i < testInterations; i++) {
	//create an empty cache
	CFUUIDRef uuid = NULL;
	BOOL worked = [GSSCredTestUtil createCredential:self.peer name:@"test@EXAMPLE.COM" attributes:NULL returningUuid:&uuid];
	
	XCTAssertTrue(worked, "cache should be created successfully");
	
	CFUUIDRef credUUID = NULL;
	NSDictionary *attributes = @{ (id)kHEIMAttrParentCredential:(__bridge id)uuid,
				      (id)kHEIMAttrLeadCredential:@YES,
				      (id)kHEIMAttrAuthTime:[NSDate date],
				      (id)kHEIMAttrServerName:@"krbtgt/EXAMPLE.COM@EXAMPLE.COM",
				      (id)kHEIMAttrData:(id)[@"this is fake data" dataUsingEncoding:NSUTF8StringEncoding],
				      (id)kHEIMAttrExpire:[NSDate dateWithTimeIntervalSinceNow:60*60],
				      (id)kHEIMAttrRenewTill:[NSDate dateWithTimeIntervalSinceNow:4*60*60]
	};
	worked = [GSSCredTestUtil createCredential:self.peer name:@"test@EXAMPLE.COM" attributes:(__bridge CFDictionaryRef _Nullable)(attributes) returningUuid:&credUUID];
	if (credUUID) {
	    NSString *uuidString = CFBridgingRelease(CFUUIDCreateString(NULL, credUUID));
	    NSString *expectationDescription = [NSString stringWithFormat:@"A renewable credential should trigger the event: %@", uuidString];
	    XCTestExpectation *expectation = [[XCTestExpectation alloc] initWithDescription:expectationDescription];
	    self.mockHelperClient.renewExpectations[uuidString] = expectation;
	    [allExpectations addObject:expectation];
	}
	CFRELEASE_NULL(uuid);
	
    }
    
    XCTAssertEqual([GSSCredTestUtil itemCount:self.peer], testInterations * 2, "There should be one parent and one child for each cred");
    XCTAssertEqual(allExpectations.count, testInterations, "There should be one expectation for each cred");
    //resume work
    heim_ipc_resume_events();
    
    [self waitForExpectations:allExpectations timeout:30];
    
}
#endif

#if TARGET_OS_OSX
- (void)testKerberosTriggerRenewFailedRetry {
    HeimCredGlobalCTX.isMultiUser = NO;
    HeimCredGlobalCTX.renewInterval = 3;  //three seconds
    
    __weak typeof(self) weakSelf = self;
    GSSCredMockHelperClient.renewBlock = ^krb5_error_code(HeimCredRef _Nonnull cred, time_t * _Nonnull expire) {
	NSString *uuidString = CFBridgingRelease(CFUUIDCreateString(NULL, cred->uuid));
	if (weakSelf.mockHelperClient.renewExpectations[uuidString]) {
	    [weakSelf.mockHelperClient.renewExpectations[uuidString] fulfill];
	}
	return KRB5_REALM_CANT_RESOLVE;
    };
    
    self.peer = [GSSCredTestUtil createPeer:@"com.apple.test" identifier:0];
    
    //create an empty cache
    CFUUIDRef uuid = NULL;
    BOOL worked = [GSSCredTestUtil createCredential:self.peer name:@"test@EXAMPLE.COM" attributes:NULL returningUuid:&uuid];
    
    XCTAssertTrue(worked, "cache should be created successfully");
    
    XCTestExpectation *expectation = [[XCTestExpectation alloc] initWithDescription:@"A renewable credential should be renewed"];
    CFUUIDRef credUUID = NULL;
    NSDictionary *attributes = @{ (id)kHEIMAttrParentCredential:(__bridge id)uuid,
				  (id)kHEIMAttrLeadCredential:@YES,
				  (id)kHEIMAttrAuthTime:[NSDate date],
				  (id)kHEIMAttrServerName:@"krbtgt/EXAMPLE.COM@EXAMPLE.COM",
				  (id)kHEIMAttrData:(id)[@"this is fake data" dataUsingEncoding:NSUTF8StringEncoding],
				  (id)kHEIMAttrExpire:[NSDate dateWithTimeIntervalSinceNow:60*60],
				  (id)kHEIMAttrRenewTill:[NSDate dateWithTimeIntervalSinceNow:4*60*60]
    };
    worked = [GSSCredTestUtil createCredential:self.peer name:@"test@EXAMPLE.COM" attributes:(__bridge CFDictionaryRef _Nullable)(attributes) returningUuid:&credUUID];
    if (credUUID) {
	NSString *uuidString = CFBridgingRelease(CFUUIDCreateString(NULL, credUUID));
	self.mockHelperClient.renewExpectations[uuidString] = expectation;
    }
    XCTAssertTrue(worked, "renewable cred should be created successfully");
    XCTAssertEqual([GSSCredTestUtil itemCount:self.peer], 2, "There should be one parent and one child");
    
    [self waitForExpectations:@[expectation] timeout:5];
    
    HeimCredRef cred = (HeimCredRef)CFDictionaryGetValue(self.peer->session->items, credUUID);
    time_t test = [[NSDate dateWithTimeIntervalSinceNow:300] timeIntervalSince1970];
    XCTAssertLessThan(test - cred->renew_time, 5, "The next renew time should be about 300 seconds from now");  //This is not an exact match to handle vaiance in response times.
    XCTAssertFalse(heim_ipc_event_is_cancelled(cred->renew_event), "The renew event should not be cancelled");
    XCTAssertEqual(cred->acquire_status, CRED_STATUS_ACQUIRE_FAILED , "The status should be failed");
    
    [GSSCredTestUtil delete:self.peer uuid:uuid];
    CFRELEASE_NULL(uuid);
}

- (void)testKerberosTriggerRenewFailedNoRetry {
    HeimCredGlobalCTX.isMultiUser = NO;
    HeimCredGlobalCTX.renewInterval = 3;  //three seconds
    
    __weak typeof(self) weakSelf = self;
    GSSCredMockHelperClient.renewBlock = ^krb5_error_code(HeimCredRef _Nonnull cred, time_t * _Nonnull expire) {
	NSString *uuidString = CFBridgingRelease(CFUUIDCreateString(NULL, cred->uuid));
	if (weakSelf.mockHelperClient.renewExpectations[uuidString]) {
	    [weakSelf.mockHelperClient.renewExpectations[uuidString] fulfill];
	}
	return KRB5KDC_ERR_PREAUTH_FAILED;
    };
    
    self.peer = [GSSCredTestUtil createPeer:@"com.apple.test" identifier:0];
    
    //create an empty cache
    CFUUIDRef uuid = NULL;
    BOOL worked = [GSSCredTestUtil createCredential:self.peer name:@"test@EXAMPLE.COM" attributes:NULL returningUuid:&uuid];
    
    XCTAssertTrue(worked, "cache should be created successfully");
    
    XCTestExpectation *expectation = [[XCTestExpectation alloc] initWithDescription:@"A renewable credential should be renewed"];
    CFUUIDRef credUUID = NULL;
    NSDictionary *attributes = @{ (id)kHEIMAttrParentCredential:(__bridge id)uuid,
				  (id)kHEIMAttrLeadCredential:@YES,
				  (id)kHEIMAttrAuthTime:[NSDate date],
				  (id)kHEIMAttrServerName:@"krbtgt/EXAMPLE.COM@EXAMPLE.COM",
				  (id)kHEIMAttrData:(id)[@"this is fake data" dataUsingEncoding:NSUTF8StringEncoding],
				  (id)kHEIMAttrExpire:[NSDate dateWithTimeIntervalSinceNow:60*60],
				  (id)kHEIMAttrRenewTill:[NSDate dateWithTimeIntervalSinceNow:4*60*60]
    };
    worked = [GSSCredTestUtil createCredential:self.peer name:@"test@EXAMPLE.COM" attributes:(__bridge CFDictionaryRef _Nullable)(attributes) returningUuid:&credUUID];
    if (credUUID) {
	NSString *uuidString = CFBridgingRelease(CFUUIDCreateString(NULL, credUUID));
	self.mockHelperClient.renewExpectations[uuidString] = expectation;
    }
    XCTAssertTrue(worked, "renewable cred should be created successfully");
    XCTAssertEqual([GSSCredTestUtil itemCount:self.peer], 2, "There should be one parent and one child");
    
    [self waitForExpectations:@[expectation] timeout:5];
    
    HeimCredRef cred = (HeimCredRef)CFDictionaryGetValue(self.peer->session->items, credUUID);
    XCTAssertTrue(heim_ipc_event_is_cancelled(cred->renew_event), "The renew event should be cancelled");
    XCTAssertEqual(cred->acquire_status, CRED_STATUS_ACQUIRE_STOPPED , "The status should be stopped");
    
    [GSSCredTestUtil delete:self.peer uuid:uuid];
    CFRELEASE_NULL(uuid);
}
#endif

#if TARGET_OS_OSX
- (void)testKerberosAcquireCredSaveLoad {
    HeimCredGlobalCTX.isMultiUser = NO;
    self.peer = [GSSCredTestUtil createPeer:@"com.apple.test" identifier:0];
    
    //create an empty cache
    CFUUIDRef uuid = NULL;
    BOOL worked = [GSSCredTestUtil createCredential:self.peer name:@"test@EXAMPLE.COM" attributes:NULL returningUuid:&uuid];
    
    XCTAssertTrue(worked, "cache should be created successfully");
    
    NSDictionary *acquireAttributes = @{ (id)kHEIMObjectType: (id)kHEIMObjectKerberosAcquireCred,
					 (id)kHEIMAttrType:(id)kHEIMTypeKerberosAcquireCred,
					 (id)kHEIMAttrParentCredential:(__bridge id)uuid,
					 (id)kHEIMAttrClientName:@"test@EXAMPLE.COM",
					 (id)kHEIMAttrData:(id)[@"this is fake password data" dataUsingEncoding:NSUTF8StringEncoding],
					 (id)kHEIMAttrCredential:(id)kCFBooleanTrue,
    };
    
    XCTestExpectation *expectation = [[XCTestExpectation alloc] initWithDescription:@"A Kerberos Acquire Cred cred should be acquired"];
    XCTestExpectation *saveExpectation = [[XCTestExpectation alloc] initWithDescription:@"The creds need to be saved."];
    _saveExpectation = saveExpectation;
    
    CFDictionaryRef attrs;
    worked = [GSSCredTestUtil executeCreateCred:self.peer forAttributes:(__bridge CFDictionaryRef _Nonnull)(acquireAttributes) returningDictionary:&attrs];
    CFUUIDRef credUUID = CFDictionaryGetValue(attrs, kHEIMAttrUUID);
    if (credUUID) {
	NSString *uuidString = CFBridgingRelease(CFUUIDCreateString(NULL, credUUID));
	self.mockHelperClient.expireExpectations[uuidString] = expectation;
    }
    XCTAssertTrue(worked, "Kerberos Acquire Cred item should be created successfully");
    XCTAssertEqual([GSSCredTestUtil itemCount:self.peer], 2, "There should be one parent and one child");
    
    [self waitForExpectations:@[expectation, saveExpectation] timeout:15];

    HeimCredRef beforeCred = (HeimCredRef)CFDictionaryGetValue(self.peer->session->items, credUUID);
    CFRetain(beforeCred);
   
    [GSSCredTestUtil freePeer:self.peer];

    CFRELEASE_NULL(HeimCredCTX.sessions);
    HeimCredCTX.sessions = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    
    readCredCache();
    
    self.peer = [GSSCredTestUtil createPeer:@"com.apple.test" identifier:0];
    
    HeimCredRef afterCred = (HeimCredRef)CFDictionaryGetValue(self.peer->session->items, credUUID);
    
    XCTAssertEqual(beforeCred->acquire_status, afterCred->acquire_status, "The acquire status should match");
    XCTAssertEqual(beforeCred->expire, afterCred->expire, "The expire times should match");
    XCTAssertEqual(beforeCred->next_acquire_time, afterCred->next_acquire_time, "The next acquire times should match");
    XCTAssertEqual(beforeCred->renew_time, afterCred->renew_time, "The renew times should match");
    
    CFRELEASE_NULL(beforeCred);
}
#endif

static NSArray<NSString*> *_entitlements;
static NSString *_altDSID;
static int _currentUid;
static int _currentAsid;
static NSString *_currentSignedIdentifier;
static XCTestExpectation *_notifyExpectation;
static XCTestExpectation *_saveExpectation;

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
    [GSSCredTestUtil flushCache];
    [_saveExpectation fulfill];
}

static CFPropertyListRef getValueFromPreferencesMock(CFStringRef key)
{
    if (CFEqual(key, CFSTR("renew-interval"))) {
	return (__bridge CFPropertyListRef)([NSNumber numberWithInt:60]);
    }
    return NULL;
}

static void notifyCachesMock(void)
{
    [_notifyExpectation fulfill];
}

static void executeOnRunQueueMock(dispatch_block_t block) {
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), block);
}

@end

