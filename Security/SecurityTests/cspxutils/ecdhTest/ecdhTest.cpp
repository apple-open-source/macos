/*
 * Copyright (c) 2008 Apple Inc. All Rights Reserved.
 * 
 * ecdhTest.cpp - Test Elliptic Curve Diffie-Hellman key exchange.
 *
 * Created Jan. 1 2008 by Doug Mitchell. 
 */
 
#include <stdlib.h>
#include <strings.h>
#include <stdio.h>
#include <unistd.h>
#include <Security/cssm.h>
#include "cspwrap.h"
#include "common.h"

#define LOOPS_DEF			32
#define KEY_SIZE_DEF		256

static void usage(char **argv)
{
	printf("usage: %s [options]\n", argv[0]);
	printf("Options:\n");
	printf("  k=keySize (default = %d)\n", KEY_SIZE_DEF);
	printf("  X (X9.63 key derivation)\n");
	printf("  l=loops (0=forever)\n");
	printf("  D (CSP/DL; default = bare CSP)\n");
	printf("  q(uiet)\n");
	printf("  v(erbose))\n");
	exit(1);
}

#define LABEL_DEF				"noLabel"
#define MAX_SHARED_INFO_LEN		400
#define MAX_DERIVED_SIZE		1024

static int doECDH(
	CSSM_CSP_HANDLE cspHand,
	CSSM_KEY_PTR	privKey,
	/* 
	 * pubKey:
	 *   Ref form - use key as pubKey as is
	 *   X509 form - use as is
	 *   OCTET_STRING form - use key data as Param
	 */
	CSSM_KEY_PTR	pubKey,
	CSSM_BOOL		bareCsp,	// false --> derive ref key and NULL-wrap it
	CSSM_BOOL		x963KDF,
	CSSM_DATA		*sharedInfo,
	uint32			deriveSizeInBits,
	CSSM_BOOL		quiet,
	CSSM_BOOL		verbose,
	
	/* result RETURNED here */
	CSSM_KEY_PTR	derivedKey)
	
{
	CSSM_DATA paramData = {0, NULL};
	CSSM_KEY_PTR contextPubKey = NULL;
	CSSM_KEYHEADER_PTR hdr = &pubKey->KeyHeader;
	
	if((hdr->BlobType == CSSM_KEYBLOB_RAW) && 
	   (hdr->Format == CSSM_KEYBLOB_RAW_FORMAT_OCTET_STRING)) {
		paramData = pubKey->KeyData;
	}
	else {
		contextPubKey = pubKey;
	}
	
	/* create key derivation context */
	CSSM_RETURN 			crtn;
	CSSM_ACCESS_CREDENTIALS	creds;
	CSSM_CC_HANDLE			ccHand;
	
	memset(&creds, 0, sizeof(CSSM_ACCESS_CREDENTIALS));
	
	CSSM_ALGORITHMS deriveAlg;
	if(x963KDF) {
		deriveAlg = CSSM_ALGID_ECDH_X963_KDF;
	}
	else {
		deriveAlg = CSSM_ALGID_ECDH;
	}
	
	crtn = CSSM_CSP_CreateDeriveKeyContext(cspHand,
		deriveAlg,
		CSSM_ALGID_RC4,	// doesn't matter, just give us the bits
		deriveSizeInBits,
		&creds,
		privKey,		// BaseKey
		0,				// IterationCount
		sharedInfo,		// Salt
		0,				// Seed
		&ccHand);
	if(crtn) {
		printError("CSSM_CSP_CreateDeriveKeyContext", crtn);
		return testError(quiet);
	}
	
	if(contextPubKey != NULL) {
		/* add pub key as a context attr */
		crtn = AddContextAttribute(ccHand,
			CSSM_ATTRIBUTE_PUBLIC_KEY,
			sizeof(CSSM_KEY),	
			CAT_Ptr,
			(void *)contextPubKey,
			0);
		if(crtn) {
			printError("AddContextAttribute(CSSM_ATTRIBUTE_PUBLIC_KEY)",
				crtn);
			return crtn;
		}
	}
	
	/* D-H derive key */
	CSSM_DATA	labelData = { strlen(LABEL_DEF), (uint8 *)LABEL_DEF };
	CSSM_KEYATTR_FLAGS keyAttr = bareCsp ? 
		(CSSM_KEYATTR_RETURN_DATA | CSSM_KEYATTR_EXTRACTABLE) :
		(CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_EXTRACTABLE);
	memset(derivedKey, 0, sizeof(CSSM_KEY));
	crtn = CSSM_DeriveKey(ccHand,
		&paramData,
		CSSM_KEYUSE_ANY,
		keyAttr,
		&labelData,
		NULL,				// cread/acl
		derivedKey);
	if(crtn) {
		printError("CSSM_DeriveKey", crtn);
	}
	CSSM_DeleteContext(ccHand);
	if(crtn) {
		return testError(quiet);
	}
	
	if(!bareCsp) {
		/* Got a ref key, give caller raw */
		CSSM_KEY refKey = *derivedKey;
		crtn = cspRefKeyToRaw(cspHand, &refKey, derivedKey);
		cspFreeKey(cspHand, &refKey);
	}
	return 0;
}

