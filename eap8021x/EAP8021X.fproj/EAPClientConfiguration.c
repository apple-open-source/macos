/*
 * Copyright (c) 2018-2019 Apple Inc. All rights reserved.
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

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>
#include <Security/SecIdentityPriv.h>
#include <Security/SecKeyPriv.h>
#include <Security/SecTrust.h>
#include <Security/SecPolicyPriv.h>
#include "EAPOLControlTypes.h"
#include "EAPClientProperties.h"
#include "EAPLog.h"
#include "myCFUtil.h"
#include "EAPTLSUtil.h"
#include "EAPCertificateUtil.h"
#include "EAPClientConfiguration.h"

STATIC Boolean
accept_types_shareable(CFMutableArrayRef accept)
{
    CFIndex			count;

    if (isA_CFArray(accept) == NULL) {
	return (FALSE);
    }
    count = CFArrayGetCount(accept);
    for (CFIndex i = 0; i < count; i++) {
	int eap_type;
	CFNumberRef type = CFArrayGetValueAtIndex(accept, i);

	if (isA_CFNumber(type) == NULL) {
	    return (FALSE);
	}
	if (CFNumberGetValue(type, kCFNumberIntType, &eap_type) == TRUE) {
	    switch (eap_type) {
		case kEAPTypeTLS:
		case kEAPTypePEAP:
		case kEAPTypeTTLS:
		case kEAPTypeEAPFAST:
		    break;
		default:
		    CFArrayRemoveValueAtIndex(accept, i);
		    --count;
		    --i;
		    break;
	    }
	}
    }
    if (count == 0) {
	return (FALSE);
    }
    return (TRUE);
}

STATIC Boolean
is_eap_configuration_shareable(CFDictionaryRef eapConfiguration)
{
    if (my_CFDictionaryGetBooleanValue(eapConfiguration,
				       kEAPClientPropOneTimeUserPassword,
				       FALSE) == TRUE) {
	EAPLOG(LOG_NOTICE, "EAP Configuration has \"OneTimePassword\" Enabled");
	return (FALSE);
    }
    return (TRUE);
}

STATIC OSStatus
copy_shareable_certificate_chain(SecIdentityRef identity,
					  CFArrayRef * ret_chain)
{
    SecCertificateRef		cert;
    CFArrayRef 			certs;
    SecPolicyRef		policy = NULL;
    OSStatus			status = errSecSuccess;
    SecTrustRef 		trust = NULL;
    SecTrustResultType 		trust_result;

    *ret_chain = NULL;
    policy = SecPolicyCreateEAP(FALSE, NULL);
    if (policy == NULL) {
	EAPLOG_FL(LOG_NOTICE, "SecPolicyCreateEAP failed");
	goto done;
    }
    status = SecIdentityCopyCertificate(identity, &cert);
    if (status != errSecSuccess) {
	EAPLOG_FL(LOG_NOTICE, "SecIdentityCopyCertificate failed: (%d)",
		  (int)status);
	goto done;
    }
    certs = CFArrayCreate(NULL, (const void **)&cert,
			  1, &kCFTypeArrayCallBacks);
    my_CFRelease(&cert);
    status = SecTrustCreateWithCertificates(certs, policy, &trust);
    my_CFRelease(&certs);
    if (status != errSecSuccess) {
	EAPLOG_FL(LOG_NOTICE, "SecTrustCreateWithCertificates failed: (%d)",
		  (int)status);
	goto done;
    }
    status = EAPTLSSecTrustEvaluate(trust, &trust_result);
    if (status != errSecSuccess) {
	EAPLOG_FL(LOG_NOTICE, "SecTrustEvaluate failed: (%d)",
		  (int)status);
    }
    /* move on with only leaf certificate if SecTrustEvaluate() fails to build a chain */
    {
	CFMutableArrayRef	array;
	CFIndex			count = SecTrustGetCertificateCount(trust);

	if (count == 0) {
	    /* this must not happen if SecIdentityCopyCertificate() returned valid certificate */
	    EAPLOG_FL(LOG_NOTICE, "SecTrustGetCertificateCount returned 0");
	    status = errSecParam;
	    goto done;
	}
	array = CFArrayCreateMutable(NULL, count, &kCFTypeArrayCallBacks);
	for (CFIndex i = 0; i < count; i++) {
	    SecCertificateRef	c = NULL;
	    CFDataRef 		d = NULL;

	    c = SecTrustGetCertificateAtIndex(trust, i);
	    d = SecCertificateCopyData(c);
	    CFArrayAppendValue(array, d);
	    my_CFRelease(&d);
	}
	*ret_chain = array;
	status = errSecSuccess;
    }

