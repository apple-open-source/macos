/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
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


/*
	policies.cpp - TP module policy implementation

	Created 10/9/2000 by Doug Mitchell. 
*/

#include <Security/cssmtype.h>
#include <Security/cssmapi.h>
#include "tpPolicies.h"
#include <Security/oidsattr.h>
#include <Security/cssmerr.h>
#include "tpdebugging.h"
#include "rootCerts.h"
#include "certGroupUtils.h"
#include <Security/x509defs.h>
#include <Security/oidscert.h>
#include <Security/certextensions.h>
#include <Security/cssmapple.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

/* 
 * Our private per-extension info. One of these per (understood) extension per
 * cert. 
 */
typedef struct {
	CSSM_BOOL		present;
	CSSM_BOOL		critical;
	CE_Data       	*extnData;		// mallocd by CL
	CSSM_DATA		*valToFree;		// the data we pass to freeField()
} iSignExtenInfo;

/*
 * Struct to keep track of info pertinent to one cert.
 */
typedef struct {

	/* extensions pertinent to iSign */
	iSignExtenInfo		authorityId;
	iSignExtenInfo		subjectId;
	iSignExtenInfo		keyUsage;
	iSignExtenInfo		extendKeyUsage;
	iSignExtenInfo		basicConstraints;
	iSignExtenInfo		netscapeCertType;
				
	/* flag indicating presence of a critical extension we don't understand */
	CSSM_BOOL			foundUnknownCritical;
	
} iSignCertInfo;
 

/*
 * Setup a single iSignExtenInfo. Called once per known extension
 * per cert. 
 */
static CSSM_RETURN tpSetupExtension(
	CssmAllocator		&alloc, 
	CSSM_DATA 			*extnData,
	iSignExtenInfo		*extnInfo)		// which component of certInfo
{
	if(extnData->Length != sizeof(CSSM_X509_EXTENSION)) {
		errorLog0("tpSetupExtension: malformed CSSM_FIELD\n");
		return CSSMERR_TP_UNKNOWN_FORMAT;
	}
	CSSM_X509_EXTENSION *cssmExt = (CSSM_X509_EXTENSION *)extnData->Data;
	extnInfo->present   = CSSM_TRUE;
	extnInfo->critical  = cssmExt->critical;
	extnInfo->extnData  = (CE_Data *)cssmExt->value.parsedValue;
	extnInfo->valToFree = extnData;
	return CSSM_OK;
}

/*
 * Fetch a known extension, set up associated iSignExtenInfo if present.
 */
static CSSM_RETURN iSignFetchExtension(
	CssmAllocator		&alloc, 
	TPCertInfo			*tpCert,
	const CSSM_OID		*fieldOid,		// which extension to fetch
	iSignExtenInfo		*extnInfo)		// where the info goes
{
	CSSM_DATA_PTR	fieldValue;			// mallocd by CL
	CSSM_RETURN		crtn;
	
	crtn = tpCert->fetchField(fieldOid, &fieldValue);
	switch(crtn) {
		case CSSM_OK:
			break;
		case CSSMERR_CL_NO_FIELD_VALUES:
			/* field not present, OK */
			return CSSM_OK;
		default:
			return crtn;
	}
	return tpSetupExtension(alloc,
			fieldValue,
			extnInfo);
}

/*
 * Search for al unknown extensions. If we find one which is flagged critical, 
 * flag certInfo->foundUnknownCritical. Only returns error on gross errors.  
 */
