/*
 * Copyright (c) 2002-2010 Apple Inc. All Rights Reserved.
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

#include "SecKey.h"
#include "SecItem.h"
#include "SecItemPriv.h"
#include <security_keychain/KeyItem.h>
#include <CommonCrypto/CommonKeyDerivation.h>

#include "SecBridge.h"

#include <security_keychain/Access.h>
#include <security_keychain/Keychains.h>
#include <security_keychain/KeyItem.h>
#include <syslog.h>

#include <security_cdsa_utils/cuCdsaUtils.h>
#include <security_cdsa_client/wrapkey.h>

#include "SecImportExportCrypto.h"

CFTypeID
SecKeyGetTypeID(void)
{
	BEGIN_SECAPI

	return gTypes().KeyItem.typeID;

	END_SECAPI1(_kCFRuntimeNotATypeID)
}

OSStatus
SecKeyCreatePair(
	SecKeychainRef keychainRef,
	CSSM_ALGORITHMS algorithm,
	uint32 keySizeInBits,
	CSSM_CC_HANDLE contextHandle,
	CSSM_KEYUSE publicKeyUsage,
	uint32 publicKeyAttr,
	CSSM_KEYUSE privateKeyUsage,
	uint32 privateKeyAttr,
	SecAccessRef initialAccess,
	SecKeyRef* publicKeyRef, 
	SecKeyRef* privateKeyRef)
{
	BEGIN_SECAPI

	Keychain keychain = Keychain::optional(keychainRef);
	SecPointer<Access> theAccess(initialAccess ? Access::required(initialAccess) : new Access("<key>"));
	SecPointer<KeyItem> pubItem, privItem;

	KeyItem::createPair(keychain,
        algorithm,
        keySizeInBits,
        contextHandle,
        publicKeyUsage,
        publicKeyAttr,
        privateKeyUsage,
        privateKeyAttr,
        theAccess,
        pubItem,
        privItem);

	// Return the generated keys.
	if (publicKeyRef)
		*publicKeyRef = pubItem->handle();
	if (privateKeyRef)
		*privateKeyRef = privItem->handle();

	END_SECAPI
}

OSStatus
SecKeyGetCSSMKey(SecKeyRef key, const CSSM_KEY **cssmKey)
{
	BEGIN_SECAPI

	Required(cssmKey) = KeyItem::required(key)->key();

	END_SECAPI
}


//
// Private APIs
//

OSStatus
SecKeyGetCSPHandle(SecKeyRef keyRef, CSSM_CSP_HANDLE *cspHandle)
{
    BEGIN_SECAPI

	SecPointer<KeyItem> keyItem(KeyItem::required(keyRef));
	Required(cspHandle) = keyItem->csp()->handle();

	END_SECAPI
}

OSStatus
SecKeyGetAlgorithmID(SecKeyRef keyRef, const CSSM_X509_ALGORITHM_IDENTIFIER **algid)
{
    BEGIN_SECAPI

	SecPointer<KeyItem> keyItem(KeyItem::required(keyRef));
	Required(algid) = &keyItem->algorithmIdentifier();

	END_SECAPI
}

OSStatus
SecKeyGetStrengthInBits(SecKeyRef keyRef, const CSSM_X509_ALGORITHM_IDENTIFIER *algid, unsigned int *strength)
{
    BEGIN_SECAPI

	SecPointer<KeyItem> keyItem(KeyItem::required(keyRef));
	Required(strength) = keyItem->strengthInBits(algid);

	END_SECAPI
}

OSStatus
SecKeyGetCredentials(
	SecKeyRef keyRef,
	CSSM_ACL_AUTHORIZATION_TAG operation,
	SecCredentialType credentialType,
	const CSSM_ACCESS_CREDENTIALS **outCredentials)
{
	BEGIN_SECAPI

	SecPointer<KeyItem> keyItem(KeyItem::required(keyRef));
	Required(outCredentials) = keyItem->getCredentials(operation, credentialType);

	END_SECAPI
}

OSStatus
SecKeyImportPair(
	SecKeychainRef keychainRef,
	const CSSM_KEY *publicCssmKey,
	const CSSM_KEY *privateCssmKey,
	SecAccessRef initialAccess,
	SecKeyRef* publicKey,
	SecKeyRef* privateKey)
{
	BEGIN_SECAPI

	Keychain keychain = Keychain::optional(keychainRef);
	SecPointer<Access> theAccess(initialAccess ? Access::required(initialAccess) : new Access("<key>"));
	SecPointer<KeyItem> pubItem, privItem;

	KeyItem::importPair(keychain,
		Required(publicCssmKey),
		Required(privateCssmKey),
        theAccess,
        pubItem,
        privItem);

	// Return the generated keys.
	if (publicKey)
		*publicKey = pubItem->handle();
	if (privateKey)
		*privateKey = privItem->handle();

	END_SECAPI
}

OSStatus
SecKeyGenerateWithAttributes(
	SecKeychainAttributeList* attrList,
	SecKeychainRef keychainRef,
	CSSM_ALGORITHMS algorithm,
	uint32 keySizeInBits,
	CSSM_CC_HANDLE contextHandle,
	CSSM_KEYUSE keyUsage,
	uint32 keyAttr,
	SecAccessRef initialAccess,
	SecKeyRef* keyRef)
{
	BEGIN_SECAPI

	Keychain keychain;
	SecPointer<Access> theAccess;

	if (keychainRef)
		keychain = KeychainImpl::required(keychainRef);
	if (initialAccess)
		theAccess = Access::required(initialAccess);

	SecPointer<KeyItem> item = KeyItem::generateWithAttributes(attrList,
        keychain,
        algorithm,
        keySizeInBits,
        contextHandle,
        keyUsage,
        keyAttr,
        theAccess);

	// Return the generated key.
	if (keyRef)
		*keyRef = item->handle();

	END_SECAPI
}

OSStatus
SecKeyGenerate(
	SecKeychainRef keychainRef,
	CSSM_ALGORITHMS algorithm,
	uint32 keySizeInBits,
	CSSM_CC_HANDLE contextHandle,
	CSSM_KEYUSE keyUsage,
	uint32 keyAttr,
	SecAccessRef initialAccess,
	SecKeyRef* keyRef)
{
	return SecKeyGenerateWithAttributes(NULL,
		keychainRef, algorithm, keySizeInBits,
		contextHandle, keyUsage, keyAttr,
		initialAccess, keyRef);
}


/* new in 10.6 */
/* Create a key from supplied data and parameters */
SecKeyRef
SecKeyCreate(CFAllocatorRef allocator,
    const SecKeyDescriptor *keyClass,
	const uint8_t *keyData,
	CFIndex keyDataLength,
	SecKeyEncoding encoding)
{
	SecKeyRef keyRef = NULL;
    OSStatus __secapiresult;
	try {
		//FIXME: needs implementation

		__secapiresult=noErr;
	}
	catch (const MacOSError &err) { __secapiresult=err.osStatus(); }
	catch (const CommonError &err) { __secapiresult=SecKeychainErrFromOSStatus(err.osStatus()); }
	catch (const std::bad_alloc &) { __secapiresult=memFullErr; }
	catch (...) { __secapiresult=internalComponentErr; }
	return keyRef;
}

