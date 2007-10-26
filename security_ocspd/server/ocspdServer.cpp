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
 * ocspdServer.cpp - Server class for OCSP helper
 *
 * Created 6 July 2004 by dmitch
 */

#include "ocspdServer.h"
#include <security_ocspd/ocspdDebug.h>
#include <security_ocspd/ocspdUtils.h>
#include "ocspdNetwork.h"
#include "ocspdDb.h"
#include "crlDb.h"
#include <Security/Security.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecAsn1Coder.h>
#include <Security/ocspTemplates.h>
#include <Security/cssmapple.h>
#include <stdlib.h>
#include <security_ocspd/ocspd.h>						/* created by MIG */

#pragma mark ----- OCSP utilities -----

/* 
 * Once we've gotten a response from a server, cook up a SecAsn1OCSPDReply.
 */
static SecAsn1OCSPDReply *ocspdGenReply(
	SecAsn1CoderRef coder, 
	const CSSM_DATA &resp,
	const CSSM_DATA &certID)
{
	SecAsn1OCSPDReply *ocspdRep = 
		(SecAsn1OCSPDReply *)SecAsn1Malloc(coder, sizeof(*ocspdRep));
	SecAsn1AllocCopyItem(coder, &resp, &ocspdRep->ocspResp);
	SecAsn1AllocCopyItem(coder, &certID, &ocspdRep->certID);
	return ocspdRep;
}

static SecAsn1OCSPDReply *ocspdHandleReq(
	SecAsn1CoderRef coder, 
	SecAsn1OCSPDRequest &request)
{
	CSSM_DATA derResp = {0, NULL};
	CSSM_RETURN crtn;	
	bool cacheReadDisable = false;
	bool cacheWriteDisable = false;
		
	if((request.cacheReadDisable != NULL) &&
	   (request.cacheReadDisable->Length != 0) &&
	   (request.cacheReadDisable->Data[0] != 0)) {
		cacheReadDisable = true;
	}
	if((request.cacheWriteDisable != NULL) &&
	   (request.cacheWriteDisable->Length != 0) &&
	   (request.cacheWriteDisable->Data[0] != 0)) {
		cacheWriteDisable = true;
	}

	if(!cacheReadDisable) {
		/* do a cache lookup */
		bool found = ocspdDbCacheLookup(coder, request.certID, request.localRespURI,
			derResp);
		if(found) {
			return ocspdGenReply(coder, derResp, request.certID);
		}
	}
	
	if(request.localRespURI) {
		if(request.ocspReq == NULL) {
			ocspdErrorLog("ocspdHandleReq: localRespURI but no request to send\n");
			return NULL;
		}
		crtn = ocspdHttpPost(coder, *request.localRespURI, *request.ocspReq, derResp);
		if(crtn == CSSM_OK) {
			SecAsn1OCSPDReply *reply = ocspdGenReply(coder, derResp, request.certID);
			if(!cacheWriteDisable) {
				ocspdDbCacheAdd(derResp, *request.localRespURI);
			}
			return reply;
		}
	}
	
	/* now try everything in requests.urls, the normal case */
	unsigned numUris = ocspdArraySize((const void **)request.urls);
	for(unsigned dex=0; dex<numUris; dex++) {
		CSSM_DATA *uri = request.urls[dex];
		crtn = ocspdHttpPost(coder, *uri, *request.ocspReq, derResp);
		if(crtn == CSSM_OK) {
			SecAsn1OCSPDReply *reply = ocspdGenReply(coder, derResp, request.certID);
			if(!cacheWriteDisable) {
				ocspdDbCacheAdd(derResp, *uri);
			}
			return reply;
		}
	}

	return NULL;
}

#pragma mark ----- Mig-referenced OCSP routines -----

/* all of these Mig-referenced routines are called out from ocspd_server() */

