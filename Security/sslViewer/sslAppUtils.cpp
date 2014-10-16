/*
 * Copyright (c) 2006-2008,2010-2013 Apple Inc. All Rights Reserved.
 */

#include "sslAppUtils.h"
//#include "sslThreading.h"
//#include "identPicker.h"
//#include <utilLib/common.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/param.h>
#include <Security/SecBase.h>

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include <Security/SecIdentityPriv.h>
#include <AssertMacros.h>
#include <sys/time.h>

#include "utilities/SecCFRelease.h"

/* Set true when PR-3074739 is merged to TOT */
#define NEW_SSL_ERRS_3074739		1


const char *sslGetCipherSuiteString(SSLCipherSuite cs)
{
	static char noSuite[40];
	
	switch(cs) {
		case SSL_NULL_WITH_NULL_NULL:
			return "SSL_NULL_WITH_NULL_NULL";
		case SSL_RSA_WITH_NULL_MD5:
			return "SSL_RSA_WITH_NULL_MD5";
		case SSL_RSA_WITH_NULL_SHA:
			return "SSL_RSA_WITH_NULL_SHA";
		case SSL_RSA_EXPORT_WITH_RC4_40_MD5:
			return "SSL_RSA_EXPORT_WITH_RC4_40_MD5";
		case SSL_RSA_WITH_RC4_128_MD5:
			return "SSL_RSA_WITH_RC4_128_MD5";
		case SSL_RSA_WITH_RC4_128_SHA:
			return "SSL_RSA_WITH_RC4_128_SHA";
		case SSL_RSA_EXPORT_WITH_RC2_CBC_40_MD5:
			return "SSL_RSA_EXPORT_WITH_RC2_CBC_40_MD5";
		case SSL_RSA_WITH_IDEA_CBC_SHA:
			return "SSL_RSA_WITH_IDEA_CBC_SHA";
		case SSL_RSA_EXPORT_WITH_DES40_CBC_SHA:
			return "SSL_RSA_EXPORT_WITH_DES40_CBC_SHA";
		case SSL_RSA_WITH_DES_CBC_SHA:
			return "SSL_RSA_WITH_DES_CBC_SHA";
		case SSL_RSA_WITH_3DES_EDE_CBC_SHA:
			return "SSL_RSA_WITH_3DES_EDE_CBC_SHA";
		case SSL_DH_DSS_EXPORT_WITH_DES40_CBC_SHA:
			return "SSL_DH_DSS_EXPORT_WITH_DES40_CBC_SHA";
		case SSL_DH_DSS_WITH_DES_CBC_SHA:
			return "SSL_DH_DSS_WITH_DES_CBC_SHA";
		case SSL_DH_DSS_WITH_3DES_EDE_CBC_SHA:
			return "SSL_DH_DSS_WITH_3DES_EDE_CBC_SHA";
		case SSL_DH_RSA_EXPORT_WITH_DES40_CBC_SHA:
			return "SSL_DH_RSA_EXPORT_WITH_DES40_CBC_SHA";
		case SSL_DH_RSA_WITH_DES_CBC_SHA:
			return "SSL_DH_RSA_WITH_DES_CBC_SHA";
		case SSL_DH_RSA_WITH_3DES_EDE_CBC_SHA:
			return "SSL_DH_RSA_WITH_3DES_EDE_CBC_SHA";
		case SSL_DHE_DSS_EXPORT_WITH_DES40_CBC_SHA:
			return "SSL_DHE_DSS_EXPORT_WITH_DES40_CBC_SHA";
		case SSL_DHE_DSS_WITH_DES_CBC_SHA:
			return "SSL_DHE_DSS_WITH_DES_CBC_SHA";
		case SSL_DHE_DSS_WITH_3DES_EDE_CBC_SHA:
			return "SSL_DHE_DSS_WITH_3DES_EDE_CBC_SHA";
		case SSL_DHE_RSA_EXPORT_WITH_DES40_CBC_SHA:
			return "SSL_DHE_RSA_EXPORT_WITH_DES40_CBC_SHA";
		case SSL_DHE_RSA_WITH_DES_CBC_SHA:
			return "SSL_DHE_RSA_WITH_DES_CBC_SHA";
		case SSL_DHE_RSA_WITH_3DES_EDE_CBC_SHA:
			return "SSL_DHE_RSA_WITH_3DES_EDE_CBC_SHA";
		case SSL_DH_anon_EXPORT_WITH_RC4_40_MD5:
			return "SSL_DH_anon_EXPORT_WITH_RC4_40_MD5";
		case SSL_DH_anon_WITH_RC4_128_MD5:
			return "SSL_DH_anon_WITH_RC4_128_MD5";
		case SSL_DH_anon_EXPORT_WITH_DES40_CBC_SHA:
			return "SSL_DH_anon_EXPORT_WITH_DES40_CBC_SHA";
		case SSL_DH_anon_WITH_DES_CBC_SHA:
			return "SSL_DH_anon_WITH_DES_CBC_SHA";
		case SSL_DH_anon_WITH_3DES_EDE_CBC_SHA:
			return "SSL_DH_anon_WITH_3DES_EDE_CBC_SHA";
		case SSL_FORTEZZA_DMS_WITH_NULL_SHA:
			return "SSL_FORTEZZA_DMS_WITH_NULL_SHA";
		case SSL_FORTEZZA_DMS_WITH_FORTEZZA_CBC_SHA:
			return "SSL_FORTEZZA_DMS_WITH_FORTEZZA_CBC_SHA";
		case SSL_RSA_WITH_RC2_CBC_MD5:
			return "SSL_RSA_WITH_RC2_CBC_MD5";
		case SSL_RSA_WITH_IDEA_CBC_MD5:
			return "SSL_RSA_WITH_IDEA_CBC_MD5";
		case SSL_RSA_WITH_DES_CBC_MD5:
			return "SSL_RSA_WITH_DES_CBC_MD5";
		case SSL_RSA_WITH_3DES_EDE_CBC_MD5:
			return "SSL_RSA_WITH_3DES_EDE_CBC_MD5";
		case SSL_NO_SUCH_CIPHERSUITE:
			return "SSL_NO_SUCH_CIPHERSUITE";
		case TLS_RSA_WITH_AES_128_CBC_SHA:
			return "TLS_RSA_WITH_AES_128_CBC_SHA";
		case TLS_DH_DSS_WITH_AES_128_CBC_SHA:
			return "TLS_DH_DSS_WITH_AES_128_CBC_SHA";
		case TLS_DH_RSA_WITH_AES_128_CBC_SHA:
			return "TLS_DH_RSA_WITH_AES_128_CBC_SHA";
		case TLS_DHE_DSS_WITH_AES_128_CBC_SHA:
			return "TLS_DHE_DSS_WITH_AES_128_CBC_SHA";
		case TLS_DHE_RSA_WITH_AES_128_CBC_SHA:
			return "TLS_DHE_RSA_WITH_AES_128_CBC_SHA";
		case TLS_DH_anon_WITH_AES_128_CBC_SHA:
			return "TLS_DH_anon_WITH_AES_128_CBC_SHA";
		case TLS_RSA_WITH_AES_256_CBC_SHA:
			return "TLS_RSA_WITH_AES_256_CBC_SHA";
		case TLS_DH_DSS_WITH_AES_256_CBC_SHA:
			return "TLS_DH_DSS_WITH_AES_256_CBC_SHA";
		case TLS_DH_RSA_WITH_AES_256_CBC_SHA:
			return "TLS_DH_RSA_WITH_AES_256_CBC_SHA";
		case TLS_DHE_DSS_WITH_AES_256_CBC_SHA:
			return "TLS_DHE_DSS_WITH_AES_256_CBC_SHA";
		case TLS_DHE_RSA_WITH_AES_256_CBC_SHA:
			return "TLS_DHE_RSA_WITH_AES_256_CBC_SHA";
		case TLS_DH_anon_WITH_AES_256_CBC_SHA:
			return "TLS_DH_anon_WITH_AES_256_CBC_SHA";

		default:
			sprintf(noSuite, "Unknown (%d)", (unsigned)cs);
			return noSuite;	
	}
}

