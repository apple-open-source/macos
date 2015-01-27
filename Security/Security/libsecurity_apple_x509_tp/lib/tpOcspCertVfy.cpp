/*
 * Copyright (c) 2004,2011-2012,2014 Apple Inc. All Rights Reserved.
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
 * tpOcspCertVfy.cpp - OCSP cert verification routines
 */

#include "tpOcspCertVfy.h"
#include "tpdebugging.h"
#include "certGroupUtils.h"
#include <Security/oidscert.h>
#include <CommonCrypto/CommonDigest.h>
#include <security_ocspd/ocspdUtils.h>

#ifndef	NDEBUG
#include <Security/SecCertificate.h>
#endif

/*
 * Is signerCert authorized to sign OCSP responses by issuerCert? IssuerCert is
 * assumed to be (i.e., must, but we don't check that here) the signer of the
 * cert being verified, which is not in the loop for this op. Just a bool returned;
 * it's authorized or it's not.
 */
static bool tpIsAuthorizedOcspSigner(
	TPCertInfo &issuerCert,		// issuer of cert being verified
	TPCertInfo &signerCert)		// potential signer of OCSP response
{
	CSSM_DATA_PTR		fieldValue = NULL;			// mallocd by CL
	CSSM_RETURN			crtn;
	bool				ourRtn = false;
	CE_ExtendedKeyUsage *eku = NULL;
	bool				foundEku = false;

	/*
	 * First see if issuerCert issued signerCert (No signature vfy yet, just
	 * subject/issuer check).
	 */
	if(!issuerCert.isIssuerOf(signerCert)) {
#ifndef	NDEBUG
		SecCertificateRef issuerRef = NULL;
		SecCertificateRef signerRef = NULL;
		const CSSM_DATA *issuerData = issuerCert.itemData();
		const CSSM_DATA *signerData = signerCert.itemData();
		crtn = SecCertificateCreateFromData(issuerData, CSSM_CERT_X_509v3, CSSM_CERT_ENCODING_BER, &issuerRef);
		crtn = SecCertificateCreateFromData(signerData, CSSM_CERT_X_509v3, CSSM_CERT_ENCODING_BER, &signerRef);
		CFStringRef issuerName = SecCertificateCopySubjectSummary(issuerRef);
		CFStringRef signerName = SecCertificateCopySubjectSummary(signerRef);
		if(issuerName) {
			CFIndex maxLength = CFStringGetMaximumSizeForEncoding(CFStringGetLength(issuerName), kCFStringEncodingUTF8) + 1;
			char* buf = (char*) malloc(maxLength);
			if (buf) {
				if (CFStringGetCString(issuerName, buf, (CFIndex)maxLength, kCFStringEncodingUTF8)) {
					tpOcspDebug("tpIsAuthorizedOcspSigner: issuerCert \"%s\"", buf);
				}
				free(buf);
			}
			CFRelease(issuerName);
		}
		if(signerName) {
			CFIndex maxLength = CFStringGetMaximumSizeForEncoding(CFStringGetLength(signerName), kCFStringEncodingUTF8) + 1;
			char* buf = (char*) malloc(maxLength);
			if (buf) {
				if (CFStringGetCString(signerName, buf, (CFIndex)maxLength, kCFStringEncodingUTF8)) {
					tpOcspDebug("tpIsAuthorizedOcspSigner: signerCert \"%s\"", buf);
				}
				free(buf);
			}
			CFRelease(signerName);
		}
		if(issuerRef) {
			CFRelease(issuerRef);
		}
		if(signerRef) {
			CFRelease(signerRef);
		}
#endif
		tpOcspDebug("tpIsAuthorizedOcspSigner: signer is not issued by issuerCert");
		return false;
	}

	/* Fetch ExtendedKeyUse field from signerCert */
	crtn = signerCert.fetchField(&CSSMOID_ExtendedKeyUsage, &fieldValue);
	if(crtn) {
		tpOcspDebug("tpIsAuthorizedOcspSigner: signer is issued by issuer, no EKU");
		return false;
	}
	CSSM_X509_EXTENSION *cssmExt = (CSSM_X509_EXTENSION *)fieldValue->Data;
	if(cssmExt->format != CSSM_X509_DATAFORMAT_PARSED) {
		tpOcspDebug("tpIsAuthorizedOcspSigner: bad extension format");
		goto errOut;
	}
	eku = (CE_ExtendedKeyUsage *)cssmExt->value.parsedValue;

	/* Look for OID_KP_OCSPSigning */
	for(unsigned dex=0; dex<eku->numPurposes; dex++) {
		if(tpCompareCssmData(&eku->purposes[dex], &CSSMOID_OCSPSigning)) {
			foundEku = true;
			break;
		}
	}
	if(!foundEku) {
		tpOcspDebug("tpIsAuthorizedOcspSigner: signer is issued by issuer, no OCSP "
			"signing EKU");
		goto errOut;
	}

	/*
	 * OK, signerCert is authorized by *someone* to sign OCSP requests, and
	 * it claims to be issued by issuer. Sig verify to be sure.
	 * FIXME this is not handling partial public keys, which would be a colossal
	 * mess to handle in this module...so we don't.
	 */
	crtn = signerCert.verifyWithIssuer(&issuerCert, NULL);
	if(crtn == CSSM_OK) {
		tpOcspDebug("tpIsAuthorizedOcspSigner: FOUND authorized signer");
		ourRtn = true;
	}
	else {
		/* This is a highly irregular situation... */
		tpOcspDebug("tpIsAuthorizedOcspSigner: signer sig verify FAIL");
	}
errOut:
	if(fieldValue != NULL) {
		signerCert.freeField(&CSSMOID_ExtendedKeyUsage, fieldValue);
	}
	return ourRtn;
}