/* define public key style */
typedef enum {
	PKT_Ref,		/* ref key */
	PKT_Wrap,		/* generate ref key, wrap to OCTET_STRING */
	PKT_X509,		/* raw key X509 format */
	PKT_Octet		/* generate to OCTET_STRING form */
} PubKeyType;

#define BoolStr(v)	(v ? "true " : "false")

static const char *KeyStypeStr(
	PubKeyType keyType)
{
	switch(keyType) {
		case PKT_Ref:	return "Ref";
		case PKT_Wrap:	return "Ref->Wrap";
		case PKT_X509:	return "X509";
		case PKT_Octet:	return "X9.62";
		default:		return "BRRZAP";
	}
}

static int doTest(
	CSSM_CSP_HANDLE cspHand,
	CSSM_BOOL		ourKeysRef,			/* our keys are reference */
	CSSM_BOOL		theirPrivKeyRef,	/* their private key is reference */
	PubKeyType		theirPubKeyType,
	unsigned		keySizeBits,
	CSSM_BOOL		bareCsp,
	CSSM_BOOL		x963KDF,
	CSSM_BOOL		useSharedInfo,		/* use the optional SharedInfo for x963KDF */
	CSSM_BOOL		verbose,
	CSSM_BOOL		quiet)
{
	
	CSSM_RETURN crtn;
	CSSM_KEY ourPriv;
	CSSM_KEY ourPub;
	bool ourKeysGend = false;
	bool theirKeysGend = false;
	bool wrappedTheirPub = false;
	bool wrappedOurPub = false;
	bool derivedKey1 = false;
	bool derivedKey2 = false;
	CSSM_DATA sharedInfo = {0, NULL};
	uint32 deriveSizeInBits;

	if(x963KDF) {
		/* arbitrary derived size */
		deriveSizeInBits = genRand(1, MAX_DERIVED_SIZE);
	}
	else {
		deriveSizeInBits = keySizeBits;
	}
	if(useSharedInfo) {
		/* length should be totally arbitrary */
		appSetupCssmData(&sharedInfo, MAX_SHARED_INFO_LEN);
		simpleGenData(&sharedInfo, 1, MAX_SHARED_INFO_LEN);
	}
	
	
	if(!quiet) {
		if(x963KDF) {
			printf("...sharedInfoLen %4lu deriveSize %4lu ",
				(unsigned long)sharedInfo.Length, (unsigned long)deriveSizeInBits);
		}
		else {
			printf("...");
		}
		printf("ourRef %s theirPrivRef %s theirPub %s\n",
			BoolStr(ourKeysRef), BoolStr(theirPrivKeyRef), 
			KeyStypeStr(theirPubKeyType));
	}
	
	crtn = cspGenKeyPair(cspHand, CSSM_ALGID_ECDSA, 
		LABEL_DEF, strlen(LABEL_DEF), keySizeBits, 
		&ourPub, ourKeysRef, CSSM_KEYUSE_DERIVE, CSSM_KEYBLOB_RAW_FORMAT_NONE,
		&ourPriv, ourKeysRef, CSSM_KEYUSE_DERIVE, CSSM_KEYBLOB_RAW_FORMAT_NONE,
		CSSM_FALSE);
	if(crtn) {
		return testError(quiet);
	}
	ourKeysGend = true;
	
	CSSM_KEY theirPriv;
	CSSM_KEY theirPub;			/* the generated one */
	CSSM_KEY theirWrappedPub;	/* optional NULL unwrap */
	CSSM_KEY_PTR theirPubPtr;
	CSSM_KEY ourWrappedPub;	/* optional NULL unwrap */
	CSSM_KEY_PTR ourPubPtr;
	CSSM_KEY derived1;
	CSSM_KEY derived2;
	CSSM_BOOL pubIsRef = CSSM_FALSE;
	CSSM_KEYBLOB_FORMAT blobForm = CSSM_KEYBLOB_RAW_FORMAT_NONE;
	int ourRtn = 0;
	
	switch(theirPubKeyType) {
		case PKT_Ref:
		case PKT_Wrap:
			pubIsRef = CSSM_TRUE;
			break;
		case PKT_X509:
			pubIsRef = CSSM_FALSE;
			break;
		case PKT_Octet:
			pubIsRef = CSSM_FALSE;
			blobForm = CSSM_KEYBLOB_RAW_FORMAT_OCTET_STRING;
			break;
	}
	
	crtn = cspGenKeyPair(cspHand, CSSM_ALGID_ECDSA, 
		LABEL_DEF, strlen(LABEL_DEF), keySizeBits, 
		&theirPub, pubIsRef, CSSM_KEYUSE_DERIVE, CSSM_KEYBLOB_RAW_FORMAT_NONE,
		&theirPriv, theirPrivKeyRef, CSSM_KEYUSE_DERIVE, CSSM_KEYBLOB_RAW_FORMAT_NONE,
		CSSM_FALSE);
	if(crtn) {
		ourRtn = testError(quiet);
		goto errOut;
	}

	if(theirPubKeyType == PKT_Wrap) {
		/* 
		 * This test mode is here mainly to ring out the key wrap and 
		 * OCTET_STRING format functionality in the CrypkitCSP, it's
		 * not really relevant to ECDH...
		 */
		crtn = cspRefKeyToRawWithFormat(cspHand, &theirPub,
			CSSM_KEYBLOB_RAW_FORMAT_OCTET_STRING, &theirWrappedPub);
		if(crtn) {
			ourRtn = testError(quiet);
			goto errOut;
		}
		theirPubPtr = &theirWrappedPub;
		wrappedTheirPub = true;
	}
	else {
		theirPubPtr = &theirPub;
	}
	
	if(!bareCsp) {
		/*
		 * For CSPDL, convert our pub key to OCTET_STRING format so it
		 * is sent as a Param - can't send a ref key (or any other pub 
		 * key) in the context
		 */
		crtn = cspRefKeyToRawWithFormat(cspHand, &ourPub,
			CSSM_KEYBLOB_RAW_FORMAT_OCTET_STRING, &ourWrappedPub);
		if(crtn) {
			ourRtn = testError(quiet);
			goto errOut;
		}
		ourPubPtr = &ourWrappedPub;
		wrappedOurPub = true;
	}
	else {
		ourPubPtr = &ourPub;
	}

	/* 
	 * Here we go, do the two sides of D-H key agreement, results to 
	 * to CSSM_KEYs.
	 */
	ourRtn = doECDH(cspHand, &ourPriv, theirPubPtr, bareCsp, 
		x963KDF, useSharedInfo ? &sharedInfo : NULL,
		deriveSizeInBits, quiet, verbose, &derived1);
	if(ourRtn) {
		goto errOut;
	}
	ourRtn = doECDH(cspHand, &theirPriv, ourPubPtr, bareCsp, 
		x963KDF, useSharedInfo ? &sharedInfo : NULL,	
		deriveSizeInBits, quiet, verbose, &derived2);
	if(ourRtn) {
		goto errOut;
	}

	if(!appCompareCssmData(&derived1.KeyData, &derived2.KeyData)) {
		printf("***Data Miscompare on ECDH key derivation\n");
	}
errOut:
	if(ourKeysGend) {
		cspFreeKey(cspHand, &ourPub);
		cspFreeKey(cspHand, &ourPriv);
	}
	if(theirKeysGend) {
		cspFreeKey(cspHand, &theirPub);
		cspFreeKey(cspHand, &theirPriv);
	}
	if(wrappedTheirPub) {
		cspFreeKey(cspHand, &theirWrappedPub);
	}
	if(wrappedOurPub) {
		cspFreeKey(cspHand, &ourWrappedPub);
	}
	if(derivedKey1) {
		cspFreeKey(cspHand, &derived1);
	}
	if(derivedKey2) {
		cspFreeKey(cspHand, &derived2);
	}
	if(sharedInfo.Data != NULL) {
		appFreeCssmData(&sharedInfo, CSSM_FALSE);
	}
	return ourRtn;
}

