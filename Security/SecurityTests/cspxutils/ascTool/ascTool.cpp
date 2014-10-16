/*
 * ascCrypt.c - simple ASC (ComCryption) encrypt/decrypt utility
 */

#include "cspwrap.h"
#include "common.h"
#include <security_cdsa_utils/cuFileIo.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <Security/cssm.h>
#include <Security/cssmapple.h>
#include <CoreFoundation/CoreFoundation.h>

static void usage(char **argv) 
{
	printf("usage:\n");
	printf("  %s op password inFile outFile [k=keysize] [o=optimize]\n", argv[0]);
	printf("  op:\n");
	printf("     e  encrypt\n");
	printf("     d  decrypt\n");
	printf("  optimize:\n");
	printf("     d  default\n");
	printf("     s  size\n");
	printf("     e  Security\n");
	printf("     t  time\n");
	printf("     z  time+size\n");
	printf("     a  ASCII\n");
	exit(1);
}

int main(int argc, char **argv)
{
	int				rtn;
	uint32			keySizeInBytes = CSP_ASC_KEY_SIZE_DEFAULT / 8;
	uint32			optimize = CSSM_ASC_OPTIMIZE_DEFAULT;
	char			*password;			// ASCII password from cmd line
	char			*inFileName;		// from cmd line
	unsigned char 	*inFile;			// raw infile data
	unsigned		inFileSize;			// in bytes
	char			*outFileName;		// from cmd line
	CSSM_CSP_HANDLE	cspHand;
	CSSM_RETURN		crtn;
	int				doEncrypt = 0;
	CSSM_DATA		passwordData;
	CSSM_DATA		saltData = {8, (uint8 *)"someSalt"};
	CSSM_DATA		inData;				// data to encrypt/decrypt, from inFile
	CSSM_DATA		outData = {0, NULL};// result data, written to outFile
	CSSM_KEY_PTR	symKey;
	int				arg;
	char			*argp;
	CFAbsoluteTime 	start, end;
	CSSM_CC_HANDLE	ccHand;				// crypto context
	CSSM_DATA		remData = {0, NULL};
	CSSM_SIZE		bytesProcessed;
	CSSM_DATA		iv = {0, NULL};
	
	if(argc < 5) {
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
	inFileName = argv[3];
	outFileName = argv[4];

	/* optional args */
	for(arg=5; arg<argc; arg++) {
		argp = argv[arg];
		switch(argp[0]) {
			case 'k':
				keySizeInBytes = atoi(&argp[2]);
				if(keySizeInBytes == 0) {
					printf("keySize of 0 illegal\n");
					exit(1);
				}
				break;
			case 'o':
				switch(argp[2]) {
					case 'd':
						optimize = CSSM_ASC_OPTIMIZE_DEFAULT;
						break;
					case 's':
						optimize = CSSM_ASC_OPTIMIZE_SIZE;
						break;
					case 'e':
						optimize = CSSM_ASC_OPTIMIZE_SECURITY;
						break;
					case 't':
						optimize = CSSM_ASC_OPTIMIZE_TIME;
						break;
					case 'z':
						optimize = CSSM_ASC_OPTIMIZE_TIME_SIZE;
						break;
					case 'a':
						optimize = CSSM_ASC_OPTIMIZE_ASCII;
						break;
					default:
						usage(argv);
				}
				break;
			default:
				usage(argv);
		}
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
	symKey = cspDeriveKey(cspHand,
		CSSM_ALGID_PKCS5_PBKDF2,
		CSSM_ALGID_ASC,
		"someLabel",		// keyLabel, not important
		9,					// keyLabelLen
		doEncrypt ? CSSM_KEYUSE_ENCRYPT : CSSM_KEYUSE_DECRYPT,
		keySizeInBytes * 8,	// keySizeInBits,
		CSSM_FALSE,			// raw key
		&passwordData,
		&saltData,
		1000,				// iterCount, 1000 is the minimum
		&iv);
	if(symKey == NULL) {
		exit(1);
	}
	
	/*
	 * Cook up a symmetric encrypt/decrypt context using the key we just derived
	 */
	crtn = CSSM_CSP_CreateSymmetricContext(cspHand,
		CSSM_ALGID_ASC,			// encryption algorithm
		CSSM_ALGMODE_NONE,		// mode
		NULL,					// access cred
		symKey,
		NULL,					// InitVector
		CSSM_PADDING_NONE,		// Padding
		NULL,					// Params
		&ccHand);
	if(crtn) {
		printError("CSSM_CSP_CreateSymmetricContext", crtn);
		exit(1);
	}

	/* add in optimal optimization attribute */
	if(optimize != CSSM_ASC_OPTIMIZE_DEFAULT) {
		crtn = AddContextAttribute(ccHand,
			CSSM_ATTRIBUTE_ASC_OPTIMIZATION,
			sizeof(uint32),
			CAT_Uint32,
			NULL,
			optimize);
		if(crtn) {
			exit(1);
		}
	}
	
	/* 
	 * Do the encrypt/decrypt.
	 */
	start = CFAbsoluteTimeGetCurrent();
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
	
	end = CFAbsoluteTimeGetCurrent();
	if(crtn == CSSM_OK) {
		double inSizeD = (double)inFileSize;
		double outSizeD = (double)outData.Length;
		CFAbsoluteTime delta = end - start;
		
		rtn = writeFile(outFileName, outData.Data, outData.Length);
		if(rtn) {
			printf("Error writing %s: %s\n", outFileName, strerror(rtn));
			exit(1);
		}
		printf("    inFile length %d bytes, outFile length %lu bytes, "
			"%d ms\n",
			inFileSize, outData.Length, (unsigned)(delta * 1000.0));
		printf("    compression = %.2f     %.2f KBytes/s\n",
			doEncrypt ? outSizeD / inSizeD : inSizeD / outSizeD,
			inSizeD / delta / 1024.0);
	}
	
	/* free key, outData, etc. */
	CSSM_ModuleDetach(cspHand);
	return rtn;
}

