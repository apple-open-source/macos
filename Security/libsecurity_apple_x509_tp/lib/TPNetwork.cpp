/*
 * Copyright (c) 2002-2011 Apple Inc. All Rights Reserved.
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
 * TPNetwork.h - LDAP, HTTP and (eventually) other network tools 
 */
 
#include "TPNetwork.h"
#include "tpdebugging.h"
#include "tpTime.h"
#include "cuEnc64.h"
#include <Security/cssmtype.h>
#include <Security/cssmapple.h>
#include <Security/oidscert.h>
#include <security_utilities/logging.h>
#include <security_ocspd/ocspdClient.h>

#define CA_ISSUERS_OID              OID_PKIX, 0x30, 0x02
#define CA_ISSUERS_OID_LEN          OID_PKIX_LENGTH + 2

static const uint8 	OID_CA_ISSUERS[]      = {CA_ISSUERS_OID};
const CSSM_OID	CSSMOID_CA_ISSUERS        = {CA_ISSUERS_OID_LEN, (uint8 *)OID_CA_ISSUERS};

typedef enum {
	LT_Crl = 1,
	LT_Cert
} LF_Type;

static CSSM_RETURN tpDecodeCert(
	Allocator		&alloc,
	CSSM_DATA		&rtnBlob)		// will be reallocated if needed
{
	const unsigned char *inbuf = (const unsigned char *)rtnBlob.Data;
	unsigned inlen = rtnBlob.Length;
	unsigned char *outbuf = NULL;
	unsigned outlen = 0;
	CSSM_RETURN ortn = cuConvertPem(inbuf, inlen, &outbuf, &outlen);

	if(ortn == 0 && outbuf != NULL) {
		/* Decoded result needs to be malloc'd via input allocator */
		unsigned char *rtnP = (unsigned char *) alloc.malloc(outlen);
		if(rtnP != NULL) {
			memcpy(rtnP, outbuf, outlen);
			rtnBlob.Data = rtnP;
			rtnBlob.Length = outlen;
		}
		free(outbuf);
		alloc.free((void *)inbuf);
	}
	return ortn;
}

static CSSM_RETURN tpFetchViaNet(
	const CSSM_DATA &url,
	const CSSM_DATA *issuer,		// optional
	LF_Type 		lfType,
	CSSM_TIMESTRING verifyTime,		// CRL only
	Allocator		&alloc,
	CSSM_DATA		&rtnBlob)		// mallocd and RETURNED
{
	if(lfType == LT_Crl) {
		return ocspdCRLFetch(alloc, url, issuer,
			true, true,				// cache r/w both enable
			verifyTime, rtnBlob);
	}
	else {
		CSSM_RETURN result = ocspdCertFetch(alloc, url, rtnBlob);
		if(result == CSSM_OK) {
			/* The data might be in PEM format; if so, convert it here */
			(void)tpDecodeCert(alloc, rtnBlob);
		}
		return result;
	}
}

static CSSM_RETURN tpCrlViaNet(
	const CSSM_DATA &url,
	const CSSM_DATA *issuer,	// optional, only if cert and CRL have same issuer
	TPVerifyContext &vfyCtx,
	TPCertInfo &forCert,		// for verifyWithContext
	TPCrlInfo *&rtnCrl)
{
	TPCrlInfo *crl = NULL;
	CSSM_DATA crlData;
	CSSM_RETURN crtn;
	Allocator &alloc = Allocator::standard();
	char cssmTime[CSSM_TIME_STRLEN+1];
	
	rtnCrl = NULL;
	
	/* verifyTime: we want a CRL that's valid right now. */
	{
		StLock<Mutex> _(tpTimeLock());
		timeAtNowPlus(0, TIME_CSSM, cssmTime);
	}

	crtn = tpFetchViaNet(url, issuer, LT_Crl, cssmTime, alloc, crlData);
	if(crtn) {
		return crtn;
	}
	try {
		crl = new TPCrlInfo(vfyCtx.clHand,
			vfyCtx.cspHand,
			&crlData,
			TIC_CopyData,
			NULL); 			// verifyTime = Now
	}
	catch(...) {
		alloc.free(crlData.Data);
		
		/* 
		 * There is a slight possibility of recovering from this error. In case
		 * the CRL came from disk cache, flush the cache and try to get the CRL
		 * from the net.
		 */
		tpDebug("   bad CRL; flushing from cache and retrying"); 
		ocspdCRLFlush(url);
		crtn = tpFetchViaNet(url, issuer, LT_Crl, cssmTime, alloc, crlData);
		if(crtn == CSSM_OK) {
			try {
				crl = new TPCrlInfo(vfyCtx.clHand,
					vfyCtx.cspHand,
					&crlData,
					TIC_CopyData,
					NULL);	
				tpDebug("   RECOVERY: good CRL obtained from net"); 
			}
			catch(...) {
				alloc.free(crlData.Data);
				tpDebug("   bad CRL; recovery FAILED (1)"); 
				return CSSMERR_APPLETP_CRL_NOT_FOUND;
			}
		}
		else {
			/* it was in cache but we can't find it on the net */
			tpDebug("   bad CRL; recovery FAILED (2)"); 
			return CSSMERR_APPLETP_CRL_NOT_FOUND;
		}
	}
	alloc.free(crlData.Data);
	
	/* 
 	 * Full CRL verify.
 	 * The verify time in the TPVerifyContext is the time at which various
	 * entities (CRL and its own cert chain) are to be verified; that's
	 * NULL for "right now". The current vfyCtx.verifyTime is the time at
	 * which the cert's revocation status to be determined; this call to 
	 * verifyWithContextNow() doesn't do that. 
	 */
	crtn = crl->verifyWithContextNow(vfyCtx, &forCert);
	if(crtn == CSSM_OK) {
		crl->uri(url);
	}
	else {
		delete crl;
		crl = NULL;
	}
	rtnCrl = crl;
	return crtn;
}

