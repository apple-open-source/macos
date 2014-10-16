/*
 * secTrustEval.cpp
 *
 * doSet up SecTrust object, do a SecTrustEvaluate, release.
 */
#include "testParams.h"
#include <Security/cssm.h>
#include <utilLib/common.h>	
#include <utilLib/cspwrap.h>
#include <clAppUtils/clutils.h>
#include <clAppUtils/tpUtils.h>
#include <security_cdsa_utils/cuFileIo.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <Security/Security.h>

#define HOLD_SEARCH_LIST	0

/* for malloc debug */
#define DO_PAUSE			0

//static const char *CERT_FILE = "amazon_v3.100.cer";
static const char *CERT_FILE = "cduniverse_v3.100.cer";

/* common data, our known good cert, shared by all threads */
static unsigned char *certData = NULL;
static unsigned certLength = 0;

/* read in our known good cert file, just once */
int secTrustEvalInit(TestParams *testParams)
{
	if(certData != NULL) {
		return 0;
	}
	if(testParams->verbose) {
		printf("secTrusEval thread %d: reading cert file %s...\n", 
			testParams->threadNum, CERT_FILE);
	}
	if(readFile(CERT_FILE, &certData, &certLength)) {
		printf("Error reading %s; aborting\n", CERT_FILE);
		printf("***This test must be run from the clxutils/threadTest directory.\n");
		return 1;
	}
	return 0;
}


int secTrustEval(TestParams *testParams)
{
	unsigned			loopNum;
	SecCertificateRef 	certRef;
	const CSSM_DATA		cdata = {certLength, (uint8 *)certData};
	
	OSStatus ortn = SecCertificateCreateFromData(&cdata,
		CSSM_CERT_X_509v3,
		CSSM_CERT_ENCODING_DER, 
		&certRef);
	if(ortn) {
		cssmPerror("SecCertificateCreateFromData", ortn);
		return (int)ortn;
	}
	
	#if HOLD_SEARCH_LIST
	CFArrayRef sl;
	ortn = SecKeychainCopySearchList(&sl);
	if(ortn) {
		cssmPerror("SecPolicySearchCreate", ortn);
		return (int)ortn;
	}
	#endif
	
	for(loopNum=0; loopNum<testParams->numLoops; loopNum++) {
		if(testParams->verbose) {
			printf("secTrustEval loop %d\n", loopNum);
		}
		else if(!testParams->quiet) {
			printChar(testParams->progressChar);
		}
		
		/* from here on emulate exactly what SecureTransport does */
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
			cssmPerror("SecPolicySearchCreate", ortn);
			return (int)ortn;
		}
		
		ortn = SecPolicySearchCopyNext(policySearch, &policy);
		if(ortn) {
			cssmPerror("SecPolicySearchCopyNext", ortn);
			return (int)ortn;
		}
		CFRelease(policySearch);
		
		SecTrustRef secTrust;
		ortn = SecTrustCreateWithCertificates(certs, policy, &secTrust);
		if(ortn) {
			cssmPerror("SecTrustCreateWithCertificates", ortn);
			return (int)ortn;
		}
		/* no action data for now */
	
		SecTrustResultType secTrustResult;
		ortn = SecTrustEvaluate(secTrust, &secTrustResult);
		if(ortn) {
			cssmPerror("SecTrustEvaluate", ortn);
			return (int)ortn;
		}
		
		CFRelease(certs);
		CFRelease(secTrust);
		CFRelease(policy);

		#if	DO_PAUSE
		fpurge(stdin);
		printf("Hit CR to continue: ");
		getchar();
		#endif
	}	/* outer loop */
	#if HOLD_SEARCH_LIST
	CFRelease(sl);
	#endif
	return 0;
}
