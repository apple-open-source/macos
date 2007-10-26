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
 *  AttributeCoder.h
 *  TokendMuscle
 */

#ifndef _TOKEND_ATTRIBUTECODER_H_
#define _TOKEND_ATTRIBUTECODER_H_

#include <security_utilities/utilities.h>
#include <Security/cssmtype.h>

namespace Tokend
{

class MetaAttribute;
class Record;
class TokenContext;


class AttributeCoder
{
	NOCOPY(AttributeCoder)
public:
	AttributeCoder() {}
	virtual ~AttributeCoder() = 0;

	virtual void decode(TokenContext *tokenContext,
		const MetaAttribute &metaAttribute, Record &record) = 0;
};


//
// A coder that derives certificate attributes for the certificate data
//
class CertificateAttributeCoder : public AttributeCoder
{
	NOCOPY(CertificateAttributeCoder)
public:
	CertificateAttributeCoder() {}
	virtual ~CertificateAttributeCoder();

	virtual void decode(TokenContext *tokenContext,
		const MetaAttribute &metaAttribute, Record &record);
private:
};

//
// A coder with a constant value
//
class ConstAttributeCoder : public AttributeCoder
{
	NOCOPY(ConstAttributeCoder)
public:
	ConstAttributeCoder(uint32 value);
	ConstAttributeCoder(bool value);
	virtual ~ConstAttributeCoder();

	virtual void decode(TokenContext *tokenContext,
		const MetaAttribute &metaAttribute, Record &record);
private:
	uint32 mValue;
};


//
// A coder whose value is a guid.
//
class GuidAttributeCoder : public AttributeCoder
{
	NOCOPY(GuidAttributeCoder)
public:
	GuidAttributeCoder(const CSSM_GUID &guid);
	virtual ~GuidAttributeCoder();

	virtual void decode(TokenContext *tokenContext,
		const MetaAttribute &metaAttribute, Record &record);
private:
	const CSSM_GUID mGuid;
};


//
// A coder whose value contains 0 values.
//
class NullAttributeCoder : public AttributeCoder
{
	NOCOPY(NullAttributeCoder)
public:
	NullAttributeCoder() {}
	virtual ~NullAttributeCoder();

	virtual void decode(TokenContext *tokenContext,
		const MetaAttribute &metaAttribute, Record &record);
};


//
// A coder whose value contains 1 zero length value.
//
class ZeroAttributeCoder : public AttributeCoder
{
	NOCOPY(ZeroAttributeCoder)
public:
	ZeroAttributeCoder() {}
	virtual ~ZeroAttributeCoder();

	virtual void decode(TokenContext *tokenContext,
		const MetaAttribute &metaAttribute, Record &record);
};


//
// A data coder for key relations
//
class KeyDataAttributeCoder : public AttributeCoder
{
	NOCOPY(KeyDataAttributeCoder)
public:

	KeyDataAttributeCoder() {}
	virtual ~KeyDataAttributeCoder();

	virtual void decode(TokenContext *tokenContext,
		const MetaAttribute &metaAttribute, Record &record);
};


//
// A coder for private key objects value is the public key hash of a
// certificate.  Generic get an attribute of a linked record coder.
//
class LinkedRecordAttributeCoder : public Tokend::AttributeCoder
{
	NOCOPY(LinkedRecordAttributeCoder)
public:
	LinkedRecordAttributeCoder() {}
	virtual ~LinkedRecordAttributeCoder();
    
    const void *certificateKey() const { return mCertificateMetaAttribute; }
    const void *publicKeyKey() const { return mPublicKeyMetaAttribute; }

	void setCertificateMetaAttribute(
		const Tokend::MetaAttribute *linkedRecordMetaAttribute)
    { mCertificateMetaAttribute = linkedRecordMetaAttribute; }
	void setPublicKeyMetaAttribute(
		const Tokend::MetaAttribute *linkedRecordMetaAttribute)
    { mPublicKeyMetaAttribute = linkedRecordMetaAttribute; }

	virtual void decode(Tokend::TokenContext *tokenContext,
                        const Tokend::MetaAttribute &metaAttribute,
                        Tokend::Record &record);
    
private:
    const Tokend::MetaAttribute *mCertificateMetaAttribute;
    const Tokend::MetaAttribute *mPublicKeyMetaAttribute;
};


//
// A coder that reads the description of an object
//
class DescriptionAttributeCoder : public AttributeCoder
{
	NOCOPY(DescriptionAttributeCoder)
public:

	DescriptionAttributeCoder() {}
	virtual ~DescriptionAttributeCoder();

	virtual void decode(TokenContext *tokenContext,
		const MetaAttribute &metaAttribute, Record &record);
};


//
// A coder that reads the data of an object
//
class DataAttributeCoder : public Tokend::AttributeCoder
{
	NOCOPY(DataAttributeCoder)
public:

	DataAttributeCoder() {}
	virtual ~DataAttributeCoder();

	virtual void decode(TokenContext *tokenContext,
		const MetaAttribute &metaAttribute, Record &record);
};


}	// end namespace Tokend

#endif /* !_TOKEND_ATTRIBUTECODER_H_ */

