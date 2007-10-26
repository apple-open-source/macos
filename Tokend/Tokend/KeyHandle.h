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
 *  KeyHandle.h
 *  TokendMuscle
 */

#ifndef _TOKEND_KEYHANDLE_H_
#define _TOKEND_KEYHANDLE_H_

#include "RecordHandle.h"

#include <security_cdsa_utilities/handleobject.h>
#include <security_cdsa_utilities/context.h>
#include <security_cdsa_utilities/cssmaclpod.h>

namespace Tokend
{

class MetaRecord;
class Record;
class TokenContext;


//
// A (nearly pure virtual) KeyHandle object which implements the crypto
// interface.
//
class KeyHandle : public RecordHandle
{
	NOCOPY(KeyHandle)
public:
    KeyHandle(const MetaRecord &metaRecord, const RefPointer<Record> &record);
    ~KeyHandle();

    virtual void getKeySize(CSSM_KEY_SIZE &keySize) = 0;
    virtual uint32 getOutputSize(const Context &context, uint32 inputSize,
		bool encrypting) = 0;
    virtual void generateSignature(const Context &context,
		CSSM_ALGORITHMS signOnly, const CssmData &input,
		CssmData &signature) = 0;
    virtual void verifySignature(const Context &context,
		CSSM_ALGORITHMS signOnly, const CssmData &input,
		const CssmData &signature) = 0;
    virtual void generateMac(const Context &context, const CssmData &input,
		CssmData &output) = 0;
    virtual void verifyMac(const Context &context, const CssmData &input,
		const CssmData &compare) = 0;
    virtual void encrypt(const Context &context, const CssmData &clear,
		CssmData &cipher) = 0;
    virtual void decrypt(const Context &context, const CssmData &cipher,
		CssmData &clear) = 0;

	virtual void exportKey(const Context &context,
		const AccessCredentials *cred, CssmKey &wrappedKey) = 0;

	virtual void wrapUsingKey(const Context &context,
		const AccessCredentials *cred, KeyHandle *wrappingKeyHandle,
		const CssmKey *wrappingKey, const CssmData *descriptiveData,
		CssmKey &wrappedKey);
	virtual void wrapKey(const Context &context, const CssmKey &subjectKey,
			const CssmData *descriptiveData, CssmKey &wrappedKey);
	virtual void unwrapKey(const Context &context,
		const AccessCredentials *cred, const AclEntryPrototype *access,
		const CssmKey &wrappedKey, CSSM_KEYUSE usage,
		CSSM_KEYATTR_FLAGS attributes, CssmData *descriptiveData,
		CSSM_HANDLE &hUnwrappedKey, CssmKey &unwrappedKey);
private:
};


//
// A (pure virtual) factory that creates KeyHandle objects.
//
class KeyHandleFactory
{
	NOCOPY(KeyHandleFactory)
public:
	KeyHandleFactory() {}
	virtual ~KeyHandleFactory() = 0;

	virtual KeyHandle *keyHandle(TokenContext *tokenContext,
		const MetaRecord &metaRecord, Record &record) const = 0;
};


} // end namespace Tokend

#endif /* !_TOKEND_KEYHANDLE_H_ */


