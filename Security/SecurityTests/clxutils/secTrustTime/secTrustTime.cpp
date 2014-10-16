/*
 * secTrustTime.cpp - measure performance of SecTrust and TP cert verify
 */
 
#include <stdlib.h>
#include <strings.h>
#include <stdio.h>
#include <unistd.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include <Security/TrustSettingsSchema.h>
#include <security_cdsa_utils/cuFileIo.h>
#include <utilLib/common.h>
#include <clAppUtils/clutils.h>
#include <clAppUtils/tpUtils.h>

#define LOOPS_DEF			100

const char *certFiles[] = {
	"keybank_v3.100.cer", "keybank_v3.101.cer", "keybank_v3.102.cer"
};

#define NUM_CERTS (sizeof(certFiles) / sizeof(certFiles[0]))

static void usage(char **argv)
{
	printf("usage: %s [options]\n", argv[0]);
	printf("Options:\n");
	printf("  -l loops        -- loops; default %d; 0=forever\n", LOOPS_DEF);
	printf("  -k              -- open and hold keychains\n");
	printf("  -t              -- TP, not SecTrust\n");
	printf("  -T              -- TP, no Trust Settings\n");
	printf("  -n              -- don't include root in cert chain\n");
	printf("  -K              -- set empty KC list\n");
	/* etc. */
	exit(1);
}

static SecCertificateRef readCertFile(
	const char *fileName)
{
	unsigned char *cp = NULL;
	unsigned len = 0;
	CSSM_DATA certData;
	OSStatus ortn;

	if(readFile(fileName, &cp, &len)) {
		printf("***Error reading file %s\n", fileName);
		return NULL;
	}
	certData.Length = len;
	certData.Data = cp;
	SecCertificateRef certRef;

	ortn = SecCertificateCreateFromData(&certData, 
			CSSM_CERT_X_509v3, CSSM_CERT_ENCODING_DER, &certRef);
	if(ortn) {
		cssmPerror("SecCertificateCreateFromData", ortn);
		return NULL;
	}
	free(cp);
	return certRef;
}

/* perfrom one cert chain evaluation using SecTrust */
static OSStatus doEval(
	CFArrayRef certArray,
	SecPolicyRef policyRef,
	CFArrayRef kcList)
{
	OSStatus ortn;
	SecTrustRef trustRef;

	ortn = SecTrustCreateWithCertificates(certArray, policyRef, &trustRef);
	if(ortn) {
		cssmPerror("SecTrustCreateWithCertificates", ortn);
		return ortn;
	}
	if(kcList) {
		ortn = SecTrustSetKeychains(trustRef, kcList);
		if(ortn) {
			cssmPerror("SecTrustCreateWithCertificates", ortn);
			return ortn;
		}
	}
	SecTrustResultType secTrustResult;
	ortn = SecTrustEvaluate(trustRef, &secTrustResult);
	if(ortn) {
		cssmPerror("SecTrustEvaluate", ortn);
		return ortn;
	}
	switch(secTrustResult) {
		case kSecTrustResultProceed:
		case kSecTrustResultUnspecified:
			break;
		default:
			printf("***Unexpected SecTrustResultType (%d)\n", (int)secTrustResult);
			ortn = -1;
	}
	CFRelease(trustRef);
	return ortn;
}

/* cached CSSM anchors - simulate old SecTrustGetCSSMAnchorCertificates() */ 
static CFArrayRef cachedRootArray = NULL;
static CSSM_DATA *cachedAnchors = NULL;
static unsigned cachedNumAnchors = 0;

static OSStatus getAnchors(
	CSSM_DATA **anchors,	/* RETURNED */
	unsigned *numAnchors)	/* RETURNED */
{
	if(cachedRootArray == NULL) {
		/* fetch, once */
		OSStatus ortn = getSystemAnchors(&cachedRootArray, &cachedAnchors, 
			&cachedNumAnchors);
		if(ortn) {
			return ortn;
		}
	}
	*anchors = cachedAnchors;
	*numAnchors = cachedNumAnchors;
	return noErr;
}

