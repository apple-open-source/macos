/*
 * Multithread exerciser - beat up on CSP, TP, and CL from multiple threads.
 *
 * Written by Doug Mitchell. 
 *
 *
 * Spawn a user-spec'd number of threads, each of which does the following:
 *
 * testThread(testParams) {
 *    roll the dice;
 *    depending on dieValue {
 *		cgVerify test;
 *		cgConstruct test;
 *      sslPing() test;
 *		etc....
 *    }
 * }
 */
#include <utilLib/common.h>
#include <utilLib/cspwrap.h>	
#include <clAppUtils/clutils.h>
#include "testParams.h"
#include <security_utilities/threading.h>
#include <security_utilities/utilities.h>
#include <security_utilities/devrandom.h>
#include <pthread.h>
#include <Security/Security.h>

#include <stdio.h>
#include <stdlib.h>

/*
 * As of 3/15/2001, can't link apps which use Security.framework against BSAFE.
 */
#define BSAFE_ENABLE	0

#define NUM_LOOPS		100
#define NUM_THREADS		20

/* function for both test init and test proper */
typedef int (*testFcn)(TestParams *testParams);

/* one test */
typedef struct {
	testFcn 	testInit;
	testFcn 	testRun;
	const char	*testName;
	char		enable;
} TestDef;

/* the tests we know about */

#define CG_CONSTRUCT_ENABLE		1		/* leak free 12/19 */
#define CG_VERIFY_ENABLE		1		/* leak free 12/19 */
#define SIGN_VFY_ENABLE			1		/* leak free */
#define SYM_TEST_ENABLE			1		/* leak free */
#define TIME_ENABLE				0		/* normally off */
#define SSL_PING_ENABLE			0		/* leak free 12/19 */
#define GET_FIELDS_ENABLE		1		/* leak free */
#define GET_CACHED_FLDS_ENABLE	1		/* leak free */
#define DER_DECODE_ENABLE		0
#define ATTACH_ENABLE			1		/* leak free */
#define SEC_TRUST_ENABLE		0		/* works but leaks per 3737232 */
#define KC_STATUS_ENABLE		0		/* currently fails: see 6368768 */
#define DIGEST_CLIENT_ENABLE	1
#define MDS_LOOKUP_ENABLE		1		/* leak free */
#define CSSM_ERR_STR_ENABLE		0		/* leak free */
#define TRUST_SETTINGS_ENABLE	1
#define DB_SETTINGS_ENABLE		0		/* not thread safe */
#define COPY_ROOTS_ENABLE       1

#if		BSAFE_ENABLE
#define RSA_SIGN_ENABLE			1
#define DES_ENABLE				1
#else
#define RSA_SIGN_ENABLE			0
#define DES_ENABLE				0
#endif	/* BSAFE_ENABLE */
#define SSL_THRASH_ENABLE		0
#define CSP_RAND_ENABLE			0

/* when adding to this table be sure to update setTestEnables() as well */
TestDef testArray[] = {
	{ cgConstructInit, 		cgConstruct,	"cgConstruct", 	CG_CONSTRUCT_ENABLE },
	{ cgVerifyInit, 		cgVerify,		"cgVerify", 	CG_VERIFY_ENABLE 	},
	{ signVerifyInit, 		signVerify,		"signVerify", 	SIGN_VFY_ENABLE 	},
	{ symTestInit, 			symTest,		"symTest", 		SYM_TEST_ENABLE 	},
	{ timeInit, 			timeThread,		"timeThread", 	TIME_ENABLE 		},
	{ sslPingInit, 			sslPing,		"sslPing", 		SSL_PING_ENABLE		},
	{ getFieldsInit, 		getFields,		"getFields", 	GET_FIELDS_ENABLE	},
	{ getCachedFieldsInit,	getCachedFields,"getCachedFields",GET_CACHED_FLDS_ENABLE},
	{ attachTestInit, 		attachTest,		"attachTest", 	ATTACH_ENABLE		},
	{ sslThrashInit, 		sslThrash,		"sslThrash", 	SSL_THRASH_ENABLE	},
	{ cspRandInit,	 		cspRand,		"cspRand", 		CSP_RAND_ENABLE		},
	{ derDecodeInit,		derDecodeTest,	"derDecode",	DER_DECODE_ENABLE 	},
	{ secTrustEvalInit,		secTrustEval,	"secTrustEval",	SEC_TRUST_ENABLE 	},
	{ kcStatusInit,			kcStatus,		"kcStatus",		KC_STATUS_ENABLE	},
	{ digestClientInit,		digestClient,	"digestClient",	DIGEST_CLIENT_ENABLE},
	{ mdsLookupInit,		mdsLookup,		"mdsLookup",	MDS_LOOKUP_ENABLE	},
	{ cssmErrStrInit,		cssmErrStr,		"cssmErrStr",	CSSM_ERR_STR_ENABLE	},
	{ trustSettingsInit,	trustSettingsEval, "trustSettingsEval", TRUST_SETTINGS_ENABLE },
	{ dbOpenCloseInit,		dbOpenCloseEval, "dbOpenClose", DB_SETTINGS_ENABLE  },
    { copyRootsInit,        copyRootsTest,  "copyRoots",    COPY_ROOTS_ENABLE   },
	#if	BSAFE_ENABLE
	{ desInit,		 		desTest,		"desTest", 		DES_ENABLE			},
	{ rsaSignInit,	 		rsaSignTest,	"rsaSignTest", 	RSA_SIGN_ENABLE		}
	#endif
};
#define NUM_THREAD_TESTS	(sizeof(testArray) / sizeof(TestDef))

