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

	/* extensions we're interested in */
	iSignExtenInfo		authorityId;
	iSignExtenInfo		subjectId;
	iSignExtenInfo		keyUsage;
	iSignExtenInfo		extendKeyUsage;
	iSignExtenInfo		basicConstraints;
	iSignExtenInfo		netscapeCertType;
	iSignExtenInfo		subjectAltName;
				
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
		tpPolicyError("tpSetupExtension: malformed CSSM_FIELD");
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
		tpPolicyError("iSignSearchUnknownExtensions: malformed CSSM_FIELD");
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
			tpPolicyError("searchUnknownExtensions: GetNextCachedFieldValue"
				"error");
			break;
		}
		if(fieldValue->Length != sizeof(CSSM_X509_EXTENSION)) {
			tpPolicyError("iSignSearchUnknownExtensions: "
				"malformed CSSM_FIELD");
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
	crtn = iSignFetchExtension(alloc,
		tpCert,
		&CSSMOID_SubjectAltName,
		&certInfo->subjectAltName);
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
	if(certInfo->subjectAltName.present) {
		CSSM_CL_FreeFieldValue(clHand, &CSSMOID_SubjectAltName, 
			certInfo->subjectAltName.valToFree);
	}
}

/* 
 * See if cert's Subject.{commonName,EmailAddress} matches caller-specified 
 * string. Returns CSSM_TRUE if match, else returns CSSM_FALSE. 
 * Also indicates whether *any* of the specified fields were found, regardless
 * of match state.
 */
typedef enum {
	SN_CommonName,			// CSSMOID_CommonName, host name format
	SN_Email				// CSSMOID_EmailAddress
} SubjSubjNameSearchType;

static CSSM_BOOL tpCompareSubjectName(
	TPCertInfo 				&cert,
	SubjSubjNameSearchType	searchType,
	const char 				*callerStr,			// already tpToLower'd
	uint32					callerStrLen,
	bool					&fieldFound)
{
	char 			*certName = NULL;			// from cert's subject name
	uint32 			certNameLen = 0;
	CSSM_DATA_PTR 	subjNameData = NULL;
	CSSM_RETURN 	crtn;
	CSSM_BOOL		ourRtn = CSSM_FALSE;
	const CSSM_OID	*oidSrch;
	
	fieldFound = false;
	switch(searchType) {
		case SN_CommonName:
			oidSrch = &CSSMOID_CommonName;
			break;
		case SN_Email:
			oidSrch = &CSSMOID_EmailAddress;
			break;
		default:
			assert(0);
			return CSSM_FALSE;
	}
	crtn = cert.fetchField(&CSSMOID_X509V1SubjectNameCStruct, &subjNameData);
	if(crtn) {
		/* should never happen, we shouldn't be here if there is no subject */
		tpPolicyError("tp_verifySslOpts: error retrieving subject name");
		return CSSM_FALSE;
	}
	CSSM_X509_NAME_PTR x509name = (CSSM_X509_NAME_PTR)subjNameData->Data;
	if((x509name == NULL) || (subjNameData->Length != sizeof(CSSM_X509_NAME))) {
		tpPolicyError("tp_verifySslOpts: malformed CSSM_X509_NAME");
		cert.freeField(&CSSMOID_X509V1SubjectNameCStruct, subjNameData);
		return CSSM_FALSE;
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
			if(tpCompareOids(&ptvp->type, oidSrch)) {
				fieldFound = true;
				certName = (char *)ptvp->value.Data;
				certNameLen = ptvp->value.Length;
				switch(searchType) {
					case SN_CommonName:
						ourRtn = tpCompareHostNames(callerStr, callerStrLen, 
							certName, certNameLen);
						break;
					case SN_Email:
						ourRtn = tpCompareEmailAddr(callerStr, callerStrLen,
							certName, certNameLen);
						break;
				}
				if(ourRtn) {
					/* success */
					break;
				}
				/* else keep going, maybe there's another common name */
			}
		}
		if(ourRtn) {
			break;
		}
	}
	cert.freeField(&CSSMOID_X509V1SubjectNameCStruct, subjNameData);
	return ourRtn;
}

