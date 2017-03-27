/*
 * Copyright (c) 2006-2008,2010 Apple Inc. All Rights Reserved.
 */

#ifndef _SSLS_APP_UTILS_H_
#define _SSLS_APP_UTILS_H_ 1

#include <Security/SecBase.h>
#include <Security/SecureTransport.h>
#include <Security/SecureTransportPriv.h>
#include <CoreFoundation/CFArray.h>
#include <stdbool.h>
#include <Security/SecCertificate.h>

#ifdef	__cplusplus
extern "C" {
#endif

#if ! SEC_OS_OSX_INCLUDES
typedef struct OpaqueSecKeychainRef *SecKeychainRef;
#endif

/* disable some Panther-only features */
#define JAGUAR_BUILD	0

const char *sslGetCipherSuiteString(SSLCipherSuite cs);
const char *sslGetProtocolVersionString(SSLProtocol prot);
const char *sslGetSSLErrString(OSStatus err);
void printSslErrStr(const char *op, OSStatus err);
const char *sslGetClientCertStateString(SSLClientCertificateState state);
const char *sslGetClientAuthTypeString(SSLClientAuthenticationType authType);

CFArrayRef getSslCerts(
	const char			*kcName,				// may be NULL, i.e., use default
	bool                encryptOnly,
	bool                completeCertChain,
	const char			*anchorFile,			// optional trusted anchor
	SecKeychainRef		*pKcRef);				// RETURNED
OSStatus sslCompleteCertChain(
	SecIdentityRef 		identity, 
	SecCertificateRef	trustedAnchor,	// optional additional trusted anchor
	bool 				includeRoot, 	// include the root in outArray
//	const CSSM_OID		*vfyPolicy,		// optional - if NULL, use SSL
	CFArrayRef			*outArray);		// created and RETURNED
CFArrayRef sslKcRefToCertArray(
	SecKeychainRef		kcRef,
	bool                encryptOnly,
	bool                completeCertChain,
//	const CSSM_OID		*vfyPolicy,		// optional - if NULL, use SSL policy to complete
	const char			*trustedAnchorFile);

OSStatus addTrustedSecCert(
	SSLContextRef 		ctx,
	SecCertificateRef 	secCert, 
	bool                replaceAnchors);
OSStatus sslReadAnchor(
	const char 			*anchorFile,
	SecCertificateRef 	*certRef);
OSStatus sslAddTrustedRoot(
	SSLContextRef 		ctx,
	const char 			*anchorFile, 
	bool                replaceAnchors);

/*
 * Assume incoming identity contains a root (e.g., created by
 * certtool) and add that cert to ST's trusted anchors. This
 * enables ST's verify of the incoming chain to succeed without 
 * a kludgy "AllowAnyRoot" specification.
 */
OSStatus addIdentityAsTrustedRoot(
	SSLContextRef 	ctx,
	CFArrayRef		identArray);
	
OSStatus sslAddTrustedRoots(
	SSLContextRef 	ctx,
	SecKeychainRef	keychain,
	bool			*foundOne);

void sslOutputDot();

/*
 * Lists of SSLCipherSuites used in sslSetCipherRestrictions. 
 */
extern const SSLCipherSuite suites40[];
extern const SSLCipherSuite suitesDES[];
extern const SSLCipherSuite suitesDES40[];
extern const SSLCipherSuite suites3DES[];
extern const SSLCipherSuite suitesRC4[];
extern const SSLCipherSuite suitesRC4_40[];
extern const SSLCipherSuite suitesRC2[];
extern const SSLCipherSuite suitesAES128[];
extern const SSLCipherSuite suitesAES256[];
extern const SSLCipherSuite suitesDH[];
extern const SSLCipherSuite suitesDHAnon[];
extern const SSLCipherSuite suitesDH_RSA[];
extern const SSLCipherSuite suitesDH_DSS[];
extern const SSLCipherSuite suites_SHA1[];
extern const SSLCipherSuite suites_MD5[];
extern const SSLCipherSuite suites_ECDHE[];
extern const SSLCipherSuite suites_ECDH[];

/*
 * Given an SSLContextRef and an array of SSLCipherSuites, terminated by
 * SSL_NO_SUCH_CIPHERSUITE, select those SSLCipherSuites which the library
 * supports and do a SSLSetEnabledCiphers() specifying those. 
 */
OSStatus sslSetEnabledCiphers(
	SSLContextRef ctx,
	const SSLCipherSuite *ciphers);

/*
 * Specify restricted sets of cipherspecs and protocols.
 */
OSStatus sslSetCipherRestrictions(
	SSLContextRef ctx,
	char cipherRestrict);

#ifndef	SPHINX
OSStatus sslSetProtocols(
	SSLContextRef 	ctx,
	const char		*acceptedProts,
	SSLProtocol		tryVersion);			// only used if acceptedProts NULL
#endif

int sslVerifyRtn(
	const char	*whichSide,		// "client" or "server"
	OSStatus	expectRtn,
	OSStatus	gotRtn);
int sslVerifyProtVers(
	const char	*whichSide,		// "client" or "server"
	SSLProtocol	expectProt,
	SSLProtocol	gotProt);		
int sslVerifyClientCertState(
	const char					*whichSide,		// "client" or "server"
	SSLClientCertificateState	expectState,
	SSLClientCertificateState	gotState);
int sslVerifyCipher(
	const char		*whichSide,		// "client" or "server"
	SSLCipherSuite	expectCipher,
	SSLCipherSuite	gotCipher);	


/*
 * Wrapper for sslIdentPicker, with optional trusted anchor specified as a filename.
 */
OSStatus sslIdentityPicker(
	SecKeychainRef		kcRef,			// NULL means use default list
	const char			*trustedAnchor,	// optional additional trusted anchor
	bool				includeRoot,	// true --> root is appended to outArray
										// false --> root not included
//	const CSSM_OID		*vfyPolicy,		// optional - if NULL, use SSL
	CFArrayRef			*outArray);		// created and RETURNED

void sslKeychainPath(
	const char *kcName,
	char *kcPath);			// allocd by caller, MAXPATHLEN

/* Verify presence of required file. Returns nonzero if not found. */
int sslCheckFile(const char *path);

/* Stringify a SSL_ECDSA_NamedCurve */
extern const char *sslCurveString(
	SSL_ECDSA_NamedCurve namedCurve);

SecKeyRef create_private_key_from_der(bool ecdsa, const unsigned char *pkey_der, size_t pkey_der_len);
CFArrayRef chain_from_der(bool ecdsa, const unsigned char *pkey_der, size_t pkey_der_len, const unsigned char *cert_der, size_t cert_der_len);

#ifdef	__cplusplus
}
#endif

#endif	/* _SSLS_APP_UTILS_H_ */
