/*
 * certVerify.cpp - execute cert/CRL verify; display results
 */
 
#include "certVerify.h"
#include "tpUtils.h"
#include <utilLib/common.h>
#include <clAppUtils/clutils.h>
#include <clAppUtils/tpUtils.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <Security/cssm.h>
#include <Security/oidsalg.h>
#include <Security/SecTrust.h>
#include <Security/SecPolicySearch.h>
#include <Security/cssmapplePriv.h>
#include <security_cdsa_utils/cuCdsaUtils.h>
#include <Security/TrustSettingsSchema.h>

static int vfyCertErrors(
	const CSSM_TP_VERIFY_CONTEXT_RESULT *vfyResult, 
	unsigned numCertErrors, 
	const char **certErrors,	// e.g., "2:CSSMERR_TP_CERT_EXPIRED"
	CSSM_BOOL quiet)
{
	if(numCertErrors == 0) {
		return 0;
	}
	if(vfyResult->NumberOfEvidences != 3) {
		printf("***vfyCertErrors: NumberOfEvidences is %u, expect 3\n",
			(unsigned)vfyResult->NumberOfEvidences);
		return 1;
	}
	
	/* numCerts from evidence[1] */
	const CSSM_EVIDENCE *ev = &vfyResult->Evidence[1];
	const CSSM_CERTGROUP *grp = (const CSSM_CERTGROUP *)ev->Evidence;
	unsigned numCerts = grp->NumCerts;
	/* array of Apple-specific info from evidence[2] */
	ev = &vfyResult->Evidence[2];
	const CSSM_TP_APPLE_EVIDENCE_INFO *info = 
				(const CSSM_TP_APPLE_EVIDENCE_INFO *)ev->Evidence;
	int ourRtn = 0;
	
	for(unsigned dex=0; dex<numCertErrors; dex++) {
		const char *str = certErrors[dex];
		char buf[8];
		unsigned i;
		
		/* 
		 * Format is certNum:errorString
		 * first get the cert number 
		 */
		for(i=0; *str != '\0'; i++, str++) {
			if(*str == ':') {
				break;
			}
			buf[i] = *str;
		}
		if(*str != ':') {
			printf("***Bad certerror value, format is certNum:errorString\n");
			return 1;
		}
		buf[i] = '\0';
		unsigned certNum = atoi(buf);
		if(certNum > (numCerts-1)) {
			printf("***certerror specified for cert %u, but only %u certs"
				" available\n", certNum, numCerts);
			return 1;
		}
		str++;			// pts to actual desired error string now
		
		/*
		 * There may be multiple per-cert statuses in the evidence; search all
		 * looking for a match 
		 */
		const CSSM_TP_APPLE_EVIDENCE_INFO *thisInfo = &info[certNum];
		char *found = NULL;
		for(unsigned i=0; i<thisInfo->NumStatusCodes; i++) {
			CSSM_RETURN actRtn = thisInfo->StatusCodes[i];
			const char *actRtnStr = cssmErrToStr(actRtn);
			found = strstr(actRtnStr, str);
			if(found) {
				break;
			}
		}
		if(found) {
			if(!quiet) {
				printf("...%s per-cert status received as expected\n", str);
			}
		}
		else {
			printf("***Per cert status %s not found\n", str);
			ourRtn = 1;
			/* might as well keep going */
		}
	}
	return ourRtn;
}

