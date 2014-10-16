/*
 * Copyright (c) 2000,2002,2005-2006 Apple Computer, Inc. All Rights Reserved.
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
#include "ocspNetwork.h"
#include <security_ocspd/ocspdUtils.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#include <security_cdsa_utils/cuEnc64.h>
#include <stdlib.h>
#include <Security/cssmapple.h>
#include <LDAP/ldap.h>

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

/* fetch via HTTP POST */

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
	/* resources to release on exit */
	CFMutableDataRef inData = NULL;
	CFReadStreamRef cfStream = NULL;
    CFHTTPMessageRef request = NULL;
	CFDataRef postData = NULL;
	CFURLRef cfUrl = NULL;
	
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
				"%ld error %ld\n", (long)error.domain, (long)error.error);
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
	}

errOut:
	CFRELEASE(inData);
	CFRELEASE(cfStream);
    CFRELEASE(request);
	CFRELEASE(postData);
	CFRELEASE(cfUrl);
	return ourRtn;
}

