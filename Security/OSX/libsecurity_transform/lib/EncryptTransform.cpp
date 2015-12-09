/*
 * Copyright (c) 2010-2014 Apple Inc. All Rights Reserved.
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


#include "EncryptTransform.h"
#include "SecEncryptTransform.h"
#include "EncryptTransformUtilities.h"
#include "Utilities.h"
#include "SecDigestTransform.h"
#include "Digest.h"
#include <Security/SecRandomP.h>
#include <Security/SecKey.h>
#include "SecMaskGenerationFunctionTransform.h"

static CFStringRef kEncryptTransformType = CFSTR("Encrypt Transform");
static CFStringRef kDecryptTransformType = CFSTR("Decrypt Transform");
//static const char *kEncryptTransformType_cstr = "Encrypt Transform";
//static const char *kDecryptTransformType_cstr = "Decrypt Transform";
static uint8 iv[16] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
static const CSSM_DATA gKeySalt = {16, iv}; // default Salt for key

dispatch_once_t EncryptDecryptBase::serializerSetUp;
dispatch_queue_t EncryptDecryptBase::serializerTransformStartingExecution;

/* --------------------------------------------------------------------------
 Implementation of the EncryptDecryptBase class
 -------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------
 method: 		EncryptDecryptBase (Constructor)
 description: 	Initialize a new instance of a EncryptDecryptBase class
 -------------------------------------------------------------------------- */
EncryptDecryptBase::EncryptDecryptBase(CFStringRef type) :
Transform(type),
m_cssm_padding(CSSM_PADDING_NONE),
m_mode(CSSM_ALGMODE_CBCPadIV8),
m_cssm_key(NULL),
m_handle((CSSM_CC_HANDLE)0),
m_forEncryption(FALSE),
m_processedData(NULL),
m_accumulator(NULL)
{
	m_forEncryption = CFEqual(type, kEncryptTransformType);
    inputAH = transforms_assume(this->getAH(kSecTransformInputAttributeName, false, false));
}

/* --------------------------------------------------------------------------
 method: 		~EncryptDecryptBase (pre-Destructor)
 description: 	Clean m_handle, let Transform::Finalize() do the rest
 -------------------------------------------------------------------------- */
void EncryptDecryptBase::Finalize()
{
	if (m_handle != (CSSM_CC_HANDLE)0)
	{
		CSSM_CC_HANDLE tmp_handle = m_handle;
		// Leaving this to the destructor causes occasional crashes.
		// This may be a CDSA thread afinity bug, or it might be more
		// local.
		dispatch_async(mDispatchQueue, ^{
			CSSM_DeleteContext(tmp_handle);
		});
		m_handle = ((CSSM_CC_HANDLE)0);
	}

	Transform::Finalize();
}


/* --------------------------------------------------------------------------
 method: 		~EncryptDecryptBase (Destructor)
 description: 	Clean up the memory of an EncryptDecryptBase object
 -------------------------------------------------------------------------- */
EncryptDecryptBase::~EncryptDecryptBase()
{
	if (NULL != m_processedData)
	{
		CFRelease(m_processedData);
		m_processedData = NULL;
	}
	if (NULL != m_accumulator)
	{
		CFRelease(m_accumulator);
		m_accumulator = NULL;
	}
}

/* --------------------------------------------------------------------------
 method: 		InitializeObject(SecKeyRef key, CFErrorRef *error)
 description: 	Initialize an instance of the base encrypt/decrypt transform
 -------------------------------------------------------------------------- */
bool EncryptDecryptBase::InitializeObject(SecKeyRef key, CFErrorRef *error)
{
	SetAttributeNoCallback(kSecEncryptKey, key);
	if (error)
	{
		*error = NULL;
	}

	return true;
}

/* --------------------------------------------------------------------------
 method: 		SerializedTransformStartingExecution()
 description: 	Get this transform ready to run, should only be called on
				the serializerTransformStartingExecution queue
 -------------------------------------------------------------------------- */