static int vfyCertStatus(
	const CSSM_TP_VERIFY_CONTEXT_RESULT *vfyResult, 
	unsigned numCertStatus,
	const char **certStatus,	// e.g., "1:0x18", leading 0x optional
	CSSM_BOOL quiet)
{
	if(numCertStatus == 0) {
		return 0;
	}
	if(vfyResult->NumberOfEvidences != 3) {
		printf("***vfyCertStatus: NumberOfEvidences is %u, expect 3\n",
			(unsigned)vfyResult->NumberOfEvidences);
		return 1;
	}
	
	/* numCerts from evidence[1] */
	const CSSM_EVIDENCE *ev = &vfyResult->Evidence[1];
	const CSSM_CERTGROUP *grp = (const CSSM_CERTGROUP *)ev->Evidence;
	unsigned numCerts = grp->NumCerts;
	/* array of Apple-specific info from evidence[2] */
	ev = &vfyResult->Evidence[2];
	const CSSM_TP_APPLE_EVIDENCE_INFO *info = 
				(const CSSM_TP_APPLE_EVIDENCE_INFO *)ev->Evidence;
	int ourRtn = 0;
	
	for(unsigned dex=0; dex<numCertStatus; dex++) {
		const char *str = certStatus[dex];
		char buf[8];
		unsigned i;
		
		/* 
		 * Format is certNum:status_in_hex
		 * first get the cert number 
		 */
		for(i=0; *str != '\0'; i++, str++) {
			if(*str == ':') {
				break;
			}
			buf[i] = *str;
		}
		if(*str != ':') {
			printf("***Bad certstatus value, format is certNum:status_in_hex\n");
			return 1;
		}
		buf[i] = '\0';
		unsigned certNum = atoi(buf);
		if(certNum > (numCerts-1)) {
			printf("***certerror specified for cert %u, but only %u certs"
				" available\n", certNum, numCerts);
			return 1;
		}
		str++;			// pts to actual desired status string now
		unsigned certStat = hexToBin(str);
		const CSSM_TP_APPLE_EVIDENCE_INFO *thisInfo = &info[certNum];
		if(certStat == thisInfo->StatusBits) {
			if(!quiet) {
				printf("...0x%x per-cert status received as expected\n", certStat);
			}
		}
		else {
			printf("***Expected per cert status 0x%x, got 0x%x\n",
				(unsigned)certStat, (unsigned)thisInfo->StatusBits);
			ourRtn = 1;
		}
	}
	return ourRtn;
}

/*
 * Ensure that the policy being evaluated is accessible via 
 * SecPolicySearch*(). Not really part of the test, but a handy place
 * to catch this common error before checking in TP changes. 
 */ 
static int verifySecPolicy(
	const CSSM_OID *oid)
{
	SecPolicySearchRef srchRef = NULL;
	OSStatus ortn;
	
	ortn = SecPolicySearchCreate(CSSM_CERT_X_509v3, oid, NULL, &srchRef);
	if(ortn) {
		cssmPerror("SecPolicySearchCreate", ortn);
		return -1;
	}
	SecPolicyRef policyRef = NULL;
	ortn = SecPolicySearchCopyNext(srchRef, &policyRef);
	if(ortn) {
		cssmPerror("SecPolicySearchCopyNext", ortn);
		printf("***The TP policy used in this test is not accessible via SecPolicySearchCopyNext().\n");
		printf("   You probably forgot to add the policy to the theOidList table in PolicyCursor.cpp\n");
		printf("   in the libsecurity_keychain project.\n");
	}
	CFRelease(srchRef);
	if(policyRef) {
		CFRelease(policyRef);
	}
	return ortn;
}