/* new in 10.6 */
/* Generate a floating key reference from a CSSM_KEY */
OSStatus
SecKeyCreateWithCSSMKey(const CSSM_KEY *cssmKey,
    SecKeyRef *keyRef)
{
	BEGIN_SECAPI

	Required(cssmKey);
	CssmClient::CSP csp(cssmKey->KeyHeader.CspId);
	CssmClient::Key key(csp, *cssmKey);
	KeyItem *item = new KeyItem(key);

	// Return the generated key.
	if (keyRef)
		*keyRef = item->handle();

	END_SECAPI
}



static u_int32_t ConvertCFStringToInteger(CFStringRef ref)
{
	if (ref == NULL)
	{
		return 0;
	}
	
	// figure out the size of the string
	int numChars = CFStringGetMaximumSizeForEncoding(CFStringGetLength(ref), kCFStringEncodingUTF8);
	char buffer[numChars];
	if (!CFStringGetCString(ref, buffer, numChars, kCFStringEncodingUTF8))
	{
		MacOSError::throwMe(paramErr);
	}
	
	return atoi(buffer);
}



static OSStatus CheckAlgorithmType(CFDictionaryRef parameters, CSSM_ALGORITHMS &algorithms)
{
	// figure out the algorithm to use
	CFStringRef ktype = (CFStringRef) CFDictionaryGetValue(parameters, kSecAttrKeyType);
	if (ktype == NULL)
	{
		return errSecParam;
	}
	
	if (CFEqual(ktype, kSecAttrKeyTypeRSA)) {
		algorithms = CSSM_ALGID_RSA;
		return noErr;
	} else if(CFEqual(ktype, kSecAttrKeyTypeECDSA)) {
		algorithms = CSSM_ALGID_ECDSA;
		return noErr;
	} else if(CFEqual(ktype, kSecAttrKeyTypeAES)) {
		algorithms = CSSM_ALGID_AES;
		return noErr;
	} else if(CFEqual(ktype, kSecAttrKeyType3DES)) {
		algorithms = CSSM_ALGID_3DES;
		return noErr;
	} else {
		return errSecUnsupportedAlgorithm;
	}
}



static OSStatus GetKeySize(CFDictionaryRef parameters, CSSM_ALGORITHMS algorithms, uint32 &keySizeInBits)
{

    // get the key size and check it for validity
    CFTypeRef ref = CFDictionaryGetValue(parameters, kSecAttrKeySizeInBits);

    keySizeInBits = kSecDefaultKeySize;

    CFTypeID bitSizeType = CFGetTypeID(ref);
    if (bitSizeType == CFStringGetTypeID())
        keySizeInBits = ConvertCFStringToInteger((CFStringRef) ref);
    else if (bitSizeType == CFNumberGetTypeID())
        CFNumberGetValue((CFNumberRef) ref, kCFNumberSInt32Type, &keySizeInBits);
    else return errSecParam;


    switch (algorithms) {
    case CSSM_ALGID_ECDSA:
        if(keySizeInBits == kSecDefaultKeySize) keySizeInBits = kSecp256r1;
        if(keySizeInBits == kSecp192r1 || keySizeInBits == kSecp256r1 || keySizeInBits == kSecp384r1 || keySizeInBits == kSecp521r1 ) return noErr;
        break;
    case CSSM_ALGID_RSA:
			  if(keySizeInBits % 8) return errSecParam;
        if(keySizeInBits == kSecDefaultKeySize) keySizeInBits = 2048;
        if(keySizeInBits >= kSecRSAMin && keySizeInBits <= kSecRSAMax) return noErr;
        break;
    case CSSM_ALGID_AES:
        if(keySizeInBits == kSecDefaultKeySize) keySizeInBits = kSecAES128;
        if(keySizeInBits == kSecAES128 || keySizeInBits == kSecAES192 || keySizeInBits == kSecAES256) return noErr;
        break;
    case CSSM_ALGID_3DES:
        if(keySizeInBits == kSecDefaultKeySize) keySizeInBits = kSec3DES192;
        if(keySizeInBits == kSec3DES192) return noErr;
        break;
    default:
        break;
    }
    return errSecParam;
}



enum AttributeType
{
	kStringType,
	kBooleanType,
	kIntegerType
};



struct ParameterAttribute
{
	const CFTypeRef *name;
	AttributeType type;
};



static ParameterAttribute gAttributes[] =
{
	{
		&kSecAttrLabel,
		kStringType
	},
	{
		&kSecAttrIsPermanent,
		kBooleanType
	},
	{
		&kSecAttrApplicationTag,
		kStringType
	},
	{
		&kSecAttrEffectiveKeySize,
		kBooleanType
	},
	{
		&kSecAttrCanEncrypt,
		kBooleanType
	},
	{
		&kSecAttrCanDecrypt,
		kBooleanType
	},
	{
		&kSecAttrCanDerive,
		kBooleanType
	},
	{
		&kSecAttrCanSign,
		kBooleanType
	},
	{
		&kSecAttrCanVerify,
		kBooleanType
	},
	{
		&kSecAttrCanUnwrap,
		kBooleanType
	}
};

