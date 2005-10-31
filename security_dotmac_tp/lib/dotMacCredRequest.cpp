/*
 * Copyright (c) 2004 Apple Computer, Inc. All Rights Reserved.
 * 
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */


/*
 * DotMacCredRequest.cpp - public session functions for Submit/Retrieve cred result
 */

#include "AppleDotMacTPSession.h"
#include "dotMacTpDebug.h"
#include "dotMacTpUtils.h"
#include "dotMacTpRpcGlue.h"
#include "dotMacTp.h"
#include <security_asn1/nssUtils.h>
#include <security_asn1/SecNssCoder.h>
#include <Security/oidsalg.h>
#include <security_cdsa_utils/cuPem.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>

void AppleDotMacTPSession::RetrieveCredResult(
	const CssmData &ReferenceIdentifier,
	const CSSM_TP_CALLERAUTH_CONTEXT *CallerAuthCredentials,
	sint32 &EstimatedTime,
	CSSM_BOOL &ConfirmationRequired,
	CSSM_TP_RESULT_SET_PTR &RetrieveOutput)
{
	/* 
	 * The only input we use is the RefId, which we created in SubmitCredRequest()
	 * in the case of a CSSMERR_APPLE_DOTMAC_REQ_QUEUED status.
	 */
	if((ReferenceIdentifier.Data == NULL) || (ReferenceIdentifier.Length == 0)) {
		dotMacErrorLog("RetrieveCredResult: NULL ReferenceIdentifier\n");
		CssmError::throwMe(CSSMERR_TP_INVALID_IDENTIFIER);
	}
	if(CallerAuthCredentials != NULL) {
		dotMacErrorLog("RetrieveCredResult: CallerAuthCredentials not used\n");
		CssmError::throwMe(CSSMERR_TP_INVALID_CALLERAUTH_CONTEXT_POINTER);
	}
	
	/*
	 * Decode the RefId to get the fields we need to fetch the cert. 
	 */
	SecNssCoder coder;
	CSSM_DATA userName;
	DotMacSignType signType;
	OSStatus ortn = dotMacDecodeRefId(coder, ReferenceIdentifier, userName, &signType);
	if(ortn) {
		dotMacErrorLog("RetrieveCredResult: Invalid RefID\n");
		CssmError::throwMe(CSSMERR_TP_INVALID_IDENTIFIER);
	}
	
	/* Fetch the cert. */
	CSSM_DATA certData = {0, NULL};
	CSSM_RETURN crtn;
	crtn = dotMacTpCertFetch(userName, signType, *this, certData);
	if(crtn) {
		/* FIXME error handling here, including no data */
		CssmError::throwMe(crtn);
	}

	ConfirmationRequired = CSSM_FALSE;
	EstimatedTime = 0;
	
	/* Return cert data as the single Results in CSSM_TP_RESULT_SET. */
	if(certData.Length == 0) {
		/* The spec says this is OK, it just means that no cert is available yet */
		dotMacErrorLog("RetrieveCredResult: no data after successful GET\n");
		CssmError::throwMe(CSSMERR_TP_CERT_NOT_VALID_YET);
	}

	RetrieveOutput = (CSSM_TP_RESULT_SET_PTR)malloc(sizeof(CSSM_TP_RESULT_SET));
	CSSM_DATA *resultData = (CSSM_DATA *)malloc(sizeof(CSSM_DATA));
	*resultData = certData;
	RetrieveOutput->NumberOfResults = 1;
	RetrieveOutput->Results = resultData;
}