/* 
 * Given a SSLProtocolVersion - typically from SSLGetProtocolVersion -
 * return a string representation.
 */
const char *sslGetProtocolVersionString(SSLProtocol prot)
{
	static char noProt[20];
	
	switch(prot) {
		case kSSLProtocolUnknown:
			return "kSSLProtocolUnknown";
		case kSSLProtocol2:
			return "kSSLProtocol2";
		case kSSLProtocol3:
			return "kSSLProtocol3";
		case kSSLProtocol3Only:
			return "kSSLProtocol3Only";
		case kTLSProtocol1:
			return "kTLSProtocol1";
		case kTLSProtocol1Only:
			return "kTLSProtocol1Only";
        case kTLSProtocol11:
			return "kTLSProtocol11";
        case kTLSProtocol12:
			return "kTLSProtocol12";
		default:
			sprintf(noProt, "Unknown (%d)", (unsigned)prot);
			return noProt;	
	}
}

/* 
 * Return string representation of SecureTransport-related OSStatus.
 */
const char *sslGetSSLErrString(OSStatus err)
{
	static char errSecSuccessStr[20];
	
	switch(err) {
		case errSecSuccess:
			return "errSecSuccess";
		case errSecAllocate:
			return "errSecAllocate";
		case errSecParam:
			return "errSecParam";
		case errSecUnimplemented:
			return "errSecUnimplemented";
		case errSecIO:
			return "errSecIO";
		case errSecBadReq:
			return "errSecBadReq";
		case errSSLProtocol:
			return "errSSLProtocol";
		case errSSLNegotiation:
			return "errSSLNegotiation";
		case errSSLFatalAlert:
			return "errSSLFatalAlert";
		case errSSLWouldBlock:
			return "errSSLWouldBlock";
		case errSSLSessionNotFound:
			return "errSSLSessionNotFound";
		case errSSLClosedGraceful:
			return "errSSLClosedGraceful";
		case errSSLClosedAbort:
			return "errSSLClosedAbort";
   		case errSSLXCertChainInvalid:
			return "errSSLXCertChainInvalid";
		case errSSLBadCert:
			return "errSSLBadCert"; 
		case errSSLCrypto:
			return "errSSLCrypto";
		case errSSLInternal:
			return "errSSLInternal";
		case errSSLModuleAttach:
			return "errSSLModuleAttach";
		case errSSLUnknownRootCert:
			return "errSSLUnknownRootCert";
		case errSSLNoRootCert:
			return "errSSLNoRootCert";
		case errSSLCertExpired:
			return "errSSLCertExpired";
		case errSSLCertNotYetValid:
			return "errSSLCertNotYetValid";
		case errSSLClosedNoNotify:
			return "errSSLClosedNoNotify";
		case errSSLBufferOverflow:
			return "errSSLBufferOverflow";
		case errSSLBadCipherSuite:
			return "errSSLBadCipherSuite";
		/* TLS/Panther addenda */
		case errSSLPeerUnexpectedMsg:
			return "errSSLPeerUnexpectedMsg";
		case errSSLPeerBadRecordMac:
			return "errSSLPeerBadRecordMac";
		case errSSLPeerDecryptionFail:
			return "errSSLPeerDecryptionFail";
		case errSSLPeerRecordOverflow:
			return "errSSLPeerRecordOverflow";
		case errSSLPeerDecompressFail:
			return "errSSLPeerDecompressFail";
		case errSSLPeerHandshakeFail:
			return "errSSLPeerHandshakeFail";
		case errSSLPeerBadCert:
			return "errSSLPeerBadCert";
		case errSSLPeerUnsupportedCert:
			return "errSSLPeerUnsupportedCert";
		case errSSLPeerCertRevoked:
			return "errSSLPeerCertRevoked";
		case errSSLPeerCertExpired:
			return "errSSLPeerCertExpired";
		case errSSLPeerCertUnknown:
			return "errSSLPeerCertUnknown";
		case errSSLIllegalParam:
			return "errSSLIllegalParam";
		case errSSLPeerUnknownCA:
			return "errSSLPeerUnknownCA";
		case errSSLPeerAccessDenied:
			return "errSSLPeerAccessDenied";
		case errSSLPeerDecodeError:
			return "errSSLPeerDecodeError";
		case errSSLPeerDecryptError:
			return "errSSLPeerDecryptError";
		case errSSLPeerExportRestriction:
			return "errSSLPeerExportRestriction";
		case errSSLPeerProtocolVersion:
			return "errSSLPeerProtocolVersion";
		case errSSLPeerInsufficientSecurity:
			return "errSSLPeerInsufficientSecurity";
		case errSSLPeerInternalError:
			return "errSSLPeerInternalError";
		case errSSLPeerUserCancelled:
			return "errSSLPeerUserCancelled";
		case errSSLPeerNoRenegotiation:
			return "errSSLPeerNoRenegotiation";
		case errSSLHostNameMismatch:
			return "errSSLHostNameMismatch";
		case errSSLConnectionRefused:
			return "errSSLConnectionRefused";
		case errSSLDecryptionFail:
			return "errSSLDecryptionFail";
		case errSSLBadRecordMac:
			return "errSSLBadRecordMac";
		case errSSLRecordOverflow:
			return "errSSLRecordOverflow";
		case errSSLBadConfiguration:
			return "errSSLBadConfiguration";
		
		/* some from the Sec layer */
		case errSecNotAvailable:			return "errSecNotAvailable";
		case errSecDuplicateItem:			return "errSecDuplicateItem";
		case errSecItemNotFound:			return "errSecItemNotFound";
#if 0
		case errSessionInvalidId:			return "errSessionInvalidId";
		case errSessionInvalidAttributes:	return "errSessionInvalidAttributes";
		case errSessionAuthorizationDenied:	return "errSessionAuthorizationDenied";
		case errSessionInternal:			return "errSessionInternal";
		case errSessionInvalidFlags:		return "errSessionInvalidFlags";
#endif

		default:
#if 0
			if (err < (CSSM_BASE_ERROR + 
			         (CSSM_ERRORCODE_MODULE_EXTENT * 8)))
			{
				/* assume CSSM error */
				return cssmErrToStr(err);
			}
			else
#endif
			{
				sprintf(errSecSuccessStr, "Unknown (%d)", (unsigned)err);
				return errSecSuccessStr;	
			}
	}
}

void printSslErrStr(
	const char 	*op,
	OSStatus 	err)
{
	printf("*** %s: %s\n", op, sslGetSSLErrString(err));
}

const char *sslGetClientCertStateString(SSLClientCertificateState state)
{
	static char noState[20];
	
	switch(state) {
		case kSSLClientCertNone:
			return "ClientCertNone";
		case kSSLClientCertRequested:
			return "CertRequested";
		case kSSLClientCertSent:
			return "ClientCertSent";
		case kSSLClientCertRejected:
			return "ClientCertRejected";
		default:
			sprintf(noState, "Unknown (%d)", (unsigned)state);
			return noState;	
	}

}

/*
 * Convert a keychain name (which may be NULL) into the CFArrayRef required
 * by SSLSetCertificate. This is a bare-bones example of this operation,
 * since it requires and assumes that there is exactly one SecIdentity
 * in the keychain - i.e., there is exactly one matching cert/private key 
 * pair. A real world server would probably search a keychain for a SecIdentity 
 * matching some specific criteria. 
 */
