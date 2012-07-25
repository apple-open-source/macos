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
	certGroupUtils.cpp

	Created 10/9/2000 by Doug Mitchell. 
*/

#include <Security/cssmtype.h>
#include <Security/cssmapi.h>
#include <Security/x509defs.h>
#include <Security/oidscert.h>
#include <Security/oidsalg.h>
#include <Security/cssmapple.h>
#include <Security/SecAsn1Coder.h>
#include <Security/keyTemplates.h>

#include "certGroupUtils.h" 
#include "tpdebugging.h"
#include "tpTime.h"

#include <string.h>				/* for memcmp */


/*
 * Copy one CSSM_DATA to another, mallocing destination. 
 */
void tpCopyCssmData(
	Allocator		&alloc,
	const CSSM_DATA	*src,
	CSSM_DATA_PTR	dst)
{
	dst->Data = (uint8 *)alloc.malloc(src->Length);
	dst->Length = src->Length;
	memmove(dst->Data, src->Data, src->Length);
}

/*
 * Malloc a CSSM_DATA, copy another one to it.
 */
CSSM_DATA_PTR tpMallocCopyCssmData(
	Allocator		&alloc,
	const CSSM_DATA	*src)
{
	CSSM_DATA_PTR dst = (CSSM_DATA_PTR)alloc.malloc(sizeof(CSSM_DATA));
	tpCopyCssmData(alloc, src, dst);
	return dst;
}

/*
 * Free the data referenced by a CSSM data, and optionally, the struct itself.
 */
void tpFreeCssmData(
	Allocator		&alloc,
	CSSM_DATA_PTR 	data,
	CSSM_BOOL 		freeStruct)
{
	if(data == NULL) {
		return;
	}
	if(data->Length != 0) {
		tpFree(alloc, data->Data);
	}
	if(freeStruct) {
		tpFree(alloc, data);
	}
	else {
		data->Length = 0;
		data->Data = NULL;
	}
}

/*
 * Compare two CSSM_DATAs, return CSSM_TRUE if identical.
 */
CSSM_BOOL tpCompareCssmData(
	const CSSM_DATA *data1,
	const CSSM_DATA *data2)
{	
	if((data1 == NULL) || (data1->Data == NULL) || 
	   (data2 == NULL) || (data2->Data == NULL) ||
	   (data1->Length != data2->Length)) {
		return CSSM_FALSE;
	}
	if(data1->Length != data2->Length) {
		return CSSM_FALSE;
	}
	if(memcmp(data1->Data, data2->Data, data1->Length) == 0) {
		return CSSM_TRUE;
	}
	else {
		return CSSM_FALSE;
	}
}

/*
 * Free memory via specified plugin's app-level allocator
 */
void tpFreePluginMemory(
	CSSM_HANDLE	hand,
	void 		*p)
{
	CSSM_API_MEMORY_FUNCS memFuncs;
	CSSM_RETURN crtn = CSSM_GetAPIMemoryFunctions(hand, &memFuncs);
	if(crtn) {
		tpErrorLog("CSSM_GetAPIMemoryFunctions failure\n");
		/* oh well, leak and continue */
		return;
	}
	memFuncs.free_func(p, memFuncs.AllocRef);
}

/*
 * Obtain the public key blob from a cert.
 */
CSSM_DATA_PTR tp_CertGetPublicKey( 
    TPCertInfo *cert,
	CSSM_DATA_PTR *valueToFree)			// used in tp_CertFreePublicKey
{
	CSSM_RETURN crtn;
	CSSM_DATA_PTR val;
	CSSM_X509_SUBJECT_PUBLIC_KEY_INFO *keyInfo;
	
	*valueToFree = NULL;
	crtn = cert->fetchField(&CSSMOID_X509V1SubjectPublicKeyCStruct, &val);
	if(crtn) {
		tpErrorLog("Error on CSSM_CL_CertGetFirstFieldValue(PublicKeyCStruct)\n");
		return NULL;
	}
	*valueToFree = val;
	keyInfo = (CSSM_X509_SUBJECT_PUBLIC_KEY_INFO *)val->Data;
	return &keyInfo->subjectPublicKey;
}

