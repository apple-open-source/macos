/*
 * Copyright (c) 2002 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */

//
// Certificate.h - Certificate objects
//
#ifndef _SECURITY_CERTIFICATE_H_
#define _SECURITY_CERTIFICATE_H_

#include <Security/Item.h>

// @@@ This should not be here.
#include <Security/SecBase.h>
#include <Security/clclient.h>
namespace Security
{

namespace KeychainCore
{

class Certificate : public ItemImpl
{
	NOCOPY(Certificate)
public:
	static CL clForType(CSSM_CERT_TYPE type);

	// new item constructor
    Certificate(const CSSM_DATA &data, CSSM_CERT_TYPE type, CSSM_CERT_ENCODING encoding);

	// db item contstructor
    Certificate(const Keychain &keychain, const PrimaryKey &primaryKey, const CssmClient::DbUniqueRecord &uniqueId);

	// PrimaryKey item contstructor
    Certificate(const Keychain &keychain, const PrimaryKey &primaryKey);

	Certificate(Certificate &certificate);
    virtual ~Certificate();

	virtual void update();
	virtual Item copyTo(const Keychain &keychain);
	virtual void didModify(); // Forget any attributes and data we just wrote to the db

    const CssmData &data();
    CSSM_CERT_TYPE type();
	CSSM_CERT_ENCODING encoding();
    void getSubject(CSSM_X509_NAME &outSubject);
    void getIssuer(CSSM_X509_NAME &outName);
   	CSSM_CL_HANDLE clHandle();

	bool operator < (Certificate &other);
	bool operator == (Certificate &other);

protected:
	virtual PrimaryKey add(Keychain &keychain);
	CSSM_HANDLE certHandle();

	CSSM_DATA_PTR *copyFieldValues(const CSSM_OID &field);
	void releaseFieldValues(const CSSM_OID &field, CSSM_DATA_PTR *fieldValues);

	void addParsedAttribute(const CSSM_DB_ATTRIBUTE_INFO &info, const CSSM_OID &field);

	CSSM_DATA_PTR copyFirstFieldValue(const CSSM_OID &field);
	void releaseFieldValue(const CSSM_OID &field, CSSM_DATA_PTR fieldValue);

	void publicKeyHash(CssmData &digestData);
	void addLabel();
	void populateAttributes();

private:
	bool mHaveTypeAndEncoding;
    CSSM_CERT_TYPE mType;
	CSSM_CERT_ENCODING mEncoding;
    CssmClient::CL mCL;
	CSSM_HANDLE mCertHandle;
};

} // end namespace KeychainCore

} // end namespace Security

#endif // !_SECURITY_CERTIFICATE_H_
