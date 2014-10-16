 /*
  * tpVerifyParsed.cpp - wrapper for CSSM_TP_CertGroupVerify using parsd anchors. 
  */

#include <Security/Security.h>
#include "tpVerifyParsed.h"
#include <Security/SecRootCertStorePriv.h>
#include <Security/RootCertCachePriv.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <clAppUtils/CertParser.h>
#include <utilLib/common.h>

/*
 * The main task is converting a set of CSSM_DATA-style anchors into a
 * SecParsedRootCertArrayRef.
 */

/* raw cert --> SecParsedRootCert */
static int parseRootCert(
	CSSM_CL_HANDLE clHand,
	const CSSM_DATA &certData,
	SecParsedRootCert &parsedRoot)
{
	try {
		CertParser cert(clHand, certData);
		uint32 len = 0;
		const void *p = cert.fieldForOid(CSSMOID_X509V1SubjectName, len);
		appCopyData(p, len, &parsedRoot.subject);
		
		/* skip key and times, I think they are going away */
		appCopyCssmData(&certData, &parsedRoot.certData);
		return 0;
	}
	catch(...) {
		printf("CertParser threw!\n");
		return -1;
	}
}

static void freeParsedRoot(
	SecParsedRootCert &parsedRoot)
{
	if(parsedRoot.subject.Data) {
		CSSM_FREE(parsedRoot.subject.Data);
	}
	if(parsedRoot.certData.Data) {
		CSSM_FREE(parsedRoot.certData.Data);
	}
}

static int createParsedCertArray(
	CSSM_CL_HANDLE	clHand,
	unsigned		numAnchorCerts,
	CSSM_DATA_PTR	anchorCerts,
	SecParsedRootCertArrayRef *arrayRef)		// RETURNED
{
	SecParsedRootCertArray *outArray = (SecParsedRootCertArray *)malloc(sizeof(*outArray));
	memset(outArray, 0, sizeof(*outArray));
	unsigned len = sizeof(SecParsedRootCert) * numAnchorCerts;
	outArray->roots = (SecParsedRootCert *)malloc(len);
	memset(outArray->roots, 0, len);
	for(unsigned dex=0; dex<numAnchorCerts; dex++) {
		if(parseRootCert(clHand, anchorCerts[dex], outArray->roots[dex])) {
			return -1;
		}
	}
	outArray->numRoots = numAnchorCerts;
	*arrayRef = outArray;
	return 0;
}

static void freeParsedCertArray(
	SecParsedRootCertArrayRef arrayRef)
{
	for(unsigned dex=0; dex<arrayRef->numRoots; dex++) {
		freeParsedRoot(arrayRef->roots[dex]);
	}
	free(arrayRef->roots);
	free((void *)arrayRef);
}

CSSM_RETURN tpCertGroupVerifyParsed(
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
	CSSM_TP_VERIFY_CONTEXT_RESULT_PTR	result)		// optional, RETURNED
{
	/* main job is building a CSSM_TP_VERIFY_CONTEXT and its components */
	CSSM_TP_VERIFY_CONTEXT		vfyCtx;
	CSSM_TP_CALLERAUTH_CONTEXT	authCtx;
	
	memset(&vfyCtx, 0, sizeof(CSSM_TP_VERIFY_CONTEXT));
	vfyCtx.Action = CSSM_TP_ACTION_DEFAULT;
	if(actionData) {
		vfyCtx.ActionData = *actionData;
	}
	else {
		vfyCtx.ActionData.Data = NULL;
		vfyCtx.ActionData.Length = 0;
	}
	vfyCtx.Cred = &authCtx;
	
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
	/* zero or one policy here */
	CSSM_FIELD	policyId;
	if(policy != NULL) {
		policyId.FieldOid = (CSSM_OID)*policy;
		authCtx.Policy.NumberOfPolicyIds = 1;
		authCtx.Policy.PolicyIds = &policyId;
		if(fieldOpts != NULL) {
			policyId.FieldValue = *fieldOpts;
		}
		else {
			policyId.FieldValue.Data = NULL;
			policyId.FieldValue.Length = 0;
		}
	}
	else {
		authCtx.Policy.NumberOfPolicyIds = 0;
		authCtx.Policy.PolicyIds = NULL;
	}
	authCtx.Policy.PolicyControl = policyOpts;
	authCtx.VerifyTime = cssmTimeStr;			// may be NULL
	authCtx.VerificationAbortOn = stopOn;
	authCtx.CallbackWithVerifiedCert = NULL;
	
	/* here's the difference between this and tpCertGroupVerify */
	SecParsedRootCertArrayRef arrayRef = NULL;
	if(numAnchorCerts) {
		if(createParsedCertArray(clHand, numAnchorCerts, anchorCerts, &arrayRef)) {
			return -1;
		}
		authCtx.NumberOfAnchorCerts = APPLE_TP_PARSED_ANCHOR_INDICATOR;
		authCtx.AnchorCerts = (CSSM_DATA_PTR)arrayRef;
	}
	else {
		authCtx.NumberOfAnchorCerts = 0;
		authCtx.AnchorCerts = NULL;
	}
	authCtx.DBList = dbListPtr;
	authCtx.CallerCredentials = NULL;
	
	CSSM_RETURN crtn = CSSM_TP_CertGroupVerify(tpHand,
		clHand,
		cspHand,
		certGroup,
		&vfyCtx,
		result);
	
	if(arrayRef) {
		freeParsedCertArray(arrayRef);
	}
	return crtn;
}
