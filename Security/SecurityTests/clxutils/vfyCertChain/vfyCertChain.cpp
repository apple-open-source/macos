/*
 * Gather up user-specified raw cert files, attempt to verify as a
 * cert chain
 */
 
#include <Security/cssm.h>
#include <Security/oidsalg.h>
#include <Security/cssmapple.h>
#include <Security/Security.h>
#include <Security/SecTrustPriv.h>
#include <Security/SecPolicyPriv.h>
#include <security_cdsa_utils/cuFileIo.h>
#include <utilLib/common.h>
#include <utilLib/cspwrap.h>	
#include <clAppUtils/clutils.h>
#include <clAppUtils/tpUtils.h>
#include <clAppUtils/sslAppUtils.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

static void usage(char **argv)
{
	printf("Usage: %s [options] certFile ..., leaf first\n", argv[0]);
	printf("Options:\n");
	printf("   -s       SSL policy (default is basic)\n");
	printf("   -e       allow expired cert\n");
	printf("   -E       allow expired root\n");
	printf("   -S serverName\n"); 
	printf("   -t timeBaseString (default = now)\n"); 
	printf("   -T       use SecTrustEvaluate\n");
	printf("   -v       verbose\n");
	exit(1);
}

static void printResult(
	CSSM_RETURN crtn)
{
	switch(crtn) {
		case CSSM_OK:
			printf("   ...successful verification\n");
			break;
		case CSSMERR_TP_INVALID_CERTIFICATE:
			printf("   ...invalid leaf cert\n");
			break;
		case CSSMERR_TP_INVALID_ANCHOR_CERT:
			printf("   ...cert chain valid (unknown root)\n");
			break;
		case CSSMERR_TP_NOT_TRUSTED:
			printf("   ...no root cert found\n");
			break;
		case CSSMERR_TP_VERIFICATION_FAILURE:
			printf("   ...bad root cert\n");
			break;
		case CSSMERR_TP_VERIFY_ACTION_FAILED:
			printf("   ...policy verification failed\n");
			break;
		case CSSMERR_TP_CERT_EXPIRED:
			printf("   ...expired cert in chain\n");
			break;
		case CSSMERR_TP_CERT_NOT_VALID_YET:
			printf("   ...not-yet-valid cert in chain\n");
			break;
		default:
			printError("tpCertGroupVerify", crtn);
			break;
	}
	
}
int main(int argc, char **argv)
{
	CSSM_CL_HANDLE		clHand;			// CL handle
	CSSM_TP_HANDLE		tpHand;			// TP handle
	CSSM_CSP_HANDLE		cspHand = 0;	// CSP handle
	CSSM_DATA_PTR		rawCerts = NULL;	
	unsigned			numCerts;		// num certs in *rawCerts
	unsigned			i;
	CSSM_CERTGROUP 		cgrp;
	const CSSM_OID		*policyId = &CSSMOID_APPLE_X509_BASIC;
	uint32				evidenceSize = 0;
	CSSM_TP_VERIFY_CONTEXT_RESULT	vfyResult;
	CSSM_CERTGROUP_PTR 	outGrp = NULL;
	int					fileArg;
	CSSM_RETURN			crtn;
	CSSM_BOOL			allowExpiredCert = CSSM_FALSE;
	bool				allowExpiredRoot = false;
	bool				useSecTrust = false;
	int					arg;
	CSSM_APPLE_TP_SSL_OPTIONS	sslOpts;
	CSSM_APPLE_TP_ACTION_DATA   tpAction;
	char				*serverName = NULL;
	bool				verbose = false;
	unsigned			numEvidences = 0;
	CSSM_DATA			fieldOpts;
	CSSM_DATA_PTR		fieldOptsPtr = NULL;
	CSSM_DATA			actionData;
	CSSM_DATA_PTR		actionDataPtr = NULL;
	char				*cssmTimeStr = NULL;
	SecTrustRef			theTrust = NULL;
	
	if(argc < 2) {
		usage(argv);
	}
	for(arg=1; arg<argc; arg++) {
		if(argv[arg][0] != '-') {
			fileArg = arg;
			break;
		}
		switch(argv[arg][1]) {
			case 's':
				policyId = &CSSMOID_APPLE_TP_SSL;
				break;
			case 'e':
				allowExpiredCert = true;
				break;
			case 'E':
				allowExpiredRoot = true;
				break;
			case 'v':
				verbose = true;
				break;
			case 'S':
				if(arg == (argc - 1)) {
					usage(argv);
				}
				serverName = argv[++arg];
				break;
			case 't':
				if(arg == (argc - 1)) {
					usage(argv);
				}
				cssmTimeStr = argv[++arg];
				break;
			case 'T':
				useSecTrust = true;
				break;
			default:
				usage(argv);
		}
	}

	/* common setup for TP and SecTrust */
	if(policyId == &CSSMOID_APPLE_TP_SSL) {
		sslOpts.Version = CSSM_APPLE_TP_SSL_OPTS_VERSION;
		sslOpts.ServerName = serverName;
		if(serverName) {
			sslOpts.ServerNameLen = strlen(serverName) + 1;
		}
		else {
			sslOpts.ServerNameLen = 0;
		}
		fieldOpts.Data = (uint8 *)&sslOpts;
		fieldOpts.Length = sizeof(sslOpts);
		fieldOptsPtr = &fieldOpts;
	}
	else if(serverName) {
		printf("***Server name option only valid for SSL policy.\n");
		usage(argv);
	}

	if(allowExpiredCert || allowExpiredRoot) {
		tpAction.Version = CSSM_APPLE_TP_ACTION_VERSION;
		tpAction.ActionFlags = 0;
		if(allowExpiredCert) {
			tpAction.ActionFlags |= CSSM_TP_ACTION_ALLOW_EXPIRED;
		}
		if(allowExpiredRoot) {
			tpAction.ActionFlags |= CSSM_TP_ACTION_ALLOW_EXPIRED_ROOT;
		}
		actionData.Data = (uint8 *)&tpAction;
		actionData.Length = sizeof(tpAction);
		actionDataPtr = &actionData;
	}
	/* else actionDataPtr NULL */
	
	
	numCerts = argc - fileArg;
	if(numCerts == 0) {
		usage(argv);
	}
	
	rawCerts = (CSSM_DATA_PTR)CSSM_MALLOC(numCerts * sizeof(CSSM_DATA));
	if(rawCerts == NULL) {
		printf("malloc error\n");
		goto abort;
	}

	/* gather cert data */
	for(i=0; i<numCerts; i++) {
		CSSM_DATA_PTR c = &rawCerts[i];
		unsigned len;
		if(readFile(argv[fileArg], &c->Data, &len)) {
			printf("Error reading %s=n", argv[fileArg]);
			exit(1);
		}
		c->Length = len;
		fileArg++;
	}
	
	if(useSecTrust) {
		SecPolicyRef				policy = NULL;
		SecPolicySearchRef			policySearch = NULL;
		SecCertificateRef 			cert;			// only lives in CFArrayRefs
		SecTrustResultType			secTrustResult;
		OSStatus					ortn;
		const char					*evalResStr = NULL;
		CSSM_TP_APPLE_EVIDENCE_INFO	*evidence = NULL;
		
		/* convert raw certs to a CFArray of SecCertificateRefs */
		CFMutableArrayRef certGroup = CFArrayCreateMutable(NULL, numCerts, 
			&kCFTypeArrayCallBacks);
		for(i=0; i<numCerts; i++) {
		ortn = SecCertificateCreateFromData(&rawCerts[i], CSSM_CERT_X_509v3,
					CSSM_CERT_ENCODING_DER, &cert);
			if(cert == NULL) {
				printf("SecCertificateCreateFromData returned %s\n", 
					sslGetSSLErrString(ortn));
				exit(1);
			}
			CFArrayAppendValue(certGroup, cert);
		}
		
		/* get a SecPolicySearchRef */
		ortn = SecPolicySearchCreate(CSSM_CERT_X_509v3,
			policyId, NULL,	&policySearch);
		if(ortn) {
			printf("SecPolicySearchCreate returned %s\n", 
				sslGetSSLErrString(ortn));
			exit(1);
		}
		ortn = SecPolicySearchCopyNext(policySearch, &policy);
		if(ortn) {
			printf("SecPolicySearchCopyNext returned %s\n", 
				sslGetSSLErrString(ortn));
			exit(1);
		}
		
		/* options only for SSL */
		if(fieldOptsPtr != NULL) {
			ortn = SecPolicySetValue(policy, fieldOptsPtr);
			if(ortn) {
				printf("SecPolicySetValue returned %s\n", 
					sslGetSSLErrString(ortn));
				exit(1);
			}
		}
		
		/* now a SecTrustRef */
		ortn = SecTrustCreateWithCertificates(certGroup, policy, &theTrust);
		if(ortn) {
			printf("SecTrustCreateWithCertificates returned %s\n", 
				sslGetSSLErrString(ortn));
			exit(1);
		}

		if(actionDataPtr) {
			CFDataRef actionData = 
				CFDataCreate(NULL, actionDataPtr->Data, actionDataPtr->Length);
			
			ortn = SecTrustSetParameters(theTrust, CSSM_TP_ACTION_DEFAULT,
				actionData);
			if(ortn) {
				printf("SecTrustSetParameters returned %s\n", sslGetSSLErrString(ortn));
				exit(1);
			}
			CFRelease(actionData);
		}
		
		/*
		 * Here we go; hand it over to SecTrust/TP. 
		 */
		ortn = SecTrustEvaluate(theTrust, &secTrustResult);
		if(ortn) {
			printf("SecTrustEvaluate returned %s\n", sslGetSSLErrString(ortn));
			exit(1);
		}
		crtn = CSSM_OK;
		switch(secTrustResult) {
			case kSecTrustResultInvalid:
				/* should never happen */
				evalResStr = "kSecTrustResultInvalid";
				break;
			case kSecTrustResultProceed:
				/* cert chain valid AND user explicitly trusts this */
				evalResStr = "kSecTrustResultProceed";
				break;
			case kSecTrustResultConfirm:
				/*
				 * Cert chain may well have verified OK, but user has flagged
				 * one of these certs as untrustable.
				 */
				evalResStr = "kSecTrustResultConfirm";
				break;
			case kSecTrustResultDeny:
				/*
				 * Cert chain may well have verified OK, but user has flagged
				 * one of these certs as untrustable.
				 */
				evalResStr = "kSecTrustResultDeny";
				break;
			case kSecTrustResultUnspecified:
				/* cert chain valid, no special UserTrust assignments */
				evalResStr = "kSecTrustResultUnspecified";
				break;
			case kSecTrustResultRecoverableTrustFailure:
				/* ? */
				evalResStr = "kSecTrustResultRecoverableTrustFailure";
				break;
			case kSecTrustResultFatalTrustFailure:
				/* ? */
				evalResStr = "kSecTrustResultFatalTrustFailure";
				break;
			case kSecTrustResultOtherError:
				/* ? */
				evalResStr = "kSecTrustResultOtherError";
				break;
			default:
				break;
		}
		printf("...SecTrustEvaluate result : ");
		if(evalResStr != NULL) {
			printf("%s\n", evalResStr);
		}
		else {
			printf("UNKNOWN (%d)\n", (int)secTrustResult);
		}
		/* get low-level TP return code */
		OSStatus ocrtn;
		ortn = SecTrustGetCssmResultCode(theTrust, &ocrtn);
		if(ortn) {
			printf("SecTrustGetCssmResultCode returned %s\n", sslGetSSLErrString(ortn));
			/*...keep going */
		}
		else {
			printResult(ocrtn);
		}
		CFArrayRef dummy;
		ortn = SecTrustGetResult(theTrust, &secTrustResult, &dummy,
			&evidence);
		if(ortn) {
			printf("SecTrustGetResult returned %s\n", sslGetSSLErrString(ortn));
			/*...keep going */
		}
		else {
			unsigned numEvidences = CFArrayGetCount(dummy);
			if(numEvidences && verbose) {
				printCertInfo(numEvidences, evidence);
			}
		}
	}
	else {
		/* connect to CL, TP, and CSP */
		cspHand = cspStartup();
		if(cspHand == 0) {
			exit(1);
		}
		/* subsequent errors to abort: */
		clHand = clStartup();
		if(clHand == 0) {
			goto abort;
		}
		tpHand = tpStartup();
		if(tpHand == 0) {
			goto abort;
		}
	
		/*
	  	 * Cook up a cert group - TP wants leaf first 
		 */
		memset(&cgrp, 0, sizeof(CSSM_CERTGROUP));
		cgrp.NumCerts = numCerts;
		cgrp.CertGroupType = CSSM_CERTGROUP_DATA;
		cgrp.CertType = CSSM_CERT_X_509v3;
		cgrp.CertEncoding = CSSM_CERT_ENCODING_DER; 
		cgrp.GroupList.CertList = rawCerts;
	
		crtn = tpCertGroupVerify(
			tpHand,
			clHand,
			cspHand,
			NULL,						// dbListPtr
			policyId,
			fieldOptsPtr,
			actionDataPtr,
			NULL,						// policyOpts
			&cgrp,
			NULL,						// anchorCerts
			0,							// NumAnchorCerts
			CSSM_TP_STOP_ON_POLICY, 
			cssmTimeStr,
			&vfyResult);				// verify result 
		printResult(crtn);
		if((vfyResult.Evidence != NULL) && (vfyResult.Evidence->Evidence != NULL)) {
			numEvidences = vfyResult.NumberOfEvidences;
			if(numEvidences == 3) {
				/* i.e., normal case */
				outGrp = (CSSM_CERTGROUP_PTR)vfyResult.Evidence[1].Evidence;
				evidenceSize = outGrp->NumCerts;
			}
			else {
				printf("***Expected numEvidences 3, got %u\n", numEvidences);
				evidenceSize = 0;
			}
		}
		printf("   num input certs %d; evidenceSize %u\n",
			numCerts, (unsigned)evidenceSize);
		if((numEvidences > 0) && verbose) {
			dumpVfyResult(&vfyResult);
		}
		freeVfyResult(&vfyResult);
	}
	
abort:
	if(rawCerts != NULL) {
		/* mallocd by readFile() */
		for(i=0; i<numCerts; i++) {
			free(rawCerts[i].Data);
		}
		CSSM_FREE(rawCerts);
	}
	if(theTrust != NULL) {
		CFRelease(theTrust);
	}
	return 0;
}
