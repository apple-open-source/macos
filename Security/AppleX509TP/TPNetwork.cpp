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


/*
 * TPNetwork.h - LDAP, HTTP and (eventually) other network tools 
 *
 * Written 10/3/2002 by Doug Mitchell.
 */
 
#include "TPNetwork.h"
#include "tpdebugging.h"
#include <Security/cssmtype.h>
#include <Security/cssmapple.h>
#include <Security/oidscert.h>
#include <Security/logging.h>
/* Unix-y fork and file stuff */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

/* normally, crlrefresh exec'd from here */
#define CRL_FETCH_TOOL	"/usr/bin/crlrefresh"

/* !NDEBUG, this env var optionally points to crlrefresh */
#define CRL_FETCH_ENV	"TP_CRLREFRESH"

#define CRL_RBUF_SIZE	1024		/* read this much at a time from pipe */

typedef enum {
	LT_Crl = 1,
	LT_Cert
} LF_Type;

static CSSM_RETURN tpFetchViaNet(
	const CSSM_DATA &url,
	LF_Type 		lfType,
	CssmAllocator	&alloc,
	CSSM_DATA		&attrBlob)		// mallocd and RETURNED
{
	char *arg1;
	int status;
	
	switch(lfType) {
		case LT_Crl:
			arg1 = "f";
			break;
		case LT_Cert:
			arg1 = "F";
			break;
		default:
			return CSSMERR_TP_INTERNAL_ERROR;
	}

	/* create pipe to catch CRL_FETCH_TOOL's output */
	int pipeFds[2];
	status = pipe(pipeFds);
	if(status) {
		tpErrorLog("tpFetchViaNet: pipe error %d\n", errno);
		return CSSMERR_TP_REQUEST_LOST;
	}
	
	pid_t pid = fork();
	if(pid < 0) {
		tpErrorLog("tpFetchViaNet: fork error %d\n", errno);
		return CSSMERR_TP_REQUEST_LOST;
	}
	if(pid == 0) {
		/* child: run CRL_FETCH_TOOL */
				
		/* don't assume URL string is NULL terminated */
		char *urlStr;
		if(url.Data[url.Length - 1] == '\0') {
			urlStr = (char *)url.Data;
		}
		else {
			urlStr = (char *)alloc.malloc(url.Length + 1);
			memmove(urlStr, url.Data, url.Length);
			urlStr[url.Length] = '\0';
		}
	
		/* set up pipeFds[1] as stdout for CRL_FETCH_TOOL */
		status = dup2(pipeFds[1], STDOUT_FILENO);
		if(status < 0) {
			tpErrorLog("tpFetchViaNet: dup2 error %d\n", errno);
			_exit(1);
		}
		close(pipeFds[0]);
		close(pipeFds[1]);
		
		char *crlFetchTool = CRL_FETCH_TOOL;
		#ifndef	NDEBUG
		char *cft = getenv(CRL_FETCH_ENV);
		if(cft) {
			crlFetchTool = cft;
		}
		#endif	/* NDEBUG */
		
		/* here we go */
		execl(crlFetchTool, CRL_FETCH_TOOL, arg1, urlStr, NULL);
		
		/* only get here on error */
		Syslog::error("TPNetwork: exec returned %d errno %d", status, errno);
		/* we are the child... */
		_exit(1);
	}

	/* parent - resulting blob comes in on pipeFds[0] */
	close(pipeFds[1]);
	int thisRead = 0;
	int totalRead = 0;
	char inBuf[CRL_RBUF_SIZE];
	attrBlob.Data = NULL;
	attrBlob.Length = 0;	// buf size until complete, then actual size of 
							//   good data
	CSSM_RETURN crtn = CSSM_OK;
	
	do {
		thisRead = read(pipeFds[0], inBuf, CRL_RBUF_SIZE);
		if(thisRead < 0) {
			switch(errno) {
				case EINTR:
					/* try some more */
					continue;
				default:
					tpErrorLog("tpFetchViaNet: read from child error %d\n", errno);
					crtn = CSSMERR_TP_REQUEST_LOST;
					break;
			}
			if(crtn) {
				break;
			}
		}
		if(thisRead == 0) {
			/* normal termination */
			attrBlob.Length = totalRead;
			break;
		}
		if(attrBlob.Length < (unsigned)(totalRead + thisRead)) {
			uint32 newLen = attrBlob.Length + CRL_RBUF_SIZE;
			attrBlob.Data = (uint8 *)alloc.realloc(attrBlob.Data, newLen);
			attrBlob.Length = newLen;
				
		}
		memmove(attrBlob.Data + totalRead, inBuf, thisRead);
		totalRead += thisRead;
	} while(1);
	
	close(pipeFds[0]);
	
	/* ensure child exits */
	pid_t rtnPid;
	do {
		rtnPid = wait4(pid, &status, 0 /* options */, NULL /* rusage */);
		if(rtnPid == pid) {
			if(!WIFEXITED(status) || (WEXITSTATUS(status) != 0)) {
				tpErrorLog("tpFetchViaNet: bad exit status from child\n");
				crtn = CSSMERR_TP_REQUEST_LOST;
			}
			/* done */
			break;
		}
		else if(rtnPid < 0) {
			if(errno == EINTR) {
				/* try again */
				continue;
			}
			/* hosed */
			tpErrorLog("tpFetchViaNet: wait4 error %d\n", errno);
			crtn = CSSMERR_TP_REQUEST_LOST;
			break;
		}
		else {
			tpErrorLog("tpFetchViaNet: wait4 returned %d\n", rtnPid);
				crtn = CSSMERR_TP_REQUEST_LOST;
		}
	} while(1);

	return crtn;
}

