/* Copyright (c) 2001,2003-2006,2008 Apple Inc.
 *
 * keyStore.c - basic key pair store/lookup routines.
 *
 * create key pair, varying pub is permanent, priv is permanent;
 * sign some data with priv;
 * lookUpPub = lookup pub by label;
 * vfy with lookUpPub;
 * lookUpPriv = lookup priv by label;
 * if(privIsPerm) {
 *	 	ensure lookUpPriv == priv;
 * 		freeKey(lookUpPriv);
 * 		obtainedPriv = obtainPubFromPriv(lookUpPub);
 * 		ensure obtainedPriv == priv;
 * 		freeKey(obtainedPriv);
 * 		delete priv;		// cspwrap does implicit freeKey here...
 * }
 * else {
 *      free priv;
 * }
 * lookUpPriv = lookup priv by label; verify fail;
 * lookUpPriv = obtainPubFromPriv(pub); verify fail;
 * freeKey(lookUpPub);
 * if pub is permament {
 * 		lookUpPub = lookup pub by label; verify OK;
 *		deleteKey(lookUpPub);
 * 		lookUpPub = lookup pub by label; verify fail;
 * }
 * lookUpPub = lookup pub by label; verify fail;
 */

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <Security/cssm.h>
#include "cspwrap.h"
#include "common.h"
#include "cspdlTesting.h"

#define LOOPS_DEF			10
#define MAX_DATA_SIZE		100
#define DB_NAME				"keyStore.db"

/* can't lookup non-permanent keys! */
#define FORCE_PUB_PERMANENT		0

static void usage(char **argv)
{
	printf("usage: %s [options]\n", argv[0]);
	printf("   Options:\n");
	printf("   l=loops (default=%d; 0=forever)\n", LOOPS_DEF);
	printf("   r(RSA; default = FEE)\n");
	printf("   p(ermanent keys, implies l=1)\n");
	printf("   k=keyChainFile\n");
	printf("   n(o sign/verify)\n");
	printf("   N(o lookup of nonexistent keys)\n");
	printf("   x (privKey always extractable)\n");
	printf("   P(ause for MallocDebug)\n");
	printf("   v(erbose)\n");
	printf("   q(uiet)\n");
	printf("   h(elp)\n");
	exit(1);
}

#define FEE_PRIV_DATA_SIZE	20

/* 
 * NULL wrap, error tolerant. 
 */
CSSM_RETURN cspNullWrapKey(
	CSSM_CSP_HANDLE cspHand,
	const CSSM_KEY  *refKey,	
	CSSM_KEY_PTR	rawKey)			// RETURNED on success, caller must FreeKey
{
	CSSM_CC_HANDLE			ccHand;
	CSSM_RETURN				crtn;
	CSSM_ACCESS_CREDENTIALS	creds;
	CSSM_DATA descData = {0, 0};
	
	memset(rawKey, 0, sizeof(CSSM_KEY));
	memset(&creds, 0, sizeof(CSSM_ACCESS_CREDENTIALS));
	crtn = CSSM_CSP_CreateSymmetricContext(cspHand,
			CSSM_ALGID_NONE,
			CSSM_ALGMODE_NONE,
			&creds,				// passPhrase,
			NULL,				// wrappingKey,
			NULL,				// initVector,
			CSSM_PADDING_NONE,
			0,					// Params
			&ccHand);
	if(crtn) {
		printError("cspNullWrapKey/CreateContext", crtn);
		return crtn;
	}
	crtn = CSSM_WrapKey(ccHand,
		&creds,
		refKey,
		&descData,			// DescriptiveData
		rawKey);
	if(CSSM_DeleteContext(ccHand)) {
		printf("CSSM_DeleteContext failure\n");
	}
	return crtn;
}


/*
 * Generate key pair, default size. This is like cspGenKeyPair in cspwrap.c,
 * tweaked for this test To allow varying permanent attribute.
 */