static CSSM_RETURN tpIssuerCertViaNet(
	const CSSM_DATA &url,
	CSSM_CL_HANDLE	clHand,
	CSSM_CSP_HANDLE	cspHand,
	const char		*verifyTime,
	TPCertInfo 		&subject,	
	TPCertInfo 		*&rtnCert)
{
	TPCertInfo *issuer = NULL;
	CSSM_DATA certData;
	CSSM_RETURN crtn;
	Allocator &alloc = Allocator::standard();
	
	crtn = tpFetchViaNet(url, NULL, LT_Cert, NULL, alloc, certData);
	if(crtn) {
		tpErrorLog("tpIssuerCertViaNet: net fetch failed\n");
		return CSSMERR_APPLETP_CERT_NOT_FOUND_FROM_ISSUER;
	}	
	try {
		issuer = new TPCertInfo(clHand,
			cspHand,
			&certData,
			TIC_CopyData,
			verifyTime);
	}
	catch(...) {
		tpErrorLog("tpIssuerCertViaNet: bad cert via net fetch\n");
		alloc.free(certData.Data);
		rtnCert = NULL;
		return CSSMERR_APPLETP_BAD_CERT_FROM_ISSUER;
	}
	alloc.free(certData.Data);
	
	/* subject/issuer match? */
	if(!issuer->isIssuerOf(subject)) {
		tpErrorLog("tpIssuerCertViaNet: wrong issuer cert via net fetch\n");
		crtn = CSSMERR_APPLETP_BAD_CERT_FROM_ISSUER;
	}
	else {
		/* yep, do a sig verify */
		crtn = subject.verifyWithIssuer(issuer);
		if(crtn) {
			tpErrorLog("tpIssuerCertViaNet: sig verify fail for cert via net "
				"fetch\n");
			crtn = CSSMERR_APPLETP_BAD_CERT_FROM_ISSUER;
		}
	}
	if(crtn) {
		assert(issuer != NULL);
		delete issuer;
		issuer = NULL;
	}
	rtnCert = issuer;
	return crtn;
}

/*
 * Fetch a CRL or a cert via a GeneralNames.
 * Shared by cert and CRL code to avoid duplicating GeneralNames traversal
 * code, despite the awkward interface for this function. 
 */
static CSSM_RETURN tpFetchViaGeneralNames(
	const CE_GeneralNames	*names,
	TPCertInfo 				&forCert,
	const CSSM_DATA			*issuer,			// optional, and only for CRLs
	TPVerifyContext			*verifyContext,		// only for CRLs
	CSSM_CL_HANDLE			clHand,				// only for certs
	CSSM_CSP_HANDLE			cspHand,			// only for certs
	const char				*verifyTime,		// optional
	/* exactly one must be non-NULL, that one is returned */
	TPCertInfo				**certInfo,
	TPCrlInfo				**crlInfo)
{	
	assert(certInfo || crlInfo);
	assert(!certInfo || !crlInfo);
	CSSM_RETURN crtn;
	
	for(unsigned nameDex=0; nameDex<names->numNames; nameDex++) {
		CE_GeneralName *name = &names->generalName[nameDex];
		switch(name->nameType) {
			case GNT_URI:
				if(name->name.Length < 5) {
					continue;
				}
				if(strncmp((char *)name->name.Data, "ldap:", 5) &&
				   strncmp((char *)name->name.Data, "http:", 5) && 
				   strncmp((char *)name->name.Data, "https:", 6)) {
					/* eventually handle other schemes here */
					continue;
				}
				if(certInfo) {
					tpDebug("   fetching cert via net"); 
					crtn = tpIssuerCertViaNet(name->name, 
						clHand,
						cspHand,
						verifyTime,
						forCert,
						*certInfo);
				}
				else {
					tpDebug("   fetching CRL via net"); 
					assert(verifyContext != NULL);
					crtn = tpCrlViaNet(name->name, 
						issuer,
						*verifyContext,
						forCert,
						*crlInfo);
				}
				switch(crtn) {
					case CSSM_OK:
					case CSSMERR_CSP_APPLE_PUBLIC_KEY_INCOMPLETE:	// caller handles
						return crtn;
					default:
						break;
				}
				/* not found/no good; try again */
				break;
			default:
				tpCrlDebug("  tpFetchCrlFromNet: unknown"
					"nameType (%u)", (unsigned)name->nameType); 
				break;
		}	/* switch nameType */
	}	/* for each name */
	if(certInfo) {
		return CSSMERR_TP_CERTGROUP_INCOMPLETE;
	}
	else {
		return CSSMERR_APPLETP_CRL_NOT_FOUND;
	}
}