static CSSM_RETURN tpCrlViaNet(
	const CSSM_DATA &url,
	TPCrlVerifyContext &vfyCtx,
	TPCertInfo &forCert,		// for verifyWithContext
	TPCrlInfo *&rtnCrl)
{
	TPCrlInfo *crl = NULL;
	CSSM_DATA crlData;
	CSSM_RETURN crtn;
	CssmAllocator &alloc = CssmAllocator::standard();
	
	crtn = tpFetchViaNet(url, LT_Crl, alloc, crlData);
	if(crtn) {
		return crtn;
	}
	try {
		crl = new TPCrlInfo(vfyCtx.clHand,
			vfyCtx.cspHand,
			&crlData,
			TIC_CopyData,
			vfyCtx.verifyTime);		// cssmTimeStr FIMXE - do we need this?
	}
	catch(...) {
		alloc.free(crlData.Data);
		rtnCrl = NULL;
		return CSSMERR_APPLETP_CRL_NOT_FOUND;
	}
	alloc.free(crlData.Data);
	
	/* full CRL verify */
	crtn = crl->verifyWithContext(vfyCtx, &forCert);
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
	CssmAllocator &alloc = CssmAllocator::standard();
	
	crtn = tpFetchViaNet(url, LT_Cert, alloc, certData);
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
	TPCrlVerifyContext		*verifyContext,		// only for CRLs
	CSSM_CL_HANDLE			clHand,				// only for certs
	CSSM_CSP_HANDLE			cspHand,			// only for certs
	const char				*verifyTime,		// only for certs, optional
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
	TPCrlVerifyContext	&vfyCtx,
	TPCrlInfo			*&crl)			// RETURNED
{
	/* does the cert have a cRlDistributionPoint? */
	CSSM_DATA_PTR fieldValue;			// mallocd by CL
	
	if(vfyCtx.verifyTime != NULL) {
		tpErrorLog("***tpFetchCrlFromNet: don't know how to time travel\n");
		return CSSMERR_APPLETP_CRL_NOT_FOUND;
	}
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
					"CE_CDNT_NameRelativeToCrlIssuerÊnot implemented\n");
				break;
				
			case CE_CDNT_FullName:
			{
				CE_GeneralNames *names = dp->distPointName->fullName;
				crtn = tpFetchViaGeneralNames(names,
					cert,
					&vfyCtx,
					0,			// clHand, use the one in vfyCtx
					0,			// cspHand, ditto
					NULL,		// verifyTime - in vfyCtx
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
		if(crtn) {
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
	/* does the cert have a issuerAltName? */
	CSSM_DATA_PTR fieldValue;			// mallocd by CL
	
	if(verifyTime != NULL) {
		tpErrorLog("***tpFetchIssuerFromNet: don't know how to time travel\n");
		return CSSMERR_TP_CERTGROUP_INCOMPLETE;
	}
	CSSM_RETURN crtn = subject.fetchField(&CSSMOID_IssuerAltName,
		&fieldValue);
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
	
	crtn = tpFetchViaGeneralNames(names,
					subject,
					NULL,		// verifyContext
					clHand,
					cspHand,
					verifyTime,
					&rtnCert,
					NULL);
	subject.freeField(&CSSMOID_IssuerAltName,	fieldValue);
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


