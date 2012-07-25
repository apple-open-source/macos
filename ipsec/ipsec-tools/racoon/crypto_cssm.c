
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
#include <Security/SecItem.h>
#include <TargetConditionals.h>
#if TARGET_OS_EMBEDDED
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


static OSStatus EvaluateCert(SecCertificateRef evalCertArray[], CFIndex evalCertArrayNumValues, CFTypeRef policyRef, SecKeyRef *publicKeyRef);

#if !TARGET_OS_EMBEDDED
static OSStatus FindPolicy(const CSSM_OID *policyOID, SecPolicyRef *policyRef);
static OSStatus CopySystemKeychain(SecKeychainRef *keychainRef);
#endif

static SecPolicyRef
crypto_cssm_x509cert_get_SecPolicyRef (CFStringRef hostname)
{
	SecPolicyRef		policyRef = NULL;
#if !TARGET_OS_EMBEDDED
    OSStatus			status;
	CSSM_OID			ourPolicyOID = CSSMOID_APPLE_TP_IP_SEC; 

	// get our policy object
	status = FindPolicy(&ourPolicyOID, &policyRef);
	if (status != noErr && status != -1) {
		plog(LLV_ERROR, LOCATION, NULL, 
			 "error %d %s.\n", status, GetSecurityErrorString(status));
	}
#else
	if (hostname) {
		policyRef = SecPolicyCreateIPSec(FALSE, hostname);
		if (policyRef == NULL) {
			plog(LLV_ERROR, LOCATION, NULL, 
				 "unable to create a SSL policyRef.\n");
		}
	}	
#endif	
	return policyRef;
}

SecCertificateRef
crypto_cssm_x509cert_get_SecCertificateRef (vchar_t *cert)
{
	SecCertificateRef	certRef = NULL;

	CFDataRef cert_data = CFDataCreateWithBytesNoCopy(NULL, cert->v, cert->l, kCFAllocatorNull);
    if (cert_data) {
        certRef = SecCertificateCreateWithData(NULL, cert_data);
        CFRelease(cert_data);
    }

	if (certRef == NULL) {
		plog(LLV_ERROR, LOCATION, NULL, 
			 "unable to create a certRef.\n");
	}
	return certRef;
}