/*
 * Check ResponderID linkage between an OCSPResponse and a cert we believe to
 * be the issuer of both that response and the cert being verified. Returns
 * true if OK.
 */
static
bool tpOcspResponderIDCheck(
	OCSPResponse	&ocspResp,
	TPCertInfo		&signer)
{
	bool shouldBeSigner = false;
	if(ocspResp.responderIDTag() == RIT_Name) {
		/*
		 * Name inside response must == signer's SubjectName.
		 * Note we can't use signer.subjectName(); that's normalized.
		 */

		const CSSM_DATA *respIdName = ocspResp.encResponderName();
		CSSM_DATA *subjectName = NULL;
		CSSM_RETURN crtn = signer.fetchField(&CSSMOID_X509V1SubjectNameStd,
			&subjectName);
		if(crtn) {
			/* bad cert */
			tpOcspDebug("tpOcspResponderIDCheck: error on fetchField(subjectName");
			return false;
		}
		if(tpCompareCssmData(respIdName, subjectName)) {
			tpOcspDebug("tpOcspResponderIDCheck: good ResponderID.byName");
			shouldBeSigner = true;
		}
		else {
			tpOcspDebug("tpOcspResponderIDCheck: BAD ResponderID.byName");
		}
		signer.freeField(&CSSMOID_X509V1SubjectNameStd, subjectName);
	}
	else {
		/* ResponderID.byKey must == SHA1(signer's public key) */
		const CSSM_KEY_PTR pubKey = signer.pubKey();
		assert(pubKey != NULL);
		uint8 digest[CC_SHA1_DIGEST_LENGTH];
		CSSM_DATA keyHash = {CC_SHA1_DIGEST_LENGTH, digest};
		CSSM_DATA pubKeyBytes = {0, NULL};
		ocspdGetPublicKeyBytes(NULL, pubKey, pubKeyBytes);
		ocspdSha1(pubKeyBytes.Data, (CC_LONG)pubKeyBytes.Length, digest);
		const CSSM_DATA *respKeyHash = &ocspResp.responderID().byKey;
		if(tpCompareCssmData(&keyHash, respKeyHash)) {
			tpOcspDebug("tpOcspResponderIDCheck: good ResponderID.byKey");
			shouldBeSigner = true;
		}
		else {
			tpOcspDebug("tpOcspResponderIDCheck: BAD ResponderID.byKey");
		}
	}
	return shouldBeSigner;
}