static void usage(char **argv)
{
    printf("Usage: %s [options]\n", argv[0]);
    printf("Options:\n");
    printf("   l=loopCount (default = %d)\n", NUM_LOOPS);
	printf("   t=threadCount (default = %d)\n", NUM_THREADS);
	printf("   e[cvsytpfabdFSrDTkmCer] - enable specific tests\n");
	printf("       c=cgConstruct    v=cgVerify    s=signVerify   y=symTest\n");
	printf("       t=timeThread     p=sslPing     f=getFields    a=attach\n");
	printf("       b=bsafeSignVfy   d=bsafeDES    F=getCachedFields\n");
	printf("       S=sslThrash      r=cspRand     D=derDecode    T=SecTrustEval\n");
	printf("       k=kcStatus       m=mdsLookup   C=digestClient e=cssmErrorStr\n");
	printf("       R=TrustSetting   B=DBOpenClose o=copyRoots\n");
	printf("   o=test_specific_opts (see source for details)\n");
	printf("   a(bort on error)\n");
	printf("   r(un loop)\n");
	printf("   q(uiet)\n");
	printf("   v(erbose)\n");
	printf("   s(ilent)\n");
	printf("   h(elp)\n");
    exit(1);
}

/* it happens from time to time on SSL ping */
#include <signal.h>
void sigpipe(int sig) 
{ 
	fflush(stdin);
	printf("***SIGPIPE***\n");
}

/* common thread-safe routines */
static Security::DevRandomGenerator devRand;

CSSM_RETURN threadGetRandData(
	const TestParams 	*testParams,
	CSSM_DATA_PTR		data,		// mallocd by caller
	unsigned			numBytes)	// how much to fill
{
	devRand.random(data->Data, numBytes);
	data->Length = numBytes;
	return CSSM_OK;
}

/* delay a random amount, 0<delay<10ms */
#define MAX_DELAY_US	10000
void randomDelay()
{
	unsigned char usec;
	devRand.random(&usec, 1);
	usec %= 10000;
	usleep(usec);
}

/* in case printf() is malevolently unsafe */

static Mutex printLock;

void printChar(char c)
{
	StLock<Mutex> _(printLock);
	printf("%c", c);
	fflush(stdout);
}

/* 
 * Optionally start up a CFRunLoop. This is needed to field keychain event callbacks, used
 * to maintain root cert cache coherency. 
 */
 
/* first we need something to register so we *have* a run loop */
static OSStatus kcCacheCallback (
   SecKeychainEvent keychainEvent,
   SecKeychainCallbackInfo *info,
   void *context)
{
	return noErr;
}

/* main thread has to wait for this to be set to know a run loop has been set up */
static int runLoopInitialized = 0;