const int kNumberOfAttributes = sizeof(gAttributes) / sizeof(ParameterAttribute);

static OSStatus ScanDictionaryForParameters(CFDictionaryRef parameters, void* attributePointers[])
{
	int i;
	for (i = 0; i < kNumberOfAttributes; ++i)
	{
		// see if the corresponding tag exists in the dictionary
		CFTypeRef value = CFDictionaryGetValue(parameters, *(gAttributes[i].name));
		if (value != NULL)
		{
			switch (gAttributes[i].type)
			{
				case kStringType:
					// just return the value
					*(CFTypeRef*) attributePointers[i] = value;
				break;
				
				case kBooleanType:
				{
					CFBooleanRef bRef = (CFBooleanRef) value;
					*(bool*) attributePointers[i] = CFBooleanGetValue(bRef);
				}
				break;
				
				case kIntegerType:
				{
					CFNumberRef nRef = (CFNumberRef) value;
					CFNumberGetValue(nRef, kCFNumberSInt32Type, attributePointers[i]);
				}
				break;
			}
		}
	}

	return noErr;
}



static OSStatus GetKeyParameters(CFDictionaryRef parameters, int keySize, bool isPublic, CSSM_KEYUSE &keyUse, uint32 &attrs, CFTypeRef &labelRef, CFDataRef &applicationTagRef)
{
	// establish default values
	labelRef = NULL;
	bool isPermanent = false;
	applicationTagRef = NULL;
	CFTypeRef effectiveKeySize = NULL;
	bool canDecrypt = isPublic ? false : true;
	bool canEncrypt = !canDecrypt;
	bool canDerive = true;
	bool canSign = isPublic ? false : true;
	bool canVerify = !canSign;
	bool canUnwrap = isPublic ? false : true;
	attrs = CSSM_KEYATTR_EXTRACTABLE;
	keyUse = 0;
	
	void* attributePointers[] = {&labelRef, &isPermanent, &applicationTagRef, &effectiveKeySize, &canEncrypt, &canDecrypt,
								 &canDerive, &canSign, &canVerify, &canUnwrap};
	
	// look for modifiers in the general dictionary
	OSStatus result = ScanDictionaryForParameters(parameters, attributePointers);
	if (result != noErr)
	{
		return result;
	}
	
	// see if we have anything which modifies the defaults
	CFTypeRef key;
	if (isPublic)
	{
		key = kSecPublicKeyAttrs;
	}
	else
	{
		key = kSecPrivateKeyAttrs;
	}
	
	CFTypeRef dType = CFDictionaryGetValue(parameters, key);
	if (dType != NULL)
	{
		// this had better be a dictionary
		if (CFGetTypeID(dType) != CFDictionaryGetTypeID())
		{
			return errSecParam;
		}
		
		// pull any additional parameters out of this dictionary
		result = ScanDictionaryForParameters(parameters, attributePointers);
		if (result != noErr)
		{
			return result;
		}
	}

	// figure out the key usage
	keyUse = 0;
	if (canDecrypt)
	{
		keyUse |= CSSM_KEYUSE_DECRYPT;
	}
	
	if (canEncrypt)
	{
		keyUse |= CSSM_KEYUSE_ENCRYPT;
	}
	
	if (canDerive)
	{
		keyUse |= CSSM_KEYUSE_DERIVE;
	}
	
	if (canSign)
	{
		keyUse |= CSSM_KEYUSE_SIGN;
	}
	
	if (canVerify)
	{
		keyUse |= CSSM_KEYUSE_VERIFY;
	}
	
	if (canUnwrap)
	{
		keyUse |= CSSM_KEYUSE_UNWRAP;
	}
	
	// public key is always extractable;
	// private key is extractable by default unless explicitly set to false
	CFTypeRef value = NULL;
	if (!isPublic && CFDictionaryGetValueIfPresent(parameters, kSecAttrIsExtractable, (const void **)&value) && value)
	{
		Boolean keyIsExtractable = CFEqual(kCFBooleanTrue, value);
		if (!keyIsExtractable)
			attrs = 0;
	}

	attrs |= CSSM_KEYATTR_PERMANENT;

	return noErr;
}



static OSStatus MakeKeyGenParametersFromDictionary(CFDictionaryRef parameters,
												   CSSM_ALGORITHMS &algorithms,
												   uint32 &keySizeInBits,
												   CSSM_KEYUSE &publicKeyUse,
												   uint32 &publicKeyAttr,
												   CFTypeRef &publicKeyLabelRef,
												   CFDataRef &publicKeyAttributeTagRef,
												   CSSM_KEYUSE &privateKeyUse,
												   uint32 &privateKeyAttr,
												   CFTypeRef &privateKeyLabelRef,
												   CFDataRef &privateKeyAttributeTagRef,
												   SecAccessRef &initialAccess)
{
	OSStatus result;
	
	result = CheckAlgorithmType(parameters, algorithms);
	if (result != noErr)
	{
		return result;
	}
	
	result = GetKeySize(parameters, algorithms, keySizeInBits);
	if (result != noErr)
	{
		return result;
	}

	result = GetKeyParameters(parameters, keySizeInBits, false, privateKeyUse, privateKeyAttr, publicKeyLabelRef, publicKeyAttributeTagRef);
	if (result != noErr)
	{
		return result;
	}
	
	result = GetKeyParameters(parameters, keySizeInBits, true, publicKeyUse, publicKeyAttr, privateKeyLabelRef, privateKeyAttributeTagRef);
	if (result != noErr)
	{
		return result;
	}
	
	if (!CFDictionaryGetValueIfPresent(parameters, kSecAttrAccess, (const void **)&initialAccess))
	{
		initialAccess = NULL;
	}
	else if (SecAccessGetTypeID() != CFGetTypeID(initialAccess))
	{
		return paramErr;
	}
	
	return noErr;
}



