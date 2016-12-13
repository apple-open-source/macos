/*
 * Copyright (c) 2016 Apple Inc. All Rights Reserved.
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

#include "TokenLogin.h"

#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>
#include <Security/SecKeyPriv.h>
#include "SecBase64P.h"
#include <Security/SecIdentity.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecKeychainPriv.h>
#include <security_utilities/cfutilities.h>
#include <libaks.h>
#include <libaks_smartcard.h>

extern "C" {
#include <ctkclient.h>
#include <coreauthd_spi.h>
}

#define kSecTokenLoginDomain CFSTR("com.apple.security.tokenlogin")

static CFStringRef cfDataToHex(CFDataRef bin)
{
    size_t len = CFDataGetLength(bin) * 2;
    CFMutableStringRef str = CFStringCreateMutable(NULL, len);

    static const char* digits[] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "A", "B", "C", "D", "E", "F"};

    const uint8_t* data = CFDataGetBytePtr(bin);
    for (size_t i = 0; i < CFDataGetLength(bin); i++) {
        CFStringAppendCString(str, digits[data[i] >> 4], 1);
        CFStringAppendCString(str, digits[data[i] & 0xf], 1);
    }
    return str;
}

static CFStringRef getPin(CFDictionaryRef context)
{
	if (!context) {
		return NULL;
	}

	CFStringRef pin = (CFStringRef)CFDictionaryGetValue(context, kSecAttrService);
	if (!pin || CFGetTypeID(pin) != CFStringGetTypeID()) {
		return NULL;
	}
	return pin;
}

static CFStringRef getTokenId(CFDictionaryRef context)
{
	if (!context) {
		return NULL;
	}

	CFStringRef tokenId = (CFStringRef)CFDictionaryGetValue(context, kSecAttrTokenID);
	if (!tokenId || CFGetTypeID(tokenId) != CFStringGetTypeID()) {
		secinfo("TokenLogin", "Invalid tokenId");
		return NULL;
	}
	return tokenId;
}

static CFDataRef getPubKeyHash(CFDictionaryRef context)
{
	if (!context) {
		return NULL;
	}

	CFDataRef pubKeyHash = (CFDataRef)CFDictionaryGetValue(context, kSecAttrPublicKeyHash);
	if (!pubKeyHash || CFGetTypeID(pubKeyHash) != CFDataGetTypeID()) {
		secinfo("TokenLogin", "Invalid pubkeyhash");
		return NULL;
	}
	return pubKeyHash;
}

static CFDataRef getPubKeyHashWrap(CFDictionaryRef context)
{
	if (!context) {
		return NULL;
	}

	CFDataRef pubKeyHashWrap = (CFDataRef)CFDictionaryGetValue(context, kSecAttrAccount);
	if (!pubKeyHashWrap || CFGetTypeID(pubKeyHashWrap) != CFDataGetTypeID()) {
		secinfo("TokenLogin", "Invalid pubkeyhashwrap");
		return NULL;
	}
	return pubKeyHashWrap;
}

static OSStatus privKeyForPubKeyHash(CFDictionaryRef context, SecKeyRef *privKey, CFTypeRef *laCtx)
{
	if (!context) {
		return errSecParam;
	}

    CFRef<CFMutableDictionaryRef> tokenAttributes = makeCFMutableDictionary(1, kSecAttrTokenID, getTokenId(context));
    CFRef<CFErrorRef> error;

	CFStringRef pin = getPin(context);
	if (pin) {
		CFRef<CFDictionaryRef> LAParams = makeCFDictionary(1, CFSTR("useDaemon"), kCFBooleanFalse);
		CFRef<CFTypeRef> LAContext = LACreateNewContextWithACMContext(LAParams.as<CFDataRef>(), error.take());
		if (!LAContext) {
			secinfo("TokenLogin", "Failed to LA Context: %@", error.get());
			return errSecParam;
		}
		if (laCtx)
			*laCtx = (CFTypeRef)CFRetain(LAContext);
        CFRef<CFDataRef> externalizedContext = LACopyACMContext(LAContext, error.take());
        if (!externalizedContext) {
            secinfo("TokenLogin", "Failed to get externalized context: %@", error.get());
            return errSecParam;
        }
        CFDictionarySetValue(tokenAttributes, kSecUseCredentialReference, externalizedContext.get());
        CFDictionarySetValue(tokenAttributes, CFSTR("PIN"), pin);
    }

    CFRef<TKTokenRef> token = TKTokenCreate(tokenAttributes, error.take());
    if (!token) {
        secinfo("TokenLogin", "Failed to create token: %@", error.get());
        return errSecParam;
    }

    CFRef<CFArrayRef> identities = TKTokenCopyIdentities(token, TKTokenKeyUsageAny, error.take());
    if (!identities || !CFArrayGetCount(identities)) {
        secinfo("TokenLogin", "No identities found for token: %@", error.get());
        return errSecParam;
    }

	CFDataRef desiredHash = getPubKeyHashWrap(context);
    CFIndex idx, count = CFArrayGetCount(identities);
    for (idx = 0; idx < count; ++idx) {
        SecIdentityRef identity = (SecIdentityRef)CFArrayGetValueAtIndex(identities, idx);
        CFRef<SecCertificateRef> certificate;
        OSStatus result = SecIdentityCopyCertificate(identity, certificate.take());
        if (result != errSecSuccess) {
            secinfo("TokenLogin", "Failed to get certificate for identity: %d", (int) result);
            continue;
        }
        
        CFRef<CFDataRef> identityHash = SecCertificateCopyPublicKeySHA1Digest(certificate);
        if (identityHash && CFEqual(desiredHash, identityHash)) {
            result = SecIdentityCopyPrivateKey(identity, privKey);
            if (result != errSecSuccess) {
                secinfo("TokenLogin", "Failed to get identity private key: %d", (int) result);
            }
            return result;
        }
    }
    
    return errSecParam;
}

OSStatus TokenLoginGetContext(const void *base64TokenLoginData, UInt32 base64TokenLoginDataLength, CFDictionaryRef *context)
{
	if (!base64TokenLoginData || !context) {
		return errSecParam;
	}

	// Token data are base64 encoded in password.
	size_t dataLen = SecBase64Decode((const char *)base64TokenLoginData, base64TokenLoginDataLength, NULL, 0);
	if (!dataLen) {
		secinfo("TokenLogin", "Invalid base64 encoded token data");
		return errSecParam;
	}

	CFRef<CFMutableDataRef> data = CFDataCreateMutable(kCFAllocatorDefault, dataLen);
	dataLen = SecBase64Decode((const char *)base64TokenLoginData, base64TokenLoginDataLength, CFDataGetMutableBytePtr(data), dataLen);
	if (!dataLen) {
		secinfo("TokenLogin", "Invalid base64 encoded token data");
		return errSecParam;
	}
	CFDataSetLength(data, dataLen);

	// Content of the password consists of a serialized dictionary containing token ID, PIN, wrap key hash etc.
	CFRef<CFErrorRef> error;
	*context = (CFDictionaryRef)CFPropertyListCreateWithData(kCFAllocatorDefault,
															 data,
															 kCFPropertyListImmutable,
															 NULL,
															 error.take());
	if (!*context || CFGetTypeID(*context) != CFDictionaryGetTypeID()) {
		secinfo("TokenLogin", "Invalid token login data property list, %@", error.get());
		return errSecParam;
	}

	if (!getPin(*context) || !getTokenId(*context) || !getPubKeyHash(*context) || !getPubKeyHashWrap(*context)) {
		secinfo("TokenLogin", "Invalid token login data context, %@", error.get());
		return errSecParam;
	}

	return errSecSuccess;
}

OSStatus TokenLoginGetUnlockKey(CFDictionaryRef context, CFDataRef *unlockKey)
{
	if (!context || !unlockKey) {
		return errSecParam;
	}

	CFRef<CFDictionaryRef> loginData;
	OSStatus result = TokenLoginGetLoginData(context, loginData.take());
	if (result != errSecSuccess) {
		secinfo("TokenLogin", "Failed to get login data: %d", (int)result);
		return result;
	}

	CFDataRef wrappedUnlockKey = (CFDataRef)CFDictionaryGetValue(loginData, kSecValueData);
	if (!wrappedUnlockKey) {
		secinfo("TokenLogin", "Wrapped unlock key not found in unlock key data");
		return errSecParam;
	}
	SecKeyAlgorithm algorithm = (SecKeyAlgorithm)CFDictionaryGetValue(loginData, kSecAttrService);
	if (!algorithm) {
		secinfo("TokenLogin", "Algorithm not found in unlock key data");
		return errSecParam;
	}

	CFRef<SecKeyRef> privKey;
	CFRef<CFTypeRef> LAContext;
	result = privKeyForPubKeyHash(context, privKey.take(), LAContext.take());
	if (result != errSecSuccess) {
		secinfo("TokenLogin", "Failed to get private key for public key hash: %d", (int)result);
		return result;
	}

	CFRef<SecKeyRef> pubKey = SecKeyCopyPublicKey(privKey);
	if (!pubKey) {
		secinfo("TokenLogin", "Failed to get public key from private key");
		return errSecParam;
	}
	CFRef<CFErrorRef> error;
	*unlockKey = SecKeyCreateDecryptedData(privKey,
										   algorithm,
										   wrappedUnlockKey,
										   error.take());
	if (!*unlockKey) {
		secinfo("TokenLogin", "Failed to unwrap unlock key: %@", error.get());
		return errSecDecode;
	}

	// we need to re-wrap already unwrapped data to avoid capturing and reusing communication with the smartcard
	CFRef<CFDataRef> reWrappedUnlockKey = SecKeyCreateEncryptedData(pubKey, algorithm, *unlockKey, error.take());
	if (!reWrappedUnlockKey) {
		secinfo("TokenLogin", "Failed to rewrap unlock key: %@", error.get());
		TokenLoginDeleteUnlockData(getPubKeyHash(context));
		return errSecParam;
	}

	CFRef<CFMutableDictionaryRef> newDict = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 4, loginData);
	if (newDict) {
		CFDictionarySetValue(newDict, kSecValueData, reWrappedUnlockKey);
		TokenLoginStoreUnlockData(context, newDict);
	}

	return errSecSuccess;
}

OSStatus TokenLoginGetLoginData(CFDictionaryRef context, CFDictionaryRef *loginData)
{
	if (!loginData || !context) {
		return errSecParam;
	}

	CFRef<CFStringRef> pubKeyHashHex = cfDataToHex(getPubKeyHash(context));
	CFPreferencesSynchronize(kSecTokenLoginDomain, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
	CFRef<CFDataRef> storedData = (CFDataRef)CFPreferencesCopyValue(pubKeyHashHex, kSecTokenLoginDomain, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
	if (!storedData) {
		secinfo("TokenLogin", "Failed to read token login plist");
		return errSecIO;
	}

	CFRef<CFErrorRef> error;
	*loginData = (CFDictionaryRef)CFPropertyListCreateWithData(kCFAllocatorDefault,
															   storedData,
															   kCFPropertyListImmutable,
															   NULL,
															   error.take());
	if (!*loginData || CFGetTypeID(*loginData) != CFDictionaryGetTypeID()) {
		secinfo("TokenLogin", "Failed to deserialize unlock key data: %@", error.get());
		return errSecParam;
	}

	return errSecSuccess;
}

OSStatus TokenLoginGetPin(CFDictionaryRef context, CFStringRef *pin)
{
	if (!pin || !context) {
		return errSecParam;
	}
	*pin = getPin(context);

	return errSecSuccess;
}

OSStatus TokenLoginUpdateUnlockData(CFDictionaryRef context, CFStringRef password)
{
	if (!context) {
		return errSecParam;
	}

	CFRef<SecKeychainRef> loginKeychain;
	OSStatus result = SecKeychainCopyLogin(loginKeychain.take());
	if (result != errSecSuccess) {
		secinfo("TokenLogin", "Failed to get user keychain: %d", (int) result);
		return result;
	}

    return SecKeychainStoreUnlockKeyWithPubKeyHash(getPubKeyHash(context), getTokenId(context), getPubKeyHashWrap(context), loginKeychain, password);
}

OSStatus TokenLoginCreateLoginData(CFStringRef tokenId, CFDataRef pubKeyHash, CFDataRef pubKeyHashWrap, CFDataRef unlockKey, CFDataRef scBlob)
{
	if (!tokenId || !pubKeyHash || !pubKeyHashWrap || !unlockKey || !scBlob)
		return errSecParam;

	CFRef<CFDictionaryRef> ctx = makeCFDictionary(3,
												  kSecAttrTokenID,			tokenId,
												  kSecAttrPublicKeyHash,	pubKeyHash,
												  kSecAttrAccount,			pubKeyHashWrap
												  );
	CFRef<SecKeyRef> privKey;
	OSStatus result = privKeyForPubKeyHash(ctx, privKey.take(), NULL);
	if (result != errSecSuccess) {
		secinfo("TokenLogin", "Failed to get private key for public key hash: %d", (int) result);
		return result;
	}

	CFRef<SecKeyRef> pubKey = SecKeyCopyPublicKey(privKey);
	if (!pubKey) {
		secinfo("TokenLogin", "Failed to get public key from private key");
		return errSecParam;
	}

	SecKeyAlgorithm algorithms[] = {
		kSecKeyAlgorithmECIESEncryptionStandardX963SHA512AESGCM,
		kSecKeyAlgorithmECIESEncryptionStandardX963SHA384AESGCM,
		kSecKeyAlgorithmECIESEncryptionStandardX963SHA256AESGCM,
		kSecKeyAlgorithmECIESEncryptionStandardX963SHA224AESGCM,
		kSecKeyAlgorithmECIESEncryptionStandardX963SHA1AESGCM,
		kSecKeyAlgorithmRSAEncryptionOAEPSHA512AESGCM,
		kSecKeyAlgorithmRSAEncryptionOAEPSHA384AESGCM,
		kSecKeyAlgorithmRSAEncryptionOAEPSHA256AESGCM,
		kSecKeyAlgorithmRSAEncryptionOAEPSHA224AESGCM,
		kSecKeyAlgorithmRSAEncryptionOAEPSHA1AESGCM
	};

	SecKeyAlgorithm algorithm = NULL;
	for (size_t i = 0; i < sizeof(algorithms) / sizeof(*algorithms); i++) {
		if (SecKeyIsAlgorithmSupported(pubKey, kSecKeyOperationTypeEncrypt, algorithms[i])
			&& SecKeyIsAlgorithmSupported(privKey, kSecKeyOperationTypeDecrypt, algorithms[i])) {
			algorithm = algorithms[i];
			break;
		}
	}
	if (algorithm == NULL) {
		secinfo("SecKeychain", "Failed to find supported wrap algorithm");
		return errSecParam;
	}

	CFRef<CFErrorRef> error;
	CFRef<CFDataRef> wrappedUnlockKey = SecKeyCreateEncryptedData(pubKey, algorithm, unlockKey, error.take());
	if (!wrappedUnlockKey) {
		secinfo("TokenLogin", "Failed to wrap unlock key: %@", error.get());
		return errSecParam;
	}

	CFRef<CFDictionaryRef> loginData = makeCFDictionary(4,
														kSecAttrService,		algorithm,
														kSecAttrPublicKeyHash,	pubKeyHashWrap,
														kSecValueData,			wrappedUnlockKey.get(),
														kSecClassKey,			scBlob
														);
	return TokenLoginStoreUnlockData(ctx, loginData);
}

OSStatus TokenLoginStoreUnlockData(CFDictionaryRef context, CFDictionaryRef loginData)
{

	CFRef<CFErrorRef> error;
	CFRef<CFDataRef> data = CFPropertyListCreateData(kCFAllocatorDefault,
										   loginData,
										   kCFPropertyListBinaryFormat_v1_0,
										   0,
										   error.take());
	if (!data) {
		secdebug("TokenLogin", "Failed to create unlock data: %@", error.get());
		return errSecInternal;
	}
    CFRef<CFStringRef> pubKeyHashHex = cfDataToHex(getPubKeyHash(context));
	CFPreferencesSetValue(pubKeyHashHex, data, kSecTokenLoginDomain, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
	CFPreferencesSynchronize(kSecTokenLoginDomain, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
	CFRef<CFDataRef> storedData = (CFDataRef)CFPreferencesCopyValue(pubKeyHashHex, kSecTokenLoginDomain, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);

	if (!storedData || !CFEqual(storedData, data)) {
        secinfo("TokenLogin", "Failed to write token login plist");
        return errSecIO;
    }

    return errSecSuccess;
}

OSStatus TokenLoginDeleteUnlockData(CFDataRef pubKeyHash)
{
    CFRef<CFStringRef> pubKeyHashHex = cfDataToHex(pubKeyHash);
	CFPreferencesSetValue(pubKeyHashHex, NULL, kSecTokenLoginDomain, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
	CFPreferencesSynchronize(kSecTokenLoginDomain, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
	CFRef<CFDataRef> storedData = (CFDataRef)CFPreferencesCopyValue(pubKeyHashHex, kSecTokenLoginDomain, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);

	if (storedData) {
		secinfo("TokenLogin", "Failed to remove unlock data");
		return errSecIO;
	}
	
    return errSecSuccess;
}

OSStatus TokenLoginGetScBlob(CFDataRef pubKeyHashWrap, CFStringRef tokenId, CFStringRef password, CFDataRef *scBlob)
{
	if (scBlob == NULL || password == NULL || pubKeyHashWrap == NULL || tokenId == NULL) {
		secinfo("TokenLogin", "TokenLoginGetScBlob wrong params");
		return errSecParam;
	}

	CFRef<CFDictionaryRef> ctx = makeCFDictionary(2,
												  kSecAttrTokenID,			tokenId,
												  kSecAttrAccount,			pubKeyHashWrap
												  );

	CFRef<SecKeyRef> privKey;
	OSStatus retval = privKeyForPubKeyHash(ctx, privKey.take(), NULL);
	if (retval != errSecSuccess) {
		secinfo("TokenLogin", "TokenLoginGetScBlob failed to get private key for public key hash: %d", (int) retval);
		return retval;
	}

	CFRef<SecKeyRef> pubKey = SecKeyCopyPublicKey(privKey);
	if (!pubKey) {
		secinfo("TokenLogin", "TokenLoginGetScBlob no pubkey");
		return errSecInternal;
	}

	CFRef<CFDictionaryRef> attributes = SecKeyCopyAttributes(pubKey);
	if (!attributes) {
		secinfo("TokenLogin", "TokenLoginGetScBlob no attributes");
		return errSecInternal;
	}

	aks_smartcard_mode_t mode;
	CFRef<CFStringRef> type = (CFStringRef)CFDictionaryGetValue(attributes, kSecAttrKeyType);
	if (CFEqual(type, kSecAttrKeyTypeRSA))
		mode = AKS_SMARTCARD_MODE_RSA;
	else if (CFEqual(type, kSecAttrKeyTypeEC))
		mode = AKS_SMARTCARD_MODE_ECDH;
	else {
		secinfo("TokenLogin", "TokenLoginGetScBlob bad type");
		return errSecNotAvailable;
	}

	CFRef<CFDataRef> publicBytes = SecKeyCopyExternalRepresentation(pubKey, NULL);
	if (!publicBytes) {
		secinfo("TokenLogin", "TokenLoginGetScBlob cannot get public bytes");
		return retval;
	}

	CFIndex maxLength = CFStringGetMaximumSizeForEncoding(CFStringGetLength(password), kCFStringEncodingUTF8) + 1;
	char* buf = (char*)malloc(maxLength);
	if (buf == NULL) {
		secinfo("TokenLogin", "TokenLoginGetScBlob no mem for buffer");
		return retval;
	}

	if (CFStringGetCString(password, buf, maxLength, kCFStringEncodingUTF8) == FALSE) {
		secinfo("TokenLogin", "TokenLoginGetScBlob no pwd cstr");
		free(buf);
		return retval;
	}

	void *sc_blob = NULL;
	size_t sc_len = 0;
	aks_smartcard_unregister(session_keybag_handle); // just to be sure no previous registration exist
	kern_return_t aks_retval = aks_smartcard_register(session_keybag_handle, (uint8_t *)buf, strlen(buf), mode, (uint8_t *)CFDataGetBytePtr(publicBytes), (size_t)CFDataGetLength(publicBytes), &sc_blob, &sc_len);
	free(buf);
	secinfo("TokenLogin", "TokenLoginGetScBlob register result %d", aks_retval);

	if (sc_blob) {
		*scBlob = CFDataCreate(kCFAllocatorDefault, (const UInt8 *)sc_blob, (CFIndex)sc_len);
		free(sc_blob);
	}
	return aks_retval;
}

OSStatus TokenLoginUnlockKeybag(CFDictionaryRef context, CFDictionaryRef loginData)
{
	if (!loginData || !context) {
		return errSecParam;
	}

	CFDataRef scBlob = (CFDataRef)CFDictionaryGetValue(loginData, kSecClassKey);
	if (scBlob == NULL) {
		secinfo("TokenLogin", "Failed to get scblob");
		return errSecInternal;
	}

	CFRef<CFErrorRef> error;
	CFRef<SecKeyRef> privKey;
	CFRef<CFTypeRef> LAContext;
	OSStatus retval = privKeyForPubKeyHash(context, privKey.take(), LAContext.take());
	if (retval != errSecSuccess) {
		secinfo("TokenLogin", "Failed to get private key for public key hash: %d", (int) retval);
		return retval;
	}

	CFRef<SecKeyRef> pubKey = SecKeyCopyPublicKey(privKey);
	if (!pubKey) {
		secinfo("TokenLogin", "Failed to get pubkey");
		return retval;
	}

	CFRef<CFDictionaryRef> attributes = SecKeyCopyAttributes(pubKey);
	if (!attributes) {
		secinfo("TokenLogin", "TokenLoginUnlockKeybag no attributes");
		return errSecInternal;
	}

	aks_smartcard_mode_t mode;
	CFStringRef type = (CFStringRef)CFDictionaryGetValue(attributes, kSecAttrKeyType);
	if (CFEqual(type, kSecAttrKeyTypeRSA))
		mode = AKS_SMARTCARD_MODE_RSA;
	else if (CFEqual(type, kSecAttrKeyTypeEC))
		mode = AKS_SMARTCARD_MODE_ECDH;
	else {
		secinfo("TokenLogin", "TokenLoginUnlockKeybag bad type");
		return errSecNotAvailable;
	}

	void *scChallenge = NULL;
	size_t scChallengeLen = 0;
	int res = aks_smartcard_request_unlock(session_keybag_handle, (uint8_t *)CFDataGetBytePtr(scBlob), (size_t)CFDataGetLength(scBlob), &scChallenge, &scChallengeLen);
	if (res != 0) {
		secinfo("TokenLogin", "TokenLoginUnlockKeybag cannot request unlock: %x", res);
		return errSecInternal;
	}
	const void *scUsk = NULL;
	size_t scUskLen = 0;
	res = aks_smartcard_get_sc_usk(scChallenge, scChallengeLen, &scUsk, &scUskLen);

	if (res != 0 || scUsk == NULL) {
		free(scChallenge);
		secinfo("TokenLogin", "TokenLoginUnlockKeybag cannot get usk: %x", res);
		return errSecInternal;
	}

	CFRef<CFTypeRef> wrappedUsk;
	if (mode == AKS_SMARTCARD_MODE_ECDH) {
		const void *ecPub = NULL;
		size_t ecPubLen = 0;
		res = aks_smartcard_get_ec_pub(scChallenge, scChallengeLen, &ecPub, &ecPubLen);
		if (res != 0 || ecPub == NULL) {
			free(scChallenge);
			secinfo("TokenLogin", "TokenLoginUnlockKeybag cannot get ecpub: %x", res);
			return errSecInternal;
		}
		wrappedUsk = CFDataCreateMutable(kCFAllocatorDefault, ecPubLen + scUskLen);
		if (!wrappedUsk) {
			free(scChallenge);
			secinfo("TokenLogin", "TokenLoginUnlockKeybag no mem for ecpubusk");
			return errSecInternal;
		}
		CFDataAppendBytes((CFMutableDataRef)wrappedUsk.get(), (const UInt8 *)ecPub, (CFIndex)ecPubLen);
		CFDataAppendBytes((CFMutableDataRef)wrappedUsk.get(), (const UInt8 *)scUsk, (CFIndex)scUskLen);
	} else {
		wrappedUsk = CFDataCreate(kCFAllocatorDefault, (const UInt8 *)scUsk, (CFIndex)scUskLen);
	}
	free(scChallenge);
	// decrypt Usk with SC
	CFRef<CFDataRef> unwrappedUsk = SecKeyCreateDecryptedData(privKey,
													mode == AKS_SMARTCARD_MODE_RSA ? kSecKeyAlgorithmRSAEncryptionOAEPSHA256 : kSecKeyAlgorithmECIESEncryptionAKSSmartCard,
													(CFDataRef)wrappedUsk.get(),
													error.take());
	if (!unwrappedUsk) {
		secinfo("TokenLogin", "TokenLoginUnlockKeybag failed to unwrap blob: %@", error.get());
		return errSecInternal;
	}

	void *scNewBlob = NULL;
	size_t scNewLen = 0;
	res = aks_smartcard_unlock(session_keybag_handle, (uint8_t *)CFDataGetBytePtr(scBlob), (size_t)CFDataGetLength(scBlob), (uint8_t *)CFDataGetBytePtr(unwrappedUsk), (size_t)CFDataGetLength(unwrappedUsk), &scNewBlob, &scNewLen);
	if (scNewBlob) {
		CFRef<CFDataRef> newBlobData = CFDataCreate(kCFAllocatorDefault, (const UInt8 *)scNewBlob, (CFIndex)scNewLen);
		free(scNewBlob);
		CFRef<CFMutableDictionaryRef> newDict = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 4, loginData);
		if (newDict) {
			CFDictionarySetValue(newDict, kSecClassKey, newBlobData.get());
			TokenLoginStoreUnlockData(context, newDict);
		}
	}
	return res;
}