/*
 * Compare ASCII form of an IP address to a CSSM_DATA containing 
 * the IP address's numeric components. Returns true on match.
 */
static CSSM_BOOL tpCompIpAddrStr(
	const char *str,
	unsigned strLen,
	const CSSM_DATA *numeric)
{
	const char *cp = str;
	const char *nextDot;
	char buf[100];
	
	if((numeric == NULL) || (numeric->Length == 0) || (str == NULL)) {
		return CSSM_FALSE;
	}
	if(cp[strLen - 1] == '\0') {
		/* ignore NULL terminator */
		strLen--;
	}
	for(unsigned dex=0; dex<numeric->Length; dex++) {
		/* cp points to start of current string digit */
		/* find next dot */
		const char *lastChar = cp + strLen;
		nextDot = cp + 1;
		for( ; nextDot<lastChar; nextDot++) {
			if(*nextDot == '.') {
				break;
			}
		}
		if(nextDot == lastChar) {
			/* legal and required on last digit */
			if(dex != (numeric->Length - 1)) {
				return CSSM_FALSE;
			}
		}
		else if(dex == (numeric->Length - 1)) {
			return CSSM_FALSE;
		}
		unsigned digLen = nextDot - cp;
		if(digLen >= sizeof(buf)) {
			/* preposterous */
			return CSSM_FALSE;
		}
		memmove(buf, cp, digLen);
		buf[digLen] = '\0';
		/* incr digLen to include the next dot */
		digLen++;
		cp += digLen;
		strLen -= digLen;
		int digVal = atoi(buf);
		if(digVal != numeric->Data[dex]) {
			return CSSM_FALSE;
		}
	}
	return CSSM_TRUE;
}

/* 
 * See if cert's subjectAltName contains an element matching caller-specified 
 * string, hostname, in the following forms:
 *
 * SAN_HostName : dnsName, iPAddress
 * SAN_Email    : RFC822Name
 *
 * Returns CSSM_TRUE if match, else returns CSSM_FALSE. 
 *
 * Also indicates whether or not a dnsName (search type HostName) or
 * RFC822Name (search type SAM_Email) was found, regardless of result
 * of comparison. 
 *
 * The appStr/appStrLen args are optional - if NULL/0, only the 
 * search for dnsName/RFC822Name is done.
 */
typedef enum {
	SAN_HostName,
	SAN_Email
} SubjAltNameSearchType;

static CSSM_BOOL tpCompareSubjectAltName(
	const iSignExtenInfo	&subjAltNameInfo,
	const char 				*appStr,			
	uint32					appStrLen,
	SubjAltNameSearchType 	searchType,
	bool					&dnsNameFound,		// RETURNED, SAN_HostName case
	bool					&emailFound)		// RETURNED, SAN_Email case
{
	dnsNameFound = false;
	emailFound = false;
	if(!subjAltNameInfo.present) {
		/* common failure, no subjectAltName found */
		return CSSM_FALSE;
	}

	CE_GeneralNames *names = &subjAltNameInfo.extnData->subjectAltName;
	CSSM_BOOL		ourRtn = CSSM_FALSE;	
	char 			*certName;
	unsigned 		certNameLen;
	
	/* Search thru the CE_GeneralNames looking for the appropriate attribute */
	for(unsigned dex=0; dex<names->numNames; dex++) {
		CE_GeneralName *name = &names->generalName[dex];
		switch(searchType) {
			case SAN_HostName:
				switch(name->nameType) {
					case GNT_IPAddress:
						if(appStr == NULL) {
							/* nothing to do here */
							break;
						}
						ourRtn = tpCompIpAddrStr(appStr, appStrLen, &name->name);
						break;
						
					case GNT_DNSName:
						if(name->berEncoded) {
							tpErrorLog("tpCompareSubjectAltName: malformed "
								"CE_GeneralName (1)\n");
							break;
						}
						certName = (char *)name->name.Data;
						if(certName == NULL) {
							tpErrorLog("tpCompareSubjectAltName: malformed "
								"CE_GeneralName (2)\n");
							break;
						}
						certNameLen = name->name.Length;
						dnsNameFound = true;
						if(appStr != NULL) {
							/* skip if caller passed in NULL */
							ourRtn = tpCompareHostNames(appStr, appStrLen, 
								certName, certNameLen);
						}
						break;
		
					default:
						/* not interested, proceed to next name */
						break;
				}
				break;	/* from case HostName */
				
			case SAN_Email:
				if(name->nameType != GNT_RFC822Name) {
					/* not interested */
					break;
				}
				certName = (char *)name->name.Data;
				if(certName == NULL) {
					tpErrorLog("tpCompareSubjectAltName: malformed "
						"GNT_RFC822Name\n");
					break;
				}
				certNameLen = name->name.Length;
				emailFound = true;
				if(appStr != NULL) {
					ourRtn = tpCompareEmailAddr(appStr, appStrLen, certName, 
						certNameLen);
				}
				break;
		}
		if(ourRtn) {
			/* success */
			break;
		}
	}
	return ourRtn;
}

