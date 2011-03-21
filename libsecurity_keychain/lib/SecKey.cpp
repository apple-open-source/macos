/*
 * Copyright (c) 2002-2008 Apple, Inc. All Rights Reserved.
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

#include <Security/SecKey.h>
#include <Security/SecItem.h>
#include <security_keychain/KeyItem.h>

#include "SecBridge.h"

#include <security_keychain/Access.h>
#include <security_keychain/Keychains.h>
#include <security_keychain/KeyItem.h>

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
	BEGIN_SECAPI

	Keychain keychain;
	SecPointer<Access> theAccess;

	if (keychainRef)
		keychain = KeychainImpl::required(keychainRef);
	if (initialAccess)
		theAccess = Access::required(initialAccess);

	SecPointer<KeyItem> item = KeyItem::generate(keychain,
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
	CFStringGetCString(ref, buffer, numChars, kCFStringEncodingUTF8);
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
	
	if (CFEqual(ktype, kSecAttrKeyTypeRSA))
	{
		algorithms = CSSM_ALGID_RSA;
		return noErr;
	}
	else
	{
		return errSecUnsupportedAlgorithm;
	}
}



static OSStatus GetKeySize(CFDictionaryRef parameters, uint32 &keySizeInBits)
{
	// get the key size and check it for validity
	CFTypeRef ref = CFDictionaryGetValue(parameters, kSecAttrKeySizeInBits);
	
	CFTypeID bitSizeType = CFGetTypeID(ref);
	if (bitSizeType == CFStringGetTypeID())
	{
		// it's a string, so we will have to convert the value
		keySizeInBits = ConvertCFStringToInteger((CFStringRef) ref);
	}
	else if (bitSizeType == CFNumberGetTypeID())
	{
		CFNumberGetValue((CFNumberRef) ref, kCFNumberSInt32Type, &keySizeInBits);
	}
	
	switch (keySizeInBits)
	{
		case 512:
		case 768:
		case 1024:
		case 2048:
			return noErr;
		
		default:
			return errSecParam;
	}
}



enum AttributeType
{
	kStringType,
	kBooleanType,
	kIntegerType
};



struct ParameterAttribute
{
	CFTypeRef name;
	AttributeType type;
};



static ParameterAttribute *gAttributes = NULL;
#define NUMBER_OF_TABLE_ATTRIBUTES 10

static void InitializeAttributes()
{
	gAttributes = new ParameterAttribute[NUMBER_OF_TABLE_ATTRIBUTES];
	
	gAttributes[0].name = kSecAttrLabel;
	gAttributes[0].type = kStringType;
	gAttributes[1].name = kSecAttrIsPermanent;
	gAttributes[1].type = kBooleanType;
	gAttributes[2].name = kSecAttrApplicationTag;
	gAttributes[2].type = kStringType;
	gAttributes[3].name = kSecAttrEffectiveKeySize;
	gAttributes[3].type = kBooleanType;
	gAttributes[4].name = kSecAttrCanEncrypt;
	gAttributes[4].type = kBooleanType;
	gAttributes[5].name = kSecAttrCanDecrypt;
	gAttributes[5].type = kBooleanType;
	gAttributes[6].name = kSecAttrCanDerive;
	gAttributes[6].type = kBooleanType;
	gAttributes[7].name = kSecAttrCanSign;
	gAttributes[7].type = kBooleanType;
	gAttributes[8].name = kSecAttrCanVerify;
	gAttributes[8].type = kBooleanType;
	gAttributes[9].name = kSecAttrCanUnwrap;
	gAttributes[9].type = kBooleanType;
}

static OSStatus ScanDictionaryForParameters(CFDictionaryRef parameters, void* attributePointers[])
{
	static OSSpinLock lock = OS_SPINLOCK_INIT;
	OSSpinLockLock(&lock);
	if (gAttributes == NULL)
	{
		InitializeAttributes();
	}
	OSSpinLockUnlock(&lock);

	int i;
	for (i = 0; i < NUMBER_OF_TABLE_ATTRIBUTES; ++i)
	{
		// see if the cooresponding tag exists in the dictionary
		CFTypeRef value = CFDictionaryGetValue(parameters, gAttributes[i].name);
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



static OSStatus GetKeyParameters(CFDictionaryRef parameters, int keySize, bool isPublic, CSSM_KEYUSE &keyUse, uint32 &attrs, CFDataRef &labelRef, CFDataRef &applicationTagRef)
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
	attrs = isPublic ? CSSM_KEYATTR_EXTRACTABLE : 0;
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
	
	attrs |= CSSM_KEYATTR_PERMANENT;

	return noErr;
}



static OSStatus MakeKeyGenParametersFromDictionary(CFDictionaryRef parameters,
												   CSSM_ALGORITHMS &algorithms,
												   uint32 &keySizeInBits,
												   CSSM_KEYUSE &publicKeyUse,
												   uint32 &publicKeyAttr,
												   CFDataRef &publicKeyLabelRef,
												   CFDataRef &publicKeyAttributeTagRef,
												   CSSM_KEYUSE &privateKeyUse,
												   uint32 &privateKeyAttr,
												   CFDataRef &privateKeyLabelRef,
												   CFDataRef &privateKeyAttributeTagRef)
{
	OSStatus result;
	
	result = CheckAlgorithmType(parameters, algorithms);
	if (result != noErr)
	{
		return result;
	}
	
	result = GetKeySize(parameters, keySizeInBits);
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
	
	return noErr;
}



static OSStatus SetKeyLabelAndTag(SecKeyRef keyRef, CFDataRef label, CFDataRef tag)
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
		attributes[i].tag = kSecKeyLabel;
		attributes[i].data = (void*) CFDataGetBytePtr(label);
		attributes[i].length = CFDataGetLength(label);
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
	Required(parameters);
	Required(publicKey);
	Required(privateKey);

	CSSM_ALGORITHMS algorithms;
	uint32 keySizeInBits;
	CSSM_KEYUSE publicKeyUse;
	uint32 publicKeyAttr;
	CFDataRef publicKeyLabelRef;
	CFDataRef publicKeyAttributeTagRef;
	CSSM_KEYUSE privateKeyUse;
	uint32 privateKeyAttr;
	CFDataRef privateKeyLabelRef;
	CFDataRef privateKeyAttributeTagRef;
	
	OSStatus result = MakeKeyGenParametersFromDictionary(parameters, algorithms, keySizeInBits, publicKeyUse, publicKeyAttr, publicKeyLabelRef,
														 publicKeyAttributeTagRef, privateKeyUse, privateKeyAttr, privateKeyLabelRef, privateKeyAttributeTagRef);
	
	if (result != noErr)
	{
		return result;
	}
	
	// finally, do the key generation
	result = SecKeyCreatePair(NULL, algorithms, keySizeInBits, 0, publicKeyUse, publicKeyAttr, privateKeyUse, privateKeyAttr, NULL, publicKey, privateKey);
	
	// set the label and print attributes on the keys
	SetKeyLabelAndTag(*publicKey, publicKeyLabelRef, publicKeyAttributeTagRef);
	SetKeyLabelAndTag(*privateKey, privateKeyLabelRef, privateKeyAttributeTagRef);
	return result;
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
	
	keyItem->RawSign(padding, dataInput, output);
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
	
	CSSM_DATA output;
	output.Data = (uint8_t*) sig;
	output.Length = sigLen;
	
	keyItem->RawVerify(padding, dataInput, output);

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
	
	keyItem->Encrypt(padding, inData, outData);
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
	
	keyItem->Decrypt(padding, inData, outData);
	*plainTextLen = outData.Length;

	END_SECAPI
}

/* new in 10.6 */
static OSStatus SecKeyGetBlockSizeInternal(SecKeyRef key, size_t &blockSize)
{
	BEGIN_SECAPI
	
	CSSM_KEY cssmKey = KeyItem::required(key)->key();
	blockSize = cssmKey.KeyHeader.LogicalKeySizeInBits;
	
	END_SECAPI
}



size_t
SecKeyGetBlockSize(SecKeyRef key)
{
	size_t blockSize = 0;
	SecKeyGetBlockSizeInternal(key, blockSize);
	return blockSize;
}

