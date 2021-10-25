
#if OCTAGON

#import "keychain/ckks/CKKSKeychainBackedKey.h"

#include <CloudKit/CloudKit.h>
#include <CloudKit/CloudKit_Private.h>
#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>

#import "keychain/categories/NSError+UsefulConstructors.h"
#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSItem.h"

@implementation CKKSKeychainBackedKey

- (instancetype)initWithWrappedAESKey:(CKKSWrappedAESSIVKey*)wrappedaeskey
                                 uuid:(NSString*)uuid
                        parentKeyUUID:(NSString*)parentKeyUUID
                             keyclass:(CKKSKeyClass*)keyclass
                               zoneID:(CKRecordZoneID*)zoneID
{
    return [self initWithAESKey:nil
                  wrappedAESKey:wrappedaeskey
                           uuid:uuid
                  parentKeyUUID:parentKeyUUID
                       keyclass:keyclass
                         zoneID:zoneID];
}

- (instancetype)initWithAESKey:(CKKSAESSIVKey* _Nullable)aeskey
                 wrappedAESKey:(CKKSWrappedAESSIVKey*)wrappedaeskey
                          uuid:(NSString*)uuid
                 parentKeyUUID:(NSString*)parentKeyUUID
                      keyclass:(CKKSKeyClass*)keyclass
                        zoneID:(CKRecordZoneID*)zoneID
{
    if((self = [super init])) {
        _uuid = uuid;
        _parentKeyUUID = parentKeyUUID;
        _zoneID = zoneID;

        _wrappedkey = wrappedaeskey;

        _keyclass = keyclass;
        _aessivkey = aeskey;
    }
    return self;
}

- (instancetype)copyWithZone:(NSZone*)zone
{
    return [[CKKSKeychainBackedKey allocWithZone:zone] initWithAESKey:[self.aessivkey copy]
                                                        wrappedAESKey:self.wrappedkey
                                                                 uuid:self.uuid
                                                        parentKeyUUID:self.parentKeyUUID
                                                             keyclass:self.keyclass
                                                               zoneID:self.zoneID];
}


- (BOOL)isEqual:(id)object
{
    if(![object isKindOfClass:[CKKSKeychainBackedKey class]]) {
        return NO;
    }

    CKKSKeychainBackedKey* obj = (CKKSKeychainBackedKey*)object;

    return ([self.uuid isEqual:obj.uuid] && [self.parentKeyUUID isEqual:obj.parentKeyUUID] &&
            [self.zoneID isEqual:obj.zoneID] && [self.wrappedkey isEqual:obj.wrappedkey] &&
            [self.keyclass isEqual:obj.keyclass] && true)
               ? YES
               : NO;
}

- (bool)wrapsSelf
{
    return [self.uuid isEqual:self.parentKeyUUID];
}

- (bool)wrapUnder:(CKKSKeychainBackedKey*)wrappingKey
            error:(NSError* __autoreleasing*)error
{
    NSError* localError = nil;
    CKKSWrappedAESSIVKey* wrappedKey = [wrappingKey wrapAESKey:self.aessivkey error:&localError];
    if (wrappedKey == nil) {
        ckkserror_global("ckkskey", "couldn't wrap key: %@", localError);
        if(error) {
            *error = localError;
        }
        return false;
    } else {
        self.wrappedkey = wrappedKey;
        self.parentKeyUUID = wrappingKey.uuid;
    }
    return true;
}

- (bool)unwrapSelfWithAESKey:(CKKSAESSIVKey*)unwrappingKey
                       error:(NSError* __autoreleasing*)error
{
    _aessivkey = [unwrappingKey unwrapAESKey:self.wrappedkey error:error];
    return (_aessivkey != nil);
}

+ (CKKSKeychainBackedKey* _Nullable)randomKeyWrappedByParent:(CKKSKeychainBackedKey*)parentKey
                                                       error:(NSError* __autoreleasing*)error
{
    return
        [self randomKeyWrappedByParent:parentKey keyclass:parentKey.keyclass error:error];
}

