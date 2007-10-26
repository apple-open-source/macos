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
 * Constant strings for Treadstone XMLRPC API.
 */
 
/* 
 * Method names.
 */
static CFStringRef kMethodSignIChatEncrypt		= CFSTR("sign." DOT_MAC_CERT_TYPE_ICHAT);
static CFStringRef kMethodSignSharedServices	= CFSTR("sign." DOT_MAC_CERT_TYPE_SHARED_SERVICES);
static CFStringRef kMethodSignEmailSigning		= CFSTR("sign." DOT_MAC_CERT_TYPE_EMAIL_SIGNING);
static CFStringRef kMethodSignEmailEncryption	= CFSTR("sign." DOT_MAC_CERT_TYPE_EMAIL_ENCRYPT);
static CFStringRef kMethodStatusIChatEncrypt	= CFSTR("status." DOT_MAC_CERT_TYPE_ICHAT);
static CFStringRef kMethodStatusSharedServices	= CFSTR("status." DOT_MAC_CERT_TYPE_SHARED_SERVICES);
static CFStringRef kMethodStatusEmailSigning	= CFSTR("status." DOT_MAC_CERT_TYPE_EMAIL_SIGNING);
static CFStringRef kMethodStatusEmailEncryption	= CFSTR("status." DOT_MAC_CERT_TYPE_EMAIL_ENCRYPT);
static CFStringRef kMethodArchiveList			= CFSTR("archive.list");
static CFStringRef kMethodArchiveSave			= CFSTR("archive.save");
static CFStringRef kMethodArchiveFetch			= CFSTR("archive.fetch");
static CFStringRef kMethodArchiveRemove			= CFSTR("archive.remove");

/*
 * Fixed parameter names.
 */
 
/* first param to sign */
static CFStringRef kParamSignIssue					= CFSTR("issue");	
/* 
 * CertTypeTag as parameter to archive.save
 * Also used as one of the out params from archive.list
 */
static CFStringRef kParamCertTypeIChat				= CFSTR(DOT_MAC_CERT_TYPE_ICHAT);
static CFStringRef kParamCertTypeSharedServices		= CFSTR(DOT_MAC_CERT_TYPE_SHARED_SERVICES);
static CFStringRef kParamCertTypeEmailEncryption	= CFSTR(DOT_MAC_CERT_TYPE_EMAIL_SIGNING);
static CFStringRef kParamCertTypeEmailSigning		= CFSTR(DOT_MAC_CERT_TYPE_EMAIL_ENCRYPT);

/* 
 * names of values in an XMLRPC response 
 */
static CFStringRef kResponseResultCode				= CFSTR("resultCode");
/* 
 * FIXME: We don't use this: Should we?
 */
// static CFStringRef kResponseTimestamp			= CFSTR("timestamp");
static CFStringRef kResponseResultBody				= CFSTR("resultBody");

/* 
 * names of values in an archive.list dictionary 
 */
static CFStringRef kArchiveListName					= CFSTR("name");
static CFStringRef kArchiveListExpires				= CFSTR("expires");
static CFStringRef kArchiveListType					= CFSTR("type");
static CFStringRef kArchiveListSerialNumber			= CFSTR("serial");

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
static CFStringRef kResultAlreadyExists			= CFSTR("FailedAlreadyExists"); 
static CFStringRef kResultCertAlreadyExists		= CFSTR("FailedCertAlreadyExists");
static CFStringRef kResultServiceError			= CFSTR("FailedServiceError");
static CFStringRef kResultParameterError		= CFSTR("FailedParameterError");
static CFStringRef kResultNotAllowed			= CFSTR("FailedNotAllowed");
static CFStringRef kResultPendingCSR			= CFSTR("FailedPendingCSR");
static CFStringRef kResultNoExistingCSR			= CFSTR("FailedNoExistingCSR");
static CFStringRef kResultNotSupportForAccount  = CFSTR("FailedNotSupportedForAccount");
static CFStringRef kResultCSRDidNotVerify		= CFSTR("FailedCSRDidNotVerify");
static CFStringRef kResultNotImplemented		= CFSTR("NotImplemented");
static CFStringRef kResultNotAuthorized			= CFSTR("NotAuthorized");
static CFStringRef kResultNotAvailable			= CFSTR("NotAvailable");
static CFStringRef kResultConsistencyCheck		= CFSTR("FailedConsistencyCheck");


