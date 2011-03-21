/*
 * Copyright (c) 2000-2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/* 
 * ocspdNetwork.cpp - Network support for ocspd and CRL/cert fetch
 */
#if OCSP_DEBUG
#define OCSP_USE_SYSLOG	1
#endif
#include <security_ocspd/ocspdDebug.h>
#include "ocspdNetwork.h"
#include <security_ocspd/ocspdUtils.h>
#include <CoreFoundation/CoreFoundation.h>
#include <security_cdsa_utils/cuEnc64.h>
#include <stdlib.h>
#include <Security/cssmapple.h>
#include <security_utilities/cfutilities.h>
#include <CoreServices/CoreServices.h>
#include <SystemConfiguration/SCDynamicStoreCopySpecific.h>

/* enable deprecated function declarations */
#ifndef LDAP_DEPRECATED
#define LDAP_DEPRECATED 1
#endif
#include <LDAP/ldap.h>

/* useful macros for CF */
#define CFReleaseSafe(CF) { CFTypeRef _cf = (CF); if (_cf) CFRelease(_cf); }
#define CFReleaseNull(CF) { CFTypeRef _cf = (CF); \
	if (_cf) { (CF) = NULL; CFRelease(_cf); } }

#pragma mark ----- async HTTP -----

/* how long to wait */
#define READ_STREAM_TIMEOUT	7.0

/* read buffer size */
#define READ_BUFFER_SIZE	4096

/* post buffer size */
#define POST_BUFFER_SIZE	1024

/* context for an asynchronous HTTP request */
typedef struct asynchttp_s {
	CFHTTPMessageRef request;
	CFHTTPMessageRef response;
	CFMutableDataRef data;
	CFIndex increment;
	size_t responseLength;
	CFReadStreamRef stream;
	CFRunLoopTimerRef timer;
	int finished;
} asynchttp_t;


static void asynchttp_complete(
	asynchttp_t *http)
{
    /* Shut down stream and timer. */
    if (http->stream) {
		CFReadStreamSetClient(http->stream, kCFStreamEventNone, NULL, NULL);
		CFReadStreamUnscheduleFromRunLoop(http->stream, CFRunLoopGetCurrent(), kCFRunLoopCommonModes);
		CFReadStreamClose(http->stream);
        CFReleaseNull(http->stream);
    }
    if (http->timer) {
        CFRunLoopTimerInvalidate(http->timer);
        CFReleaseNull(http->timer);
    }
	/* We're done. */
	http->finished = 1;
}

static void asynchttp_free(
	asynchttp_t *http)
{
    if(http == NULL) return;
	CFReleaseNull(http->request);
	CFReleaseNull(http->response);
	CFReleaseNull(http->data);
	CFReleaseNull(http->stream);
	CFReleaseNull(http->timer);
}

static void asynchttp_timer_proc(
	CFRunLoopTimerRef timer,
	void *info)
{
    asynchttp_t *http = (asynchttp_t *)info;
#if HTTP_DEBUG
    CFStringRef req_meth = http->request ? CFHTTPMessageCopyRequestMethod(http->request) : NULL;
    CFURLRef req_url = http->request ? CFHTTPMessageCopyRequestURL(http->request) : NULL;
    //%%% Add logging of url that timed out.
    //asl_log(NULL, NULL, ASL_LEVEL_NOTICE, "Timeout during %@ %@.", req_meth, req_url);
    if(req_url) CFRelease(req_url);
    if(req_meth) CFRelease(req_meth);
#endif
    asynchttp_complete(http);
}

