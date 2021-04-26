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
#import "SecDbKeychainItem.h"
#import "SecdTestKeychainUtilities.h"
#import "CKKS.h"
#import "SecDbKeychainItemV7.h"
#import "SecDbKeychainMetadataKeyStore.h"
#import "SecDbBackupManager_Internal.h"
#import "SecAKSObjCWrappers.h"
#import "SecItemPriv.h"
#import "SecTaskPriv.h"
#import "server_security_helpers.h"
#import "SecItemServer.h"
#import "spi.h"
#import "SecDbKeychainSerializedItemV7.h"
#import "SecDbKeychainSerializedMetadata.h"
#import "SecDbKeychainSerializedSecretData.h"
#import "SecDbKeychainSerializedAKSWrappedKey.h"
#import "SecCDKeychain.h"
#import <utilities/SecCFWrappers.h>
#import <SecurityFoundation/SFEncryptionOperation.h>
#import <SecurityFoundation/SFCryptoServicesErrors.h>
#import <SecurityFoundation/SFKeychain.h>
#import <XCTest/XCTest.h>
#import <OCMock/OCMock.h>
#include <corecrypto/ccpbkdf2.h>
#include <corecrypto/ccsha2.h>
#include <corecrypto/ccaes.h>
#include <corecrypto/ccmode.h>
#include <corecrypto/ccwrap.h>
#include "CheckV12DevEnabled.h"

void* testlist = NULL;

// TODO: Switch to '1' closer to deployment, but leave at '0' for now to test not breaking people
static int testCheckV12DevEnabled(void) {
    return 0;
}

@implementation KeychainXCTestFailureLogger
- (instancetype)init {
    if((self = [super init])) {
    }
    return self;
}

- (void)testCase:(XCTestCase *)testCase didRecordIssue:(XCTIssue *)issue {
    secnotice("keychainxctest", "XCTest failure: (%@)%@:%lu error: %@ -- %@\n%@",
              testCase.name,
              issue.sourceCodeContext.location.fileURL,
              (unsigned long)issue.sourceCodeContext.location.lineNumber,
              issue.compactDescription,
              issue.detailedDescription,
              issue.sourceCodeContext.callStack);
}
@end

#if USE_KEYSTORE

@interface SecDbKeychainItemV7 ()

+ (SFAESKeySpecifier*)keySpecifier;

@end

@interface FakeAKSRefKey : NSObject <SecAKSRefKey>
@end

@implementation FakeAKSRefKey {
    SFAESKey* _key;
}

- (instancetype)initWithKeybag:(keybag_handle_t)keybag keyclass:(keyclass_t)keyclass
{
    if (self = [super init]) {
        _key = [[SFAESKey alloc] initRandomKeyWithSpecifier:[[SFAESKeySpecifier alloc] initWithBitSize:SFAESKeyBitSize256] error:nil];
    }

    return self;
}

- (instancetype)initWithBlob:(NSData*)blob keybag:(keybag_handle_t)keybag
{
    if (self = [super init]) {
        _key = [[SFAESKey alloc] initWithData:blob specifier:[[SFAESKeySpecifier alloc] initWithBitSize:SFAESKeyBitSize256] error:nil];
    }

    return self;
}

- (NSData*)wrappedDataForKey:(SFAESKey*)key
{
    SFAuthenticatedEncryptionOperation* encryptionOperation = [[SFAuthenticatedEncryptionOperation alloc] initWithKeySpecifier:[[SFAESKeySpecifier alloc] initWithBitSize:SFAESKeyBitSize256]];
    return [NSKeyedArchiver archivedDataWithRootObject:[encryptionOperation encrypt:key.keyData withKey:_key error:nil] requiringSecureCoding:YES error:nil];
}

- (SFAESKey*)keyWithWrappedData:(NSData*)wrappedKeyData
{
    SFAESKeySpecifier* keySpecifier = [[SFAESKeySpecifier alloc] initWithBitSize:SFAESKeyBitSize256];
    SFAuthenticatedEncryptionOperation* encryptionOperation = [[SFAuthenticatedEncryptionOperation alloc] initWithKeySpecifier:keySpecifier];
    NSData* keyData = [encryptionOperation decrypt:[NSKeyedUnarchiver unarchivedObjectOfClass:[SFAuthenticatedCiphertext class] fromData:wrappedKeyData error:nil] withKey:_key error:nil];
    return [[SFAESKey alloc] initWithData:keyData specifier:keySpecifier error:nil];
}

