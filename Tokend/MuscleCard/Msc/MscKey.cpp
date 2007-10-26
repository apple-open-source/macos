/*
 *  Copyright (c) 2004 Apple Computer, Inc. All Rights Reserved.
 * 
 *  @APPLE_LICENSE_HEADER_START@
 *  
 *  This file contains Original Code and/or Modifications of Original Code
 *  as defined in and that are subject to the Apple Public Source License
 *  Version 2.0 (the 'License'). You may not use this file except in
 *  compliance with the License. Please obtain a copy of the License at
 *  http://www.opensource.apple.com/apsl/ and read it before using this
 *  file.
 *  
 *  The Original Code and all software distributed under the License are
 *  distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 *  EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 *  INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 *  Please see the License for the specific language governing rights and
 *  limitations under the License.
 *  
 *  @APPLE_LICENSE_HEADER_END@
 */

/*
 *  MscKey.cpp
 *  TokendMuscle
 */

#include "MscKey.h"
#include "MscError.h"

void MscKey::importKey(const MSCKeyACL& keyACL,const void *keyBlob,size_t keyBlobSize,
	MSCKeyPolicy& keyPolicy,MSCPVoid32 pAddParams, MSCUChar8 addParamsSize)
{
	MSC_RV rv = MSCImportKey(&Required(mConnection),number(),const_cast<MSCKeyACL *>(&keyACL),
		reinterpret_cast<MSCPUChar8>(const_cast<void *>(keyBlob)),keyBlobSize,&keyPolicy,pAddParams,addParamsSize);
	if (rv!=MSC_SUCCESS)
		MscError::throwMe(rv);
}

void MscKey::exportKey(void *keyBlob,size_t keyBlobSize,MSCPVoid32 pAddParams, MSCUChar8 addParamsSize)
{
	uint32_t kbs = keyBlobSize;
	MSC_RV rv = MSCExportKey(&Required(mConnection),number(),
		reinterpret_cast<MSCPUChar8>(keyBlob),&kbs,pAddParams,addParamsSize);
	if (rv!=MSC_SUCCESS)
		MscError::throwMe(rv);
}

void MscKey::extAuthenticate(MSCUChar8 cipherMode,MSCUChar8 cipherDirection,const char *pData,size_t dataSize)
{
	MSC_RV rv = MSCExtAuthenticate(&Required(mConnection),number(),cipherMode,cipherDirection,
		reinterpret_cast<unsigned char *>(const_cast<char *>(pData)),dataSize);
	if (rv!=MSC_SUCCESS)
		MscError::throwMe(rv);
}

