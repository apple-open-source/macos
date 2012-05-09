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
 * tpOcspVerify.cpp - top-level OCSP verification
 */
 
#include "tpOcspVerify.h"
#include "tpdebugging.h"
#include "ocspRequest.h"
#include "tpOcspCache.h"
#include "tpOcspCertVfy.h"
#include <security_ocspd/ocspResponse.h>
#include "certGroupUtils.h"
#include <Security/certextensions.h>
#include <Security/oidsattr.h>
#include <Security/oidscert.h>
#include <security_asn1/SecNssCoder.h>
#include <security_ocspd/ocspdClient.h>
#include <security_ocspd/ocspdUtils.h>

#pragma mark ---- private routines ----

/* 
 * Get a smart CertID for specified cert and issuer
 */
static CSSM_RETURN tpOcspGetCertId(
	TPCertInfo			&subject,
	TPCertInfo			&issuer,
	OCSPClientCertID	*&certID)		/* mallocd by coder and RETURNED */
{
	CSSM_RETURN crtn;
	CSSM_DATA_PTR issuerSubject = NULL;
	CSSM_DATA_PTR issuerPubKeyData = NULL;
	CSSM_KEY_PTR issuerPubKey;
	CSSM_DATA_PTR subjectSerial = NULL;
	
	crtn = subject.fetchField(&CSSMOID_X509V1SerialNumber, &subjectSerial);
	if(crtn) {
		return crtn;
	}
	crtn = subject.fetchField(&CSSMOID_X509V1IssuerNameStd, &issuerSubject);
	if(crtn) {
		return crtn;
	}
	crtn = issuer.fetchField(&CSSMOID_CSSMKeyStruct, &issuerPubKeyData);
	if(crtn) {
		return crtn;
	}
	assert(issuerPubKeyData->Length == sizeof(CSSM_KEY));
	issuerPubKey = (CSSM_KEY_PTR)issuerPubKeyData->Data;
	certID = new OCSPClientCertID(*issuerSubject, issuerPubKey->KeyData, *subjectSerial);
	
	subject.freeField(&CSSMOID_X509V1SerialNumber, subjectSerial);
	issuer.freeField(&CSSMOID_X509V1IssuerNameStd, issuerSubject);
	issuer.freeField(&CSSMOID_CSSMKeyStruct, issuerPubKeyData);
	return CSSM_OK;
}

/* 
 * Examine cert, looking for AuthorityInfoAccess, with id-ad-ocsp URIs. Create
 * an NULL_terminated array of CSSM_DATAs containing the URIs if found. 
 */
static CSSM_DATA **tpOcspUrlsFromCert(
	TPCertInfo &subject, 
	SecNssCoder &coder)
{
	CSSM_DATA_PTR extField = NULL;
	CSSM_RETURN crtn;
	
	crtn = subject.fetchField(&CSSMOID_AuthorityInfoAccess, &extField);
	if(crtn) {
		tpOcspDebug("tpOcspUrlsFromCert: no AIA extension");
		return NULL;
	}
	if(extField->Length != sizeof(CSSM_X509_EXTENSION)) {
		tpErrorLog("tpOcspUrlsFromCert: malformed CSSM_FIELD");
		return NULL;
	}
	CSSM_X509_EXTENSION *cssmExt = (CSSM_X509_EXTENSION *)extField->Data;
	if(cssmExt->format != CSSM_X509_DATAFORMAT_PARSED) {
		tpErrorLog("tpOcspUrlsFromCert: malformed CSSM_X509_EXTENSION");
		return NULL;
	}
	
	CE_AuthorityInfoAccess *aia = 
		(CE_AuthorityInfoAccess *)cssmExt->value.parsedValue;
	CSSM_DATA **urls = NULL;
	unsigned numUrls = 0;
	for(unsigned dex=0; dex<aia->numAccessDescriptions; dex++) {
		CE_AccessDescription *ad = &aia->accessDescriptions[dex];
		if(!tpCompareCssmData(&ad->accessMethod, &CSSMOID_AD_OCSP)) {
			continue;
		}
		CE_GeneralName *genName = &ad->accessLocation;
		if(genName->nameType != GNT_URI) {
			tpErrorLog("tpOcspUrlsFromCert: CSSMOID_AD_OCSP, but not type URI");
			continue;
		}
		
		/* got one */
		if(urls == NULL) {
			urls = coder.mallocn<CSSM_DATA_PTR>(2);
		}
		else {
			/* realloc */
			CSSM_DATA **oldUrls = urls;
			urls = coder.mallocn<CSSM_DATA_PTR>(numUrls + 2);
			for(unsigned i=0; i<numUrls; i++) {
				urls[i] = oldUrls[i];
			}
		}
		urls[numUrls] = coder.mallocn<CSSM_DATA>();
		coder.allocCopyItem(genName->name, *urls[numUrls++]);
		urls[numUrls] = NULL;
		#ifndef	NDEBUG
		{
			CSSM_DATA urlStr;
			coder.allocItem(urlStr, genName->name.Length + 1);
			memmove(urlStr.Data, genName->name.Data, genName->name.Length);
			urlStr.Data[urlStr.Length-1] = '\0';
			tpOcspDebug("tpOcspUrlsFromCert: found URI %s", urlStr.Data);
		}
		#endif
	}
	subject.freeField(&CSSMOID_AuthorityInfoAccess, extField);
	return urls;
}

