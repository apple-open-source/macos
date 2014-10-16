/*
 * secTime.cpp - measure performance of Sec* ops
 */
 
#include <Security/Security.h>
#include <security_cdsa_utils/cuFileIo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <CoreFoundation/CoreFoundation.h>
#include <security_utilities/devrandom.h>
#include <clAppUtils/certVerify.h>
#include <clAppUtils/clutils.h>
#include <utilLib/common.h>

/*
 * Hard coded test params
 */
 
/* 
 * The keychain we open and search for certs.
 * MUST exist in ~/Library/Keychains.
 * YOU can create it with 
 *  % certtool c k=secTimeKc c p=secTimeKc Z
 */
#define ST_KC_NAME		"secTimeKc"

/*
 * Certs to verify with SecTrust. We have a variety because the timing on
 * this test is highly dependent on the position of the verifying anchor
 * within the system anchors list. (This does not apply to Trust Settings). 
 */
typedef struct {
	const char *certFileName;
	const char *hostName;
} CertToVerify;

static const CertToVerify certsToVerify[] = 
{
	{ 
		/* issuer: Secure Server Certification Authority */
		"amazon_v3.100.cer",
		"www.amazon.com"
	},
	{
		/* issuer: Equifax Secure Certificate Authority */
		"firstamlink.cer",		
		"www.firstamlink.com"
	},
};

#define NUM_ST_CERTS	(sizeof(certsToVerify) / sizeof(certsToVerify[0]))

/* explicit anchor for certsToVerify[0] */
#define ST_ANCHOR_NAME	"SecureServer.509.cer"		/* verifies ST_CERT_HOST */

/*
 * Cert chain for cgv3*()
 */
#define THAWTE_LEAF	"dmitchThawte.cer"
#define THAWTE_CA	"ThawteCA.cer"
#define THAWTE_ROOT	"ThawteRoot.cer"

static void usage(char **argv)
{
    printf("Usage: %s [option ...]\n", argv[0]);
    printf("Options:\n");
	printf("   t=testspec; default=all\n");
	printf("     test specs: o keychainOpen\n");
	printf("                 s keychainSearch\n");
	printf("                 e secTrustEvaluate\n");
	printf("                 k SecKeychainCopySearchList\n");
	printf("                 v TP CertGroupVerify, system anchor\n");
	printf("                 V TP CertGroupVerify, explicit anchor\n");
	printf("                 3 TP CertGroupVerify, 3 certs w/anchor\n");
	printf("   l=loops (only valid if testspec is given)\n");
	exit(1);
}

static void printSecErr(
	const char *op,
	OSStatus ortn)
{
	printf("%s returned %ld\n", op, (unsigned long)ortn);
}

/*
 * Struct passed around to test-specific functions.
 */
typedef struct {
	const char 			*testName;
	void				*testPriv;
} TestParams;

/*
 * Each subtest has three functions - init, run, cleanup, called out 
 * from main().
 *
 * Init's job is one-time setup - open files, setup buffers, etc. 
 * PErsistent state can be saved in TestParams->testPriv.
 */
typedef OSStatus (*testInitFcn)(TestParams *testParams);

/*
 * Run's job is to perform 1 iteration with as 
 * little overhead as possible. Return the time spect actually
 * doing the deed in *timeSpent.
 */
typedef OSStatus (*testRunFcn)(TestParams *testParams,
	unsigned loopNum,
	CFAbsoluteTime *timeSpent);
	
/*
 * CLeanu up any resources allocd in init.
 */
typedef OSStatus (*testCleanupFcn)(TestParams *testParams);

/*
 * Static declaration of a test
 */
typedef struct {
	const char 			*testName;
	unsigned			loops;
	testInitFcn			init;
	testRunFcn			run;
	testCleanupFcn		cleanup;
	char				testSpec;		// for t=xxx cmd line opt
} TestDefs;

#pragma mark ---- Individual tests ----

#ifdef	use_these_as_a_template

