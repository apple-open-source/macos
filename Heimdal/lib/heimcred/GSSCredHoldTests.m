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

@interface GSSCredHoldTests : XCTestCase
@property (nullable) struct peer * peer;
@property (nonatomic) MockManagedAppManager *mockManagedAppManager;
@end

@implementation GSSCredHoldTests {
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
    HeimCredGlobalCTX.encryptData = encryptDataMock;
    HeimCredGlobalCTX.decryptData = decryptDataMock;
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
    archivePath = [[NSString alloc] initWithFormat:@"%@/Library/Caches/com.apple.GSSCred.simulator-archive", NSHomeDirectory()];
#else
    archivePath = @"/var/tmp/heim-credential-store.archive";
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
- (void)testCreatingAndHoldingCredential {
    HeimCredGlobalCTX.isMultiUser = NO;
    HeimCredGlobalCTX.useUidMatching = NO;
    [GSSCredTestUtil freePeer:self.peer];
    self.peer = [GSSCredTestUtil createPeer:@"com.apple.fake" identifier:0];

    CFUUIDRef uuid = NULL;
    BOOL worked = [GSSCredTestUtil createCredentialAndCache:self.peer name:@"test@EXAMPLE.COM" returningCacheUuid:&uuid];

    XCTAssertTrue(worked, "Credential should be created successfully");

    CFDictionaryRef attributes;
    worked = [GSSCredTestUtil fetchCredential:self.peer uuid:uuid returningDictionary:&attributes];
    XCTAssertTrue(worked, "Cache should be fetched successfully using it's uuid");

    int64_t error = [GSSCredTestUtil hold:self.peer uuid:uuid];
    XCTAssertEqual(error, 0, "hold should not error");

    worked = [GSSCredTestUtil fetchCredential:self.peer uuid:uuid returningDictionary:&attributes];
    XCTAssertTrue(worked, "Cache should still exist after hold");

    error = [GSSCredTestUtil unhold:self.peer uuid:uuid];
    XCTAssertEqual(error, 0, "unhold should not error");

    error = [GSSCredTestUtil unhold:self.peer uuid:uuid];
    XCTAssertEqual(error, 0, "hold when it should delete should not error");

    worked = [GSSCredTestUtil fetchCredential:self.peer uuid:uuid returningDictionary:&attributes];
    XCTAssertFalse(worked, "Cache should be deleted after retain count is zero");
}

- (void)testSaveLoadHoldingCredential {
    HeimCredGlobalCTX.isMultiUser = NO;
    HeimCredGlobalCTX.useUidMatching = NO;
    [GSSCredTestUtil freePeer:self.peer];
    self.peer = [GSSCredTestUtil createPeer:@"com.apple.fake" identifier:0];

    CFUUIDRef uuid = NULL;
    BOOL worked = [GSSCredTestUtil createCredentialAndCache:self.peer name:@"test@EXAMPLE.COM" returningCacheUuid:&uuid];

    XCTAssertTrue(worked, "Credential should be created successfully");

    CFDictionaryRef attributes;
    worked = [GSSCredTestUtil fetchCredential:self.peer uuid:uuid returningDictionary:&attributes];
    XCTAssertTrue(worked, "Cache should be fetched successfully using it's uuid");

    int64_t error = [GSSCredTestUtil hold:self.peer uuid:uuid];
    XCTAssertEqual(error, 0, "hold should not error");

    worked = [GSSCredTestUtil fetchCredential:self.peer uuid:uuid returningDictionary:&attributes];
    XCTAssertTrue(worked, "Cache should still exist after hold");

    [GSSCredTestUtil freePeer:self.peer];

    CFRELEASE_NULL(HeimCredCTX.sessions);
    HeimCredCTX.sessions = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    readCredCache();

    self.peer = [GSSCredTestUtil createPeer:@"com.apple.fake" identifier:0];

    worked = [GSSCredTestUtil fetchCredential:self.peer uuid:uuid returningDictionary:&attributes];
    XCTAssertTrue(worked, "Cache should still exist after hold");

    NSNumber *count = CFDictionaryGetValue(attributes, kHEIMAttrRetainStatus);

    XCTAssertEqual([count intValue], 2, "The retain count should match after save/load");

}

- (void)testUpdatingRetainStatusShouldFail {

    HeimCredGlobalCTX.isMultiUser = NO;
    HeimCredGlobalCTX.useUidMatching = YES;
    _currentUid = 501;
    [GSSCredTestUtil freePeer:self.peer];
    self.peer = [GSSCredTestUtil createPeer:@"com.apple.fake" identifier:0];

    CFUUIDRef uuid = NULL;
    BOOL worked = [GSSCredTestUtil createCredentialAndCache:self.peer name:@"test@EXAMPLE.COM" returningCacheUuid:&uuid];
    XCTAssertTrue(worked, "Credential was created successfully");

    NSDictionary *attributes = @{(id)kHEIMAttrRetainStatus:@10};
    uint64_t result = [GSSCredTestUtil setAttributes:self.peer uuid:uuid attributes:(__bridge CFDictionaryRef)(attributes) returningDictionary:NULL];
    XCTAssertEqual(result, kHeimCredErrorUpdateNotAllowed, "Updating the retain status should not be allowed.");

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

