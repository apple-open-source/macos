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

/*!
	@header SecCertificateRequest
	SecCertificateRequest implements a way to issue a certificate request to a
	certificate authority.
*/

#ifndef _SECURITY_SECCERTIFICATEREQUEST_H_
#define _SECURITY_SECCERTIFICATEREQUEST_H_

#include <Security/SecBase.h>
#include <Security/cssmtype.h>


#if defined(__cplusplus)
extern "C" {
#endif

/*!
    @typedef SecCertificateRequestRef
    @abstract Contains information about a certificate request.
*/
typedef struct OpaqueSecCertificateRequestRef *SecCertificateRequestRef;

/*!
	@function SecCertificateRequestGetTypeID
	Returns the type identifier of all SecCertificateRequest instances.
*/
CFTypeID SecCertificateRequestGetTypeID(void);

/*!
	@function SecCertificateRequestCreate
	Create a certificate request operation based on a policy and certificate type.  If a policy is not specified, one will be chosen for the caller. Once the requeste is created, a request reference is returned. For this request reference, you can set attributes for it by using SecCertificateRequestSetAttribute(). To submit the request call SecCertificateRequestSubmit(). 
    @param certificateType The certificate type (i.e. X509, PGP, etc). These types are in cssmtype.h
    @param requestType The identifier to the type of request to submit (i.e. issue, verify, revoke, etc.). These are defined in cssmtype.h
    @param certRequest A returned reference to the certificate request.
	@result noErr 0 No error.
*/
OSStatus SecCertificateRequestCreate(
        SecPolicyRef policy,
        CSSM_CERT_TYPE certificateType,
        CSSM_TP_AUTHORITY_REQUEST_TYPE requestType,
        SecCertificateRequestRef* certRequest);

/*!
	@function SecCertificateRequestSetPrivateKey
	For a given certificate request, set the private key for which the assocaited public key will be certified.
    @param certRequest A reference to the certificate request.
	@param privateKeyItemRef The keychain item private key to be used for this certificate request. The private key item must be of class type kSecAppleKeyItemClass.
    @result noErr 0 No error.
*/
OSStatus SecCertificateRequestSetPrivateKey(
        SecCertificateRequestRef certRequest,
        SecKeychainItemRef privateKeyItemRef);
        
/*!
	@function SecCertificateRequestSetAttribute
	For a given certificate request, set an optional attribute for the request. For example, an attribute can be the caller credentials or any other attribute needed for the certificate request operation. 
    @param oid An BER-encoded oid that defines the attribute (i.e. CSSMOID_CommonName, CSSMOID_SerialNumber, etc.)
	@param value The value for the attribute.
    @result noErr 0 No error.
*/
OSStatus SecCertificateRequestSetAttribute(
        SecCertificateRequestRef certRequest,
        const CSSM_OID* oid,
        const CSSM_DATA* value);

/*!
	@function SecCertificateRequestSubmit
	Submit a certificate request to be processed by the Security framework. Once the request is submitted, an estimated time is returned indicating when the request results can be retrieved. Once the estimated time has elapsed, obtain the result by calling SecCertificateRequestGetResult(). 
    @param certRequest A reference to the certificate request.
    @param keychain The keychain in which to store the new certificate (for a new cert request) and the cert request item reference.
    @param estimatedTime The number of estimated seconds before the result can be retrieved.
    @param certRequestItemRef The returned persistent reference for the submitted request. This item is stored in the keychain specified by the keychain parameter. This item can be viewed as an certificate request operation that is still pending.
	@result noErr 0 No error.
*/
OSStatus SecCertificateRequestSubmit(
        SecCertificateRequestRef certRequest,
        SecKeychainRef keychain,
        sint32* estimatedTime,
        SecKeychainItemRef* certRequestItemRef);

/*!
	@function SecCertificateRequestCreateFromItem
	Given a keychain item reference (a persistent reference for a certificate request), create a certificate request reference to be used by subsuequent calls that take a SecCertificateRequestRef. The keychain item must be obtained by calling SecKeychainSearchCreateFromAttributes() and SecKeychainCopySearchNextItem() for an item with the class of kSecAppleCertificateRequestItemClass. 
    @param certRequestItemRef A keychain item reference for the certificate request(%%%kSecGenericPasswordItemClass?)
	@param certRequestRef The returned certificate request reference.
    @result noErr 0 No error.
*/
OSStatus SecCertificateRequestCreateFromItem(
        SecKeychainItemRef certRequestItemRef,
        SecCertificateRequestRef* certRequestRef);

/*!
	@function SecCertificateRequestGetType
	Returns the certificate request type (i.e. issue, revoke, etc) for a given certificate request item reference.
    @param certRequestRef A reference to a submitted request.
	@param requestType The returned request type.
    @result noErr 0 No error.
*/
OSStatus SecCertificateRequestGetType(
        SecCertificateRequestRef certRequestRef,
        CSSM_TP_AUTHORITY_REQUEST_TYPE* requestType);

/*!
	@function SecCertificateRequestGetResult
	Get the results of a certificate request. If the request is still pending, the estimated time will be returned which indicates when to call this function again.
    @param certRequestRef A reference for the submitted request.
    @param estimatedTime The number of estimated seconds before the result can be retrieved.
	@param certficateRef The returned certificate reference for a CSSM_TP_AUTHORITY_REQUEST_CERTISSUE only. All other request types return NULL here.
    @result noErr 0 No error.
*/
OSStatus SecCertificateRequestGetResult(
        SecCertificateRequestRef certRequestRef,
        sint32* estimatedTime,
        SecCertificateRef* certificateRef);

#if defined(__cplusplus)
}
#endif

#endif /* !_SECURITY_SECCERTIFICATEREQUEST_H_ */