static CSSM_RETURN iSignSearchUnknownExtensions(
	TPCertInfo			*tpCert,
	iSignCertInfo		*certInfo)
{
	CSSM_RETURN 	crtn;
	CSSM_DATA_PTR 	fieldValue = NULL;
	CSSM_HANDLE		searchHand = CSSM_INVALID_HANDLE;
	uint32 			numFields = 0;
	
	crtn = CSSM_CL_CertGetFirstCachedFieldValue(tpCert->clHand(),
		tpCert->cacheHand(),
		&CSSMOID_X509V3CertificateExtensionCStruct,
		&searchHand,
		&numFields, 
		&fieldValue);
	switch(crtn) {
		case CSSM_OK:
			/* found one, proceed */
			break;
		case CSSMERR_CL_NO_FIELD_VALUES:
			/* no unknown extensions present, OK */
			return CSSM_OK;
		default:
			return crtn;
	}
	
	if(fieldValue->Length != sizeof(CSSM_X509_EXTENSION)) {
		errorLog0("iSignSearchUnknownExtensions: malformed CSSM_FIELD\n");
		return CSSMERR_TP_UNKNOWN_FORMAT;
	}
	CSSM_X509_EXTENSION *cssmExt = (CSSM_X509_EXTENSION *)fieldValue->Data;
	if(cssmExt->critical) {
		/* BRRZAPP! Found an unknown extension marked critical */
		certInfo->foundUnknownCritical = CSSM_TRUE;
		goto fini;
	}
	CSSM_CL_FreeFieldValue(tpCert->clHand(), 
		&CSSMOID_X509V3CertificateExtensionCStruct, 
		fieldValue);
	fieldValue = NULL;
	
	/* process remaining unknown extensions */
	for(unsigned i=1; i<numFields; i++) {
		crtn = CSSM_CL_CertGetNextCachedFieldValue(tpCert->clHand(),
			searchHand,
			&fieldValue);
		if(crtn) {
			/* should never happen */
			errorLog0("searchUnknownExtensions: GetNextCachedFieldValue error\n");
			break;
		}
		if(fieldValue->Length != sizeof(CSSM_X509_EXTENSION)) {
			errorLog0("iSignSearchUnknownExtensions: malformed CSSM_FIELD\n");
			crtn = CSSMERR_TP_UNKNOWN_FORMAT;
			break;
		}
		CSSM_X509_EXTENSION *cssmExt = (CSSM_X509_EXTENSION *)fieldValue->Data;
		if(cssmExt->critical) {
			/* BRRZAPP! Found an unknown extension marked critical */
			certInfo->foundUnknownCritical = CSSM_TRUE;
			break;
		}
		CSSM_CL_FreeFieldValue(tpCert->clHand(), 
			&CSSMOID_X509V3CertificateExtensionCStruct, 
			fieldValue);
		fieldValue = NULL;
	} /* for additional fields */
				
fini:
	if(fieldValue) {
		CSSM_CL_FreeFieldValue(tpCert->clHand(), 
			&CSSMOID_X509V3CertificateExtensionCStruct, 
			fieldValue);
	}
	if(searchHand != CSSM_INVALID_HANDLE) {
		CSSM_CL_CertAbortQuery(tpCert->clHand(), searchHand);
	}
	return crtn;
}
/*
 * Given a TPCertInfo, fetch the associated iSignCertInfo fields. 
 * Returns CSSM_FAIL on error. 
 */
static CSSM_RETURN iSignGetCertInfo(
	CssmAllocator		&alloc, 
	TPCertInfo			*tpCert,
	iSignCertInfo		*certInfo)
{
	CSSM_RETURN			crtn;
	
	/* first grind thru the extensions we're interested in */
	crtn = iSignFetchExtension(alloc,
		tpCert,
		&CSSMOID_AuthorityKeyIdentifier,
		&certInfo->authorityId);
	if(crtn) {
		return crtn;
	}
	crtn = iSignFetchExtension(alloc,
		tpCert,
		&CSSMOID_SubjectKeyIdentifier,
		&certInfo->subjectId);
	if(crtn) {
		return crtn;
	}
	crtn = iSignFetchExtension(alloc,
		tpCert,
		&CSSMOID_KeyUsage,
		&certInfo->keyUsage);
	if(crtn) {
		return crtn;
	}
	crtn = iSignFetchExtension(alloc,
		tpCert,
		&CSSMOID_ExtendedKeyUsage,
		&certInfo->extendKeyUsage);
	if(crtn) {
		return crtn;
	}
	crtn = iSignFetchExtension(alloc,
		tpCert,
		&CSSMOID_BasicConstraints,
		&certInfo->basicConstraints);
	if(crtn) {
		return crtn;
	}
	crtn = iSignFetchExtension(alloc,
		tpCert,
		&CSSMOID_NetscapeCertType,
		&certInfo->netscapeCertType);
	if(crtn) {
		return crtn;
	}

	/* now look for extensions we don't understand - the only thing we're interested
	 * in is the critical flag. */
	return iSignSearchUnknownExtensions(tpCert, certInfo);
}

/*
 * Free (via CL) the fields allocated in iSignGetCertInfo().
 */
static void iSignFreeCertInfo(
	CSSM_CL_HANDLE	clHand,
	iSignCertInfo	*certInfo)
{
	if(certInfo->authorityId.present) {
		CSSM_CL_FreeFieldValue(clHand, &CSSMOID_AuthorityKeyIdentifier, 
			certInfo->authorityId.valToFree);
	}
	if(certInfo->subjectId.present) {
		CSSM_CL_FreeFieldValue(clHand, &CSSMOID_SubjectKeyIdentifier, 
			certInfo->subjectId.valToFree);
	}
	if(certInfo->keyUsage.present) {
		CSSM_CL_FreeFieldValue(clHand, &CSSMOID_KeyUsage, 
			certInfo->keyUsage.valToFree);
	}
	if(certInfo->extendKeyUsage.present) {
		CSSM_CL_FreeFieldValue(clHand, &CSSMOID_ExtendedKeyUsage, 
			certInfo->extendKeyUsage.valToFree);
	}
	if(certInfo->basicConstraints.present) {
		CSSM_CL_FreeFieldValue(clHand, &CSSMOID_BasicConstraints, 
			certInfo->basicConstraints.valToFree);
	}
	if(certInfo->netscapeCertType.present) {
		CSSM_CL_FreeFieldValue(clHand, &CSSMOID_NetscapeCertType, 
			certInfo->netscapeCertType.valToFree);
	}
}

