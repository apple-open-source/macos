/*
 * tpUtils.h - TP and cert group test support
 */

#ifndef	_TP_UTILS_H_
#define _TP_UTILS_H_

#include <Security/cssmtype.h>
#include <Security/x509defs.h>
#include <Security/cssmapple.h>
#include <time.h>
#include <MacTypes.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define TP_DB_ENABLE	1

/*
 * Given an array of certs and an uninitialized CSSM_CERTGROUP, place the
 * certs into the certgroup and optionally into one of a list of DBs in 
 * random order. Optionaly the first cert in the array is placed in the 
 * first element of certgroup. Only error is memory error. It's legal to 
 * pass in an empty cert array. 
 */
CSSM_RETURN tpMakeRandCertGroup(
	CSSM_CL_HANDLE			clHand,
	CSSM_DL_DB_LIST_PTR		dbList,
	const CSSM_DATA_PTR		certs,
	unsigned				numCerts,
	CSSM_CERTGROUP_PTR		certGroup,
	CSSM_BOOL				firstCertIsSubject,	// true: certs[0] goes to head 
												//   of certGroup
	CSSM_BOOL				verbose,
	CSSM_BOOL				allInDbs,			// all certs go to DBs
	CSSM_BOOL				skipFirstDb);		// no certs go to db[0]
	
CSSM_RETURN tpStoreCert(
	CSSM_DL_DB_HANDLE		dlDb,
	const CSSM_DATA_PTR		cert,
	/* REQUIRED fields */
	CSSM_CERT_TYPE			certType,		// e.g. CSSM_CERT_X_509v3
	uint32					serialNum,
	const CSSM_DATA			*issuer,		// (shouldn't this be subject?)
											// normalized & encoded
	/* OPTIONAL fields */
	CSSM_CERT_ENCODING		certEncoding,	// e.g. CSSM_CERT_ENCODING_DER
	const CSSM_DATA			*printName,
	const CSSM_DATA			*subject);		// normalized & encoded
	
/*
 * Store a cert when we don't already know the required fields. We'll 
 * extract them.
 */
CSSM_RETURN tpStoreRawCert(
	CSSM_DL_DB_HANDLE		dlDb,
	CSSM_CL_HANDLE			clHand,
	const CSSM_DATA_PTR		cert);

/* 
 * Generate numKeyPairs key pairs of specified algorithm and size.
 * Key labels will be 'keyLabelBase' concatenated with a 4-digit
 * decimal number.
 */
CSSM_RETURN tpGenKeys(
	CSSM_CSP_HANDLE	cspHand,
	CSSM_DL_DB_HANDLE dbHand,			/* keys go here */
	unsigned		numKeyPairs,
	uint32			keyGenAlg,		/* CSSM_ALGID_RSA, etc. */
	uint32			keySizeInBits,			
	const char 		*keyLabelBase,	/* C string */
	CSSM_KEY_PTR	pubKeys,		/* array of keys RETURNED here */
	CSSM_KEY_PTR	privKeys,		/* array of keys RETURNED here */
	CSSM_DATA_PTR	paramData = NULL);	// optional DSA params

/* 
 * Generate a cert chain using specified key pairs. The last cert in the
 * chain (certs[numCerts-1]) is a root cert, self-signed. 
 */
CSSM_RETURN tpGenCerts(
	CSSM_CSP_HANDLE	cspHand,
	CSSM_CL_HANDLE	clHand,
	unsigned		numCerts,
	uint32			sigAlg,			/* CSSM_ALGID_SHA1WithRSA, etc. */
	const char 		*nameBase,		/* C string */
	CSSM_KEY_PTR	pubKeys,		/* array of public keys */
	CSSM_KEY_PTR	privKeys,		/* array of private keys */
	CSSM_DATA_PTR	certs,			/* array of certs RETURNED here */
	const char		*notBeforeStr,	/* from genTimeAtNowPlus() */
	const char		*notAfterStr);	/* from genTimeAtNowPlus() */

