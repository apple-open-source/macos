/*
 * Copyright (c) 2003-2004 Apple Computer, Inc. All Rights Reserved.
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
 *
 * SecFileVaultCert.h
 */
 
#ifndef	_SEC_FILEVAULTCERT_H_
#define _SEC_FILEVAULTCERT_H_

#include <Security/Security.h>
#include <CoreFoundation/CoreFoundation.h>

#include <Security/SecBase.h>
#include <CoreFoundation/CFURL.h>

class SecFileVaultCert
{
public:
    SecFileVaultCert();
    ~SecFileVaultCert();
    
    OSStatus createPair(CFStringRef hostName,CFStringRef userName,SecKeychainRef kcRef, CFDataRef *cert);
    
private:
    
    OSStatus generateKeyPair(
        CSSM_CSP_HANDLE 	cspHand,
        CSSM_DL_DB_HANDLE 	dlDbHand,
        CSSM_ALGORITHMS 	keyAlg,				// e.g., CSSM_ALGID_RSA
        uint32				keySizeInBits,
        const char 			*keyLabel,			// C string
        CSSM_KEY_PTR 		*pubKeyPtr,			// mallocd, created, RETURNED
        CSSM_KEY_PTR 		*privKeyPtr);
    
    OSStatus createRootCert(
        CSSM_TP_HANDLE		tpHand,		
        CSSM_CL_HANDLE		clHand,
        CSSM_CSP_HANDLE		cspHand,
        CSSM_KEY_PTR		subjPubKey,
        CSSM_KEY_PTR		signerPrivKey,
        const char			*hostName,			// CSSMOID_CommonName
        const char 			*userName,			// CSSMOID_Description
        CSSM_ALGORITHMS 	sigAlg,
        const CSSM_OID		*sigOid,
        CSSM_DATA_PTR		certData);			// mallocd and RETURNED
        void printError(const char *errDescription,const char *errLocation,OSStatus crtn);
        void randUint32(uint32 &u);

    CSSM_RETURN refKeyToRaw(
        CSSM_CSP_HANDLE	cspHand,
        const CSSM_KEY	*refKey,	
        CSSM_KEY_PTR	rawKey);
    
    CSSM_RETURN setPubKeyHash(
        CSSM_CSP_HANDLE 	cspHand,
        CSSM_DL_DB_HANDLE 	dlDbHand,
        const CSSM_KEY		*pubOrPrivKey,	// to get hash; raw or ref/CSPDL
        const char			*keyLabel);		// look up by this
};

#pragma mark ----- Certificate Management -----

/*
 * Create a key pair and a self-signed certificate. The private key and 
 * the cert are stored in the specified keychain; a copy of the cert is 
 * also returned. 
 *
 * Arguments
 * ---------
 *
 * hostName : The name of this host, e.g., "crypto.apple.com". This 
 *		must match exactly the string later passed as peerHostName 
 *		to SR_SecureTransportConfigure() (see below). This must be 
 *		convertable	to an ASCII C string. 
 *
 * userName : e.g., "James P. Sullivan". Must be convertable to an 
 *		ASCII C string.
 *
 * keychainName : the keychain where the certificate will be stored.
 *
 * cert : the root cert which can be distributed to peers (where it will be
 *		imported via SR_CertificateImport(), below). This is not sensitive
 *		data; it can be bandied about freely. Caller must CFRelease this. 
 */
OSStatus SR_CertificateAndKeyCreate(
	CFStringRef 	hostName,
	CFStringRef		userName,
	SecKeychainRef	keychain,
	CFDataRef		*cert);				// RETURNED
	
/*
 * Import a peer's certificate into specified keychain.
 */
OSStatus SR_CertificateImport(
	SecKeychainRef	keychain,
	CFDataRef		cert);

#pragma mark ----- Operating parameters -----

/*
 * These are some constants which are used in the SecRendezvous 
 * library. Clients of the library don't have to know about these,
 * but they might be useful or interesting.
 */
 
/*
 * The two TLS ciphersuites we support - the first one for 
 * authenticated connections, the second for unauthenticated.
 *
 * Subsequent to calling SR_SecureTransportConfigure(), an app
 * can determine which of these ciphersuites was actually 
 * negotiated by calling SSLGetNegotiatedCipher().
 */
#define SR_CIPHER_AUTHENTICATED		SSL_RSA_WITH_RC4_128_SHA
#define SR_CIPHER_UNAUTHENTICATED	SSL_DH_anon_WITH_RC4_128_MD5

/*
 * Parameters used to create key pairs and certificates in
 * SR_CertificateAndKeyCreate().
 */
#define SR_KEY_ALGORITHM			CSSM_ALGID_RSA
#define SR_KEY_SIZE_IN_BITS			1024

/* 
 * The CSSM_ALGORITHMS and OID values defining the signature
 * algorithm in the generated certificate.
 */
#define SR_CERT_SIGNATURE_ALGORITHM	CSSM_ALGID_SHA1WithRSA
#define SR_CERT_SIGNATURE_ALG_OID	CSSMOID_SHA1WithRSA

#endif	/* _SEC_FILEVAULTCERT_H_ */

