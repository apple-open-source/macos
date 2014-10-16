/* cgVerifyThr.cpp - simple version CertGroupVerify test */

#include "testParams.h"
#include <Security/cssm.h>
#include <utilLib/common.h>
#include <utilLib/cspwrap.h>
#include <clAppUtils/clutils.h>
#include <clAppUtils/tpUtils.h>
#include <clAppUtils/timeStr.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <Security/oidsalg.h>

/* for memory leak debug only, with only one thread running */
#define DO_PAUSE			0

/*** start of code directly copied from ../cgVerify/cgVerify.cpp ***/
#define NUM_CERTS_MIN		4
#define KEYGEN_ALG_DEF		CSSM_ALGID_RSA
#define SIG_ALG_DEF			CSSM_ALGID_SHA1WithRSA
#define LOOPS_DEF			10
#define CG_KEY_SIZE_DEFAULT	CSP_RSA_KEY_SIZE_DEFAULT
#define SECONDS_TO_LIVE		(60 * 60 * 24)		/* certs are valid for this long */

#define CERT_IN_DB		0

/*
 * How we define the "expected result".
 */
typedef enum {
	ER_InvalidAnchor,		// root in certGroup, not found in AnchorCerts
	ER_RootInCertGroup,		// root in certGroup, copy in AnchorCerts
	ER_AnchorVerify,		// end of chain verified by an anchor
	ER_NoRoot				// no root, no anchor verify
} ExpectResult;

static int testError()
{
	char resp;

	printf("Attach via debugger for more info.\n");
	printf("a to abort, c to continue: ");
	resp = getchar();
	return (resp == 'a');
}

