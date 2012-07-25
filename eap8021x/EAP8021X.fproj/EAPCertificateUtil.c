
/*
 * Copyright (c) 2001-2011 Apple Inc. All rights reserved.
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
 * EAPCertificateUtil.c
 * - certificate utility functions
 */


/* 
 * Modification History
 *
 * April 2, 2004	Dieter Siegmund (dieter@apple.com)
 * - created
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <TargetConditionals.h>
#include <Security/SecItem.h>
#if ! TARGET_OS_EMBEDDED
#include <Security/SecIdentitySearch.h>
#include <Security/SecKeychain.h>
#include <Security/SecKeychainItem.h>
#include <Security/SecKeychainItemPriv.h>
#include <Security/SecKeychainSearch.h>
#include <Security/SecPolicySearch.h>
#include <Security/oidsalg.h>
#include <Security/oidscert.h>
#include <Security/oidsattr.h>
#include <Security/cssmapi.h>
#include <Security/cssmapple.h>
#include <Security/certextensions.h>
#include <Security/cssmtype.h>
#include <Security/x509defs.h>
#include <Security/SecCertificateOIDs.h>
#endif /* TARGET_OS_EMBEDDED */
#include <Security/SecIdentity.h>
#include <Security/SecCertificate.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecTrust.h>
#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFNumber.h>
#include <SystemConfiguration/SCValidation.h>
#include <string.h>
#include "EAPTLSUtil.h"
#include "EAPCertificateUtil.h"
#include "EAPSecurity.h"
#include "myCFUtil.h"

#define kEAPSecIdentityHandleType	CFSTR("IdentityHandleType")
#define kEAPSecIdentityHandleTypeCertificateData	CFSTR("CertificateData")
#define kEAPSecIdentityHandleData		CFSTR("IdentityHandleData")

static __inline__ SecCertificateRef
_EAPCFDataCreateSecCertificate(CFDataRef data_cf)
{
    return (SecCertificateCreateWithData(NULL, data_cf));
}

OSStatus
EAPSecIdentityListCreate(CFArrayRef * ret_array)
{
    const void *		keys[] = {
	kSecClass,
	kSecReturnRef,
	kSecMatchLimit
    };
    CFDictionaryRef		query;
    CFTypeRef			results = NULL;
    OSStatus			status = noErr;
    const void *		values[] = {
	kSecClassIdentity,
	kCFBooleanTrue,
	kSecMatchLimitAll
    };

    query = CFDictionaryCreate(NULL, keys, values,
			       sizeof(keys) / sizeof(*keys),
			       &kCFTypeDictionaryKeyCallBacks,
			       &kCFTypeDictionaryValueCallBacks);
    status = SecItemCopyMatching(query, &results);
    CFRelease(query);
    if (status == noErr) {
	*ret_array = results;
    }
    return (status);
}

/* 
 * Function: IdentityCreateFromDictionary
 *
 * Purpose:
 *   This function locates a SecIdentityRef matching the passed in
 *   EAPSecIdentityHandle, in the form of a dictionary.  It also handles the
 *   NULL case i.e. find the first identity.
 *
 *   Old EAPSecIdentityHandle's used a dictionary with two key/value
 *   pairs, one for the type, the second for the data corresponding to the
 *   entire certificate.  
 *
 *   This function grabs all of the identities, then finds a match, either
 *   the first identity (dict == NULL), or one that matches the given
 *   certificate.
 * Returns:
 *   noErr and a non-NULL SecIdentityRef in *ret_identity if an identity
 *   was found, non-noErr otherwise.
 */
