/*
	File:		 cryptTool.c
	
	Description: simple encrypt/decrypt utility to demonstrate CDSA API
                 used for symmetric encryption

	Author:		dmitch

	Copyright: 	Copyright (c) 2001,2003,2005-2006 Apple Computer, Inc. All Rights Reserved.
	
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

#include "common.h"
#include <security_cdsa_utils/cuFileIo.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void usage(char **argv) 
{
	printf("usage:\n");
	printf("  %s op password keySize inFile outFile [a=algorithm]\n", argv[0]);
	printf("  op:\n");
	printf("     e  encrypt\n");
	printf("     d  decrypt\n");
	printf("  algorithm:\n");
	printf("     4  RC4 (default if no algorithm specified)\n");
	printf("     c  ASC/ComCryption\n");
	printf("     d  DES\n");
	printf("     a  AES\n");
	exit(1);
}

/*
 * Derive symmetric key.
 */
static CSSM_RETURN ctDeriveKey(CSSM_CSP_HANDLE cspHand,
		uint32				keyAlg,			// CSSM_ALGID_RC5, etc.
		const char 			*keyLabel,
		unsigned 			keyLabelLen,
		uint32 				keyUsage,		// CSSM_KEYUSE_ENCRYPT, etc.
		uint32 				keySizeInBits,
		CSSM_DATA_PTR		password,		// in PKCS-5 lingo
		CSSM_DATA_PTR		salt,			// ditto
		uint32				iterationCnt,	// ditto
		CSSM_KEY_PTR		key)
{
	CSSM_RETURN					crtn;
	CSSM_CC_HANDLE 				ccHand;
	uint32						keyAttr;
	CSSM_DATA					dummyLabel;
	CSSM_PKCS5_PBKDF2_PARAMS 	pbeParams;
	CSSM_DATA					pbeData;
	CSSM_ACCESS_CREDENTIALS		creds;
	
	memset(key, 0, sizeof(CSSM_KEY));
	memset(&creds, 0, sizeof(CSSM_ACCESS_CREDENTIALS));
	crtn = CSSM_CSP_CreateDeriveKeyContext(cspHand,
		CSSM_ALGID_PKCS5_PBKDF2,
		keyAlg,
		keySizeInBits,
		&creds,
		NULL,			// BaseKey
		iterationCnt,
		salt,
		NULL,			// seed
		&ccHand);
	if(crtn) {
		printError("CSSM_CSP_CreateDeriveKeyContext", crtn);
		return crtn;
	}
	keyAttr = CSSM_KEYATTR_EXTRACTABLE | CSSM_KEYATTR_RETURN_REF | 
			  CSSM_KEYATTR_SENSITIVE;
	dummyLabel.Length = keyLabelLen;
	dummyLabel.Data = (uint8 *)keyLabel;
	
	/* passing in password is pretty strange....*/
	pbeParams.Passphrase = *password;
	pbeParams.PseudoRandomFunction = CSSM_PKCS5_PBKDF2_PRF_HMAC_SHA1;
	pbeData.Data = (uint8 *)&pbeParams;
	pbeData.Length = sizeof(pbeParams);
	crtn = CSSM_DeriveKey(ccHand,
		&pbeData,
		keyUsage,
		keyAttr,
		&dummyLabel,
		NULL,			// cred and acl
		key);
	if(crtn) {
		printError("CSSM_DeriveKey", crtn);
		return crtn;
	}
	crtn = CSSM_DeleteContext(ccHand);
	if(crtn) {
		printError("CSSM_DeleteContext", crtn);
	}
	return crtn;
}


