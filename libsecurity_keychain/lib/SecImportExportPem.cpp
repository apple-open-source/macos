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
 *
 * SecImportExportPem.cpp - private PEM routines for SecImportExport
 */

#include "SecImportExportPem.h"
#include "SecExternalRep.h"
#include "SecImportExportUtils.h"
#include <security_cdsa_utils/cuEnc64.h>
#include <security_cdsa_utils/cuPem.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* 
 * Text parsing routines. 
 *
 * Search incoming text for specified string. Does not assume inText is
 * NULL terminated. Returns pointer to start of found string in inText.
 */
static const char *findStr(
	const char *inText,
	unsigned inTextLen,
	const char *str)				// NULL terminated - search for this
{
	/* probably not the hottest string search algorithm... */
	const char *cp;
	unsigned srchStrLen = strlen(str);
	char c = str[0];
	
	/* last char * we can search in inText for start of str */
	const char *endCp = inText + inTextLen - srchStrLen;
	
	for(cp=inText; cp<=endCp; cp++) {
		if(*cp == c) {
			if(!memcmp(cp, str, srchStrLen)) {
				return cp;
			}
		}
	}
	return NULL;
}

/*
 * Obtain one line from current text. Returns a mallocd, NULL-terminated string
 * which caller must free(). Also returns the number of chars consumed including
 * the returned chars PLUS EOL terminators (\n and/or \r).
 *
 * ALWAYS returns a mallocd string if there is ANY data remaining per the 
 * incoming inTextLen. Returns NULL if inTextLen is zero.
 */
static const char *getLine(
	const char *inText,
	unsigned inTextLen,			// RETURNED
	unsigned *consumed)			// RETURNED
	
{
	*consumed = 0;
	const char *cp = inText;
	const char *newline = NULL;		// if we found a newline, this points to the first one
	
	while(inTextLen) {
		char c = *cp;
		if((c == '\r') || (c == '\n')) {
			if(newline == NULL) {
				/* first newline */
				newline = cp;
			}
		}
		else if(newline != NULL) {
			/* non newline after newline, done */
			break;
		}
		(*consumed)++;
		inTextLen--;
		cp++;
	}
	unsigned linelen;
	if(newline) {
		linelen = newline - inText;
	}
	else {
		linelen = *consumed;
	}
	char *rtn = (char *)malloc(linelen + 1);
	memmove(rtn, inText, linelen);
	rtn[linelen] = 0;
	return rtn;
}

/*
 * Table to facilitate conversion of known PEM header strings to 
 * the things we know about.
 */
typedef struct {
	const char			*pemStr;			// e.g. PEM_STRING_X509, "CERTIFICATE"
	SecExternalItemType itemType;
	SecExternalFormat   format;
	CSSM_ALGORITHMS		keyAlg;
} PemHeader;

#define NOALG   CSSM_ALGID_NONE

static const PemHeader PemHeaders[] = 
{
	/* from openssl/pem.h standard header */
	{ PEM_STRING_X509_OLD, kSecItemTypeCertificate, kSecFormatX509Cert, NOALG},
	{ PEM_STRING_X509,  kSecItemTypeCertificate, kSecFormatX509Cert, NOALG },
	{ PEM_STRING_EVP_PKEY, kSecItemTypePrivateKey, kSecFormatOpenSSL, NOALG},
	{ PEM_STRING_PUBLIC, kSecItemTypePublicKey, kSecFormatOpenSSL, NOALG },
	{ PEM_STRING_RSA, kSecItemTypePrivateKey, kSecFormatOpenSSL, CSSM_ALGID_RSA },
	{ PEM_STRING_RSA_PUBLIC, kSecItemTypePublicKey, kSecFormatOpenSSL, CSSM_ALGID_RSA },
	{ PEM_STRING_DSA, kSecItemTypePrivateKey, kSecFormatOpenSSL, CSSM_ALGID_DSA },
	{ PEM_STRING_DSA_PUBLIC, kSecItemTypePublicKey, kSecFormatOpenSSL, CSSM_ALGID_DSA },
	{ PEM_STRING_PKCS7, kSecItemTypeAggregate, kSecFormatPKCS7, NOALG },
	{ PEM_STRING_PKCS8, kSecItemTypePrivateKey, kSecFormatWrappedPKCS8, NOALG },
	{ PEM_STRING_PKCS8INF, kSecItemTypePrivateKey, kSecFormatUnknown, NOALG },
	/* we define these  */
	{ PEM_STRING_DH_PUBLIC, kSecItemTypePublicKey, kSecFormatOpenSSL, CSSM_ALGID_DH },
	{ PEM_STRING_DH_PRIVATE, kSecItemTypePrivateKey, kSecFormatOpenSSL, CSSM_ALGID_DH },
	{ PEM_STRING_PKCS12, kSecItemTypeAggregate, kSecFormatPKCS12, NOALG },
	{ PEM_STRING_SESSION, kSecItemTypeSessionKey, kSecFormatRawKey, NOALG },
	{ PEM_STRING_ECDSA_PUBLIC, kSecItemTypePublicKey, kSecFormatOpenSSL, CSSM_ALGID_ECDSA },
	{ PEM_STRING_ECDSA_PRIVATE, kSecItemTypePrivateKey, kSecFormatOpenSSL, CSSM_ALGID_ECDSA }
};
#define NUM_PEM_HEADERS (sizeof(PemHeaders) / sizeof(PemHeader))

