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

#include <Security/SecCertificate.h>

#include "SecBridge.h"


CFTypeID
SecCertificateGetTypeID(void)
{
	BEGIN_SECAPI

	return gTypes().certificate.typeId;

	END_SECAPI1(_kCFRuntimeNotATypeID)
}


OSStatus
SecCertificateCreateFromData(const CSSM_DATA *data, CSSM_CERT_TYPE type, CSSM_CERT_ENCODING encoding, SecCertificateRef *certificate)
{
	BEGIN_SECAPI

	RefPointer<Certificate> certificatePtr(new Certificate(Required(data), type, encoding));
	Required(certificate) = gTypes().certificate.handle(*certificatePtr);

	END_SECAPI
}


OSStatus
SecCertificateAddToKeychain(SecCertificateRef certificate, SecKeychainRef keychain)
{
	BEGIN_SECAPI

	Item item(gTypes().certificate.required(certificate));
	Keychain::optional(keychain)->add(item);

	END_SECAPI
}

OSStatus
SecCertificateGetData(SecCertificateRef certificate, CSSM_DATA_PTR data)
{
	BEGIN_SECAPI

	Required(data) = gTypes().certificate.required(certificate)->data();

	END_SECAPI
}


OSStatus
SecCertificateGetType(SecCertificateRef certificate, CSSM_CERT_TYPE *certificateType)
{
    BEGIN_SECAPI

	Required(certificateType) = gTypes().certificate.required(certificate)->type();

    END_SECAPI
}


OSStatus
SecCertificateGetSubject(SecCertificateRef certificate, CSSM_X509_NAME* subject)
{
    BEGIN_SECAPI

	gTypes().certificate.required(certificate)->getSubject(Required(subject));

    END_SECAPI
}


OSStatus
SecCertificateGetIssuer(SecCertificateRef certificate, CSSM_X509_NAME* issuer)
{
    BEGIN_SECAPI

	gTypes().certificate.required(certificate)->getIssuer(Required(issuer));

    END_SECAPI
}


OSStatus
SecCertificateGetCLHandle(SecCertificateRef certificate, CSSM_CL_HANDLE *clHandle)
{
    BEGIN_SECAPI

	Required(clHandle) = gTypes().certificate.required(certificate)->clHandle();

    END_SECAPI
}