int main(int argc, char **argv)
{
	int						rtn;
	uint32					keySizeInBytes;		// from cmd line
	char					*password;			// ASCII password from cmd line
	char					*inFileName;		// from cmd line
	unsigned char 			*inFile;			// raw infile data
	unsigned				inFileSize;			// in bytes
	char					*outFileName;		// from cmd line
	CSSM_CSP_HANDLE			cspHand;
	CSSM_RETURN				crtn;
	int						doEncrypt = 0;
	CSSM_DATA				passwordData;
	CSSM_DATA				saltData = {8, (uint8 *)"someSalt"};
	CSSM_DATA				inData;				// data to encrypt/decrypt, from inFile
	CSSM_DATA				outData = {0, NULL};// result data, written to outFile
	CSSM_CC_HANDLE			ccHand;				// crypto context
	CSSM_DATA				remData = {0, NULL};
	CSSM_SIZE				bytesProcessed;
	CSSM_KEY				symKey;
	char					algSpec = '4';
	CSSM_ALGORITHMS			keyAlg = 0;
	CSSM_ALGORITHMS			encrAlg = 0;
	CSSM_ENCRYPT_MODE		encrMode = 0;
	CSSM_PADDING			padding = 0;
	/* max of 16 bytes of init vector for the algs we use */
	CSSM_DATA				initVect = {16, (uint8 *)"someStrangeInitVector"};
	CSSM_DATA_PTR			initVectPtr = NULL;
	
	if(argc < 6) {
		usage(argv);
	}
	
	/* gather up cmd line args */
	switch(argv[1][0]) {
		case 'e':
			doEncrypt = 1;
			break;
		case 'd':
			doEncrypt = 0;
			break;
		default:
			usage(argv);
	}
	password = argv[2];
	passwordData.Data = (uint8 *)password;
	passwordData.Length = strlen(password);
	keySizeInBytes = atoi(argv[3]);
	if(keySizeInBytes == 0) {
		printf("keySize of 0 illegal\n");
		exit(1);
	}
	inFileName = argv[4];
	outFileName = argv[5];

	/* optional algorithm specifier */
	if(argc == 7) {
		if(argv[6][0] != 'a') {
			usage(argv);
		}
		algSpec = argv[6][2];
	}
	
	/* algorithm-specific parameters */
	switch(algSpec) {
		case '4':
			/* RC4 stream cipher - no padding, no IV, variable key size */
			keyAlg   = CSSM_ALGID_RC4;
			encrAlg  = CSSM_ALGID_RC4;
			encrMode = CSSM_ALGMODE_NONE;
			padding  = CSSM_PADDING_NONE;
			break;
		case 'c':
			/* ComCryption stream cipher - no padding, no IV, variable key size */
			keyAlg   = CSSM_ALGID_ASC;
			encrAlg  = CSSM_ALGID_ASC;
			encrMode = CSSM_ALGMODE_NONE;
			padding  = CSSM_PADDING_NONE;
			break;
		case 'd':
			/* DES block cipher, block size = 8 bytes, fixed key size */
			if(keySizeInBytes != 8) {
				printf("***DES must have key size of 8 bytes\n");
				exit(1);
			}
			keyAlg   = CSSM_ALGID_DES;
			encrAlg  = CSSM_ALGID_DES;
			encrMode = CSSM_ALGMODE_CBCPadIV8;
			padding  = CSSM_PADDING_PKCS7;
			initVect.Length = 8;
			initVectPtr = &initVect;
			break;
		case 'a':
			/* AES block cipher, block size = 16 bytes, fixed key size */
			if(keySizeInBytes != 16) {
				printf("***AES must have key size of 8 bytes\n");
				exit(1);
			}
			keyAlg   = CSSM_ALGID_AES;
			encrAlg  = CSSM_ALGID_AES;
			encrMode = CSSM_ALGMODE_CBCPadIV8;
			padding  = CSSM_PADDING_PKCS7;
			initVect.Length = 16;
			initVectPtr = &initVect;
			break;
		default:
			usage(argv);
	}

	/* read inFile from disk */
	rtn = readFile(inFileName, &inFile, &inFileSize);
	if(rtn) {
		printf("Error reading %s: %s\n", inFileName, strerror(rtn));
		exit(1);
	}
	inData.Data = inFile;
	inData.Length = inFileSize;
	
	/* attach to CSP */
	cspHand = cspStartup();
	if(cspHand == 0) {
		exit(1);
	}

	/*
	 * Derive an actual encryption/decryption key from the password ASCII text. 
	 * We could use the ASCII text directly as key material but using the DeriveKey
	 * function is much more secure (besides being an industry-standard way to 
	 * convert an ASCII password into binary key material). 
	 */
	crtn = ctDeriveKey(cspHand,
		keyAlg,
		"someLabel",		// keyLabel, not important
		9,					// keyLabelLen
		doEncrypt ? CSSM_KEYUSE_ENCRYPT : CSSM_KEYUSE_DECRYPT,
		keySizeInBytes * 8,	// keySizeInBits,
		&passwordData,
		&saltData,
		1000,				// iterCount, 1000 is the minimum
		&symKey);
	if(crtn) {
		exit(1);
	}
	
	/*
	 * Cook up a symmetric encrypt/decrypt context using the key we just derived
	 */
	crtn = CSSM_CSP_CreateSymmetricContext(cspHand,
		encrAlg,				// encryption algorithm
		encrMode,				// mode
		NULL,					// access cred
		&symKey,
		initVectPtr,			// InitVector
		padding,				// Padding
		NULL,					// Params
		&ccHand);
	if(crtn) {
		printError("CSSM_CSP_CreateSymmetricContext", crtn);
		exit(1);
	}

	/* 
	 * Do the encrypt/decrypt.
	 * We do this with the init/update/final sequence only to demonstrate its
	 * usage.
	 */
	if(doEncrypt) {
		crtn = CSSM_EncryptDataInit(ccHand);
		if(crtn) {
			printError("CSSM_EncryptDataInit", crtn);
			exit(1);
		}
		
		/* this step can be performed an arbitrary number of times, with
		 * the appropriate housekeeping of inData and outData */
		crtn = CSSM_EncryptDataUpdate(ccHand,
			&inData,
			1,
			&outData,
			1,
			&bytesProcessed);
		if(crtn) {
			printError("CSSM_EncryptDataUpdate", crtn);
			exit(1);
		}
		outData.Length = bytesProcessed;
		
		/* one call more to clean up */
		crtn = CSSM_EncryptDataFinal(ccHand, &remData);
		if(crtn) {
			printError("CSSM_EncryptDataFinal", crtn);
			exit(1);
		}
		if(remData.Length != 0) {
			/* append remaining data to outData */
			uint32 newLen = outData.Length + remData.Length;
			outData.Data = (uint8 *)appRealloc(outData.Data,
				newLen,
				NULL);
			memmove(outData.Data + outData.Length, remData.Data, remData.Length);
			outData.Length = newLen;
			appFree(remData.Data, NULL);
		}
	}
	else {
		crtn = CSSM_DecryptDataInit(ccHand);
		if(crtn) {
			printError("CSSM_DecryptDataInit", crtn);
			exit(1);
		}
		
		/* this step can be performed an arbitrary number of times, with
		 * the appropriate housekeeping of inData and outData */
		crtn = CSSM_DecryptDataUpdate(ccHand,
			&inData,
			1,
			&outData,
			1,
			&bytesProcessed);
		if(crtn) {
			printError("CSSM_DecryptDataUpdate", crtn);
			exit(1);
		}
		outData.Length = bytesProcessed;
		
		/* one call more to clean up */
		crtn = CSSM_DecryptDataFinal(ccHand, &remData);
		if(crtn) {
			printError("CSSM_DecryptDataFinal", crtn);
			exit(1);
		}
		if(remData.Length != 0) {
			/* append remaining data to outData */
			uint32 newLen = outData.Length + remData.Length;
			outData.Data = (uint8 *)appRealloc(outData.Data,
				newLen,
				NULL);
			memmove(outData.Data + outData.Length, remData.Data, remData.Length);
			outData.Length = newLen;
			appFree(remData.Data, NULL);
		}
	}
	if(crtn == CSSM_OK) {
		rtn = writeFile(outFileName, outData.Data, outData.Length);
		if(rtn) {
			printf("Error writing %s: %s\n", outFileName, strerror(rtn));
			exit(1);
		}
		else {
			printf("SUCCESS: inFile length %u bytes, outFile length %u bytes\n",
				inFileSize, (unsigned)outData.Length);
		}
	}
	/* free resources */
	crtn = CSSM_DeleteContext(ccHand);
	if(crtn) {
		printError("CSSM_DeleteContext", crtn);
	}
	crtn = CSSM_FreeKey(cspHand, 
		NULL,			// access cred
		&symKey,	
		CSSM_FALSE);	// don't delete since it wasn't permanent
	if(crtn) {
		printError("CSSM_FreeKey", crtn);
	}
	free(inFile);		// mallocd by readFile() 
	
	/* this was mallocd by CSP */
	appFree(outData.Data, NULL);
	CSSM_ModuleDetach(cspHand);
	return rtn;
}