/* 
 * Create an SecAsn1OCSPDRequest for one cert. This consists of:
 *
 * -- cooking up an OCSPRequest if net fetch is enabled or a local responder
 *    is configured;
 * -- extracting URLs from subject cert if net fetch is enabled;
 * -- creating an SecAsn1OCSPDRequest, mallocd in coder's space;
 */
static SecAsn1OCSPDRequest *tpGenOcspdReq(
	TPVerifyContext		&vfyCtx,
	SecNssCoder			&coder,
	TPCertInfo			&subject,
	TPCertInfo			&issuer,
	OCSPClientCertID	&certId,
	const CSSM_DATA		**urls,		// from subject's AuthorityInfoAccess
	CSSM_DATA			&nonce)		// possibly mallocd in coder's space and RETURNED
{
	OCSPRequest				*ocspReq = NULL;
	SecAsn1OCSPDRequest		*ocspdReq = NULL;	// to return
	OCSPClientCertID		*certID = NULL;
	CSSM_RETURN				crtn;
	bool					deleteCertID = false;
	
	/* gather options or their defaults */
	CSSM_APPLE_TP_OCSP_OPT_FLAGS optFlags = 0;
	const CSSM_APPLE_TP_OCSP_OPTIONS *ocspOpts = vfyCtx.ocspOpts;
	CSSM_DATA_PTR localResponder = NULL;
	CSSM_DATA_PTR localResponderCert = NULL;
	if(ocspOpts != NULL) {
		optFlags = vfyCtx.ocspOpts->Flags;
		localResponder = ocspOpts->LocalResponder;
		localResponderCert = ocspOpts->LocalResponderCert;
	}
	bool genNonce = optFlags & CSSM_TP_OCSP_GEN_NONCE ? true : false;
	bool requireRespNonce = optFlags & CSSM_TP_OCSP_REQUIRE_RESP_NONCE ? true : false;
	
	/* 
	 * One degenerate case in case we can't really do anything.
	 * If no URI and no local responder, only proceed if cache is not disabled
	 * and we're requiring full OCSP per cert.
	 */
	if( ( (optFlags & CSSM_TP_ACTION_OCSP_CACHE_READ_DISABLE) ||
		  !(optFlags & CSSM_TP_ACTION_OCSP_REQUIRE_PER_CERT)
		) && 
	   (localResponder == NULL) &&  
	   (urls == NULL)) {
	   tpOcspDebug("tpGenOcspdReq: no route to OCSP; NULL return");
	   return NULL;
	}

	/* do we need an OCSP request? */
	if(!(optFlags & CSSM_TP_ACTION_OCSP_DISABLE_NET) || (localResponder != NULL)) {
		try {
			ocspReq = new OCSPRequest(subject, issuer, genNonce);
			certID = ocspReq->certID();
		}
		catch(...) {
			/* not sure how this could even happen but that was a fair amount of code */
			tpErrorLog("tpGenOcspdReq: error cooking up OCSPRequest\n");
			if(ocspReq != NULL) {
				delete ocspReq;
				return NULL;
			}
		}
	}
	
	/* certID needed one way or the other */
	if(certID == NULL) {
		crtn = tpOcspGetCertId(subject, issuer, certID);
		if(crtn) {
			goto errOut;
		}
		deleteCertID = true;
	}
	
	/*
	 * Create the SecAsn1OCSPDRequest. All fields optional.
	 */
	ocspdReq = coder.mallocn<SecAsn1OCSPDRequest>();
	memset(ocspdReq, 0, sizeof(*ocspdReq));
	if(optFlags & CSSM_TP_ACTION_OCSP_CACHE_WRITE_DISABLE) {
		ocspdReq->cacheWriteDisable = coder.mallocn<CSSM_DATA>();
		ocspdReq->cacheWriteDisable->Data = coder.mallocn<uint8>();
		ocspdReq->cacheWriteDisable->Data[0] = 0xff;
		ocspdReq->cacheWriteDisable->Length = 1;
	}
	/* 
	 * Note we're enforcing a not-so-obvious policy here: if nonce match is 
	 * required, disk cache reads by ocspd are disabled. In-core cache is 
	 * still enabled and hits in that cache do NOT require nonce matches. 
	 */
	if((optFlags & CSSM_TP_ACTION_OCSP_CACHE_READ_DISABLE) || requireRespNonce) {
		ocspdReq->cacheReadDisable = coder.mallocn<CSSM_DATA>();
		ocspdReq->cacheReadDisable->Data = coder.mallocn<uint8>();
		ocspdReq->cacheReadDisable->Data[0] = 0xff;
		ocspdReq->cacheReadDisable->Length = 1;
	}
	/* CertID, only required field */
	coder.allocCopyItem(*certID->encode(), ocspdReq->certID);
	if(ocspReq != NULL) {
		ocspdReq->ocspReq = coder.mallocn<CSSM_DATA>();
		coder.allocCopyItem(*ocspReq->encode(), *ocspdReq->ocspReq);
		if(genNonce) {
			/* nonce not available until encode() called */
			coder.allocCopyItem(*ocspReq->nonce(), nonce);
		}
	}
	if(localResponder != NULL) {
		ocspdReq->localRespURI = localResponder;
	}
	if(!(optFlags & CSSM_TP_ACTION_OCSP_DISABLE_NET)) {
		ocspdReq->urls = const_cast<CSSM_DATA **>(urls);
	}
	
errOut:
	delete ocspReq;
	if(deleteCertID) {
		delete certID;
	}
	return ocspdReq;
}

