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

#include <Security/SecCertificateRequest.h>

#include "SecBridge.h"
#include "CertificateRequest.h"
#include "SecImportExport.h"
#include "SecCertificate.h"

CFTypeID
SecCertificateRequestGetTypeID(void)
{
	BEGIN_SECAPI

	return gTypes().CertificateRequest.typeID;

	END_SECAPI1(_kCFRuntimeNotATypeID)
}


OSStatus SecCertificateRequestCreate(
        const CSSM_OID *policy,
        CSSM_CERT_TYPE certificateType,
        CSSM_TP_AUTHORITY_REQUEST_TYPE requestType,
	    SecKeyRef privateKeyItemRef,
	    SecKeyRef publicKeyItemRef,
	    const SecCertificateRequestAttributeList* attributeList,
        SecCertificateRequestRef* certRequest)
{
	BEGIN_SECAPI
	Required(certRequest);
	Required(policy);
	*certRequest = (new CertificateRequest(*policy, certificateType, requestType,
		privateKeyItemRef, publicKeyItemRef, attributeList))->handle();
	END_SECAPI
}


OSStatus SecCertificateRequestSubmit(
        SecCertificateRequestRef certRequest,
        sint32* estimatedTime)
{
	BEGIN_SECAPI

	CertificateRequest::required(certRequest)->submit(estimatedTime);

	END_SECAPI
}


OSStatus SecCertificateRequestGetType(
        SecCertificateRequestRef certRequestRef,
        CSSM_TP_AUTHORITY_REQUEST_TYPE *requestType)
{
	BEGIN_SECAPI

	Required(requestType);
	*requestType = CertificateRequest::required(certRequestRef)->reqType();

	END_SECAPI
}

OSStatus SecCertificateRequestGetResult(
        SecCertificateRequestRef certRequestRef,
        SecKeychainRef keychain,
        sint32 *estimatedTime,
        SecCertificateRef *certificateRef)
{
	BEGIN_SECAPI

	CssmData certData;
	*certificateRef = NULL;
	CertificateRequest::required(certRequestRef)->getResult(estimatedTime, certData);
	if(certData.data() != NULL) {
		/*
		 * Convert to SecCertifcateRef, optionally import. 
		 */
		CFDataRef cfCert = CFDataCreate(NULL, (UInt8 *)certData.data(), certData.Length);
		SecExternalItemType itemType = kSecItemTypeCertificate;
		CFArrayRef outItems = NULL;
		bool freeKcRef = false;
		OSStatus ortn;
		
		if(keychain == NULL) {
			/* 
			 * Unlike most Sec* calls, if the keychain argument to SecKeychainItemImport()
			 * is NULL, the item is not imported to the default keychain. At our
			 * interface, however, a NULL keychain means "import to the default
			 * keychain". 
			 */
			ortn = SecKeychainCopyDefault(&keychain);
			if(ortn) {
				certReqDbg("GetResult: SecKeychainCopyDefault failure");
				/* oh well, there's nothing we can do about this */
			}
			else {
				freeKcRef = true;
			}
		}
		ortn = SecKeychainItemImport(cfCert, NULL,
			NULL,			// format, don't care
			&itemType,
			0,				// flags
			NULL,			// keyParams
			keychain,		// optional, like ours
			&outItems);
		CFRelease(cfCert);
		if(freeKcRef) {
			CFRelease(keychain);
		}
		if(ortn) {
			certReqDbg("SecCertificateRequestGetResult: SecKeychainItemImport failure");
			MacOSError::throwMe(ortn);
		}
		CFIndex numItems = CFArrayGetCount(outItems);
		switch(numItems) {
			case 0:
				certReqDbg("SecCertificateRequestGetResult: import zero items");
				MacOSError::throwMe(errSecInternalComponent);
			default:
				certReqDbg("SecCertificateRequestGetResult: import %d items", 
					(int)numItems);
				/* but drop thru anyway, take the first one */
			case 1:
				SecCertificateRef certRef = 
					(SecCertificateRef)(CFArrayGetValueAtIndex(outItems, 0));
				if(CFGetTypeID(certRef) != SecCertificateGetTypeID()) {
					certReqDbg("SecCertificateRequestGetResult: bad type");
				}
				else {
					CFRetain(certRef);
					*certificateRef = certRef;
				}
		}
		CFRelease(outItems);
	}	
	END_SECAPI
}

OSStatus SecCertificateFindRequest(
        const CSSM_OID *policy,
        CSSM_CERT_TYPE certificateType,
        CSSM_TP_AUTHORITY_REQUEST_TYPE requestType,
		SecKeyRef publicKeyItemRef,				
		SecKeyRef privateKeyItemRef,				
		const SecCertificateRequestAttributeList* attributeList,
        SecCertificateRequestRef* certRequest)
{
	BEGIN_SECAPI

	Required(certRequest);
	Required(policy);
	*certRequest = (new CertificateRequest(*policy, certificateType, requestType,
		privateKeyItemRef, publicKeyItemRef, attributeList, false))->handle();
	END_SECAPI
}


OSStatus SecCertificateRequestGetData(
	SecCertificateRequestRef	certRequestRef,
	CSSM_DATA					*data)
{
	BEGIN_SECAPI

	Required(data);
	CertificateRequest::required(certRequestRef)->getReturnData(CssmData::overlay(*data));

	END_SECAPI
}
