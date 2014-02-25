/*
 * Copyright (c) 2002-2013 Apple Inc. All Rights Reserved.
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
#include "CertificateValues.h"
#include "SecCertificateP.h"
#include "SecCertificatePrivP.h"
//
//#include "AppleBaselineEscrowCertificates.h"
//
//######################################################################
//%%% start workaround: rdar://14292830
//######################################################################

#ifndef sec_AppleBaselineEscrowCertificates_h
#define sec_AppleBaselineEscrowCertificates_h

struct RootRecord
{
	size_t	_length;
	UInt8*	_bytes;
};

/* ==========================================================================
    Production Escrow Certificates
   ========================================================================== */

static const UInt8 kProductionEscrowRootGM[] = {
	0x30,0x82,0x03,0xD0,0x30,0x82,0x02,0xB8,0xA0,0x03,0x02,0x01,0x02,0x02,0x01,0x64,
	0x30,0x0D,0x06,0x09,0x2A,0x86,0x48,0x86,0xF7,0x0D,0x01,0x01,0x0B,0x05,0x00,0x30,
	0x79,0x31,0x0C,0x30,0x0A,0x06,0x03,0x55,0x04,0x05,0x13,0x03,0x31,0x30,0x30,0x31,
	0x0B,0x30,0x09,0x06,0x03,0x55,0x04,0x06,0x13,0x02,0x55,0x53,0x31,0x13,0x30,0x11,
	0x06,0x03,0x55,0x04,0x0A,0x13,0x0A,0x41,0x70,0x70,0x6C,0x65,0x20,0x49,0x6E,0x63,
	0x2E,0x31,0x26,0x30,0x24,0x06,0x03,0x55,0x04,0x0B,0x13,0x1D,0x41,0x70,0x70,0x6C,
	0x65,0x20,0x43,0x65,0x72,0x74,0x69,0x66,0x69,0x63,0x61,0x74,0x69,0x6F,0x6E,0x20,
	0x41,0x75,0x74,0x68,0x6F,0x72,0x69,0x74,0x79,0x31,0x1F,0x30,0x1D,0x06,0x03,0x55,
	0x04,0x03,0x13,0x16,0x45,0x73,0x63,0x72,0x6F,0x77,0x20,0x53,0x65,0x72,0x76,0x69,
	0x63,0x65,0x20,0x52,0x6F,0x6F,0x74,0x20,0x43,0x41,0x30,0x1E,0x17,0x0D,0x31,0x33,
	0x30,0x38,0x30,0x32,0x32,0x33,0x32,0x34,0x34,0x34,0x5A,0x17,0x0D,0x32,0x33,0x30,
	0x38,0x30,0x32,0x32,0x33,0x32,0x34,0x34,0x34,0x5A,0x30,0x79,0x31,0x0C,0x30,0x0A,
	0x06,0x03,0x55,0x04,0x05,0x13,0x03,0x31,0x30,0x30,0x31,0x0B,0x30,0x09,0x06,0x03,
	0x55,0x04,0x06,0x13,0x02,0x55,0x53,0x31,0x13,0x30,0x11,0x06,0x03,0x55,0x04,0x0A,
	0x13,0x0A,0x41,0x70,0x70,0x6C,0x65,0x20,0x49,0x6E,0x63,0x2E,0x31,0x26,0x30,0x24,
	0x06,0x03,0x55,0x04,0x0B,0x13,0x1D,0x41,0x70,0x70,0x6C,0x65,0x20,0x43,0x65,0x72,
	0x74,0x69,0x66,0x69,0x63,0x61,0x74,0x69,0x6F,0x6E,0x20,0x41,0x75,0x74,0x68,0x6F,
	0x72,0x69,0x74,0x79,0x31,0x1F,0x30,0x1D,0x06,0x03,0x55,0x04,0x03,0x13,0x16,0x45,
	0x73,0x63,0x72,0x6F,0x77,0x20,0x53,0x65,0x72,0x76,0x69,0x63,0x65,0x20,0x52,0x6F,
	0x6F,0x74,0x20,0x43,0x41,0x30,0x82,0x01,0x22,0x30,0x0D,0x06,0x09,0x2A,0x86,0x48,
	0x86,0xF7,0x0D,0x01,0x01,0x01,0x05,0x00,0x03,0x82,0x01,0x0F,0x00,0x30,0x82,0x01,
	0x0A,0x02,0x82,0x01,0x01,0x00,0xD0,0xA3,0xF4,0x56,0x7D,0x3F,0x46,0x31,0xD2,0x56,
	0xA0,0xDF,0x42,0xA0,0x29,0x83,0x1E,0xB9,0x82,0xB5,0xA5,0xFF,0x3E,0xDE,0xB5,0x0F,
	0x4A,0x8A,0x28,0x60,0xCF,0x75,0xB4,0xA0,0x70,0x7C,0xF5,0xE2,0x94,0xF3,0x22,0x02,
	0xC8,0x81,0xCE,0x34,0xC7,0x66,0x6A,0x18,0xAA,0xB4,0xFD,0x6D,0xB0,0x0B,0xDD,0x4A,
	0xDD,0xCF,0xE0,0x08,0x1B,0x1C,0xA6,0xDB,0xBA,0xB2,0xC1,0xA4,0x10,0x5F,0x35,0x4F,
	0x8B,0x8B,0x7A,0xA3,0xDB,0x3C,0xF6,0x54,0x95,0x42,0xAD,0x2A,0x3B,0xFE,0x06,0x8C,
	0xE1,0x92,0xF1,0x60,0x97,0x58,0x1B,0xD9,0x8F,0xBE,0xFB,0x46,0x4C,0x29,0x5C,0x1C,
	0xF0,0x20,0xB6,0x2B,0xA5,0x12,0x09,0x9B,0x28,0x41,0x34,0x97,0x9F,0xF3,0x88,0x4B,
	0x69,0x72,0xEA,0x3A,0x27,0xB0,0x50,0x1D,0x88,0x29,0x0D,0xBB,0xED,0x04,0xA2,0x11,
	0xCF,0x0C,0x5B,0x65,0x61,0x35,0xBD,0xF2,0x0D,0xFC,0xE2,0xB9,0x20,0xD3,0xB7,0x03,
	0x70,0x39,0xD5,0xE0,0x86,0x7C,0x04,0xCC,0xC9,0xA1,0x85,0xB4,0x9B,0xBC,0x88,0x4E,
	0xD7,0xAD,0x5C,0xFF,0x2C,0x0D,0x80,0x8E,0x51,0x39,0x20,0x8B,0xAF,0x1E,0x46,0x95,
	0xFA,0x0D,0x1B,0xD2,0xBF,0x80,0xE0,0x9F,0x6D,0x4A,0xF5,0x31,0x67,0x18,0x11,0xA5,
	0x63,0x27,0x08,0xEE,0xD9,0x07,0x29,0xD0,0xD4,0x36,0x91,0x5B,0xFB,0x4A,0x0B,0x07,
	0xD1,0x0D,0x79,0x16,0x6E,0x16,0x02,0x23,0x80,0xC6,0x15,0x07,0x6D,0xA0,0x06,0xB6,
	0x45,0x90,0xB0,0xAE,0xA4,0xAD,0x0E,0x75,0x04,0x2B,0x2B,0x78,0xF1,0x57,0x84,0x23,
	0x87,0x24,0xEC,0x58,0xC4,0xF1,0x02,0x03,0x01,0x00,0x01,0xA3,0x63,0x30,0x61,0x30,
	0x0F,0x06,0x03,0x55,0x1D,0x13,0x01,0x01,0xFF,0x04,0x05,0x30,0x03,0x01,0x01,0xFF,
	0x30,0x0E,0x06,0x03,0x55,0x1D,0x0F,0x01,0x01,0xFF,0x04,0x04,0x03,0x02,0x01,0x06,
	0x30,0x1D,0x06,0x03,0x55,0x1D,0x0E,0x04,0x16,0x04,0x14,0xFD,0x78,0x96,0x53,0x80,
	0xD6,0xF6,0xDC,0xA6,0xC3,0x59,0x06,0x38,0xED,0x79,0x3E,0x8F,0x50,0x1B,0x50,0x30,
	0x1F,0x06,0x03,0x55,0x1D,0x23,0x04,0x18,0x30,0x16,0x80,0x14,0xFD,0x78,0x96,0x53,
	0x80,0xD6,0xF6,0xDC,0xA6,0xC3,0x59,0x06,0x38,0xED,0x79,0x3E,0x8F,0x50,0x1B,0x50,
	0x30,0x0D,0x06,0x09,0x2A,0x86,0x48,0x86,0xF7,0x0D,0x01,0x01,0x0B,0x05,0x00,0x03,
	0x82,0x01,0x01,0x00,0x71,0x15,0xCA,0x87,0xD0,0x2D,0xB5,0x18,0xD5,0x35,0x7A,0xCD,
	0xDF,0x62,0x28,0xF0,0x0B,0x63,0x4D,0x4E,0x02,0xBA,0x3D,0xB8,0xB4,0x37,0xEA,0xB0,
	0x93,0x93,0xAB,0x1C,0xFD,0x9F,0xE8,0x72,0xBF,0xF3,0xDB,0xE6,0xAD,0x16,0xFE,0x71,
	0x61,0xA8,0x5A,0xD0,0x58,0x0F,0x65,0x7A,0x57,0x7A,0xE0,0x34,0x80,0x8E,0xBB,0x41,
	0x01,0xE7,0xB0,0x3B,0xF7,0x2B,0x3A,0x6D,0x44,0x2A,0x3A,0x04,0x52,0xFA,0x2B,0x7B,
	0x3B,0x21,0xDD,0x0C,0x70,0x3D,0xFB,0x45,0xC6,0x79,0x68,0x62,0xE2,0x89,0xB8,0x25,
	0xEE,0x63,0x76,0x02,0xB2,0x22,0xE9,0x53,0x85,0x68,0x3E,0x75,0xB6,0x0B,0x65,0xE9,
	0x1C,0xBA,0x84,0x93,0xB0,0x8A,0xEF,0xB5,0x1A,0x12,0xE4,0x8F,0xAE,0xD5,0x5C,0xA1,
	0x05,0x4A,0x01,0xBC,0x6F,0xF9,0x58,0x5E,0xF7,0x04,0x61,0xEE,0xF5,0xC6,0xA0,0x1B,
	0x44,0x2E,0x5A,0x3A,0x59,0xA1,0xB3,0xB0,0xF4,0xB6,0xCB,0xE0,0x6C,0x2B,0x59,0x8A,
	0xFB,0x6A,0xE0,0xA2,0x57,0x09,0x79,0xC1,0xDD,0xFB,0x84,0x86,0xEB,0x66,0x29,0x73,
	0xAE,0xBF,0x58,0xAE,0x47,0x4D,0x48,0x37,0xD6,0xB1,0x8C,0x5F,0x26,0x5F,0xB5,0x26,
	0x07,0x0B,0x85,0xB7,0x36,0x37,0x14,0xCF,0x5E,0x55,0xA5,0x3C,0xF3,0x1E,0x79,0x50,
	0xBB,0x85,0x3B,0xB2,0x94,0x68,0xB0,0x25,0x4F,0x75,0xEC,0xF0,0xF9,0xC0,0x5A,0x2D,
	0xE5,0xED,0x67,0xCD,0x88,0x55,0xA0,0x42,0xDE,0x78,0xBC,0xFE,0x30,0xB1,0x62,0x2D,
	0xE1,0xFD,0xEC,0x75,0x03,0xA6,0x1F,0x7C,0xC4,0x3A,0x4A,0x59,0xFE,0x77,0xC3,0x99,
	0x96,0x87,0x44,0xC3,
};