CFArrayRef getSslCerts( 
	const char			*kcName,				// may be NULL, i.e., use default
	bool                encryptOnly,
	bool                completeCertChain,
	const char			*anchorFile,			// optional trusted anchor
	SecKeychainRef		*pKcRef)				// RETURNED
{
#if 0
	SecKeychainRef 		kcRef = nil;
	OSStatus			ortn;
	
	*pKcRef = nil;
	
	/* pick a keychain */
	if(kcName) {
		ortn = SecKeychainOpen(kcName, &kcRef);
		if(ortn) {
			printf("SecKeychainOpen returned %d.\n", (int)ortn);
			printf("Cannot open keychain at %s. Aborting.\n", kcName);
			return NULL;
		}
	}
	else {
		/* use default keychain */
		ortn = SecKeychainCopyDefault(&kcRef);
		if(ortn) {
			printf("SecKeychainCopyDefault returned %d; aborting.\n", (int)ortn);
			return nil;
		}
	}
	*pKcRef = kcRef;
	return sslKcRefToCertArray(kcRef, encryptOnly, completeCertChain, anchorFile);
#else
	SecCertificateRef cert = NULL;
	SecIdentityRef identity = NULL;
	CFMutableArrayRef certificates = NULL, result = NULL;
	CFMutableDictionaryRef certQuery = NULL, keyQuery = NULL, keyResult = NULL;
	SecTrustRef trust = NULL;
	SecKeyRef key = NULL;
	CFTypeRef pkdigest = NULL;

	// Find the first private key in the keychain and return both it's
	// attributes and a ref to it.
	require(keyQuery = CFDictionaryCreateMutable(NULL, 0, NULL, NULL), errOut);
	CFDictionaryAddValue(keyQuery, kSecClass, kSecClassKey);
	CFDictionaryAddValue(keyQuery, kSecAttrKeyClass, kSecAttrKeyClassPrivate);
	CFDictionaryAddValue(keyQuery, kSecReturnRef, kCFBooleanTrue);
	CFDictionaryAddValue(keyQuery, kSecReturnAttributes, kCFBooleanTrue);
	require_noerr(SecItemCopyMatching(keyQuery, (CFTypeRef *)&keyResult),
		errOut);
	require(key = (SecKeyRef)CFDictionaryGetValue(keyResult, kSecValueRef),
		errOut);
	require(pkdigest = CFDictionaryGetValue(keyResult, kSecAttrApplicationLabel),
		errOut);

	// Find the first certificate that has the same public key hash as the
	// returned private key and return it as a ref.
	require(certQuery = CFDictionaryCreateMutable(NULL, 0, NULL, NULL), errOut);
	CFDictionaryAddValue(certQuery, kSecClass, kSecClassCertificate);
	CFDictionaryAddValue(certQuery, kSecAttrPublicKeyHash, pkdigest);
	CFDictionaryAddValue(certQuery, kSecReturnRef, kCFBooleanTrue);
	require_noerr(SecItemCopyMatching(certQuery, (CFTypeRef *)&cert), errOut);

	// Create an identity from the key and certificate.
	require(identity = SecIdentityCreate(NULL, cert, key), errOut);

	// Build a (partial) certificate chain from cert
	require(certificates = CFArrayCreateMutable(NULL, 0,
		&kCFTypeArrayCallBacks), errOut);
	CFArrayAppendValue(certificates, cert);
	require_noerr(SecTrustCreateWithCertificates(certificates, NULL, &trust),
		errOut);
	SecTrustResultType tresult;
	require_noerr(SecTrustEvaluate(trust, &tresult), errOut);

	CFIndex certCount, ix;
	// We need at least 1 certificate
	require(certCount = SecTrustGetCertificateCount(trust), errOut);

	// Build a result where element 0 is the identity and the other elements
	// are the certs in the chain starting at the first intermediate up to the
	// anchor, if we found one, or as far as we were able to build the chain
	// if not.
	require(result = CFArrayCreateMutable(NULL, certCount, &kCFTypeArrayCallBacks),
		errOut);

	// We are commited to returning a result now, so do not use require below
	// this line without setting result to NULL again.
	CFArrayAppendValue(result, identity);
	for (ix = 1; ix < certCount; ++ix) {
		CFArrayAppendValue(result, SecTrustGetCertificateAtIndex(trust, ix));
	}

errOut:
	CFReleaseSafe(trust);
	CFReleaseSafe(certificates);
	CFReleaseSafe(identity);
	CFReleaseSafe(cert);
	CFReleaseSafe(certQuery);
	CFReleaseSafe(keyResult);
	CFReleaseSafe(keyQuery);

    return result;
#endif
}

#if 0
/*
 * Determine if specified SecCertificateRef is a self-signed cert.
 * We do this by comparing the subject and issuerr names; no cryptographic
 * verification is performed.
 *
 * Returns true if the cert appears to be a root. 
 */
static bool isCertRefRoot(
	SecCertificateRef certRef)
{
	bool brtn = false;
#if 0
	/* just search for the two attrs we want */
	UInt32 tags[2] = {kSecSubjectItemAttr, kSecIssuerItemAttr};
	SecKeychainAttributeInfo attrInfo;
	attrInfo.count = 2;
	attrInfo.tag = tags;
	attrInfo.format = NULL;
	SecKeychainAttributeList *attrList = NULL;
	SecKeychainAttribute *attr1 = NULL;
	SecKeychainAttribute *attr2 = NULL;
	
	OSStatus ortn = SecKeychainItemCopyAttributesAndData(
		(SecKeychainItemRef)certRef, 
		&attrInfo,
		NULL,			// itemClass
		&attrList, 
		NULL,			// length - don't need the data
		NULL);			// outData
	if(ortn) {
		cssmPerror("SecKeychainItemCopyAttributesAndData", ortn);
		/* may want to be a bit more robust here, but this should
		 * never happen */
		return false;
	}
	/* subsequent errors to errOut: */
	
	if((attrList == NULL) || (attrList->count != 2)) {
		printf("***Unexpected result fetching label attr\n");
		goto errOut;
	}
	
	/* rootness is just byte-for-byte compare of the two names */ 
	attr1 = &attrList->attr[0];
	attr2 = &attrList->attr[1];
	if(attr1->length == attr2->length) {
		if(memcmp(attr1->data, attr2->data, attr1->length) == 0) {
			brtn = true;
		}
	}
errOut:
	SecKeychainItemFreeAttributesAndData(attrList, NULL);
#endif
	return brtn;
}
#endif

#if 0
/*
 * Given a SecIdentityRef, do our best to construct a complete, ordered, and 
 * verified cert chain, returning the result in a CFArrayRef. The result is 
 * suitable for use when calling SSLSetCertificate().
 */
