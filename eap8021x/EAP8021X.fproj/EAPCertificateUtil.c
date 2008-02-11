
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
#include <Security/SecIdentity.h>
#include <Security/SecIdentitySearch.h>
#include <Security/SecKeychain.h>
#include <Security/SecKeychainItem.h>
#include <Security/SecKeychainItemPriv.h>
#include <Security/SecKeychainSearch.h>
#include <Security/SecCertificate.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecTrust.h>
#include <Security/SecPolicySearch.h>
#include <Security/oidsalg.h>
#include <Security/oidscert.h>
#include <Security/oidsattr.h>
#include <Security/cssmapi.h>
#include <Security/cssmapple.h>
#include <Security/certextensions.h>
#include <Security/x509defs.h>
#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFNumber.h>
#include <SystemConfiguration/SCValidation.h>
#include <string.h>

#include "EAPCertificateUtil.h"
#include "EAPSecurity.h"
#include "myCFUtil.h"

#define kEAPSecIdentityHandleType	CFSTR("IdentityHandleType")
#define kEAPSecIdentityHandleTypeCertificateData	CFSTR("CertificateData")
#define kEAPSecIdentityHandleData		CFSTR("IdentityHandleData")

static SecCertificateRef
_EAPCFDataCreateSecCertificate(CFDataRef data_cf);

static CFDataRef
_EAPSecCertificateCreateCFData(SecCertificateRef cert);

static OSStatus
_EAPSecIdentityCreateCertificateTrustChain(SecIdentityRef identity, 
					   CFArrayRef * ret_chain)
{
    SecPolicyRef		policy = NULL;
    SecPolicySearchRef		policy_search = NULL;
    OSStatus			status;
    CFArrayRef 			status_chain = NULL;
    SecTrustRef 		trust = NULL;
    SecTrustResultType 		trust_result;

    *ret_chain = NULL;

    status = SecPolicySearchCreate(CSSM_CERT_X_509v3,
				   &CSSMOID_APPLE_X509_BASIC,
				   NULL,
				   &policy_search);
    if (status != noErr) {
	fprintf(stderr, "SecPolicySearchCreate failed: %s (%d)\n",
		EAPSecurityErrorString(status), (int)status);
	goto done;
    }
    status = SecPolicySearchCopyNext(policy_search, &policy);
    if (status != noErr) {
	fprintf(stderr, "SecPolicySearchCopyNext rtn %s (%d)\n",
		EAPSecurityErrorString(status), (int)status);
	goto done;
    }

    {
	SecCertificateRef		cert = NULL;
	CFArrayRef 			certs;

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
    }
    status = SecTrustEvaluate(trust, &trust_result);
    if (status != noErr) {
	fprintf(stderr, "SecTrustEvaluate returned %s (%d)\n",
		EAPSecurityErrorString(status), (int)status);
    }
    {
	CSSM_TP_APPLE_EVIDENCE_INFO * ignored;

	status = SecTrustGetResult(trust, &trust_result, 
				   &status_chain, &ignored);
	if (status != noErr) {
	    fprintf(stderr, "SecTrustGetResult failed: %s (%d)\n",
		    EAPSecurityErrorString(status), (int)status);
	    my_CFRelease(&status_chain);	/* just in case */
	    goto done;
	}
	else {
	    *ret_chain = status_chain;
	}
    }

 done:
    my_CFRelease(&trust);
    my_CFRelease(&policy);
    my_CFRelease(&policy_search);
    return (status);
}