static OSStatus xxxInit(
	TestParams *testParams)
{
	return noErr;
}

static OSStatus xxRun(
	TestParams *testParams,
	unsigned loopNum,
	CFAbsoluteTime *timeSpent)
{
	CFAbsoluteTime startTime = CFAbsoluteTimeGetCurrent();
	/* do the op here */
	*timeSpent = CFAbsoluteTimeGetCurrent() - startTime;
	return noErr;
}

static OSStatus xxxCleanup(
	TestParams *testParams)
{
	return noErr;
}
#endif	/* template */

#pragma mark -- keychain open --

static OSStatus kcOpenInit(
	TestParams *testParams)
{
	return noErr;
}

static OSStatus kcOpenRun(
	TestParams *testParams,
	unsigned loopNum,
	CFAbsoluteTime *timeSpent)
{
	SecKeychainRef kcRef;
	SecKeychainStatus status;
	
	CFAbsoluteTime startTime = CFAbsoluteTimeGetCurrent();
	OSStatus ortn = SecKeychainOpen(ST_KC_NAME, &kcRef);
	if(ortn) {
		printSecErr("SecKeychainOpen", ortn);
		return ortn;
	}
	ortn = SecKeychainGetStatus(kcRef, &status);
	if(ortn) {
		printSecErr("SecKeychainGetStatus", ortn);
		CFRelease(kcRef);
		if(errSecNoSuchKeychain == ortn) {
			printf("The keychain %s does not exist. Please create it"
				" and populate it like so:\n", ST_KC_NAME);
			printf(" certtool c k=%s c p=%s Z\n",
				ST_KC_NAME, ST_KC_NAME);
		}
		return ortn;
	}
	CFAbsoluteTime endTime = CFAbsoluteTimeGetCurrent();
	*timeSpent = endTime - startTime;
	CFRelease(kcRef);
	return noErr;
}

static OSStatus kcOpenCleanup(
	TestParams *testParams)
{
	return noErr;
}

#pragma mark -- keychain lookup --

/* 
 * Private *testPriv is a kcRef.
 */
static OSStatus kcSearchInit(
	TestParams *testParams)
{
	SecKeychainRef kcRef;
	OSStatus ortn = SecKeychainOpen(ST_KC_NAME, &kcRef);
	if(ortn) {
		printSecErr("SecKeychainOpen", ortn);
		return ortn;
	}
	testParams->testPriv = (void *)kcRef;
	return noErr;
}

static OSStatus kcSearchRun(
	TestParams *testParams,
	unsigned loopNum,
	CFAbsoluteTime *timeSpent)
{
	SecKeychainRef kcRef = (SecKeychainRef)testParams->testPriv;
	SecKeychainSearchRef srchRef = NULL;
	SecKeychainItemRef certRef = NULL;
	CFAbsoluteTime endTime;
	CFAbsoluteTime startTime = CFAbsoluteTimeGetCurrent();
	
	/* search for any cert */
	OSStatus ortn = SecKeychainSearchCreateFromAttributes(kcRef,
		kSecCertificateItemClass,
		NULL,		// no attrs
		&srchRef);
	if(ortn) {
		printSecErr("SecKeychainSearchCreateFromAttributes", ortn);
		return ortn;
	}
	ortn = SecKeychainSearchCopyNext(srchRef, &certRef);
	if(ortn) {
		printSecErr("SecKeychainSearchCopyNext", ortn);
		goto done;
	}
	endTime = CFAbsoluteTimeGetCurrent();
	*timeSpent = endTime - startTime;
done:
	if(srchRef) {
		CFRelease(srchRef);
	}
	if(certRef) {
		CFRelease(certRef);
	}
	return ortn;
}


static OSStatus kcSearchCleanup(
	TestParams *testParams)
{
	SecKeychainRef kcRef = (SecKeychainRef)testParams->testPriv;
	CFRelease(kcRef);
	return noErr;
}

#pragma mark -- SecTrustEvaluate --