int certVerify(CertVerifyArgs *vfyArgs)
{
	if(vfyArgs->version != CERT_VFY_ARGS_VERS) {
		printf("***CertVerifyArgs.Version mismatch. Clean and rebuild.\n");
		return -1;
	}
	
	/* main job is building a CSSM_TP_VERIFY_CONTEXT and its components */
	CSSM_TP_VERIFY_CONTEXT			vfyCtx;
	CSSM_TP_CALLERAUTH_CONTEXT		authCtx;
	CSSM_TP_VERIFY_CONTEXT_RESULT	vfyResult;
	CSSM_APPLE_TP_SSL_OPTIONS		sslOpts;
	CSSM_APPLE_TP_SMIME_OPTIONS		smimeOpts;
	
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
	/* up to 3 policies */
	CSSM_FIELD	policyIds[3];
	CSSM_FIELD  *policyPtr = &policyIds[0];
	uint32 numPolicies = 0;
	memset(policyIds, 0, 3 * sizeof(CSSM_FIELD));

	switch(vfyArgs->vfyPolicy) {
		case CVP_SSL:
		case CVP_IPSec:
			if(vfyArgs->vfyPolicy == CVP_SSL) {
				policyPtr->FieldOid = CSSMOID_APPLE_TP_SSL;
			}
			else {
				policyPtr->FieldOid = CSSMOID_APPLE_TP_IP_SEC;
			}
			/* otherwise these policies are identical */
			/* sslOpts is optional */
			if((vfyArgs->sslHost != NULL) || vfyArgs->sslClient) {
				memset(&sslOpts, 0, sizeof(sslOpts));
				sslOpts.Version = CSSM_APPLE_TP_SSL_OPTS_VERSION;
				sslOpts.ServerName = vfyArgs->sslHost;
				if(vfyArgs->sslHost != NULL) {
					sslOpts.ServerNameLen = strlen(vfyArgs->sslHost) + 1;
				}
				if(vfyArgs->sslClient) {
					sslOpts.Flags |= CSSM_APPLE_TP_SSL_CLIENT;
				}
				policyPtr->FieldValue.Data = (uint8 *)&sslOpts;
				policyPtr->FieldValue.Length = sizeof(sslOpts);
			}
			break;
		case CVP_SMIME:
		case CVP_iChat:
			if(vfyArgs->vfyPolicy == CVP_SMIME) {
				policyPtr->FieldOid = CSSMOID_APPLE_TP_SMIME;
			}
			else {
				policyPtr->FieldOid = CSSMOID_APPLE_TP_ICHAT;
			}
			/* otherwise these policies are identical */
			/* smimeOpts is optional */
			if(vfyArgs->senderEmail != NULL) {
				smimeOpts.Version = CSSM_APPLE_TP_SMIME_OPTS_VERSION;
				smimeOpts.IntendedUsage = vfyArgs->intendedKeyUse;
				smimeOpts.SenderEmail = vfyArgs->senderEmail;
				smimeOpts.SenderEmailLen = strlen(vfyArgs->senderEmail) + 1;
				policyPtr->FieldValue.Data = (uint8 *)&smimeOpts;
				policyPtr->FieldValue.Length = sizeof(smimeOpts);
			}
			break;
		case CVP_Basic:
			policyPtr->FieldOid = CSSMOID_APPLE_X509_BASIC;
			break;
		case CVP_SWUpdateSign:
			/* no options */
			policyPtr->FieldOid = CSSMOID_APPLE_TP_SW_UPDATE_SIGNING;
			break;
		case CVP_ResourceSigning:
			/* no options */
			policyPtr->FieldOid = CSSMOID_APPLE_TP_RESOURCE_SIGN;
			break;
		case CVP_PKINIT_Server:
			/* no options */
			policyPtr->FieldOid = CSSMOID_APPLE_TP_PKINIT_SERVER;
			break;
		case CVP_PKINIT_Client:
			/* no options */
			policyPtr->FieldOid = CSSMOID_APPLE_TP_PKINIT_CLIENT;
			break;
		case CVP_AppleCodeSigning:
			/* no options */
			policyPtr->FieldOid = CSSMOID_APPLE_TP_CODE_SIGNING;
			break;
		case CVP_PackageSigning:
			/* no options */
			policyPtr->FieldOid = CSSMOID_APPLE_TP_PACKAGE_SIGNING;
			break;
		default:
			printf("***certVerify: bogus vfyPolicy\n");
			return 1;
	}
	if(verifySecPolicy(&policyPtr->FieldOid)) {
		return -1;
	}
	policyPtr++;
	numPolicies++;
	
	CSSM_APPLE_TP_CRL_OPTIONS crlOpts;
	if((vfyArgs->revokePolicy == CRP_CRL) || (vfyArgs->revokePolicy == CRP_CRL_OCSP)) {
		memset(&crlOpts, 0, sizeof(crlOpts));
		policyPtr->FieldOid = CSSMOID_APPLE_TP_REVOCATION_CRL;

		crlOpts.Version = CSSM_APPLE_TP_CRL_OPTS_VERSION;
		crlOpts.CrlFlags = 0;
		crlOpts.crlStore = NULL;
		policyPtr->FieldValue.Data = (uint8 *)&crlOpts;
		policyPtr->FieldValue.Length = sizeof(crlOpts);
		if(vfyArgs->requireCrlForAll) {
			crlOpts.CrlFlags |= CSSM_TP_ACTION_REQUIRE_CRL_PER_CERT;
		}
		if(vfyArgs->crlNetFetchEnable) {
			crlOpts.CrlFlags |= CSSM_TP_ACTION_FETCH_CRL_FROM_NET;
		}
		if(vfyArgs->requireCrlIfPresent) {
			crlOpts.CrlFlags |= CSSM_TP_ACTION_REQUIRE_CRL_IF_PRESENT;
		}
		crlOpts.crlStore = vfyArgs->crlDlDb;
		policyPtr++;
		numPolicies++;
	}
	
	CSSM_APPLE_TP_OCSP_OPTIONS ocspOpts;
	CSSM_DATA respUriData;
	CSSM_DATA respCertData = {vfyArgs->responderCertLen, 
			(uint8 *)vfyArgs->responderCert};
	if((vfyArgs->revokePolicy == CRP_OCSP) || (vfyArgs->revokePolicy == CRP_CRL_OCSP)) {
		memset(&ocspOpts, 0, sizeof(ocspOpts));
		policyPtr->FieldOid = CSSMOID_APPLE_TP_REVOCATION_OCSP;

		crlOpts.Version = CSSM_APPLE_TP_OCSP_OPTS_VERSION;
		policyPtr->FieldValue.Data = (uint8 *)&ocspOpts;
		policyPtr->FieldValue.Length = sizeof(ocspOpts);
		if(vfyArgs->requireOcspForAll) {
			ocspOpts.Flags |= CSSM_TP_ACTION_OCSP_REQUIRE_PER_CERT;
		}
		if(vfyArgs->requireOcspIfPresent) {
			ocspOpts.Flags |= CSSM_TP_ACTION_OCSP_REQUIRE_IF_RESP_PRESENT;
		}
		if(vfyArgs->disableCache) {
			ocspOpts.Flags |= (CSSM_TP_ACTION_OCSP_CACHE_READ_DISABLE |
						       CSSM_TP_ACTION_OCSP_CACHE_WRITE_DISABLE);
		}
		if(vfyArgs->disableOcspNet) {
			ocspOpts.Flags |= CSSM_TP_ACTION_OCSP_DISABLE_NET;
		}
		if(vfyArgs->generateOcspNonce) {
			ocspOpts.Flags |= CSSM_TP_OCSP_GEN_NONCE;
		}
		if(vfyArgs->requireOcspRespNonce) {
			ocspOpts.Flags |= CSSM_TP_OCSP_REQUIRE_RESP_NONCE;
		}
		if(vfyArgs->responderURI != NULL) {
			respUriData.Data = (uint8 *)vfyArgs->responderURI;
			respUriData.Length = strlen(vfyArgs->responderURI);
			ocspOpts.LocalResponder = &respUriData;
		}
		if(vfyArgs->responderCert != NULL) {
			ocspOpts.LocalResponderCert = &respCertData;
		}
		/* other OCSP options here */
		policyPtr++;
		numPolicies++;
	}
	
	authCtx.Policy.NumberOfPolicyIds = numPolicies;

	authCtx.Policy.PolicyIds = policyIds;
	authCtx.Policy.PolicyControl = NULL;
	
	authCtx.VerifyTime = vfyArgs->vfyTime;			// may be NULL
	authCtx.VerificationAbortOn = CSSM_TP_STOP_ON_POLICY;
	authCtx.CallbackWithVerifiedCert = NULL;
	
	/*
	 * DLDBs - the caller's optional set, plus two more we open
	 * if trust settings are enabled and we're told to use system
	 * anchors. (System anchors are normally passed in via 
	 * authCtx.AnchorCerts; they're passed in as DLDBs when
	 * using TrustSettings.)
	 */
	uint32 totalNumDbs = 0;
	uint32 numCallerDbs = 0;
	CSSM_BOOL weOpenedDbs = CSSM_FALSE;
	if(vfyArgs->dlDbList != NULL) {
		totalNumDbs = numCallerDbs = vfyArgs->dlDbList->NumHandles;
	}
	if(vfyArgs->useTrustSettings && vfyArgs->useSystemAnchors) {
		/* we'll cook up two more DBs and possible append them */	
		totalNumDbs += 2;
		weOpenedDbs = CSSM_TRUE;
	}
	CSSM_DL_DB_HANDLE dlDbHandles[totalNumDbs];
	CSSM_DL_DB_LIST dlDbList;
	CSSM_DL_HANDLE dlHand = 0;
	for(unsigned dex=0; dex<numCallerDbs; dex++) {
		dlDbHandles[dex] = vfyArgs->dlDbList->DLDBHandle[dex];
	}
	if(weOpenedDbs) {
		/* get a DL handle, somehow */
		if(numCallerDbs == 0) {
			/* new DL handle */
			dlHand = cuDlStartup();
			dlDbHandles[0].DLHandle = dlHand;
			dlDbHandles[1].DLHandle = dlHand;
		}
		else {
			/* use the same one caller passed in */
			dlDbHandles[numCallerDbs].DLHandle     = dlDbHandles[0].DLHandle;
			dlDbHandles[numCallerDbs + 1].DLHandle = dlDbHandles[0].DLHandle;
		}
		/* now open two DBs */
		dlDbHandles[numCallerDbs].DBHandle = 
			cuDbStartupByName(dlDbHandles[numCallerDbs].DLHandle,
				(char *)ADMIN_CERT_STORE_PATH, CSSM_FALSE, CSSM_TRUE);
		dlDbHandles[numCallerDbs + 1].DBHandle = 
			cuDbStartupByName(dlDbHandles[numCallerDbs].DLHandle,
				(char *)SYSTEM_ROOT_STORE_PATH, CSSM_FALSE, CSSM_TRUE);
	}
	dlDbList.DLDBHandle = dlDbHandles;
	dlDbList.NumHandles = totalNumDbs;
	authCtx.DBList = &dlDbList;

	CFArrayRef cfAnchors = NULL;
	CSSM_DATA *cssmAnchors = NULL;
	unsigned numAnchors = 0;

	if(vfyArgs->useSystemAnchors) {
		if(!vfyArgs->useTrustSettings) {
			/* standard system anchors - ingore error, I'm sure the
			 * current test will eventually fail */
			getSystemAnchors(&cfAnchors, &cssmAnchors, &numAnchors);
			authCtx.NumberOfAnchorCerts = numAnchors;
			authCtx.AnchorCerts = cssmAnchors;
		}
	}
	else {
		/* anchors are our caller's roots */
		if(vfyArgs->roots) {
			authCtx.NumberOfAnchorCerts = vfyArgs->roots->numBlobs();
			authCtx.AnchorCerts = vfyArgs->roots->blobList();
		}
	}
	authCtx.CallerCredentials = NULL;
	
	if(vfyArgs->crls) {
		/* cook up CRL group */
		CSSM_CRLGROUP_PTR cssmCrls = &vfyCtx.Crls;
		cssmCrls->CrlType = CSSM_CRL_TYPE_X_509v1;
		cssmCrls->CrlEncoding = CSSM_CRL_ENCODING_DER;
		cssmCrls->NumberOfCrls = vfyArgs->crls->numBlobs();
		cssmCrls->GroupCrlList.CrlList = vfyArgs->crls->blobList();
		cssmCrls->CrlGroupType = CSSM_CRLGROUP_DATA;
	}
	
	/* CSSM_APPLE_TP_ACTION_DATA */
	CSSM_APPLE_TP_ACTION_DATA tpAction;
	tpAction.Version = CSSM_APPLE_TP_ACTION_VERSION;
	tpAction.ActionFlags = 0;
	if(vfyArgs->leafCertIsCA) {
		tpAction.ActionFlags |= CSSM_TP_ACTION_LEAF_IS_CA;
	}
	if(vfyArgs->certNetFetchEnable) {
		tpAction.ActionFlags |= CSSM_TP_ACTION_FETCH_CERT_FROM_NET;
	}
	if(vfyArgs->allowExpiredRoot) {
		tpAction.ActionFlags |= CSSM_TP_ACTION_ALLOW_EXPIRED_ROOT;
	}
	if(!vfyArgs->allowUnverified) {
		tpAction.ActionFlags |= CSSM_TP_ACTION_REQUIRE_REV_PER_CERT;
	}
	if(vfyArgs->useTrustSettings) {
		tpAction.ActionFlags |= CSSM_TP_ACTION_TRUST_SETTINGS;
	}
	if(vfyArgs->implicitAnchors) {
		tpAction.ActionFlags |= CSSM_TP_ACTION_IMPLICIT_ANCHORS;
	}
	
	/* CSSM_TP_VERIFY_CONTEXT */
	vfyCtx.ActionData.Data   = (uint8 *)&tpAction;
	vfyCtx.ActionData.Length = sizeof(tpAction);

	vfyCtx.Action = CSSM_TP_ACTION_DEFAULT;
	vfyCtx.Cred = &authCtx;
	
	/* cook up cert group */
	CSSM_CERTGROUP cssmCerts;
	cssmCerts.CertType = CSSM_CERT_X_509v3;
	cssmCerts.CertEncoding = CSSM_CERT_ENCODING_DER;
	cssmCerts.NumCerts = vfyArgs->certs->numBlobs();
	cssmCerts.GroupList.CertList = vfyArgs->certs->blobList();
	cssmCerts.CertGroupType = CSSM_CERTGROUP_DATA;
	
	int ourRtn = 0;
	CSSM_RETURN crtn = CSSM_TP_CertGroupVerify(vfyArgs->tpHand,
		vfyArgs->clHand,
		vfyArgs->cspHand,
		&cssmCerts,
		&vfyCtx,
		&vfyResult);
	if(vfyArgs->expectedErrStr != NULL) {
		const char *actRtn;
		if(crtn == CSSM_OK) {
			/* cssmErrorString munges this to "[ok]" */
			actRtn = "CSSM_OK";
		}
		else {
			actRtn = cssmErrToStr(crtn);
		}
		char *found = strstr(actRtn, vfyArgs->expectedErrStr);
		if(found) {
			if(!vfyArgs->quiet) {
				printf("...%s received as expected\n", vfyArgs->expectedErrStr);
			}
		}
		else {
			printf("***CSSM_TP_CertGroupVerify error\n");
			printf("   expected rtn : %s\n", vfyArgs->expectedErrStr);
			printf("     actual rtn : %s\n", actRtn);
			ourRtn = 1;
		}
	}
	else {
		if(crtn) {
			if(!vfyArgs->quiet) {
				printError("CSSM_TP_CertGroupVerify", crtn);
			}
			ourRtn = 1;
		}
		else if(!vfyArgs->quiet) {
			printf("...verify successful\n");
		}
	}
	if(vfyArgs->certErrors) {
		if(vfyCertErrors(&vfyResult, vfyArgs->numCertErrors, vfyArgs->certErrors, 
				vfyArgs->quiet)) {
			ourRtn = 1;
		}
	}
	if(vfyArgs->certStatus) {
		if(vfyCertStatus(&vfyResult, vfyArgs->numCertStatus, vfyArgs->certStatus,
				vfyArgs->quiet)) {
			ourRtn = 1;
		}
	}
	if(vfyArgs->verbose) {
		dumpVfyResult(&vfyResult);
	}
	freeVfyResult(&vfyResult);
	if(weOpenedDbs) {
		/* close the DBs and maybe the DL we opened */
		CSSM_DL_DbClose(dlDbHandles[numCallerDbs]);
		CSSM_DL_DbClose(dlDbHandles[numCallerDbs + 1]);
		if(dlHand != 0) {
			cuDlDetachUnload(dlHand);
		}
	}
	if(cfAnchors) {
		CFRelease(cfAnchors);
	}
	if(cssmAnchors) {
		free(cssmAnchors);
	}
	return ourRtn;
}

