/*
	File:		 CertUI.cpp
	
	Description: stdio-based routines to get cert info from user. 

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

#include "CertUI.h"
#include <Security/x509defs.h>
#include <Security/oidsattr.h>
#include <Security/oidscert.h>
#include <Security/oidsalg.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <ctype.h>
#include <cdsaUtils/cdsaUtils.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>

void showError(
	OSStatus ortn,
	const char *errStr)
{
	printf("%s returned %d\n", errStr, (int)ortn);
}


/* 
 * Safe gets().
 * -- guaranteed no buffer overflow
 * -- guaranteed NULL-terminated string
 * -- handles empty string (i.e., response is just CR) properly
 */
void getString(
	char *buf,
	unsigned bufSize)
{
	unsigned dex;
	char c;
	char *cp = buf;
	
	for(dex=0; dex<bufSize-1; dex++) {
		c = getchar();
		if(!isprint(c)) {
			break;
		}
		switch(c) {
			case '\n':
			case '\r':
				goto done;
			default:
				*cp++ = c;
		}
	}
done:
	*cp = '\0';
}

/*
 * Prompt and safe getString.
 */
void getStringWithPrompt(
	const char *prompt,			// need not end in newline
	char *buf,
	unsigned bufSize)
{
	fpurge(stdin);
	printf("%s", prompt);
	fflush(stdout);
	getString(buf, bufSize);
}	

static const NameOidInfo nameOidInfo[MAX_NAMES] = 
{
	{ &CSSMOID_CommonName,				"Common Name      ", "www.apple.com"},
	{ &CSSMOID_CountryName,				"Country          ", "US"},
	{ &CSSMOID_OrganizationName,		"Organization     ", "Apple Computer, Inc."},
	{ &CSSMOID_OrganizationalUnitName,	"Organization Unit", "Apple Data Security"},
	{ &CSSMOID_StateProvinceName,		"State/Province   ", "California" }
};

static const char *oidToDesc(
	const CSSM_OID *oid) 
{
	unsigned dex;
	
	for(dex=0; dex<MAX_NAMES; dex++) {
		if(cuCompareCssmData(oid, nameOidInfo[dex].oid)) {
			return nameOidInfo[dex].description;
		}
	}
	printf("oidToDesc error!\n");
	exit(1);
	/* NOT REACHED */
	return NULL;
}

void getNameOids(
	CSSM_APPLE_TP_NAME_OID *subjectNames,	// size MAX_NAMES mallocd by caller
	uint32 *numNames)						// RETURNED
{
	bool ok = false;
	const NameOidInfo *nameOidIn;
	CSSM_APPLE_TP_NAME_OID *nameOidOut = subjectNames;
	unsigned dex;
	char resp[200];
	unsigned outNames;
	
	*numNames = 0;
	memset(subjectNames, 0, MAX_NAMES * sizeof(CSSM_APPLE_TP_NAME_OID));
	
	printf("\nYou will now specify the various components of the certificate's\n"
		   "Relative Distinguished Name (RDN). An RDN has a number of \n"
		   "components, all of which are optional, but at least one of \n"
		   "which must be present. \n\n"
		   "Note that if you are creating a certificate for use in an \n"
		   "SSL/TLS server, the Common Name component of the RDN must match\n"
		   "exactly the host name of the server. This must not be an IP\n"
		   "address, but the actual domain name, e.g. www.apple.com.\n\n"
		   "Entering a CR for a given RDN component results in no value for\n"
		   "that component.\n\n");
	while(!ok) {
		nameOidOut = subjectNames;
		outNames = 0;
		for(dex=0; dex<MAX_NAMES; dex++) {
			nameOidIn = &nameOidInfo[dex];
			fpurge(stdin);
			printf("%s (e.g, %s) : ", 
				nameOidIn->description, nameOidIn->example);
			fflush(stdout);
			getString(resp, sizeof(resp));
			if(resp[0] != '\0') {
				unsigned len = strlen(resp) + 1;
				nameOidOut->string = (char *)malloc(len);
				strcpy((char *)nameOidOut->string, resp);
				nameOidOut->oid = nameOidIn->oid;
				nameOidOut++;
				outNames++;
			}
		}
		if(outNames == 0) {
			printf("\nYou must enter at least one value RDN component.\n\n");
			continue;
		}
		printf("\nYou have specified:\n");
		for(dex=0; dex<outNames; dex++) {
			nameOidOut = &subjectNames[dex];
			printf("  %s : %s\n", oidToDesc(nameOidOut->oid), nameOidOut->string);
		}
		getStringWithPrompt("Is this OK (y/anything)? ", resp, sizeof(resp));
		if(resp[0] == 'y') {
			ok = true;
			break;
		}
	}
	*numNames = outNames;
}