done:
    my_CFRelease(&trust);
    my_CFRelease(&policy);
    return (status);
}

#define kEAPShareablePropCertificateChain 	CFSTR("certificates") /* Array */
#define kEAPShareablePropPrivKey 		CFSTR("key") /* Data */
#define kEAPShareablePropPrivKeyAttribs 	CFSTR("attributes") /* Dictionary */

STATIC CFDictionaryRef
copy_shareable_identity_info(CFDataRef identityHandle)
{
    OSStatus			status;
    SecIdentityRef 		identity = NULL;
    SecKeyRef 			privateKey = NULL;
    CFMutableDictionaryRef 	retDict = NULL;
    CFDictionaryRef 		allAttribDict = NULL;
    CFMutableDictionaryRef 	attribDict = NULL;
    CFDataRef 			keyData = NULL;
    CFArrayRef			trust_chain = NULL;

    status = EAPSecIdentityHandleCreateSecIdentity((EAPSecIdentityHandleRef)identityHandle, &identity);
    if (status != errSecSuccess) {
	EAPLOG(LOG_ERR, "EAPSecIdentityHandleCreateSecIdentity() failed: (%d)", (int)status);
	return NULL;
    }
    if (identity == NULL) {
	EAPLOG(LOG_ERR, "Failed to find identity in the keychain: (%d)", (int)status);
	return NULL;
    }
    status = copy_shareable_certificate_chain(identity, &trust_chain);
    if (status != errSecSuccess || trust_chain == NULL) {
	EAPLOG(LOG_ERR, "Failed to get a certificate chain from identity: (%d)", (int)status);
	goto done;
    }

    status = SecIdentityCopyPrivateKey(identity, &privateKey);
    if (status != errSecSuccess || privateKey == NULL) {
	EAPLOG(LOG_ERR, "Failed to get a private key from identity: (%d)", (int)status);
	goto done;
    }

    keyData = SecKeyCopyExternalRepresentation(privateKey, NULL);
    if (keyData == NULL) {
	EAPLOG(LOG_ERR, "Failed to get an external representation of private key");
	goto done;
    }
    allAttribDict = SecKeyCopyAttributeDictionary(privateKey);
    if (allAttribDict == NULL) {
	EAPLOG(LOG_ERR, "Failed to get all the keychain item attributes for private key");
	goto done;
    }
    attribDict = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
					   &kCFTypeDictionaryValueCallBacks);
    {
	const void * attrib_class_val = CFDictionaryGetValue(allAttribDict, kSecAttrKeyClass);
	const void * attrib_type_val = CFDictionaryGetValue(allAttribDict, kSecAttrKeyType);
	if (attrib_class_val == NULL || attrib_type_val == NULL) {
	    /* this must not happen */
	    EAPLOG(LOG_ERR, "Failed to find class and/or type item attributes for private key");
	    goto done;
	}
	CFDictionaryAddValue(attribDict, kSecAttrKeyClass, attrib_class_val);
	CFDictionaryAddValue(attribDict, kSecAttrKeyType, attrib_type_val);
    }
    retDict = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
					&kCFTypeDictionaryValueCallBacks);
    CFDictionaryAddValue(retDict, kEAPShareablePropCertificateChain, trust_chain);
    CFDictionaryAddValue(retDict, kEAPShareablePropPrivKey, keyData);
    CFDictionaryAddValue(retDict, kEAPShareablePropPrivKeyAttribs, attribDict);

done:
    my_CFRelease(&identity);
    my_CFRelease(&allAttribDict);
    my_CFRelease(&attribDict);
    my_CFRelease(&trust_chain);
    my_CFRelease(&privateKey);
    my_CFRelease(&trust_chain);
    my_CFRelease(&keyData);
    return retDict;
}


