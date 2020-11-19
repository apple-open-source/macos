#import "SecDbBackupTestsBase.h"
#import "keychain/securityd/SecDbBackupManager.h"

#if !SECDB_BACKUPS_ENABLED

@interface SecDbBackupTests : XCTestCase
@end

@implementation SecDbBackupTests
@end

#else // SECDB_BACKUPS_ENABLED

#import "keychain/securityd/SecDbBackupManager_Internal.h"


#import <objc/runtime.h>
#include "utilities/der_plist.h"
#include <Security/SecItemPriv.h>

@interface SecDbBackupTests : SecDbBackupTestsBase

@end

SecDbBackupManager* _manager;

@implementation SecDbBackupTests

+ (void)setUp {
    [super setUp];
}

+ (void)tearDown {
	[super tearDown];
}

- (void)setUp {
    [super setUp];
    [SecDbBackupManager resetManager];
    _manager = [SecDbBackupManager manager];
}

- (void)setFakeBagIdentity {
    SecDbBackupBagIdentity* identity = [SecDbBackupBagIdentity new];
    NSUUID* nsuuid = [NSUUID UUID];
    uuid_t uuid;
    [nsuuid getUUIDBytes:uuid];
    identity.baguuid = [NSData dataWithBytes:uuid length:UUIDBYTESLENGTH];
    NSMutableData* digest = [NSMutableData dataWithLength:CC_SHA512_DIGEST_LENGTH];
    CC_SHA512(identity.baguuid.bytes, (CC_LONG)identity.baguuid.length, digest.mutableBytes);
    identity.baghash = digest;
    _manager.bagIdentity = identity;
}

- (SFAESKey*)randomAESKey {
    return [[SFAESKey alloc] initRandomKeyWithSpecifier:[[SFAESKeySpecifier alloc] initWithBitSize:SFAESKeyBitSize256] error:nil];
}

- (NSData*)bagIdentData {
    NSDictionary* bagIdentDict = @{@"baguuid" : _manager.bagIdentity.baguuid, @"baghash" : _manager.bagIdentity.baghash};
    return (__bridge_transfer NSData*)CFPropertyListCreateDERData(NULL, (__bridge CFDictionaryRef)bagIdentDict, NULL);
}

#pragma mark - Tests

- (void)testAAA_MakeSureThisMakesSense {
    XCTAssertEqual(checkV12DevEnabled(), 1, "V12 dev flag is off, no good will come of this");
    if (!checkV12DevEnabled) {
        abort();
    }
}

- (void)testCreateBackupBagSecret {
    NSError* error;
    NSData* secret = [_manager createBackupBagSecret:&error];
    XCTAssertNil(error, "Error creating backup bag secret");
    XCTAssertNotNil(secret, "No NSData from creating backup bag secret: %@", error);
    XCTAssertEqual(secret.length, BACKUPBAG_PASSPHRASE_LENGTH, "NSData is not %i bytes long", BACKUPBAG_PASSPHRASE_LENGTH);

    // Good luck testing randomness, but let's stipulate we don't accept all-zeroes as a key
    uint8_t buf[BACKUPBAG_PASSPHRASE_LENGTH] = {0};
    XCTAssertNotEqual(memcmp(secret.bytes, buf, MIN(BACKUPBAG_PASSPHRASE_LENGTH, secret.length)), 0, "Secret is all zeroes");

    XCTAssert([secret isMemberOfClass:objc_lookUpClass("_NSClrDat")], "Secret is not a zeroing NSData");
}

- (void)testCreateAndSaveBackupBag {
    NSError* error;
    NSData* secret = [_manager createBackupBagSecret:&error];
    XCTAssertNil(error, "Unable to generate secret");
    XCTAssertNotNil(secret, "Didn't get secret");
    keybag_handle_t handle = [_manager createBackupBagWithSecret:secret error:&error];
    XCTAssertNil(error, "Got error creating backup bag: %@", error);
    XCTAssertNotEqual(handle, bad_keybag_handle, "Unexpected bag handle");
    keybag_state_t keybagstate;
    XCTAssertEqual(aks_get_lock_state(handle, &keybagstate), kAKSReturnSuccess, "Unable to check lock state of backup bag");
    XCTAssert(keybagstate | keybag_state_locked, "Keybag unexpectedly not locked");
    XCTAssert(keybagstate | keybag_state_been_unlocked, "Keybag unexpectedly never been unlocked (huh?)");

    XCTAssert([_manager saveBackupBag:handle asDefault:YES error:&error]);
    XCTAssertNil(error, "Error saving backup bag to keychain");
    XCTAssertEqual(aks_unload_bag(handle), kAKSReturnSuccess, "Couldn't unload backup bag");
}

