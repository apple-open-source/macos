/*
 *  Copyright (c) 2004-2007 Apple Inc. All Rights Reserved.
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
 *  PIVKeyHandle.h
 *  TokendPIV
 */

#ifndef _PIVKEYHANDLE_H_
#define _PIVKEYHANDLE_H_

#include "KeyHandle.h"

#include <deque>
#include "byte_string.h"
#include "SecureBufferAllocator.h"

class PIVToken;
class PIVKeyRecord;

//
// A KeyHandle object which implements the crypto interface to piv.
//
class PIVKeyHandle: public Tokend::KeyHandle
{
	NOCOPY(PIVKeyHandle)
public:
    PIVKeyHandle(PIVToken &cacToken, const Tokend::MetaRecord &metaRecord,
		PIVKeyRecord &cacKey);
    ~PIVKeyHandle();

    virtual void getKeySize(CSSM_KEY_SIZE &keySize);
    virtual uint32 getOutputSize(const Context &context, uint32 inputSize,
		bool encrypting);
    virtual void generateSignature(const Context &context,
		CSSM_ALGORITHMS signOnly, const CssmData &input, CssmData &signature);
    virtual void verifySignature(const Context &context,
		CSSM_ALGORITHMS alg, const CssmData &input,
			const CssmData &signature);
    virtual void generateMac(const Context &context, const CssmData &input,
		CssmData &output);
    virtual void verifyMac(const Context &context, const CssmData &input,
		const CssmData &compare);
    virtual void encrypt(const Context &context, const CssmData &clear,
		CssmData &cipher);
	/* Implemented such that the decrypted data has limited external exposure
	 * Value is, however, cached until destroyed */
    virtual void decrypt(const Context &context, const CssmData &cipher,
		CssmData &clear);

	virtual void exportKey(const Context &context,
		const AccessCredentials *cred, CssmKey &wrappedKey);
private:
	PIVToken &mToken;
	PIVKeyRecord &mKey;
	/* Fixed queue of crypto data to keep the CssmData values used
	 * so that when the Key Handle keys away, the CssmData references go away.
	 * Fixed queue to prevent unbounded growth.
	 * TODO: Need spec on how to do this 'right' -- preferred setup would be for
	 * the data buffer be provided
	 */
//	static const unsigned MAX_BUFFERS = 2;
//	SecureBufferAllocator<MAX_BUFFERS> bufferAllocator;
};


//
// A factory that creates PIVKeyHandle objects.
//
class PIVKeyHandleFactory : public Tokend::KeyHandleFactory
{
	NOCOPY(PIVKeyHandleFactory)
public:
	PIVKeyHandleFactory() {}
	virtual ~PIVKeyHandleFactory();

	virtual Tokend::KeyHandle *keyHandle(Tokend::TokenContext *tokenContext,
		const Tokend::MetaRecord &metaRecord, Tokend::Record &record) const;
};


#endif /* !_PIVKEYHANDLE_H_ */

