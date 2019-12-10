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

#import <TargetConditionals.h>
#if !TARGET_OS_BRIDGE

#import "SecKeybagSupport.h"

#if __has_include(<libaks.h>)
#import <libaks.h>
#endif

#if __has_include(<libaks_ref_key.h>)
#import <libaks_ref_key.h>
#endif

#if __has_include(<MobileKeyBag/MobileKeyBag.h>)
#import <MobileKeyBag/MobileKeyBag.h>
#endif

#include <os/variant_private.h>

#import <ctkclient/ctkclient.h>
#import <coreauthd_spi.h>
#import <ACMLib.h>
#import <LocalAuthentication/LAPublicDefines.h>
#import <LocalAuthentication/LAPrivateDefines.h>
#import <LocalAuthentication/LACFSupport.h>
#import <SecurityFoundation/SFEncryptionOperation.h>
#import "mockaks.h"
#import "utilities/der_plist.h"
#import "tests/secdmockaks/generated_source/MockAKSRefKey.h"
#import "tests/secdmockaks/generated_source/MockAKSOptionalParameters.h"

bool hwaes_key_available(void)
{
    return false;
}

#define SET_FLAG_ON(v, flag) v |= (flag)
#define SET_FLAG_OFF(v, flag) v &= ~(flag)

@interface SecMockAKS ()
@property (class, readonly) NSMutableDictionary<NSNumber*, NSNumber*>* lockedStates;
@property (class, readonly) dispatch_queue_t mutabilityQueue;
@end

@implementation SecMockAKS
static NSMutableDictionary* _lockedStates = nil;
static dispatch_queue_t _mutabilityQueue = nil;
static keybag_state_t _keybag_state = keybag_state_unlocked | keybag_state_been_unlocked;
static NSMutableArray<NSError *>* _decryptRefKeyErrors = nil;
/*
 * Method that limit where this rather in-secure version of AKS can run
 */

+ (void)trapdoor {
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
#if DEBUG || TARGET_OS_SIMULATOR
        return;
#else
        const char *argv0 = getprogname();

        if (strcmp(argv0, "securityd") == 0 || strcmp(argv0, "secd") == 0) {
            abort();
        }
        if (os_variant_has_internal_content("securityd")) {
            return;
        }
#if TARGET_OS_IPHONE /* all three platforms: ios, watch, tvos */
        if (os_variant_uses_ephemeral_storage("securityd")) {
            return;
        }
        if (os_variant_allows_internal_security_policies("securityd")) {
            return;
        }
        abort();
#endif /* TARGET_OS_IPHONE */
#endif /* !DEBUG || !TARGET_OS_SIMULATOR */
    });
}

+ (NSMutableDictionary*)lockedStates
{
    if(_lockedStates == nil) {
        _lockedStates = [NSMutableDictionary dictionary];
    }
    return _lockedStates;
}

+ (dispatch_queue_t)mutabilityQueue
{
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        _mutabilityQueue = dispatch_queue_create("SecMockAKS", DISPATCH_QUEUE_SERIAL);
    });
    return _mutabilityQueue;
}

+ (keybag_state_t)keybag_state
{
    return _keybag_state;
}

+ (void)setKeybag_state:(keybag_state_t)keybag_state
{
    _keybag_state = keybag_state;
}

+ (void)reset
{
    dispatch_sync(self.mutabilityQueue, ^{
        [self.lockedStates removeAllObjects];
        self.keybag_state = keybag_state_unlocked;
    });
}

+ (bool)isLocked:(keyclass_t)key_class
{
    __block bool isLocked = false;
    dispatch_sync(self.mutabilityQueue, ^{
        NSNumber* key = [NSNumber numberWithInt:key_class];
        isLocked = [self.lockedStates[key] boolValue];
    });
    return isLocked;
}

+ (bool)isSEPDown 
{
    return false;
}

+ (bool)useGenerationCount
{
    return false;
}

+ (void)lockClassA
{
    dispatch_sync(self.mutabilityQueue, ^{
        self.lockedStates[[NSNumber numberWithInt:key_class_a]] = [NSNumber numberWithBool:YES];
        self.lockedStates[[NSNumber numberWithInt:key_class_ak]] = [NSNumber numberWithBool:YES];
        self.lockedStates[[NSNumber numberWithInt:key_class_aku]] = [NSNumber numberWithBool:YES];
        self.lockedStates[[NSNumber numberWithInt:key_class_akpu]] = [NSNumber numberWithBool:YES];

        SET_FLAG_ON(self.keybag_state, keybag_state_locked);
        SET_FLAG_OFF(self.keybag_state, keybag_state_unlocked);
        // don't touch keybag_state_been_unlocked; leave it as-is
    });
}

