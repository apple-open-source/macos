/*
 * userTrustTest.cpp - simple test of SecTrustSetUserTrustLegacy() and 
 *					   SecTrustGetUserTrust()
 */
 
#include <stdlib.h>
#include <strings.h>
#include <stdio.h>
#include <unistd.h>
#include <Security/Security.h>
#include <Security/SecTrustPriv.h>
#include <Security/SecPolicyPriv.h>
#include <clAppUtils/tpUtils.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>

#define IGNORE_EXISTING_STATE		0

static void usage(char **argv)
{
	printf("usage: %s [options] known_good_leaf_cert [ca_cert...]\n", argv[0]);
	printf("Options:\n");
	printf("  -q              -- quiet\n");
	exit(1);
}

static char *secTrustResultStr(
	SecTrustResultType result)
{
	static char unknownStr[100];

	switch(result) {
		case kSecTrustResultInvalid: return "kSecTrustResultInvalid";
		case kSecTrustResultProceed: return "kSecTrustResultProceed";
		case kSecTrustResultConfirm: return "kSecTrustResultConfirm";
		case kSecTrustResultDeny:    return "kSecTrustResultDeny";
		case kSecTrustResultUnspecified: return "kSecTrustResultUnspecified";
		case kSecTrustResultRecoverableTrustFailure: 
			return "kSecTrustResultRecoverableTrustFailure";
		case kSecTrustResultFatalTrustFailure: return "kSecTrustResultFatalTrustFailure";
		case kSecTrustResultOtherError: return "kSecTrustResultOtherError";
		default:
			sprintf(unknownStr, "UNKNOWN ResultType (%d)\n", 
				(int)result);
			return unknownStr;
	}
}

/* do a SecTrustEvaluate, ensure resultType is as specified */
static int doEval(
	CFArrayRef certs,
	SecPolicyRef policy,
	SecTrustResultType expectedResult,
	bool quiet)
{
	OSStatus ortn;
	SecTrustRef trustRef = NULL;
	SecTrustResultType result;
	int ourRtn = 0;

	ortn = SecTrustCreateWithCertificates(certs, policy, &trustRef);
	if(ortn) {
		cssmPerror("SecTrustCreateWithCertificates", ortn);
		return -1;
	}
	ortn = SecTrustEvaluate(trustRef, &result);
	if(ortn) {
		/* shouldn't fail no matter what resultType we expect */
		cssmPerror("SecTrustEvaluate", ortn);
		ourRtn = -1;
		goto errOut;
	}
	if(expectedResult == result) {
		if(!quiet) {
			printf("...got %s as expected\n", secTrustResultStr(result));
		}
	}
	else {
		printf("***Expected %s, got %s\n", secTrustResultStr(expectedResult),
			secTrustResultStr(result));
		ourRtn = -1;
	}
errOut:
	CFRelease(trustRef);
	return ourRtn;
}

/* Do a SecTrustGetUserTrust(), ensure result is as specified */
static int doGetUserTrust(
	SecCertificateRef certRef,
	SecPolicyRef policy,
	SecTrustResultType expectedResult)
{
	SecTrustResultType foundResult;
	OSStatus ortn = SecTrustGetUserTrust(certRef, policy, &foundResult);
	if(ortn) {
		cssmPerror("SecTrustGetUserTrust", ortn);
		return -1;
	}
	if(foundResult != expectedResult) {
		printf("***Expected current resultType %s; found %s\n",
			secTrustResultStr(expectedResult), secTrustResultStr(foundResult));
		return -1;
	}
	return 0;
}
	
/* Do SecTrustSetUserTrustLegacy() followed by SecTrustGetUserTrust() */
static int doSetVerifyUserTrust(
	SecCertificateRef certRef,
	SecPolicyRef policy,
	SecTrustResultType result)
{
	OSStatus ortn;
	ortn = SecTrustSetUserTrustLegacy(certRef, policy, result);
	if(ortn) {
		cssmPerror("SecTrustSetUserTrustLegacy", ortn);
		return -1;
	}
	return doGetUserTrust(certRef, policy, result);
}