/* 
 * Apply a verified OCSPSingleResponse to a TPCertInfo. 
 */
static CSSM_RETURN tpApplySingleResp(
	OCSPSingleResponse				&singleResp,
	TPCertInfo						&cert,
	unsigned						dex,			// for debug
	CSSM_APPLE_TP_OCSP_OPT_FLAGS	flags,			// for OCSP_SUFFICIENT
	bool							&processed)		// set true iff CS_Good or CS_Revoked found
{
	SecAsn1OCSPCertStatusTag certStatus = singleResp.certStatus();
	CSSM_RETURN crtn = CSSM_OK;
	switch(certStatus) {
		case CS_Good:
			tpOcspDebug("tpApplySingleResp: CS_Good for cert %u", dex);
			cert.revokeCheckGood(true);
			if(flags & CSSM_TP_ACTION_OCSP_SUFFICIENT) {
				/* no more revocation checking necessary for this cert */
				cert.revokeCheckComplete(true);
			}
			processed = true;
			break;
		case CS_Revoked:
			tpOcspDebug("tpApplySingleResp: CS_Revoked for cert %u", dex);
			switch(singleResp.crlReason()) {
				case CE_CR_CertificateHold:
					crtn = CSSMERR_TP_CERT_SUSPENDED;
					break;
				default:
					/* FIXME - may want more detailed CRLReason-specific errors */
					crtn = CSSMERR_TP_CERT_REVOKED;
					break;
			}
			if(!cert.addStatusCode(crtn)) {
				crtn = CSSM_OK;
			}
			processed = true;
			break;
		case CS_Unknown:
			/* not an error, no per-cert status, nothing here */
			tpOcspDebug("tpApplySingleResp: CS_Unknown for cert %u", dex);
			break;
		default:
			tpOcspDebug("tpApplySingleResp: BAD certStatus (%d) for cert %u", 
					(int)certStatus, dex);
			if(cert.addStatusCode(CSSMERR_APPLETP_OCSP_STATUS_UNRECOGNIZED)) {
				crtn = CSSMERR_APPLETP_OCSP_STATUS_UNRECOGNIZED;
			}
			break;
	}
	return crtn;
}