static OSStatus SetKeyLabelAndTag(SecKeyRef keyRef, CFTypeRef label, CFDataRef tag)
{	
	int numToModify = 0;
	if (label != NULL)
	{
		numToModify += 1;
	}
	
	if (tag != NULL)
	{
		numToModify += 1;
	}
	
	if (numToModify == 0)
	{
		return noErr;
	}
	
	SecKeychainAttributeList attrList;
	SecKeychainAttribute attributes[numToModify];
	
	int i = 0;
	
	if (label != NULL)
	{
		if (CFStringGetTypeID() == CFGetTypeID(label)) {
			CFStringRef label_string = static_cast<CFStringRef>(label);
			attributes[i].tag = kSecKeyPrintName;
			attributes[i].data = (void*) CFStringGetCStringPtr(label_string, kCFStringEncodingUTF8);
			if (NULL == attributes[i].data) {
				CFIndex buffer_length = CFStringGetMaximumSizeForEncoding(CFStringGetLength(label_string), kCFStringEncodingUTF8);
				attributes[i].data = alloca((size_t)buffer_length);
				if (NULL == attributes[i].data) {
					UnixError::throwMe(ENOMEM);
				}
				if (!CFStringGetCString(label_string, static_cast<char *>(attributes[i].data), buffer_length, kCFStringEncodingUTF8)) {
					MacOSError::throwMe(paramErr);
				}
			}
			attributes[i].length = strlen(static_cast<char *>(attributes[i].data));
		} else if (CFDataGetTypeID() == CFGetTypeID(label)) {
			// 10.6 bug compatibility
			CFDataRef label_data = static_cast<CFDataRef>(label);
			attributes[i].tag = kSecKeyLabel;
			attributes[i].data = (void*) CFDataGetBytePtr(label_data);
			attributes[i].length = CFDataGetLength(label_data);
		} else {
			MacOSError::throwMe(paramErr);
		}
		i++;
	}
	
	if (tag != NULL)
	{
		attributes[i].tag = kSecKeyApplicationTag;
		attributes[i].data = (void*) CFDataGetBytePtr(tag);
		attributes[i].length = CFDataGetLength(tag);
		i++;
	}
	
	attrList.count = numToModify;
	attrList.attr = attributes;

	return SecKeychainItemModifyAttributesAndData((SecKeychainItemRef) keyRef, &attrList, 0, NULL);
}



/* new in 10.6 */
/* Generate a private/public keypair. */
OSStatus
SecKeyGeneratePair(
	CFDictionaryRef parameters,
	SecKeyRef *publicKey,
	SecKeyRef *privateKey)
{
	BEGIN_SECAPI
	
	Required(parameters);
	Required(publicKey);
	Required(privateKey);

	CSSM_ALGORITHMS algorithms;
	uint32 keySizeInBits;
	CSSM_KEYUSE publicKeyUse;
	uint32 publicKeyAttr;
	CFTypeRef publicKeyLabelRef;
	CFDataRef publicKeyAttributeTagRef;
	CSSM_KEYUSE privateKeyUse;
	uint32 privateKeyAttr;
	CFTypeRef privateKeyLabelRef;
	CFDataRef privateKeyAttributeTagRef;
	SecAccessRef initialAccess;
	SecKeychainRef keychain;
	
	OSStatus result = MakeKeyGenParametersFromDictionary(parameters, algorithms, keySizeInBits, publicKeyUse, publicKeyAttr, publicKeyLabelRef,
														 publicKeyAttributeTagRef, privateKeyUse, privateKeyAttr, privateKeyLabelRef, privateKeyAttributeTagRef,
														 initialAccess);
	
	if (result != noErr)
	{
		return result;
	}

	// verify keychain parameter
	keychain = NULL;
	if (!CFDictionaryGetValueIfPresent(parameters, kSecUseKeychain, (const void **)&keychain))
		keychain = NULL;
	else if (SecKeychainGetTypeID() != CFGetTypeID(keychain))
		keychain = NULL;

	// do the key generation
	result = SecKeyCreatePair(keychain, algorithms, keySizeInBits, 0, publicKeyUse, publicKeyAttr, privateKeyUse, privateKeyAttr, initialAccess, publicKey, privateKey);
	if (result != noErr)
	{
		return result;
	}
	
	// set the label and print attributes on the keys
	SetKeyLabelAndTag(*publicKey, publicKeyLabelRef, publicKeyAttributeTagRef);
	SetKeyLabelAndTag(*privateKey, privateKeyLabelRef, privateKeyAttributeTagRef);
	return result;
	
	END_SECAPI
}

/* new in 10.6 */
OSStatus
SecKeyRawSign(
    SecKeyRef           key,
	SecPadding          padding,
	const uint8_t       *dataToSign,
	size_t              dataToSignLen,
	uint8_t             *sig,
	size_t              *sigLen)
{
	BEGIN_SECAPI

	Required(key);
	SecPointer<KeyItem> keyItem(KeyItem::required(key));
	CSSM_DATA dataInput;
	
	dataInput.Data = (uint8_t*) dataToSign;
	dataInput.Length = dataToSignLen;
	
	CSSM_DATA output;
	output.Data = sig;
	output.Length = *sigLen;
	
	const AccessCredentials* credentials = keyItem->getCredentials(CSSM_ACL_AUTHORIZATION_SIGN, kSecCredentialTypeDefault);

	keyItem->RawSign(padding, dataInput, credentials, output);
	*sigLen = output.Length;

	END_SECAPI
}