CFErrorRef EncryptDecryptBase::SerializedTransformStartingExecution()
{
	CFErrorRef result = NULL;	// Assume all is well
	SecKeyRef key = (SecKeyRef) GetAttribute(kSecEncryptKey);
	if (NULL == key)
	{
		return CreateSecTransformErrorRef(kSecTransformErrorAttributeNotFound, "The attribute %@ was not found.", kSecEncryptKey);
	}

	OSStatus err = errSecSuccess;
	err = SecKeyGetCSSMKey(key, (const CSSM_KEY **)&m_cssm_key);
	if (errSecSuccess != err)
	{
		CFStringRef result = SecCopyErrorMessageString(err, NULL);
		CFErrorRef retValue = CreateSecTransformErrorRef(err, "CDSA error (%@).", result);
		CFRelease(result);
		return retValue;
	}

	CSSM_CSP_HANDLE csp;
	err = SecKeyGetCSPHandle(key, &csp);
	if (errSecSuccess != err)
	{
		CFStringRef result = SecCopyErrorMessageString(err, NULL);
		CFErrorRef retValue = CreateSecTransformErrorRef(err, "CDSA error (%@).", result);
		CFRelease(result);
		return retValue;
	}

	CSSM_ALGORITHMS	keyAlg = m_cssm_key->KeyHeader.AlgorithmId;

	m_cssm_padding = CSSM_PADDING_NONE;
	CFStringRef paddingStr = (CFStringRef) GetAttribute(kSecPaddingKey);
	CFStringRef modeStr = (CFStringRef) GetAttribute (kSecEncryptionMode);
	CFDataRef ivData = (CFDataRef) GetAttribute(kSecIVKey);

	Boolean hasPadding = (paddingStr != NULL);
	Boolean hasMode = (modeStr != NULL);
	Boolean hasIVData = (ivData != NULL);
	Boolean isSymmetrical = (m_cssm_key->KeyHeader.KeyClass == CSSM_KEYCLASS_SESSION_KEY);


    if (!hasPadding)
	{
		if (CSSM_ALGID_RSA == keyAlg || CSSM_ALGID_ECDSA == keyAlg)
		{
			m_cssm_padding = CSSM_PADDING_PKCS1;
		}
		else
		{
			m_cssm_padding = CSSM_PADDING_PKCS7;
		}
		m_oaep_padding = false;
	}
	else
	{
		if (CFStringCompare(kSecPaddingOAEPKey, paddingStr, kCFCompareAnchored)) {
			m_oaep_padding = false;
			m_cssm_padding = ConvertPaddingStringToEnum(paddingStr);
		} else {
			m_cssm_padding = CSSM_PADDING_NONE;
			m_oaep_padding = true;
			m_accumulator = CFDataCreateMutable(NULL, 0);
			if (!m_accumulator) {
				return GetNoMemoryErrorAndRetain();
			}
		}
	}

	if (!hasMode)
	{
		m_mode = (CSSM_PADDING_NONE == m_cssm_padding) ? CSSM_ALGMODE_CBC_IV8 : CSSM_ALGMODE_CBCPadIV8;
	}
	else
	{
		m_mode = ConvertEncryptModeStringToEnum(modeStr, (CSSM_PADDING_NONE != m_cssm_padding));
	}


	CSSM_RETURN crtn = CSSM_OK;
	CSSM_ACCESS_CREDENTIALS	creds;
	CSSM_ACCESS_CREDENTIALS* credPtr = NULL;
	memset(&creds, 0, sizeof(CSSM_ACCESS_CREDENTIALS));

	err = SecKeyGetCredentials(key,
	                           (m_forEncryption) ? CSSM_ACL_AUTHORIZATION_ENCRYPT : CSSM_ACL_AUTHORIZATION_DECRYPT,
	                           kSecCredentialTypeDefault,
	                           (const CSSM_ACCESS_CREDENTIALS **)&credPtr);
	if (errSecSuccess != err)
	{
		memset(&creds, 0, sizeof(CSSM_ACCESS_CREDENTIALS));
		credPtr = &creds;
	}

	if (isSymmetrical)
	{
		CSSM_DATA initVector;
		if (hasIVData)
		{
			initVector.Length = CFDataGetLength(ivData);
			initVector.Data = const_cast<uint8_t*>(CFDataGetBytePtr(ivData));
		}
		else
		{
			initVector.Length = gKeySalt.Length;
			initVector.Data = (uint8 *)malloc(initVector.Length);
			initVector.Data = gKeySalt.Data;
		}

		crtn = CSSM_CSP_CreateSymmetricContext(csp, keyAlg, m_mode, credPtr, m_cssm_key,
		                                       &initVector, m_cssm_padding, NULL, &m_handle);

		// Need better error here
		if (crtn != CSSM_OK)
		{
			CFStringRef result = SecCopyErrorMessageString(crtn, NULL);
			CFErrorRef retValue = CreateSecTransformErrorRef(kSecTransformErrorNotInitializedCorrectly, "CDSA error (%@).", result);
			CFRelease(result);
			return retValue;
		}
	}
	else
	{
		crtn = CSSM_CSP_CreateAsymmetricContext(csp, keyAlg, credPtr, m_cssm_key, m_cssm_padding, &m_handle);

		// Need better error here
		if (crtn != CSSM_OK)
		{
			CFStringRef result = SecCopyErrorMessageString(crtn, NULL);
			CFErrorRef retValue = CreateSecTransformErrorRef(kSecTransformErrorNotInitializedCorrectly, "CDSA error (%@).", result);
			CFRelease(result);
			return retValue;
		}
	}

	// Encryption
	crtn = (m_forEncryption) ? CSSM_EncryptDataInit(m_handle) : CSSM_DecryptDataInit(m_handle);
	// Need better error here
	if (crtn != CSSM_OK)
	{
			CFStringRef result = SecCopyErrorMessageString(crtn, NULL);
			CFErrorRef retValue = CreateSecTransformErrorRef(kSecTransformErrorNotInitializedCorrectly, "CDSA encrypt/decrypt init error (%@).", result);
			CFRelease(result);
			return retValue;
	}


	return result;
}

/* --------------------------------------------------------------------------
 method: 		TransformStartingExecution()
 description: 	Get this transform ready to run.
 NOTE:			the encrypt/decrypt setup is not safe to call for a single
				key from multiple threads at once, TransformStartingExecution is
				responsable making sure this doesn't happen,
				SerializedTransformStartingExecution() does the real set up work.
 -------------------------------------------------------------------------- */