void AppleDotMacTPSession::SubmitArchiveRequest(
	DotMacArchiveType archiveType,					// OID preparsed
	const CSSM_DATA *altHost,						// optional
	CSSM_TP_AUTHORITY_REQUEST_TYPE RequestType,
	const CSSM_TP_REQUEST_SET &RequestInput,
	const CSSM_TP_CALLERAUTH_CONTEXT *CallerAuthContext,
	sint32 &EstimatedTime,
	CssmData &ReferenceIdentifier)
{
	CSSM_APPLE_DOTMAC_TP_ARCHIVE_REQUEST *archReq = 
		(CSSM_APPLE_DOTMAC_TP_ARCHIVE_REQUEST *)RequestInput.Requests;
	const CSSM_DATA *pfxIn = NULL;
	CSSM_DATA *pfxOut = NULL;
	const CSSM_DATA *archiveName = NULL;
	const CSSM_DATA *timeString = NULL;
	
	if((archReq == NULL) || 
	   (archReq->userName.Data == NULL) ||
	   (archReq->password.Data == NULL)) {
		dotMacErrorLog("SubmitArchiveRequest: bad username/pwd\n");
		CssmError::throwMe(CSSMERR_TP_INVALID_REQUEST_INPUTS);
	}
	
	/* further verification per request type */
	bool badInputs = false;
	switch(archiveType) {
		case DMAT_List:
			/* nothing further needed */
			break;
		case DMAT_Store:
			if((archReq->archiveName.Data == NULL) ||
			   (archReq->timeString.Data == NULL) || 
			   (archReq->pfx.Data == NULL)) {
				badInputs = true;
			}
			pfxIn = &archReq->pfx;
			archiveName = &archReq->archiveName;
			timeString = &archReq->timeString;
			break;
		case DMAT_Fetch:
			pfxOut = &archReq->pfx;
			/* and drop thru */
		case DMAT_Remove:
			if(archReq->archiveName.Data == NULL) {
				badInputs = true;
			}
			archiveName = &archReq->archiveName;
			break;
		default:
			assert(0);
			CssmError::throwMe(internalComponentErr);
	}
	if(badInputs) {
		dotMacErrorLog("SubmitArchiveRequest: bad per-method inputs\n");
		CssmError::throwMe(CSSMERR_TP_INVALID_REQUEST_INPUTS);
	}
	
	CSSM_DATA hostName;
	if(altHost) {
		hostName = *altHost;
	}
	else {
		hostName.Data = (uint8 *)DOT_MAC_ARCHIVE_HOST_NAME;
		hostName.Length = strlen(DOT_MAC_ARCHIVE_HOST_NAME);
	}
	CSSM_RETURN crtn;
	
	crtn = dotMacPostArchiveReq(archiveType,
		archReq->userName, archReq->password, hostName,		// all required
		archiveName, pfxIn, timeString, pfxOut,
		&archReq->numArchives, &archReq->archives,
		*this);
	if(crtn) {
		CssmError::throwMe(crtn);
	}
}

typedef enum {
	CRO_Sign,
	CRO_Archive,
	CRO_Lookup
} CredReqOp;

