
/*
 * Copyright (c) 2001-2004 Apple Computer, Inc. All rights reserved.
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


/*
 * Racoon module for verifying and signing certificates through Security
 * Framework and CSSM
 */

#include <Security/SecBase.h>
#include <Security/SecCertificate.h>
#include <Security/SecPolicy.h>
#include <Security/SecIdentity.h>
#include <Security/SecIdentityPriv.h>
#include <Security/SecIdentitySearch.h>
#include <Security/SecKeychain.h>
#include <Security/SecKeychainItem.h>
#include <Security/SecKeychainItemPriv.h>
#include <Security/SecKey.h>
#include <Security/SecKeyPriv.h>
#include <Security/SecTrust.h>
#include <Security/oidsalg.h>
#include <Security/cssmapi.h>
#include <Security/SecPolicySearch.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>
#include "plog.h"
#include "debug.h"
#include "misc.h"

#include "crypto_cssm.h"



static OSStatus FindPolicy(const CSSM_OID *policyOID, SecPolicyRef *policyRef);
static OSStatus EvaluateCert(SecCertificateRef cert, CFTypeRef policyRef);
static OSStatus CopySystemKeychain(SecKeychainRef *keychainRef);
static const char *GetSecurityErrorString(OSStatus err);


/*
 * Verify cert using security framework
 */
int crypto_cssm_check_x509cert(vchar_t *cert)
{
	OSStatus			status;
	SecCertificateRef	certRef = 0;
	CSSM_DATA			certData;
	CSSM_OID			ourPolicyOID = CSSMOID_APPLE_TP_IP_SEC; 
	SecPolicyRef		policyRef = 0;

	// create cert ref
	certData.Length = cert->l;
	certData.Data = cert->v;
	status = SecCertificateCreateFromData(&certData, CSSM_CERT_X_509v3, CSSM_CERT_ENCODING_DER,
		&certRef);
	if (status != noErr)
		goto end;
	
	// get our policy object
	status = FindPolicy(&ourPolicyOID, &policyRef);
	if (status != noErr)
		goto end;
		
	// setup policy options ???
	// no options used at present - verification of subjectAltName fields, etc.
	// are done elsewhere in racoon in oakley_check_certid()
	
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
	SecCertificateRef				certificateRef = NULL;
	SecIdentityRef 					identityRef = NULL;
	SecIdentitySearchRef			idSearchRef = NULL;
	SecKeychainRef 					keychainRef = NULL;
	SecKeyRef						privateKeyRef = NULL;
	const CSSM_KEY					*cssmKey = NULL;
	CSSM_CSP_HANDLE 				cspHandle = nil;
	CSSM_CC_HANDLE					cssmContextHandle = nil;
	const CSSM_ACCESS_CREDENTIALS	*credentials = NULL;
	uint32							bytesEncrypted = 0;
	CSSM_DATA						clearData;
	CSSM_DATA						cipherData;
	CSSM_DATA						remData;
	CSSM_CONTEXT_ATTRIBUTE			newAttr;
	vchar_t							*sig = NULL;

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
	clearData.Data = hash->v;
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
	sig = vmalloc(cipherData.Length);
	if (sig == NULL)
		goto end;
		
	sig->v = cipherData.Data;
		
end:
	if (certificateRef)
		CFRelease(certificateRef);
	if (keychainRef)
		CFRelease(keychainRef);
	if (identityRef)
		CFRelease(identityRef);
	if (privateKeyRef)
		CFRelease(privateKeyRef);
	if (idSearchRef)
		CFRelease(idSearchRef);
	if (cssmContextHandle)
		CSSM_DeleteContext(cssmContextHandle);
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
vchar_t* crypto_cssm_get_x509cert(CFDataRef persistentCertRef)
{

	OSStatus				status;
	CSSM_DATA				cssmData;
	vchar_t					*cert = NULL;
	SecIdentityRef 			identityRef = NULL;
	SecIdentitySearchRef	idSearchRef = NULL;
	SecKeychainRef 			keychainRef = NULL;
	SecCertificateRef		certificateRef = NULL;


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
		
end:
	if (certificateRef)
		CFRelease(certificateRef);
	if (identityRef)
		CFRelease(identityRef);
	if (idSearchRef)
		CFRelease(idSearchRef);
	if (keychainRef)
		CFRelease(keychainRef);

	if (status != noErr && status != -1) {
		plog(LLV_ERROR, LOCATION, NULL, 
			"error %d %s.\n", status, GetSecurityErrorString(status));
		status = -1;
	}			
	return cert;

}


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
		

/*
 * Evaluate the trust of a cert using the policy provided
 */
static OSStatus EvaluateCert(SecCertificateRef cert, CFTypeRef policyRef)
{
	OSStatus					status;
	SecTrustRef					trustRef = 0;
	SecTrustResultType 			evalResult;
	
	SecCertificateRef			evalCertArray[1] = { cert };
	
	CFArrayRef			cfCertRef = CFArrayCreate((CFAllocatorRef) NULL, (void*)evalCertArray, 1,
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
		plog(LLV_ERROR, LOCATION, NULL, 
			"error evaluating certificate.\n");
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
		case errSecDuplicateItem:
			return "errSecDuplicateItem";
		case errSecItemNotFound:
			return "errSecItemNotFound";
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
		default:
			return "<unknown>";
    }
}