CFErrorRef EncryptDecryptBase::TransformStartingExecution()
{

	dispatch_once(&serializerSetUp, ^{
		serializerTransformStartingExecution = dispatch_queue_create("com.apple.security.EncryptDecrypt.key-setup", NULL);
	});

	__block CFErrorRef result = NULL;	// Assume all is well

	dispatch_sync(serializerTransformStartingExecution, ^{
		result = SerializedTransformStartingExecution();
	});
	return result;
}

/* --------------------------------------------------------------------------
 method: 		TransformCanExecute
 description: 	Do we have a key?
 -------------------------------------------------------------------------- */
Boolean EncryptDecryptBase::TransformCanExecute()
{
	// make sure we have a key -- there may be some circumstance when one isn't available
	// and besides, it helps test this logic
	SecKeyRef key = (SecKeyRef) GetAttribute(kSecEncryptKey);
	return key != NULL;
}

void EncryptDecryptBase::SendCSSMError(CSSM_RETURN retCode)
{
	// make a CFErrorRef for the error message
	CFStringRef errorString = SecCopyErrorMessageString(retCode, NULL);
	CFErrorRef errorRef = CreateGenericErrorRef(kCFErrorDomainOSStatus, retCode, "%@", errorString);
	CFRelease(errorString);

	SendAttribute(kSecTransformOutputAttributeName, errorRef);
	CFRelease(errorRef);
}

void xor_bytes(UInt8 *dst, const UInt8 *src1, const UInt8 *src2, CFIndex length);
void xor_bytes(UInt8 *dst, const UInt8 *src1, const UInt8 *src2, CFIndex length)
{
	// NOTE: this can be made faster, but see if we already have a faster version somewhere first.

	// _mm_xor_ps would be nice here
	// failing that, getting to an aligned boundry and switching to uint64_t
	// would be good.

	while (length--) {
		*dst++ = *src1++ ^ *src2++;
	}
}

extern "C" {
	extern CFDataRef oaep_unpadding_via_c(CFDataRef encodedMessage);
}

