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
 *  CACNGKeyHandle.h
 *  TokendMuscle
 */

#ifndef _CACNGKEYHANDLE_H_
#define _CACNGKEYHANDLE_H_

#include "KeyHandle.h"

class CACNGToken;
class CACNGKeyRecord;


//
// A KeyHandle object which implements the crypto interface to muscle.
//
class CACNGKeyHandle: public Tokend::KeyHandle
{
	NOCOPY(CACNGKeyHandle)
public:
    CACNGKeyHandle(CACNGToken &cacToken, const Tokend::MetaRecord &metaRecord,
		CACNGKeyRecord &cacKey);
    ~CACNGKeyHandle();

    virtual void getKeySize(CSSM_KEY_SIZE &keySize);
    virtual uint32 getOutputSize(const Context &context, uint32 inputSize,
		bool encrypting);
    virtual void generateSignature(const Context &context,
		CSSM_ALGORITHMS signOnly, const CssmData &input, CssmData &signature);
    virtual void verifySignature(const Context &context,
		CSSM_ALGORITHMS signOnly, const CssmData &input,
			const CssmData &signature);
    virtual void generateMac(const Context &context, const CssmData &input,
		CssmData &output);
    virtual void verifyMac(const Context &context, const CssmData &input,
		const CssmData &compare);
    virtual void encrypt(const Context &context, const CssmData &clear,
		CssmData &cipher);
    virtual void decrypt(const Context &context, const CssmData &cipher,
		CssmData &clear);

	virtual void exportKey(const Context &context,
		const AccessCredentials *cred, CssmKey &wrappedKey);

	virtual void getOwner(AclOwnerPrototype &owner);
	virtual void getAcl(const char *tag, uint32 &count, AclEntryInfo *&auths);

private:
	CACNGToken &mToken;
	CACNGKeyRecord &mKey;
};


//
// A factory that creates CACNGKeyHandle objects.
//
class CACNGKeyHandleFactory : public Tokend::KeyHandleFactory
{
	NOCOPY(CACNGKeyHandleFactory)
public:
	CACNGKeyHandleFactory() {}
	virtual ~CACNGKeyHandleFactory();

	virtual Tokend::KeyHandle *keyHandle(Tokend::TokenContext *tokenContext,
		const Tokend::MetaRecord &metaRecord, Tokend::Record &record) const;
};


#endif /* !_CACNGKEYHANDLE_H_ */