- (NSData*)refKeyBlob
{
    return _key.keyData;
}

@end

@implementation SFKeychainServerFakeConnection {
    NSArray* _fakeAccessGroups;
}

- (void)setFakeAccessGroups:(NSArray*)fakeAccessGroups
{
    _fakeAccessGroups = fakeAccessGroups.copy;
}

- (NSArray*)clientAccessGroups
{
    return _fakeAccessGroups ?: @[@"com.apple.token"];
}

@end

@implementation KeychainXCTest {
    id _keychainPartialMock;
    CFArrayRef _originalAccessGroups;
    bool _simcrashenabled;
}

static KeychainXCTestFailureLogger* _testFailureLoggerVariable;

@synthesize keychainPartialMock = _keychainPartialMock;

+ (void)setUp
{
    [super setUp];
    SecCKKSDisable();

    self.testFailureLogger = [[KeychainXCTestFailureLogger alloc] init];
    [[XCTestObservationCenter sharedTestObservationCenter] addTestObserver:self.testFailureLogger];

    // Do not want test code to be allowed to init real keychain!
    secd_test_setup_temp_keychain("keychaintestthrowaway", NULL);
    securityd_init(NULL);
}

- (void)setUp
{
    _simcrashenabled = __security_simulatecrash_enabled();
    __security_simulatecrash_enable(false);

    [super setUp];
    
    self.lockState = LockStateUnlocked;
    self.allowDecryption = true;
    self.didAKSDecrypt = NO;
    self.simulateRolledAKSKey = NO;

    secnotice("keychainxctest", "Beginning test %@", self.name);

    self.keyclassUsedForAKSDecryption = 0;
    
    self.keySpecifier = [[SFAESKeySpecifier alloc] initWithBitSize:SFAESKeyBitSize256];
    [self setNewFakeAKSKey:[NSData dataWithBytes:"1234567890123456789012345678901" length:32]];
    
    self.mockSecDbKeychainItemV7 = OCMClassMock([SecDbKeychainItemV7 class]);
    [[[self.mockSecDbKeychainItemV7 stub] andCall:@selector(decryptionOperation) onObject:self] decryptionOperation];

    self.mockSecAKSObjCWrappers = OCMClassMock([SecAKSObjCWrappers class]);
    [[[[self.mockSecAKSObjCWrappers stub] andCall:@selector(fakeAKSEncryptWithKeybag:keyclass:plaintext:outKeyclass:ciphertext:error:) onObject:self] ignoringNonObjectArgs] aksEncryptWithKeybag:0 keyclass:0 plaintext:[OCMArg any] outKeyclass:NULL ciphertext:[OCMArg any] error:NULL];
    [[[[self.mockSecAKSObjCWrappers stub] andCall:@selector(fakeAKSDecryptWithKeybag:keyclass:ciphertext:outKeyclass:plaintext:error:) onObject:self] ignoringNonObjectArgs] aksDecryptWithKeybag:0 keyclass:0 ciphertext:[OCMArg any] outKeyclass:NULL plaintext:[OCMArg any] error:NULL];

    // bring back with <rdar://problem/37523001>
//    [[[self.mockSecDbKeychainItemV7 stub] andCall:@selector(isKeychainUnlocked) onObject:self] isKeychainUnlocked];

    id refKeyMock = OCMClassMock([SecAKSRefKey class]);
    [[[refKeyMock stub] andCall:@selector(alloc) onObject:[FakeAKSRefKey class]] alloc];

    checkV12DevEnabled = testCheckV12DevEnabled;
    NSArray* partsOfName = [self.name componentsSeparatedByCharactersInSet:[NSCharacterSet characterSetWithCharactersInString:@" ]"]];
    self.keychainDirectoryPrefix = partsOfName[1];

    // Calls SecKeychainDbReset which also resets metadata keys and backup manager
    secd_test_setup_temp_keychain([self.keychainDirectoryPrefix UTF8String], NULL);

    _originalAccessGroups = SecAccessGroupsGetCurrent();
    SecResetLocalSecuritydXPCFakeEntitlements();
}

- (void)tearDown
{
    [self.mockSecDbKeychainItemV7 stopMocking];
    [self.mockSecAKSObjCWrappers stopMocking];
    [self resetEntitlements];
    SecAccessGroupsSetCurrent(_originalAccessGroups);
    __security_simulatecrash_enable(_simcrashenabled);

    if(self.keychainDirectoryPrefix) {
        XCTAssertTrue(secd_test_teardown_delete_temp_keychain([self.keychainDirectoryPrefix UTF8String]), "Should be able to delete the temp keychain");
    } else {
        XCTFail("Should have had a keychain directory to remove");
    }

    secnotice("keychainxctest", "Ending test %@", self.name);

    [super tearDown];
}

