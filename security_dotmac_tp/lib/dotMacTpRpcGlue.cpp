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
 * dotMacTpRpcGlue.cpp - glue between CDSA and XMLRPC for .mac TP
 */
 
#include "dotMacTpRpcGlue.h"
#include "dotMacTpUtils.h"
#include "dotMacTpDebug.h"
#include "dotMacTpMutils.h"
#include <stdint.h>
#include <CoreFoundation/CoreFoundation.h>
#include "dotMacXmlRpc.h"
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>
#include <Security/cssmapple.h>

/* Dump XMLRPC Result dictionary */
#if		DICTIONARY_DEBUG	
#define RESULTS_DICTIONARY_DEBUG		1
#define REQUEST_DICTIONARY_DEBUG		1
#else
#define RESULTS_DICTIONARY_DEBUG		0
#define REQUEST_DICTIONARY_DEBUG		0
#endif

/* 
 * Force/simulate SuccessQueued errors to test this module before the server can 
 * actually generate those errors.
 */
#ifndef NDEBUG
#define FORCE_SUCCESS_QUEUED			0
#else
#define FORCE_SUCCESS_QUEUED			0
#endif
#if		FORCE_SUCCESS_QUEUED
/* actual behavior is tweakable by debugger */
int forceQueued = 1;
#endif  /* FORCE_SUCCESS_QUEUED */

/*
 * Force SuccessQueued even if server returns a really bad error
 */
#ifndef NDEBUG
#define FORCE_SUCCESS_QUEUED_ALWAYS		0
#else
#define FORCE_SUCCESS_QUEUED			0
#endif

#define CFRELEASE(cf)		\
	if(cf != NULL) {		\
		CFRelease(cf);		\
	}

/*
 * Constant strings for Pinstripe XMLRPC API
 */
 
/* 
 * method names 
 */
static CFStringRef kMethSignIdentity		= CFSTR("ichat.signingRequest");
static CFStringRef kMethSignEmail			= CFSTR("sign.email");
static CFStringRef kMethSignEmailEncrypt	= CFSTR("sign.emailencrypt");
static CFStringRef kMethArchiveList			= CFSTR("archive.list");
static CFStringRef kMethArchiveStore		= CFSTR("archive.store");
static CFStringRef kMethArchiveFetch		= CFSTR("archive.fetch");
static CFStringRef kMethArchiveRemove		= CFSTR("archive.remove");
static CFStringRef kMethSigningStatus		= CFSTR("ichat.signingStatus");

/* 
 * names of values in an XMLRPC response 
 */
static CFStringRef kResponseResultCode		= CFSTR("resultCode");
static CFStringRef kResponseTimestamp		= CFSTR("timestamp");
static CFStringRef kResponseResultInfo		= CFSTR("resultInfo");
static CFStringRef kResponseResultBody		= CFSTR("resultBody");

/* 
 * names of values in an archive.list dictionary 
 */
static CFStringRef kArchiveListName			= CFSTR("name");
static CFStringRef kArchiveListExpires		= CFSTR("expires");

/* 
 * resultCode strings 
 */
/* OK, resultBody contains the cert */
static CFStringRef kResultSuccess				= CFSTR("Success");
/* OK, resultBody contains seconds to availability of cert */
static CFStringRef kResultQueued				= CFSTR("SuccessQueued");
/* not really OK, caller must visit URL specified in resultBody */
static CFStringRef kResultRedirected			= CFSTR("SuccessRedirected");
static CFStringRef kResultFailed				= CFSTR("Failed");
static CFStringRef kResultAlreadyExists			= CFSTR("FailedAlreadyExists"); //%%% appears to be deprecated now
static CFStringRef kResultCertAlreadyExists		= CFSTR("FailedCertAlreadyExists");
static CFStringRef kResultServiceError			= CFSTR("FailedServiceError");
static CFStringRef kResultParameterError		= CFSTR("FailedParameterError");
static CFStringRef kResultNotAllowed			= CFSTR("FailedNotAllowed");
static CFStringRef kResultNotImplemented		= CFSTR("NotImplemented");
static CFStringRef kResultNotAuthorized			= CFSTR("NotAuthorized");
static CFStringRef kResultNotAvailable			= CFSTR("NotAvailable");
static CFStringRef kResultNotSupportForAccount  = CFSTR("FailedNotSupportedForAccount");
static CFStringRef kResultFailedNotFound		= CFSTR("FailedNotFound");
static CFStringRef kResultPendingCSR			= CFSTR("FailedPendingCSR");
static CFStringRef kResultNoExistingCSR			= CFSTR("FailedNoExistingCSR");


