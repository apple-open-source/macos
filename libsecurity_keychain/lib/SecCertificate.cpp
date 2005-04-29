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


OSStatus
SecCertificateAddToKeychain(SecCertificateRef certificate, SecKeychainRef keychain)
{
	BEGIN_SECAPI

	Item item(Certificate::required(certificate));
	Keychain::optional(keychain)->add(item);

	END_SECAPI
}

OSStatus
SecCertificateGetData(SecCertificateRef certificate, CSSM_DATA_PTR data)
{
	BEGIN_SECAPI

	Required(data) = Certificate::required(certificate)->data();

	END_SECAPI
}


OSStatus
SecCertificateGetType(SecCertificateRef certificate, CSSM_CERT_TYPE *certificateType)
{
    BEGIN_SECAPI

	Required(certificateType) = Certificate::required(certificate)->type();

    END_SECAPI
}


OSStatus
SecCertificateGetSubject(SecCertificateRef certificate, CSSM_X509_NAME* subject)
{
    BEGIN_SECAPI

	Certificate::required(certificate)->getSubject(Required(subject));

    END_SECAPI
}


OSStatus
SecCertificateGetIssuer(SecCertificateRef certificate, CSSM_X509_NAME* issuer)
{
    BEGIN_SECAPI

	Certificate::required(certificate)->getIssuer(Required(issuer));

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
SecCertificateGetCommonName(SecCertificateRef certificate, CFStringRef *commonName)
{
    BEGIN_SECAPI

	Required(commonName) = Certificate::required(certificate)->commonName();

    END_SECAPI
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

	secdebug("kcsearch", "SecKeychainSearchCreateForCertificateByIssuerAndSN(%p)",
		keychainOrArray);
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

	secdebug("kcsearch", "SecKeychainSearchCreateForCertificateBySubjectKeyID(%p)",
		keychainOrArray);
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

	secdebug("kcsearch", "SecKeychainSearchCreateForCertificateByEmail(%p, %s)",
		keychainOrArray, emailAddress);
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

