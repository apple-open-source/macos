/*
 * Copyright (c) 2006 Apple Computer, Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 *
 * verify_cert.c
 */

#include <Security/SecTrust.h>
#include <Security/SecKeychain.h>
#include <Security/SecPolicy.h>
#include <Security/SecPolicySearch.h>
#include <Security/cssmapple.h>
#include <Security/oidsalg.h>
#include <stdlib.h>
#include <unistd.h>
#include "trusted_cert_utils.h"

/* 
 * Read file as a cert, add to a CFArray, creating the array if necessary
 */
static int addCertFile(
	const char *fileName,
	CFMutableArrayRef *array)
{
	SecCertificateRef certRef;
	
	if(readCertFile(fileName, &certRef)) {
		return -1;
	}	
	if(*array == NULL) {
		*array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	}
	CFArrayAppendValue(*array, certRef);
	CFRelease(certRef);
	return 0;
}

int
verify_cert(int argc, char * const *argv)
{
	extern char			*optarg;
	extern int			optind;
	OSStatus			ortn;
	int					arg;
	CFMutableArrayRef	certs = NULL;
	CFMutableArrayRef	roots = NULL;
	CFMutableArrayRef	keychains = NULL;
	const CSSM_OID		*policy = &CSSMOID_APPLE_X509_BASIC;
	SecKeychainRef		kcRef = NULL;
	int					ourRtn = 0;
	bool				quiet = false;
	SecPolicyRef		policyRef = NULL;
	SecTrustRef			trustRef = NULL;
	SecPolicySearchRef	searchRef = NULL;
	const char			*emailAddrs = NULL;
	const char			*sslHost = NULL;
	CSSM_APPLE_TP_SSL_OPTIONS	sslOpts;
	CSSM_APPLE_TP_SMIME_OPTIONS	smimeOpts;
	CSSM_APPLE_TP_ACTION_FLAGS actionFlags = 0;
	CSSM_APPLE_TP_ACTION_DATA	actionData;
	CSSM_DATA			optionData;
	CFDataRef			cfActionData = NULL;
	SecTrustResultType	resultType;
	OSStatus			ocrtn;
	
	if(argc < 2) {
		return 2; /* @@@ Return 2 triggers usage message. */
	}
	optind = 1;
	while ((arg = getopt(argc, argv, "c:r:p:k:e:s:lnq")) != -1) {
		switch (arg) {
			case 'c':
				/* this can be specifed multiple times */
				if(addCertFile(optarg, &certs)) {
					ourRtn = 1;
					goto errOut;
				}
				break;
			case 'r':
				/* this can be specifed multiple times */
				if(addCertFile(optarg, &roots)) {
					ourRtn = 1;
					goto errOut;
				}
				break;
			case 'p':
				policy = policyStringToOid(optarg);
				if(policy == NULL) {
					ourRtn = 2;
					goto errOut;
				}
				break;
			case 'k':
				ortn = SecKeychainOpen(optarg, &kcRef);
				if(ortn) {
					cssmPerror("SecKeychainOpen", ortn);
					ourRtn = 1;
					goto errOut;
				}
				/* this can be specifed multiple times */
				if(keychains == NULL) {
					keychains = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
				}	
				CFArrayAppendValue(keychains, kcRef);
				CFRelease(kcRef);
				break;
			case 'l':
				actionFlags |= CSSM_TP_ACTION_LEAF_IS_CA;
				break;
			case 'n':
				/* No keychains, signalled by empty keychain array */
				if(keychains != NULL) {
					fprintf(stderr, "-k and -n are mutually exclusive\n");
					ourRtn = 2;
					goto errOut;
				}
				keychains = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
				break;
			case 'e':
				emailAddrs = optarg;
				break;
			case 's':
				sslHost = optarg;
				break;
			case 'q':
				quiet = true;
				break;
			default:
				ourRtn = 2;
				goto errOut;
		}
	}
	if(optind != argc) {
		ourRtn = 2;
		goto errOut;
	}
	
	if(certs == NULL) {
		if(roots == NULL) {
			fprintf(stderr, "***No certs specified.\n");
			ourRtn = 2;
			goto errOut;
		}
		if(CFArrayGetCount(roots) != 1) {
			fprintf(stderr, "***Multiple roots and no certs not allowed.\n");
			ourRtn = 2;
			goto errOut;
		}
		
		/* no certs and one root: verify the root */
		certs = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		CFArrayAppendValue(certs, CFArrayGetValueAtIndex(roots, 0));
		actionFlags |= CSSM_TP_ACTION_LEAF_IS_CA;
	}
	
	/* cook up a SecPolicyRef */
	ortn = SecPolicySearchCreate(CSSM_CERT_X_509v3,
		policy,
		NULL,				// policy opts
		&searchRef);
	if(ortn) {
		cssmPerror("SecPolicySearchCreate", ortn);
		ourRtn = 1;
		goto errOut;
	}
	ortn = SecPolicySearchCopyNext(searchRef, &policyRef);
	if(ortn) {
		cssmPerror("SecPolicySearchCopyNext", ortn);
		ourRtn = 1;
		goto errOut;
	}
	
	/* per-policy options */
	if(compareOids(policy, &CSSMOID_APPLE_TP_SSL)) {
		if(sslHost != NULL) {
			memset(&sslOpts, 0, sizeof(sslOpts));
			sslOpts.Version = CSSM_APPLE_TP_SSL_OPTS_VERSION;
			sslOpts.ServerName = sslHost;
			sslOpts.ServerNameLen = strlen(sslHost);
			optionData.Data = (uint8 *)&sslOpts;
			optionData.Length = sizeof(sslOpts);
			ortn = SecPolicySetValue(policyRef, &optionData);
			if(ortn) {
				cssmPerror("SecPolicySetValue", ortn);
				ourRtn = 1;
				goto errOut;
			}
		}
	}
	if(compareOids(policy, &CSSMOID_APPLE_TP_SMIME)) {
		if(emailAddrs != NULL) {
			memset(&smimeOpts, 0, sizeof(smimeOpts));
			smimeOpts.Version = CSSM_APPLE_TP_SMIME_OPTS_VERSION;
			smimeOpts.SenderEmail = emailAddrs;
			smimeOpts.SenderEmailLen = strlen(emailAddrs);
			optionData.Data = (uint8 *)&smimeOpts;
			optionData.Length = sizeof(smimeOpts);
			ortn = SecPolicySetValue(policyRef, &optionData);
			if(ortn) {
				cssmPerror("SecPolicySetValue", ortn);
				ourRtn = 1;
				goto errOut;
			}
		}
	}

	/* Now create a SecTrustRef and set its options */
	ortn = SecTrustCreateWithCertificates(certs, policyRef, &trustRef);
	if(ortn) {
		cssmPerror("SecTrustCreateWithCertificates", ortn);
		ourRtn = 1;
		goto errOut;
	}
	
	/* roots (anchors) are optional */
	if(roots != NULL) {
		ortn = SecTrustSetAnchorCertificates(trustRef, roots);
		if(ortn) {
			cssmPerror("SecTrustSetAnchorCertificates", ortn);
			ourRtn = 1;
			goto errOut;
		}
	}
	if(actionFlags) {
		memset(&actionData, 0, sizeof(actionData));
		actionData.Version = CSSM_APPLE_TP_ACTION_VERSION;
		actionData.ActionFlags = actionFlags;
		cfActionData = CFDataCreate(NULL, (UInt8 *)&actionData, sizeof(actionData));
		ortn = SecTrustSetParameters(trustRef, CSSM_TP_ACTION_DEFAULT, cfActionData);
		if(ortn) {
			cssmPerror("SecTrustSetParameters", ortn);
			ourRtn = 1;
			goto errOut;
		}
	}
	if(keychains) {
		ortn = SecTrustSetKeychains(trustRef, keychains);
		if(ortn) {
			cssmPerror("SecTrustSetKeychains", ortn);
			ourRtn = 1;
			goto errOut;
		}
	}
	
	/* GO */
	ortn = SecTrustEvaluate(trustRef, &resultType);
	if(ortn) {
		/* should never fail - error on this doesn't mean the cert verified badly */
		cssmPerror("SecTrustEvaluate", ortn);
		ourRtn = 1;
		goto errOut;
	}
	switch(resultType) {
		case kSecTrustResultUnspecified:
			/* cert chain valid, no special UserTrust assignments */
		case kSecTrustResultProceed:
			/* cert chain valid AND user explicitly trusts this */
			break;
		case kSecTrustResultDeny:
			if(!quiet) {
				fprintf(stderr, "SecTrustEvaluate result: kSecTrustResultDeny\n");
			}
			ourRtn = 1;
			break;
		case kSecTrustResultConfirm:
			/*
			 * Cert chain may well have verified OK, but user has flagged
			 * one of these certs as untrustable.
			 */
			if(!quiet) {
				fprintf(stderr, "SecTrustEvaluate result: kSecTrustResultConfirm\n");
			}
			ourRtn = 1;
			break;
		default:
			ourRtn = 1;
			if(!quiet) {
				/* See what the TP had to say about this */
				ortn = SecTrustGetCssmResultCode(trustRef, &ocrtn);
				if(ortn) {
					cssmPerror("SecTrustGetCssmResultCode", ortn);
				}
				else  {
					cssmPerror("Cert Verify Result", ocrtn);
				}
			}
			break;
	}
	
	if((ourRtn == 0) & !quiet) {
		printf("...certificate verification successful.\n");
	}
errOut:
	/* cleanup */
	CFRELEASE(certs);
	CFRELEASE(roots);
	CFRELEASE(keychains);
	CFRELEASE(policyRef);
	CFRELEASE(trustRef);
	CFRELEASE(searchRef);
	CFRELEASE(cfActionData);
	return ourRtn;
}
