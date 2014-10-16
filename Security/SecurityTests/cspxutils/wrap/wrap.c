/*
 * encypt/decrypt using wrapped key
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <Security/cssm.h>
#include "cspwrap.h"
#include "common.h"
#include <security_cdsa_utils/cuFileIo.h>
#include <ctype.h>

static void usage(char **argv)
{
	printf("Usage: \n");
	printf("   %s w keyFile passPhrase1 passPhrase2 textToEncrypt textFile\n", argv[0]);
	printf("   %s u keyFile passPhrase1 textFile\n", argv[0]);
	exit(1);
}

char *iv = (char *)"someInit";
char *salt = (char *)"some20bytesOfGrainySalt";

char *wrapLabel = (char *)"wrapLabel";
char *encrLabel = (char *)"encrLabel";

#define IV_LEN				8
#define SALT_LEN			20
#define ITER_COUNT			1000
#define WRAPPING_KEY_ALG	CSSM_ALGID_3DES_3KEY
#define WRAPPING_ALG		CSSM_ALGID_3DES_3KEY_EDE
#define WRAPPING_KEY_SIZE	192
#define ENCRYPTING_KEY_ALG	CSSM_ALGID_3DES_3KEY
#define ENCRYPTING_ALG		CSSM_ALGID_3DES_3KEY_EDE
#define ENCRYPTING_KEY_SIZE	192

int main(int argc, char **argv)
{
	int 			doWrap = 0;
	CSSM_RETURN 	crtn;
	CSSM_CSP_HANDLE cspHand;
	CSSM_KEY_PTR	wrappingKey;
	CSSM_DATA		saltData = {SALT_LEN, (uint8 *)salt};
	CSSM_DATA		ivData = {IV_LEN, (uint8 *)iv};
	CSSM_DATA		phraseData;
	unsigned char	*keyFileData;
	unsigned		keyFileLen;
	unsigned char	*textFileData;
	unsigned		textFileLen;
	CSSM_DATA		ptext;
	CSSM_DATA		ctext;
	CSSM_KEY		wrappedKey;
	unsigned		i;
	
	if(argc < 2) {
		usage(argv);
	}
	switch(argv[1][0]) {
		case 'w':
			doWrap = 1;
			if(argc != 7) {
				usage(argv);
			}
			break;
		case 'u':
			doWrap = 0;
			if(argc != 5) {
				usage(argv);
			}
			break;
		default: 
			usage(argv);
	}
	cspHand = cspDlDbStartup(CSSM_TRUE, NULL);
	if(cspHand == 0) {
		exit(1);
	}

	/* passphrase1 ==> wrappingKey */
	phraseData.Data = (uint8 *)argv[3];
	phraseData.Length = strlen(argv[3]);
	wrappingKey = cspDeriveKey(cspHand,
		CSSM_ALGID_PKCS5_PBKDF2,
		WRAPPING_KEY_ALG,
		wrapLabel,
		strlen(wrapLabel),
		CSSM_KEYUSE_ANY,
		WRAPPING_KEY_SIZE,
		CSSM_TRUE,			// ref key
		&phraseData,
		&saltData,
		ITER_COUNT,
		&ivData);
	if(wrappingKey == NULL) {
		printf("Error creating key from \'%s\'\n", argv[3]);
		exit(1);
	}

	if(doWrap) {
		/* passphrase2 ==> encrKey */
		CSSM_KEY_PTR	encrKey;
		phraseData.Data = (uint8 *)argv[4];
		phraseData.Length = strlen(argv[4]);
		encrKey = cspDeriveKey(cspHand,
			CSSM_ALGID_PKCS5_PBKDF2,
			ENCRYPTING_KEY_ALG,
			encrLabel,
			strlen(encrLabel),
			CSSM_KEYUSE_ANY,
			ENCRYPTING_KEY_SIZE,
			CSSM_TRUE,			// ref key
			&phraseData,
			&saltData,
			ITER_COUNT,
			&ivData);
		if(encrKey == NULL) {
			printf("Error creating key from \'%s\'\n", argv[4]);
			exit(1);
		}
		
		/* encrypt textToEncrypt, write it to textFile */
		ptext.Data = (uint8 *)argv[5];
		ptext.Length = strlen(argv[5]);
		ctext.Data = NULL;
		ctext.Length = 0;
		crtn = cspEncrypt(cspHand,
			CSSM_ALGID_3DES_3KEY_EDE,
			CSSM_ALGMODE_CBCPadIV8,
			CSSM_PADDING_PKCS5,
			encrKey,
			NULL,
			0,
			0,
			&ivData,
			&ptext,
			&ctext,
			CSSM_FALSE);
		if(crtn) {
			printf("Error encrypting.\n");
			exit(1);
		}
		if(writeFile(argv[6], ctext.Data, ctext.Length)) {
			printf("Error writing to %s\n", argv[6]);
			exit(1);
		}
		
		/* now wrap encrKey with wrappingKey and write the wrapped blob */
		crtn = cspWrapKey(cspHand,
			encrKey,
			wrappingKey,
			WRAPPING_ALG,
			CSSM_ALGMODE_CBCPadIV8,
			CSSM_KEYBLOB_WRAPPED_FORMAT_NONE,
			CSSM_PADDING_PKCS5,
			&ivData,
			NULL,
			&wrappedKey);
		if(crtn) {
			exit(1);
		}
		if(writeFile(argv[2], wrappedKey.KeyData.Data, wrappedKey.KeyData.Length)) {
			printf("error writing to %s\n", argv[2]);
			exit(1);
		}
		printf("...wrote %lu bytes of encrypted text to %s\n", 
			ctext.Length, argv[6]);
		printf("...wrote %lu bytes of wrapped key data to %s\n",
			wrappedKey.KeyData.Length, argv[2]);
	}
	else {
		/* read in encrypted text and wrapped key blob */
		CSSM_KEY	decrKey;
		CSSM_DATA	outDescData2 = {0, NULL};
		
		if(readFile(argv[2], &keyFileData, &keyFileLen)) {
			printf("Error reading %s\n", argv[2]);
		}
		if(readFile(argv[4], &textFileData, &textFileLen)) {
			printf("Error reading %s\n", argv[2]);
		}
		
		/* cook up a reasonable "wrapped key" */
		memset(&wrappedKey, 0, sizeof(CSSM_KEY));
		wrappedKey.KeyHeader.HeaderVersion = CSSM_KEYHEADER_VERSION;
		wrappedKey.KeyHeader.BlobType = CSSM_KEYBLOB_WRAPPED;
		wrappedKey.KeyHeader.Format = CSSM_KEYBLOB_WRAPPED_FORMAT_APPLE_CUSTOM;
		wrappedKey.KeyHeader.AlgorithmId = CSSM_ALGID_3DES_3KEY;
		wrappedKey.KeyHeader.KeyClass = CSSM_KEYCLASS_SESSION_KEY;
		wrappedKey.KeyHeader.LogicalKeySizeInBits = ENCRYPTING_KEY_SIZE;
		wrappedKey.KeyHeader.KeyAttr = CSSM_KEYATTR_EXTRACTABLE;
		wrappedKey.KeyHeader.KeyUsage = CSSM_KEYUSE_ANY;
		wrappedKey.KeyHeader.WrapAlgorithmId = WRAPPING_ALG;
		wrappedKey.KeyHeader.WrapMode = CSSM_ALGMODE_CBCPadIV8;
		wrappedKey.KeyData.Data = keyFileData;
		wrappedKey.KeyData.Length = keyFileLen;
		
		/* unwrap the key to get decrypting key */
		crtn = cspUnwrapKey(cspHand,
			&wrappedKey,
			wrappingKey,
			WRAPPING_ALG,
			CSSM_ALGMODE_CBCPadIV8,
			CSSM_PADDING_PKCS5,
			&ivData,
			&decrKey,
			&outDescData2,
			encrLabel,
			strlen(encrLabel));
		if(crtn) {
			printf("Error on unwrap.\n");
			exit(1);
		}
		
		/* decrypt the text file and print its result */
		ctext.Data = textFileData;
		ctext.Length = textFileLen;
		ptext.Data = NULL;
		ptext.Length = 0;
		crtn = cspDecrypt(cspHand,
			CSSM_ALGID_3DES_3KEY_EDE,
			CSSM_ALGMODE_CBCPadIV8,
			CSSM_PADDING_PKCS5,
			&decrKey,
			NULL,
			0,
			0,
			&ivData,
			&ctext,
			&ptext,
			CSSM_FALSE);
		if(crtn) {
			printf("Error on decrypt.\n");
			exit(1);
		}
		printf("...original text: ");
		for(i=0; i<ptext.Length; i++) {
			if(isprint(ptext.Data[i])) {
				printf("%c", ptext.Data[i]);
			}
			else {
				printf("-%02X-", ptext.Data[i]);
			}
		}
		printf("\n");
	}
	return 0;
}