OSStatus sslCompleteCertChain(
	SecIdentityRef 		identity, 
	SecCertificateRef	trustedAnchor,	// optional additional trusted anchor
	bool 				includeRoot, 	// include the root in outArray
	CFArrayRef			*outArray)		// created and RETURNED
{
	CFMutableArrayRef 			certArray;
	SecTrustRef					secTrust = NULL;
	SecPolicyRef				policy = NULL;
	SecPolicySearchRef			policySearch = NULL;
	SecTrustResultType			secTrustResult;
	CSSM_TP_APPLE_EVIDENCE_INFO *dummyEv;			// not used
	CFArrayRef					certChain = NULL;   // constructed chain
	CFIndex 					numResCerts;
	
	certArray = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	CFArrayAppendValue(certArray, identity);
	
	/*
	 * Case 1: identity is a root; we're done. Note that this case
	 * overrides the includeRoot argument.
	 */
	SecCertificateRef certRef;
	OSStatus ortn = SecIdentityCopyCertificate(identity, &certRef);
	if(ortn) {
		/* should never happen */
		cssmPerror("SecIdentityCopyCertificate", ortn);
		return ortn;
	}
	bool isRoot = isCertRefRoot(certRef);
	if(isRoot) {
		*outArray = certArray;
		CFRelease(certRef);
		return errSecSuccess;
	}
	
	/* 
	 * Now use SecTrust to get a complete cert chain, using all of the 
	 * user's keychains to look for intermediate certs.
	 * NOTE this does NOT handle root certs which are not in the system
	 * root cert DB. (The above case, where the identity is a root cert, does.)
	 */
	CFMutableArrayRef subjCerts = CFArrayCreateMutable(NULL, 1, &kCFTypeArrayCallBacks);
	CFArraySetValueAtIndex(subjCerts, 0, certRef);
			
	/* the array owns the subject cert ref now */
	CFRelease(certRef);
	
	/* Get a SecPolicyRef for generic X509 cert chain verification */
	ortn = SecPolicySearchCreate(CSSM_CERT_X_509v3,
		&CSSMOID_APPLE_X509_BASIC,
		NULL,				// value
		&policySearch);
	if(ortn) {
		cssmPerror("SecPolicySearchCreate", ortn);
		goto errOut;
	}
	ortn = SecPolicySearchCopyNext(policySearch, &policy);
	if(ortn) {
		cssmPerror("SecPolicySearchCopyNext", ortn);
		goto errOut;
	}

	/* build a SecTrustRef for specified policy and certs */
	ortn = SecTrustCreateWithCertificates(subjCerts,
		policy, &secTrust);
	if(ortn) {
		cssmPerror("SecTrustCreateWithCertificates", ortn);
		goto errOut;
	}
	
	if(trustedAnchor) {
		/* 
		 * Tell SecTrust to trust this one in addition to the current
		 * trusted system-wide anchors.
		 */
		CFMutableArrayRef newAnchors;
		CFArrayRef currAnchors;
		
		ortn = SecTrustCopyAnchorCertificates(&currAnchors);
		if(ortn) {
			/* should never happen */
			cssmPerror("SecTrustCopyAnchorCertificates", ortn);
			goto errOut;
		}
		newAnchors = CFArrayCreateMutableCopy(NULL,
			CFArrayGetCount(currAnchors) + 1,
			currAnchors);
		CFRelease(currAnchors);
		CFArrayAppendValue(newAnchors, trustedAnchor);
		ortn = SecTrustSetAnchorCertificates(secTrust, newAnchors);
		CFRelease(newAnchors);
		if(ortn) {
			cssmPerror("SecTrustSetAnchorCertificates", ortn);
			goto errOut;
		}
	}
	/* evaluate: GO */
	ortn = SecTrustEvaluate(secTrust, &secTrustResult);
	if(ortn) {
		cssmPerror("SecTrustEvaluate", ortn);
		goto errOut;
	}
	switch(secTrustResult) {
		case kSecTrustResultUnspecified:
			/* cert chain valid, no special UserTrust assignments */
		case kSecTrustResultProceed:
			/* cert chain valid AND user explicitly trusts this */
			break;
		default:
			/*
			 * Cert chain construction failed. 
			 * Just go with the single subject cert we were given.
			 */
			printf("***Warning: could not construct completed cert chain\n");
			ortn = errSecSuccess;
			goto errOut;
	}

	/* get resulting constructed cert chain */
	ortn = SecTrustGetResult(secTrust, &secTrustResult, &certChain, &dummyEv);
	if(ortn) {
		cssmPerror("SecTrustEvaluate", ortn);
		goto errOut;
	}
	
	/*
	 * Copy certs from constructed chain to our result array, skipping 
	 * the leaf (which is already there, as a SecIdentityRef) and possibly
	 * a root.
	 */
	numResCerts = CFArrayGetCount(certChain);
	if(numResCerts < 2) {
		/*
		 * Can't happen: if subject was a root, we'd already have returned. 
		 * If chain doesn't verify to a root, we'd have bailed after
		 * SecTrustEvaluate().
		 */
		printf("***sslCompleteCertChain screwup: numResCerts %d\n", 
			(int)numResCerts);
		ortn = errSecSuccess;
		goto errOut;
	}
	if(!includeRoot) {
		/* skip the last (root) cert) */
		numResCerts--;
	}
	for(CFIndex dex=1; dex<numResCerts; dex++) {
		certRef = (SecCertificateRef)CFArrayGetValueAtIndex(certChain, dex);
		CFArrayAppendValue(certArray, certRef);
	}
errOut:
	/* clean up */
	if(secTrust) {
		CFRelease(secTrust);
	}
	if(subjCerts) {
		CFRelease(subjCerts);
	}
	if(policy) {
		CFRelease(policy);
	}
	if(policySearch) {
		CFRelease(policySearch);
	}
	*outArray = certArray;
	return ortn;
}


/*
 * Given an open keychain, find a SecIdentityRef and munge it into
 * a CFArrayRef required by SSLSetCertificate().
 */
CFArrayRef sslKcRefToCertArray(
	SecKeychainRef		kcRef,
	bool                encryptOnly,
	bool                completeCertChain,
	const char			*trustedAnchorFile)
{
	/* quick check to make sure the keychain exists */
	SecKeychainStatus kcStat;
	OSStatus ortn = SecKeychainGetStatus(kcRef, &kcStat);
	if(ortn) {
		printSslErrStr("SecKeychainGetStatus", ortn);
		printf("Can not open keychain. Aborting.\n");
		return nil;
	}
	
	/*
	 * Search for "any" identity matching specified key use; 
	 * in this app, we expect there to be exactly one. 
	 */
	SecIdentitySearchRef srchRef = nil;
	ortn = SecIdentitySearchCreate(kcRef, 
		encryptOnly ? CSSM_KEYUSE_DECRYPT : CSSM_KEYUSE_SIGN,
		&srchRef);
	if(ortn) {
		printf("SecIdentitySearchCreate returned %d.\n", (int)ortn);
		printf("Cannot find signing key in keychain. Aborting.\n");
		return nil;
	}
	SecIdentityRef identity = nil;
	ortn = SecIdentitySearchCopyNext(srchRef, &identity);
	if(ortn) {
		printf("SecIdentitySearchCopyNext returned %d.\n", (int)ortn);
		printf("Cannot find signing key in keychain. Aborting.\n");
		return nil;
	}
	if(CFGetTypeID(identity) != SecIdentityGetTypeID()) {
		printf("SecIdentitySearchCopyNext CFTypeID failure!\n");
		return nil;
	}

	/* 
	 * Found one. 
	 */
	if(completeCertChain) {
		/* 
		 * Place it and the other certs needed to verify it -
		 * up to but not including the root - in a CFArray.
		 */
		SecCertificateRef anchorCert = NULL;
		if(trustedAnchorFile) {
			ortn = sslReadAnchor(trustedAnchorFile, &anchorCert);
			if(ortn) {
				printf("***Error reading anchor file\n");
			}
		}
		CFArrayRef ca;
		ortn = sslCompleteCertChain(identity, anchorCert, false, &ca);
		if(anchorCert) {
			CFRelease(anchorCert);
		}
		return ca;
	}
	else {
		/* simple case, just this one identity */
		CFArrayRef ca = CFArrayCreate(NULL,
			(const void **)&identity,
			1,
			NULL);
		if(ca == nil) {
			printf("CFArrayCreate error\n");
		}
		return ca;
	}
}
#endif

OSStatus addTrustedSecCert(
	SSLContextRef 		ctx,
	SecCertificateRef 	secCert, 
	bool                replaceAnchors)
{
	OSStatus ortn;
	CFMutableArrayRef array;
	
	if(secCert == NULL) {
		printf("***addTrustedSecCert screwup\n");
		return errSecParam;
	}
	array = CFArrayCreateMutable(kCFAllocatorDefault,
		(CFIndex)1, &kCFTypeArrayCallBacks);
	if(array == NULL) {
		return errSecAllocate;	
	}
	CFArrayAppendValue(array, secCert);
	ortn = SSLSetTrustedRoots(ctx, array, replaceAnchors ? true : false);
	if(ortn) {
		printSslErrStr("SSLSetTrustedRoots", ortn);
	}
	CFRelease(array);
	return ortn;
}

#if 0
OSStatus sslReadAnchor(
	const char 			*anchorFile,
	SecCertificateRef 	*certRef)
{
	OSStatus ortn;
	SecCertificateRef secCert;
	unsigned char *certData;
	unsigned certLen;
	CSSM_DATA cert;
	
	if(readFile(anchorFile, &certData, &certLen)) {
		return -1;
	}
	cert.Data = certData;
	cert.Length = certLen;
	ortn = SecCertificateCreateFromData(&cert,
			CSSM_CERT_X_509v3,
			CSSM_CERT_ENCODING_DER,
			&secCert);
	free(certData);
	if(ortn) {
		printf("***SecCertificateCreateFromData returned %d\n", (int)ortn);
		return ortn;
	}
	*certRef = secCert;
	return errSecSuccess;
}
#endif