static OSStatus
IdentityCreateFromDictionary(CFDictionaryRef dict,
			     SecIdentityRef * ret_identity)
{
    SecCertificateRef		cert_to_match = NULL;
    int				count;
    int				i;
    CFArrayRef			identity_list;
    OSStatus			status;

    *ret_identity = NULL;
    if (dict != NULL) {
	CFStringRef	certid_type;
	CFDataRef	certid_data;
	
	status = EINVAL;
	certid_type = CFDictionaryGetValue(dict, kEAPSecIdentityHandleType);
	if (isA_CFString(certid_type) == NULL) {
	    goto done;
	}
	if (!CFEqual(certid_type, kEAPSecIdentityHandleTypeCertificateData)) {
	    goto done;
	}
	certid_data = CFDictionaryGetValue(dict, kEAPSecIdentityHandleData);
	if (isA_CFData(certid_data) == NULL) {
	    goto done;
	}
	cert_to_match = _EAPCFDataCreateSecCertificate(certid_data);
	if (cert_to_match == NULL) {
	    goto done;
	}
    }
    status = EAPSecIdentityListCreate(&identity_list);
    if (status != noErr) {
	goto done;
    }
    count = CFArrayGetCount(identity_list);
    for (i = 0; *ret_identity == NULL && i < count; i++) {
	SecIdentityRef		identity;
	SecCertificateRef	this_cert;
	
	identity = (SecIdentityRef)CFArrayGetValueAtIndex(identity_list, i);
	if (cert_to_match == NULL) {
	    /* just return the first one */
	    CFRetain(identity);
	    *ret_identity = identity;
	    break;
	}
	status = SecIdentityCopyCertificate(identity, &this_cert);
	if (this_cert == NULL) {
	    fprintf(stderr, 
		    "IdentityCreateFromDictionary:"
		    "SecIdentityCopyCertificate failed, %s (%d)\n",
		    EAPSecurityErrorString(status), (int)status);
	    break;
	}
	if (CFEqual(cert_to_match, this_cert)) {
	    /* found a match */
	    CFRetain(identity);
	    *ret_identity = identity;
	}
	CFRelease(this_cert);
    }
    CFRelease(identity_list);

 done:
    my_CFRelease(&cert_to_match);
    return (status);
}

#if TARGET_OS_EMBEDDED

static OSStatus
IdentityCreateFromData(CFDataRef data, SecIdentityRef * ret_identity)
{
    const void *		keys[] = {
	kSecClass,
	kSecReturnRef,
	kSecValuePersistentRef
    };
    CFDictionaryRef		query;
    CFTypeRef			results = NULL;
    OSStatus			status = noErr;
    const void *		values[] = {
	kSecClassIdentity,
	kCFBooleanTrue,
	data
    };

    *ret_identity = NULL;
    query = CFDictionaryCreate(NULL, keys, values,
			       sizeof(keys) / sizeof(*keys),
			       &kCFTypeDictionaryKeyCallBacks,
			       &kCFTypeDictionaryValueCallBacks);
    status = SecItemCopyMatching(query, &results);
    CFRelease(query);
    if (status == noErr) {
	*ret_identity = (SecIdentityRef)results;
    }
    return (status);
}

#else /* TARGET_OS_EMBEDDED */

static OSStatus
IdentityCreateFromData(CFDataRef data, SecIdentityRef * ret_identity)
{
    SecKeychainItemRef		cert;
    OSStatus			status;

    status = SecKeychainItemCopyFromPersistentReference(data, &cert);
    if (status != noErr) {
	return (status);
    }
    status = SecIdentityCreateWithCertificate(NULL,
					      (SecCertificateRef) cert,
					      ret_identity);
    CFRelease(cert);
    return (status);
}
    
#endif /* TARGET_OS_EMBEDDED */

/*
 * Function: EAPSecIdentityHandleCreateSecIdentity
 * Purpose:
 *   Creates a SecIdentityRef for the given EAPSecIdentityHandle.
 *
 *   The handle 'cert_id' is NULL, a non-NULL dictionary, or a non-NULL data.
 *   Any other input is invalid.
 *
 * Returns:
 *   noErr and !NULL *ret_identity on success, non-noErr otherwise.
 */    
OSStatus
EAPSecIdentityHandleCreateSecIdentity(EAPSecIdentityHandleRef cert_id, 
				      SecIdentityRef * ret_identity)
{
    *ret_identity = NULL;
    if (cert_id == NULL
	|| isA_CFDictionary(cert_id) != NULL) {
	return (IdentityCreateFromDictionary(cert_id, ret_identity));
    }
    if (isA_CFData(cert_id) != NULL) {
	return (IdentityCreateFromData((CFDataRef)cert_id, ret_identity));
    }
    return (EINVAL);
}

