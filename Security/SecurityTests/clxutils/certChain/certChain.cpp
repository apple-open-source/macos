/*
 * Given a cert, produce a complete ordered cert chain back to a root.
 * Intermediate certs can be in any user keychain.
 */
#include <stdlib.h>
#include <stdio.h>
#include <security_cdsa_utils/cuFileIo.h>		/* private */
#include <security_cdsa_utils/cuPrintCert.h>	/* private */
#include <Security/Security.h>
#include <Security/SecTrustPriv.h>  /* private */

static void usage(char **argv)
{
	printf("Usage:\n");
	printf("  %s certFileName [d(isable intermediates) [f filebase] [n(o cert dump)]\n", argv[0]);
	exit(1);
}

int main(int argc, char **argv)
{
	unsigned char				*certData = NULL;   // subject cert, raw data
	unsigned					certDataLen = 0;
	OSStatus					ortn;
	SecTrustRef					secTrust = NULL;
	CFMutableArrayRef			subjCerts = NULL;		
	SecPolicyRef				policy = NULL;
	SecPolicySearchRef			policySearch = NULL;
	SecTrustResultType			secTrustResult;
	CSSM_RETURN					crtn = CSSM_OK;
	CSSM_TP_APPLE_EVIDENCE_INFO *dummyEv;			// not used
	CFArrayRef					certChain = NULL;   // constructed chain
	CFIndex						numCerts;
	bool						disableLocalIntermediates = false;
	char						*fileBase = NULL;
	bool						enableCertDump = true;
	
	if(argc < 2) {
		usage(argv);
	}
	if(readFile(argv[1], &certData, &certDataLen)) {
		printf("***Error reading cert from %s. Aborting.\n", argv[1]);
		exit(1);
	}
	for(int arg=2; arg<argc; arg++) {
		char *argp = argv[arg];
		switch(argp[0]) {
			case 'd':
				disableLocalIntermediates = true;
				break;
			case 'n':
				enableCertDump = false;
				break;
			case 'f':
				arg++;
				if(arg == argc) {
					usage(argv);
				}
				fileBase = argv[arg];
				break;
			default:
				usage(argv);
		}
	}
	
	/* SecCertificateRef form of subject cert */
	SecCertificateRef certRef = NULL;
	CSSM_DATA cdata = {(uint32)certDataLen, (uint8 *)certData};
	ortn = SecCertificateCreateFromData(&cdata,	
		CSSM_CERT_X_509v3,
		CSSM_CERT_ENCODING_DER, 
		&certRef);
	if(ortn) {
		cssmPerror("SecCertificateCreateFromData", ortn);
		goto errOut;
	}
	
	/* make a one-element array */
	subjCerts = CFArrayCreateMutable(NULL, 1, &kCFTypeArrayCallBacks);
	CFArraySetValueAtIndex(subjCerts, 0, certRef);
			
	/* the array owns the subject cert ref now */
	CFRelease(certRef);
	
	/* Get a SecPolicyRef for generic X509 cert chain verification */
	ortn = SecPolicySearchCreate(CSSM_CERT_X_509v3,
		&CSSMOID_APPLE_X509_BASIC,
		NULL,				// value
		&policySearch);
	if(ortn) {
		cssmPerror("SecPolicySearchCreate", ortn);
		goto errOut;
	}
	ortn = SecPolicySearchCopyNext(policySearch, &policy);
	if(ortn) {
		cssmPerror("SecPolicySearchCopyNext", ortn);
		goto errOut;
	}

	/* build a SecTrustRef for specified policy and certs */
	ortn = SecTrustCreateWithCertificates(subjCerts,
		policy, &secTrust);
	if(ortn) {
		cssmPerror("SecTrustCreateWithCertificates", ortn);
		goto errOut;
	}
	
	if(disableLocalIntermediates) {
		/*
		 * Avoid searching user keychains for intermediate certs 
		 * by specifying an empty array of keychains
		 */
		CFMutableArrayRef kcList;
		kcList = CFArrayCreateMutable(NULL, 0, NULL);
		if(kcList == NULL) {
			printf("***CFArrayCreateMutable error\n");
			ortn = -1;
			goto errOut;
		}
		ortn = SecTrustSetKeychains(secTrust, kcList);
		if(ortn) {
			cssmPerror("SecTrustSetKeychains", ortn);
			goto errOut;
		}
		CFRelease(kcList);
	}
	
	/* evaluate: GO */
	ortn = SecTrustEvaluate(secTrust, &secTrustResult);
	if(ortn) {
		cssmPerror("SecTrustEvaluate", ortn);
		goto errOut;
	}
	switch(secTrustResult) {
		case kSecTrustResultUnspecified:
			/* cert chain valid, no special UserTrust assignments */
		case kSecTrustResultProceed:
			/* cert chain valid AND user explicitly trusts this */
			break;
		case kSecTrustResultDeny:
		case kSecTrustResultConfirm:
			/*
			 * Cert chain may well have verified OK, but user has flagged
			 * one of these certs as untrustable.
			 */
			printf("***User specified that a cert in this chain is untrusted.\n");
			goto errOut;
			
		default:
		{
			/* private SPI to get low-level CSSM error */
			OSStatus osCrtn;
			ortn = SecTrustGetCssmResultCode(secTrust, (OSStatus *)&crtn);
			if(ortn) {
				cssmPerror("SecTrustEvaluate", ortn);
				goto errOut;
			}
		}
	}
	if(crtn) {	
		/* get some detailed error info */
		switch(crtn) {
			case CSSMERR_TP_INVALID_ANCHOR_CERT: 
				printf("***Verified to unknown anchor cert\n");
				break;
			case CSSMERR_TP_NOT_TRUSTED:
				printf("***Can not verify to a root cert \n");
				break;
			case CSSMERR_TP_CERT_EXPIRED:
				printf("***A cert in this chain has expired\n");
				break;
			case CSSMERR_TP_CERT_NOT_VALID_YET:
				printf("***A cert in this chain is not yet valid\n");
				break;
			default:
				printf("Other error from SecTrustEvaluate\n");
				cssmPerror("SecTrustEvaluate", crtn);
				break;
		}
	} 	/* SecTrustEvaluate error */

	/* get resulting constructed cert chain */
	ortn = SecTrustGetResult(secTrust, &secTrustResult, &certChain, &dummyEv);
	if(ortn) {
		cssmPerror("SecTrustEvaluate", ortn);
		goto errOut;
	}
	
	/* display the results */
	numCerts = CFArrayGetCount(certChain);
	printf("Number of certs in constructed cert chain = %d\n", (int)numCerts);
	if(enableCertDump) {
		for(unsigned i=0; i<numCerts; i++) {
			CSSM_DATA cd;
			certRef = (SecCertificateRef)CFArrayGetValueAtIndex(certChain, i);
			ortn = SecCertificateGetData(certRef, &cd);
			if(ortn) {
				printf("***SecCertificateGetData returned %d\n", (int)ortn);
				continue;
			}
			printf("\n================== Cert %d ===================\n\n", i);
			printCert(cd.Data, cd.Length, CSSM_FALSE);
			printf("\n=============== End of Cert %d ===============\n", i);
			
			if((fileBase != NULL) & (i > 0)) {
				char fname[200];
				sprintf(fname, "%s_%u", fileBase, i);
				if(writeFile(fname, cd.Data, cd.Length)) {
					printf("***Error writing to %s\n", fname);
				}	
				else {
					printf("...write %lu bytes to %s\n", cd.Length, fname);
				}
			}
		}
	}
errOut:
	if(certData) {
		/* mallocds by readFile() */
		free(certData);
	}
	if(secTrust) {
		CFRelease(secTrust);
	}
	if(subjCerts) {
		CFRelease(subjCerts);
	}
	if(policy) {
		CFRelease(policy);
	}
	if(policySearch) {
		CFRelease(policySearch);
	}
	return (int)ortn;
}
