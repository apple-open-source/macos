/*
 * Copyright (c) 2002-2007 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

//
// Certificate.cpp
//
#include <security_keychain/Certificate.h>
#include <security_cdsa_utilities/Schema.h>
#include <Security/oidscert.h>
#include <Security/oidsattr.h>
#include <Security/SecCertificate.h>
#include <Security/SecCertificatePriv.h>
#include <security_cdsa_client/cspclient.h>
#include <security_keychain/KeyItem.h>
#include <security_keychain/KCCursor.h>
#include <vector>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>
//#include "CLFieldsCommon.h"


using namespace KeychainCore;

CL
Certificate::clForType(CSSM_CERT_TYPE type)
{
	return CL(gGuidAppleX509CL);
}

Certificate::Certificate(const CSSM_DATA &data, CSSM_CERT_TYPE type, CSSM_CERT_ENCODING encoding) :
	ItemImpl(CSSM_DL_DB_RECORD_X509_CERTIFICATE, reinterpret_cast<SecKeychainAttributeList *>(NULL), UInt32(data.Length), reinterpret_cast<const void *>(data.Data)),
	mHaveTypeAndEncoding(true),
	mPopulated(false),
    mType(type),
    mEncoding(encoding),
    mCL(clForType(type)),
	mCertHandle(0),
	mV1SubjectPublicKeyCStructValue(NULL),
    mV1SubjectNameCStructValue(NULL),
    mV1IssuerNameCStructValue(NULL)
{
	if (data.Length == 0 || data.Data == NULL)
		MacOSError::throwMe(paramErr);
}

// db item constructor
Certificate::Certificate(const Keychain &keychain, const PrimaryKey &primaryKey, const CssmClient::DbUniqueRecord &uniqueId) :
	ItemImpl(keychain, primaryKey, uniqueId),
	mHaveTypeAndEncoding(false),
	mPopulated(false),
    mCL(NULL),
	mCertHandle(0),
	mV1SubjectPublicKeyCStructValue(NULL),
    mV1SubjectNameCStructValue(NULL),
    mV1IssuerNameCStructValue(NULL)
{
}



Certificate* Certificate::make(const Keychain &keychain, const PrimaryKey &primaryKey, const CssmClient::DbUniqueRecord &uniqueId)
{
	Certificate* c = new Certificate(keychain, primaryKey, uniqueId);
	keychain->addItem(primaryKey, c);
	return c;
}



Certificate* Certificate::make(const Keychain &keychain, const PrimaryKey &primaryKey)
{
	Certificate* c = new Certificate(keychain, primaryKey);
	keychain->addItem(primaryKey, c);
	return c;
}




// PrimaryKey item constructor
Certificate::Certificate(const Keychain &keychain, const PrimaryKey &primaryKey) :
	ItemImpl(keychain, primaryKey),
	mHaveTypeAndEncoding(false),
	mPopulated(false),
    mCL(NULL),
	mCertHandle(0),
	mV1SubjectPublicKeyCStructValue(NULL),
    mV1SubjectNameCStructValue(NULL),
    mV1IssuerNameCStructValue(NULL)
{
	// @@@ In this case we don't know the type...
}

Certificate::Certificate(Certificate &certificate) :
	ItemImpl(certificate),
	mHaveTypeAndEncoding(certificate.mHaveTypeAndEncoding),
	mPopulated(false /* certificate.mPopulated */),
    mType(certificate.mType),
    mEncoding(certificate.mEncoding),
    mCL(certificate.mCL),
	mCertHandle(0),
	mV1SubjectPublicKeyCStructValue(NULL),
    mV1SubjectNameCStructValue(NULL),
    mV1IssuerNameCStructValue(NULL)
{
}

Certificate::~Certificate() throw()
{
	if (mV1SubjectPublicKeyCStructValue)
		releaseFieldValue(CSSMOID_X509V1SubjectPublicKeyCStruct, mV1SubjectPublicKeyCStructValue);

	if (mCertHandle && mCL)
		CSSM_CL_CertAbortCache(mCL->handle(), mCertHandle);

    if (mV1SubjectNameCStructValue)
        releaseFieldValue(CSSMOID_X509V1SubjectNameCStruct, mV1SubjectNameCStructValue);

    if (mV1IssuerNameCStructValue)
        releaseFieldValue(CSSMOID_X509V1IssuerNameCStruct, mV1IssuerNameCStructValue);
}

