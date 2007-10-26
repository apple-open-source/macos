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
 *  PIVRecord.h
 *  TokendPIV
 */

#ifndef _PIVRECORD_H_
#define _PIVRECORD_H_

#include "Record.h"

class PIVToken;

class PIVRecord : public Tokend::Record
{
	NOCOPY(PIVRecord)
public:
	PIVRecord(const unsigned char *application, size_t applicationSize, const char *description) :
		mApplication(application), mApplicationSize(applicationSize), mDescription(description) {}
	~PIVRecord();

	virtual const char *description() { return mDescription; }

protected:
    const unsigned char *application() const { return mApplication; }

protected:
	const unsigned char *mApplication;
	size_t mApplicationSize;
	const char *mDescription;
};


class PIVKeyRecord : public PIVRecord
{
	NOCOPY(PIVKeyRecord)
public:
	PIVKeyRecord(const unsigned char *application, size_t applicationSize, const char *description,
                 const Tokend::MetaRecord &metaRecord, bool signOnly);
    ~PIVKeyRecord();

	size_t sizeInBits() const { return 1024; }
	void computeCrypt(PIVToken &pivToken, bool sign, const unsigned char *data,
		size_t dataLength, unsigned char *result, size_t &resultLength);

    virtual void getAcl(const char *tag, uint32 &count,
		AclEntryInfo *&aclList);
private:
	bool mSignOnly;
	AutoAclEntryInfoList mAclEntries;
};


class PIVDataRecord : public PIVRecord
{
	NOCOPY(PIVDataRecord)
public:
	PIVDataRecord(const unsigned char *application, size_t applicationSize, const char *description) :
		PIVRecord(application, applicationSize, description), mIsCertificate(false), mAllowCaching(true) {}
	virtual ~PIVDataRecord();

	virtual Tokend::Attribute *getDataAttribute(Tokend::TokenContext *tokenContext);

protected:
	
	bool mIsCertificate;
	bool mAllowCaching;
};

class PIVCertificateRecord : public PIVDataRecord
{
	NOCOPY(PIVCertificateRecord)
public:
	PIVCertificateRecord(const unsigned char *application, size_t applicationSize,
		const char *description) :
		PIVDataRecord(application, applicationSize, description) {mIsCertificate = true; mAllowCaching = true; }
	virtual ~PIVCertificateRecord();
};

class PIVProtectedRecord : public PIVDataRecord
{
	NOCOPY(PIVProtectedRecord)
public:
	PIVProtectedRecord(const unsigned char *application, size_t applicationSize, const char *description) :
		PIVDataRecord(application, applicationSize, description) {mIsCertificate = false; mAllowCaching = false; }
	virtual ~PIVProtectedRecord();

    virtual void getAcl(const char *tag, uint32 &count,
		AclEntryInfo *&aclList);
private:
	AutoAclEntryInfoList mAclEntries;
};

#endif /* !_PIVRECORD_H_ */
