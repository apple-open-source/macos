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
// Certificate.cpp
//
#include <Security/Certificate.h>
#include <Security/Schema.h>
#include <Security/oidscert.h>
#include <Security/SecCertificate.h>
#include <Security/cspclient.h>

using namespace KeychainCore;

CL
Certificate::clForType(CSSM_CERT_TYPE type)
{
	return CL(gGuidAppleX509CL);
}

Certificate::Certificate(const CSSM_DATA &data, CSSM_CERT_TYPE type, CSSM_CERT_ENCODING encoding) :
	ItemImpl(CSSM_DL_DB_RECORD_X509_CERTIFICATE, reinterpret_cast<SecKeychainAttributeList *>(NULL), UInt32(data.Length), reinterpret_cast<const void *>(data.Data)),
	mHaveTypeAndEncoding(true),
    mType(type),
    mEncoding(encoding),
    mCL(clForType(type)),
	mCertHandle(0)
{
}

// db item contstructor
Certificate::Certificate(const Keychain &keychain, const PrimaryKey &primaryKey, const CssmClient::DbUniqueRecord &uniqueId) :
	ItemImpl(keychain, primaryKey, uniqueId),
	mHaveTypeAndEncoding(false),
    mCL(NULL),
	mCertHandle(0)
{
}

// PrimaryKey item contstructor
Certificate::Certificate(const Keychain &keychain, const PrimaryKey &primaryKey) :
	ItemImpl(keychain, primaryKey),
	mHaveTypeAndEncoding(false),
    mCL(NULL),
	mCertHandle(0)
{
	// @@@ In this case we don't know the type...
}

Certificate::Certificate(Certificate &certificate) :
	ItemImpl(certificate),
	mHaveTypeAndEncoding(certificate.mHaveTypeAndEncoding),
    mType(certificate.mType),
    mEncoding(certificate.mEncoding),
    mCL(certificate.mCL),
	mCertHandle(0)
{
}

Certificate::~Certificate()
{
	if (mCertHandle)
		CSSM_CL_CertAbortCache(mCL->handle(), mCertHandle);
}

CSSM_HANDLE
Certificate::certHandle()
{
	const CSSM_DATA *cert = &data();
	if (!mCertHandle)
	{
		if (CSSM_RETURN retval = CSSM_CL_CertCache(mCL->handle(), cert, &mCertHandle))
			CssmError::throwMe(retval);
	}

	return mCertHandle;
}

/* Return a zero terminated list of CSSM_DATA_PTR's with the values of the field specified by field.  Caller must call releaseFieldValues to free the storage allocated by this call.  */
CSSM_DATA_PTR *
Certificate::copyFieldValues(const CSSM_OID &field)
{
	CSSM_CL_HANDLE clHandle = mCL->handle();
	CSSM_DATA_PTR fieldValue, *fieldValues;
	CSSM_HANDLE resultsHandle = 0;
	uint32 numberOfFields = 0;
	CSSM_RETURN result;

	result = CSSM_CL_CertGetFirstCachedFieldValue(clHandle, certHandle(), &field, &resultsHandle, &numberOfFields, &fieldValue);
	if (result)
	{
		if (result == CSSMERR_CL_NO_FIELD_VALUES)
			return NULL;

		CssmError::throwMe(result);
	}

	fieldValues = new CSSM_DATA_PTR[numberOfFields + 1];
	fieldValues[0] = fieldValue;
	fieldValues[numberOfFields] = NULL;

	for (uint32 value = 1; value < numberOfFields; ++value)
	{
		CSSM_RETURN cresult = CSSM_CL_CertGetNextCachedFieldValue(clHandle, resultsHandle, &fieldValues[value]);
		if (cresult)
		{
			fieldValues[value] = NULL;
			result = cresult;
			break; // No point in continuing really.
		}
	}

	if (result)
	{
		releaseFieldValues(field, fieldValues);
		CssmError::throwMe(result);
	}

	return fieldValues;
}

void
Certificate::releaseFieldValues(const CSSM_OID &field, CSSM_DATA_PTR *fieldValues)
{
	if (fieldValues)
	{
		CSSM_CL_HANDLE clHandle = mCL->handle();
	
		for (int ix = 0; fieldValues[ix]; ++ix)
			CSSM_CL_FreeFieldValue(clHandle, &field, fieldValues[ix]);
	
		delete[] fieldValues;
	}
}

void
Certificate::addParsedAttribute(const CSSM_DB_ATTRIBUTE_INFO &info, const CSSM_OID &field)
{
	CSSM_DATA_PTR *fieldValues = copyFieldValues(field);
	if (fieldValues)
	{
		CssmDbAttributeData &anAttr = mDbAttributes->add(info);
		for (int ix = 0; fieldValues[ix]; ++ix)
			anAttr.add(*fieldValues[ix], *mDbAttributes);
	
		releaseFieldValues(field, fieldValues);
	}
}