static OSStatus
EAPSecIdentityCreateCertificateTrustChain(SecIdentityRef identity, 
					  CFArrayRef * ret_chain)
{
    SecCertificateRef		cert;
    CFArrayRef 			certs;
    SecPolicyRef		policy = NULL;
    OSStatus			status;
    SecTrustRef 		trust = NULL;
    SecTrustResultType 		trust_result;

    *ret_chain = NULL;
    status = EAPSecPolicyCopy(&policy);
    if (status != noErr) {
	fprintf(stderr, "EAPSecPolicyCopy failed: %s (%d)\n",
		EAPSecurityErrorString(status), (int)status);
	goto done;
    }
    status = SecIdentityCopyCertificate(identity, &cert);
    if (status != noErr) {
	fprintf(stderr, "SecIdentityCopyCertificate failed: %s (%d)\n",
		EAPSecurityErrorString(status), (int)status);
	goto done;
    }
    certs = CFArrayCreate(NULL, (const void **)&cert, 
			  1, &kCFTypeArrayCallBacks);
    my_CFRelease(&cert);
    status = SecTrustCreateWithCertificates(certs, policy, &trust);
    my_CFRelease(&certs);
    if (status != noErr) {
	fprintf(stderr, "SecTrustCreateWithCertificates failed: %s (%d)\n",
		EAPSecurityErrorString(status), (int)status);
	goto done;
    }
    status = SecTrustEvaluate(trust, &trust_result);
    if (status != noErr) {
	fprintf(stderr, "SecTrustEvaluate returned %s (%d)\n",
		EAPSecurityErrorString(status), (int)status);
    }
    {
	CFMutableArrayRef	array;
	int			count = SecTrustGetCertificateCount(trust);
	int			i;

	if (count == 0) {
	    fprintf(stderr, "SecTrustGetCertificateCount returned 0)\n");
	    goto done;
	}
	array = CFArrayCreateMutable(NULL, count, &kCFTypeArrayCallBacks);
	for (i = 0; i < count; i++) {
	    SecCertificateRef	s;

	    s = SecTrustGetCertificateAtIndex(trust, i);
	    CFArrayAppendValue(array, s);
	}
	*ret_chain = array;
    }

 done:
    my_CFRelease(&trust);
    my_CFRelease(&policy);
    return (status);
}

/*
 * Function: EAPSecIdentityCreateTrustChain
 *
 * Purpose:
 *   Turns an SecIdentityRef into the array required by
 *   SSLSetCertificates().  See the <Security/SecureTransport.h> for more
 *   information.
 *
 * Returns:
 *   noErr and *ret_array != NULL on success, non-noErr otherwise.
 */
OSStatus
EAPSecIdentityCreateTrustChain(SecIdentityRef identity, CFArrayRef * ret_array)
{
    CFMutableArrayRef		array = NULL;
    int				count;
    OSStatus			status;
    CFArrayRef			trust_chain = NULL;

    *ret_array = NULL;
    status = EAPSecIdentityCreateCertificateTrustChain(identity,
						       &trust_chain);
    if (status != noErr) {
	fprintf(stderr, 
		"EAPSecIdentityCreateCertificateTrustChain failed: %s (%d)\n",
		EAPSecurityErrorString(status), (int)status);
	goto done;
    }

    count = CFArrayGetCount(trust_chain);
    array = CFArrayCreateMutableCopy(NULL, count, trust_chain);
    /* array[0] contains the identity's cert, replace it with the identity */
    CFArraySetValueAtIndex(array, 0, identity);
    *ret_array = array;

 done:
    my_CFRelease(&trust_chain);
    return (status);
}

/*
 * Function: EAPSecIdentityHandleCreateSecIdentityTrustChain
 *
 * Purpose:
 *   Turns an EAPSecIdentityHandle into the array required by
 *   SSLSetCertificates().  See the <Security/SecureTransport.h> for more
 *   information.
 *
 * Returns:
 *   noErr and *ret_array != NULL on success, non-noErr otherwise.
 */
OSStatus
EAPSecIdentityHandleCreateSecIdentityTrustChain(EAPSecIdentityHandleRef cert_id,
						CFArrayRef * ret_array)
{
    SecIdentityRef		identity = NULL;
    OSStatus			status;

    *ret_array = NULL;
    status = EAPSecIdentityHandleCreateSecIdentity(cert_id, &identity);
    if (status != noErr) {
	goto done;
    }
    status = EAPSecIdentityCreateTrustChain(identity, ret_array);

 done:
    my_CFRelease(&identity);
    return (status);
}

/*
 * Function: EAPSecIdentityHandleCreate
 * Purpose:
 *   Return the persistent reference for a given SecIdentityRef.
 * Returns:
 *   !NULL SecIdentityRef on success, NULL otherwise.
 */