#if 	TP_ROOT_CERT_ENABLE
/*
 * Common code for comparing a root to a list of known embedded roots.
 */
static CSSM_BOOL tp_isKnownRootCert(
	TPCertInfo				*rootCert,		// raw cert to compare
	const tpRootCert		*knownRoots,
	unsigned				numKnownRoots)
{
	const CSSM_DATA		*subjectName = NULL;
	CSSM_DATA_PTR		publicKey = NULL;
	unsigned			dex;
	CSSM_BOOL			brtn = CSSM_FALSE;
	CSSM_DATA_PTR		valToFree = NULL;
	
	subjectName = rootCert->subjectName();
	publicKey = tp_CertGetPublicKey(rootCert, &valToFree);	
	if(publicKey == NULL) {
		errorLog0("tp_isKnownRootCert: error retrieving public key info!\n");
		goto errOut;
	}
	
	/*
	 * Grind thru the list of known certs, demanding perfect match of 
	 * both fields 
	 */
	for(dex=0; dex<numKnownRoots; dex++) {
		if(!tpCompareCssmData(subjectName, 
	                          &knownRoots[dex].subjectName)) {
	    	continue;
	    }
		if(!tpCompareCssmData(publicKey,
	                          &knownRoots[dex].publicKey)) {
	    	continue;
	    }
#if 	ENABLE_APPLE_DEBUG_ROOT
	    if( dex == (knownRoots - 1) ){
	    	brtn = CSSM_FALSE;
	    	//tpSetError(CSSM_TP_DEBUG_CERT);
	    	break;
	    }
#endif
	    brtn = CSSM_TRUE;
	    break;
	}
errOut:
	tp_CertFreePublicKey(rootCert->clHand(), valToFree);
	return brtn;
}

/*
 * See if specified root cert is a known (embedded) iSign root cert.
 * Returns CSSM_TRUE if the cert is a known root cert. 
 *
 * Note as of 6/12/02, we do not distinguish between internally 
 * cached iSign roots and SSL roots. Maybe someday we will do so again,
 * so let's leave these two functions separate.
 */
static CSSM_BOOL tp_isIsignRootCert(
	CSSM_CL_HANDLE			clHand,
	TPCertInfo				*rootCert)		// raw cert from cert group
{
	const tpRootCert *roots;
	unsigned numRoots;
	roots = TPRootStore::tpGlobalRoots().rootCerts(clHand, numRoots);
	return tp_isKnownRootCert(rootCert, roots, numRoots);
}

/*
 * See if specified root cert is a known (embedded) SSL root cert.
 * Returns CSSM_TRUE if the cert is a known root cert. 
 */
static CSSM_BOOL tp_isSslRootCert(
	CSSM_CL_HANDLE			clHand,
	TPCertInfo				*rootCert)		// raw cert from cert group
{
	const tpRootCert *roots;
	unsigned numRoots;
	roots = TPRootStore::tpGlobalRoots().rootCerts(clHand, numRoots);
	return tp_isKnownRootCert(rootCert, roots, numRoots);
}

/*
 * Attempt to verify specified cert (from the end of a chain) with one of
 * our known SSL roots. 
 */