int main(int argc, char **argv)
{
	int		 		arg;
	char			*argp;
	CSSM_CSP_HANDLE cspHand;
	unsigned		loop;
	int				ourRtn = 0;
	
	unsigned	keySize = KEY_SIZE_DEF;
	unsigned	loops = LOOPS_DEF;
	CSSM_BOOL	quiet = CSSM_FALSE;
	CSSM_BOOL	verbose = CSSM_FALSE;
	CSSM_BOOL	bareCsp = CSSM_TRUE;
	CSSM_BOOL	x963KDF = CSSM_FALSE;
	
	for(arg=1; arg<argc; arg++) { 
		argp = argv[arg];
	    switch(argp[0]) {
			case 'k':
				keySize = atoi(&argp[2]);
				break;
			case 'X':
				x963KDF = true;
				break;
		    case 'l':
				loops = atoi(&argp[2]);
				break;
			case 'D':
				bareCsp = CSSM_FALSE;
				break;
			case 'q':
				quiet = CSSM_TRUE;
				break;
		    case 'v':
		    	verbose = CSSM_TRUE;
				break;
			default:
				usage(argv);
		}
	}
	testStartBanner("ecdhTest", argc, argv);	

	cspHand = cspDlDbStartup(bareCsp, NULL);
	if(cspHand == 0) {
		exit(1);
	}
	
	for(loop=1; ; loop++) {
		if(!quiet) {
			printf("...Loop %d\n", loop);
		}
		
		/* test mode from l.s. bits of loop counter */

		CSSM_BOOL ourKeysRef = (loop & 0x04) ? CSSM_TRUE : CSSM_FALSE;
		CSSM_BOOL theirPrivKeyRef = (loop & 0x08) ? CSSM_TRUE : CSSM_FALSE;
		PubKeyType theirPubKeyType;
		switch(loop & 0x03) {
			case 0:
				theirPubKeyType = PKT_Ref;
				break;
			case 1:
				theirPubKeyType = PKT_Wrap;
				break;
			case 2:
				theirPubKeyType = PKT_X509;
				break;
			default:
				theirPubKeyType = PKT_Octet;
				break;
		}
		
		if(!bareCsp) {
			/* 
			 * Generated keys have to be reference
			 * pub keys have to be passed as Param
			 */
			ourKeysRef = CSSM_TRUE;
			theirPrivKeyRef = CSSM_TRUE;
			theirPubKeyType = PKT_Wrap;
		}
		
		CSSM_BOOL useSharedInfo = CSSM_FALSE;
		if(x963KDF & ((loop & 0x01) == 0)) {
			useSharedInfo = CSSM_TRUE;
		}
		ourRtn = doTest(cspHand, ourKeysRef, theirPrivKeyRef, theirPubKeyType,
			keySize, bareCsp, x963KDF, useSharedInfo, verbose, quiet);
		if(ourRtn) {
			break;
		}
		if(loops && (loop == loops)) {
			break;
		}
	}
	CSSM_ModuleDetach(cspHand);
	if((ourRtn == 0) && !quiet) {
		printf("OK\n");
	}

	return ourRtn;
}