/* this is the thread which actually runs the CFRunLoop */
void *cfRunLoopThread(void *arg)
{
	OSStatus ortn = SecKeychainAddCallback(kcCacheCallback, 
		kSecTrustSettingsChangedEventMask, NULL);
	if(ortn) {
		printf("registerCacheCallbacks: SecKeychainAddCallback returned %d", (int32_t)ortn);
		/* Not sure how this could ever happen - maybe if there is no run loop active? */
		return NULL;
	}
	runLoopInitialized = 1;
	CFRunLoopRun();
	/* should not be reached */
	printf("\n*** Hey! CFRunLoopRun() exited!***\n");
	return NULL;
}

static int startCFRunLoop()
{
	pthread_t runLoopThread;
	
	int result = pthread_create(&runLoopThread, NULL, cfRunLoopThread, NULL);
	if(result) {
		printf("***pthread_create returned %d, aborting\n", result);
		return -1;
	}
	return 0;
}

/* main pthread body */
void *testThread(void *arg)
{
	TestParams *testParams = (TestParams *)arg;
	int status;
	
	TestDef *thisTestDef = &testArray[testParams->testNum];
	status = thisTestDef->testRun(testParams);
	if(!testParams->quiet) {
		printf("\n...thread %d test %s exiting with status %d\n", 
			testParams->threadNum, thisTestDef->testName, status);
	}
	pthread_exit((void*)status);
	/* NOT REACHED */
	return (void *)status;
}

/*
 * Set enables in testArray[] 
 */
static void setOneEnable(testFcn f)
{
	unsigned dex;
	for(dex=0; dex<NUM_THREAD_TESTS; dex++) {
		if(testArray[dex].testRun == f) {
			testArray[dex].enable = 1;
			return;
		}
	}
	printf("****setOneEnable: test not found\n");
	exit(1);
}

static void setTestEnables(const char *enables, char **argv)
{
	/* first turn 'em all off */
	unsigned dex;
	for(dex=0; dex<NUM_THREAD_TESTS; dex++) {
		testArray[dex].enable = 0;
	}
	
	/* enable specific ones */
	while(*enables != '\0') {
		switch(*enables) {
			case 'c':	setOneEnable(cgConstruct); break;
			case 'v':	setOneEnable(cgVerify); break;
			case 's':	setOneEnable(signVerify); break;
			case 'y':	setOneEnable(symTest); break;
			case 't':	setOneEnable(timeThread); break;
			case 'p':	setOneEnable(sslPing); break;
			case 'f':	setOneEnable(getFields); break;
			case 'F':	setOneEnable(getCachedFields); break;
			case 'a':	setOneEnable(attachTest); break;
			case 'S':	setOneEnable(sslThrash); break;
			case 'r':	setOneEnable(cspRand); break;
			case 'D':	setOneEnable(derDecodeTest); break;
			case 'T':	setOneEnable(secTrustEval); break;
			case 'k':	setOneEnable(kcStatus); break;
			case 'C':	setOneEnable(digestClient); break;
			case 'm':	setOneEnable(mdsLookup); break;
			case 'e':	setOneEnable(cssmErrStr); break;
			case 'R':	setOneEnable(trustSettingsEval); break;
			case 'B':   setOneEnable(dbOpenCloseEval); break;
			case 'o':   setOneEnable(copyRootsTest); break;
			#if		BSAFE_ENABLE
			case 'b':	setOneEnable(rsaSignTest); break;
			case 'd':	setOneEnable(desTest); break;
			#endif
			default:
				usage(argv);
		}
		enables++;
	}
}

