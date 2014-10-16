/* Copyright (c) 1998,2003-2005 Apple Computer, Inc.
 *
 * miniWrap.c - simple key wrap/unwrap exerciser.
 *
 * Revision History
 * ----------------
 *   4 May 2000  Doug Mitchell
 *		Ported to X/CDSA2. 
 *  22 May 1998 Doug Mitchell at Apple
 *		Created.
 */
 
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <Security/cssm.h>
#include "cspwrap.h"
#include "common.h"
#include "cspdlTesting.h"

/*
 * Temporary hack to use CSSM_KEYBLOB_WRAPPED_FORMAT_{PKCS7,PKCS8}, which
 * are no longer supported as of 7/28/00
 */
#define PKCS8_FORMAT_ENABLE		1
#define PKCS7_FORMAT_ENABLE		0

#define ENCR_USAGE_NAME		"noLabel"
#define ENCR_USAGE_NAME_LEN	(strlen(ENCR_USAGE_NAME))
#define WRAP_USAGE_NAME		"noWrapLabel"
#define WRAP_USAGE_NAME_LEN	(strlen(WRAP_USAGE_NAME))
#define LOOPS_DEF		10
#define MAX_PTEXT_SIZE	1000
#define LOOP_PAUSE		10

/*
 * A new restriction for X: when wrapping using an RSA key, you can't
 * wrap a key which is  bigger than the RSA key itself because the
 * wrap (Encrypt) is a one-shot deal, unlike the OS9 CSP which 
 * handled multiple chunks. This only effectively restricts the
 * use of an RSA key to wrap symmetric keys, which doesn't seem like
 * an unreasonable restriction. 
 */
#define RSA_WRAP_RESTRICTION		1

/*
 * Currently the CSP can use wrapping keys flagged exclusively for wrapping
 * (CSSM_KEYUSE_{WRAP,UNWRAP} for the actual wrap sinceÊthe wrap/unwrap op is 
 * done with an encrypt/decrypt op. The WrapKey op doesn't even see the 
 * wrapping key - it's in the context we pass it. Thus for now wrap/unwrap
 * keys have to be marked with CSSM_KEYUSE_ANY.
 */
#define WRAP_USAGE_ANY			0

static void usage(char **argv)
{
	printf("usage: %s [options]\n", argv[0]);
	printf("   Options:\n");
	printf("   f (only wrap RSA private key)\n");
	printf("   d (only wrap DES key)\n");
	printf("   S (do symmetric wrap only)\n");
	printf("   a (do asymmetric wrap only)\n");
	printf("   n (do null wrap only)\n");
	printf("   m (dump malloc info)\n");
	printf("   r (ref keys only)\n");
	printf("   w (wrap only)\n");
	printf("   e (export)\n");
	printf("   q (quiet)\n");
	printf("   k (force PKCS7/8)\n");
	#if		PKCS7_FORMAT_ENABLE || PKCS8_FORMAT_ENABLE
	printf("   K (skip PKCS7/8) (pkcs normally enable)\n");
	#else
	printf("   K (allow PKCS7/8) (pkcs normally disabled)\n");
	#endif	/* PKCS_FORMAT_ENABLE */
	printf("   D (CSP/DL; default = bare CSP)\n");
	printf("   l=loops (default=%d; 0=forever)\n", LOOPS_DEF);
	printf("   p(ause every %d loops)\n", LOOP_PAUSE);
	printf("   h(elp)\n");
	exit(1);
}

/* not all algs need this, pass it in anyway */
CSSM_DATA initVector = {8, (uint8 *)"someVect"};

/* 
 * local verbose wrap/unwrap functions.
 */
