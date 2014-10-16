/*
 * cgVerifyParsed.cpp - basic test of TP's CertGroupVerify using parsed anchors
 *
 * ------- THIS TEST IS OBSOLETE; WE DON'T SUPPORT PARSED ANCHORS ----------
 * 
 * cook up array of n key pairs;
 * cook up cert chain to go with them;
 * main test loop {
 *    numCerts = total # of incoming certs;
 *	  test one of four or five "expected result" cases {
 *		case root in certGroup but not found in AnchorCerts:
 *			certGroup = tpMakeRandCertGroup(certs[0..numCerts-1];
 *			anchorCerts = tpMakeRandCertGroup[certs[0...numCerts-2];
 *			expErr = CSSMERR_TP_INVALID_ANCHOR_CERT;
 *			expEvidenceSize = numCerts;
 *		case root in certGroup, found a copy in AnchorCerts:
 *			certGroup = tpMakeRandCertGroup(certs[0..numCerts-1];
 *			anchorCerts = tpMakeRandCertGroup[certs[0...numCerts-1];
 *			expErr = CSSM_OK;
 *			expEvidenceSize = numCerts;
 *		case verified by an AnchorCert:
 *			n = rand(1, numCerts-2);
 *			certGroup = tpMakeRandCertGroup(certs[0..n]);
 *			anchorCerts = tpMakeRandCertGroup[certs[0...numCerts-2];
 *			expErr = CSSM_OK;
 *			expEvidenceSize = n+2;
 *		case no root found:
 *			n = rand(1, numCerts-3);
 *			certGroup = tpMakeRandCertGroup(certs[0..n]);
 *			anchorCerts = tpMakeRandCertGroup[certs[n+2...numCerts-2];
 *					anchorCerts may be empty....
 *			expErr = CSSMERR_TP_NOT_TRUSTED;
 *			expEvidenceSize = n+1;
 *		case incomplete public key (DSA only):
 *			root public keys is incomplete;
 *			certGroup = tpMakeRandCertGroup(certs[0..numCerts-1];
 *			anchorCerts = tpMakeRandCertGroup[certs[0...numCerts-1];
 *			expErr = CSSM_OK;
 *			expEvidenceSize = numCerts;
 *    }
 *    result = certGroupVerify();
 *    verify expected result and getError();
 *    delete certs from DB;
 * }
 */
 
#include <Security/cssm.h>
#include <utilLib/common.h>
#include <utilLib/cspwrap.h>
#include <clAppUtils/clutils.h>
#include <clAppUtils/tpUtils.h>
#include <clAppUtils/timeStr.h>
#include <utilLib/nssAppUtils.h>
#include <utilLib/fileIo.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <Security/oidsalg.h>
#include "tpVerifyParsed.h"

#define NUM_CERTS_DEF		10
#define KEYGEN_ALG_DEF		CSSM_ALGID_RSA
#define SIG_ALG_DEF			CSSM_ALGID_SHA1WithRSA
#define LOOPS_DEF			10
#define SECONDS_TO_LIVE		(60 * 60 * 24)		/* certs are valid for this long */
//#define SECONDS_TO_LIVE		5

#define CERT_IN_DB			1
#define DB_NAME				"cgVerify.db"
#define DSA_PARAM_FILE		"dsaParam512.der"

/*
 * How we define the "expected result".
 */
typedef enum {
	ER_InvalidAnchor,		// root in certGroup, not found in AnchorCerts
	ER_RootInCertGroup,		// root in certGroup, copy in AnchorCerts
	ER_AnchorVerify,		// end of chain verified by an anchor
	ER_NoRoot,				// no root, no anchor verify
	ER_IncompleteKey		// un-completable public key (all keys are partial), DSA
							//   ONLY
} ExpectResult;

static void usage(char **argv)
{
	printf("Usage: %s [options]\n", argv[0]);
	printf("   Options:\n");
	printf("   n=numCerts; default = %d\n", NUM_CERTS_DEF);
	printf("   l=loops; default=%d; 0=forever\n", LOOPS_DEF);
	printf("   a=alg (f=FEE/MD5, F=FEE/SHA1, e=FEE/ECDSA, s=RSA/SHA1, m=RSA/MD5,\n");
	printf("          d=DSA; 6=RSA/SHA256, 3=RSA/SHA384, 5=RSA/SHA512; default = RSA/SHA1\n");
	printf("   k=keySizeInBits\n");
	printf("   d(isable DB)\n");
	printf("   P(ause on each loop)\n");
	printf("   N (no partial pub keys)\n");
	printf("   v(erbose)\n");
	printf("   q(uiet)\n");
	printf("   h(elp)\n");
	exit(1);
}