+ (CKKSKeychainBackedKey* _Nullable)randomKeyWrappedByParent:(CKKSKeychainBackedKey*)parentKey
                                                    keyclass:(CKKSKeyClass*)keyclass
                                                       error:(NSError* __autoreleasing*)error
{
    CKKSAESSIVKey* aessivkey = [CKKSAESSIVKey randomKey:error];
    if(aessivkey == nil) {
        return nil;
    }

    CKKSWrappedAESSIVKey* wrappedKey = [parentKey wrapAESKey:aessivkey error:error];
    if(wrappedKey == nil) {
        return nil;
    }

    return [[CKKSKeychainBackedKey alloc] initWithAESKey:aessivkey
                                           wrappedAESKey:wrappedKey
                                                    uuid:[[NSUUID UUID] UUIDString]
                                           parentKeyUUID:parentKey.uuid
                                                keyclass:keyclass
                                                  zoneID:parentKey.zoneID];
}

+ (CKKSKeychainBackedKey* _Nullable)randomKeyWrappedBySelf:(CKRecordZoneID*)zoneID
                                                     error:(NSError* __autoreleasing*)error
{
    CKKSAESSIVKey* aessivkey = [CKKSAESSIVKey randomKey:error];
    if(aessivkey == nil) {
        return nil;
    }

    return [self keyWrappedBySelf:aessivkey
                             uuid:[[NSUUID UUID] UUIDString]
                         keyclass:SecCKKSKeyClassTLK
                           zoneID:zoneID
                            error:error];
}

+ (CKKSKeychainBackedKey* _Nullable)keyWrappedBySelf:(CKKSAESSIVKey*)aeskey
                                                uuid:(NSString*)uuid
                                            keyclass:(CKKSKeyClass*)keyclass
                                              zoneID:(CKRecordZoneID*)zoneID
                                               error:(NSError**)error
{
    return [self key:aeskey
        wrappedByKey:aeskey
                uuid:uuid
       parentKeyUUID:uuid
            keyclass:keyclass
              zoneID:zoneID
               error:error];
}

+ (CKKSKeychainBackedKey* _Nullable)key:(CKKSAESSIVKey*)aessivkey
                           wrappedByKey:(CKKSAESSIVKey*)wrappingKey
                                   uuid:(NSString*)uuid
                          parentKeyUUID:(NSString*)parentKeyUUID
                               keyclass:(CKKSKeyClass*)keyclass
                                 zoneID:(CKRecordZoneID*)zoneID
                                  error:(NSError**)error
{

    NSError* localError = nil;
    CKKSWrappedAESSIVKey* wrappedKey = [wrappingKey wrapAESKey:aessivkey error:&localError];
    if (wrappedKey == nil) {
        ckkserror_global("ckkskey", "couldn't wrap key: %@", localError);
        if(error) {
            *error = localError;
        }
        return nil;
    }

    return [[CKKSKeychainBackedKey alloc] initWithAESKey:aessivkey
                                           wrappedAESKey:wrappedKey
                                                    uuid:uuid
                                           parentKeyUUID:parentKeyUUID
                                                keyclass:keyclass
                                                  zoneID:zoneID];
}

- (CKKSAESSIVKey*)ensureKeyLoadedFromKeychain:(NSError* __autoreleasing*)error
{
    if(self.aessivkey) {
        return self.aessivkey;
    }

    // Attempt to load this key from the keychain
    if([self loadKeyMaterialFromKeychain:error]) {
        return self.aessivkey;
    }

    return nil;
}

- (bool)trySelfWrappedKeyCandidate:(CKKSAESSIVKey*)candidate
                             error:(NSError* __autoreleasing*)error
{
    if(![self wrapsSelf]) {
        if(error) {
            *error = [NSError errorWithDomain:CKKSErrorDomain
                                         code:CKKSKeyNotSelfWrapped
                                     userInfo:@{
                                         NSLocalizedDescriptionKey : [NSString
                                             stringWithFormat:@"%@ is not self-wrapped", self]
                                     }];
        }
        return false;
    }

    CKKSAESSIVKey* unwrapped = [candidate unwrapAESKey:self.wrappedkey error:error];
    if(unwrapped && [unwrapped isEqual:candidate]) {
        _aessivkey = unwrapped;
        return true;
    }

    return false;
}

- (CKKSWrappedAESSIVKey*)wrapAESKey:(CKKSAESSIVKey*)keyToWrap
                              error:(NSError* __autoreleasing*)error
{
    CKKSAESSIVKey* key = [self ensureKeyLoadedFromKeychain:error];
    CKKSWrappedAESSIVKey* wrappedkey = [key wrapAESKey:keyToWrap error:error];
    return wrappedkey;
}