/* new in 10.6 */
OSStatus
SecKeyRawVerify(
    SecKeyRef           key,
	SecPadding          padding,
	const uint8_t       *signedData,
	size_t              signedDataLen,
	const uint8_t       *sig,
	size_t              sigLen)
{
	BEGIN_SECAPI

	Required(key);
	
	SecPointer<KeyItem> keyItem(KeyItem::required(key));
	CSSM_DATA dataInput;
	
	dataInput.Data = (uint8_t*) signedData;
	dataInput.Length = signedDataLen;
	
	CSSM_DATA signature;
	signature.Data = (uint8_t*) sig;
	signature.Length = sigLen;
	
	const AccessCredentials* credentials = keyItem->getCredentials(CSSM_ACL_AUTHORIZATION_ANY, kSecCredentialTypeDefault);

	keyItem->RawVerify(padding, dataInput, credentials, signature);

	END_SECAPI
}

/* new in 10.6 */
OSStatus
SecKeyEncrypt(
    SecKeyRef           key,
	SecPadding          padding,
	const uint8_t		*plainText,
	size_t              plainTextLen,
	uint8_t             *cipherText,
	size_t              *cipherTextLen)
{
	BEGIN_SECAPI

	SecPointer<KeyItem> keyItem(KeyItem::required(key));
	CSSM_DATA inData, outData;
	inData.Data = (uint8*) plainText;
	inData.Length = plainTextLen;
	outData.Data = cipherText;
	outData.Length = *cipherTextLen;
	
	const AccessCredentials* credentials = keyItem->getCredentials(CSSM_ACL_AUTHORIZATION_SIGN, kSecCredentialTypeDefault);

	keyItem->Encrypt(padding, inData, credentials, outData);
	*cipherTextLen = outData.Length;

	END_SECAPI
}

/* new in 10.6 */
OSStatus
SecKeyDecrypt(
    SecKeyRef           key,                /* Private key */
	SecPadding          padding,			/* kSecPaddingNone, kSecPaddingPKCS1, kSecPaddingOAEP */
	const uint8_t       *cipherText,
	size_t              cipherTextLen,		/* length of cipherText */
	uint8_t             *plainText,	
	size_t              *plainTextLen)		/* IN/OUT */
{
	BEGIN_SECAPI

	SecPointer<KeyItem> keyItem(KeyItem::required(key));
	CSSM_DATA inData, outData;
	inData.Data = (uint8*) cipherText;
	inData.Length = cipherTextLen;
	outData.Data = plainText;
	outData.Length = *plainTextLen;
	
	const AccessCredentials* credentials = keyItem->getCredentials(CSSM_ACL_AUTHORIZATION_SIGN, kSecCredentialTypeDefault);

	keyItem->Decrypt(padding, inData, credentials, outData);
	*plainTextLen = outData.Length;

	END_SECAPI
}

/* new in 10.6 */
size_t
SecKeyGetBlockSize(SecKeyRef key)
{
	size_t blockSize = 0;
    OSStatus __secapiresult;
	try {
		CSSM_KEY cssmKey = KeyItem::required(key)->key();
		blockSize = cssmKey.KeyHeader.LogicalKeySizeInBits;

		__secapiresult=noErr;
	}
	catch (const MacOSError &err) { __secapiresult=err.osStatus(); }
	catch (const CommonError &err) { __secapiresult=SecKeychainErrFromOSStatus(err.osStatus()); }
	catch (const std::bad_alloc &) { __secapiresult=memFullErr; }
	catch (...) { __secapiresult=internalComponentErr; }
	return blockSize;
}


/*
    M4 Additions
*/

static CFTypeRef
utilGetStringFromCFDict(CFDictionaryRef parameters, CFTypeRef key, CFTypeRef defaultValue)
{
		CFTypeRef value = CFDictionaryGetValue(parameters, key);
        if (value != NULL) return value;
        return defaultValue;
}

static uint32_t
utilGetNumberFromCFDict(CFDictionaryRef parameters, CFTypeRef key, uint32_t defaultValue)
{
        uint32_t integerValue;
		CFTypeRef value = CFDictionaryGetValue(parameters, key);
        if (value != NULL) {
            CFNumberRef nRef = (CFNumberRef) value;
            CFNumberGetValue(nRef, kCFNumberSInt32Type, &integerValue);
            return integerValue;
        }
        return defaultValue;
 }

static uint32_t
utilGetMaskValFromCFDict(CFDictionaryRef parameters, CFTypeRef key, uint32_t maskValue)
{
		CFTypeRef value = CFDictionaryGetValue(parameters, key);
        if (value != NULL) {
            CFBooleanRef bRef = (CFBooleanRef) value;
            if(CFBooleanGetValue(bRef)) return maskValue;
        }
        return 0;
}

