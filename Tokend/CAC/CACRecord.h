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
 *  CACRecord.h
 *  TokendMuscle
 */

#ifndef _CACRECORD_H_
#define _CACRECORD_H_

#include "Record.h"

class CACToken;

class CACRecord : public Tokend::Record
{
	NOCOPY(CACRecord)
public:
	CACRecord(const unsigned char *application, const char *description) :
		mApplication(application), mDescription(description) {}
	~CACRecord();

	virtual const char *description() { return mDescription; }

protected:
    const unsigned char *application() const { return mApplication; }

protected:
	const unsigned char *mApplication;
	const char *mDescription;
};


class CACCertificateRecord : public CACRecord
{
	NOCOPY(CACCertificateRecord)
public:
	CACCertificateRecord(const unsigned char *application,
		const char *description) :
		CACRecord(application, description) {}
	~CACCertificateRecord();

	virtual Tokend::Attribute *getDataAttribute(Tokend::TokenContext *tokenContext);
};

class CACKeyRecord : public CACRecord
{
	NOCOPY(CACKeyRecord)
public:
	CACKeyRecord(const unsigned char *application, const char *description,
                 const Tokend::MetaRecord &metaRecord, bool signOnly);
    ~CACKeyRecord();

	size_t sizeInBits() const { return 1024; }
	void computeCrypt(CACToken &cacToken, bool sign, const unsigned char *data,
		size_t dataLength, unsigned char *result, size_t &resultLength);

    virtual void getAcl(const char *tag, uint32 &count,
		AclEntryInfo *&aclList);
private:
	bool mSignOnly;
	AutoAclEntryInfoList mAclEntries;
};


class CACTBRecord : public CACRecord
{
	NOCOPY(CACTBRecord)
public:
	CACTBRecord(const unsigned char *application, const char *description) :
		CACRecord(application, description) {}
	~CACTBRecord();

	virtual Tokend::Attribute *getDataAttribute(Tokend::TokenContext *tokenContext);

protected:
    void getSize(CACToken &cacToken, size_t &tbsize, size_t &vbsize);
	Tokend::Attribute *getDataAttribute(CACToken &cacToken, bool getTB);
};


class CACVBRecord : public CACTBRecord
{
	NOCOPY(CACVBRecord)
public:
	CACVBRecord(const unsigned char *application, const char *description) :
		CACTBRecord(application, description) {}
	~CACVBRecord();

	virtual Tokend::Attribute *getDataAttribute(Tokend::TokenContext *tokenContext);
    virtual void getAcl(const char *tag, uint32 &count,
		AclEntryInfo *&aclList);
private:
	AutoAclEntryInfoList mAclEntries;
};


#endif /* !_CACRECORD_H_ */


