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
#include <Security/oidsattr.h>
#include <Security/SecCertificate.h>
#include <Security/SecCertificatePriv.h>
#include <Security/cspclient.h>
#include <Security/KeyItem.h>
#include <Security/KCCursor.h>
#include <vector>
#include "CLFieldsCommon.h"


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
	mCertHandle(0),
	mV1SubjectPublicKeyCStructValue(NULL)
{
}

// db item contstructor
Certificate::Certificate(const Keychain &keychain, const PrimaryKey &primaryKey, const CssmClient::DbUniqueRecord &uniqueId) :
	ItemImpl(keychain, primaryKey, uniqueId),
	mHaveTypeAndEncoding(false),
    mCL(NULL),
	mCertHandle(0),
	mV1SubjectPublicKeyCStructValue(NULL)
{
}

// PrimaryKey item contstructor
Certificate::Certificate(const Keychain &keychain, const PrimaryKey &primaryKey) :
	ItemImpl(keychain, primaryKey),
	mHaveTypeAndEncoding(false),
    mCL(NULL),
	mCertHandle(0),
	mV1SubjectPublicKeyCStructValue(NULL)
{
	// @@@ In this case we don't know the type...
}

Certificate::Certificate(Certificate &certificate) :
	ItemImpl(certificate),
	mHaveTypeAndEncoding(certificate.mHaveTypeAndEncoding),
    mType(certificate.mType),
    mEncoding(certificate.mEncoding),
    mCL(certificate.mCL),
	mCertHandle(0),
	mV1SubjectPublicKeyCStructValue(NULL)
{
}

Certificate::~Certificate() throw()
{
	if (mV1SubjectPublicKeyCStructValue)
		releaseFieldValue(CSSMOID_X509V1SubjectPublicKeyCStruct, mV1SubjectPublicKeyCStructValue);

	if (mCertHandle && mCL)
		CSSM_CL_CertAbortCache(mCL->handle(), mCertHandle);
}

CSSM_HANDLE
Certificate::certHandle()
{
	const CSSM_DATA *cert = &data();
	if (!mCertHandle)
	{
		if (CSSM_RETURN retval = CSSM_CL_CertCache(clHandle(), cert, &mCertHandle))
			CssmError::throwMe(retval);
	}

	return mCertHandle;
}

/* Return a zero terminated list of CSSM_DATA_PTR's with the values of the field specified by field.  Caller must call releaseFieldValues to free the storage allocated by this call.  */
CSSM_DATA_PTR *
Certificate::copyFieldValues(const CSSM_OID &field)
{
	CSSM_CL_HANDLE clh = clHandle();
	CSSM_DATA_PTR fieldValue, *fieldValues;
	CSSM_HANDLE resultsHandle = 0;
	uint32 numberOfFields = 0;
	CSSM_RETURN result;

	result = CSSM_CL_CertGetFirstCachedFieldValue(clh, certHandle(), &field, &resultsHandle, &numberOfFields, &fieldValue);
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
		CSSM_RETURN cresult = CSSM_CL_CertGetNextCachedFieldValue(clh, resultsHandle, &fieldValues[value]);
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
		CSSM_CL_HANDLE clh = clHandle();
	
		for (int ix = 0; fieldValues[ix]; ++ix)
			CSSM_CL_FreeFieldValue(clh, &field, fieldValues[ix]);
	
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
	CSSM_CL_HANDLE clh = clHandle();
	CSSM_DATA_PTR fieldValue;
	CSSM_HANDLE resultsHandle = 0;
	uint32 numberOfFields = 0;
	CSSM_RETURN result;

	result = CSSM_CL_CertGetFirstCachedFieldValue(clh, certHandle(), &field, &resultsHandle, &numberOfFields, &fieldValue);
	if (result)
	{
		if (result == CSSMERR_CL_NO_FIELD_VALUES)
			return NULL;

		CssmError::throwMe(result);
	}

	result = CSSM_CL_CertAbortQuery(clh, resultsHandle);

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
		CSSM_CL_HANDLE clh = clHandle();
		CSSM_CL_FreeFieldValue(clh, &field, fieldValue);
	}
}



