/*
 * cgConstruct.c - basic test of TP's CertGroupConstruct
 *
 * cook up array of n key pairs;
 * cook up cert chain to go with them;
 * main test loop {
 * 	pick a random spot to break the cert chain - half the time use the
 *		whole chain, half the time break it;
 * 	cook up CertGroup frag big enough for the unbroken part of the chain;
 * 	put cert[0] in certGroup[0];
 * 	for each cert from cert[1] to break point {
 *		roll the  dice and put the cert in either a random place
 *			in certGroup or in DB;
 * 	}
 *  resultGroup = certGroupConstruct();
 *  vfy result Grp is identical to unbroken part of chain;
 *  delete certs from DB;
 * }
 */
 
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

#define NUM_CERTS_DEF		10
#define NUM_DBS_DEF			3
#define KEYGEN_ALG_DEF		CSSM_ALGID_RSA
#define SIG_ALG_DEF			CSSM_ALGID_SHA1WithRSA 
#define LOOPS_DEF			10
#define DB_NAME_BASE		"cgConstruct"	/* default */
#define SECONDS_TO_LIVE		(60 * 60 * 24)	/* certs are valid for this long */

/* Read-only access not supported */ 
#define PUBLIC_READ_ENABLE	0

static void usage(char **argv)
{
	printf("Usage: %s [options]\n", argv[0]);
	printf("   Options:\n");
	printf("   n=numCerts; default = %d\n", NUM_CERTS_DEF);
	printf("   l=loops; default=%d; 0=forever\n", LOOPS_DEF);
	printf("   a=alg (f=FEE/MD5, f=FEE/SHA1, e=FEE/ECDSA, r=RSA, E=ANSI ECDSA; default = RSA\n");
	printf("   K=keySizeInBits\n");
	#if	TP_DB_ENABLE
	printf("   d=numDBs, default = %d\n", NUM_DBS_DEF);
	printf("   A(ll certs to DBs)\n");
	printf("   k (skip first DB when storing)\n");
	#if PUBLIC_READ_ENABLE
	printf("   p(ublic access open on read)\n");
	#endif
	#endif
	printf("   f=fileNameBase (default = %s)\n", DB_NAME_BASE);
	printf("   P(ause on each loop)\n");
	printf("   v(erbose)\n");
	printf("   q(uiet)\n");
	printf("   h(elp)\n");
	exit(1);
}

