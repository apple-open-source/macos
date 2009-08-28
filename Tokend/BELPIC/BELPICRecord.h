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
 *  BELPICRecord.h
 *  TokendMuscle
 */

#ifndef _BELPICRECORD_H_
#define _BELPICRECORD_H_

#include "Record.h"

#include <security_cdsa_utilities/cssmcred.h>

class BELPICToken;

class BELPICRecord : public Tokend::Record
{
	NOCOPY(BELPICRecord)
public:
	BELPICRecord(const char *description) :
		mDescription(description) {}
	virtual ~BELPICRecord();

	virtual const char *description() { return mDescription; }

protected:
	const char *mDescription;
};


class BELPICBinaryFileRecord : public BELPICRecord
{
	NOCOPY(BELPICBinaryFileRecord)
public:
	BELPICBinaryFileRecord(const uint8_t *df, const uint8_t *ef,
						   const char *description) :
	BELPICRecord(description), mDF(df), mEF(ef) {}
	virtual ~BELPICBinaryFileRecord();
	
	virtual Tokend::Attribute *getDataAttribute(Tokend::TokenContext *tokenContext) = 0;
	
protected:
	const uint8_t *mDF;
	const uint8_t *mEF;
};

class BELPICCertificateRecord : public BELPICBinaryFileRecord
{
	NOCOPY(BELPICCertificateRecord)
public:
	BELPICCertificateRecord(const uint8_t *df, const uint8_t *ef,
							   const char *description) :
		BELPICBinaryFileRecord(df, ef, description) {}
	virtual ~BELPICCertificateRecord();
	
	virtual Tokend::Attribute *getDataAttribute(Tokend::TokenContext *tokenContext);
};

class BELPICProtectedRecord : public BELPICBinaryFileRecord
{
	NOCOPY(BELPICProtectedRecord)
public:
	BELPICProtectedRecord(const uint8_t *df, const uint8_t *ef, const char *description) :
		BELPICBinaryFileRecord(df, ef, description) {}
	virtual ~BELPICProtectedRecord();
	
	virtual Tokend::Attribute *getDataAttribute(Tokend::TokenContext *tokenContext);
	virtual void getAcl(const char *tag, uint32 &count,
						AclEntryInfo *&aclList);
private:
	AutoAclEntryInfoList mAclEntries;
};


class BELPICKeyRecord : public BELPICRecord
{
	NOCOPY(BELPICKeyRecord)
public:
	BELPICKeyRecord(const uint8_t *keyId, const char *description,
		const Tokend::MetaRecord &metaRecord, bool signOnly);
    virtual ~BELPICKeyRecord();

	size_t sizeInBits() const { return 1024; }
	void computeCrypt(BELPICToken &belpicToken, bool sign,
		const AccessCredentials *cred, const unsigned char *data,
		size_t dataLength, unsigned char *result, size_t &resultLength);

	void getAcl(const char *tag, uint32 &count, AclEntryInfo *&acls);

private:
	const uint8_t *mKeyId;
	bool mSignOnly;
	AutoAclEntryInfoList mAclEntries;
};


#endif /* !_BELPICRECORD_H_ */