OSStatus
EAPSecIdentityListCreate(CFArrayRef * ret_array)
{
    CFMutableArrayRef		array;
    SecIdentityRef		identity = NULL;
    SecIdentitySearchRef 	search = NULL;
    OSStatus			status = noErr;

    *ret_array = NULL;
    status = SecIdentitySearchCreate(NULL, CSSM_KEYUSE_SIGN, &search);
    if (status != noErr) {
	fprintf(stderr, "SecIdentitySearchCreate failed, %s (%d)\n", 
		EAPSecurityErrorString(status), (int)status);
	return (status);
    }
    array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    while (TRUE) {
	status = SecIdentitySearchCopyNext(search, &identity);
	if (status != noErr) {
	    if (status != errSecItemNotFound) {
		fprintf(stderr, "SecIdentitySearchCopyNext failed, %s (%d)\n",
			EAPSecurityErrorString(status), (int)status);
	    }
	    break; /* out of while */
	}
	CFArrayAppendValue(array, identity);
	my_CFRelease(&identity);
    }
    my_CFRelease(&search);
    if (CFArrayGetCount(array) == 0) {
	my_CFRelease(&array);
    }
    else {
	status = noErr;
    }
    *ret_array = array;
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
	
	status = paramErr;
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
	if (EAPSecCertificateEqual(cert_to_match, this_cert)) {
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
    return (paramErr);
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
    CFMutableArrayRef		array = NULL;
    int				count;
    SecIdentityRef		identity = NULL;
    OSStatus			status;
    CFArrayRef			trust_chain = NULL;

    *ret_array = NULL;
    status = EAPSecIdentityHandleCreateSecIdentity(cert_id, &identity);
    if (status != noErr) {
	goto done;
    }
    status = _EAPSecIdentityCreateCertificateTrustChain(identity,
							&trust_chain);
    if (status != noErr) {
	fprintf(stderr, 
		"_EAPSecIdentityCreateCertificateTrustChain failed: %s (%d)\n",
		EAPSecurityErrorString(status), (int)status);
	goto done;
    }
    count = CFArrayGetCount(trust_chain);
    array = CFArrayCreateMutable(NULL, count + 1, &kCFTypeArrayCallBacks);
    CFArrayAppendValue(array, identity); /* identity into [0] */
    CFArrayAppendArray(array, trust_chain, CFRangeMake(0, count));
    *ret_array = array;

 done:
    my_CFRelease(&trust_chain);
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

/*
 * Function: _EAPCFDataCreateSecCertificate
 * Purpose:
 *   Creates a SecCertificateRef from a CFDataRef.
 */
static SecCertificateRef
_EAPCFDataCreateSecCertificate(CFDataRef data_cf)
{
    SecCertificateRef	cert = NULL;
    CSSM_DATA		data;
    OSStatus 		status;

    if (data_cf == NULL) {
	goto done;
    }
    data.Length = CFDataGetLength(data_cf);
    data.Data = (uint8 *)CFDataGetBytePtr(data_cf);
    status = SecCertificateCreateFromData(&data, 
					  CSSM_CERT_X_509v3, 
					  CSSM_CERT_ENCODING_DER, &cert);
    if (status != noErr) {
	fprintf(stderr, "SecCertificateCreateFromData failed, %d", (int)status);
    }
 done:
    return (cert);
}

/*
 * Function: _EAPSecCertificateCreateCFData
 * Purpose:
 *   Creates a CFDataRef from a SecCertificateRef.
 */
static CFDataRef
_EAPSecCertificateCreateCFData(SecCertificateRef cert)
{
    CSSM_DATA		cert_data;
    CFDataRef		data = NULL;
    OSStatus 		status;

    status = SecCertificateGetData(cert, &cert_data);
    if (status != noErr) {
	goto done;
    }
    data = CFDataCreate(NULL, cert_data.Data, cert_data.Length);
 done:
    return (data);
}

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
	    goto failed;
	}
	data = _EAPSecCertificateCreateCFData(cert);
	if (data == NULL) {
	    goto failed;
	}
	CFArrayAppendValue(array, data);
	my_CFRelease(&data);
    }
    return (array);

 failed:
    my_CFRelease(&array);
    return (NULL);
}

/*
 * Function: mySecCertificateArrayCreateCFDataArray (deprecated)
 * Purpose:
 *   Binary/source compatibility to EAPSecCertificateArrayCreateCFDataArray.
 */
