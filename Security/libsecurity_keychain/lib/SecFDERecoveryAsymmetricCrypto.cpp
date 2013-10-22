/* 
 * Copyright (c) 2010 Apple Inc. All Rights Reserved.
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

#include "SecFDERecoveryAsymmetricCrypto.h"
#include "SecBridge.h"

#include <security_cdsa_client/clclient.h>
#include <security_cdsa_client/wrapkey.h>
#include <security_utilities/cfutilities.h>

#include <Security/SecCertificate.h>
#include <Security/SecKeychain.h>
#include <Security/SecKeychainSearch.h>
#include <Security/SecKey.h>

static void encodePrivateKeyHeader(const CssmData &inBlob, CFDataRef certificate, FVPrivateKeyHeader &outHeader);
static CFDataRef decodePrivateKeyHeader(SecKeychainRef keychainName, const FVPrivateKeyHeader &inHeader);
static void throwIfError(CSSM_RETURN rv);

#pragma mark ----- Public SPI -----

int SecFDERecoveryWrapCRSKWithPubKey(const uint8_t *crsk, size_t crskLen, 
	SecCertificateRef certificateRef, FVPrivateKeyHeader *outHeader)
{
	BEGIN_SECAPI

	CssmData inBlob((unsigned char *)crsk, (size_t)crskLen);
	Required(certificateRef);
	CFRef<CFDataRef> certificateData(SecCertificateCopyData(certificateRef));
	encodePrivateKeyHeader(inBlob, certificateData, Required(outHeader));

	END_SECAPI
}

CFDataRef SecFDERecoveryUnwrapCRSKWithPrivKey(SecKeychainRef keychain, const FVPrivateKeyHeader *inHeader)
{
	CFDataRef result = NULL;
	OSStatus __secapiresult = 0;

	try
	{
		result = decodePrivateKeyHeader(keychain, Required(inHeader));
	}
	catch (const MacOSError &err) { __secapiresult=err.osStatus(); }
	catch (const CommonError &err) { __secapiresult=SecKeychainErrFromOSStatus(err.osStatus()); }
	catch (const std::bad_alloc &) { __secapiresult=errSecAllocate; }
	catch (...) { __secapiresult=errSecInternalComponent; }
	secdebug("FDERecovery", "SecFDERecoveryUnwrapCRSKWithPrivKey: %d", (int)__secapiresult);
	return result;
}


#pragma mark ----- Asymmetric Crypto -----

static void encodePrivateKeyHeader(const CssmData &inBlob, CFDataRef certificate, FVPrivateKeyHeader &outHeader)
{
	CssmClient::CL cl(gGuidAppleX509CL);
	const CssmData cert(const_cast<UInt8 *>(CFDataGetBytePtr(certificate)), CFDataGetLength(certificate));
	CSSM_KEY_PTR key;
	if (CSSM_RETURN rv = CSSM_CL_CertGetKeyInfo(cl->handle(), &cert, &key))
		CssmError::throwMe(rv);
	
	Security::CssmClient::CSP fCSP(gGuidAppleCSP);

	// Set it up so the cl is used to free key and key->KeyData
	// CssmAutoData _keyData(cl.allocator());
	// _keyData.set(CssmData::overlay(key->KeyData));
	CssmAutoData _key(cl.allocator());
	_key.set(reinterpret_cast<uint8 *>(key), sizeof(*key));
	CssmClient::Key cKey(fCSP, *key);
	
	/* Given a CSSM_KEY_PTR in any format, obtain the SHA-1 hash of the 
		* associated key blob. 
		* Key is specified in CSSM_CSP_CreatePassThroughContext.
		* Hash is allocated by the CSP, in the App's memory, and returned
		* in *outData. */
	CssmClient::PassThrough passThrough(fCSP);
	passThrough.key(key);
	void *outData;
	passThrough(CSSM_APPLECSP_KEYDIGEST, NULL, &outData);
	CssmData *cssmData = reinterpret_cast<CssmData *>(outData);
	
	assert(cssmData->Length <= sizeof(outHeader.publicKeyHash));
	outHeader.publicKeyHashSize = (uint32_t)cssmData->Length;
	memcpy(outHeader.publicKeyHash, cssmData->Data, cssmData->Length);
	fCSP.allocator().free(cssmData->Data);
	fCSP.allocator().free(cssmData);
	
	/* Now encrypt the blob with the public key. */
    CssmClient::Encrypt encrypt(fCSP, key->KeyHeader.AlgorithmId);
	encrypt.key(cKey);
	CssmData clearBuf(outHeader.encryptedBlob, sizeof(outHeader.encryptedBlob));
	CssmAutoData remData(fCSP.allocator());
	encrypt.padding(CSSM_PADDING_PKCS1);
	
	outHeader.encryptedBlobSize = (uint32_t)encrypt.encrypt(inBlob, clearBuf, remData.get());
	if (outHeader.encryptedBlobSize > sizeof(outHeader.encryptedBlob))
		secdebug("FDERecovery", "encodePrivateKeyHeader: encrypted blob too big: %d", outHeader.encryptedBlobSize);
}