void tp_CertFreePublicKey(
	CSSM_CL_HANDLE	clHand,
	CSSM_DATA_PTR	value)
{
  	CSSM_CL_FreeFieldValue(clHand, &CSSMOID_X509V1SubjectPublicKeyCStruct, value);
}

/*
 * Obtain signature algorithm info from a cert.
 */
CSSM_X509_ALGORITHM_IDENTIFIER_PTR tp_CertGetAlgId( 
    TPCertInfo	 	*cert,
	CSSM_DATA_PTR 	*valueToFree)			// used in tp_CertFreeAlgId
{
	CSSM_RETURN crtn;
	CSSM_DATA_PTR val;
	
	*valueToFree = NULL;
	crtn = cert->fetchField(&CSSMOID_X509V1SignatureAlgorithm, &val);
	if(crtn) {
		tpErrorLog("Error on fetchField(CSSMOID_X509V1SignatureAlgorithm)\n");
		return NULL;
	}
	*valueToFree = val;
	return (CSSM_X509_ALGORITHM_IDENTIFIER_PTR)val->Data;
}

void tp_CertFreeAlgId(
	CSSM_CL_HANDLE	clHand,
	CSSM_DATA_PTR	value)
{
  	CSSM_CL_FreeFieldValue(clHand, &CSSMOID_X509V1SignatureAlgorithm, value);
}

/*
 * Determine if two certs - passed in encoded form - are equivalent. 
 */
CSSM_BOOL tp_CompareCerts(
	const CSSM_DATA			*cert1,
	const CSSM_DATA			*cert2)
{
	return tpCompareCssmData(cert1, cert2);
}

/*
 * Convert a C string to lower case in place. NULL terminator not needed.
 */
void tpToLower(
	char *str,
	unsigned strLen)
{
	for(unsigned i=0; i<strLen; i++) {
		*str = tolower(*str);
		str++;
	}
}

/*
 * Normalize an RFC822 addr-spec. This consists of converting
 * all characters following the '@' character to lower case.
 * A true normalizeAll results in lower-casing all characters
 * (e.g. for iChat). 
 */
void tpNormalizeAddrSpec(
	char		*addr,
	unsigned	addrLen,
	bool		normalizeAll)
{
	if (addr == NULL) {
		tpPolicyError("tpNormalizeAddrSpec: bad addr");
		return;
	}
	if(!normalizeAll) {
		while((addrLen != 0) && (*addr != '@')) {
			addr++;
			addrLen--;
		}
		if(addrLen == 0) {
			tpPolicyError("tpNormalizeAddrSpec: bad addr-spec");
			return;
		}
	}
	tpToLower(addr, addrLen);
}

/***
 *** dnsName compare support.
 *** Please do not make any changes to this code without talking to 
 *** dmitch about updating (if necessary) and running (always)
 *** regression tests which specifically test this logic.
 ***/
 
/*
 * Max length of a distinguished name component (label) we handle.
 * Various RFCs spec this out at 63 bytes; we're just allocating space
 * for these on the stack, so why not cut some slack. 
 */
#define MAX_DNS_COMP_LEN	128

/*
 * Obtain the next component from a DNS Name.
 * Caller mallocs outBuf, size >= MAX_DNS_COMP_LEN.
 * Returns true if a component was found.
 */
