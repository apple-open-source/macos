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
	DotMacSignType				signType,
	SecNssCoder					&coder,		// results mallocd in this address space
	CSSM_DATA					&refId)		// RETURNED, PEM encoded
{
	DotMacTpPendingRequest req;
	
	/* set up a DotMacTpPendingRequest */
	req.userName = userName;
	uint8 reqType;
	switch(signType) {
		case DMST_Identity:
			reqType = RT_Identity;
			break;
		case DMST_EmailSigning:
			reqType = RT_EmailSign;
			break;
		case DMST_EmailEncrypting:
			reqType = RT_EmailEncrypt;
			break;
		default:
			assert(0);
			dotMacErrorLog("dotMacEncodeRefId: bad signType");
			return internalComponentErr;
	}
	req.reqType.Data = &reqType;
	req.reqType.Length = 1;
	
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
	SecNssCoder					&coder,		// results mallocd in this address space
	const CSSM_DATA				&refId,		// PEM encoded
	CSSM_DATA					&userName,	// RETURNED, UTF8, no NULL
	DotMacSignType				*signType)  // RETURNED
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
	if(req.reqType.Length != 1) {
		dotMacErrorLog("dotMacDecodeRefId: reqType length (%lu) error", req.reqType.Length);
		return paramErr;
	}
	switch(req.reqType.Data[0]) {
		case RT_Identity:
			*signType = DMST_Identity;
			break;
		case RT_EmailSign:
			*signType = DMST_EmailSigning;
			break;
		case RT_EmailEncrypt:
			*signType = DMST_EmailEncrypting;
			break;
		default:
			dotMacErrorLog("dotMacDecodeRefId: bad reqType");
			return paramErr;
	}
	return noErr;
}