CFDataRef EncryptDecryptBase::remove_oaep_padding(CFDataRef encodedMessage)
{
#if 1
	return oaep_unpadding_via_c(encodedMessage);
#else
    CFStringRef hashAlgo = NULL;
    CFDataRef message = NULL, maskedSeed = NULL, maskedDB = NULL, seedMask = NULL, seed = NULL, dbMask = NULL;
    CFDataRef pHash = NULL, pHashPrime = NULL;
    CFDataRef EncodingParameters = NULL;
    CFErrorRef error = NULL;
    UInt8 *raw_seed = NULL, *raw_DB = NULL, *addr01 = NULL;
    SecTransformRef mgf_maskedDB = NULL, mgf_dbMask = NULL, hash = NULL;
    int hLen = -1;
	// RSA's OAEP documentation assumes the crypto layer will remove the leading (partial) byte,
	// but CDSA leaves that responsability to us (we did ask it for "no padding" after all).
	// (use extraPaddingLength = 0 when using a layer that does strip that byte)
    const int extraPaddingLength = 1;

	// The numbered steps below correspond to RSA Laboratories' RSAES-OAEP Encryption Scheme
	// document's numbered steps.

    // NOTE: we omit step 1: "If the length of P is greater than the input limitation for the hash
    // function (2^61 − 1 octets for SHA-1) then output ‘‘decoding error’’ and stop."; we don't have
    // ready access to the input limits of the hash functions, and in the real world we won't be
    // seeing messages that long anyway.

    // (2) If emLen < 2hLen + 1, output ‘‘decoding error’’ and stop.
    hashAlgo = (CFStringRef)this->GetAttribute(kSecOAEPMGF1DigestAlgorithmAttributeName);
	if (hashAlgo == NULL) {
		hashAlgo = kSecDigestSHA1;
	}
    hLen = Digest::LengthForType(hashAlgo);
    if (CFDataGetLength(encodedMessage) < 2*hLen + 1) {
        goto out;
    }

    // (3) Let maskedSeed be the first hLen octets of EM and let maskedDB be the remaining emLen−hLen
    // octets.
    maskedSeed = CFDataCreateWithBytesNoCopy(NULL, CFDataGetBytePtr(encodedMessage) +extraPaddingLength, hLen, kCFAllocatorNull);
    maskedDB = CFDataCreateWithBytesNoCopy(NULL, CFDataGetBytePtr(encodedMessage) + hLen +extraPaddingLength, CFDataGetLength(encodedMessage) - hLen -extraPaddingLength, kCFAllocatorNull);

    // (4) Let seedMask = MGF(maskedDB, hLen).
    mgf_maskedDB = SecCreateMaskGenerationFunctionTransform(hashAlgo, hLen, &error);
    if (!mgf_maskedDB) {
        goto out;
    }
    if (!SecTransformSetAttribute(mgf_maskedDB, kSecTransformInputAttributeName, maskedDB, &error)) {
        goto out;
    }
    seedMask = (CFDataRef)SecTransformExecute(mgf_maskedDB, &error);
    if (!seedMask) {
        goto out;
    }
	(void)transforms_assume(hLen == CFDataGetLength(seedMask));

    // (5) Let seed = maskedSeed ⊕ seedMask.
    raw_seed = (UInt8*)malloc(hLen);
    xor_bytes(raw_seed, CFDataGetBytePtr(maskedSeed), CFDataGetBytePtr(seedMask), hLen);
    seed = CFDataCreateWithBytesNoCopy(NULL, raw_seed, hLen, kCFAllocatorNull);
    if (!seed) {
		free(raw_seed);
		error = GetNoMemoryErrorAndRetain();
		goto out;
	}
    // (6) Let dbMask = MGF (seed, emLen − hLen).
    mgf_dbMask = SecCreateMaskGenerationFunctionTransform(hashAlgo, CFDataGetLength(encodedMessage) - hLen, &error);
    if (!mgf_dbMask) {
        goto out;
    }
    if (!SecTransformSetAttribute(mgf_dbMask, kSecTransformInputAttributeName, seed, &error)) {
        goto out;
    }
    dbMask = (CFDataRef)SecTransformExecute(mgf_dbMask, &error);
    if (!dbMask) {
        goto out;
    }

    // (7) Let DB = maskedDB ⊕ dbMask.
    raw_DB = (UInt8*)malloc(CFDataGetLength(dbMask));
    xor_bytes(raw_DB, CFDataGetBytePtr(maskedDB), CFDataGetBytePtr(dbMask), CFDataGetLength(dbMask));

    // (8) Let pHash = Hash(P), an octet string of length hLen.
    hash = SecDigestTransformCreate(hashAlgo, 0, &error);
	if (!hash) {
		goto out;
	}
    EncodingParameters = (CFDataRef)this->GetAttribute(kSecOAEPEncodingParametersAttributeName);
	if (EncodingParameters) {
		CFRetain(EncodingParameters);
	} else {
		EncodingParameters = CFDataCreate(NULL, (UInt8*)"", 0);
		if (!EncodingParameters) {
			goto out;
		}
	}
	if (!SecTransformSetAttribute(hash, kSecTransformInputAttributeName, EncodingParameters, &error)) {
		goto out;
	}

	pHash = (CFDataRef)transforms_assume(SecTransformExecute(hash, &error));
	if (!pHash) {
		goto out;
	}
	(void)transforms_assume(hLen == CFDataGetLength(pHash));


    // (9) Separate DB into an octet string pHash’ consisting of the first hLen octets of DB, a
    // (possibly empty) octet string PS consisting of consecutive zero octets following pHash’,
    // and a message M as If there is no 01 octet to separate PS from M , output ‘‘decoding error’’ and stop.
    pHashPrime = CFDataCreateWithBytesNoCopy(NULL, raw_DB, hLen, kCFAllocatorNull);
    if (CFEqual(pHash, pHashPrime)) {
        addr01 = (UInt8*)memchr(raw_DB + hLen, 0x01, CFDataGetLength(dbMask) - hLen);
        if (!addr01) {
            goto out;
        }
        message = CFDataCreate(NULL, addr01 + 1, (CFDataGetLength(dbMask) - ((addr01 - raw_DB) + 1)) -extraPaddingLength);
    } else {
        // (10) If pHash’ does not equal pHash, output ‘‘decoding error’’ and stop.
        goto out;
    }

out:
    if (!message) {
        if (!error) {
            error = CreateSecTransformErrorRef(kSecTransformErrorInvalidInput, "decoding error");
        }
        SetAttributeNoCallback(kSecTransformOutputAttributeName, error);
    }

	// Release eveything except:
	//	hashAlgo (obtained via get)
	//	message (return value)
	CFSafeRelease(maskedSeed);
	CFSafeRelease(maskedDB);
	CFSafeRelease(seedMask);
	CFSafeRelease(seed);
	CFSafeRelease(dbMask);
	CFSafeRelease(pHash);
	CFSafeRelease(pHashPrime);
	CFSafeRelease(mgf_dbMask);
	CFSafeRelease(mgf_maskedDB);
	CFSafeRelease(hash);
	CFSafeRelease(EncodingParameters);
	// raw_seed is free'd via CFData, addr01 was never allocated, so raw_DB is our lot
	free(raw_DB);

    // (11) Output M.
    return message;
#endif
}

extern "C" {
	extern CFDataRef oaep_padding_via_c(int desired_message_length, CFDataRef dataValue);
}