/* perfrom one cert chain evaluation using CSSM_TP_CertGroupVerify */
static CSSM_RETURN doTpEval(
	CSSM_TP_HANDLE tpHand,
	CSSM_CL_HANDLE clHand,
	CSSM_CSP_HANDLE cspHand,
	CSSM_DATA_PTR certs,
	uint32 numCerts,
	bool useTrustSettings)
{
	CSSM_FIELD policyId;

	policyId.FieldOid = CSSMOID_APPLE_X509_BASIC;
	policyId.FieldValue.Data = NULL;
	policyId.FieldValue.Length = 0;

	CSSM_TP_CALLERAUTH_CONTEXT		authCtx;
	memset(&authCtx, 0, sizeof(CSSM_TP_CALLERAUTH_CONTEXT));
	authCtx.Policy.NumberOfPolicyIds = 1;
	authCtx.Policy.PolicyIds = &policyId;	
	authCtx.VerificationAbortOn = CSSM_TP_STOP_ON_POLICY;
	if(!useTrustSettings) {
		OSStatus ortn = getAnchors(&authCtx.AnchorCerts, 
			&authCtx.NumberOfAnchorCerts);
		if(ortn) {
			return ortn;
		}
	}
	CSSM_APPLE_TP_ACTION_DATA tpAction;
	tpAction.Version = CSSM_APPLE_TP_ACTION_VERSION;
	if(useTrustSettings) {
		tpAction.ActionFlags = CSSM_TP_ACTION_TRUST_SETTINGS;
	}
	else {
		tpAction.ActionFlags = 0;
	}

	CSSM_TP_VERIFY_CONTEXT vfyCtx;
	memset(&vfyCtx, 0, sizeof(CSSM_TP_VERIFY_CONTEXT));
	vfyCtx.ActionData.Data   = (uint8 *)&tpAction;
	vfyCtx.ActionData.Length = sizeof(tpAction);
	vfyCtx.Action = CSSM_TP_ACTION_DEFAULT;
	vfyCtx.Cred = &authCtx;

	CSSM_CERTGROUP cssmCerts;
	cssmCerts.CertType = CSSM_CERT_X_509v3;
	cssmCerts.CertEncoding = CSSM_CERT_ENCODING_DER;
	cssmCerts.NumCerts = numCerts;
	cssmCerts.GroupList.CertList = certs;
	cssmCerts.CertGroupType = CSSM_CERTGROUP_DATA;

	CSSM_RETURN crtn = CSSM_TP_CertGroupVerify(tpHand, clHand, cspHand,
		&cssmCerts,
		&vfyCtx,
		NULL);		/* no results */
	if(crtn) {
		cssmPerror("CSSM_TP_CertGroupVerify", crtn);
	}
	return crtn;
}
	