static struct RootRecord kProductionEscrowRootRecord = {sizeof(kProductionEscrowRootGM), (UInt8*)kProductionEscrowRootGM};
static struct RootRecord* kProductionEscrowRoots[] = {&kProductionEscrowRootRecord};
static const int kNumberOfProductionEscrowRoots = (int)(sizeof(kProductionEscrowRoots)/sizeof(kProductionEscrowRoots[0]));

static struct RootRecord kBaseLineEscrowRootRecord = {sizeof(kProductionEscrowRootGM), (UInt8*)kProductionEscrowRootGM};
static struct RootRecord* kBaseLineEscrowRoots[] = {&kBaseLineEscrowRootRecord};
static const int kNumberOfBaseLineEscrowRoots = (int)(sizeof(kBaseLineEscrowRoots)/sizeof(kBaseLineEscrowRoots[0]));


#endif
//######################################################################
//%%% end workaround: rdar://14292830
////######################################################################


extern CSSM_KEYUSE ConvertArrayToKeyUsage(CFArrayRef usage);

#define SEC_CONST_DECL(k,v) CFTypeRef k = (CFTypeRef)(CFSTR(v));

SEC_CONST_DECL (kSecCertificateProductionEscrowKey, "ProductionEscrowKey");
SEC_CONST_DECL (kSecCertificateEscrowFileName, "AppleESCertificates");


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

		__secapiresult=errSecSuccess;
	}
	catch (const MacOSError &err) { __secapiresult=err.osStatus(); }
	catch (const CommonError &err) { __secapiresult=SecKeychainErrFromOSStatus(err.osStatus()); }
	catch (const std::bad_alloc &) { __secapiresult=errSecAllocate; }
	catch (...) { __secapiresult=errSecInternalComponent; }
    return certificate;
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

