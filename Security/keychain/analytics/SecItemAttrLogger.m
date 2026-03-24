/*
 * Copyright (c) 2025 Apple Inc. All Rights Reserved.
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

#include "SecItemAttrLogger.h"
#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>
#include <Security/SecKeyPriv.h>
#include <Security/SecPasswordGenerate.h>
#include <Security/SecSharedCredential.h>
#include <Security/SecTask.h>
#include "ipc/server_entitlement_helpers.h"
#include <utilities/debugging.h>
#include <TargetConditionals.h>
#include <string.h>
#include <stdbool.h>
#include <mach/mach_time.h>
#import "utilities/SecCoreAnalytics.h"
#import "keychain/categories/NSError+UsefulConstructors.h"

// Encoding version constant
// *NOTE*: UPDATE THIS VERSION WHEN MAKING CHANGES TO ATTRIBUTES LIST.
#define ENCODING_VERSION 1

// Special attributes
// (Both kSecAttrType and kSecAttrKeyType expands to "type") and g_key_definitions only holds onto the kSecAttrKeyType info
#define kSecAttrTypeIndex 8
#define kSecAttrKeyTypeIndex 112

// Constants for valueIndex special values
#define KEY_NOT_TRACKS_VALUE -1     // Key doesn't track values (e.g., kSecAttrAccount)
#define KEY_INVALID_VALUE -2        // Invalid/unrecognized value for value-tracking key
#define KEY_INVALID_VALUE_TYPE -3   // Wrong type for value-tracking key

// Key definition structure - defines a SecItem attribute key
typedef struct {
    const CFStringRef *key;         // Pointer to the CFStringRef constant
    int key_index;                  // Index of this key in the array
    int value_count;                // Number of possible values (0 = doesn't track values)
} KeyDef;

// Value definition structure - defines a possible value for a key
typedef struct {
    const CFStringRef *value;       // Pointer to the value CFStringRef constant
    int value_index;                // Index within the parent key's value range (0-based)
    int parent_key_index;           // Index of the parent key
} ValueDef;

// Macros for defining keys and values
#define KEY_DEF(attr, idx, count) {&attr, idx, count}
#define VALUE_DEF(attr, val_idx, parent_idx) {&attr, val_idx, parent_idx}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

// ============================================================================
// ALL SECITEM ATTRIBUTE KEYS
// ============================================================================
static KeyDef g_key_definitions[] = {
    KEY_DEF(kSecClass, 0, 6),  // Tracks 6 values
    KEY_DEF(kSecAttrAccessControl, 1, 0),
    KEY_DEF(kSecAttrAccessGroup, 2, 0),
    KEY_DEF(kSecAttrCreationDate, 3, 0),
    KEY_DEF(kSecAttrModificationDate, 4, 0),
    KEY_DEF(kSecAttrDescription, 5, 0),
    KEY_DEF(kSecAttrComment, 6, 0),
    KEY_DEF(kSecAttrCreator, 7, 0),
    KEY_DEF(kSecAttrType, kSecAttrTypeIndex, 0),
    KEY_DEF(kSecAttrLabel, 9, 0),
    KEY_DEF(kSecAttrIsInvisible, 10, 0),
    KEY_DEF(kSecAttrIsNegative, 11, 0),
    KEY_DEF(kSecAttrAccount, 12, 0),
    KEY_DEF(kSecAttrService, 13, 0),
    KEY_DEF(kSecAttrGeneric, 14, 0),
    KEY_DEF(kSecAttrSecurityDomain, 15, 0),
    KEY_DEF(kSecAttrServer, 16, 0),
    KEY_DEF(kSecAttrPort, 17, 0),
    KEY_DEF(kSecAttrPath, 18, 0),
    KEY_DEF(kSecAttrVolume, 19, 0),
    KEY_DEF(kSecAttrAddress, 20, 0),
    KEY_DEF(kSecAttrAFPServerSignature, 21, 0),
    KEY_DEF(kSecAttrAlias, 22, 0),
    KEY_DEF(kSecAttrSubject, 23, 0),
    KEY_DEF(kSecAttrIssuer, 24, 0),
    KEY_DEF(kSecAttrSerialNumber, 25, 0),
    KEY_DEF(kSecAttrSubjectKeyID, 26, 0),
    KEY_DEF(kSecAttrPublicKeyHash, 27, 0),
    KEY_DEF(kSecAttrCertificateType, 28, 0),
    KEY_DEF(kSecAttrCertificateEncoding, 29, 0),
    KEY_DEF(kSecAttrApplicationLabel, 30, 0),
    KEY_DEF(kSecAttrIsPermanent, 31, 0),
    KEY_DEF(kSecAttrIsPrivate, 32, 0),
    KEY_DEF(kSecAttrIsModifiable, 33, 0),
    KEY_DEF(kSecAttrApplicationTag, 34, 0),
    KEY_DEF(kSecAttrKeyCreator, 35, 0),
    KEY_DEF(kSecAttrEffectiveKeySize, 36, 0),
    KEY_DEF(kSecAttrStartDate, 37, 0),
    KEY_DEF(kSecAttrEndDate, 38, 0),
    KEY_DEF(kSecAttrIsSensitive, 39, 0),
    KEY_DEF(kSecAttrWasAlwaysSensitive, 40, 0),
    KEY_DEF(kSecAttrIsExtractable, 41, 0),
    KEY_DEF(kSecAttrWasNeverExtractable, 42, 0),
    KEY_DEF(kSecAttrCanEncrypt, 43, 0),
    KEY_DEF(kSecAttrCanDecrypt, 44, 0),
    KEY_DEF(kSecAttrCanDerive, 45, 0),
    KEY_DEF(kSecAttrCanSign, 46, 0),
    KEY_DEF(kSecAttrCanVerify, 47, 0),
    KEY_DEF(kSecAttrCanSignRecover, 48, 0),
    KEY_DEF(kSecAttrCanVerifyRecover, 49, 0),
    KEY_DEF(kSecAttrCanWrap, 50, 0),
    KEY_DEF(kSecAttrCanUnwrap, 51, 0),
    KEY_DEF(kSecAttrSyncViewHint, 52, 0),
    KEY_DEF(kSecAttrScriptCode, 53, 0),
    KEY_DEF(kSecAttrHasCustomIcon, 54, 0),
    KEY_DEF(kSecAttrCRLType, 55, 0),
    KEY_DEF(kSecAttrCRLEncoding, 56, 0),
    KEY_DEF(kSecAttrTombstone, 57, 0),
    KEY_DEF(kSecAttrMultiUser, 58, 0),
    KEY_DEF(kSecAttrNoLegacy, 59, 0),
    KEY_DEF(kSecAttrTokenOID, 60, 0),
    KEY_DEF(kSecAttrUUID, 61, 0),
    KEY_DEF(kSecAttrPersistantReference, 62, 0),
    KEY_DEF(kSecAttrPersistentReference, 63, 0),
    KEY_DEF(kSecAttrSysBound, 64, 0),
    KEY_DEF(kSecAttrSHA1, 65, 0),
    KEY_DEF(kSecAttrDeriveSyncIDFromItemAttributes, 66, 0),
    KEY_DEF(kSecAttrPCSPlaintextServiceIdentifier, 67, 0),
    KEY_DEF(kSecAttrPCSPlaintextPublicKey, 68, 0),
    KEY_DEF(kSecAttrPCSPlaintextPublicIdentity, 69, 0),
    KEY_DEF(kSecDataInetExtraNotes, 70, 0),
    KEY_DEF(kSecDataInetExtraHistory, 71, 0),
    KEY_DEF(kSecDataInetExtraClientDefined0, 72, 0),
    KEY_DEF(kSecDataInetExtraClientDefined1, 73, 0),
    KEY_DEF(kSecDataInetExtraClientDefined2, 74, 0),
    KEY_DEF(kSecDataInetExtraClientDefined3, 75, 0),
    KEY_DEF(kSecAttrAccessGroupToken, 76, 0),
    KEY_DEF(kSecMatchPolicy, 77, 0),
    KEY_DEF(kSecMatchItemList, 78, 0),
    KEY_DEF(kSecMatchSearchList, 79, 0),
    KEY_DEF(kSecMatchIssuers, 80, 0),
    KEY_DEF(kSecMatchEmailAddressIfPresent, 81, 0),
    KEY_DEF(kSecMatchSubjectContains, 82, 0),
    KEY_DEF(kSecMatchHostOrSubdomainOfHost, 83, 0),
    KEY_DEF(kSecMatchCaseInsensitive, 84, 0),
    KEY_DEF(kSecMatchTrustedOnly, 85, 0),
    KEY_DEF(kSecMatchValidOnDate, 86, 0),
    KEY_DEF(kSecMatchLimit, 87, 2),  // Tracks 2 values
    KEY_DEF(kSecReturnData, 88, 0),
    KEY_DEF(kSecReturnAttributes, 89, 0),
    KEY_DEF(kSecReturnRef, 90, 0),
    KEY_DEF(kSecReturnPersistentRef, 91, 0),
    KEY_DEF(kSecValueData, 92, 0),
    KEY_DEF(kSecValueRef, 93, 0),
    KEY_DEF(kSecValuePersistentRef, 94, 0),
#if !TARGET_OS_BRIDGE
    KEY_DEF(kSecUseItemList, 95, 0),
#endif
    KEY_DEF(kSecUseTombstones, 96, 0),
    KEY_DEF(kSecUseCredentialReference, 97, 0),
    KEY_DEF(kSecUseOperationPrompt, 98, 0),
    KEY_DEF(kSecUseNoAuthenticationUI, 99, 0),
    KEY_DEF(kSecUseAuthenticationContext, 100, 0),
#if TARGET_OS_IOS || TARGET_OS_OSX
    KEY_DEF(kSecUseSystemKeychainAlways, 101, 0),
#endif
    KEY_DEF(kSecUseSystemKeychain, 102, 0),
    KEY_DEF(kSecUseSyncBubbleKeychain, 103, 0),
    KEY_DEF(kSecUseCallerName, 104, 0),
    KEY_DEF(kSecUseTokenRawItems, 105, 0),
    KEY_DEF(kSecUseDataProtectionKeychain, 106, 0),
    KEY_DEF(kSecUseAuthenticationUI, 107, 3),  // Tracks 3 values
    KEY_DEF(kSecAttrAccessible, 108, 8),  // Tracks 8 values
    KEY_DEF(kSecAttrProtocol, 109, 31),  // Tracks 31 values
    KEY_DEF(kSecAttrAuthenticationType, 110, 8),  // Tracks 8 values
    KEY_DEF(kSecAttrKeyClass, 111, 3),  // Tracks 3 values
    KEY_DEF(kSecAttrKeyType, kSecAttrKeyTypeIndex, 13),  // Tracks 13 values
    KEY_DEF(kSecAttrKeySizeInBits, 113, 0),
    KEY_DEF(kSecAttrKeySizeKyber768, 114, 0),
    KEY_DEF(kSecAttrKeySizeKyber1024, 115, 0),
    KEY_DEF(kSecAttrKeySizeMLKEM768, 116, 0),
    KEY_DEF(kSecAttrKeySizeMLKEM1024, 117, 0),
    KEY_DEF(kSecAttrKeySizeMLDSA65, 118, 0),
    KEY_DEF(kSecAttrKeySizeMLDSA87, 119, 0),
    KEY_DEF(kSecAttrSynchronizable, 120, 3),  // Tracks 3 values: NO(0), YES(1), kSecAttrSynchronizableAny(2)
    KEY_DEF(kSecAttrTokenID, 121, 3),  // Tracks 3 values
    KEY_DEF(kSecAttrSharingGroup, 122, 0),
    KEY_DEF(kSecAttrSharingGroupNone, 123, 0),
    KEY_DEF(kSecPrivateKeyAttrs, 124, 0),
    KEY_DEF(kSecPublicKeyAttrs, 125, 0),
    KEY_DEF(kSecKeyApplePayEnabled, 126, 0),
    KEY_DEF(kSecKeyOSBound, 127, 0),
    KEY_DEF(kSecKeySealedHashesBound, 128, 0),
    KEY_DEF(kSecAttrSecureEnclaveKeyBlob, 129, 0),
#if TARGET_OS_IOS || TARGET_OS_OSX
    KEY_DEF(kSecSharedPassword, 130, 0),
#endif
    KEY_DEF(kSOSInternalAccessGroup, 131, 0),
#if TARGET_OS_TV
    KEY_DEF(kSecUseUserIndependentKeychain, 132, 0),
#elif TARGET_OS_OSX
    KEY_DEF(kSecUseCertificatesWithMatchIssuers, 133, 0),
    KEY_DEF(kSecAttrAccess, 134, 0),
    KEY_DEF(kSecAttrPRF, 135, 0),
    KEY_DEF(kSecAttrSalt, 136, 0),
    KEY_DEF(kSecAttrRounds, 137, 0),
    KEY_DEF(kSecMatchSubjectStartsWith, 138, 0),
    KEY_DEF(kSecMatchSubjectEndsWith, 139, 0),
    KEY_DEF(kSecMatchSubjectWholeString, 140, 0),
    KEY_DEF(kSecMatchDiacriticInsensitive, 141, 0),
    KEY_DEF(kSecMatchWidthInsensitive, 142, 0),
    KEY_DEF(kSecUseKeychain, 143, 0),
    KEY_DEF(kSecAttrKeyTypeDES, 144, 0),
    KEY_DEF(kSecAttrKeyType3DES, 145, 0),
    KEY_DEF(kSecAttrKeyTypeRC2, 146, 0),
    KEY_DEF(kSecAttrKeyTypeRC4, 147, 0),
    KEY_DEF(kSecAttrKeyTypeDSA, 148, 0),
    KEY_DEF(kSecAttrKeyTypeCAST, 149, 0),
    KEY_DEF(kSecAttrKeyTypeECDSA, 150, 0),
    KEY_DEF(kSecAttrKeyTypeAES, 151, 0),
#endif
};

// ============================================================================
// ALL POSSIBLE VALUES FOR KEYS THAT TRACK VALUES
// ============================================================================
static ValueDef g_value_definitions[] = {
    // Values for kSecClass (parent_key_index=0)
    VALUE_DEF(kSecClassGenericPassword, 0, 0),
    VALUE_DEF(kSecClassInternetPassword, 1, 0),
    VALUE_DEF(kSecClassAppleSharePassword, 2, 0),
    VALUE_DEF(kSecClassCertificate, 3, 0),
    VALUE_DEF(kSecClassKey, 4, 0),
    VALUE_DEF(kSecClassIdentity, 5, 0),
    
    // Values for kSecMatchLimit (parent_key_index=87)
    VALUE_DEF(kSecMatchLimitOne, 0, 87),
    VALUE_DEF(kSecMatchLimitAll, 1, 87),
    
    // Values for kSecUseAuthenticationUI (parent_key_index=107)
    VALUE_DEF(kSecUseAuthenticationUIAllow, 0, 107),
    VALUE_DEF(kSecUseAuthenticationUIFail, 1, 107),
    VALUE_DEF(kSecUseAuthenticationUISkip, 2, 107),
    
    // Values for kSecAttrAccessible (parent_key_index=108)
    VALUE_DEF(kSecAttrAccessibleWhenUnlocked, 0, 108),
    VALUE_DEF(kSecAttrAccessibleAfterFirstUnlock, 1, 108),
    VALUE_DEF(kSecAttrAccessibleAlways, 2, 108),
    VALUE_DEF(kSecAttrAccessibleWhenUnlockedThisDeviceOnly, 3, 108),
    VALUE_DEF(kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly, 4, 108),
    VALUE_DEF(kSecAttrAccessibleAlwaysThisDeviceOnly, 5, 108),
    VALUE_DEF(kSecAttrAccessibleWhenPasscodeSetThisDeviceOnly, 6, 108),
    VALUE_DEF(kSecAttrAccessibleUntilReboot, 7, 108),
    
    // Values for kSecAttrProtocol (parent_key_index=109)
    VALUE_DEF(kSecAttrProtocolFTP, 0, 109),
    VALUE_DEF(kSecAttrProtocolFTPAccount, 1, 109),
    VALUE_DEF(kSecAttrProtocolHTTP, 2, 109),
    VALUE_DEF(kSecAttrProtocolIRC, 3, 109),
    VALUE_DEF(kSecAttrProtocolNNTP, 4, 109),
    VALUE_DEF(kSecAttrProtocolPOP3, 5, 109),
    VALUE_DEF(kSecAttrProtocolSMTP, 6, 109),
    VALUE_DEF(kSecAttrProtocolSOCKS, 7, 109),
    VALUE_DEF(kSecAttrProtocolIMAP, 8, 109),
    VALUE_DEF(kSecAttrProtocolLDAP, 9, 109),
    VALUE_DEF(kSecAttrProtocolAppleTalk, 10, 109),
    VALUE_DEF(kSecAttrProtocolAFP, 11, 109),
    VALUE_DEF(kSecAttrProtocolTelnet, 12, 109),
    VALUE_DEF(kSecAttrProtocolSSH, 13, 109),
    VALUE_DEF(kSecAttrProtocolFTPS, 14, 109),
    VALUE_DEF(kSecAttrProtocolHTTPS, 15, 109),
    VALUE_DEF(kSecAttrProtocolHTTPProxy, 16, 109),
    VALUE_DEF(kSecAttrProtocolHTTPSProxy, 17, 109),
    VALUE_DEF(kSecAttrProtocolFTPProxy, 18, 109),
    VALUE_DEF(kSecAttrProtocolSMB, 19, 109),
    VALUE_DEF(kSecAttrProtocolRTSP, 20, 109),
    VALUE_DEF(kSecAttrProtocolRTSPProxy, 21, 109),
    VALUE_DEF(kSecAttrProtocolDAAP, 22, 109),
    VALUE_DEF(kSecAttrProtocolEPPC, 23, 109),
    VALUE_DEF(kSecAttrProtocolIPP, 24, 109),
    VALUE_DEF(kSecAttrProtocolNNTPS, 25, 109),
    VALUE_DEF(kSecAttrProtocolLDAPS, 26, 109),
    VALUE_DEF(kSecAttrProtocolTelnetS, 27, 109),
    VALUE_DEF(kSecAttrProtocolIMAPS, 28, 109),
    VALUE_DEF(kSecAttrProtocolIRCS, 29, 109),
    VALUE_DEF(kSecAttrProtocolPOP3S, 30, 109),
    
    // Values for kSecAttrAuthenticationType (parent_key_index=110)
    VALUE_DEF(kSecAttrAuthenticationTypeNTLM, 0, 110),
    VALUE_DEF(kSecAttrAuthenticationTypeMSN, 1, 110),
    VALUE_DEF(kSecAttrAuthenticationTypeDPA, 2, 110),
    VALUE_DEF(kSecAttrAuthenticationTypeRPA, 3, 110),
    VALUE_DEF(kSecAttrAuthenticationTypeHTTPBasic, 4, 110),
    VALUE_DEF(kSecAttrAuthenticationTypeHTTPDigest, 5, 110),
    VALUE_DEF(kSecAttrAuthenticationTypeHTMLForm, 6, 110),
    VALUE_DEF(kSecAttrAuthenticationTypeDefault, 7, 110),
    
    // Values for kSecAttrKeyClass (parent_key_index=111)
    VALUE_DEF(kSecAttrKeyClassPublic, 0, 111),
    VALUE_DEF(kSecAttrKeyClassPrivate, 1, 111),
    VALUE_DEF(kSecAttrKeyClassSymmetric, 2, 111),
    
    // Values for kSecAttrKeyType (parent_key_index=112)
    VALUE_DEF(kSecAttrKeyTypeRSA, 0, 112),
    VALUE_DEF(kSecAttrKeyTypeEC, 1, 112),
    VALUE_DEF(kSecAttrKeyTypeECSECPrimeRandom, 2, 112),
    VALUE_DEF(kSecAttrKeyTypeECSECPrimeRandomPKA, 3, 112),
    VALUE_DEF(kSecAttrKeyTypeSecureEnclaveAttestation, 4, 112),
    VALUE_DEF(kSecAttrKeyTypeSecureEnclaveAnonymousAttestation, 5, 112),
    VALUE_DEF(kSecAttrKeyTypeEd25519, 6, 112),
    VALUE_DEF(kSecAttrKeyTypeX25519, 7, 112),
    VALUE_DEF(kSecAttrKeyTypeEd448, 8, 112),
    VALUE_DEF(kSecAttrKeyTypeX448, 9, 112),
    VALUE_DEF(kSecAttrKeyTypeKyber, 10, 112),
    VALUE_DEF(kSecAttrKeyTypeMLKEM, 11, 112),
    VALUE_DEF(kSecAttrKeyTypeMLDSA, 12, 112),

    // Values for kSecAttrSynchronizable (parent_key_index=120)
    // Value index 0: CFBoolean NO (kCFBooleanFalse)
    // Value index 1: CFBoolean YES (kCFBooleanTrue)
    VALUE_DEF(kSecAttrSynchronizableAny, 2, 120),

    // Values for kSecAttrTokenID (parent_key_index=121)
    VALUE_DEF(kSecAttrTokenIDSecureEnclave, 0, 121),
    VALUE_DEF(kSecAttrTokenIDAppleKeyStore, 1, 121),
    VALUE_DEF(kSecAttrTokenIDSecureElement, 2, 121),
};

#pragma clang diagnostic pop

// Array sizes
static const size_t g_num_keys = sizeof(g_key_definitions) / sizeof(g_key_definitions[0]);
static const size_t g_num_values = sizeof(g_value_definitions) / sizeof(g_value_definitions[0]);

// Static lookup dictionaries for fast key and value lookups
static CFMutableDictionaryRef g_key_lookup_dict = NULL;
static CFMutableDictionaryRef g_value_lookup_dict = NULL;
static dispatch_once_t g_lookup_dict_once;

// Initialize the lookup dictionaries
static void init_lookup_dicts(void) {
    dispatch_once(&g_lookup_dict_once, ^{
        // Initialize key lookup dictionary
        g_key_lookup_dict = CFDictionaryCreateMutable(NULL, g_num_keys,
                                                      &kCFTypeDictionaryKeyCallBacks, NULL);
        if (g_key_lookup_dict) {
            for (size_t i = 0; i < g_num_keys; i++) {
                CFStringRef key = *g_key_definitions[i].key;
                if (key) {
                    if (CFDictionaryContainsKey(g_key_lookup_dict, key)) {
                        secerror("SecItemAttrLogger: Duplicate key found in g_key_definitions at index %zu: %@", i, key);
                    }
                    CFDictionarySetValue(g_key_lookup_dict, key, &g_key_definitions[i]);
                }
            }
        }

        // Initialize value lookup dictionary
        g_value_lookup_dict = CFDictionaryCreateMutable(NULL, g_num_values,
                                                        &kCFTypeDictionaryKeyCallBacks, NULL);
        if (g_value_lookup_dict) {
            for (size_t i = 0; i < g_num_values; i++) {
                CFStringRef value = *g_value_definitions[i].value;
                if (value) {
                    if (CFDictionaryContainsKey(g_value_lookup_dict, value)) {
                        secerror("SecItemAttrLogger: Duplicate value found in g_value_definitions at index %zu: %@", i, value);
                    }
                    CFDictionarySetValue(g_value_lookup_dict, value, &g_value_definitions[i]);
                }
            }
        }
        
        if (g_key_lookup_dict && g_value_lookup_dict) {
            secnotice("SecItemAttrLogger", "Initialized lookup dicts: %zu keys, %zu values",
                      CFDictionaryGetCount(g_key_lookup_dict),
                      CFDictionaryGetCount(g_value_lookup_dict));
        } else {
            secerror("SecItemAttrLogger: Failed to initialize lookup dictionaries in SecItemAttrLogger");
        }
    });
}

// Helper to find key definition
static const KeyDef *find_key_def(CFStringRef key) {
    init_lookup_dicts();
    if (g_key_lookup_dict) {
        return (const KeyDef *)CFDictionaryGetValue(g_key_lookup_dict, key);
    }
    return NULL;
}

// Helper to find value definition
static const ValueDef *find_value_def(CFStringRef value) {
    init_lookup_dicts();
    if (g_value_lookup_dict) {
        return (const ValueDef *)CFDictionaryGetValue(g_value_lookup_dict, value);
    }
    return NULL;
}

// helper for type inference.. used only for logging purposes
static const char* get_cf_type_name(CFTypeRef obj) {
    if (!obj) return "NULL";

    CFTypeID typeID = CFGetTypeID(obj);
    if (typeID == CFStringGetTypeID()) return "CFString";
    if (typeID == CFNumberGetTypeID()) return "CFNumber";
    if (typeID == CFBooleanGetTypeID()) return "CFBoolean";
    if (typeID == CFDataGetTypeID()) return "CFData";
    if (typeID == CFArrayGetTypeID()) return "CFArray";
    if (typeID == CFDictionaryGetTypeID()) return "CFDictionary";
    if (typeID == CFDateGetTypeID()) return "CFDate";

    return "Unknown";
}

// Helper function to handle kSecAttrSynchronizable encoding
// Returns the value index for the synchronizable attribute
static int handle_synchronizable_value(const void *value, const KeyDef *key_def) {
    if (!value) {
        return KEY_INVALID_VALUE_TYPE;
    }

    if (CFGetTypeID(value) == CFBooleanGetTypeID()) {
        // CFBoolean value: NO=0, YES=1
        return CFBooleanGetValue((CFBooleanRef)value) ? 1 : 0;
    } else if (CFGetTypeID(value) == CFStringGetTypeID()) {
        // CFString value: check if it's kSecAttrSynchronizableAny
        const ValueDef *value_def = find_value_def((CFStringRef)value);
        if (value_def && value_def->parent_key_index == key_def->key_index) {
            // It's kSecAttrSynchronizableAny (value_index=2)
            return value_def->value_index;
        } else {
            secerror("SecItemAttrLogger: Invalid value for kSecAttrSynchronizable, got %@", (CFStringRef)value);
            return KEY_INVALID_VALUE;
        }
    } else {
        secerror("SecItemAttrLogger: Invalid type for kSecAttrSynchronizable: got %s", get_cf_type_name((CFTypeRef)value));
        return KEY_INVALID_VALUE_TYPE;
    }
}

// Helper function to handle kSecMatchLimit encoding
// Returns the value index for the match limit attribute
static int handle_match_limit_value(const void *value, const KeyDef *key_def) {
    if (!value) {
        return KEY_INVALID_VALUE_TYPE;
    }

    if (CFGetTypeID(value) == CFStringGetTypeID()) {
        // CFString value: check if it's kSecMatchLimitOne or kSecMatchLimitAll
        const ValueDef *value_def = find_value_def((CFStringRef)value);
        if (value_def && value_def->parent_key_index == key_def->key_index) {
            // It's a known constant (kSecMatchLimitOne=0 or kSecMatchLimitAll=1)
            return value_def->value_index;
        } else {
            secerror("SecItemAttrLogger: Invalid string value for kSecMatchLimit, got %@", (CFStringRef)value);
            return KEY_INVALID_VALUE;
        }
    } else if (CFGetTypeID(value) == CFNumberGetTypeID()) {
        // CFNumber value: encode as value_count (i.e offset to differentiate from other two constants) + number
        int numValue;
        if (CFNumberGetValue((CFNumberRef)value, kCFNumberIntType, &numValue)) {
            return key_def->value_count + numValue;
        } else {
            secerror("SecItemAttrLogger: Invalid number value for kSecMatchLimit");
            return KEY_INVALID_VALUE_TYPE;
        }
    } else {
        secerror("SecItemAttrLogger: Invalid type for kSecMatchLimit: expected CFString or CFNumber, got %s", get_cf_type_name((CFTypeRef)value));
        return KEY_INVALID_VALUE_TYPE;
    }
}

// Helper function to handle type field (kSecAttrType/kSecAttrKeyType) encoding
// Returns the value index for the type attribute based on class name
static int handle_type_value(CFStringRef className, const void *value, const KeyDef *key_def) {
    if (!value) {
        return KEY_INVALID_VALUE_TYPE;
    }

    // Determine behavior based on class name
    if (className && (CFEqual(className, kSecClassGenericPassword) || CFEqual(className, kSecClassInternetPassword))) {
        // genp or inet class: don't track value
        return KEY_NOT_TRACKS_VALUE;
    } else if (className && (CFEqual(className, kSecClassKey) || CFEqual(className, kSecClassIdentity))) {
        // Value should be a CFString constant (e.g., kSecAttrKeyTypeRSA)
        if (CFGetTypeID(value) == CFStringGetTypeID()) {
            const ValueDef *value_def = find_value_def((CFStringRef)value);
            if (value_def && value_def->parent_key_index == key_def->key_index) {
                return value_def->value_index;
            } else {
                secerror("SecItemAttrLogger: Invalid value for `type` keyValue: %@", (CFStringRef)value);
                return KEY_INVALID_VALUE;
            }
        } else {
            secerror("SecItemAttrLogger: Invalid type for type field in keys/idnt class: expected CFString, got %s", get_cf_type_name((CFTypeRef)value));
            return KEY_INVALID_VALUE_TYPE;
        }
    } else {
        // Unknown class or no class provided
        secerror("SecItemAttrLogger: Unknown or missing class for type field handling");
        return KEY_INVALID_VALUE_TYPE;
    }
}

// To track info while dynamically building encoding
@interface PresentKeyWrapper : NSObject
@property (nonatomic, assign) int keyIndex;
@property (nonatomic, assign) int valueIndex;  // key index, `KEY_NOT_TRACKS_VALUE`, `KEY_INVALID_VALUE`, `KEY_INVALID_VALUE_TYPE`
@end

@implementation PresentKeyWrapper
@end

// Context structure for dict_applier to carry both queryProcessedInfo and class name
typedef struct {
    NSMutableArray *queryProcessedInfo;
    CFStringRef className;  // Can be NULL if class not found
} DictApplierContext;

// CFDictionaryApplierFunction callback for collecting present keys
static void dict_applier(const void *key, const void *value, void *context) {
    DictApplierContext *ctx = (DictApplierContext *)context;
    NSMutableArray *queryProcessedInfo = ctx->queryProcessedInfo;
    CFStringRef className = ctx->className;

    if (CFGetTypeID(key) != CFStringGetTypeID()) {
        secerror("SecItemAttrLogger: Type mismatch for key: expected CFString, got %s", get_cf_type_name((CFTypeRef)key));
        return;
    }

    CFStringRef key_str = (CFStringRef)key;

    const KeyDef *key_def = find_key_def(key_str);

    if (key_def) {
        PresentKeyWrapper *wrapper = [[PresentKeyWrapper alloc] init];
        wrapper.keyIndex = key_def->key_index;
        wrapper.valueIndex = KEY_NOT_TRACKS_VALUE;  // Default: key doesn't track values

        // Special handling for type field (kSecAttrType or kSecAttrKeyType)
        if (key_def->key_index == kSecAttrTypeIndex || key_def->key_index == kSecAttrKeyTypeIndex) {
            // Determine the correct key index based on class
            if (className && (CFEqual(className, kSecClassGenericPassword) || CFEqual(className, kSecClassInternetPassword))) {
                // genp or inet class: use kSecAttrTypeIndex
                wrapper.keyIndex = kSecAttrTypeIndex;
            } else if (className && (CFEqual(className, kSecClassKey) || CFEqual(className, kSecClassIdentity))) {
                // keys or idnt class: use kSecAttrKeyTypeIndex
                wrapper.keyIndex = kSecAttrKeyTypeIndex;
            }
            wrapper.valueIndex = handle_type_value(className, value, key_def);
        } else if (key_def->value_count > 0) { // handle all cases where keys tracks values
            // This key tracks values - check if the value is a known constant
            if (key_def->key_index == 120) {  // kSecAttrSynchronizable
                wrapper.valueIndex = handle_synchronizable_value(value, key_def);
            } else if (key_def->key_index == 87) {  // kSecMatchLimit
                wrapper.valueIndex = handle_match_limit_value(value, key_def);
            } else if (value && CFGetTypeID(value) == CFStringGetTypeID()) {
                // Value is a CFString, try to find it in our value definitions
                const ValueDef *value_def = find_value_def((CFStringRef)value);
                
                if (value_def && value_def->parent_key_index == key_def->key_index) {
                    // Set key with value index
                    wrapper.valueIndex = value_def->value_index;
                } else {
                    // Value not recognized
                    secerror("SecItemAttrLogger: Invalid value passed for attr: %@, value: %@", key_str, (CFStringRef)value);
                    wrapper.valueIndex = KEY_INVALID_VALUE;
                }
            } else {
                // Non-string value (CFNumber, CFBoolean, CFData, etc.)
                secerror("SecItemAttrLogger: Type mismatch for value: expected CFString, got: %s for attr: %@", get_cf_type_name((CFTypeRef)value), key_str);
                wrapper.valueIndex = KEY_INVALID_VALUE_TYPE;
            }
        }

        [queryProcessedInfo addObject:wrapper];
    } else {
        // Key not found in our definitions
        secerror("SecItemAttrLogger: attr not found in definitions - attr name: %@", key_str);
    }
}

// Implementation of SecItemAttrLoggerInfo
@interface SecItemAttrLoggerInfo ()
@property (nonatomic, readwrite) NSInteger encodingVersion;
@property (nonatomic, readwrite, copy) NSString *encoding;
@property (nonatomic, readwrite) NSUInteger numKeys;
@end

@implementation SecItemAttrLoggerInfo

+ (instancetype)loggerInfoWithDictionary:(NSDictionary *)dictionary {
    SecItemAttrLoggerInfo *info = [[SecItemAttrLoggerInfo alloc] init];
    info.encodingVersion = ENCODING_VERSION;
    info.numKeys = g_num_keys;

    // Extract kSecClass to determine which class we're dealing with
    CFStringRef className = NULL;
    CFTypeRef classValue = CFDictionaryGetValue((__bridge CFDictionaryRef)dictionary, kSecClass);

    if (classValue && CFGetTypeID(classValue) == CFStringGetTypeID()) {
        className = (CFStringRef)classValue;
    }

    // Create NSMutableArray to collect present keys
    NSMutableArray<PresentKeyWrapper *> *queryProcessedInfo = [NSMutableArray array];

    // Set up context with both queryProcessedInfo and className
    DictApplierContext context = {
        .queryProcessedInfo = queryProcessedInfo,
        .className = className
    };

    // Iterate through the dictionary and collect present keys
    CFDictionaryApplyFunction((__bridge CFDictionaryRef)dictionary, dict_applier, &context);

    // Sort keys by keyIndex for deterministic output
    NSArray<PresentKeyWrapper *> *sortedKeys = [queryProcessedInfo sortedArrayUsingComparator:^NSComparisonResult(PresentKeyWrapper *obj1, PresentKeyWrapper *obj2) {
        if (obj1.keyIndex < obj2.keyIndex) {
            return NSOrderedAscending;
        } else if (obj1.keyIndex > obj2.keyIndex) {
            return NSOrderedDescending;
        }
        return NSOrderedSame;
    }];

    // Build the compressed encoding string
    info.encoding = [self buildEncodingStringFromSortedKeys:sortedKeys];

    return info;
}

+ (NSString *)buildEncodingStringFromSortedKeys:(NSArray<PresentKeyWrapper *> *)sortedKeys {
    if (sortedKeys.count == 0) {
        return @"";
    }

    NSMutableString *actualEncoding = [NSMutableString string];

    for (PresentKeyWrapper *wrapper in sortedKeys) {
        if (wrapper.valueIndex == KEY_NOT_TRACKS_VALUE) {
            // Key doesn't track values
            [actualEncoding appendFormat:@"%d;", wrapper.keyIndex];
        } else {
            // Normal value or error constant - encode the actual valueIndex
            [actualEncoding appendFormat:@"%d,%d;", wrapper.keyIndex, wrapper.valueIndex];
        }
    }

    return actualEncoding;
}

// helpful for debugging purposes
+ (void)printEncodingInfo:(SecItemAttrLoggerInfo *)loggerInfo {
    if (!loggerInfo) {
        secerror("SecItemAttrLogger: NULL loggerInfo provided");
        return;
    }

    if (!loggerInfo.encoding) {
        secnotice("SecItemAttrLogger", "SecItemAttrLoggerInfo: NULL encoding");
        return;
    }

    secnotice("SecItemAttrLogger", "========== SecItemAttrLoggerInfo ==========");
    secnotice("SecItemAttrLogger", "Encoding Version: %ld", (long)loggerInfo.encodingVersion);
    secnotice("SecItemAttrLogger", "Encoding: %s", [loggerInfo.encoding UTF8String] ?: "(null)");

    if (loggerInfo.encoding.length == 0) {
        secnotice("SecItemAttrLogger", "No attributes present (empty encoding)");
        secnotice("SecItemAttrLogger", "==========================================");
        return;
    }

    secnotice("SecItemAttrLogger", "Attributes present:");

    // Parse compressed encoding format: "key_index[,value_index];..."
    NSArray *segments = [loggerInfo.encoding componentsSeparatedByString:@";"];
    for (NSString *segment in segments) {
        if (segment.length == 0) continue;

        // Parse key_index and value_index
        NSArray *parts = [segment componentsSeparatedByString:@","];
        int keyIndex = [parts[0] intValue];

        int valueIndex = KEY_NOT_TRACKS_VALUE;
        if (parts.count > 1) {
            valueIndex = [parts[1] intValue];
        }

        // Look up and print the key name
        if (keyIndex >= 0 && keyIndex < (int)g_num_keys) {
            const KeyDef *key_def = &g_key_definitions[keyIndex];
            CFStringRef key = *key_def->key;

            if (valueIndex == KEY_NOT_TRACKS_VALUE) {
                // Key doesn't track values
                secnotice("SecItemAttrLogger", "  [%d] %@", keyIndex, key);
            } else if (valueIndex == KEY_INVALID_VALUE) {
                // Invalid value error
                secnotice("SecItemAttrLogger", "  [%d] %@ = value_index:%d (KEY_INVALID_VALUE)", keyIndex, key, valueIndex);
            } else if (valueIndex == KEY_INVALID_VALUE_TYPE) {
                // Invalid value type error
                secnotice("SecItemAttrLogger", "  [%d] %@ = value_index:%d (KEY_INVALID_VALUE_TYPE)", keyIndex, key, valueIndex);
            } else {
                // Normal value
                secnotice("SecItemAttrLogger", "  [%d] %@ = value_index:%d", keyIndex, key, valueIndex);
            }
        }
    }

    secnotice("SecItemAttrLogger", "==========================================");
}

@end

// Helper function to calculate time difference in microseconds (similar to KCSharingTimeDiff)
static uint64_t SecItemTimeDiffInMicroseconds(uint64_t start, uint64_t stop)
{
    static uint64_t time_overhead_measured = 0;
    static double timebase_factor = 0;

    if (time_overhead_measured == 0) {
        uint64_t t0 = mach_absolute_time();
        time_overhead_measured = mach_absolute_time() - t0;

        struct mach_timebase_info timebase_info = {};
        mach_timebase_info(&timebase_info);
        timebase_factor = ((double)timebase_info.numer)/((double)timebase_info.denom);
    }
    // output in microseconds
    return ((stop - start - time_overhead_measured) * timebase_factor) / NSEC_PER_USEC;
}

static void addLoggerInfoToMetrics(NSMutableDictionary*  metrics, NSString* key, NSDictionary* dict) {
    if (dict == nil) {
        return;
    }

    SecItemAttrLoggerInfo *loggerInfo = [SecItemAttrLoggerInfo loggerInfoWithDictionary:dict];
    if (loggerInfo.encoding) {
        metrics[key] = loggerInfo.encoding;
    }
}

// Report telemetry
void reportQueryAccessStructure(
    CFDictionaryRef dict1,
    CFDictionaryRef dict2,
    const char *operation,
    SecurityClient *client,
    CFStringRef accessGroup,
    uint64_t start_time,
    uint64_t end_time,
    int result_count,        // -1 if not available
    size_t result_size,      // 0 if not available
    CFErrorRef error)        // Error from operation, can be NULL
{
    if (!operation) {
        secerror("SecItemAttrLogger: Missing operation name");
        return;
    }

    // At least one dictionary must be provided
    if (!dict1 && !dict2) {
        secerror("SecItemAttrLogger: Missing dict params");
        return;
    }

    // Calculate elapsed time if start/end times provided
    uint64_t db_time_us = 0;
    if (start_time > 0 && end_time > 0) {
        db_time_us = SecItemTimeDiffInMicroseconds(start_time, end_time);
    }

    // Don't take ownership of the caller's dicts, they may still want to use them.
    // But we need them to be captured with ARC by the block below.
    // So these are strong references.
    NSDictionary* nsDict1 = (__bridge NSDictionary*)dict1;
    NSDictionary* nsDict2 = (__bridge NSDictionary*)dict2;

    // Determine what to report based on presence of access group and application identifier
    // Priority: if both present -> access group, otherwise whichever is present, else "unknown"
    NSString* identifierToReport = nil;
    if (accessGroup) {
        // Only access group present
        // See note above about not taking ownership
        identifierToReport = (__bridge NSString*)accessGroup;
    } else if (client && client->task) {
        // This returns a +1, it has Copy in the name, so use __bridge_transfer
        identifierToReport = (__bridge_transfer NSString*)SecTaskCopyApplicationIdentifier(client->task);
    } else {
        identifierToReport = @"unknown";
    }

    NSError* nsError = (__bridge NSError*)error;

    // Capture musr status
    bool hasMusr = (client && client->musr != NULL);

    // Send analytics event
    [SecCoreAnalytics sendEventLazy:@"com.apple.security.keychain.SecItemAPIUsage" builder:^{
        NSMutableDictionary *metrics = [NSMutableDictionary dictionary];

        // we already checked that this is non-NULL above
        metrics[@"operation"] = @(operation);

        // non-NULL from above
        metrics[@"client"] = identifierToReport;

        addLoggerInfoToMetrics(metrics, @"attributesOne", nsDict1);
        addLoggerInfoToMetrics(metrics, @"attributesTwo", nsDict2);

        // Add musr field (1 if present, 0 if not)
        metrics[@"musr"] = hasMusr ? @1 : @0;

        // Add error as nested JSON string (similar to KCSharing)
        // Format the CFErrorRef as JSON (similar to KCSharing)
        NSString *errorJSON = nil;
        if (nsError) {
            errorJSON = [nsError formatAsNestedJSON];
        } else if (error) {
            errorJSON = @"{Error:{\"Invalid error object\"}}";
        } else {
            errorJSON = @"{Success:{}}";
        }
        if (errorJSON) {
            metrics[@"error"] = errorJSON;
        }

        if (db_time_us >= 0) {
            metrics[@"time"] = @(db_time_us);
        }

        // Add optional performance metrics only if they are available
        if (result_count >= 0) {
            metrics[@"itemcount"] = @(result_count);
        }
        if (result_size > 0) {
            metrics[@"size"] = @(result_size);
        }

        metrics[@"encodingVersion"] = @(ENCODING_VERSION);

        return metrics;
    }];
}
