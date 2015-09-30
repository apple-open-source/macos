/*
 * Copyright (c) 2003,2011-2012,2014 Apple Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please 
 * obtain a copy of the License at http://www.apple.com/publicsource and 
 * read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER 
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, 
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. 
 * Please see the License for the specific language governing rights and 
 * limitations under the License.
 */
 
/*
	File:		 cuPem.h 
	
	Description: PEM encode/decode routines

	Author:		 dmitch

*/

#include "cuPem.h"
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <ctype.h>
#include "cuEnc64.h"

#define PEM_SCAN_LEN		8192

/*
 * Determine if specified blob appears to be PEM format.
 * Returns 1 if so, 0 if not.
 */
int isPem(
	const unsigned char 	*inData,
	unsigned 				inDataLen)
{
	/*
	 * 1. The entire blob must be printable ASCII.
	 */
	const unsigned char *cp = inData;
	for(unsigned dex=0; dex<inDataLen; dex++, cp++) {
		if(!isprint(*cp) && !isspace(*cp)) {
			return 0;
		}
	}
	
	/*
	 * Search for "-----BEGIN " and "-----END". 
	 * No strnstr() on X, so copy and NULL terminate to use strstr.
	 * First, get the first PEM_SCAN_LEN chars or inDataLen, whichever
	 * is less.
	 */
	unsigned char buf[PEM_SCAN_LEN + 1];
	unsigned len = inDataLen;
	if(len > PEM_SCAN_LEN) {
		len = PEM_SCAN_LEN;
	}
	memcpy(buf, inData, len);
	buf[len] = '\0';
	const char *p = strstr((const char *)buf, "-----BEGIN ");
	if(p == NULL) {
		return 0;
	}
	
	/*
	 * Now the last PEM_SCAN_LEN chars or inDataLen, whichever is less.
	 */
	if(inDataLen > PEM_SCAN_LEN) {
		memcpy(buf, inData + inDataLen - PEM_SCAN_LEN, PEM_SCAN_LEN);
		buf[PEM_SCAN_LEN] = '\0';
	}
	/* else we already have whole blob in buf[] */
	p = strstr((const char *)buf, "-----END ");
	if(p == NULL) {
		return 0;
	}
	/* success */
	return 1;
}

int pemEncode(
	const unsigned char 	*inData,
	unsigned 				inDataLen,
	unsigned char 			**outData,
	unsigned 				*outDataLen,
	const char 				*headerString)
{
	unsigned char *enc;
	unsigned encLen;
	
	/* First base64 encode */
	enc = cuEnc64WithLines(inData, inDataLen, 64, &encLen);
	if(enc == NULL) {
		/* malloc error is actually the only known failure */
		printf("***pemEncode: Error encoding file. Aborting.\n");
		return -1;
	}
		
	/* estimate outsize - just be sloppy, way conservative */
	size_t outSize = encLen + (2 * strlen(headerString)) + 200;
	*outData = (unsigned char *)malloc(outSize);
	sprintf((char *)*outData, "-----BEGIN %s-----\n%s-----END %s-----\n",
		headerString, (char *)enc, headerString);
	*outDataLen = (unsigned int)strlen((char *)*outData);

	if((*outData)[*outDataLen - 1] == '\0') {
		(*outDataLen)--;
	}
	free(enc);
	return 0;
}

int pemDecode(
	const unsigned char 	*inData,
	unsigned 				inDataLen,
	unsigned char 			**outData,
	unsigned 				*outDataLen)
{
	char *cp;
	char *curr1, *curr2;
	char *startPem = NULL;
	char *endPem = NULL;
	unsigned char *out;
	unsigned outLen;
	int ourRtn = 0;
	char *freeCp = NULL;

	/* make the whole thing a NULL-terminated string */
	if(inData[inDataLen - 1] != '\0') {
		cp = freeCp = (char *)malloc(inDataLen + 1);
		memmove(cp, inData, inDataLen);
		cp[inDataLen] = '\0';
		inDataLen++;
	}
	else {
		/* already is */
		cp = (char *)inData;
	}
	
	/* cp is start of NULL-terminated buffer, size inDataLen */
	/* skip over everything until "-----" */
	curr1 = strstr(cp, "-----");
	if(curr1 == NULL) {
		printf("***pemDecode: no terminator found\n");
		ourRtn = -1;
		goto abort;
	}
	
	/* find end of separator line, handling both flavors of terminator */
	cp = curr1;
	curr1 = strchr(cp, '\n');
	curr2 = strchr(cp, '\r');
	if((curr1 == NULL) & (curr2 == NULL)) {
		printf("***pemDecode: Bad PEM format (1)\n");
		ourRtn = -1;
		goto abort;
	}
	if(curr1 == NULL) {
		startPem = curr2;
	}
	else {
		startPem = curr1;
	}
	
	/* startPem points to end of separator line */
	/* locate ending terminator and lop it off */
	curr1 = strstr(startPem, "-----");
	if(curr1 == NULL) {
		printf("***pemDecode: Bad PEM format (2)\n");
		ourRtn = -1;
		goto abort;
	}
	endPem = curr1;
	/* endPem points to last PEM data plus one */
	
	out = cuDec64((unsigned char *)startPem, (unsigned int)(endPem-startPem), &outLen);
	if(out == NULL) {
		printf("Bad PEM format (3)\n");
		ourRtn = -1;
		goto abort;
	}
	*outData = out;
	*outDataLen = outLen;
abort:
	if(freeCp) {
		free(freeCp);
	}
	return ourRtn;
}

