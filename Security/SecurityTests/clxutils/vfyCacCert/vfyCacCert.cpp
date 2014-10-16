/*
 * Verify one CAC cert using standard system-wide cert and CRL keychains.
 */
#include <stdlib.h>
#include <stdio.h>
#include <security_cdsa_utils/cuFileIo.h>
#include <Security/cuCdsaUtils.h>	/* private */
//#include "clutils.h"
#include <Security/cssmerrno.h> 	/* private cssmErrorString() */
#include <Security/cssm.h>
#include <Security/oidsalg.h>
#include <Security/SecTrust.h>

/* 
 * More-or-less hard-coded locations of system-wide keychains containing
 * intermediate certs (which are required for this operation) and
 * CRLs (which is optional; it's just a cache for performance reasons).
 */
#define X509_CERT_DB	"/System/Library/Keychains/X509Certificates"
#define X509_CRL_DB		"/private/var/db/crls/crlcache.db"

static void usage(char **argv)
{
	printf("Usage: %s certFileName [options]\n", argv[0]);
	printf("Options:\n");
	printf("   a   allow unverified certs\n");
	printf("   d   disable CRL verification\n");
	printf("   n   no network fetch of CRLs\n");
	exit(1);
}

/*** Display verify results ***/

static void statusBitTest(
	CSSM_TP_APPLE_CERT_STATUS certStatus, 
	uint32 bit,
	const char *str)
{
	if(certStatus & bit) {
		printf("%s  ", str);
	}
}

static void printCertInfo(
	unsigned numCerts,							// from CertGroup
	const CSSM_TP_APPLE_EVIDENCE_INFO *info)
{
	CSSM_TP_APPLE_CERT_STATUS cs;
	
	for(unsigned i=0; i<numCerts; i++) {
		const CSSM_TP_APPLE_EVIDENCE_INFO *thisInfo = &info[i];
		cs = thisInfo->StatusBits;
		printf("   cert %u:\n", i);
		printf("      StatusBits     : 0x%x", (unsigned)cs);
		if(cs) {
			printf(" ( ");
			statusBitTest(cs, CSSM_CERT_STATUS_EXPIRED, "EXPIRED");
			statusBitTest(cs, CSSM_CERT_STATUS_NOT_VALID_YET, 
				"NOT_VALID_YET");
			statusBitTest(cs, CSSM_CERT_STATUS_IS_IN_INPUT_CERTS, 
				"IS_IN_INPUT_CERTS");
			statusBitTest(cs, CSSM_CERT_STATUS_IS_IN_ANCHORS, 
				"IS_IN_ANCHORS");
			statusBitTest(cs, CSSM_CERT_STATUS_IS_ROOT, "IS_ROOT");
			statusBitTest(cs, CSSM_CERT_STATUS_IS_FROM_NET, "IS_FROM_NET");
			printf(")\n");
		}
		else {
			printf("\n");
		}
		printf("      NumStatusCodes : %u ",
			thisInfo->NumStatusCodes);
		for(unsigned j=0; j<thisInfo->NumStatusCodes; j++) {
			printf("%s  ", 
				cssmErrorString(thisInfo->StatusCodes[j]).c_str());
		}
		printf("\n");
		printf("      Index: %u\n", thisInfo->Index);
	}
	return;
}

/* we really only need CSSM_EVIDENCE_FORM_APPLE_CERT_INFO */
#define SHOW_ALL_VFY_RESULTS		0