- (CKKSAESSIVKey*)unwrapAESKey:(CKKSWrappedAESSIVKey*)keyToUnwrap
                         error:(NSError* __autoreleasing*)error
{
    CKKSAESSIVKey* key = [self ensureKeyLoadedFromKeychain:error];
    CKKSAESSIVKey* unwrappedkey = [key unwrapAESKey:keyToUnwrap error:error];
    return unwrappedkey;
}

- (NSData*)encryptData:(NSData*)plaintext
     authenticatedData:(NSDictionary<NSString*, NSData*>*)ad
                 error:(NSError* __autoreleasing*)error
{
    CKKSAESSIVKey* key = [self ensureKeyLoadedFromKeychain:error];
    NSData* data = [key encryptData:plaintext authenticatedData:ad error:error];
    return data;
}

- (NSData*)decryptData:(NSData*)ciphertext
     authenticatedData:(NSDictionary<NSString*, NSData*>*)ad
                 error:(NSError* __autoreleasing*)error
{
    CKKSAESSIVKey* key = [self ensureKeyLoadedFromKeychain:error];
    NSData* data = [key decryptData:ciphertext authenticatedData:ad error:error];
    return data;
}

/* Functions to load and save keys from the keychain (where we get to store actual key material!) */
- (BOOL)saveKeyMaterialToKeychain:(NSError* __autoreleasing*)error
{
    return [self saveKeyMaterialToKeychain:true error:error];
}