static int doTest(
	CSSM_TP_HANDLE	tpHand,
	CSSM_CL_HANDLE	clHand,
	CSSM_CSP_HANDLE	cspHand,
	CSSM_DL_DB_HANDLE dlDb,
	CSSM_DATA_PTR	certs,
	unsigned		numCerts,
	CSSM_KEY_PTR	pubKeys,		// for partial key detect
	CSSM_BOOL		useDb,
	ExpectResult	expectResult,	
	CSSM_BOOL		verbose,
	CSSM_BOOL		quiet)
{
	unsigned			cgEnd;				// last cert in certGroupFrag
	unsigned			anchorStart;		// first cert in anchorGroup
	unsigned			anchorEnd;			// last cert in anchorGroup
	CSSM_CERTGROUP		certGroupFrag;		// INPUT to CertGroupVerify
	CSSM_CERTGROUP		anchorCerts;		// ditto 
	unsigned			die;				// random number
	CSSM_DL_DB_LIST		dbList;
	CSSM_DL_DB_LIST_PTR	dbListPtr;
	CSSM_RETURN			expErr;				// expected rtn from GroupVfy()
	int					rtn = 0;
	char				*expResStr;
	uint32				expEvidenceSize;	// expected evidenceSize
	unsigned			evidenceSize;		// actual evidence size
	CSSM_TP_VERIFY_CONTEXT_RESULT	vfyResult;
	CSSM_CERTGROUP_PTR 	outGrp = NULL;
	CSSM_RETURN			crtn;
	CSSM_DL_DB_HANDLE_PTR	dlDbPtr;
	unsigned numAnchors;
	
	memset(&vfyResult, 0, sizeof(CSSM_TP_VERIFY_CONTEXT_RESULT));
	
	if(useDb) {
		dlDbPtr = &dlDb;
		dbList.NumHandles = 1;
		dbList.DLDBHandle = &dlDb;
		dbListPtr = &dbList;
	}
	else {
		/* not yet */
		dlDbPtr = NULL;
		dbListPtr = NULL;
	}
 	
 	/* the four test cases */
 	switch(expectResult) {
		case ER_InvalidAnchor:		
			/* root in certGroup, not found in AnchorCerts */
			cgEnd = numCerts - 1;		// certGroupFrag is the whole pile
			anchorStart = 0;			// anchors = all except root
			anchorEnd = numCerts - 2;
			expErr = CSSMERR_TP_INVALID_ANCHOR_CERT;
			expEvidenceSize = numCerts;
			expResStr = "InvalidAnchor (root in certGroup but not in anchors)";
			break;
			
		case ER_RootInCertGroup:		
			/* root in certGroup, copy in AnchorCerts */
			cgEnd = numCerts - 1;		// certGroupFrag = the whole pile
			anchorStart = 0;			// anchors = the whole pile
			anchorEnd = numCerts - 1;
			expErr = CSSM_OK;
			expEvidenceSize = numCerts;
			expResStr = "Good (root in certGroup AND in anchors)";
			break;

		case ER_AnchorVerify:	
			/* non-root end of chain verified by an anchor */
			/* break chain at random place other than end */
			/* die is the last cert in certGroupFrag */
			die = genRand(0, numCerts-2);
			cgEnd = die;				// certGroupFrag up to break point
			anchorStart = 0;			// anchors = all
			anchorEnd = numCerts - 1;
			if(pubKeys[die+1].KeyHeader.KeyAttr & CSSM_KEYATTR_PARTIAL) {
				/* this will fail due to an unusable anchor */
				expErr = CSSMERR_TP_CERTIFICATE_CANT_OPERATE;
				expResStr = "Root ONLY in anchors but has partial pub key";
			}
			else {
				expErr = CSSM_OK;
				expResStr = "Good (root ONLY in anchors)";
			}
			/* size = # certs in certGroupFrag, plus one anchor */ 
			expEvidenceSize = die + 2;
			break;
			
		case ER_NoRoot:			
			/* no root, no anchor verify */
			/* break chain at random place other than end */
			/* die is the last cert in certGroupFrag */
			/* skip a cert, then anchors start at die + 2 */
			die = genRand(0, numCerts-2);
			cgEnd = die;				// certGroupFrag up to break point
			anchorStart = die + 2;		// anchors = n+1...numCerts-2
										// may be empty if n == numCerts-2
			anchorEnd = numCerts - 2;
			if((die != 0) &&			// partial leaf not reported as partial!
			   (pubKeys[die].KeyHeader.KeyAttr & CSSM_KEYATTR_PARTIAL)) {
				/* this will fail due to an unusable cert (this one) */
				expErr = CSSMERR_TP_CERTIFICATE_CANT_OPERATE;
				expResStr = "Not Trusted (no root, no anchor verify), partial";
			}
			else {
				expErr = CSSMERR_TP_NOT_TRUSTED;
				expResStr = "Not Trusted (no root, no anchor verify)";
			}
			expEvidenceSize = die + 1;
			break;
			
		case ER_IncompleteKey:
			/* 
			 * Anchor has incomplete pub key 
			 * Root in certGroup, copy in AnchorCerts
			 * Avoid putting anchor in certGroupFrag because the TP will think
			 * it's NOT a root and it'll show up twice in the evidence - once
			 * from certGroupFrag (at which point the search for a root
			 * keeps going), and once from Anchors. 
			 */
			cgEnd = numCerts - 2;		// certGroupFrag = the whole pile less the anchor
			anchorStart = 0;			// anchors = the whole pile
			anchorEnd = numCerts - 1;
			expErr = CSSMERR_TP_CERTIFICATE_CANT_OPERATE;
			expEvidenceSize = numCerts;
			expResStr = "Partial public key in anchor";
			break;
 	}
 	
 	if(verbose) {
 		printf("   ...expectResult = %s\n", expResStr);
 	}
 	
 	/* cook up two cert groups */
 	if(verbose) {
 		printf("   ...building certGroupFrag from certs[0..%d]\n",
 			cgEnd);
 	}
  	if(tpMakeRandCertGroup(clHand,
	 		dbListPtr,
	 		certs,				// certGroupFrag always starts at 0
	 		cgEnd+1,			// # of certs
	 		&certGroupFrag,
	 		CSSM_TRUE,			// firstCertIsSubject
	 		verbose,
	 		CSSM_FALSE,			// allInDbs
	 		CSSM_FALSE)) {		// skipFirstDb
	 	printf("Error in tpMakeRandCertGroup\n");
	 	return 1;		
	}
	
 	if(verbose) {
 		printf("   ...building anchorCerts from certs[%d..%d]\n",
 			anchorStart, anchorEnd);
 	}
 	if(anchorEnd > (numCerts - 1)) {
 		printf("anchorEnd overflow\n");
 		exit(1);
 	} 
	if(anchorStart >= anchorEnd) {
		/* legal in some corner cases, ==> empty enchors */
		numAnchors = 0;
	}
	else {
		numAnchors = anchorEnd - anchorStart + 1;
	}
 	/* anchors do not go in DB */
  	if(tpMakeRandCertGroup(clHand,
	 		NULL,
	 		certs + anchorStart,
	 		numAnchors,			// # of certs
	 		&anchorCerts,
	 		CSSM_FALSE,			// firstCertIsSubject
	 		verbose,
	 		CSSM_FALSE,			// allInDbs
	 		CSSM_FALSE)) {		// skipFirstDb
	 	printf("Error in tpMakeRandCertGroup\n");
	 	return 1;		
	}
	
	crtn = tpCertGroupVerifyParsed(
		tpHand,
		clHand,
		cspHand,
		dbListPtr,
		&CSSMOID_APPLE_X509_BASIC,	// policy
		NULL,						// fieldOpts
		NULL,						// actionData
		NULL,						// policyOpts
		&certGroupFrag,
		anchorCerts.GroupList.CertList,	// passed as CSSM_DATA_PTR, not CERTGROUP....
		anchorCerts.NumCerts, 
		CSSM_TP_STOP_ON_POLICY,
		NULL,					// cssmTimeStr
		&vfyResult);
		
	/* first verify format of result */
	if( (vfyResult.NumberOfEvidences != 3) ||
	    (vfyResult.Evidence == NULL) ||
		(vfyResult.Evidence[0].EvidenceForm != CSSM_EVIDENCE_FORM_APPLE_HEADER) ||
		(vfyResult.Evidence[1].EvidenceForm != CSSM_EVIDENCE_FORM_APPLE_CERTGROUP) ||
		(vfyResult.Evidence[2].EvidenceForm != CSSM_EVIDENCE_FORM_APPLE_CERT_INFO) ||
		(vfyResult.Evidence[0].Evidence == NULL) ||
		(vfyResult.Evidence[1].Evidence == NULL) ||
		(vfyResult.Evidence[2].Evidence == NULL)) {
		printf("***Malformed VerifyContextResult\n");
		rtn = testError(quiet);
		if(rtn) {
			return rtn;
		}
	}
	if((vfyResult.Evidence != NULL) && (vfyResult.Evidence[1].Evidence != NULL)) {
		outGrp = (CSSM_CERTGROUP_PTR)vfyResult.Evidence[1].Evidence;
		evidenceSize = outGrp->NumCerts;
	}
	else {
		/* in case no evidence returned */
		evidenceSize = 0;
	}
	if((crtn != expErr) || 
	   (evidenceSize != expEvidenceSize)) {
		printf("***Error on expectResult %s\n", expResStr);
		printf("   err  %s   expErr  %s\n", 
			cssmErrToStr(crtn), cssmErrToStr(expErr));
		printf("   evidenceSize  %d   expEvidenceSize  %lu\n", 
			evidenceSize, expEvidenceSize);
		rtn = testError(quiet);
	}
	else {
		rtn = 0;
	}

	/* free resources */
	tpFreeCertGroup(&certGroupFrag, 
		CSSM_FALSE,			// caller malloc'd the actual certs 
		CSSM_FALSE);		// struct is on stack
	tpFreeCertGroup(&anchorCerts, 
		CSSM_FALSE,			// caller malloc'd the actual certs 
		CSSM_FALSE);		// struct is on stack
	freeVfyResult(&vfyResult);
	if(useDb) {
		clDeleteAllCerts(dlDb);
	}
	return rtn;
}