#if TARGET_OS_EMBEDDED
EAPSecIdentityHandleRef
EAPSecIdentityHandleCreate(SecIdentityRef identity)
{
    const void *		keys[] = {
	kSecReturnPersistentRef,
	kSecValueRef
    };
    CFDictionaryRef		query;
    CFTypeRef			results = NULL;
    OSStatus			status = noErr;
    const void *		values[] = {
	kCFBooleanTrue,
	identity
    };

    query = CFDictionaryCreate(NULL, keys, values,
			       sizeof(keys) / sizeof(*keys),
			       &kCFTypeDictionaryKeyCallBacks,
			       &kCFTypeDictionaryValueCallBacks);
    status = SecItemCopyMatching(query, &results);
    if (status != noErr) {
	results = NULL;
	fprintf(stderr, "EAPSecIdentityHandleCreate() failed, %d\n",
		(int)status);
    }
    CFRelease(query);
    return (results);
}
#else /* TARGET_OS_EMBEDDED */
EAPSecIdentityHandleRef
EAPSecIdentityHandleCreate(SecIdentityRef identity)
{
    SecCertificateRef		cert;
    CFDataRef			data;
    OSStatus			status;

    status = SecIdentityCopyCertificate(identity, &cert);
    if (status != noErr) {
	fprintf(stderr, 
		"SecIdentityCopyCertificate failed, %s (%d)\n",
		EAPSecurityErrorString(status), (int)status);
	return (NULL);
    }
    status = SecKeychainItemCreatePersistentReference((SecKeychainItemRef)cert,
						      &data);
    CFRelease(cert);
    if (status != noErr) {
	fprintf(stderr, 
		"SecIdentityCopyCertificate failed, %s (%d)\n",
		EAPSecurityErrorString(status), (int)status);
	return (NULL);
    }
    return (data);
}
#endif /* TARGET_OS_EMBEDDED */

/*
 * Function: EAPSecCertificateArrayCreateCFDataArray
 * Purpose:
 *   Convert a CFArray[SecCertificate] to CFArray[CFData].
 */
CFArrayRef
EAPSecCertificateArrayCreateCFDataArray(CFArrayRef certs)
{
    CFMutableArrayRef	array = NULL;
    int			count = CFArrayGetCount(certs);
    int			i;

    array = CFArrayCreateMutable(NULL, count, &kCFTypeArrayCallBacks);
    for (i = 0; i < count; i++) {
	SecCertificateRef	cert;
	CFDataRef		data;

	cert = (SecCertificateRef)
	    isA_SecCertificate(CFArrayGetValueAtIndex(certs, i));
	if (cert == NULL) {
	    continue;
	}
	data = SecCertificateCopyData(cert);
	if (data == NULL) {
	    continue;
	}
	CFArrayAppendValue(array, data);
	my_CFRelease(&data);
    }
    return (array);
}

/*
 * Function: EAPCFDataArrayCreateSecCertificateArray
 * Purpose:
 *   Convert a CFArray[CFData] to CFArray[SecCertificate].
 */
CFArrayRef
EAPCFDataArrayCreateSecCertificateArray(CFArrayRef certs)
{
    CFMutableArrayRef	array = NULL;
    int			count = CFArrayGetCount(certs);
    int			i;
    
    array = CFArrayCreateMutable(NULL, count, &kCFTypeArrayCallBacks);
    for (i = 0; i < count; i++) {
	SecCertificateRef	cert;
	CFDataRef		data;

	data = isA_CFData((CFDataRef)CFArrayGetValueAtIndex(certs, i));
	if (data == NULL) {
	    goto failed;
	}
	cert = _EAPCFDataCreateSecCertificate(data);
	if (cert == NULL) {
	    goto failed;
	}
	CFArrayAppendValue(array, cert);
	my_CFRelease(&cert);
    }
    return (array);

 failed:
    my_CFRelease(&array);
    return (NULL);
}

CFTypeRef
isA_SecCertificate(CFTypeRef obj)
{
    return (isA_CFType(obj, SecCertificateGetTypeID()));
}


#if TARGET_OS_EMBEDDED
typedef CFArrayRef (*cert_names_func_t)(SecCertificateRef cert);
static void
dict_insert_cert_name_attr(CFMutableDictionaryRef dict, CFStringRef key,
			   cert_names_func_t func, SecCertificateRef cert)
{
    CFArrayRef	names;
    
    names = (*func)(cert);
    if (names != NULL) {
	if (CFEqual(key, kEAPSecCertificateAttributeCommonName)) {
	    CFIndex     count;
	    count = CFArrayGetCount(names);
	    CFDictionarySetValue(dict,
				 key,
				 CFArrayGetValueAtIndex(names, count - 1));
	} else {
	    CFDictionarySetValue(dict, 
			         key,
			         CFArrayGetValueAtIndex(names, 0));
	}
	CFRelease(names);
    }
    return;
}
typedef struct  {
    cert_names_func_t	func;
    CFStringRef	 	key;
} cert_names_func_info_t;