/*
 * Verify the signature of an OCSP response. Caller is responsible for all other
 * verification of the response, this is just the crypto.
 * Returns true on success.
 */
static bool tpOcspResponseSigVerify(
	TPVerifyContext		&vfyCtx,
	OCSPResponse		&ocspResp,		// parsed response
	TPCertInfo			&signer)
{
	/* get signature algorithm in CSSM form from the response */
	const SecAsn1OCSPBasicResponse &basicResp = ocspResp.basicResponse();
	const CSSM_OID *algOid = &basicResp.algId.algorithm;
	CSSM_ALGORITHMS sigAlg;

	if(!cssmOidToAlg(algOid, &sigAlg)) {
		tpOcspDebug("tpOcspResponseSigVerify: unknown signature algorithm");
	}

	/* signer's public key from the cert */
	const CSSM_KEY *pubKey = signer.pubKey();

	/* signature: on decode, length is in BITS */
	CSSM_DATA sig = basicResp.sig;
	sig.Length /= 8;

	CSSM_RETURN crtn;
	CSSM_CC_HANDLE sigHand;
	bool ourRtn = false;
	crtn = CSSM_CSP_CreateSignatureContext(vfyCtx.cspHand, sigAlg, NULL,
		pubKey, &sigHand);
	if(crtn) {
		#ifndef	NDEBUG
		cssmPerror("tpOcspResponseSigVerify, CSSM_CSP_CreateSignatureContext", crtn);
		#endif
		return false;
	}
	crtn = CSSM_VerifyData(sigHand, &basicResp.tbsResponseData, 1,
		CSSM_ALGID_NONE, &sig);
	if(crtn) {
		#ifndef	NDEBUG
		cssmPerror("tpOcspResponseSigVerify, CSSM_VerifyData", crtn);
		#endif
	}
	else {
		ourRtn = true;
	}
	CSSM_DeleteContext(sigHand);
	return ourRtn;
}

/* possible return from tpIsOcspIssuer() */
typedef enum {
	OIS_No,			// not the issuer
	OIS_Good,		// is the issuer and signature matches
	OIS_BadSig,		// appears to be issuer, but signature doesn't match
} OcspIssuerStatus;

/* type of rawCert passed to tpIsOcspIssuer */
typedef enum {
	OCT_Local,		// LocalResponder - no checking other than signature
	OCT_Issuer,		// it's the issuer of the cert being verified
	OCT_Provided,	// came with response, provenance unknown
} OcspCertType;

/*
 * Did specified cert issue the OCSP response?
 *
 * This implements the algorithm described in RFC2560, section 4.2.2.2,
 * "Authorized Responders". It sees if the cert could be the issuer of the
 * OCSP response per that algorithm; then if it could, it performs signature
 * verification.
 */