CFDataRef decodePrivateKeyHeader(SecKeychainRef keychain, const FVPrivateKeyHeader &inHeader)
{	
	// kSecKeyLabel is defined in libsecurity_keychain/lib/SecKey.h
	SecKeychainAttribute attrs[] =
	{
		{ 6 /* kSecKeyLabel */, inHeader.publicKeyHashSize, const_cast<uint8 *>(inHeader.publicKeyHash) }
	};
	SecKeychainAttributeList attrList =
	{
		sizeof(attrs) / sizeof(SecKeychainAttribute),
		attrs
	};
	CSSM_CSP_HANDLE cspHandle = 0;
	const CSSM_KEY *cssmKey = NULL;
    const CSSM_ACCESS_CREDENTIALS *accessCred = NULL;
    CSSM_CC_HANDLE cc = 0;
	
	SecKeychainSearchRef _searchRef;
	throwIfError(SecKeychainSearchCreateFromAttributes(keychain, CSSM_DL_DB_RECORD_PRIVATE_KEY, &attrList, &_searchRef));
	CFRef<SecKeychainSearchRef> searchRef(_searchRef);
	
	SecKeychainItemRef _item;
	if (SecKeychainSearchCopyNext(searchRef, &_item))
		return false;
	
	CFRef<SecKeyRef> keyItem(reinterpret_cast<SecKeyRef>(_item));
	throwIfError(SecKeyGetCSPHandle(keyItem, &cspHandle));
	throwIfError(SecKeyGetCSSMKey(keyItem, &cssmKey));
    throwIfError(SecKeyGetCredentials(keyItem, CSSM_ACL_AUTHORIZATION_DECRYPT, kSecCredentialTypeDefault, &accessCred));
    throwIfError(CSSM_CSP_CreateAsymmetricContext(cspHandle, cssmKey->KeyHeader.AlgorithmId, accessCred, cssmKey, CSSM_PADDING_PKCS1, &cc));
	CFDataRef result;
	
	try
	{		
		CssmMemoryFunctions memFuncs;
		throwIfError(CSSM_GetAPIMemoryFunctions(cspHandle, &memFuncs));
		CssmMemoryFunctionsAllocator allocator(memFuncs);
		
		const CssmData cipherBuf(const_cast<uint8 *>(inHeader.encryptedBlob), inHeader.encryptedBlobSize);
		CssmAutoData clearBuf(allocator);
		CssmAutoData remData(allocator);
		size_t bytesDecrypted;
		CSSM_RETURN crx = CSSM_DecryptData(cc, &cipherBuf, 1, &clearBuf.get(), 1, &bytesDecrypted, &remData.get());
		secdebug("FDERecovery", "decodePrivateKeyHeader: CSSM_DecryptData result: %d", crx);
		throwIfError(crx);
//		throwIfError(CSSM_DecryptData(cc, &cipherBuf, 1, &clearBuf.get(), 1, &bytesDecrypted, &remData.get()));
		clearBuf.length(bytesDecrypted);
//		rawKey.copy(clearBuf.get());
		result = CFDataCreate(kCFAllocatorDefault, (const UInt8 *)clearBuf.get().data(), clearBuf.get().length());
//		result = parseKeyBlob(clearBuf.get());
	}
	catch(...)
	{
		CSSM_DeleteContext(cc);
		throw;
	}
	
	throwIfError(CSSM_DeleteContext(cc));
	
	return result;
}

static void throwIfError(CSSM_RETURN rv)
{
	if (rv)
		CssmError::throwMe(rv);
}

