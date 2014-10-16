/*
 * Copyright (c) 2008-2014 Apple Inc. All Rights Reserved.
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

/*!
	@header SecTrustPriv
	The functions and data types in SecTrustPriv implement trust computation
	and allow the user to apply trust decisions to the trust configuration.
*/

#ifndef _SECURITY_SECTRUSTPRIV_H_
#define _SECURITY_SECTRUSTPRIV_H_

#include <Security/SecTrust.h>
#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFDictionary.h>

__BEGIN_DECLS

typedef enum {
	useNetworkDefault,		// default policy: network fetch enabled only for SSL
	useNetworkDisabled,		// explicitly disable network use for any policy
	useNetworkEnabled		// explicitly enable network use for any policy
} SecNetworkPolicy;

/* Constants used as keys in property lists.  See
   SecTrustCopySummaryPropertiesAtIndex for more information. */
extern CFTypeRef kSecPropertyKeyType;
extern CFTypeRef kSecPropertyKeyLabel;
extern CFTypeRef kSecPropertyKeyLocalizedLabel;
extern CFTypeRef kSecPropertyKeyValue;

extern CFTypeRef kSecPropertyTypeWarning;
extern CFTypeRef kSecPropertyTypeSuccess;
extern CFTypeRef kSecPropertyTypeSection;
extern CFTypeRef kSecPropertyTypeData;
extern CFTypeRef kSecPropertyTypeString;
extern CFTypeRef kSecPropertyTypeURL;
extern CFTypeRef kSecPropertyTypeDate;

/* Constants used as keys in the dictionary returned by SecTrustCopyInfo. */
extern CFTypeRef kSecTrustInfoExtendedValidationKey;
extern CFTypeRef kSecTrustInfoCompanyNameKey;
extern CFTypeRef kSecTrustInfoRevocationKey;
extern CFTypeRef kSecTrustInfoRevocationValidUntilKey;

/*!
	@function SecTrustCopySummaryPropertiesAtIndex
	@abstract Return a property array for the certificate.
	@param trust A reference to the trust object to evaluate.
	@param ix The index of the requested certificate.  Indices run from 0
    (leaf) to the anchor (or last certificate found if no anchor was found).
    @result A property array. It is the caller's responsibility to CFRelease
    the returned array when it is no longer needed. This function returns a
    short summary description of the certificate in question.  The property
    at index 0 of the array might also include general information about the
    entire chain's validity in the context of this trust evaluation.

	@discussion Returns a property array for this trust certificate. A property
    array is an array of CFDictionaryRefs.  Each dictionary (we call it a
    property for short) has the following keys:

	   kSecPropertyKeyType This key's value determines how this property
	       should be displayed. Its associated value is one of the
               following:
           kSecPropertyTypeWarning
		       The kSecPropertyKeyLocalizedLabel and kSecPropertyKeyLabel keys are not
			   set.  The kSecPropertyKeyValue is a CFStringRef which should
			   be displayed in yellow with a warning triangle.
           kSecPropertyTypeError
		       The kSecPropertyKeyLocalizedLabel and kSecPropertyKeyLabel keys are not
			   set.  The kSecPropertyKeyValue is a CFStringRef which should
			   be displayed in red with an error X.
           kSecPropertyTypeSuccess
		       The kSecPropertyKeyLocalizedLabel and kSecPropertyKeyLabel keys are not
			   set.  The kSecPropertyKeyValue is a CFStringRef which should
			   be displayed in green with a checkmark in front of it.
           kSecPropertyTypeTitle
		       The kSecPropertyKeyLocalizedLabel and kSecPropertyKeyLabel keys are not
			   set.  The kSecPropertyKeyValue is a CFStringRef which should
			   be displayed in a larger bold font.
           kSecPropertyTypeSection
		       The optional kSecPropertyKeyLocalizedLabel is a CFStringRef with the name
			   of the next section to display.  The value of the
			   kSecPropertyKeyValue key is a CFArrayRef which is a property
			   array as defined here.
           kSecPropertyTypeData
		       The optional kSecPropertyKeyLocalizedLabel is a CFStringRef containing
			   the localized label for the value for the kSecPropertyKeyValue.
			   The type of this value is a CFDataRef.  Its contents should be
			   displayed as: "bytes length_of_data : hexdump_of_data". Ideally
			   the UI will only show one line of hex dump data and have a
			   disclosure arrow to see the remainder.
           kSecPropertyTypeString
		       The optional kSecPropertyKeyLocalizedLabel is a CFStringRef containing
			   the localized label for the value for the kSecPropertyKeyValue.
			   The type of this value is a CFStringRef.  It's contents should be
			   displayed in the normal font.
           kSecPropertyTypeURL
		       The optional kSecPropertyKeyLocalizedLabel is a CFStringRef containing
			   the localized label for the value for the kSecPropertyKeyValue.
			   The type of this value is a CFURLRef.  It's contents should be
			   displayed as a hyperlink.
           kSecPropertyTypeDate
		       The optional kSecPropertyKeyLocalizedLabel is a CFStringRef containing
			   the localized label for the value for the kSecPropertyKeyValue.
			   The type of this value is a CFDateRef.  It's contents should be
			   displayed in human readable form (probably in the current
			   timezone).
	   kSecPropertyKeyLocalizedLabel
	       Human readable localized label for a given property.
	   kSecPropertyKeyValue
	       See description of kSecPropertyKeyType to determine what the value
		   for this key is.
	   kSecPropertyKeyLabel
           Non localized key (label) for this value.  This is only
		   present for properties with fixed label names.
	@param certificate A reference to the certificate to evaluate.
    @result A property array. It is the caller's responsability to CFRelease
    the returned array when it is no longer needed.
*/
CFArrayRef SecTrustCopySummaryPropertiesAtIndex(SecTrustRef trust, CFIndex ix);