OSStatus sslAddTrustedRoot(
	SSLContextRef 	ctx,
	const char 		*anchorFile, 
	bool            replaceAnchors)
{
#if 0
	OSStatus ortn;
	SecCertificateRef secCert;
	
	ortn = sslReadAnchor(anchorFile, &secCert);
	if(ortn) {
		printf("***Error reading %s. SSLSetTrustedRoots skipped.\n",
			anchorFile);
		return ortn;
	}
	return addTrustedSecCert(ctx, secCert, replaceAnchors);
#else
	return 0;
#endif
}

#if 0
/* Per 3537606 this is no longer necessary */
/*
 * Assume incoming identity contains a root (e.g., created by
 * certtool) and add that cert to ST's trusted anchors. This
 * enables ST's verify of the incoming chain to succeed without 
 * a kludgy "AllowAnyRoot" specification.
 */
OSStatus addIdentityAsTrustedRoot(
	SSLContextRef 	ctx,
	CFArrayRef		identArray)
{
	CFIndex numItems = CFArrayGetCount(identArray);
	if(numItems == 0) {
		printf("***addIdentityAsTrustedRoot: empty identArray\n");
		return errSecParam;
	}
	
	/* Root should be the last item - could be identity, could be cert */
	CFTypeRef theItem = CFArrayGetValueAtIndex(identArray, numItems - 1);
	if(CFGetTypeID(theItem) == SecIdentityGetTypeID()) {
		/* identity */
		SecCertificateRef certRef;
		OSStatus ortn = SecIdentityCopyCertificate(
			(SecIdentityRef)theItem, &certRef);
		if(ortn) {
			cssmPerror("SecIdentityCopyCertificate", ortn);
			printf("***Error gettting cert from identity\n");
			return ortn;
		}
		ortn = addTrustedSecCert(ctx, certRef, false);
		CFRelease(certRef);
		return ortn;
	}
	else if(CFGetTypeID(theItem) == SecCertificateGetTypeID()) {
		/* certificate */
		return addTrustedSecCert(ctx, (SecCertificateRef)theItem, false);
	}
	else {
		printf("***Bogus item in identity array\n");
		return errSecParam;
	}
}
#else
OSStatus addIdentityAsTrustedRoot(
	SSLContextRef 	ctx,
	CFArrayRef		identArray)
{
	return errSecSuccess;
}   
#endif

/*
 * Lists of SSLCipherSuites used in sslSetCipherRestrictions. Note that the 
 * SecureTransport library does not implement all of these; we only specify
 * the ones it claims to support.
 */