CSSM_HANDLE
Certificate::certHandle()
{
	StLock<Mutex>_(mMutex);
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
	StLock<Mutex>_(mMutex);
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

	CSSM_CL_CertAbortQuery(clh, resultsHandle);

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
	StLock<Mutex>_(mMutex);
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
	StLock<Mutex>_(mMutex);
	CSSM_DATA_PTR *fieldValues = copyFieldValues(field);
	if (fieldValues)
	{
		CssmDbAttributeData &anAttr = mDbAttributes->add(info);
		for (int ix = 0; fieldValues[ix]; ++ix)
			anAttr.add(*fieldValues[ix], *mDbAttributes);

		releaseFieldValues(field, fieldValues);
	}
}

void
Certificate::addSubjectKeyIdentifier()
{
	StLock<Mutex>_(mMutex);
	const CSSM_DB_ATTRIBUTE_INFO &info = Schema::attributeInfo(kSecSubjectKeyIdentifierItemAttr);
	const CSSM_OID &field = CSSMOID_SubjectKeyIdentifier;

	CSSM_DATA_PTR *fieldValues = copyFieldValues(field);
	if (fieldValues)
	{
		CssmDbAttributeData &anAttr = mDbAttributes->add(info);
		for (int ix = 0; fieldValues[ix]; ++ix)
		{
			const CSSM_X509_EXTENSION *extension = reinterpret_cast<const CSSM_X509_EXTENSION *>(fieldValues[ix]->Data);
			if (extension == NULL || fieldValues[ix]->Length != sizeof(CSSM_X509_EXTENSION))
			{
				assert(extension != NULL && fieldValues[ix]->Length == sizeof(CSSM_X509_EXTENSION));
				continue;
			}
			const CE_SubjectKeyID *skid = reinterpret_cast<CE_SubjectKeyID *>(extension->value.parsedValue);
			if (skid == NULL)
			{
				assert(skid != NULL);
				continue;
			}
			anAttr.add(*skid, *mDbAttributes);
		}

		releaseFieldValues(field, fieldValues);
	}
}