static void
utilGetKeyParametersFromCFDict(CFDictionaryRef parameters, CSSM_ALGORITHMS *algorithm, uint32 *keySizeInBits, CSSM_KEYUSE *keyUsage, CSSM_KEYCLASS *keyClass)
{
    CFTypeRef algorithmDictValue = utilGetStringFromCFDict(parameters, kSecAttrKeyType, kSecAttrKeyTypeAES);
    CFTypeRef keyClassDictValue = utilGetStringFromCFDict(parameters, kSecAttrKeyClass, kSecAttrKeyClassSymmetric);

    if(CFEqual(algorithmDictValue, kSecAttrKeyTypeAES)) {
        *algorithm = CSSM_ALGID_AES;
        *keySizeInBits = 128;
        *keyClass = CSSM_KEYCLASS_SESSION_KEY;
    } else if(CFEqual(algorithmDictValue, kSecAttrKeyTypeDES)) {
        *algorithm = CSSM_ALGID_DES;
        *keySizeInBits = 128;
         *keyClass = CSSM_KEYCLASS_SESSION_KEY;
    } else if(CFEqual(algorithmDictValue, kSecAttrKeyType3DES)) {
        *algorithm = CSSM_ALGID_3DES_3KEY_EDE;
        *keySizeInBits = 128;
        *keyClass = CSSM_KEYCLASS_SESSION_KEY;
    } else if(CFEqual(algorithmDictValue, kSecAttrKeyTypeRC4)) {
        *algorithm = CSSM_ALGID_RC4;
        *keySizeInBits = 128;
        *keyClass = CSSM_KEYCLASS_SESSION_KEY;
    } else if(CFEqual(algorithmDictValue, kSecAttrKeyTypeRC2)) {
        *algorithm = CSSM_ALGID_RC2;
        *keySizeInBits = 128;
         *keyClass = CSSM_KEYCLASS_SESSION_KEY;
    } else if(CFEqual(algorithmDictValue, kSecAttrKeyTypeCAST)) {
        *algorithm = CSSM_ALGID_CAST;
        *keySizeInBits = 128;
         *keyClass = CSSM_KEYCLASS_SESSION_KEY;
    } else if(CFEqual(algorithmDictValue, kSecAttrKeyTypeRSA)) {
        *algorithm = CSSM_ALGID_RSA;
        *keySizeInBits = 128;
         *keyClass = CSSM_KEYCLASS_PRIVATE_KEY;
    } else if(CFEqual(algorithmDictValue, kSecAttrKeyTypeDSA)) {
        *algorithm = CSSM_ALGID_DSA;
        *keySizeInBits = 128;
         *keyClass = CSSM_KEYCLASS_PRIVATE_KEY;
    } else if(CFEqual(algorithmDictValue, kSecAttrKeyTypeECDSA)) {
        *algorithm = CSSM_ALGID_ECDSA;
        *keySizeInBits = 128;
        *keyClass = CSSM_KEYCLASS_PRIVATE_KEY;
    } else {
        *algorithm = CSSM_ALGID_AES;
        *keySizeInBits = 128;
        *keyClass = CSSM_KEYCLASS_SESSION_KEY;
    }
    
    if(CFEqual(keyClassDictValue, kSecAttrKeyClassPublic)) {
        *keyClass = CSSM_KEYCLASS_PUBLIC_KEY;
    } else if(CFEqual(keyClassDictValue, kSecAttrKeyClassPrivate)) {
        *keyClass = CSSM_KEYCLASS_PRIVATE_KEY;
    } else if(CFEqual(keyClassDictValue, kSecAttrKeyClassSymmetric)) {
         *keyClass = CSSM_KEYCLASS_SESSION_KEY;
    }

    *keySizeInBits = utilGetNumberFromCFDict(parameters, kSecAttrKeySizeInBits, *keySizeInBits);
    *keyUsage =  utilGetMaskValFromCFDict(parameters, kSecAttrCanEncrypt, CSSM_KEYUSE_ENCRYPT) |
                utilGetMaskValFromCFDict(parameters, kSecAttrCanDecrypt, CSSM_KEYUSE_DECRYPT) |
                utilGetMaskValFromCFDict(parameters, kSecAttrCanWrap, CSSM_KEYUSE_WRAP) |
                utilGetMaskValFromCFDict(parameters, kSecAttrCanUnwrap, CSSM_KEYUSE_UNWRAP);

	
    if(*keyClass == CSSM_KEYCLASS_PRIVATE_KEY || *keyClass == CSSM_KEYCLASS_PUBLIC_KEY) {
		*keyUsage |=  utilGetMaskValFromCFDict(parameters, kSecAttrCanSign, CSSM_KEYUSE_SIGN) |
					utilGetMaskValFromCFDict(parameters, kSecAttrCanVerify, CSSM_KEYUSE_VERIFY);
    }

    if(*keyUsage == 0) {
		switch (*keyClass) {
			case CSSM_KEYCLASS_PRIVATE_KEY:
				*keyUsage = CSSM_KEYUSE_DECRYPT | CSSM_KEYUSE_UNWRAP | CSSM_KEYUSE_SIGN;
				break;
			case CSSM_KEYCLASS_PUBLIC_KEY:
				*keyUsage = CSSM_KEYUSE_ENCRYPT | CSSM_KEYUSE_VERIFY | CSSM_KEYUSE_WRAP;
				break;
			default:
				*keyUsage = CSSM_KEYUSE_ENCRYPT | CSSM_KEYUSE_DECRYPT | CSSM_KEYUSE_WRAP | CSSM_KEYUSE_UNWRAP | CSSM_KEYUSE_SIGN | CSSM_KEYUSE_VERIFY;
				break;
		}
	}
}

static CFStringRef
utilCopyDefaultKeyLabel(void)
{
	// generate a default label from the current date
	CFDateRef dateNow = CFDateCreate(kCFAllocatorDefault, CFAbsoluteTimeGetCurrent());
	CFStringRef defaultLabel = CFCopyDescription(dateNow);
	CFRelease(dateNow);

	return defaultLabel;
}