- (void)testCreateAndSaveBagTwice {
    NSError* error;
    NSData* secret = [_manager createBackupBagSecret:&error];
    XCTAssertNil(error, "Unable to generate secret");
    XCTAssertNotNil(secret, "Didn't get secret");
    keybag_handle_t handle = [_manager createBackupBagWithSecret:secret error:&error];
    XCTAssertNil(error, "Got error creating backup bag: %@", error);
    XCTAssertNotEqual(handle, bad_keybag_handle, "Unexpected bag handle");

    XCTAssert([_manager saveBackupBag:handle asDefault:YES error:&error]);
    XCTAssertNil(error, "Error saving backup bag to keychain");

    XCTAssertFalse([_manager saveBackupBag:handle asDefault:YES error:&error]);
    XCTAssert(error, "Unexpectedly did not get error saving same bag twice");
    XCTAssertEqual(error.code, SecDbBackupWriteFailure, "Unexpected error code for double insertion: %@", error);
    XCTAssertEqual(aks_unload_bag(handle), kAKSReturnSuccess, "Couldn't unload backup bag");
}

- (void)testLoadNonExistentDefaultBackupBag {
    NSError* error;
    XCTAssertEqual([_manager loadBackupBag:nil error:&error], bad_keybag_handle, "Found default bag after not inserting any");
    XCTAssertEqual(error.code, SecDbBackupNoBackupBagFound, "Didn't get an appropriate error for missing keybag: %@", error);
}

- (void)testLoadDefaultBackupBag {
    NSError* error;
    keybag_handle_t handle = bad_keybag_handle;
    handle = [_manager createBackupBagWithSecret:[_manager createBackupBagSecret:&error] error:&error];
    XCTAssertNotEqual(handle, bad_keybag_handle, "Didn't get a good keybag handle");
    XCTAssertNotEqual(handle, device_keybag_handle, "Got device keybag handle (or manager is nil)");
    XCTAssertNil(error, "Error creating backup bag");
    [_manager saveBackupBag:handle asDefault:YES error:&error];
    XCTAssertNil(error, "Error saving backup bag");

    uuid_t uuid1 = {0};
    XCTAssertEqual(aks_get_bag_uuid(handle, uuid1), kAKSReturnSuccess, "Couldn't get bag uuid");
    XCTAssertEqual(aks_unload_bag(handle), kAKSReturnSuccess, "Couldn't unload backup bag");
    handle = bad_keybag_handle;

    handle = [_manager loadBackupBag:nil error:&error];
    XCTAssertNotEqual(handle, bad_keybag_handle, "Got bad handle loading default keybag");
    XCTAssertNil(error, "Got error loading default keybag");

    uuid_t uuid2 = {0};
    XCTAssertEqual(aks_get_bag_uuid(handle, uuid2), kAKSReturnSuccess, "Couldn't get bag uuid");
    XCTAssertEqual(aks_unload_bag(handle), kAKSReturnSuccess, "Couldn't unload backup bag");
    XCTAssertEqual(memcmp(uuid1, uuid2, UUIDBYTESLENGTH), 0, "UUIDs do not match after backup bag save/load");

    // sanity check
    uuid_t uuidnull = {0};
    XCTAssertNotEqual(memcmp(uuid1, uuidnull, UUIDBYTESLENGTH), 0, "uuid1 is all zeroes");
    XCTAssertNotEqual(memcmp(uuid2, uuidnull, UUIDBYTESLENGTH), 0, "uuid2 is all zeroes");

    // TODO: signature match?
}