CFDictionaryRef
EAPClientConfigurationCopyShareable(CFDictionaryRef eapConfiguration)
{
    CFMutableDictionaryRef 	retEAPConfiguration = NULL;
    CFMutableDictionaryRef 	retShareableConfiguration = NULL;
    CFMutableArrayRef 		newEAPTypes = NULL;
    CFArrayRef 			eapTypes = NULL;
    CFDataRef 			identityHandle = NULL;
    CFDictionaryRef 		shareableIdentityInfo = NULL;

    if (!eapConfiguration) {
	EAPLOG(LOG_NOTICE, "Invalid parameters");
	return NULL;
    }
    eapTypes = CFDictionaryGetValue(eapConfiguration, kEAPClientPropAcceptEAPTypes);
    if (eapTypes == NULL || (CFArrayGetCount(eapTypes) == 0)) {
	goto done;
    }
    newEAPTypes = CFArrayCreateMutableCopy(NULL, 0, eapTypes);
    if (accept_types_shareable(newEAPTypes) == FALSE) {
	EAPLOG(LOG_NOTICE, "EAP types are not shareable");
	goto done;
    }
    if (is_eap_configuration_shareable(eapConfiguration) == FALSE) {
	EAPLOG(LOG_NOTICE, "EAP configuration is not shareable");
	goto done;
    }
    identityHandle = CFDictionaryGetValue(eapConfiguration, kEAPClientPropTLSIdentityHandle);
    if (identityHandle) {
	shareableIdentityInfo = copy_shareable_identity_info(identityHandle);
	if (shareableIdentityInfo == NULL) {
	    EAPLOG(LOG_ERR, "Failed to get identity from identity handle");
	    goto done;
	}
    }
    retEAPConfiguration = CFDictionaryCreateMutableCopy(NULL, 0, eapConfiguration);
    CFDictionaryRemoveValue(retEAPConfiguration, kEAPClientPropTLSIdentityHandle);
    CFDictionarySetValue(retEAPConfiguration, kEAPClientPropAcceptEAPTypes, newEAPTypes);
    retShareableConfiguration = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
							  &kCFTypeDictionaryValueCallBacks);
    CFDictionaryAddValue(retShareableConfiguration, kEAPOLControlEAPClientConfiguration, retEAPConfiguration);
    if (shareableIdentityInfo) {
    	CFDictionaryAddValue(retShareableConfiguration, kEAPClientPropTLSShareableIdentityInfo, shareableIdentityInfo);
    }

done:
    my_CFRelease(&newEAPTypes);
    my_CFRelease(&retEAPConfiguration);
    my_CFRelease(&shareableIdentityInfo);
    return retShareableConfiguration;
}