CFDictionaryRef
EAPSecCertificateCopyAttributesDictionary(const SecCertificateRef cert)
{
    cert_names_func_info_t	cert_names_info[] = {
	{ SecCertificateCopyRFC822Names,
	  kEAPSecCertificateAttributeRFC822Name },
	{ SecCertificateCopyNTPrincipalNames,
	  kEAPSecCertificateAttributeNTPrincipalName },
	{ SecCertificateCopyCommonNames,
	  kEAPSecCertificateAttributeCommonName },
	{ NULL, NULL }
    };
    CFMutableDictionaryRef 	dict = NULL;
    bool			is_root = false;
    cert_names_func_info_t * 	scan;

    dict = CFDictionaryCreateMutable(NULL, 0,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);
    for (scan = cert_names_info; scan->func != NULL; scan++) {
	dict_insert_cert_name_attr(dict, scan->key, scan->func, cert);
    }
    if (is_root) {
	CFDictionarySetValue(dict, kEAPSecCertificateAttributeIsRoot,
			     kCFBooleanTrue);
    }
    if (CFDictionaryGetCount(dict) == 0) {
	CFRelease(dict);
	dict = NULL;
    }
    return (dict);
}

CFStringRef
EAPSecCertificateCopySHA1DigestString(SecCertificateRef cert)
{
    const UInt8 *	bytes;
    CFIndex		count;
    CFIndex		i;
    CFDataRef 		hash;
    CFMutableStringRef	str;

    hash = SecCertificateGetSHA1Digest(cert);
    count = CFDataGetLength(hash);
    bytes = CFDataGetBytePtr(hash);
    str = CFStringCreateMutable(NULL, 0);
    for (i = 0; i < count; i++) {
	CFStringAppendFormat(str, NULL, CFSTR("%02x"), bytes[i]);
    }
    return (str);
}

#else /* TARGET_OS_EMBEDDED */

static void
dictSetValue(CFMutableDictionaryRef dict, CFStringRef key, CFTypeRef val,
	     Boolean use_last)
{
    if (isA_CFArray(val) != NULL) {
	int		count;

	count = CFArrayGetCount(val);
	if (count > 0) {
	    val = CFArrayGetValueAtIndex(val, use_last ? (count - 1) : 0);
	}
    }
    if (isA_CFString(val) != NULL) {
	CFDictionarySetValue(dict, key, val);
    }
}

CFDictionaryRef
EAPSecCertificateCopyAttributesDictionary(const SecCertificateRef cert)
{
    CFDictionaryRef		cert_values;
    CFArrayRef			cert_keys;
    CFMutableDictionaryRef 	dict = NULL;
    CFArrayRef			email_addresses;
    CFDictionaryRef		entry;
    int				i;
    CFTypeRef			value;
    const void *		values[] = {
	kSecOIDSubjectAltName,
	kSecOIDCommonName,
    };
    int				values_count = (sizeof(values)
						/ sizeof(values[0]));
    
    cert_keys = CFArrayCreate(NULL, values, values_count,
			      &kCFTypeArrayCallBacks);

    cert_values = SecCertificateCopyValues(cert, cert_keys, NULL);
    CFRelease(cert_keys);
    if (cert_values == NULL) {
	return (NULL);
    }
    dict = CFDictionaryCreateMutable(NULL, 0,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);

    /* get the common name */
    entry = CFDictionaryGetValue(cert_values, kSecOIDCommonName);
    if (entry != NULL) {
	value = CFDictionaryGetValue(entry, kSecPropertyKeyValue);
	if (value != NULL) {
	    dictSetValue(dict, kEAPSecCertificateAttributeCommonName,
			 value, TRUE);
	}
    }
    
    /* get the NTPrincipalName */
    entry = CFDictionaryGetValue(cert_values, kSecOIDSubjectAltName);
    value = NULL;
    if (entry != NULL) {
	value = CFDictionaryGetValue(entry, kSecPropertyKeyValue);
    }
    if (isA_CFArray(value) != NULL) {
	int		count = CFArrayGetCount(value);

	for (i = 0; i < count; i++) {
	    CFStringRef		label;
	    CFDictionaryRef	subj_alt = CFArrayGetValueAtIndex(value, i);
	    CFTypeRef		this_val;
	    
	    label = CFDictionaryGetValue(subj_alt, kSecPropertyKeyLabel);
	    this_val = CFDictionaryGetValue(subj_alt, kSecPropertyKeyValue);
	    if (label == NULL || this_val == NULL) {
		continue;
	    }
	    /* NT Principal Name */
	    if (CFEqual(label, kSecOIDMS_NTPrincipalName)) {
		dictSetValue(dict, 
			     kEAPSecCertificateAttributeNTPrincipalName,
			     this_val, FALSE);
	    }
	}
    }
    email_addresses = NULL;
    SecCertificateCopyEmailAddresses(cert, &email_addresses);
    if (email_addresses != NULL) {
	dictSetValue(dict, kEAPSecCertificateAttributeRFC822Name,
		     email_addresses, FALSE);
	CFRelease(email_addresses);
    }
    CFRelease(cert_values);
    return (dict);
}
#endif /* TARGET_OS_EMBEDDED */