/* 
 * An exceptional case: synchronously flush the OCSPD cache and send a new
 * resquest for just this one cert. 
 */
static OCSPResponse *tpOcspFlushAndReFetch(
	TPVerifyContext		&vfyCtx, 
	SecNssCoder			&coder, 
	TPCertInfo			&subject,
	TPCertInfo			&issuer, 
	OCSPClientCertID	&certID)
{
	const CSSM_DATA *derCertID = certID.encode();
	CSSM_RETURN crtn;
	
	crtn = ocspdCacheFlush(*derCertID);
	if(crtn) {
		#ifndef	NDEBUG
		cssmPerror("ocspdCacheFlush", crtn);
		#endif
		return NULL;
	}
	
	/* Cook up an OCSPDRequests, one request, just for this */
	/* send it to ocsdp */
	/* munge reply into an OCSPRsponse and return it */
	tpOcspDebug("pOcspFlushAndReFetch: Code on demand");
	return NULL;
}

class PendingRequest
{
public:
	PendingRequest(
		TPCertInfo &subject,
		TPCertInfo &issuer,
		OCSPClientCertID &cid,
		CSSM_DATA **u,
		unsigned dex);
	~PendingRequest()	{}
	
	TPCertInfo			&subject;
	TPCertInfo			&issuer;
	OCSPClientCertID	&certID;	// owned by caller
	CSSM_DATA			**urls;		// owner-managed array of URLs obtained from subject's
									// AuthorityInfoAccess.id-ad-ocsp. 
	CSSM_DATA			nonce;		// owner-managed copy of this requests' nonce, if it 
									//   has one 
	unsigned			dex;		// in inputCerts, for debug
	bool				processed;
};

PendingRequest::PendingRequest(
		TPCertInfo &subj,
		TPCertInfo &iss,
		OCSPClientCertID &cid,
		CSSM_DATA **u,
		unsigned dx)
		: subject(subj), issuer(iss), certID(cid), 
		  urls(u), dex(dx), processed(false)
{
	nonce.Data = NULL;
	nonce.Length = 0;
}

#pragma mark ---- public API ----