CSSM_BOOL tp_verifyWithSslRoots(
	CSSM_CL_HANDLE	clHand, 
	CSSM_CSP_HANDLE	cspHand, 
	TPCertInfo		*certToVfy)			// last in chain, not root
{
	CSSM_KEY 		rootKey;			// pub key manufactured from tpRootCert info
	CSSM_CC_HANDLE	ccHand;				// signature context
	CSSM_RETURN		crtn;
	unsigned 		dex;
	const tpRootCert *rootInfo;
	CSSM_BOOL		brtn = CSSM_FALSE;
	CSSM_KEYHEADER	*hdr = &rootKey.KeyHeader;
	CSSM_X509_ALGORITHM_IDENTIFIER_PTR algId;
	CSSM_DATA_PTR	valToFree = NULL;
	CSSM_ALGORITHMS	sigAlg;
	const tpRootCert *rootCerts = NULL;
	unsigned 		numRootCerts = 0;
		
	memset(&rootKey, 0, sizeof(CSSM_KEY));
	
	/*
	 * Get signature algorithm from subject key
	 */
	algId = tp_CertGetAlgId(certToVfy, &valToFree);
	if(algId == NULL) {
		/* bad cert */
		return CSSM_FALSE;
	}
	/* subsequest errors to errOut: */
	
	/* map to key and signature algorithm */
	sigAlg = tpOidToAldId(&algId->algorithm, &hdr->AlgorithmId);
	if(sigAlg == CSSM_ALGID_NONE) {
		errorLog0("tp_verifyWithSslRoots: unknown sig alg\n");
		goto errOut;
	}
	
	/* Set up other constant key fields */
	hdr->BlobType = CSSM_KEYBLOB_RAW;
	switch(hdr->AlgorithmId) {
		case CSSM_ALGID_RSA:
			hdr->Format = CSSM_KEYBLOB_RAW_FORMAT_PKCS1;
			break;
		case CSSM_ALGID_DSA:
			hdr->Format = CSSM_KEYBLOB_RAW_FORMAT_FIPS186;
			break;
		case CSSM_ALGID_FEE:
			hdr->Format = CSSM_KEYBLOB_RAW_FORMAT_OCTET_STRING;
			break;
		default:
			/* punt */
			hdr->Format = CSSM_KEYBLOB_RAW_FORMAT_NONE;
	}
	hdr->KeyClass = CSSM_KEYCLASS_PUBLIC_KEY;
	hdr->KeyAttr = CSSM_KEYATTR_MODIFIABLE | CSSM_KEYATTR_EXTRACTABLE;
	hdr->KeyUsage = CSSM_KEYUSE_VERIFY;
	
	rootCerts = TPRootStore::tpGlobalRoots().rootCerts(clHand, numRootCerts);
	for(dex=0; dex<numRootCerts; dex++) {
		rootInfo = &rootCerts[dex];
		if(!tpIsSameName(&rootInfo->subjectName, certToVfy->issuerName())) {
			/* not this root */
			continue;
		}

		/* only variation in key in the loop - raw key bits and size */
		rootKey.KeyData = rootInfo->publicKey;
		hdr->LogicalKeySizeInBits = rootInfo->keySize;
		crtn = CSSM_CSP_CreateSignatureContext(cspHand,
			sigAlg,
			NULL,		// AcccedCred
			&rootKey,
			&ccHand);
		if(crtn) {
			errorLog0("tp_verifyWithSslRoots: CSSM_CSP_CreateSignatureContext err\n");
			CssmError::throwMe(CSSMERR_TP_INTERNAL_ERROR);
		}
		crtn = CSSM_CL_CertVerify(clHand,
			ccHand,
			certToVfy->certData(),
			NULL,			// no signer cert
			NULL,			// VerifyScope
			0);				// ScopeSize
		CSSM_DeleteContext(ccHand);
		if(crtn == CSSM_OK) {
			/* success! */
			brtn = CSSM_TRUE;
			break;
		}
	}
errOut:
	if(valToFree != NULL) {
		tp_CertFreeAlgId(clHand, valToFree);
	}
	return brtn;
}
#endif	/* TP_ROOT_CERT_ENABLE */

/*
 * Convert a C string to lower case in place. NULL terminator not needed.
 */
static void tpToLower(
	char *str,
	unsigned strLen)
{
	for(unsigned i=0; i<strLen; i++) {
		*str++ = tolower(*str);
	}
}

/*
 * Verify SSL options. Currently this just consists of matching the 
 * leaf cert's subject common name against the caller's (optional)
 * server name.
 */