/*
 * Fetch CRL(s) from specified cert if the cert has a cRlDistributionPoint
 * extension.
 *
 * Return values:
 *   CSSM_OK - found and returned fully verified CRL 
 *   CSSMERR_APPLETP_CRL_NOT_FOUND - no CRL in cRlDistributionPoint
 *   Anything else - gross error, typically from last LDAP/HTTP attempt
 *
 * FIXME - this whole mechanism sort of falls apart if verifyContext.verifyTime
 * is non-NULL. How are we supposed to get the CRL which was valid at 
 * a specified time in the past?
 */
CSSM_RETURN tpFetchCrlFromNet(
	TPCertInfo 			&cert,
	TPVerifyContext		&vfyCtx,
	TPCrlInfo			*&crl)			// RETURNED
{
	/* does the cert have a cRlDistributionPoint? */
	CSSM_DATA_PTR fieldValue;			// mallocd by CL
	
	CSSM_RETURN crtn = cert.fetchField(&CSSMOID_CrlDistributionPoints,
		&fieldValue);
	switch(crtn) {
		case CSSM_OK:
			break;
		case CSSMERR_CL_NO_FIELD_VALUES:
			/* field not present */
			return CSSMERR_APPLETP_CRL_NOT_FOUND;
		default:
			/* gross error */
			return crtn;
	}
	if(fieldValue->Length != sizeof(CSSM_X509_EXTENSION)) {
		tpErrorLog("tpFetchCrlFromNet: malformed CSSM_FIELD");
		return CSSMERR_TP_UNKNOWN_FORMAT;
	}
	CSSM_X509_EXTENSION *cssmExt = (CSSM_X509_EXTENSION *)fieldValue->Data;
	CE_CRLDistPointsSyntax *dps = 
		(CE_CRLDistPointsSyntax *)cssmExt->value.parsedValue;
	TPCrlInfo *rtnCrl = NULL;

	/* default return if we don't find anything */
	crtn = CSSMERR_APPLETP_CRL_NOT_FOUND;
	for(unsigned dex=0; dex<dps->numDistPoints; dex++) {
		CE_CRLDistributionPoint *dp = &dps->distPoints[dex];
		if(dp->distPointName == NULL) {
			continue;
		}
		/*
		 * FIXME if this uses an indirect CRL, we need to follow the 
		 * crlIssuer field... TBD.
		 */
		switch(dp->distPointName->nameType) {
			case CE_CDNT_NameRelativeToCrlIssuer:
				/* not yet */
				tpErrorLog("tpFetchCrlFromNet: "
					"CE_CDNT_NameRelativeToCrlIssuer not implemented\n");
				break;
				
			case CE_CDNT_FullName:
			{
				/*
				 * Since we don't support indirect CRLs (yet), we always pass
				 * the cert-to-be-verified's issuer as the CRL issuer for 
				 * cache lookup.
				 */
				CE_GeneralNames *names = dp->distPointName->dpn.fullName;
				crtn = tpFetchViaGeneralNames(names,
					cert,
					cert.issuerName(),
					&vfyCtx,
					0,			// clHand, use the one in vfyCtx
					0,			// cspHand, ditto
					vfyCtx.verifyTime,	
					NULL,		
					&rtnCrl);
				break;
			}	/* CE_CDNT_FullName */
			
			default:
				/* not yet */
				tpErrorLog("tpFetchCrlFromNet: "
					"unknown distPointName->nameType (%u)\n",
						(unsigned)dp->distPointName->nameType);
				break;
		}	/* switch distPointName->nameType */
		if(crtn == CSSM_OK) {
			/* i.e., tpFetchViaGeneralNames SUCCEEDED */
			break;
		}
	}	/* for each distPoints */

	cert.freeField(&CSSMOID_CrlDistributionPoints,	fieldValue);
	if(crtn == CSSM_OK) {
		assert(rtnCrl != NULL);
		crl = rtnCrl;
	}
	return crtn;
}