unsigned hexDigit(char digit)
{
	if((digit >= '0') && (digit <= '9')) {
		return digit - '0';
	}
	if((digit >= 'a') && (digit <= 'f')) {
		return 10 + digit - 'a';
	}
	if((digit >= 'A') && (digit <= 'F')) {
		return 10 + digit - 'A';
	}
	printf("***BAD HEX DIGIT (%c)\n", digit);
	return 0;
}

/* convert ASCII string in hex to unsigned */
unsigned hexToBin(const char *hex)
{
	unsigned rtn = 0;
	const char *cp = hex;
	if((cp[0] == '0') && (cp[1] == 'x')) {
		cp += 2;
	}
	if(strlen(cp) > 8) {
		printf("***BAD HEX STRING (%s)\n", cp);
		return 0;
	}
	while(*cp) {
		rtn <<= 4;
		rtn += hexDigit(*cp);
		cp++;
	}
	return rtn;
}

/*
 * A slightly simplified version of certVerify: 
 *		-- no CRLs (includes allowUnverified = CSSM_FALSE)
 *		-- revokePOlicy = None
 *		-- no DlDbs
 *		-- no net fetch
 *		-- time = now. 
 */
int certVerifySimple(
	CSSM_TP_HANDLE			tpHand, 
	CSSM_CL_HANDLE 			clHand,
	CSSM_CSP_HANDLE 		cspHand,
	BlobList				&certs,
	BlobList				&roots,
	CSSM_BOOL				useSystemAnchors,
	CSSM_BOOL				leafCertIsCA,
	CSSM_BOOL				allowExpiredRoot,
	CertVerifyPolicy		vfyPolicy,
	const char				*sslHost,		// optional, SSL policy
	CSSM_BOOL				sslClient,		// normally server side
	const char				*senderEmail,	// optional, SMIME
	CE_KeyUsage				intendedKeyUse,	// optional, SMIME only
	const char				*expectedErrStr,// e.g.,
											// "CSSMERR_APPLETP_CRL_NOT_TRUSTED"
				
	/* 
	 * expected per-cert errors
	 * format is certNum:errorString
	 * e.g., "1:CSSMERR_APPLETP_CRL_NOT_TRUSTED"
	 */
	unsigned 				numCertErrors,
	const char 				**certErrors,	// per-cert status
	
	/*
	 * Expected per-cert status (CSSM_TP_APPLE_EVIDENCE_INFO.StatusBits)
	 * format is certNum:status_in_hex
	 * e.g., "1:0x18", leading 0x optional
	 */
	unsigned				numCertStatus,
	const char				**certStatus,
	CSSM_BOOL				useTrustSettings,
	CSSM_BOOL				quiet,
	CSSM_BOOL				verbose)
{
	CertVerifyArgs vfyArgs;
	memset(&vfyArgs, 0, sizeof(vfyArgs));
	vfyArgs.version = CERT_VFY_ARGS_VERS;
	vfyArgs.tpHand = tpHand;
	vfyArgs.clHand = clHand;
	vfyArgs.cspHand = cspHand;
	vfyArgs.certs = &certs;
	vfyArgs.roots = &roots;
	vfyArgs.useSystemAnchors = useSystemAnchors;
	vfyArgs.useTrustSettings = useTrustSettings;
	vfyArgs.leafCertIsCA = leafCertIsCA;
	vfyArgs.allowExpiredRoot = allowExpiredRoot;
	vfyArgs.vfyPolicy = vfyPolicy;
	vfyArgs.sslHost = sslHost;
	vfyArgs.sslClient = sslClient;
	vfyArgs.senderEmail = senderEmail;
	vfyArgs.intendedKeyUse = intendedKeyUse;
	vfyArgs.allowUnverified = CSSM_TRUE;
	vfyArgs.expectedErrStr = expectedErrStr;
	vfyArgs.numCertErrors = numCertErrors;
	vfyArgs.certErrors = certErrors;
	vfyArgs.numCertStatus = numCertStatus;
	vfyArgs.certStatus = certStatus;
	vfyArgs.quiet = quiet;
	vfyArgs.verbose = verbose;
	return certVerify(&vfyArgs);
}