static void dumpVfyResult(
	const CSSM_TP_VERIFY_CONTEXT_RESULT *vfyResult)
{
	unsigned numEvidences = vfyResult->NumberOfEvidences;
	unsigned numCerts = 0;
	printf("Returned evidence:\n");
	for(unsigned dex=0; dex<numEvidences; dex++) {
		CSSM_EVIDENCE_PTR ev = &vfyResult->Evidence[dex];
		#if SHOW_ALL_VFY_RESULTS
		printf("   Evidence %u:\n", dex);
		#endif
		switch(ev->EvidenceForm) {
			case CSSM_EVIDENCE_FORM_APPLE_HEADER:
			{
				#if SHOW_ALL_VFY_RESULTS
				const CSSM_TP_APPLE_EVIDENCE_HEADER *hdr = 
					(const CSSM_TP_APPLE_EVIDENCE_HEADER *)(ev->Evidence);
				printf("      Form = HEADER; Version = %u\n", hdr->Version);
				#endif
				break;
			}
			case CSSM_EVIDENCE_FORM_APPLE_CERTGROUP:
			{
				const CSSM_CERTGROUP *grp = (const CSSM_CERTGROUP *)ev->Evidence;
				numCerts = grp->NumCerts;
				#if SHOW_ALL_VFY_RESULTS
				/* parse the rest of this eventually */
				/* Note we depend on this coming before the CERT_INFO */
				printf("      Form = CERTGROUP; numCerts = %u\n", numCerts);
				#endif
				break;
			}
			case CSSM_EVIDENCE_FORM_APPLE_CERT_INFO:	
			{
				const CSSM_TP_APPLE_EVIDENCE_INFO *info = 
					(const CSSM_TP_APPLE_EVIDENCE_INFO *)ev->Evidence;
				printCertInfo(numCerts, info);
				break;
			}
			default:
				printf("***UNKNOWN Evidence form (%u)\n", 
					(unsigned)ev->EvidenceForm);
				break;
		}
	}
}

/* free a CSSM_CERT_GROUP */ 
void tpFreeCertGroup(
	CSSM_CERTGROUP_PTR	certGroup,
	CSSM_BOOL	 		freeCertData,	// free individual CertList.Data 
	CSSM_BOOL			freeStruct)		// free the overall CSSM_CERTGROUP
{
	unsigned dex;
	
	if(certGroup == NULL) {
		return;	
	}
	
	if(freeCertData) {
		/* free the individual cert Data fields */
		for(dex=0; dex<certGroup->NumCerts; dex++) {
			APP_FREE(certGroup->GroupList.CertList[dex].Data);
		}
	}

	/* and the array of CSSM_DATAs */
	if(certGroup->GroupList.CertList) {
		APP_FREE(certGroup->GroupList.CertList);
	}
	
	if(freeStruct) {
		APP_FREE(certGroup);
	}
}


/*
 * Free the contents of a CSSM_TP_VERIFY_CONTEXT_RESULT returned from
 * CSSM_TP_CertGroupVerify().
 */