void AppleDotMacTPSession::SubmitCredRequest(
	const CSSM_TP_AUTHORITY_ID *PreferredAuthority,		// optional
	CSSM_TP_AUTHORITY_REQUEST_TYPE RequestType,
	const CSSM_TP_REQUEST_SET &RequestInput,
	const CSSM_TP_CALLERAUTH_CONTEXT *CallerAuthContext,
	sint32 &EstimatedTime,
	CssmData &ReferenceIdentifier)
{
	CredReqOp op = CRO_Sign;
	
	/* default host, overridable via PreferredAuthority */
	CSSM_DATA hostName = { strlen(DOT_MAC_SIGN_HOST_NAME),
						  (uint8 *)DOT_MAC_SIGN_HOST_NAME };
	CSSM_DATA *altHost = NULL;
	
	/* qualify inputs */
	if(PreferredAuthority) {
		/* only valid option: host name */
		if(PreferredAuthority->AuthorityCert != NULL) {
			dotMacErrorLog("SubmitCredRequest: AuthorityCert illegal\n");
			CssmError::throwMe(CSSMERR_TP_INVALID_AUTHORITY);
		}
		if(PreferredAuthority->AuthorityLocation == NULL) {
			dotMacErrorLog("SubmitCredRequest: AuthorityLocation invalid\n");
			CssmError::throwMe(CSSMERR_TP_INVALID_AUTHORITY);
		}
		if(PreferredAuthority->AuthorityLocation->AddressType != CSSM_ADDR_NAME) {
			dotMacErrorLog("SubmitCredRequest: AddressType invalid\n");
			CssmError::throwMe(CSSMERR_TP_INVALID_AUTHORITY);
		}
		hostName = PreferredAuthority->AuthorityLocation->Address;
		if(hostName.Data == NULL) {
			dotMacErrorLog("SubmitCredRequest: Address invalid\n");
			CssmError::throwMe(CSSMERR_TP_INVALID_AUTHORITY);
		}
		/* for archive only (it has a different default) */
		altHost = &hostName;
	}
	switch(RequestType) {
		case CSSM_TP_AUTHORITY_REQUEST_CERTISSUE:
			break;
		case CSSM_TP_AUTHORITY_REQUEST_CERTLOOKUP:
			op = CRO_Lookup;
			break;
		default:
			CssmError::throwMe(CSSMERR_TP_UNSUPPORTED_SERVICE);
	}
	if(CallerAuthContext == NULL) {
		CssmError::throwMe(CSSMERR_TP_INVALID_CALLERAUTH_CONTEXT_POINTER);
	}
	if((RequestInput.NumberOfRequests != 1) ||
	   (RequestInput.Requests == NULL)) {
		CssmError::throwMe(CSSMERR_TP_INVALID_REQUEST_INPUTS);
	}
	const CSSM_TP_POLICYINFO *tpPolicy = &CallerAuthContext->Policy;
	if((tpPolicy->NumberOfPolicyIds != 1) || (tpPolicy->PolicyIds == NULL)) {
		CssmError::throwMe(CSSMERR_TP_INVALID_POLICY_IDENTIFIERS);
	}
	
	DotMacSignType signType = DMST_Identity;
	DotMacArchiveType archiveType = DMAT_List;
	CSSM_DATA csr = {0, NULL};
	
	const CSSM_OID *oid = &tpPolicy->PolicyIds->FieldOid;
	if(nssCompareCssmData(oid, &CSSMOID_DOTMAC_CERT_REQ_IDENTITY)) {
		signType = DMST_Identity;
	}
	else if(nssCompareCssmData(oid, &CSSMOID_DOTMAC_CERT_REQ_EMAIL_SIGN)) {
		signType = DMST_EmailSigning;
	}
	else if(nssCompareCssmData(oid, &CSSMOID_DOTMAC_CERT_REQ_EMAIL_ENCRYPT)) {
		signType = DMST_EmailEncrypting;
	}
	else if(nssCompareCssmData(oid, &CSSMOID_DOTMAC_CERT_REQ_ARCHIVE_LIST)) {
		op = CRO_Archive;
		archiveType = DMAT_List;
	}
	else if(nssCompareCssmData(oid, &CSSMOID_DOTMAC_CERT_REQ_ARCHIVE_STORE)) {
		op = CRO_Archive;
		archiveType = DMAT_Store;
	}
	else if(nssCompareCssmData(oid, &CSSMOID_DOTMAC_CERT_REQ_ARCHIVE_FETCH)) {
		op = CRO_Archive;
		archiveType = DMAT_Fetch;
	}
	else if(nssCompareCssmData(oid, &CSSMOID_DOTMAC_CERT_REQ_ARCHIVE_REMOVE)) {
		op = CRO_Archive;
		archiveType = DMAT_Remove;
	}
	else {
		CssmError::throwMe(CSSMERR_TP_INVALID_POLICY_IDENTIFIERS);
	}

	CSSM_APPLE_DOTMAC_TP_CERT_REQUEST *csrReq = 
		(CSSM_APPLE_DOTMAC_TP_CERT_REQUEST *)RequestInput.Requests;
	switch(op) {
		case CRO_Archive:
			SubmitArchiveRequest(archiveType, altHost, RequestType,
				RequestInput, CallerAuthContext, EstimatedTime, ReferenceIdentifier);
			return;
		case CRO_Lookup:
		{
			if((csrReq == NULL) || (csrReq->userName.Data == NULL)) {
				dotMacErrorLog("SubmitCsrRequest(Lookup): bad username\n");
				CssmError::throwMe(CSSMERR_TP_INVALID_REQUEST_INPUTS);
			}
			if((csrReq != NULL) && (csrReq->flags & CSSM_DOTMAC_TP_IS_REQ_PENDING)) {
				/* 
				 * We're just asking the server if there is a request pending
				 * for this user
				 */
				if(csrReq->password.Data == NULL) {
					dotMacErrorLog("TP_IS_REQ_PENDING: no passphrase\n");
					CssmError::throwMe(CSSMERR_TP_INVALID_REQUEST_INPUTS);
				}
				CSSM_RETURN crtn = dotMacPostReqPendingPing(csrReq->userName,
					csrReq->password,
					hostName);
				/* this RPC does not have a "success" return */
				assert(crtn != CSSM_OK);
				if(crtn) {
					CssmError::throwMe(crtn);
				}
			}
			else {
				CSSM_RETURN crtn = dotMacTpCertFetch(csrReq->userName, signType, *this, 
					ReferenceIdentifier);
				if(crtn) {
					dotMacErrorLog("SubmitCsrRequest(Lookup): error on fetch\n");
					CssmError::throwMe(crtn);
				}
			}
			return;
		}	
		case CRO_Sign:
			/* proceed to main body */
			break;
	}
	
	if((csrReq->userName.Data == NULL) ||
	   (csrReq->password.Data == NULL)) {
		dotMacErrorLog("SubmitCsrRequest: bad username/pwd\n");
		CssmError::throwMe(CSSMERR_TP_INVALID_REQUEST_INPUTS);
	}
	if(!(csrReq->flags & CSSM_DOTMAC_TP_EXIST_CSR)) {
		/* Generating a CSR requires even more... */
		if((csrReq->cspHand == 0) || 
		   (csrReq->clHand == 0) ||
		   (csrReq->numTypeValuePairs == 0) ||
		   (csrReq->typeValuePairs == NULL) ||
		   (csrReq->publicKey == NULL) ||
		   (csrReq->privateKey == NULL)) {
			dotMacErrorLog("SubmitCsrRequest: bad CSR generating params\n");
			CssmError::throwMe(CSSMERR_TP_INVALID_REQUEST_INPUTS);
		}
	}
	else if(csrReq->csr.Data == NULL) {
		dotMacErrorLog("SubmitCsrRequest: bad incoming CSR\n");
		CssmError::throwMe(CSSMERR_TP_INVALID_REQUEST_INPUTS);
	}

	/* local vars prior to goto */
	SecNssCoder			coder;
	CSSM_X509_NAME		x509Name;
	const CSSM_KEY		*pubKey = csrReq->publicKey;
	const CSSM_KEY		*actPubKey = NULL;
	CSSM_BOOL			freeRawKey = CSSM_FALSE;
	CSSM_KEY			rawPubKey;	
	unsigned char		*pemCsr = NULL;
	unsigned			pemCsrLen;
	CSSM_RETURN			crtn = CSSM_OK;
	CSSM_CC_HANDLE		sigHand = 0;
	CSSM_DATA_PTR		csrPtr = NULL;
	
	if(csrReq->flags & CSSM_DOTMAC_TP_EXIST_CSR) {
		/* Skip the CSR gen */
		csr = csrReq->csr;
		goto doPost;
	}
	
	/* Build an X509Name for the CL */
	dotMacTpbuildX509Name(coder, csrReq->numTypeValuePairs, csrReq->typeValuePairs,
		x509Name);
		
	/* convert possible ref key to raw; CL requires this */
	
	switch(pubKey->KeyHeader.BlobType) {
		case CSSM_KEYBLOB_RAW:
			actPubKey = pubKey;
			break;
		case CSSM_KEYBLOB_REFERENCE:
			dotMacRefKeyToRaw(csrReq->cspHand, pubKey, &rawPubKey);
			actPubKey = &rawPubKey;
			freeRawKey = CSSM_TRUE;
			break;
		default:
			dotMacErrorLog("SubmitCsrRequest: bad key blob type (%u)",
				(unsigned)pubKey->KeyHeader.BlobType);
			CssmError::throwMe(CSSMERR_TP_INVALID_REQUEST_INPUTS);
	}
	/* subsequent errors to errOut: */

	/* cook up a CL-passthrough-specific request */
	CSSM_APPLE_CL_CSR_REQUEST clReq;
	memset(&clReq, 0, sizeof(clReq));
	clReq.subjectNameX509 	= &x509Name;
	clReq.signatureAlg		= DOT_MAC_CSR_SIGNATURE_ALGID;
	clReq.signatureOid		= DOT_MAC_CSR_SIGNATURE_ALGOID;
	clReq.cspHand 			= csrReq->cspHand;
	clReq.subjectPublicKey  = actPubKey;
	clReq.subjectPrivateKey = csrReq->privateKey;
	
	/* A crypto handle to pass to the CL */
	crtn = CSSM_CSP_CreateSignatureContext(csrReq->cspHand,
			clReq.signatureAlg,
			(CallerAuthContext ? CallerAuthContext->CallerCredentials : NULL),
			csrReq->privateKey,
			&sigHand);
	if(crtn) {
		dotMacErrorLog("CSSM_CSP_CreateSignatureContext returned %ld", crtn);
		goto errOut;
	}
	
	/* down to the CL to do the actual work */
	crtn = CSSM_CL_PassThrough(csrReq->clHand,
		sigHand,
		CSSM_APPLEX509CL_OBTAIN_CSR,
		&clReq,
		(void **)&csrPtr);
	if(crtn) {
		dotMacErrorLog("CSSM_CL_PassThrough returned %ld", crtn);
		goto errOut;
	}
	if(csrPtr == NULL) {
		dotMacErrorLog("SubmitCredRequest: CL returned NULL CSR\n");
		crtn = internalComponentErr;
		goto errOut;
	}
	
	/* base64 encode */
	if(pemEncode(csrPtr->Data, csrPtr->Length, &pemCsr, &pemCsrLen, 
		"CERTIFICATE REQUEST")) {
		dotMacErrorLog("***Error on PEM encode of CSR\n");
		crtn = memFullErr;
		goto errOut;
	}
	
	if(csrReq->flags & CSSM_DOTMAC_TP_RETURN_CSR) {
		/* caller wants a copy of the CSR */
		csrReq->csr.Data = (uint8 *)malloc(pemCsrLen);
		memmove(csrReq->csr.Data, pemCsr, pemCsrLen);
		csrReq->csr.Length = pemCsrLen;
	}
	csr.Data = pemCsr;
	csr.Length = pemCsrLen;

doPost:
	if(!(csrReq->flags & CSSM_DOTMAC_TP_DO_NOT_POST)) {
		
		/* do the net request */
		CSSM_DATA hostName;
		CSSM_DATA resultBody;
		
		if(altHost) {
			hostName = *altHost;
		}
		else {
			hostName.Data = (uint8 *)DOT_MAC_SIGN_HOST_NAME;
			hostName.Length = strlen(DOT_MAC_SIGN_HOST_NAME);
		}
		crtn = dotMacPostCertReq(signType,
			csrReq->userName,
			csrReq->password,
			hostName,
			csrReq->flags & CSSM_DOTMAC_TP_SIGN_RENEW ? true : false,
			csr,
			coder,
			EstimatedTime,
			resultBody);
		switch(crtn) {
			/* Some cases return data to caller */
			case noErr:								/* resultBody = PEM-encoded cert */
			case CSSMERR_APPLE_DOTMAC_REQ_REDIRECT: /* resultBody = URL */
			case CSSMERR_APPLE_DOTMAC_REQ_QUEUED:   /* resultBody = opaque data we'll use later */
				if(resultBody.Data != NULL) {
					ReferenceIdentifier.Data = (uint8 *)malloc(resultBody.Length);
					ReferenceIdentifier.Length = resultBody.Length;
					memmove(ReferenceIdentifier.Data, resultBody.Data, resultBody.Length);
					
					/* skip trailing NULL - it's just ASCII data */
					if(resultBody.Data[resultBody.Length-1] == '\0') {
						ReferenceIdentifier.Length--;
					}
				}
				else {
					dotMacErrorLog("***SubmitCredReq: expected RefId.Data, got none\n");
				}
				break;
			default:
				break;
		}
	}

errOut:
	/* free local resources */
	if(sigHand) {
		CSSM_DeleteContext(sigHand);
	}
	if(freeRawKey) {
		CSSM_FreeKey(csrReq->cspHand, NULL, &rawPubKey, CSSM_FALSE);
	}
	if(pemCsr) {
		free(pemCsr);
	}
	if(crtn) {
		CssmError::throwMe(crtn);
	}

}