- (BOOL)saveKeyMaterialToKeychain:(bool)stashTLK error:(NSError* __autoreleasing*)error
{
    // Note that we only store the key class, view, UUID, parentKeyUUID, and key material in the keychain
    // Any other metadata must be stored elsewhere and filled in at load time.

    if(![self ensureKeyLoadedFromKeychain:error]) {
        // No key material, nothing to save to keychain.
        return NO;
    }

    // iOS keychains can't store symmetric keys, so we're reduced to storing this key as a password
    NSData* keydata =
        [[[NSData alloc] initWithBytes:self.aessivkey->key length:self.aessivkey->size]
            base64EncodedDataWithOptions:0];
    NSMutableDictionary* query = [@{
        (id)kSecClass : (id)kSecClassInternetPassword,
        (id)kSecAttrAccessible : (id)kSecAttrAccessibleWhenUnlocked,
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
        (id)kSecAttrDescription : self.keyclass,
        (id)kSecAttrServer : self.zoneID.zoneName,
        (id)kSecAttrAccount : self.uuid,
        (id)kSecAttrPath : self.parentKeyUUID,
        (id)kSecAttrIsInvisible : @YES,
        (id)kSecValueData : keydata,
    } mutableCopy];

    // Only TLKs are synchronizable. Other keyclasses must synchronize via key hierarchy.
    if([self.keyclass isEqualToString:SecCKKSKeyClassTLK]) {
        // Use PCS-MasterKey view so they'll be initial-synced under SOS.
        query[(id)kSecAttrSyncViewHint] = (id)kSecAttrViewHintPCSMasterKey;
        query[(id)kSecAttrSynchronizable] = (id)kCFBooleanTrue;
    }

    // Class C keys are accessible after first unlock; TLKs and Class A keys are accessible only when unlocked
    if([self.keyclass isEqualToString:SecCKKSKeyClassC]) {
        query[(id)kSecAttrAccessible] = (id)kSecAttrAccessibleAfterFirstUnlock;
    } else {
        query[(id)kSecAttrAccessible] = (id)kSecAttrAccessibleWhenUnlocked;
    }

    NSError* localError = nil;
    [CKKSKeychainBackedKey setKeyMaterialInKeychain:query error:&localError];

    if(localError && error) {
        *error = [NSError errorWithDomain:@"securityd"
                                     code:localError.code
                                 userInfo:@{
                                     NSLocalizedDescriptionKey :
                                         [NSString stringWithFormat:@"Couldn't save %@ to keychain: %d",
                                                                    self,
                                                                    (int)localError.code],
                                     NSUnderlyingErrorKey : localError,
                                 }];
    }

    // TLKs are synchronizable. Stash them nonsyncably nearby.
    // Don't report errors here.
    if(stashTLK && [self.keyclass isEqualToString:SecCKKSKeyClassTLK]) {
        query = [@{
            (id)kSecClass : (id)kSecClassInternetPassword,
            (id)kSecAttrAccessible : (id)kSecAttrAccessibleWhenUnlocked,
            (id)kSecUseDataProtectionKeychain : @YES,
            (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
            (id)kSecAttrDescription : [self.keyclass stringByAppendingString:@"-nonsync"],
            (id)kSecAttrServer : self.zoneID.zoneName,
            (id)kSecAttrAccount : self.uuid,
            (id)kSecAttrPath : self.parentKeyUUID,
            (id)kSecAttrIsInvisible : @YES,
            (id)kSecValueData : keydata,
        } mutableCopy];
        query[(id)kSecAttrSynchronizable] = (id)kCFBooleanFalse;

        NSError* stashError = nil;
        [CKKSKeychainBackedKey setKeyMaterialInKeychain:query error:&localError];

        if(stashError) {
            ckkserror_global("ckkskey", "Couldn't stash %@ to keychain: %@", self, stashError);
        }
    }

    return (localError == nil) ? YES : NO;
}

+ (NSDictionary*)setKeyMaterialInKeychain:(NSDictionary*)query
                                    error:(NSError* __autoreleasing*)error
{
    CFTypeRef result = NULL;
    OSStatus status = SecItemAdd((__bridge CFDictionaryRef)query, &result);

    NSError* localerror = nil;

    // Did SecItemAdd fall over due to an existing item?
    if(status == errSecDuplicateItem) {
        // Add every primary key attribute to this find dictionary
        NSMutableDictionary* findQuery = [[NSMutableDictionary alloc] init];
        findQuery[(id)kSecClass] = query[(id)kSecClass];
        findQuery[(id)kSecAttrSynchronizable] = query[(id)kSecAttrSynchronizable];
        findQuery[(id)kSecAttrSyncViewHint] = query[(id)kSecAttrSyncViewHint];
        findQuery[(id)kSecAttrAccessGroup] = query[(id)kSecAttrAccessGroup];
        findQuery[(id)kSecAttrAccount] = query[(id)kSecAttrAccount];
        findQuery[(id)kSecAttrServer] = query[(id)kSecAttrServer];
        findQuery[(id)kSecAttrPath] = query[(id)kSecAttrPath];
        findQuery[(id)kSecUseDataProtectionKeychain] = query[(id)kSecUseDataProtectionKeychain];

        NSMutableDictionary* updateQuery = [query mutableCopy];
        updateQuery[(id)kSecClass] = nil;

        status = SecItemUpdate((__bridge CFDictionaryRef)findQuery, (__bridge CFDictionaryRef)updateQuery);

        if(status) {
            localerror = [NSError
                errorWithDomain:@"securityd"
                           code:status
                    description:[NSString stringWithFormat:@"SecItemUpdate: %d", (int)status]];
        }
    } else {
        localerror = [NSError
            errorWithDomain:@"securityd"
                       code:status
                description:[NSString stringWithFormat:@"SecItemAdd: %d", (int)status]];
    }

    if(status) {
        CFReleaseNull(result);

        if(error) {
            *error = localerror;
        }
        return nil;
    }

    NSDictionary* resultDict = CFBridgingRelease(result);
    return resultDict;
}

+ (NSDictionary*)queryKeyMaterialInKeychain:(NSDictionary*)query
                                      error:(NSError* __autoreleasing*)error
{
    CFTypeRef result = NULL;
    OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)query, &result);

    if(status) {
        CFReleaseNull(result);

        if(error) {
            *error = [NSError
                errorWithDomain:@"securityd"
                           code:status
                       userInfo:@{
                           NSLocalizedDescriptionKey :
                               [NSString stringWithFormat:@"SecItemCopyMatching: %d", (int)status]
                       }];
        }
        return nil;
    }

    NSDictionary* resultDict = CFBridgingRelease(result);
    return resultDict;
}

