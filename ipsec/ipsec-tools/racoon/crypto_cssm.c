
/*
 * Copyright (c) 2001-2004 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */


/*
 * Racoon module for verifying and signing certificates through Security
 * Framework and CSSM
 */

#include <Security/SecCertificate.h>
#include <Security/SecPolicy.h>
#include <Security/SecTrust.h>
#include <Security/SecKey.h>
#include <Security/SecIdentity.h>

#include <TargetConditionals.h>
#if TARGET_OS_EMBEDDED
#include <Security/SecItem.h>
#include <Security/SecTrustPriv.h>
#include <Security/SecPolicyPriv.h>
#include <Security/SecCertificatePriv.h>
#else
#include <Security/SecBase.h>
#include <Security/SecIdentityPriv.h>
#include <Security/SecIdentitySearch.h>
#include <Security/SecKeychain.h>
#include <Security/SecKeychainItem.h>
#include <Security/SecKeychainItemPriv.h>

#include <Security/SecKeyPriv.h>
#include <Security/oidsalg.h>
#include <Security/cssmapi.h>
#include <Security/SecPolicySearch.h>
#endif

#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>
#include "plog.h"
#include "debug.h"
#include "misc.h"
#include "oakley.h"


#include "crypto_cssm.h"


static OSStatus EvaluateCert(SecCertificateRef cert, CFTypeRef policyRef);
static const char *GetSecurityErrorString(OSStatus err);
#if !TARGET_OS_EMBEDDED
static OSStatus FindPolicy(const CSSM_OID *policyOID, SecPolicyRef *policyRef);
static OSStatus CopySystemKeychain(SecKeychainRef *keychainRef);
#endif

/*
 * Verify cert using security framework
 */
int crypto_cssm_check_x509cert(vchar_t *cert, CFStringRef hostname, cert_status_t certStatus)
{
	OSStatus			status;
	SecCertificateRef	certRef = NULL;
	SecPolicyRef		policyRef = NULL;

#if !TARGET_OS_EMBEDDED
	CSSM_DATA			certData;
	CSSM_OID			ourPolicyOID = CSSMOID_APPLE_TP_IP_SEC; 

	// create cert ref
	certData.Length = cert->l;
	certData.Data = (uint8 *)cert->v;
	status = SecCertificateCreateFromData(&certData, CSSM_CERT_X_509v3, CSSM_CERT_ENCODING_DER,
		&certRef);
	if (status != noErr)
		goto end;
	
	// get our policy object
	status = FindPolicy(&ourPolicyOID, &policyRef);
	if (status != noErr)
		goto end;
	// no options used at present - verification of subjectAltName fields, etc.
	// are done elsewhere in racoon in oakley_check_certid()
		
#else
	CFDataRef cert_data = CFDataCreateWithBytesNoCopy(NULL, cert->v, cert->l, kCFAllocatorNull);
    if (cert_data) {
        certRef = SecCertificateCreateWithData(NULL, cert_data);
        CFRelease(cert_data);
    }

	if (certRef == NULL) {
		plog(LLV_ERROR, LOCATION, NULL, 
			"unable to create a certRef.\n");
		status = -1;
		goto end;
	}

	if (hostname) {
		policyRef = SecPolicyCreateIPSec(FALSE, hostname);
		if (policyRef == NULL) {
			plog(LLV_ERROR, LOCATION, NULL, 
				"unable to create a SSL policyRef.\n");
			status = -1;
			goto end;
		}
	}
	
#endif
	
	// evaluate cert
	status = EvaluateCert(certRef, policyRef);
	
end:

	if (certRef)
		CFRelease(certRef);
	if (policyRef)
		CFRelease(policyRef);
	
	if (status != noErr && status != -1) {
		plog(LLV_ERROR, LOCATION, NULL, 
			"error %d %s.\n", status, GetSecurityErrorString(status));
		status = -1;
	} else if (certStatus) {
		plog(LLV_ERROR, LOCATION, NULL,
			 "certificate failed date verification: %d.\n", certStatus);
		status = -1;
	}
	return status;

}

/*
 * Encrypt a hash via CSSM using the private key in the keychain
 * from an identity.
 */