/* new in 10.6 */
CFDataRef
SecCertificateCopyData(SecCertificateRef certificate)
{
	CFDataRef data = NULL;
    OSStatus __secapiresult = errSecSuccess;
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
	catch (const std::bad_alloc &) { __secapiresult=errSecAllocate; }
	catch (...) { __secapiresult=errSecInternalComponent; }
    return data;
}

CFDataRef
SecCertificateGetSHA1Digest(SecCertificateRef certificate)
{
	CFDataRef data = NULL;
    OSStatus __secapiresult = errSecSuccess;
	try {
		data = Certificate::required(certificate)->sha1Hash();
	}
	catch (const MacOSError &err) { __secapiresult=err.osStatus(); }
	catch (const CommonError &err) { __secapiresult=SecKeychainErrFromOSStatus(err.osStatus()); }
	catch (const std::bad_alloc &) { __secapiresult=errSecAllocate; }
	catch (...) { __secapiresult=errSecInternalComponent; }
    return data;
}

CFArrayRef
SecCertificateCopyDNSNames(SecCertificateRef certificate)
{
	CFArrayRef names = NULL;
	OSStatus __secapiresult = errSecSuccess;
	try {
		names = Certificate::required(certificate)->copyDNSNames();
	}
	catch (const MacOSError &err) { __secapiresult=err.osStatus(); }
	catch (const CommonError &err) { __secapiresult=SecKeychainErrFromOSStatus(err.osStatus()); }
	catch (const std::bad_alloc &) { __secapiresult=errSecAllocate; }
	catch (...) { __secapiresult=errSecInternalComponent; }
    return names;
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

		__secapiresult=errSecSuccess;
	}
	catch (const MacOSError &err) { __secapiresult=err.osStatus(); }
	catch (const CommonError &err) { __secapiresult=SecKeychainErrFromOSStatus(err.osStatus()); }
	catch (const std::bad_alloc &) { __secapiresult=errSecAllocate; }
	catch (...) { __secapiresult=errSecInternalComponent; }
    return summary;
}