void MscKey::convert(CssmKey &cssmk)
{
	cssmk.header().cspGuid(Guid::overlay(gGuidAppleSdCSPDL));

    switch (type())
	{
	case MSC_KEY_RSA_PRIVATE:
	case MSC_KEY_RSA_PRIVATE_CRT:
	case MSC_KEY_RSA_PUBLIC:
		cssmk.algorithm(CSSM_ALGID_RSA);
		break;

	case MSC_KEY_DSA_PRIVATE:
	case MSC_KEY_DSA_PUBLIC:
		cssmk.algorithm(CSSM_ALGID_DSA);
		break;

	case MSC_KEY_DES:
		cssmk.algorithm(CSSM_ALGID_DES);
		break;
	case MSC_KEY_3DES:
		// @@@ Which algid is this?
		cssmk.algorithm(CSSM_ALGID_3DES);
		//cssmk.algorithm(CSSM_ALGID_3DES_3KEY_EDE);
		//cssmk.algorithm(CSSM_ALGID_3DES_2KEY_EDE);
		//cssmk.algorithm(CSSM_ALGID_3DES_1KEY_EEE);
		//cssmk.algorithm(CSSM_ALGID_3DES_3KEY_EEE);
		//cssmk.algorithm(CSSM_ALGID_3DES_2KEY_EEE);
		break;
	case MSC_KEY_3DES3:
		// @@@ Which algid is this?
		cssmk.algorithm(CSSM_ALGID_3DES_3KEY_EDE);
		//cssmk.algorithm(CSSM_ALGID_3DES_3KEY_EEE);
		break;
	default:
		cssmk.algorithm(CSSM_ALGID_CUSTOM);
		break;
	}

	cssmk.blobType(CSSM_KEYBLOB_REFERENCE);					// Keys are always reference keys
	cssmk.blobFormat(CSSM_KEYBLOB_REF_FORMAT_INTEGER);
    switch (type())
	{
	case MSC_KEY_RSA_PRIVATE:
	case MSC_KEY_RSA_PRIVATE_CRT:
	case MSC_KEY_DSA_PRIVATE:
		cssmk.keyClass(CSSM_KEYCLASS_PRIVATE_KEY);
		break;
	case MSC_KEY_RSA_PUBLIC:
	case MSC_KEY_DSA_PUBLIC:
		cssmk.keyClass(CSSM_KEYCLASS_PUBLIC_KEY);
		break;
	case MSC_KEY_DES:
	case MSC_KEY_3DES:
	case MSC_KEY_3DES3:
		cssmk.keyClass(CSSM_KEYCLASS_SESSION_KEY);
		break;
	default:
		cssmk.keyClass(CSSM_KEYCLASS_OTHER);
		break;
	}

	cssmk.header().LogicalKeySizeInBits = size();

	cssmk.setAttribute(CSSM_KEYATTR_PERMANENT);
	if (acl().read() != MSC_AUT_NONE)
		cssmk.setAttribute(CSSM_KEYATTR_EXTRACTABLE);
	else
		cssmk.setAttribute(CSSM_KEYATTR_SENSITIVE);

	if (acl().write() != MSC_AUT_NONE)
		cssmk.setAttribute(CSSM_KEYATTR_MODIFIABLE);

	if (acl().use() != MSC_AUT_ALL)
		cssmk.setAttribute(CSSM_KEYATTR_PRIVATE);

	// Muscle doesn't really encode these values.
	cssmk.clearAttribute(CSSM_KEYATTR_ALWAYS_SENSITIVE | CSSM_KEYATTR_NEVER_EXTRACTABLE);

	// Set up key usage fields
	MSCUShort16 cipherDirection = keyPolicy.cipherDirection;
	if (cipherDirection & MSC_KEYPOLICY_DIR_SIGN)
		cssmk.usage(CSSM_KEYUSE_SIGN);
	if (cipherDirection & MSC_KEYPOLICY_DIR_VERIFY)
		cssmk.usage(CSSM_KEYUSE_VERIFY);
	if (cipherDirection & MSC_KEYPOLICY_DIR_ENCRYPT)
		cssmk.usage(CSSM_KEYUSE_ENCRYPT | CSSM_KEYUSE_WRAP);
	if (cipherDirection & MSC_KEYPOLICY_DIR_DECRYPT)
		cssmk.usage(CSSM_KEYUSE_DECRYPT | CSSM_KEYUSE_UNWRAP);
	// @@@ no mapping to CSSM_KEYUSE_ANY, CSSM_KEYUSE_SIGN_RECOVER, CSSM_KEYUSE_VERIFY_RECOVER, CSSM_KEYUSE_DERIVE
}

void MscKey::computeCrypt(MSCUChar8 cipherMode, MSCUChar8 cipherDirection,
	const MSCUChar8 *inputData, size_t inputDataSize,
	MSCUChar8 *outputData, size_t &outputDataSize)
{
	MSCCryptInit cryptInit = { number(), cipherMode, cipherDirection, };
	uint32_t outsz = outputDataSize;
	MSC_RV rv = MSCComputeCrypt(mConnection, &cryptInit, const_cast<MSCUChar8 *>(inputData),
		inputDataSize, outputData, &outsz);
	outputDataSize = outsz;
	if (rv != MSC_SUCCESS)
		MscError::throwMe(rv);
}


#if defined(DEBUGDUMP)
void MscKey::debugDump()
{
	Debug::dump("key: %hhu type: 0x%02hhX size: %hu policy(mode: 0x%04hX direction: 0x%04hX) acl(read: 0x%04hX write: 0x%04hX use: 0x%04hX)\n", keyNum, keyType, keySize, keyPolicy.cipherMode, keyPolicy.cipherDirection, keyACL.readPermission, keyACL.writePermission, keyACL.usePermission);
}
#endif /* !defined(DEBUGDUMP) */

