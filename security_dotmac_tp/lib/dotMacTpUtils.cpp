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
 * DotMacTpUtils.cpp
 */

#include "AppleDotMacTPSession.h"
#include "dotMacTpDebug.h"
#include "dotMacTpUtils.h"
#include "dotMacTpMutils.h"
#include "dotMacTp.h"
#include "dotMacTpAsn1Templates.h"
#include <security_asn1/SecNssCoder.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>
#include <security_cdsa_utils/cuPem.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#include <SystemConfiguration/SCDynamicStoreCopySpecific.h>

#define CFRELEASE(cf)			if(cf != NULL) { CFRelease(cf); }

/*
 * Given an array of name/value pairs, cook up a CSSM_X509_NAME in specified
 * SecNssCoder's address space.
 */
void dotMacTpbuildX509Name(
	SecNssCoder						&coder,
	uint32							numTypeValuePairs,  // size of typeValuePairs[]
	CSSM_X509_TYPE_VALUE_PAIR_PTR	typeValuePairs,
	CSSM_X509_NAME					&x509Name)
{
	memset(&x509Name, 0, sizeof(x509Name));
	
	/* 
	 * One RDN per type/value pair per common usage out in the world 
	 * This actual CSSM_X509_TYPE_VALUE_PAIR is NOT re-mallocd; it's copied
	 * directly into the outgoing CSSM_X509_NAME.
	 */
	x509Name.RelativeDistinguishedName = 
		coder.mallocn<CSSM_X509_RDN>(numTypeValuePairs);
	for(unsigned nameDex=0; nameDex<numTypeValuePairs; nameDex++) {
		CSSM_X509_RDN_PTR rdn = &x509Name.RelativeDistinguishedName[nameDex];
		rdn->numberOfPairs = 1;
		rdn->AttributeTypeAndValue = &typeValuePairs[nameDex];
	}
	x509Name.numberOfRDNs = numTypeValuePairs;
}

/* Convert a reference key to a raw key. */
void dotMacRefKeyToRaw(
	CSSM_CSP_HANDLE	cspHand,
	const CSSM_KEY	*refKey,	
	CSSM_KEY_PTR	rawKey)			// RETURNED
{
	CSSM_CC_HANDLE		ccHand;
	CSSM_RETURN			crtn;
	CSSM_ACCESS_CREDENTIALS	creds;
	
	memset(rawKey, 0, sizeof(CSSM_KEY));
	memset(&creds, 0, sizeof(CSSM_ACCESS_CREDENTIALS));
	crtn = CSSM_CSP_CreateSymmetricContext(cspHand,
			CSSM_ALGID_NONE,
			CSSM_ALGMODE_NONE,
			&creds,				// passPhrase
			NULL,				// wrapping key
			NULL,				// init vector
			CSSM_PADDING_NONE,	// Padding
			0,					// Params
			&ccHand);
	if(crtn) {
		dotMacErrorLog("dotMacRefKeyToRaw: context err");
		CssmError::throwMe(crtn);
	}
	
	crtn = CSSM_WrapKey(ccHand,
		&creds,
		refKey,
		NULL,			// DescriptiveData
		rawKey);
	if(crtn != CSSM_OK) {
		dotMacErrorLog("dotMacRefKeyToRaw: wrapKey err");
		CssmError::throwMe(crtn);
	}
	CSSM_DeleteContext(ccHand);
}

/*
 * Encode/decode ReferenceIdentitifiers for queued requests.
 * We PEM encode/decode here to keep things orthogonal, since returned
 * certs and URLs are also in PEM or at least UTF8 format. 
 */
OSStatus dotMacEncodeRefId(  
	const CSSM_DATA				&userName,	// UTF8, no NULL
	DotMacCertTypeTag			certTypeTag,
	SecNssCoder					&coder,		// results mallocd in this address space
	CSSM_DATA					&refId)		// RETURNED, PEM encoded
{
	DotMacTpPendingRequest req;
	
	/* set up a DotMacTpPendingRequest */
	req.userName = userName;
	uint8 certType = certTypeTag;
	req.certTypeTag.Data = &certType;
	req.certTypeTag.Length = 1;
	
	/* DER encode */
	CSSM_DATA tempData = {0, NULL};
	PRErrorCode prtn = coder.encodeItem(&req, DotMacTpPendingRequestTemplate, tempData);
	if(prtn) {
		dotMacErrorLog("dotMacEncodeRefId: encodeItem error");
		return internalComponentErr;
	}
	
	/* PEM encode */
	unsigned char *pem;
	unsigned pemLen;
	if(pemEncode(tempData.Data, tempData.Length, &pem, &pemLen, "REFERENCE ID")) {
		dotMacErrorLog("dotMacEncodeRefId: pemEncode error");
		return internalComponentErr;
	}
	refId.Data = NULL;
	refId.Length = 0;
	coder.allocCopyItem(pem, pemLen, refId);
	free(pem);
	
	return noErr;
}