int main(int argc, char **argv)
{   
	CSSM_CSP_HANDLE	cspHand = 0;
	CSSM_CL_HANDLE	clHand = 0;
	CSSM_TP_HANDLE	tpHand = 0;
	unsigned		errCount = 0;
	TestParams		*testParams;
	TestParams		*thisTest;
	unsigned		dex;
	pthread_t		*threadList;
	int				arg;
	char			*argp;
	int				result;
	TestDef			*thisTestDef;
	unsigned		numValidTests;
	unsigned		i,j;
	
	/* user-spec'd parameters */
	char			quiet = 0;
	char			verbose = 0;
	unsigned		numThreads = NUM_THREADS;
	unsigned		numLoops = NUM_LOOPS;
	char			*testOpts = NULL;
	bool			abortOnError = false;
	bool			silent = false;
	
	for(arg=1; arg<argc; arg++) {
		argp = argv[arg];
		switch(argp[0]) {
			case 'l':
				numLoops = atoi(&argp[2]);
				break;
		    case 't':
				numThreads = atoi(&argp[2]);
				break;
				break;
			case 'q':
				quiet = 1;
				break;
			case 'v':
				verbose = 1;
				break;
			case 'o':
				if((argp[1] != '=') || (argp[2] == '\0')) {
					usage(argv);
				}
				testOpts = argp + 2;
				break;
			case 'e':
				setTestEnables(argp + 1, argv);
				break;
			case 'a':
				abortOnError = true;
				break;
			case 'r':
				startCFRunLoop();
				break;
			case 's':
				silent = true;
				quiet = 1;
				break;
			default:
				usage(argv);
		}
	}

	/* attach to all three modules */
	cspHand = cspStartup();
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
	signal(SIGPIPE, sigpipe);
	
	/* malloc and init TestParams for all requested threads */
	testParams = (TestParams *)malloc(numThreads * sizeof(TestParams));
	for(dex=0; dex<numThreads; dex++) {
		thisTest 				= &testParams[dex];
		thisTest->numLoops 		= numLoops;
		thisTest->verbose 		= verbose;
		thisTest->quiet 		= quiet;
		thisTest->threadNum 	= dex;
		thisTest->cspHand 		= cspHand;
		thisTest->clHand 		= clHand;
		thisTest->tpHand 		= tpHand;
		thisTest->testOpts		= testOpts;
		
		if(dex < 10) {
			/* 0..9 */
			thisTest->progressChar 	= '0' + dex;
		}
		else if(dex < 36) {
			/* a..z */
			thisTest->progressChar 	= 'a' + dex - 10;
		}
		else {
			/* A..Z and if X can run more threads than that, I'll be surprised */
			thisTest->progressChar 	= 'Z' + dex - 36;
		}
	}
	
	/* Adjust testArray for tests which are actually enabled */
	numValidTests = 0;
	dex=0;
	for(i=0; i<NUM_THREAD_TESTS; i++) {
		if(testArray[dex].enable) {
			numValidTests++;
			dex++;
		}
		else {
			/* delete this one, move remaining tests up */
			for(j=dex; j<NUM_THREAD_TESTS-1; j++) {
				testArray[j] = testArray[j+1];
			}
			/* and re-examine testArray[dex], which we just rewrote */
		}
	}
	
	if(!silent) {
		printf("Starting threadTest; args: ");
		for(i=1; i<(unsigned)argc; i++) {
			printf("%s ", argv[i]);
		}
		printf("\n");
	}
	
	/* assign a test module to each thread and run its init routine */
	for(dex=0; dex<numThreads; dex++) {
		/* roll the dice */
		thisTest = &testParams[dex];
		thisTest->testNum = genRand(0, numValidTests - 1);
		
		thisTestDef = &testArray[thisTest->testNum];
		if(!quiet) {
			printf("...thread %d: test %s\n", dex, thisTestDef->testName);
		}
		result = thisTestDef->testInit(thisTest);
		if(result) {
			printf("***Error on %s init; aborting\n", thisTestDef->testName);
			errCount++;
			goto abort;
		}
	}
	

	/* start up each thread */
	threadList = (pthread_t *)malloc(numThreads * sizeof(pthread_t));
	for(dex=0; dex<numThreads; dex++) {
		int result = pthread_create(&threadList[dex], NULL, 
			testThread, &testParams[dex]);
		if(result) {
			printf("***pthread_create returned %d, aborting\n", result);
			errCount++;
			goto abort;
		}
	}
	
	/* wait for each thread to complete */
	for(dex=0; dex<numThreads; dex++) {
		void *status;
		result = pthread_join(threadList[dex], &status);
		if(result) {
			printf("***pthread_join returned %d, aborting\n", result);
			goto abort;
		}
		if(!quiet) {
			printf("\n...joined thread %d, status %d\n", 
				dex, status ? 1 : 0);
		}
		if(status != NULL) {
			errCount++;
			if(abortOnError) {
				break;
			}
		}
	}
	if(errCount || !quiet) {
		printf("threadTest complete; errCount %d\n", errCount);
	}
abort:
	if(cspHand != 0) {
		CSSM_ModuleDetach(cspHand);
	}
	if(clHand != 0) {
		CSSM_ModuleDetach(clHand);
	}
	if(tpHand != 0) {
		CSSM_ModuleDetach(tpHand);
	}
	return errCount;
}