/*
 * PEM decode incoming data which we've previously determined to contain
 * exactly one reasonably well formed PEM blob (it has no more than one
 * START and END line - though it may have none - and is all ASCII).
 *
 * Returned SecImportRep may or may not have a known type and format and 
 * (if it is a key) algorithm. 
 */
static OSStatus impExpImportSinglePEM(
	const char			*currCp,
	unsigned			lenToGo,
	CFMutableArrayRef	importReps)		// output appended here
{
	unsigned consumed;
	const char *currLine = NULL;		// mallocd by getLine()
	CFMutableArrayRef pemParamLines = NULL;
	OSStatus ortn = noErr;
	CFDataRef cdata = NULL;
	Security::KeychainCore::SecImportRep *rep = NULL;
	const char *start64;
	unsigned base64Len;	
	const char *end64;
	unsigned char *decData;
	unsigned decDataLen;
	
	/* we try to glean these from the header, but it's not fatal if we can not */
	SecExternalFormat format = kSecFormatUnknown;
	SecExternalItemType itemType = kSecItemTypeUnknown;
	CSSM_ALGORITHMS keyAlg = CSSM_ALGID_NONE;
	
	/* search to START line, parse it to get type/format/alg */
	const char *startLine = findStr(currCp, lenToGo, "-----BEGIN");
	if(startLine != NULL) {
		/* possibly skip over leading garbage */
		consumed = startLine - currCp;
		lenToGo -= consumed;
		currCp = startLine;
		
		/* get C string of START line */
		currLine = getLine(startLine, lenToGo, &consumed);
		if(currLine == NULL) {
			/* somehow got here with no data */
			assert(lenToGo == 0);
			SecImpInferDbg("impExpImportSinglePEM empty data");
			ortn = errSecUnsupportedFormat;
			goto errOut;
		}
		assert(consumed <= lenToGo);
		currCp  += consumed;
		lenToGo -= consumed;

		/*
		 * Search currLine for known PEM header strings.
		 * It is not an error if we don't recognize this
		 * header.
		 */
		for(unsigned dex=0; dex<NUM_PEM_HEADERS; dex++) {
			const PemHeader *ph = &PemHeaders[dex];
			if(!strstr(currLine, ph->pemStr)) {
				continue;
			}
			/* found one! */
			format   = ph->format;
			itemType = ph->itemType;
			keyAlg   = ph->keyAlg;
			break;
		}
		
		free((void *)currLine);
	}

	/* 
	 * Skip empty lines. Save all lines containing ':' (used by openssl 
	 * to specify key wrapping parameters). These will be saved in 
	 * outgoing SecImportReps' pemParamLines.
	 */
	for( ; ; ) {
		currLine = getLine(currCp, lenToGo, &consumed);
		if(currLine == NULL) {
			/* out of data */
			SecImpInferDbg("impExpImportSinglePEM out of data after START line");
			ortn = errSecUnsupportedFormat;
			goto errOut;
		}
		bool skipThis = false;
		unsigned lineLen = strlen(currLine);
		if(lineLen == 0) {
			/* empty line */
			skipThis = true;
		}
		if(strchr(currLine, ':')) {
			/* 
			 * Save this PEM header info. Used for traditional openssl
			 * wrapped keys to indicate IV.
			 */
			SecImpInferDbg("import PEM: param line %s", currLine);
			CFStringRef cfStr = CFStringCreateWithCString(NULL, currLine,
				kCFStringEncodingASCII);
			if(pemParamLines == NULL) {
				/* first param line */
				pemParamLines = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
				
				/* 
				 * If it says "ENCRYPTED" and this is a private key,
				 * flag the fact that it's wrapped in openssl format
				 */
				if(strstr(currLine, "ENCRYPTED")) {
					if((format == kSecFormatOpenSSL) &&
					   (itemType == kSecItemTypePrivateKey)) {
						format = kSecFormatWrappedOpenSSL;
					}
				}
			}
			CFArrayAppendValue(pemParamLines, cfStr);
			CFRelease(cfStr);		// array owns it 
			skipThis = true;
		}
		free((void *)currLine);
		if(!skipThis) {
			/* looks like good stuff; procees */
			break;
		}
		/* skip this line */
		assert(consumed <= lenToGo);
		currCp  += consumed;
		lenToGo -= consumed;
	}
	if(lenToGo == 0) {
		SecImpInferDbg("impExpImportSinglePEM no valid base64 data");
		ortn = errSecUnsupportedFormat;
		goto errOut;
	}

	/* 
	 * currCP points to start of base64 data - mark it and search for end line.
	 * We skip everything after the end line.
	 */
	start64 = currCp;
	base64Len = lenToGo;			// if no END
	end64 = findStr(currCp, lenToGo, "-----END");
	if(end64 != NULL) {
		if(end64 == start64) {
			/* Empty, nothing between START and END */
			SecImpInferDbg("impExpImportSinglePEM no base64 between terminators");
			ortn = errSecUnsupportedFormat;
			goto errOut;
		}
		base64Len = end64 - start64;
	}
	/* else no END, no reason to complain about that as long as base64 decode works OK */
	
	/* Base 64 decode */
	decData = cuDec64((const unsigned char *)start64, base64Len, &decDataLen);
	if(decData == NULL) {
		SecImpInferDbg("impExpImportSinglePEM bad base64 data");
		ortn = errSecUnsupportedFormat;
		goto errOut;
	}
	
	cdata = CFDataCreate(NULL, decData, decDataLen);
	free((void *)decData);
	rep = new Security::KeychainCore::SecImportRep(cdata, itemType, format, keyAlg,
		pemParamLines);
	CFArrayAppendValue(importReps, rep);
	CFRelease(cdata);		// SecImportRep holds ref
	return noErr;
	
errOut:
	if(pemParamLines != NULL) {
		CFRelease(pemParamLines);
	}
	return ortn;
}