CFDataRef EncryptDecryptBase::apply_oaep_padding(CFDataRef dataValue)
{
#if 1
	// MGF1 w/ SHA1 assumed here

	CFErrorRef error = NULL;
	int hLen = Digest::LengthForType(kSecDigestSHA1);
	CFNumberRef desired_message_length_cf = (CFNumberRef)this->GetAttribute(kSecOAEPMessageLengthAttributeName);
	int desired_message_length = 0;
	CSSM_QUERY_SIZE_DATA RSA_size;
	CFDataRef EM = NULL;

	if (desired_message_length_cf) {
		CFNumberGetValue(desired_message_length_cf, kCFNumberIntType, &desired_message_length);
	} else {
		// take RSA (or whatever crypto) block size onto account too
		RSA_size.SizeInputBlock = (uint32)(CFDataGetLength(dataValue) + 2*hLen +1);
		RSA_size.SizeOutputBlock = 0;
		OSStatus status = CSSM_QuerySize(m_handle, CSSM_TRUE, 1, &RSA_size);
		if (status != errSecSuccess) {
			CFStringRef errorString = SecCopyErrorMessageString(status, NULL);
			error = CreateSecTransformErrorRef(kSecTransformErrorInvalidOperation, "CDSA error (%@).", errorString);
			CFRelease(errorString);
			SetAttributeNoCallback(kSecTransformOutputAttributeName, error);
			(void)transforms_assume_zero(EM);
			return EM;
		}
		(void)transforms_assume(RSA_size.SizeInputBlock <= RSA_size.SizeOutputBlock);
		desired_message_length = RSA_size.SizeOutputBlock;
	}
	CFDataRef returnData = oaep_padding_via_c(desired_message_length, dataValue);
	return returnData;

#else
	CFDataRef seed = NULL, dbMask = NULL, maskedDB = NULL, seedMask = NULL, padHash = NULL, padZeros = NULL;
    CFDataRef EncodingParameters = NULL;
	CFMutableDataRef EM = NULL, dataBlob = NULL;
	CFNumberRef desired_message_length_cf = NULL;
	CFErrorRef error = NULL;
	CFStringRef hashAlgo = NULL;
	UInt8 *raw_padZeros = NULL, *raw_seed = NULL, *raw_maskedSeed = NULL, *raw_maskedDB = NULL;
	SecTransformRef mgf_dbMask = NULL, mgf_seedMask = NULL, hash = NULL;
	CFIndex paddingNeeded = -1, padLen = -1;
	int hLen = -1;
	CSSM_QUERY_SIZE_DATA RSA_size;

	// NOTE: we omit (1) If the length of P is greater than the input limitation for the hash function
	// (2^61 − 1 octets for SHA-1) then output ‘‘parameter string too long’’ and stop.
	// We don't have ready access to the input limit of the hash functions, and in the real world
	// we won't be seeing a message that long anyway.

	// (2) If mLen > emLen − 2hLen − 1, output ‘‘message too long’’ and stop.
	hashAlgo = (CFStringRef)this->GetAttribute(kSecOAEPMGF1DigestAlgorithmAttributeName);
	if (hashAlgo == NULL) {
		hashAlgo = kSecDigestSHA1;
	}
	hLen = Digest::LengthForType(hashAlgo);
	desired_message_length_cf = (CFNumberRef)this->GetAttribute(kSecOAEPMessageLengthAttributeName);
	int desired_message_length = 0;
	if (desired_message_length_cf) {
		CFNumberGetValue(desired_message_length_cf, kCFNumberIntType, &desired_message_length);
	} else {
		// take RSA (or whatever crypto) block size onto account too
		RSA_size.SizeInputBlock = CFDataGetLength(dataValue) + 2*hLen +1;
		RSA_size.SizeOutputBlock = 0;
		OSStatus status = CSSM_QuerySize(m_handle, CSSM_TRUE, 1, &RSA_size);
		if (status != errSecSuccess) {
			CFStringRef errorString = SecCopyErrorMessageString(status, NULL);
			error = CreateSecTransformErrorRef(kSecTransformErrorInvalidOperation, "CDSA error (%@).", errorString);
			CFRelease(errorString);
			goto out;
		}
		(void)transforms_assume(RSA_size.SizeInputBlock <= RSA_size.SizeOutputBlock);
		desired_message_length = RSA_size.SizeOutputBlock -1;
	}
	padLen = (desired_message_length - (2*hLen) -1) - CFDataGetLength(dataValue);
	if (padLen < 0) {
		error = CreateSecTransformErrorRef(kSecTransformErrorInvalidLength, "Your message is too long for your message length, it needs to be %d bytes shorter, or you need to adjust the kSecOAEPMessageLengthAttributeName attribute", -padLen);
        goto out;
	}

	// (3) Generate an octet string PS consisting of emLen − mLen − 2hLen − 1 zero octets. The length of PS may be 0.
	raw_padZeros = (UInt8*)calloc(padLen, 1);
	if (!raw_padZeros) {
		error = GetNoMemoryErrorAndRetain();
		goto out;
	}
	padZeros = CFDataCreateWithBytesNoCopy(NULL, raw_padZeros, padLen, kCFAllocatorMalloc);
	if (!padZeros) {
		free(raw_padZeros);
		error = GetNoMemoryErrorAndRetain();
		goto out;
	}

	// (4) Let pHash = Hash(P), an octet string of length hLen.
	hash = SecDigestTransformCreate(hashAlgo, 0, &error);
	if (!hash) {
		goto out;
	}
    EncodingParameters = (CFDataRef)this->GetAttribute(kSecOAEPEncodingParametersAttributeName);
	if (EncodingParameters) {
		CFRetain(EncodingParameters);
	} else {
		EncodingParameters = CFDataCreate(NULL, (UInt8*)"", 0);
		if (!EncodingParameters) {
			error = GetNoMemoryErrorAndRetain();
			goto out;
		}
	}
	if (!SecTransformSetAttribute(hash, kSecTransformInputAttributeName, EncodingParameters, &error)) {
		goto out;
	}

	padHash = (CFDataRef)transforms_assume(SecTransformExecute(hash, &error));
	if (!padHash) {
		goto out;
	}
	(void)transforms_assume(hLen == CFDataGetLength(padHash));

	// (5) Concatenate pHash,PS, the message M, and other padding to form a data block DB as DB = pHash∥PS∥01∥M.
	dataBlob = CFDataCreateMutable(NULL, CFDataGetLength(padHash) + padLen + 1 + CFDataGetLength(dataValue));
	if (!dataBlob) {
		error = GetNoMemoryErrorAndRetain();
		goto out;
	}
	CFDataAppendBytes(dataBlob, CFDataGetBytePtr(padHash), hLen);
	CFDataAppendBytes(dataBlob, raw_padZeros, padLen);
	CFDataAppendBytes(dataBlob, (UInt8*)"\01", 1);
	CFDataAppendBytes(dataBlob, CFDataGetBytePtr(dataValue), CFDataGetLength(dataValue));

	// (6) Generate a random octet string seed of length hLen.
	seed = (CFDataRef)this->GetAttribute(CFSTR("FixedSeedForOAEPTesting"));
	raw_seed = NULL;
	if (seed) {
		(void)transforms_assume(hLen == CFDataGetLength(seed));
		CFRetain(seed);
	} else {
		seed = SecRandomCopyData(kSecRandomDefault, hLen);
		if (!seed) {
			error = GetNoMemoryErrorAndRetain();
			goto out;
		}
	}
    raw_seed = (UInt8*)CFDataGetBytePtr(seed);

	// (7) Let dbMask = MGF (seed, emLen − hLen).
	mgf_dbMask = transforms_assume(SecCreateMaskGenerationFunctionTransform(hashAlgo, desired_message_length - hLen, &error));
	if (!mgf_dbMask) {
		goto out;
	}
	if (!SecTransformSetAttribute(mgf_dbMask, kSecTransformInputAttributeName, seed, &error)) {
		goto out;
	}
	dbMask = (CFDataRef)SecTransformExecute(mgf_dbMask, &error);

	// (8) Let maskedDB = DB ⊕ dbMask.
	// NOTE: we do some allocations above...you know, we should be able to malloc ONE buffer of the
	// proper size.
	raw_maskedDB = (UInt8 *)malloc(CFDataGetLength(dbMask));
	if (!raw_maskedDB) {
		error = GetNoMemoryErrorAndRetain();
		goto out;
	}
	xor_bytes(raw_maskedDB, CFDataGetBytePtr(dbMask), CFDataGetBytePtr(dataBlob), CFDataGetLength(dbMask));
	maskedDB = CFDataCreateWithBytesNoCopy(NULL, raw_maskedDB, CFDataGetLength(dataBlob), kCFAllocatorMalloc);
	if (!maskedDB) {
		free(raw_maskedDB);
		error = GetNoMemoryErrorAndRetain();
		goto out;
	}

	// (9) Let seedMask = MGF(maskedDB, hLen).
	mgf_seedMask = transforms_assume(SecCreateMaskGenerationFunctionTransform(hashAlgo, hLen, &error));
	if (!mgf_seedMask) {
		goto out;
	}
	if (!SecTransformSetAttribute(mgf_seedMask, kSecTransformInputAttributeName, maskedDB, &error)) {
		goto out;
	}
	seedMask = transforms_assume((CFDataRef)SecTransformExecute(mgf_seedMask, &error));
	if (!seedMask) {
		goto out;
	}

	// (10) Let maskedSeed = seed ⊕ seedMask
	raw_maskedSeed = (UInt8 *)malloc(hLen);
	if (!raw_maskedSeed) {
		error = GetNoMemoryErrorAndRetain();
		goto out;
	}
	xor_bytes(raw_maskedSeed, raw_seed, CFDataGetBytePtr(seedMask), hLen);

	// (11) Let EM = maskedSeed∥maskedDB (if we didn't have to pushback the NULL we could do this without physically concatanating)
	// (figure out amount of leading zero padding we need)
	RSA_size.SizeInputBlock = hLen + CFDataGetLength(maskedDB);
	CSSM_QuerySize(m_handle, CSSM_TRUE, 1, &RSA_size);
	paddingNeeded = RSA_size.SizeOutputBlock - RSA_size.SizeInputBlock;
	(void)transforms_assume(paddingNeeded >= 0);

	EM = CFDataCreateMutable(NULL, CFDataGetLength(maskedDB) + hLen + paddingNeeded);
	if (!EM) {
		error = GetNoMemoryErrorAndRetain();
		goto out;
	}
	while(paddingNeeded--) {
		CFDataAppendBytes(EM, (UInt8*)"", 1);
	}

	CFDataAppendBytes(EM, raw_maskedSeed, hLen);
	CFDataAppendBytes(EM, raw_maskedDB, CFDataGetLength(maskedDB));
out:
	if (error) {
		SetAttributeNoCallback(kSecTransformOutputAttributeName, error);
		(void)transforms_assume_zero(EM);
	}

	CFSafeRelease(seed); // via get??
	CFSafeRelease(dbMask);
	CFSafeRelease(maskedDB);
	CFSafeRelease(seedMask);
	CFSafeRelease(padHash);
	CFSafeRelease(padZeros);
	CFSafeRelease(EncodingParameters);
	CFSafeRelease(dataBlob);
	// desired_message_length_cf -- via get
	// hashAlgo -- via get
	CFSafeRelease(mgf_dbMask);
	CFSafeRelease(mgf_seedMask);
	CFSafeRelease(hash);
	// raw_* are all freed by their associated CFDatas, except raw_maskedSeed
	free(raw_maskedSeed);

	// (12) Output EM.
	return EM;
#endif
}