- (void)testLoadBackupBagByUUID {
    NSError* error;
    keybag_handle_t handle1 = bad_keybag_handle;
    handle1 = [_manager createBackupBagWithSecret:[_manager createBackupBagSecret:&error] error:&error];
    XCTAssertNotEqual(handle1, bad_keybag_handle, "Didn't get a good keybag handle");
    XCTAssertNil(error, "Error creating backup bag");
    XCTAssert([_manager saveBackupBag:handle1 asDefault:NO error:&error], "Unable to save bag 1");
    XCTAssertNil(error, "Error saving backup bag");

    keybag_handle_t handle2 = bad_keybag_handle;
    handle2 = [_manager createBackupBagWithSecret:[_manager createBackupBagSecret:&error] error:&error];
    XCTAssertNotEqual(handle2, bad_keybag_handle, "Didn't get a good keybag handle");
    XCTAssertNil(error, "Error creating backup bag");
    XCTAssert([_manager saveBackupBag:handle2 asDefault:NO error:&error], "Unable to save bag 2");
    XCTAssertNil(error, "Error saving backup bag");

    uuid_t uuid1 = {0};
    uuid_t uuid2 = {0};
    XCTAssertEqual(aks_get_bag_uuid(handle1, uuid1), kAKSReturnSuccess, "Couldn't get bag 1 uuid");
    XCTAssertEqual(aks_get_bag_uuid(handle2, uuid2), kAKSReturnSuccess, "Couldn't get bag 2 uuid");
    XCTAssertEqual(aks_unload_bag(handle1), kAKSReturnSuccess, "Couldn't unload backup bag 1");
    XCTAssertEqual(aks_unload_bag(handle2), kAKSReturnSuccess, "Couldn't unload backup bag 2");
    handle1 = bad_keybag_handle;
    handle2 = bad_keybag_handle;

    XCTAssertNotEqual(handle1 = [_manager loadBackupBag:[[NSUUID alloc] initWithUUIDBytes:uuid1] error:&error], bad_keybag_handle, "Didn't get handle loading bag 1 by UUID");
    XCTAssertNotEqual(handle2 = [_manager loadBackupBag:[[NSUUID alloc] initWithUUIDBytes:uuid2] error:&error], bad_keybag_handle, "Didn't get handle loading bag 2 by UUID");

    uuid_t uuid1_2 = {0};
    uuid_t uuid2_2 = {0};
    XCTAssertEqual(aks_get_bag_uuid(handle1, uuid1_2), kAKSReturnSuccess, "Couldn't get bag 1 uuid");
    XCTAssertEqual(aks_get_bag_uuid(handle2, uuid2_2), kAKSReturnSuccess, "Couldn't get bag 2 uuid");

    XCTAssertEqual(memcmp(uuid1, uuid1_2, UUIDBYTESLENGTH), 0, "UUIDs do not match after bag 1 save/load");
    XCTAssertEqual(memcmp(uuid2, uuid2_2, UUIDBYTESLENGTH), 0, "UUIDs do not match after bag 2 save/load");

    XCTAssertEqual(aks_unload_bag(handle1), kAKSReturnSuccess, "Couldn't unload backup bag 1");
    XCTAssertEqual(aks_unload_bag(handle2), kAKSReturnSuccess, "Couldn't unload backup bag 2");
}

- (void)testCreateBackupInfrastructure
{
    NSError* error;
    XCTAssert([_manager createOrLoadBackupInfrastructure:&error], @"Couldn't create/load backup infrastructure");
    XCTAssertNil(error, @"Error creating/loading backup infrastructure");

    SFECKeyPair* ak = [_manager fetchKCSKForKeyclass:key_class_ak error:&error];
    XCTAssertNotNil(ak);
    XCTAssertNil(error);

    SFECKeyPair* ck = [_manager fetchKCSKForKeyclass:key_class_ck error:&error];
    XCTAssertNotNil(ck);
    XCTAssertNil(error);

    SFECKeyPair* dk = [_manager fetchKCSKForKeyclass:key_class_dk error:&error];
    XCTAssertNotNil(dk);
    XCTAssertNil(error);

    SFECKeyPair* aku = [_manager fetchKCSKForKeyclass:key_class_aku error:&error];
    XCTAssertNotNil(aku);
    XCTAssertNil(error);

    SFECKeyPair* cku = [_manager fetchKCSKForKeyclass:key_class_cku error:&error];
    XCTAssertNotNil(cku);
    XCTAssertNil(error);

    SFECKeyPair* dku = [_manager fetchKCSKForKeyclass:key_class_dku error:&error];
    XCTAssertNotNil(dku);
    XCTAssertNil(error);

    SFECKeyPair* akpu = [_manager fetchKCSKForKeyclass:key_class_akpu error:&error];
    XCTAssertNil(akpu);
    XCTAssertEqual(error.code, SecDbBackupNoKCSKFound);
}