/* is host name in the form of a.b.c.d, where a,b,c, and d are digits? */
static CSSM_BOOL tpIsNumeric(
	const char *hostName, 
	unsigned hostNameLen)
{
	if(hostName[hostNameLen - 1] == '\0') {
		/* ignore NULL terminator */
		hostNameLen--;
	}
	for(unsigned i=0; i<hostNameLen; i++) {
		char c = *hostName++;
		if(isdigit(c)) {
			continue;
		}
		if(c != '.') {
			return CSSM_FALSE;
		}
	}
	return CSSM_TRUE;
}

/*
 * Verify SSL options. Currently this just consists of matching the 
 * leaf cert's subject common name against the caller's (optional)
 * server name.
 */
static CSSM_RETURN tp_verifySslOpts(
	TPCertGroup &certGroup,
	const CSSM_DATA *sslFieldOpts,
	const iSignCertInfo &leafCertInfo)
{
	/* first validate optional SSL options */
	if((sslFieldOpts == NULL) || (sslFieldOpts->Data == NULL)) {
		/* optional */
		return CSSM_OK;
	}
	CSSM_APPLE_TP_SSL_OPTIONS *sslOpts;
	sslOpts = (CSSM_APPLE_TP_SSL_OPTIONS *)sslFieldOpts->Data;
	switch(sslOpts->Version) {
		case CSSM_APPLE_TP_SSL_OPTS_VERSION:
			if(sslFieldOpts->Length != sizeof(CSSM_APPLE_TP_SSL_OPTIONS)) {
				return CSSMERR_TP_INVALID_POLICY_IDENTIFIERS;
			}
			break;
		/* handle backwards compatibility here if necessary */
		default:
			return CSSMERR_TP_INVALID_POLICY_IDENTIFIERS;
	}

	unsigned hostNameLen = sslOpts->ServerNameLen;
	
	if(hostNameLen == 0) {
		/* optional */
		return CSSM_OK;
	}
	if(sslOpts->ServerName == NULL) {
		return CSSMERR_TP_INVALID_POINTER;
	}

	/* convert caller's hostname string to lower case */
	char *hostName = (char *)certGroup.alloc().malloc(hostNameLen);
	memmove(hostName, sslOpts->ServerName, hostNameLen);
	tpToLower(hostName, hostNameLen);
	
	TPCertInfo *leaf = certGroup.certAtIndex(0);
	assert(leaf != NULL);
	
	CSSM_BOOL match = CSSM_FALSE;
	
	/* First check subjectAltName... */
	bool dnsNameFound = false;
	bool dummy;
	match = tpCompareSubjectAltName(leafCertInfo.subjectAltName, 
		hostName, hostNameLen, 
		SAN_HostName, dnsNameFound, dummy);
		
	/* 
	 * Then common name, if
	 *  -- no match from subjectAltName, AND
	 *  -- dnsName was NOT found, AND
	 *  -- hostName is not strictly numeric form (1.2.3.4)
	 */
	if(!match && !dnsNameFound && !tpIsNumeric(hostName, hostNameLen)) {
		bool fieldFound;
		match = tpCompareSubjectName(*leaf, SN_CommonName, hostName, hostNameLen,
			fieldFound);
	}
	certGroup.alloc().free(hostName);	
	if(match) {
		return CSSM_OK;
	}
	else {
		leaf->addStatusCode(CSSMERR_APPLETP_HOSTNAME_MISMATCH);
		return CSSMERR_TP_VERIFY_ACTION_FAILED;
	}
}