// Simulate device being in "before first unlock"
+ (void)lockClassA_C
{
    dispatch_sync(self.mutabilityQueue, ^{
        self.lockedStates[[NSNumber numberWithInt:key_class_a]] = [NSNumber numberWithBool:YES];
        self.lockedStates[[NSNumber numberWithInt:key_class_ak]] = [NSNumber numberWithBool:YES];
        self.lockedStates[[NSNumber numberWithInt:key_class_aku]] = [NSNumber numberWithBool:YES];
        self.lockedStates[[NSNumber numberWithInt:key_class_akpu]] = [NSNumber numberWithBool:YES];
        self.lockedStates[[NSNumber numberWithInt:key_class_c]] = [NSNumber numberWithBool:YES];
        self.lockedStates[[NSNumber numberWithInt:key_class_ck]] = [NSNumber numberWithBool:YES];
        self.lockedStates[[NSNumber numberWithInt:key_class_cku]] = [NSNumber numberWithBool:YES];

        SET_FLAG_ON(self.keybag_state, keybag_state_locked);
        SET_FLAG_OFF(self.keybag_state, keybag_state_unlocked);
        SET_FLAG_OFF(self.keybag_state, keybag_state_been_unlocked);
    });
}

+ (void)unlockAllClasses
{
    dispatch_sync(self.mutabilityQueue, ^{
        [self.lockedStates removeAllObjects];

        SET_FLAG_OFF(self.keybag_state, keybag_state_locked);
        SET_FLAG_ON(self.keybag_state, keybag_state_unlocked);
        SET_FLAG_ON(self.keybag_state, keybag_state_been_unlocked);
    });
}

+ (void)failNextDecryptRefKey: (NSError* _Nonnull) decryptRefKeyError {
    if (_decryptRefKeyErrors == NULL) {
        _decryptRefKeyErrors = [NSMutableArray array];
    }
    @synchronized(_decryptRefKeyErrors) {
        [_decryptRefKeyErrors addObject: decryptRefKeyError];
    }
}

+ (NSError * _Nullable)popDecryptRefKeyFailure {
    NSError* error = nil;
    if (_decryptRefKeyErrors == NULL) {
        return nil;
    }
    @synchronized(_decryptRefKeyErrors) {
        if(_decryptRefKeyErrors.count > 0) {
            error = _decryptRefKeyErrors[0];
            [_decryptRefKeyErrors removeObjectAtIndex:0];
        }
    }
    return error;
}


@end

kern_return_t
aks_load_bag(const void * data, int length, keybag_handle_t* handle)
{
    *handle = 17;
    return 0;
}

kern_return_t
aks_create_bag(const void * passcode, int length, keybag_type_t type, keybag_handle_t* handle)
{
    [SecMockAKS unlockAllClasses];
    *handle = 17;
    return 0;
}

kern_return_t
aks_unload_bag(keybag_handle_t handle)
{
    return kAKSReturnSuccess;
}

kern_return_t
aks_save_bag(keybag_handle_t handle, void ** data, int * length)
{
    assert(handle != bad_keybag_handle);
    assert(data);
    assert(length);

    *data = calloc(1, 12);
    memcpy(*data, "keybag dump", 12);
    *length = 12;

    return kAKSReturnSuccess;
}

kern_return_t
aks_get_system(keybag_handle_t special_handle, keybag_handle_t *handle)
{
    *handle = 17;
    return kAKSReturnSuccess;
}

kern_return_t
aks_get_bag_uuid(keybag_handle_t handle, uuid_t uuid)
{
    memcpy(uuid, "0123456789abcdf", sizeof(uuid_t));
    return kAKSReturnSuccess;
}

kern_return_t aks_lock_bag(keybag_handle_t handle)
{
    if (handle == KEYBAG_DEVICE) {
        [SecMockAKS lockClassA];
    }
    return kAKSReturnSuccess;
}

kern_return_t aks_unlock_bag(keybag_handle_t handle, const void *passcode, int length)
{
    if (handle == KEYBAG_DEVICE) {
        [SecMockAKS unlockAllClasses];
    }
    return kAKSReturnSuccess;
}