/*
	This method computes the keyIdentifier for the public key in the cert as
	described below:
	
      The keyIdentifier is composed of the 160-bit SHA-1 hash of the
      value of the BIT STRING subjectPublicKey (excluding the tag,
      length, and number of unused bits).
*/
const CssmData &
Certificate::publicKeyHash()
{
	if (mPublicKeyHash.Length)
		return mPublicKeyHash;

	CSSM_DATA_PTR keyPtr = copyFirstFieldValue(CSSMOID_CSSMKeyStruct);
	if (keyPtr && keyPtr->Data)
	{
		CssmClient::CSP csp(gGuidAppleCSP);
		CssmClient::PassThrough passThrough(csp);
		CSSM_KEY *key = reinterpret_cast<CSSM_KEY *>(keyPtr->Data);
		void *outData;
		CssmData *cssmData;

		/* Given a CSSM_KEY_PTR in any format, obtain the SHA-1 hash of the 
		 * associated key blob. 
		 * Key is specified in CSSM_CSP_CreatePassThroughContext.
		 * Hash is allocated by the CSP, in the App's memory, and returned
		 * in *outData. */
		passThrough.key(key);
		passThrough(CSSM_APPLECSP_KEYDIGEST, NULL, &outData);
		cssmData = reinterpret_cast<CssmData *>(outData);

		assert(cssmData->Length <= sizeof(mPublicKeyHashBytes));
		mPublicKeyHash.Data = mPublicKeyHashBytes;
		mPublicKeyHash.Length = cssmData->Length;
		memcpy(mPublicKeyHash.Data, cssmData->Data, cssmData->Length);
		csp.allocator().free(cssmData->Data);
		csp.allocator().free(cssmData);
	}
	
	releaseFieldValue(CSSMOID_CSSMKeyStruct, keyPtr);

	return mPublicKeyHash;
}

/*
 * Given an CSSM_X509_NAME, Find the first name/value pair with 
 * a printable value which matches the specified OID (e.g., CSSMOID_CommonName). 
 * Returns NULL if none found. 
 */
static const CSSM_DATA *
findPrintableField(
	const CSSM_X509_NAME &x509Name,
	const CSSM_OID *tvpType)			// NULL means "any printable field"
{
	for(uint32 rdnDex=0; rdnDex<x509Name.numberOfRDNs; rdnDex++) {
		const CSSM_X509_RDN *rdnPtr = 
			&x509Name.RelativeDistinguishedName[rdnDex];
		for(uint32 tvpDex=0; tvpDex<rdnPtr->numberOfPairs; tvpDex++) {
			const CSSM_X509_TYPE_VALUE_PAIR *tvpPtr = 
				&rdnPtr->AttributeTypeAndValue[tvpDex];
		
			/* type/value pair: match caller's specified type? */
			if((tvpType != NULL) &&
			   ((tvpPtr->type.Length != tvpType->Length) ||
			    memcmp(tvpPtr->type.Data, tvpType->Data, tvpType->Length))) {
					continue;
			}
			
			/* printable? */
			switch(tvpPtr->valueType) {
				case BER_TAG_PRINTABLE_STRING:
				case BER_TAG_IA5_STRING:
				case BER_TAG_T61_STRING:
				case BER_TAG_PKIX_UTF8_STRING:
					/* success */
					return &tvpPtr->value;
				default:
					break;
			}
		}	/* for each pair */
	}		/* for each RDN */
	
	/* no printable component of specified type found */
	return NULL;
}

/*
 * Infer printable label for a given an CSSM_X509_NAME. Returns NULL
 * if no appropriate printable name found.
 */
const CSSM_DATA *SecInferLabelFromX509Name(
	const CSSM_X509_NAME *x509Name)
{
	const CSSM_DATA	*printValue;
	/*
	 * Search order (take the first one found with a printable
	 * value):
	 *  -- common name
	 *  -- Orgnaizational Unit
	 *  -- Organization
	 *  -- field of any kind
	 */
	printValue = findPrintableField(*x509Name, &CSSMOID_CommonName);
	if(printValue != NULL) {
		return printValue;
	}
	printValue = findPrintableField(*x509Name, &CSSMOID_OrganizationalUnitName);
	if(printValue != NULL) {
		return printValue;
	}
	printValue = findPrintableField(*x509Name, &CSSMOID_OrganizationName);
	if(printValue != NULL) {
		return printValue;
	}
	/* take anything */
	return findPrintableField(*x509Name, NULL);
}

