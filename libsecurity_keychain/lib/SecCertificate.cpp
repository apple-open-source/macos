/*
 * Copyright (c) 2002-2004 Apple Computer, Inc. All Rights Reserved.
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

#include <Security/SecCertificate.h>
#include <Security/SecCertificatePriv.h>
#include <security_keychain/Certificate.h>
#include <security_keychain/Item.h>
#include <security_keychain/KCCursor.h>
#include <Security/cssmapi.h>
#include <Security/cssmapple.h>
#include <security_cdsa_client/cspclient.h>
#include <security_cdsa_client/clclient.h>
#include <security_cdsa_client/tpclient.h>
#include <Security/cssmtype.h>

#include "SecBridge.h"

// %%% used by SecCertificate{Copy,Set}Preference
#include <Security/SecKeychainItemPriv.h>
#include <Security/SecIdentityPriv.h>
#include <security_keychain/KCCursor.h>
#include <security_cdsa_utilities/Schema.h>
#include <sys/param.h>

using namespace CssmClient;

CFTypeID
SecCertificateGetTypeID(void)
{
	BEGIN_SECAPI

	return gTypes().Certificate.typeID;

	END_SECAPI1(_kCFRuntimeNotATypeID)
}


OSStatus
SecCertificateCreateFromData(const CSSM_DATA *data, CSSM_CERT_TYPE type, CSSM_CERT_ENCODING encoding, SecCertificateRef *certificate)
{
	BEGIN_SECAPI

	SecPointer<Certificate> certificatePtr(new Certificate(Required(data), type, encoding));
	Required(certificate) = certificatePtr->handle();

	END_SECAPI
}

/* new in 10.6 */
SecCertificateRef
SecCertificateCreateWithData(CFAllocatorRef allocator, CFDataRef data)
{
	SecCertificateRef certificate = NULL;
    OSStatus __secapiresult;
	try {
		CSSM_DATA cssmCertData;
		cssmCertData.Length = (data) ? (CSSM_SIZE)CFDataGetLength(data) : 0;
		cssmCertData.Data = (data) ? (uint8 *)CFDataGetBytePtr(data) : NULL;

		//NOTE: there isn't yet a Certificate constructor which accepts a CFAllocatorRef
		SecPointer<Certificate> certificatePtr(new Certificate(cssmCertData, CSSM_CERT_X_509v3, CSSM_CERT_ENCODING_DER));
		certificate = certificatePtr->handle();
	}
	catch (const MacOSError &err) { __secapiresult=err.osStatus(); }
	catch (const CommonError &err) { __secapiresult=SecKeychainErrFromOSStatus(err.osStatus()); }
	catch (const std::bad_alloc &) { __secapiresult=memFullErr; }
	catch (...) { __secapiresult=internalComponentErr; }
    return certificate;
}

OSStatus
SecCertificateAddToKeychain(SecCertificateRef certificate, SecKeychainRef keychain)
{
	BEGIN_SECAPI

	Item item(Certificate::required(certificate));
	Keychain k = Keychain::optional(keychain);
	k->add(item);

	END_SECAPI
}

OSStatus
SecCertificateGetData(SecCertificateRef certificate, CSSM_DATA_PTR data)
{
	BEGIN_SECAPI

	Required(data) = Certificate::required(certificate)->data();

	END_SECAPI
}

/* new in 10.6 */
CFDataRef
SecCertificateCopyData(SecCertificateRef certificate)
{
	CFDataRef data = NULL;
    OSStatus __secapiresult;
	try {
		CssmData output = Certificate::required(certificate)->data();
		CFIndex length = (CFIndex)output.length();
		const UInt8 *bytes = (const UInt8 *)output.data();
		if (length && bytes) {
			data = CFDataCreate(NULL, bytes, length);
		}
	}
	catch (const MacOSError &err) { __secapiresult=err.osStatus(); }
	catch (const CommonError &err) { __secapiresult=SecKeychainErrFromOSStatus(err.osStatus()); }
	catch (const std::bad_alloc &) { __secapiresult=memFullErr; }
	catch (...) { __secapiresult=internalComponentErr; }
    return data;
}