kern_return_t aks_get_lock_state(keybag_handle_t handle, keybag_state_t *state)
{
    *state = SecMockAKS.keybag_state;
    return kAKSReturnSuccess;
}

//static uint8_t staticAKSKey[32] = "1234567890123456789012";

#define PADDINGSIZE 8

kern_return_t
aks_wrap_key(const void * key, int key_size, keyclass_t key_class, keybag_handle_t handle, void * wrapped_key, int * wrapped_key_size_inout, keyclass_t * class_out)
{
    [SecMockAKS trapdoor];

    if ([SecMockAKS isSEPDown]) {
        return kAKSReturnBusy;
    }

    // Assumes non-device keybags are asym
    if ([SecMockAKS isLocked:key_class] && handle == KEYBAG_DEVICE) {
        return kAKSReturnNoPermission;
    }

    if (class_out) {    // Not a required parameter
        *class_out = key_class;
        if ([SecMockAKS useGenerationCount]) {
            *class_out |= (key_class_last + 1);
        }
    }

    if (key_size > APPLE_KEYSTORE_MAX_KEY_LEN) {
        abort();
    }
    if (handle != KEYBAG_DEVICE) {      // For now assumes non-device bags are asym
        if (APPLE_KEYSTORE_MAX_ASYM_WRAPPED_KEY_LEN > *wrapped_key_size_inout) {
            abort();
        }
    } else {
        if (APPLE_KEYSTORE_MAX_SYM_WRAPPED_KEY_LEN > *wrapped_key_size_inout) {
            abort();
        }
    }

    *wrapped_key_size_inout = key_size + PADDINGSIZE;
    memcpy(wrapped_key, key, key_size);
    memset(((uint8_t *)wrapped_key) + key_size, 0xff, PADDINGSIZE);
    return kAKSReturnSuccess;
}

kern_return_t
aks_unwrap_key(const void * wrapped_key, int wrapped_key_size, keyclass_t key_class, keybag_handle_t handle, void * key, int * key_size_inout)
{
    [SecMockAKS trapdoor];

    if ([SecMockAKS isSEPDown]) {
        return kAKSReturnBusy;
    }

    if ([SecMockAKS isLocked:key_class]) {
        return kAKSReturnNoPermission;
    }

    if (wrapped_key_size < PADDINGSIZE) {
        abort();
    }
    static const char expected_padding[PADDINGSIZE] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
    if (memcmp(((const uint8_t *)wrapped_key) + (wrapped_key_size - PADDINGSIZE), expected_padding, PADDINGSIZE) != 0) {
        return kAKSReturnDecodeError;
    }
    if (*key_size_inout < wrapped_key_size - PADDINGSIZE) {
        abort();
    }
    *key_size_inout = wrapped_key_size - PADDINGSIZE;
    memcpy(key, wrapped_key, *key_size_inout);

    return kAKSReturnSuccess;
}

@interface MockAKSRefKeyObject: NSObject
@property NSData *keyData;
@property SFAESKey *key;
@property NSData *acmHandle;
@property NSData *externalData;

@property NSData *blob; // blob is exteralized format

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithKeyData:(NSData *)keyData parameters:(NSData *)parameters error:(NSError **)error;

@end
@implementation MockAKSRefKeyObject

- (instancetype)initWithKeyData:(NSData *)keyData
                     parameters:(NSData *)parameters
                          error:(NSError **)error
{
    if ((self = [super init]) != NULL) {

        self.keyData = [keyData copy];
        SFAESKeySpecifier* keySpecifier = [[SFAESKeySpecifier alloc] initWithBitSize: SFAESKeyBitSize256];
        @try {
            self.key = [[SFAESKey alloc] initWithData:self.keyData specifier:keySpecifier error:error];
            if (self.key == NULL) {
                return NULL;
            }
        } @catch (NSException *exception) {
            *error = [NSError errorWithDomain:@"foo" code:kAKSReturnBadArgument userInfo:nil];
            return NULL;
        }

        MockAKSOptionalParameters *params = [[MockAKSOptionalParameters alloc] initWithData:parameters];

        // enforce that the extra data is DER like
        if (params.externalData) {
            CFTypeRef cf = NULL;
            CFErrorRef cferror = NULL;
            uint8_t *der = (uint8_t *)params.externalData.bytes;
            der_decode_plist(NULL, false, &cf, &cferror, der, der + params.externalData.length);
            if (cf == NULL) {
                *error = [NSError errorWithDomain:@"foo" code:kAKSReturnBadArgument userInfo:nil];
                return NULL;
            }
            CFReleaseNull(cf);
        }


        self.externalData = params.externalData;
        self.acmHandle = params.acmHandle;

        MockAKSRefKey *blob = [[MockAKSRefKey alloc] init];
        blob.key = self.keyData;
        blob.optionalParams = parameters;
        self.blob = blob.data;
    }
    return self;
}