static void handle_server_response(
	CFReadStreamRef stream,
    CFStreamEventType type,
	void *info)
{
    asynchttp_t *http = (asynchttp_t *)info;
    switch (type)
	{
		case kCFStreamEventHasBytesAvailable:
		{
			size_t len = (http->increment) ? http->increment : READ_BUFFER_SIZE;
			UInt8 *buf = (UInt8 *) malloc(len);
			if (!buf) {
				ocspdErrorLog("http stream: failed to malloc %ld bytes\n", len);
				asynchttp_complete(http);
				break;
			}
			if (!http->data) {
				http->data = CFDataCreateMutable(kCFAllocatorDefault, 0);
				http->responseLength = 0;
			}
			do {
				CFIndex bytesRead = CFReadStreamRead(stream, buf, len);
				if (bytesRead < 0) {
					/* Negative length == error */
					asynchttp_complete(http);
					break;
				} else if (bytesRead == 0) {
					/* Read 0 bytes, we're done */
					ocspdDebug("http stream: transfer complete, moved %ld bytes\n", 
						http->responseLength);
					asynchttp_complete(http);
					break;
				} else {
					/* Read some number of bytes */
					CFDataAppendBytes(http->data, buf, bytesRead);
					http->responseLength += bytesRead;
				}
			} while (CFReadStreamHasBytesAvailable(stream));
			free(buf);
			break;
		}
		case kCFStreamEventErrorOccurred:
		{
			CFStreamError error = CFReadStreamGetError(stream);
			ocspdErrorLog("http stream: %p kCFStreamEventErrorOccurred, domain: %ld error: %ld\n",
				stream, error.domain, error.error);
			if (error.domain == kCFStreamErrorDomainPOSIX) {
				ocspdErrorLog("CFReadStream POSIX error: %s\n", strerror(error.error));
			} else if (error.domain == kCFStreamErrorDomainMacOSStatus) {
				ocspdErrorLog("CFReadStream OSStatus error: %ld\n", (long int)error.error);
			} else {
				ocspdErrorLog("CFReadStream domain: %ld error: %ld\n", error.domain, (long int)error.error);
			}
			asynchttp_complete(http);
			break;
		}
		case kCFStreamEventEndEncountered:
		{
			http->response = (CFHTTPMessageRef)CFReadStreamCopyProperty(
				stream, kCFStreamPropertyHTTPResponseHeader);
			ocspdErrorLog("http stream: %p kCFStreamEventEndEncountered hdr: %p\n",
				stream, http->response);
			CFHTTPMessageSetBody(http->response, http->data);
			asynchttp_complete(http);
			break;
		}
		default:
		{
			ocspdErrorLog("handle_server_response: unexpected event type: %lu\n", type);
			break;
		}
    }
}


#pragma mark ----- OCSP support -----

/* POST method has Content-Type header line equal to "application/ocsp-request" */
static CFStringRef kContentType		= CFSTR("Content-Type");
static CFStringRef kAppOcspRequest	= CFSTR("application/ocsp-request");

#if OCSP_DEBUG
#define DUMP_BLOBS	1
#endif

#define OCSP_GET_FILE	"/tmp/ocspGet"
#define OCSP_RESP_FILE	"/tmp/ocspResp"

#if		DUMP_BLOBS

#include <security_cdsa_utils/cuFileIo.h>

static void writeBlob(
	const char *fileName,
	const char *whatIsIt,
	const unsigned char *data,
	unsigned dataLen)
{
	if(writeFile(fileName, data, dataLen)) {
		printf("***Error writing %s to %s\n", whatIsIt, fileName);
	}
	else {
		printf("...wrote %u bytes of %s to %s\n", dataLen, whatIsIt, fileName);
	}
}

#else

#define writeBlob(f,w,d,l)

#endif	/* DUMP_BLOBS */

#if		ENABLE_OCSP_VIA_GET