static bool tpNextDnsComp(
	const char 	*inBuf,
	uint32		&inBufLen,		// IN/OUT
	char		*outBuf,		// component RETURNED here
	uint32		&outBufLen)		// RETURNED length of component
{
	outBufLen = 0;
	if(inBufLen == 0) {
		return false;
	}
	
	/* skip over leading '.' */
	if(*inBuf == '.') {
		inBuf++;
		if(--inBufLen == 0) {
			return false;
		}
	}
	
	/* copy chars until out of data or next '.' found */
	do {
		if(*inBuf == '.') {
			break;
		}
		*outBuf++ = *inBuf++;
		inBufLen--;
		outBufLen++;
		if(outBufLen >= MAX_DNS_COMP_LEN) {
			/* abort */
			break;
		}
	} while(inBufLen != 0);
	if(outBufLen) {
		return true;
	}
	else {
		return false;
	}
}

/*
 * Find location of specified substring in given bigstring. Returns
 * pointer to start of substring in bigstring, else returns NULL.
 */
static const char *tpSubStr(
	const char 	*bigstr,
	uint32 		bigstrLen,
	const char 	*substr,
	uint32		substrLen)
{
	/* stop searching substrLen chars before end of bigstr */
	const char *endBigStr = bigstr + bigstrLen - substrLen;
	for( ; bigstr <= endBigStr; ) {
		if(*bigstr == *substr) {
			/* first char match - remainder? */
			if(substrLen == 1) {
				/* don't count on memcmp(a,b,0) */
				return bigstr;
			}
			if(!memcmp(bigstr+1, substr+1, substrLen - 1)) {
				return bigstr;
			} 
		}
		bigstr++;
	} 
	return NULL;
}

/*
 * Compare two DNS components, with full wildcard check. We assume
 * that no '.' chars exist (per the processing performed in 
 * tpNextDnsComp()). Returns CSSM_TRUE on match, else CSSM_FALSE.
 */
static CSSM_BOOL tpCompareComps(
	const char 	*hostComp, 			// no wildcards
	uint32 		hostCompLen, 
	const char 	*certComp, 			// wildcards OK here
	uint32		certCompLen)
{
	const char *endCertComp = certComp + certCompLen;
	const char *endHostComp = hostComp + hostCompLen;
	do {
		/* wild card in cert name? */
		const char *wildCard = tpSubStr(certComp, certCompLen,
			"*", 1);
		if(wildCard == NULL) {
			/* no, require perfect literal match right now */
			if((hostCompLen == certCompLen) &&
					!memcmp(hostComp, certComp, certCompLen)) {
				return CSSM_TRUE;
			}
			else {
				return CSSM_FALSE;
			}
		}
		
		if(wildCard != certComp) {
			/* 
			 * Require literal match of hostComp with certComp
			 * up until (but not including) the wildcard
			 */
			uint32 subStrLen = wildCard - certComp;
			if(subStrLen > hostCompLen) {
				/* out of host name chars */
				return CSSM_FALSE;
			}
			if(memcmp(certComp, hostComp, subStrLen)) {
				return CSSM_FALSE;
			}
			/* OK, skip over substring */
			hostComp    += subStrLen;
			hostCompLen -= subStrLen;
			/* start parsing at the wildcard itself */
			certComp     = wildCard;
			certCompLen -= subStrLen;
			continue;
		}
		
		/*
		 * Currently looking at a wildcard.
		 *
		 * Find substring in hostComp which matches from the char after
		 * the wildcard up to whichever of these comes next:
		 *
		 *  -- end of certComp 
		 *  -- another wildcard
		 */
		wildCard++;		
		if(wildCard == endCertComp) {
			/* 
			 * -- Wild card at end of cert's DNS
			 * -- nothing else to match - rest of hostComp is the wildcard
			 *    match
			 * -- done, success 
			 */
			return CSSM_TRUE;
		}
		
		const char *afterSubStr;		// in certComp
		afterSubStr = tpSubStr(wildCard, endCertComp - wildCard,
			"*", 1);
		if(afterSubStr == NULL) {
			/* no more wildcards - use end of certComp */
			afterSubStr = endCertComp;
		}
		uint32 subStrLen = afterSubStr - wildCard;
		const char *foundSub = tpSubStr(hostComp, hostCompLen,
			wildCard, subStrLen);
		if(foundSub == NULL) {
			/* No match of explicit chars */
			return CSSM_FALSE;
		}
		
		/* found it - skip past this substring */
		hostComp    = foundSub + subStrLen;
		hostCompLen = endHostComp - hostComp;
		certComp    = afterSubStr;
		certCompLen = endCertComp - afterSubStr;
		
	} while((hostCompLen != 0) || (certCompLen != 0));
	if((hostCompLen == 0) && (certCompLen == 0)) {
		return CSSM_TRUE;
	}
	else {
		/* end of one but not the other */
		return CSSM_FALSE;
	}
}