/*
 * Verify SMIME options. 
 */
#define CE_CIPHER_MASK	(~(CE_KU_EncipherOnly | CE_KU_DecipherOnly))

static CSSM_RETURN tp_verifySmimeOpts(
	TPCertGroup &certGroup,
	const CSSM_DATA *smimeFieldOpts,
	const iSignCertInfo &leafCertInfo)
{
	/* 
	 * First validate optional S/MIME options.
	 */
	CSSM_APPLE_TP_SMIME_OPTIONS *smimeOpts = NULL;
	if(smimeFieldOpts != NULL) {
		smimeOpts = (CSSM_APPLE_TP_SMIME_OPTIONS *)smimeFieldOpts->Data;
	}
	if(smimeOpts != NULL) {
		switch(smimeOpts->Version) {
			case CSSM_APPLE_TP_SMIME_OPTS_VERSION:
				if(smimeFieldOpts->Length != 
						sizeof(CSSM_APPLE_TP_SMIME_OPTIONS)) {
					return CSSMERR_TP_INVALID_POLICY_IDENTIFIERS;
				}
				break;
			/* handle backwards compatibility here if necessary */
			default:
				return CSSMERR_TP_INVALID_POLICY_IDENTIFIERS;
		}
	}
	
	TPCertInfo *leaf = certGroup.certAtIndex(0);
	assert(leaf != NULL);

	/* Verify optional email address */
	unsigned emailLen = 0;
	if(smimeOpts != NULL) {
		emailLen = smimeOpts->SenderEmailLen;
	}
	bool emailFoundInSAN = false;
	if(emailLen != 0) {
		if(smimeOpts->SenderEmail == NULL) {
			return CSSMERR_TP_INVALID_POINTER;
		}

		/* normalize caller's email string */
		char *email = (char *)certGroup.alloc().malloc(emailLen);
		memmove(email, smimeOpts->SenderEmail, emailLen);
		tpNormalizeAddrSpec(email, emailLen);
		
		CSSM_BOOL match = false;
		
		/* 
		 * First check subjectAltName. The emailFound bool indicates
		 * that *some* email address was found, regardless of a match
		 * condition.
		 */
		bool dummy;
		match = tpCompareSubjectAltName(leafCertInfo.subjectAltName, 
			email, emailLen, 
			SAN_Email, dummy, emailFoundInSAN);
		
		/* 
		 * Then subject DN, CSSMOID_EmailAddress, if no match from 
		 * subjectAltName
		 */
		bool emailFoundInDn = false;
		if(!match) {
			match = tpCompareSubjectName(*leaf, SN_Email, email, emailLen,
				emailFoundInDn);
		}
		certGroup.alloc().free(email);	
		
		/*
		 * Error here only if no match found but there was indeed *some*
		 * email address in the cert.
		 */
		if(!match && (emailFoundInSAN || emailFoundInDn)) {
			leaf->addStatusCode(CSSMERR_APPLETP_SMIME_EMAIL_ADDRS_NOT_FOUND);
			tpPolicyError("SMIME email addrs in cert but no match");
			return CSSMERR_TP_VERIFY_ACTION_FAILED;
		}
	}
	
	/*
	 * Going by the letter of the law, here's what RFC 2632 has to say
	 * about the legality of an empty Subject Name:
	 *
	 *    ...the subject DN in a user's (i.e. end-entity) certificate MAY 
	 *    be an empty SEQUENCE in which case the subjectAltName extension 
	 *    will include the subject's identifier and MUST be marked as 
	 *    critical.
	 *
	 * OK, first examine the leaf cert's subject name.
	 */
	CSSM_RETURN crtn;
	CSSM_DATA_PTR subjNameData = NULL;
	crtn = leaf->fetchField(&CSSMOID_X509V1SubjectNameCStruct, &subjNameData);
	if(crtn) {
		/* This should really never happen */
		tpPolicyError("SMIME policy: error fetching subjectName");
		leaf->addStatusCode(CSSMERR_TP_INVALID_CERTIFICATE);
		return CSSMERR_TP_INVALID_CERTIFICATE;
	}
	/* must do a leaf->freeField(&CSSMOID_X509V1SubjectNameCStruct on exit */
	
	const CSSM_X509_NAME *x509Name = (const CSSM_X509_NAME *)subjNameData->Data;
	if(x509Name->numberOfRDNs == 0) {
		/* 
		 * Empty subject name. If we haven't already seen a valid 
		 * email address in the subject alternate name (by looking
		 * for a specific address specified by app), try to find 
		 * one now.
		 */
		if(!emailFoundInSAN &&		// haven't found one, and
		   (emailLen == 0)) {		// didn't even look yet
			bool dummy;
			tpCompareSubjectAltName(leafCertInfo.subjectAltName, 
					NULL, 0,				// email, emailLen, 
					SAN_Email, dummy, 
					emailFoundInSAN);		// the variable we're updating
		}
		if(!emailFoundInSAN) {
			tpPolicyError("SMIME policy fail: empty subject name and "
				"no Email Addrs in SubjectAltName");
			leaf->addStatusCode(CSSMERR_APPLETP_SMIME_NO_EMAIL_ADDRS);
			leaf->freeField(&CSSMOID_X509V1SubjectNameCStruct, subjNameData);
			return CSSMERR_TP_VERIFY_ACTION_FAILED;
		}
		
		/*
		 * One more thing: this leaf must indeed have a subjAltName
		 * extension and it must be critical. We would not have gotten this
		 * far if the subjAltName extension was not actually present....
		 */
		assert(leafCertInfo.subjectAltName.present);
		if(!leafCertInfo.subjectAltName.critical) {
			tpPolicyError("SMIME policy fail: empty subject name and "
				"no Email Addrs in SubjectAltName");
			leaf->addStatusCode(CSSMERR_APPLETP_SMIME_SUBJ_ALT_NAME_NOT_CRIT);
			leaf->freeField(&CSSMOID_X509V1SubjectNameCStruct, subjNameData);
			return CSSMERR_TP_VERIFY_ACTION_FAILED;
		}
	}
	leaf->freeField(&CSSMOID_X509V1SubjectNameCStruct, subjNameData);
	 
	/*
	 * Enforce the usage of the key associated with the leaf cert.
	 * Cert's KeyUsage must be a superset of what the app is trying to do.
	 * Note the {en,de}cipherONly flags are handledÊseparately....
	 */
	const iSignExtenInfo &kuInfo = leafCertInfo.keyUsage;
	if(kuInfo.present) {
		CE_KeyUsage certKu = *((CE_KeyUsage *)kuInfo.extnData);
		CE_KeyUsage appKu = smimeOpts->IntendedUsage;
		CE_KeyUsage intersection = certKu & appKu;
		if((intersection & CE_CIPHER_MASK) != (appKu & CE_CIPHER_MASK)) {
			tpPolicyError("SMIME KeyUsage err: appKu 0x%x  certKu 0x%x",
				appKu, certKu);
			leaf->addStatusCode(CSSMERR_APPLETP_SMIME_BAD_KEY_USE);
			return CSSMERR_TP_VERIFY_ACTION_FAILED;
		}
		
		/* Now the en/de cipher only bits - for keyAgreement only */
		if(appKu & CE_KU_KeyAgreement) {
			/* 
			 * 1. App wants to use this for key agreement; it must
			 *    say what it wants to do with the derived key.
			 *    In this context, the app's XXXonly bit means that
			 *    it wants to use the key for that op - not necessarliy
			 *    "only". 
			 */
			if((appKu & (CE_KU_EncipherOnly | CE_KU_DecipherOnly)) == 0) {
				tpPolicyError("SMIME KeyUsage err: KeyAgreement with "
					"no Encipher or Decipher");
				leaf->addStatusCode(CSSMERR_APPLETP_SMIME_BAD_KEY_USE);
				return CSSMERR_TP_VERIFY_ACTION_FAILED;
			}
			
			/*
			 * 2. If cert restricts to encipher only make sure the
			 *    app isn't trying to decipher.
			 */
			if((certKu & CE_KU_EncipherOnly) && 
			   (appKu & CE_KU_DecipherOnly)) {
				tpPolicyError("SMIME KeyUsage err: cert EncipherOnly, "
					"app wants to decipher");
				leaf->addStatusCode(CSSMERR_APPLETP_SMIME_BAD_KEY_USE);
				return CSSMERR_TP_VERIFY_ACTION_FAILED;
			}
			
			/*
			 * 3. If cert restricts to decipher only make sure the
			 *    app isn't trying to encipher.
			 */
			if((certKu & CE_KU_DecipherOnly) && 
			   (appKu & CE_KU_EncipherOnly)) {
				tpPolicyError("SMIME KeyUsage err: cert DecipherOnly, "
					"app wants to encipher");
				leaf->addStatusCode(CSSMERR_APPLETP_SMIME_BAD_KEY_USE);
				return CSSMERR_TP_VERIFY_ACTION_FAILED;
			}
		}
	}
	
	/*
	 * Ensure that, if an extendedKeyUsage extension is present in the 
	 * leaf, that either emailProtection or anyExtendedKeyUsage usages is present
	 */
	const iSignExtenInfo &ekuInfo = leafCertInfo.extendKeyUsage;
	if(ekuInfo.present) {
		bool foundGoodEku = false;
		CE_ExtendedKeyUsage *eku = (CE_ExtendedKeyUsage *)ekuInfo.extnData;
		assert(eku != NULL);
		for(unsigned i=0; i<eku->numPurposes; i++) {
			if(tpCompareOids(&eku->purposes[i], &CSSMOID_EmailProtection)) {
				foundGoodEku = true;
				break;
			}
			if(tpCompareOids(&eku->purposes[i], &CSSMOID_ExtendedKeyUsageAny)) {
				foundGoodEku = true;
				break;
			}
		}
		if(!foundGoodEku) {
			leaf->addStatusCode(CSSMERR_APPLETP_SMIME_BAD_EXT_KEY_USE);
			return CSSMERR_TP_VERIFY_ACTION_FAILED;
		}
	}
	 
	return CSSM_OK;
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
 * RFC 2632, "S/MIME Version 3 Certificate Handling", section
 * 4.4.2, says that KeyUsage extensions MUST be flagged critical, 
 * but Thawte's intermediate cert (common namd "Thawte Personal 
 * Freemail Issuing CA" does not meet this requirement.
 */
#define SMIME_KEY_USAGE_MUST_BE_CRITICAL		0

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
	CSSM_APPLE_TP_ACTION_FLAGS		actionFlags,
	const CSSM_DATA					*policyFieldData,	// optional
	void							*policyOpts)		// future options
{
	iSignCertInfo 			*certInfo = NULL;
	uint32					numCerts;
	iSignCertInfo			*thisCertInfo;
	uint16					expUsage;
	uint16					actUsage;
	unsigned				certDex;
	CSSM_BOOL				cA = CSSM_FALSE;		// init for compiler warning
	bool					isLeaf;					// end entity
	bool					isRoot;					// root cert
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
			return CSSMERR_TP_VERIFY_ACTION_FAILED;
		}
		if(numCerts <= 1) {
			/* nope, not for iSign */
			return CSSMERR_TP_VERIFY_ACTION_FAILED;
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
			tpPolicyError("tp_policyVerify: critical flag in unknown "
				"extension");
			thisTpCertInfo->addStatusCode(CSSMERR_APPLETP_UNKNOWN_CRITICAL_EXTEN);
			policyFail = CSSM_TRUE;
		}
		
		/* 
		 * Note it's possible for both of these to be true, for a 
		 * of length one (kTPx509Basic, kCrlPolicy only!)
		 * FIXME: should this code work of the last cert in the chain is
		 * NOT a root?
		 */
		isLeaf = thisTpCertInfo->isLeaf();
		isRoot = thisTpCertInfo->isSelfSigned();
			
		/*
		 * BasicConstraints.cA
		 * iSign:   	 required in all but leaf and root,
		 *          	 for which it is optional (with default values of false
		 *         	 	 for leaf and true for root).
		 * kTPx509Basic,
		 * kTP_SSL,
		 * kTP_SMIME     always optional, default of false for leaf and
		 *				 true for others
		 * All:     	 cA must be false for leaf, true for others
		 */
		if(!thisCertInfo->basicConstraints.present) {
			/*
			 * No basicConstraints present; infer a cA value if appropriate.
			 */
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
					case kCrlPolicy:
					case kTP_SMIME:
						/* 
						 * not present, not leaf, not root.... 
						 * ....RFC2459 says this can not be a CA 
						 */
						cA = CSSM_FALSE;
						break;
					case kTPiSign:
						/* required for iSign in this position */
						tpPolicyError("tp_policyVerify: no "
								"basicConstraints");
						policyFail = CSSM_TRUE;
						thisTpCertInfo->addStatusCode(
							CSSMERR_APPLETP_NO_BASIC_CONSTRAINTS);
						break;
					default:
						/* not reached */
						assert(0);
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
				tpPolicyError("tp_policyVerify: basicConstraints marked "
					"not critical");
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
					tpPolicyError("tp_policyVerify: pathLenConstraint "
						"exceeded");
					policyFail = CSSM_TRUE;
					thisTpCertInfo->addStatusCode(
							CSSMERR_APPLETP_PATH_LEN_CONSTRAINT);
				}
			}
		}
		
		if(isLeaf) {
			/* 
			 * Special cases to allow a chain of length 1, leaf and root 
			 * both true, and for caller to override the "leaf can't be a CA"
			 * requirement when a CA cert is explicitly being evaluated as the 
			 * leaf.
			 */
			if(cA && !isRoot && 
			   !(actionFlags & CSSM_TP_ACTION_LEAF_IS_CA)) {
				tpPolicyError("tp_policyVerify: cA true for leaf");
				policyFail = CSSM_TRUE;
				thisTpCertInfo->addStatusCode(CSSMERR_APPLETP_INVALID_CA);
			}
		} else if(!cA) {
			tpPolicyError("tp_policyVerify: cA false for non-leaf");
			policyFail = CSSM_TRUE;
			thisTpCertInfo->addStatusCode(CSSMERR_APPLETP_INVALID_CA);
		}
		
		/*
		 * Authority Key Identifier optional
		 * iSign   		: only allowed in !root. 
		 *           	  If present, must not be critical.
		 * kTPx509Basic,
		 * kTP_SSL,
		 * kTP_SMIME    : ignored (though used later for chain verification)
		 */ 
		if((policy == kTPiSign) && thisCertInfo->authorityId.present) {
			if(isRoot) {
				tpPolicyError("tp_policyVerify: authorityId in root");
				policyFail = CSSM_TRUE;
				thisTpCertInfo->addStatusCode(CSSMERR_APPLETP_INVALID_AUTHORITY_ID); 
			}
			if(thisCertInfo->authorityId.critical) {
				/* illegal per RFC 2459 */
				tpPolicyError("tp_policyVerify: authorityId marked "
					"critical");
				policyFail = CSSM_TRUE;
				thisTpCertInfo->addStatusCode(CSSMERR_APPLETP_INVALID_AUTHORITY_ID); 
			}
		}

		/*
		 * Subject Key Identifier optional 
		 * iSign   		 : can't be critical. 
		 * kTPx509Basic,
		 * kTP_SSL,
		 * kTP_SMIME     : ignored (though used later for chain verification)
		 */ 
		if(thisCertInfo->subjectId.present) {
			if((policy == kTPiSign) && thisCertInfo->subjectId.critical) {
				tpPolicyError("tp_policyVerify: subjectId marked critical");
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
		 * kTPx509Basic,
		 * kTP_SSL,
		 * kTP_SMIME,   : non-leaf  : usage = keyCertSign
		 *			  	  Leaf: don't care
		 * kCrlPolicy   : Leaf: usage = CRLSign
		 * kTP_SMIME   	: if present, must be critical
		 */ 
		if(thisCertInfo->keyUsage.present) {
			/*
			 * Leaf cert: usage = digitalSignature
			 * Others:    usage = keyCertSign
			 * We only require that one bit to be set, we ignore others. 
			 */
			if(isLeaf) {
				switch(policy) {
					case kTPiSign:
						expUsage = CE_KU_DigitalSignature;
						break;
					case kCrlPolicy:
						/* if present, this bit must be set */
						expUsage = CE_KU_CRLSign;
						break;
					default:
						/* hack to accept whatever's there */
						expUsage = thisCertInfo->keyUsage.extnData->keyUsage;
						break;
				}
			}
			else {
				/* this is true for all policies */
				expUsage = CE_KU_KeyCertSign;
			}
			actUsage = thisCertInfo->keyUsage.extnData->keyUsage;
			if(!(actUsage & expUsage)) {
				tpPolicyError("tp_policyVerify: bad keyUsage (leaf %s; "
					"usage 0x%x)",
					(certDex == 0) ? "TRUE" : "FALSE", actUsage);
				policyFail = CSSM_TRUE;
				thisTpCertInfo->addStatusCode(CSSMERR_APPLETP_INVALID_KEY_USAGE); 
			}
			#if 0
			if((policy == kTP_SMIME) && !thisCertInfo->keyUsage.critical) {
				/*
				 * Per Radar 3410245, allow this for intermediate certs.
				 */
				if(SMIME_KEY_USAGE_MUST_BE_CRITICAL || isLeaf || isRoot) {
					tpPolicyError("tp_policyVerify: key usage, !critical, SMIME");
					policyFail = CSSM_TRUE;
					thisTpCertInfo->addStatusCode(CSSMERR_APPLETP_SMIME_KEYUSAGE_NOT_CRITICAL);
				}
			}
			#endif
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
					tpPolicyError("tp_policyVerify: netscape-cert-type, "
						"!ObjectSign");
					policyFail = CSSM_TRUE;
					thisTpCertInfo->addStatusCode(CSSMERR_APPLETP_INVALID_KEY_USAGE);
				}
			}
			else if(!isRoot) {
				tpPolicyError("tp_policyVerify: !isRoot, no keyUsage, "
					"!(leaf and netscapeCertType)");
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
			tpPolicyError("tp_policyVerify: bad extendUsage->numPurposes "
				"(%d)",
				(int)extendUsage->numPurposes);
			policyFail = CSSM_TRUE;
			(certGroup->certAtIndex(0))->addStatusCode(
				CSSMERR_APPLETP_INVALID_EXTENDED_KEY_USAGE); 
		}
		if(!tpCompareOids(extendUsage->purposes,
				&CSSMOID_ExtendedUseCodeSigning)) {
			tpPolicyError("tp_policyVerify: bad extendKeyUsage");
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
			tpPolicyError("tp_policyVerify: bad key ID linkage");
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
		CSSM_RETURN cerr = tp_verifySslOpts(*certGroup, policyFieldData,
			certInfo[0]);
		if(cerr) {
			policyFail = CSSM_TRUE;
		}
	}
	
	/* S/MIME */
	if(policy == kTP_SMIME) {
		CSSM_RETURN cerr = tp_verifySmimeOpts(*certGroup, policyFieldData,
			certInfo[0]);
		if(cerr) {
			policyFail = CSSM_TRUE;
		}
	}
	
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