#if TP_DB_ENABLE
static int doOpenDbs(
	CSSM_DL_HANDLE			dlHand,
	const char 				*dbNameBase,
	CSSM_DL_DB_HANDLE_PTR 	dlDbPtr,
	unsigned 				numDbs,
	CSSM_BOOL 				publicReadOnly,		// ignored if !PUBLIC_READ_ENABLE
	CSSM_BOOL				quiet)
{
	unsigned i;
	char dbName[20];
	CSSM_BOOL doCreate = (publicReadOnly ? CSSM_FALSE : CSSM_TRUE);
	
	for(i=0; i<numDbs; i++) {
		dlDbPtr[i].DLHandle = dlHand;
		sprintf(dbName, "%s%d", dbNameBase, i);
		CSSM_RETURN crtn = tpKcOpen(dlHand, dbName, 
			dbName,			// file name as pwd
			doCreate,
			&dlDbPtr[i].DBHandle);
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
	CSSM_DL_HANDLE		dlHand,
	CSSM_DL_DB_LIST_PTR	dbList,
	CSSM_DATA_PTR		certs,
	unsigned			numCerts,
	CSSM_BOOL			verbose,
	CSSM_BOOL			allInDbs,
	CSSM_BOOL			skipFirstDb,
	CSSM_BOOL			publicRead,		// close/open with public access
	const char			*fileBaseName,
	CSSM_BOOL			quiet)
{
	unsigned				certsToUse;		// # of certs we actually use
	CSSM_CERTGROUP			certGroupFrag;	// INPUT to CertGroupConstruct
	CSSM_CERTGROUP_PTR		resultGroup;	// OUTPUT from "
	unsigned				certDex;
	int						rtn = 0;
	CSSM_RETURN				crtn;
	
	#if	TP_DB_ENABLE
	if(publicRead && (dbList != NULL)) {
		/* DBs are closed on entry, open r/w */
		if(doOpenDbs(dlHand,
				fileBaseName,
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
			#if		TP_DB_ENABLE
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
	 	printf("Error in tpMakeRandCertGroup\n");
	 	return testError(quiet);
	}
 		
	if(certGroupFrag.NumCerts > certsToUse) {
		printf("Error NOMAD sterlize\n");
		exit(1);
	}
	
	#if	TP_DB_ENABLE
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
				printError("CSSM_DL_DbClose", crtn);
				if(testError(quiet)) {
					return 1;
				}
			}
		}
		if(verbose) {
			printf("   ...opening DBs read-only\n");
		}
		if(doOpenDbs(dlHand,
				fileBaseName,
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
		return testError(quiet);
	}
	
 	/* vfy resultGroup is identical to unbroken part of chain */
 	if(verbose) {
 		printf("   ...CSSM_TP_CertGroupConstruct returned %u certs\n",
 			(unsigned)resultGroup->NumCerts);
 	}
	if(resultGroup->NumCerts != certsToUse) {
		printf("***resultGroup->NumCerts was %u, expected %u\n",
			(unsigned)resultGroup->NumCerts, (unsigned)certsToUse);
		rtn = testError(quiet);
		goto abort;
	}
	for(certDex=0; certDex<certsToUse; certDex++) {
		if(!appCompareCssmData(&certs[certDex], 
					&resultGroup->GroupList.CertList[certDex])) {
			printf("***certs[%d] miscompare\n", certDex);
			rtn = testError(quiet);
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
	#if	TP_DB_ENABLE
	if(dbList != NULL) {
		unsigned i;
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
					printError("CSSM_DL_DbClose", crtn);
					if(testError(quiet)) {
						return 1;
					}
				}
			}
		}
	}
	#endif
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
	CSSM_DL_DB_LIST		dbList = {0, NULL};	/* for storing certs */
	CSSM_DL_DB_LIST_PTR	dbListPtr;			/* pts to dbList or NULL */
	unsigned			i;
	char				*notAfterStr;
	char 				*notBeforeStr;
	CSSM_DL_HANDLE 		dlHand;
	
	/* all three of these are arrays with numCert elements */
	CSSM_KEY_PTR		pubKeys = NULL;
	CSSM_KEY_PTR		privKeys = NULL;
	CSSM_DATA_PTR		certs = NULL;
	
	/* Keys do NOT go in the cert DB */
	CSSM_DL_DB_HANDLE 	keyDb = {0, 0};

	/*
	 * User-spec'd params
	 */
	unsigned	loops = LOOPS_DEF;
	CSSM_BOOL	verbose = CSSM_FALSE;
	CSSM_BOOL	quiet = CSSM_FALSE;
	unsigned	numCerts = NUM_CERTS_DEF;
	uint32		keyGenAlg = KEYGEN_ALG_DEF;
	uint32		sigAlg = SIG_ALG_DEF;
	unsigned	numDBs = NUM_DBS_DEF;
	CSSM_BOOL	allInDbs = CSSM_FALSE;
	CSSM_BOOL	skipFirstDb = CSSM_FALSE;
	CSSM_BOOL	publicRead = CSSM_FALSE;
	CSSM_BOOL	doPause = CSSM_FALSE;
	uint32		keySizeInBits = CSP_KEY_SIZE_DEFAULT;
	const char	*fileBaseName = DB_NAME_BASE;
	
	for(arg=1; arg<argc; arg++) {
		argp = argv[arg];
		switch(argp[0]) {
		    case 'l':
				loops = atoi(&argp[2]);
				break;
		    case 'n':
				numCerts = atoi(&argp[2]);
				break;
		    case 'K':
				keySizeInBits = atoi(&argp[2]);
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
					case 'E':
						keyGenAlg = CSSM_ALGID_ECDSA;
						sigAlg = CSSM_ALGID_SHA1WithECDSA;
						break;
					case 'r':
						break;
					default:
						usage(argv);
				}
				break;
			case 'd':
				numDBs = atoi(&argp[2]);
				break;
			case 'A':
				allInDbs = CSSM_TRUE;
				break;
			case 'k':
				skipFirstDb = CSSM_TRUE;
				break;
			#if PUBLIC_READ_ENABLE
			case 'p':
				publicRead = CSSM_TRUE;
				break;
			#endif
			case 'f':
				fileBaseName = &argp[2];
				break;
			case 'P':
				doPause = CSSM_TRUE;
				break;
		    case 'h':
		    default:
				usage(argv);
		}
	}

	/* attach to all the modules we need */
	cspHand = cspStartup();
	if(cspHand == 0) {
		exit(1);
	}
	if(cspHand == 0) {
		exit(1);
	}
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
	
	printf("Starting cgConstruct; args: ");
	for(i=1; i<(unsigned)argc; i++) {
		printf("%s ", argv[i]);
	}
	printf("\n");
	
	/* generate key pairs */
	if(!quiet) {
		printf("generating keys...\n");
	}
	if(tpGenKeys(cspHand,
			keyDb,
			numCerts,
			keyGenAlg, 
			keySizeInBits,
			"cgConstruct",		// keyLabelBase
			pubKeys,
			privKeys)) {
		goto abort;
	}
	notBeforeStr = genTimeAtNowPlus(0);
	notAfterStr = genTimeAtNowPlus(SECONDS_TO_LIVE);

	#if	TP_DB_ENABLE
	/* create numDbs new DBs */
	if(numDBs != 0) {
		dlHand = dlStartup();
		if(dlHand == 0) {
			exit(1);
		}
		dbList.NumHandles = numDBs;
		dbList.DLDBHandle = 
			(CSSM_DL_DB_HANDLE_PTR)CSSM_CALLOC(numDBs, sizeof(CSSM_DL_DB_HANDLE));
		if(!publicRead) {
			/*
			 * In this case, this is the only time we open these DBs - they
			 * stay open for the duration of the test
			 */
			if(verbose) {
				printf("   ...opening DBs read/write\n");
			}
			if(doOpenDbs(dlHand,
					fileBaseName,
					dbList.DLDBHandle,
					numDBs,
					CSSM_FALSE,		// publicReadOnly: this is create/write
					quiet)) {
				goto abort;
			}
		}
		dbListPtr = &dbList;
	}		
	else {
		/* it's required anyway... */
		dbList.NumHandles = 0;
		dbList.DLDBHandle = NULL;
		dbListPtr = &dbList;
	}
	#else
	/* it's required anyway... */
	dbList.NumHandles = 0;
	dbList.DLDBHandle = NULL;
	dbListPtr = &dbList;
	#endif
	
	for(loop=1; ; loop++) {
		if(!quiet) {
			printf("...loop %d\n", loop);
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
				dlHand,
				dbListPtr,
				certs,
				numCerts,
				verbose,
				allInDbs,
				skipFirstDb,
				publicRead,
				fileBaseName,
				quiet)) {
			break;			
		}
		for(i=0; i<numCerts; i++) {
			appFreeCssmData(&certs[i], CSSM_FALSE);
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
	/* free keys and certs */
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
	if(dbList.DLDBHandle != NULL) {
		/* don't have to close, detach should do that */
		CSSM_FREE(dbList.DLDBHandle);
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