/* fetch via HTTP GET */
CSSM_RETURN ocspdHttpGet(
	SecAsn1CoderRef		coder, 
	const CSSM_DATA 	&url,
	const CSSM_DATA		&ocspReq,	// DER encoded
	CSSM_DATA			&fetched)	// mallocd in coder space and RETURNED
{
	ocspdHttpDebug("ocspdHttpGet: start\n");

	CSSM_RETURN result = CSSM_OK;
	CFDataRef urlData = NULL;
	SInt32 errorCode;
	unsigned char *endp = NULL;
	bool done = false;
	unsigned totalLen;
	unsigned char *fullUrl = NULL;
	CFURLRef cfUrl = NULL;
	Boolean brtn;
	CFIndex len;

	/* trim off possible NULL terminator from incoming URL */
	uint32 urlLen = url.Length;
	if(url.Data[urlLen - 1] == '\0') {
		urlLen--;
	}
	
	#if OCSP_DEBUG
	{
		char *ustr = (char *)malloc(urlLen + 1);
		memmove(ustr, url.Data, urlLen);
		ustr[urlLen] = '\0';
		ocspdDebug("ocspdHttpGet: fetching from URI %s\n", ustr);
		free(ustr);
	}
	#endif

	/* base64 encode the OCSP request; that's used as a path */
	unsigned char *req64 = NULL;
	unsigned req64Len = 0;
	req64 = cuEnc64(ocspReq.Data, ocspReq.Length, &req64Len);
	if(req64 == NULL) {
		ocspdErrorLog("ocspdHttpGet: error base64-encoding request\n");
		result = CSSMERR_TP_INTERNAL_ERROR;
		goto cleanup;
	}
	
	writeBlob(OCSP_GET_FILE, "OCSP Request as URL", req64, req64Len);
	
	/* trim off trailing NULL and newline */
	endp = req64 + req64Len - 1;
	for(;;) {
		switch(*endp) {
			case '\0':
			case '\n':
			case '\r':
				endp--;
				req64Len--;
				break;
			default:
				done = true;
				break;
		}
		if(done) {
			break;
		}
	}
	
	/* concatenate: URL plus path (see RFC 2616 3.2.2, 5.1.2) */
	totalLen = urlLen +	1 +	req64Len;
	fullUrl = (unsigned char *)malloc(totalLen);
	memmove(fullUrl, url.Data, urlLen);
	fullUrl[urlLen] = '/';
	memmove(fullUrl + urlLen + 1, req64, req64Len);
	
	cfUrl = CFURLCreateWithBytes(NULL,
		fullUrl, totalLen,
		kCFStringEncodingUTF8,		// right?
		NULL);						// this is absolute path 
	if(!cfUrl) {
		ocspdErrorLog("ocspdHttpGet: CFURLCreateWithBytes returned NULL\n");
		result = CSSMERR_APPLETP_CRL_BAD_URI;
		goto cleanup;
	}
	brtn = CFURLCreateDataAndPropertiesFromResource(NULL,
		cfUrl,
		&urlData, 
		NULL,			// no properties
		NULL,
		&errorCode);
	if(!brtn) {
		ocspdErrorLog("ocspdHttpGet: CFURLCreateDataAndPropertiesFromResource err: %d\n",
			(int)errorCode);
		result = CSSMERR_APPLETP_CRL_BAD_URI;
		goto cleanup;
	}
	if(urlData == NULL) {
		ocspdErrorLog("ocspdHttpGet: CFURLCreateDataAndPropertiesFromResource: no data\n");
		result = CSSMERR_APPLETP_CRL_BAD_URI;
		goto cleanup;
	}
	len = CFDataGetLength(urlData);
	fetched.Data = (uint8 *)SecAsn1Malloc(coder, len);
	fetched.Length = len;
	memmove(fetched.Data, CFDataGetBytePtr(urlData), len);
	writeBlob(OCSP_RESP_FILE, "OCSP Response", fetched.Data, fetched.Length);
cleanup:
	CFReleaseSafe(cfUrl);
	CFReleaseSafe(urlData);
	if(fullUrl) {
		free(fullUrl);
	}
	if(req64) {
		free(req64);
	}
	return result;
}

#endif		/* ENABLE_OCSP_VIA_GET */

/* fetch via HTTP POST */