STATIC CFDataRef
import_shareable_identity(CFDictionaryRef identityDict)
{
    CFDataRef 		keyData = NULL;
    CFArrayRef 		certDataArray = NULL;
    SecCertificateRef 	leaf = NULL;
    CFDictionaryRef 	attribs = NULL;
    SecKeyRef 		key = NULL;
    SecIdentityRef 	identity = NULL;
    CFDataRef 		retPersist = NULL;
    CFIndex 		count = 0;

    certDataArray = isA_CFArray(CFDictionaryGetValue(identityDict, kEAPShareablePropCertificateChain));
    if (certDataArray == NULL) {
	EAPLOG(LOG_ERR, "Failed to get certitifate array");
	return NULL;
    }
    count = CFArrayGetCount(certDataArray);
    if (count < 1) {
	EAPLOG(LOG_ERR, "Failed to get valid certitifate array");
	return NULL;
    }
    keyData = isA_CFData(CFDictionaryGetValue(identityDict, kEAPShareablePropPrivKey));
    if (keyData == NULL) {
	EAPLOG(LOG_ERR, "Failed to get key data");
	return NULL;
    }
    attribs = isA_CFDictionary(CFDictionaryGetValue(identityDict, kEAPShareablePropPrivKeyAttribs));
    if (attribs == NULL) {
	EAPLOG(LOG_ERR, "Failed to get attributes dictionary");
	return NULL;
    }
    /* leaf resides at index 0 */
    leaf = SecCertificateCreateWithData(kCFAllocatorDefault, (CFDataRef)CFArrayGetValueAtIndex(certDataArray, 0));
    if (leaf == NULL) {
	EAPLOG(LOG_ERR, "SecCertificateCreateWithData returned NULL");
	goto done;
    }

    key = SecKeyCreateWithData(keyData, attribs, NULL);
    if (key == NULL) {
	EAPLOG(LOG_ERR, "SecKeyCreateWithData returned NULL");
	goto done;
    }

    identity = SecIdentityCreate(kCFAllocatorDefault, leaf, key);
    if (identity == NULL) {
	EAPLOG(LOG_NOTICE, "SecIdentityCreate returned NULL");
	goto done;
    }
    {
#define kEAPAppleIdentitiesKeychainGroup 	CFSTR("com.apple.identities")
#define kEAPAppleCertificatesKeychainGroup 	CFSTR("com.apple.certificates")

	OSStatus 			status = errSecSuccess;
	CFMutableDictionaryRef 		query = NULL;

	/* first add identity in the keychain access group "com.apple.identities" */
	query = CFDictionaryCreateMutable(NULL, 0,
					  &kCFTypeDictionaryKeyCallBacks,
					  &kCFTypeDictionaryValueCallBacks);
	CFDictionaryAddValue(query, kSecAttrAccessGroup, kEAPAppleIdentitiesKeychainGroup);
	CFDictionaryAddValue(query, kSecUseSystemKeychain, kCFBooleanTrue);
	CFDictionaryAddValue(query, kSecValueRef, identity);
	CFDictionaryAddValue(query, kSecReturnPersistentRef, kCFBooleanTrue);
	CFDictionaryAddValue(query, kSecAttrAccessible, kSecAttrAccessibleAlwaysThisDeviceOnlyPrivate);
	status = SecItemAdd(query, (CFTypeRef *)&retPersist);
	switch(status) {
	    case errSecDuplicateItem:
		EAPLOG(LOG_DEBUG, "The identity already exists in keychain");
		status = SecItemCopyMatching(query, (CFTypeRef *)&retPersist);
		if (status != errSecSuccess) {
		    EAPLOG(LOG_ERR, "Failed to get persistent reference for identity in keychain (%d)", (int)status);
		    goto done;
		}
	    case errSecSuccess:
		break;
	    default:
		EAPLOG(LOG_ERR, "Failed to store identity in keychain (%d)", (int)status);
		goto done;
	}

	/* now add anchor certificates (if any) in the keychain access group "com.apple.certificates" */
	CFDictionarySetValue(query, kSecAttrAccessGroup, kEAPAppleCertificatesKeychainGroup);
	CFDictionaryRemoveValue(query, kSecReturnPersistentRef);
	for (CFIndex i = 1; i < count; i++) {
	    CFDataRef d = (CFDataRef)CFArrayGetValueAtIndex(certDataArray, i);
	    SecCertificateRef c =  SecCertificateCreateWithData(kCFAllocatorDefault, d);
	    if (c == NULL) {
		EAPLOG(LOG_ERR, "SecCertificateCreateWithData returned NULL anchor certificate");
		break;
	    }
	    CFDictionarySetValue(query, kSecValueRef, c);
	    status = SecItemAdd(query, NULL);
	    switch(status) {
		case errSecDuplicateItem:
		    EAPLOG(LOG_DEBUG, "The anchor certificate already exists in keychain");
		case errSecSuccess:
		    break;
		default:
		    EAPLOG(LOG_NOTICE, "Failed to store anchor certificate in keychain (%d)", (int)status);
		    break;
	    }
	    my_CFRelease(&c);
	}
	my_CFRelease(&query);
    }
done:
    my_CFRelease(&key);
    my_CFRelease(&leaf);
    my_CFRelease(&identity);
    return (CFDataRef)retPersist;
}

CFDictionaryRef
EAPClientConfigurationCopyAndImport(CFDictionaryRef shareableEapConfiguration)
{
    CFMutableDictionaryRef 	retEapConfiguration = NULL;
    CFDictionaryRef		eapConfiguration = NULL;
    CFDictionaryRef 		identityInfo = NULL;

    if (isA_CFDictionary(shareableEapConfiguration) == NULL) {
	EAPLOG(LOG_NOTICE, "Invalid parameters");
	return NULL;
    }
    eapConfiguration = isA_CFDictionary(CFDictionaryGetValue(shareableEapConfiguration,
							    kEAPOLControlEAPClientConfiguration));
    if (!eapConfiguration) {
	EAPLOG(LOG_NOTICE, "Missing EAP Configuration dictionary");
	return NULL;
    }
    identityInfo = isA_CFDictionary(CFDictionaryGetValue(shareableEapConfiguration,
							     kEAPClientPropTLSShareableIdentityInfo));
    if (identityInfo) {
	CFDataRef identityHandle = import_shareable_identity(identityInfo);
	if (identityHandle == NULL) {
	    /* this must not happen if we have a valid identiyInfo object */
	    EAPLOG(LOG_ERR, "Failed to create shareable identity handle from shareable identity info");
	    return NULL;
	}
	retEapConfiguration = CFDictionaryCreateMutableCopy(NULL, 0, eapConfiguration);
	CFDictionaryAddValue(retEapConfiguration, kEAPClientPropTLSIdentityHandle, identityHandle);
	my_CFRelease(&identityHandle);
	return retEapConfiguration;
    } else {
	return CFRetain(eapConfiguration);
    }
}
