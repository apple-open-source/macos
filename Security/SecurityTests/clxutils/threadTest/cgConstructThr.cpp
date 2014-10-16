/* cgConstructThr.cpp - simple version CertGroupConstruct test */

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

/* for memory leak debug only, with only one thread running */
#define DO_PAUSE			0

/*** start of code directly copied from ../cgConstruct/cgConstruct.cpp ***/
#define NUM_CERTS_MIN		4
#define NUM_DBS_DEF			3
#define KEYGEN_ALG_DEF		CSSM_ALGID_RSA
#define SIG_ALG_DEF			CSSM_ALGID_SHA1WithRSA 
#define LOOPS_DEF			10
#define DB_NAME_BASE		"cgConstruct"
#define CG_KEY_SIZE_DEFAULT	CSP_RSA_KEY_SIZE_DEFAULT
#define SECONDS_TO_LIVE		(60 * 60 * 24)		/* certs are valid for this long */

#define CG_CONSTRUCT_TP_DB		0

static int testError()
{
	char resp;

	fpurge(stdin);
	printf("Attach via debugger for more info.\n");
	printf("a to abort, c to continue: ");
	resp = getchar();
	return (resp == 'a');
}

#if CG_CONSTRUCT_TP_DB
static int doOpenDbs(
	CSSM_DL_HANDLE			dlHand,
	char 					*dbNameBase,
	CSSM_DL_DB_HANDLE_PTR 	dlDbPtr,
	unsigned 				numDbs,
	CSSM_BOOL 				publicReadOnly,		// ignored if !PUBLIC_READ_ENABLE
	CSSM_BOOL				quiet)
{
	unsigned i;
	char dbName[20];
	CSSM_BOOL doCreate = (publicReadOnly ? CSSM_FALSE : CSSM_TRUE);
	
	for(i=0; i<numDbs; i++) {
		sprintf(dbName, "%s%d", dbNameBase, i);
		CSSM_RETURN crtn = tpKcOpen(dbName, 
			&dlDbPtr[i],
			doCreate,
			dlHand);
		if(crtn) {
			printf("Can't create %d DBs\n", numDbs);
			return testError(quiet);
		}
	}
	return 0;
}
#endif

static int doTest(
	CSSM_TP_HANDLE		tpHand,
	CSSM_CL_HANDLE		clHand,
	CSSM_CSP_HANDLE		cspHand,
	CSSM_DL_DB_LIST_PTR	dbList,
	CSSM_DATA_PTR		certs,
	unsigned			numCerts,
	CSSM_BOOL			verbose,
	CSSM_BOOL			allInDbs,
	CSSM_BOOL			skipFirstDb,
	CSSM_BOOL			publicRead)		// close/open with public access
{
	unsigned				certsToUse;		// # of certs we actually use
	CSSM_CERTGROUP			certGroupFrag;	// INPUT to CertGroupConstruct
	CSSM_CERTGROUP_PTR		resultGroup;	// OUTPUT from "
	unsigned				certDex;
	int						rtn = 0;
	CSSM_RETURN				crtn;
	
	#if	CG_CONSTRUCT_TP_DB
	if(publicRead && (dbList != NULL)) {
		/* DBs are closed on entry, open r/w */
		if(doOpenDbs(0,
				DB_NAME_BASE,
				dbList->DLDBHandle,
				dbList->NumHandles,
				CSSM_FALSE,
				quiet)) {		// publicReadOnly: this is create/write
			return 1;
		}	
	}
	/* else DBs are already open and stay that way */
	#endif
	
	/*
	 * Pick a random spot to break the cert chain - half the time use the
	 * whole chain, half the time break it.
	 */
	certsToUse = genRand(1, numCerts * 2);
	if(certsToUse > numCerts) {
		/* use the whole chain */
		certsToUse = numCerts;
	}
 	if(verbose) {
 		printf("   ...numCerts %d  certsToUse %d\n", numCerts, certsToUse);   
 	}

 	if(tpMakeRandCertGroup(clHand,
			#if		CG_CONSTRUCT_TP_DB
	 		dbList,
			#else
			NULL,
			#endif
	 		certs,
	 		certsToUse,
	 		&certGroupFrag,
	 		CSSM_TRUE,			// firstCertIsSubject
	 		verbose,
	 		allInDbs,
	 		skipFirstDb)) {
	 	printf("\nError in tpMakeRandCertGroup\n");
	 	return testError();
	}
 		
	if(certGroupFrag.NumCerts > certsToUse) {
		printf("Error NOMAD sterlize\n");
		exit(1);
	}
	
	#if	CG_CONSTRUCT_TP_DB
	if(publicRead) {
		/* close existing DBs and open again read-only */
		
		unsigned i;
		CSSM_RETURN crtn;
		
		if(verbose) {
			printf("   ...closing DBs\n");
		}
		for(i=0; i<dbList->NumHandles; i++) {
			crtn = CSSM_DL_DbClose(dbList->DLDBHandle[i]);
			if(crtn) {
				printError("CSSM_DL_DbClose");
				if(testError()) {
					return 1;
				}
			}
		}
		if(verbose) {
			printf("   ...opening DBs read-only\n");
		}
		if(doOpenDbs(0,
				DB_NAME_BASE,
				dbList->DLDBHandle,
				dbList->NumHandles,
				CSSM_TRUE, 		// publicReadOnly: this is read only
				quiet)) {
			return 1;
		}
	}
	#endif
	
	/* 
	 * Okay, some of the certs we were given are in the DB, some are in 
	 * random places in certGroupFrag, some are nowhere (if certsToUse is 
	 * less than numCerts). Have the TP construct us an ordered verified
	 * group.
	 */
	crtn = CSSM_TP_CertGroupConstruct(
		tpHand,
		clHand,
		cspHand,
		dbList,
		NULL,			// ConstructParams
		&certGroupFrag,
		&resultGroup);
	if(crtn) {
		printError("CSSM_TP_CertGroupConstruct", crtn);
		return testError();
	}
	
 	/* vfy resultGroup is identical to unbroken part of chain */
 	if(verbose) {
 		printf("   ...CSSM_TP_CertGroupConstruct returned %u certs\n",
 			(unsigned)resultGroup->NumCerts);
 	}
	if(resultGroup->NumCerts != certsToUse) {
		printf("\n***cgConstruct: resultGroup->NumCerts was %u, expected %u\n",
			(unsigned)resultGroup->NumCerts, (unsigned)certsToUse);
		rtn = testError();
		goto abort;
	}
	for(certDex=0; certDex<certsToUse; certDex++) {
		if(!appCompareCssmData(&certs[certDex], 
					&resultGroup->GroupList.CertList[certDex])) {
			printf("\ncgConstruct: ***certs[%d] miscompare\n", certDex);
			rtn = testError();
			goto abort;
		}
	}
abort:
	/* free resurces */
	tpFreeCertGroup(&certGroupFrag, 
		CSSM_FALSE,			// caller malloc'd the actual certs 
		CSSM_FALSE);		// struct is on stack
	tpFreeCertGroup(resultGroup, 
		CSSM_TRUE,			// mallocd by TP 
		CSSM_TRUE);			// ditto 
	#if	CG_CONSTRUCT_TP_DB
	if(dbList != NULL) {
		int i;
		CSSM_RETURN crtn;
		
		if(verbose) {
			printf("   ...deleting all certs from DBs\n");
		}	
		for(i=0; i<dbList->NumHandles; i++) {		
			clDeleteAllCerts(dbList->DLDBHandle[i]);
		}
		if(publicRead) {
			if(verbose) {
				printf("   ...closing DBs\n");
			}
			for(i=0; i<dbList->NumHandles; i++) {
				crtn = CSSM_DL_DbClose(dbList->DLDBHandle[i]);
				if(crtn) {
					printError("CSSM_DL_DbClose");
					if(testError()) {
						return 1;
					}
				}
			}
		}
	}
	#endif
	return rtn;
}
/*** end of code directly copied from ../cgConstruct/cgConstruct.cpp ***/

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