CFStringRef
SecCertificateCopyIssuerSummary(SecCertificateRef certificate)
{
	CFStringRef issuerStr = NULL;
	SecCertificateRefP certP = NULL;
	CFDataRef certData = SecCertificateCopyData(certificate);
	if (certData) {
		certP = SecCertificateCreateWithDataP(NULL, certData);
		CFRelease(certData);
	}
	if (certP) {
		issuerStr = SecCertificateCopyIssuerSummaryP(certP);
		CFRelease(certP);
	}
	return issuerStr;
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
SecKeychainSearchCreateForCertificateByIssuerAndSN_CF(CFTypeRef keychainOrArray, CFDataRef issuer,
	CFDataRef serialNumber, SecKeychainSearchRef *searchRef)
{
    BEGIN_SECAPI

	Required(searchRef);

	StorageManager::KeychainList keychains;
	globals().storageManager.optionalSearchList(keychainOrArray, keychains);
	Required(issuer);
	Required(serialNumber);
	KCCursor cursor(Certificate::cursorForIssuerAndSN_CF(keychains, issuer, serialNumber));
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
		return errSecParam;

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

OSStatus
SecCertificateCopyPreference(
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

SecCertificateRef
SecCertificateCopyPreferred(
	CFStringRef name,
	CFArrayRef keyUsage)
{
	// This function will look for a matching preference in the following order:
	// - matches the name and the supplied key use
	// - matches the name and the special 'ANY' key use
	// - matches the name with no key usage constraint

	SecCertificateRef certRef = NULL;
	CSSM_KEYUSE keyUse = ConvertArrayToKeyUsage(keyUsage);
	OSStatus status = SecCertificateCopyPreference(name, keyUse, &certRef);
	if (status != errSecSuccess && keyUse != CSSM_KEYUSE_ANY)
		status = SecCertificateCopyPreference(name, CSSM_KEYUSE_ANY, &certRef);
	if (status != errSecSuccess && keyUse != 0)
		status = SecCertificateCopyPreference(name, 0, &certRef);

	return certRef;
}

static OSStatus
SecCertificateFindPreferenceItemWithNameAndKeyUsage(
	CFTypeRef keychainOrArray,
	CFStringRef name,
	int32_t keyUsage,
	SecKeychainItemRef *itemRef)
{
    BEGIN_SECAPI

	StorageManager::KeychainList keychains;
	globals().storageManager.optionalSearchList(keychainOrArray, keychains);
	KCCursor cursor(keychains, kSecGenericPasswordItemClass, NULL);

	char idUTF8[MAXPATHLEN];
    idUTF8[0] = (char)'\0';
	if (name)
	{
		if (!CFStringGetCString(name, idUTF8, sizeof(idUTF8)-1, kCFStringEncodingUTF8))
			idUTF8[0] = (char)'\0';
	}
    size_t idUTF8Len = strlen(idUTF8);
    if (!idUTF8Len)
        MacOSError::throwMe(errSecParam);

    CssmData service(const_cast<char *>(idUTF8), idUTF8Len);
    cursor->add(CSSM_DB_EQUAL, Schema::attributeInfo(kSecServiceItemAttr), service);
	cursor->add(CSSM_DB_EQUAL, Schema::attributeInfo(kSecTypeItemAttr), (FourCharCode)'cprf');
    if (keyUsage)
        cursor->add(CSSM_DB_EQUAL, Schema::attributeInfo(kSecScriptCodeItemAttr), (sint32)keyUsage);

	Item item;
	if (!cursor->next(item))
		MacOSError::throwMe(errSecItemNotFound);

	if (itemRef)
		*itemRef=item->handle();

    END_SECAPI
}

static
OSStatus SecCertificateDeletePreferenceItemWithNameAndKeyUsage(
	CFTypeRef keychainOrArray,
	CFStringRef name,
	int32_t keyUsage)
{
	// when a specific key usage is passed, we'll only match & delete that pref;
	// when a key usage of 0 is passed, all matching prefs should be deleted.
	// maxUsages represents the most matches there could theoretically be, so
	// cut things off at that point if we're still finding items (if they can't
	// be deleted for some reason, we'd never break out of the loop.)

	OSStatus status;
	SecKeychainItemRef item = NULL;
	int count = 0, maxUsages = 12;
	while (++count <= maxUsages &&
			(status = SecCertificateFindPreferenceItemWithNameAndKeyUsage(keychainOrArray, name, keyUsage, &item)) == errSecSuccess) {
		status = SecKeychainItemDelete(item);
		CFRelease(item);
		item = NULL;
	}

	// it's not an error if the item isn't found
	return (status == errSecItemNotFound) ? errSecSuccess : status;
}

OSStatus SecCertificateSetPreference(
    SecCertificateRef certificate,
    CFStringRef name,
    CSSM_KEYUSE keyUsage,
    CFDateRef date)
{
	if (!name) {
		return errSecParam;
	}
	if (!certificate) {
		// treat NULL certificate as a request to clear the preference
		// (note: if keyUsage is 0, this clears all key usage prefs for name)
		return SecCertificateDeletePreferenceItemWithNameAndKeyUsage(NULL, name, keyUsage);
	}

    BEGIN_SECAPI

	// determine the account attribute
	//
	// This attribute must be synthesized from certificate label + pref item type + key usage,
	// as only the account and service attributes can make a generic keychain item unique.
	// For 'iprf' type items (but not 'cprf'), we append a trailing space. This insures that
	// we can save a certificate preference if an identity preference already exists for the
	// given service name, and vice-versa.
	// If the key usage is 0 (i.e. the normal case), we omit the appended key usage string.
	//
    CFStringRef labelStr = nil;
	Certificate::required(certificate)->inferLabel(false, &labelStr);
	if (!labelStr) {
        MacOSError::throwMe(errSecDataTooLarge); // data is "in a format which cannot be displayed"
	}
	CFIndex accountUTF8Len = CFStringGetMaximumSizeForEncoding(CFStringGetLength(labelStr), kCFStringEncodingUTF8) + 1;
	const char *templateStr = "%s [key usage 0x%X]";
	const int keyUsageMaxStrLen = 8;
	accountUTF8Len += strlen(templateStr) + keyUsageMaxStrLen;
	char accountUTF8[accountUTF8Len];
    if (!CFStringGetCString(labelStr, accountUTF8, accountUTF8Len-1, kCFStringEncodingUTF8))
		accountUTF8[0] = (char)'\0';
	if (keyUsage)
		snprintf(accountUTF8, accountUTF8Len-1, templateStr, accountUTF8, keyUsage);
    CssmData account(const_cast<char *>(accountUTF8), strlen(accountUTF8));
    CFRelease(labelStr);

	// service attribute (name provided by the caller)
	CFIndex serviceUTF8Len = CFStringGetMaximumSizeForEncoding(CFStringGetLength(name), kCFStringEncodingUTF8) + 1;;
	char serviceUTF8[serviceUTF8Len];
    if (!CFStringGetCString(name, serviceUTF8, serviceUTF8Len-1, kCFStringEncodingUTF8))
        serviceUTF8[0] = (char)'\0';
    CssmData service(const_cast<char *>(serviceUTF8), strlen(serviceUTF8));

    // look for existing preference item, in case this is an update
	StorageManager::KeychainList keychains;
	globals().storageManager.getSearchList(keychains);
	KCCursor cursor(keychains, kSecGenericPasswordItemClass, NULL);
    FourCharCode itemType = 'cprf';
    cursor->add(CSSM_DB_EQUAL, Schema::attributeInfo(kSecServiceItemAttr), service);
	cursor->add(CSSM_DB_EQUAL, Schema::attributeInfo(kSecTypeItemAttr), itemType);
    if (keyUsage)
        cursor->add(CSSM_DB_EQUAL, Schema::attributeInfo(kSecScriptCodeItemAttr), (sint32)keyUsage);
    if (date)
        ; // %%%TBI

	Item item(kSecGenericPasswordItemClass, 'aapl', 0, NULL, false);
    bool add = (!cursor->next(item));
	// at this point, we either have a new item to add or an existing item to update

    // set item attribute values
    item->setAttribute(Schema::attributeInfo(kSecServiceItemAttr), service);
    item->setAttribute(Schema::attributeInfo(kSecTypeItemAttr), itemType);
    item->setAttribute(Schema::attributeInfo(kSecAccountItemAttr), account);
    item->setAttribute(Schema::attributeInfo(kSecScriptCodeItemAttr), (sint32)keyUsage);
    item->setAttribute(Schema::attributeInfo(kSecLabelItemAttr), service);

    // date
    if (date)
        ; // %%%TBI

	// generic attribute (store persistent certificate reference)
	CFDataRef pItemRef = nil;
	Certificate::required(certificate)->copyPersistentReference(pItemRef);
	if (!pItemRef) {
		MacOSError::throwMe(errSecInvalidItemRef);
    }
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

		try {
			keychain->add(item);
		}
		catch (const MacOSError &err) {
			if (err.osStatus() != errSecDuplicateItem)
				throw; // if item already exists, fall through to update
		}
    }
	item->update();

    END_SECAPI
}

OSStatus SecCertificateSetPreferred(
	SecCertificateRef certificate,
	CFStringRef name,
	CFArrayRef keyUsage)
{
	CSSM_KEYUSE keyUse = ConvertArrayToKeyUsage(keyUsage);
	return SecCertificateSetPreference(certificate, name, keyUse, NULL);
}

CFDictionaryRef SecCertificateCopyValues(SecCertificateRef certificate, CFArrayRef keys, CFErrorRef *error)
{
	CFDictionaryRef result = NULL;
	OSStatus __secapiresult;
	try
	{
		CertificateValues cv(certificate);
		result = cv.copyFieldValues(keys,error);
		__secapiresult=0;
	}
	catch (const MacOSError &err) { __secapiresult=err.osStatus(); }
	catch (const CommonError &err) { __secapiresult=SecKeychainErrFromOSStatus(err.osStatus()); }
	catch (const std::bad_alloc &) { __secapiresult=errSecAllocate; }
	catch (...) { __secapiresult=errSecInternalComponent; }
	return result;
}

CFStringRef SecCertificateCopyLongDescription(CFAllocatorRef alloc, SecCertificateRef certificate, CFErrorRef *error)
{
	return SecCertificateCopyShortDescription(alloc, certificate, error);
}

CFStringRef SecCertificateCopyShortDescription(CFAllocatorRef alloc, SecCertificateRef certificate, CFErrorRef *error)
{
	CFStringRef result = NULL;
	OSStatus __secapiresult;
	try
	{
		__secapiresult = SecCertificateInferLabel(certificate, &result);
	}
	catch (const MacOSError &err) { __secapiresult=err.osStatus(); }
	catch (const CommonError &err) { __secapiresult=SecKeychainErrFromOSStatus(err.osStatus()); }
	catch (const std::bad_alloc &) { __secapiresult=errSecAllocate; }
	catch (...) { __secapiresult=errSecInternalComponent; }
	if (error!=NULL && __secapiresult!=errSecSuccess)
	{
		*error = CFErrorCreate(kCFAllocatorDefault, kCFErrorDomainOSStatus,
			__secapiresult ? __secapiresult : CSSM_ERRCODE_INTERNAL_ERROR, NULL);
	}
	return result;
}

CFDataRef SecCertificateCopySerialNumber(SecCertificateRef certificate, CFErrorRef *error)
{
	CFDataRef result = NULL;
	OSStatus __secapiresult;
	try
	{
		CertificateValues cv(certificate);
		result = cv.copySerialNumber(error);
		__secapiresult=0;
	}
	catch (const MacOSError &err) { __secapiresult=err.osStatus(); }
	catch (const CommonError &err) { __secapiresult=SecKeychainErrFromOSStatus(err.osStatus()); }
	catch (const std::bad_alloc &) { __secapiresult=errSecAllocate; }
	catch (...) { __secapiresult=errSecInternalComponent; }
	return result;
}

CFDataRef SecCertificateCopyNormalizedIssuerContent(SecCertificateRef certificate, CFErrorRef *error)
{
	CFDataRef result = NULL;
	OSStatus __secapiresult;
	try
	{
		CertificateValues cv(certificate);
		result = cv.copyNormalizedIssuerContent(error);
		__secapiresult=0;
	}
	catch (const MacOSError &err) { __secapiresult=err.osStatus(); }
	catch (const CommonError &err) { __secapiresult=SecKeychainErrFromOSStatus(err.osStatus()); }
	catch (const std::bad_alloc &) { __secapiresult=errSecAllocate; }
	catch (...) { __secapiresult=errSecInternalComponent; }
	return result;
}

CFDataRef SecCertificateCopyNormalizedSubjectContent(SecCertificateRef certificate, CFErrorRef *error)
{
	CFDataRef result = NULL;
	OSStatus __secapiresult;
	try
	{
		CertificateValues cv(certificate);
		result = cv.copyNormalizedSubjectContent(error);
		__secapiresult=0;
	}
	catch (const MacOSError &err) { __secapiresult=err.osStatus(); }
	catch (const CommonError &err) { __secapiresult=SecKeychainErrFromOSStatus(err.osStatus()); }
	catch (const std::bad_alloc &) { __secapiresult=errSecAllocate; }
	catch (...) { __secapiresult=errSecInternalComponent; }
	return result;
}

CFDataRef SecCertificateCopyIssuerSequence(SecCertificateRef certificate)
{
	CFDataRef result = NULL;
	OSStatus __secapiresult;
	try
	{
		CertificateValues cv(certificate);
		result = cv.copyIssuerSequence(NULL);
		__secapiresult=0;
	}
	catch (const MacOSError &err) { __secapiresult=err.osStatus(); }
	catch (const CommonError &err) { __secapiresult=SecKeychainErrFromOSStatus(err.osStatus()); }
	catch (const std::bad_alloc &) { __secapiresult=errSecAllocate; }
	catch (...) { __secapiresult=errSecInternalComponent; }
	return result;
}

CFDataRef SecCertificateCopySubjectSequence(SecCertificateRef certificate)
{
	CFDataRef result = NULL;
	OSStatus __secapiresult;
	try
	{
		CertificateValues cv(certificate);
		result = cv.copySubjectSequence(NULL);
		__secapiresult=0;
	}
	catch (const MacOSError &err) { __secapiresult=err.osStatus(); }
	catch (const CommonError &err) { __secapiresult=SecKeychainErrFromOSStatus(err.osStatus()); }
	catch (const std::bad_alloc &) { __secapiresult=errSecAllocate; }
	catch (...) { __secapiresult=errSecInternalComponent; }
	return result;
}

bool SecCertificateIsValid(SecCertificateRef certificate, CFAbsoluteTime verifyTime)
{
	bool result = NULL;
	OSStatus __secapiresult;
	try
	{
		CFErrorRef error = NULL;
		CertificateValues cv(certificate);
		result = cv.isValid(verifyTime, &error);
		if (error) CFRelease(error);
		__secapiresult=0;
	}
	catch (const MacOSError &err) { __secapiresult=err.osStatus(); }
	catch (const CommonError &err) { __secapiresult=SecKeychainErrFromOSStatus(err.osStatus()); }
	catch (const std::bad_alloc &) { __secapiresult=errSecAllocate; }
	catch (...) { __secapiresult=errSecInternalComponent; }
	return result;
}

/*
 * deprecated function name
 */
bool SecCertificateIsValidX(SecCertificateRef certificate, CFAbsoluteTime verifyTime)
{
	return SecCertificateIsValid(certificate, verifyTime);
}


CFAbsoluteTime SecCertificateNotValidBefore(SecCertificateRef certificate)
{
	CFAbsoluteTime result = 0;
	OSStatus __secapiresult;
	try
	{
		CFErrorRef error = NULL;
		CertificateValues cv(certificate);
		result = cv.notValidBefore(&error);
		if (error) CFRelease(error);
		__secapiresult=0;
	}
	catch (const MacOSError &err) { __secapiresult=err.osStatus(); }
	catch (const CommonError &err) { __secapiresult=SecKeychainErrFromOSStatus(err.osStatus()); }
	catch (const std::bad_alloc &) { __secapiresult=errSecAllocate; }
	catch (...) { __secapiresult=errSecInternalComponent; }
	return result;
}

CFAbsoluteTime SecCertificateNotValidAfter(SecCertificateRef certificate)
{
	CFAbsoluteTime result = 0;
	OSStatus __secapiresult;
	try
	{
		CFErrorRef error = NULL;
		CertificateValues cv(certificate);
		result = cv.notValidAfter(&error);
		if (error) CFRelease(error);
		__secapiresult=0;
	}
	catch (const MacOSError &err) { __secapiresult=err.osStatus(); }
	catch (const CommonError &err) { __secapiresult=SecKeychainErrFromOSStatus(err.osStatus()); }
	catch (const std::bad_alloc &) { __secapiresult=errSecAllocate; }
	catch (...) { __secapiresult=errSecInternalComponent; }
	return result;
}

/* new in 10.8 */
SecCertificateRef SecCertificateCreateWithBytes(CFAllocatorRef allocator,
    const UInt8 *bytes, CFIndex length)
{
	SecCertificateRef certificate = NULL;
	OSStatus __secapiresult;
	try {
		CSSM_DATA cssmCertData = { (CSSM_SIZE)length, (uint8 *)bytes };

		//NOTE: there isn't yet a Certificate constructor which accepts a CFAllocatorRef
		SecPointer<Certificate> certificatePtr(new Certificate(cssmCertData, CSSM_CERT_X_509v3, CSSM_CERT_ENCODING_DER));
		certificate = certificatePtr->handle();

		__secapiresult=errSecSuccess;
	}
	catch (const MacOSError &err) { __secapiresult=err.osStatus(); }
	catch (const CommonError &err) { __secapiresult=SecKeychainErrFromOSStatus(err.osStatus()); }
	catch (const std::bad_alloc &) { __secapiresult=errSecAllocate; }
	catch (...) { __secapiresult=errSecInternalComponent; }
	return certificate;
}

/* new in 10.8 */
CFIndex SecCertificateGetLength(SecCertificateRef certificate)
{
	CFIndex length = 0;
	OSStatus __secapiresult;
	try {
		CssmData output = Certificate::required(certificate)->data();
		length = (CFIndex)output.length();
		__secapiresult=errSecSuccess;
	}
	catch (const MacOSError &err) { __secapiresult=err.osStatus(); }
	catch (const CommonError &err) { __secapiresult=SecKeychainErrFromOSStatus(err.osStatus()); }
	catch (const std::bad_alloc &) { __secapiresult=errSecAllocate; }
	catch (...) { __secapiresult=errSecInternalComponent; }
	return length;
}

/* new in 10.8 */
const UInt8 *SecCertificateGetBytePtr(SecCertificateRef certificate)
{
	const UInt8 *bytes = NULL;
	OSStatus __secapiresult;
	try {
		CssmData output = Certificate::required(certificate)->data();
		bytes = (const UInt8 *)output.data();
		__secapiresult=errSecSuccess;
	}
	catch (const MacOSError &err) { __secapiresult=err.osStatus(); }
	catch (const CommonError &err) { __secapiresult=SecKeychainErrFromOSStatus(err.osStatus()); }
	catch (const std::bad_alloc &) { __secapiresult=errSecAllocate; }
	catch (...) { __secapiresult=errSecInternalComponent; }
	return bytes;
}

static CFArrayRef CopyEscrowCertificates(CFErrorRef *error)
{
	// Return array of CFDataRef certificates.
    CFArrayRef result = NULL;
	int iCnt;
	int numRoots = 0;

	// Get the hard coded set of production roots
	// static struct RootRecord* kProductionEscrowRoots[] = {&kOldEscrowRootRecord, &kProductionEscrowRootRecord};

	numRoots = kNumberOfProductionEscrowRoots;
	CFDataRef productionCerts[numRoots];
	struct RootRecord* pRootRecord = NULL;

	for (iCnt = 0; iCnt < numRoots; iCnt++)
	{
		pRootRecord = kProductionEscrowRoots[iCnt];
		if (NULL != pRootRecord && pRootRecord->_length > 0 && NULL != pRootRecord->_bytes)
		{
			productionCerts[iCnt] = CFDataCreate(kCFAllocatorDefault, pRootRecord->_bytes, pRootRecord->_length);
		}
	}
	result = CFArrayCreate(kCFAllocatorDefault, (const void **)productionCerts, numRoots, &kCFTypeArrayCallBacks);
	for (iCnt = 0; iCnt < numRoots; iCnt++)
	{
		if (NULL != productionCerts[iCnt])
		{
			CFRelease(productionCerts[iCnt]);
		}
	}

	return result;
}

/* new in 10.9 */
CFArrayRef SecCertificateCopyEscrowRoots(SecCertificateEscrowRootType escrowRootType)
{
	CFArrayRef result = NULL;
	int iCnt;
	int numRoots = 0;
	CFDataRef certData = NULL;

	// The request is for the base line certificates.
	// Use the hard coded data to generate the return array
	if (kSecCertificateBaselineEscrowRoot == escrowRootType)
	{
		// Get the hard coded set of roots
		numRoots = kNumberOfBaseLineEscrowRoots;
	    SecCertificateRef baseLineCerts[numRoots];
	    struct RootRecord* pRootRecord = NULL;

	    for (iCnt = 0; iCnt < numRoots; iCnt++)
	    {
	        pRootRecord = kBaseLineEscrowRoots[iCnt];
	        if (NULL != pRootRecord && pRootRecord->_length > 0 && NULL != pRootRecord->_bytes)
	        {
				certData = CFDataCreate(kCFAllocatorDefault, pRootRecord->_bytes, pRootRecord->_length);
				if (NULL != certData)
				{
					baseLineCerts[iCnt] = SecCertificateCreateWithData(kCFAllocatorDefault, certData);
					CFRelease(certData);
				}
	        }
	    }
		result = CFArrayCreate(kCFAllocatorDefault, (const void **)baseLineCerts, numRoots, &kCFTypeArrayCallBacks);
		for (iCnt = 0; iCnt < numRoots; iCnt++)
		{
			if (NULL != baseLineCerts[iCnt])
			{
				CFRelease(baseLineCerts[iCnt]);
			}
		}
	}
	// The request is for the current certificates.
	else if (kSecCertificateProductionEscrowRoot == escrowRootType)
	{
		CFErrorRef error = NULL;
		CFArrayRef cert_datas = CopyEscrowCertificates(&error);
		if (NULL != error || NULL == cert_datas || 0 == (numRoots = (int)CFArrayGetCount(cert_datas)))
		{
			if (NULL != error)
			{
				CFRelease(error);
			}

			if (NULL != cert_datas)
			{
				CFRelease(cert_datas);
			}
			return result;
		}

		SecCertificateRef assetCerts[numRoots];
		for (iCnt = 0; iCnt < numRoots; iCnt++)
		{
			certData = (CFDataRef)CFArrayGetValueAtIndex(cert_datas, iCnt);
			if (NULL != certData)
			{
				SecCertificateRef aCertRef = SecCertificateCreateWithData(kCFAllocatorDefault, certData);
				assetCerts[iCnt] = aCertRef;
			}
			else
			{
				assetCerts[iCnt] = NULL;
			}
		}

		if (numRoots > 0)
		{
			result = CFArrayCreate(kCFAllocatorDefault, (const void **)assetCerts, numRoots, &kCFTypeArrayCallBacks);
			for (iCnt = 0; iCnt < numRoots; iCnt++)
			{
				if (NULL != assetCerts[iCnt])
				{
					CFRelease(assetCerts[iCnt]);
				}
			}
		}
		CFRelease(cert_datas);
	}
    return result;
}