const SSLCipherSuite suites40[] = {
	SSL_RSA_EXPORT_WITH_RC4_40_MD5,
	SSL_RSA_EXPORT_WITH_RC2_CBC_40_MD5,
	SSL_RSA_EXPORT_WITH_DES40_CBC_SHA,
	SSL_DH_DSS_EXPORT_WITH_DES40_CBC_SHA,
	SSL_DH_RSA_EXPORT_WITH_DES40_CBC_SHA,
	SSL_DHE_DSS_EXPORT_WITH_DES40_CBC_SHA,
	SSL_DHE_RSA_EXPORT_WITH_DES40_CBC_SHA,
	SSL_DH_anon_EXPORT_WITH_RC4_40_MD5,
	SSL_DH_anon_EXPORT_WITH_DES40_CBC_SHA,
	SSL_NO_SUCH_CIPHERSUITE
};
const SSLCipherSuite suitesDES[] = {
	SSL_RSA_WITH_DES_CBC_SHA,
	SSL_DH_DSS_WITH_DES_CBC_SHA,
	SSL_DH_RSA_WITH_DES_CBC_SHA,
	SSL_DHE_DSS_WITH_DES_CBC_SHA,
	SSL_DHE_RSA_WITH_DES_CBC_SHA,
	SSL_DH_anon_WITH_DES_CBC_SHA,
	SSL_RSA_WITH_DES_CBC_MD5,
	SSL_NO_SUCH_CIPHERSUITE
};
const SSLCipherSuite suitesDES40[] = {
	SSL_RSA_EXPORT_WITH_DES40_CBC_SHA,
	SSL_DH_DSS_EXPORT_WITH_DES40_CBC_SHA,
	SSL_DH_RSA_EXPORT_WITH_DES40_CBC_SHA,
	SSL_DHE_DSS_EXPORT_WITH_DES40_CBC_SHA,
	SSL_DHE_RSA_EXPORT_WITH_DES40_CBC_SHA,
	SSL_DH_anon_EXPORT_WITH_DES40_CBC_SHA,
	SSL_NO_SUCH_CIPHERSUITE
};
const SSLCipherSuite suites3DES[] = {
	SSL_RSA_WITH_3DES_EDE_CBC_SHA,
	SSL_DH_DSS_WITH_3DES_EDE_CBC_SHA,
	SSL_DH_RSA_WITH_3DES_EDE_CBC_SHA,
	SSL_DHE_DSS_WITH_3DES_EDE_CBC_SHA,
	SSL_DHE_RSA_WITH_3DES_EDE_CBC_SHA,
	SSL_DH_anon_WITH_3DES_EDE_CBC_SHA,
	SSL_RSA_WITH_3DES_EDE_CBC_MD5,
	SSL_NO_SUCH_CIPHERSUITE
};
const SSLCipherSuite suitesRC4[] = {
	SSL_RSA_WITH_RC4_128_MD5,
	SSL_RSA_WITH_RC4_128_SHA,
	SSL_DH_anon_WITH_RC4_128_MD5,
	SSL_NO_SUCH_CIPHERSUITE
};
const SSLCipherSuite suitesRC4_40[] = {
	SSL_RSA_EXPORT_WITH_RC4_40_MD5,
	SSL_DH_anon_EXPORT_WITH_RC4_40_MD5,
	SSL_NO_SUCH_CIPHERSUITE
};
const SSLCipherSuite suitesRC2[] = {
	SSL_RSA_WITH_RC2_CBC_MD5,
	SSL_RSA_EXPORT_WITH_RC2_CBC_40_MD5,
	SSL_NO_SUCH_CIPHERSUITE
};
const SSLCipherSuite suitesAES128[] = {
	TLS_RSA_WITH_AES_128_CBC_SHA,
	TLS_DH_DSS_WITH_AES_128_CBC_SHA,
	TLS_DH_RSA_WITH_AES_128_CBC_SHA,
	TLS_DHE_DSS_WITH_AES_128_CBC_SHA,
	TLS_DHE_RSA_WITH_AES_128_CBC_SHA,
	TLS_DH_anon_WITH_AES_128_CBC_SHA,
    SSL_NO_SUCH_CIPHERSUITE
};
const SSLCipherSuite suitesAES256[] = {
	TLS_RSA_WITH_AES_256_CBC_SHA,
	TLS_DH_DSS_WITH_AES_256_CBC_SHA,
	TLS_DH_RSA_WITH_AES_256_CBC_SHA,
	TLS_DHE_DSS_WITH_AES_256_CBC_SHA,
	TLS_DHE_RSA_WITH_AES_256_CBC_SHA,
	TLS_DH_anon_WITH_AES_256_CBC_SHA,
    SSL_NO_SUCH_CIPHERSUITE
};
const SSLCipherSuite suitesDH[] = {
    SSL_DH_DSS_WITH_DES_CBC_SHA,
    SSL_DH_DSS_WITH_3DES_EDE_CBC_SHA,
    SSL_DH_DSS_EXPORT_WITH_DES40_CBC_SHA,
    SSL_DH_RSA_WITH_DES_CBC_SHA,
    SSL_DH_RSA_WITH_3DES_EDE_CBC_SHA,
    SSL_DH_RSA_EXPORT_WITH_DES40_CBC_SHA,
    SSL_DHE_DSS_WITH_DES_CBC_SHA,
    SSL_DHE_DSS_WITH_3DES_EDE_CBC_SHA,
    SSL_DHE_RSA_EXPORT_WITH_DES40_CBC_SHA,
    SSL_DHE_RSA_WITH_DES_CBC_SHA,
    SSL_DHE_RSA_WITH_3DES_EDE_CBC_SHA,
    SSL_DHE_DSS_EXPORT_WITH_DES40_CBC_SHA,
    SSL_DH_anon_WITH_RC4_128_MD5,
    SSL_DH_anon_WITH_DES_CBC_SHA,
    SSL_DH_anon_WITH_3DES_EDE_CBC_SHA,
    SSL_DH_anon_EXPORT_WITH_RC4_40_MD5,
    SSL_DH_anon_EXPORT_WITH_DES40_CBC_SHA,
	TLS_DH_DSS_WITH_AES_128_CBC_SHA,
	TLS_DH_RSA_WITH_AES_128_CBC_SHA,
	TLS_DHE_DSS_WITH_AES_128_CBC_SHA,
	TLS_DHE_RSA_WITH_AES_128_CBC_SHA,
	TLS_DH_anon_WITH_AES_128_CBC_SHA,
	TLS_DH_DSS_WITH_AES_256_CBC_SHA,
	TLS_DH_RSA_WITH_AES_256_CBC_SHA,
	TLS_DHE_DSS_WITH_AES_256_CBC_SHA,
	TLS_DHE_RSA_WITH_AES_256_CBC_SHA,
	TLS_DH_anon_WITH_AES_256_CBC_SHA,
	SSL_NO_SUCH_CIPHERSUITE
};
const SSLCipherSuite suitesDHAnon[] = {
    SSL_DH_anon_WITH_RC4_128_MD5,
    SSL_DH_anon_WITH_DES_CBC_SHA,
    SSL_DH_anon_WITH_3DES_EDE_CBC_SHA,
    SSL_DH_anon_EXPORT_WITH_RC4_40_MD5,
    SSL_DH_anon_EXPORT_WITH_DES40_CBC_SHA,
	TLS_DH_anon_WITH_AES_128_CBC_SHA,
	TLS_DH_anon_WITH_AES_256_CBC_SHA,
	SSL_NO_SUCH_CIPHERSUITE
};
const SSLCipherSuite suitesDH_RSA[] = {
    SSL_DH_RSA_WITH_DES_CBC_SHA,
    SSL_DH_RSA_WITH_3DES_EDE_CBC_SHA,
    SSL_DHE_RSA_WITH_DES_CBC_SHA,
    SSL_DHE_RSA_WITH_3DES_EDE_CBC_SHA,
    SSL_DH_RSA_EXPORT_WITH_DES40_CBC_SHA,
    SSL_DHE_RSA_EXPORT_WITH_DES40_CBC_SHA,
	TLS_DH_RSA_WITH_AES_128_CBC_SHA,
	TLS_DHE_RSA_WITH_AES_128_CBC_SHA,
	TLS_DH_RSA_WITH_AES_256_CBC_SHA,
	TLS_DHE_RSA_WITH_AES_256_CBC_SHA,
	SSL_NO_SUCH_CIPHERSUITE
};
const SSLCipherSuite suitesDH_DSS[] = {
    SSL_DH_DSS_WITH_DES_CBC_SHA,
    SSL_DH_DSS_WITH_3DES_EDE_CBC_SHA,
    SSL_DHE_DSS_WITH_DES_CBC_SHA,
    SSL_DHE_DSS_WITH_3DES_EDE_CBC_SHA,
    SSL_DH_DSS_EXPORT_WITH_DES40_CBC_SHA,
    SSL_DHE_DSS_EXPORT_WITH_DES40_CBC_SHA,
	TLS_DH_DSS_WITH_AES_128_CBC_SHA,
	TLS_DHE_DSS_WITH_AES_128_CBC_SHA,
	TLS_DH_DSS_WITH_AES_256_CBC_SHA,
	TLS_DHE_DSS_WITH_AES_256_CBC_SHA,
	SSL_NO_SUCH_CIPHERSUITE
};
const SSLCipherSuite suites_SHA1[] = {
	SSL_RSA_WITH_RC4_128_SHA,
	SSL_RSA_EXPORT_WITH_DES40_CBC_SHA,
	SSL_RSA_WITH_IDEA_CBC_SHA,
	SSL_RSA_EXPORT_WITH_DES40_CBC_SHA,
	SSL_RSA_WITH_DES_CBC_SHA,
	SSL_RSA_WITH_3DES_EDE_CBC_SHA,
	SSL_DH_DSS_EXPORT_WITH_DES40_CBC_SHA,
	SSL_DH_DSS_WITH_DES_CBC_SHA,
	SSL_DH_DSS_WITH_3DES_EDE_CBC_SHA,
	SSL_DH_RSA_EXPORT_WITH_DES40_CBC_SHA,
	SSL_DH_RSA_WITH_DES_CBC_SHA,
	SSL_DH_RSA_WITH_3DES_EDE_CBC_SHA,
	SSL_DHE_DSS_EXPORT_WITH_DES40_CBC_SHA,
	SSL_DHE_DSS_WITH_DES_CBC_SHA,
	SSL_DHE_DSS_WITH_3DES_EDE_CBC_SHA,
	SSL_DHE_RSA_EXPORT_WITH_DES40_CBC_SHA,
	SSL_DHE_RSA_WITH_DES_CBC_SHA,
	SSL_DHE_RSA_WITH_3DES_EDE_CBC_SHA,
	SSL_DH_anon_EXPORT_WITH_DES40_CBC_SHA,
	SSL_DH_anon_WITH_DES_CBC_SHA,
	SSL_DH_anon_WITH_3DES_EDE_CBC_SHA,
	SSL_FORTEZZA_DMS_WITH_NULL_SHA,
	SSL_FORTEZZA_DMS_WITH_FORTEZZA_CBC_SHA,
	TLS_RSA_WITH_AES_128_CBC_SHA,
	TLS_DH_DSS_WITH_AES_128_CBC_SHA,
	TLS_DH_RSA_WITH_AES_128_CBC_SHA,
	TLS_DHE_DSS_WITH_AES_128_CBC_SHA,
	TLS_DHE_RSA_WITH_AES_128_CBC_SHA,
	TLS_DH_anon_WITH_AES_128_CBC_SHA,
	TLS_RSA_WITH_AES_256_CBC_SHA,
	TLS_DH_DSS_WITH_AES_256_CBC_SHA,
	TLS_DH_RSA_WITH_AES_256_CBC_SHA,
	TLS_DHE_DSS_WITH_AES_256_CBC_SHA,
	TLS_DHE_RSA_WITH_AES_256_CBC_SHA,
	TLS_DH_anon_WITH_AES_256_CBC_SHA,
	SSL_NO_SUCH_CIPHERSUITE
};
const SSLCipherSuite suites_MD5[] = {
	SSL_RSA_EXPORT_WITH_RC4_40_MD5,
	SSL_RSA_WITH_RC4_128_MD5,
	SSL_RSA_EXPORT_WITH_RC2_CBC_40_MD5,
	SSL_DH_anon_EXPORT_WITH_RC4_40_MD5,
	SSL_DH_anon_WITH_RC4_128_MD5,
	SSL_NO_SUCH_CIPHERSUITE
};


/*
 * Given an SSLContextRef and an array of SSLCipherSuites, terminated by
 * SSL_NO_SUCH_CIPHERSUITE, select those SSLCipherSuites which the library
 * supports and do a SSLSetEnabledCiphers() specifying those. 
 */