/* Return a CSSM_DATA_PTR with the value of the first field specified by field.  Caller must call releaseFieldValue to free the storage allocated by this call.  */
CSSM_DATA_PTR
Certificate::copyFirstFieldValue(const CSSM_OID &field)
{
	CSSM_CL_HANDLE clHandle = mCL->handle();
	CSSM_DATA_PTR fieldValue;
	CSSM_HANDLE resultsHandle = 0;
	uint32 numberOfFields = 0;
	CSSM_RETURN result;

	result = CSSM_CL_CertGetFirstCachedFieldValue(clHandle, certHandle(), &field, &resultsHandle, &numberOfFields, &fieldValue);
	if (result)
	{
		if (result == CSSMERR_CL_NO_FIELD_VALUES)
			return NULL;

		CssmError::throwMe(result);
	}

	result = CSSM_CL_CertAbortQuery(clHandle, resultsHandle);

	if (result)
	{
		releaseFieldValue(field, fieldValue);
		CssmError::throwMe(result);
	}

	return fieldValue;
}

void
Certificate::releaseFieldValue(const CSSM_OID &field, CSSM_DATA_PTR fieldValue)
{
	if (fieldValue)
	{
		CSSM_CL_HANDLE clHandle = mCL->handle();
		CSSM_CL_FreeFieldValue(clHandle, &field, fieldValue);
	}
}



/*
	This method computes the keyIdentifier for the public key in the cert as
	described below:
	
      The keyIdentifier is composed of the 160-bit SHA-1 hash of the
      value of the BIT STRING subjectPublicKey (excluding the tag,
      length, and number of unused bits).
*/
void
Certificate::publicKeyHash(CssmData &digestData)
{
#if 0
	CSSM_DATA_PTR *keysPtr = copyFieldValues(CSSMOID_X509V1SubjectPublicKey);

	if (keysPtr && keysPtr[0])
	{
		CssmData &key = CssmData::overlay(*keysPtr[0]);
		CssmClient::CSP csp(gGuidAppleCSP);
		CssmClient::Digest digest(csp, CSSM_ALGID_SHA1);
		digest.digest(key, digestData);
	}

	releaseFieldValues(CSSMOID_X509V1SubjectPublicKey, keysPtr);
#else
	CSSM_DATA_PTR keyPtr = copyFirstFieldValue(CSSMOID_CSSMKeyStruct);
	if (keyPtr && keyPtr->Data)
	{
		CssmClient::CSP csp(gGuidAppleCSP);
		CssmClient::PassThrough passThrough(csp);
		CSSM_KEY *key = reinterpret_cast<CSSM_KEY *>(keyPtr->Data);
		void *outData;
		CssmData *cssmData;

		/* Given a CSSM_KEY_PTR in any format, obtain the SSHA-1 hash of the 
		* associated key blob. 
		* Key is specified in CSSM_CSP_CreatePassThroughContext.
		* Hash is allocated bythe CSP, in the App's memory, and returned
		* in *outData. */
		passThrough.key(key);
		passThrough(CSSM_APPLECSP_KEYDIGEST, NULL, &outData);
		cssmData = reinterpret_cast<CssmData *>(outData);
		assert(cssmData->Length <= digestData.Length);
		digestData.Length = cssmData->Length;
		memcpy(digestData.Data, cssmData->Data, cssmData->Length);
		csp.allocator().free(cssmData->Data);
		csp.allocator().free(cssmData);
	}

	releaseFieldValue(CSSMOID_CSSMKeyStruct, keyPtr);
#endif
}

void
Certificate::addLabel()
{
	// Set label attribute for this certificate, based on the X509 subject name.
	const CSSM_OID &fieldOid = CSSMOID_X509V1SubjectNameCStruct;
	CSSM_DATA_PTR fieldValue = copyFirstFieldValue(fieldOid);
	if (fieldValue && fieldValue->Data)
	{
		CSSM_X509_NAME_PTR x509Name = (CSSM_X509_NAME_PTR)fieldValue->Data;
		CSSM_X509_TYPE_VALUE_PAIR *ptvp=0;
		CSSM_X509_RDN_PTR rdnp;
		unsigned int rdnDex, pairDex;
		
		// iterate through all RDN pairs; ptvp points to last entry when done
		if (x509Name->numberOfRDNs) {
			rdnp = &x509Name->RelativeDistinguishedName[x509Name->numberOfRDNs-1];
			if (rdnp->numberOfPairs)
				ptvp = &rdnp->AttributeTypeAndValue[rdnp->numberOfPairs-1];
		}
		if (ptvp)
		{
			CSSM_BER_TAG btag = ptvp->valueType;
			if (btag==BER_TAG_PRINTABLE_STRING || btag==BER_TAG_IA5_STRING ||
				btag==BER_TAG_T61_STRING || btag==BER_TAG_PKIX_UTF8_STRING)
			{
				mDbAttributes->add(Schema::attributeInfo(kSecLabelItemAttr), ptvp->value);
			}
		}
		releaseFieldValue(fieldOid, fieldValue);
	}
}