/* quickie parameter names which don't go over the wire, just for ordering */
static CFStringRef kP1 = CFSTR("p1");
static CFStringRef kP2 = CFSTR("p2");
static CFStringRef kP3 = CFSTR("p3");
static CFStringRef kP4 = CFSTR("p4");

/*
 * Convert an XMLRPC resultCode string to an OSStatus/CSSM_RETURN.
 */
static OSStatus dotMacParseResult(
	CFStringRef resultCode)
{
	if(CFEqual(resultCode, kResultSuccess)) {
		#if		FORCE_SUCCESS_QUEUED
		if(forceQueued) {
			printf("...Forcing REQ_QUEUED status\n");
			return CSSMERR_APPLE_DOTMAC_REQ_QUEUED;
		}
		#endif  /* FORCE_SUCCESS_QUEUED */
		return noErr;
	}
	else if(CFEqual(resultCode, kResultQueued)) {
		return CSSMERR_APPLE_DOTMAC_REQ_QUEUED;
	}
	else if(CFEqual(resultCode, kResultRedirected)) {
		return CSSMERR_APPLE_DOTMAC_REQ_REDIRECT;
	}
	else if(CFEqual(resultCode, kResultFailed)) {
		return CSSMERR_APPLE_DOTMAC_REQ_SERVER_ERR;
	}
	else if(CFEqual(resultCode, kResultAlreadyExists) ||
			CFEqual(resultCode, kResultCertAlreadyExists)) {
		return CSSMERR_APPLE_DOTMAC_REQ_SERVER_ALREADY_EXIST;
	}
	else if(CFEqual(resultCode, kResultServiceError)) {
		return CSSMERR_APPLE_DOTMAC_REQ_SERVER_SERVICE_ERROR;
	}
	else if(CFEqual(resultCode, kResultParameterError)) {
		return CSSMERR_APPLE_DOTMAC_REQ_SERVER_PARAM;
	}
	else if(CFEqual(resultCode, kResultNotAllowed)) {
		return CSSMERR_APPLE_DOTMAC_REQ_SERVER_AUTH;
	}
	else if(CFEqual(resultCode, kResultNotImplemented)) {
		return CSSMERR_APPLE_DOTMAC_REQ_SERVER_UNIMPL;
	}
	else if(CFEqual(resultCode, kResultNotAuthorized)) {
		return CSSMERR_APPLE_DOTMAC_REQ_SERVER_AUTH;
	}
	else if(CFEqual(resultCode, kResultNotAvailable)) {
		return CSSMERR_APPLE_DOTMAC_REQ_SERVER_NOT_AVAIL;
	}
	else if(CFEqual(resultCode, kResultNotSupportForAccount)) {
		/* FIXME might want a .mac TP-specific error for this */
		return CSSMERR_TP_REQUEST_REJECTED;
	}
	else if(CFEqual(resultCode, kResultPendingCSR)) {
		return CSSMERR_APPLE_DOTMAC_REQ_IS_PENDING;
	}
	else if(CFEqual(resultCode, kResultNoExistingCSR)) {
		return CSSMERR_APPLE_DOTMAC_NO_REQ_PENDING;
	}
	
	else {
		dotMacErrorLog("dotMacParseResult: unknown error\n");
		return ioErr;
	}
}

static inline CFStringRef cDataToCfstr(const CSSM_DATA &cdata)
{
	return CFStringCreateWithBytes(NULL, 
		cdata.Data, (CFIndex)cdata.Length, 
		kCFStringEncodingASCII, false);
}

static void cfstrToCdata(
	const CFStringRef	cfstr,
	CSSM_DATA			&cdata,
	Allocator			&alloc)
{
	CFDataRef cfData = CFStringCreateExternalRepresentation(NULL,
		cfstr, kCFStringEncodingUTF8, 0);
	if(cfData == NULL) {
		dotMacErrorLog("dotMac cfstrToCdatafailure");
		CssmError::throwMe(internalComponentErr);
	}
	cdata.Length = CFDataGetLength(cfData);
	cdata.Data = (uint8 *)alloc.malloc(cdata.Length);
	memmove(cdata.Data, CFDataGetBytePtr(cfData), cdata.Length);
	CFRelease(cfData);
}