OSStatus sslSetEnabledCiphers(
	SSLContextRef ctx,
	const SSLCipherSuite *ciphers)
{
	size_t numSupported;
	OSStatus ortn;
	SSLCipherSuite *supported = NULL;
	SSLCipherSuite *enabled = NULL;
	unsigned enabledDex = 0;	// index into enabled
	unsigned supportedDex = 0;	// index into supported
	unsigned inDex = 0;			// index into ciphers
	
	/* first get all the supported ciphers */
	ortn = SSLGetNumberSupportedCiphers(ctx, &numSupported);
	if(ortn) {
		printSslErrStr("SSLGetNumberSupportedCiphers", ortn);
		return ortn;
	}
	supported = (SSLCipherSuite *)malloc(numSupported * sizeof(SSLCipherSuite));
	ortn = SSLGetSupportedCiphers(ctx, supported, &numSupported);
	if(ortn) {
		printSslErrStr("SSLGetSupportedCiphers", ortn);
		return ortn;
	}
	
	/* 
	 * Malloc an array we'll use for SSLGetEnabledCiphers - this will  be
	 * bigger than the number of suites we actually specify 
	 */
	enabled = (SSLCipherSuite *)malloc(numSupported * sizeof(SSLCipherSuite));
	
	/* 
	 * For each valid suite in ciphers, see if it's in the list of 
	 * supported ciphers. If it is, add it to the list of ciphers to be
	 * enabled. 
	 */
	for(inDex=0; ciphers[inDex] != SSL_NO_SUCH_CIPHERSUITE; inDex++) {
		for(supportedDex=0; supportedDex<numSupported; supportedDex++) {
			if(ciphers[inDex] == supported[supportedDex]) {
				enabled[enabledDex++] = ciphers[inDex];
				break;
			}
		}
	}
	
	/* send it on down. */
	ortn = SSLSetEnabledCiphers(ctx, enabled, enabledDex);
	if(ortn) {
		printSslErrStr("SSLSetEnabledCiphers", ortn);
	}
	free(enabled);
	free(supported);
	return ortn;
}

/*
 * Specify a restricted set of cipherspecs.
 */
OSStatus sslSetCipherRestrictions(
	SSLContextRef ctx,
	char cipherRestrict)
{
	OSStatus ortn;
	
	if(cipherRestrict == '\0') {
		return errSecSuccess;		// actually should not have been called 
	}
	switch(cipherRestrict) {
		case 'e':
			ortn = sslSetEnabledCiphers(ctx, suites40);
			break;
		case 'd':
			ortn = sslSetEnabledCiphers(ctx, suitesDES);
			break;
		case 'D':
			ortn = sslSetEnabledCiphers(ctx, suitesDES40);
			break;
		case '3':
			ortn = sslSetEnabledCiphers(ctx, suites3DES);
			break;
		case '4':
			ortn = sslSetEnabledCiphers(ctx, suitesRC4);
			break;
		case '$':
			ortn = sslSetEnabledCiphers(ctx, suitesRC4_40);
			break;
		case '2':
			ortn = sslSetEnabledCiphers(ctx, suitesRC2);
			break;
		case 'a':
			ortn = sslSetEnabledCiphers(ctx, suitesAES128);
			break;
		case 'A':
			ortn = sslSetEnabledCiphers(ctx, suitesAES256);
			break;
		case 'h':
			ortn = sslSetEnabledCiphers(ctx, suitesDH);
			break;
		case 'H':
			ortn = sslSetEnabledCiphers(ctx, suitesDHAnon);
			break;
		case 'r':
			ortn = sslSetEnabledCiphers(ctx, suitesDH_RSA);
			break;
		case 's':
			ortn = sslSetEnabledCiphers(ctx, suitesDH_DSS);
			break;
		default:
			printf("***bad cipherSpec***\n");
			exit(1);
	}
	return ortn;
}

#if 0
int sslVerifyClientCertState(
	char						*whichSide,		// "client" or "server"
	SSLClientCertificateState	expectState,
	SSLClientCertificateState	gotState)
{
	if(expectState == SSL_CLIENT_CERT_IGNORE) {
		/* app says "don't bopther checking" */
		return 0;
	}
	if(expectState == gotState) {
		return 0;
	}
	printf("***%s: Expected clientCertState %s; got %s\n", whichSide,
		sslGetClientCertStateString(expectState),
		sslGetClientCertStateString(gotState));
	return 1;
}

int sslVerifyRtn(
	char		*whichSide,		// "client" or "server"
	OSStatus	expectRtn,
	OSStatus	gotRtn)		
{
	if(expectRtn == gotRtn) {
		return 0;
	}
	printf("***%s: Expected return %s; got %s\n", whichSide,
		sslGetSSLErrString(expectRtn),
		sslGetSSLErrString(gotRtn));
	return 1;
}

int sslVerifyProtVers(
	char		*whichSide,		// "client" or "server"
	SSLProtocol	expectProt,
	SSLProtocol	gotProt)		
{
	if(expectProt == SSL_PROTOCOL_IGNORE) {
		/* app says "don't bopther checking" */
		return 0;
	}
	if(expectProt == gotProt) {
		return 0;
	}
	printf("***%s: Expected return %s; got %s\n", whichSide,
		sslGetProtocolVersionString(expectProt),
		sslGetProtocolVersionString(gotProt));
	return 1;
}

int sslVerifyCipher(
	char			*whichSide,		// "client" or "server"
	SSLCipherSuite	expectCipher,
	SSLCipherSuite	gotCipher)		
{
	if(expectCipher == SSL_CIPHER_IGNORE) {
		/* app says "don't bopther checking" */
		return 0;
	}
	if(expectCipher == gotCipher) {
		return 0;
	}
	printf("***%s: Expected return %s; got %s\n", whichSide,
		sslGetCipherSuiteString(expectCipher),
		sslGetCipherSuiteString(gotCipher));
	return 1;
}


OSStatus sslSetProtocols(
	SSLContextRef 	ctx,
	const char		*acceptedProts,
	SSLProtocol		tryVersion)			// only used if acceptedProts NULL
{
	OSStatus ortn;
	
	if(acceptedProts) {
		#if JAGUAR_BUILD
		printf("***SSLSetProtocolVersionEnabled not supported in this config.\n");
		exit(1);
		#endif
		ortn = SSLSetProtocolVersionEnabled(ctx, kSSLProtocolAll, false);
		if(ortn) {
			printSslErrStr("SSLSetProtocolVersionEnabled(all off)", ortn);
			return ortn;
		}
		for(const char *cp = acceptedProts; *cp; cp++) {
			SSLProtocol prot;
			switch(*cp) {
				case '2':
					prot = kSSLProtocol2;
					break;
				case '3':
					prot = kSSLProtocol3;
					break;
				case 't':
					prot = kTLSProtocol1;
					break;
				default:
					printf("***BRRZAP! Bad acceptedProts string %s. Aborting.\n", acceptedProts);
					exit(1);
			}
			ortn = SSLSetProtocolVersionEnabled(ctx, prot, true);
			if(ortn) {
				printSslErrStr("SSLSetProtocolVersionEnabled", ortn);
				return ortn;
			}
		}
	}
	else {
		ortn = SSLSetProtocolVersion(ctx, tryVersion);
		if(ortn) {
			printSslErrStr("SSLSetProtocolVersion", ortn);
			return ortn;
		} 
	}
	return errSecSuccess;
}

void sslShowResult(
	char				*whichSide,		// "client" or "server"
	SslAppTestParams	*params)
{
	printf("%s status:\n", whichSide);
	if(params->acceptedProts) {
		printf("   Allowed SSL versions   : %s\n", params->acceptedProts);
	}
	else {
		printf("   Attempted  SSL version : %s\n", 
			sslGetProtocolVersionString(params->tryVersion));
	}
	printf("   Result                 : %s\n", sslGetSSLErrString(params->ortn));
	printf("   Negotiated SSL version : %s\n", 
		sslGetProtocolVersionString(params->negVersion));
	printf("   Negotiated CipherSuite : %s\n",
		sslGetCipherSuiteString(params->negCipher));
	if(params->certState != kSSLClientCertNone) {
		printf("   Client Cert State      : %s\n",
			sslGetClientCertStateString(params->certState));
	}
}
#endif

/* print a '.' every few seconds to keep UI alive while connecting */
static time_t lastTime = (time_t)0;
#define TIME_INTERVAL		3

void sslOutputDot()
{
	time_t thisTime = time(0);
	
	if((thisTime - lastTime) >= TIME_INTERVAL) {
		printf("."); fflush(stdout);
		lastTime = thisTime;
	}
}