static OcspIssuerStatus tpIsOcspIssuer(
	TPVerifyContext		&vfyCtx,
	OCSPResponse		&ocspResp,		// parsed response
	/* on input specify at least one of the following two */
	const CSSM_DATA		*signerData,
	TPCertInfo			*signer,
	OcspCertType		certType,		// where rawCert came from
	TPCertInfo			*issuer,		// OPTIONAL, if known
	TPCertInfo			**signerRtn)	// optionally RETURNED if at all possible
{
	assert((signerData != NULL) || (signer != NULL));

	/* get signer as TPCertInfo if caller hasn't provided */
	TPCertInfo *tmpSigner = NULL;
	if(signer == NULL) {
		try {
			tmpSigner = new TPCertInfo(vfyCtx.clHand, vfyCtx.cspHand, signerData,
				TIC_CopyData, vfyCtx.verifyTime);
		}
		catch(...) {
			tpOcspDebug("tpIsOcspIssuer: bad cert");
			return OIS_No;
		}
		signer = tmpSigner;
	}
	if(signer == NULL) {
		return OIS_No;
	}
	if(signerRtn != NULL) {
		*signerRtn = signer;
	}

	/*
	 * Qualification of "this can be the signer" depends on where the
	 * signer came from.
	 */
	bool shouldBeSigner = false;
	OcspIssuerStatus ourRtn = OIS_No;

	switch(certType) {
		case OCT_Local:			// caller trusts this and thinks it's the signer
			shouldBeSigner = true;
			break;
		case OCT_Issuer:		// last resort, the actual issuer
			/* check ResponderID linkage */
			shouldBeSigner = tpOcspResponderIDCheck(ocspResp, *signer);
			break;
		case OCT_Provided:
		{
			/*
			 * This cert came with the response.
			 */
			if(issuer == NULL) {
				/*
				 * careful, might not know the issuer...how would this path ever
				 * work then? I don't think it needs to because you can NOT
				 * do OCSP on a cert without its issuer in hand.
				 */
				break;
			}

			/* check EKU linkage */
			shouldBeSigner = tpIsAuthorizedOcspSigner(*issuer, *signer);
			break;
		}
	}
	if(!shouldBeSigner) {
		goto errOut;
	}

	/* verify the signature */
	if(tpOcspResponseSigVerify(vfyCtx, ocspResp, *signer)) {
		ourRtn = OIS_Good;
	}

errOut:
	if((signerRtn == NULL) && (tmpSigner != NULL)) {
		delete tmpSigner;
	}
	return ourRtn;

}