/* quickie parameter names which don't go over the wire, just for ordering */
static CFStringRef kP1 = CFSTR("p1");
static CFStringRef kP2 = CFSTR("p2");
static CFStringRef kP3 = CFSTR("p3");
static CFStringRef kP4 = CFSTR("p4");
static CFStringRef kP5 = CFSTR("p5");

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
	else if(CFEqual(resultCode, kResultParameterError)) {
		return CSSMERR_APPLE_DOTMAC_REQ_SERVER_PARAM;
	}
	else if(CFEqual(resultCode, kResultNotAllowed)) {
		return CSSMERR_APPLE_DOTMAC_REQ_SERVER_AUTH;
	}
	else if(CFEqual(resultCode, kResultPendingCSR)) {
		return CSSMERR_APPLE_DOTMAC_REQ_IS_PENDING;
	}
	else if(CFEqual(resultCode, kResultNoExistingCSR)) {
		return CSSMERR_APPLE_DOTMAC_NO_REQ_PENDING;
	}
	else if(CFEqual(resultCode, kResultNotSupportForAccount)) {
		/* FIXME might want a .mac TP-specific error for this */
		return CSSMERR_TP_REQUEST_REJECTED;
	}
	else if(CFEqual(resultCode, kResultCSRDidNotVerify)) {
		return CSSMERR_APPLE_DOTMAC_CSR_VERIFY_FAIL;
	}
	else if(CFEqual(resultCode, kResultServiceError)) {
		return CSSMERR_APPLE_DOTMAC_REQ_SERVER_SERVICE_ERROR;
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
	else if(CFEqual(resultCode, kResultConsistencyCheck)) {
		return CSSMERR_APPLE_DOTMAC_FAILED_CONSISTENCY_CHECK;
	}
	
	else {
		dotMacErrorLog("dotMacParseResult: unknown error\n");
		return ioErr;
	}
}

/* CSSM_DATA <--> CFStringRef */
static inline CFStringRef cDataToCfstr(
	const CSSM_DATA &cdata)
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

/*
 * Convert between binary data - e.g. a cert serial number - and a CFStringRef.
 * We do this with the same logic as SecurityInterface: if the data is 8 bytes
 * or less, encode it as a decimal number, else stringify it as ASCII hex. 
 */
static const char hexChars[] = "0123456789ABCDEF";

static CFStringRef cDataToCfAsciiStr(
	const CSSM_DATA &cdata)
{
	if(cdata.Length > sizeof(uint64)) {
		char asciiData[(2 * cdata.Length) + 1];
		const unsigned char *inp = (const unsigned char *)cdata.Data;
		char *outp = asciiData;
		
		for(unsigned dex=0; dex<cdata.Length; dex++) {
			unsigned c = *inp++;
			outp[1] = hexChars[c & 0xf];
			c >>= 4;
			outp[0] = hexChars[c];
			outp += 2;
		}
		*outp = 0;
		return CFStringCreateWithCString(NULL, asciiData, kCFStringEncodingASCII);
	}
	else {
		uint64 value = 0;
		for(unsigned i=0; i<cdata.Length; i++) {
			value <<= 8;
			value += cdata.Data[i];
		}
		char cStr[200];
		snprintf(cStr, sizeof(cStr), "%llu", value);
		return CFStringCreateWithCString(NULL, cStr, kCFStringEncodingASCII);
	}
}

/* 
 * Convert a serial number string as created by cDataToCfAsciiStr() to
 * a CSSM_DATA. We have to assume it's in decimal here.
 */
