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

#import "SecKeybagSupport.h"

#if USE_KEYSTORE
#import <libaks.h>
#import <libaks_ref_key.h>
#import <MobileKeyBag/MobileKeyBag.h>
#endif

#import <CryptoTokenKit/CryptoTokenKit.h>
#import <ctkclient.h>
#import <coreauthd_spi.h>
#import <ACMLib.h>
#import <LocalAuthentication/LAPublicDefines.h>
#import <LocalAuthentication/LAPrivateDefines.h>
#import <LocalAuthentication/LACFSupport.h>
#import <SecurityFoundation/SFEncryptionOperation.h>
#import "mockaks.h"

#if USE_KEYSTORE

@implementation SecMockAKS

+ (bool)isLocked:(keyclass_t)key_class
{
    return false;
}

+ (bool)isSEPDown 
{
    return false;
}

+ (bool)useGenerationCount
{
    return false;
}

@end

kern_return_t
aks_load_bag(const void * data, int length, keybag_handle_t* handle)
{
    *handle = 17;
    return 0;
}

kern_return_t aks_unlock_bag(keybag_handle_t handle, const void * passcode, int length)
{
    return 0;
}

kern_return_t
aks_create_bag(const void * passcode, int length, keybag_type_t type, keybag_handle_t* handle)
{
    *handle = 17;
    return 0;
}

kern_return_t
aks_unload_bag(keybag_handle_t handle)
{
    return -1;
}

kern_return_t
aks_save_bag(keybag_handle_t handle, void ** data, int * length)
{
    return 0;
}

kern_return_t
aks_get_system(keybag_handle_t special_handle, keybag_handle_t *handle)
{
    *handle = 17;
    return 0;
}

//static uint8_t staticAKSKey[32] = "1234567890123456789012";

#define PADDINGSIZE 8

kern_return_t
aks_wrap_key(const void * key, int key_size, keyclass_t key_class, keybag_handle_t handle, void * wrapped_key, int * wrapped_key_size_inout, keyclass_t * class_out)
{
    if ([SecMockAKS isLocked:key_class]) {
        return kIOReturnNotPermitted;
    }
    if ([SecMockAKS isSEPDown]) {
        return kIOReturnBusy;
    }

    *class_out = key_class;
    if ([SecMockAKS useGenerationCount]) {
        *class_out |= (key_class_last + 1);
    }

    if (key_size + PADDINGSIZE > *wrapped_key_size_inout) {
        abort();
    }
    *wrapped_key_size_inout = key_size + PADDINGSIZE;
    memcpy(wrapped_key, key, key_size);
    memset(((uint8_t *)wrapped_key) + key_size, 0xff, PADDINGSIZE);
    return 0;
}

kern_return_t
aks_unwrap_key(const void * wrapped_key, int wrapped_key_size, keyclass_t key_class, keybag_handle_t handle, void * key, int * key_size_inout)
{
    if ([SecMockAKS isLocked:key_class]) {
        return kIOReturnNotPermitted;
    }
    if ([SecMockAKS isSEPDown]) {
        return kIOReturnBusy;
    }

    if (wrapped_key_size < PADDINGSIZE) {
        abort();
    }
    static const char expected_padding[PADDINGSIZE] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
    if (memcmp(((const uint8_t *)wrapped_key) + (wrapped_key_size - PADDINGSIZE), expected_padding, PADDINGSIZE) != 0) {
        abort();
    }
    if (*key_size_inout < wrapped_key_size - PADDINGSIZE) {
        abort();
    }
    *key_size_inout = wrapped_key_size - PADDINGSIZE;
    memcpy(key, wrapped_key, *key_size_inout);

    return 0;
}

int
aks_ref_key_create(keybag_handle_t handle, keyclass_t cls, aks_key_type_t type, const uint8_t *params, size_t params_len, aks_ref_key_t *ot)
{
    SFAESKeySpecifier* keySpecifier = [[SFAESKeySpecifier alloc] initWithBitSize:SFAESKeyBitSize256];
    SFAESKey* key = [[SFAESKey alloc] initRandomKeyWithSpecifier:keySpecifier error:nil];
    *ot = (__bridge_retained aks_ref_key_t)key;
    return kAKSReturnSuccess;
}

