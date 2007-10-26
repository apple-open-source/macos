/*
 * Copyright (c) 2002,2000 Apple Computer, Inc. All rights reserved.
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

#define ocspdHttpDebug(args...)     secdebug("ocspdHttp", ## args)

#pragma mark ----- OCSP support -----

/* POST method has Content-Type header line equal to "application/ocsp-request" */
static CFStringRef kContentType		= CFSTR("Content-Type");
static CFStringRef kAppOcspRequest	= CFSTR("application/ocsp-request");

#ifndef	NDEBUG
#define DUMP_BLOBS	0
#else
#define DUMP_BLOBS	0
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
	CSSM_RETURN ourRtn = CSSM_OK;
	CFDataRef urlData = NULL;
	SInt32 errorCode;
	unsigned char *endp = NULL;
	bool done = false;
	unsigned totalLen;
	unsigned char *fullUrl = NULL;
	CFURLRef cfUrl = NULL;
	Boolean brtn;
	CFIndex len;
	
	/* trim off possible NULL terminator */
	uint32 urlLen = url.Length;
	if(url.Data[urlLen - 1] == '\0') {
		urlLen--;
	}
	
	#ifndef	NDEBUG
	{
		char *ustr = (char *)malloc(urlLen + 1);
		memmove(ustr, url.Data, urlLen);
		ustr[urlLen] = '\0';
		ocspdDebug("ocspdHttpGet: fetching from URI %s", ustr);
		free(ustr);
	}
	#endif

	/* base64 encode the OCSP request; that's used as a path */
	unsigned char *req64 = NULL;
	unsigned req64Len = 0;
	req64 = cuEnc64(ocspReq.Data, ocspReq.Length, &req64Len);
	if(req64 == NULL) {
		ocspdErrorLog("httpFetch: error base64-encoding request\n");
		return CSSMERR_TP_INTERNAL_ERROR;
	}
	/* subsequent errors to errOut: */
	
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
	
	/* concatenate: URL plus path */
	totalLen = urlLen +	1 +	req64Len;
	fullUrl = (unsigned char *)malloc(totalLen);
	memmove(fullUrl, url.Data, urlLen);
	fullUrl[urlLen] = '/';
	memmove(fullUrl + urlLen + 1, req64, req64Len);
	
	cfUrl = CFURLCreateWithBytes(NULL,
		fullUrl, totalLen,
		kCFStringEncodingUTF8,		// right?
		NULL);						// this is absolute path 
	if(cfUrl == NULL) {
		ocspdErrorLog("CFURLCreateWithBytes returned NULL\n");
		/* FIXME..? */
		ourRtn = CSSMERR_APPLETP_CRL_BAD_URI;
		goto errOut;
	}
	brtn = CFURLCreateDataAndPropertiesFromResource(NULL,
		cfUrl,
		&urlData, 
		NULL,			// no properties
		NULL,
		&errorCode);
	CFRelease(cfUrl);
	if(!brtn) {
		ocspdErrorLog("CFURLCreateDataAndPropertiesFromResource err: %d\n",
			(int)errorCode);
		ourRtn = CSSMERR_APPLETP_CRL_BAD_URI;
		goto errOut;
	}
	if(urlData == NULL) {
		ocspdErrorLog("CFURLCreateDataAndPropertiesFromResource: no data\n");
		ourRtn = CSSMERR_APPLETP_CRL_BAD_URI;
		goto errOut;
	}
	len = CFDataGetLength(urlData);
	fetched.Data = (uint8 *)SecAsn1Malloc(coder, len);
	fetched.Length = len;
	memmove(fetched.Data, CFDataGetBytePtr(urlData), len);
	writeBlob(OCSP_RESP_FILE, "OCSP Response", fetched.Data, fetched.Length);
errOut:
	if(urlData) {
		CFRelease(urlData);
	}
	if(fullUrl) {
		free(fullUrl);
	}
	if(req64) {
		free(req64);
	}
	return CSSM_OK;
}

#endif		/* ENABLE_OCSP_VIA_GET */

/* fetch via HTTP POST */

/* SPI to specify timeout on CFReadStream */
#define _kCFStreamPropertyReadTimeout   CFSTR("_kCFStreamPropertyReadTimeout")

/* the timeout we set */
#define READ_STREAM_TIMEOUT		15

#define POST_BUFSIZE	1024