static void CfAsciiStrToCdata(
	const CFStringRef	cfStr,
	CSSM_DATA			&cdata,
	Allocator			&alloc)
{
	/* the string MUST be in ASCII */
	CFDataRef strData = CFStringCreateExternalRepresentation(NULL, cfStr,
		kCFStringEncodingASCII, 0);
	if(strData == NULL) {
		dotMacDebug("dotMac CfAsciiStrToCdata: ASCII conversion FAILED!");
		return;
	}
	unsigned len = (unsigned)CFDataGetLength(strData);
	const char *inp = (const char *)CFDataGetBytePtr(strData);
	char cStr[len + 1];
	memmove(cStr, inp, len);
	cStr[len] = '\0';
	
	uint64 val = 0;
	sscanf(cStr, "%llu", &val);
	
	/* convert 64-bit val to byte array */
	int byteNum = 7;			// byte within 64-bit val
	unsigned shift = 7*8;		// bits to shift right to move that into one byte
	uint64 mask = 0xff00000000000000ULL;
	
	/* skip over zeroes in m.s. bytes */
	while(val & mask) {
		byteNum--;
		shift -= 8;
		mask >>= 8;
	}
	
	cdata.Data = (uint8 *)alloc.malloc(byteNum + 1);
	cdata.Length = byteNum + 1;
	uint8 *outp = cdata.Data;
	
	do {
		uint64 v = val >> shift;
		*outp++ = v & 0xff;
		shift -= 8;
		byteNum--;
	} while(byteNum >= 0);
	
	CFRelease(strData);	
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
	DotMacCertTypeTag	certType,
	const CSSM_DATA		&userName,
	const CSSM_DATA		&password,
	const CSSM_DATA		&hostName,
	bool				renew,				// this is obsolete; currently ignored 
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
	
	switch(certType) {
		case CSSM_DOT_MAC_TYPE_ICHAT:
			method = kMethodSignIChatEncrypt;
			break;
		case CSSM_DOT_MAC_TYPE_SHARED_SERVICES:
			method = kMethodSignSharedServices;
			break;
		case CSSM_DOT_MAC_TYPE_EMAIL_ENCRYPT:
			method = kMethodSignEmailEncryption;
			break;
		case CSSM_DOT_MAC_TYPE_EMAIL_SIGNING:
			method = kMethodSignEmailSigning;
			break;
		default:
			return paramErr;
	}

	argDict = CFDictionaryCreateMutable(NULL, 0, &kCFCopyStringDictionaryKeyCallBacks,
		&kCFTypeDictionaryValueCallBacks);
	argOrder = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	
	/* this used to be "new" or "renew" */
	CFDictionaryAddValue(argDict, kP1, kParamSignIssue);
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
			ortn = dotMacEncodeRefId(userName, certType, coder, resultBodyData);
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

/* 
 * Return archive list in one of two formats.
 * Returned memory is allocated in provided allocator's space. 
 *
 * This version is used when the app provided a v1 CSSM_APPLE_DOTMAC_TP_ARCHIVE_REQUEST,
 * and it therefore expecting an array of DotMacArchive in return. This can be
 * deleted when we've ensured that nobody is using the v1 request. 
 */
static OSStatus dotMacReturnArchiveList_v1(
	CFDictionaryRef		resultDict,
	unsigned			*numArchives,	// RETURNED
	DotMacArchive		**archives,		// RETURNED
	Allocator			&alloc)
{
	CFArrayRef archList;
	archList = (CFArrayRef)CFDictionaryGetValue(resultDict, 
			kResponseResultBody);
	if((archList == NULL) || (CFGetTypeID(archList) != CFArrayGetTypeID())) {
		dotMacErrorLog("archive(list): resultBody is not an array\n");
		return ioErr;
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
			return ioErr;
		}
		
		/* extract two fields from this dictionary: name and expiration date */
		CFStringRef cfstr = (CFStringRef)CFDictionaryGetValue(dict, 
				kArchiveListName);
		if((cfstr == NULL) || (CFGetTypeID(cfstr) != CFStringGetTypeID())) {
			dotMacErrorLog("archive(list): archiveName is not a string\n");
			return ioErr;
		}
		cfstrToCdata(cfstr, dmarc->archiveName, alloc);
		
		cfstr = (CFStringRef)CFDictionaryGetValue(dict, kArchiveListExpires);
		if((cfstr == NULL) || (CFGetTypeID(cfstr) != CFStringGetTypeID())) {
			dotMacErrorLog("archive(list): expires is not a string\n");
			return ioErr;
		}
		cfstrToCdata(cfstr, dmarc->timeString, alloc);
	}
	*numArchives = numEntries;
	return noErr;
}

