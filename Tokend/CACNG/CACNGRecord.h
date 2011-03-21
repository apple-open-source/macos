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
 *  CACNGRecord.h
 *  TokendMuscle
 */

#ifndef _CACNGRECORD_H_
#define _CACNGRECORD_H_

#include "Record.h"
#include "CACNGApplet.h"

#include <security_cdsa_utilities/context.h>

class CACNGToken;

class CACNGRecord : public Tokend::Record
{
	NOCOPY(CACNGRecord)
public:
	CACNGRecord(const char *description) :
		 mDescription(description) {}
	~CACNGRecord();

	virtual const char *description() { return mDescription; }

protected:
	const char *mDescription;
};


class CACNGCertificateRecord : public CACNGRecord
{
	NOCOPY(CACNGCertificateRecord)
public:
	CACNGCertificateRecord(
		shared_ptr<CACNGIDObject> identity,
		const char *description) :
		CACNGRecord(description), identity(identity) {}
	~CACNGCertificateRecord();

	virtual Tokend::Attribute *getDataAttribute(Tokend::TokenContext *tokenContext);
private:
	shared_ptr<CACNGIDObject> identity;
};

class CACNGKeyRecord : public CACNGRecord
{
	NOCOPY(CACNGKeyRecord)
public:
	CACNGKeyRecord(shared_ptr<CACNGIDObject> identity, const char *description, const Tokend::MetaRecord &metaRecord, bool signOnly, bool requireNewPin = false);
    ~CACNGKeyRecord();

	size_t sizeInBits() const { return identity->getKeySize(); }
	virtual void computeCrypt(CACNGToken &cacToken, bool sign, const unsigned char *data,
		size_t dataLength, unsigned char *result, size_t &resultLength);

	virtual void getOwner(AclOwnerPrototype &owner);
    virtual void getAcl(const char *tag, uint32 &count,
		AclEntryInfo *&aclList);
private:
	shared_ptr<CACNGIDObject> identity;
	const bool mSignOnly;
	AutoAclEntryInfoList mAclEntries;
	AutoAclOwnerPrototype mAclOwner;
	const bool requireNewPin;
};

class CACNGDataRecord : public CACNGRecord
{
	NOCOPY(CACNGDataRecord)
public:
	CACNGDataRecord(shared_ptr<CACNGReadable> buffer, const char *description) :
		CACNGRecord(description), buffer(buffer) {}
	~CACNGDataRecord();

	virtual Tokend::Attribute *getDataAttribute(Tokend::TokenContext *tokenContext);
    virtual void getAcl(const char *tag, uint32 &count, AclEntryInfo *&aclList);
protected:
	shared_ptr<CACNGReadable> buffer;
	AutoAclEntryInfoList mAclEntries;
};

#endif /* !_CACNGRECORD_H_ */


