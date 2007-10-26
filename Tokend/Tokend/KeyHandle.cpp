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
 *  KeyHandle.cpp
 *  TokendMuscle
 */

#include "KeyHandle.h"

namespace Tokend
{

//
// KeyHandle
//
KeyHandle::KeyHandle(const MetaRecord &metaRecord,
	const RefPointer<Record> &record) :
	RecordHandle(metaRecord, record)
{
}

KeyHandle::~KeyHandle()
{
}

void KeyHandle::wrapUsingKey(const Context &context,
	const AccessCredentials *cred, KeyHandle *wrappingKeyHandle,
	const CssmKey *wrappingKey, const CssmData *descriptiveData,
	CssmKey &wrappedKey)
{
	/* We are being asked to wrap this key using another key. */
	secdebug("crypto", "wrapKey alg: %u", context.algorithm());
	IFDUMPING("crypto", context.dump("wrapKey context"));
	if (wrappingKeyHandle)
	{
		secdebug("tokend",
			"wrapKey of a reference key using a reference key not supported");
		CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
	}

	/* First export the key from the card. */
	exportKey(context, cred, wrappedKey);

	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void KeyHandle::wrapKey(const Context &context, const CssmKey &subjectKey,
		const CssmData *descriptiveData, CssmKey &wrappedKey)
{
	/* We are being asked to wrap a raw subject key using a key on the card. */
	secdebug("tokend", "wrapKey of a raw subject key not supported");
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void KeyHandle::unwrapKey(const Context &context,
	const AccessCredentials *cred, const AclEntryPrototype *access,
	const CssmKey &wrappedKey, CSSM_KEYUSE usage,
	CSSM_KEYATTR_FLAGS attributes, CssmData *descriptiveData,
	CSSM_HANDLE &hUnwrappedKey, CssmKey &unwrappedKey)
{
	secdebug("crypto", "unwrapKey alg: %u", context.algorithm());
	IFDUMPING("crypto", context.dump("unwrapKey context"));
#if 0
	/* Make sure our key type matches the context type */
	if (keyClass() == CSSM_KEYCLASS_SESSION_KEY)
	{
		if (context.type() != CSSM_ALGCLASS_SYMMETRIC))
			CssmError::throwMe(CSSMERR_CSP_INVALID_CONTEXT);
	}
	else
#endif
	if (context.type() != CSSM_ALGCLASS_ASYMMETRIC)
		CssmError::throwMe(CSSMERR_CSP_INVALID_CONTEXT);

	/* validate wrappedKey */
	if (wrappedKey.keyClass() != CSSM_KEYCLASS_SESSION_KEY)
		CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_CLASS);

	if(wrappedKey.blobType() != CSSM_KEYBLOB_WRAPPED)
		CssmError::throwMe(CSSMERR_CSP_KEY_BLOB_TYPE_INCORRECT);

	/* validate requested storage and usage */
	if (!(attributes & CSSM_KEYATTR_RETURN_DATA)
		|| (attributes & (CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_RETURN_NONE
			| CSSM_KEYATTR_PERMANENT | CSSM_KEYATTR_PRIVATE)) != 0)
		CssmError::throwMe(CSSMERR_CSP_INVALID_KEYATTR_MASK);

	/* prepare outgoing header */
	CssmKey::Header &hdr = unwrappedKey.header();
	hdr.clearPod();
    hdr.HeaderVersion = CSSM_KEYHEADER_VERSION;
	hdr.cspGuid(gGuidAppleSdCSPDL);
	hdr.blobType(CSSM_KEYBLOB_RAW);
    hdr.algorithm(wrappedKey.algorithm());
    hdr.keyClass(wrappedKey.keyClass());
    hdr.KeyUsage = usage;
    hdr.KeyAttr = attributes & ~(CSSM_KEYATTR_RETURN_DATA
		| CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_RETURN_NONE);

    // defaults (change as needed)
	hdr.StartDate = wrappedKey.header().StartDate;
	hdr.EndDate = wrappedKey.header().EndDate;
	unwrappedKey.KeyData.Data = NULL;	// ignore possible incoming KeyData
	unwrappedKey.KeyData.Length = 0;

	/* validate wrappedKey format */
	if (wrappedKey.blobFormat() != CSSM_KEYBLOB_WRAPPED_FORMAT_PKCS7)
		CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_WRAPPED_KEY_FORMAT);

	/* There is no descriptiveData in a PKCS7 wrapped blob. */
	if (descriptiveData)
	{
		descriptiveData->Data = NULL;
		descriptiveData->Length = 0;
	}

	/* Decrypt the key blob. */
	decrypt(context, wrappedKey.keyData(), unwrappedKey.keyData());
	
	/* We are assuming a CSSM_KEYBLOB_WRAPPED_FORMAT_PKCS7 from here on. */
	hdr.blobFormat(CSSM_KEYBLOB_RAW_FORMAT_OCTET_STRING);
	hdr.LogicalKeySizeInBits = unwrappedKey.length() * 8;
}



//
// KeyHandleFactory
//
KeyHandleFactory::~KeyHandleFactory()
{
}


} // end namespace Tokend


