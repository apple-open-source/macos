/*
 * Copyright (c) 2002-2010 Apple Inc. All Rights Reserved.
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
// CertificateValues.cpp
//
#include <security_keychain/Certificate.h>
#include <Security/oidscert.h>
#include <Security/oidsattr.h>
#include <Security/SecCertificate.h>
#include <Security/SecCertificatePriv.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>
#include "SecCertificateOIDs.h"
#include "CertificateValues.h"
#include "SecCertificateP.h"
#include "SecCertificatePrivP.h"
#include <CoreFoundation/CFNumber.h>

extern "C" void appendProperty(CFMutableArrayRef properties, CFStringRef propertyType, CFStringRef label, CFTypeRef value);

extern CFStringRef kSecPropertyKeyType;
extern CFStringRef kSecPropertyKeyLabel;
extern CFStringRef kSecPropertyKeyLocalizedLabel;
extern CFStringRef kSecPropertyKeyValue;

extern CFStringRef kSecPropertyTypeData;
extern CFStringRef kSecPropertyTypeString;
extern CFStringRef kSecPropertyTypeURL;
extern CFStringRef kSecPropertyTypeDate;

CFStringRef kSecPropertyTypeArray             = CFSTR("array");
CFStringRef kSecPropertyTypeNumber            = CFSTR("number");


#pragma mark ---------- CertificateValues Implementation ----------

using namespace KeychainCore;

void addFieldValues(const void *key, const void *value, void *context);
void addPropertyToFieldValues(const void *value, void *context);
void filterFieldValues(const void *key, const void *value, void *context);
void validateKeys(const void *value, void *context);

CFDictionaryRef CertificateValues::mOIDRemap = NULL;

typedef struct FieldValueFilterContext
{
	CFMutableDictionaryRef filteredValues;
	CFArrayRef filterKeys;
} FieldValueFilterContext;

CertificateValues::CertificateValues(SecCertificateRef certificateRef) : mCertificateRef(certificateRef), 
	mCertificateData(NULL)
{
}

CertificateValues::~CertificateValues() throw()
{
	if (mCertificateData)
		CFRelease(mCertificateData);
}

CFDictionaryRef CertificateValues::copyFieldValues(CFArrayRef keys, CFErrorRef *error)
{
	if (keys)
	{
		if (CFGetTypeID(keys)!=CFArrayGetTypeID())
			return NULL;
		CFRange range = CFRangeMake(0, CFArrayGetCount((CFArrayRef)keys));
		bool failed = false;
		CFArrayApplyFunction(keys, range, validateKeys, &failed);
		if (failed)
			return NULL;
	}

	if (mCertificateData)
	{
		CFRelease(mCertificateData);
		mCertificateData = NULL;
	}
	if (!mCertificateData) //!mCertificateData)
	{
		mCertificateData = SecCertificateCopyData(mCertificateRef);	// OK to call, no big lock
		if (!mCertificateData && error)
		{
			*error = CFErrorCreate(NULL, kCFErrorDomainOSStatus, errSecInvalidCertificateRef, NULL);
			return NULL;
		}
	}

	SecCertificateRefP certificateP = SecCertificateCreateWithDataP(kCFAllocatorDefault, mCertificateData);
	if (!certificateP && error)
	{
		*error = CFErrorCreate(NULL, kCFErrorDomainOSStatus, errSecInvalidCertificateGroup, NULL);
		return NULL;
	}
	
	CFMutableDictionaryRef fieldValues=CFDictionaryCreateMutable(kCFAllocatorDefault, 0, 
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

	// Return an array of CFStringRefs representing the common names in the certificates subject if any
	CFArrayRef commonNames=SecCertificateCopyCommonNames(certificateP);
	if (commonNames)
	{
		CFMutableArrayRef additionalValues = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
		appendProperty(additionalValues, kSecPropertyTypeArray, CFSTR("CN"), commonNames);
		CFDictionaryAddValue(fieldValues, kSecOIDCommonName, (CFTypeRef)CFArrayGetValueAtIndex(additionalValues, 0));
		CFRelease(commonNames);
		CFRelease(additionalValues);
	}

	// These can exist in the subject alt name or in the subject
	CFArrayRef dnsNames=SecCertificateCopyDNSNames(certificateP);
	if (dnsNames)
	{
		CFMutableArrayRef additionalValues = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
		appendProperty(additionalValues, kSecPropertyTypeArray, CFSTR("DNS"), dnsNames);
		CFDictionaryAddValue(fieldValues, CFSTR("DNSNAMES"), (CFTypeRef)CFArrayGetValueAtIndex(additionalValues, 0));
		CFRelease(dnsNames);
		CFRelease(additionalValues);
	}

	CFArrayRef ipAddresses=SecCertificateCopyIPAddresses(certificateP);
	if (ipAddresses)
	{
		CFMutableArrayRef additionalValues = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
		appendProperty(additionalValues, kSecPropertyTypeArray, CFSTR("IP"), dnsNames);
		CFDictionaryAddValue(fieldValues, CFSTR("IPADDRESSES"), (CFTypeRef)CFArrayGetValueAtIndex(additionalValues, 0));
		CFRelease(ipAddresses);
		CFRelease(additionalValues);
	}

	// These can exist in the subject alt name or in the subject
	CFArrayRef emailAddrs=SecCertificateCopyRFC822Names(certificateP);
	if (emailAddrs)
	{
		CFMutableArrayRef additionalValues = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
		appendProperty(additionalValues, kSecPropertyTypeArray, CFSTR("DNS"), dnsNames);
		CFDictionaryAddValue(fieldValues, kSecOIDEmailAddress, (CFTypeRef)CFArrayGetValueAtIndex(additionalValues, 0));
		CFRelease(emailAddrs);
		CFRelease(additionalValues);
	}

	CFAbsoluteTime notBefore = SecCertificateNotValidBefore(certificateP);
	CFNumberRef notBeforeRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &notBefore);
	if (notBeforeRef)
	{
		CFMutableArrayRef additionalValues = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
		appendProperty(additionalValues, kSecPropertyTypeNumber, CFSTR("Not Valid Before"), notBeforeRef);
		CFDictionaryAddValue(fieldValues, kSecOIDX509V1ValidityNotBefore, (CFTypeRef)CFArrayGetValueAtIndex(additionalValues, 0));
		CFRelease(notBeforeRef);
		CFRelease(additionalValues);
	}

	CFAbsoluteTime notAfter = SecCertificateNotValidAfter(certificateP);
	CFNumberRef notAfterRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &notAfter);
	if (notAfterRef)
	{
		CFMutableArrayRef additionalValues = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
		appendProperty(additionalValues, kSecPropertyTypeNumber, CFSTR("Not Valid After"), notAfterRef);
		CFDictionaryAddValue(fieldValues, kSecOIDX509V1ValidityNotAfter, (CFTypeRef)CFArrayGetValueAtIndex(additionalValues, 0));
		CFRelease(notAfterRef);
		CFRelease(additionalValues);
	}

	SecKeyUsage keyUsage=SecCertificateGetKeyUsage(certificateP);
	CFNumberRef ku = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &keyUsage);
	if (ku)
	{
		CFMutableArrayRef additionalValues = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
		appendProperty(additionalValues, kSecPropertyTypeNumber, CFSTR("Key Usage"), ku);
		CFDictionaryAddValue(fieldValues, kSecOIDKeyUsage, (CFTypeRef)CFArrayGetValueAtIndex(additionalValues, 0));
		CFRelease(ku);
		CFRelease(additionalValues);
	}

	CFArrayRef ekus = SecCertificateCopyExtendedKeyUsage(certificateP);
	if (ekus)
	{
		CFMutableArrayRef additionalValues = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
		appendProperty(additionalValues, kSecPropertyTypeArray, CFSTR("Extended Key Usage"), ekus);
		CFDictionaryAddValue(fieldValues, kSecOIDExtendedKeyUsage, (CFTypeRef)CFArrayGetValueAtIndex(additionalValues, 0));
		CFRelease(ekus);
		CFRelease(additionalValues);
	}

	// Add all values from properties dictionary
	CFArrayRef properties = SecCertificateCopyProperties(certificateP);
	if (properties)
	{
		CFRange range = CFRangeMake(0, CFArrayGetCount((CFArrayRef)properties));
		CFArrayApplyFunction(properties, range, addPropertyToFieldValues, fieldValues);
	//	CFDictionaryApplyFunction(properties, addFieldValues, fieldValues);
		CFRelease(properties);
	}

	CFAbsoluteTime verifyTime = CFAbsoluteTimeGetCurrent();
	CFMutableArrayRef summaryProperties = 
		SecCertificateCopySummaryProperties(certificateP, verifyTime);
	if (summaryProperties)
	{
		CFRange range = CFRangeMake(0, CFArrayGetCount((CFArrayRef)summaryProperties));
		CFArrayApplyFunction(summaryProperties, range, addPropertyToFieldValues, fieldValues);
//		CFDictionaryApplyFunction(summaryProperties, addFieldValues, fieldValues);
//		CFDictionaryAddValue(fieldValues, CFSTR("summaryProperties"), summaryProperties);
		CFRelease(summaryProperties);
	}

	if (certificateP)
		CFRelease(certificateP);

	if (keys==NULL)
		return (CFDictionaryRef)fieldValues;
	
	// Otherwise, we need to filter
	CFMutableDictionaryRef filteredFieldValues=CFDictionaryCreateMutable(kCFAllocatorDefault, 0, 
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

	FieldValueFilterContext fvcontext;
	fvcontext.filteredValues = filteredFieldValues;
	fvcontext.filterKeys = keys;
		
	CFDictionaryApplyFunction(fieldValues, filterFieldValues, &fvcontext);

	CFRelease(fieldValues);
	return (CFDictionaryRef)filteredFieldValues;
}

void validateKeys(const void *value, void *context)
{
	if (value == NULL || (CFGetTypeID(value)!=CFStringGetTypeID()))
		if (context)
			*(bool *)context = true;
}

void filterFieldValues(const void *key, const void *value, void *context)
{
	// each element of keys is a CFStringRef with an OID, e.g.
	// CFTypeRef kSecOIDTitle = CFSTR("2.5.4.12");

	CFTypeRef fieldKey = (CFTypeRef)key;
	if (fieldKey == NULL || (CFGetTypeID(fieldKey)!=CFStringGetTypeID()) || context==NULL)
		return;
	
	FieldValueFilterContext *fvcontext = (FieldValueFilterContext *)context;
		
	CFRange range = CFRangeMake(0, CFArrayGetCount(fvcontext->filterKeys));
	CFIndex idx = CFArrayGetFirstIndexOfValue(fvcontext->filterKeys, range, fieldKey);
	if (idx != kCFNotFound)
		CFDictionaryAddValue(fvcontext->filteredValues, fieldKey, value);
}

void addFieldValues(const void *key, const void *value, void *context)
{
	CFMutableDictionaryRef fieldValues = (CFMutableDictionaryRef)context;
	CFDictionaryAddValue(fieldValues, key, value);
}

void addPropertyToFieldValues(const void *value, void *context)
{
	CFMutableDictionaryRef fieldValues = (CFMutableDictionaryRef)context;
	if (CFGetTypeID(value)==CFDictionaryGetTypeID())
	{
		CFStringRef label = (CFStringRef)CFDictionaryGetValue((CFDictionaryRef)value, kSecPropertyKeyLabel);
#if 0
		CFStringRef typeD = (CFStringRef)CFDictionaryGetValue((CFDictionaryRef)value, kSecPropertyKeyType);
		CFTypeRef valueD = (CFStringRef)CFDictionaryGetValue((CFDictionaryRef)value, kSecPropertyKeyValue);
#endif
		CFStringRef key = CertificateValues::remapLabelToKey(label);
		if (key) 
			CFDictionaryAddValue(fieldValues, key, value);
	}
}

CFStringRef CertificateValues::remapLabelToKey(CFStringRef label)
{
	if (!label)
		return NULL;
		
	if (!mOIDRemap)
	{
		CFTypeRef keys[] =
		{
			CFSTR("Subject Name"),
			CFSTR("Normalized Subject Name"),
			CFSTR("Issuer Name"),
			CFSTR("Normalized Subject Name"),
			CFSTR("Version"),
			CFSTR("Serial Number"),
			CFSTR("Signature Algorithm"),
			CFSTR("Subject Unique ID"),
			CFSTR("Issuer Unique ID"),
			CFSTR("Public Key Algorithm"),
			CFSTR("Public Key Data"),
			CFSTR("Signature"),
			CFSTR("Not Valid Before"),
			CFSTR("Not Valid After"),
			CFSTR("Expires")
		};
		
		CFTypeRef values[] =
		{ 
			kSecOIDX509V1SubjectName,
			kSecOIDX509V1SubjectNameStd,
			kSecOIDX509V1IssuerName, 
			kSecOIDX509V1IssuerNameStd, 
			kSecOIDX509V1Version,
			kSecOIDX509V1SerialNumber,
			kSecOIDX509V1SignatureAlgorithm,	// or CSSMOID_X509V1SignatureAlgorithmTBS?
			kSecOIDX509V1CertificateSubjectUniqueId,
			kSecOIDX509V1CertificateIssuerUniqueId,
			kSecOIDX509V1SubjectPublicKeyAlgorithm,
			kSecOIDX509V1SubjectPublicKey,
			kSecOIDX509V1Signature,
			kSecOIDX509V1ValidityNotBefore,
			kSecOIDX509V1ValidityNotAfter,
			kSecOIDInvalidityDate	
		};

		mOIDRemap = CFDictionaryCreate(NULL, keys, values,
			(sizeof(keys) / sizeof(*keys)), &kCFTypeDictionaryKeyCallBacks, 
			&kCFTypeDictionaryValueCallBacks);
	}
	
	CFTypeRef result = (CFTypeRef)CFDictionaryGetValue(mOIDRemap, label);

	return result?(CFStringRef)result:label;
}

CFDataRef CertificateValues::copySerialNumber(CFErrorRef *error)
{
	SecCertificateRefP certificateP = getSecCertificateRefP(error);
		
    return certificateP?SecCertificateCopySerialNumberP(certificateP):NULL;
}

CFDataRef CertificateValues::getNormalizedIssuerContent(CFErrorRef *error)
{
	// We wrap with SecDERItemCopySequence, since SecItemCopyMatching expects it
	SecCertificateRefP certificateP = getSecCertificateRefP(error);
    return certificateP ? SecCertificateGetNormalizedIssuer(certificateP) : NULL;
}

CFDataRef CertificateValues::getNormalizedSubjectContent(CFErrorRef *error)
{
	// We wrap with SecDERItemCopySequence, since SecItemCopyMatching expects it
	SecCertificateRefP certificateP = getSecCertificateRefP(error);
	
    return certificateP ? SecCertificateGetNormalizedSubject(certificateP) : NULL;
}

SecCertificateRefP CertificateValues::getSecCertificateRefP(CFErrorRef *error)
{
	// SecCertificateCopyData returns an object created with CFDataCreate, so we
	// own it and must release it
	
	if (mCertificateData)
	{
		CFRelease(mCertificateData);
		mCertificateData = NULL;
	}
	
	mCertificateData = SecCertificateCopyData(mCertificateRef);	// OK to call, no big lock
	if (!mCertificateData && error)
	{
		*error = CFErrorCreate(NULL, kCFErrorDomainOSStatus, errSecInvalidCertificateRef, NULL);
		return NULL;
	}

	SecCertificateRefP certificateP = SecCertificateCreateWithDataP(kCFAllocatorDefault, mCertificateData);
	if (!certificateP && error)
	{
		*error = CFErrorCreate(NULL, kCFErrorDomainOSStatus, errSecInvalidCertificateGroup, NULL);
		return NULL;
	}
	
	return certificateP;
}

#pragma mark ---------- OID Constants ----------

CFTypeRef kSecOIDADC_CERT_POLICY = CFSTR("1.2.840.113635.100.5.3");
CFTypeRef kSecOIDAPPLE_CERT_POLICY = CFSTR("1.2.840.113635.100.5.1");
CFTypeRef kSecOIDAPPLE_EKU_CODE_SIGNING = CFSTR("1.2.840.113635.100.4.1");
CFTypeRef kSecOIDAPPLE_EKU_CODE_SIGNING_DEV = CFSTR("1.2.840.113635.100.4.1.1");
CFTypeRef kSecOIDAPPLE_EKU_ICHAT_ENCRYPTION = CFSTR("1.2.840.113635.100.4.3");
CFTypeRef kSecOIDAPPLE_EKU_ICHAT_SIGNING = CFSTR("1.2.840.113635.100.4.2");
CFTypeRef kSecOIDAPPLE_EKU_RESOURCE_SIGNING = CFSTR("1.2.840.113635.100.4.1.4");
CFTypeRef kSecOIDAPPLE_EKU_SYSTEM_IDENTITY = CFSTR("1.2.840.113635.100.4.4");
CFTypeRef kSecOIDAPPLE_EXTENSION = CFSTR("1.2.840.113635.100.6");
CFTypeRef kSecOIDAPPLE_EXTENSION_ADC_APPLE_SIGNING = CFSTR("1.2.840.113635.100.6.1.2.0.0");
CFTypeRef kSecOIDAPPLE_EXTENSION_ADC_DEV_SIGNING = CFSTR("1.2.840.113635.100.6.1.2.0");
CFTypeRef kSecOIDAPPLE_EXTENSION_APPLE_SIGNING = CFSTR("1.2.840.113635.100.6.1.1");
CFTypeRef kSecOIDAPPLE_EXTENSION_CODE_SIGNING = CFSTR("1.2.840.113635.100.6.1");
CFTypeRef kSecOIDAPPLE_EXTENSION_INTERMEDIATE_MARKER = CFSTR("1.2.840.113635.100.6.2");
CFTypeRef kSecOIDAPPLE_EXTENSION_WWDR_INTERMEDIATE = CFSTR("1.2.840.113635.100.6.2.1");
CFTypeRef kSecOIDAPPLE_EXTENSION_ITMS_INTERMEDIATE = CFSTR("1.2.840.113635.100.6.2.2");
CFTypeRef kSecOIDAPPLE_EXTENSION_AAI_INTERMEDIATE = CFSTR("1.2.840.113635.100.6.2.3");
CFTypeRef kSecOIDAuthorityInfoAccess = CFSTR("1.3.6.1.5.5.7.1.1");
CFTypeRef kSecOIDAuthorityKeyIdentifier = CFSTR("2.5.29.35");
CFTypeRef kSecOIDBasicConstraints = CFSTR("2.5.29.19");
CFTypeRef kSecOIDBiometricInfo = CFSTR("1.3.6.1.5.5.7.1.2");
CFTypeRef kSecOIDCSSMKeyStruct = CFSTR("2.16.840.1.113741.2.1.1.1.20");
CFTypeRef kSecOIDCertIssuer = CFSTR("2.5.29.29");
CFTypeRef kSecOIDCertificatePolicies = CFSTR("2.5.29.32");
CFTypeRef kSecOIDClientAuth = CFSTR("1.3.6.1.5.5.7.3.2");
CFTypeRef kSecOIDCollectiveStateProvinceName = CFSTR("2.5.4.8.1");
CFTypeRef kSecOIDCollectiveStreetAddress = CFSTR("2.5.4.9.1");
CFTypeRef kSecOIDCommonName = CFSTR("2.5.4.3");
CFTypeRef kSecOIDCountryName = CFSTR("2.5.4.6");
CFTypeRef kSecOIDCrlDistributionPoints = CFSTR("2.5.29.31");
CFTypeRef kSecOIDCrlNumber = CFSTR("2.5.29.20");
CFTypeRef kSecOIDCrlReason = CFSTR("2.5.29.21");
CFTypeRef kSecOIDDOTMAC_CERT_EMAIL_ENCRYPT = CFSTR("1.2.840.113635.100.3.2.3");
CFTypeRef kSecOIDDOTMAC_CERT_EMAIL_SIGN = CFSTR("1.2.840.113635.100.3.2.2");
CFTypeRef kSecOIDDOTMAC_CERT_EXTENSION = CFSTR("1.2.840.113635.100.3.2");
CFTypeRef kSecOIDDOTMAC_CERT_IDENTITY = CFSTR("1.2.840.113635.100.3.2.1");
CFTypeRef kSecOIDDOTMAC_CERT_POLICY = CFSTR("1.2.840.113635.100.5.2");
CFTypeRef kSecOIDDeltaCrlIndicator = CFSTR("2.5.29.27");
CFTypeRef kSecOIDDescription = CFSTR("2.5.4.13");
CFTypeRef kSecOIDEKU_IPSec = CFSTR("1.3.6.1.5.5.8.2.2");
CFTypeRef kSecOIDEmailAddress = CFSTR("1.2.840.113549.1.9.1");
CFTypeRef kSecOIDEmailProtection = CFSTR("1.3.6.1.5.5.7.3.4");
CFTypeRef kSecOIDExtendedKeyUsage = CFSTR("2.5.29.37");
CFTypeRef kSecOIDExtendedKeyUsageAny = CFSTR("2.5.29.37.0");
CFTypeRef kSecOIDExtendedUseCodeSigning = CFSTR("1.3.6.1.5.5.7.3.3");
CFTypeRef kSecOIDGivenName = CFSTR("2.5.4.42");
CFTypeRef kSecOIDHoldInstructionCode = CFSTR("2.5.29.23");
CFTypeRef kSecOIDInvalidityDate = CFSTR("2.5.29.24");
CFTypeRef kSecOIDIssuerAltName = CFSTR("2.5.29.18");
CFTypeRef kSecOIDIssuingDistributionPoint = CFSTR("2.5.29.28");
CFTypeRef kSecOIDIssuingDistributionPoints = CFSTR("2.5.29.28");
CFTypeRef kSecOIDKERBv5_PKINIT_KP_CLIENT_AUTH = CFSTR("1.3.6.1.5.2.3.4");
CFTypeRef kSecOIDKERBv5_PKINIT_KP_KDC = CFSTR("1.3.6.1.5.2.3.5");
CFTypeRef kSecOIDKeyUsage = CFSTR("2.5.29.15");
CFTypeRef kSecOIDLocalityName = CFSTR("2.5.4.7");
CFTypeRef kSecOIDMS_NTPrincipalName = CFSTR("1.3.6.1.4.1.311.20.2.3");
CFTypeRef kSecOIDMicrosoftSGC = CFSTR("1.3.6.1.4.1.311.10.3.3");
CFTypeRef kSecOIDNameConstraints = CFSTR("2.5.29.30");
CFTypeRef kSecOIDNetscapeCertSequence = CFSTR("2.16.840.1.113730.2.5");
CFTypeRef kSecOIDNetscapeCertType = CFSTR("2.16.840.1.113730.1.1");
CFTypeRef kSecOIDNetscapeSGC = CFSTR("2.16.840.1.113730.4.1");
CFTypeRef kSecOIDOCSPSigning = CFSTR("1.3.6.1.5.5.7.3.9");
CFTypeRef kSecOIDOrganizationName = CFSTR("2.5.4.10");
CFTypeRef kSecOIDOrganizationalUnitName = CFSTR("2.5.4.11");
CFTypeRef kSecOIDPolicyConstraints = CFSTR("2.5.29.36");
CFTypeRef kSecOIDPolicyMappings = CFSTR("2.5.29.33");
CFTypeRef kSecOIDPrivateKeyUsagePeriod = CFSTR("2.5.29.16");
CFTypeRef kSecOIDQC_Statements = CFSTR("1.3.6.1.5.5.7.1.3");
CFTypeRef kSecOIDSerialNumber = CFSTR("2.5.4.5");
CFTypeRef kSecOIDServerAuth = CFSTR("1.3.6.1.5.5.7.3.1");
CFTypeRef kSecOIDStateProvinceName = CFSTR("2.5.4.8");
CFTypeRef kSecOIDStreetAddress = CFSTR("2.5.4.9");
CFTypeRef kSecOIDSubjectAltName = CFSTR("2.5.29.17");
CFTypeRef kSecOIDSubjectDirectoryAttributes = CFSTR("2.5.29.9");
CFTypeRef kSecOIDSubjectEmailAddress = CFSTR("2.16.840.1.113741.2.1.1.1.50.3");
CFTypeRef kSecOIDSubjectInfoAccess = CFSTR("1.3.6.1.5.5.7.1.11");
CFTypeRef kSecOIDSubjectKeyIdentifier = CFSTR("2.5.29.14");
CFTypeRef kSecOIDSubjectPicture = CFSTR("2.16.840.1.113741.2.1.1.1.50.2");
CFTypeRef kSecOIDSubjectSignatureBitmap = CFSTR("2.16.840.1.113741.2.1.1.1.50.1");
CFTypeRef kSecOIDSurname = CFSTR("2.5.4.4");
CFTypeRef kSecOIDTimeStamping = CFSTR("1.3.6.1.5.5.7.3.8");
CFTypeRef kSecOIDTitle = CFSTR("2.5.4.12");
CFTypeRef kSecOIDUseExemptions = CFSTR("2.16.840.1.113741.2.1.1.1.50.4");
CFTypeRef kSecOIDX509V1CertificateIssuerUniqueId = CFSTR("2.16.840.1.113741.2.1.1.1.11");
CFTypeRef kSecOIDX509V1CertificateSubjectUniqueId = CFSTR("2.16.840.1.113741.2.1.1.1.12");
CFTypeRef kSecOIDX509V1IssuerName = CFSTR("2.16.840.1.113741.2.1.1.1.5");
CFTypeRef kSecOIDX509V1IssuerNameCStruct = CFSTR("2.16.840.1.113741.2.1.1.1.5.1");
CFTypeRef kSecOIDX509V1IssuerNameLDAP = CFSTR("2.16.840.1.113741.2.1.1.1.5.2");
CFTypeRef kSecOIDX509V1IssuerNameStd = CFSTR("2.16.840.1.113741.2.1.1.1.23");
CFTypeRef kSecOIDX509V1SerialNumber = CFSTR("2.16.840.1.113741.2.1.1.1.3");
CFTypeRef kSecOIDX509V1Signature = CFSTR("2.16.840.1.113741.2.1.3.2.2");
CFTypeRef kSecOIDX509V1SignatureAlgorithm = CFSTR("2.16.840.1.113741.2.1.3.2.1");
CFTypeRef kSecOIDX509V1SignatureAlgorithmParameters = CFSTR("2.16.840.1.113741.2.1.3.2.3");
CFTypeRef kSecOIDX509V1SignatureAlgorithmTBS = CFSTR("2.16.840.1.113741.2.1.3.2.10");
CFTypeRef kSecOIDX509V1SignatureCStruct = CFSTR("2.16.840.1.113741.2.1.3.2.0.1");
CFTypeRef kSecOIDX509V1SignatureStruct = CFSTR("2.16.840.1.113741.2.1.3.2.0");
CFTypeRef kSecOIDX509V1SubjectName = CFSTR("2.16.840.1.113741.2.1.1.1.8");
CFTypeRef kSecOIDX509V1SubjectNameCStruct = CFSTR("2.16.840.1.113741.2.1.1.1.8.1");
CFTypeRef kSecOIDX509V1SubjectNameLDAP = CFSTR("2.16.840.1.113741.2.1.1.1.8.2");
CFTypeRef kSecOIDX509V1SubjectNameStd = CFSTR("2.16.840.1.113741.2.1.1.1.22");
CFTypeRef kSecOIDX509V1SubjectPublicKey = CFSTR("2.16.840.1.113741.2.1.1.1.10");
CFTypeRef kSecOIDX509V1SubjectPublicKeyAlgorithm = CFSTR("2.16.840.1.113741.2.1.1.1.9");
CFTypeRef kSecOIDX509V1SubjectPublicKeyAlgorithmParameters = CFSTR("2.16.840.1.113741.2.1.1.1.18");
CFTypeRef kSecOIDX509V1SubjectPublicKeyCStruct = CFSTR("2.16.840.1.113741.2.1.1.1.20.1");
CFTypeRef kSecOIDX509V1ValidityNotAfter = CFSTR("2.16.840.1.113741.2.1.1.1.7");
CFTypeRef kSecOIDX509V1ValidityNotBefore = CFSTR("2.16.840.1.113741.2.1.1.1.6");
CFTypeRef kSecOIDX509V1Version = CFSTR("2.16.840.1.113741.2.1.1.1.2");
CFTypeRef kSecOIDX509V3Certificate = CFSTR("2.16.840.1.113741.2.1.1.1.1");
CFTypeRef kSecOIDX509V3CertificateCStruct = CFSTR("2.16.840.1.113741.2.1.1.1.1.1");
CFTypeRef kSecOIDX509V3CertificateExtensionCStruct = CFSTR("2.16.840.1.113741.2.1.1.1.13.1");
CFTypeRef kSecOIDX509V3CertificateExtensionCritical = CFSTR("2.16.840.1.113741.2.1.1.1.16");
CFTypeRef kSecOIDX509V3CertificateExtensionId = CFSTR("2.16.840.1.113741.2.1.1.1.15");
CFTypeRef kSecOIDX509V3CertificateExtensionStruct = CFSTR("2.16.840.1.113741.2.1.1.1.13");
CFTypeRef kSecOIDX509V3CertificateExtensionType = CFSTR("2.16.840.1.113741.2.1.1.1.19");
CFTypeRef kSecOIDX509V3CertificateExtensionValue = CFSTR("2.16.840.1.113741.2.1.1.1.17");
CFTypeRef kSecOIDX509V3CertificateExtensionsCStruct = CFSTR("2.16.840.1.113741.2.1.1.1.21.1");
CFTypeRef kSecOIDX509V3CertificateExtensionsStruct = CFSTR("2.16.840.1.113741.2.1.1.1.21");
CFTypeRef kSecOIDX509V3CertificateNumberOfExtensions = CFSTR("2.16.840.1.113741.2.1.1.1.14");
CFTypeRef kSecOIDX509V3SignedCertificate = CFSTR("2.16.840.1.113741.2.1.1.1.0");
CFTypeRef kSecOIDX509V3SignedCertificateCStruct = CFSTR("2.16.840.1.113741.2.1.1.1.0.1");