/* wrap key function. */
static CSSM_RETURN wrapKey(CSSM_CSP_HANDLE cspHand,
	const CSSM_KEY_PTR		unwrappedKey,		// must be ref
	const CSSM_KEY_PTR		wrappingKey,
	CSSM_ALGORITHMS			wrapAlg,
	CSSM_ENCRYPT_MODE		wrapMode,
	CSSM_KEYBLOB_FORMAT		wrapFormat,			// NONE, PKCS7, PKCS8
	CSSM_PADDING 			wrapPad,
	CSSM_KEY_PTR			wrappedKey)			// RETURNED
{
	CSSM_CC_HANDLE		ccHand;
	CSSM_RETURN			crtn;
	CSSM_RETURN			crtn2;
	#if	WRAP_KEY_REQUIRES_CREDS
	CSSM_ACCESS_CREDENTIALS	creds;
	#endif
	
	#if 0
	if(unwrappedKey->KeyHeader.BlobType != CSSM_KEYBLOB_REFERENCE) {
		printf("Hey! you can only wrap a reference key!\n");
		return CSSM_ERRCODE_INTERNAL_ERROR;
	}
	#endif
	memset(wrappedKey, 0, sizeof(CSSM_KEY));
	memset(&creds, 0, sizeof(CSSM_ACCESS_CREDENTIALS));
	/* special case for NULL wrap - no wrapping key */
	if((wrappingKey == NULL) ||
	   (wrappingKey->KeyHeader.KeyClass == CSSM_KEYCLASS_SESSION_KEY)) {
		crtn = CSSM_CSP_CreateSymmetricContext(cspHand,
				wrapAlg,
				wrapMode,
				&creds,					// accessCred
				wrappingKey,
				&initVector,
				wrapPad,				// Padding
				NULL,					// Reserved
				&ccHand);
		if(crtn) {
			printError("cspWrapKey/CreateContext", crtn);
			return CSSM_ERRCODE_INTERNAL_ERROR;
		}
	}
	else {
		crtn = CSSM_CSP_CreateAsymmetricContext(cspHand,
				wrapAlg,
				&creds,			// passPhrase
				wrappingKey,
				wrapPad,		// Padding
				&ccHand);
		if(crtn) {
			printError("cspWrapKey/CreateContext", crtn);
			return CSSM_ERRCODE_INTERNAL_ERROR;
		}
		/* CMS requires 8-byte IV */
		crtn = AddContextAttribute(ccHand,
			CSSM_ATTRIBUTE_INIT_VECTOR,
			sizeof(CSSM_DATA),
			CAT_Ptr,
			&initVector,
			0);
		if(crtn) {
			printError("CSSM_UpdateContextAttributes", crtn);
			return crtn;
		}
	}
	if(wrapFormat != CSSM_KEYBLOB_WRAPPED_FORMAT_NONE) {
		/* only add this attribute if it's not the default */
		CSSM_CONTEXT_ATTRIBUTE attr;
		attr.AttributeType = CSSM_ATTRIBUTE_WRAPPED_KEY_FORMAT;
		attr.AttributeLength = sizeof(uint32);
		attr.Attribute.Uint32 = wrapFormat;
		crtn = CSSM_UpdateContextAttributes(
			ccHand,
			1,
			&attr);
		if(crtn) {
			printError("CSSM_UpdateContextAttributes", crtn);
			return crtn;
		}
	}
	crtn = CSSM_WrapKey(ccHand,
		#if	WRAP_KEY_REQUIRES_CREDS
		&creds,
		#else
		NULL,			// AccessCred
		#endif
		unwrappedKey,
		NULL,			// DescriptiveData
		wrappedKey);
	if(crtn != CSSM_OK) {
		printError("CSSM_WrapKey", crtn);
	}
	if((crtn2 = CSSM_DeleteContext(ccHand))) {
		printError("CSSM_DeleteContext", crtn2);
	}
	return crtn;
}