OSStatus
SecCertificateGetType(SecCertificateRef certificate, CSSM_CERT_TYPE *certificateType)
{
    BEGIN_SECAPI

	Required(certificateType) = Certificate::required(certificate)->type();

    END_SECAPI
}


OSStatus
SecCertificateGetSubject(SecCertificateRef certificate, const CSSM_X509_NAME **subject)
{
    BEGIN_SECAPI

    Required(subject) = Certificate::required(certificate)->subjectName();

    END_SECAPI
}


OSStatus
SecCertificateGetIssuer(SecCertificateRef certificate, const CSSM_X509_NAME **issuer)
{
    BEGIN_SECAPI

	Required(issuer) = Certificate::required(certificate)->issuerName();

    END_SECAPI
}


OSStatus
SecCertificateGetCLHandle(SecCertificateRef certificate, CSSM_CL_HANDLE *clHandle)
{
    BEGIN_SECAPI

	Required(clHandle) = Certificate::required(certificate)->clHandle();

    END_SECAPI
}

/*
 * Private API to infer a display name for a SecCertificateRef which
 * may or may not be in a keychain.
 */
OSStatus
SecCertificateInferLabel(SecCertificateRef certificate, CFStringRef *label)
{
    BEGIN_SECAPI

	Certificate::required(certificate)->inferLabel(false,
		&Required(label));

    END_SECAPI
}

OSStatus
SecCertificateCopyPublicKey(SecCertificateRef certificate, SecKeyRef *key)
{
    BEGIN_SECAPI

	Required(key) = Certificate::required(certificate)->publicKey()->handle();

    END_SECAPI
}

OSStatus
SecCertificateGetAlgorithmID(SecCertificateRef certificate, const CSSM_X509_ALGORITHM_IDENTIFIER **algid)
{
    BEGIN_SECAPI

	Required(algid) = Certificate::required(certificate)->algorithmID();

    END_SECAPI
}

OSStatus
SecCertificateCopyCommonName(SecCertificateRef certificate, CFStringRef *commonName)
{
    BEGIN_SECAPI

	Required(commonName) = Certificate::required(certificate)->commonName();

    END_SECAPI
}

/* new in 10.6 */
CFStringRef
SecCertificateCopySubjectSummary(SecCertificateRef certificate)
{
	CFStringRef summary = NULL;
    OSStatus __secapiresult;
	try {
		Certificate::required(certificate)->inferLabel(false, &summary);
	}
	catch (const MacOSError &err) { __secapiresult=err.osStatus(); }
	catch (const CommonError &err) { __secapiresult=SecKeychainErrFromOSStatus(err.osStatus()); }
	catch (const std::bad_alloc &) { __secapiresult=memFullErr; }
	catch (...) { __secapiresult=internalComponentErr; }
    return summary;
}

OSStatus
SecCertificateCopySubjectComponent(SecCertificateRef certificate, const CSSM_OID *component, CFStringRef *result)
{
    BEGIN_SECAPI

	Required(result) = Certificate::required(certificate)->distinguishedName(&CSSMOID_X509V1SubjectNameCStruct, component);

    END_SECAPI
}

OSStatus
SecCertificateGetCommonName(SecCertificateRef certificate, CFStringRef *commonName)
{
    // deprecated SPI signature; replaced by SecCertificateCopyCommonName
    return SecCertificateCopyCommonName(certificate, commonName);
}

OSStatus
SecCertificateGetEmailAddress(SecCertificateRef certificate, CFStringRef *emailAddress)
{
    BEGIN_SECAPI

	Required(emailAddress) = Certificate::required(certificate)->copyFirstEmailAddress();

    END_SECAPI
}

OSStatus
SecCertificateCopyEmailAddresses(SecCertificateRef certificate, CFArrayRef *emailAddresses)
{
    BEGIN_SECAPI

	Required(emailAddresses) = Certificate::required(certificate)->copyEmailAddresses();

    END_SECAPI
}