/* --------------------------------------------------------------------------
 method: 		AttributeChanged
 description: 	deal with input
 -------------------------------------------------------------------------- */
void EncryptDecryptBase::AttributeChanged(SecTransformAttributeRef ah, CFTypeRef value)
{
	// sanity check our arguments
	if (ah != inputAH)
	{
		return; // we only deal with input
	}

	if (value != NULL)
	{
		CFTypeID valueType = CFGetTypeID(value);
		if (valueType != CFDataGetTypeID())
		{
			CFStringRef realType = CFCopyTypeIDDescription(valueType);
			CFErrorRef error = CreateSecTransformErrorRef(kSecTransformErrorNotInitializedCorrectly, "Value is not a CFDataRef -- this one is a %@", realType);
			CFRelease(realType);
			SetAttributeNoCallback(kSecTransformOutputAttributeName, error);
			return;
		}

		if (m_forEncryption && m_accumulator) {
			CFDataRef d = (CFDataRef)value;
			CFDataAppendBytes(m_accumulator, CFDataGetBytePtr(d), CFDataGetLength(d));
			return;
		}
	}

	if (m_forEncryption && m_accumulator) {
		(void)transforms_assume_zero(value);
		value = m_accumulator;
		m_accumulator = NULL;
		dispatch_async(this->mDispatchQueue, ^{
			CFSafeRelease(value);
		});
		this->Pushback(inputAH, NULL);

        if (m_oaep_padding) {
			value = apply_oaep_padding((CFDataRef)value);
			dispatch_async(this->mDispatchQueue, ^{
				CFSafeRelease(value);
			});
        }
	}

	// add the input to our cryptor
	CFDataRef valueRef = (CFDataRef) value;
	CSSM_RETURN	crtn = CSSM_OK;
	Boolean inFinal = FALSE;

	if (valueRef != NULL)
	{
		// Convert to A CSSM_DATA
		CSSM_DATA dataStruct;
		dataStruct.Length = CFDataGetLength(valueRef);
        dataStruct.Data = const_cast<uint8_t*>(CFDataGetBytePtr(valueRef));

		CSSM_DATA intermediateDataStruct;
		memset(&intermediateDataStruct, 0, sizeof(intermediateDataStruct));

		CSSM_SIZE bytesProcessed = 0;

		if (m_forEncryption)
		{
			crtn = CSSM_EncryptDataUpdate(m_handle,
										  &dataStruct,
										  1,
										  &intermediateDataStruct,
										  1,
										  &bytesProcessed);
		}
		else
		{
			crtn = CSSM_DecryptDataUpdate(m_handle,
										  &dataStruct,
										  1,
										  &intermediateDataStruct,
										  1,
										  &bytesProcessed);
		}

		if (CSSM_OK != crtn)
		{
			SendCSSMError(crtn);
			return;
		}


		if (intermediateDataStruct.Length > 0)
		{
			if (NULL == m_processedData)
			{
				m_processedData = CFDataCreateMutable(kCFAllocatorDefault, 0);
			}

			CFDataAppendBytes(m_processedData, intermediateDataStruct.Data, bytesProcessed);
			free(intermediateDataStruct.Data);
		}
	}
	else
	{
		// Finalize

		inFinal = TRUE;
		CSSM_DATA remData;
		memset(&remData, 0, sizeof(remData));

		crtn = (m_forEncryption) ? CSSM_EncryptDataFinal(m_handle, &remData) : CSSM_DecryptDataFinal(m_handle, &remData);

		if (CSSM_OK == crtn)
		{
            if (m_forEncryption == false && m_accumulator) {
                (void)transforms_assume_zero(m_processedData);
                if (remData.Length > 0) {
                    CFDataAppendBytes(m_accumulator, remData.Data, remData.Length);
                }
            } else {
                if (NULL == m_processedData)
                {
                    m_processedData = CFDataCreateMutable(kCFAllocatorDefault, 0);
                }

                if (remData.Length > 0)
                {
                    CFDataAppendBytes(m_processedData, remData.Data, remData.Length);
                }
            }
		}

		free(remData.Data);

		if (CSSM_OK != crtn)
		{
			SendCSSMError(crtn);
			return;
		}
	}

	if (NULL != m_processedData)
	{
        SendAttribute(kSecTransformOutputAttributeName, m_processedData);
		CFRelease(m_processedData);
		m_processedData = NULL;
	}

	if (inFinal)
	{
        if (m_oaep_padding && m_forEncryption == false) {
            CFTypeRef unpadded = remove_oaep_padding(m_accumulator);
            SendAttribute(kSecTransformOutputAttributeName, unpadded);
            CFRelease(unpadded);
        }
		SendAttribute(kSecTransformOutputAttributeName, NULL);
	}


}