CSSM_RETURN freeVfyResult(
	CSSM_TP_VERIFY_CONTEXT_RESULT *ctx)
{
	int numCerts = -1;
	CSSM_RETURN crtn = CSSM_OK;
	
	for(unsigned i=0; i<ctx->NumberOfEvidences; i++) {
		CSSM_EVIDENCE_PTR evp = &ctx->Evidence[i];
		switch(evp->EvidenceForm) {
			case CSSM_EVIDENCE_FORM_APPLE_HEADER:
				/* Evidence = (CSSM_TP_APPLE_EVIDENCE_HEADER *) */
				APP_FREE(evp->Evidence);
				evp->Evidence = NULL;
				break;
			case CSSM_EVIDENCE_FORM_APPLE_CERTGROUP:
			{
				/* Evidence = CSSM_CERTGROUP_PTR */
				CSSM_CERTGROUP_PTR cgp = (CSSM_CERTGROUP_PTR)evp->Evidence;
				numCerts = cgp->NumCerts;	
				tpFreeCertGroup(cgp, CSSM_TRUE, CSSM_TRUE);
				evp->Evidence = NULL;
				break;
			}
			case CSSM_EVIDENCE_FORM_APPLE_CERT_INFO:
			{
				/* Evidence = array of CSSM_TP_APPLE_EVIDENCE_INFO */
				if(numCerts < 0) {
					/* Haven't gotten a CSSM_CERTGROUP_PTR! */
					printf("***Malformed VerifyContextResult (2)\n");
					crtn = CSSMERR_TP_INTERNAL_ERROR;
					break;
				}
				CSSM_TP_APPLE_EVIDENCE_INFO *evInfo = 
					(CSSM_TP_APPLE_EVIDENCE_INFO *)evp->Evidence;
				for(unsigned k=0; k<(unsigned)numCerts; k++) {
					/* Dispose of StatusCodes, UniqueRecord */
					CSSM_TP_APPLE_EVIDENCE_INFO *thisEvInfo = 
						&evInfo[k];
					if(thisEvInfo->StatusCodes) {
						APP_FREE(thisEvInfo->StatusCodes);
					}
					if(thisEvInfo->UniqueRecord) {
						CSSM_RETURN crtn = 
							CSSM_DL_FreeUniqueRecord(thisEvInfo->DlDbHandle,
								thisEvInfo->UniqueRecord);
						if(crtn) {
							cuPrintError("CSSM_DL_FreeUniqueRecord", crtn);
							break;
						}
						thisEvInfo->UniqueRecord = NULL;
					}
				}	/* for each cert info */
				APP_FREE(evp->Evidence);
				evp->Evidence = NULL;
				break;
			}	/* CSSM_EVIDENCE_FORM_APPLE_CERT_INFO */
		}		/* switch(evp->EvidenceForm) */
	}			/* for each evidence */
	if(ctx->Evidence) {
		APP_FREE(ctx->Evidence);
		ctx->Evidence = NULL;
	}
	return crtn;
}

static int testError(CSSM_BOOL quiet)
{
	char resp;
	
	if(quiet) {
		printf("\n***Test aborting.\n");
		exit(1);
	}
	fpurge(stdin);
	printf("a to abort, c to continue: ");
	resp = getchar();
	return (resp == 'a');
}

/*
 * The heart of CAC certification verification.
 */
