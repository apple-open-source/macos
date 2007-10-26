/*
 * Copyright (c) 2004 Apple Computer, Inc. All Rights Reserved.
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


//
// tdtransit - security tokend client library transition code.
//
#include "tdtransit.h"
#include "tokend.h"
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>


namespace Security {
namespace Tokend {


using MachPlusPlus::VMGuard;


void ClientSession::check(kern_return_t rc)
{
	switch (rc) {
	case KERN_SUCCESS:
		return;
	case MIG_SERVER_DIED:
		fault();
		CssmError::throwMe(CSSM_ERRCODE_DEVICE_FAILED);
	default:
		MachPlusPlus::check(rc);
	}
}


//
// DataOutput helper.
// This happens "at the end" of a glue method, via the DataOutput destructor.
//
DataOutput::~DataOutput()
{
	// @@@ Why are we setting up a VMGuard if mData is NULL?
	VMGuard _(mData, mLength);
	if (mData) {	// was assigned to; IPC returned OK
		if (argument) {	// buffer was provided
			if (argument.length() < mLength)
				CssmError::throwMe(CSSMERR_CSP_OUTPUT_LENGTH_ERROR);
			argument.length(mLength);
		} else {	// allocate buffer
			argument = CssmData(allocator.malloc(mLength), mLength);
		}
		memcpy(argument.data(), mData, mLength);
	}
}


//
// Create a packaged-up Context for IPC transmission.
// In addition to collecting the context into a contiguous blob for transmission,
// we also evaluate CssmCryptoData callbacks at this time.
//
SendContext::SendContext(const Security::Context &ctx) : context(ctx)
{
	CssmCryptoData cryptoDataValue;	// holding area for CssmCryptoData element
	IFDEBUG(uint32 cryptoDataUsed = 0);
	Context::Builder builder(Allocator::standard());
	for (unsigned n = 0; n < ctx.attributesInUse(); n++) {
		switch (ctx[n].baseType()) {
		case CSSM_ATTRIBUTE_DATA_CRYPTO_DATA: {
			CssmCryptoData &data = ctx[n];	// extract CssmCryptoData value
			cryptoDataValue = data();		// evaluate callback (if any)
			builder.setup(&cryptoDataValue); // use evaluted value
			IFDEBUG(cryptoDataUsed++);
			break;
		}
		default:
			builder.setup(ctx[n]);
			break;
		}
	}
	attributeSize = builder.make();
	for (unsigned n = 0; n < ctx.attributesInUse(); n++) {
		const Context::Attr &attr = ctx[n];
		switch (attr.baseType()) {
		case CSSM_ATTRIBUTE_DATA_CRYPTO_DATA:
			builder.put(attr.type(), &cryptoDataValue);
			break;
		default:
			builder.put(attr);
			break;
		}
	}
	uint32 count;	// not needed
	builder.done(attributes, count);
	assert(cryptoDataUsed <= 1);	// no more than one slot converted
}


//
// Setup and basic control
//
void ClientSession::probe(Score &score, std::string &tokenUid)
{
	char uid[PATH_MAX];
	IPC(tokend_client_probe(TOKEND_ARGS, &score, uid));
	tokenUid = uid;
}


void ClientSession::establish(Guid &guid, uint32 ssid,
	uint32 flags, const char *cacheDirectory, const char *workDirectory,
	char mdsDirectory[PATH_MAX], char printName[PATH_MAX])
{
	IPC(tokend_client_establish(TOKEND_ARGS, guid, ssid,
		flags, cacheDirectory, workDirectory, mdsDirectory, printName));
}


//
// The soft kiss-of-death. This is a one-way RPC; no reply is sent or received;
// the call returns immediately.
//
void ClientSession::terminate(uint32 reason, uint32 options)
{
	IPC((rcode = CSSM_OK, tokend_client_terminate(mServicePort, reason, options)));
}


//
// Data search/retrieval interface
//
RecordHandle ClientSession::findFirst(const CssmQuery &query,
	CssmDbRecordAttributeData *inAttributes, size_t inAttributesLength,
	SearchHandle &hSearch, CssmData *outData, KeyHandle &hKey,
	CssmDbRecordAttributeData *&outAttributes, mach_msg_type_number_t &outAttributesLength)
{
	Copier<CssmQuery> theQuery(&query, internalAllocator);
	void *dataPtr; mach_msg_type_number_t dataLength;
	RecordHandle hRecord;
	CssmDbRecordAttributeData *outAttributesBase;
	CssmDbRecordAttributeData *tmpOutAttributes = NULL;
	IPC(tokend_client_findFirst(TOKEND_ARGS, COPY(theQuery), COPYFLAT(inAttributes),
		outData != NULL, &dataPtr, &dataLength, &hKey,
		&tmpOutAttributes, &outAttributesLength, &outAttributesBase,
		&hSearch, &hRecord));
//	DataOutput out_data(data, returnAllocator);
	if (outAttributesLength)
	{
		outAttributes = static_cast<CssmDbRecordAttributeData *>(malloc(outAttributesLength));
		memcpy(outAttributes, tmpOutAttributes, outAttributesLength);
		relocate(outAttributes, outAttributesBase);
	}
	else
		outAttributes = NULL;
	if (outData)
		*outData = CssmData(dataPtr, dataLength);
	if (tmpOutAttributes)
		mig_deallocate(reinterpret_cast<vm_address_t>(tmpOutAttributes), outAttributesLength);
	return hRecord;
}

RecordHandle ClientSession::findNext(SearchHandle hSearch,
	CssmDbRecordAttributeData *inAttributes, size_t inAttributesLength,
	CssmData *outData, KeyHandle &hKey,
	CssmDbRecordAttributeData *&outAttributes, mach_msg_type_number_t &outAttributesLength)
{
	void *dataPtr; mach_msg_type_number_t dataLength;
	RecordHandle hRecord;
	CssmDbRecordAttributeData *outAttributesBase;
	CssmDbRecordAttributeData *tmpOutAttributes = NULL;
	IPC(tokend_client_findNext(TOKEND_ARGS, hSearch, COPYFLAT(inAttributes),
		outData != NULL, &dataPtr, &dataLength, &hKey,
		&tmpOutAttributes, &outAttributesLength, &outAttributesBase,
		&hRecord));
	if (outAttributesLength)
	{
		outAttributes = static_cast<CssmDbRecordAttributeData *>(malloc(outAttributesLength));
		memcpy(outAttributes, tmpOutAttributes, outAttributesLength);
		relocate(outAttributes, outAttributesBase);
	}
	else
		outAttributes = NULL;
	if (outData)
		*outData = CssmData(dataPtr, dataLength);
	return hRecord;
}

void ClientSession::findRecordHandle(RecordHandle hRecord,
	CssmDbRecordAttributeData *inAttributes, size_t inAttributesLength,
	CssmData *outData, KeyHandle &hKey,
	CssmDbRecordAttributeData *&outAttributes, mach_msg_type_number_t &outAttributesLength)
{
	void *dataPtr; mach_msg_type_number_t dataLength;
	CssmDbRecordAttributeData *outAttributesBase;
	CssmDbRecordAttributeData *tmpOutAttributes = NULL;
	IPC(tokend_client_findRecordHandle(TOKEND_ARGS, hRecord, COPYFLAT(inAttributes),
		outData != NULL, &dataPtr, &dataLength, &hKey,
		&tmpOutAttributes, &outAttributesLength, &outAttributesBase));
	if (outAttributesLength)
	{
		outAttributes = static_cast<CssmDbRecordAttributeData *>(malloc(outAttributesLength));
		memcpy(outAttributes, tmpOutAttributes, outAttributesLength);
		relocate(outAttributes, outAttributesBase);
	}
	else
		outAttributes = NULL;
	if (outData)
		*outData = CssmData(dataPtr, dataLength);
}

void ClientSession::insertRecord(CSSM_DB_RECORDTYPE recordType,
	const CssmDbRecordAttributeData *inAttributes, size_t attributesLength,
	const CssmData &data, RecordHandle &hRecord)
{
	CssmDbRecordAttributeData *attributes = const_cast<CssmDbRecordAttributeData*>(inAttributes);
	IPC(tokend_client_insertRecord(TOKEND_ARGS, recordType, COPYFLAT(attributes),
		DATA(data), &hRecord));
}

void ClientSession::modifyRecord(CSSM_DB_RECORDTYPE recordType, RecordHandle &hRecord,
	const CssmDbRecordAttributeData *inAttributes, size_t attributesLength,
	const CssmData *data, CSSM_DB_MODIFY_MODE modifyMode)
{
	CssmDbRecordAttributeData *attributes = const_cast<CssmDbRecordAttributeData*>(inAttributes);
	IPC(tokend_client_modifyRecord(TOKEND_ARGS, recordType, &hRecord, COPYFLAT(attributes),
		data != NULL, OPTIONALDATA(data), modifyMode));
}

void ClientSession::deleteRecord(RecordHandle hRecord)
{
	IPC(tokend_client_deleteRecord(TOKEND_ARGS, hRecord));
}

void ClientSession::releaseSearch(SearchHandle hSearch)
{
	IPC(tokend_client_releaseSearch(TOKEND_ARGS, hSearch));
}

void ClientSession::releaseRecord(RecordHandle hRecord)
{
	IPC(tokend_client_releaseRecord(TOKEND_ARGS, hRecord));
}


//
// Key management and inquiry
//
void ClientSession::releaseKey(KeyHandle hKey)
{
	IPC(tokend_client_releaseKey(TOKEND_ARGS, hKey));
}

void ClientSession::queryKeySizeInBits(KeyHandle hKey, CssmKeySize &result)
{
    IPC(tokend_client_queryKeySizeInBits(TOKEND_ARGS, hKey, &result));
}


void ClientSession::getOutputSize(const Context &context, KeyHandle hKey,
    uint32 inputSize, bool encrypt, uint32 &result)
{
	SendContext ctx(context);
    IPC(tokend_client_getOutputSize(TOKEND_ARGS, CONTEXT(ctx), hKey, inputSize, encrypt, &result));
}


//
// Signatures and MACs
//
void ClientSession::generateSignature(const Context &context, KeyHandle hKey,
	const CssmData &data, CssmData &signature, CSSM_ALGORITHMS signOnlyAlgorithm)
{
	SendContext ctx(context);
	DataOutput sig(signature, returnAllocator);
	IPC(tokend_client_generateSignature(TOKEND_ARGS, CONTEXT(ctx), hKey, signOnlyAlgorithm,
		DATA(data), DATA(sig)));
}

void ClientSession::verifySignature(const Context &context, KeyHandle hKey,
	const CssmData &data, const CssmData &signature, CSSM_ALGORITHMS verifyOnlyAlgorithm)
{
	SendContext ctx(context);
	IPC(tokend_client_verifySignature(TOKEND_ARGS, CONTEXT(ctx), hKey, verifyOnlyAlgorithm,
		DATA(data), DATA(signature)));
}


void ClientSession::generateMac(const Context &context, KeyHandle hKey,
	const CssmData &data, CssmData &signature)
{
	SendContext ctx(context);
	DataOutput sig(signature, returnAllocator);
	IPC(tokend_client_generateMac(TOKEND_ARGS, CONTEXT(ctx), hKey,
		DATA(data), DATA(sig)));
}

void ClientSession::verifyMac(const Context &context, KeyHandle hKey,
	const CssmData &data, const CssmData &signature)
{
	SendContext ctx(context);
	IPC(tokend_client_verifyMac(TOKEND_ARGS, CONTEXT(ctx), hKey,
		DATA(data), DATA(signature)));
}


//
// Encryption/Decryption
//
	
void ClientSession::encrypt(const Context &context, KeyHandle hKey,
	const CssmData &clear, CssmData &cipher)
{
	SendContext ctx(context);
	DataOutput cipherOut(cipher, returnAllocator);
	IPC(tokend_client_encrypt(TOKEND_ARGS, CONTEXT(ctx), hKey, DATA(clear), DATA(cipherOut)));
}

void ClientSession::decrypt(const Context &context, KeyHandle hKey,
	const CssmData &cipher, CssmData &clear)
{
	SendContext ctx(context);
	DataOutput clearOut(clear, returnAllocator);
	IPC(tokend_client_decrypt(TOKEND_ARGS, CONTEXT(ctx), hKey, DATA(cipher), DATA(clearOut)));
}


//
// Key generation
//
void ClientSession::generateKey(const Security::Context &context,
		const AccessCredentials *cred, const AclEntryPrototype *proto,
		uint32 keyUsage, uint32 keyAttr,
        KeyHandle &hKey, CssmKey *&key)
{
	SendContext ctx(context);
	Copier<AccessCredentials> creds(cred, internalAllocator);
	Copier<AclEntryPrototype> owner(proto, internalAllocator);
	CssmKey *keyBase; mach_msg_type_number_t keyLength;
	IPC(tokend_client_generateKey(TOKEND_ARGS, CONTEXT(ctx),
		COPY(creds), COPY(owner), keyUsage, keyAttr, &hKey, COPY_OUT(key)));
	relocate(key, keyBase);
}

void ClientSession::generateKey(const Security::Context &context,
		const AccessCredentials *cred, const AclEntryPrototype *proto,
		CSSM_KEYUSE pubKeyUsage, CSSM_KEYATTR_FLAGS pubKeyAttr,
		CSSM_KEYUSE privKeyUsage, CSSM_KEYATTR_FLAGS privKeyAttr,
		KeyHandle &hPubKey, CssmKey *&pubKey,
        KeyHandle &hPrivKey, CssmKey *&privKey)
{
	SendContext ctx(context);
	Copier<AccessCredentials> creds(cred, internalAllocator);
	Copier<AclEntryPrototype> owner(proto, internalAllocator);
	CssmKey *pubKeyBase; mach_msg_type_number_t pubKeyLength;
	CssmKey *privKeyBase; mach_msg_type_number_t privKeyLength;
	IPC(tokend_client_generateKeyPair(TOKEND_ARGS, CONTEXT(ctx),
		COPY(creds), COPY(owner),
		pubKeyUsage, pubKeyAttr, privKeyUsage, privKeyAttr,
		&hPubKey, COPY_OUT(pubKey), &hPrivKey, COPY_OUT(privKey)));
	relocate(pubKey, pubKeyBase);
	relocate(privKey, privKeyBase);
}


//
// Key wrapping and unwrapping
//
void ClientSession::wrapKey(const Context &context, const AccessCredentials *cred,
	KeyHandle hWrappingKey, const CssmKey *wrappingKey,
	KeyHandle hSubjectKey, const CssmKey *subjectKey,
	const CssmData &descriptiveData, CssmWrappedKey *&wrappedKey)
{
	SendContext ctx(context);
	Copier<AccessCredentials> creds(cred, internalAllocator);
	Copier<CssmKey> cWrappingKey(wrappingKey, internalAllocator);
	Copier<CssmKey> cSubjectKey(subjectKey, internalAllocator);
	CssmKey *wrappedKeyBase; mach_msg_type_number_t wrappedKeyLength;
	IPC(tokend_client_wrapKey(TOKEND_ARGS, CONTEXT(ctx), hWrappingKey, COPY(cWrappingKey), COPY(creds),
		hSubjectKey, COPY(cSubjectKey), DATA(descriptiveData), COPY_OUT(wrappedKey)))
}

void ClientSession::unwrapKey(const Security::Context &context,
	const AccessCredentials *cred, const AclEntryPrototype *proto,
	KeyHandle hWrappingKey, const CssmKey *wrappingKey,
	KeyHandle hPublicKey, const CssmKey *publicKey,
	const CssmWrappedKey &wrappedKey, uint32 usage, uint32 attrs,
	CssmData &descriptiveData, KeyHandle &hKey, CssmKey *&key)
{
	SendContext ctx(context);
	Copier<AccessCredentials> creds(cred, internalAllocator);
	Copier<AclEntryPrototype> owner(proto, internalAllocator);
	Copier<CssmKey> cWrappingKey(wrappingKey, internalAllocator);
	Copier<CssmKey> cPublicKey(publicKey, internalAllocator);
	Copier<CssmWrappedKey> cWrappedKey(&wrappedKey, internalAllocator);
	CssmKey *keyBase; mach_msg_type_number_t keyLength;
	DataOutput descriptor(descriptiveData, returnAllocator);
	IPC(tokend_client_unwrapKey(TOKEND_ARGS, CONTEXT(ctx), hWrappingKey, COPY(cWrappingKey),
		COPY(creds), COPY(owner), hPublicKey, COPY(cPublicKey),
		COPY(cWrappedKey), usage, attrs, DATA(descriptor),
        &hKey, COPY_OUT(key)));
	relocate(key, keyBase);
}


//
// Key derivation
//
void ClientSession::deriveKey(DbHandle db, const Context &context,
	KeyHandle hBaseKey, const CssmKey *baseKey,
    uint32 keyUsage, uint32 keyAttr, CssmData &param,
    const AccessCredentials *cred, const AclEntryPrototype *proto,
    KeyHandle &hKey, CssmKey *&key)
{
    SendContext ctx(context);
	Copier<AccessCredentials> creds(cred, internalAllocator);
	Copier<AclEntryPrototype> owner(proto, internalAllocator);
	Copier<CssmKey> cBaseKey(baseKey, internalAllocator);
	CssmDeriveData inForm(param, context.algorithm());
	Copier<CssmDeriveData> inParam(&inForm, internalAllocator);
	CssmKey *keyBase; mach_msg_type_number_t keyLength;
    DataOutput paramOutput(param, returnAllocator);
	IPC(tokend_client_deriveKey(TOKEND_ARGS, CONTEXT(ctx), hBaseKey, COPY(cBaseKey),
		COPY(creds), COPY(owner), COPY(inParam), DATA(paramOutput),
		keyUsage, keyAttr, &hKey, COPY_OUT(key)));
}


//
// ACL getting/setting interface.
// Note that this layer, unlike the securityd-client one, does not return its
// output data as separate allocated (chunked) copies. It returns its data
// directly as a (relocated) flat blob.
//
void ClientSession::getAcl(AclKind kind, GenericHandle key, const char *tag,
	uint32 &count, AclEntryInfo * &info)
{
	AclEntryInfo *infoBase;
	mach_msg_type_number_t infoLength;
	IPC(tokend_client_getAcl(TOKEND_ARGS, kind, key,
		(tag != NULL), tag ? tag : "",
		&count, COPY_OUT(info)));

	// relocate incoming AclEntryInfo array
	ReconstituteWalker relocator(info, infoBase);
	for (uint32 n = 0; n < count; n++)
		walk(relocator, info[n]);
}

void ClientSession::changeAcl(AclKind kind, GenericHandle key,
	const AccessCredentials &cred, const AclEdit &edit)
{
	Copier<AccessCredentials> creds(&cred, internalAllocator);
	Copier<AclEntryInput> infos(edit.newEntry(), internalAllocator);
	IPC(tokend_client_changeAcl(TOKEND_ARGS, kind, key, COPY(creds),
		edit.mode(), edit.handle(), COPY(infos)));
}

void ClientSession::getOwner(AclKind kind, GenericHandle handle, AclOwnerPrototype *&owner)
{
	AclOwnerPrototype *ownerBase; mach_msg_type_number_t ownerLength;
	IPC(tokend_client_getOwner(TOKEND_ARGS, kind, handle, COPY_OUT(owner)));
	// turn the returned AclOwnerPrototype into its proper output form
	relocate(owner, ownerBase);
}

void ClientSession::changeOwner(AclKind kind, GenericHandle key, const AccessCredentials &cred,
	const AclOwnerPrototype &edit)
{
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}


void ClientSession::authenticate(CSSM_DB_ACCESS_TYPE mode, const AccessCredentials *cred)
{
	Copier<AccessCredentials> creds(cred, internalAllocator);
	IPC(tokend_client_authenticate(TOKEND_ARGS, mode, COPY(creds)));
}

bool ClientSession::isLocked()
{
	uint32 locked = 0;
	IPC(tokend_client_isLocked(TOKEND_ARGS, &locked));
	return locked;
}


}	// namespace Tokend
}	// namespace Security