void
Certificate::inferLabel(bool addLabel, CFStringRef *rtnString)
{
	// Set PrintName and optionally the Alias attribute for this certificate, based on the 
	// X509 SubjectAltName and SubjectName.
	const CSSM_DATA *printName = NULL;
	std::vector<CssmData> emailAddresses;
	CSSM_DATA puntData;

	// Find the SubjectAltName fields, if any, and extract all the GNT_RFC822Name entries from all of them
	const CSSM_OID &sanOid = CSSMOID_SubjectAltName;
	CSSM_DATA_PTR *sanValues = copyFieldValues(sanOid);
	const CSSM_OID &snOid = CSSMOID_X509V1SubjectNameCStruct;
	CSSM_DATA_PTR snValue = copyFirstFieldValue(snOid);

	getEmailAddresses(sanValues, snValue, emailAddresses);

	if (snValue && snValue->Data)
	{
		const CSSM_X509_NAME &x509Name = *(const CSSM_X509_NAME *)snValue->Data;
		printName = SecInferLabelFromX509Name(&x509Name);
	}

	if (printName == NULL)
	{
		/* If the we couldn't find a label use the emailAddress instead. */
		if (!emailAddresses.empty())
			printName = &emailAddresses[0];
		else
		{
			/* punt! */
			puntData.Data = (uint8 *)"X509 Certificate";
			puntData.Length = 16;
			printName = &puntData;
		}
	}

	/* If we couldn't find an email address just use the printName which might be the url or something else useful. */
	if (emailAddresses.empty())
		emailAddresses.push_back(CssmData::overlay(*printName));

	/* Do a check to see if a '\0' was at the end of printName and strip it. */
	CssmData cleanedUpPrintName(printName->Data, printName->Length);
	if (cleanedUpPrintName.Length && cleanedUpPrintName.Data[cleanedUpPrintName.Length - 1] == '\0')
		cleanedUpPrintName.Length--;

	/* What do we do with the inferred label - return it or add it mDbAttributes? */
	if (addLabel)
	{
		mDbAttributes->add(Schema::kX509CertificatePrintName, cleanedUpPrintName);
		CssmDbAttributeData &attrData = mDbAttributes->add(Schema::kX509CertificateAlias);

		/* Add the email addresses to attrData and normalize them. */
		uint32 ix = 0;
		for (std::vector<CssmData>::const_iterator it = emailAddresses.begin(); it != emailAddresses.end(); ++it, ++ix)
		{
			/* Add the email address using the allocator from mDbAttributes. */
			attrData.add(*it, *mDbAttributes);
			/* Normalize the emailAddresses in place since attrData already copied it. */
			normalizeEmailAddress(attrData.Value[ix]);
		}
	}

	if (rtnString)
	{
		/* Encoding is kCFStringEncodingUTF8 since the string is either
		   PRINTABLE_STRING, IA5_STRING, T61_STRING or PKIX_UTF8_STRING. */
		*rtnString = CFStringCreateWithBytes(NULL, cleanedUpPrintName.Data,
			(CFIndex)cleanedUpPrintName.Length, kCFStringEncodingUTF8, true);
	}

	// Clean up
	if (snValue)
		releaseFieldValue(snOid, snValue);
	if (sanValues)
		releaseFieldValues(sanOid, sanValues);
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
	mDbAttributes->add(Schema::attributeInfo(kSecPublicKeyHashItemAttr), publicKeyHash());
	inferLabel(true);
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

const CSSM_X509_ALGORITHM_IDENTIFIER *
Certificate::algorithmID()
{
	if (!mV1SubjectPublicKeyCStructValue)
		mV1SubjectPublicKeyCStructValue = copyFirstFieldValue(CSSMOID_X509V1SubjectPublicKeyCStruct);

	CSSM_X509_SUBJECT_PUBLIC_KEY_INFO *info = (CSSM_X509_SUBJECT_PUBLIC_KEY_INFO *)mV1SubjectPublicKeyCStructValue->Data;
	CSSM_X509_ALGORITHM_IDENTIFIER *algid = &info->algorithm;
	return algid;
}

CFStringRef
Certificate::commonName()
{
	CFStringRef rtnString;
	const CSSM_OID &fieldOid = CSSMOID_X509V1SubjectNameCStruct;
	CSSM_DATA_PTR fieldValue = copyFirstFieldValue(fieldOid);
	CSSM_X509_NAME_PTR x509Name = (CSSM_X509_NAME_PTR)fieldValue->Data;
	const CSSM_DATA	*printValue = NULL;
	if (fieldValue && fieldValue->Data)
		printValue = findPrintableField(*x509Name, &CSSMOID_CommonName);

	if (printValue == NULL)
		rtnString = NULL;
	else
	{
		/* Encoding is kCFStringEncodingUTF8 since the string is either
		   PRINTABLE_STRING, IA5_STRING, T61_STRING or PKIX_UTF8_STRING. */
		rtnString = CFStringCreateWithBytes(NULL, printValue->Data,
			(CFIndex)printValue->Length, kCFStringEncodingUTF8, true);
	}

	releaseFieldValue(CSSMOID_X509V1SubjectNameCStruct, fieldValue);

	return rtnString;
}

/*
 * Return a CFString containing the first email addresses for this certificate, based on the 
 * X509 SubjectAltName and SubjectName.
 */
CFStringRef
Certificate::copyFirstEmailAddress()
{
	CFStringRef rtnString;

	const CSSM_OID &sanOid = CSSMOID_SubjectAltName;
	CSSM_DATA_PTR *sanValues = copyFieldValues(sanOid);
	const CSSM_OID &snOid = CSSMOID_X509V1SubjectNameCStruct;
	CSSM_DATA_PTR snValue = copyFirstFieldValue(snOid);
	std::vector<CssmData> emailAddresses;

	getEmailAddresses(sanValues, snValue, emailAddresses);
	if (emailAddresses.empty())
		rtnString = NULL;
	else
	{
		/* Encoding is kCFStringEncodingUTF8 since the string is either
		   PRINTABLE_STRING, IA5_STRING, T61_STRING or PKIX_UTF8_STRING. */
		rtnString = CFStringCreateWithBytes(NULL, emailAddresses[0].Data,
			(CFIndex)emailAddresses[0].Length, kCFStringEncodingUTF8, true);
	}

	// Clean up
	if (snValue)
		releaseFieldValue(snOid, snValue);
	if (sanValues)
		releaseFieldValues(sanOid, sanValues);

	return rtnString;
}

/*
 * Return a CFArray containing the email addresses for this certificate, based on the 
 * X509 SubjectAltName and SubjectName.
 */
CFArrayRef
Certificate::copyEmailAddresses()
{
	CFMutableArrayRef array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	std::vector<CssmData> emailAddresses;

	// Find the SubjectAltName fields, if any, and extract all the GNT_RFC822Name entries from all of them
	const CSSM_OID &sanOid = CSSMOID_SubjectAltName;
	CSSM_DATA_PTR *sanValues = copyFieldValues(sanOid);

	const CSSM_OID &snOid = CSSMOID_X509V1SubjectNameCStruct;
	CSSM_DATA_PTR snValue = copyFirstFieldValue(snOid);

	getEmailAddresses(sanValues, snValue, emailAddresses);

	for (std::vector<CssmData>::const_iterator it = emailAddresses.begin(); it != emailAddresses.end(); ++it)
	{
		/* Encoding is kCFStringEncodingUTF8 since the string is either
		   PRINTABLE_STRING, IA5_STRING, T61_STRING or PKIX_UTF8_STRING. */
		CFStringRef string = CFStringCreateWithBytes(NULL, it->Data, static_cast<CFIndex>(it->Length), kCFStringEncodingUTF8, true);
		CFArrayAppendValue(array, string);
		CFRelease(string);
	}

	// Clean up
	if (snValue)
		releaseFieldValue(snOid, snValue);
	if (sanValues)
		releaseFieldValues(sanOid, sanValues);

	return array;
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
Certificate::copyTo(const Keychain &keychain, Access *newAccess)
{
	/* Certs can't have access controls. */
	if (newAccess)
		MacOSError::throwMe(errSecNoAccessForItem);

	Item item(new Certificate(data(), type(), encoding()));
	keychain->add(item);
	return item;
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
		keychain->resetSchema();

		mUniqueId = db->insert(recordType, mDbAttributes.get(), mData.get());
	}

	mPrimaryKey = keychain->makePrimaryKey(recordType, mUniqueId);
    mKeychain = keychain;

	return mPrimaryKey;
}

SecPointer<KeyItem>
Certificate::publicKey()
{
	SecPointer<KeyItem> keyItem;
	// Return a CSSM_DATA_PTR with the value of the first field specified by field.
	// Caller must call releaseFieldValue to free the storage allocated by this call.
	// call OSStatus SecKeyGetCSSMKey(SecKeyRef key, const CSSM_KEY **cssmKey); to retrieve

	CSSM_DATA_PTR keyPtr = copyFirstFieldValue(CSSMOID_CSSMKeyStruct);
	if (keyPtr && keyPtr->Data)
	{
		CssmClient::CSP csp(gGuidAppleCSP);
		CssmKey *cssmKey = reinterpret_cast<CssmKey *>(keyPtr->Data);
		CssmClient::Key key(csp, *cssmKey);
		keyItem = new KeyItem(key);
		// Clear out KeyData since KeyItem() takes over ownership of the key, and we don't want it getting released.
		cssmKey->KeyData.Data = NULL;
		cssmKey->KeyData.Length = 0;
	}

	releaseFieldValue(CSSMOID_CSSMKeyStruct, keyPtr);

	return keyItem;
}

KCCursor
Certificate::cursorForIssuerAndSN(const StorageManager::KeychainList &keychains, const CssmData &issuer, const CssmData &serialNumber)
{
	CssmAutoData fieldValue(CssmAllocator::standard(CssmAllocator::normal));
	uint32 numFields;

    // We need to decode issuer, normalize it, then re-encode it
	if (!getField_normRDN_NSS(issuer, numFields, fieldValue))
		MacOSError::throwMe(errSecDataNotAvailable);

	// Code basically copied from SecKeychainSearchCreateFromAttributes and SecKeychainSearchCopyNext:
	KCCursor cursor(keychains, kSecCertificateItemClass, NULL);
	cursor->conjunctive(CSSM_DB_AND);
    cursor->add(CSSM_DB_EQUAL, Schema::kX509CertificateIssuer, fieldValue.get());
	cursor->add(CSSM_DB_EQUAL, Schema::kX509CertificateSerialNumber, serialNumber);

	return cursor;
}

KCCursor
Certificate::cursorForSubjectKeyID(const StorageManager::KeychainList &keychains, const CssmData &subjectKeyID)
{
	KCCursor cursor(keychains, kSecCertificateItemClass, NULL);
	cursor->conjunctive(CSSM_DB_AND);
	cursor->add(CSSM_DB_EQUAL, Schema::kX509CertificateSubjectKeyIdentifier, subjectKeyID);

	return cursor;
}

KCCursor
Certificate::cursorForEmail(const StorageManager::KeychainList &keychains, const char *emailAddress)
{
	KCCursor cursor(keychains, kSecCertificateItemClass, NULL);
	if (emailAddress)
	{
		cursor->conjunctive(CSSM_DB_AND);
		CssmSelectionPredicate &pred = cursor->add(CSSM_DB_EQUAL, Schema::kX509CertificateAlias, emailAddress);
		/* Normalize the emailAddresses in place since cursor already copied it. */
		normalizeEmailAddress(pred.Attribute.Value[0]);
	}

	return cursor;
}

SecPointer<Certificate>
Certificate::findByIssuerAndSN(const StorageManager::KeychainList &keychains, const CssmData &issuer, const CssmData &serialNumber)
{
	Item item;
	if (!cursorForIssuerAndSN(keychains, issuer, serialNumber)->next(item))
		CssmError::throwMe(errSecItemNotFound);

	return static_cast<Certificate *>(&*item);
}

SecPointer<Certificate>
Certificate::findBySubjectKeyID(const StorageManager::KeychainList &keychains, const CssmData &subjectKeyID)
{
	Item item;
	if (!cursorForSubjectKeyID(keychains, subjectKeyID)->next(item))
		CssmError::throwMe(errSecItemNotFound);

	return static_cast<Certificate *>(&*item);
}

SecPointer<Certificate>
Certificate::findByEmail(const StorageManager::KeychainList &keychains, const char *emailAddress)
{
	Item item;
	if (!cursorForEmail(keychains, emailAddress)->next(item))
		CssmError::throwMe(errSecItemNotFound);

	return static_cast<Certificate *>(&*item);
}

/* Normalize emailAddresses in place. */
void
Certificate::normalizeEmailAddress(CSSM_DATA &emailAddress)
{
	/* Do a check to see if a '\0' was at the end of emailAddress and strip it. */
	if (emailAddress.Length && emailAddress.Data[emailAddress.Length - 1] == '\0')
		emailAddress.Length--;
	bool foundAt = false;
	for (uint32 ix = 0; ix < emailAddress.Length; ++ix)
	{
		uint8 ch = emailAddress.Data[ix];
		if (foundAt)
		{
			if ('A' <= ch && ch <= 'Z')
				emailAddress.Data[ix] = ch + 'a' - 'A';
		}
		else if (ch == '@')
			foundAt = true;
	}
}

void
Certificate::getEmailAddresses(CSSM_DATA_PTR *sanValues, CSSM_DATA_PTR snValue, std::vector<CssmData> &emailAddresses)
{
	// Get the email addresses for this certificate, based on the 
	// X509 SubjectAltName and SubjectName.

	// Find the SubjectAltName fields, if any, and extract all the GNT_RFC822Name entries from all of them
	if (sanValues)
	{
		for (CSSM_DATA_PTR *sanIx = sanValues; *sanIx; ++sanIx)
		{
			CSSM_DATA_PTR sanValue = *sanIx;
			if (sanValue && sanValue->Data)
			{
				CSSM_X509_EXTENSION *cssmExt = (CSSM_X509_EXTENSION *)sanValue->Data;
				CE_GeneralNames *parsedValue = (CE_GeneralNames *)cssmExt->value.parsedValue;
		
				/* Grab all the values that are of type GNT_RFC822Name. */
				for (uint32 i = 0; i < parsedValue->numNames; ++i)
				{
					if (parsedValue->generalName[i].nameType == GNT_RFC822Name)
					{
						if (parsedValue->generalName[i].berEncoded) // can't handle this
							continue;
		
						emailAddresses.push_back(CssmData::overlay(parsedValue->generalName[i].name));
					}
				}
			}
		}
	}

	if (emailAddresses.empty() && snValue && snValue->Data)
	{
		const CSSM_X509_NAME &x509Name = *(const CSSM_X509_NAME *)snValue->Data;
		for (uint32 rdnDex = 0; rdnDex < x509Name.numberOfRDNs; rdnDex++)
		{
			const CSSM_X509_RDN *rdnPtr = 
				&x509Name.RelativeDistinguishedName[rdnDex];
			for (uint32 tvpDex = 0; tvpDex < rdnPtr->numberOfPairs; tvpDex++)
			{
				const CSSM_X509_TYPE_VALUE_PAIR *tvpPtr = 
					&rdnPtr->AttributeTypeAndValue[tvpDex];
			
				/* type/value pair: match caller's specified type? */
				if (((tvpPtr->type.Length != CSSMOID_EmailAddress.Length) ||
					memcmp(tvpPtr->type.Data, CSSMOID_EmailAddress.Data, CSSMOID_EmailAddress.Length))) {
						continue;
				}

				/* printable? */
				switch (tvpPtr->valueType)
				{
					case BER_TAG_PRINTABLE_STRING:
					case BER_TAG_IA5_STRING:
					case BER_TAG_T61_STRING:
					case BER_TAG_PKIX_UTF8_STRING:
						/* success */
						emailAddresses.push_back(CssmData::overlay(tvpPtr->value));
						break;
					default:
						break;
				}
			}	/* for each pair */
		}		/* for each RDN */
	}
}