OSStatus dotMacDecodeRefId(
	SecNssCoder					&coder,			// results mallocd in this address space
	const CSSM_DATA				&refId,			// PEM encoded
	CSSM_DATA					&userName,		// RETURNED, UTF8, no NULL
	DotMacCertTypeTag			*certTypeTag)	// RETURNED
{
	/* PEM decode */
	unsigned char *unPem;
	unsigned unPemLen;
	if(pemDecode(refId.Data, refId.Length, &unPem, &unPemLen)) {
		dotMacErrorLog("dotMacDecodeRefId: pemDecode error");
		return internalComponentErr;
	}
	
	/* DER decode */
	CSSM_DATA tempData;
	tempData.Data = unPem;
	tempData.Length = unPemLen;
	
	DotMacTpPendingRequest req;
	memset(&req, 0, sizeof(req));
	
	PRErrorCode prtn = coder.decodeItem(tempData, DotMacTpPendingRequestTemplate, &req);
	free(unPem);
	if(prtn) {
		dotMacErrorLog("dotMacDecodeRefId: decodeItem error");
		return paramErr;
	}
	
	/* decoded params back to caller */
	userName = req.userName;
	if(req.certTypeTag.Length != 1) {
		dotMacErrorLog("dotMacDecodeRefId: reqType length (%lu) error", req.certTypeTag.Length);
		return paramErr;
	}
	*certTypeTag = req.certTypeTag.Data[0];
	return noErr;
}

/* SPI to specify timeout on CFReadStream */
#define _kCFStreamPropertyReadTimeout   CFSTR("_kCFStreamPropertyReadTimeout")

/* the read timeout we set, in seconds */
#define READ_STREAM_TIMEOUT		15

/* amount of data per CFReadStreamRead() */
#define READ_FRAGMENT_SIZE		512