/* 
 * Priv data is an array of SecCertificateRef containing certs to evaluate. 
 * Each run evaluates one of them.
 */
static OSStatus secTrustInit(
	TestParams *testParams)
{
	unsigned char *certData;
	unsigned certLen;
	CSSM_DATA cdata;
	
	SecCertificateRef *certRefs;
	
	certRefs = (SecCertificateRef *)malloc(
		sizeof(SecCertificateRef) * NUM_ST_CERTS);
		
	for(unsigned dex=0; dex<NUM_ST_CERTS; dex++) {
		if(readFile(certsToVerify[dex].certFileName, &certData, &certLen)) {
			printf("***Can not find cert file %s. Aborting.\n",
				certsToVerify[dex].certFileName);
			return -1;
		}
		cdata.Data = certData;
		cdata.Length = certLen;
		SecCertificateRef certRef;
		OSStatus ortn = SecCertificateCreateFromData(&cdata,
			CSSM_CERT_X_509v3,
			CSSM_CERT_ENCODING_DER, 
			&certRef);
		if(ortn) {
			printSecErr("SecCertificateCreateFromData", ortn);
			return ortn;
		}
		free(certData);			// mallocd by readFile() 
		certRefs[dex] = certRef;
	}
	testParams->testPriv = certRefs;
	return noErr;
}

static OSStatus secTrustRun(
	TestParams *testParams,
	unsigned loopNum,
	CFAbsoluteTime *timeSpent)
{
	unsigned whichDex = loopNum % NUM_ST_CERTS;
	SecCertificateRef *certRefs = (SecCertificateRef *)testParams->testPriv;
	SecCertificateRef certRef = certRefs[whichDex];
	
	/* 
	 * We measure the whole enchilada, that's what SecureTransport
	 * has to do
	 */
	CFAbsoluteTime startTime = CFAbsoluteTimeGetCurrent();

	CFMutableArrayRef certs;
	certs = CFArrayCreateMutable(NULL, 1, &kCFTypeArrayCallBacks);
	CFArrayInsertValueAtIndex(certs, 0, certRef);

	SecPolicyRef		policy = NULL;
	SecPolicySearchRef	policySearch = NULL;

	OSStatus ortn = SecPolicySearchCreate(CSSM_CERT_X_509v3,
		&CSSMOID_APPLE_TP_SSL,
		NULL,				// policy opts
		&policySearch);
	if(ortn) {
		printSecErr("SecPolicySearchCreate", ortn);
		return ortn;
	}
	
	ortn = SecPolicySearchCopyNext(policySearch, &policy);
	if(ortn) {
		printSecErr("SecPolicySearchCopyNext", ortn);
		return ortn;
	}
	CFRelease(policySearch);
	
	SecTrustRef secTrust;
	ortn = SecTrustCreateWithCertificates(certs, policy, &secTrust);
	if(ortn) {
		printSecErr("SecTrustCreateWithCertificates", ortn);
		return ortn;
	}
	/* no action data for now */

	SecTrustResultType secTrustResult;
	ortn = SecTrustEvaluate(secTrust, &secTrustResult);
	if(ortn) {
		printSecErr("SecTrustEvaluate", ortn);
		return ortn;
	}
	
	CFRelease(certs);
	CFRelease(secTrust);
	CFRelease(policy);
	
	CFAbsoluteTime endTime = CFAbsoluteTimeGetCurrent();
	*timeSpent = endTime - startTime;

	return noErr;
}

static OSStatus secTrustCleanup(
	TestParams *testParams)
{
	SecCertificateRef *certRefs = (SecCertificateRef*)testParams->testPriv;
	for(unsigned dex=0; dex<NUM_ST_CERTS; dex++) {
		CFRelease(certRefs[dex]);
	}
	free(certRefs);
	return noErr;
}

#pragma mark -- SecKeychainCopySearchList --

static OSStatus kcCSLInit(
	TestParams *testParams)
{
	return noErr;
}