kern_return_t ocsp_server_ocspdFetch (
	mach_port_t serverport,
	Data ocspd_req,
	mach_msg_type_number_t ocspd_reqCnt,
	Data *ocspd_rep,
	mach_msg_type_number_t *ocspd_repCnt)
{
	ServerActivity();
	ocspdDebug("ocsp_server_ocspFetch top");
	*ocspd_rep = NULL;
	*ocspd_repCnt = 0;
	kern_return_t krtn = 0;
	unsigned numRequests;
	SecAsn1OCSPReplies replies;
	unsigned numReplies = 0;
	uint8 version = OCSPD_REPLY_VERS;
	
	/* decode top-level SecAsn1OCSPDRequests */
	SecAsn1CoderRef coder;
	SecAsn1CoderCreate(&coder);
	SecAsn1OCSPDRequests requests;
	memset(&requests, 0, sizeof(requests));
	if(SecAsn1Decode(coder, ocspd_req, ocspd_reqCnt, kSecAsn1OCSPDRequestsTemplate,
			&requests)) {
		ocspdErrorLog("ocsp_server_ocspdFetch: decode error\n");
		krtn = CSSMERR_APPLETP_OCSP_BAD_REQUEST;
		goto errOut;
	}
	if((requests.version.Length == 0) ||
	   (requests.version.Data[0] != OCSPD_REQUEST_VERS)) {
		/* 
		 * Eventually handle backwards compatibility here
		 */
		ocspdErrorLog("ocsp_server_ocspdFetch: request version mismatch\n");
		krtn = CSSMERR_APPLETP_OCSP_BAD_REQUEST;
		goto errOut;
	}
	
	numRequests = ocspdArraySize((const void **)requests.requests);
	replies.replies = (SecAsn1OCSPDReply **)SecAsn1Malloc(coder, (numRequests + 1) * 
		sizeof(SecAsn1OCSPDReply *));
	memset(replies.replies, 0, (numRequests + 1) * sizeof(SecAsn1OCSPDReply *));
	replies.version.Data = &version;
	replies.version.Length = 1;
	
	/* preparing for net fetch: enable another thread */
	OcspdServer::active().longTermActivity();

	/* This may need to be threaded, one thread per request */
	for(unsigned dex=0; dex<numRequests; dex++) {
		SecAsn1OCSPDReply *reply = ocspdHandleReq(coder, *(requests.requests[dex]));
		if(reply != NULL) {
			replies.replies[numReplies++] = reply;
		}
	}
	
	/* if we got any replies, sent them back to client */
	if(replies.replies[0] != NULL) {
		CSSM_DATA derRep = {0, NULL};
		if(SecAsn1EncodeItem(coder, &replies, kSecAsn1OCSPDRepliesTemplate,
				&derRep)) {
			ocspdErrorLog("ocsp_server_ocspdFetch: encode error\n");
			krtn = CSSMERR_TP_INTERNAL_ERROR;
			goto errOut;
		}
		
		/* 
		 * Use server's persistent Allocator to alloc this and tell 
		 * MachServer to dealloc after the RPC completes
		 */
		Allocator &alloc = OcspdServer::active().alloc();
		*ocspd_rep = alloc.malloc(derRep.Length);
		memmove(*ocspd_rep, derRep.Data, derRep.Length);
		*ocspd_repCnt = derRep.Length;
		MachPlusPlus::MachServer::active().releaseWhenDone(alloc, *ocspd_rep);
	}
	ocspdDebug("ocsp_server_ocspFetch returning %u bytes of replies", 
		(unsigned)*ocspd_repCnt);

errOut:
	SecAsn1CoderRelease(coder);
	return krtn;
}

kern_return_t ocsp_server_ocspdCacheFlush (
	mach_port_t serverport,
	Data certID,
	mach_msg_type_number_t certIDCnt)
{
	ServerActivity();
	ocspdDebug("ocsp_client_ocspdCacheFlush");
	CSSM_DATA certIDData = {certIDCnt, (uint8 *)certID};
	ocspdDbCacheFlush(certIDData);
	return 0;
}

kern_return_t ocsp_server_ocspdCacheFlushStale (
	mach_port_t serverport)
{
	ServerActivity();
	ocspdDebug("ocsp_server_ocspdCacheFlushStale");
	ocspdDbCacheFlushStale();
	return 0;

}

/*
 * Given a CSSM_DATA which was allocated in our server's alloc space, 
 * pass referent data back to caller and schedule a dealloc after the RPC
 * completes with MachServer.
 */