/*
 * PEM decode incoming data, appending SecImportRep's to specified array.
 * Returned SecImportReps may or may not have a known type and format and 
 * (if they are keys) algorithm. 
 */
OSStatus impExpParsePemToImportRefs(
	CFDataRef			importedData,
	CFMutableArrayRef	importReps,		// output appended here
	bool				*isPem)			// true means we think it was PEM regardless of 
										// final return code	
{
	/*
	 * First task: is this PEM or at least base64 encoded?
	 */
	const char *currCp = (const char *)CFDataGetBytePtr(importedData);
	const char *cp = currCp;
	unsigned lenToGo = CFDataGetLength(importedData);
	OSStatus ortn;
	
	*isPem = false;
	bool isAscii = true;
	for(unsigned dex=0; dex<lenToGo; dex++, cp++) {
		if(!isprint(*cp) && !isspace(*cp)) {
			isAscii = false;
			break;
		}
	}
	if(!isAscii) {
		return noErr;
	}

	/* search for START line */
	const char *startLine = findStr(currCp, lenToGo, "-----BEGIN");
	if(startLine == NULL) {
		/* Assume one item, raw base64 */
		SecImpInferDbg("impExpParsePemToImportRefs no PEM headers, assuming raw base64");
		ortn = impExpImportSinglePEM(currCp, lenToGo, importReps);
		if(ortn == noErr) {
			*isPem = true;
		}
		return ortn;
	}

	/* break up input into chunks between START and END lines */
	ortn = noErr;
	bool gotSomePem = false;
	do {
		/* get to beginning of START line */
		startLine = findStr(currCp, lenToGo, "-----BEGIN");
		if(startLine == NULL) {
			break;
		}
		unsigned consumed = startLine - currCp;
		assert(consumed <= lenToGo);
		lenToGo -= consumed;
		currCp  += consumed;
		
		/* get to beginning of END line */
		const char *endLine = findStr(currCp+10, lenToGo, "-----END");
		unsigned toDecode = lenToGo;
		if(endLine) {
			consumed = endLine - startLine;
			assert(consumed <= lenToGo);
			currCp  += consumed;
			lenToGo -= consumed;
			
			/* find end of END line */
			const char *tmpLine = getLine(endLine, lenToGo, &consumed);
			assert((tmpLine != NULL) && (tmpLine[0] != 0));
			/* don't decode the terminators */
			toDecode = endLine - startLine + strlen(tmpLine);
			free((void *)tmpLine);
			
			/* skip past END line and newlines */
			assert(consumed <= lenToGo);
			currCp  += consumed;
			lenToGo -= consumed;
		}
		else {
			/* no END line, we'll allow that - decode to end of file */
			lenToGo = 0;
		}
		
		ortn = impExpImportSinglePEM(startLine, toDecode, importReps);
		if(ortn) {
			break;
		}
		gotSomePem = true;
	} while(lenToGo != 0);
	if(ortn == noErr) {
		if(gotSomePem) {
			*isPem = true;
		}
		else {
			SecImpInferDbg("impExpParsePemToImportRefs empty at EOF, no PEM found");
			ortn = kSecFormatUnknown;
		}
	}
	return ortn;
}
	