static OSStatus kcCSLRun(
	TestParams *testParams,
	unsigned loopNum,
	CFAbsoluteTime *timeSpent)
{
	CFArrayRef sl;
	CFAbsoluteTime startTime = CFAbsoluteTimeGetCurrent();
	OSStatus ortn = SecKeychainCopySearchList(&sl);
	if(ortn) {
		printSecErr("SecKeychainCopySearchList", ortn);
		return ortn;
	}
	CFRelease(sl);
	*timeSpent = CFAbsoluteTimeGetCurrent() - startTime;
	return noErr;
}

static OSStatus kcCSLCleanup(
	TestParams *testParams)
{
	return noErr;
}

#pragma mark -- CSSM_TP_CertGroupVerify, system anchors --

/* private data allocated in cgvInit */
typedef struct {
	CSSM_TP_HANDLE			tpHand;
	CSSM_CL_HANDLE 			clHand;
	CSSM_CSP_HANDLE 		cspHand;
	BlobList				*certs[NUM_ST_CERTS];
	BlobList				*anchors;		/* cgvAnchor* only */
} CgvParams;

static OSStatus cgvInit(
	TestParams *testParams)
{
	CgvParams *cgvParams = (CgvParams *)malloc(sizeof(CgvParams));
	memset(cgvParams, 0, sizeof(CgvParams));
	cgvParams->tpHand    = tpStartup();
	cgvParams->clHand    = clStartup();
	cgvParams->cspHand   = cspStartup();
	for(unsigned dex=0; dex<NUM_ST_CERTS; dex++) {
		cgvParams->certs[dex] = new BlobList();
		cgvParams->certs[dex]->addFile(certsToVerify[dex].certFileName);
	}
	cgvParams->anchors   = NULL;
	testParams->testPriv = cgvParams;
	return noErr;
}

static OSStatus cgvRun(
	TestParams *testParams,
	unsigned loopNum,
	CFAbsoluteTime *timeSpent)
{
	CgvParams *cgvParams = (CgvParams *)testParams->testPriv;
	BlobList nullList;
	unsigned whichDex = loopNum % NUM_ST_CERTS;
	BlobList *certBlob = cgvParams->certs[whichDex];
	
	CFAbsoluteTime startTime = CFAbsoluteTimeGetCurrent();
	int rtn = certVerifySimple(
		cgvParams->tpHand,
		cgvParams->clHand,
		cgvParams->cspHand,
		*certBlob,				// contains one cert, the subject
		nullList,				// roots
		CSSM_TRUE,				// useSystemAnchors
		CSSM_FALSE,				// leafCertIsCa
		CSSM_FALSE,				// allow expired root
		CVP_SSL,
		certsToVerify[whichDex].hostName,
		CSSM_FALSE,				// sslClient
		NULL,					// senderEmail
		0,						// key use
		NULL,					// expected error str
		0,						// numCertErrors
		NULL,
		0,						// numCertStatus
		NULL,
		CSSM_FALSE,				// trustSettings
		CSSM_TRUE,				// quiet
		CSSM_FALSE);			// verbose
	*timeSpent = CFAbsoluteTimeGetCurrent() - startTime;
	if(rtn) {
		printf("***certVerify error\n");
		return (OSStatus)rtn;
	} 
	return noErr;
}

static OSStatus cgvCleanup(
	TestParams *testParams)
{
	CgvParams *cgvParams = (CgvParams *)testParams->testPriv;
	CSSM_ModuleDetach(cgvParams->cspHand);
	CSSM_ModuleDetach(cgvParams->tpHand);
	CSSM_ModuleDetach(cgvParams->clHand);
	for(unsigned dex=0; dex<NUM_ST_CERTS; dex++) {
		delete cgvParams->certs[dex];
	}
	if(cgvParams->anchors) {
		delete(cgvParams->anchors);
	}
	free(cgvParams);
	return noErr;
}

#pragma mark -- CSSM_TP_CertGroupVerify, explicit anchors --