int
aks_ref_key_encrypt(aks_ref_key_t handle,
                    const uint8_t *der_params, size_t der_params_len,
                    const void * data, size_t data_len,
                    void ** out_der, size_t * out_der_len)
{
    return -1;
}

int
aks_ref_key_decrypt(aks_ref_key_t handle,
                        const uint8_t *der_params, size_t der_params_len,
                        const void * data, size_t data_len,
                        void ** out_der, size_t * out_der_len)
{
    return -1;
}

int
aks_ref_key_delete(aks_ref_key_t handle, const uint8_t *der_params, size_t der_params_len)
{
    return -1;
}

int
aks_operation_optional_params(const uint8_t * access_groups, size_t access_groups_len, const uint8_t * external_data, size_t external_data_len, const void * acm_handle, int acm_handle_len, void ** out_der, size_t * out_der_len)
{
    return -1;
}

int aks_ref_key_create_with_blob(keybag_handle_t keybag, const uint8_t *ref_key_blob, size_t ref_key_blob_len, aks_ref_key_t* handle)
{
    aks_ref_key_create(keybag, 0, 0, NULL, 0, handle);
    return 0;
}

const uint8_t * aks_ref_key_get_blob(aks_ref_key_t refkey, size_t *out_blob_len)
{
    *out_blob_len = 2;
    return (const uint8_t *)"20";
}
int
aks_ref_key_free(aks_ref_key_t* key)
{
    return 0;
}

const uint8_t *
aks_ref_key_get_external_data(aks_ref_key_t refkey, size_t *out_external_data_len)
{
    *out_external_data_len = 2;
    return (const uint8_t *)"21";
}


kern_return_t
aks_assert_hold(keybag_handle_t handle, uint32_t type, uint64_t timeout)
{
    return 0;
}

kern_return_t
aks_assert_drop(keybag_handle_t handle, uint32_t type)
{
    return 0;
}

kern_return_t
aks_generation(keybag_handle_t handle,
               generation_option_t generation_option,
               uint32_t * current_generation)
{
    *current_generation = 0;
    return 0;
}

kern_return_t
aks_get_bag_uuid(keybag_handle_t handle, uuid_t uuid)
{
    memcpy(uuid, "0123456789abcdf", sizeof(uuid_t));
    return 0;
}

kern_return_t
aks_get_lock_state(keybag_handle_t handle, keybag_state_t *state)
{
    *state = keybag_state_been_unlocked;
    return 0;
}

CFStringRef kMKBDeviceModeMultiUser = CFSTR("kMKBDeviceModeMultiUser");
CFStringRef kMKBDeviceModeSingleUser = CFSTR("kMKBDeviceModeSingleUser");
CFStringRef kMKBDeviceModeKey = CFSTR("kMKBDeviceModeKey");

static CFStringRef staticKeybagHandle = CFSTR("keybagHandle");

int
MKBKeyBagCreateWithData(CFDataRef keybagBlob, MKBKeyBagHandleRef* newHandle)
{
    *newHandle = (MKBKeyBagHandleRef)staticKeybagHandle;
    return 0;
}

int
MKBKeyBagUnlock(MKBKeyBagHandleRef keybag, CFDataRef passcode)
{
    if (keybag == NULL || !CFEqual(keybag, staticKeybagHandle)) {
        abort();
    }
    return 0;
}

int MKBKeyBagGetAKSHandle(MKBKeyBagHandleRef keybag, int32_t *handle)
{
    if (keybag == NULL || !CFEqual(keybag, staticKeybagHandle)) {
        abort();
    }
    *handle = 17;
    return 0;
}

int MKBGetDeviceLockState(CFDictionaryRef options)
{
    if ([SecMockAKS isLocked:key_class_ak] )
        return kMobileKeyBagDeviceIsLocked;
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
    return 0;
}

#endif /* USE_KEYSTORE */

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
    return 0;
}

ACMStatus
ACMContextRemovePassphraseCredentialsByPurposeAndScope(const ACMContextRef context, ACMPassphrasePurpose purpose, ACMScope scope)
{
    return 0;
}