vchar_t* crypto_cssm_getsign(CFDataRef persistentCertRef, vchar_t* hash)
{

	OSStatus						status;
	SecIdentityRef 					identityRef = NULL;
	SecKeyRef						privateKeyRef = NULL;
	vchar_t							*sig = NULL;

#if !TARGET_OS_EMBEDDED
	u_int32_t						bytesEncrypted = 0;
	SecCertificateRef				certificateRef = NULL;
	SecIdentitySearchRef			idSearchRef = NULL;
	SecKeychainRef 					keychainRef = NULL;
	const CSSM_KEY					*cssmKey = NULL;
	CSSM_CSP_HANDLE 				cspHandle = nil;
	CSSM_CC_HANDLE					cssmContextHandle = nil;
	const CSSM_ACCESS_CREDENTIALS	*credentials = NULL;
	//CSSM_SIZE						bytesEncrypted = 0;	//%%%%HWR fix this - need new headers on Leopard
	CSSM_DATA						clearData;
	CSSM_DATA						cipherData;
	CSSM_DATA						remData;
	CSSM_CONTEXT_ATTRIBUTE			newAttr;

	remData.Length = 0;
	remData.Data = 0;

	if (persistentCertRef) {	
		// get cert from keychain
		status = SecKeychainItemCopyFromPersistentReference(persistentCertRef, (SecKeychainItemRef*)&certificateRef);
		if (status != noErr)
			goto end;
	
		// get keychain ref where cert is contained
		status = SecKeychainItemCopyKeychain((SecKeychainItemRef)certificateRef, &keychainRef);
		if (status != noErr)
			goto end;
	
		// get identity from the certificate
		status = SecIdentityCreateWithCertificate(keychainRef, certificateRef, &identityRef);
		if (status != noErr)
			goto end;	
			
	} else {
	
		// copy system keychain
		status = CopySystemKeychain(&keychainRef);
		if (status != noErr)
			goto end;

		// serach for first identity in system keychain
		status = SecIdentitySearchCreate(keychainRef, CSSM_KEYUSE_SIGN, &idSearchRef);
		if (status != noErr)
			goto end;
		
		status = SecIdentitySearchCopyNext(idSearchRef, &identityRef);
		if (status != noErr)
			goto end;

		// get certificate from identity
		status = SecIdentityCopyCertificate(identityRef, &certificateRef);
		if (status != noErr)
			goto end;
	}
	
	// get private key from identity
	status = SecIdentityCopyPrivateKey(identityRef, &privateKeyRef);
	if (status != noErr)
		goto end;
		
	// get CSSM_KEY pointer from key ref
	status = SecKeyGetCSSMKey(privateKeyRef, &cssmKey);
	if (status != noErr)
		goto end;
		
	// get CSSM CSP handle
	status = SecKeychainGetCSPHandle(keychainRef, &cspHandle);
	if (status != noErr)
		goto end;
		
	// create CSSM credentials to unlock private key for encryption - no UI to be used
	status = SecKeyGetCredentials(privateKeyRef, CSSM_ACL_AUTHORIZATION_ENCRYPT,
				kSecCredentialTypeNoUI, &credentials);
	if (status != noErr)
		goto end;	

	// create asymmetric context for encryption
	status = CSSM_CSP_CreateAsymmetricContext(cspHandle, CSSM_ALGID_RSA, credentials, cssmKey, 
			CSSM_PADDING_PKCS1, &cssmContextHandle);
	if (status != noErr)
		goto end;
		
	// add mode attribute to use private key for encryption
	newAttr.AttributeType     = CSSM_ATTRIBUTE_MODE;
	newAttr.AttributeLength   = sizeof(uint32);
	newAttr.Attribute.Data    = (CSSM_DATA_PTR)CSSM_ALGMODE_PRIVATE_KEY;
	status = CSSM_UpdateContextAttributes(cssmContextHandle, 1, &newAttr);
	if(status != noErr)
		goto end;
			
	// and finally - encrypt data
	clearData.Length = hash->l;
	clearData.Data = (uint8 *)hash->v;
	cipherData.Length = 0;
	cipherData.Data = NULL;
	status = CSSM_EncryptData(cssmContextHandle, &clearData, 1, &cipherData, 1, &bytesEncrypted, 
						&remData);
	if (status != noErr)
		goto end;
	
	if (remData.Length != 0) {	// something didn't go right - should be zero
		status = -1;
		plog(LLV_ERROR, LOCATION, NULL, 
			"unencrypted data remaining after encrypting hash.\n");
		goto end;
	}

	// alloc buffer for result
	sig = vmalloc(0);
	if (sig == NULL)
		goto end;
		
	sig->l = cipherData.Length;
	sig->v = (caddr_t)cipherData.Data;

#else

	CFDictionaryRef		persistFind = NULL;
	const void			*keys_persist[] = { kSecReturnRef, kSecValuePersistentRef };
	const void			*values_persist[] = { kCFBooleanTrue, persistentCertRef };

	#define SIG_BUF_SIZE 1024
	
	/* find identity by persistent ref */
	persistFind = CFDictionaryCreate(NULL, keys_persist, values_persist,
		(sizeof(keys_persist) / sizeof(*keys_persist)), NULL, NULL);
	if (persistFind == NULL)
		goto end;
	
	status = SecItemCopyMatching(persistFind, (CFTypeRef *)&identityRef);
	if (status != noErr)
		goto end;
		
	status = SecIdentityCopyPrivateKey(identityRef, &privateKeyRef);
	if (status != noErr)
		goto end;

	// alloc buffer for result
	sig = vmalloc(SIG_BUF_SIZE);
	if (sig == NULL)
		goto end;
	
	status = SecKeyRawSign(privateKeyRef, kSecPaddingPKCS1, hash->v,
		hash->l, sig->v, &sig->l);				

#endif	
					
		
end:
	if (identityRef)
		CFRelease(identityRef);
	if (privateKeyRef)
		CFRelease(privateKeyRef);
		
#if !TARGET_OS_EMBEDDED
	if (certificateRef)
		CFRelease(certificateRef);
	if (keychainRef)
		CFRelease(keychainRef);
	if (idSearchRef)
		CFRelease(idSearchRef);
	if (cssmContextHandle)
		CSSM_DeleteContext(cssmContextHandle);
#else
	if (persistFind)
		CFRelease(persistFind);
#endif
	
	if (status != noErr) {
		if (sig) {
			vfree(sig);
			sig = NULL;
		}
	}

	if (status != noErr && status != -1) {
		plog(LLV_ERROR, LOCATION, NULL, 
			"error %d %s.\n", status, GetSecurityErrorString(status));
		status = -1;
	}			
	return sig;
	
}