CFArrayRef
mySecCertificateArrayCreateCFDataArray(CFArrayRef certs)
{
    return (EAPSecCertificateArrayCreateCFDataArray(certs));

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

Boolean
EAPSecCertificateEqual(SecCertificateRef cert1, SecCertificateRef cert2)
{
    CSSM_DATA		cert1_data;
    CSSM_DATA		cert2_data;
    OSStatus		status;

    status = SecCertificateGetData(cert1, &cert1_data);
    if (status != noErr) {
	return (FALSE);
    }
    status = SecCertificateGetData(cert2, &cert2_data);
    if (status != noErr) {
	return (FALSE);
    }
    if (cert1_data.Length != cert2_data.Length) {
	return (FALSE);
    }
    return (!bcmp(cert1_data.Data, cert2_data.Data, cert1_data.Length));
}

Boolean
EAPSecCertificateListEqual(CFArrayRef list1, CFArrayRef list2)
{
    int		count1 = 0;
    int		count2 = 0;
    int		i;

    count1 = CFArrayGetCount(list1);
    count2 = CFArrayGetCount(list2);
    if (count1 != count2) {
	return (FALSE);
    }
    for (i = 0; i < count1; i++) {
	SecCertificateRef	cert1;
	SecCertificateRef	cert2;

	cert1 = (SecCertificateRef)CFArrayGetValueAtIndex(list1, i);
	cert2 = (SecCertificateRef)CFArrayGetValueAtIndex(list2, i);
	if (EAPSecCertificateEqual(cert1, cert2) == FALSE) {
	    return (FALSE);
	}
    }
    return (TRUE);
}

/**
 ** Certificate attributes:
 ** The following code is transcribed from SFCertificateData.m.  Once
 ** SecCertificateRef's give us what we need, we can remove this code.
 **/
#define MS_OID						OID_DOD, 0x01, 0x04, 0x01, 0x82, 0x37
#define MS_OID_LEN					2 + 5
#define MS_ENROLLMENT_OID			MS_OID, 0x14
#define MS_ENROLLMENT_LEN			MS_OID_LEN + 1
#define MS_PRINCIPAL_NAME			MS_ENROLLMENT_OID, 0x02, 0x03
#define MS_PRINCIPAL_NAME_LEN		MS_ENROLLMENT_LEN + 2
static const uint8 	OID_MS_NTPrincipalName[] = {MS_PRINCIPAL_NAME};
const CSSM_OID	CSSMOID_MS_NTPrincipalName	 = {MS_PRINCIPAL_NAME_LEN, (uint8 *)OID_MS_NTPrincipalName};

typedef struct {
    uint32	ID;				/* Identifier */
    uint32	tag;			/* Tag */
    uint32	length;			/* Data length */
    bool	indefinite;		/* Item has indefinite length */
    uint32	headerSize;		/* Size of tag+length */
    uint32	header[ 16 ];	/* Tag+length data */
} ASN1Item;
// ASN.1 stuff
#define TAG_MASK	0x1F	/* Bits 5 - 1 */
#define EOC			0x00	/* 0: End-of-contents octets */
#define LEN_XTND	0x80	/* Indefinite or long form */
#define LEN_MASK	0x7F	/* Bits 7 - 1 */

// ---------------------------------------------------------------------------
//	getASN1ItemInfo
// ---------------------------------------------------------------------------
// Given a pointer to an ASN.1-encoded object and a pointer to an ASN1Item
// struct, fill out that struct with tag and length information.
// inObjectLen prevents reading past the end of the object being parsed.
// Returns TRUE if successful, FALSE if not.

static bool 
getASN1ItemInfo (uint8 *inObject, int inObjectLen, ASN1Item *item)
{
    uint32 tag, length, index = 0;
    uint8 *p = inObject;
    uint8 *q = inObject+inObjectLen;
	
    if (!p || !item || inObjectLen < 2) return(false);

    memset( item, 0, sizeof( ASN1Item ) );
    item->indefinite = FALSE;
    tag = item->header[ index++ ] = *p++;
    item->ID = tag & ~TAG_MASK;
    tag &= TAG_MASK;
    if( tag == TAG_MASK ) /* long tag encoded as sequence of 7-bit values */
	{
	    uint32 value;
	    tag = 0;
	    do 
		{
		    value = *p++;
		    tag = ( tag << 7 ) | ( value & 0x7F );
		    item->header[ index++ ] = value;
		}
	    while ((value & LEN_XTND) && (p < q) && (*p != EOC));
	}
    item->tag = tag;
    if ((!(p < q)) || (*p == EOC)) return(false);
	
    length = item->header[ index++ ] = *p++;
    if (!(p < q)) return(false);
    item->headerSize = index;
    if ( length & LEN_XTND )
	{
	    uint32 i;

	    length &= LEN_MASK;
	    if( length > 4 )
		{
		    /* Object length field is greater than 4 bytes?! */
		    return(false);
		}
	    item->headerSize += length;
	    item->length = 0;
	    if ( !length )
		item->indefinite = true;
	    for ( i = 0; i < length; i++ )
		{
		    uint32 ch;
		    if (!(p < q)) return(false);
		    ch = *p++;

		    item->length = ( item->length << 8 ) | ch;
		    item->header[ i + index ] = ch;
		}
	}
    else	/* standard 1-byte length field */
	{
	    item->length = length;
	    if ((q-p) < length) return(false);
	}

    return(true);
}

static CSSM_BOOL compareCssmData(
				 const CSSM_DATA *d1,
				 const CSSM_DATA *d2)
{	
    if (d1->Length != d2->Length) {
	return CSSM_FALSE;
    }
    if(memcmp(d1->Data, d2->Data, d1->Length)) {
	return CSSM_FALSE;
    }
    return CSSM_TRUE;	
}

static CSSM_BOOL compareOids(
			     const CSSM_OID *oid1,
			     const CSSM_OID *oid2)
{
    if((oid1 == NULL) || (oid2 == NULL)) {
	return CSSM_FALSE;
    }	
    if(oid1->Length != oid2->Length) {
	return CSSM_FALSE;
    }
    if(memcmp(oid1->Data, oid2->Data, oid1->Length)) {
	return CSSM_FALSE;
    }
    else {
	return CSSM_TRUE;
    }
}

static bool
extension_common_valid(const CSSM_DATA * value, bool expect_parsed)
{
    CSSM_X509_EXTENSION *cssmExt = (CSSM_X509_EXTENSION *)value->Data;

    if (value->Length != sizeof(CSSM_X509_EXTENSION)) {
	return (false);
    }
    switch (cssmExt->format) {
    case CSSM_X509_DATAFORMAT_ENCODED:
	if (expect_parsed) {
	    return (false);
	}
	/* we don't use the value.tagAndValue field yet */
	if (cssmExt->BERvalue.Data == NULL
	    || 0 /* cssmExt->value.tagAndValue == NULL */) {
	    return (false);
	}
	break;
    case CSSM_X509_DATAFORMAT_PARSED:
	if (!expect_parsed) {
	    return (false);
	}
	if ((cssmExt->BERvalue.Data == NULL)
	    || (cssmExt->value.parsedValue == NULL)) {
	    return (false);
	}
	break;
    case CSSM_X509_DATAFORMAT_PAIR:
	/* we don't use the value.valuePair field yet */
	if (cssmExt->BERvalue.Data == NULL
	    || 0 /* cssmExt->value.valuePair == NULL */) {
	    return (false);
	}
	break;
    default:
	return (false);
	break;
    }
    return (true);
}

static CFStringRef
myCFStringCreateWithDerData(CFAllocatorRef alloc, 
			    CSSM_BER_TAG tag_type, const CSSM_DATA * value)
{
    CFStringEncoding	encoding = kCFStringEncodingInvalidId;
    CFStringRef		str = NULL;

    switch (tag_type) {
    case BER_TAG_PRINTABLE_STRING:
    case BER_TAG_IA5_STRING:
	encoding = kCFStringEncodingASCII;
	break;
    case BER_TAG_PKIX_UTF8_STRING:
    case BER_TAG_GENERAL_STRING:
    case BER_TAG_PKIX_UNIVERSAL_STRING:
	encoding = kCFStringEncodingUTF8;
	break;
    case BER_TAG_T61_STRING:
    case BER_TAG_VIDEOTEX_STRING:
    case BER_TAG_ISO646_STRING:
	encoding = kCFStringEncodingISOLatin1;
	break;
    case BER_TAG_PKIX_BMP_STRING:
	encoding = kCFStringEncodingUnicode;
    default:
	break;
    }
    if (encoding != kCFStringEncodingInvalidId) {
	str = CFStringCreateWithBytes(alloc, value->Data,
				      (CFIndex)value->Length, encoding, true);
    }
    return (str);
}

static CFStringRef
myCFStringCreateFromPrintableBERSequence(CFAllocatorRef alloc, 
					 const CSSM_DATA * value)
{
    char *	eos;
    ASN1Item 	item;
    CSSM_DATA 	item_data = *value;
    CFStringRef ret_str = NULL;

    /* determine end-of-sequence based on initial tag */
    if (!getASN1ItemInfo(item_data.Data, item_data.Length, &item)) {
	return (NULL);
    }
    eos = (char *)(item_data.Data + (UInt32)item.headerSize + item.length);
    while (getASN1ItemInfo(item_data.Data, item_data.Length, &item)) {
	if (((char *)item_data.Data + (UInt32)item.headerSize + item.length) 
	    > eos) {
	    break;
	}
	item_data.Data += item.headerSize;
	item_data.Length = item.length;
	switch (item.tag) {
	case BER_TAG_UNKNOWN:
	case BER_TAG_SEQUENCE:
	case BER_TAG_SET:
	    /* skip constructed object lengths */
	    break;
	case BER_TAG_PKIX_UTF8_STRING:
	case BER_TAG_NUMERIC_STRING:
	case BER_TAG_PRINTABLE_STRING:
	case BER_TAG_T61_STRING:
	case BER_TAG_VIDEOTEX_STRING:
	case BER_TAG_IA5_STRING:
	case BER_TAG_GRAPHIC_STRING:
	case BER_TAG_ISO646_STRING:
	case BER_TAG_GENERAL_STRING:
	case BER_TAG_PKIX_UNIVERSAL_STRING:
	    ret_str = myCFStringCreateWithDerData(alloc, item.tag, &item_data);
	    goto done;
	default:
	    item_data.Data += item_data.Length;
	    break;
	}
    }
 done:
    return (ret_str);
}

static void
parse_subject_struct(CFMutableDictionaryRef dict, const CSSM_DATA * d)
{
    CSSM_X509_RDN_PTR    	rdnp;
    int				r;
    CSSM_X509_NAME_PTR 		name = (CSSM_X509_NAME_PTR)d->Data;

    if ((name == NULL) || (d->Length != sizeof(CSSM_X509_NAME))) {
	return;
    }
	
    for (r = 0; r < name->numberOfRDNs; r++) {
	int				p;

	rdnp = &name->RelativeDistinguishedName[r];
	for (p = 0; p < rdnp->numberOfPairs; p++) {
	    CFStringRef			key = NULL;
	    CSSM_X509_TYPE_VALUE_PAIR *	ptvp;
	    CFStringRef			value;

	    ptvp = &rdnp->AttributeTypeAndValue[p];
	    if (compareOids(&ptvp->type, &CSSMOID_CommonName)) {
		key = kEAPSecCertificateAttributeCommonName;
	    }
	    else if (compareOids(&ptvp->type, &CSSMOID_EmailAddress)) {
		key = kEAPSecCertificateAttributeEmailAddress;
	    }
	    else {
		continue;
	    }
	    value = myCFStringCreateWithDerData(NULL, ptvp->valueType,
						&ptvp->value);
	    if (value != NULL) {
		CFStringRef	current;

		current = CFDictionaryGetValue(dict, key);
		if (current == NULL) {
		    CFDictionarySetValue(dict, key, value);
		}
		CFRelease(value);
	    }
	}
    }
    return;
}

static void
parse_subject_alt_name(CFMutableDictionaryRef dict, const CSSM_DATA * d)
{
    CSSM_X509_EXTENSION * 	cssmExt = (CSSM_X509_EXTENSION *)d->Data;
    int				i;
    CE_GeneralName *		name;
    CE_OtherName * 		other;
    CE_GeneralNames *		san;

    if (extension_common_valid(d, true) == false) {
	return;
    }
    san = (CE_GeneralNames *)cssmExt->value.parsedValue;
    for (i = 0; i < san->numNames; i++) {
	CFStringRef	key = NULL;
	CFStringRef	value = NULL;
	CSSM_OID 	tmp_oid = { 0, nil };
	CSSM_DATA 	tmp_value = { 0, nil };


	name = &san->generalName[i];
	switch (name->nameType) {
	case GNT_RFC822Name:
	    key = kEAPSecCertificateAttributeRFC822Name;
	    value = CFStringCreateWithBytes(NULL, name->name.Data, 
					    name->name.Length,
					    kCFStringEncodingASCII, 0);
	    break;
	case GNT_OtherName:
	    other = (CE_OtherName *)(name->name.Data);
	    /* work-around for 3722123 */
	    if (other != NULL) {
		tmp_oid = other->typeId;
		tmp_value = other->value;
	    }
	    if (tmp_oid.Data && tmp_value.Data == NULL) {
		uint32 	unparsed_len = tmp_oid.Length;
		uint8 	header_len = 2;
		if (unparsed_len > header_len)	{
		    uint8 oid_len = tmp_oid.Data[1];
		    if (unparsed_len > (oid_len + header_len)) {
			tmp_oid.Length = oid_len;
			tmp_oid.Data 
			    = (uint8*)(tmp_oid.Data + (uint32)header_len);
			tmp_value.Length 
			    = unparsed_len - (oid_len + header_len);
			tmp_value.Data 
			    = (uint8*)(tmp_oid.Data + (uint32)tmp_oid.Length);
		    }
		}
	    }
	    if (compareOids(&tmp_oid, &CSSMOID_MS_NTPrincipalName)) {
		key = kEAPSecCertificateAttributeNTPrincipalName;
		value = myCFStringCreateFromPrintableBERSequence(NULL,
								 &tmp_value);
		break;
	    }
	    break;
	default:
	    break;
	}
	if (key != NULL && value != NULL) {
	    CFStringRef	current;

	    current = CFDictionaryGetValue(dict, key);
	    if (current == NULL) {
		CFDictionarySetValue(dict, key, value);
	    }
	}
	if (value != NULL) {
	    CFRelease(value);
	}
    }
    return;
}

CFDictionaryRef
EAPSecCertificateCopyAttributesDictionary(const SecCertificateRef cert)
{
    CSSM_DATA		cert_data;
    CSSM_CL_HANDLE	clh;
    CSSM_RETURN 	crtn;
    CFMutableDictionaryRef dict = NULL;
    CSSM_DATA_PTR	issuer = NULL;
    CSSM_FIELD_PTR	fields;
    int			i;
    bool		is_root = false;
    uint32		n_fields;
    CSSM_DATA_PTR	subject = NULL;
    CSSM_DATA_PTR	subject_alt = NULL;
    CSSM_DATA_PTR	subject_struct = NULL;

    (void)SecCertificateGetCLHandle(cert, &clh);
    (void)SecCertificateGetData(cert, &cert_data);
    crtn = CSSM_CL_CertGetAllFields(clh, &cert_data, &n_fields, &fields);
    if (crtn) {
	fprintf(stderr, "CSSM_CL_CertGetAllFields failed %d\n",
		(int)crtn);
	goto done;
    }
    dict = CFDictionaryCreateMutable(NULL, 0,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);
    for (i = 0; i < n_fields; i++) {
	if (compareOids(&fields[i].FieldOid, 
			&CSSMOID_X509V1IssuerName)) {
	    issuer = &fields[i].FieldValue;
	}
	else if (compareOids(&fields[i].FieldOid, 
			     &CSSMOID_X509V1SubjectName)) {
	    subject = &fields[i].FieldValue;
	}
	else if (compareOids(&fields[i].FieldOid, 
			     &CSSMOID_SubjectAltName)) {
	    subject_alt = &fields[i].FieldValue;
	}
	else if (compareOids(&fields[i].FieldOid, 
			     &CSSMOID_X509V1SubjectNameCStruct)) {
	    subject_struct = &fields[i].FieldValue;
	}
    }

    is_root = (issuer != NULL) && (subject != NULL)
	&& (issuer->Data != NULL) && (subject->Data != NULL)
	&& (issuer->Data != subject->Data)
	&& compareCssmData(subject, issuer);
    if (is_root) {
	CSSM_RETURN 	crtn;

	/* verify that the cert is self-signed [3949265] */
	crtn = CSSM_CL_CertVerify(clh, CSSM_INVALID_HANDLE, 
				  &cert_data, &cert_data, NULL, 0);
	if (crtn != CSSM_OK) {
	    is_root = false;
	}
    }
    if (is_root) {
	CFDictionarySetValue(dict, kEAPSecCertificateAttributeIsRoot,
			     kCFBooleanTrue);
    }
    if (subject_alt != NULL) {
	parse_subject_alt_name(dict, subject_alt);
    }
    if (subject_struct != NULL) {
	parse_subject_struct(dict, subject_struct);
    }
    /* free memory */
    if (fields != NULL) {
	crtn = CSSM_CL_FreeFields(clh, n_fields, &fields);
    }
    if (CFDictionaryGetCount(dict) == 0) {
	CFRelease(dict);
	dict = NULL;
    }
 done:
    return (dict);
}

CFStringRef
EAPSecCertificateCopyUserNameString(SecCertificateRef cert)
{
    CFStringRef			attrs[] = {
	kEAPSecCertificateAttributeNTPrincipalName,
	kEAPSecCertificateAttributeRFC822Name,
	kEAPSecCertificateAttributeCommonName,
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
func()
{
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
    do {
	CFDictionaryRef		dict;

	status = SecKeychainSearchCopyNext(search, &item);
	if (status != noErr) {
	    goto failed;
	}
	attr_list.count = 0;
	attr_list.attr = NULL;
	status = SecKeychainItemCopyContent(item, 
					    NULL, /* item class */
					    &attr_list, 
					    &data.Length, (void * *)(&data.Data));
	if (status != noErr) {
	    fprintf(stderr, "SecKeychainItemCopyContent failed, %d", (int)status);
	    goto failed;
	}
	status = SecCertificateCreateFromData(&data, 
					      CSSM_CERT_X_509v3, 
					      CSSM_CERT_ENCODING_BER, &cert);
	SecKeychainItemFreeContent(&attr_list, data.Data);
	if (status != noErr) {
	    fprintf(stderr, "SecCertificateCreateFromData failed, %d", (int)status);
	    goto failed;
	}
	dict = EAPSecCertificateCopyAttributesDictionary(cert);
	if (dict != NULL) {
	    (void)CFShow(dict);
	    printf("\n");
	    CFRelease(dict);
	}
	if (item != NULL) {
	    CFRelease(item);
	}
	if (cert != NULL) {
	    CFRelease(cert);
	}
    } while (1);

 failed:
    if (search != NULL) {
	CFRelease(search);
    }
    return;

}

int
main(int argc, const char * argv[])
{
    func();
    if (argc > 1) {
	sleep(120);
    }
    exit(0);
    return (0);
}
#endif TEST_EAPSecCertificateCopyAttributesDictionary

#ifdef TEST_EAPSecIdentity
int
main(int argc, const char * argv[])
{
    int		count;
    int		i;
    CFArrayRef	list = NULL;
    OSStatus	status;

    status = EAPSecIdentityListCreate(&list);
    if (status != noErr) {
	fprintf(stderr, "EAPSecIdentityListCreate failed, %s (%d)\n",
		EAPSecurityErrorString(status), status);
	exit(1);
    }
    count = CFArrayGetCount(list);
    for (i = 0; i < count; i++) {
	EAPSecIdentityHandleRef	handle;
	SecIdentityRef		identity;
	CFDataRef		data;

	identity = (SecIdentityRef)CFArrayGetValueAtIndex(list, i);
	handle = EAPSecIdentityHandleCreate(identity);
	{
	    SecIdentityRef	new_id;

	    status = EAPSecIdentityHandleCreateSecIdentity(handle,
							   &new_id);
	    if (status != noErr) {
		fprintf(stderr, 
			"EAPSecIdentityHandleCreateSecIdentity failed %s (%d)\n",
			EAPSecurityErrorString(status), status);
	    }
	    else {
		my_CFRelease(&new_id);
	    }
	}
	data = CFPropertyListCreateXMLData(NULL, handle);
	if (data != NULL) {
	    write(1, CFDataGetBytePtr(data), CFDataGetLength(data));
	    CFRelease(data);
	}
	CFRelease(handle);
    }
    CFRelease(list);
    exit(0);
}
#endif TEST_EAPSecIdentity