/* V. 2, Treadstone style Archive list */
static OSStatus dotMacReturnArchiveList_v2(
	CFDictionaryRef		resultDict,
	unsigned			*numArchives,	// RETURNED
	DotMacArchive_v2	**archives,		// RETURNED
	Allocator			&alloc)
{
	CFArrayRef archList;
	archList = (CFArrayRef)CFDictionaryGetValue(resultDict, 
			kResponseResultBody);
	if((archList == NULL) || (CFGetTypeID(archList) != CFArrayGetTypeID())) {
		dotMacErrorLog("archive(list): resultBody is not an array\n");
		return ioErr;
	}
	assert(numArchives != NULL);
	assert(archives != NULL);
	CFIndex numEntries = CFArrayGetCount(archList);
	*archives = (DotMacArchive_v2 *)alloc.malloc(sizeof(DotMacArchive_v2) * numEntries);
	memset(*archives, 0, sizeof(DotMacArchive_v2) * numEntries);
	for(CFIndex dex=0; dex<numEntries; dex++) {
		DotMacArchive_v2 *dmarc = &((*archives)[dex]);
		CFDictionaryRef dict = 
			(CFDictionaryRef)CFArrayGetValueAtIndex(archList, dex);
		if((dict == NULL) || (CFGetTypeID(dict) != CFDictionaryGetTypeID())) {
			dotMacErrorLog("archive(list): result element is not a dict\n");
			return ioErr;
		}
		
		/* 
		 * Extract 4 fields from this dictionary: 
		 * name 
		 * expiration date
		 * certTypeTag
		 * serial number
		 */
		CFStringRef cfstr = (CFStringRef)CFDictionaryGetValue(dict, 
				kArchiveListName);
		if((cfstr == NULL) || (CFGetTypeID(cfstr) != CFStringGetTypeID())) {
			dotMacErrorLog("archive(list): archiveName is not a string\n");
			return ioErr;
		}
		cfstrToCdata(cfstr, dmarc->archiveName, alloc);
		
		/* expiration date */
		cfstr = (CFStringRef)CFDictionaryGetValue(dict, kArchiveListExpires);
		if((cfstr == NULL) || (CFGetTypeID(cfstr) != CFStringGetTypeID())) {
			dotMacErrorLog("archive(list): expires is not a string\n");
			return ioErr;
		}
		cfstrToCdata(cfstr, dmarc->timeString, alloc);

		/* certTypeTag */
		cfstr = (CFStringRef)CFDictionaryGetValue(dict, kArchiveListType);
		if((cfstr == NULL) || (CFGetTypeID(cfstr) != CFStringGetTypeID())) {
			dotMacErrorLog("archive(list): CertType is not a string\n");
			return ioErr;
		}
		if(CFEqual(cfstr, kParamCertTypeIChat)) {
			dmarc->certTypeTag = CSSM_DOT_MAC_TYPE_ICHAT;
		}
		else if(CFEqual(cfstr, kParamCertTypeSharedServices)) {
			dmarc->certTypeTag = CSSM_DOT_MAC_TYPE_SHARED_SERVICES;
		}
		else if(CFEqual(cfstr, kParamCertTypeEmailEncryption)) {
			dmarc->certTypeTag = CSSM_DOT_MAC_TYPE_EMAIL_ENCRYPT;
		}
		else if(CFEqual(cfstr, kParamCertTypeEmailSigning)) {
			dmarc->certTypeTag = CSSM_DOT_MAC_TYPE_EMAIL_SIGNING;
		}
		else {
			dmarc->certTypeTag = CSSM_DOT_MAC_TYPE_UNSPECIFIED;
		}

		/* serial number */
		cfstr = (CFStringRef)CFDictionaryGetValue(dict, kArchiveListSerialNumber);
		if((cfstr == NULL) || (CFGetTypeID(cfstr) != CFStringGetTypeID())) {
			dotMacErrorLog("archive(list): Serial Number is not a string\n");
			return ioErr;
		}
		CfAsciiStrToCdata(cfstr, dmarc->serialNumber, alloc);
	}
	*numArchives = numEntries;
	return noErr;
}