/* 
 * Generate a cert chain using specified key pairs. The last cert in the
 * chain (certs[numCerts-1]) is a root cert, self-signed. Store
 * the certs indicated by corresponding element on storeArray. If 
 * storeArray[n].DLHandle == 0, the cert is not stored. 
 */
CSSM_RETURN tpGenCertsStore(
	CSSM_CSP_HANDLE		cspHand,
	CSSM_CL_HANDLE		clHand,
	unsigned			numCerts,
	uint32				sigAlg,			/* CSSM_ALGID_SHA1WithRSA, etc. */
	const char 			*nameBase,		/* C string */
	CSSM_KEY_PTR		pubKeys,		/* array of public keys */
	CSSM_KEY_PTR		privKeys,		/* array of private keys */
	CSSM_DL_DB_HANDLE	*storeArray,	/* array of certs stored here  */
	CSSM_DATA_PTR		certs,			/* array of certs RETURNED here */
	const char			*notBeforeStr,	/* from genTimeAtNowPlus() */
	const char			*notAfterStr);	/* from genTimeAtNowPlus() */

/* free a CSSM_CERT_GROUP */
void tpFreeCertGroup(
	CSSM_CERTGROUP_PTR	certGroup,
	CSSM_BOOL	 		freeCertData,		// free individual CertList.Data 
	CSSM_BOOL			freeStruct);			// free the overall CSSM_CERTGROUP

CSSM_BOOL tpCompareCertGroups(
	const CSSM_CERTGROUP	*grp1,
	const CSSM_CERTGROUP	*grp2);

CSSM_RETURN clDeleteAllCerts(CSSM_DL_DB_HANDLE dlDb);

/*
 * Wrapper for CSSM_TP_CertGroupVerify. 
 */
CSSM_RETURN tpCertGroupVerify(
	CSSM_TP_HANDLE						tpHand,
	CSSM_CL_HANDLE						clHand,
	CSSM_CSP_HANDLE 					cspHand,
	CSSM_DL_DB_LIST_PTR					dbListPtr,
	const CSSM_OID						*policy,		// optional
	const CSSM_DATA						*fieldOpts,		// optional
	const CSSM_DATA						*actionData,	// optional
	void								*policyOpts,
	const CSSM_CERTGROUP 				*certGroup,
	CSSM_DATA_PTR						anchorCerts,
	unsigned							numAnchorCerts,
	CSSM_TP_STOP_ON						stopOn,		// CSSM_TP_STOP_ON_POLICY, etc.
	CSSM_TIMESTRING						cssmTimeStr,// optional
	CSSM_TP_VERIFY_CONTEXT_RESULT_PTR	result);	// RETURNED

CSSM_RETURN tpKcOpen(
	CSSM_DL_HANDLE		dlHand,
	const char			*kcName,
	const char			*pwd,				// optional to avoid UI	
	CSSM_BOOL			doCreate,
	CSSM_DB_HANDLE		*dbHand);			// RETURNED

CSSM_RETURN freeVfyResult(
	CSSM_TP_VERIFY_CONTEXT_RESULT *ctx);

void printCertInfo(
	unsigned numCerts,							// from CertGroup
	const CSSM_TP_APPLE_EVIDENCE_INFO *info);
	
void dumpVfyResult(
	const CSSM_TP_VERIFY_CONTEXT_RESULT *vfyResult);

/* 
 * Obtain system anchors in CF and in CSSM_DATA form.
 * Caller must CFRelease the returned rootArray and 
 * free() the returned CSSM_DATA array, but not its
 * contents - SecCertificates themselves own that.
 */
OSStatus getSystemAnchors(
	CFArrayRef *rootArray,	/* RETURNED */
	CSSM_DATA **anchors,	/* RETURNED */
	unsigned *numAnchors);	/* RETURNED */

/* get a SecCertificateRef from a file */
SecCertificateRef certFromFile(
	const char *fileName);

#ifdef	__cplusplus
}
#endif
#endif	/* _TP_UTILS_H_ */