static CSSM_RETURN tp_verifySslOpts(
	TPCertGroup &certGroup,
	const CSSM_APPLE_TP_SSL_OPTIONS *sslOpts)
{
	if(sslOpts == NULL) {
		/* optional */
		return CSSM_OK;
	}

	CSSM_DATA_PTR subjNameData = NULL;
	char *serverName = NULL;
	unsigned serverNameLen = sslOpts->ServerNameLen;
	char *commonName = NULL;
	uint32 commonNameLen = 0;
	
	if(serverNameLen == 0) {
		/* optional */
		return CSSM_OK;
	}
	if(sslOpts->ServerName == NULL) {
		return CSSMERR_TP_INVALID_POINTER;
	}
	
	/* Obtain subject name of leaf cert in CSSM_X509_NAME_PTR form */
	TPCertInfo *leaf = certGroup.certAtIndex(0);
	assert(leaf != NULL);
	CSSM_RETURN crtn;
	crtn = leaf->fetchField(&CSSMOID_X509V1SubjectNameCStruct, &subjNameData);
	if(crtn) {
		/* should never happen, we shouldn't be here if there is no subject */
		errorLog0("tp_verifySslOpts: error retrieving subject name\n");
		return crtn;
	}
	CSSM_X509_NAME_PTR x509name = (CSSM_X509_NAME_PTR)subjNameData->Data;
	if((x509name == NULL) || (subjNameData->Length != sizeof(CSSM_X509_NAME))) {
		errorLog0("tp_verifySslOpts: malformed CSSM_X509_NAME\n");
		crtn = CSSMERR_TP_INVALID_CERTGROUP;
		goto done;
	}

	/* Now grunge thru the X509 name looking for a common name */
	CSSM_X509_TYPE_VALUE_PAIR 	*ptvp;
	CSSM_X509_RDN_PTR    		rdnp;
	unsigned					rdnDex;
	unsigned					pairDex;
	
	for(rdnDex=0; rdnDex<x509name->numberOfRDNs; rdnDex++) {
		rdnp = &x509name->RelativeDistinguishedName[rdnDex];
		for(pairDex=0; pairDex<rdnp->numberOfPairs; pairDex++) {
			ptvp = &rdnp->AttributeTypeAndValue[pairDex];
			if(tpCompareOids(&ptvp->type, &CSSMOID_CommonName)) {
				commonName = (char *)ptvp->value.Data;
				commonNameLen = ptvp->value.Length;
				break;
			}
		}
	}
	if(commonName == NULL) {
		errorLog0("tp_verifySslOpts: NO COMMON NAME in subject\n");
		crtn = CSSMERR_TP_VERIFY_ACTION_FAILED;
		goto done;
	}
	
	/* tolerate optional NULL terminators for both */
	if(commonName[commonNameLen - 1] == '\0') {
		commonNameLen--;
	}
	if(sslOpts->ServerName[serverNameLen - 1] == '\0') {
		serverNameLen--;
	}
	
	/* convert both name strings to lower case. The one in the X509 Name can
	 * be done in place; we have to malloc and copy the caller's string. */
	tpToLower(commonName, commonNameLen);
	serverName = (char *)certGroup.alloc().malloc(serverNameLen);
	memmove(serverName, sslOpts->ServerName, serverNameLen);
	tpToLower(serverName, serverNameLen);
	
	/* case 1: exact match */
	if((serverNameLen == commonNameLen) &&
	    !memcmp(commonName, serverName, commonNameLen)) {
		crtn = CSSM_OK;
		goto done;
	}
	
	/* case 2: handle optional '*' in cert's common name */
	if(commonName[0] == '*') {
		/* last (commonNameLen - 1) chars have to match */
		unsigned effectLen = commonNameLen - 1;		// skip '*' 
		if(serverNameLen < effectLen) {
			errorLog0("tp_verifySslOpts: subject/server name wildcard mismatch (1)\n");
			crtn = CSSMERR_TP_VERIFY_ACTION_FAILED;
		}
		else if(memcmp(commonName+1,		// skip '*'
		         serverName + serverNameLen - effectLen,
				 effectLen)) {
			errorLog0("tp_verifySslOpts: subject/server name wildcard mismatch (2)\n");
			crtn = CSSMERR_TP_VERIFY_ACTION_FAILED;
		}
		else {
			/* wildcard match */
			crtn = CSSM_OK;
		}
	}
	else {
		/* mismatch */
		errorLog0("tp_verifySslOpts: subject/server name mismatch\n");
		crtn = CSSMERR_TP_VERIFY_ACTION_FAILED;
	}
done:
	if(subjNameData != NULL) {
		leaf->freeField(&CSSMOID_X509V1SubjectNameCStruct, subjNameData);
	}
	if(serverName != NULL) {
		certGroup.alloc().free(serverName);	
	}
	if(crtn == CSSMERR_TP_VERIFY_ACTION_FAILED) {
		leaf->addStatusCode(CSSMERR_APPLETP_HOSTNAME_MISMATCH);
	}
	return crtn;
}

/*
 * RFC2459 says basicConstraints must be flagged critical for
 * CA certs, but Verisign doesn't work that way.
 */
#define BASIC_CONSTRAINTS_MUST_BE_CRITICAL		0

/*
 * TP iSign spec says Extended Key Usage required for leaf certs,
 * but Verisign doesn't work that way. 
 */
#define EXTENDED_KEY_USAGE_REQUIRED_FOR_LEAF	0

/*
 * TP iSign spec says Subject Alternate Name required for leaf certs,
 * but Verisign doesn't work that way. 
 */
#define SUBJECT_ALT_NAME_REQUIRED_FOR_LEAF		0

/*
 * TP iSign spec originally required KeyUsage for all certs, but
 * Verisign doesn't have that in their roots.
 */
#define KEY_USAGE_REQUIRED_FOR_ROOT				0

/*
 * Public routine to perform TP verification on a constructed 
 * cert group.
 * Returns CSSM_TRUE on success.
 * Asumes the chain has passed basic subject/issuer verification. First cert of
 * incoming certGroup is end-entity (leaf). 
 *
 * Per-policy details:
 *   iSign: Assumes that last cert in incoming certGroup is a root cert.
 *			Also assumes a cert group of more than one cert.
 *   kTPx509Basic: CertGroup of length one allowed. 
 */