/* Return a CSSM_DATA_PTR with the value of the first field specified by field.  Caller must call releaseFieldValue to free the storage allocated by this call.  */
CSSM_DATA_PTR
Certificate::copyFirstFieldValue(const CSSM_OID &field)
{
	StLock<Mutex>_(mMutex);
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
	StLock<Mutex>_(mMutex);
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
	StLock<Mutex>_(mMutex);
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

const CssmData &
Certificate::subjectKeyIdentifier()
{
	StLock<Mutex>_(mMutex);
	if (mSubjectKeyID.Length)
		return mSubjectKeyID;

	CSSM_DATA_PTR fieldValue = copyFirstFieldValue(CSSMOID_SubjectKeyIdentifier);
	if (fieldValue && fieldValue->Data && fieldValue->Length == sizeof(CSSM_X509_EXTENSION))
	{
		const CSSM_X509_EXTENSION *extension = reinterpret_cast<const CSSM_X509_EXTENSION *>(fieldValue->Data);
		const CE_SubjectKeyID *skid = reinterpret_cast<CE_SubjectKeyID *>(extension->value.parsedValue);	// CSSM_DATA

		if (skid->Length <= sizeof(mSubjectKeyIDBytes))
		{
			mSubjectKeyID.Data = mSubjectKeyIDBytes;
			mSubjectKeyID.Length = skid->Length;
			memcpy(mSubjectKeyID.Data, skid->Data, skid->Length);
		}
		else
			mSubjectKeyID.Length = 0;
	}

	releaseFieldValue(CSSMOID_SubjectKeyIdentifier, fieldValue);

	return mSubjectKeyID;
}


/*
 * Given an CSSM_X509_NAME, Find the first (or last) name/value pair with
 * a printable value which matches the specified OID (e.g., CSSMOID_CommonName).
 * Returns the CFString-style encoding associated with name component's BER tag.
 * Returns NULL if none found.
 */
static const CSSM_DATA *
findPrintableField(
	const CSSM_X509_NAME &x509Name,
	const CSSM_OID *tvpType,				// NULL means "any printable field"
	bool lastInstance,						// false means return first instance
	CFStringBuiltInEncodings *encoding)		// RETURNED
{
	const CSSM_DATA *result = NULL;
	for(uint32 rdnDex=0; rdnDex<x509Name.numberOfRDNs; rdnDex++) {
		const CSSM_X509_RDN *rdnPtr =
			&x509Name.RelativeDistinguishedName[rdnDex];
		for(uint32 tvpDex=0; tvpDex<rdnPtr->numberOfPairs; tvpDex++) {
			const CSSM_X509_TYPE_VALUE_PAIR *tvpPtr =
				&rdnPtr->AttributeTypeAndValue[tvpDex];

			/* type/value pair: match caller's specified type? */
			if(tvpType != NULL && tvpType->Data != NULL) {
				if(tvpPtr->type.Length != tvpType->Length) {
					continue;
				}
				if(memcmp(tvpPtr->type.Data, tvpType->Data, tvpType->Length)) {
					/* If we don't have a match but the requested OID is CSSMOID_UserID,
					 * look for a matching X.500 UserID OID: (0.9.2342.19200300.100.1.1)  */
					const char cssm_userid_oid[] = { 0x09,0x49,0x86,0x49,0x1f,0x12,0x8c,0xe4,0x81,0x81 };
					const char x500_userid_oid[] = { 0x09,0x92,0x26,0x89,0x93,0xF2,0x2C,0x64,0x01,0x01 };
					if(!(tvpType->Length == sizeof(cssm_userid_oid) &&
						!memcmp(tvpPtr->type.Data, x500_userid_oid, sizeof(x500_userid_oid)) &&
						!memcmp(tvpType->Data, cssm_userid_oid, sizeof(cssm_userid_oid)))) {
						continue;
					}
				}
			}

			/* printable? */
			switch(tvpPtr->valueType) {
				case BER_TAG_PRINTABLE_STRING:
				case BER_TAG_IA5_STRING:
					*encoding = kCFStringEncodingASCII;
					result = &tvpPtr->value;
					break;
				case BER_TAG_PKIX_UTF8_STRING:
				case BER_TAG_GENERAL_STRING:
				case BER_TAG_PKIX_UNIVERSAL_STRING:
					*encoding = kCFStringEncodingUTF8;
					result = &tvpPtr->value;
					break;
				case BER_TAG_T61_STRING:
				case BER_TAG_VIDEOTEX_STRING:
				case BER_TAG_ISO646_STRING:
					*encoding = kCFStringEncodingISOLatin1;
					result = &tvpPtr->value;
					break;
				case BER_TAG_PKIX_BMP_STRING:
					*encoding = kCFStringEncodingUnicode;
					result = &tvpPtr->value;
					break;
				default:
					/* not printable */
					break;
			}
			/* if we found a result and we want the first instance, return it now. */
			if(result && !lastInstance) {
				return result;
			}

		}	/* for each pair */
	}		/* for each RDN */

	/* result is NULL if no printable component was found */
	return result;
}

/*
 * Infer printable label for a given CSSM_X509_NAME. Returns NULL
 * if no appropriate printable name found. Returns the CFString-style
 * encoding associated with name component's BER tag. Also optionally
 * returns Description component and its encoding if present and the
 * returned name component was one we explicitly requested.
 */
static const CSSM_DATA *inferLabelFromX509Name(
	const CSSM_X509_NAME *x509Name,
	CFStringBuiltInEncodings *encoding,			// RETURNED
	const CSSM_DATA **description,				// optionally RETURNED
	CFStringBuiltInEncodings *descrEncoding)	// RETURNED if description != NULL
{
	const CSSM_DATA	*printValue;
	if(description != NULL) {
		*description = findPrintableField(*x509Name, &CSSMOID_Description, false, descrEncoding);
	}
	/*
	 * Search order (take the first one found with a printable
	 * value):
	 *  -- common name
	 *  -- Organizational Unit
	 *  -- Organization
	 *  -- email address
	 *  -- field of any kind
	 */
	printValue = findPrintableField(*x509Name, &CSSMOID_CommonName, true, encoding);
	if(printValue != NULL) {
		return printValue;
	}
	printValue = findPrintableField(*x509Name, &CSSMOID_OrganizationalUnitName, false, encoding);
	if(printValue != NULL) {
		return printValue;
	}
	printValue = findPrintableField(*x509Name, &CSSMOID_OrganizationName, false, encoding);
	if(printValue != NULL) {
		return printValue;
	}
	printValue = findPrintableField(*x509Name, &CSSMOID_EmailAddress, false, encoding);
	if(printValue != NULL) {
		return printValue;
	}
	/* if we didn't get one of the above names, don't append description */
	if(description != NULL) {
		*description = NULL;
	}
	/* take anything */
	return findPrintableField(*x509Name, NULL, false, encoding);
}

/*
 * Infer printable label for a given an CSSM_X509_NAME. Returns NULL
 * if no appropriate printable name found.
 */
const CSSM_DATA *SecInferLabelFromX509Name(
	const CSSM_X509_NAME *x509Name)
{
	/* callees of this routine don't care about the encoding */
	CFStringBuiltInEncodings encoding = kCFStringEncodingASCII;
	return inferLabelFromX509Name(x509Name, &encoding, NULL, &encoding);
}


void
Certificate::inferLabel(bool addLabel, CFStringRef *rtnString)
{
	StLock<Mutex>_(mMutex);
	// Set PrintName and optionally the Alias attribute for this certificate, based on the
	// X509 SubjectAltName and SubjectName.
	const CSSM_DATA *printName = NULL;
	const CSSM_DATA *description = NULL;
	std::vector<CssmData> emailAddresses;
	CSSM_DATA puntData;
	CssmAutoData printPlusDescr(Allocator::standard());
	CssmData printPlusDescData;
	CFStringBuiltInEncodings printEncoding = kCFStringEncodingUTF8;
	CFStringBuiltInEncodings descrEncoding = kCFStringEncodingUTF8;

	// Find the SubjectAltName fields, if any, and extract all the GNT_RFC822Name entries from all of them
	const CSSM_OID &sanOid = CSSMOID_SubjectAltName;
	CSSM_DATA_PTR *sanValues = copyFieldValues(sanOid);
	const CSSM_OID &snOid = CSSMOID_X509V1SubjectNameCStruct;
	CSSM_DATA_PTR snValue = copyFirstFieldValue(snOid);

	getEmailAddresses(sanValues, snValue, emailAddresses);

	if (snValue && snValue->Data)
	{
		const CSSM_X509_NAME &x509Name = *(const CSSM_X509_NAME *)snValue->Data;
		printName = inferLabelFromX509Name(&x509Name, &printEncoding,
			&description, &descrEncoding);
        if (printName)
        {
            /* Don't ever use "Thawte Freemail Member" as the label for a cert.  Instead force
               a fall back on the email address. */
            const char tfm[] = "Thawte Freemail Member";
            if ( (printName->Length == sizeof(tfm) - 1) &&
			      !memcmp(printName->Data, tfm, sizeof(tfm) - 1)) {
                printName = NULL;
			}
        }
	}

	/* Do a check to see if a '\0' was at the end of printName and strip it. */
	CssmData cleanedUpPrintName;
	if((printName != NULL) &&
	   (printName->Length != 0) &&
	   (printEncoding != kCFStringEncodingISOLatin1) &&
	   (printEncoding != kCFStringEncodingUnicode) &&
	   (printName->Data[printName->Length - 1] == '\0')) {
		cleanedUpPrintName.Data = printName->Data;
		cleanedUpPrintName.Length = printName->Length - 1;
		printName = &cleanedUpPrintName;
	}

	if((printName != NULL) && (description != NULL) && (description->Length != 0))
	{
		/*
		 * Munge Print Name (which in this case is the CommonName) and Description
		 * together with the Description in parentheses. We convert from whatever
		 * format Print Name and Description are in to UTF8 here.
		 */
		CFRef<CFMutableStringRef> combo(CFStringCreateMutable(NULL, 0));
		CFRef<CFStringRef> cfPrint(CFStringCreateWithBytes(NULL, printName->Data,
			(CFIndex)printName->Length, printEncoding, true));
		CssmData cleanedUpDescr(description->Data, description->Length);
		if ((cleanedUpDescr.Data[cleanedUpDescr.Length - 1] == '\0') &&
			(descrEncoding != kCFStringEncodingISOLatin1) &&
			(descrEncoding != kCFStringEncodingUnicode)) {
			cleanedUpDescr.Length--;
		}
		CFRef<CFStringRef> cfDesc(CFStringCreateWithBytes(NULL, cleanedUpDescr.Data,
			(CFIndex)cleanedUpDescr.Length, descrEncoding, true));
		CFStringAppend(combo, cfPrint);
		CFStringAppendCString(combo, " (", kCFStringEncodingASCII);
		CFStringAppend(combo, cfDesc);
		CFStringAppendCString(combo, ")", kCFStringEncodingASCII);
		CFRef<CFDataRef> comboData(CFStringCreateExternalRepresentation(NULL, combo,
			kCFStringEncodingUTF8, 0));
		printPlusDescr.copy(CFDataGetBytePtr(comboData), CFDataGetLength(comboData));
		printPlusDescData = printPlusDescr;
		printName = &printPlusDescData;
		printEncoding = kCFStringEncodingUTF8;
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
		printEncoding = kCFStringEncodingUTF8;
	}

	/* If we couldn't find an email address just use the printName which might be the url or something else useful. */
	if (emailAddresses.empty())
		emailAddresses.push_back(CssmData::overlay(*printName));

	/* What do we do with the inferred label - return it or add it mDbAttributes? */
	if (addLabel)
	{
		mDbAttributes->add(Schema::kX509CertificatePrintName, *printName);
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
		CFStringBuiltInEncodings testEncoding = printEncoding;
		if(testEncoding == kCFStringEncodingISOLatin1) {
			// try UTF-8 first
			testEncoding = kCFStringEncodingUTF8;
		}
		*rtnString = CFStringCreateWithBytes(NULL, printName->Data,
			(CFIndex)printName->Length, testEncoding, true);
		if(*rtnString == NULL && printEncoding == kCFStringEncodingISOLatin1) {
			// string cannot be represented in UTF-8, fall back to ISO Latin 1
			*rtnString = CFStringCreateWithBytes(NULL, printName->Data,
				(CFIndex)printName->Length, printEncoding, true);
		}
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
	StLock<Mutex>_(mMutex);
	if (mPopulated)
		return;

	addParsedAttribute(Schema::attributeInfo(kSecSubjectItemAttr), CSSMOID_X509V1SubjectName);
	addParsedAttribute(Schema::attributeInfo(kSecIssuerItemAttr), CSSMOID_X509V1IssuerName);
	addParsedAttribute(Schema::attributeInfo(kSecSerialNumberItemAttr), CSSMOID_X509V1SerialNumber);

	addSubjectKeyIdentifier();

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

	mPopulated = true;
}

const CssmData &
Certificate::data()
{
	StLock<Mutex>_(mMutex);
	CssmDataContainer *data = mData.get();
	if (!data && mKeychain)
	{
	    // Make sure mUniqueId is set.
		dbUniqueRecord();
		CssmDataContainer _data;
		mData = NULL;
		/* new data allocated by CSPDL, implicitly freed by CssmDataContainer */
		mUniqueId->get(NULL, &_data);
		/* this saves a copy to be freed at destruction and to be passed to caller */
		setData(_data.length(), _data.data());
		return *mData.get();
	}

	// If the data hasn't been set we can't return it.
	if (!data)
		MacOSError::throwMe(errSecDataNotAvailable);

	return *data;
}

CSSM_CERT_TYPE
Certificate::type()
{
	StLock<Mutex>_(mMutex);
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
	StLock<Mutex>_(mMutex);
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

const CSSM_X509_ALGORITHM_IDENTIFIER_PTR
Certificate::algorithmID()
{
	StLock<Mutex>_(mMutex);
	if (!mV1SubjectPublicKeyCStructValue)
		mV1SubjectPublicKeyCStructValue = copyFirstFieldValue(CSSMOID_X509V1SubjectPublicKeyCStruct);

	CSSM_X509_SUBJECT_PUBLIC_KEY_INFO *info = (CSSM_X509_SUBJECT_PUBLIC_KEY_INFO *)mV1SubjectPublicKeyCStructValue->Data;
	CSSM_X509_ALGORITHM_IDENTIFIER *algid = &info->algorithm;
	return algid;
}

CFStringRef
Certificate::commonName()
{
	StLock<Mutex>_(mMutex);
	return distinguishedName(&CSSMOID_X509V1SubjectNameCStruct, &CSSMOID_CommonName);
}

CFStringRef
Certificate::distinguishedName(const CSSM_OID *sourceOid, const CSSM_OID *componentOid)
{
	StLock<Mutex>_(mMutex);
	CFStringRef rtnString = NULL;
	CSSM_DATA_PTR fieldValue = copyFirstFieldValue(*sourceOid);
	CSSM_X509_NAME_PTR x509Name = (CSSM_X509_NAME_PTR)fieldValue->Data;
	const CSSM_DATA	*printValue = NULL;
	CFStringBuiltInEncodings encoding;

	if (fieldValue && fieldValue->Data)
		printValue = findPrintableField(*x509Name, componentOid, true, &encoding);

	if (printValue)
		rtnString = CFStringCreateWithBytes(NULL, printValue->Data,
			CFIndex(printValue->Length), encoding, true);

	releaseFieldValue(*sourceOid, fieldValue);

	return rtnString;
}


/*
 * Return a CFString containing the first email addresses for this certificate, based on the
 * X509 SubjectAltName and SubjectName.
 */
CFStringRef
Certificate::copyFirstEmailAddress()
{
	StLock<Mutex>_(mMutex);
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
	StLock<Mutex>_(mMutex);
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

const CSSM_X509_NAME_PTR
Certificate::subjectName()
{
	StLock<Mutex>_(mMutex);
	if (!mV1SubjectNameCStructValue)
		if ((mV1SubjectNameCStructValue = copyFirstFieldValue(CSSMOID_X509V1SubjectNameCStruct)) == NULL)
            return NULL;

    return (const CSSM_X509_NAME_PTR)mV1SubjectNameCStructValue->Data;
}

const CSSM_X509_NAME_PTR
Certificate::issuerName()
{
	StLock<Mutex>_(mMutex);
	if (!mV1IssuerNameCStructValue)
		if ((mV1IssuerNameCStructValue = copyFirstFieldValue(CSSMOID_X509V1IssuerNameCStruct)) == NULL)
            return NULL;

    return (const CSSM_X509_NAME_PTR)mV1IssuerNameCStructValue->Data;
}

CSSM_CL_HANDLE
Certificate::clHandle()
{
	StLock<Mutex>_(mMutex);
	if (!mCL)
		mCL = clForType(type());

	return mCL->handle();
}

bool
Certificate::operator < (Certificate &other)
{
	// Certificates in different keychains are considered equal if data is equal
	// Note that the Identity '<' operator relies on this assumption.
	return data() < other.data();
}

bool
Certificate::operator == (Certificate &other)
{
	// Certificates in different keychains are considered equal if data is equal
	// Note that the Identity '==' operator relies on this assumption.
	return data() == other.data();
}

bool
Certificate::equal(SecCFObject &other)
{
    return (*this) == (Certificate &)other;
}

void
Certificate::update()
{
	ItemImpl::update();
}

Item
Certificate::copyTo(const Keychain &keychain, Access *newAccess)
{
	StLock<Mutex>_(mMutex);
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
	StLock<Mutex>_(mMutex);
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
		if (e.osStatus() != CSSMERR_DL_INVALID_RECORDTYPE)
			throw;

		// Create the cert relation and try again.
		db->createRelation(CSSM_DL_DB_RECORD_X509_CERTIFICATE,
			"CSSM_DL_DB_RECORD_X509_CERTIFICATE",
			Schema::X509CertificateSchemaAttributeCount,
			Schema::X509CertificateSchemaAttributeList,
			Schema::X509CertificateSchemaIndexCount,
			Schema::X509CertificateSchemaIndexList);
		keychain->keychainSchema()->didCreateRelation(
			CSSM_DL_DB_RECORD_X509_CERTIFICATE,
			"CSSM_DL_DB_RECORD_X509_CERTIFICATE",
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

SecPointer<KeyItem>
Certificate::publicKey()
{
	StLock<Mutex>_(mMutex);
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

// This function "borrowed" from the X509 CL, which is (currently) linked into
// the Security.framework as a built-in plugin.
extern "C" bool getField_normRDN_NSS (
	const CSSM_DATA		&derName,
	uint32				&numFields,		// RETURNED (if successful, 0 or 1)
	CssmOwnedData		&fieldValue);	// RETURNED

KCCursor
Certificate::cursorForIssuerAndSN(const StorageManager::KeychainList &keychains, const CssmData &issuer, const CssmData &serialNumber)
{
	CssmAutoData fieldValue(Allocator::standard(Allocator::normal));
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
Certificate::cursorForIssuerAndSN_CF(const StorageManager::KeychainList &keychains, CFDataRef issuer, CFDataRef serialNumber)
{
	// This assumes a normalized issuer
	CSSM_DATA issuerCSSM, serialNumberCSSM;

	issuerCSSM.Length = CFDataGetLength(issuer);
	issuerCSSM.Data = const_cast<uint8 *>(CFDataGetBytePtr(issuer));

	serialNumberCSSM.Length = CFDataGetLength(serialNumber);
	serialNumberCSSM.Data = const_cast<uint8 *>(CFDataGetBytePtr(serialNumber));

	// Code basically copied from SecKeychainSearchCreateFromAttributes and SecKeychainSearchCopyNext:
	KCCursor cursor(keychains, kSecCertificateItemClass, NULL);
	cursor->conjunctive(CSSM_DB_AND);
	cursor->add(CSSM_DB_EQUAL, Schema::kX509CertificateIssuer, issuerCSSM);
	cursor->add(CSSM_DB_EQUAL, Schema::kX509CertificateSerialNumber, serialNumberCSSM);

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
Certificate::findInKeychain(const StorageManager::KeychainList &keychains)
{
	StLock<Mutex>_(mMutex);
	const CSSM_OID &issuerOid = CSSMOID_X509V1IssuerName;
	CSSM_DATA_PTR issuerPtr = copyFirstFieldValue(issuerOid);
	CssmData issuer(issuerPtr->Data, issuerPtr->Length);

	const CSSM_OID &serialOid = CSSMOID_X509V1SerialNumber;
	CSSM_DATA_PTR serialPtr = copyFirstFieldValue(serialOid);
	CssmData serial(serialPtr->Data, serialPtr->Length);

	SecPointer<Certificate> foundCert = NULL;
	try {
		foundCert = findByIssuerAndSN(keychains, issuer, serial);
	} catch (...) {
		foundCert = NULL;
	}

	releaseFieldValue(issuerOid, issuerPtr);
	releaseFieldValue(serialOid, serialPtr);

	return foundCert;
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

void Certificate::willRead()
{
	populateAttributes();
}

Boolean Certificate::isSelfSigned()
{
	StLock<Mutex>_(mMutex);
	CSSM_DATA_PTR issuer = NULL;
	CSSM_DATA_PTR subject = NULL;
	OSStatus ortn = noErr;
	Boolean brtn = false;

	issuer  = copyFirstFieldValue(CSSMOID_X509V1IssuerNameStd);
	subject = copyFirstFieldValue(CSSMOID_X509V1SubjectNameStd);
	if((issuer == NULL) || (subject == NULL)) {
		ortn = paramErr;
	}
	else if((issuer->Length == subject->Length) &&
		!memcmp(issuer->Data, subject->Data, issuer->Length)) {
		brtn = true;
	}
	if(brtn) {
		/* names match: verify signature */
		CSSM_RETURN crtn;
		CSSM_DATA certData = data();
		crtn = CSSM_CL_CertVerify(clHandle(), 0,
			&certData, &certData, NULL, 0);
		if(crtn) {
			brtn = false;
		}
	}
	if(issuer) {
		releaseFieldValue(CSSMOID_X509V1IssuerNameStd,	issuer);
	}
	if(subject) {
		releaseFieldValue(CSSMOID_X509V1SubjectNameStd, subject);
	}
	if(ortn) {
		MacOSError::throwMe(ortn);
	}
	return brtn;
}