/*
 * Compare hostname, is presented to the TP in 
 * CSSM_APPLE_TP_SSL_OPTIONS.ServerName, to a server name obtained
 * from the server's cert (i.e., from subjectAltName or commonName).
 * Limited wildcard checking is performed here. 
 *
 * The incoming hostname is assumed to have been processed by tpToLower();
 * we'll perform that processing on certName here. 
 *
 * Trailing '.' characters in both host names will be ignored per Radar 3996792.
 *
 * Returns CSSM_TRUE on match, else CSSM_FALSE.
 */
CSSM_BOOL tpCompareHostNames(
	const char	 	*hostName,		// spec'd by app, tpToLower'd
	uint32			hostNameLen,
	char			*certName,		// from cert, we tpToLower
	uint32			certNameLen)
{
	tpToLower(certName, certNameLen);

	/* tolerate optional NULL terminators for both */
	if(hostNameLen && (hostName[hostNameLen - 1] == '\0')) {
		hostNameLen--;
	}
	if(certNameLen && (certName[certNameLen - 1] == '\0')) {
		certNameLen--;
	}
	
	if((hostNameLen == 0) || (certNameLen == 0)) {
		/* trivial case with at least one empty name */
		if(hostNameLen == certNameLen) {
			return CSSM_TRUE;
		}
		else {
			return CSSM_FALSE;
		}
	}
	
	/* trim off trailing dots */
	if(hostName[hostNameLen - 1] == '.') {
		hostNameLen--;
	}
	if(certName[certNameLen - 1] == '.') {
		certNameLen--;
	}
	
	/* Case 1: exact match */
	if((certNameLen == hostNameLen) &&
	    !memcmp(certName, hostName, certNameLen)) {
		return CSSM_TRUE;
	}
	
	/* 
	 * Case 2: Compare one component at a time, handling wildcards in
	 * cert's server name. The characters implicitly matched by a 
	 * wildcard span only one component of a dnsName.
	 */
	do {
		/* get next component from each dnsName */
		char hostComp[MAX_DNS_COMP_LEN];
		char certComp[MAX_DNS_COMP_LEN];
		uint32 hostCompLen;
		uint32 certCompLen;
		
		bool foundHost = tpNextDnsComp(hostName, hostNameLen,
				hostComp, hostCompLen);
		bool foundCert = tpNextDnsComp(certName, certNameLen,
				certComp, certCompLen);
		if(foundHost != foundCert) {
			/* unequal number of components */
			tpPolicyError("tpCompareHostNames: wildcard mismatch (1)");
			return CSSM_FALSE;
		}
		if(!foundHost) {
			/* normal successful termination */
			return CSSM_TRUE;
		}
		
		/* compare individual components */
		if(!tpCompareComps(hostComp, hostCompLen, 
				certComp, certCompLen)) {
			tpPolicyError("tpCompareHostNames: wildcard mismatch (2)");
			return CSSM_FALSE;
		}
		
		/* skip over this component
		 * (note: since tpNextDnsComp will first skip over a leading '.',
		 * we must make sure to skip over it here as well.)
		 */
		if(*hostName == '.') hostName++;
		hostName += hostCompLen;
		if(*certName == '.') certName++;
		certName += certCompLen;
	} while(1);
	/* NOT REACHED */
	//assert(0):
	return CSSM_FALSE;
}