/* fetch cert via HTTP */
CSSM_RETURN dotMacTpCertFetch(
	const CSSM_DATA		&userName,  // UTF8, no NULL
	DotMacCertTypeTag	certType,
	Allocator			&alloc,		// results mallocd here
	CSSM_DATA			&result)	// RETURNED
{
	unsigned rawUrlLen;
	char *typeArg;
	CSSM_RETURN crtn = CSSM_OK;
	
	switch(certType) {
		case CSSM_DOT_MAC_TYPE_ICHAT:
		case CSSM_DOT_MAC_TYPE_UNSPECIFIED:
			typeArg = DOT_MAC_CERT_TYPE_ICHAT;
			break;
		case CSSM_DOT_MAC_TYPE_SHARED_SERVICES:
			typeArg = DOT_MAC_CERT_TYPE_SHARED_SERVICES;
			break;
		case CSSM_DOT_MAC_TYPE_EMAIL_SIGNING:
			typeArg = DOT_MAC_CERT_TYPE_EMAIL_SIGNING;
			break;
		case CSSM_DOT_MAC_TYPE_EMAIL_ENCRYPT:
			typeArg = DOT_MAC_CERT_TYPE_EMAIL_ENCRYPT;
			break;
		default:
			dotMacErrorLog("dotMacTpCertFetch: bad signType");
			return paramErr;
	}

	/* URL :=  http://certinfo.mac.com/locate?accountName&type=certTypeTag */
	rawUrlLen = strlen(DOT_MAC_LOOKUP_SCHEMA) +		/* http:// */
		strlen(DOT_MAC_LOOKUP_HOST) +				/* certmgmt.mac.com */
		strlen(DOT_MAC_LOOKUP_PATH) +				/* /locate? */
		userName.Length +							/* joe */
		strlen(DOT_MAC_LOOKUP_TYPE) +				/* &type= */
		strlen(typeArg) +							/* dmSharedServices */
		1;											/* NULL */
	unsigned char rawUrl[rawUrlLen];
	unsigned char *cp = rawUrl;
	
	unsigned len = strlen(DOT_MAC_LOOKUP_SCHEMA);
	memmove(cp, DOT_MAC_LOOKUP_SCHEMA, len);
	cp += len;
	
	len = strlen(DOT_MAC_LOOKUP_HOST);
	memmove(cp, DOT_MAC_LOOKUP_HOST, len);
	cp += len;
	
	len = strlen(DOT_MAC_LOOKUP_PATH);
	memmove(cp, DOT_MAC_LOOKUP_PATH, len);
	cp += len;
	
	memmove(cp, userName.Data, userName.Length);
	cp += userName.Length;
	
	len = strlen(DOT_MAC_LOOKUP_TYPE);
	memmove(cp, DOT_MAC_LOOKUP_TYPE, len);
	cp += len;

	len = strlen(typeArg);
	memmove(cp, typeArg, len);
	cp += len;

	*cp = '\0';		// for debugging only, actually 
	dotMacDebug("dotMacTpCertFetch: URL %s", rawUrl);
	
	CFURLRef cfUrl = CFURLCreateWithBytes(NULL,
		rawUrl, rawUrlLen - 1,		// no NULL
		kCFStringEncodingUTF8,	
		NULL);						// absolute path 
	if(cfUrl == NULL) {
		dotMacErrorLog("dotMacTpCertFetch: CFURLCreateWithBytes returned NULL\n");
		return paramErr;
	}
	/* subsequent errors to errOut: */
	
	CFHTTPMessageRef httpRequestRef = NULL;
	CFReadStreamRef httpStreamRef = NULL;
	CFNumberRef cfnTo = NULL;
	CFDictionaryRef proxyDict = NULL;
	SInt32 ito = READ_STREAM_TIMEOUT;
	CFMutableDataRef fetchedData = CFDataCreateMutable(NULL, 0);
	UInt8 readFrag[READ_FRAGMENT_SIZE];
	CFIndex bytesRead;
	CFIndex resultLen;
	
	httpRequestRef = CFHTTPMessageCreateRequest(NULL, 
		CFSTR("GET"), cfUrl, kCFHTTPVersion1_1);
	if(!httpRequestRef) {
		dotMacErrorLog("***Error creating HTTPMessage from '%s'\n", rawUrl);
		crtn = ioErr;
		goto errOut;
	}
	
	// open the stream
	httpStreamRef = CFReadStreamCreateForHTTPRequest(NULL, httpRequestRef);
	if(!httpStreamRef) {
		dotMacErrorLog("***Error creating stream for '%s'\n", rawUrl);
		crtn = ioErr;
		goto errOut;
	}

	// set a reasonable timeout
	cfnTo = CFNumberCreate(NULL, kCFNumberSInt32Type, &ito);
    if(!CFReadStreamSetProperty(httpStreamRef, _kCFStreamPropertyReadTimeout, cfnTo)) {
		// oh well - keep going 
	}
	
	// set up possible proxy info 
	proxyDict = SCDynamicStoreCopyProxies(NULL);
	if(proxyDict) {
		CFReadStreamSetProperty(httpStreamRef, kCFStreamPropertyHTTPProxy, proxyDict);
	}

	if(CFReadStreamOpen(httpStreamRef) == false) {
		dotMacErrorLog("***Error opening stream for '%s'\n", rawUrl);
		crtn = ioErr;
		goto errOut;
	}
	
	// read data from the stream
	bytesRead = CFReadStreamRead(httpStreamRef, readFrag, sizeof(readFrag)); 
	while (bytesRead > 0) {
		CFDataAppendBytes(fetchedData, readFrag, bytesRead);
		bytesRead = CFReadStreamRead(httpStreamRef, readFrag, sizeof(readFrag));
	}
	
	if (bytesRead < 0) {
		dotMacErrorLog("***Error reading URL '%s'\n", rawUrl);
		crtn = ioErr;
		goto errOut;
	}

	resultLen = CFDataGetLength(fetchedData);
	if(resultLen == 0) {
		dotMacErrorLog("***No data available from URL '%s'\n", rawUrl);
		/* but don't abort on this one - it means "no cert found" */
		goto errOut;
	}
	
	/* 
	 * Only pass back good data.
	 * FIXME this is a back to workaround nonconforming .Mac server behavior. 
	 * It currently sends HTML data that is *not* a cert when it wants to 
	 * indicate "no certs found". It should just return empty data, which 
	 * we'd detect above. For now we have to determine manually if the data
	 * contains some PEM-formated stuff.
	 */
	{
		/* Scan for PEM armour */
		bool isPEM = false;
		const char *srchStr = "-----BEGIN CERTIFICATE-----";
		unsigned srchStrLen = strlen(srchStr);
		const char *p = (const char *)CFDataGetBytePtr(fetchedData);
		if(resultLen > (int)srchStrLen) {
			/* no sense checking if result is smaller than that search string */
			unsigned srchLen = resultLen - srchStrLen;
			for(unsigned dex=0; dex< srchLen; dex++) {
				if(!strncmp(p, srchStr, srchStrLen)) {
					isPEM = true;
					break;
				}
				p++;
			}
		}
		if(isPEM) {
			result.Data = (uint8 *)alloc.malloc(resultLen);
			result.Length = resultLen;
			memmove(result.Data, CFDataGetBytePtr(fetchedData), resultLen);
		}
		else {
			result.Data = NULL;
			result.Length = 0;
		}
	}
errOut:
	CFRELEASE(cfUrl);
	CFRELEASE(httpRequestRef);
	CFRELEASE(httpStreamRef);
	CFRELEASE(cfnTo);
	CFRELEASE(proxyDict);

	return crtn;
}