/* fetch cert via HTTP */
CSSM_RETURN dotMacTpCertFetch(
	const CSSM_DATA		&userName,  // UTF8, no NULL
	DotMacSignType		signType,
	Allocator			&alloc,		// results mallocd here
	CSSM_DATA			&result)	// RETURNED
{
	unsigned char *rawUrl;
	unsigned rawUrlLen;
	char *path;
	CSSM_RETURN crtn = CSSM_OK;
	
	switch(signType) {
		case DMST_Identity:
			path = DOT_MAC_LOOKUP_ID_PATH;
			break;
		case DMST_EmailSigning:
			path = DOT_MAC_LOOKUP_SIGN_PATH;
			break;
		case DMST_EmailEncrypting:
			path = DOT_MAC_LOOKUP_ENCRYPT_PATH;
			break;
		default:
			dotMacErrorLog("dotMacTpCertFetch: bad signType");
			return paramErr;
	}

	rawUrlLen = strlen(DOT_MAC_LOOKUP_SCHEMA) +		/* http:// */
		strlen(DOT_MAC_LOOKUP_HOST) +				/* certmgmt.mac.com */
		strlen(path) +								/* /lookup/identity? */
		userName.Length +							/* joe */
		1;											/* NULL */
	rawUrl = (unsigned char *)malloc(rawUrlLen);
	if(rawUrl == NULL) {
		return memFullErr;
	}
	unsigned char *cp = rawUrl;
	
	unsigned len = strlen(DOT_MAC_LOOKUP_SCHEMA);
	memmove(cp, DOT_MAC_LOOKUP_SCHEMA, len);
	cp += len;
	
	len = strlen(DOT_MAC_LOOKUP_HOST);
	memmove(cp, DOT_MAC_LOOKUP_HOST, len);
	cp += len;
	
	len = strlen(path);
	memmove(cp, path, len);
	cp += len;
	
	memmove(cp, userName.Data, userName.Length);
	cp += userName.Length;
	
	*cp = '\0';		// for debugging only, actually 
	dotMacDebug("dotMacTpCertFetch: URL %s", rawUrl);
	
	CFURLRef cfUrl = CFURLCreateWithBytes(NULL,
		rawUrl, rawUrlLen - 1,		// no NULL
		kCFStringEncodingUTF8,	
		NULL);						// absolute path 
	free(rawUrl);
	if(cfUrl == NULL) {
		dotMacErrorLog("dotMacTpCertFetch: CFURLCreateWithBytes returned NULL\n");
		return paramErr;
	}
	CFDataRef urlData = NULL;
	SInt32 errorCode = 0;
	
	/* Make sure we can see the HTTP status line */
	CFStringRef statKey = kCFURLHTTPStatusCode;
	CFArrayRef desiredValues = CFArrayCreate(NULL, (const void **)&statKey, 1, NULL);
	CFDictionaryRef dict = NULL;
	Boolean brtn = CFURLCreateDataAndPropertiesFromResource(NULL,
		cfUrl,
		&urlData, 
		&dict,			// properties
		desiredValues,  
		&errorCode);
	CFRelease(cfUrl);
	CFRelease(desiredValues);
	if(!brtn || (errorCode != 0)) {
		dotMacErrorLog("dotMacTpCertFetch: CFURLCreateDataAndPropertiesFromResource "
				"err: %d\n", (int)errorCode);
		if(urlData) {
			CFRelease(urlData);
		}
		return CSSMERR_TP_INVALID_NETWORK_ADDR;
	}
	if(urlData == NULL) {
		dotMacErrorLog("dotMacTpCertFetch: CFURLCreateDataAndPropertiesFromResource:"
			"no data\n");
		return CSSMERR_TP_INVALID_NETWORK_ADDR;
	}
	if(dict == NULL) {
		dotMacErrorLog("dotMacTpCertFetch: CFURLCreateDataAndPropertiesFromResource:"
			"no properties\n");
		return CSSMERR_TP_INVALID_NETWORK_ADDR;
	}
	CFNumberRef cfStatNum = 
		(CFNumberRef)CFDictionaryGetValue(dict, kCFURLHTTPStatusCode);
	if(cfStatNum == NULL) {
		dotMacErrorLog("dotMacTpCertFetch: no HTTP status\n");
		return CSSMERR_TP_INVALID_NETWORK_ADDR;
	}
	long statNum;
	if(!CFNumberGetValue(cfStatNum, kCFNumberLongType, &statNum)) {
		dotMacErrorLog("dotMacTpCertFetch: error converting HTTP status\n");
		/* but keep going */
	}
	else {
		if(statNum != 200) {
			dotMacErrorLog("dotMacTpCertFetch: HTTP status %ld\n", statNum);
			crtn = dotMacHttpStatToOs(statNum);
		}
	}
	CFIndex resultLen = CFDataGetLength(urlData);
	
	/* Only pass back good data */
	if(crtn == CSSM_OK) {
		/* Scan for PEM armour */
		bool isPEM = false;
		const char *s = "-----BEGIN CERTIFICATE-----";
		uint8 *p = (uint8 *)CFDataGetBytePtr(urlData);
		uint32 l = resultLen;
		uint32 m = strlen(s);
		while(l > m) {
			if (*p == 0x2D && !strncmp((const char*)p, s, m)) {
				isPEM = true;
				break;
			}
			p++;
			l--;
		}
		if(isPEM) {
			result.Data = (uint8 *)alloc.malloc(l);
			result.Length = l;
			memmove(result.Data, p, l);
		}
		else {
			result.Data = NULL;
			result.Length = 0;
		}
	#if 0
		/* Dump raw and filtered data to temporary files for debugging */
		FILE *rawFile = fopen("/tmp/debug_dotmac_data", "w");\
		if (resultLen)
			fwrite(CFDataGetBytePtr(urlData), 1, resultLen, rawFile);
		fclose(rawFile);
		FILE *dbgFile = fopen("/tmp/debug_dotmac_result", "w");
		if (result.Length)
			fwrite(result.Data, 1, result.Length, dbgFile);
		fclose(dbgFile);
	#endif
	}
	CFRelease(urlData);
	if(dict) {
		CFRelease(dict);
	}
	return crtn;
}