@end

int
aks_ref_key_create(keybag_handle_t handle, keyclass_t key_class, aks_key_type_t type, const uint8_t *params, size_t params_len, aks_ref_key_t *ot)
{
    MockAKSRefKeyObject *key;
    @autoreleasepool {
        [SecMockAKS trapdoor];
        NSError *error = NULL;

        if ([SecMockAKS isLocked:key_class]) {
            return kAKSReturnNoPermission;
        }

        NSData *keyData = [NSData dataWithBytes:"1234567890123456789012345678901" length:32];
        NSData *parameters = NULL;
        if (params && params_len != 0) {
            parameters = [NSData dataWithBytes:params length:params_len];
        }

        key = [[MockAKSRefKeyObject alloc] initWithKeyData:keyData parameters:parameters error:&error];
        if(key == NULL) {
            if (error) {
                return (int)error.code;
            }
            return kAKSReturnError;
        }
    }

    *ot = (__bridge_retained aks_ref_key_t)key;
    return kAKSReturnSuccess;
}

int
aks_ref_key_encrypt(aks_ref_key_t handle,
                    const uint8_t *der_params, size_t der_params_len,
                    const void * data, size_t data_len,
                    void ** out_der, size_t * out_der_len)
{
    [SecMockAKS trapdoor];

    // No current error injection
    NSError* error = nil;
    NSData* nsdata = [NSData dataWithBytes:data length:data_len];

    MockAKSRefKeyObject *key = (__bridge MockAKSRefKeyObject*)handle;

    SFAuthenticatedEncryptionOperation* op = [[SFAuthenticatedEncryptionOperation alloc] initWithKeySpecifier:key.key.keySpecifier authenticationMode:SFAuthenticatedEncryptionModeGCM];


    SFAuthenticatedCiphertext* ciphertext = [op encrypt:nsdata withKey:key.key error:&error];

    if(error || !ciphertext || !out_der || !out_der_len) {
        return kAKSReturnError;
    }

    NSData* cipherBytes = [NSKeyedArchiver archivedDataWithRootObject:ciphertext requiringSecureCoding:YES error:&error];
    if(error || !cipherBytes) {
        return kAKSReturnError;
    }

    *out_der = calloc(1, cipherBytes.length);

    memcpy(*out_der, cipherBytes.bytes, cipherBytes.length);
    *out_der_len = cipherBytes.length;

    return kAKSReturnSuccess;
}