static CFURLRef createUrl(
	const char *schema,
	const CSSM_DATA &hostName, 
	const char *path)
{
	int schemaLen = strlen(schema); 
	int urlLength = schemaLen + hostName.Length + strlen(path) + 1;
	char *urlStr = (char *)malloc(urlLength);
	memmove(urlStr, schema, schemaLen);
	memmove(urlStr + schemaLen, hostName.Data, hostName.Length);
	urlStr[schemaLen + hostName.Length] = '\0';
	strcat(urlStr, path);
	CFURLRef url = CFURLCreateWithBytes(NULL, (const UInt8 *)urlStr, urlLength-1, 
		kCFStringEncodingASCII, NULL);
	dotMacDebug("dotMac createUrl: URL %s", urlStr);
	free(urlStr);
	return url;
}

OSStatus dotMacPostCertReq(
	DotMacSignType		signType,
	const CSSM_DATA		&userName,
	const CSSM_DATA		&password,
	const CSSM_DATA		&hostName,
	bool				renew,
	const CSSM_DATA		&csr,				// DER encoded 
	SecNssCoder			&coder,
	sint32				&estTime,			// possibly returned
	CSSM_DATA			&resultBodyData)	// possibly returned
{
	resultBodyData.Data = NULL;
	resultBodyData.Length = 0;

	/* 
	 * First gather arguments into CF form.
	 */
	CFURLRef url = createUrl(DOT_MAC_SIGN_SCHEMA, hostName, DOT_MAC_SIGN_PATH);
	CFStringRef userStr = cDataToCfstr(userName);
	CFStringRef pwdStr  = cDataToCfstr(password);
	CFStringRef csrStr  = cDataToCfstr(csr);
	
	/*
	 * Now cook up arguments for an XMLRPC.
	 */
	CFMutableDictionaryRef argDict;
	CFMutableArrayRef argOrder;
	CFStringRef method;
	
	switch(signType) {
		case DMST_Identity:
			method = kMethSignIdentity;
			break;
		case DMST_EmailSigning:
			method = kMethSignEmail;
			break;
		case DMST_EmailEncrypting:
			method = kMethSignEmailEncrypt;
			break;
		default:
			return paramErr;
	}

	argDict = CFDictionaryCreateMutable(NULL, 0, &kCFCopyStringDictionaryKeyCallBacks,
		&kCFTypeDictionaryValueCallBacks);
	argOrder = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	
	CFStringRef param1 = renew ? CFSTR("renew") : CFSTR("new");
	CFDictionaryAddValue(argDict, kP1, param1);
	CFArrayAppendValue(argOrder, kP1);
	
	CFDictionaryAddValue(argDict, kP2, csrStr);
	CFArrayAppendValue(argOrder, kP2);
	
	OSStatus ortn;
	CFDictionaryRef resultDict = NULL;
	uint32_t httpStat;
	
	ortn = performAuthenticatedXMLRPC(url, method, argDict, argOrder, userStr, pwdStr,
		&resultDict, &httpStat);
	
	if(ortn) {
		#if FORCE_SUCCESS_QUEUED_ALWAYS
		dotMacErrorLog("dotMacPostCertReq: FORCING queued success\n");
		ortn = CSSMERR_APPLE_DOTMAC_REQ_QUEUED;
		goto proceedQueued;
		#endif
		dotMacErrorLog("dotMacPostCertReq: XMLRPC returned %ld\n", ortn);
		/* And we're dead - means RPC did not complete */
		goto errOut;
	}
	if(resultDict == NULL) {
		dotMacErrorLog("dotMacPostCertReq: XMLRPC failed to return a dictionary\n");
		ortn = ioErr;
		goto errOut;
	}
	#if RESULTS_DICTIONARY_DEBUG
	dumpDictionary("Results dictionary", resultDict);
	#endif

	CFStringRef resultCode;
	resultCode = (CFStringRef)CFDictionaryGetValue(resultDict, kResponseResultCode);
	if((resultCode == NULL) || (CFGetTypeID(resultCode) != CFStringGetTypeID())) {
		dotMacErrorLog("dotMacPostCertReq: resultCode is not a string\n");
		ortn = ioErr;
		goto errOut;
	}
	ortn = dotMacParseResult(resultCode);
	
#if	FORCE_SUCCESS_QUEUED_ALWAYS
proceedQueued:
#endif

	/* For some results, we have some data to give to caller */
	switch(ortn) {
		case noErr:								/* resultBody = PEM-encoded cert */
		case CSSMERR_APPLE_DOTMAC_REQ_REDIRECT: /* resultBody = URL */
		{
			CFStringRef resultBody;
			resultBody = (CFStringRef)CFDictionaryGetValue(resultDict, 
					kResponseResultBody);
			if((resultBody == NULL) ||(CFGetTypeID(resultBody) != CFStringGetTypeID())) {
				dotMacErrorLog("dotMacPostCertReq: resultBody is not a string\n");
				ortn = ioErr;
				goto errOut;
			}
			CFIndex len = CFStringGetLength(resultBody);
			coder.allocItem(resultBodyData, (size_t)len + 1);
			if(!CFStringGetCString(resultBody, (char *)resultBodyData.Data, len+1, 
					kCFStringEncodingUTF8)) {
				dotMacErrorLog("dotMacPostCertReq: resultBody is not convertible to "
					"UTF8\n");
				ortn = ioErr;
			}
			break;
		}
		
		case CSSMERR_APPLE_DOTMAC_REQ_QUEUED:
		{
			/* 
			 * Cook up an opaque reference ID which enables us to fetch the
			 * result of this queued request at a later time. 
			 *
			 * The estimated availability time is returned in the 
			 * resultsBody as a string value.
			 */
			ortn = dotMacEncodeRefId(userName, signType, coder, resultBodyData);
			if(ortn == noErr) {
				ortn = CSSMERR_APPLE_DOTMAC_REQ_QUEUED;
			}

			/* Return the estimated time */
			CFStringRef resultBody;
			resultBody = (CFStringRef)CFDictionaryGetValue(resultDict, 
							kResponseResultBody);
			if((resultBody == NULL) ||(CFGetTypeID(resultBody) != CFStringGetTypeID())) {
				dotMacErrorLog("dotMacPostCertReq: resultBody is not a string\n");
				ortn = ioErr;
				goto errOut;
			}
			SInt32 timeValue = CFStringGetIntValue(resultBody);
			estTime = (sint32) timeValue;
			break;
		}
		default:
			dotMacErrorLog("dotMacPostCertReq: unhandled result %d\n", (int)ortn);
			break;
	}
	
errOut:
	CFRelease(url);
	CFRelease(userStr);
	CFRelease(pwdStr);
	CFRelease(csrStr);
	CFRelease(argDict);
	CFRelease(argOrder);
	if(resultDict) {
		CFRelease(resultDict);
	}
	return ortn;

}