/*
 * Retrieve a cert from the keychain
 */
vchar_t* crypto_cssm_get_x509cert(CFDataRef persistentCertRef,
                                  cert_status_t *certStatus)
{

	OSStatus				status;
	vchar_t					*cert = NULL;
	SecIdentityRef 			identityRef = NULL;
	SecCertificateRef		certificateRef = NULL;

#if !TARGET_OS_EMBEDDED
	CSSM_DATA				cssmData;	
	SecIdentitySearchRef	idSearchRef = NULL;
	SecKeychainRef 			keychainRef = NULL;

	// get cert ref
	if (persistentCertRef) {
		status = SecKeychainItemCopyFromPersistentReference(persistentCertRef, (SecKeychainItemRef*)&certificateRef);
		if (status != noErr)
			goto end;
	} else {
		// copy system keychain
		status = CopySystemKeychain(&keychainRef);
		if (status != noErr)
			goto end;

		// find first identity in system keychain
		status = SecIdentitySearchCreate(keychainRef, CSSM_KEYUSE_SIGN, &idSearchRef);
		if (status != noErr)
			goto end;
		
		status = SecIdentitySearchCopyNext(idSearchRef, &identityRef);
		if (status != noErr)
			goto end;

		// get certificate from identity
		status = SecIdentityCopyCertificate(identityRef, &certificateRef);
		if (status != noErr)
			goto end;
		
	}

	// get certificate data
	cssmData.Length = 0;
	cssmData.Data = NULL;
	status = SecCertificateGetData(certificateRef, &cssmData);
	if (status != noErr)
		goto end;
		
	if (cssmData.Length == 0)
		goto end;
	
	cert = vmalloc(cssmData.Length);
	if (cert == NULL)
		goto end;	
	
	// cssmData struct just points to the data
	// data must be copied to be returned
	memcpy(cert->v, cssmData.Data, cssmData.Length);	
	
	// verify expiry or missing fields
	if (certStatus) {
		*certStatus = CERT_STATUS_OK;
	}
#else
		
	CFDictionaryRef		persistFind = NULL;
	const void			*keys_persist[] = { kSecReturnRef, kSecValuePersistentRef };
	const void			*values_persist[] = { kCFBooleanTrue, persistentCertRef };
	size_t				dataLen;
	CFDataRef			certData = NULL;
	CFAbsoluteTime		timeNow = 0;
	CFAbsoluteTime		notvalidbeforedate = 0;
	CFAbsoluteTime		notvalidafterdate = 0;
	CFDateRef			nowcfdatedata = NULL;
	CFDateRef			notvalidbeforedatedata = NULL;
	CFDateRef			notvalidafterdatedata = NULL;
	CFArrayRef			certProparray = NULL;
	CFRange				range;
	CFDictionaryRef		*values = NULL;
	CFDictionaryRef		propDict = NULL;
	const void			*datevalue = NULL;
	const void			*labelvalue = NULL;
	CFGregorianDate		gregoriandate;
	int					count;
	int					i;
	
	/* find identity by persistent ref */
	persistFind = CFDictionaryCreate(NULL, keys_persist, values_persist,
		(sizeof(keys_persist) / sizeof(*keys_persist)), NULL, NULL);
	if (persistFind == NULL)
		goto end;
	
	status = SecItemCopyMatching(persistFind, (CFTypeRef *)&identityRef);
	if (status != noErr)
		goto end;

	status = SecIdentityCopyCertificate(identityRef, &certificateRef);
	if (status != noErr)
		goto end;

	certData = SecCertificateCopyData(certificateRef);
	if (certData == NULL)
		goto end;
	
	dataLen = CFDataGetLength(certData);
	if (dataLen == 0)
		goto end;
	
	cert = vmalloc(dataLen);
	if (cert == NULL)
		goto end;	
	
	CFDataGetBytes(certData, CFRangeMake(0, dataLen), cert->v); 
				
	// verify expiry or missing fields
	if (certStatus) {
		
		*certStatus = CERT_STATUS_OK;
		
		if ((certProparray = SecCertificateCopyProperties(certificateRef))){
			if ((count = CFArrayGetCount( certProparray ))){
				range.location = 0;
				range.length = count;
				if ( (values = CFAllocatorAllocate(NULL, count * sizeof(CFDictionaryRef), 0))){
					CFArrayGetValues(certProparray, range, (const void **)values);
					for( i = 0; i < count; i++)
					{  
						if ((propDict = values[i])){
							if ( CFDictionaryContainsValue(propDict, kSecPropertyTypeDate) ){
								if ( CFDictionaryGetValueIfPresent(propDict, kSecPropertyKeyValue, (const void**)&datevalue)){
									/* get kSecPropertyKeyLabel */
									if ( (datevalue) && (CFDictionaryGetValueIfPresent(propDict, kSecPropertyKeyLabel, (const void**)&labelvalue))){
										if ( (labelvalue) && (CFStringCompare( (CFStringRef)labelvalue, CFSTR("Not Valid Before"), 0) == kCFCompareEqualTo)){
											if ( notvalidbeforedate = CFDateGetAbsoluteTime(datevalue))
												notvalidbeforedatedata = CFDateCreate(NULL, notvalidbeforedate);
										}else if ((labelvalue) && (CFStringCompare( (CFStringRef)labelvalue, CFSTR("Not Valid After"), 0 ) == kCFCompareEqualTo)){
											if ( notvalidafterdate = CFDateGetAbsoluteTime(datevalue))
												notvalidafterdatedata = CFDateCreate(NULL, notvalidafterdate);
										}
									}
								}
							}
						}
						
					}	
				}
			}
		}
		
		if ( (timeNow = CFAbsoluteTimeGetCurrent()) && (nowcfdatedata = CFDateCreate( NULL, timeNow))){
			if ( notvalidbeforedatedata ){
				gregoriandate = CFAbsoluteTimeGetGregorianDate(notvalidbeforedate, NULL);
				plog(LLV_DEBUG, LOCATION, NULL, 
					 "cert not valid before yr %d, mon %d, days %d, hours %d, min %d\n", gregoriandate.year, gregoriandate.month, gregoriandate.day, gregoriandate.hour, gregoriandate.minute);
				gregoriandate = CFAbsoluteTimeGetGregorianDate(notvalidafterdate, NULL);
				plog(LLV_DEBUG, LOCATION, NULL, 
					 "cert not valid after yr %d, mon %d, days %d, hours %d, min %d\n", gregoriandate.year, gregoriandate.month, gregoriandate.day, gregoriandate.hour, gregoriandate.minute);
				if ( CFDateCompare( nowcfdatedata, notvalidbeforedatedata, NULL ) == kCFCompareLessThan){
					plog(LLV_ERROR, LOCATION, NULL, 
						 "current time before valid time\n");
					*certStatus = CERT_STATUS_PREMATURE;
				}
				else if (notvalidafterdatedata && (CFDateCompare( nowcfdatedata, notvalidafterdatedata, NULL ) == kCFCompareGreaterThan)){
					plog(LLV_ERROR, LOCATION, NULL, 
						 "current time after valid time\n");
					*certStatus = CERT_STATUS_EXPIRED;
				}else {
					plog(LLV_INFO, LOCATION, NULL, "certificate expiration date OK\n");
					*certStatus = CERT_STATUS_OK;
				}

			}

		}
	}

#endif
		
end:
	if (certificateRef)
		CFRelease(certificateRef);
	if (identityRef)
		CFRelease(identityRef);
#if !TARGET_OS_EMBEDDED
	if (idSearchRef)
		CFRelease(idSearchRef);
	if (keychainRef)
		CFRelease(keychainRef);
#else
	if (notvalidbeforedatedata)
		CFRelease(notvalidbeforedatedata);
	if (notvalidafterdatedata)
		CFRelease(notvalidafterdatedata);
	if (certProparray)
		CFRelease(certProparray);
	if (values)
		CFAllocatorDeallocate(NULL, values);
	if (nowcfdatedata)
		CFRelease(nowcfdatedata);
	if (persistFind)
		CFRelease(persistFind);
	if (certData)
		CFRelease(certData);
#endif
	
	if (status != noErr && status != -1) {
		plog(LLV_ERROR, LOCATION, NULL, 
			"error %d %s.\n", status, GetSecurityErrorString(status));
		status = -1;
	}			
	return cert;

}

