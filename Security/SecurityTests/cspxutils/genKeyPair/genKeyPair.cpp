/*
 * genKeyPair.cpp - create a key pair, store in specified keychain
 */
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <Security/Security.h>
#include "cspwrap.h"
#include "common.h"

static void usage(char **argv)
{
	printf("usage: %s keychain [options]\n", argv[0]);
	printf("Options:\n");
	printf("   -l label         -- no default\n");
	printf("   -a r|f|d         -- algorithm RSA/FEE/DSA, default = RSA\n");
	printf("   -k bits          -- key size in bits, default is 1024/128/512 for RSA/FEE/DSA\n");
	exit(1);
}

/*
 * Generate key pair of arbitrary algorithm. 
 * FEE keys will have random private data.
 * Like the cspGenKeyPair() in cspwrap.c except this provides a DLDB handle. 
 */
static CSSM_RETURN genKeyPair(CSSM_CSP_HANDLE cspHand,
	CSSM_DL_DB_HANDLE dlDbHand,
	uint32 algorithm,
	const char *keyLabel,
	unsigned keyLabelLen,
	uint32 keySize,					// in bits
	CSSM_KEY_PTR pubKey,			// mallocd by caller
	uint32 pubKeyUsage,				// CSSM_KEYUSE_ENCRYPT, etc.
	CSSM_KEY_PTR privKey,			// mallocd by caller
	uint32 privKeyUsage)			// CSSM_KEYUSE_DECRYPT, etc.
{
	CSSM_RETURN				crtn;
	CSSM_CC_HANDLE 			ccHand;
	CSSM_DATA				keyLabelData;
	uint32					pubAttr;
	uint32					privAttr;
	CSSM_RETURN 			ocrtn = CSSM_OK;
	
	/* pre-context-create algorithm-specific stuff */
	switch(algorithm) {
		case CSSM_ALGID_FEE:
			if(keySize == CSP_KEY_SIZE_DEFAULT) {
				keySize = CSP_FEE_KEY_SIZE_DEFAULT;
			}
			break;
		case CSSM_ALGID_RSA:
			if(keySize == CSP_KEY_SIZE_DEFAULT) {
				keySize = CSP_RSA_KEY_SIZE_DEFAULT;
			}
			break;
		case CSSM_ALGID_DSA:
			if(keySize == CSP_KEY_SIZE_DEFAULT) {
				keySize = CSP_DSA_KEY_SIZE_DEFAULT;
			}
			break;
		default:
			printf("cspGenKeyPair: Unknown algorithm\n");
			break;
	}
	keyLabelData.Data        = (uint8 *)keyLabel,
	keyLabelData.Length      = keyLabelLen;
	memset(pubKey, 0, sizeof(CSSM_KEY));
	memset(privKey, 0, sizeof(CSSM_KEY));
	
	crtn = CSSM_CSP_CreateKeyGenContext(cspHand,
		algorithm,
		keySize,
		NULL,					// Seed
		NULL,					// Salt
		NULL,					// StartDate
		NULL,					// EndDate
		NULL,					// Params
		&ccHand);
	if(crtn) {
		printError("CSSM_CSP_CreateKeyGenContext", crtn);
		ocrtn = crtn;
		goto abort;
	}
	/* cook up attribute bits */
	pubAttr = CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_EXTRACTABLE | CSSM_KEYATTR_PERMANENT;
	privAttr = CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_EXTRACTABLE | CSSM_KEYATTR_PERMANENT;

	/* post-context-create algorithm-specific stuff */
	switch(algorithm) {
		 case CSSM_ALGID_DSA:
			/* 
			 * extra step - generate params - this just adds some
			 * info to the context
			 */
			{
				CSSM_DATA dummy = {0, NULL};
				crtn = CSSM_GenerateAlgorithmParams(ccHand, 
					keySize, &dummy);
				if(crtn) {
					printError("CSSM_GenerateAlgorithmParams", crtn);
					return crtn;
				}
				appFreeCssmData(&dummy, CSSM_FALSE);
			}
			break;
		default:
			break;
	}
	
	/* add in DL/DB to context */
	crtn = cspAddDlDbToContext(ccHand, dlDbHand.DLHandle, dlDbHand.DBHandle);
	if(crtn) {
		ocrtn = crtn;
		goto abort;
	}

	crtn = CSSM_GenerateKeyPair(ccHand,
		pubKeyUsage,
		pubAttr,
		&keyLabelData,
		pubKey,
		privKeyUsage,
		privAttr,
		&keyLabelData,			// same labels
		NULL,					// CredAndAclEntry
		privKey);
	if(crtn) {
		printError("CSSM_GenerateKeyPair", crtn);
		ocrtn = crtn;
		goto abort;
	}
abort:
	if(ccHand != 0) {
		crtn = CSSM_DeleteContext(ccHand);
		if(crtn) {
			printError("CSSM_DeleteContext", crtn);
			ocrtn = CSSM_ERRCODE_INTERNAL_ERROR;
		}
	}
	return ocrtn;
}

int main(int argc, char **argv)
{
	char *kcName = NULL;
	char *label = NULL;
	CSSM_ALGORITHMS keyAlg = CSSM_ALGID_RSA;
	unsigned keySizeInBits = CSP_KEY_SIZE_DEFAULT;
	
	if(argc < 2) {
		usage(argv);
	}
	kcName = argv[1];
	
	extern char *optarg;
	extern int optind;
	
	optind = 2;
	int arg;
	while ((arg = getopt(argc, argv, "l:a:k:h")) != -1) {
		switch (arg) {
			case 'l':
				label = optarg;
				break;
			case 'a':
				switch(optarg[0]) {
					case 'r':
						keyAlg = CSSM_ALGID_RSA;
						break;
					case 'f':
						keyAlg = CSSM_ALGID_FEE;
						break;
					case 'd':
						keyAlg = CSSM_ALGID_DSA;
						break;
					default:
						usage(argv);
				}
				break;
			case 'k':
				keySizeInBits = atoi(optarg);
				break;
			default:
				usage(argv);
		}
	}
	if(optind != argc) {
		usage(argv);
	}
	
	SecKeychainRef kcRef = nil;
	OSStatus ortn;
	
	ortn = SecKeychainOpen(kcName, &kcRef);
	if(ortn) {
		cssmPerror("SecKeychainOpen", ortn);
		exit(1);
	}
	
	CSSM_CSP_HANDLE cspHand = 0;
	CSSM_DL_DB_HANDLE dlDbHand = {0, 0};
	ortn = SecKeychainGetCSPHandle(kcRef, &cspHand);
	if(ortn) {
		cssmPerror("SecKeychainGetCSPHandle", ortn);
		exit(1);
	}
	ortn = SecKeychainGetDLDBHandle(kcRef, &dlDbHand);
	if(ortn) {
		cssmPerror("SecKeychainGetDLDBHandle", ortn);
		exit(1);
	}
	
	CSSM_KEY privKey;
	CSSM_KEY pubKey;
	CSSM_RETURN crtn;
	
	crtn = genKeyPair(cspHand, dlDbHand,
		keyAlg, 
		label, (label ? strlen(label) : 0),
		keySizeInBits,
		&pubKey,
		CSSM_KEYUSE_ANY,			// may want to parameterize
		&privKey,
		CSSM_KEYUSE_ANY);			// may want to parameterize
	if(crtn) {
		printf("**Error creating key pair.\n");
	}
	else {
		printf("...key pair created in keychain %s.\n", kcName);
	}
	
	cspFreeKey(cspHand, &privKey);
	cspFreeKey(cspHand, &pubKey);
	CFRelease(kcRef);
	return 0;
}