static cert_status_t
crypto_cssm_check_x509cert_dates (SecCertificateRef certificateRef)
{
	cert_status_t       certStatus = CERT_STATUS_OK;
#if TARGET_OS_EMBEDDED
	CFAbsoluteTime		timeNow = 0;
	CFAbsoluteTime		notvalidbeforedate = 0;
	CFAbsoluteTime		notvalidafterdate = 0;
	CFDateRef			nowcfdatedata = NULL;
	CFDateRef			notvalidbeforedatedata = NULL;
	CFDateRef			notvalidafterdatedata = NULL;
	CFArrayRef			certProparray = NULL;
	CFDictionaryRef		propDict = NULL;
	const void			*datevalue = NULL;
	const void			*labelvalue = NULL;
	CFGregorianDate		gregoriandate;
	CFIndex				count;
	CFIndex				i;
	
	if ((certProparray = SecCertificateCopyProperties(certificateRef))){
		if ((count = CFArrayGetCount( certProparray ))){
			for( i = 0; i < count; i++) {  
				if ((propDict = CFArrayGetValueAtIndex(certProparray, i))) {
					if ( CFDictionaryGetValueIfPresent(propDict, kSecPropertyKeyValue, (const void**)&datevalue)){
						/* get kSecPropertyKeyLabel */
						if ( (datevalue) && (CFDictionaryGetValueIfPresent(propDict, kSecPropertyKeyLabel, (const void**)&labelvalue))){
							if ( (labelvalue) && (CFStringCompare( (CFStringRef)labelvalue, CFSTR("Not Valid Before"), 0) == kCFCompareEqualTo)){
								if ( notvalidbeforedate = CFDateGetAbsoluteTime(datevalue)) {
									if (notvalidbeforedatedata) {
										CFRelease(notvalidbeforedatedata);
									}
									notvalidbeforedatedata = CFDateCreate(NULL, notvalidbeforedate);
								}
							}else if ((labelvalue) && (CFStringCompare( (CFStringRef)labelvalue, CFSTR("Not Valid After"), 0 ) == kCFCompareEqualTo)){
								if ( notvalidafterdate = CFDateGetAbsoluteTime(datevalue)) {
									if (notvalidafterdatedata) {
										CFRelease(notvalidafterdatedata);
									}
									notvalidafterdatedata = CFDateCreate(NULL, notvalidafterdate);
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
				certStatus = CERT_STATUS_PREMATURE;
			} else if (notvalidafterdatedata && (CFDateCompare( nowcfdatedata, notvalidafterdatedata, NULL ) == kCFCompareGreaterThan)){
				plog(LLV_ERROR, LOCATION, NULL, 
					 "current time after valid time\n");
				certStatus = CERT_STATUS_EXPIRED;
			}else {
				plog(LLV_INFO, LOCATION, NULL, "certificate expiration date OK\n");
				certStatus = CERT_STATUS_OK;
			}
		}
	}

	if (notvalidbeforedatedata)
		CFRelease(notvalidbeforedatedata);
	if (notvalidafterdatedata)
		CFRelease(notvalidafterdatedata);
	if (certProparray)
		CFRelease(certProparray);
	if (nowcfdatedata)
		CFRelease(nowcfdatedata);
#endif
	return certStatus;
}

/*
 * Verify cert using security framework
 */
int crypto_cssm_check_x509cert (cert_t *hostcert, cert_t *certchain, CFStringRef hostname, SecKeyRef *publicKeyRef)
{
	cert_t             *p;
	cert_status_t       certStatus = 0;
	OSStatus			status;
	CFIndex             certArrayRefNumValues = 0;
	CFIndex             n = 0;
	int                 certArraySiz;
	SecCertificateRef  *certArrayRef = NULL;
	SecPolicyRef		policyRef = crypto_cssm_x509cert_get_SecPolicyRef(hostname);
	
	if (!hostcert || !certchain) {
		return -1;
	}
	
	// find the total number of certs
	for (p = certchain; p; p = p->chain, n++);
	if (n> 1) {
		plog(LLV_DEBUG2, LOCATION, NULL,
			 "%s: checking chain of %d certificates.\n", __FUNCTION__, n);
	}
	
	certArraySiz = n * sizeof(CFTypeRef);
	certArrayRef = CFAllocatorAllocate(NULL, certArraySiz, 0);
	if (!certArrayRef) {
		return -1;
	}
	bzero(certArrayRef, certArraySiz);
	if ((certArrayRef[certArrayRefNumValues] = crypto_cssm_x509cert_get_SecCertificateRef(&hostcert->cert))) {
		/* don't overwrite any pending status */
		if (!hostcert->status) {
			hostcert->status = crypto_cssm_check_x509cert_dates(certArrayRef[certArrayRefNumValues]);
			if (hostcert->status) {
				plog(LLV_ERROR, LOCATION, NULL,
					 "host certificate failed date verification: %d.\n", hostcert->status);
				certStatus = hostcert->status;
			}
		}
		certArrayRefNumValues++;
	}
	for (p = certchain; p && certArrayRefNumValues < n; p = p->chain) {
		if (p != hostcert) {
			if ((certArrayRef[certArrayRefNumValues] = crypto_cssm_x509cert_get_SecCertificateRef(&p->cert))) {
				/* don't overwrite any pending status */
				if (!p->status) {
					p->status = crypto_cssm_check_x509cert_dates(certArrayRef[certArrayRefNumValues]);
					if (p->status) {
						plog(LLV_ERROR, LOCATION, NULL,
							 "other certificate in chain failed date verification: %d.\n", p->status);
						if (!certStatus) {
							certStatus = p->status;
						}
					}
				}
				certArrayRefNumValues++;
			}
		}
	}
	
	// evaluate cert
	status = EvaluateCert(certArrayRef, certArrayRefNumValues, policyRef, publicKeyRef);
	
	while (certArrayRefNumValues) {
		CFRelease(certArrayRef[--certArrayRefNumValues]);
	}
	CFAllocatorDeallocate(NULL, certArrayRef);
	
	if (policyRef)
		CFRelease(policyRef);
	
	if (status != noErr && status != -1) {
		plog(LLV_ERROR, LOCATION, NULL, 
			 "error %d %s.\n", status, GetSecurityErrorString(status));
		status = -1;
	} else if (certStatus == CERT_STATUS_PREMATURE || certStatus == CERT_STATUS_EXPIRED) {
		status = -1;
	}
	return status;
	
}


int crypto_cssm_verify_x509sign(SecKeyRef publicKeyRef, vchar_t *hash, vchar_t *signature)
{
	return SecKeyRawVerify(publicKeyRef, kSecPaddingPKCS1, hash->v, hash->l, signature->v, signature->l);	
}

/*
 * Encrypt a hash via CSSM using the private key in the keychain
 * from an identity.
 */
vchar_t* crypto_cssm_getsign(CFDataRef persistentCertRef, vchar_t* hash)
{

	OSStatus						status = -1;
	SecIdentityRef 					identityRef = NULL;
	SecKeyRef						privateKeyRef = NULL;
	vchar_t							*sig = NULL;


	CFDictionaryRef		persistFind = NULL;
	const void			*keys_persist[] = { kSecReturnRef, kSecValuePersistentRef, kSecClass};
	const void			*values_persist[] = { kCFBooleanTrue, persistentCertRef, kSecClassIdentity};
    
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
					
		
end:
	if (identityRef)
		CFRelease(identityRef);
	if (privateKeyRef)
		CFRelease(privateKeyRef);
		
	if (persistFind)
		CFRelease(persistFind);
	
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

	OSStatus				status = -1;
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
	
	/* find identity by persistent ref */
	persistFind = CFDictionaryCreate(NULL, keys_persist, values_persist,
		(sizeof(keys_persist) / sizeof(*keys_persist)), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
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
		*certStatus = crypto_cssm_check_x509cert_dates(certificateRef);
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
static OSStatus EvaluateCert(SecCertificateRef evalCertArray[], CFIndex evalCertArrayNumValues, CFTypeRef policyRef, SecKeyRef *publicKeyRef)
{
	OSStatus					status;
	SecTrustRef					trustRef = 0;
	SecTrustResultType 			evalResult;

	CFArrayRef					errorStrings;
	
	CFArrayRef	cfCertRef = CFArrayCreate((CFAllocatorRef) NULL, (void*)evalCertArray, evalCertArrayNumValues,
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
				
		status = -1;
		goto end;
	}
			
	/* get and return the public key */
	*publicKeyRef = SecTrustCopyPublicKey(trustRef);
	
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