+ (void)tearDown {
    secd_test_teardown_delete_temp_keychain("keychaintestthrowaway");
    SecResetLocalSecuritydXPCFakeEntitlements();
    [super tearDown];

    [[XCTestObservationCenter sharedTestObservationCenter] removeTestObserver:self.testFailureLogger];
}

+ (KeychainXCTestFailureLogger*)testFailureLogger {
    return _testFailureLoggerVariable;
}

+ (void)setTestFailureLogger:(KeychainXCTestFailureLogger*)logger {
    _testFailureLoggerVariable = logger;
}

- (bool)isKeychainUnlocked
{
    return self.lockState == LockStateUnlocked;
}

- (id)decryptionOperation
{
    return self.allowDecryption ? [[SFAuthenticatedEncryptionOperation alloc] initWithKeySpecifier:[SecDbKeychainItemV7 keySpecifier]] : nil;
}

- (bool)setNewFakeAKSKey:(NSData*)newKeyData
{
    self.fakeAKSKey = newKeyData;
    return true;
}

- (NSData*)wrapKey:(NSData*)plaintextKey withKey:(NSData*)wrappingKey
{
    const struct ccmode_ecb *ecb_mode = ccaes_ecb_encrypt_mode();
    ccecb_ctx_decl(ccecb_context_size(ecb_mode), key);
    NSMutableData* wrappedKey = [NSMutableData dataWithLength:ccwrap_wrapped_size(plaintextKey.length)];

    ccecb_init(ecb_mode, key, wrappingKey.length, wrappingKey.bytes);

    size_t obytes = 0;
    int wrap_status = ccwrap_auth_encrypt(ecb_mode, key, plaintextKey.length, plaintextKey.bytes,
                                          &obytes, wrappedKey.mutableBytes);
    if (wrap_status == 0) {
        assert(obytes == wrappedKey.length);
    } else {
        wrappedKey = nil;
    }

    ccecb_ctx_clear(ccecb_context_size(ecb_mode), key);
    return wrappedKey;
}

- (NSData*)unwrapKey:(NSData*)ciphertextKey withKey:(NSData*)wrappingKey
{
    const struct ccmode_ecb *ecb_mode = ccaes_ecb_decrypt_mode();
    ccecb_ctx_decl(ccecb_context_size(ecb_mode), key);
    NSMutableData *unwrappedKey = [NSMutableData dataWithLength:ccwrap_unwrapped_size(ciphertextKey.length)];

    ccecb_init(ecb_mode, key, wrappingKey.length, wrappingKey.bytes);

    size_t obytes = 0;
    int status = ccwrap_auth_decrypt(ecb_mode, key, ciphertextKey.length, ciphertextKey.bytes,
                                            &obytes, unwrappedKey.mutableBytes);
    if (status == 0) {
        assert(obytes == (size_t)[unwrappedKey length]);
    } else {
        unwrappedKey = nil;
    }

    ccecb_ctx_clear(ccecb_context_size(ecb_mode), key);
    return unwrappedKey;
}

- (bool)fakeAKSEncryptWithKeybag:(keybag_handle_t)keybag
                        keyclass:(keyclass_t)keyclass
                       plaintext:(NSData*)plaintext
                     outKeyclass:(keyclass_t*)outKeyclass
                      ciphertext:(NSMutableData*)ciphertextOut
                           error:(NSError**)error
{
    if (self.lockState == LockStateLockedAndDisallowAKS) {
        if (error) {
            *error = [NSError errorWithDomain:(__bridge NSString *)kSecErrorDomain code:errSecInteractionNotAllowed userInfo:NULL];
        }
        return false;
    }

    if (keybag == KEYBAG_DEVICE) {
        XCTAssertLessThanOrEqual(ciphertextOut.length, APPLE_KEYSTORE_MAX_SYM_WRAPPED_KEY_LEN);
    } else {    // this'll do for now: assume non-device bags are asymmetric backup bags
        XCTAssertLessThanOrEqual(ciphertextOut.length, APPLE_KEYSTORE_MAX_ASYM_WRAPPED_KEY_LEN);
    }

    NSData* wrappedKey = [self wrapKey:plaintext withKey:self.fakeAKSKey];
    if (ciphertextOut.length >= wrappedKey.length) {
        memcpy(ciphertextOut.mutableBytes, wrappedKey.bytes, wrappedKey.length);
        ciphertextOut.length = wrappedKey.length;   // simulate ks_crypt behavior
        if (self.simulateRolledAKSKey && outKeyclass) {
            *outKeyclass = keyclass | (key_class_last + 1);
        } else if (outKeyclass) {
            *outKeyclass = keyclass;
        }
        return true;
    } else {
        XCTFail(@"output buffer too small for wrapped key");
        return false;
    }
}