/* unwrap key function. */
static CSSM_RETURN unwrapKey(CSSM_CSP_HANDLE cspHand,
	const CSSM_KEY_PTR		wrappedKey,
	const CSSM_KEY_PTR		unwrappingKey,
	CSSM_ALGORITHMS			unwrapAlg,
	CSSM_ENCRYPT_MODE		unwrapMode,
	CSSM_PADDING 			unwrapPad,
	CSSM_KEY_PTR			unwrappedKey,		// RETURNED
	const unsigned char 	*keyLabel,
	unsigned 				keyLabelLen)
{
	CSSM_CC_HANDLE		ccHand;
	CSSM_RETURN			crtn;
	CSSM_RETURN			crtn2;
	CSSM_DATA			labelData;
	uint32				keyAttr;
	CSSM_DATA			descData = { 0, NULL };
	CSSM_ACCESS_CREDENTIALS	creds;
	 
	memset(unwrappedKey, 0, sizeof(CSSM_KEY));
	memset(&creds, 0, sizeof(CSSM_ACCESS_CREDENTIALS));
	if((unwrappingKey == NULL) ||
	   (unwrappingKey->KeyHeader.KeyClass == CSSM_KEYCLASS_SESSION_KEY)) {
		crtn = CSSM_CSP_CreateSymmetricContext(cspHand,
				unwrapAlg,
				unwrapMode,
				&creds,				// accessCreds
				unwrappingKey,
				&initVector,	
				unwrapPad,			// Padding
				0,					// Reserved
				&ccHand);
		if(crtn) {
			printError("cspUnwrapKey/CreateContext", crtn);
			return CSSM_ERRCODE_INTERNAL_ERROR;
		}
	}
	else {
		crtn = CSSM_CSP_CreateAsymmetricContext(cspHand,
				unwrapAlg,
				&creds,			// passPhrase,
				unwrappingKey,
				unwrapPad,		// Padding
				&ccHand);
		if(crtn) {
			printError("cspUnwrapKey/CreateContext", crtn);
			return CSSM_ERRCODE_INTERNAL_ERROR;
		}
		/* CMS requires 8-byte IV */
		crtn = AddContextAttribute(ccHand,
			CSSM_ATTRIBUTE_INIT_VECTOR,
			sizeof(CSSM_DATA),
			CAT_Ptr,
			&initVector,
			0);
		if(crtn) {
			printError("CSSM_UpdateContextAttributes", crtn);
			return crtn;
		}
	}
	labelData.Data = (uint8 *)keyLabel;
	labelData.Length = keyLabelLen;
	
	/*
	 * New keyAttr - clear some old bits, make sure we ask for ref key
	 */
	keyAttr = wrappedKey->KeyHeader.KeyAttr;
	keyAttr &= ~(CSSM_KEYATTR_ALWAYS_SENSITIVE | CSSM_KEYATTR_NEVER_EXTRACTABLE);
	keyAttr |= CSSM_KEYATTR_RETURN_REF;
	crtn = CSSM_UnwrapKey(ccHand,
		NULL,		// PublicKey
		wrappedKey,
		CSSM_KEYUSE_ANY,		// FIXME
		keyAttr,
		&labelData,
		NULL,					// CredAndAclEntry
		unwrappedKey,
		&descData);				// required 
	if(crtn != CSSM_OK) {
		printError("CSSM_UnwrapKey", crtn);
	}
	if((crtn2 = CSSM_DeleteContext(ccHand))) {
		printError("CSSM_DeleteContext", crtn2);
	}
	return crtn;
}

#define UNWRAPPED_LABEL	"unwrapped thing"
#define NULL_TEST	0
#if		NULL_TEST