int cgConstructInit(TestParams *testParams)
{
	unsigned			numKeys = NUM_CERTS_MIN + testParams->threadNum;
	TT_KeyPairs			*keyPairs;
	
	if(testParams->verbose) {
		printf("cgConstruct thread %d: generating keys...\n", 
			testParams->threadNum);
	}
	keyPairs = (TT_KeyPairs *)CSSM_MALLOC(sizeof(TT_KeyPairs));
	keyPairs->numKeys = numKeys;
	keyPairs->pubKeys  = (CSSM_KEY_PTR)CSSM_CALLOC(numKeys, sizeof(CSSM_KEY));
	keyPairs->privKeys = (CSSM_KEY_PTR)CSSM_CALLOC(numKeys, sizeof(CSSM_KEY));
	CSSM_DL_DB_HANDLE nullDb = {0, 0};
	if(tpGenKeys(testParams->cspHand,
			nullDb,					// dbHand
			numKeys,
			KEYGEN_ALG_DEF, 
			CG_KEY_SIZE_DEFAULT,
			"cgConstruct",		// keyLabelBase
			keyPairs->pubKeys,
			keyPairs->privKeys)) {
		goto abort;
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

int cgConstruct(TestParams *testParams)
{
	unsigned 			loopNum;
	int					status = -1;	// exit status, default = error
	TT_KeyPairs			*keyPairs = (TT_KeyPairs *)testParams->perThread;
	unsigned			dex;
	
	/* all three of these are arrays with numCert elements */
	CSSM_KEY_PTR		pubKeys = keyPairs->pubKeys;
	CSSM_KEY_PTR		privKeys = keyPairs->privKeys;
	CSSM_DATA_PTR		certs = NULL;
	
	unsigned			numCerts = keyPairs->numKeys;
	uint32				sigAlg = SIG_ALG_DEF;
	CSSM_DL_DB_LIST		dbList = {0, NULL};	/* for storing certs */
	CSSM_DL_DB_LIST_PTR	dbListPtr;			/* pts to dbList or NULL */
	CSSM_BOOL			publicRead = CSSM_FALSE;
	CSSM_BOOL			allInDbs = CSSM_FALSE;
	CSSM_BOOL			skipFirstDb = CSSM_FALSE;
	
	/* malloc empty certs */
	certs    = (CSSM_DATA_PTR)CSSM_CALLOC(numCerts, sizeof(CSSM_DATA));
	if(certs == NULL) {
		printf("not enough memory for %u certs.\n", numCerts);
		goto abort;
	}
	memset(certs, 0, numCerts * sizeof(CSSM_DATA));
	
	dbList.NumHandles = 0;
	dbList.DLDBHandle = NULL;
	dbListPtr = &dbList;
	for(loopNum=0; loopNum<testParams->numLoops; loopNum++) {
		
		/* generate certs */
		if(testParams->verbose) {
			printf("cgConstruct thread %d: generating certs...\n", 
				testParams->threadNum);
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
			status = 1;
			goto abort;
		}
	
		status = doTest(testParams->tpHand,
				testParams->clHand,
				testParams->cspHand,
				dbListPtr,
				certs,
				numCerts,
				testParams->verbose,
				allInDbs,
				skipFirstDb,
				publicRead);
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