- (bool)fakeAKSDecryptWithKeybag:(keybag_handle_t)keybag
                        keyclass:(keyclass_t)keyclass
                      ciphertext:(NSData*)ciphertextIn
                     outKeyclass:(keyclass_t*)outKeyclass
                       plaintext:(NSMutableData*)plaintext
                           error:(NSError**)error
{
    if (self.lockState == LockStateLockedAndDisallowAKS) {
        if (error) {
            *error = [NSError errorWithDomain:(__bridge NSString *)kSecErrorDomain code:errSecInteractionNotAllowed userInfo:NULL];
        }
        return false;
    }
    
    if (self.simulateRolledAKSKey && keyclass < key_class_last) {
        // let's make decryption fail like it would if this were an old metadata key entry made with a generational AKS key, but we didn't store that info in the database
        return false;
    }

    self.keyclassUsedForAKSDecryption = keyclass;
    NSData* unwrappedKey = [self unwrapKey:ciphertextIn withKey:self.fakeAKSKey];
    if (unwrappedKey && plaintext.length >= unwrappedKey.length) {
        memcpy(plaintext.mutableBytes, unwrappedKey.bytes, unwrappedKey.length);
        plaintext.length = unwrappedKey.length;     // simulate ks_crypt behavior
        self.didAKSDecrypt = YES;
        return true;
    } else if (unwrappedKey) {
        XCTFail(@"output buffer too small for unwrapped key");
        return false;
    } else {
        if (error && !self.simulateRolledAKSKey && keyclass > key_class_last) {
            // for this case we want to simulate what happens when we try decrypting with a rolled keyclass on a device which has never been rolled, which is it ends up with a NotPermitted error from AKS which the security layer translates as locked keybag
            *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecInteractionNotAllowed userInfo:nil];
        }
        else if (error) {
            *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecDecode userInfo:nil];
        }
        return false;
    }
}

- (NSData*)getDatabaseKeyDataWithError:(NSError**)error
{
    if (_lockState == LockStateUnlocked) {
        return [NSData dataWithBytes:"1234567890123456789012345678901" length:32];
    }
    else {
        if (error) {
            // <rdar://problem/38972671> add SFKeychainErrorDeviceLocked
            *error = [NSError errorWithDomain:SFKeychainErrorDomain code:SFKeychainErrorFailedToCommunicateWithServer userInfo:nil];
        }
        return nil;
    }
}

// Mock SecTask entitlement retrieval API, so that we can test access group entitlement parsing code in SecTaskCopyAccessGroups()
static NSDictionary *currentEntitlements = nil;
static BOOL currentEntitlementsValidated = YES;

CFTypeRef SecTaskCopyValueForEntitlement(SecTaskRef task, CFStringRef entitlement, CFErrorRef *error) {
    id value = currentEntitlements[(__bridge id)entitlement];
    if (value == nil && error != NULL) {
        *error = (CFErrorRef)CFBridgingRetain([NSError errorWithDomain:NSPOSIXErrorDomain code:EINVAL userInfo:nil]);
    }
    return CFBridgingRetain(value);
}

Boolean SecTaskEntitlementsValidated(SecTaskRef task) {
    return currentEntitlementsValidated;
}

- (void)setEntitlements:(NSDictionary<NSString *, id> *)entitlements validated:(BOOL)validated {
    currentEntitlements = entitlements;
    currentEntitlementsValidated = validated;
    id task = CFBridgingRelease(SecTaskCreateFromSelf(kCFAllocatorDefault));
    NSArray *currentAccessGroups = CFBridgingRelease(SecTaskCopyAccessGroups((__bridge SecTaskRef)task));
    SecAccessGroupsSetCurrent((__bridge CFArrayRef)currentAccessGroups);    // SetCurrent retains the access groups
}

- (void)resetEntitlements {
    currentEntitlements = nil;
    currentEntitlementsValidated = YES;
}

@end

#endif