- (void)testCreateOrLoadBackupInfrastructureFromC
{
    CFErrorRef cferror = NULL;
    XCTAssertTrue(SecDbBackupCreateOrLoadBackupInfrastructure(&cferror), @"Could create backup infrastructure from C");
    XCTAssertFalse(cferror, @"Do not expect error creating backup infrastructure from C: %@", cferror);
    CFReleaseNull(cferror);
}

// Should not run this on real AKS because don't want to lock keybag
- (void)disabledtestCreateOrLoadBackupInfrastructureWhileLocked
{
    NSError* error;
    XCTAssertFalse([_manager createOrLoadBackupInfrastructure:&error], @"Keychain locked, don't expect to create infrastructure");
    XCTAssertEqual(error.code, SecDbBackupKeychainLocked, @"Expected failure creating backup infrastructure while locked");
}

// Should not run this on real AKS because don't want to lock keybag
- (void)disabledtestCreateOrLoadBackupInfrastructureWhileLockedFromC
{
    CFErrorRef cferror = NULL;
    XCTAssertFalse(SecDbBackupCreateOrLoadBackupInfrastructure(&cferror), @"Could create backup infrastructure from C");
    XCTAssertTrue(cferror, @"Expect error creating backup infrastructure while locked from C: %@", cferror);
    if (cferror) {
        XCTAssertEqual(CFErrorGetCode(cferror), errSecInteractionNotAllowed, @"Expect errSecInteractionNotAllowed creating backup infrastructure while locked from C");
    }
    CFReleaseNull(cferror);
}

- (void)testOnlyOneDefaultBackupBag {
    // Generate two backup bags, each successively claiming makeDefault
    // Expect: the second call should fail
}

- (void)testLoadBackupBagFromDifferentDevice {
    // Generate keybag on some device, manually insert it on some other device and try to load it.
    // Expect: failure unless in recovery mode.
}

- (void)testLoadBackupBagWithGarbageData {
    // manually write garbage to keychain, then call loadBackupBag
    // Expect: ?
}

- (void)testCreateKCSK {
    [self setFakeBagIdentity];

    NSError* error;
    XCTAssertNil([_manager createKCSKForKeyClass:key_class_ck withWrapper:nil error:&error], @"Shouldn't get KCSK without wrapper");
    XCTAssertEqual(error.code, SecDbBackupInvalidArgument, @"createKSCKForKeyClass ought to be angry about not having a wrapper");
    error = nil;

    SFAESKey* key = [self randomAESKey];
    XCTAssertNotNil(key, @"Expect key from SFAESKey");
    SecDbBackupKeyClassSigningKey* kcsk = [_manager createKCSKForKeyClass:key_class_ak withWrapper:key error:&error];
    XCTAssertNotNil(kcsk, @"Got a KCSK");
    XCTAssertNil(error, @"Did not expect KCSK error: %@", error);

    // Let's examine the KCSK

    XCTAssertEqual(kcsk.keyClass, key_class_ak, @"key class matches");

    // Verify refkey
    aks_ref_key_t refkey = NULL;
    XCTAssertNotNil(kcsk.aksRefKey, @"Got an AKS ref key");
    XCTAssertEqual(aks_ref_key_create_with_blob(KEYBAG_DEVICE, kcsk.aksRefKey.bytes,
                                                kcsk.aksRefKey.length, &refkey), kAKSReturnSuccess, @"Got a refkey out of kcsk blob");

    // Verify aksWrappedKey
    void* aksunwrappedbytes = NULL;
    size_t aksunwrappedlen = 0;
    XCTAssertEqual(aks_ref_key_decrypt(refkey, NULL, 0, kcsk.aksWrappedKey.bytes, kcsk.aksWrappedKey.length, &aksunwrappedbytes, &aksunwrappedlen), kAKSReturnSuccess, @"Successfully unwrapped KCSK private key");
    SFECKeyPair* aksunwrapped = [_manager getECKeyPairFromDERBytes:aksunwrappedbytes length:aksunwrappedlen error:&error];
    XCTAssertNil(error, @"No error reconstructing AKS backup key");
    XCTAssert(aksunwrapped, @"Got key from getECKeyPairFromDERBytes");
    aks_ref_key_free(&refkey);

    // Verify backupWrappedKey
    SFAuthenticatedEncryptionOperation* op = [[SFAuthenticatedEncryptionOperation alloc] initWithKeySpecifier:[[SFAESKeySpecifier alloc] initWithBitSize:SFAESKeyBitSize256]];
    SFAuthenticatedCiphertext* ciphertext = [NSKeyedUnarchiver unarchivedObjectOfClass:[SFAuthenticatedCiphertext class] fromData:kcsk.backupWrappedKey error:&error];
    XCTAssertNotNil(ciphertext, @"Reconstituted ciphertext from kcsk (%@)", error);
    XCTAssertNil(error, @"Didn't expect error reconstituting ciphertext from kcsk: %@", error);

    NSData* bagIdentData = [self bagIdentData];
    NSData* bkunwrappedData = [op decrypt:ciphertext withKey:key additionalAuthenticatedData:bagIdentData error:&error];
    XCTAssertNotNil(bkunwrappedData, @"backup-wrapped key decrypts");
    SFECKeyPair* bkunwrapped = [[SFECKeyPair alloc] initWithData:bkunwrappedData specifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384] error:&error];
    XCTAssertNotNil(bkunwrapped, @"unwrapped blob turns into an SFECKey (%@)", error);

    XCTAssertEqualObjects(aksunwrapped, bkunwrapped, @"Private key same between aks and bk");
}