+ (NSDictionary*)fetchKeyMaterialItemFromKeychain:(CKKSKeychainBackedKey*)key
                                           resave:(bool*)resavePtr
                                            error:(NSError* __autoreleasing*)error
{
    NSMutableDictionary* query = [@{
        (id)kSecClass : (id)kSecClassInternetPassword,
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
        (id)kSecAttrDescription : key.keyclass,
        (id)kSecAttrAccount : key.uuid,
        (id)kSecAttrServer : key.zoneID.zoneName,
        (id)kSecAttrPath : key.parentKeyUUID,
        (id)kSecReturnAttributes : @YES,
        (id)kSecReturnData : @YES,
    } mutableCopy];

    // Synchronizable items are only found if you request synchronizable items. Only TLKs are synchronizable.
    if([key.keyclass isEqualToString:SecCKKSKeyClassTLK]) {
        query[(id)kSecAttrSynchronizable] = (id)kCFBooleanTrue;
    }

    NSError* localError = nil;
    NSDictionary* result = [self queryKeyMaterialInKeychain:query error:&localError];
    NSError* originalError = localError;

    // If we found the item or errored in some interesting way, return.
    if(result) {
        return result;
    }
    if(localError && localError.code != errSecItemNotFound) {
        if(error) {
            *error = [NSError errorWithDomain:@"securityd"
                                         code:localError.code
                                     userInfo:@{
                                         NSLocalizedDescriptionKey :
                                             [NSString stringWithFormat:@"Couldn't load %@ from keychain: %d",
                                                                        key,
                                                                        (int)localError.code],
                                         NSUnderlyingErrorKey : localError,
                                     }];
        }
        return result;
    }
    localError = nil;

    if([key.keyclass isEqualToString:SecCKKSKeyClassTLK]) {
        //didn't find a regular tlk?  how about a piggy?
        query = [@{
            (id)kSecClass : (id)kSecClassInternetPassword,
            (id)kSecUseDataProtectionKeychain : @YES,
            (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
            (id)kSecAttrDescription : [key.keyclass stringByAppendingString:@"-piggy"],
            (id)kSecAttrSynchronizable : (id)kSecAttrSynchronizableAny,
            (id)kSecAttrAccount : [NSString stringWithFormat:@"%@-piggy", key.uuid],
            (id)kSecAttrServer : key.zoneID.zoneName,
            (id)kSecReturnAttributes : @YES,
            (id)kSecReturnData : @YES,
            (id)kSecMatchLimit : (id)kSecMatchLimitOne,
        } mutableCopy];

        result = [self queryKeyMaterialInKeychain:query error:&localError];
        if(localError == nil) {
            ckksnotice_global("ckkskey", "loaded a piggy TLK (%@)", key.uuid);

            if(resavePtr) {
                *resavePtr = true;
            }

            return result;
        }

        if(localError && localError.code != errSecItemNotFound) {
            if(error) {
                *error = [NSError errorWithDomain:@"securityd"
                                             code:localError.code
                                         userInfo:@{
                                             NSLocalizedDescriptionKey : [NSString
                                                 stringWithFormat:@"Couldn't load %@ from keychain: %d",
                                                                  key,
                                                                  (int)localError.code],
                                             NSUnderlyingErrorKey : localError,
                                         }];
            }
            return nil;
        }
    }

    localError = nil;

    // Try to load a stashed TLK
    if([key.keyclass isEqualToString:SecCKKSKeyClassTLK]) {
        localError = nil;

        // Try to look for the non-syncable stashed tlk and resurrect it.
        query = [@{
            (id)kSecClass : (id)kSecClassInternetPassword,
            (id)kSecUseDataProtectionKeychain : @YES,
            (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
            (id)kSecAttrDescription : [key.keyclass stringByAppendingString:@"-nonsync"],
            (id)kSecAttrServer : key.zoneID.zoneName,
            (id)kSecAttrAccount : key.uuid,
            (id)kSecReturnAttributes : @YES,
            (id)kSecReturnData : @YES,
            (id)kSecAttrSynchronizable : @NO,
        } mutableCopy];

        result = [self queryKeyMaterialInKeychain:query error:&localError];
        if(localError == nil) {
            ckksnotice_global("ckkskey", "loaded a stashed TLK (%@)", key.uuid);

            if(resavePtr) {
                *resavePtr = true;
            }

            return result;
        }

        if(localError && localError.code != errSecItemNotFound) {
            if(error) {
                *error = [NSError errorWithDomain:@"securityd"
                                             code:localError.code
                                         userInfo:@{
                                             NSLocalizedDescriptionKey : [NSString
                                                 stringWithFormat:@"Couldn't load %@ from keychain: %d",
                                                                  key,
                                                                  (int)localError.code],
                                             NSUnderlyingErrorKey : localError,
                                         }];
            }
            return nil;
        }
    }

    // We didn't early-return. Use whatever error the original fetch produced.
    if(error) {
        *error = [NSError errorWithDomain:@"securityd"
                                     code:originalError ? originalError.code : errSecParam
                              description:[NSString stringWithFormat:@"Couldn't load %@ from keychain: %d",
                                                                     key,
                                                                     (int)originalError.code]
                               underlying:originalError];
    }

    return result;
}

- (BOOL)loadKeyMaterialFromKeychain:(NSError* __autoreleasing*)error
{
    bool resave = false;
    NSDictionary* result = [CKKSKeychainBackedKey fetchKeyMaterialItemFromKeychain:self
                                                                            resave:&resave
                                                                             error:error];
    if(!result) {
        return NO;
    }

    NSData* b64keymaterial = result[(id)kSecValueData];
    NSMutableData* keymaterial =
        [[NSMutableData alloc] initWithBase64EncodedData:b64keymaterial options:0];
    if(!keymaterial) {
        ckkserror_global("ckkskey",  "Unable to unbase64 key: %@", self);
        if(error) {
            *error = [NSError
                errorWithDomain:CKKSErrorDomain
                           code:CKKSKeyUnknownFormat
                    description:[NSString stringWithFormat:@"unable to unbase64 key: %@", self]];
        }
        return NO;
    }

    CKKSAESSIVKey* key = [[CKKSAESSIVKey alloc] initWithBytes:(uint8_t*)keymaterial.bytes
                                                          len:keymaterial.length];
    memset_s(keymaterial.mutableBytes, keymaterial.length, 0, keymaterial.length);
    self.aessivkey = key;

    if(resave) {
        ckksnotice_global("ckkskey", "Resaving %@ as per request", self);
        NSError* resaveError = nil;
        [self saveKeyMaterialToKeychain:&resaveError];
        if(resaveError) {
            ckksnotice_global("ckkskey", "Resaving %@ failed: %@", self, resaveError);
        }
    }

    return !!(self.aessivkey) ? YES : NO;
}

- (BOOL)deleteKeyMaterialFromKeychain:(NSError* __autoreleasing*)error
{
    NSMutableDictionary* query = [@{
        (id)kSecClass : (id)kSecClassInternetPassword,
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
        (id)kSecAttrDescription : self.keyclass,
        (id)kSecAttrAccount : self.uuid,
        (id)kSecAttrServer : self.zoneID.zoneName,
        (id)kSecReturnData : @YES,
    } mutableCopy];

    // Synchronizable items are only found if you request synchronizable items. Only TLKs are synchronizable.
    if([self.keyclass isEqualToString:SecCKKSKeyClassTLK]) {
        query[(id)kSecAttrSynchronizable] = (id)kCFBooleanTrue;
    }

    OSStatus status = SecItemDelete((__bridge CFDictionaryRef)query);

    if(status) {
        if(error) {
            *error = [NSError
                errorWithDomain:@"securityd"
                           code:status
                       userInfo:@{
                           NSLocalizedDescriptionKey : [NSString
                               stringWithFormat:@"Couldn't delete %@ from keychain: %d", self, (int)status]
                       }];
        }
        return NO;
    }
    return YES;
}

#pragma mark Utility

- (NSString*)description
{
    return [NSString stringWithFormat:@"<%@(%@): %@ (%@)>",
                                      NSStringFromClass([self class]),
                                      self.zoneID.zoneName,
                                      self.uuid,
                                      self.keyclass];
}

- (NSData*)serializeAsProtobuf:(NSError* __autoreleasing*)error
{
    if(![self ensureKeyLoadedFromKeychain:error]) {
        return nil;
    }
    CKKSSerializedKey* proto = [[CKKSSerializedKey alloc] init];

    proto.uuid = self.uuid;
    proto.zoneName = self.zoneID.zoneName;
    proto.keyclass = self.keyclass;
    proto.key =
        [[NSData alloc] initWithBytes:self.aessivkey->key length:self.aessivkey->size];

    return proto.data;
}

+ (CKKSKeychainBackedKey*)loadFromProtobuf:(NSData*)data
                                     error:(NSError* __autoreleasing*)error
{
    CKKSSerializedKey* key = [[CKKSSerializedKey alloc] initWithData:data];
    if(key && key.uuid && key.zoneName && key.keyclass && key.key) {
        CKKSAESSIVKey* aessivkey = [[CKKSAESSIVKey alloc] initWithBytes:(uint8_t*)key.key.bytes
                                                                    len:key.key.length];

        return [CKKSKeychainBackedKey keyWrappedBySelf:aessivkey
                                                  uuid:key.uuid
                                              keyclass:(CKKSKeyClass*)key.keyclass  // TODO sanitize
                                                zoneID:[[CKRecordZoneID alloc] initWithZoneName:key.zoneName
                                                                                      ownerName:CKCurrentUserDefaultName]
                                                 error:error];
    }

    if(error) {
        *error = [NSError errorWithDomain:CKKSErrorDomain
                                     code:CKKSProtobufFailure
                              description:@"Data failed to parse as a CKKSSerializedKey"];
    }
    return nil;
}

#pragma mark NSSecureCoding

+ (BOOL)supportsSecureCoding
{
    return YES;
}

- (void)encodeWithCoder:(nonnull NSCoder*)coder
{
    [coder encodeObject:self.uuid forKey:@"uuid"];
    [coder encodeObject:self.parentKeyUUID forKey:@"parentKeyUUID"];
    [coder encodeObject:self.keyclass forKey:@"keyclass"];
    [coder encodeObject:self.zoneID forKey:@"zoneID"];
    [coder encodeObject:self.wrappedkey forKey:@"wrappedkey"];
}

- (nullable instancetype)initWithCoder:(nonnull NSCoder*)decoder
{
    if ((self = [super init])) {
        _uuid = [decoder decodeObjectOfClass:[NSString class] forKey:@"uuid"];
        _parentKeyUUID =
            [decoder decodeObjectOfClass:[NSString class] forKey:@"parentKeyUUID"];
        _keyclass = (CKKSKeyClass*)[decoder decodeObjectOfClass:[NSString class]
                                                         forKey:@"keyclass"];
        _zoneID = [decoder decodeObjectOfClass:[CKRecordZoneID class] forKey:@"zoneID"];

        _wrappedkey = [decoder decodeObjectOfClass:[CKKSWrappedAESSIVKey class]
                                            forKey:@"wrappedkey"];
    }
    return self;
}

@end

#pragma mark - CKKSKeychainBackedKeySet

@implementation CKKSKeychainBackedKeySet

- (instancetype)initWithTLK:(CKKSKeychainBackedKey*)tlk
                     classA:(CKKSKeychainBackedKey*)classA
                     classC:(CKKSKeychainBackedKey*)classC
                  newUpload:(BOOL)newUpload
{
    if((self = [super init])) {
        _tlk = tlk;
        _classA = classA;
        _classC = classC;
        _newUpload = newUpload;
    }
    return self;
}

- (NSString*)description
{
    return [NSString stringWithFormat: @"<CKKSKeychainBackedKeySet: tlk:%@, classA:%@, classC:%@, newUpload:%d>",
            self.tlk,
            self.classA,
            self.classC,
            self.newUpload];
}

+ (BOOL)supportsSecureCoding
{
    return YES;
}

- (void)encodeWithCoder:(nonnull NSCoder*)coder
{
    [coder encodeObject:self.tlk forKey:@"tlk"];
    [coder encodeObject:self.classA forKey:@"classA"];
    [coder encodeObject:self.classC forKey:@"classC"];
    [coder encodeBool:self.newUpload forKey:@"newUpload"];
}

- (nullable instancetype)initWithCoder:(nonnull NSCoder*)decoder
{
    if ((self = [super init])) {
        _tlk = [decoder decodeObjectOfClass:[CKKSKeychainBackedKey class] forKey:@"tlk"];
        _classA = [decoder decodeObjectOfClass:[CKKSKeychainBackedKey class] forKey:@"classA"];
        _classC = [decoder decodeObjectOfClass:[CKKSKeychainBackedKey class] forKey:@"classC"];
        _newUpload = [decoder decodeBoolForKey:@"newUpload"];
    }
    return self;
}

@end

#endif  // OCTAGON