int main(int argc, char **argv)
{
	int					arg;
	char				*argp;
	unsigned			loop;
	CSSM_TP_HANDLE		tpHand = 0;
	CSSM_CL_HANDLE		clHand = 0;
	CSSM_CSP_HANDLE		cspHand = 0;
	CSSM_DL_DB_HANDLE	dlDbHand = {0, 0};
	ExpectResult		expectResult;
	char				*notAfterStr;
	char 				*notBeforeStr;
	unsigned 			i;
	CSSM_DATA 			paramData;
	CSSM_DATA_PTR 		paramDataP = NULL;
	unsigned 			numTests = 4;
	
	/* all three of these are arrays with numCert elements */
	CSSM_KEY_PTR		pubKeys = NULL;
	CSSM_KEY_PTR		privKeys = NULL;
	CSSM_DATA_PTR		certs = NULL;
	
	/* Keys do NOT go in the cert DB */
	CSSM_DL_DB_HANDLE 	keyDb = {0, 0};
	CSSM_KEY			savedRoot;		// for ER_IncompleteKey
	
	/*
	 * User-spec'd params
	 */
	unsigned	loops = LOOPS_DEF;
	CSSM_BOOL	verbose = CSSM_FALSE;
	CSSM_BOOL	quiet = CSSM_FALSE;
	unsigned	numCerts = NUM_CERTS_DEF;
	uint32		keyGenAlg = KEYGEN_ALG_DEF;
	uint32		sigAlg = SIG_ALG_DEF;
	#if	CERT_IN_DB
	CSSM_BOOL	useDb = CSSM_TRUE;
	#else
	CSSM_BOOL	useDb = CSSM_FALSE;
	#endif
	CSSM_BOOL	doPause = CSSM_FALSE;
	uint32		keySizeInBits = CSP_KEY_SIZE_DEFAULT;
	CSSM_BOOL	noPartialKeys = CSSM_FALSE;
	char		dbName[100];		/* DB_NAME_pid */
	
	for(arg=1; arg<argc; arg++) {
		argp = argv[arg];
		switch(argp[0]) {
		    case 'l':
				loops = atoi(&argp[2]);
				break;
		    case 'k':
				keySizeInBits = atoi(&argp[2]);
				break;
		    case 'n':
				numCerts = atoi(&argp[2]);
				break;
		    case 'v':
		    	verbose = CSSM_TRUE;
				break;
		    case 'q':
		    	quiet = CSSM_TRUE;
				break;
			case 'a':
				switch(argp[2]) {
					case 'f':
						keyGenAlg = CSSM_ALGID_FEE;
						sigAlg = CSSM_ALGID_FEE_MD5;
						break;
					case 'F':
						keyGenAlg = CSSM_ALGID_FEE;
						sigAlg = CSSM_ALGID_FEE_SHA1;
						break;
					case 'e':
						keyGenAlg = CSSM_ALGID_FEE;
						sigAlg = CSSM_ALGID_SHA1WithECDSA;
						break;
					case 's':
						keyGenAlg = CSSM_ALGID_RSA;
						sigAlg = CSSM_ALGID_SHA1WithRSA;
						break;
					case 'm':
						keyGenAlg = CSSM_ALGID_RSA;
						sigAlg = CSSM_ALGID_MD5WithRSA;
						break;
					case 'd':
						keyGenAlg = CSSM_ALGID_DSA;
						sigAlg = CSSM_ALGID_SHA1WithDSA;
						break;
					case '6':
						keyGenAlg = CSSM_ALGID_RSA;
						sigAlg = CSSM_ALGID_SHA256WithRSA;
						break;
					case '3':
						keyGenAlg = CSSM_ALGID_RSA;
						sigAlg = CSSM_ALGID_SHA512WithRSA;
						break;
					case '5':
						keyGenAlg = CSSM_ALGID_RSA;
						sigAlg = CSSM_ALGID_SHA512WithRSA;
						break;
					default:
						usage(argv);
				}
				break;
			case 'N':
				noPartialKeys = CSSM_TRUE;
				break;
			case 'd':
				useDb = CSSM_FALSE;
				break;
			case 'P':
				doPause = CSSM_TRUE;
				break;
		    case 'h':
		    default:
				usage(argv);
		}
	}

	sprintf(dbName, "%s_%d", DB_NAME, (int)getpid());

	if(numCerts < 2) {
		printf("Can't run with cert chain smaller than 2\n");
		exit(1); 	
	}

	/* attach to all the modules we need */
	cspHand = cspStartup();
	if(cspHand == 0) {
		exit(1);
	}
	#if CERT_IN_DB
	if(useDb) {
		dlDbHand.DLHandle = dlStartup();
		if(dlDbHand.DLHandle == 0) {
			exit(1);
		}
		CSSM_RETURN crtn = tpKcOpen(dlDbHand.DLHandle, dbName, dbName,
			CSSM_TRUE, &dlDbHand.DBHandle);
		if(crtn) {
			printf("Error opening keychain %s; aborting.\n", dbName);
			exit(1);
		}
	}
	#endif
	clHand = clStartup();
	if(clHand == 0) {
		goto abort;
	}
	tpHand = tpStartup(); 
	if(tpHand == 0) {
		goto abort;
	}
	
	/* malloc empty keys and certs */
	pubKeys  = (CSSM_KEY_PTR)CSSM_CALLOC(numCerts, sizeof(CSSM_KEY));
	privKeys = (CSSM_KEY_PTR)CSSM_CALLOC(numCerts, sizeof(CSSM_KEY));
	certs    = (CSSM_DATA_PTR)CSSM_CALLOC(numCerts, sizeof(CSSM_DATA));
	if((pubKeys == NULL) || (privKeys == NULL) || (certs == NULL)) {
		printf("not enough memory for %u keys pairs and certs.\n",
			numCerts);
		goto abort;
	}

	printf("Starting cgVerify; args: ");
	for(i=1; i<(unsigned)argc; i++) {
		printf("%s ", argv[i]);
	}
	printf("\n");

	/* generate key pairs */
	if(!quiet) {
		printf("generating keys...\n");
	}
	if(keyGenAlg == CSSM_ALGID_DSA) {
		unsigned len;
		if(!readFile(DSA_PARAM_FILE, (unsigned char **)&paramData.Data,	&len)) {
			if(!quiet) {
				printf("...using DSA params from %s\n", DSA_PARAM_FILE);
			}
			paramData.Length = len;
			paramDataP = &paramData;
		}
		else {
			printf("***warning: no param file. KeyGen is going to be slow!\n");
			printf("***You might consider running this from the clxutils/cgVerify "
				"directory.\n");
		}
	}
	if(tpGenKeys(cspHand,
			keyDb,
			numCerts,
			keyGenAlg, 
			keySizeInBits,
			"cgVerify",		// keyLabelBase
			pubKeys,
			privKeys,
			paramDataP)) {
		goto abort;
	}
	notBeforeStr = genTimeAtNowPlus(0);
	notAfterStr = genTimeAtNowPlus(SECONDS_TO_LIVE);

	/*
	 * If DSA, insert some random partial public keys which are not 
	 * fatal (i.e., root can not be partial). We include the leaf in this
	 * loop - the TP is *supposed* to ignore that situation, ane we make
	 * sure it does. 
	 */
	if((keyGenAlg == CSSM_ALGID_DSA) && !noPartialKeys) {
		for(unsigned dex=0; dex<(numCerts-1); dex++) {
			int die = genRand(0,1);
			if(die) {
				/* this one gets partialized */
				CSSM_KEY newKey;
				if(verbose) {
					printf("...making partial DSA pub key at index %u\n", dex);
				}
				CSSM_RETURN crtn = extractDsaPartial(cspHand, &pubKeys[dex], &newKey);
				if(crtn) {
					printf("***Error converting to partial key. Aborting.\n");
					exit(1);
				}
				CSSM_FREE(pubKeys[dex].KeyData.Data);
				pubKeys[dex] = newKey;
			}
		}
	}
	if(!quiet) {
		printf("starting %s test\n", argv[0]);
		
	}
	if(keyGenAlg == CSSM_ALGID_DSA) {
		numTests = 5;
	}
	for(loop=1; ; loop++) {
		if(!quiet) {
			printf("...loop %d\n", loop);
		}
		
		/* cycle thru test scenarios */
		switch(loop % numTests) {
			case 0:
				expectResult = ER_InvalidAnchor;
				break;
			case 1:
				expectResult = ER_RootInCertGroup;
				break;
			case 2:
				expectResult = ER_AnchorVerify;
				break;
			case 3:
				expectResult = ER_NoRoot;
				break;
			case 4:
				/* DSA only */
				expectResult = ER_IncompleteKey;
				savedRoot = pubKeys[numCerts-1];
				/* make anchor unusable */
				if(extractDsaPartial(cspHand, &savedRoot, &pubKeys[numCerts-1])) {
					printf("...error partializing anchor key; aborting\n");
					exit(1);
				}
				break;
		}
		if(tpGenCerts(cspHand,
				clHand,
				numCerts,
				sigAlg, 
				"cgConstruct",	// nameBase
				pubKeys,
				privKeys,
				certs,
				notBeforeStr,
				notAfterStr)) {
			break;
		}

		if(doTest(tpHand,
				clHand,
				cspHand,
				dlDbHand,
				certs,
				numCerts,
				pubKeys,
				useDb,
				expectResult,
				verbose,
				quiet)) {
			break;			
		}
		for(i=0; i<numCerts; i++) {
			appFreeCssmData(&certs[i], CSSM_FALSE);
		}
		if(expectResult == ER_IncompleteKey) {
			CSSM_FREE(pubKeys[numCerts-1].KeyData.Data);
			pubKeys[numCerts-1] = savedRoot;
		}

		memset(certs, 0, numCerts * sizeof(CSSM_DATA));
		if(loops && (loop == loops)) {
			break;
		}
		if(doPause) {
			printf("Hit CR to continue: ");
			fpurge(stdin);
			getchar();
		}
	}
abort:
	if(privKeys != NULL) {
		for(i=0; i<numCerts; i++) {
			if(privKeys[i].KeyData.Data != NULL) {
				cspFreeKey(cspHand, &privKeys[i]);
			}
		}
		CSSM_FREE(privKeys);
	}
	if(pubKeys != NULL) {
		for(i=0; i<numCerts; i++) {
			if(pubKeys[i].KeyData.Data != NULL) {
				cspFreeKey(cspHand, &pubKeys[i]);
			}
		}
		CSSM_FREE(pubKeys);
	}
	if(certs != NULL) {
		for(i=0; i<numCerts; i++) {
			appFreeCssmData(&certs[i], CSSM_FALSE);
		}
		CSSM_FREE(certs);
	}
	if(cspHand != 0) {
		CSSM_ModuleDetach(cspHand);
	}
	if(clHand != 0) {
		CSSM_ModuleDetach(clHand);
	}
	if(tpHand != 0) {
		CSSM_ModuleDetach(tpHand);
	}
	
	if(!quiet) {
		printf("%s test complete\n", argv[0]);
	}
	return 0;
}