SecKeyRef
SecKeyGenerateSymmetric(CFDictionaryRef parameters, CFErrorRef *error)
{
	OSStatus result = paramErr; // default result for an early exit
	SecKeyRef key = NULL;
	SecKeychainRef keychain = NULL;
	SecAccessRef access;
	CFStringRef label;
	CFStringRef appLabel;
	CFStringRef appTag;
	CFStringRef dateLabel = NULL;
	
	CSSM_ALGORITHMS algorithm;
	uint32 keySizeInBits;
	CSSM_KEYUSE keyUsage;
	uint32 keyAttr = CSSM_KEYATTR_RETURN_DEFAULT;
	CSSM_KEYCLASS keyClass;
	CFTypeRef value;
	Boolean isPermanent;
	
	// verify keychain parameter
	if (!CFDictionaryGetValueIfPresent(parameters, kSecUseKeychain, (const void **)&keychain))
		keychain = NULL;
	else if (SecKeychainGetTypeID() != CFGetTypeID(keychain)) {
		keychain = NULL;
		goto errorExit;
	}
	else
		CFRetain(keychain);

	// verify permanent parameter
	isPermanent = CFDictionaryGetValueIfPresent(parameters, kSecAttrIsPermanent, (const void **)&value) && value && CFEqual(kCFBooleanTrue, value);
	if (isPermanent && keychain == NULL) {
		// no keychain was specified, so use the default keychain
		result = SecKeychainCopyDefault(&keychain);
	}

	// verify access parameter
	if (!CFDictionaryGetValueIfPresent(parameters, kSecAttrAccess, (const void **)&access))
		access = NULL;
	else if (SecAccessGetTypeID() != CFGetTypeID(access))
		goto errorExit;

	// verify label parameter
	if (!CFDictionaryGetValueIfPresent(parameters, kSecAttrLabel, (const void **)&label))
		label = (dateLabel = utilCopyDefaultKeyLabel()); // no label provided, so use default
	else if (CFStringGetTypeID() != CFGetTypeID(label))
		goto errorExit;

	// verify application label parameter
	if (!CFDictionaryGetValueIfPresent(parameters, kSecAttrApplicationLabel, (const void **)&appLabel))
		appLabel = (dateLabel) ? dateLabel : (dateLabel = utilCopyDefaultKeyLabel());
	else if (CFStringGetTypeID() != CFGetTypeID(appLabel))
		goto errorExit;

	// verify application tag parameter
	if (!CFDictionaryGetValueIfPresent(parameters, kSecAttrApplicationTag, (const void **)&appTag))
		appTag = NULL;
	else if (CFStringGetTypeID() != CFGetTypeID(appTag))
		goto errorExit;

    utilGetKeyParametersFromCFDict(parameters, &algorithm, &keySizeInBits, &keyUsage, &keyClass);

	if (!keychain) {
		// the generated key will not be stored in any keychain
		result = SecKeyGenerate(keychain, algorithm, keySizeInBits, 0, keyUsage, keyAttr, access, &key);
	}
	else {
		// we can set the label attributes on the generated key if it's a keychain item
		size_t labelBufLen = (label) ? (size_t)CFStringGetMaximumSizeForEncoding(CFStringGetLength(label), kCFStringEncodingUTF8) + 1 : 0;
		char *labelBuf = (char *)malloc(labelBufLen);
		size_t appLabelBufLen = (appLabel) ? (size_t)CFStringGetMaximumSizeForEncoding(CFStringGetLength(appLabel), kCFStringEncodingUTF8) + 1 : 0;
		char *appLabelBuf = (char *)malloc(appLabelBufLen);
		size_t appTagBufLen = (appTag) ? (size_t)CFStringGetMaximumSizeForEncoding(CFStringGetLength(appTag), kCFStringEncodingUTF8) + 1 : 0;
		char *appTagBuf = (char *)malloc(appTagBufLen);

		if (label && !CFStringGetCString(label, labelBuf, labelBufLen-1, kCFStringEncodingUTF8))
			labelBuf[0]=0;
		if (appLabel && !CFStringGetCString(appLabel, appLabelBuf, appLabelBufLen-1, kCFStringEncodingUTF8))
			appLabelBuf[0]=0;
		if (appTag && !CFStringGetCString(appTag, appTagBuf, appTagBufLen-1, kCFStringEncodingUTF8))
			appTagBuf[0]=0;
		
		SecKeychainAttribute attrs[] = {
			{ kSecKeyPrintName, strlen(labelBuf), (char *)labelBuf },
			{ kSecKeyLabel, strlen(appLabelBuf), (char *)appLabelBuf },
			{ kSecKeyApplicationTag, strlen(appTagBuf), (char *)appTagBuf }	};
		SecKeychainAttributeList attributes = { sizeof(attrs) / sizeof(attrs[0]), attrs };
		if (!appTag) --attributes.count;
	
		result = SecKeyGenerateWithAttributes(&attributes,
			keychain, algorithm, keySizeInBits, 0,
			keyUsage, keyAttr, access, &key);

		free(labelBuf);
		free(appLabelBuf);
		free(appTagBuf);
	}

errorExit:
	if (result && error) {
		*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainOSStatus, result, NULL);
	}
	if (dateLabel)
		CFRelease(dateLabel);
	if (keychain)
		CFRelease(keychain);

    return key;
}



SecKeyRef
SecKeyCreateFromData(CFDictionaryRef parameters, CFDataRef keyData, CFErrorRef *error)
{
	CSSM_ALGORITHMS		algorithm;
    uint32				keySizeInBits;
    CSSM_KEYUSE			keyUsage;
    CSSM_KEYCLASS		keyClass;
    CSSM_RETURN			crtn;
	
    utilGetKeyParametersFromCFDict(parameters, &algorithm, &keySizeInBits, &keyUsage, &keyClass);
	
	CSSM_CSP_HANDLE cspHandle = cuCspStartup(CSSM_FALSE); // TRUE => CSP, FALSE => CSPDL
	
	SecKeyImportExportParameters iparam;
	memset(&iparam, 0, sizeof(iparam));
	iparam.keyUsage = keyUsage;
	
	SecExternalItemType itype;
	switch (keyClass) {
		case CSSM_KEYCLASS_PRIVATE_KEY:
			itype = kSecItemTypePrivateKey;
			break;
		case CSSM_KEYCLASS_PUBLIC_KEY:
			itype = kSecItemTypePublicKey;
			break;
		case CSSM_KEYCLASS_SESSION_KEY:
			itype = kSecItemTypeSessionKey;
			break;
		default:
			itype = kSecItemTypeUnknown;
			break;
	}
	
	CFMutableArrayRef ka = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	// NOTE: if we had a way to specify values other then kSecFormatUnknown we might be more useful.
	crtn = impExpImportRawKey(keyData, kSecFormatUnknown, itype, algorithm, NULL, cspHandle, 0, NULL, NULL, ka);
	if (crtn == CSSM_OK && CFArrayGetCount((CFArrayRef)ka)) {
		SecKeyRef sk = (SecKeyRef)CFArrayGetValueAtIndex((CFArrayRef)ka, 0);
		CFRetain(sk);
		CFRelease(ka);
		return sk;
	} else {
		if (error) {
			*error = CFErrorCreate(NULL, kCFErrorDomainOSStatus, crtn ? crtn : CSSM_ERRCODE_INTERNAL_ERROR, NULL);
		}
		return NULL;
	}
}