int
aks_ref_key_decrypt(aks_ref_key_t handle,
                    const uint8_t *der_params, size_t der_params_len,
                    const void * data, size_t data_len,
                    void ** out_der, size_t * out_der_len)
{
    @autoreleasepool {
        [SecMockAKS trapdoor];

        NSError *error = [SecMockAKS popDecryptRefKeyFailure];
        if (error) {
            return (int)error.code;
        }

        if (!out_der || !out_der_len || !data) {
            return kAKSReturnError;
        }

        if (der_params) {
            NSData *paramsData = [NSData dataWithBytes:der_params length:der_params_len];
            MockAKSOptionalParameters *params = [[MockAKSOptionalParameters alloc] initWithData:paramsData];

            if (params.hasExternalData && !params.hasAcmHandle) {
                return kSKSReturnPolicyInvalid;
            }
            (void)params; /* check ACM context if the item uses policy */
        }


        NSData* nsdata = [NSData dataWithBytes:data length:data_len];

        MockAKSRefKeyObject* key =  (__bridge MockAKSRefKeyObject*)handle;

        SFAuthenticatedCiphertext* ciphertext = [NSKeyedUnarchiver unarchivedObjectOfClass:[SFAuthenticatedCiphertext class] fromData:nsdata error:&error];

        if(error || !ciphertext) {
            return kAKSReturnDecodeError;
        }

        // this should not be needed...
        if (![ciphertext isKindOfClass:[SFAuthenticatedCiphertext class]]) {
            return kAKSReturnDecodeError;
        }
        if (![ciphertext.ciphertext isKindOfClass:[NSData class]]) {
            return kAKSReturnDecodeError;
        }
        if (![ciphertext.authenticationCode isKindOfClass:[NSData class]]) {
            return kAKSReturnDecodeError;
        }
        if (![ciphertext.initializationVector isKindOfClass:[NSData class]]) {
            return kAKSReturnDecodeError;
        }

        NSData* plaintext = NULL;

        SFAuthenticatedEncryptionOperation* op = [[SFAuthenticatedEncryptionOperation alloc] initWithKeySpecifier:key.key.keySpecifier authenticationMode:SFAuthenticatedEncryptionModeGCM];
        plaintext = [op decrypt:ciphertext withKey:key.key error:&error];

        if(error || !plaintext) {
            return kAKSReturnDecodeError;
        }

        /*
         * AAAAAAAAHHHHHHHHH
         * The output of aks_ref_key_encrypt is not the decrypted data, it's a DER blob that contains an octet string of the data....
         */

        CFErrorRef cfError = NULL;
        NSData* derData = (__bridge_transfer NSData*)CFPropertyListCreateDERData(NULL, (__bridge CFTypeRef)plaintext, &cfError);
        CFReleaseNull(cfError);
        if (derData == NULL) {
            return kAKSReturnDecodeError;
        }

        *out_der = calloc(1, derData.length);
        if (*out_der == NULL) {
            abort();
        }

        memcpy(*out_der, derData.bytes, derData.length);
        *out_der_len = derData.length;

        return kAKSReturnSuccess;
    }
}

int aks_ref_key_wrap(aks_ref_key_t handle,
                     uint8_t *der_params, size_t der_params_len,
                     const uint8_t *key, size_t key_len,
                     void **out_der, size_t *out_der_len)
{
    [SecMockAKS trapdoor];

    return aks_ref_key_encrypt(handle, der_params, der_params_len, key, key_len, out_der, out_der_len);
}

int aks_ref_key_unwrap(aks_ref_key_t handle,
                       uint8_t *der_params, size_t der_params_len,
                       const uint8_t *wrapped, size_t wrapped_len,
                       void **out_der, size_t *out_der_len)
{
    [SecMockAKS trapdoor];

    return aks_ref_key_decrypt(handle, der_params, der_params_len, wrapped, wrapped_len, out_der, out_der_len);
}


int
aks_ref_key_delete(aks_ref_key_t handle, const uint8_t *der_params, size_t der_params_len)
{
    return kAKSReturnSuccess;
}

int
aks_operation_optional_params(const uint8_t * access_groups, size_t access_groups_len, const uint8_t * external_data, size_t external_data_len, const void * acm_handle, int acm_handle_len, void ** out_der, size_t * out_der_len)
{
    @autoreleasepool {
        [SecMockAKS trapdoor];

        MockAKSOptionalParameters* params = [[MockAKSOptionalParameters alloc] init];

        if (access_groups) {
            params.accessGroups = [NSData dataWithBytes:access_groups length:access_groups_len];
        }
        if (external_data) {
            params.externalData = [NSData dataWithBytes:external_data length:external_data_len];
        }
        if (acm_handle) {
            params.acmHandle = [NSData dataWithBytes:acm_handle length:acm_handle_len];
        }

        NSData *result = params.data;
        *out_der = malloc(result.length);
        memcpy(*out_der, result.bytes, result.length);
        *out_der_len = result.length;
        return kAKSReturnSuccess;
    }
}

int aks_ref_key_create_with_blob(keybag_handle_t keybag, const uint8_t *ref_key_blob, size_t ref_key_blob_len, aks_ref_key_t* handle)
{
    NSError *error = NULL;
    MockAKSRefKeyObject *key = NULL;

    @autoreleasepool {
        [SecMockAKS trapdoor];
        NSData *data = [NSData dataWithBytes:ref_key_blob length:ref_key_blob_len];
        
        MockAKSRefKey *blob = [[MockAKSRefKey alloc] initWithData:data];
        if (blob == NULL) {
            return kAKSReturnBadData;
        }
        if (!blob.hasKey || blob.key.length == 0) {
            return kAKSReturnBadData;
        }

        key = [[MockAKSRefKeyObject alloc] initWithKeyData:blob.key parameters:blob.optionalParams error:&error];
    }
    if (key == NULL) {
        *handle = (aks_ref_key_t)-1;
        if (error.code) {
            return (int)error.code;
        }
        return kAKSReturnError;
    }

    *handle = (__bridge_retained aks_ref_key_t)key;
    return kAKSReturnSuccess;
}