CFStringRef
EAPSecCertificateCopyUserNameString(SecCertificateRef cert)
{
    CFStringRef			attrs[] = {
	kEAPSecCertificateAttributeNTPrincipalName,
	kEAPSecCertificateAttributeCommonName,
	kEAPSecCertificateAttributeRFC822Name,
	NULL
    };
    CFDictionaryRef		dict = NULL;
    int				i;
    CFStringRef			user_name = NULL;

    dict = EAPSecCertificateCopyAttributesDictionary(cert);
    if (dict == NULL) {
	goto done;
    }
    for (i = 0; attrs[i] != NULL; i++) {
	user_name = CFDictionaryGetValue(dict, attrs[i]);
	if (user_name != NULL) {
	    break;
	}
    }
 done:
    if (user_name != NULL) {
	CFRetain(user_name);
    }
    my_CFRelease(&dict);
    return (user_name);
}


#ifdef TEST_EAPSecCertificateCopyAttributesDictionary
static void
dump_as_xml(CFPropertyListRef p);

static void
dump_cert(SecCertificateRef cert);

#if TARGET_OS_EMBEDDED
static CFArrayRef
copyAllCerts(void)
{
    const void *	keys[] = {
	kSecClass,
	kSecReturnRef,
	kSecMatchLimit
    };
    CFDictionaryRef	query;
    CFTypeRef		results;
    OSStatus		status;
    const void *	values[] = {
	kSecClassCertificate,
	kCFBooleanTrue,
	kSecMatchLimitAll
    };

    query = CFDictionaryCreate(NULL, keys, values,
			       sizeof(keys) / sizeof(*keys),
			       &kCFTypeDictionaryKeyCallBacks,
			       &kCFTypeDictionaryValueCallBacks);
    status = SecItemCopyMatching(query, &results);
    CFRelease(query);
    if (status == noErr) {
	return (results);
    }
    return (NULL);
}


#else /* TARGET_OS_EMBEDDED */

static CFArrayRef
copyAllCerts(void)
{
    CFMutableArrayRef		array = NULL;
    SecKeychainAttributeList	attr_list;
    SecCertificateRef		cert = NULL;
    CSSM_DATA			data;
    SecKeychainItemRef		item = NULL;
    SecKeychainSearchRef	search = NULL;
    OSStatus 			status;

    status = SecKeychainSearchCreateFromAttributes(NULL, 
						   kSecCertificateItemClass,
						   NULL,
						   &search);
    if (status != noErr) {
	fprintf(stderr, "SecKeychainSearchCreateFromAttributes failed, %d",
		(int)status);
	goto failed;
    }
    array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    do {
	UInt32		this_len;

	status = SecKeychainSearchCopyNext(search, &item);
	if (status != noErr) {
	    break;
	}
	attr_list.count = 0;
	attr_list.attr = NULL;
	status = SecKeychainItemCopyContent(item, 
					    NULL, /* item class */
					    &attr_list, 
					    &this_len, (void * *)(&data.Data));
	if (status != noErr) {
	    fprintf(stderr, "SecKeychainItemCopyContent failed, %d", (int)status);
	    break;
	}
	data.Length = this_len;
	status = SecCertificateCreateFromData(&data, 
					      CSSM_CERT_X_509v3, 
					      CSSM_CERT_ENCODING_BER, &cert);
	SecKeychainItemFreeContent(&attr_list, data.Data);
	if (status != noErr) {
	    fprintf(stderr, "SecCertificateCreateFromData failed, %d", (int)status);
	    break;
	}
	CFArrayAppendValue(array, cert);
	if (item != NULL) {
	    CFRelease(item);
	}
	if (cert != NULL) {
	    CFRelease(cert);
	}
    } while (1);

 failed:
    my_CFRelease(&search);
    if (array != NULL && CFArrayGetCount(array) == 0) {
	CFRelease(array);
	array = NULL;
    }
    return (array);

}
#endif /* TARGET_OS_EMBEDDED */

static void
showAllCerts(void)
{
    CFArrayRef	certs;
    int		count;
    int		i;

    certs = copyAllCerts();
    if (certs == NULL) {
	return;
    }
    count = CFArrayGetCount(certs);
    for (i = 0; i < count; i++) {
	SecCertificateRef	cert;

	cert = (SecCertificateRef)CFArrayGetValueAtIndex(certs, i);
	dump_cert(cert);
    }
    CFRelease(certs);
    return;
}

int
main(int argc, const char * argv[])
{
    showAllCerts();
    if (argc > 1) {
	sleep(120);
    }
    exit(0);
    return (0);
}
#endif /* TEST_EAPSecCertificateCopyAttributesDictionary */