#if !TARGET_OS_EMBEDDED
/*
 * Find a policy ref by OID
 */
static OSStatus FindPolicy(const CSSM_OID *policyOID, SecPolicyRef *policyRef)
{
	
	OSStatus			status;
	SecPolicySearchRef	searchRef = nil;
	
	status = SecPolicySearchCreate(CSSM_CERT_X_509v3, policyOID, NULL, &searchRef);
	if (status != noErr)
		goto end;
		
	status = SecPolicySearchCopyNext(searchRef, policyRef);
	
end:
	if (searchRef)
		CFRelease(searchRef);

	if (status != noErr) {
		plog(LLV_ERROR, LOCATION, NULL, 
			"error %d %s.\n", status, GetSecurityErrorString(status));
		status = -1;
	}			
	return status;
}
#endif		

/*
 * Evaluate the trust of a cert using the policy provided
 */
static OSStatus EvaluateCert(SecCertificateRef cert, CFTypeRef policyRef)
{
	OSStatus					status;
	SecTrustRef					trustRef = 0;
	SecTrustResultType 			evalResult;

#if TARGET_OS_EMBEDDED	
	CFArrayRef					errorStrings;
#else
	CSSM_TP_APPLE_EVIDENCE_INFO			*statusChain;
	CFArrayRef					certChain;
#endif
	
	SecCertificateRef			evalCertArray[1] = { cert };
	
	CFArrayRef	cfCertRef = CFArrayCreate((CFAllocatorRef) NULL, (void*)evalCertArray, 1,
								&kCFTypeArrayCallBacks);
										
	if (!cfCertRef) {
		plog(LLV_ERROR, LOCATION, NULL, 
			"unable to create CFArray.\n");		
		return -1;
	}
		
	status = SecTrustCreateWithCertificates(cfCertRef, policyRef, &trustRef);
	if (status != noErr)
		goto end;

	status = SecTrustEvaluate(trustRef, &evalResult);
	if (status != noErr)
		goto end;
	
	if (evalResult != kSecTrustResultProceed && evalResult != kSecTrustResultUnspecified) {
		plog(LLV_ERROR, LOCATION, NULL, "Error evaluating certificate.\n");

		switch (evalResult) {
			case kSecTrustResultInvalid:
				plog(LLV_DEBUG, LOCATION, NULL, "eval result = kSecTrustResultInvalid.\n");
				break;
			case kSecTrustResultProceed:
				plog(LLV_DEBUG, LOCATION, NULL, "eval result = kSecTrustResultProceed.\n");
				break;
			case kSecTrustResultConfirm:
				plog(LLV_DEBUG, LOCATION, NULL, "eval result = kSecTrustResultConfirm.\n");
				break;
			case kSecTrustResultDeny:
				plog(LLV_DEBUG, LOCATION, NULL, "eval result = kSecTrustResultDeny.\n");
				break;
			case kSecTrustResultUnspecified:
				plog(LLV_DEBUG, LOCATION, NULL, "eval result = kSecTrustResultUnspecified.\n");
				break;
			case kSecTrustResultRecoverableTrustFailure:
				plog(LLV_DEBUG, LOCATION, NULL, "eval result = kSecTrustResultRecoverableTrustFailure.\n");
				break;
			case kSecTrustResultFatalTrustFailure:
				plog(LLV_DEBUG, LOCATION, NULL, "eval result = kSecTrustResultFatalTrustFailure.\n");
				break;
			case kSecTrustResultOtherError:
				plog(LLV_DEBUG, LOCATION, NULL, "eval result = kSecTrustResultOtherError.\n");
				break;
			default:
				plog(LLV_DEBUG, LOCATION, NULL, "eval result unknown: value = %d.\n", (int)evalResult);
				break;
		}


#if TARGET_OS_EMBEDDED			
		errorStrings =  SecTrustCopyProperties(trustRef);
		if (errorStrings) {
			
			CFDictionaryRef dict;
			CFStringRef val;
			const char *str;	
			CFIndex count, maxcount = CFArrayGetCount(errorStrings);
		
			plog(LLV_ERROR, LOCATION, NULL, "---------------Returned error strings: ---------------.\n");
			for (count = 0; count < maxcount; count++) {
				dict = CFArrayGetValueAtIndex(errorStrings, count);
				if (dict && (CFGetTypeID(dict) == CFDictionaryGetTypeID())) {
					val = CFDictionaryGetValue(dict, kSecPropertyKeyType);
					if (val && (CFGetTypeID(val) == CFStringGetTypeID())) {
						str = CFStringGetCStringPtr(val, kCFStringEncodingMacRoman);	
						if (str)		
							plog(LLV_ERROR, LOCATION, NULL, "type = %s.\n", str);
					}
					val = CFDictionaryGetValue(dict, kSecPropertyKeyValue);
					if (val && (CFGetTypeID(val) == CFStringGetTypeID())) {
						str = CFStringGetCStringPtr(val, kCFStringEncodingMacRoman);	
						if (str)		
							plog(LLV_ERROR, LOCATION, NULL, "value = %s.\n", str);
					}
				}
			}
			plog(LLV_ERROR, LOCATION, NULL, "-----------------------------------------------------.\n");			
			CFRelease(errorStrings);
		}
		
#else
		SecTrustGetResult(trustRef, &evalResult, &certChain, &statusChain);
		plog(LLV_ERROR, LOCATION, NULL, "Cert status bits = 0x%x.\n", statusChain->StatusBits);
		plog(LLV_ERROR, LOCATION, NULL, "Cert status NumStatusCodes = 0x%x.\n", statusChain->NumStatusCodes);
		{
			int i;
			for (i = 0; i < statusChain->NumStatusCodes; i++)		
				plog(LLV_ERROR, LOCATION, NULL, "Cert status code i = 0x%x  %d.\n", *(statusChain->StatusCodes + i), *(statusChain->StatusCodes + i));
		}
		plog(LLV_ERROR, LOCATION, NULL, "Cert status Index = %d.\n", statusChain->Index);
		CFRelease(certChain);
#endif
		
		status = -1;
	}
			
	
end:
	if (cfCertRef)
		CFRelease(cfCertRef);
	if (trustRef)
		CFRelease(trustRef);

	if (status != noErr && status != -1) {
		plog(LLV_ERROR, LOCATION, NULL, 
			"error %d %s.\n", status, GetSecurityErrorString(status));
		status = -1;
	}			
	return status;
}