/*
 * PEM encode a single SecExportRep's data, appending to a CFData.
 */
OSStatus impExpPemEncodeExportRep(
	CFDataRef			derData,
	const char			*pemHeader,
	CFArrayRef			pemParamLines,  // optional 
	CFMutableDataRef	outData)
{
	unsigned char *enc;
	unsigned encLen;
	
	char headerLine[200];
	if(strlen(pemHeader) > 150) {
		return paramErr;
	}

	/* First base64 encode */
	enc = cuEnc64WithLines(CFDataGetBytePtr(derData), CFDataGetLength(derData), 
		64, &encLen);
	if(enc == NULL) {
		/* malloc error is actually the only known failure */
		SecImpExpDbg("impExpPemEncodeExportRep: cuEnc64WithLines failure");
		return memFullErr;
	}
	
	/* strip off trailing NULL */
	if((encLen != 0) && (enc[encLen - 1] == '\0')) {
		encLen--;
	}
	sprintf(headerLine, "-----BEGIN %s-----\n", pemHeader);
	CFDataAppendBytes(outData, (const UInt8 *)headerLine, strlen(headerLine));
	
	/* optional PEM parameters lines (currently used for openssl wrap format only) */
	if(pemParamLines != NULL) {
		CFIndex numLines = CFArrayGetCount(pemParamLines);
		for(CFIndex dex=0; dex<numLines; dex++) {
			CFStringRef cfStr = 
				(CFStringRef)CFArrayGetValueAtIndex(pemParamLines, dex);
			char cStr[512];
			UInt8 nl = '\n';
			if(!CFStringGetCString(cfStr, cStr, sizeof(cStr), 
					kCFStringEncodingASCII)) {
				/* 
				 * Should never happen; this module created this CFString
				 * from a C string with ASCII encoding. Keep going, though
				 * this is probably fatal to the exported representation.
				 */
				SecImpExpDbg("impExpPemEncodeExportRep: pemParamLine screwup");
				continue; 
			}
			CFDataAppendBytes(outData, (const UInt8 *)cStr, strlen(cStr));
			CFDataAppendBytes(outData, &nl, 1);
		}
	}
	CFDataAppendBytes(outData, enc, encLen);
	sprintf(headerLine, "-----END %s-----\n", pemHeader);
	CFDataAppendBytes(outData, (const UInt8 *)headerLine, strlen(headerLine));
	free((void *)enc);
	return noErr;
}
	