static int doTest(CSSM_CSP_HANDLE cspHand,
	CSSM_KEY_PTR encrKey,
	CSSM_KEY_PTR decrKey,		// we wrap this one
	CSSM_KEY_PTR wrappingKey,	// ...using this key, NULL for null wrap
	CSSM_KEY_PTR unwrappingKey,
	CSSM_ALGORITHMS wrapAlg,
	CSSM_ENCRYPT_MODE wrapMode,
	CSSM_PADDING wrapPad,
	CSSM_ALGORITHMS encrAlg,
	CSSM_ENCRYPT_MODE encrMode,
	CSSM_PADDING encrPad,
	CSSM_BOOL wrapOnly,
	uint32 maxPtextSize,		// max size to encrypt
	CSSM_BOOL quiet)
{
	return 0;
}
#else	/* NULL_TEST */
/*
 * NULL Wrapping decrKey - a private key - only works for DEBUG CSPs. 
 * We'll always wrap decrKey, except for NULL wrap when 
 * NULL_WRAP_DECR_KEY is false. 
 */
#define NULL_WRAP_DECR_KEY	1
 
static int doTest(CSSM_CSP_HANDLE cspHand,
	CSSM_KEY_PTR encrKey,		// we wrap this one
	CSSM_KEY_PTR decrKey,		// ...or this one, depending on WRAP_DECR_KEY
	CSSM_KEY_PTR wrappingKey,	// ...using this key, NULL for null wrap
	CSSM_KEY_PTR unwrappingKey,
	CSSM_ALGORITHMS wrapAlg,
	CSSM_ENCRYPT_MODE wrapMode,
	CSSM_KEYBLOB_FORMAT	wrapFormat,	// NONE, PKCS7, PKCS8
	CSSM_PADDING wrapPad,
	CSSM_ALGORITHMS encrAlg,
	CSSM_ENCRYPT_MODE encrMode,
	CSSM_PADDING encrPad,
	CSSM_BOOL wrapOnly,
	uint32 maxPtextSize,		// max size to encrypt
	CSSM_BOOL quiet)
{
	CSSM_DATA	ptext;
	CSSM_DATA	ctext;
	CSSM_DATA	rptext;
	CSSM_KEY	wrappedKey;
	CSSM_KEY	unwrappedKey;
	CSSM_RETURN	crtn;
	CSSM_KEY_PTR	realEncrKey;	// encrKey or &unwrappedKey
	CSSM_KEY_PTR	realDecrKey;	// decrKey or &unwrappedKey
	
	/* wrap decrKey or encrKey using wrappingKey ==> wrappedKey */
	if((wrappingKey == NULL) && !NULL_WRAP_DECR_KEY) {
		/* NULL wrap of pub key */
		crtn = wrapKey(cspHand,
			encrKey,
			wrappingKey,
			wrapAlg,
			wrapMode,
			wrapFormat,
			wrapPad,
			&wrappedKey);
		realEncrKey = &unwrappedKey;
		realDecrKey = decrKey;
	}
	else {
		/* normal case, wrap priv key (may be NULL if NULL_WRAP_DECR_KEY) */
		crtn = wrapKey(cspHand,
			decrKey,
			wrappingKey,
			wrapAlg,
			wrapMode,
			wrapFormat,
			wrapPad,
			&wrappedKey);
		realEncrKey = encrKey;
		realDecrKey = &unwrappedKey;
	}
	
	if(crtn) {
		return testError(quiet);
	}
	if((wrappingKey != NULL) &&	// skip for NULL wrap
	   (wrapFormat != CSSM_KEYBLOB_WRAPPED_FORMAT_NONE)) {
		/* don't want default, verify we got what we want */
		if(wrappedKey.KeyHeader.Format != wrapFormat) {
			printf("wrapped key format mismatch: expect %u; got %u\n",
				(unsigned)wrapFormat, (unsigned)wrappedKey.KeyHeader.Format);
			if(testError(quiet)) {
				return 1;
			}
		}
	}
	if(wrapOnly) {
		cspFreeKey(cspHand, &wrappedKey);
		goto done;
	}
	/* unwrap wrappedKey using unwrappingKey ==> unwrappedKey; */
	crtn = unwrapKey(cspHand,
		&wrappedKey,
		unwrappingKey,
		wrapAlg,
		wrapMode,
		wrapPad,
		&unwrappedKey,
		(uint8 *)UNWRAPPED_LABEL,
		15);
	if(crtn) {
		return testError(quiet);
	}

	/* cook up ptext */
	ptext.Data = (uint8 *)CSSM_MALLOC(maxPtextSize);
	simpleGenData(&ptext, 1, maxPtextSize);
	/* encrypt using realEncrKey ==> ctext */
	ctext.Data = NULL;
	ctext.Length = 0;
	crtn = cspEncrypt(cspHand,
		encrAlg,
		encrMode,
		encrPad,
		realEncrKey,
		NULL,		// no 2nd key
		0,			// effectiveKeySize
		0,			// rounds
		&initVector,
		&ptext,
		&ctext,
		CSSM_TRUE);	// mallocCtext
	if(crtn) {
		return testError(quiet);
	}

	/* decrypt ctext with realDecrKey ==> rptext; */
	rptext.Data = NULL;
	rptext.Length = 0;
	crtn = cspDecrypt(cspHand,
		encrAlg,
		encrMode,
		encrPad,
		realDecrKey,
		NULL,			// no 2nd key
		0,				// effectiveKeySize
		0,				// rounds
		&initVector,
		&ctext,
		&rptext,
		CSSM_TRUE);
	if(crtn) {
		return testError(quiet);
	}

	/* compare ptext vs. rptext; */
	if(ptext.Length != rptext.Length) {
		printf("ptext length mismatch\n");
		return testError(quiet);
	}
	if(memcmp(ptext.Data, rptext.Data, ptext.Length)) {
		printf("***data miscompare\n");
		return testError(quiet);
	}
	/* free resources */
	cspFreeKey(cspHand, &wrappedKey);
	cspFreeKey(cspHand, &unwrappedKey);
	CSSM_FREE(ptext.Data);
	CSSM_FREE(ctext.Data);
	CSSM_FREE(rptext.Data);
done:
	return 0;
}
#endif	/* NULL_TEST */