static OSStatus cgvAnchorInit(
	TestParams *testParams)
{
	CgvParams *cgvParams = (CgvParams *)malloc(sizeof(CgvParams));
	memset(cgvParams, 0, sizeof(CgvParams));
	cgvParams->tpHand    = tpStartup();
	cgvParams->clHand    = clStartup();
	cgvParams->cspHand   = cspStartup();
	cgvParams->certs[0]  = new BlobList();
	cgvParams->certs[0]->addFile(certsToVerify[0].certFileName);
	cgvParams->anchors   = new BlobList();
	cgvParams->anchors->addFile(ST_ANCHOR_NAME);
	testParams->testPriv = cgvParams;
	return noErr;
}

static OSStatus cgvAnchorRun(
	TestParams *testParams,
	unsigned loopNum,
	CFAbsoluteTime *timeSpent)
{
	CgvParams *cgvParams = (CgvParams *)testParams->testPriv;
	BlobList nullList;

	CertVerifyArgs vfyArgs;
	memset(&vfyArgs, 0, sizeof(vfyArgs));
	vfyArgs.version = CERT_VFY_ARGS_VERS;
	
	vfyArgs.tpHand = cgvParams->tpHand;
	vfyArgs.clHand = cgvParams->clHand;
	vfyArgs.cspHand = cgvParams->cspHand;
	vfyArgs.certs = cgvParams->certs[0];
	vfyArgs.roots = cgvParams->anchors;
	vfyArgs.allowUnverified = CSSM_TRUE;
	vfyArgs.vfyPolicy = CVP_SSL;
	vfyArgs.revokePolicy = CRP_None;
	vfyArgs.sslHost = certsToVerify[0].hostName;
	vfyArgs.revokePolicy = CRP_None;
	vfyArgs.quiet = CSSM_TRUE;
	
	CFAbsoluteTime startTime = CFAbsoluteTimeGetCurrent();
	int rtn = certVerify(&vfyArgs);
	*timeSpent = CFAbsoluteTimeGetCurrent() - startTime;
	if(rtn) {
		printf("***certVerify error\n");
		return (OSStatus)rtn;
	} 
	return noErr;
}

/* cleanup - use cgvCleanup() */

#pragma mark -- CSSM_TP_CertGroupVerify, 3 certs with anchor --

static OSStatus cgv3Init(
	TestParams *testParams)
{
	CgvParams *cgvParams = (CgvParams *)malloc(sizeof(CgvParams));
	memset(cgvParams, 0, sizeof(CgvParams));
	cgvParams->tpHand    = tpStartup();
	cgvParams->clHand    = clStartup();
	cgvParams->cspHand   = cspStartup();
	cgvParams->certs[0]  = new BlobList();
	cgvParams->certs[0]->addFile(THAWTE_LEAF);
	cgvParams->certs[0]->addFile(THAWTE_CA);
	cgvParams->certs[0]->addFile(THAWTE_ROOT);
	cgvParams->anchors   = new BlobList();
	cgvParams->anchors->addFile(THAWTE_ROOT);
	testParams->testPriv = cgvParams;
	return noErr;
}

static OSStatus cgv3Run(
	TestParams *testParams,
	unsigned loopNum,
	CFAbsoluteTime *timeSpent)
{
	CgvParams *cgvParams = (CgvParams *)testParams->testPriv;
	BlobList nullList;

	CertVerifyArgs vfyArgs;
	memset(&vfyArgs, 0, sizeof(vfyArgs));
	vfyArgs.version = CERT_VFY_ARGS_VERS;
	
	vfyArgs.tpHand = cgvParams->tpHand;
	vfyArgs.clHand = cgvParams->clHand;
	vfyArgs.cspHand = cgvParams->cspHand;
	vfyArgs.certs = cgvParams->certs[0];	/* that's three certs */
	vfyArgs.roots = cgvParams->anchors;
	vfyArgs.allowUnverified = CSSM_TRUE;
	vfyArgs.vfyPolicy = CVP_Basic;
	vfyArgs.revokePolicy = CRP_None;
	vfyArgs.sslHost = certsToVerify[0].hostName;
	vfyArgs.revokePolicy = CRP_None;
	vfyArgs.quiet = CSSM_TRUE;
	
	CFAbsoluteTime startTime = CFAbsoluteTimeGetCurrent();
	int rtn = certVerify(&vfyArgs);
	*timeSpent = CFAbsoluteTimeGetCurrent() - startTime;
	if(rtn) {
		printf("***certVerify error\n");
		return (OSStatus)rtn;
	} 
	return noErr;
}