const uint8_t * aks_ref_key_get_blob(aks_ref_key_t refkey, size_t *out_blob_len)
{
    MockAKSRefKeyObject *key = (__bridge MockAKSRefKeyObject*)refkey;

    *out_blob_len = key.blob.length;
    return (const uint8_t *)key.blob.bytes;
}

int
aks_ref_key_free(aks_ref_key_t* refkey)
{
    if (*refkey != NULL) {
        MockAKSRefKeyObject *key = (__bridge MockAKSRefKeyObject*)*refkey;
        CFTypeRef cfkey = CFBridgingRetain(key);
        CFRelease(cfkey);
        *refkey = NULL;
    }
    return kAKSReturnSuccess;
}

const uint8_t *
aks_ref_key_get_external_data(aks_ref_key_t refkey, size_t *out_external_data_len)
{
    MockAKSRefKeyObject *key = (__bridge MockAKSRefKeyObject*)refkey;

    *out_external_data_len = key.externalData.length;
    return (const uint8_t *)key.externalData.bytes;
}

kern_return_t
aks_assert_hold(keybag_handle_t handle, uint32_t type, uint64_t timeout)
{
    if ([SecMockAKS isLocked:key_class_ak]) {
        return kAKSReturnNoPermission;
    }
    return kAKSReturnSuccess;
}

kern_return_t
aks_assert_drop(keybag_handle_t handle, uint32_t type)
{
    return kAKSReturnSuccess;
}

kern_return_t
aks_generation(keybag_handle_t handle,
               generation_option_t generation_option,
               uint32_t * current_generation)
{
    *current_generation = 0;
    return kAKSReturnSuccess;
}

CFStringRef kMKBDeviceModeMultiUser = CFSTR("kMKBDeviceModeMultiUser");
CFStringRef kMKBDeviceModeSingleUser = CFSTR("kMKBDeviceModeSingleUser");
CFStringRef kMKBDeviceModeKey = CFSTR("kMKBDeviceModeKey");

static CFStringRef staticKeybagHandle = CFSTR("keybagHandle");

int
MKBKeyBagCreateWithData(CFDataRef keybagBlob, MKBKeyBagHandleRef* newHandle)
{
    *newHandle = (MKBKeyBagHandleRef)staticKeybagHandle;
    return kMobileKeyBagSuccess;
}

int
MKBKeyBagUnlock(MKBKeyBagHandleRef keybag, CFDataRef passcode)
{
    if (keybag == NULL || !CFEqual(keybag, staticKeybagHandle)) {
        abort();
    }
    return kMobileKeyBagSuccess;
}

int MKBKeyBagGetAKSHandle(MKBKeyBagHandleRef keybag, int32_t *handle)
{
    if (keybag == NULL || !CFEqual(keybag, staticKeybagHandle)) {
        abort();
    }
    *handle = 17;
    return kMobileKeyBagSuccess;
}

int MKBGetDeviceLockState(CFDictionaryRef options)
{
    if ([SecMockAKS isLocked:key_class_ak]) {
        return kMobileKeyBagDeviceIsLocked;
    }
    return kMobileKeyBagDeviceIsUnlocked;
}

CF_RETURNS_RETAINED CFDictionaryRef
MKBUserTypeDeviceMode(CFDictionaryRef options, CFErrorRef * error)
{
    return CFBridgingRetain(@{
        (__bridge NSString *)kMKBDeviceModeKey : (__bridge NSString *)kMKBDeviceModeSingleUser,
    });
}

int MKBForegroundUserSessionID( CFErrorRef * error)
{
    return kMobileKeyBagSuccess;
}

const CFTypeRef kAKSKeyAcl = (CFTypeRef)CFSTR("kAKSKeyAcl");
const CFTypeRef kAKSKeyAclParamRequirePasscode = (CFTypeRef)CFSTR("kAKSKeyAclParamRequirePasscode");