#if !TARGET_OS_EMBEDDED
/*
 * Copy the system keychain
 */
static OSStatus CopySystemKeychain(SecKeychainRef *keychainRef)
{

	OSStatus status;

	status = SecKeychainSetPreferenceDomain(kSecPreferencesDomainSystem);
	if (status != noErr)
		goto end;

	status = SecKeychainCopyDomainDefault(kSecPreferencesDomainSystem, keychainRef);
	
end:

	if (status != noErr) {
		plog(LLV_ERROR, LOCATION, NULL, 
			"error %d %s.\n", status, GetSecurityErrorString(status));
		status = -1;
	}			
	return status;

}
#endif

/* 
 * Return string representation of Security-related OSStatus.
 */
const char *
GetSecurityErrorString(OSStatus err)
{
    switch(err) {
		case noErr:
			return "noErr";
		case memFullErr:
			return "memFullErr";
		case paramErr:
			return "paramErr";
		case unimpErr:
			return "unimpErr";
	
		/* SecBase.h: */
		case errSecNotAvailable:
			return "errSecNotAvailable";
#if !TARGET_OS_EMBEDDED
		case errSecReadOnly:
			return "errSecReadOnly";
		case errSecAuthFailed:
			return "errSecAuthFailed";
		case errSecNoSuchKeychain:
			return "errSecNoSuchKeychain";
		case errSecInvalidKeychain:
			return "errSecInvalidKeychain";
		case errSecDuplicateKeychain:
			return "errSecDuplicateKeychain";
		case errSecDuplicateCallback:
			return "errSecDuplicateCallback";
		case errSecInvalidCallback:
			return "errSecInvalidCallback";
		case errSecBufferTooSmall:
			return "errSecBufferTooSmall";
		case errSecDataTooLarge:
			return "errSecDataTooLarge";
		case errSecNoSuchAttr:
			return "errSecNoSuchAttr";
		case errSecInvalidItemRef:
			return "errSecInvalidItemRef";
		case errSecInvalidSearchRef:
			return "errSecInvalidSearchRef";
		case errSecNoSuchClass:
			return "errSecNoSuchClass";
		case errSecNoDefaultKeychain:
			return "errSecNoDefaultKeychain";
		case errSecInteractionNotAllowed:
			return "errSecInteractionNotAllowed";
		case errSecReadOnlyAttr:
			return "errSecReadOnlyAttr";
		case errSecWrongSecVersion:
			return "errSecWrongSecVersion";
		case errSecKeySizeNotAllowed:
			return "errSecKeySizeNotAllowed";
		case errSecNoStorageModule:
			return "errSecNoStorageModule";
		case errSecNoCertificateModule:
			return "errSecNoCertificateModule";
		case errSecNoPolicyModule:
			return "errSecNoPolicyModule";
		case errSecInteractionRequired:
			return "errSecInteractionRequired";
		case errSecDataNotAvailable:
			return "errSecDataNotAvailable";
		case errSecDataNotModifiable:
			return "errSecDataNotModifiable";
		case errSecCreateChainFailed:
			return "errSecCreateChainFailed";
		case errSecACLNotSimple:
			return "errSecACLNotSimple";
		case errSecPolicyNotFound:
			return "errSecPolicyNotFound";
		case errSecInvalidTrustSetting:
			return "errSecInvalidTrustSetting";
		case errSecNoAccessForItem:
			return "errSecNoAccessForItem";
		case errSecInvalidOwnerEdit:
			return "errSecInvalidOwnerEdit";
#endif
		case errSecDuplicateItem:
			return "errSecDuplicateItem";
		case errSecItemNotFound:
			return "errSecItemNotFound";
		default:
			return "<unknown>";
    }
}

