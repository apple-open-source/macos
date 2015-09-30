/*
 * Copyright (c) 1998,2011,2014 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

 
#include "Crypt.h"
#include "falloc.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "ckutilsPlatform.h"

#define MIN_PASSWD_LENGTH	4
#define MAX_PASSWD_LENGTH	20
#define DEPTH_DEFAULT		FEE_DEPTH_DEFAULT

#undef	BOOL
#undef	YES
#undef	NO
#define BOOL	int
#define YES	1
#define NO	0

static unsigned char *passwdPool;

static unsigned doBlobTest(unsigned minPasswdLen, 
	unsigned maxPasswdLen, 
	BOOL verbose,
	unsigned depth);
static void usage(char **argv);

int main(int argc, char **argv)
{
	BOOL 		seedSpec = NO;		// YES ==> user specified
	unsigned 	loopNum;
	int 		arg;
	char 		*argp;
	
	/*
	 * User-spec'd variables
	 */
	unsigned 	minPasswordLen = MIN_PASSWD_LENGTH;
	unsigned 	maxPasswordLen = MAX_PASSWD_LENGTH;
	int 		seed = 0;
	unsigned 	loops = 1;	
	BOOL		quiet = NO;
	BOOL		verbose = NO;
	unsigned	depth = DEPTH_DEFAULT;
	
	#if	macintosh
	argc = ccommand(&argv);
	#endif
	for(arg=1; arg<argc; arg++) {
		argp = argv[arg];
		switch(argp[0]) {
		    case 'n':
			minPasswordLen = atoi(&argp[2]);
			break;
		    case 'x':
			maxPasswordLen = atoi(&argp[2]);
			break;
		    case 's':
			seed = atoi(&argp[2]);
			seedSpec = YES;
			break;
		    case 'l':
			loops = atoi(&argp[2]);
			break;
		    case 'D':
			depth = atoi(&argp[2]);
			break;
		    case 'q':
		    	quiet = YES;
			break;
		    case 'v':
		    	verbose = YES;
			break;
		    case 'h':
		    default:
			usage(argv);
		}
	}
	if(seedSpec == NO) {
		unsigned long	tim;
		time(&tim);
		seed = (unsigned)tim;
	}
	SRAND(seed);

	passwdPool = fmalloc(maxPasswordLen + 4);
	
	printf("Starting %s: minPasswd %d maxPasswd %d seed %d depth %d\n",
		argv[0],
		minPasswordLen, maxPasswordLen, seed, depth);
	
	for(loopNum=1; ; loopNum++) {
		if(!quiet) {
			printf("..loop %d\n", loopNum);
		}
		if(doBlobTest(minPasswordLen, maxPasswordLen, verbose, 
				depth)) {
			return 1;
		}
		if(loops && (loopNum == loops)) {
			break;
		}
	}
	if(!quiet) {
		printf("%s test complete\n", argv[0]);
	}
	return 0;
}

static void usage(char **argv)
{
	printf("usage: %s [options]\n", argv[0]);
	printf("   Options:\n");
	printf("   l=loops (0=forever)\n");
	printf("   n=minPasswordLen\n");
	printf("   x=maxPasswdLen\n");
	printf("   s=seed\n");
	printf("   D=depth (default=%d)\n", DEPTH_DEFAULT);
	printf("   q(uiet)\n");
	printf("   v(erbose)\n");
	printf("   h(elp)\n");
	exit(1);
}

static unsigned char *genPasswd(unsigned passwdLength) 
{
	unsigned *ip = (unsigned *)passwdPool;
	unsigned intCount = (passwdLength + 3) / 4;
	int i;
	unsigned char *rtn;
	
	for (i=0; i<intCount; i++) {
		*ip++ = RAND();
	}
	rtn = fmalloc(passwdLength);
	bcopy(passwdPool, rtn, passwdLength);
	return rtn;
}

