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

#include <Security/StorageManager.h>
// @@@ This should not be here.
#include <Security/SecBase.h>
#include <Security/clclient.h>

namespace Security
{

namespace KeychainCore
{

class KeyItem;

class Certificate : public ItemImpl
{
	NOCOPY(Certificate)
public:
	SECCFFUNCTIONS(Certificate, SecCertificateRef, errSecInvalidItemRef)

	static CL clForType(CSSM_CERT_TYPE type);

	// new item constructor
    Certificate(const CSSM_DATA &data, CSSM_CERT_TYPE type, CSSM_CERT_ENCODING encoding);

	// db item contstructor
    Certificate(const Keychain &keychain, const PrimaryKey &primaryKey, const CssmClient::DbUniqueRecord &uniqueId);

	// PrimaryKey item contstructor
    Certificate(const Keychain &keychain, const PrimaryKey &primaryKey);

	Certificate(Certificate &certificate);
    virtual ~Certificate() throw();

	virtual void update();
	virtual Item copyTo(const Keychain &keychain, Access *newAccess = NULL);
	virtual void didModify(); // Forget any attributes and data we just wrote to the db

    const CssmData &data();
    CSSM_CERT_TYPE type();
	CSSM_CERT_ENCODING encoding();
	CFStringRef commonName();
	CFStringRef copyFirstEmailAddress();
	CFArrayRef copyEmailAddresses();
    void getSubject(CSSM_X509_NAME &outSubject);
    void getIssuer(CSSM_X509_NAME &outName);
	const CSSM_X509_ALGORITHM_IDENTIFIER *algorithmID();
   	CSSM_CL_HANDLE clHandle();
	void inferLabel(bool addLabel, CFStringRef *rtnString = NULL);
	SecPointer<KeyItem> publicKey();
	const CssmData &publicKeyHash();

	static KCCursor cursorForIssuerAndSN(const StorageManager::KeychainList &keychains, const CssmData &issuer, const CssmData &serialNumber);
	static KCCursor cursorForSubjectKeyID(const StorageManager::KeychainList &keychains, const CssmData &subjectKeyID);
	static KCCursor cursorForEmail(const StorageManager::KeychainList &keychains, const char *emailAddress);

	static SecPointer<Certificate> findByIssuerAndSN(const StorageManager::KeychainList &keychains, const CssmData &issuer, const CssmData &serialNumber);
	static SecPointer<Certificate> findBySubjectKeyID(const StorageManager::KeychainList &keychains, const CssmData &subjectKeyID);
	static SecPointer<Certificate> findByEmail(const StorageManager::KeychainList &keychains, const char *emailAddress);

	static void normalizeEmailAddress(CSSM_DATA &emailAddress);
	static void getEmailAddresses(CSSM_DATA_PTR *sanValues, CSSM_DATA_PTR snValue, std::vector<CssmData> &emailAddresses);

	bool operator < (Certificate &other);
	bool operator == (Certificate &other);

public:
	CSSM_DATA_PTR copyFirstFieldValue(const CSSM_OID &field);
	void releaseFieldValue(const CSSM_OID &field, CSSM_DATA_PTR fieldValue);

	CSSM_DATA_PTR *copyFieldValues(const CSSM_OID &field);
	void releaseFieldValues(const CSSM_OID &field, CSSM_DATA_PTR *fieldValues);

protected:
	virtual PrimaryKey add(Keychain &keychain);
	CSSM_HANDLE certHandle();

	void addParsedAttribute(const CSSM_DB_ATTRIBUTE_INFO &info, const CSSM_OID &field);

	void populateAttributes();

private:
	bool mHaveTypeAndEncoding;
    CSSM_CERT_TYPE mType;
	CSSM_CERT_ENCODING mEncoding;
    CssmClient::CL mCL;
	CSSM_HANDLE mCertHandle;
	CssmData mPublicKeyHash;
	uint8 mPublicKeyHashBytes[20];
	CSSM_DATA_PTR mV1SubjectPublicKeyCStructValue; // Hack to prevent algorithmID() from leaking.
};

} // end namespace KeychainCore

} // end namespace Security

#endif // !_SECURITY_CERTIFICATE_H_