- (void)testCreateRecoverySetForRecoveryKey {
    // Not Implemented
}

- (void)testCreateRecoverySetForAKS {
    NSError* error;

    [self setFakeBagIdentity];

    SecDbBackupRecoverySet* set = [_manager createRecoverySetWithBagSecret:nil forType:SecDbBackupRecoveryTypeAKS error:&error];
    XCTAssertNil(set, @"No set without secret!");
    XCTAssertEqual(error.code, SecDbBackupInvalidArgument, @"Expected different error without secret: %@", error);
    error = nil;

    NSData* secret = [_manager createBackupBagSecret:&error];
    set = [_manager createRecoverySetWithBagSecret:secret forType:SecDbBackupRecoveryTypeAKS error:&error];
    XCTAssertNotNil(set, @"Got aks recoveryset from backup manager");
    XCTAssertNil(error, @"Didn't expect error obtaining recoveryset: %@", error);

    XCTAssertEqual(set.recoveryType, SecDbBackupRecoveryTypeAKS, @"Unexpected recovery type");
    XCTAssertEqualObjects(set.bagIdentity, _manager.bagIdentity, @"Bag identity copied properly");
    XCTAssertNotNil(set.wrappedBagSecret, @"Have bag secret in recovery set");
    XCTAssertNotNil(set.wrappedKCSKSecret, @"Have kcsk secret in recovery set");
    XCTAssertNotNil(set.wrappedRecoveryKey, @"Have recovery key in recovery set");

    NSMutableData* recoverykeydata = [NSMutableData dataWithLength:APPLE_KEYSTORE_MAX_KEY_LEN];
    [SecAKSObjCWrappers aksDecryptWithKeybag:KEYBAG_DEVICE keyclass:key_class_aku
                                  ciphertext:set.wrappedRecoveryKey outKeyclass:nil plaintext:recoverykeydata error:&error];
    XCTAssertNil(error, @"Able to decrypt recovery key: %@", error);
    SFAESKey* recoverykey = [[SFAESKey alloc] initWithData:recoverykeydata specifier:[[SFAESKeySpecifier alloc]
                                                                                      initWithBitSize:SFAESKeyBitSize256] error:&error];
    XCTAssert(recoverykey, @"Got a recovery key from blob");
    XCTAssertNil(error, @"Didn't get error from recovery key blob: %@", error);

    SFAuthenticatedEncryptionOperation* op = [[SFAuthenticatedEncryptionOperation alloc] initWithKeySpecifier:[[SFAESKeySpecifier alloc]
                                                                                                               initWithBitSize:SFAESKeyBitSize256]];
    NSData* bagsecret = [op decrypt:[NSKeyedUnarchiver unarchivedObjectOfClass:[SFAuthenticatedCiphertext class] fromData:set.wrappedBagSecret error:&error] withKey:recoverykey error:&error];
    XCTAssert(bagsecret, @"Reconstituted bag secret");
    XCTAssertNil(error, @"Didn't expect error reconstituting bag secret: %@", error);
    XCTAssertEqualObjects(bagsecret, secret, @"Returned bag secret same as provided secret");

    NSData* kcsksecret = [op decrypt:[NSKeyedUnarchiver unarchivedObjectOfClass:[SFAuthenticatedCiphertext class] fromData:set.wrappedKCSKSecret error:&error] withKey:recoverykey error:&error];
    XCTAssert(kcsksecret, @"Reconstituted kcsk secret");
    XCTAssertNil(error, @"Didn't expect error reconstituting kcsk secret: %@", error);
}