int vfyCert(
	CSSM_TP_HANDLE			tpHand, 
	CSSM_CL_HANDLE 			clHand,
	CSSM_CSP_HANDLE 		cspHand,
	const void *			certData,
	unsigned				certLength,
	
	/* these three booleans will eventually come from system preferences */
	bool					enableCrlCheck,
	bool					requireFullCrlVerify,
	bool					enableFetchFromNet,
	CSSM_DL_DB_HANDLE_PTR	certKeychain,
	CSSM_DL_DB_HANDLE_PTR	crlKeychain)
{
	/* main job is building a CSSM_TP_VERIFY_CONTEXT and its components */
	CSSM_TP_VERIFY_CONTEXT			vfyCtx;
	CSSM_TP_CALLERAUTH_CONTEXT		authCtx;
	CSSM_TP_VERIFY_CONTEXT_RESULT	vfyResult;
	
	memset(&vfyCtx, 0, sizeof(CSSM_TP_VERIFY_CONTEXT));
	memset(&authCtx, 0, sizeof(CSSM_TP_CALLERAUTH_CONTEXT));
	
	/* CSSM_TP_CALLERAUTH_CONTEXT components */
	/* 
		typedef struct cssm_tp_callerauth_context {
			CSSM_TP_POLICYINFO Policy;
			CSSM_TIMESTRING VerifyTime;
			CSSM_TP_STOP_ON VerificationAbortOn;
			CSSM_TP_VERIFICATION_RESULTS_CALLBACK CallbackWithVerifiedCert;
			uint32 NumberOfAnchorCerts;
			CSSM_DATA_PTR AnchorCerts;
			CSSM_DL_DB_LIST_PTR DBList;
			CSSM_ACCESS_CREDENTIALS_PTR CallerCredentials;
		} CSSM_TP_CALLERAUTH_CONTEXT, *CSSM_TP_CALLERAUTH_CONTEXT_PTR;
	*/
	/* two policies */
	CSSM_FIELD	policyIds[2];
	CSSM_APPLE_TP_CRL_OPTIONS crlOpts;
	policyIds[0].FieldOid = CSSMOID_APPLE_X509_BASIC;
	policyIds[0].FieldValue.Data = NULL;
	policyIds[0].FieldValue.Length = 0;
	if(enableCrlCheck) {
		policyIds[1].FieldOid = CSSMOID_APPLE_TP_REVOCATION_CRL;
		policyIds[1].FieldValue.Data = (uint8 *)&crlOpts;
		policyIds[1].FieldValue.Length = sizeof(crlOpts);
		crlOpts.Version = CSSM_APPLE_TP_CRL_OPTS_VERSION;
		crlOpts.CrlFlags = 0;
		if(requireFullCrlVerify) {
			crlOpts.CrlFlags |= CSSM_TP_ACTION_REQUIRE_CRL_PER_CERT;
		}
		if(enableFetchFromNet) {
			crlOpts.CrlFlags |= CSSM_TP_ACTION_FETCH_CRL_FROM_NET;
		}
		/* optional, may well not exist */
		/* FIXME: as of 12/4/02 this field is ignored by the TP
		 * and may well go away from the CSSM_APPLE_TP_CRL_OPTIONS
		 * struct. */
		crlOpts.crlStore = crlKeychain;
	
		authCtx.Policy.NumberOfPolicyIds = 2;
	}
	else {
		/* No CRL checking */
		authCtx.Policy.NumberOfPolicyIds = 1;
	}
	authCtx.Policy.PolicyIds = policyIds;
	authCtx.Policy.PolicyControl = NULL;
	
	authCtx.VerifyTime = NULL;
	authCtx.VerificationAbortOn = CSSM_TP_STOP_ON_POLICY;
	authCtx.CallbackWithVerifiedCert = NULL;
	
	/* anchors */
	const CSSM_DATA *anchors;
	uint32 anchorCount;
	OSStatus ortn;
	ortn = SecTrustGetCSSMAnchorCertificates(&anchors, &anchorCount);
	if(ortn) {
		printf("SecTrustGetCSSMAnchorCertificates returned %u\n", ortn);
		return -1;
	}
	authCtx.NumberOfAnchorCerts = anchorCount;
	authCtx.AnchorCerts = const_cast<CSSM_DATA_PTR>(anchors);
	
	/* DBList of intermediate certs and CRLs */
	CSSM_DL_DB_HANDLE handles[2];
	unsigned numDbs = 0;
	if(certKeychain != NULL) {
		handles[0] = *certKeychain;
		numDbs++;
	}
	if(crlKeychain != NULL) {
		handles[numDbs] = *crlKeychain;
		numDbs++;
	}
	CSSM_DL_DB_LIST dlDbList;
	dlDbList.NumHandles = numDbs;
	dlDbList.DLDBHandle = &handles[0];
	
	authCtx.DBList = &dlDbList; 
	authCtx.CallerCredentials = NULL;
	
	/* CSSM_TP_VERIFY_CONTEXT */
	vfyCtx.ActionData.Data = NULL;
	vfyCtx.ActionData.Length = 0;
	vfyCtx.Action = CSSM_TP_ACTION_DEFAULT;
	vfyCtx.Cred = &authCtx;
	
	/* cook up cert group */
	CSSM_CERTGROUP cssmCerts;
	cssmCerts.CertType = CSSM_CERT_X_509v3;
	cssmCerts.CertEncoding = CSSM_CERT_ENCODING_DER;
	cssmCerts.NumCerts = 1;
	CSSM_DATA cert;
	cert.Data = (uint8 *)certData;
	cert.Length = certLength;
	cssmCerts.GroupList.CertList = &cert;
	cssmCerts.CertGroupType = CSSM_CERTGROUP_DATA;
	
	CSSM_RETURN crtn = CSSM_TP_CertGroupVerify(tpHand,
		clHand,
		cspHand,
		&cssmCerts,
		&vfyCtx,
		&vfyResult);
	if(crtn) {
		cuPrintError("CSSM_TP_CertGroupVerify", crtn);
	}
	else {
		printf("...verify successful\n");
	}
	dumpVfyResult(&vfyResult);
	freeVfyResult(&vfyResult);
	if(crtn) {
		return testError(0);
	}
	else {
		return 0;
	}
}