static OSStatus cgv3Cleanup(
	TestParams *testParams)
{
	CgvParams *cgvParams = (CgvParams *)testParams->testPriv;
	CSSM_ModuleDetach(cgvParams->cspHand);
	CSSM_ModuleDetach(cgvParams->tpHand);
	CSSM_ModuleDetach(cgvParams->clHand);
	delete cgvParams->certs[0];
	delete(cgvParams->anchors);
	free(cgvParams);
	return noErr;
}

#pragma mark ---- Static array of all tests ----

static TestDefs testDefs[] = 
{
	{ 	"Keychain open",
		100,
		kcOpenInit,
		kcOpenRun,
		kcOpenCleanup,
		'o',
	},
	{ 	"Keychain cert search",
		100,
		kcSearchInit,
		kcSearchRun,
		kcSearchCleanup,
		's',
	},
	{ 	"SecTrustEvaluate",
		100,
		secTrustInit,
		secTrustRun,
		secTrustCleanup,
		'e',
	},
	{ 	"TP CertGroupVerify, system anchors",
		100,
		cgvInit,
		cgvRun,
		cgvCleanup,
		'v',
	},
	{ 	"TP CertGroupVerify, explicit anchor",
		100,
		cgvAnchorInit,
		cgvAnchorRun,
		cgvCleanup,
		'V',
	},
	{ 	"TP CertGroupVerify, 3 certs with anchor",
		100,
		cgv3Init,
		cgv3Run,
		cgv3Cleanup,
		'3',
	},
	{ 	"SecKeychainCopySearchList",
		100,
		kcCSLInit,
		kcCSLRun,
		kcCSLCleanup,
		'k',
	},
};

#define NUM_TESTS	(sizeof(testDefs) / sizeof(testDefs[0]))

int main(int argc, char **argv)
{
	TestParams 	testParams;
	TestDefs	*testDef;
	OSStatus 	ortn;
	int 		arg;
	char		*argp;
	unsigned	cmdLoops = 0;		// can be specified in cmd line
									// if not, use TestDefs.loops
	char		testSpec = '\0';	// allows specification of one test
									// otherwise run all
									
	for(arg=1; arg<argc; arg++) {
		argp = argv[arg];
		switch(argp[0]) {
			case 't':
				testSpec = argp[2];
				break;
			case 'l':
				cmdLoops = atoi(&argp[2]);
				break;
			default:
				usage(argv);
		}
	}

	for(unsigned testNum=0; testNum<NUM_TESTS; testNum++) {
		testDef = &testDefs[testNum];
		unsigned loopCount;
		
		if(testSpec && (testDef->testSpec != testSpec)) {
			continue;
		}
		printf("%s:\n", testDef->testName);
		ortn = testDef->init(&testParams);
		if(ortn) {
			exit(1);
		}
		if(cmdLoops) {
			/* user specified */
			loopCount = cmdLoops;
		}	
		else {
			/* default */
			loopCount = testDef->loops;
		}
		CFAbsoluteTime totalTime = 0;
		CFAbsoluteTime thisTime;
		for(unsigned loop=0; loop<loopCount; loop++) {
			ortn = testDef->run(&testParams, loop, &thisTime);
			if(ortn) {
				exit(1);
			}
			totalTime += thisTime;
		}
		testDef->cleanup(&testParams);
		printf("   %3.2f ms per op\n", (totalTime / loopCount) * 1000.0);
	}
	return 0;
}