void
Certificate::populateAttributes()
{
	addParsedAttribute(Schema::attributeInfo(kSecSubjectItemAttr), CSSMOID_X509V1SubjectName);
	addParsedAttribute(Schema::attributeInfo(kSecIssuerItemAttr), CSSMOID_X509V1IssuerName);
	addParsedAttribute(Schema::attributeInfo(kSecSerialNumberItemAttr), CSSMOID_X509V1SerialNumber);

	addParsedAttribute(Schema::attributeInfo(kSecSubjectKeyIdentifierItemAttr), CSSMOID_SubjectKeyIdentifier);

	if(!mHaveTypeAndEncoding)
		MacOSError::throwMe(errSecDataNotAvailable); // @@@ Or some other error.

	// Adjust mType based on the actual version of the cert.
	CSSM_DATA_PTR versionPtr = copyFirstFieldValue(CSSMOID_X509V1Version);
	if (versionPtr && versionPtr->Data && versionPtr->Length == sizeof(uint32))
	{
		mType = CSSM_CERT_X_509v1 + (*reinterpret_cast<uint32 *>(versionPtr->Data));
	}
	else
		mType = CSSM_CERT_X_509v1;

	releaseFieldValue(CSSMOID_X509V1Version, versionPtr);

	mDbAttributes->add(Schema::attributeInfo(kSecCertTypeItemAttr), mType);
	mDbAttributes->add(Schema::attributeInfo(kSecCertEncodingItemAttr), mEncoding);

	uint8 digestBytes[20];
	CssmData digestData(digestBytes, 20);
	publicKeyHash(digestData);

	mDbAttributes->add(Schema::attributeInfo(kSecPublicKeyHashItemAttr), digestData);
	addLabel();
}

const CssmData &
Certificate::data()
{
	CssmDataContainer *data = mData.get();
	if (!data && mKeychain)
	{
	    // Make sure mUniqueId is set.
		dbUniqueRecord();
		data = new CssmDataContainer();
		mData.reset(data);
		mUniqueId->get(NULL, data); 
	}

	// If the data hasn't been set we can't return it.
	if (!data)
		MacOSError::throwMe(errSecDataNotAvailable);

	return *data;
}

CSSM_CERT_TYPE
Certificate::type()
{
	if (!mHaveTypeAndEncoding)
	{
		SecKeychainAttribute attr;
		attr.tag = kSecCertTypeItemAttr;
		attr.data = &mType;
		attr.length = sizeof(mType);
		getAttribute(attr, NULL);
	}

	return mType;
}

CSSM_CERT_ENCODING
Certificate::encoding()
{
	if (!mHaveTypeAndEncoding)
	{
		SecKeychainAttribute attr;
		attr.tag = kSecCertEncodingItemAttr;
		attr.data = &mEncoding;
		attr.length = sizeof(mEncoding);
		getAttribute(attr, NULL);
	}

	return mEncoding;
}

void
Certificate::getSubject(CSSM_X509_NAME &outSubject)
{
}

void
Certificate::getIssuer(CSSM_X509_NAME &outName)
{
}

CSSM_CL_HANDLE
Certificate::clHandle()
{
	if (!mCL)
		mCL = clForType(type());

	return mCL->handle();
}

bool
Certificate::operator < (Certificate &other)
{
	return data() < other.data();
}

bool
Certificate::operator == (Certificate &other)
{
	return data() == other.data();
}

void
Certificate::update()
{
	ItemImpl::update();
}

Item
Certificate::copyTo(const Keychain &keychain)
{
	return ItemImpl::copyTo(keychain);
}

void
Certificate::didModify()
{
}

PrimaryKey
Certificate::add(Keychain &keychain)
{
	// If we already have a Keychain we can't be added.
	if (mKeychain)
		MacOSError::throwMe(errSecDuplicateItem);

	populateAttributes();

	CSSM_DB_RECORDTYPE recordType = mDbAttributes->recordType();

	Db db(keychain->database());
	// add the item to the (regular) db
	try
	{
		mUniqueId = db->insert(recordType, mDbAttributes.get(), mData.get());
	}
	catch (const CssmError &e)
	{
		if (e.cssmError() != CSSMERR_DL_INVALID_RECORDTYPE)
			throw;

		// Create the cert relation and try again.
		db->createRelation(CSSM_DL_DB_RECORD_X509_CERTIFICATE, "CSSM_DL_DB_RECORD_X509_CERTIFICATE",
			Schema::X509CertificateSchemaAttributeCount,
			Schema::X509CertificateSchemaAttributeList,
			Schema::X509CertificateSchemaIndexCount,
			Schema::X509CertificateSchemaIndexList);

		mUniqueId = db->insert(recordType, mDbAttributes.get(), mData.get());
	}

	mPrimaryKey = keychain->makePrimaryKey(recordType, mUniqueId);
    mKeychain = keychain;

	return mPrimaryKey;
}