int main(int argc, char **argv)
{
	int						arg;
	char					*argp;
	int 					i;
	CSSM_CSP_HANDLE 		cspHand;
	CSSM_RETURN				crtn;
	CSSM_KEY				origPub;		// we generate if !desSubj
	CSSM_KEY				origPriv;
	CSSM_KEY_PTR			origSess;		// we generate if desSubj
	CSSM_KEY_PTR			origEncrKey;	// pts to origPub or origSess
	CSSM_KEY_PTR			origDecrKey;	// pts to origPriv or origSess
	CSSM_ALGORITHMS			encrAlg;
	CSSM_ENCRYPT_MODE		encrMode;
	CSSM_PADDING			encrPad;
	int						rtn = 0;
	CSSM_BOOL				genRsaKey;
	uint32					maxPtextSize;
	CSSM_BOOL				encrIsRef = CSSM_TRUE;
	CSSM_BOOL				decrIsRef = CSSM_TRUE;
	CSSM_KEYBLOB_FORMAT		wrapFormat = CSSM_KEYBLOB_WRAPPED_FORMAT_NONE;
	unsigned				loop;
	
	/* user-specified vars */
	unsigned				loops = LOOPS_DEF;
	CSSM_BOOL				pause = CSSM_FALSE;
	CSSM_BOOL				doSymmWrap = CSSM_TRUE;
	CSSM_BOOL				doAsymmWrap = CSSM_TRUE;
	CSSM_BOOL				doNullWrap = CSSM_TRUE;
	CSSM_BOOL				doSymmEncrOnly = CSSM_FALSE;
	CSSM_BOOL				doAsymmEncrOnly = CSSM_FALSE;
	CSSM_BOOL				wrapOnly = CSSM_FALSE;
	CSSM_BOOL				quiet = CSSM_FALSE;
	CSSM_BOOL				bareCsp = CSSM_TRUE;
	CSSM_BOOL				forcePkcs = CSSM_FALSE;
	CSSM_BOOL				refKeysOnly = CSSM_FALSE;
	#if	PKCS_FORMAT_ENABLE
	CSSM_BOOL				skipPkcs = CSSM_FALSE;
	#else
	CSSM_BOOL				skipPkcs = CSSM_TRUE;
	#endif
	
	for(arg=1; arg<argc; arg++) {
		argp = argv[arg];
		switch(argp[0]) {
			case 'S':
				doAsymmWrap = CSSM_FALSE;
				doNullWrap = CSSM_FALSE;
				break;
			case 'a':
				doSymmWrap = CSSM_FALSE;
				doNullWrap = CSSM_FALSE;
				break;
			case 'n':
				doSymmWrap = CSSM_FALSE;
				doAsymmWrap = CSSM_FALSE;
				break;
			case 'f':
				doAsymmEncrOnly = CSSM_TRUE;
				break;
			case 'd':		// symmetric encrypt only option
			case 'e':		// export option - avoids asymetric encrypt/decrypt
				doSymmEncrOnly = CSSM_TRUE;
				break;
		    case 'l':
				loops = atoi(&argp[2]);
				break;
			case 'w':
				wrapOnly = CSSM_TRUE;
				break;
			case 'p':
				pause = CSSM_TRUE;
				break;
			case 'D':
				bareCsp = CSSM_FALSE;
				#if CSPDL_ALL_KEYS_ARE_REF
		    	refKeysOnly = CSSM_TRUE;
				#endif
				break;
			case 'k':
				forcePkcs = CSSM_TRUE;
				break;
			case 'K':
				#if		PKCS7_FORMAT_ENABLE || PKCS8_FORMAT_ENABLE
				skipPkcs = CSSM_TRUE;
				#else
				skipPkcs = CSSM_FALSE;
				#endif
				break;
			case 'r':
				refKeysOnly = CSSM_TRUE;
				break;
			case 'q':
				quiet = CSSM_TRUE;
				break;
			default:
				usage(argv);
		}
	}

	#if 0
	#if	!PKCS_FORMAT_ENABLE
	if(skipPkcs) {
		if(doAsymmEncrOnly) {
			printf("Asymmetric keys can only be wrapped via PKCS; aborting\n");
			usage(argv);
		}
		else if(!doSymmWrap) {
			printf("AsymmetricWrapping can only be done via PKCS; aborting\n");
			usage(argv);
		}
		doSymmEncrOnly = CSSM_TRUE;
		doSymmWrap = CSSM_TRUE;
		doAsymmWrap = CSSM_FALSE;
	}
	#endif	/* !PKCS_FORMAT_ENABLE */
	#endif
	
	cspHand = cspDlDbStartup(bareCsp, NULL);
	if(cspHand == 0) {
		exit(1);
	}
	
	printf("Starting miniWrap; args: ");
	for(i=1; i<argc; i++) {
		printf("%s ", argv[i]);
	}
	printf("\n");
	
	for(loop=1; ; loop++) {
		if((loop % LOOP_PAUSE) == 0) {
			if(!quiet) {
				printf("...loop %d\n", loop);
			}
			if(pause) {
				fpurge(stdin);
				printf("Hit CR to proceed: ");
				getchar();
			}
		}
		
		/* mix up ref and raw keys - with X, we can wrap a raw key */
		if(!refKeysOnly) {
			encrIsRef   = (loop & 2) ? CSSM_TRUE : CSSM_FALSE;
			decrIsRef   = (loop & 4) ? CSSM_TRUE : CSSM_FALSE;
		}
		
		/* first generate key to be wrapped */
		if(!doAsymmEncrOnly && (doSymmEncrOnly || ((loop & 1) == 0))) {
			if(!quiet) {
				printf("...wrapping DES key (%s)\n", 
					encrIsRef ? "ref" : "raw");
			}
			origSess = cspGenSymKey(cspHand,
				CSSM_ALGID_DES,
				ENCR_USAGE_NAME,
				ENCR_USAGE_NAME_LEN,
				CSSM_KEYUSE_ENCRYPT | CSSM_KEYUSE_DECRYPT,
				CSP_KEY_SIZE_DEFAULT,
				encrIsRef);			
			if(origSess == NULL) {
				rtn = 1;
				goto testDone;
			}
			origDecrKey = origEncrKey = origSess;
			encrAlg = CSSM_ALGID_DES;
			encrMode = CSSM_ALGMODE_CBCPadIV8;
			encrPad = CSSM_PADDING_PKCS5;
			maxPtextSize = MAX_PTEXT_SIZE;	// i.e., unlimited
		}
		else {
			origSess = NULL;
		}
		if(!doSymmEncrOnly && (doAsymmEncrOnly || ((loop & 1) == 1))) {
			if(!quiet) {
				printf("...wrapping RSA key (pub %s priv %s)\n",
					(encrIsRef ? "ref" : "raw"),
					(decrIsRef ? "ref" : "raw"));
			}
			crtn = cspGenKeyPair(cspHand,
				CSSM_ALGID_RSA,
				ENCR_USAGE_NAME,
				ENCR_USAGE_NAME_LEN,
				CSP_KEY_SIZE_DEFAULT,
				&origPub,
				encrIsRef,			// pubIsRef
				CSSM_KEYUSE_ENCRYPT,
				CSSM_KEYBLOB_RAW_FORMAT_NONE,
				&origPriv,
				decrIsRef,			// privIsRef
				CSSM_KEYUSE_DECRYPT,
				CSSM_KEYBLOB_RAW_FORMAT_NONE,
				CSSM_FALSE);		// genSeed
			if(crtn) {
				rtn = 1;
				goto testDone;
			}
			origDecrKey = &origPriv;
			origEncrKey = &origPub;
			encrAlg = CSSM_ALGID_RSA;
			encrMode = CSSM_ALGMODE_NONE;
			encrPad = CSSM_PADDING_PKCS1;
			genRsaKey = CSSM_TRUE;
			maxPtextSize = origPriv.KeyHeader.LogicalKeySizeInBits / 8;
			// a weird BSAFE requirement which is not documented
			maxPtextSize -= 11;
		}
		else {
			genRsaKey = CSSM_FALSE;
		}
		
		/* now the tests, symmetric and/or asymmetric wrapping */
		if(doSymmWrap) {
			CSSM_KEY_PTR wrapKey;
			if(!quiet) {
				printf("   ...Doing symmetric wrap\n");
			}
			wrapKey = cspGenSymKey(cspHand,
				CSSM_ALGID_DES,
				WRAP_USAGE_NAME,
				WRAP_USAGE_NAME_LEN,
				/* for now, wrapping keys have to have keyuse_any */
				/* CSSM_KEYUSE_WRAP | CSSM_KEYUSE_UNWRAP, */
				WRAP_USAGE_ANY ? CSSM_KEYUSE_ANY :
						CSSM_KEYUSE_WRAP | CSSM_KEYUSE_UNWRAP,
				CSP_KEY_SIZE_DEFAULT,
				CSSM_TRUE);		// FIXME - try both
			if(wrapKey == NULL) {
				rtn = 1;
				goto testDone;
			}
			if(forcePkcs) {
				/* symmetric wrapping key ==> PKCS7 */
				wrapFormat = CSSM_KEYBLOB_WRAPPED_FORMAT_PKCS7;
			}
			else {
				/* default */
				wrapFormat = CSSM_KEYBLOB_WRAPPED_FORMAT_NONE;
			}
			if(doTest(cspHand,
					origEncrKey,
					origDecrKey,
					wrapKey,
					wrapKey,
					CSSM_ALGID_DES,				// wrapAlg
					CSSM_ALGMODE_CBCPadIV8,		// wrapMode
					wrapFormat,
					CSSM_PADDING_PKCS5,			// wrapPad
					encrAlg,
					encrMode,
					encrPad,
					wrapOnly,
					maxPtextSize,
					quiet)) {
				rtn = 1;
				goto testDone;
			}
			cspFreeKey(cspHand, wrapKey);
			CSSM_FREE(wrapKey);					// mallocd by cspGenSymKey
			wrapKey = NULL;
		}
		if(doAsymmWrap &&
		   !(RSA_WRAP_RESTRICTION && (origEncrKey != origDecrKey))) {
			/* skip wrapping asymmetric key with asymmetric key */
			CSSM_KEY wrapPrivKey;
			CSSM_KEY wrapPubKey;
			
			if(!quiet) {
				printf("   ...Doing asymmetric wrap\n");
			}
			crtn = cspGenKeyPair(cspHand,
				CSSM_ALGID_RSA,
				WRAP_USAGE_NAME,
				WRAP_USAGE_NAME_LEN,
				CSP_RSA_KEY_SIZE_DEFAULT,	
				&wrapPubKey,
				CSSM_TRUE,				// both are ref
				WRAP_USAGE_ANY ? CSSM_KEYUSE_ANY : CSSM_KEYUSE_WRAP,
				CSSM_KEYBLOB_RAW_FORMAT_NONE,
				&wrapPrivKey,
				CSSM_TRUE,				// FIXME privIsRef
				WRAP_USAGE_ANY ? CSSM_KEYUSE_ANY : CSSM_KEYUSE_UNWRAP,
				CSSM_KEYBLOB_RAW_FORMAT_NONE,
				CSSM_FALSE);			// genSeed
			if(crtn) {
				rtn = 1;
				goto testDone;
			}
			if(forcePkcs) {
				/* asymmetric wrapping key ==> PKCS8 */
				wrapFormat = CSSM_KEYBLOB_WRAPPED_FORMAT_PKCS8;
			}
			else {
				wrapFormat = CSSM_KEYBLOB_WRAPPED_FORMAT_NONE;
			}
			if(doTest(cspHand,
					origEncrKey,
					origDecrKey,
					&wrapPubKey,
					&wrapPrivKey,
					CSSM_ALGID_RSA,			// wrapAlg
					CSSM_ALGMODE_NONE,		// wrapMode
					wrapFormat,
					CSSM_PADDING_PKCS1,		// wrapPad
					encrAlg,
					encrMode,
					encrPad,
					wrapOnly,
					maxPtextSize,
					quiet)) {
				rtn = 1;
				goto testDone;
			}
			cspFreeKey(cspHand, &wrapPubKey);
			cspFreeKey(cspHand, &wrapPrivKey);	
		}
		//if(doNullWrap && (origDecrKey != origEncrKey)) {
		if(doNullWrap) {
			/* with X, we can do NULL wrap/unwrap of any key */
			if(!quiet) {
				printf("   ...Doing NULL wrap\n");
			}
			if(doTest(cspHand,
					origEncrKey,
					origDecrKey,
					NULL,
					NULL,
					CSSM_ALGID_NONE,		// wrapAlg
					CSSM_ALGMODE_NONE,		// wrapMode
					CSSM_KEYBLOB_WRAPPED_FORMAT_NONE,
					CSSM_PADDING_NONE,		// wrapPad
					encrAlg,
					encrMode,
					encrPad,
					wrapOnly,
					maxPtextSize,
					quiet)) {
				rtn = 1;
				goto testDone;
			}
		}
		
		if(origSess != NULL) {
			cspFreeKey(cspHand, origSess);
			CSSM_FREE(origSess);
		}
		if(genRsaKey) {
			cspFreeKey(cspHand, &origPub);
			cspFreeKey(cspHand, &origPriv);
		}
		if(loops && (loop == loops)) {
			break;
		}
	}
testDone:
	CSSM_ModuleDetach(cspHand);
	if((rtn == 0) && !quiet) {
		printf("%s test complete\n", argv[0]);
	}
	return rtn;
}