CSSM_RETURN ocspdHttpPost(
	SecAsn1CoderRef		coder, 
	const CSSM_DATA 	&url,
	const CSSM_DATA		&ocspReq,	// DER encoded
	CSSM_DATA			&fetched)	// mallocd in coder space and RETURNED
{
	ocspdHttpDebug("ocspdHttpPost: start\n");

	CSSM_RETURN result = CSSM_OK;
	CFURLRef cfUrl = NULL;
	CFDictionaryRef proxyDict = NULL;
	CFStreamClientContext clientContext = { 0, NULL, NULL, NULL, NULL };
	CFRunLoopTimerContext timerContext = { 0, NULL, NULL, NULL, NULL };
	CFAbsoluteTime startTime, stopTime;
	asynchttp_t *httpContext = NULL;

	CFDataRef postData = NULL;

	/* trim off possible NULL terminator from incoming URL */
	uint32 urlLen = url.Length;
	if(url.Data[urlLen - 1] == '\0') {
		urlLen--;
	}

	/* create URL with explicit path (see RFC 2616 3.2.2, 5.1.2) */
	cfUrl = CFURLCreateWithBytes(NULL,
		url.Data, urlLen,
		kCFStringEncodingUTF8,		// right?
		NULL);						// this is absolute path 
	if(cfUrl) {
		CFStringRef pathStr = CFURLCopyLastPathComponent(cfUrl);
		if(pathStr) {
			if (CFStringGetLength(pathStr) == 0) {
				CFURLRef tmpUrl = CFURLCreateCopyAppendingPathComponent(NULL,
					cfUrl, CFSTR(""), FALSE);
				CFRelease(cfUrl);
				cfUrl = tmpUrl;
			}
			CFRelease(pathStr);
		}
	}
	if(!cfUrl) {
		ocspdErrorLog("ocspdHttpPost: CFURLCreateWithBytes returned NULL\n");
		result = CSSMERR_APPLETP_CRL_BAD_URI;
		goto cleanup;
	}
	
	#if OCSP_DEBUG
	{
		char *ustr = (char *)malloc(urlLen + 1);
		memmove(ustr, url.Data, urlLen);
		ustr[urlLen] = '\0';
		ocspdDebug("ocspdHttpPost: posting to URI %s\n", ustr);
		free(ustr);
	}
	#endif

	writeBlob(OCSP_GET_FILE, "OCSP Request as POST data", ocspReq.Data, ocspReq.Length);
	postData = CFDataCreate(NULL, ocspReq.Data, ocspReq.Length);

	/* allocate our http context */
	httpContext = (asynchttp_t *) malloc(sizeof(asynchttp_t));
	if(!httpContext) {
		result = memFullErr;
		goto cleanup;
	}
	memset(httpContext, 0, sizeof(asynchttp_t));

	/* read this many bytes at a time */
	httpContext->increment = POST_BUFFER_SIZE;

	/* create the http POST request */
	httpContext->request = CFHTTPMessageCreateRequest(kCFAllocatorDefault, 
		CFSTR("POST"), cfUrl, kCFHTTPVersion1_1);
	if(!httpContext->request) {
		ocspdErrorLog("ocspdHttpPost: error creating CFHTTPMessage\n");
		result = CSSMERR_TP_INTERNAL_ERROR;
		goto cleanup;
	}
    
	/* set the body and required header fields */
	CFHTTPMessageSetBody(httpContext->request, postData);
	CFHTTPMessageSetHeaderFieldValue(httpContext->request,
		kContentType, kAppOcspRequest);

	/* create the stream */
	httpContext->stream = CFReadStreamCreateForHTTPRequest(kCFAllocatorDefault, 
		httpContext->request);
	if(!httpContext->stream) {
		ocspdErrorLog("ocspdHttpPost: error creating CFReadStream\n");
		result = CSSMERR_TP_INTERNAL_ERROR;
		goto cleanup;
	}

	/* set up possible proxy info */
	proxyDict = SCDynamicStoreCopyProxies(NULL);
	if(proxyDict) {
		ocspdDebug("ocspdHttpPost: setting proxy dict\n");
		CFReadStreamSetProperty(httpContext->stream, kCFStreamPropertyHTTPProxy, proxyDict);
        CFReleaseNull(proxyDict);
	}
	
	/* set a reasonable timeout */
    timerContext.info = httpContext;
    httpContext->timer = CFRunLoopTimerCreate(kCFAllocatorDefault,
        CFAbsoluteTimeGetCurrent() + READ_STREAM_TIMEOUT,
        0, 0, 0, asynchttp_timer_proc, &timerContext);
    if (httpContext->timer == NULL) {
		ocspdErrorLog("ocspdHttpPost: error setting kCFStreamPropertyReadTimeout\n");
		/* but keep going */
    } else {
        CFRunLoopAddTimer(CFRunLoopGetCurrent(), httpContext->timer,
            kCFRunLoopDefaultMode);
    }

	/* set up our response callback and schedule it on the current run loop */
	httpContext->finished = 0;
	clientContext.info = httpContext;
    CFReadStreamSetClient(httpContext->stream,
        (kCFStreamEventHasBytesAvailable | kCFStreamEventErrorOccurred | kCFStreamEventEndEncountered),
        handle_server_response, &clientContext);
    CFReadStreamScheduleWithRunLoop(httpContext->stream, CFRunLoopGetCurrent(), kCFRunLoopCommonModes);

	/* open the stream */
	if(CFReadStreamOpen(httpContext->stream) == false) {
		ocspdErrorLog("ocspdHttpPost: error opening CFReadStream\n");
		result = CSSMERR_APPLETP_NETWORK_FAILURE;
		goto cleanup;
	}

	/* cycle the run loop until we get a response or time out */
	startTime = stopTime = CFAbsoluteTimeGetCurrent();
	while (!httpContext->finished) {
		(void)CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.1, TRUE);
		CFAbsoluteTime curTime = CFAbsoluteTimeGetCurrent();
		if (curTime != stopTime) {
			stopTime = curTime;
		}
	}

	if(!httpContext->data || httpContext->responseLength == 0) {
		ocspdErrorLog("CFReadStreamRead: no data was read after %f seconds\n",
			stopTime-startTime);
		result = CSSMERR_APPLETP_NETWORK_FAILURE;
		goto cleanup;
	}
	ocspdDebug("ocspdHttpPost: total %lu bytes read in %f seconds\n",
		(unsigned long)httpContext->responseLength, stopTime-startTime);
	SecAsn1AllocCopy(coder, CFDataGetBytePtr(httpContext->data),
		CFDataGetLength(httpContext->data), &fetched);
	writeBlob(OCSP_RESP_FILE, "OCSP Response", fetched.Data, fetched.Length);
	result = CSSM_OK;