void 
SecKeyGeneratePairAsync(CFDictionaryRef parametersWhichMightBeMutiable, dispatch_queue_t deliveryQueue, 
						SecKeyGeneratePairBlock result)
{
	CFDictionaryRef parameters = CFDictionaryCreateCopy(NULL, parametersWhichMightBeMutiable);
	dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
		SecKeyRef publicKey = NULL;
		SecKeyRef privateKey = NULL;
		OSStatus status = SecKeyGeneratePair(parameters, &publicKey, &privateKey);
		dispatch_async(deliveryQueue, ^{
			CFErrorRef error = NULL;
			if (noErr != status) {
				error = CFErrorCreate(NULL, kCFErrorDomainOSStatus, status, NULL);
			}
			result(publicKey, privateKey, error);
			if (error) {
				CFRelease(error);
			}
			if (publicKey) {
				CFRelease(publicKey);
			}
			if (privateKey) {
				CFRelease(privateKey);
			}
			CFRelease(parameters);
		});
	});
}

SecKeyRef
SecKeyDeriveFromPassword(CFStringRef password, CFDictionaryRef parameters, CFErrorRef *error)
{
    char *thePassword = NULL;
    CFIndex passwordLen;
    uint8_t *salt = NULL;
    size_t saltLen;
    CCPBKDFAlgorithm algorithm;
    uint rounds;
    uint8_t *derivedKey = NULL;
    size_t derivedKeyLen;
    CFDataRef saltDictValue, algorithmDictValue;

    /* Pick Values from parameters */
    
    if((saltDictValue = (CFDataRef) CFDictionaryGetValue(parameters, kSecAttrSalt)) == NULL) {
        *error = CFErrorCreate(NULL, kCFErrorDomainOSStatus, errSecMissingAlgorithmParms, NULL);
        return NULL;
    } 
    
    derivedKeyLen = utilGetNumberFromCFDict(parameters, kSecAttrKeySizeInBits, 128);
	// This value come in bits but the rest of the code treats it as bytes
	derivedKeyLen /= 8;

    algorithmDictValue = (CFDataRef) utilGetStringFromCFDict(parameters, kSecAttrPRF, kSecAttrPRFHmacAlgSHA256);
    
    rounds = utilGetNumberFromCFDict(parameters, kSecAttrRounds, 0);

    /* Convert any remaining parameters and get the password bytes */
    
    saltLen = CFDataGetLength(saltDictValue);
    if((salt = (uint8_t *) malloc(saltLen)) == NULL) {
        *error = CFErrorCreate(NULL, kCFErrorDomainOSStatus, errSecAllocate, NULL);
        return NULL;
    }
    
    CFDataGetBytes(saltDictValue, CFRangeMake(0, saltLen), (UInt8 *) salt);

    passwordLen = CFStringGetMaximumSizeForEncoding(CFStringGetLength(password), kCFStringEncodingUTF8) + 1;
    if((thePassword = (char *) malloc(passwordLen)) == NULL) {
        free(salt);
        *error = CFErrorCreate(NULL, kCFErrorDomainOSStatus, errSecAllocate, NULL);
        return NULL;
    }
    CFStringGetBytes(password, CFRangeMake(0, CFStringGetLength(password)), kCFStringEncodingUTF8, '?', FALSE, (UInt8*)thePassword, passwordLen, &passwordLen);
    
    if((derivedKey = (uint8_t *) malloc(derivedKeyLen)) == NULL) {
        free(salt);
        bzero(thePassword, strlen(thePassword));
        free(thePassword);
        *error = CFErrorCreate(NULL, kCFErrorDomainOSStatus, errSecAllocate, NULL);
        return NULL;
    }
    
  
    if(algorithmDictValue == NULL) {
        algorithm = kCCPRFHmacAlgSHA1; /* default */
    } else if(CFEqual(algorithmDictValue, kSecAttrPRFHmacAlgSHA1)) {
        algorithm = kCCPRFHmacAlgSHA1;
    } else if(CFEqual(algorithmDictValue, kSecAttrPRFHmacAlgSHA224)) {
        algorithm = kCCPRFHmacAlgSHA224;
    } else if(CFEqual(algorithmDictValue, kSecAttrPRFHmacAlgSHA256)) {
        algorithm = kCCPRFHmacAlgSHA256;
    } else if(CFEqual(algorithmDictValue, kSecAttrPRFHmacAlgSHA384)) {
        algorithm = kCCPRFHmacAlgSHA384;
    } else if(CFEqual(algorithmDictValue, kSecAttrPRFHmacAlgSHA512)) {
        algorithm = kCCPRFHmacAlgSHA512;
    }
    
    if(rounds == 0) {
        rounds = 33333; // we need to pass back a consistent value since there's no way to record the round count.
    }
        

    if(CCKeyDerivationPBKDF(kCCPBKDF2, thePassword, passwordLen, salt, saltLen, algorithm, rounds, derivedKey, derivedKeyLen)) {
        *error = CFErrorCreate(NULL, kCFErrorDomainOSStatus, errSecInternalError, NULL);
        return NULL;
    }
    
    free(salt);
    bzero(thePassword, strlen(thePassword));
    free(thePassword);
    
    CFDataRef keyData = CFDataCreate(NULL, derivedKey, derivedKeyLen);
    bzero(derivedKey, derivedKeyLen);
    free(derivedKey);

    SecKeyRef retval =  SecKeyCreateFromData(parameters, keyData, error);
    return retval;

}

CFDataRef
SecKeyWrapSymmetric(SecKeyRef keyToWrap, SecKeyRef wrappingKey, CFDictionaryRef parameters, CFErrorRef *error)
{
    *error = CFErrorCreate(NULL, kCFErrorDomainOSStatus, errSecUnimplemented, NULL);
    return NULL;
}

SecKeyRef
SecKeyUnwrapSymmetric(CFDataRef *keyToUnwrap, SecKeyRef unwrappingKey, CFDictionaryRef parameters, CFErrorRef *error)
{
    *error = CFErrorCreate(NULL, kCFErrorDomainOSStatus, errSecUnimplemented, NULL);
    return NULL;
}