/*
 * Compare email address, is presented to the TP in 
 * CSSM_APPLE_TP_SMIME_OPTIONS.SenderEmail, to a string obtained
 * from the sender's cert (i.e., from subjectAltName or Subject DN).
 *
 * Returns CSSM_TRUE on match, else CSSM_FALSE.
 *
 * Incoming appEmail string has already been tpNormalizeAddrSpec'd.
 * We do that for certEmail string here. 
 */
CSSM_BOOL tpCompareEmailAddr(
	const char	 	*appEmail,		// spec'd by app, normalized
	uint32			appEmailLen,
	char			*certEmail,		// from cert, we normalize
	uint32			certEmailLen,
	bool			normalizeAll)	// true : lower-case all certEmail characters

{
	tpNormalizeAddrSpec(certEmail, certEmailLen, normalizeAll);

	/* tolerate optional NULL terminators for both */
	if(appEmailLen > 0 && appEmail[appEmailLen - 1] == '\0') {
		appEmailLen--;
	}
	if(certEmailLen > 0 && certEmail[certEmailLen - 1] == '\0') {
		certEmailLen--;
	}
	if((certEmailLen == appEmailLen) &&
	    !memcmp(certEmail, appEmail, certEmailLen)) {
		return CSSM_TRUE;
	}
	else {
		/* mismatch */
		tpPolicyError("tpCompareEmailAddr: app/cert email addrs mismatch");
		return CSSM_FALSE;
	}
}

/* 
 * Following a CSSMOID_ECDSA_WithSpecified algorithm is an encoded
 * ECDSA_SigAlgParams containing the digest agorithm OID. Decode and return 
 * a unified ECDSA/digest alg (e.g. CSSM_ALGID_SHA512WithECDSA).
 * Returns nonzero on error.
 */
int decodeECDSA_SigAlgParams(
	const CSSM_DATA *params,
	CSSM_ALGORITHMS *cssmAlg)		/* RETURNED */
{
	SecAsn1CoderRef coder = NULL;
	if(SecAsn1CoderCreate(&coder)) {
		tpErrorLog("***Error in SecAsn1CoderCreate()\n");
		return -1;
	}
	CSSM_X509_ALGORITHM_IDENTIFIER algParams;
	memset(&algParams, 0, sizeof(algParams));
	int ourRtn = 0;
	bool algFound = false;
	if(SecAsn1DecodeData(coder, params, kSecAsn1AlgorithmIDTemplate,
			&algParams)) {
		tpErrorLog("***Error decoding CSSM_X509_ALGORITHM_IDENTIFIER\n");
		ourRtn = -1;
		goto errOut;
	}
	CSSM_ALGORITHMS digestAlg;
	algFound = cssmOidToAlg(&algParams.algorithm, &digestAlg);
	if(!algFound) {
		tpErrorLog("***Unknown algorithm in CSSM_X509_ALGORITHM_IDENTIFIER\n");
		ourRtn = -1;
		goto errOut;
	}
	switch(digestAlg) {
		case CSSM_ALGID_SHA1:
			*cssmAlg = CSSM_ALGID_SHA1WithECDSA;
			break;
		case CSSM_ALGID_SHA224:
			*cssmAlg = CSSM_ALGID_SHA224WithECDSA;
			break;
		case CSSM_ALGID_SHA256:
			*cssmAlg = CSSM_ALGID_SHA256WithECDSA;
			break;
		case CSSM_ALGID_SHA384:
			*cssmAlg = CSSM_ALGID_SHA384WithECDSA;
			break;
		case CSSM_ALGID_SHA512:
			*cssmAlg = CSSM_ALGID_SHA512WithECDSA;
			break;
		default:
			tpErrorLog("***Unknown algorithm in ECDSA_SigAlgParams\n");
			ourRtn = -1;
	}
errOut:
	SecAsn1CoderRelease(coder);
	return ourRtn;
}