cleanup:
	CFReleaseSafe(postData);
	CFReleaseSafe(cfUrl);
	asynchttp_free(httpContext);
	
	return result;
}

#pragma mark ----- LDAP fetch -----

/*
 * LDAP attribute names, used if not present in URI.
 */
#define LDAP_ATTR_CERT		"cacertificate;binary"
#define LDAP_ATTR_CRL		"certificaterevocationlist;binary"

/*
 * Default LDAP options.
 */
#define LDAP_REFERRAL_DEFAULT	LDAP_OPT_ON

static CSSM_RETURN ldapRtnToCssm(
	int rtn)
{
	switch(rtn) {
		case LDAP_SERVER_DOWN:
		case LDAP_TIMEOUT:
		case LDAP_CONNECT_ERROR:
			return CSSMERR_APPLETP_CRL_SERVER_DOWN;
		case LDAP_PARAM_ERROR:
		case LDAP_FILTER_ERROR:
			return CSSMERR_APPLETP_CRL_BAD_URI;
		default:
			return CSSMERR_APPLETP_CRL_NOT_FOUND;
	}
}

static CSSM_RETURN ldapFetch(
	Allocator			&alloc, 
	const CSSM_DATA 	&url,
	LF_Type				lfType,
	CSSM_DATA			&fetched)	// mallocd in alloc space and RETURNED
{
	BerValue 		**value = NULL;
	LDAPURLDesc 	*urlDesc = NULL;
	int 			rtn;
	LDAPMessage 	*msg = NULL;
	LDAP 			*ldap = NULL;
	LDAPMessage 	*entry = NULL;
	bool 			mallocdString = false;
	char 			*urlStr;
	int 			numEntries;
	CSSM_RETURN 	ourRtn = CSSM_OK;
	/* attr input to ldap_search_s() */
	char			*attrArray[2];
	char			**attrArrayP = NULL;
	
	/* don't assume URL string is NULL terminated */
	if(url.Data[url.Length - 1] == '\0') {
		urlStr = (char *)url.Data;
	}
	else {
		urlStr = (char *)malloc(url.Length + 1);
		memmove(urlStr, url.Data, url.Length);
		urlStr[url.Length] = '\0';
		mallocdString = true;
	}
	
	/* break up the URL into something usable */
	rtn = ldap_url_parse(urlStr, &urlDesc);
	if(rtn) {
		ocspdErrorLog("ldap_url_parse returned %d", rtn);
		return CSSMERR_APPLETP_CRL_BAD_URI;
	}
	
	/*
	 * Determine what attr we're looking for.
	 */
	if((urlDesc->lud_attrs != NULL) &&		// attrs present in URL
	   (urlDesc->lud_attrs[0] != NULL) &&	// at least one attr present
	   (urlDesc->lud_attrs[1] == NULL))	{
		/*
		 * Exactly one attr present in the caller-specified URL;
		 * assume that this is exactly what we want. 
		 */
		attrArrayP = &urlDesc->lud_attrs[0];
	}
	else {
		/* use caller-specified attr */
		switch(lfType) {
			case LT_Crl:
				attrArray[0] = (char *)LDAP_ATTR_CRL;
				break;
			case LT_Cert:
				attrArray[0] = (char *)LDAP_ATTR_CERT;
				break;
			default:
				printf("***ldapFetch screwup: bogus lfType (%d)\n",
					(int)lfType);
				return CSSMERR_CSSM_INTERNAL_ERROR;
		}
		attrArray[1] = NULL;
		attrArrayP = &attrArray[0];
	}
	
	/* establish connection */
	rtn = ldap_initialize(&ldap, urlStr);
	if(rtn) {
		ocspdErrorLog("ldap_initialize returned %d\n", rtn);
		return ldapRtnToCssm(rtn);
	}
	/* subsequent errors to cleanup: */
	rtn = ldap_simple_bind_s(ldap, NULL, NULL);
	if(rtn) {
		ocspdErrorLog("ldap_simple_bind_s returned %d\n", rtn);
		ourRtn = ldapRtnToCssm(rtn);
		goto cleanup;
	}
	
	rtn = ldap_set_option(ldap, LDAP_OPT_REFERRALS, LDAP_REFERRAL_DEFAULT);
	if(rtn) {
		ocspdErrorLog("ldap_set_option(referrals) returned %d\n", rtn);
		ourRtn = ldapRtnToCssm(rtn);
		goto cleanup;
	}
	
	rtn = ldap_search_s(
		ldap, 
		urlDesc->lud_dn, 
		LDAP_SCOPE_SUBTREE,
		urlDesc->lud_filter, 
		urlDesc->lud_attrs, 
		0, 			// attrsonly
		&msg);
	if(rtn) {
		ocspdErrorLog("ldap_search_s returned %d\n", rtn);
		ourRtn = ldapRtnToCssm(rtn);
		goto cleanup;
	}

	/* 
	 * We require exactly one entry (for now).
	 */
	numEntries = ldap_count_entries(ldap, msg);
	if(numEntries != 1) {
		ocspdErrorLog("tpCrlViaLdap: numEntries %d\n", numEntries);
		ourRtn = CSSMERR_APPLETP_CRL_NOT_FOUND;
		goto cleanup;
	}
	
	entry = ldap_first_entry(ldap, msg);
	value = ldap_get_values_len(ldap, msg, attrArrayP[0]);
	if(value == NULL) {
		ocspdErrorLog("Error on ldap_get_values_len\n");
		ourRtn = CSSMERR_APPLETP_CRL_NOT_FOUND;
		goto cleanup;
	}
	
	fetched.Length = value[0]->bv_len;
	fetched.Data = (uint8 *)alloc.malloc(fetched.Length);
	memmove(fetched.Data, value[0]->bv_val, fetched.Length);

	ldap_value_free_len(value);
	ourRtn = CSSM_OK;
cleanup:
	if(msg) {
		ldap_msgfree(msg);
	}
	if(mallocdString) {
		free(urlStr);
	}
	ldap_free_urldesc(urlDesc);
	rtn = ldap_unbind(ldap);
	if(rtn) {
		ocspdErrorLog("Error %d on ldap_unbind\n", rtn);
		/* oh well */
	}
	return ourRtn;
}