/*
 * Free strings mallocd in getNameOids.
 */
void freeNameOids(
	CSSM_APPLE_TP_NAME_OID *subjectNames,	
	uint32 numNames)						
{
	for(unsigned i=0; i<numNames; i++) {
		if(subjectNames[i].string) {
			free((char *)subjectNames[i].string);
		}
	}
}

/* key size verifier - one for each key alg */

static bool rsaKeySizeVerify(
	unsigned keySize)
{
	if(keySize < 512) {
		return false;
	}
	if(keySize > 2048) {
		return false;
	}
	return true;
}

static bool dsaKeySizeVerify(
	unsigned keySize)
{
	return((keySize >= 512) & (keySize <= 2048));
}

static bool feeKeySizeVerify(
	unsigned keySize)
{
	switch(keySize) {
		case 128:
		case 161:
		case 192:
			return true;
		default:
			return false;
	}
}

typedef bool (*keySizeVerifyFcn)(unsigned keySize);

/* map between algorithms, string, char selector, OID */
typedef struct _AlgInfo {
	CSSM_ALGORITHMS			alg;
	char					*str;
	char					selector;
	const CSSM_OID			*oid;				// only for signatures
	uint32					defaultKeySize;		// only for keys
	char					*keyRangeString;	// only for keys
	const struct _AlgInfo	*sigAlgInfo;		// only for keys 	
	keySizeVerifyFcn		vfyFcn;		// only for keys
} AlgInfo;

/*
 * Note: CSSM_ALGID_MD2WithRSA does not work due to an inimplemented 
 * Security Server feature. Even though CSP nad CL support this, we
 * don't really want to provide this capability anyway - it's a known
 * insecure digest algorithm.
 */
static const AlgInfo rsaSigAlgInfo[] = 
{
	{ CSSM_ALGID_MD5WithRSA,  	"RSA with MD5", '5', &CSSMOID_MD5WithRSA},
//	{ CSSM_ALGID_MD2WithRSA,  	"RSA with MD2", '2', &CSSMOID_MD2WithRSA},
	{ CSSM_ALGID_SHA1WithRSA, 	"RSA with SHA1", 's', &CSSMOID_SHA1WithRSA},
	{ CSSM_ALGID_NONE, 			NULL,   0 }
};

static const AlgInfo feeSigAlgInfo[] = 
{
	{ CSSM_ALGID_FEE_MD5,  		"FEE with MD5", '5', &CSSMOID_APPLE_FEE_MD5  },
	{ CSSM_ALGID_FEE_SHA1, 		"FEE with SHA1", 's', &CSSMOID_APPLE_FEE_SHA1  },
	{ CSSM_ALGID_SHA1WithECDSA, "ECDSA/SHA1", 'e', &CSSMOID_APPLE_ECDSA },
	{ CSSM_ALGID_NONE, 			NULL,   0,  NULL }
};

static const AlgInfo dsaSigAlgInfo[] = 
{
	{ CSSM_ALGID_SHA1WithDSA, 	"DSA with SHA1", 's', &CSSMOID_APPLE_FEE_MD5  },
	{ CSSM_ALGID_NONE, 			NULL,   0,  NULL }
};

static const AlgInfo keyAlgInfo[] = 
{
	{ CSSM_ALGID_RSA, 	"RSA", 'r', NULL, 512, "512..2048", 
		rsaSigAlgInfo, rsaKeySizeVerify},
	{ CSSM_ALGID_DSA, 	"DSA", 'd', NULL, 512, "512..2048", 
		dsaSigAlgInfo, dsaKeySizeVerify},
	{ CSSM_ALGID_FEE, 	"FEE", 'f', NULL, 128, "128, 161, 192", 
		feeSigAlgInfo, feeKeySizeVerify},
	{ CSSM_ALGID_NONE, 	NULL,   0,  NULL }
};


/* map a char response to an element of an AlgInfo array */
static const AlgInfo *algInfoForSelect(
	const AlgInfo 	*algInfo,		// NULL terminated
	char			c)
{
	while(algInfo->str != NULL) {
		if(algInfo->selector == c) {
			return algInfo;
		}
		algInfo++;
	}
	/* not found */
	return NULL;
}

/* map a CSSM_ALGORITHM to an entry in keyAlgInfo[] */
static const AlgInfo *algInfoForAlg(
	CSSM_ALGORITHMS	alg)
{
	const AlgInfo *algInfo = keyAlgInfo;
	while(algInfo->str != NULL) {
		if(algInfo->alg == alg) {
			return algInfo;
		}
		algInfo++;
	}
	/* not found */
	return NULL;
}