static int doTest(
	CSSM_TP_HANDLE	tpHand,
	CSSM_CL_HANDLE	clHand,
	CSSM_CSP_HANDLE	cspHand,
	CSSM_DL_DB_HANDLE dlDb,
	CSSM_DATA_PTR	certs,
	unsigned		numCerts,
	CSSM_BOOL		useDb,
	ExpectResult	expectResult,	
	CSSM_BOOL		verbose)
{
	unsigned			cgEnd;				// last cert in certGroupFrag
	unsigned			anchorStart;		// first cert in anchorGroup
	unsigned			anchorEnd;			// last cert in anchorGroup
	CSSM_CERTGROUP		certGroupFrag;		// INPUT to CertGroupVerify
	CSSM_CERTGROUP		anchorCerts;		// ditto 
	unsigned			die;				// random number
	CSSM_DL_DB_LIST		dbList;
	CSSM_DL_DB_LIST_PTR	dbListPtr;
	CSSM_DL_DB_HANDLE_PTR	dlDbPtr;
	CSSM_RETURN			expErr;				// expected rtn from GroupVfy()
	int					rtn = 0;
	const char			*expResStr;
	uint32				expEvidenceSize;	// expected evidenceSize
	unsigned			evidenceSize;		// actual evidence size
	CSSM_TP_VERIFY_CONTEXT_RESULT	vfyResult;
	CSSM_CERTGROUP_PTR 	outGrp = NULL;
	CSSM_RETURN			crtn;
	
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
			/* break chain at random place other than start and end-2 */
			die = genRand(1, numCerts-3);
			cgEnd = die;				// certGroupFrag up to break point
			anchorStart = 0;			// anchors = all
			anchorEnd = numCerts - 1;
			expErr = CSSM_OK;
			/* size = # certs in certGroupFrag, plus one anchor */ 
			expEvidenceSize = die + 2;
			expResStr = "Good (root ONLY in anchors)";
			break;
			
		case ER_NoRoot:			
			/* no root, no anchor verify */
			/* break chain at random place other than start and end-1 */
			die = genRand(1, numCerts-2);
			cgEnd = die;				// certGroupFrag up to break point
			/* and skip one cert */
			anchorStart = die + 2;		// anchors = n+1...numCerts-2
										// may be empty if n == numCerts-2
			anchorEnd = numCerts - 1;
			expErr = CSSMERR_TP_NOT_TRUSTED;
			expEvidenceSize = die + 1;
			expResStr = "Not Trusted (no root, no anchor verify)";
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
	 	printf("\nError in tpMakeRandCertGroup\n");
	 	return 1;		
	}
	
	if(anchorStart > anchorEnd) {
		/* legal for ER_NoRoot */
		if((expectResult != ER_NoRoot) || (anchorStart != numCerts)) {
			printf("Try again, pal.\n");
			exit(1);
		}
	}
 	if(verbose) {
 		printf("   ...building anchorCerts from certs[%d..%d]\n",
 			anchorStart, anchorEnd);
 	}
 	if(anchorEnd > (numCerts - 1)) {
 		printf("anchorEnd overflow\n");
 		exit(1);
 	} 
 	/* anchors do not go in DB */
  	if(tpMakeRandCertGroup(clHand,
	 		NULL,
	 		certs + anchorStart,
	 		anchorEnd - anchorStart + 1,			// # of certs
	 		&anchorCerts,
	 		CSSM_FALSE,			// firstCertIsSubject
	 		verbose,
	 		CSSM_FALSE,			// allInDbs
	 		CSSM_FALSE)) {		// skipFirstDb
	 	printf("\nError in tpMakeRandCertGroup\n");
	 	return 1;		
	}
	
	crtn = tpCertGroupVerify(
		tpHand,
		clHand,
		cspHand,
		dbListPtr,
		&CSSMOID_APPLE_X509_BASIC, 	// Policy
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
		return 1;
	}
	if((vfyResult.Evidence != NULL) && (vfyResult.Evidence[1].Evidence != NULL)) {
		outGrp = (CSSM_CERTGROUP_PTR)vfyResult.Evidence[1].Evidence;
		evidenceSize = outGrp->NumCerts;
	}
	else {
		/* in case no evidence returned */
		evidenceSize = 0;
	}

	/* %%% since non-root anchors are permitted as of <rdar://5685316>,
	 * the test assumptions have become invalid: these tests generate
	 * an anchors list which always includes the full chain, so by
	 * definition, the evidence chain will never be longer than 2,
	 * since the leaf's issuer is always an anchor.
	 * %%% need to revisit and rewrite these tests. -kcm
	 */
	if ((evidenceSize > 1) && (evidenceSize < expEvidenceSize) &&
		(crtn == CSSM_OK || crtn == CSSMERR_TP_CERTIFICATE_CANT_OPERATE)) {
		/* ignore, for now */
		expErr = crtn;
		expEvidenceSize = evidenceSize;
	}
	
	if((crtn != expErr) || 
	   (evidenceSize != expEvidenceSize)) {
		printf("\n***cgVerify: Error on tpCertGroupVerify expectResult %s\n", 
			expResStr);
		printf("   err  %s   expErr  %s\n", 
			cssmErrToStr(crtn), cssmErrToStr(expErr));
		printf("   evidenceSize  %d   expEvidenceSize  %u\n", 
			evidenceSize, (unsigned)expEvidenceSize);
		printf("   numCerts %d  cgEnd %d  anchorStart %d  anchorEnd %d\n",
			numCerts, cgEnd, anchorStart, anchorEnd);
		rtn = testError();
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

/*** end of code directly copied from ../cgVerify/cgVerify.cpp ***/

/*
 * For debug only - ensure that the given array of public keys are all unique 
 * Only saw this when using FEE RNG (i.e., no SecurityServer running).
 */
int comparePubKeys(
	unsigned numKeys,
	const CSSM_KEY *pubKeys)	
{
	unsigned i,j;
	
	for(i=0; i<numKeys-1; i++) {
		for(j=i+1; j<numKeys; j++) {
			if(appCompareCssmData(&pubKeys[i].KeyData, &pubKeys[j].KeyData)) {
				printf("***HEY! DUPLICATE PUBLIC KEYS in cgVerify!\n");
				return testError();
			}
		}
	}
	return 0;
}

 
/*
 * key pairs - created in cgConstructInit, stored in testParams->perThread
 */
typedef struct {
	CSSM_KEY_PTR pubKeys;
	CSSM_KEY_PTR privKeys;
	unsigned numKeys;
	char *notBeforeStr;		// to use thread-safe tpGenCerts()
	char *notAfterStr;		// to use thread-safe tpGenCerts()
}	TT_KeyPairs;


int cgVerifyInit(TestParams *testParams)
{
	unsigned			numKeys = NUM_CERTS_MIN + testParams->threadNum;
	TT_KeyPairs			*keyPairs;
	
	if(testParams->verbose) {
		printf("cgVerify thread %d: generating keys...\n", 
			testParams->threadNum);
	}
	keyPairs = (TT_KeyPairs *)CSSM_MALLOC(sizeof(TT_KeyPairs));
	keyPairs->numKeys = numKeys;
	keyPairs->pubKeys  = (CSSM_KEY_PTR)CSSM_CALLOC(numKeys, sizeof(CSSM_KEY));
	keyPairs->privKeys = (CSSM_KEY_PTR)CSSM_CALLOC(numKeys, sizeof(CSSM_KEY));
	CSSM_DL_DB_HANDLE	dlDbHand = {0, 0};
	if(tpGenKeys(testParams->cspHand,
			dlDbHand,
			numKeys,
			KEYGEN_ALG_DEF, 
			CG_KEY_SIZE_DEFAULT,
			"cgVerify",			// keyLabelBase
			keyPairs->pubKeys,
			keyPairs->privKeys)) {
		goto abort;
	}
	if(comparePubKeys(numKeys, keyPairs->pubKeys)) {
		return 1;
	}
	keyPairs->notBeforeStr = genTimeAtNowPlus(0);
	keyPairs->notAfterStr = genTimeAtNowPlus(SECONDS_TO_LIVE);
	
	testParams->perThread = keyPairs;
	return 0;
	
abort:
	printf("Error generating keys; aborting\n");
	CSSM_FREE(keyPairs->pubKeys);
	CSSM_FREE(keyPairs->privKeys);
	CSSM_FREE(keyPairs);
	return 1;
}

int cgVerify(TestParams *testParams)
{
	unsigned 			loopNum;
	int					status = -1;		// exit status
	unsigned			dex;
	
	TT_KeyPairs			*keyPairs = (TT_KeyPairs *)testParams->perThread;

	/* all three of these are arrays with numCert elements */
	CSSM_KEY_PTR		pubKeys = keyPairs->pubKeys;
	CSSM_KEY_PTR		privKeys = keyPairs->privKeys;
	CSSM_DATA_PTR		certs = NULL;
	
	unsigned			numCerts = keyPairs->numKeys;
	uint32				sigAlg = SIG_ALG_DEF;
	ExpectResult		expectResult;
	#if	CERT_IN_DB
	CSSM_BOOL			useDb = CSSM_TRUE;
	#else
	CSSM_BOOL			useDb = CSSM_FALSE;
	#endif
	CSSM_DL_DB_HANDLE	dlDbHand = {0, 0};
	
	/* malloc empty certs */
	certs = (CSSM_DATA_PTR)CSSM_CALLOC(numCerts, sizeof(CSSM_DATA));
	if(certs == NULL) {
		printf("not enough memory for %u certs.\n", numCerts);
		goto abort;
	}
	memset(certs, 0, numCerts * sizeof(CSSM_DATA));

	for(loopNum=0; loopNum<testParams->numLoops; loopNum++) {
		/* generate certs */
		if(testParams->verbose) {
			printf("generating certs...\n");
		}
		else if(!testParams->quiet) {
			printChar(testParams->progressChar);
		}
		if(tpGenCerts(testParams->cspHand,
				testParams->clHand,
				numCerts,
				sigAlg, 
				"cgConstruct",	// nameBase
				pubKeys,
				privKeys,
				certs,
				keyPairs->notBeforeStr,
				keyPairs->notAfterStr)) {
			goto abort;
		}

		/* cycle thru test scenarios */
		switch(loopNum % 4) {
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
		}
		status = doTest(testParams->tpHand,
				testParams->clHand,
				testParams->cspHand,
				dlDbHand,
				certs,
				numCerts,
				useDb,
				expectResult,
				testParams->verbose);
		if(status) {
			break;
		}
		/* free certs */
		for(dex=0; dex<numCerts; dex++) {
			CSSM_FREE(certs[dex].Data);
		}
		memset(certs, 0, numCerts * sizeof(CSSM_DATA));
		#if DO_PAUSE
		fpurge(stdin);
		printf("Hit CR to proceed: ");
		getchar();
		#endif
	}
abort:
	/* free resources */
	for(dex=0; dex<numCerts; dex++) {
		if(certs[dex].Data) {
			CSSM_FREE(certs[dex].Data);
		}
	}
	CSSM_FREE(keyPairs->pubKeys);
	CSSM_FREE(keyPairs->privKeys);
	CSSM_FREE(keyPairs);
	return status;
}