#pragma mark ----- HTTP fetch via GET -----

/* fetch via HTTP */
static CSSM_RETURN httpFetch(
	Allocator			&alloc, 
	const CSSM_DATA 	&url,
	CSSM_DATA			&fetched)	// mallocd in alloc space and RETURNED
{
	ocspdHttpDebug("httpFetch: start\n");

	CSSM_RETURN result = CSSM_OK;
	CFURLRef cfUrl = NULL;
	CFDictionaryRef proxyDict = NULL;
	CFStreamClientContext clientContext = { 0, NULL, NULL, NULL, NULL };
	CFRunLoopTimerContext timerContext = { 0, NULL, NULL, NULL, NULL };
	CFAbsoluteTime startTime, stopTime;
	asynchttp_t *httpContext = NULL;

	/* trim off possible NULL terminator from incoming URL */
	CSSM_DATA theUrl = url;
	if(theUrl.Data[theUrl.Length - 1] == '\0') {
		theUrl.Length--;
	}

	/* create URL with explicit path (see RFC 2616 3.2.2, 5.1.2) */
	cfUrl = CFURLCreateWithBytes(NULL,
		theUrl.Data, theUrl.Length,
		kCFStringEncodingUTF8,		// right?
		NULL);						// this is absolute path 
	if(cfUrl) {
		CFStringRef pathStr = CFURLCopyLastPathComponent(cfUrl);
		if(pathStr) {
			if (CFStringGetLength(pathStr) == 0) {
				CFURLRef tmpUrl = CFURLCreateCopyAppendingPathComponent(NULL,
					cfUrl, CFSTR(""), FALSE);
				CFRelease(cfUrl);
				cfUrl = tmpUrl;
			}
			CFRelease(pathStr);
		}
	}
	if(!cfUrl) {
		ocspdErrorLog("httpFetch: CFURLCreateWithBytes returned NULL\n");
		result = CSSMERR_APPLETP_CRL_BAD_URI;
		goto cleanup;
	}

	#if OCSP_DEBUG
	{
		char *ustr = (char *)malloc(theUrl.Length + 1);
		memmove(ustr, theUrl.Data, theUrl.Length);
		ustr[theUrl.Length] = '\0';
		ocspdDebug("httpFetch: GET URI %s\n", ustr);
		free(ustr);
	}
	#endif

	/* allocate our http context */
	httpContext = (asynchttp_t *) malloc(sizeof(asynchttp_t));
	if(!httpContext) {
		result = memFullErr;
		goto cleanup;
	}
	memset(httpContext, 0, sizeof(asynchttp_t));

	/* read this many bytes at a time */
	httpContext->increment = READ_BUFFER_SIZE;

	/* create the http GET request */
	httpContext->request = CFHTTPMessageCreateRequest(NULL, 
		CFSTR("GET"), cfUrl, kCFHTTPVersion1_1);
	if(!httpContext->request) {
		ocspdErrorLog("httpFetch: error creating CFHTTPMessage\n");
		result = CSSMERR_TP_INTERNAL_ERROR;
		goto cleanup;
	}
	
	/* create the stream */
	httpContext->stream = CFReadStreamCreateForHTTPRequest(NULL, 
		httpContext->request);
	if(!httpContext->stream) {
		ocspdErrorLog("httpFetch: error creating CFReadStream\n");
		result = CSSMERR_TP_INTERNAL_ERROR;
		goto cleanup;
	}

	/* set up possible proxy info */
	proxyDict = SCDynamicStoreCopyProxies(NULL);
	if(proxyDict) {
		ocspdDebug("httpFetch: setting proxy dict\n");
		CFReadStreamSetProperty(httpContext->stream, kCFStreamPropertyHTTPProxy, proxyDict);
        CFReleaseNull(proxyDict);
	}

	/* set a reasonable timeout */
    timerContext.info = httpContext;
    httpContext->timer = CFRunLoopTimerCreate(kCFAllocatorDefault,
        CFAbsoluteTimeGetCurrent() + READ_STREAM_TIMEOUT,
        0, 0, 0, asynchttp_timer_proc, &timerContext);
    if (httpContext->timer == NULL) {
        ocspdErrorLog("httpFetch: error creating timer\n");
		/* oh well - keep going */
    } else {
        CFRunLoopAddTimer(CFRunLoopGetCurrent(), httpContext->timer,
            kCFRunLoopDefaultMode);
    }

	/* set up our response callback and schedule it on the current run loop */
	httpContext->finished = 0;
	clientContext.info = httpContext;
    CFReadStreamSetClient(httpContext->stream,
        (kCFStreamEventHasBytesAvailable | kCFStreamEventErrorOccurred | kCFStreamEventEndEncountered),
        handle_server_response, &clientContext);
    CFReadStreamScheduleWithRunLoop(httpContext->stream, CFRunLoopGetCurrent(), kCFRunLoopCommonModes);

	/* open the stream */
	if(CFReadStreamOpen(httpContext->stream) == false) {
		ocspdErrorLog("httpFetch: error opening CFReadStream\n");
		result = CSSMERR_APPLETP_NETWORK_FAILURE;
		goto cleanup;
	}

	/* cycle the run loop until we get a response or time out */
	startTime = stopTime = CFAbsoluteTimeGetCurrent();
	while (!httpContext->finished) {
		(void)CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.1, TRUE);
		CFAbsoluteTime curTime = CFAbsoluteTimeGetCurrent();
		if (curTime != stopTime) {
			stopTime = curTime;
		}
	}

	if(httpContext->responseLength == 0) {
		ocspdErrorLog("httpFetch: CFReadStreamRead: no data was read after %f seconds\n",
			stopTime-startTime);
		result = CSSMERR_APPLETP_NETWORK_FAILURE;
		goto cleanup;
	}
	ocspdDebug("httpFetch: total %lu bytes read in %f seconds\n",
		(unsigned long)httpContext->responseLength, stopTime-startTime);
	if(httpContext->data) {
		CFIndex len = CFDataGetLength(httpContext->data);
		fetched.Data = (uint8 *)alloc.malloc(len);	
		fetched.Length = len;
		memmove(fetched.Data, CFDataGetBytePtr(httpContext->data), len);
	} else {
		result = CSSMERR_TP_INTERNAL_ERROR;
	}
cleanup:
	CFReleaseSafe(cfUrl);
	asynchttp_free(httpContext);

	return result;
}

/* Fetch cert or CRL from net, we figure out the schema */
CSSM_RETURN ocspdNetFetch(
	Allocator			&alloc, 
	const CSSM_DATA 	&url,
	LF_Type				lfType,
	CSSM_DATA			&fetched)	// mallocd in alloc space and RETURNED
{
	#if OCSP_DEBUG
	{
		char *ustr = (char *)malloc(url.Length + 1);
		memmove(ustr, url.Data, url.Length);
		ustr[url.Length] = '\0';
		ocspdDebug("ocspdNetFetch: fetching from URI %s\n", ustr);
		free(ustr);
	}
	#endif

	if(url.Length < 5) {
		return CSSMERR_APPLETP_CRL_BAD_URI;
	}
	if(!strncmp((char *)url.Data, "ldap:", 5)) {
		return ldapFetch(alloc, url, lfType, fetched);
	}
	if(!strncmp((char *)url.Data, "http:", 5) ||
	   !strncmp((char *)url.Data, "https:", 6)) {	
		return httpFetch(alloc, url, fetched);
	}
	return CSSMERR_APPLETP_CRL_BAD_URI;
}