/* --------------------------------------------------------------------------
 method: 		CopyState
 description: 	Copy the current state of this transform
 -------------------------------------------------------------------------- */
CFDictionaryRef EncryptDecryptBase::CopyState()
{
	// make a dictionary for our state
	CFMutableDictionaryRef state = (CFMutableDictionaryRef) CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	CFStringRef paddingStr = (CFStringRef) GetAttribute(kSecPaddingKey);
	CFStringRef modeStr = (CFStringRef) GetAttribute (kSecEncryptionMode);
	CFDataRef ivData = (CFDataRef) GetAttribute(kSecIVKey);
	if (NULL != paddingStr)
	{
		CFDictionaryAddValue(state, kSecPaddingKey, paddingStr);
	}

	if (NULL != modeStr)
	{
		CFDictionaryAddValue(state, kSecEncryptionMode, modeStr);
	}

	if (NULL != ivData)
	{
		CFDictionaryAddValue(state, kSecIVKey, ivData);
	}

	return state;
}

/* --------------------------------------------------------------------------
 method: 		RestoreState
 description: 	Restore the state of this transform from a dictionary
 -------------------------------------------------------------------------- */
void EncryptDecryptBase::RestoreState(CFDictionaryRef state)
{
	if (NULL == state)
	{
		return;
	}

	CFStringRef paddingStr = (CFStringRef)CFDictionaryGetValue(state, kSecPaddingKey);
	CFStringRef modeStr = (CFStringRef)CFDictionaryGetValue(state, kSecEncryptionMode);
	CFDataRef ivData = (CFDataRef)CFDictionaryGetValue(state, kSecIVKey);

	if (NULL != paddingStr)
	{
		SetAttribute(kSecPaddingKey, paddingStr);
	}

	if (NULL != modeStr)
	{
		SetAttribute(kSecEncryptionMode, modeStr);
	}

	if (NULL != ivData)
	{
		SetAttribute(kSecIVKey, ivData);
	}

}