const CFTypeRef kAKSKeyOpDefaultAcl = (CFTypeRef)CFSTR("kAKSKeyOpDefaultAcl");
const CFTypeRef kAKSKeyOpEncrypt = (CFTypeRef)CFSTR("kAKSKeyOpEncrypt");
const CFTypeRef kAKSKeyOpDecrypt = (CFTypeRef)CFSTR("kAKSKeyOpDecrypt");
const CFTypeRef kAKSKeyOpSync = (CFTypeRef)CFSTR("kAKSKeyOpSync");
const CFTypeRef kAKSKeyOpDelete = (CFTypeRef)CFSTR("kAKSKeyOpDelete");
const CFTypeRef kAKSKeyOpCreate = (CFTypeRef)CFSTR("kAKSKeyOpCreate");
const CFTypeRef kAKSKeyOpSign = (CFTypeRef)CFSTR("kAKSKeyOpSign");
const CFTypeRef kAKSKeyOpSetKeyClass = (CFTypeRef)CFSTR("kAKSKeyOpSetKeyClass");
const CFTypeRef kAKSKeyOpWrap = (CFTypeRef)CFSTR("kAKSKeyOpWrap");
const CFTypeRef kAKSKeyOpUnwrap = (CFTypeRef)CFSTR("kAKSKeyOpUnwrap");
const CFTypeRef kAKSKeyOpComputeKey = (CFTypeRef)CFSTR("kAKSKeyOpComputeKey");
const CFTypeRef kAKSKeyOpAttest = (CFTypeRef)CFSTR("kAKSKeyOpAttest");
const CFTypeRef kAKSKeyOpTranscrypt = (CFTypeRef)CFSTR("kAKSKeyOpTranscrypt");
const CFTypeRef kAKSKeyOpECIESEncrypt = (CFTypeRef)CFSTR("kAKSKeyOpECIESEncrypt");
const CFTypeRef kAKSKeyOpECIESDecrypt = (CFTypeRef)CFSTR("kAKSKeyOpECIESDecrypt");
const CFTypeRef kAKSKeyOpECIESTranscode = (CFTypeRef)CFSTR("kAKSKeyOpECIESTranscode");


TKTokenRef TKTokenCreate(CFDictionaryRef attributes, CFErrorRef *error)
{
    return NULL;
}

CFTypeRef TKTokenCopyObjectData(TKTokenRef token, CFDataRef objectID, CFErrorRef *error)
{
    return NULL;
}

CFDataRef TKTokenCreateOrUpdateObject(TKTokenRef token, CFDataRef objectID, CFMutableDictionaryRef attributes, CFErrorRef *error)
{
    return NULL;
}

CFDataRef TKTokenCopyObjectAccessControl(TKTokenRef token, CFDataRef objectID, CFErrorRef *error)
{
    return NULL;
}
bool TKTokenDeleteObject(TKTokenRef token, CFDataRef objectID, CFErrorRef *error)
{
    return false;
}

CFDataRef TKTokenCopyPublicKeyData(TKTokenRef token, CFDataRef objectID, CFErrorRef *error)
{
    return NULL;
}

CFTypeRef TKTokenCopyOperationResult(TKTokenRef token, CFDataRef objectID, CFIndex secKeyOperationType, CFArrayRef algorithm,
                                     CFIndex secKeyOperationMode, CFTypeRef in1, CFTypeRef in2, CFErrorRef *error)
{
    return NULL;
}

CF_RETURNS_RETAINED CFDictionaryRef TKTokenControl(TKTokenRef token, CFDictionaryRef attributes, CFErrorRef *error)
{
    return NULL;
}

CFTypeRef LACreateNewContextWithACMContext(CFDataRef acmContext, CFErrorRef *error)
{
    return NULL;
}

CFDataRef LACopyACMContext(CFTypeRef context, CFErrorRef *error)
{
    return NULL;
}

bool LAEvaluateAndUpdateACL(CFTypeRef context, CFDataRef acl, CFTypeRef operation, CFDictionaryRef hints, CFDataRef *updatedACL, CFErrorRef *error)
{
    return false;
}

ACMContextRef
ACMContextCreateWithExternalForm(const void *externalForm, size_t dataLength)
{
    return NULL;
}

ACMStatus
ACMContextDelete(ACMContextRef context, bool destroyContext)
{
    return kACMErrorSuccess;
}

ACMStatus
ACMContextRemovePassphraseCredentialsByPurposeAndScope(const ACMContextRef context, ACMPassphrasePurpose purpose, ACMScope scope)
{
    return kACMErrorSuccess;
}

#endif // TARGET_OS_BRIDGE