CSSM_RETURN tpVerifyCertGroupWithOCSP(
	TPVerifyContext	&vfyCtx,
	TPCertGroup 	&certGroup)		// to be verified 
{
	assert(vfyCtx.clHand != 0);
	assert(vfyCtx.policy == kRevokeOcsp);
	
	CSSM_RETURN ourRtn = CSSM_OK;
	OcspRespStatus respStat;
	SecNssCoder coder;
	CSSM_RETURN crtn;
	SecAsn1OCSPDRequests ocspdReqs;
	SecAsn1OCSPReplies ocspdReplies;
	unsigned numRequests = 0;			// in ocspdReqs
	CSSM_DATA derOcspdRequests = {0, NULL};
	CSSM_DATA derOcspdReplies = {0, NULL};
	uint8 version = OCSPD_REQUEST_VERS;
	unsigned numReplies;
	unsigned numCerts = certGroup.numCerts();
	if(numCerts <= 1) {
		/* Can't verify without an issuer; we're done */
		return CSSM_OK;
	}
	numCerts--;
	
	/* gather options or their defaults */
	CSSM_APPLE_TP_OCSP_OPT_FLAGS optFlags = 0;
	const CSSM_APPLE_TP_OCSP_OPTIONS *ocspOpts = vfyCtx.ocspOpts;
	CSSM_DATA_PTR localResponder = NULL;
	CSSM_DATA_PTR localResponderCert = NULL;
	bool cacheReadDisable = false;
	bool cacheWriteDisable = false;
	bool genNonce = false;			// in outgoing request
	bool requireRespNonce = false;	// in incoming response
	PRErrorCode prtn;
	
	if(ocspOpts != NULL) {
		optFlags = vfyCtx.ocspOpts->Flags;
		localResponder = ocspOpts->LocalResponder;
		localResponderCert = ocspOpts->LocalResponderCert;
	}
	if(optFlags & CSSM_TP_ACTION_OCSP_CACHE_READ_DISABLE) {
		cacheReadDisable = true;
	}
	if(optFlags & CSSM_TP_ACTION_OCSP_CACHE_WRITE_DISABLE) {
		cacheWriteDisable = true;
	}
	if(optFlags & CSSM_TP_OCSP_GEN_NONCE) {
		genNonce = true;
	}
	if(optFlags & CSSM_TP_OCSP_REQUIRE_RESP_NONCE) {
		requireRespNonce = true;
	}
	if(requireRespNonce & !genNonce) {
		/* no can do */
		tpErrorLog("tpVerifyCertGroupWithOCSP: requireRespNonce, !genNonce\n");
		return CSSMERR_TP_INVALID_REQUEST_INPUTS;
	}
	
	tpOcspDebug("tpVerifyCertGroupWithOCSP numCerts %u optFlags 0x%lx", 
		numCerts, (unsigned long)optFlags);
		
	/*
	 * create list of pendingRequests parallel to certGroup
	 */
	PendingRequest **pending = coder.mallocn<PendingRequest *>(numCerts);
	memset(pending, 0, (numCerts * sizeof(PendingRequest *)));
	
	for(unsigned dex=0; dex<numCerts; dex++) {
		OCSPClientCertID *certID = NULL;
		TPCertInfo *subject = certGroup.certAtIndex(dex);
		
		if(subject->trustSettingsFound()) {
			/* functionally equivalent to root - we're done */
			tpOcspDebug("...tpVerifyCertGroupWithOCSP: terminate per user trust at %u", 
				(unsigned)dex);
			goto postOcspd;
		}
		TPCertInfo *issuer = certGroup.certAtIndex(dex + 1);
		crtn = tpOcspGetCertId(*subject, *issuer, certID);
		if(crtn) {
			tpErrorLog("tpVerifyCertGroupWithOCSP: error extracting cert fields; "
				"aborting\n");
			goto errOut;
		}
		
		/* 
		 * We use the URLs in the subject cert's AuthorityInfoAccess extension
		 * for two things - mainly to get the URL(s) for actual OCSP transactions,
		 * but also for CSSM_TP_ACTION_OCSP_REQUIRE_IF_RESP_PRESENT processing.
		 * So, we do the per-cert processing to get them right now even if we
		 * wind up using a local responder or getting verification from cache. 
		 */
		CSSM_DATA **urls = tpOcspUrlsFromCert(*subject, coder);
		pending[dex] = new PendingRequest(*subject, *issuer, *certID, urls, dex);
	}
	/* subsequent errors to errOut: */
	
	/* 
	 * Create empty SecAsn1OCSPDRequests big enough for all certs 
	 */
	ocspdReqs.requests = coder.mallocn<SecAsn1OCSPDRequest *>(numCerts + 1);
	memset(ocspdReqs.requests, 0, (numCerts + 1) * sizeof(SecAsn1OCSPDRequest *));
	ocspdReqs.version.Data = &version;
	ocspdReqs.version.Length = 1;
	
	/* 
	 * For each cert, either obtain a cached OCSPResponse, or create
	 * a request to get one. 
	 *
	 * NOTE: in-core cache reads (via tpOcspCacheLookup() do NOT involve a
	 * nonce check, no matter what the app says. If nonce checking is required by the
	 * app, responses don't get added to cache if the nonce doesn't match, but once
	 * a response is validated and added to cache it's fair game for that task. 
	 */
	for(unsigned dex=0; dex<numCerts; dex++) {
		PendingRequest *pendReq = pending[dex];
		OCSPSingleResponse *singleResp = NULL;
		if(!cacheReadDisable) {
			singleResp = tpOcspCacheLookup(pendReq->certID, localResponder);
		}
		if(singleResp) {
			tpOcspDebug("...tpVerifyCertGroupWithOCSP: localCache hit (1) dex %u", 
				(unsigned)dex);
			crtn = tpApplySingleResp(*singleResp, pendReq->subject, dex, optFlags,
				pendReq->processed);
			delete singleResp;
			if(pendReq->processed) {
				/* definitely done with this cert one way or the other */
				if(crtn && (ourRtn == CSSM_OK)) {
					/* real cert error: first error encountered; we'll keep going */
					ourRtn = crtn;
				}
				continue;
			}
			if(crtn) {
				/* 
				 * This indicates a bad cached response. Well that's kinda weird, let's 
				 * just flush this out and try a normal transaction.
				 */
				tpOcspCacheFlush(pendReq->certID);
			}
		}
		
		/* 
		 * Prepare a request for ocspd
		 */
		SecAsn1OCSPDRequest *ocspdReq = tpGenOcspdReq(vfyCtx, coder, 
			pendReq->subject, pendReq->issuer, pendReq->certID, 
			const_cast<const CSSM_DATA **>(pendReq->urls),
			pendReq->nonce);
		if(ocspdReq == NULL) {
			/* tpGenOcspdReq determined there was no route to OCSP responder */
			tpOcspDebug("tpVerifyCertGroupWithOCSP: no OCSP possible for cert %u", dex);
			continue;
		}
		ocspdReqs.requests[numRequests++] = ocspdReq;
	}
	if(numRequests == 0) {
		/* no candidates for OCSP: almost done */
		goto postOcspd;
	}
	
	/* ship requests off to ocspd, get ocspReplies back */
	if(coder.encodeItem(&ocspdReqs, kSecAsn1OCSPDRequestsTemplate, derOcspdRequests)) {
		tpErrorLog("tpVerifyCertGroupWithOCSP: error encoding ocspdReqs\n");
		ourRtn = CSSMERR_TP_INTERNAL_ERROR;
		goto errOut;
	}
	crtn = ocspdFetch(vfyCtx.alloc, derOcspdRequests, derOcspdReplies);
	if(crtn) {
		tpErrorLog("tpVerifyCertGroupWithOCSP: error during ocspd RPC\n");
		#ifndef	NDEBUG
		cssmPerror("ocspdFetch", crtn);
		#endif
		/* But this is not necessarily fatal...update per-cert status and check 
		 * caller requirements below */
		goto postOcspd;
	}
	memset(&ocspdReplies, 0, sizeof(ocspdReplies));
	prtn = coder.decodeItem(derOcspdReplies, kSecAsn1OCSPDRepliesTemplate, 
		&ocspdReplies);
	/* we're done with this, mallocd in ocspdFetch() */
	vfyCtx.alloc.free(derOcspdReplies.Data);
	if(prtn) {
		/* 
		 * This can happen when an OCSP server provides bad data...we cannot
		 * determine which cert is associated with this bad response;
		 * just flag it with the first one and proceed to the loop that
		 * handles CSSM_TP_ACTION_OCSP_REQUIRE_PER_CERT.
		 */
		tpErrorLog("tpVerifyCertGroupWithOCSP: error decoding ocspd reply\n");
		pending[0]->subject.addStatusCode(CSSMERR_APPLETP_OCSP_BAD_RESPONSE);
		goto postOcspd;
	}
	if((ocspdReplies.version.Length != 1) ||
	   (ocspdReplies.version.Data[0] != OCSPD_REPLY_VERS)) {
		tpErrorLog("tpVerifyCertGroupWithOCSP: ocspd reply version mismatch\n");
		if(ourRtn == CSSM_OK) {
			ourRtn = CSSMERR_TP_INTERNAL_ERROR;	// maybe something better?
		}
		goto errOut;
	}
	
	/* process each reply */
	numReplies = ocspdArraySize((const void **)ocspdReplies.replies);
	for(unsigned dex=0; dex<numReplies; dex++) {
		SecAsn1OCSPDReply *reply = ocspdReplies.replies[dex];
		
		/* Cook up our version of an OCSPResponse from the encoded data */
		OCSPResponse *ocspResp = NULL;
		try {
			ocspResp = new OCSPResponse(reply->ocspResp, TP_OCSP_CACHE_TTL);
		}
		catch(...) {
			tpErrorLog("tpVerifyCertGroupWithOCSP: error decoding ocsp response\n");
			/* what the heck, keep going */
			continue;
		}
		
		/* 
		 * Find matching subject cert if possible (it's technically optional for 
		 * verification of the response in some cases, e.g., local responder).
		 */
		PendingRequest *pendReq = NULL;				// fully qualified
		PendingRequest *reqWithIdMatch = NULL;		// CertID match only, not nonce
		for(unsigned pdex=0; pdex<numCerts; pdex++) {
		
			/* first check ID match; that is required no matter what */
			if((pending[pdex])->certID.compareToExist(reply->certID)) {
				reqWithIdMatch = pending[pdex];
			}
			if(reqWithIdMatch == NULL) {
				continue;
			}
			if(!genNonce) {
				/* that's good enough */
				pendReq = reqWithIdMatch;
				tpOcspDebug("OCSP processs reply: CertID match, no nonce");
				break;
			}
			if(tpCompareCssmData(&reqWithIdMatch->nonce, ocspResp->nonce())) {
				tpOcspDebug("OCSP processs reply: nonce MATCH");
				pendReq = reqWithIdMatch;
				break;
			}
			
			/*
			 * In this case we keep going; if we never find a match, then we can 
			 * use reqWithIdMatch if !requireRespNonce.
			 */
			tpOcspDebug("OCSP processs reply: certID match, nonce MISMATCH");
		}
		if(pendReq == NULL) {
			if(requireRespNonce) {
				tpOcspDebug("OCSP processs reply: tossing out response due to "
						"requireRespNonce");
				delete ocspResp;
				if(ourRtn == CSSM_OK) {
					ourRtn = CSSMERR_APPLETP_OCSP_NONCE_MISMATCH;
				}
				continue;
			}
			if(reqWithIdMatch != NULL) {
				/*
				 * Nonce mismatch but caller thinks that's OK. Log it and proceed.
				 */
				assert(genNonce);
				tpOcspDebug("OCSP processs reply: using bad nonce due to !requireRespNonce");
				pendReq = reqWithIdMatch;
				pendReq->subject.addStatusCode(CSSMERR_APPLETP_OCSP_NONCE_MISMATCH);
			}
		}
		TPCertInfo *issuer = NULL;
		if(pendReq != NULL) {
			issuer = &pendReq->issuer;
		}
		
		/* verify response and either throw out or add to local cache */
		respStat = tpVerifyOcspResp(vfyCtx, coder, issuer, *ocspResp, crtn);
		switch(respStat) {
			case ORS_Good:
				break;
			case ORS_Unknown:
				/* not an error but we can't use it */
				if((crtn != CSSM_OK) && (pendReq != NULL)) {
					/* pass this info back to caller here... */
					pendReq->subject.addStatusCode(crtn);
				}
				delete ocspResp;
				continue;
			case ORS_Bad:
				delete ocspResp;
				/* 
				 * An exceptional case: synchronously flush the OCSPD cache and send a 
				 * new request for just this one cert. 
				 * FIXME: does this really buy us anything? A DOS attacker who managed
				 * to get this bogus response into our cache is likely to be able
				 * to do it again and again.
				 */
				tpOcspDebug("tpVerifyCertGroupWithOCSP: flush/refetch for cert %u", dex);
				ocspResp = tpOcspFlushAndReFetch(vfyCtx, coder, pendReq->subject,
					pendReq->issuer, pendReq->certID);
				if(ocspResp == NULL) {
					tpErrorLog("tpVerifyCertGroupWithOCSP: error on flush/refetch\n");
					ourRtn = CSSMERR_APPLETP_OCSP_BAD_RESPONSE;
					goto errOut;
				}
				respStat = tpVerifyOcspResp(vfyCtx, coder, issuer, *ocspResp, crtn);
				if(respStat != ORS_Good) {
					tpErrorLog("tpVerifyCertGroupWithOCSP: verify error after "
							"flush/refetch\n");
					if((crtn != CSSM_OK) && (pendReq != NULL)) {
						/* pass this info back to caller here... */
						if(pendReq->subject.addStatusCode(crtn)) {
							ourRtn = CSSMERR_APPLETP_OCSP_BAD_RESPONSE;
						}
					}
					else {
						ourRtn = CSSMERR_APPLETP_OCSP_BAD_RESPONSE;
					}
					goto errOut;
				}
				/* Voila! Recovery. Proceed. */
				tpOcspDebug("tpVerifyCertGroupWithOCSP: refetch for cert %u SUCCEEDED", 
						dex);
				break;
		} /* switch response status */
		
		if(!cacheWriteDisable) {
			tpOcspCacheAdd(reply->ocspResp, localResponder);
		}
		
		/* attempt to apply to pendReq */
		if(pendReq != NULL) {
			OCSPSingleResponse *singleResp = 
				ocspResp->singleResponseFor(pendReq->certID);
			if(singleResp) {
				crtn = tpApplySingleResp(*singleResp, pendReq->subject, pendReq->dex, 
					optFlags, pendReq->processed);
				if(crtn && (ourRtn == CSSM_OK)) {
					ourRtn = crtn;
				}
				delete singleResp;
			}
		}	/* a reply which matches a pending request */
		
		/* 
		 * Done with this - note local OCSP response cache doesn't store this
		 * object; it stores an encoded copy.
		 */
		delete ocspResp;
	}	/* for each reply */

postOcspd:

	/* 
	 * Now process each cert which hasn't had an OCSP response applied to it. 
	 * This can happen if we get back replies which are not strictly in 1-1 sync with 
	 * our requests but which nevertheless contain valid info for more than one
	 * cert each.
	 */
	for(unsigned dex=0; dex<numCerts; dex++) {
		PendingRequest *pendReq = pending[dex];
		if(pendReq == NULL) {
			/* i.e. terminated due to user trust */
			tpOcspDebug("...tpVerifyCertGroupWithOCSP: NULL pendReq dex %u", 
					(unsigned)dex);
			break;
		}
		if(pendReq->processed) {
			continue;
		}
		OCSPSingleResponse *singleResp = NULL;
		/* Note this corner case will not work if cache is disabled. */
		if(!cacheReadDisable) {
			singleResp = tpOcspCacheLookup(pendReq->certID, localResponder);
		}
		if(singleResp) {
			tpOcspDebug("...tpVerifyCertGroupWithOCSP: localCache (2) hit dex %u", 
					(unsigned)dex);
			crtn = tpApplySingleResp(*singleResp, pendReq->subject, dex, optFlags, 
					pendReq->processed);
			if(crtn) {
				if(ourRtn == CSSM_OK) {
					ourRtn = crtn;
				}
			}
			delete singleResp;
		}
		if(!pendReq->processed) {
			/* Couldn't perform OCSP for this cert. */
			tpOcspDebug("tpVerifyCertGroupWithOCSP: OCSP_UNAVAILABLE for cert %u", dex);
			bool required = false;
			CSSM_RETURN responseStatus = CSSM_OK;
			if(pendReq->subject.numStatusCodes() > 0) {
				/*
				 * Check whether we got a response for this cert, but it was rejected
				 * due to being improperly signed. That should result in an actual
				 * error, even under Best Attempt processing. (10743149)
				 */
				if(pendReq->subject.hasStatusCode(CSSMERR_APPLETP_OCSP_BAD_RESPONSE)) {
					responseStatus = CSSMERR_APPLETP_OCSP_BAD_RESPONSE;
				} else if(pendReq->subject.hasStatusCode(CSSMERR_APPLETP_OCSP_SIG_ERROR)) {
					responseStatus = CSSMERR_APPLETP_OCSP_SIG_ERROR;
				} else if(pendReq->subject.hasStatusCode(CSSMERR_APPLETP_OCSP_NO_SIGNER)) {
					responseStatus = CSSMERR_APPLETP_OCSP_NO_SIGNER;
				}
			}
			if(responseStatus == CSSM_OK) {
				/* no response available (as opposed to getting an invalid response) */
				pendReq->subject.addStatusCode(CSSMERR_APPLETP_OCSP_UNAVAILABLE);
			}
			if(optFlags & CSSM_TP_ACTION_OCSP_REQUIRE_PER_CERT) {
				/* every cert needs OCSP */
				tpOcspDebug("tpVerifyCertGroupWithOCSP: response required for all certs, missing for cert %u", dex);
				required = true;
			}
			else if(optFlags & CSSM_TP_ACTION_OCSP_REQUIRE_IF_RESP_PRESENT) {
				/* this cert needs OCSP if it had an AIA extension with an OCSP URI */
				if(pendReq->urls) {
					tpOcspDebug("tpVerifyCertGroupWithOCSP: OCSP URI present but no valid response for cert %u", dex);
					required = true;
				}
			}
			if( (required && pendReq->subject.isStatusFatal(CSSMERR_APPLETP_OCSP_UNAVAILABLE)) ||
				(responseStatus != CSSM_OK && pendReq->subject.isStatusFatal(responseStatus)) ) {
				/* fatal error, but we keep on processing */
				if(ourRtn == CSSM_OK) {
					ourRtn = (responseStatus != CSSM_OK) ? responseStatus : CSSMERR_APPLETP_OCSP_UNAVAILABLE;
				}
			}
		}
	}
errOut:	
	for(unsigned dex=0; dex<numCerts; dex++) {
		PendingRequest *pendReq = pending[dex];
		if(pendReq == NULL) {
			/* i.e. terminated due to user trust */
			break;
		}
		delete &pendReq->certID;
		delete pendReq;
	}
	return ourRtn;
}