static int doTest(
	CFArrayRef certArray,
	SecPolicyRef policy,
	bool quiet)
{
	int ourRtn = 0;
	SecCertificateRef leafCert = (SecCertificateRef)CFArrayGetValueAtIndex(
		certArray, 0);

	if(!quiet) {
		printf("Verifying cert is good as is...\n");
	}
	ourRtn = doEval(certArray, policy, kSecTrustResultUnspecified, quiet);
	if(ourRtn && !IGNORE_EXISTING_STATE) {
		return ourRtn;
	}

	if(!quiet) {
		printf("Verifying cert currently has kSecTrustResultUnspecified...\n");
	}
	if(doGetUserTrust(leafCert, policy, kSecTrustResultUnspecified)) {
		ourRtn = -1;
		/* but keep going */
	}

	if(!quiet) {
		printf("setting and verifying SecTrustResultDeny...\n");
	}
	if(doSetVerifyUserTrust(leafCert, policy, kSecTrustResultDeny)) {
		ourRtn = -1;
	}

	if(!quiet) {
		printf("Verify cert with SecTrustResultDeny...\n");
	}
	ourRtn = doEval(certArray, policy, kSecTrustResultDeny, quiet);
	if(ourRtn) {
		ourRtn = -1;
	}

	if(!quiet) {
		printf("setting and verifying kSecTrustResultConfirm...\n");
	}
	if(doSetVerifyUserTrust(leafCert, policy, kSecTrustResultConfirm)) {
		ourRtn = -1;
	}

	if(!quiet) {
		printf("Verify cert with kSecTrustResultConfirm...\n");
	}
	ourRtn = doEval(certArray, policy, kSecTrustResultConfirm, quiet);
	if(ourRtn) {
		ourRtn = -1;
	}

	if(!quiet) {
		printf("setting and verifying kSecTrustResultUnspecified...\n");
	}
	if(doSetVerifyUserTrust(leafCert, policy, kSecTrustResultUnspecified)) {
		ourRtn = -1;
	}

	if(!quiet) {
		printf("Verify cert with kSecTrustResultUnspecified...\n");
	}
	ourRtn = doEval(certArray, policy, kSecTrustResultUnspecified, quiet);
	
	if(!quiet) {
		printf("Verify SecTrustSetUserTrust(kSecTrustResultConfirm) fails...\n");
	}
	/* verify Radar 4642125 - this should fail, not crash */
	OSStatus ortn = SecTrustSetUserTrust(leafCert, policy, kSecTrustResultConfirm);
	if(ortn != unimpErr) {
		printf("***SecTrustSetUserTrust returned %ld; expected %ld (unimpErr)\n",
			(long)ortn, (long)unimpErr);
		ourRtn = -1;
	}
	return ourRtn;
}

int main(int argc, char **argv)
{
	bool quiet = false;
	
	int arg;
	while ((arg = getopt(argc, argv, "qh")) != -1) {
		switch (arg) {
			case 'q':
				quiet = true;
				break;
			case 'h':
				usage(argv);
		}
	}
	
	unsigned numCerts = argc - optind;
	if(numCerts == 0) {
		usage(argv);
	}
	CFMutableArrayRef certArray = CFArrayCreateMutable(NULL, 0, 
		&kCFTypeArrayCallBacks);
	for(int dex=optind; dex<argc; dex++) {
		SecCertificateRef certRef = certFromFile(argv[dex]);
		if(certRef == NULL) {
			exit(1);
		}
		CFArrayAppendValue(certArray, certRef);
		CFRelease(certRef);
	}
	
	OSStatus ortn;
	SecPolicyRef policyRef = NULL;
	ortn = SecPolicyCopy(CSSM_CERT_X_509v3, &CSSMOID_APPLE_TP_SSL, &policyRef);
	if(ortn) {
		cssmPerror("SecPolicyCopy", ortn);
		exit(1);
	}
	
	int ourRtn = doTest(certArray, policyRef, quiet);
	CFRelease(policyRef);
	CFRelease(certArray);
	return ourRtn;
}