int main(int argc, char **argv)
{
	unsigned dex;
	CSSM_RETURN crtn;

	/* common SecTrust args */
	CFMutableArrayRef 	kcList = NULL;
	CFMutableArrayRef 	certArray = NULL;
	SecPolicyRef      	policyRef = NULL;
	unsigned			numCerts = NUM_CERTS;
	CFArrayRef			emptyKCList = NULL;

	/* common TP args */
	CSSM_TP_HANDLE tpHand;
	CSSM_CL_HANDLE clHand;
	CSSM_CSP_HANDLE cspHand;
	CSSM_DATA cssmCerts[NUM_CERTS];

	/* user-spec'd variables */
	unsigned loops = LOOPS_DEF;
	bool holdKeychains = false;		/* hold references to KC list during operation */
	bool useTp = false;				/* TP, not SecTrust */
	bool useTrustSettings = true;	/* TP w/TrustSettings; false = old school TP way */
	bool noRoot = false;			/* don't include root in chain to be verified */
	bool emptyList = false;			/* SecTrust only: specify empty KC list */

	extern char *optarg;
	int arg;
	while ((arg = getopt(argc, argv, "l:ktTnKh")) != -1) {
		switch (arg) {
			case 'l':
				loops = atoi(optarg);
				break;
			case 'k':
				holdKeychains = true;
				break;
			case 't':
				useTp = true;
				break;
			case 'T':
				useTp = true;
				useTrustSettings = false;
				break;
			case 'n':
				numCerts--;
				noRoot = true;
				break;
			case 'K':
				emptyList = true;
				emptyKCList = CFArrayCreate(NULL, NULL, 0, &kCFTypeArrayCallBacks);
				break;
			case 'h':
				usage(argv);
		}
	}
	if(optind != argc) {
		usage(argv);
	}
	
	/* gather certs to verify */
	certArray = CFArrayCreateMutable(NULL, 3, &kCFTypeArrayCallBacks);
	for(dex=0; dex<numCerts; dex++) {
		SecCertificateRef certRef = readCertFile(certFiles[dex]);
		if(certRef == NULL) {
			exit(1);
		}
		CFArrayInsertValueAtIndex(certArray, dex, certRef);
		CFRelease(certRef);
	}

	/* prepare for one method or another */
	if(useTp) {
		for(dex=0; dex<numCerts; dex++) {
			crtn = SecCertificateGetData(
				(SecCertificateRef)CFArrayGetValueAtIndex(certArray, dex),
					&cssmCerts[dex]);
			if(crtn) {
				cssmPerror("SecCertificateGetData", crtn);
				exit(1);
			}
		}
		tpHand  = tpStartup();
		clHand  = clStartup();
		cspHand = cspStartup();
	}
	else {
		/* cook up reusable policy object */
		SecPolicySearchRef	policySearch = NULL;
		OSStatus ortn = SecPolicySearchCreate(CSSM_CERT_X_509v3,
			&CSSMOID_APPLE_X509_BASIC,
			NULL,				// policy opts
			&policySearch);
		if(ortn) {
			cssmPerror("SecPolicySearchCreate", ortn);
			exit(1);
		}
		ortn = SecPolicySearchCopyNext(policySearch, &policyRef);
		if(ortn) {
			cssmPerror("SecPolicySearchCopyNext", ortn);
			exit(1);
		}
		CFRelease(policySearch);

		if(holdKeychains) {
			/* the standard keychains */
			ortn = SecKeychainCopySearchList((CFArrayRef *)&kcList);
			if(ortn) {
				cssmPerror("SecKeychainCopySearchList", ortn);
				exit(1);
			}

			/* plus the ones TrustSettings needs */
			SecKeychainRef rootKc;
			ortn = SecKeychainOpen(SYSTEM_ROOT_STORE_PATH, &rootKc);
			if(ortn) {
				cssmPerror("SecKeychainOpen", ortn);
				exit(1);
			}
			CFArrayAppendValue(kcList, rootKc);
			CFRelease(rootKc);
		}
	}

	CFAbsoluteTime startTimeFirst;
	CFAbsoluteTime endTimeFirst;
	CFAbsoluteTime startTimeMulti;
	CFAbsoluteTime endTimeMulti;

	/* print a banner describing current test parameters */
	printf("Starting test: mode = ");
	if(useTp) {
		if(useTrustSettings) {
			printf("TP w/TrustSettings");
		}
		else {
			printf("TP w/o TrustSettings");
		}
	}
	else {
		printf("SecTrust");
		if(holdKeychains) {
			printf("; hold KC refs");
		}
		if(emptyList) {
			printf("; empty KC list");
		}
	}
	if(noRoot) {
		printf("; no root in input certs\n");
	}
	else {
		printf("\n");
	}

	/* GO */
	startTimeFirst = CFAbsoluteTimeGetCurrent();
	if(useTp) {
		if(doTpEval(tpHand, clHand, cspHand, cssmCerts, numCerts,
				useTrustSettings)) {
			exit(1);
		}
		endTimeFirst = CFAbsoluteTimeGetCurrent();

		startTimeMulti = CFAbsoluteTimeGetCurrent();
		for(dex=0; dex<loops; dex++) {
			if(doTpEval(tpHand, clHand, cspHand, cssmCerts, numCerts,
					useTrustSettings)) {
				exit(1);
			}
		}
	}
	else {
		if(doEval(certArray, policyRef, emptyKCList)) {
			exit(1);
		}
		endTimeFirst = CFAbsoluteTimeGetCurrent();

		startTimeMulti = CFAbsoluteTimeGetCurrent();
		for(dex=0; dex<loops; dex++) {
			if(doEval(certArray, policyRef, emptyKCList)) {
				exit(1);
			}
		}
	}
	endTimeMulti = CFAbsoluteTimeGetCurrent();
	CFTimeInterval elapsed = endTimeMulti - startTimeMulti;

	printf("First eval = %4.1f ms\n", (endTimeFirst - startTimeFirst) * 1000.0);
	printf("Next evals = %4.2f ms/op (%f s total for %u loops)\n",
		elapsed * 1000.0 / loops, elapsed, loops);

	return 0;
}