static unsigned doBlobTest(unsigned minPasswdLen, 
	unsigned maxPasswdLen,
	BOOL verbose,
	unsigned depth)
{
	unsigned char 	*myPasswd = NULL;
	unsigned 	myPasswdLen;
	feePubKey 	myPrivate = NULL;
	feePubKey 	myPrivateCopy = NULL;	// from blob
	feePubKey 	myPublic = NULL;	// from blob from myPrivate
	feePubKey 	myPublicCopy = NULL;	// from blob from myPublic
	unsigned	rtn = 0;
	feeReturn	frtn;
	unsigned char 	*privBlob = NULL;
	unsigned	privBlobLen;
	unsigned char 	*pubBlob = NULL;
	unsigned	pubBlobLen;
	
	for(myPasswdLen=minPasswdLen; 
	    myPasswdLen<maxPasswdLen; 
	    myPasswdLen++) {
		
	    	if(verbose) {
		    printf("....myPasswdLen %d\n", myPasswdLen);
		}

		/*
		 * my private password
		 */
		myPasswd = genPasswd(myPasswdLen);
		
		/*
		 * Fully capable Public Key object
		 */
		myPrivate = feePubKeyAlloc();
		frtn = feePubKeyInitFromPrivDataDepth(myPrivate,
			myPasswd,
			myPasswdLen,
			depth,
			1);
		if(frtn) {
			printf("feePubKeyInitFromPrivDataDepth: %s\n",
				feeReturnString(frtn));
			rtn = 1;
			goto out;
		}
		
		/* private blob */
		frtn = feePubKeyCreatePrivBlob(myPrivate,
			&privBlob,
			&privBlobLen);
		if(frtn) {
			printf("feePubKeyCreatePrivBlob: %s\n",
				feeReturnString(frtn));
			rtn = 1;
			goto out;
		}

		/* private key from private blob */
		myPrivateCopy = feePubKeyAlloc();
		frtn = feePubKeyInitFromPrivBlob(myPrivateCopy,
			privBlob,
			privBlobLen);
		if(frtn) {
			printf("feePubKeyInitFromKeyBlob (private): %s\n",
				feeReturnString(frtn));
			rtn = 1;
			goto out;
		}
		if(!feePubKeyIsPrivate(myPrivateCopy)) {
			printf("Unexpected !feePubKeyIsPrivate!\n");
			rtn = 1;
			goto out;
		}

		/* public blob from private key */
		frtn = feePubKeyCreatePubBlob(myPrivate,
			&pubBlob,
			&pubBlobLen);
		if(frtn) {
			printf("feePubKeyCreatePubBlob (1): %s\n",
				feeReturnString(frtn));
			rtn = 1;
			goto out;
		}

		/* public key from public blob */
		myPublic = feePubKeyAlloc();
		frtn = feePubKeyInitFromPubBlob(myPublic,
			pubBlob,
			pubBlobLen);
		if(frtn) {
			printf("feePubKeyInitFromKeyBlob (pub 1): %s\n",
				feeReturnString(frtn));
			rtn = 1;
			goto out;
		}
		if(feePubKeyIsPrivate(myPublic)) {
			printf("Unexpected feePubKeyIsPrivate (1)!\n");
			rtn = 1;
			goto out;
		}
		ffree(pubBlob);
		pubBlob = NULL;
	
		/* public blob from public key */
		frtn = feePubKeyCreatePubBlob(myPublic,
			&pubBlob,
			&pubBlobLen);
		if(frtn) {
			printf("feePubKeyCreatePubBlob (2): %s\n",
				feeReturnString(frtn));
			rtn = 1;
			goto out;
		}

		/* public key from public blob */
		myPublicCopy = feePubKeyAlloc();
		frtn = feePubKeyInitFromPubBlob(myPublicCopy,
			pubBlob,
			pubBlobLen);
		if(frtn) {
			printf("feePubKeyInitFromKeyBlob (pub 2): %s\n",
				feeReturnString(frtn));
			rtn = 1;
			goto out;
		}
		if(feePubKeyIsPrivate(myPublicCopy)) {
			printf("Unexpected feePubKeyIsPrivate (2)!\n");
			rtn = 1;
			goto out;
		}

		/* private blob from pub key - should fail */
		frtn = feePubKeyCreatePrivBlob(myPublic,
			&pubBlob,
			&pubBlobLen);
		if(frtn == FR_Success) {
		    printf("Unexpected feePubKeyCreatePrivBlob success\n");
		    rtn = 1;
		    goto out;
		}
		
		/*
		 * OK, we have four keys; they should all be equal (in
		 * terms of their actual public data).
		 */
		if(!feePubKeyIsEqual(myPrivate, myPrivateCopy)) {
			printf("myPrivate != myPrivateCopy\n");
			rtn = 1;
			goto out;
		}
		if(!feePubKeyIsEqual(myPrivate, myPublic)) {
			printf("myPrivate != myPublic\n");
			rtn = 1;
			goto out;
		}
		if(!feePubKeyIsEqual(myPrivate, myPublicCopy)) {
			printf("myPrivate != myPublicCopy\n");
			rtn = 1;
			goto out;
		}
		if(!feePubKeyIsEqual(myPublic, myPublicCopy)) {
			printf("myPublic != myPublicCopy\n");
			rtn = 1;
			goto out;
		}
	    out:
		if(myPasswd) {
			ffree(myPasswd);
		}
		if(myPrivate) {
			feePubKeyFree(myPrivate);
		}
		if(myPrivateCopy) {
			feePubKeyFree(myPrivateCopy);
		}
		if(myPublic) {
			feePubKeyFree(myPublic);
		}
		if(myPublic) {
			feePubKeyFree(myPublicCopy);
		}
		if(privBlob) {
			ffree(privBlob);
		}
		if(pubBlob) {
			ffree(pubBlob);
		}
		if(rtn) {
			break;
		}
	}
	return rtn;
}