CSSM_RETURN ocspdHttpPost(
	SecAsn1CoderRef		coder, 
	const CSSM_DATA 	&url,
	const CSSM_DATA		&ocspReq,	// DER encoded
	CSSM_DATA			&fetched)	// mallocd in coder space and RETURNED
{
	CSSM_RETURN ourRtn = CSSM_OK;
	CFIndex thisMove;
	UInt8 inBuf[POST_BUFSIZE];
	SInt32 ito;
	/* resources to release on exit */
	CFMutableDataRef inData = NULL;
	CFReadStreamRef cfStream = NULL;
    CFHTTPMessageRef request = NULL;
	CFDataRef postData = NULL;
	CFURLRef cfUrl = NULL;
	CFNumberRef cfnTo = NULL;
	CFRef<CFDictionaryRef> proxyDict;
	
	ocspdHttpDebug("ocspdHttpPost top");

	/* trim off possible NULL terminator from incoming URL */
	uint32 urlLen = url.Length;
	if(url.Data[urlLen - 1] == '\0') {
		urlLen--;
	}
	
	cfUrl = CFURLCreateWithBytes(NULL,
		url.Data, urlLen,
		kCFStringEncodingUTF8,		// right?
		NULL);						// this is absolute path 
	if(cfUrl == NULL) {
		ocspdErrorLog("CFURLCreateWithBytes returned NULL\n");
		/* FIXME..? */
		return CSSMERR_APPLETP_CRL_BAD_URI;
	}
	/* subsequent errors to errOut: */
	
	#ifndef	NDEBUG
	{
		char *ustr = (char *)malloc(urlLen + 1);
		memmove(ustr, url.Data, urlLen);
		ustr[urlLen] = '\0';
		ocspdDebug("ocspdHttpPost: posting to URI %s", ustr);
		free(ustr);
	}
	#endif

	writeBlob(OCSP_GET_FILE, "OCSP Request as POST data", ocspReq.Data, ocspReq.Length);
	postData = CFDataCreate(NULL, ocspReq.Data, ocspReq.Length);
	
    /* Create a new HTTP request. */
    request = CFHTTPMessageCreateRequest(kCFAllocatorDefault, CFSTR("POST"), cfUrl,
		kCFHTTPVersion1_1);
    if (request == NULL) {
		ocspdErrorLog("ocspdHttpPost: error creating CFHTTPMessage\n");
		ourRtn = CSSMERR_TP_INTERNAL_ERROR;
		goto errOut;
    }
    
	// Set the body and required header fields.
	CFHTTPMessageSetBody(request, postData);
	CFHTTPMessageSetHeaderFieldValue(request, kContentType, kAppOcspRequest);
	
    // Create the stream for the request.
    cfStream = CFReadStreamCreateForHTTPRequest(kCFAllocatorDefault, request);
    if (cfStream == NULL) {
		ocspdErrorLog("ocspdHttpPost: error creating CFReadStream\n");
		ourRtn = CSSMERR_TP_INTERNAL_ERROR;
		goto errOut;
    }
	
	/* SUTiDenver: set a reasonable timeout */
	ito = READ_STREAM_TIMEOUT;
	cfnTo = CFNumberCreate(NULL, kCFNumberSInt32Type, &ito);
    if(!CFReadStreamSetProperty(cfStream, _kCFStreamPropertyReadTimeout, cfnTo)) {
		ocspdErrorLog("ocspdHttpPost: error setting _kCFStreamPropertyReadTimeout\n");
		/* but keep going */
	}

	/* set up possible proxy info */
	proxyDict.take(SCDynamicStoreCopyProxies(NULL));
	if(proxyDict) {
		ocspdHttpDebug("ocspdHttpPost setting proxy dict");
		CFReadStreamSetProperty(cfStream, kCFStreamPropertyHTTPProxy, proxyDict);
	}
	
	/* go, synchronously */
	if(!CFReadStreamOpen(cfStream)) {
		ocspdErrorLog("ocspdHttpPost: error opening CFReadStream\n");
		ourRtn = CSSMERR_TP_INTERNAL_ERROR;
		goto errOut;
	}
	inData = CFDataCreateMutable(NULL, 0);
	for(;;) {
		thisMove = CFReadStreamRead(cfStream, inBuf, POST_BUFSIZE);
		if(thisMove < 0) {
			CFStreamError error = CFReadStreamGetError(cfStream);
			ocspdErrorLog("ocspdHttpPost: error on CFReadStreamRead: domain "
				"%d error %ld\n", (int)error.domain, (long)error.error);
			ourRtn = CSSMERR_APPLETP_NETWORK_FAILURE;
			break;
		}
		else if(thisMove == 0) {
			ocspdDebug("ocspdHttpPost: transfer complete, moved %ld bytes", 
				CFDataGetLength(inData));
			ourRtn = CSSM_OK;
			break;
		}
		else {
			CFDataAppendBytes(inData, inBuf, thisMove);
		}
	}
	if(ourRtn == CSSM_OK) {
		SecAsn1AllocCopy(coder, CFDataGetBytePtr(inData), CFDataGetLength(inData),
			&fetched);
		writeBlob(OCSP_RESP_FILE, "OCSP Response", fetched.Data, fetched.Length);
		ocspdHttpDebug("ocspdHttpPost fetched %lu bytes", (unsigned long)fetched.Length);
	}

errOut:
	CFRELEASE(inData);
	CFRELEASE(cfStream);
    CFRELEASE(request);
	CFRELEASE(postData);
	CFRELEASE(cfUrl);
	CFRELEASE(cfnTo);
	return ourRtn;
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

#define kResponseIncrement  4096

/* fetch via HTTP */
static CSSM_RETURN httpFetch(
	Allocator			&alloc, 
	const CSSM_DATA 	&url,
	CSSM_DATA			&fetched)	// mallocd in alloc space and RETURNED
{
	ocspdHttpDebug("httpFetch top");
	
	/* trim off possible NULL terminator */
	CSSM_DATA theUrl = url;
	if(theUrl.Data[theUrl.Length - 1] == '\0') {
		theUrl.Length--;
	}
	CFRef<CFURLRef> cfUrl(CFURLCreateWithBytes(NULL,
		theUrl.Data, theUrl.Length,
		kCFStringEncodingUTF8,		// right?
		NULL));						// this is absolute path 
	if(!cfUrl) {
		ocspdErrorLog("CFURLCreateWithBytes returned NULL\n");
		return CSSMERR_APPLETP_CRL_BAD_URI;
	}
	
	CFRef<CFHTTPMessageRef> httpRequestRef(CFHTTPMessageCreateRequest(NULL, 
		CFSTR("GET"), cfUrl, kCFHTTPVersion1_1));
	if(!httpRequestRef) {
		return CSSMERR_APPLETP_NETWORK_FAILURE;
	}
	
	// open the stream
	CFRef<CFReadStreamRef> httpStreamRef(CFReadStreamCreateForHTTPRequest(NULL, 
		httpRequestRef));
	if(!httpStreamRef) {
		return CSSMERR_APPLETP_NETWORK_FAILURE;
	}

	// set a reasonable timeout
	SInt32 ito = READ_STREAM_TIMEOUT;
	CFRef<CFNumberRef> cfnTo(CFNumberCreate(NULL, kCFNumberSInt32Type, &ito));
    if(!CFReadStreamSetProperty(httpStreamRef, _kCFStreamPropertyReadTimeout, cfnTo)) {
		// oh well - keep going 
	}
	
	/* set up possible proxy info */
	CFRef<CFDictionaryRef> proxyDict(SCDynamicStoreCopyProxies(NULL));
	if(proxyDict) {
		ocspdHttpDebug("httpFetch setting proxy dict");
		CFReadStreamSetProperty(httpStreamRef, kCFStreamPropertyHTTPProxy, proxyDict);
	}

	if(CFReadStreamOpen(httpStreamRef) == false) {
		return CSSMERR_APPLETP_NETWORK_FAILURE;
	}
	
	fetched.Data = (uint8 *)alloc.malloc(kResponseIncrement);	
	size_t responseBufferLength = kResponseIncrement;
	size_t responseLength = 0;

	ocspdHttpDebug("httpFetch starting read");
	
	// read data from the stream
	CFIndex bytesRead = CFReadStreamRead (httpStreamRef, (UInt8*)fetched.Data, 
		kResponseIncrement);
	while (bytesRead > 0) {
		responseLength += bytesRead;
		responseBufferLength = responseLength + kResponseIncrement;
		fetched.Data = (uint8 *)alloc.realloc (fetched.Data, responseBufferLength);
		bytesRead = CFReadStreamRead(httpStreamRef, (UInt8*) fetched.Data + responseLength, 
			kResponseIncrement);
	}
	
	if (bytesRead < 0) {
		return CSSMERR_APPLETP_NETWORK_FAILURE;
	}

	if(responseLength == 0) {
		ocspdErrorLog("CFReadStreamRead: no data\n");
		return CSSMERR_APPLETP_NETWORK_FAILURE;
	}
	fetched.Length = responseLength;
	ocspdHttpDebug("httpFetch total %lu bytes read", (unsigned long)responseLength);
	return CSSM_OK;
}

/* Fetch cert or CRL from net, we figure out the schema */
CSSM_RETURN ocspdNetFetch(
	Allocator			&alloc, 
	const CSSM_DATA 	&url,
	LF_Type				lfType,
	CSSM_DATA			&fetched)	// mallocd in alloc space and RETURNED
{
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


