/*
	File:		 pem.h 
	
	Description: PEM encode/decode routines

	Author:		dmitch

	Copyright: 	© Copyright 2002 Apple Computer, Inc. All rights reserved.
	
	Disclaimer:	IMPORTANT:  This Apple software is supplied to you by Apple 
	            Computer, Inc. ("Apple") in consideration of your agreement to 
				the following terms, and your use, installation, modification 
				or redistribution of this Apple software constitutes acceptance 
				of these terms.  If you do not agree with these terms, please 
				do not use, install, modify or redistribute this Apple software.

				In consideration of your agreement to abide by the following 
				terms, and subject to these terms, Apple grants you a personal, 
				non-exclusive license, under Apple's copyrights in this 
				original Apple software (the "Apple Software"), to use, 
				reproduce, modify and redistribute the Apple Software, with 
				or without modifications, in source and/or binary forms; 
				provided that if you redistribute the Apple Software in 
				its entirety and without modifications, you must retain
				this notice and the following text and disclaimers in all 
				such redistributions of the Apple Software.  Neither the 
				name, trademarks, service marks or logos of Apple Computer, 
				Inc. may be used to endorse or promote products derived from the
				Apple Software without specific prior written permission from 
				Apple.  Except as expressly stated in this notice, no other 
				rights or licenses, express or implied, are granted by Apple 
				herein, including but not limited to any patent rights that
				may be infringed by your derivative works or by other works 
				in which the Apple Software may be incorporated.

				The Apple Software is provided by Apple on an "AS IS" basis.  
				APPLE MAKES NO WARRANTIES, EXPRESS OR IMPLIED, INCLUDING 
				WITHOUT LIMITATION THE IMPLIED WARRANTIES OF NON-INFRINGEMENT,
				MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, 
				REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION ALONE 
				OR IN COMBINATION WITH YOUR PRODUCTS.

				IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, 
				INCIDENTAL OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
				LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
				LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
				ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION, MODIFICATION 
				AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED 
				AND WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING 
				NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE 
				HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "pem.h"
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include "cuEnc64.h"

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
	enc = enc64WithLines(inData, inDataLen, 64, &encLen);
	if(enc == NULL) {
		/* malloc error is actually the only known failure */
		printf("***pemEncode: Error encoding file. Aborting.\n");
		return -1;
	}
		
	/* estimate outsize - just be sloppy, way conservative */
	unsigned outSize = encLen + (2 * strlen(headerString)) + 200;
	*outData = (unsigned char *)malloc(outSize);
	sprintf((char *)*outData, "-----BEGIN %s-----\n%s-----END %s-----\n",
		headerString, (char *)enc, headerString);
	*outDataLen = strlen((char *)*outData);

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
	int freeCp = 0;
	char *curr1, *curr2;
	char *startPem = NULL;
	char *endPem = NULL;
	unsigned char *out;
	unsigned outLen;
	int ourRtn = 0;

	/* make the whole thing a NULL-terminated string */
	if(inData[inDataLen - 1] != '\0') {
		cp = (char *)malloc(inDataLen + 1);
		memmove(cp, inData, inDataLen);
		cp[inDataLen] = '\0';
		inDataLen++;
		freeCp = 1;
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
	
	out = dec64((unsigned char *)startPem, endPem-startPem, &outLen);
	if(out == NULL) {
		printf("Bad PEM format (3)\n");
		ourRtn = -1;
		goto abort;
	}
	*outData = out;
	*outDataLen = outLen;
abort:
	if(freeCp) {
		free(cp);
	}
	return ourRtn;
}