void passDataToCaller(
	CSSM_DATA		&srcData,		// allocd in our server's alloc space
	Data			*outData,
	mach_msg_type_number_t *outDataCnt)
{
	Allocator &alloc = OcspdServer::active().alloc();
	*outData    = srcData.Data;
	*outDataCnt = srcData.Length;
	MachPlusPlus::MachServer::active().releaseWhenDone(alloc, srcData.Data);
}

#pragma mark ----- Mig-referenced routines for cert and CRL maintenance -----

/* 
 * Fetch a cert from the net. Currently we don't do any validation at all - 
 * should we? 
 *
 * I'm sure someone will ask "why don't we cache these certs?"; the main reason
 * is that the keychain schema does not allow for the storage of an expiration
 * date attribute or a URI, so we'd have to cook up yet another cert schema
 * (The system already has two - the Open Group standard and the custom version
 * we use) and I really don't want to do that. 
 *
 * Perhaps a future enhancement. As of Tiger this feature is never really used
 * (though it certainly works). 
 */
kern_return_t ocsp_server_certFetch (
	mach_port_t serverport,
	Data cert_url,
	mach_msg_type_number_t cert_urlCnt,
	Data *cert_data,
	mach_msg_type_number_t *cert_dataCnt)
{
	ServerActivity();
	CSSM_DATA urlData = { cert_urlCnt, (uint8 *)cert_url};
	CSSM_DATA certData = {0, NULL};
	kern_return_t krtn;
	
	/* preparing for net fetch: enable another thread */
	OcspdServer::active().longTermActivity();

	krtn = ocspdNetFetch(OcspdServer::active().alloc(), urlData, LT_Cert, certData);
	/* if we got any data, sent it back to client */
	if(krtn == 0) {
		if(certData.Length == 0) {
			ocspdErrorLog("ocsp_server_certFetch: no cert found\n");
			krtn = CSSMERR_APPLETP_NETWORK_FAILURE;
		}
		else {
			/* 
			 * We used server's persistent Allocator to alloc this; tell 
			 * MachServer to dealloc after the RPC completes
			 */
			passDataToCaller(certData, cert_data, cert_dataCnt);
		}
	}
	ocspdCrlDebug("ocsp_server_certFetch returning %lu bytes", certData.Length);
	return krtn;
}

kern_return_t ocsp_server_crlFetch (
	mach_port_t serverport,
	Data crl_url,
	mach_msg_type_number_t crl_urlCnt,
	Data crl_issuer,					// optional
	mach_msg_type_number_t crl_issuerCnt,
	boolean_t cache_read,
	boolean_t cache_write,
	Data verifyTime,
	mach_msg_type_number_t verifyTimeCnt,
	Data *crl_data,
	mach_msg_type_number_t *crl_dataCnt)
{
	ServerActivity();
	const CSSM_DATA urlData = {crl_urlCnt, (uint8 *)crl_url};
	CSSM_DATA crlData = {0, NULL};
	Allocator &alloc = OcspdServer::active().alloc();
	
	/*
	 * 1. Read from cache if enabled. Look up by issuer if we have it, else
	 *    look up by URL. Per Radar 4565280, the same CRL might be
	 *    vended from different URLs; we don't care where we got it 
	 *    from at this point as long as the client knew - by the absence
	 *    of a crlIssuer field in the crlDistributionPoints extension - 
	 *    that the issuer of the CRL is the same as the issuer of the cert
	 *    being verified. 
	 */
	if(cache_read) {
		const CSSM_DATA vfyTimeData = {verifyTimeCnt, (uint8 *)verifyTime};
		const CSSM_DATA issuerData  = {crl_issuerCnt, (uint8 *)crl_issuer};
		const CSSM_DATA *issuerPtr;
		const CSSM_DATA *urlPtr;
		bool brtn;
		
		if(crl_issuerCnt) {
			/* look up by issuer */
			issuerPtr = &issuerData;
			urlPtr    = NULL;
		}
		else {
			/* look up by URL */
			issuerPtr = NULL;
			urlPtr    = &urlData;
		}
		brtn = crlCacheLookup(alloc, urlPtr, issuerPtr, vfyTimeData, crlData);
		if(brtn) {
			/* Cache hit: Pass CRL back to caller & dealloc */
			assert((crlData.Data != NULL) && (crlData.Length != 0));
			passDataToCaller(crlData, crl_data, crl_dataCnt);
			return 0;
		}
	}
	
	/* 
	 * 2. Obtain from net
	 */
	CSSM_RETURN crtn;

	/* preparing for net fetch: enable another thread */
	OcspdServer::active().longTermActivity();

	crtn = ocspdNetFetch(alloc, urlData, LT_Crl, crlData);
	if(crtn) {
		ocspdErrorLog("server_crlFetch: CRL not found on net");
		return CSSMERR_APPLETP_NETWORK_FAILURE;
	}
	else {
		if(crlData.Length == 0) {
			ocspdErrorLog("server_crlFetch: no CRL data found\n");
			return CSSMERR_APPLETP_NETWORK_FAILURE;
		}
		else {
			/* Pass CRL back to caller & dealloc */
			passDataToCaller(crlData, crl_data, crl_dataCnt);
			ocspdCrlDebug("ocsp_server_crlFetch got %lu bytes from net", crlData.Length);
		}
	}
	
	/* 
	 * 3. Add to cache if enabled
	 */
	if(cache_write) {
		crlCacheAdd(crlData, urlData);
	}

	return 0;
}