OSStatus
SecCertificateCopyFieldValues(SecCertificateRef certificate, const CSSM_OID *field, CSSM_DATA_PTR **fieldValues)
{
/* Return a zero terminated list of CSSM_DATA_PTR's with the values of the field specified by field.  Caller must call releaseFieldValues to free the storage allocated by this call.  */
    BEGIN_SECAPI

	Required(fieldValues) = Certificate::required(certificate)->copyFieldValues(Required(field));

    END_SECAPI
}

OSStatus
SecCertificateReleaseFieldValues(SecCertificateRef certificate, const CSSM_OID *field, CSSM_DATA_PTR *fieldValues)
{
    BEGIN_SECAPI

	Certificate::required(certificate)->releaseFieldValues(Required(field), fieldValues);

    END_SECAPI
}

OSStatus
SecCertificateCopyFirstFieldValue(SecCertificateRef certificate, const CSSM_OID *field, CSSM_DATA_PTR *fieldValue)
{
    BEGIN_SECAPI

	Required(fieldValue) = Certificate::required(certificate)->copyFirstFieldValue(Required(field));

    END_SECAPI
}

OSStatus
SecCertificateReleaseFirstFieldValue(SecCertificateRef certificate, const CSSM_OID *field, CSSM_DATA_PTR fieldValue)
{
    BEGIN_SECAPI

	Certificate::required(certificate)->releaseFieldValue(Required(field), fieldValue);

    END_SECAPI
}

OSStatus
SecCertificateFindByIssuerAndSN(CFTypeRef keychainOrArray,const CSSM_DATA *issuer,
	const CSSM_DATA *serialNumber, SecCertificateRef *certificate)
{
	BEGIN_SECAPI

	StorageManager::KeychainList keychains;
	globals().storageManager.optionalSearchList(keychainOrArray, keychains);
	Required(certificate) = Certificate::findByIssuerAndSN(keychains, CssmData::required(issuer), CssmData::required(serialNumber))->handle();

	END_SECAPI
}

OSStatus
SecCertificateFindBySubjectKeyID(CFTypeRef keychainOrArray, const CSSM_DATA *subjectKeyID,
	SecCertificateRef *certificate)
{
	BEGIN_SECAPI

	StorageManager::KeychainList keychains;
	globals().storageManager.optionalSearchList(keychainOrArray, keychains);
	Required(certificate) = Certificate::findBySubjectKeyID(keychains, CssmData::required(subjectKeyID))->handle();

	END_SECAPI
}

OSStatus
SecCertificateFindByEmail(CFTypeRef keychainOrArray, const char *emailAddress, SecCertificateRef *certificate)
{
	BEGIN_SECAPI

	StorageManager::KeychainList keychains;
	globals().storageManager.optionalSearchList(keychainOrArray, keychains);
	Required(certificate) = Certificate::findByEmail(keychains, emailAddress)->handle();

	END_SECAPI
}

OSStatus
SecKeychainSearchCreateForCertificateByIssuerAndSN(CFTypeRef keychainOrArray, const CSSM_DATA *issuer,
	const CSSM_DATA *serialNumber, SecKeychainSearchRef *searchRef)
{
    BEGIN_SECAPI

	Required(searchRef);

	StorageManager::KeychainList keychains;
	globals().storageManager.optionalSearchList(keychainOrArray, keychains);
	KCCursor cursor(Certificate::cursorForIssuerAndSN(keychains, CssmData::required(issuer), CssmData::required(serialNumber)));
	*searchRef = cursor->handle();

	END_SECAPI
}

OSStatus
SecKeychainSearchCreateForCertificateBySubjectKeyID(CFTypeRef keychainOrArray, const CSSM_DATA *subjectKeyID,
	SecKeychainSearchRef *searchRef)
{
    BEGIN_SECAPI

	Required(searchRef);

	StorageManager::KeychainList keychains;
	globals().storageManager.optionalSearchList(keychainOrArray, keychains);
	KCCursor cursor(Certificate::cursorForSubjectKeyID(keychains, CssmData::required(subjectKeyID)));
	*searchRef = cursor->handle();

	END_SECAPI
}