/* post archive request */
OSStatus dotMacPostArchiveReq(
	DotMacArchiveType	archiveType,
	const CSSM_DATA		&userName,
	const CSSM_DATA		&password,
	const CSSM_DATA		&hostName,
	const CSSM_DATA		*archiveName,
	const CSSM_DATA		*pfxIn,			// for store only
	const CSSM_DATA		*timeString,	// for store only
	CSSM_DATA			*pfxOut,		// RETURNED for fetch, allocated via alloc
	unsigned			*numArchives,	// RETURNED for list
	DotMacArchive		**archives,		// RETURNED for list, allocated via alloc
	Allocator			&alloc)
{
	/* init return values */
	if(pfxOut) {
		pfxOut->Data = NULL;
		pfxOut->Length = 0;
	}
	if(numArchives) {
		*numArchives = NULL;
	}
	if(archives) {
		*archives = NULL;
	}

	/* 
	 * gather arguments into CF form.
	 * NOTE WELL: we rely on caller to validate required inputs, it just works
	 * out a lot cleaner to verify there than here. 
	 */
	CFURLRef url = createUrl(DOT_MAC_ARCHIVE_SCHEMA, hostName, DOT_MAC_ARCHIVE_PATH);
	CFStringRef userStr = cDataToCfstr(userName);
	CFStringRef pwdStr  = cDataToCfstr(password);
	CFStringRef pfxInStr = NULL;
	if(pfxIn) {
		pfxInStr = cDataToCfstr(*pfxIn);
	}
	CFStringRef arhiveNameStr = NULL;
	if(archiveName) {
		arhiveNameStr = cDataToCfstr(*archiveName);
	}
	CFStringRef timeStringStr = NULL;
	if(timeString) {
		timeStringStr = cDataToCfstr(*timeString);
	}
	
	/*
	 * Now cook up arguments for an XMLRPC.
	 */
	CFMutableDictionaryRef argDict;
	CFMutableArrayRef argOrder;
	CFStringRef method;
	argDict = CFDictionaryCreateMutable(NULL, 0, &kCFCopyStringDictionaryKeyCallBacks,
		&kCFTypeDictionaryValueCallBacks);
	argOrder = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	
	switch(archiveType) {
		case DMAT_List:
			method = kMethArchiveList;
			/* no parameters */
			break;
		case DMAT_Store:
			method = kMethArchiveStore;
			/* parameters: name, date, pfx */
			assert(arhiveNameStr != NULL);
			assert(timeStringStr != NULL);
			assert(pfxInStr != NULL);
			CFDictionaryAddValue(argDict, kP1, arhiveNameStr);
			CFArrayAppendValue(argOrder, kP1);
			CFDictionaryAddValue(argDict, kP2, timeStringStr);
			CFArrayAppendValue(argOrder, kP2);
			CFDictionaryAddValue(argDict, kP3, pfxInStr);
			CFArrayAppendValue(argOrder, kP3);
			break;
		case DMAT_Fetch:
			method = kMethArchiveFetch;
			/* parameters: name */
			assert(arhiveNameStr != NULL);
			CFDictionaryAddValue(argDict, kP1, arhiveNameStr);
			CFArrayAppendValue(argOrder, kP1);
			break;
		case DMAT_Remove:
			method = kMethArchiveRemove;
			/* parameters: name */
			assert(arhiveNameStr != NULL);
			CFDictionaryAddValue(argDict, kP1, arhiveNameStr);
			CFArrayAppendValue(argOrder, kP1);
			break;
		default:
			return paramErr;
	}

	OSStatus ortn;
	CFDictionaryRef resultDict = NULL;
	uint32_t httpStat;
	
	#if REQUEST_DICTIONARY_DEBUG
	dumpDictionary("Arguments dictionary", argDict);
	#endif
	ortn = performAuthenticatedXMLRPC(url, method, argDict, argOrder, userStr, pwdStr,
		&resultDict, &httpStat);
	
	if(ortn) {
		dotMacErrorLog("dotMacPostArchiveReq: XMLRPC returned %ld\n", ortn);
		/* And we're dead - means RPC did not complete */
		goto errOut;
	}
	if(resultDict == NULL) {
		dotMacErrorLog("dotMacPostArchiveReq: XMLRPC failed to return a dictionary\n");
		ortn = ioErr;
		goto errOut;
	}
	#if RESULTS_DICTIONARY_DEBUG
	dumpDictionary("Results dictionary", resultDict);
	#endif

	CFStringRef resultCode;
	resultCode = (CFStringRef)CFDictionaryGetValue(resultDict, kResponseResultCode);
	if((resultCode == NULL) || (CFGetTypeID(resultCode) != CFStringGetTypeID())) {
		dotMacErrorLog("dotMacPostArchiveReq: resultCode is not a string\n");
		ortn = ioErr;
		goto errOut;
	}
	
	/* no partial success on this one - it worked or it didn't */
	ortn = dotMacParseResult(resultCode);
	if(ortn) {
		goto errOut;
	}
	
	/* For some ops, we have some data to give to caller */
	switch(archiveType) {
		case DMAT_List:
		{
			/* This is the hard one */
			CFArrayRef archList;
			archList = (CFArrayRef)CFDictionaryGetValue(resultDict, 
					kResponseResultBody);
			if((archList == NULL) || (CFGetTypeID(archList) != CFArrayGetTypeID())) {
				dotMacErrorLog("archive(list): resultBody is not an array\n");
				ortn = ioErr;
				goto errOut;
			}
			assert(numArchives != NULL);
			assert(archives != NULL);
			CFIndex numEntries = CFArrayGetCount(archList);
			*archives = (DotMacArchive *)alloc.malloc(sizeof(DotMacArchive) * numEntries);
			memset(*archives, 0, sizeof(DotMacArchive) * numEntries);
			for(CFIndex dex=0; dex<numEntries; dex++) {
				DotMacArchive *dmarc = &((*archives)[dex]);
				CFDictionaryRef dict = 
					(CFDictionaryRef)CFArrayGetValueAtIndex(archList, dex);
				if((dict == NULL) || (CFGetTypeID(dict) != CFDictionaryGetTypeID())) {
					dotMacErrorLog("archive(list): result element is not a dict\n");
					ortn = ioErr;
					goto errOut;
				}
				CFStringRef cfstr = (CFStringRef)CFDictionaryGetValue(dict, 
						kArchiveListName);
				if((cfstr == NULL) || (CFGetTypeID(cfstr) != CFStringGetTypeID())) {
					dotMacErrorLog("archive(list): archiveName is not a string\n");
					ortn = ioErr;
					goto errOut;
				}
				cfstrToCdata(cfstr, dmarc->archiveName, alloc);
				cfstr = (CFStringRef)CFDictionaryGetValue(dict, 
						kArchiveListExpires);
				if((cfstr == NULL) || (CFGetTypeID(cfstr) != CFStringGetTypeID())) {
					dotMacErrorLog("archive(list): expires is not a string\n");
					ortn = ioErr;
					goto errOut;
				}
				cfstrToCdata(cfstr, dmarc->timeString, alloc);

			}
			*numArchives = numEntries;
			break;
		}	
		case DMAT_Store:
			/* no returned data */
			break;

		case DMAT_Fetch:
		{
			/* resultBody is a PKCS12 PFX */
			CFStringRef pfxStr;
			pfxStr = (CFStringRef)CFDictionaryGetValue(resultDict, 
					kResponseResultBody);
			if((pfxStr == NULL) ||(CFGetTypeID(pfxStr) != CFStringGetTypeID())) {
				dotMacErrorLog("archive(fetch): resultBody is not a string\n");
				ortn = ioErr;
				goto errOut;
			}
			assert(pfxOut != NULL);
			cfstrToCdata(pfxStr, *pfxOut, alloc);
			break;
		}
		case DMAT_Remove:
			/* no returned data */
			break;

		default:
			return paramErr;
	}
	
errOut:
	CFRELEASE(url);
	CFRELEASE(userStr);
	CFRELEASE(pwdStr);
	CFRELEASE(pfxInStr);
	CFRELEASE(arhiveNameStr);
	CFRELEASE(timeStringStr);
	CFRELEASE(argDict);
	CFRELEASE(argOrder);
	CFRELEASE(resultDict);

	return ortn;

}