CSSM_RETURN tp_policyVerify(
	TPPolicy						policy,
	CssmAllocator					&alloc,
	CSSM_CL_HANDLE					clHand,
	CSSM_CSP_HANDLE					cspHand,
	TPCertGroup 					*certGroup,
	CSSM_BOOL						verifiedToRoot,	// last cert is good root
	const CSSM_APPLE_TP_ACTION_DATA	*actionData,
	const CSSM_APPLE_TP_SSL_OPTIONS	*sslOpts,
	void							*policyOpts)	// future options
{
	iSignCertInfo 			*certInfo = NULL;
	uint32					numCerts;
	iSignCertInfo			*thisCertInfo;
	uint16					expUsage;
	uint16					actUsage;
	unsigned				certDex;
	CSSM_BOOL				cA = CSSM_FALSE;		// init for compiler warning
	CSSM_BOOL				isLeaf;					// end entity
	CSSM_BOOL				isRoot;					// root cert
	CE_ExtendedKeyUsage		*extendUsage;
	CE_AuthorityKeyID		*authorityId;
	CSSM_RETURN				outErr = CSSM_OK;		// for gross, non-policy errors
	CSSM_BOOL				policyFail = CSSM_FALSE;
	
	/* First, kTPDefault is a nop here */
	if(policy == kTPDefault) {
		return CSSM_OK;
	}
	
	if(certGroup == NULL) {
		return CSSMERR_TP_INVALID_CERTGROUP;
	}
	numCerts = certGroup->numCerts();
	if(numCerts == 0) {
		return CSSMERR_TP_INVALID_CERTGROUP;
	}
	if(policy == kTPiSign) {
		if(!verifiedToRoot) {
			/* no way, this requires a root cert */
			return CSSMERR_TP_INVALID_CERTGROUP;
		}
		if(numCerts <= 1) {
			/* nope, not for iSign */
			return CSSMERR_TP_INVALID_CERTGROUP;
		}
	}
	
	/* cook up an iSignCertInfo array */
	certInfo = (iSignCertInfo *)tpCalloc(alloc, numCerts, sizeof(iSignCertInfo));
	/* subsequent errors to errOut: */
	
	/* fill it with interesting info from parsed certs */
	for(certDex=0; certDex<numCerts; certDex++) {
		if(iSignGetCertInfo(alloc, 
				certGroup->certAtIndex(certDex),		
				&certInfo[certDex])) {
			(certGroup->certAtIndex(certDex))->addStatusCode(
				CSSMERR_TP_INVALID_CERTIFICATE);
			/* this one is fatal */
			outErr = CSSMERR_TP_INVALID_CERTIFICATE;
			goto errOut;
		}	
	}
		
	/*
	 * OK, the heart of TP enforcement.
	 * First check for presence of required extensions and 
	 * critical extensions we don't understand.
	 */
	for(certDex=0; certDex<numCerts; certDex++) {
		thisCertInfo = &certInfo[certDex];
		TPCertInfo *thisTpCertInfo = certGroup->certAtIndex(certDex);
		
		if(thisCertInfo->foundUnknownCritical) {
			/* illegal for all policies */
			errorLog0("tp_policyVerify: critical flag in unknown extension\n");
			thisTpCertInfo->addStatusCode(CSSMERR_APPLETP_UNKNOWN_CRITICAL_EXTEN);
			policyFail = CSSM_TRUE;
		}
		
		/* 
		 * Note it's possible for both of these to be true, for a 
		 * of length one (kTPx509Basic only!)
		 */
		isLeaf = (certDex == 0)              ? CSSM_TRUE : CSSM_FALSE;
		isRoot = (certDex == (numCerts - 1)) ? CSSM_TRUE : CSSM_FALSE;
			
		/*
		 * BasicConstraints.cA
		 * iSign:   	 required in all but leaf and root,
		 *          	 for which it is optional (with default values of false
		 *         	 	 for leaf and true for root).
		 * kTPx509Basic,
		 * kTP_SSL:      always optional, default of false for leaf and
		 *				 true for others
		 * All:     	 cA must be false for leaf, true for others
		 */
		if(!thisCertInfo->basicConstraints.present) {
			if(isLeaf) {
				/* cool, use default; note that kTPx509Basic with
				 * certGroup length of one may take this case */
				cA = CSSM_FALSE;
			}
			else if(isRoot) {
				/* cool, use default */
				cA = CSSM_TRUE;
			}
			else {
				switch(policy) {
					case kTPx509Basic:
					case kTP_SSL:
						/* 
						 * not present, not leaf, not root, kTPx509Basic 
						 * ....RFC2459 says this can not be a CA 
						 */
						cA = CSSM_FALSE;
						break;
					case kTPiSign:
						/* required for iSign in this position */
						errorLog0("tp_policyVerify: no basicConstraints\n");
						policyFail = CSSM_TRUE;
						thisTpCertInfo->addStatusCode(
							CSSMERR_APPLETP_NO_BASIC_CONSTRAINTS);
						break;
					default:
						/* not reached */
						break;
				}
			}
		}
		else {
			/* basicConstraints present */
			#if		BASIC_CONSTRAINTS_MUST_BE_CRITICAL
			/* disabled for verisign compatibility */
			if(!thisCertInfo->basicConstraints.critical) {
				/* per RFC 2459 */
				errorLog0("tp_policyVerify: basicConstraints marked not critical\n");
				policyFail = CSSM_TRUE;
				thisTpCertInfo->addStatusCode(CSSMERR_TP_VERIFY_ACTION_FAILED);
			}
			#endif	/* BASIC_CONSTRAINTS_MUST_BE_CRITICAL */

			const CE_BasicConstraints *bcp = 
				&thisCertInfo->basicConstraints.extnData->basicConstraints;
			
			cA = bcp->cA;
			
			/* Verify pathLenConstraint if present */
			if(!isLeaf &&							// leaf, certDex=0, don't care
			   cA && 								// p.l.c. only valid for CAs
			   bcp->pathLenConstraintPresent) {		// present?
				/*
				 * pathLenConstraint=0 legal for certDex 1 only
				 * pathLenConstraint=1 legal for certDex {1,2}
				 * etc. 
				 */ 
				if(certDex > (bcp->pathLenConstraint + 1)) {
					errorLog0("tp_policyVerify: pathLenConstraint exceeded\n");
					policyFail = CSSM_TRUE;
					thisTpCertInfo->addStatusCode(
							CSSMERR_APPLETP_PATH_LEN_CONSTRAINT);
				}
			}
		}
		
		if(isLeaf) {
			/* special case to allow a chain of length 1, leaf and root 
			 * both true (kTPx509Basic, kTP_SSL only) */
			if(cA && !isRoot) {
				errorLog0("tp_policyVerify: cA true for leaf\n");
				policyFail = CSSM_TRUE;
				thisTpCertInfo->addStatusCode(CSSMERR_APPLETP_INVALID_CA);
			}
		} else if(!cA) {
			errorLog0("tp_policyVerify: cA false for non-leaf\n");
			policyFail = CSSM_TRUE;
			thisTpCertInfo->addStatusCode(CSSMERR_APPLETP_INVALID_CA);
		}
		
		/*
		 * Authority Key Identifier optional
		 * iSign   		: only allowed in !root. 
		 *           	  If present, must not be critical.
		 * kTPx509Basic : 
		 * kTP_SSL      : ignored (though used later for chain verification)
		 */ 
		if((policy == kTPiSign) && thisCertInfo->authorityId.present) {
			if(isRoot) {
				errorLog0("tp_policyVerify: authorityId in root\n");
				policyFail = CSSM_TRUE;
				thisTpCertInfo->addStatusCode(CSSMERR_APPLETP_INVALID_AUTHORITY_ID); 
			}
			if(thisCertInfo->authorityId.critical) {
				/* illegal per RFC 2459 */
				errorLog0("tp_policyVerify: authorityId marked critical\n");
				policyFail = CSSM_TRUE;
				thisTpCertInfo->addStatusCode(CSSMERR_APPLETP_INVALID_AUTHORITY_ID); 
			}
		}

		/*
		 * Subject Key Identifier optional 
		 * iSign   		 : can't be critical. 
		 * kTPx509Basic,
		 * kTP_SSL       : ignored (though used later for chain verification)
		 */ 
		if(thisCertInfo->subjectId.present) {
			if((policy == kTPiSign) && thisCertInfo->subjectId.critical) {
				errorLog0("tp_policyVerify: subjectId marked critical\n");
				policyFail = CSSM_TRUE;
				thisTpCertInfo->addStatusCode(CSSMERR_APPLETP_INVALID_SUBJECT_ID); 
			}
		}
		
		/*
		 * Key Usage optional except as noted required
		 * iSign    	: required for non-root/non-leaf
		 *            	  Leaf cert : if present, usage = digitalSignature
		 *				  Exception : if leaf, and keyUsage not present, 
		 *					          netscape-cert-type must be present, with
		 *							  Object Signing bit set
		 * kTPx509Basic : non-leaf  : usage = keyCertSign
		 *			  	  Leaf: don't care
		 */ 
		if(thisCertInfo->keyUsage.present) {
			/*
			 * Leaf cert: usage = digitalSignature
			 * Others:    usage = keyCertSign
			 * We only require that one bit to be set, we ignore others. 
			 */
			if(isLeaf) {
				if(policy == kTPiSign) {
					expUsage = CE_KU_DigitalSignature;
				}
				else {
					/* hack to accept whatever's there */
					expUsage = thisCertInfo->keyUsage.extnData->keyUsage;
				}
			}
			else {
				/* this is true for all policies */
				expUsage = CE_KU_KeyCertSign;
			}
			actUsage = thisCertInfo->keyUsage.extnData->keyUsage;
			if(!(actUsage & expUsage)) {
				errorLog2("tp_policyVerify: bad keyUsage (leaf %s; usage 0x%x)\n",
					(certDex == 0) ? "TRUE" : "FALSE", actUsage);
				policyFail = CSSM_TRUE;
				thisTpCertInfo->addStatusCode(CSSMERR_APPLETP_INVALID_KEY_USAGE); 
			}
		}
		else if(policy == kTPiSign) {
			/* 
			 * iSign requires keyUsage present for non root OR
			 * netscape-cert-type/ObjectSigning for leaf
			 */
			if(isLeaf && thisCertInfo->netscapeCertType.present) {
				CE_NetscapeCertType ct = 
					thisCertInfo->netscapeCertType.extnData->netscapeCertType;
					
				if(!(ct & CE_NCT_ObjSign)) {
					errorLog0("tp_policyVerify: netscape-cert-type, !ObjectSign\n");
					policyFail = CSSM_TRUE;
					thisTpCertInfo->addStatusCode(CSSMERR_APPLETP_INVALID_KEY_USAGE);
				}
			}
			else if(!isRoot) {
				errorLog0("tp_policyVerify: !isRoot, no keyUsage, !(leaf and netscapeCertType)\n");
				policyFail = CSSM_TRUE;
				thisTpCertInfo->addStatusCode(CSSMERR_APPLETP_INVALID_KEY_USAGE); 
			}
		}				
	}	/* for certDex, checking presence of extensions */

	/*
	 * Special case checking for leaf (end entity) cert	
	 *
	 * iSign only: Extended key usage, optional for leaf, 
	 * value CSSMOID_ExtendedUseCodeSigning
	 */
	if((policy == kTPiSign) && certInfo[0].extendKeyUsage.present) {
		extendUsage = &certInfo[0].extendKeyUsage.extnData->extendedKeyUsage;
		if(extendUsage->numPurposes != 1) {
			errorLog1("tp_policyVerify: bad extendUsage->numPurposes (%d)\n",
				(int)extendUsage->numPurposes);
			policyFail = CSSM_TRUE;
			(certGroup->certAtIndex(0))->addStatusCode(
				CSSMERR_APPLETP_INVALID_EXTENDED_KEY_USAGE); 
		}
		if(!tpCompareOids(extendUsage->purposes,
				&CSSMOID_ExtendedUseCodeSigning)) {
			errorLog0("tp_policyVerify: bad extendKeyUsage\n");
			policyFail = CSSM_TRUE;
			(certGroup->certAtIndex(0))->addStatusCode(
				CSSMERR_APPLETP_INVALID_EXTENDED_KEY_USAGE); 
		}
	}
	
	/*
	 * Verify authorityId-->subjectId linkage.
	 * All optional - skip if needed fields not present.
	 * Also, always skip last (root) cert.  
	 */
	for(certDex=0; certDex<(numCerts-1); certDex++) {
		if(!certInfo[certDex].authorityId.present ||
		   !certInfo[certDex+1].subjectId.present) {
		 	continue;  
		}
		authorityId = &certInfo[certDex].authorityId.extnData->authorityKeyID;
		if(!authorityId->keyIdentifierPresent) {
			/* we only know how to compare keyIdentifier */
			continue;
		}
		if(!tpCompareCssmData(&authorityId->keyIdentifier,
				&certInfo[certDex+1].subjectId.extnData->subjectKeyID)) {
			errorLog0("tp_policyVerify: bad key ID linkage\n");
			policyFail = CSSM_TRUE;
			(certGroup->certAtIndex(certDex))->addStatusCode(
					CSSMERR_APPLETP_INVALID_ID_LINKAGE); 
		}
	}
	
	/* 
	 * SSL: optionally verify common name.
	 * FIXME - should this be before or after the root cert test? How can
	 * we return both errors?
	 */
	if(policy == kTP_SSL) {
		CSSM_RETURN cerr = tp_verifySslOpts(*certGroup, sslOpts);
		if(cerr) {
			policyFail = CSSM_TRUE;
		}
	}
	
	/* iSign, SSL: compare root against known root certs */
	/* FIXME - this goes away soon */
	#if		TP_ROOT_CERT_ENABLE
	if((outErr == CSSM_OK) &&	// skip if we have a gross error (other than policy failure)
	   (actionData != NULL) &&
	   (actionData->ActionFlags & 0x80000000)) {	// The secret "enable root cert check" flag
		TPCertInfo *lastCert = certGroup->lastCert();
		if(policy == kTPiSign) {
			bool brtn = tp_isIsignRootCert(clHand, lastCert);
			if(!brtn) {
				policyFail = CSSM_TRUE;
			}
		}
		else if(verifiedToRoot && (policy == kTP_SSL)) {
			/* note SSL doesn't require root here */
			bool brtn = tp_isSslRootCert(clHand, lastCert);
			if(!brtn) {
				outErr = CSSMERR_TP_INVALID_ANCHOR_CERT;
			}
		}
	}
	#endif	/* TP_ROOT_CERT_ENABLE */
	if(policyFail && (outErr == CSSM_OK)) {
		/* only error in this function was policy failure */
		outErr = CSSMERR_TP_VERIFY_ACTION_FAILED;
	}
errOut:
	/* free resources */
	for(certDex=0; certDex<numCerts; certDex++) {
		thisCertInfo = &certInfo[certDex];
		iSignFreeCertInfo(clHand, thisCertInfo);
	}
	tpFree(alloc, certInfo);
	return outErr;
}