#if TEST_EAPSecIdentity || TEST_EAPSecCertificateCopyAttributesDictionary
#include <SystemConfiguration/SCPrivate.h>

static void
dump_as_xml(CFPropertyListRef p)
{
    CFDataRef	xml;
    
    xml = CFPropertyListCreateXMLData(NULL, p);
    if (xml != NULL) {
	fwrite(CFDataGetBytePtr(xml), CFDataGetLength(xml), 1,
	       stdout);
	CFRelease(xml);
    }
    return;
}

static void
dump_cert(SecCertificateRef cert)
{
    CFDictionaryRef		attrs = NULL;

    attrs = EAPSecCertificateCopyAttributesDictionary(cert);
    if (attrs != NULL) {
	printf("Attributes:\n");
	dump_as_xml(attrs);
	CFRelease(attrs);
    }
#if TARGET_OS_EMBEDDED
    else {
	CFStringRef summary;

	summary = SecCertificateCopySubjectSummary(cert);
	if (summary != NULL) {
	    printf("Summary:\n");
	    dump_as_xml(summary);
	    CFRelease(summary);
	}
    }
#endif /* TARGET_OS_EMBEDDED */
    {
	CFStringRef	username;

	username = EAPSecCertificateCopyUserNameString(cert);
	SCPrint(TRUE, stdout, CFSTR("Username = '%@'\n"), username);
	my_CFRelease(&username);
    }
}
#endif /* TEST_EAPSecIdentity || TEST_EAPSecCertificateCopyAttributesDictionary */

#ifdef TEST_EAPSecIdentity
static void
show_all_identities(void)
{
    int			count;
    int			i;
    CFArrayRef		list = NULL;
    OSStatus		status;

    status = EAPSecIdentityListCreate(&list);
    if (status != noErr) {
	fprintf(stderr, "EAPSecIdentityListCreate failed, %s (%d)\n",
		EAPSecurityErrorString(status), (int)status);
	exit(1);
    }
    count = CFArrayGetCount(list);
    for (i = 0; i < count; i++) {
	SecCertificateRef 	cert = NULL;
	EAPSecIdentityHandleRef	handle = NULL;
	SecIdentityRef		identity;
	SecIdentityRef		new_id = NULL;

	identity = (SecIdentityRef)CFArrayGetValueAtIndex(list, i);
	handle = EAPSecIdentityHandleCreate(identity);
	if (handle == NULL) {
	    fprintf(stderr, "EAPSecIdentityHandleCreate failed\n");
	    exit(1);
	}
	status = EAPSecIdentityHandleCreateSecIdentity(handle,
						       &new_id);
	if (status != noErr) {
	    fprintf(stderr, 
		    "EAPSecIdentityHandleCreateSecIdentity failed %s (%d)\n",
		    EAPSecurityErrorString(status), (int)status);
	    exit(1);
	}
	status = SecIdentityCopyCertificate(new_id, &cert);
	if (status != noErr) {
	    fprintf(stderr, "SecIdentityCopyCertificate failed %d\n",
		    (int)status);
	    exit(1);
	}
	printf("\nCertificate[%d]:\n", i);
	dump_cert(cert);
	printf("Handle:\n");
	dump_as_xml(handle);

	my_CFRelease(&cert);
	my_CFRelease(&new_id);
	my_CFRelease(&handle);
    }
    CFRelease(list);
    return;
}

static void
get_identity(const char * filename)
{
    SecCertificateRef	cert;
    CFTypeRef		handle;
    SecIdentityRef	identity;
    OSStatus		status;

    handle = my_CFPropertyListCreateFromFile(filename);
    if (handle == NULL) {
	fprintf(stderr, "could not read '%s'\n", filename);
	exit(1);
    }
    status = EAPSecIdentityHandleCreateSecIdentity(handle, &identity);
    if (status != noErr) {
	fprintf(stderr, "could not turn handle into identity, %d\n",
		(int)status);
	exit(1);
    }
    status = SecIdentityCopyCertificate(identity, &cert);
    if (status != noErr) {
	fprintf(stderr, "SecIdentityCopyCertificate failed %d\n",
		(int)status);
	exit(1);
    }
    dump_cert(cert);
    CFRelease(cert);
    CFRelease(handle);
    CFRelease(identity);
    return;
}

#if TARGET_OS_EMBEDDED

static OSStatus
remove_identity(SecIdentityRef identity)
{
    const void *		keys[] = {
	kSecValueRef
    };
    CFDictionaryRef		query;
    OSStatus			status = noErr;
    const void *		values[] = {
	identity
    };

    query = CFDictionaryCreate(NULL, keys, values,
			       sizeof(keys) / sizeof(*keys),
			       &kCFTypeDictionaryKeyCallBacks,
			       &kCFTypeDictionaryValueCallBacks);
    status = SecItemDelete(query);
    if (status != noErr) {
	fprintf(stderr, "SecItemDelete() failed, %d\n", (int)status);
    }
    CFRelease(query);
    return (status);
}