/* post archive request */
OSStatus dotMacPostArchiveReq(
	uint32				version,
	DotMacCertTypeTag	certTypeTag,
	DotMacArchiveType	archiveType,
	const CSSM_DATA		&userName,
	const CSSM_DATA		&password,
	const CSSM_DATA		&hostName,
	const CSSM_DATA		*archiveName,
	const CSSM_DATA		*pfxIn,			// for store only
	const CSSM_DATA		*timeString,	// for store only
	const CSSM_DATA		*serialNumber,	// for store only
	CSSM_DATA			*pfxOut,		// RETURNED for fetch, allocated via alloc
	unsigned			*numArchives,	// RETURNED for list
	// at most one of the following is returned, and for list only
	DotMacArchive		**archives_v1,	// RETURNED for list, allocated via alloc
	DotMacArchive_v2	**archives_v2,		
	Allocator			&alloc)
{
	/* init return values */
	if(pfxOut) {
		pfxOut->Data = NULL;
		pfxOut->Length = 0;
	}
	if(numArchives) {
		*numArchives = 0;
	}
	if(archives_v1) {
		*archives_v1 = NULL;
	}
	if(archives_v2) {
		*archives_v2 = NULL;
	}
	/* 
	 * gather arguments into CF form.
	 * NOTE: This implementation always does a Treadstone-style XMLRPC, regardless
	 *       of what the caller (the app) is trying to do. If the app is doing 
	 *       a v1 (pre-Treadstone op), we use default/NULL values for params
	 *       like serial number that we have to send to the server; in the case
	 *		 of a list op, we return one of two forms of archive (DotMacArchive
	 *		 or DotMacArchive_v2). 
	 * NOTE WELL: we rely on caller to validate required inputs, it just works
	 *		 out a lot cleaner to verify there than here. 
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
	
	uint8 zero = 0;
	CSSM_DATA zeroData = {1, &zero};
	CFStringRef serialNumberStr = NULL;
	CFStringRef certTypeStr = NULL;
	if(archiveType == DMAT_Store) {
		/* 
		 * Need a serial number for this. If caller didn't provide one, 
		 * make an empty one.
		 * WARNING this actually will result in a failure on the server side
		 * since the server requires the correct serial number. 
		 */
		if(serialNumber == NULL) {
			serialNumber = &zeroData;
		}
		serialNumberStr = cDataToCfAsciiStr(*serialNumber);
		
		/* certTypeTag --> string */
		switch(certTypeTag) {
			case CSSM_DOT_MAC_TYPE_ICHAT:
				certTypeStr = kParamCertTypeIChat;
				break;
			case CSSM_DOT_MAC_TYPE_SHARED_SERVICES:
				certTypeStr = kParamCertTypeSharedServices;
				break;
			case CSSM_DOT_MAC_TYPE_EMAIL_ENCRYPT:
				certTypeStr = kParamCertTypeEmailEncryption;
				break;
			case CSSM_DOT_MAC_TYPE_EMAIL_SIGNING:
				certTypeStr = kParamCertTypeEmailSigning;
				break;
			default:
				dotMacErrorLog("dotMacPostArchiveReq: Bad cert type\n");
				return paramErr;
		}
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
			method = kMethodArchiveList;
			/* no parameters */
			break;
		case DMAT_Store:
			method = kMethodArchiveSave;
			/* parameters: name, pfx, certType, date, serialNumber */
			assert(arhiveNameStr != NULL);
			assert(timeStringStr != NULL);
			assert(pfxInStr != NULL);
			assert(serialNumberStr != NULL);
			assert(certTypeStr != NULL);
			CFDictionaryAddValue(argDict, kP1, arhiveNameStr);
			CFArrayAppendValue(argOrder, kP1);
			CFDictionaryAddValue(argDict, kP2, pfxInStr);
			CFArrayAppendValue(argOrder, kP2);
			CFDictionaryAddValue(argDict, kP3, certTypeStr);
			CFArrayAppendValue(argOrder, kP3);
			CFDictionaryAddValue(argDict, kP4, timeStringStr);
			CFArrayAppendValue(argOrder, kP4);
			CFDictionaryAddValue(argDict, kP5, serialNumberStr);
			CFArrayAppendValue(argOrder, kP5);
			break;
		case DMAT_Fetch:
			method = kMethodArchiveFetch;
			/* parameters: name */
			assert(arhiveNameStr != NULL);
			CFDictionaryAddValue(argDict, kP1, arhiveNameStr);
			CFArrayAppendValue(argOrder, kP1);
			break;
		case DMAT_Remove:
			method = kMethodArchiveRemove;
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
			if(version == CSSM_DOT_MAC_TP_ARCHIVE_REQ_VERSION_v1) {
				assert(archives_v1 != NULL);
				ortn = dotMacReturnArchiveList_v1(resultDict, numArchives, archives_v1, alloc);
			}
			else {
				assert(archives_v2 != NULL);
				ortn = dotMacReturnArchiveList_v2(resultDict, numArchives, archives_v2, alloc);
			}
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
	DotMacCertTypeTag	certType,
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
	
	CFStringRef method;
	
	switch(certType) {
		case CSSM_DOT_MAC_TYPE_ICHAT:
			method = kMethodStatusIChatEncrypt;
			break;
		case CSSM_DOT_MAC_TYPE_SHARED_SERVICES:
			method = kMethodStatusSharedServices;
			break;
		case CSSM_DOT_MAC_TYPE_EMAIL_ENCRYPT:
			method = kMethodStatusEmailEncryption;
			break;
		case CSSM_DOT_MAC_TYPE_EMAIL_SIGNING:
			method = kMethodStatusEmailSigning;
			break;
		default:
			return paramErr;
	}
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
	
	ortn = performAuthenticatedXMLRPC(url, method, argDict, argOrder, 
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