OcspRespStatus tpVerifyOcspResp(
	TPVerifyContext		&vfyCtx,
	SecNssCoder			&coder,
	TPCertInfo			*issuer,		// issuer of the related cert, may be issuer of
										//   reply, may not be known
	OCSPResponse		&ocspResp,
	CSSM_RETURN			&cssmErr)		// possible per-cert error
{
	OcspRespStatus	ourRtn = ORS_Unknown;
	CSSM_RETURN		crtn;

	tpOcspDebug("tpVerifyOcspResp top");

	switch(ocspResp.responseStatus()) {
		case RS_Success:
			crtn = CSSM_OK;
			break;
		case RS_MalformedRequest:
			crtn = CSSMERR_APPLETP_OCSP_RESP_MALFORMED_REQ;
			break;
		case RS_InternalError:
			crtn = CSSMERR_APPLETP_OCSP_RESP_INTERNAL_ERR;
			break;
		case RS_TryLater:
			crtn = CSSMERR_APPLETP_OCSP_RESP_TRY_LATER;
			break;
		case RS_SigRequired:
			crtn = CSSMERR_APPLETP_OCSP_RESP_SIG_REQUIRED;
			break;
		case RS_Unauthorized:
			crtn = CSSMERR_APPLETP_OCSP_RESP_UNAUTHORIZED;
			break;
		default:
			crtn = CSSMERR_APPLETP_OCSP_BAD_RESPONSE;
			break;
	}
	if(crtn) {
		tpOcspDebug("tpVerifyOcspResp aborting due to response status %d",
			(int)(ocspResp.responseStatus()));
		cssmErr = crtn;
		return ORS_Unknown;
	}
	cssmErr = CSSM_OK;

	/* one of our main jobs is to locate the signer of the response, here */
	TPCertInfo *signerInfo = NULL;
	TPCertInfo *signerInfoTBD = NULL;		// if non NULL at end, we delete
	/* we'll be verifying into this cert group */
	TPCertGroup	ocspCerts(vfyCtx.alloc, TGO_Caller);
	CSSM_BOOL verifiedToRoot;
	CSSM_BOOL verifiedToAnchor;
	CSSM_BOOL verifiedViaTrustSetting;

	const CSSM_APPLE_TP_OCSP_OPTIONS *ocspOpts = vfyCtx.ocspOpts;
	OcspIssuerStatus issuerStat;

	/*
	 * Set true if we ever find an apparent issuer which does not correctly
	 * pass signature verify. If true and we never success, that's a XXX error.
	 */
	bool foundBadIssuer = false;
	bool foundLocalResponder = false;
	uint32 numSignerCerts = ocspResp.numSignerCerts();

	/*
	 * This cert group, allocated by AppleTPSession::CertGroupVerify(),
	 * serves two functions here:
	 *
	 * -- it accumulates certs we get from the net (as parts of OCSP responses)
	 *    for user in verifying OCSPResponse-related certs.
	 *    TPCertGroup::buildCertGroup() uses this group as one of the many
	 *    sources of certs when building a cert chain.
	 *
	 * -- it provides a container into which to stash TPCertInfos which
	 *    persist at least as long as the TPVerifyContext; it's of type TGO_Group,
	 *    so all of the certs added to it get freed when the group does.
	 */
	assert(vfyCtx.signerCerts != NULL);

	TPCertGroup &gatheredCerts = vfyCtx.gatheredCerts;

	/* set up for disposal of TPCertInfos created by TPCertGroup::buildCertGroup() */
	TPCertGroup	certsToBeFreed(vfyCtx.alloc, TGO_Group);

	/*
	 * First job is to find the cert which signed this response.
	 * Give priority to caller's LocalResponderCert.
	 */
	if((ocspOpts != NULL) && (ocspOpts->LocalResponderCert != NULL)) {
		TPCertInfo *responderInfo = NULL;
		issuerStat = tpIsOcspIssuer(vfyCtx, ocspResp,
			ocspOpts->LocalResponderCert, NULL,
			OCT_Local, issuer, &responderInfo);
		switch(issuerStat) {
			case OIS_BadSig:
				foundBadIssuer = true;
				/* drop thru */
			case OIS_No:
				if(responderInfo != NULL) {
					/* can't use it - should this be an immediate error? */
					delete responderInfo;
				}
				break;
			case OIS_Good:
				assert(responderInfo != NULL);
				signerInfo = signerInfoTBD = responderInfo;
				foundLocalResponder = true;
				tpOcspDebug("tpVerifyOcspResp: signer := LocalResponderCert");
				break;
		}
	}

	if((signerInfo == NULL) && (numSignerCerts != 0)) {
		/*
		 * App did not specify a local responder (or provided a bad one)
		 * and the response came with some certs. Try those.
		 */
		TPCertInfo *respCert = NULL;
		for(unsigned dex=0; dex<numSignerCerts; dex++) {
			const CSSM_DATA *certData = ocspResp.signerCert(dex);
			if(signerInfo == NULL) {
				/* stop trying this after we succeed... */
				issuerStat = tpIsOcspIssuer(vfyCtx, ocspResp,
					certData, NULL,
					OCT_Provided, issuer, &respCert);
				switch(issuerStat) {
					case OIS_No:
						break;
					case OIS_Good:
						assert(respCert != NULL);
						signerInfo = signerInfoTBD = respCert;
						tpOcspDebug("tpVerifyOcspResp: signer := signerCert[%u]", dex);
						break;
					case OIS_BadSig:
						foundBadIssuer = true;
						break;
				}
			}
			else {
				/*
				 * At least add this cert to certGroup for verification.
				 * OcspCert will own the TPCertInfo.
				 */
				try {
					respCert = new TPCertInfo(vfyCtx.clHand, vfyCtx.cspHand, certData,
						TIC_CopyData, vfyCtx.verifyTime);
				}
				catch(...) {
					tpOcspDebug("tpVerifyOcspResp: BAD signerCert[%u]", dex);
				}
			}
			/* if we got a TPCertInfo, and it's not the signer, add it to certGroup */
			if((respCert != NULL) && (respCert != signerInfo)) {
				gatheredCerts.appendCert(respCert);
			}
		}
	}

	if((signerInfo == NULL) && (issuer != NULL)) {
		/*
		 * Haven't found it yet, try the actual issuer
		 */
		issuerStat = tpIsOcspIssuer(vfyCtx, ocspResp,
			NULL, issuer,
			OCT_Issuer, issuer, NULL);
		switch(issuerStat) {
			case OIS_BadSig:
				ourRtn = ORS_Unknown;
				cssmErr = CSSMERR_APPLETP_OCSP_SIG_ERROR;
				goto errOut;
			case OIS_No:
				break;
			case OIS_Good:
				signerInfo = issuer;
				tpOcspDebug("tpVerifyOcspResp: signer := issuer");
				break;
		}
	}

	if(signerInfo == NULL) {
		if((issuer != NULL) && !issuer->isStatusFatal(CSSMERR_APPLETP_OCSP_NO_SIGNER)) {
			/* user wants to proceed without verifying! */
			tpOcspDebug("tpVerifyOcspResp: no signer found, user allows!");
			ourRtn = ORS_Good;
		}
		else {
			tpOcspDebug("tpVerifyOcspResp: no signer found");
			ourRtn = ORS_Unknown;
			/* caller adds to per-cert status */
			cssmErr = CSSMERR_APPLETP_OCSP_NO_SIGNER;
		}
		goto errOut;
	}

	if(signerInfo != NULL && !foundLocalResponder) {
		/*
		 * tpIsOcspIssuer has verified that signerInfo is the signer of the
		 * OCSP response, and that it is either the issuer of the cert being
		 * checked or is a valid authorized responder for that issuer based on
		 * key id linkage and EKU. There is no stipulation in RFC2560 to also
		 * build the chain back to a trusted anchor; however, we'll continue to
		 * enforce this for the local responder case. (10742723)
		 */
		tpOcspDebug("tpVerifyOcspResp SUCCESS");
		ourRtn = ORS_Good;
		goto errOut;
	}

	/*
	 * Last remaining task is to verify the signer, and all the certs back to
	 * an anchor
	 */

	/* start from scratch with both of these groups */
	gatheredCerts.setAllUnused();
	vfyCtx.signerCerts->setAllUnused();
	crtn = ocspCerts.buildCertGroup(
			*signerInfo,			// subject item
			vfyCtx.signerCerts,		// inCertGroup the original group-to-be-verified
			vfyCtx.dbList,			// optional
			vfyCtx.clHand,
			vfyCtx.cspHand,
			vfyCtx.verifyTime,
			vfyCtx.numAnchorCerts,
			vfyCtx.anchorCerts,
			certsToBeFreed,			// local to-be-freed right now
			&gatheredCerts,			// accumulate gathered certs here
			CSSM_FALSE,				// subjectIsInGroup
			vfyCtx.actionFlags,
			vfyCtx.policyOid,
			vfyCtx.policyStr,
			vfyCtx.policyStrLen,
			kSecTrustSettingsKeyUseSignRevocation,
			verifiedToRoot,
			verifiedToAnchor,
			verifiedViaTrustSetting);
	if(crtn) {
		tpOcspDebug("tpVerifyOcspResp buildCertGroup failure");
		cssmErr = crtn;
		ourRtn = ORS_Unknown;
		goto errOut;
	}

	if(!verifiedToAnchor && !verifiedViaTrustSetting) {
		/* required */
		ourRtn = ORS_Unknown;
		if(verifiedToRoot) {
			/* verified to root which is not an anchor */
			tpOcspDebug("tpVerifyOcspResp root, no anchor");
			cssmErr = CSSMERR_APPLETP_OCSP_INVALID_ANCHOR_CERT;
		}
		else {
			/* partial chain, no root, not verifiable by anchor */
			tpOcspDebug("tpVerifyOcspResp no root, no anchor");
			cssmErr = CSSMERR_APPLETP_OCSP_NOT_TRUSTED;
		}
		if((issuer != NULL) && !issuer->isStatusFatal(cssmErr)) {
			tpOcspDebug("...ignoring last error per trust setting");
			ourRtn = ORS_Good;
		}
		else {
			ourRtn = ORS_Unknown;
		}
	}
	else {
		tpOcspDebug("tpVerifyOcspResp SUCCESS; chain verified");
		ourRtn = ORS_Good;
	}

	/* FIXME policy verify? */

errOut:
	delete signerInfoTBD;
	/* any other cleanup? */
	return ourRtn;
}