/* --------------------------------------------------------------------------
 Implementation of the EncryptTransform
 -------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------
 method: 		EncryptTransform (Constructor)
 description: 	Make a new EncryptTransform
 -------------------------------------------------------------------------- */
EncryptTransform::EncryptTransform() : EncryptDecryptBase(kEncryptTransformType)
{
}

/* --------------------------------------------------------------------------
 method: 		~EncryptTransform (Destructor)
 description: 	Clean up the memory of anEncryptTransform
 -------------------------------------------------------------------------- */
EncryptTransform::~EncryptTransform()
{
}

/* --------------------------------------------------------------------------
 method: 		[static] Make
 description: 	Make a new instance of this class
 -------------------------------------------------------------------------- */
SecTransformRef EncryptTransform::Make()
{
	EncryptTransform* tr = new EncryptTransform();
	SecTransformRef str = (SecTransformRef) CoreFoundationHolder::MakeHolder(kEncryptTransformType, tr);
	return str;
}

/* --------------------------------------------------------------------------
 Interface and implementation of the EncryptTransformFactory
 -------------------------------------------------------------------------- */

class EncryptTransformFactory : public TransformFactory
{
public:
	EncryptTransformFactory();
	CFTypeRef Make();
};


/* --------------------------------------------------------------------------
 method: 		EncryptTransformFactory (Constructor)
 description:
 -------------------------------------------------------------------------- */
EncryptTransformFactory::EncryptTransformFactory() :
TransformFactory(kEncryptTransformType)
{}


/* --------------------------------------------------------------------------
 method: 		MakeTransformFactory
 description: 	Make an instance of this factory class
 -------------------------------------------------------------------------- */
TransformFactory* EncryptTransform::MakeTransformFactory()
{
	return new EncryptTransformFactory;
}

/* --------------------------------------------------------------------------
 method: 		Make
 description: 	Create an instance of this class
 -------------------------------------------------------------------------- */
CFTypeRef EncryptTransformFactory::Make()
{
	return EncryptTransform::Make();
}


/* --------------------------------------------------------------------------
 method: 		DecryptTransform (Constructor)
 description: 	Make a new DecryptTransform
 -------------------------------------------------------------------------- */
DecryptTransform::DecryptTransform() : EncryptDecryptBase(kDecryptTransformType)
{
}

/* --------------------------------------------------------------------------
 method: 		~DecryptTransform (Destructor)
 description: 	Clean up the memory of anDecryptTransform
 -------------------------------------------------------------------------- */
DecryptTransform::~DecryptTransform()
{
}


/* --------------------------------------------------------------------------
 method: 		[static] Make
 description: 	Make a new instance of this class
 -------------------------------------------------------------------------- */
SecTransformRef DecryptTransform::Make()
{
	DecryptTransform* tr = new DecryptTransform();
	SecTransformRef str = (SecTransformRef) CoreFoundationHolder::MakeHolder(kDecryptTransformType, tr);
	return str;
}

/* --------------------------------------------------------------------------
 Interface and implementation of the DecryptTransformFactory
 -------------------------------------------------------------------------- */

class DecryptTransformFactory : public TransformFactory
{
public:
	DecryptTransformFactory();
	CFTypeRef Make();
};


/* --------------------------------------------------------------------------
 method: 		DecryptTransformFactory (Constructor)
 description:
 -------------------------------------------------------------------------- */
DecryptTransformFactory::DecryptTransformFactory() :
TransformFactory(kDecryptTransformType)
{}


/* --------------------------------------------------------------------------
 method: 		MakeTransformFactory
 description: 	Make an instance of this factory class
 -------------------------------------------------------------------------- */
TransformFactory* DecryptTransform::MakeTransformFactory()
{
	return new DecryptTransformFactory;
}

/* --------------------------------------------------------------------------
 method: 		Make
 description: 	Create an instance of this class
 -------------------------------------------------------------------------- */
CFTypeRef DecryptTransformFactory::Make()
{
	return DecryptTransform::Make();
}