/*!
	@function SecTrustCopyDetailedPropertiesAtIndex
	@abstract Return a property array for the certificate.
	@param trust A reference to the trust object to evaluate.
	@param ix The index of the requested certificate.  Indices run from 0
    (leaf) to the anchor (or last certificate found if no anchor was found).
    @result A property array. It is the caller's responsibility to CFRelease
    the returned array when it is no longer needed.
    See SecTrustCopySummaryPropertiesAtIndex on how to intepret this array.
	Unlike that function call this function returns a detailed description
    of the certificate in question.
*/
CFArrayRef SecTrustCopyDetailedPropertiesAtIndex(SecTrustRef trust, CFIndex ix);

/*!
	@function SecTrustCopyProperties
	@abstract Return a property array for this trust evaluation.
	@param trust A reference to the trust object to evaluate.
    @result A property array. It is the caller's responsibility to CFRelease
    the returned array when it is no longer needed. See
    SecTrustCopySummaryPropertiesAtIndex for a detailed description of this array.
    Unlike that function, this function returns a short text string suitable for
    display in a sheet explaining to the user why this certificate chain is
    not trusted for this operation. This function may return NULL if the
    certificate chain was trusted.
*/
CFArrayRef SecTrustCopyProperties(SecTrustRef trust);

/*!
	@function SecTrustCopyInfo
	@abstract Return a dictionary with additional information about the
    evaluated certificate chain for use by clients.
	@param trust A reference to an evaluated trust object.
	@discussion Returns a dictionary for this trust evaluation. This
    dictionary may have the following keys:

        kSecTrustInfoExtendedValidationKey this key will be present and have
            a value of kCFBooleanTrue if this chain was validated for EV.
        kSecTrustInfoCompanyNameKey Company name field of subject of leaf
            certificate, this field is meant to be displayed to the user
            if the kSecTrustInfoExtendedValidationKey is present.
        kSecTrustInfoRevocationKey this key will be present iff this chain
            had its revocation checked. The value will be a kCFBooleanTrue
            if revocation checking was successful and none of the
            certificates in the chain were revoked.
            The value will be kCFBooleanFalse if no current revocation status
            could be obtained for one or more certificates in the chain due
            to connection problems or timeouts etc.  This is a hint to a
            client to retry revocation checking at a later time.
        kSecTrustInfoRevocationValidUntilKey this key will be present iff
            kSecTrustInfoRevocationKey has a value of kCFBooleanTrue.
            The value will be a CFDateRef representing the earliest date at
            which the revocation info for one of the certificates in this chain
            might change.

	@result A dictionary with various fields that can be displayed to the user,
    or NULL if no additional info is available or the trust has not yet been
    validated.  The caller is responsible for calling CFRelease on the value
    returned when it is no longer needed.
*/
CFDictionaryRef SecTrustCopyInfo(SecTrustRef trust);

/* For debugging purposes. */
CFArrayRef SecTrustGetDetails(SecTrustRef trust);

/* For debugging purposes. */
CFStringRef SecTrustCopyFailureDescription(SecTrustRef trust);

/*!
	@function SecTrustSetPolicies
	@abstract Set the trust policies against which the trust should be verified.
	@param trust A reference to a trust object.
    @param policies An array of one or more policies. You may pass a
    SecPolicyRef to represent a single policy.
	@result A result code.  See "Security Error Codes" (SecBase.h).
    @discussion This function does not invalidate the trust, but should do so in the future.
*/
OSStatus SecTrustSetPolicies(SecTrustRef trust, CFTypeRef policies)
   __OSX_AVAILABLE_STARTING(__MAC_10_3, __IPHONE_6_0);

OSStatus SecTrustGetOTAPKIAssetVersionNumber(int* versionNumber);

OSStatus SecTrustOTAPKIGetUpdatedAsset(int* didUpdateAsset);

__END_DECLS

#endif /* !_SECURITY_SECTRUSTPRIV_H_ */