kern_return_t ocsp_server_crlRefresh
(
	mach_port_t serverport,
	uint32_t stale_days,
	uint32_t expire_overlap_seconds,
	boolean_t purge_all,
	boolean_t full_crypto_verify)
{
	/* preparing for possible CRL verify, requiring an RPC to ourself
     * for Trust Settings fetch. enable another thread. */
	ServerActivity();
	OcspdServer::active().longTermActivity();
	crlCacheRefresh(stale_days, expire_overlap_seconds, purge_all, 
		full_crypto_verify, true);
	return 0;
}

kern_return_t ocsp_server_crlFlush(
	mach_port_t serverport,
	Data cert_url,
	mach_msg_type_number_t cert_urlCnt)
{
	ServerActivity();
	CSSM_DATA urlData = {cert_urlCnt, (uint8 *)cert_url};
	crlCacheFlush(urlData);
	return 0;
}

#pragma mark ----- MachServer::Timer subclass to handle periodic flushes of DB caches -----

#define OCSPD_REFRESH_DEBUG		0

#if		!OCSPD_REFRESH_DEBUG
/* fire a minute after we launch, then once a week */
#define OCSPD_TIMER_FIRST		(60.0)
#define OCSPD_TIMER_INTERVAL	(60.0 * 60.0 * 24.0 * 7.0)

#else
#define OCSPD_TIMER_FIRST		(10.0)
#define OCSPD_TIMER_INTERVAL	(60.0)
#endif

void OcspdServer::OcspdTimer::action()
{
	secdebug("ocspdRefresh", "OcspdTimer firing");
	ocspdDbCacheFlushStale();
	crlCacheRefresh(0,		// stale_days
					0,		// expire_overlap_seconds, 
					false,	// purge_all
					false,	// full_crypto_verify
					false);	// do Refresh
	Time::Interval nextFire = OCSPD_TIMER_INTERVAL;
	secdebug("ocspdRefresh", "OcspdTimer scheduling");
	mServer.setTimer(this, nextFire);
}

#pragma mark ----- OcspdServer, trivial subclass of MachPlusPlus::MachServer -----

OcspdServer::OcspdServer(const char *bootstrapName) 
	: MachServer(bootstrapName),
	  mAlloc(Allocator::standard()),
	  mTimer(*this)
{
	maxThreads(MAX_OCSPD_THREADS);
	
	/* schedule a refresh */
	Time::Interval nextFire = OCSPD_TIMER_FIRST;
	setTimer(&mTimer, nextFire);
}

OcspdServer::~OcspdServer()
{
}

/* the boundary between MachServer and MIG-oriented code */

boolean_t ocspd_server(mach_msg_header_t *, mach_msg_header_t *);

boolean_t OcspdServer::handle(mach_msg_header_t *in, mach_msg_header_t *out)
{
	ocspdDebug("OcspdServer::handle msg_id %d", (int)in->msgh_id);
	return ocspd_server(in, out);
}