- (void)testWrapItemKey {
    SFAESKey* randomKey = [self randomAESKey];
    NSError* error;
    SecDbBackupWrappedKey* itemKey = [_manager wrapItemKey:randomKey forKeyclass:key_class_akpu error:&error];
    XCTAssertNil(itemKey, @"Do not expect result wrapping to akpu");
    XCTAssertEqual(error.code, SecDbBackupInvalidArgument, @"Expect invalid argument error wrapping to akpu");

    error = nil;
    itemKey = [_manager wrapItemKey:randomKey forKeyclass:key_class_ak error:&error];
    XCTAssertNil(error, @"No error wrapping item to ak");
    XCTAssertEqualObjects(itemKey.baguuid, _manager.bagIdentity.baguuid, @"item wrapped under expected bag uuid");

    // TODO: implement decryption and test it
}

- (void)testWrapMetadataKey {
    SFAESKey* randomKey = [self randomAESKey];
    NSError* error;
    SecDbBackupWrappedKey* itemKey = [_manager wrapMetadataKey:randomKey forKeyclass:key_class_akpu error:&error];
    XCTAssertNil(itemKey, @"Do not expect result wrapping to akpu");
    XCTAssertEqual(error.code, SecDbBackupInvalidArgument, @"Expect invalid argument error wrapping to akpu");

    error = nil;
    itemKey = [_manager wrapMetadataKey:randomKey forKeyclass:key_class_ak error:&error];
    XCTAssertNil(error, @"No error wrapping item to ak");
    XCTAssertEqualObjects(itemKey.baguuid, _manager.bagIdentity.baguuid, @"item wrapped under expected bag uuid");

    // TODO: implement decryption and test it
}

// Does not inspect the item because it's encrypted and no code yet built to do recovery.
- (void)testSecItemAddAddsBackupEncryption {
    NSDictionary* q = @{(id)kSecClass : (id)kSecClassGenericPassword,
                        (id)kSecValueData : [@"password" dataUsingEncoding:NSUTF8StringEncoding],
                        (id)kSecAttrAccessGroup : @"com.apple.security.securityd",
                        (id)kSecUseDataProtectionKeychain : @(YES),
                        (id)kSecReturnAttributes : @(YES)
                        };
    OSStatus status = SecItemAdd((__bridge CFDictionaryRef)q, NULL);
    XCTAssertEqual(status, errSecSuccess, @"Regular old SecItemAdd succeeds");

    __block CFErrorRef cfError = NULL;
    __block bool ok = true;
    __block NSData* readUUID;
    ok &= kc_with_dbt(false, &cfError, ^bool(SecDbConnectionRef dbt) {
        NSString* sql = @"SELECT backupUUID FROM genp WHERE agrp = 'com.apple.security.securityd'";
        ok &= SecDbPrepare(dbt, (__bridge CFStringRef)sql, &cfError, ^(sqlite3_stmt *stmt) {
            ok &= SecDbStep(dbt, stmt, &cfError, ^(bool *stop) {
                readUUID = [[NSData alloc] initWithBytes:sqlite3_column_blob(stmt, 0) length:sqlite3_column_bytes(stmt, 0)];
            });
        });
        return ok;
    });

    XCTAssert(ok, @"Talking to keychain went okay");
    XCTAssertEqual(cfError, NULL, @"Talking to keychain didn't yield an error (%@)", cfError);
    CFReleaseNull(cfError);
    XCTAssert(readUUID, @"Got stuff out of the keychain");

    XCTAssertEqualObjects(readUUID, _manager.bagIdentity.baguuid, @"backup UUID is good");
}

@end

#endif  // SECDB_BACKUPS_ENABLED