static void
remove_all_identities(void)
{
    int			count;
    int			i;
    CFArrayRef		list = NULL;
    OSStatus		status;

    status = EAPSecIdentityListCreate(&list);
    if (status != noErr) {
	fprintf(stderr, "EAPSecIdentityListCreate failed, %s (%d)\n",
		EAPSecurityErrorString(status), (int)status);
	exit(1);
    }
    count = CFArrayGetCount(list);
    for (i = 0; i < count; i++) {
	SecCertificateRef 	cert = NULL;
	SecIdentityRef		identity;

	identity = (SecIdentityRef)CFArrayGetValueAtIndex(list, i);
	status = SecIdentityCopyCertificate(identity, &cert);
	if (status != noErr) {
	    fprintf(stderr, "SecIdentityCopyCertificate failed %d\n",
		    (int)status);
	    exit(1);
	}
	printf("Removing:\n");
	dump_cert(cert);
	my_CFRelease(&cert);
	remove_identity(identity);
    }
    CFRelease(list);
    return;
}
#endif /* TARGET_OS_EMBEDDED */

static void
usage(const char * progname)
{
    fprintf(stderr, "%s: list\n", progname);
    fprintf(stderr, "%s: get <filename-containing-handle>\n", progname);
#if TARGET_OS_EMBEDDED
    fprintf(stderr, "%s: remove\n", progname);
#endif /* TARGET_OS_EMBEDDED */
    exit(1);
    return;
}

enum {
    kCommandList,
    kCommandGet,
    kCommandRemove
};

int
main(int argc, char * argv[])
{
    int		command = kCommandList;

    if (argc > 1) {
	if (strcmp(argv[1], "list") == 0) {
	    ;
	}
	else if (strcmp(argv[1], "get") == 0) {
	    if (argc < 3) {
		usage(argv[0]);
	    }
	    command = kCommandGet;
	}
#if TARGET_OS_EMBEDDED
	else if (strcmp(argv[1], "remove") == 0) {
	    command = kCommandRemove;
	}
#endif /* TARGET_OS_EMBEDDED */
	else {
	    usage(argv[0]);
	}
    }

    switch (command) {
    case kCommandList:
	show_all_identities();
	break;
    case kCommandGet:
	get_identity(argv[2]);
	break;
#if TARGET_OS_EMBEDDED
    case kCommandRemove:
	remove_all_identities();
	break;
#endif /* TARGET_OS_EMBEDDED */
    }
    exit(0);
}
#endif /* TEST_EAPSecIdentity */

#ifdef TEST_EAPSecIdentityHandleCreateSecIdentityTrustChain

int
main()
{
    int			count;
    int			i;
    CFArrayRef		list = NULL;
    CFArrayRef		trust_chain;
    OSStatus		status;

    status = EAPSecIdentityHandleCreateSecIdentityTrustChain(NULL,
							     &trust_chain);
    if (status != noErr) {
	fprintf(stderr, 
		"EAPSecIdentityHandleCreateSecIdentityTrustChain"
		" failed %s (%d)\n",
		EAPSecurityErrorString(status), (int)status);
	exit(2);
    }
    CFShow(trust_chain);
    CFRelease(trust_chain);
    status = EAPSecIdentityListCreate(&list);
    if (status != noErr) {
	fprintf(stderr, 
		"EAPSecIdentityListCreate"
		" failed %s (%d)\n",
		EAPSecurityErrorString(status), (int)status);
	exit(2);
    }
    count = CFArrayGetCount(list);
    for (i = 0; i < count; i++) {
	EAPSecIdentityHandleRef h;
	SecIdentityRef		ident = (SecIdentityRef)CFArrayGetValueAtIndex(list, i);

	h = EAPSecIdentityHandleCreate(ident);
	status = EAPSecIdentityHandleCreateSecIdentityTrustChain(h,
								 &trust_chain);
	if (status != noErr) {
	    fprintf(stderr, 
		    "EAPSecIdentityHandleCreateSecIdentityTrustChain"
		    " failed %s (%d)\n",
		    EAPSecurityErrorString(status), (int)status);
	    exit(2);
	}
	CFRelease(h);
	fprintf(stderr, "[%d]:\n", i);
	CFShow(trust_chain);
	CFRelease(trust_chain);
    }
    exit(0);
    return (0);
}


#endif /* TEST_EAPSecIdentityHandleCreateSecIdentityTrustChain */