int main(int argc, char **argv)
{
	int rtn;
	CSSM_RETURN crtn;
	CSSM_DL_HANDLE dlHand;
	CSSM_CSP_HANDLE cspHand;
	CSSM_CL_HANDLE clHand;
	CSSM_TP_HANDLE tpHand;
	CSSM_DL_DB_HANDLE certKeychain;
	CSSM_DL_DB_HANDLE crlKeychain;
	CSSM_DL_DB_HANDLE_PTR certKeychainPtr = NULL;
	CSSM_DL_DB_HANDLE_PTR crlKeychainPtr = NULL;
	unsigned char *certData;
	unsigned certLength;
	bool requireFullCrlVerify = true;
	bool enableFetchFromNet = true;
	bool enableCrlCheck = true;
	int arg;
	char *argp;
	
	if(argc < 2) {
		usage(argv);
	}
	for(arg=2; arg<argc; arg++) {
		argp = argv[arg];
		switch(argp[0]) {
			case 'a':
				requireFullCrlVerify = false;
				break;
			case 'd':
				enableCrlCheck = false;
				break;
			case 'n':
				enableFetchFromNet = false;
				break;
			default:
				usage(argv);
		}
	}
	
	/* in the real world all these should be verified to be nonzero */
	cspHand = cuCspStartup(CSSM_TRUE);
	clHand = cuClStartup();
	tpHand = cuTpStartup();
	dlHand = cuDlStartup();
	
	/* get the cert */
	rtn = readFile(argv[1], &certData, &certLength);
	if(rtn) {
		printf("***Error reading cert file %s\n", argv[1]);
		exit(1);
	}
	
	/* get the intermediate certs */
	crtn = CSSM_DL_DbOpen(dlHand,
		X509_CERT_DB, 
		NULL,			// DbLocation
		CSSM_DB_ACCESS_READ,
		NULL, 			// CSSM_ACCESS_CREDENTIALS *AccessCred
		NULL,			// void *OpenParameters
		&certKeychain.DBHandle);
	if(crtn) {
		cuPrintError("CSSM_DL_DbOpen", crtn);
		printf("***Error opening intermediate cert file %s. This op will"
			"probably fail.n", X509_CERT_DB);
	}
	else {
		certKeychain.DLHandle = dlHand;
		certKeychainPtr = &certKeychain;
	}
	
	/* and the CRL cache */
	crtn = CSSM_DL_DbOpen(dlHand,
		X509_CRL_DB, 
		NULL,			// DbLocation
		CSSM_DB_ACCESS_READ,
		NULL, 			// CSSM_ACCESS_CREDENTIALS *AccessCred
		NULL,			// void *OpenParameters
		&crlKeychain.DBHandle);
	if(crtn == CSSM_OK) {
		crlKeychain.DLHandle = dlHand;
		crlKeychainPtr = &crlKeychain;
	}
	
	/* go for it */
	vfyCert(tpHand, clHand, cspHand, certData, certLength, 
		enableCrlCheck, requireFullCrlVerify, enableFetchFromNet,
		certKeychainPtr, crlKeychainPtr);
		
	/* Cleanup - release handles, free certData, etc. */
	free(certData);
	if(crlKeychainPtr != NULL) {
		CSSM_DL_DbClose(crlKeychain);
	}
	if(certKeychainPtr != NULL) {
		CSSM_DL_DbClose(certKeychain);
	}
	CSSM_ModuleDetach(dlHand);
	CSSM_ModuleDetach(clHand);
	CSSM_ModuleDetach(tpHand);
	CSSM_ModuleDetach(cspHand);
	
	return 0;

}