/*
 * Fetch issuer cert of specified cert if the cert has an issuerAltName
 * with a URI. If non-NULL cert is returned, it has passed subject/issuer
 * name comparison and signature verification with target cert.
 *
 * Return values:
 *   CSSM_OK - found and returned issuer cert 
 *   CSSMERR_TP_CERTGROUP_INCOMPLETE - no URL in issuerAltName
 *   CSSMERR_CSP_APPLE_PUBLIC_KEY_INCOMPLETE - found and returned issuer
 *      cert, but signature verification needs subsequent retry.
 *   Anything else - gross error, typically from last LDAP/HTTP attempt
 */
CSSM_RETURN tpFetchIssuerFromNet(
	TPCertInfo			&subject,
	CSSM_CL_HANDLE		clHand,
	CSSM_CSP_HANDLE		cspHand,
	const char			*verifyTime,
	TPCertInfo			*&issuer)		// RETURNED
{
	CSSM_OID_PTR fieldOid = NULL;
	CSSM_DATA_PTR fieldValue = NULL;	// mallocd by CL
	CSSM_RETURN crtn;
	bool hasAIA = false;

	/* look for the Authority Info Access extension first */
	fieldOid = (CSSM_OID_PTR)&CSSMOID_AuthorityInfoAccess;
	crtn = subject.fetchField(fieldOid,
		&fieldValue);
	hasAIA = (crtn == CSSM_OK);
	if (!hasAIA) {
		/* fall back to Issuer Alternative Name extension */
		fieldOid = (CSSM_OID_PTR)&CSSMOID_IssuerAltName;
		crtn = subject.fetchField(fieldOid,
								  &fieldValue);
	}
	switch(crtn) {
		case CSSM_OK:
			break;
		case CSSMERR_CL_NO_FIELD_VALUES:
			/* field not present */
			return CSSMERR_TP_CERTGROUP_INCOMPLETE;
		default:
			/* gross error */
			return crtn;
	}
	if(fieldValue->Length != sizeof(CSSM_X509_EXTENSION)) {
		tpPolicyError("tpFetchIssuerFromNet: malformed CSSM_FIELD");
		return CSSMERR_TP_UNKNOWN_FORMAT;
	}
	CSSM_X509_EXTENSION *cssmExt = (CSSM_X509_EXTENSION *)fieldValue->Data;
	CE_GeneralNames *names = (CE_GeneralNames *)cssmExt->value.parsedValue;
	TPCertInfo *rtnCert = NULL;
	if (hasAIA) {	/* authority info access */
		CE_AuthorityInfoAccess *access = (CE_AuthorityInfoAccess *)cssmExt->value.parsedValue;
		for (uint32 index = 0; access && index < access->numAccessDescriptions; index++) {
			CE_AccessDescription *accessDesc = &access->accessDescriptions[index];
			CSSM_OID_PTR methodOid = (CSSM_OID_PTR)&accessDesc->accessMethod;
			/* look for the CA Issuers method */
			if(methodOid->Data != NULL && methodOid->Length == CSSMOID_CA_ISSUERS.Length &&
			   !memcmp(methodOid->Data, CSSMOID_CA_ISSUERS.Data, methodOid->Length)) {
				CE_GeneralNames aiaNames = { 1, &accessDesc->accessLocation };
				/* attempt to fetch cert from named location */
				crtn = tpFetchViaGeneralNames(&aiaNames,
											  subject,
											  NULL,		// issuer - not used
											  NULL,		// verifyContext
											  clHand,
											  cspHand,
											  verifyTime,
											  &rtnCert,
											  NULL);
				if (crtn == CSSM_OK ||
					crtn == CSSMERR_CSP_APPLE_PUBLIC_KEY_INCOMPLETE) {
					break; // got one
				}
			}
		}
		subject.freeField(fieldOid,	fieldValue);
	}
	else {  /* issuer alt name */
		crtn = tpFetchViaGeneralNames(names,
						subject,
						NULL,		// issuer - not used
						NULL,		// verifyContext
						clHand,
						cspHand,
						verifyTime,
						&rtnCert,
						NULL);
		subject.freeField(fieldOid,	fieldValue);
	}
	switch(crtn) {
		case CSSM_OK:
		case CSSMERR_CSP_APPLE_PUBLIC_KEY_INCOMPLETE:
			issuer = rtnCert;
			break;
		default:
			break;
	}
	return crtn;
}