static CSSM_RETURN genKeyPair(CSSM_CSP_HANDLE cspHand,
	CSSM_DL_HANDLE		dlHand,
	CSSM_DB_HANDLE		dbHand,			
	const CSSM_DATA_PTR keyLabel,
	CSSM_KEY_PTR 		pubKey,				// mallocd by caller
	uint32 				pubKeyUsage,		// CSSM_KEYUSE_ENCRYPT, etc.
	uint32 				pubKeyAttr,
	CSSM_KEY_PTR 		privKey,			// mallocd by caller
	uint32				privKeyUsage,		// CSSM_KEYUSE_DECRYPT, etc.
	uint32 				privKeyAttr,
	uint32				keyGenAlg)
{
	CSSM_RETURN				crtn;
	CSSM_CC_HANDLE 			ccHand;
	CSSM_RETURN 			ocrtn = CSSM_OK;
	uint32					keySize;
	
	if(keyGenAlg == CSSM_ALGID_FEE) {
		keySize = CSP_FEE_KEY_SIZE_DEFAULT;
	}
	else {
		keySize = CSP_RSA_KEY_SIZE_DEFAULT;
	}
	memset(pubKey, 0, sizeof(CSSM_KEY));
	memset(privKey, 0, sizeof(CSSM_KEY));

	crtn = CSSM_CSP_CreateKeyGenContext(cspHand,
		keyGenAlg,
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

	/* add in DL/DB to context */
	crtn = cspAddDlDbToContext(ccHand, dlHand, dbHand);
	if(crtn) {
		ocrtn = crtn;
		goto abort;
	}
	crtn = CSSM_GenerateKeyPair(ccHand,
		pubKeyUsage,
		pubKeyAttr,
		keyLabel,
		pubKey,
		privKeyUsage,
		privKeyAttr,
		keyLabel,				// same labels
		NULL,					// cred/acl
		privKey);
	if(crtn) {
		printError("CSSM_GenerateKeyPair", crtn);
		ocrtn = crtn;
		goto abort;
	}

	/* basic checks...*/
	if(privKey->KeyHeader.BlobType != CSSM_KEYBLOB_REFERENCE) {
		printf("privKey blob type: exp %u got %u\n",
			CSSM_KEYBLOB_REFERENCE, (unsigned)privKey->KeyHeader.BlobType);
	}
	if(pubKeyAttr & CSSM_KEYATTR_RETURN_REF) {
		if(pubKey->KeyHeader.BlobType != CSSM_KEYBLOB_REFERENCE) {
			printf("pubKey blob type: exp %u got %u\n",
				CSSM_KEYBLOB_REFERENCE, (unsigned)privKey->KeyHeader.BlobType);
			ocrtn = -1;
			goto abort;
		}
	}
	else {
		if(pubKey->KeyHeader.BlobType != CSSM_KEYBLOB_RAW) {
			printf("pubKey blob type: exp %u got %u\n",
				CSSM_KEYBLOB_RAW, (unsigned)privKey->KeyHeader.BlobType);
			ocrtn = -1;
			goto abort;
		}
	}

abort:
	if(ccHand != 0) {
		crtn = CSSM_DeleteContext(ccHand);
		if(crtn) {
			printError("CSSM_DeleteContext", crtn);
			ocrtn = crtn;
		}
	}
	return ocrtn;
}

#define KEY_LABEL "testKey"

/* 
 * when true, keyref in key obtained from DL differs from
 * keyref in key from CSP
 */
#define DL_REF_KEYS_DIFFER		1

/*
 * ObtainPrivateKeyFromPublicKey doesn't work yet
 */
#define DO_OBTAIN_FROM_PUB		CSPDL_OBTAIN_PRIV_FROM_PUB

/* we're assumed to be logged in for access to private objects */
static int doTest(CSSM_CSP_HANDLE cspHand,
	CSSM_DL_HANDLE	dlHand,
	CSSM_DB_HANDLE	dbHand,		
	CSSM_BOOL		pubIsPerm,		// pub is permanent
	CSSM_BOOL		privIsPerm,		// priv is permanent
	CSSM_BOOL		privIsExtractable,
	CSSM_BOOL		permKeys,		// leave them in the KC
	CSSM_BOOL		doSignVerify, 
	CSSM_BOOL		doFailedLookup,
	CSSM_DATA_PTR 	ptext,
	CSSM_BOOL	 	verbose,
	CSSM_BOOL 		quiet,
	uint32			keyGenAlg,
	uint32			sigAlg)
{
	CSSM_KEY		pubKey;			// from GenerateKeyPair
	CSSM_KEY		privKey;
	CSSM_KEY_PTR	lookUpPub;		// from cspLookUpKeyByLabel, etc.
	CSSM_KEY_PTR	lookUpPriv;
	CSSM_RETURN 	crtn;
	CSSM_DATA		sig;
	CSSM_DATA		labelData;
	CSSM_KEY		obtainedPriv;
	uint32			pubAttr;
	uint32			privAttr;
	CSSM_KEY		rawPrivKey;
	labelData.Data = (uint8 *)KEY_LABEL;
	labelData.Length = strlen(KEY_LABEL);
	CSSM_BOOL		doLookup;
	
 	/* create key pair */
 	if(verbose) {
 		printf("   ...generating key pair (pubIsPerm %d privIsPerm %d privIsExtract"
			" %d)\n", (int)pubIsPerm, (int)privIsPerm, (int)privIsExtractable);
 	}
 	pubAttr = CSSM_KEYATTR_EXTRACTABLE | CSSM_KEYATTR_RETURN_REF;
 	if(pubIsPerm) {
 		pubAttr |= CSSM_KEYATTR_PERMANENT;
 	}
	
	/*
	 * To use a NULL wrap to test detection of !EXTRACTABLE, we're relying on
	 * being able to create !SENSITIVE private keys. We'll make 'em sensitive
	 * if we're not trying to null wrap them.
	 */
	privAttr =  CSSM_KEYATTR_RETURN_REF;
 	if(privIsPerm) {
 		privAttr |= CSSM_KEYATTR_PERMANENT;
 	}
	if(privIsExtractable) {
 		privAttr |= CSSM_KEYATTR_EXTRACTABLE;
	}
	else {
		privAttr |= CSSM_KEYATTR_SENSITIVE;
	}
	#if	CSPDL_KEYATTR_PRIVATE
 		privAttr |= CSSM_KEYATTR_PRIVATE;
	#endif
 	crtn = genKeyPair(cspHand,
		dlHand,
 		dbHand,
		&labelData,
		&pubKey,
		CSSM_KEYUSE_VERIFY,			// pubKeyUsage
		pubAttr,
		&privKey,
		CSSM_KEYUSE_SIGN,
		privAttr,
		keyGenAlg);
	if(crtn) {
		return testError(quiet);
	}

 	/* lookUpPub = lookup pub by label; */
	doLookup = CSSM_TRUE;
 	if(verbose) {
 		if(pubIsPerm) {
	 		printf("   ...lookup pub by label\n");
 		}
 		else {
			if(doFailedLookup) {
				printf("   ...lookup (nonexistent) pub by label\n");
			}
			else {
				doLookup = CSSM_FALSE;
				lookUpPub = NULL;
			}
 		}
 	}
	if(doLookup) {
		lookUpPub = cspLookUpKeyByLabel(dlHand, dbHand, &labelData, CKT_Public);
	}
	if(pubIsPerm) {
		if(lookUpPub == NULL) {
			printf("lookup pub by label failed\n");
			return testError(quiet);
		}
	
		/* sign some data with priv; */
		sig.Data = NULL;
		sig.Length = 0;
		if(doSignVerify) {
			if(cspSign(cspHand,
					sigAlg,
					&privKey,
					ptext,
					&sig)) {
				return testError(quiet);
			}
		}
		/* verify header compare */
		if(memcmp(&lookUpPub->KeyHeader, &pubKey.KeyHeader,
				sizeof(CSSM_KEYHEADER))) {
			printf("**pubKey header miscompare\n");
			return testError(quiet);
		}
	
		/* vfy with lookUpPub; */
		if(doSignVerify) {
			if(cspSigVerify(cspHand,
					sigAlg,
					lookUpPub,
					ptext,
					&sig,
					CSSM_OK)) {
				return testError(quiet);
			}
		
			CSSM_FREE(sig.Data);
			sig.Data = NULL;
			sig.Data = 0;
		}
	}
	else {
		if(doLookup && (lookUpPub != NULL)) {
			printf("***Unexpected success on cspLookUpKeyByLabel(pub, not perm)\n");
			return testError(quiet);
		}
	}
	
	/* 
	 * Ensure proper behavior of extractable bit 
	 */
	if(verbose) {
		printf("   ...null wrap %s private key\n", 
			privIsExtractable ? "EXTRACTABLE" : "!EXTRACTABLE");
	}
	crtn = cspNullWrapKey(cspHand, &privKey, &rawPrivKey);
	if(privIsExtractable) {
		if(crtn) {
			printError("Null Wrap(extractable private key)", crtn);
			return testError(quiet);
		}
		if(verbose) {
			printf("   ...free rawPrivKey\n");
		}
	 	cspFreeKey(cspHand, &rawPrivKey);
	}
	else {
		if(crtn != CSSMERR_CSP_INVALID_KEYATTR_MASK) {
			printError("Null Wrap of !extractable private key: expected "
				"INVALID_KEYATTR_MASK, got", crtn);
			if(crtn == CSSM_OK) {
			 	cspFreeKey(cspHand, &rawPrivKey);
			}
			return testError(quiet);
		}
	}
	/* lookUpPriv = lookup priv by label; ensure == privKey; */
	doLookup = CSSM_TRUE;
 	if(verbose) {
 		if(privIsPerm) {
	 		printf("   ...lookup priv by label\n");
 		}
 		else {
			if(doFailedLookup) {
				printf("   ...lookup (nonexistent) priv by label\n");
			}
			else {
				doLookup = CSSM_FALSE;
				lookUpPriv = NULL;
			}
 		}
 	}
	if(doLookup) {
		lookUpPriv = cspLookUpKeyByLabel(dlHand, dbHand, &labelData, CKT_Private);
	}
 	if(privIsPerm) {
	 	if(lookUpPriv == NULL) {
	 		printf("lookup priv by label failed\n");
	 		return testError(quiet);
	 	}

		/* note we "know" that both keys are ref keys...*/
		if(lookUpPriv->KeyData.Length != privKey.KeyData.Length) {
			printf("priv key data length mismatch\n");
			return testError(quiet);
		}
		#if		DL_REF_KEYS_DIFFER
		if(!memcmp(lookUpPriv->KeyData.Data, privKey.KeyData.Data,
				privKey.KeyData.Length)) {
			printf("priv key ref data mismatch\n");
			return testError(quiet);
		}
		#else	/* DL_REF_KEYS_DIFFER */
		if(memcmp(lookUpPriv->KeyData.Data, privKey.KeyData.Data,
				privKey.KeyData.Length)) {
			printf("unexpected priv key ref data match\n");
			return testError(quiet);
		}
		#endif	/* DL_REF_KEYS_DIFFER */
			
		/* verify header compare in any case */
		if(memcmp(&lookUpPriv->KeyHeader, &privKey.KeyHeader,
				sizeof(CSSM_KEYHEADER))) {
			printf("**privKey header miscompare\n");
			return testError(quiet);
		}
		
		/* sign with lookUpPriv, verify with pubKey */
		sig.Data = NULL;
		sig.Length = 0;
		if(doSignVerify) {
			if(verbose) {
				printf("   ...sign with lookup priv\n");
			}
			if(cspSign(cspHand,
					sigAlg,
					lookUpPriv,
					ptext,
					&sig)) {
				return testError(quiet);
			}
			if(verbose) {
				printf("   ...verify with pub\n");
			}
			if(cspSigVerify(cspHand,
					sigAlg,
					&pubKey,
					ptext,
					&sig,
					CSSM_OK)) {
				printf("***sign with lookUpPriv, vfy with pub FAILED\n");
				return testError(quiet);
			}
			CSSM_FREE(sig.Data);
			sig.Data = NULL;
			sig.Data = 0;
		}
		
	 	/* free lookUpPriv from cache, but it's permanent */
	 	if(verbose) {
	 		printf("   ...free lookupPriv\n");
	 	}
	 	if(cspFreeKey(cspHand, lookUpPriv)) {
	 		printf("Error on cspFreeKey(lookUpPriv)\n");
	 		return testError(quiet);
	 	}
	 	CSSM_FREE(lookUpPriv);		// mallocd during lookup
	 	lookUpPriv = NULL;

		#if	DO_OBTAIN_FROM_PUB
	 	/* obtainedPriv = obtainPubFromPriv(pub); ensure == priv; */
	 	if(verbose) {
	 		printf("   ...ObtainPrivateKeyFromPublicKey\n");
	 	}
	 	obtainedPriv.KeyData.Data = NULL;
	 	obtainedPriv.KeyData.Length = 0;
	 	crtn = CSSM_CSP_ObtainPrivateKeyFromPublicKey(cspHand,
	 		lookUpPub,
	 		&obtainedPriv);
	 	if(crtn) {
	 		printError("ObtainPrivateKeyFromPublicKey", crtn);
	 		return testError(quiet);
	 	}

	 	/* free obtainedPriv from cache, but it's permanent */
	 	if(verbose) {
	 		printf("   ...free obtainedPriv\n");
	 	}
	 	if(cspFreeKey(cspHand, &obtainedPriv)) {
	 		printf("Error on cspFreeKey(obtainedPriv)\n");
	 		return testError(quiet);
	 	}
		
		#endif	/* DO_OBTAIN_FROM_PUB */
		
		if(!permKeys) {
			/* delete priv - implies freeKey as well */
			if(verbose) {
				printf("   ...delete privKey\n");
			}
			crtn = cspDeleteKey(cspHand, dlHand, dbHand, &labelData, &privKey);
			if(crtn) {
				printf("Error deleting priv\n");
				return testError(quiet);
			}
	
			/* lookUpPriv = lookup priv by label; verify fail; */
			if(doFailedLookup) {
				if(verbose) {
					printf("   ...lookup (nonexistent) priv by label\n");
				}
				lookUpPriv = cspLookUpKeyByLabel(dlHand, dbHand, &labelData, CKT_Private);
				if(lookUpPriv != NULL) {
					printf("Unexpected success on cspLookUpKeyByLabel(priv)\n");
					return testError(quiet);
				}
			}
			else {
				lookUpPriv = NULL;
			}
		}
	}
	else if(doLookup) {
		/* !privIsPerm - just free it and it's all gone */
		if(lookUpPriv != NULL) {
			printf("***Unexpected success on cspLookUpKeyByLabel(priv, not perm)\n");
			return testError(quiet);
		}
		if(verbose) {
			printf("   ...free privKey\n");
		}
	 	if(cspFreeKey(cspHand, &privKey)) {
	 		printf("Error on cspFreeKey(privKey)\n");
	 		return testError(quiet);
	 	}
	}
	/* CSP, DL have no knowledge of privKey or its variations */

 	/* obtainedPriv = obtainPubFromPriv(pub); verify fail;*/
 	/* Note this should be safe even if DO_OBTAIN_FROM_PUB == 0 */
 	obtainedPriv.KeyData.Data = NULL;
 	obtainedPriv.KeyData.Length = 0;
 	if(verbose) {
 		printf("   ...obtain (nonexistent) priv by public\n");
 	}
 	crtn = CSSM_CSP_ObtainPrivateKeyFromPublicKey(cspHand,
 		&pubKey,
 		&obtainedPriv);
	switch(crtn) {
		case CSSM_OK:
			printf("Unexpected success on ObtainPrivateKeyFromPublicKey\n");
			return testError(quiet);
		case CSSMERR_CSP_PRIVATE_KEY_NOT_FOUND:
			break;
		#if	!CSPDL_OBTAIN_PRIV_FROM_PUB
		case CSSMERR_CSP_FUNCTION_NOT_IMPLEMENTED:
			/* OK */
			break;
		#endif
		default:
			printf("Unexpected err ObtainPrivateKeyFromPublicKey\n");
			printError("got this", crtn);
			return testError(quiet);
 	}

 	/* free one or both copies of pub as appropriate */
 	if(verbose) {
 		printf("   ...free pubKey\n");
 	}
 	crtn = cspFreeKey(cspHand, &pubKey);
	if(crtn) {
		printf("Error freeing pubKey\n");
		return testError(quiet);
	}
	if(pubIsPerm) {
	 	if(verbose) {
	 		printf("   ...free lookUpPub\n");
	 	}
	 	crtn = cspFreeKey(cspHand, lookUpPub);
		if(crtn) {
			printf("Error freeing lookUpPub\n");
			return testError(quiet);
		}
	}
	if(lookUpPub) {
		CSSM_FREE(lookUpPub);			// mallocd by lookup in any case
	}

	if(pubIsPerm) {
		/* pub should still be there in DL */
		/* lookUpPub = lookup pub by label; verify OK; */
	 	if(verbose) {
	 		printf("   ...lookup pub by label (2)\n");
	 	}
	 	lookUpPub = cspLookUpKeyByLabel(dlHand, dbHand, &labelData, CKT_Public);
	 	if(lookUpPub == NULL) {
	 		printf("lookup pub by label (2) failed\n");
	 		return testError(quiet);
	 	}

		if(!permKeys) {
			/* now really delete it */
			if(verbose) {
				printf("   ...delete lookUpPub\n");
			}
			crtn = cspDeleteKey(cspHand, dlHand, dbHand, &labelData, lookUpPub);
			if(crtn) {
				printf("Error deleting lookUpPub\n");
				return testError(quiet);
			}
			CSSM_FREE(lookUpPub);		// mallocd by lookup
		}
	}
	/* else freeKey should have removed all trace */
	
	if(!permKeys && doFailedLookup) {
		/* lookUpPub = lookup pub by label; verify fail; */
		if(verbose) {
			printf("   ...lookup (nonexistent) pub by label\n");
		}
		lookUpPub = cspLookUpKeyByLabel(dlHand, dbHand, &labelData, CKT_Public);
		if(lookUpPub != NULL) {
			printf("Unexpected success on cspLookUpKeyByLabel(pub) (2)\n");
			return testError(quiet);
		}
	}
	return 0;
}

int main(int argc, char **argv)
{
	int					arg;
	char				*argp;
	unsigned			loop;
	CSSM_DATA			ptext;
	CSSM_CSP_HANDLE 	cspHand;
	CSSM_DB_HANDLE		dbHand;
	CSSM_DL_HANDLE		dlHand;
	CSSM_BOOL			pubIsPerm;
	CSSM_BOOL			privIsPerm;
	CSSM_BOOL			privIsExtractable;
	uint32				keyGenAlg = CSSM_ALGID_FEE;
	uint32				sigAlg = CSSM_ALGID_FEE_MD5;
	int					rtn = 0;
	CSSM_RETURN			crtn;
	
	/*
	 * User-spec'd params
	 */
	unsigned	loops = LOOPS_DEF;
	CSSM_BOOL	verbose = CSSM_FALSE;
	CSSM_BOOL	quiet = CSSM_FALSE;
	CSSM_BOOL	permKeys = CSSM_FALSE;
	char		dbName[100];		/* DB_NAME_pid */
	CSSM_BOOL	useExistDb = CSSM_FALSE;
	CSSM_BOOL	doPause = CSSM_FALSE;
	CSSM_BOOL	doSignVerify = CSSM_TRUE;
	CSSM_BOOL	doFailedLookup = CSSM_TRUE;
	CSSM_BOOL	privAlwaysExtractable = CSSM_FALSE;
	
	dbName[0] = '\0';
	
	for(arg=1; arg<argc; arg++) {
		argp = argv[arg];
		switch(argp[0]) {
		    case 'l':
				loops = atoi(&argp[2]);
				break;
		    case 'v':
		    	verbose = CSSM_TRUE;
				break;
		    case 'q':
		    	quiet = CSSM_TRUE;
				break;
		    case 'p':
		    	permKeys = CSSM_TRUE;
				loops = 1;
				break;
			case 'r':
				keyGenAlg = CSSM_ALGID_RSA;
				sigAlg = CSSM_ALGID_MD5WithRSA;
				break;
			case 'k':
				memmove(dbName, &argp[2], strlen(&argp[2]) + 1);
				useExistDb = CSSM_TRUE;
				break;
			case 'P':
				doPause = CSSM_TRUE;
				break;
			case 'n':
				doSignVerify = CSSM_FALSE;
				break;
			case 'N':
				doFailedLookup = CSSM_FALSE;
				break;
			case 'x':
				privAlwaysExtractable = CSSM_TRUE;
				break;
		    case 'h':
		    default:
				usage(argv);
		}
	}
	
	if(dbName[0] == '\0') {
		sprintf(dbName, "%s_%d", DB_NAME, (int)getpid());
	}
	
	ptext.Data = (uint8 *)CSSM_MALLOC(MAX_DATA_SIZE);
	ptext.Length = MAX_DATA_SIZE;
	/* data generated in test loop */
	if(ptext.Data == NULL) {
		printf("Insufficient heap\n");
		exit(1);
	}

	testStartBanner("keyStore", argc, argv);

	/* attach to CSP/DL */
	cspHand = cspDlDbStartup(CSSM_FALSE, NULL);
	if(cspHand == 0) {
		exit(1);
	}
	dlHand = dlStartup();
	if(dlHand == 0) {
		exit(1);
	}
	if(useExistDb) {
		/* this one may well require SecurityAgent UI */
		crtn = dbCreateOpen(dlHand, dbName, CSSM_FALSE, CSSM_FALSE, NULL,
			&dbHand);
	}
	else {
		/* hands-free test */
		crtn = dbCreateOpen(dlHand, dbName, CSSM_TRUE, CSSM_TRUE, dbName,
			&dbHand);
	}
	if(crtn) {
		exit(1);
	}
	for(loop=1; ; loop++) {

		if(!quiet) {
			printf("...loop %d\n", loop);
		}
		appGetRandomBytes(ptext.Data, ptext.Length);

		if(permKeys) {
			pubIsPerm = privIsPerm = CSSM_TRUE;
		}
		else {
			#if CSPDL_ALL_KEYS_ARE_PERMANENT
			pubIsPerm = CSSM_TRUE;
			privIsPerm = CSSM_TRUE;
			#else 
	
			/* mix up pubIsPerm, privIsPerm */
			pubIsPerm  = (loop & 1) ? CSSM_TRUE : CSSM_FALSE;
			privIsPerm = (loop & 2) ? CSSM_TRUE : CSSM_FALSE;
			#if FORCE_PUB_PERMANENT
			pubIsPerm = CSSM_TRUE;
			#endif
			#endif	/* CSPDL_ALL_KEYS_ARE_PERMANENT */
		}
		privIsExtractable = ((loop & 4) || privAlwaysExtractable) ? CSSM_TRUE : CSSM_FALSE;
		if(doTest(cspHand,
				dlHand,
				dbHand,
				pubIsPerm,
				privIsPerm,
				privIsExtractable,
				permKeys,
				doSignVerify, doFailedLookup,
				&ptext,
				verbose,
				quiet,
				keyGenAlg,
				sigAlg)) {
			rtn = 1;
			break;
		}
		if(loops && (loop == loops)) {
			break;
		}
		if(doPause) {
			fpurge(stdin);
			printf("CR to continue: ");
			getchar();
		}
	}

	cspShutdown(cspHand, CSSM_FALSE);
	/* FIXME - DB close? DL shutdown? */
	if((rtn == 0) && !quiet) {
		printf("%s test complete\n", argv[0]);
	}
	if((rtn == 0) & !permKeys) {
		/* be nice: if we ran OK delete the cruft DB we created */
		unlink(dbName);
	}
	return rtn;
}