OSStatus
SecKeychainSearchCreateForCertificateByEmail(CFTypeRef keychainOrArray, const char *emailAddress,
	SecKeychainSearchRef *searchRef)
{
    BEGIN_SECAPI

	Required(searchRef);

	StorageManager::KeychainList keychains;
	globals().storageManager.optionalSearchList(keychainOrArray, keychains);
	KCCursor cursor(Certificate::cursorForEmail(keychains, emailAddress));
	*searchRef = cursor->handle();

	END_SECAPI
}

/* NOT EXPORTED YET; copied from SecurityInterface but could be useful in the future.
CSSM_CSP_HANDLE
SecGetAppleCSPHandle()
{
	BEGIN_SECAPI
	return CSP(gGuidAppleCSP)->handle();
	END_SECAPI1(NULL);
}

CSSM_CL_HANDLE
SecGetAppleCLHandle()
{
	BEGIN_SECAPI
	return CL(gGuidAppleX509CL)->handle();
	END_SECAPI1(NULL);
}
*/

CSSM_RETURN
SecDigestGetData (CSSM_ALGORITHMS alg, CSSM_DATA* digest, const CSSM_DATA* data)
{
	BEGIN_SECAPI
	// sanity checking
	if (!digest || !digest->Data || !digest->Length || !data || !data->Data || !data->Length)
		return paramErr;

	CSP csp(gGuidAppleCSP);
	Digest context(csp, alg);
	CssmData input(data->Data, data->Length);
	CssmData output(digest->Data, digest->Length);

	context.digest(input, output);
	digest->Length = output.length();

	return CSSM_OK;
	END_SECAPI1(1);
}

/* determine whether a cert is self-signed */
OSStatus SecCertificateIsSelfSigned(
	SecCertificateRef certificate,
	Boolean *isSelfSigned)		/* RETURNED */
{
    BEGIN_SECAPI

	*isSelfSigned = Certificate::required(certificate)->isSelfSigned();

	END_SECAPI
}

OSStatus SecCertificateCopyPreference(
    CFStringRef name,
    CSSM_KEYUSE keyUsage,
    SecCertificateRef *certificate)
{
    BEGIN_SECAPI

	Required(name);
	Required(certificate);
	StorageManager::KeychainList keychains;
	globals().storageManager.getSearchList(keychains);
	KCCursor cursor(keychains, kSecGenericPasswordItemClass, NULL);

	char idUTF8[MAXPATHLEN];
    if (!CFStringGetCString(name, idUTF8, sizeof(idUTF8)-1, kCFStringEncodingUTF8))
        idUTF8[0] = (char)'\0';
    CssmData service(const_cast<char *>(idUTF8), strlen(idUTF8));
    FourCharCode itemType = 'cprf';
    cursor->add(CSSM_DB_EQUAL, Schema::attributeInfo(kSecServiceItemAttr), service);
	cursor->add(CSSM_DB_EQUAL, Schema::attributeInfo(kSecTypeItemAttr), itemType);
    if (keyUsage)
        cursor->add(CSSM_DB_EQUAL, Schema::attributeInfo(kSecScriptCodeItemAttr), (sint32)keyUsage);

	Item prefItem;
	if (!cursor->next(prefItem))
		MacOSError::throwMe(errSecItemNotFound);

	// get persistent certificate reference
	SecKeychainAttribute itemAttrs[] = { { kSecGenericItemAttr, 0, NULL } };
	SecKeychainAttributeList itemAttrList = { sizeof(itemAttrs) / sizeof(itemAttrs[0]), itemAttrs };
	prefItem->getContent(NULL, &itemAttrList, NULL, NULL);

	// find certificate, given persistent reference data
	CFDataRef pItemRef = CFDataCreateWithBytesNoCopy(NULL, (const UInt8 *)itemAttrs[0].data, itemAttrs[0].length, kCFAllocatorNull);
	SecKeychainItemRef certItemRef = nil;
	OSStatus status = SecKeychainItemCopyFromPersistentReference(pItemRef, &certItemRef); //%%% need to make this a method of ItemImpl
	prefItem->freeContent(&itemAttrList, NULL);
	if (pItemRef)
		CFRelease(pItemRef);
	if (status)
		return status;

	*certificate = (SecCertificateRef)certItemRef;

    END_SECAPI
}