/* get key size and algorithm for subject key */
void getKeyParams(
	CSSM_ALGORITHMS		&keyAlg,
	uint32				&keySizeInBits)
{
	char resp[200];
	const AlgInfo *keyInfo;
	const AlgInfo *tempInfo;
	
	/* get a key algorithm */
	printf("\nPlease specify parameters for the key pair you will generate.\n\n");
	while(1) {
		/* break when we get a valid key algorithm */
		tempInfo = keyAlgInfo;
		while(tempInfo->str != NULL) {
			printf("  %c  %s\n", tempInfo->selector, tempInfo->str);
			tempInfo++;
		}
		getStringWithPrompt("\nSelect key algorithm by letter: ", resp, sizeof(resp));
		if(resp[0] == '\0') {
			printf("***There is no default. Please choose a key algorithm.\n");
			continue;
		}
		keyInfo = algInfoForSelect(keyAlgInfo, resp[0]);
		if(keyInfo) {
			break;
		}
	}
	
	while(1) {
		/* until we get a valid key size */
		printf("\nValid key sizes for %s are %s; default is %u\n",
			keyInfo->str, keyInfo->keyRangeString, (unsigned)keyInfo->defaultKeySize);
		getStringWithPrompt("Enter key size in bits or CR for default: ", 
			resp, sizeof(resp));
		if(resp[0] == '\0') {
			keySizeInBits = keyInfo->defaultKeySize;
		}
		else {
			keySizeInBits = atoi(resp);
		}
		if(keyInfo->vfyFcn(keySizeInBits)) {
			printf("\nYou have selected algorithm %s, key size %u bits.\n",
				keyInfo->str, (unsigned)keySizeInBits);
			getStringWithPrompt("OK (y/anything)? ", resp, sizeof(resp));
			if(resp[0] == 'y') {
				break;
			}
		}
		else {
			printf("***%u is not a legal key size for algorithm %s.\n",
				(unsigned)keySizeInBits, keyInfo->str);
		}
	}
	keyAlg = keyInfo->alg;
}

/* given a signing key, obtain signing algorithm (int and oid format) */
OSStatus getSigAlg(
	const CSSM_KEY	*signingKey,
	CSSM_ALGORITHMS	&sigAlg,
	const CSSM_OID * &sigOid)
{
	char resp[200];
	const AlgInfo *keyInfo;
	const AlgInfo *tempInfo;
	const AlgInfo *sigInfoArray;
	const AlgInfo *sigInfo;

	keyInfo = algInfoForAlg(signingKey->KeyHeader.AlgorithmId);
	if(keyInfo == NULL) {
		printf("***Signing key has unknown algorithm (%u).\n", 
			(unsigned)signingKey->KeyHeader.AlgorithmId);
		return paramErr;
	}
	sigInfoArray = keyInfo->sigAlgInfo;
	printf("\nPlease specify the algorithm with which your certificate will be "
		"signed.\n\n");
	while(1) {
		/* break when we get a valid sig algorithm */
		tempInfo = sigInfoArray;
		while(tempInfo->str != NULL) {
			printf("  %c  %s\n", tempInfo->selector, tempInfo->str);
			tempInfo++;
		}
		getStringWithPrompt("\nSelect signature algorithm by letter: ", 
			resp, sizeof(resp));
		if(resp[0] == '\0') {
			printf("***There is no default. Please choose a signature algorithm.\n");
			continue;
		}
		sigInfo = algInfoForSelect(sigInfoArray, resp[0]);
		if(sigInfo == NULL) {
			printf("Try again.\n");
			continue;
		}
		printf("\nYou have selected algorithm %s.\n", sigInfo->str);
		getStringWithPrompt("OK (y/anything)? ", resp, sizeof(resp));
		if(resp[0] == 'y') {
			break;
		}
	}
	sigAlg = sigInfo->alg;
	sigOid = sigInfo->oid;
	return noErr;
}

CU_KeyUsage getKeyUsage(bool isRoot)
{
	char resp[200];
	char *prompt;
	
	if(isRoot) {
		/* root HAS to be capable of signing */
		prompt = "Enter cert/key usage (s=signing, b=signing AND encrypting): ";
	}
	else {
		prompt = "Enter cert/key usage (s=signing, e=encrypting, b=both): ";
	}
	while(1) {
		getStringWithPrompt(prompt, resp, sizeof(resp));
		switch(resp[0]) {
			case 's':
				return kKeyUseSigning;
			case 'e':
				if(isRoot) {
					continue;
				}
				return kKeyUseEncrypting;
			case 'b':
				return kKeyUseSigning | kKeyUseEncrypting;
				
		}
	}
}