/* 
 * Post "Is request pending?" request.
 * Aside from gross network failures and so forth this returns one of two values:
 *
 * CSSMERR_APPLE_DOTMAC_REQ_IS_PENDING -- a request is pending
 * CSSMERR_APPLE_DOTMAC_NO_REQ_PENDING -- *no* request pending
 */
OSStatus dotMacPostReqPendingPing(
	const CSSM_DATA		&userName,
	const CSSM_DATA		&password,
	const CSSM_DATA		&hostName)
{
	/* 
	 * First gather arguments into CF form.
	 */
	CFURLRef url = createUrl(DOT_MAC_SIGN_SCHEMA, hostName, DOT_MAC_SIGN_PATH);
	CFStringRef userStr = cDataToCfstr(userName);
	CFStringRef pwdStr  = cDataToCfstr(password);
	
	/*
	 * Cook up empty arguments dict for an XMLRPC.
	 */
	CFMutableDictionaryRef argDict = 
		CFDictionaryCreateMutable(NULL, 0, &kCFCopyStringDictionaryKeyCallBacks,
		&kCFTypeDictionaryValueCallBacks);
	CFMutableArrayRef argOrder = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

	OSStatus ortn;
	CFDictionaryRef resultDict = NULL;
	uint32_t httpStat;
	
	ortn = performAuthenticatedXMLRPC(url, kMethSigningStatus, argDict, argOrder, 
		userStr, pwdStr, &resultDict, &httpStat);
	
	if(ortn) {
		dotMacErrorLog("dotMacPostReqPendingPing: XMLRPC returned %ld\n", ortn);
		/* And we're dead - means RPC did not complete */
		goto errOut;
	}
	if(resultDict == NULL) {
		dotMacErrorLog("dotMacPostReqPendingPing: XMLRPC failed to return a dictionary\n");
		ortn = ioErr;
		goto errOut;
	}
	#if RESULTS_DICTIONARY_DEBUG
	dumpDictionary("Results dictionary", resultDict);
	#endif

	CFStringRef resultCode;
	resultCode = (CFStringRef)CFDictionaryGetValue(resultDict, kResponseResultCode);
	if((resultCode == NULL) || (CFGetTypeID(resultCode) != CFStringGetTypeID())) {
		dotMacErrorLog("dotMacPostCertReq: resultCode is not a string\n");
		ortn = ioErr;
		goto errOut;
	}
	ortn = dotMacParseResult(resultCode);
	/* should not return success */
	dotMacDebug("dotMacPostReqPendingPing: ortn %lu", ortn);
	
errOut:
	CFRELEASE(url);
	CFRELEASE(userStr);
	CFRELEASE(pwdStr);
	CFRELEASE(argDict);
	CFRELEASE(argOrder);
	CFRELEASE(resultDict);
	return ortn;
}