OSStatus SecCertificateSetPreference(
    SecCertificateRef certificate,
    CFStringRef name,
    CSSM_KEYUSE keyUsage,
    CFDateRef date)
{
    BEGIN_SECAPI

	if (!certificate || !name)
		MacOSError::throwMe(paramErr);

    // first look for existing preference, in case this is an update
	StorageManager::KeychainList keychains;
	globals().storageManager.getSearchList(keychains);
	KCCursor cursor(keychains, kSecGenericPasswordItemClass, NULL);

	char idUTF8[MAXPATHLEN];
    if (!CFStringGetCString(name, idUTF8, sizeof(idUTF8)-1, kCFStringEncodingUTF8))
        idUTF8[0] = (char)'\0';
    CssmData service(const_cast<char *>(idUTF8), strlen(idUTF8));
    FourCharCode itemType = 'cprf';
    cursor->add(CSSM_DB_EQUAL, Schema::attributeInfo(kSecServiceItemAttr), service);
	cursor->add(CSSM_DB_EQUAL, Schema::attributeInfo(kSecTypeItemAttr), itemType);
    if (keyUsage)
        cursor->add(CSSM_DB_EQUAL, Schema::attributeInfo(kSecScriptCodeItemAttr), (sint32)keyUsage);
    if (date)
        ; // %%%TBI

	Item item(kSecGenericPasswordItemClass, 'aapl', 0, NULL, false);
    bool add = (!cursor->next(item));

    // service (use provided string)
    item->setAttribute(Schema::attributeInfo(kSecServiceItemAttr), service);

    // label (use service string as default label)
    item->setAttribute(Schema::attributeInfo(kSecLabelItemAttr), service);

    // type
    item->setAttribute(Schema::attributeInfo(kSecTypeItemAttr), itemType);

    // account (use label of certificate)
    CFStringRef labelString = nil;
    OSStatus status = SecCertificateInferLabel(certificate, &labelString);
    if (!labelString || !CFStringGetCString(labelString, idUTF8, sizeof(idUTF8)-1, kCFStringEncodingUTF8))
        MacOSError::throwMe(errSecDataTooLarge);
    CssmData account(const_cast<void *>(reinterpret_cast<const void *>(idUTF8)), strlen(idUTF8));
    item->setAttribute(Schema::attributeInfo(kSecAccountItemAttr), account);
    CFRelease(labelString);

    // key usage (overload script code)
    if (keyUsage)
        item->setAttribute(Schema::attributeInfo(kSecScriptCodeItemAttr), (sint32)keyUsage);
    
    // date
    if (date)
        ; // %%%TBI

	// generic attribute (store persistent certificate reference)
	CFDataRef pItemRef = nil;
	status = SecKeychainItemCreatePersistentReference((SecKeychainItemRef)certificate, &pItemRef);
	if (!pItemRef)
		status = errSecInvalidItemRef;
	if (status)
		MacOSError::throwMe(status);
	const UInt8 *dataPtr = CFDataGetBytePtr(pItemRef);
	CFIndex dataLen = CFDataGetLength(pItemRef);
	CssmData pref(const_cast<void *>(reinterpret_cast<const void *>(dataPtr)), dataLen);
	item->setAttribute(Schema::attributeInfo(kSecGenericItemAttr), pref);
	CFRelease(pItemRef);

    if (add) {
        Keychain keychain = nil;
        try {
            keychain = globals().storageManager.defaultKeychain();
            if (!keychain->exists())
                MacOSError::throwMe(errSecNoSuchKeychain);	// Might be deleted or not available at this time.
        }
        catch(...) {
            keychain = globals().storageManager.defaultKeychainUI(item);
        }

        keychain->add(item);
    }
	item->update();

    END_SECAPI
}