#if 0
/* main server pthread body */
static void *sslServerThread(void *arg)
{
	SslAppTestParams *testParams = (SslAppTestParams *)arg;
	OSStatus status;
	
	status = sslAppServe(testParams);
	pthread_exit((void*)status);
	/* NOT REACHED */
	return (void *)status;
}

/*
 * Run one session, with the server in a separate thread.
 * On entry, serverParams->port is the port we attempt to run on;
 * the server thread may overwrite that with a different port if it's 
 * unable to open the port we specify. Whatever is left in 
 * serverParams->port is what's used for the client side. 
 */
#define CLIENT_WAIT_SECONDS		1
int sslRunSession(
	SslAppTestParams*serverParams,
	SslAppTestParams *clientParams,
	const char 		*testDesc)
{
	pthread_t serverPthread;
	OSStatus clientRtn;
	void *serverRtn;
	
	if(testDesc && !clientParams->quiet) {
		printf("===== %s =====\n", testDesc);
	}
	
	if(pthread_mutex_init(&serverParams->pthreadMutex, NULL)) {
		printf("***Error initializing mutex; aborting.\n");
		return -1;
	}
	if(pthread_cond_init(&serverParams->pthreadCond, NULL)) {
		printf("***Error initializing pthreadCond; aborting.\n");
		return -1;
	}
	serverParams->serverReady = false;		// server sets true
	
	int result = pthread_create(&serverPthread, NULL, 
			sslServerThread, serverParams);
	if(result) {
		printf("***Error starting up server thread; aborting.\n");
		return result;
	}
	
	/* wait for server to set up a socket we can connect to */
	if(pthread_mutex_lock(&serverParams->pthreadMutex)) {
		printf("***Error acquiring server lock; aborting.\n");
		return -1;
	}
	while(!serverParams->serverReady) {
		if(pthread_cond_wait(&serverParams->pthreadCond, &serverParams->pthreadMutex)) {
			printf("***Error waiting server thread; aborting.\n");
			return -1;
		}
	}
	pthread_mutex_unlock(&serverParams->pthreadMutex);
	pthread_cond_destroy(&serverParams->pthreadCond);
	pthread_mutex_destroy(&serverParams->pthreadMutex);
	
	clientParams->port = serverParams->port;
	clientRtn = sslAppClient(clientParams);
	/* server doesn't shut down its socket until it sees this */
	serverParams->clientDone = 1;
	result = pthread_join(serverPthread, &serverRtn);
	if(result) {
		printf("***pthread_join returned %d, aborting\n", result);
		return result;
	}
	
	if(serverParams->verbose) {
		sslShowResult("server", serverParams);
	}
	if(clientParams->verbose) {
		sslShowResult("client", clientParams);
	}
	
	/* verify results */
	int ourRtn = 0;
	ourRtn += sslVerifyRtn("server", serverParams->expectRtn, serverParams->ortn);
	ourRtn += sslVerifyRtn("client", clientParams->expectRtn, clientParams->ortn);
	ourRtn += sslVerifyProtVers("server", serverParams->expectVersion, 
		serverParams->negVersion);
	ourRtn += sslVerifyProtVers("client", clientParams->expectVersion, 
		clientParams->negVersion);
	ourRtn += sslVerifyClientCertState("server", serverParams->expectCertState, 
		serverParams->certState);
	ourRtn += sslVerifyClientCertState("client", clientParams->expectCertState, 
		clientParams->certState);
	if(serverParams->ortn == errSecSuccess) {
		ourRtn += sslVerifyCipher("server", serverParams->expectCipher, 
			serverParams->negCipher);
	}
	if(clientParams->ortn == errSecSuccess) {
		ourRtn += sslVerifyCipher("client", clientParams->expectCipher, 
			clientParams->negCipher);
	}
	return ourRtn;
}

static bool isCertRoot(
	SecCertificateRef cert)
{
	/* FIXME - per Radar 3247491, the Sec-level functions we'd like to use for this
	 * haven't been written yet...
	CSSM_X509_NAME subject;
	CSSM_X509_NAME issuer;
	OSStatus ortn;
	... */
	return true;
}

/*
 * Add all of the roots in a given KC to SSL ctx's trusted anchors.
 */
OSStatus sslAddTrustedRoots(
	SSLContextRef 	ctx,
	SecKeychainRef	keychain,
	bool			*foundOne)		// RETURNED, true if we found 
									//    at least one root cert
{
	OSStatus 				ortn;
	SecCertificateRef 		secCert;
	SecKeychainSearchRef 	srch;
	
	*foundOne = false;
	ortn = SecKeychainSearchCreateFromAttributes(keychain,
		kSecCertificateItemClass,
		NULL,			// any attrs
		&srch);
	if(ortn) {
		printSslErrStr("SecKeychainSearchCreateFromAttributes", ortn);
		return ortn;
	}
	
	/*
	 * Only use root certs. Not an error if we don't find any.
	 */
	do {
		ortn = SecKeychainSearchCopyNext(srch, 
			(SecKeychainItemRef *)&secCert);
		if(ortn) {
			break;
		}
		
		/* see if it's a root */
		if(!isCertRoot(secCert)) {
			continue;
		}
		
		/* Tell Secure Transport to trust this one. */
		ortn = addTrustedSecCert(ctx, secCert, false);
		if(ortn) {
			/* fatal */
			printSslErrStr("addTrustedSecCert", ortn);
			return ortn;
		}
		CFRelease(secCert);
		*foundOne = true;
	} while(ortn == errSecSuccess);
	CFRelease(srch);
	return errSecSuccess;
}

/*
 * Wrapper for sslIdentPicker, with optional trusted anchor specified as a filename.
 */
OSStatus sslIdentityPicker(
	SecKeychainRef		kcRef,			// NULL means use default list
	const char			*trustedAnchor,	// optional additional trusted anchor
	bool				includeRoot,	// true --> root is appended to outArray
										// false --> root not included
	CFArrayRef			*outArray)		// created and RETURNED
{
	SecCertificateRef trustedCert = NULL;
	OSStatus ortn;
	
	if(trustedAnchor) { 
		ortn = sslReadAnchor(trustedAnchor, &trustedCert);
		if(ortn) {
			printf("***Error reading %s. sslIdentityPicker proceeding with no anchor.\n",
				trustedAnchor);
			trustedCert = NULL;
		}
	}
	ortn = sslIdentPicker(kcRef, trustedCert, includeRoot, outArray);
	if(trustedCert) {
		CFRelease(trustedCert);
	}
	return ortn;
}

/*
 * Given a keychain name, convert it into a full path using the "SSL regression 
 * test suite algorithm". The Sec layer by default locates root root's keychains
 * in different places depending on whether we're actually logged in as root
 * or running via e.g. cron, so we force the location of root keychains to 
 * a hard-coded path. User keychain names we leave alone.
 */
void sslKeychainPath(
	const char *kcName,
	char *kcPath)			// allocd by caller, MAXPATHLEN
{
	if(kcName[0] == '\0') {
		kcPath[0] = '\0';
	}
	else if(geteuid() == 0) {
		/* root */
		sprintf(kcPath, "/Library/Keychains/%s", kcName);
	}
	else {
		/* user, leave alone */
		strcpy(kcPath, kcName);
	}
}

/* Verify presence of required file. Returns nonzero if not found. */
int sslCheckFile(const char *path)
{
	struct stat sb;

	if(stat(path, &sb)) {
		printf("***Can't find file %s.\n", path);
		printf("   Try running in the build directory, perhaps after running the\n"
			   "   makeLocalCert script.\n");
		return 1;
	}
	return 0;
}

#endif

/* Stringify a SSL_ECDSA_NamedCurve */
extern const char *sslCurveString(
	SSL_ECDSA_NamedCurve namedCurve)
{
	static char unk[100];

	switch(namedCurve) {
		case SSL_Curve_None:	  return "Curve_None";
		case SSL_Curve_secp256r1: return "secp256r1";
		case SSL_Curve_secp384r1: return "secp384r1";
		case SSL_Curve_secp521r1: return "secp521r1";
		default:
			sprintf(unk, "Unknown <%d>", (int)namedCurve);
			return unk;
	}
}
