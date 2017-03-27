/*
 * Copyright (c) 2003-2007,2009-2010,2013-2017 Apple Inc. All Rights Reserved.
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
 * verify-cert.c
 */

#define CFRELEASE(cf)	if (cf) { CFRelease(cf); }

#include <Security/SecCertificate.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecTrust.h>
#include <Security/SecPolicy.h>
#include <utilities/fileIo.h>

#include <sys/stat.h>
#include <stdio.h>
#include <time.h>

CFStringRef policyToConstant(const char *policy);
int verify_cert(int argc, char * const *argv);

static int addCertFile(const char *fileName, CFMutableArrayRef *array) {
    SecCertificateRef certRef = NULL;
    CFDataRef dataRef = NULL;
    unsigned char *buf = NULL;
    size_t numBytes;
    int rtn = 0;

    if (readFileSizet(fileName, &buf, &numBytes)) {
        rtn = -1;
        goto errOut;
    }

    dataRef = CFDataCreate(NULL, buf, numBytes);
    certRef = SecCertificateCreateWithData(NULL, dataRef);
    if (!certRef) {
        certRef = SecCertificateCreateWithPEM(NULL, dataRef);
        if (!certRef) {
            rtn = -1;
            goto errOut;
        }
    }

    if (*array == NULL) {
        *array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    }

    CFArrayAppendValue(*array, certRef);

errOut:
    /* Cleanup */
    free(buf);
    CFRELEASE(dataRef);
    CFRELEASE(certRef);
    return rtn;
}

CFStringRef policyToConstant(const char *policy) {
    if (policy == NULL) {
        return NULL;
    }
    else if (!strcmp(policy, "basic")) {
        return kSecPolicyAppleX509Basic;
    }
    else if (!strcmp(policy, "ssl")) {
        return kSecPolicyAppleSSL;
    }
    else if (!strcmp(policy, "smime")) {
        return kSecPolicyAppleSMIME;
    }
    else if (!strcmp(policy, "eap")) {
        return kSecPolicyAppleEAP;
    }
    else if (!strcmp(policy, "IPSec")) {
        return kSecPolicyAppleIPsec;
    }
    else if (!strcmp(policy, "appleID")) {
        return kSecPolicyAppleIDValidation;
    }
    else if (!strcmp(policy, "codeSign")) {
        return kSecPolicyAppleCodeSigning;
    }
    else if (!strcmp(policy, "timestamping")) {
        return kSecPolicyAppleTimeStamping;
    }
    else if (!strcmp(policy, "revocation")) {
        return kSecPolicyAppleRevocation;
    }
    else if (!strcmp(policy, "passbook")) {
        /* Passbook not implemented */
        return NULL;
    }
    else {
        return NULL;
    }
}

int verify_cert(int argc, char * const *argv) {
	extern char	*optarg;
	extern int optind;
	int	arg;

	CFMutableArrayRef certs = NULL;
	CFMutableArrayRef roots = NULL;

    CFMutableDictionaryRef dict = NULL;
    CFStringRef name = NULL;
    CFBooleanRef client = kCFBooleanFalse;

    OSStatus ortn;
	int	ourRtn = 0;
	bool quiet = false;

    struct tm time;
    CFGregorianDate gregorianDate;
    CFDateRef dateRef = NULL;

    CFStringRef policy = NULL;
	SecPolicyRef policyRef = NULL;
    Boolean fetch = true;
	SecTrustRef	trustRef = NULL;
	SecTrustResultType resultType;

	if (argc < 2) {
        /* Return 2 triggers usage message. */
		return 2;
	}

	optind = 1;

	while ((arg = getopt(argc, argv, "c:r:p:d:n:LqC")) != -1) {
		switch (arg) {
			case 'c':
				/* Can be specified multiple times */
				if (addCertFile(optarg, &certs)) {
                    fprintf(stderr, "Cert file error\n");
                    ourRtn = 1;
					goto errOut;
				}
				break;
			case 'r':
				/* Can be specified multiple times */
				if (addCertFile(optarg, &roots)) {
                    fprintf(stderr, "Root file error\n");
					ourRtn = 1;
                    goto errOut;
				}
				break;
			case 'p':
                policy = policyToConstant(optarg);
				if (policy == NULL) {
                    fprintf(stderr, "Policy processing error\n");
                    ourRtn = 2;
					goto errOut;
				}
				break;
			case 'L':
                /* Force no network fetch of certs */
                fetch = false;
				break;
			case 'n':
                if (name == NULL) {
                    name = CFStringCreateWithCString(NULL, optarg, kCFStringEncodingUTF8);
                }
				break;
			case 'q':
				quiet = true;
				break;
            case 'C':
                /* Set to client */
                client = kCFBooleanTrue;
                break;
            case 'd':
                memset(&time, 0, sizeof(struct tm));
                if (strptime(optarg, "%Y-%m-%d-%H:%M:%S", &time) == NULL) {
                    if (strptime(optarg, "%Y-%m-%d", &time) == NULL) {
                        fprintf(stderr, "Date processing error\n");
                        ourRtn = 2;
                        goto errOut;
                    }
                }

                gregorianDate.second = time.tm_sec;
                gregorianDate.minute = time.tm_min;
                gregorianDate.hour = time.tm_hour;
                gregorianDate.day = time.tm_mday;
                gregorianDate.month = time.tm_mon + 1;
                gregorianDate.year = time.tm_year + 1900;

                if (dateRef == NULL) {
                    dateRef = CFDateCreate(NULL, CFGregorianDateGetAbsoluteTime(gregorianDate, NULL));
                }
                break;
			default:
                fprintf(stderr, "Usage error\n");
                ourRtn = 2;
				goto errOut;
		}
	}

	if (optind != argc) {
		ourRtn = 2;
		goto errOut;
	}

    if (policy == NULL) {
        policy = kSecPolicyAppleX509Basic;
    }

	if (certs == NULL) {
        if (roots == NULL) {
			fprintf(stderr, "No certs specified.\n");
			ourRtn = 2;
			goto errOut;
		}
		if (CFArrayGetCount(roots) != 1) {
			fprintf(stderr, "Multiple roots and no certs not allowed.\n");
			ourRtn = 2;
			goto errOut;
		}

		/* No certs and one root: verify the root */
		certs = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		CFArrayAppendValue(certs, CFArrayGetValueAtIndex(roots, 0));
	}

    /* Per-policy options */
    if (!CFStringCompare(policy, kSecPolicyAppleSSL, 0) || !CFStringCompare(policy, kSecPolicyAppleIPsec, 0)) {
        dict = CFDictionaryCreateMutable(NULL, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

        if (name == NULL) {
            fprintf(stderr, "Name not specified for IPsec or SSL policy. '-n' is a required option for these policies.");
            ourRtn = 2;
            goto errOut;
        }
        CFDictionaryAddValue(dict, kSecPolicyName, name);
        CFDictionaryAddValue(dict, kSecPolicyClient, client);
    }
    else if (!CFStringCompare(policy, kSecPolicyAppleEAP, 0)) {
        dict = CFDictionaryCreateMutable(NULL, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

        CFDictionaryAddValue(dict, kSecPolicyClient, client);
    }
    else if (!CFStringCompare(policy, kSecPolicyAppleSMIME, 0)) {
        dict = CFDictionaryCreateMutable(NULL, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

        if (name == NULL) {
            fprintf(stderr, "Name not specified for SMIME policy. '-n' is a required option for this policy.");
            ourRtn = 2;
            goto errOut;
        }
        CFDictionaryAddValue(dict, kSecPolicyName, name);
    }

    policyRef = SecPolicyCreateWithProperties(policy, dict);

	/* Now create a SecTrustRef and set its options */
	ortn = SecTrustCreateWithCertificates(certs, policyRef, &trustRef);
	if (ortn) {
        fprintf(stderr, "SecTrustCreateWithCertificates\n");
		ourRtn = 1;
		goto errOut;
	}

	/* Roots (anchors) are optional */
	if (roots != NULL) {
		ortn = SecTrustSetAnchorCertificates(trustRef, roots);
		if (ortn) {
            fprintf(stderr, "SecTrustSetAnchorCertificates\n");
			ourRtn = 1;
			goto errOut;
		}
	}
    if (fetch == false) {
        ortn = SecTrustSetNetworkFetchAllowed(trustRef, fetch);
        if (ortn) {
            fprintf(stderr, "SecTrustSetNetworkFetchAllowed\n");
            ourRtn = 1;
            goto errOut;
        }
    }

    /* Set verification time for trust object */
    if (dateRef != NULL) {
        ortn = SecTrustSetVerifyDate(trustRef, dateRef);
        if (ortn) {
            fprintf(stderr, "SecTrustSetVerifyDate\n");
            ourRtn = 1;
            goto errOut;
        }
    }

	/* Evaluate certs */
	ortn = SecTrustEvaluate(trustRef, &resultType);
	if (ortn) {
		/* Should never fail - error doesn't mean the cert verified badly */
        fprintf(stderr, "SecTrustEvaluate\n");
		ourRtn = 1;
		goto errOut;
	}
	switch (resultType) {
		case kSecTrustResultUnspecified:
			/* Cert chain valid, no special UserTrust assignments */
		case kSecTrustResultProceed:
			/* Cert chain valid AND user explicitly trusts this */
			break;
		case kSecTrustResultDeny:
            /* User-configured denial */
			if (!quiet) {
				fprintf(stderr, "SecTrustEvaluate result: kSecTrustResultDeny\n");
			}
			ourRtn = 1;
			break;
		case kSecTrustResultConfirm:
			/* Cert chain may well have verified OK, but user has flagged
			 one of these certs as untrustable. */
			if (!quiet) {
				fprintf(stderr, "SecTrustEvaluate result: kSecTrustResultConfirm\n");
			}
			ourRtn = 1;
			break;
        case kSecTrustResultInvalid:
            /* SecTrustEvaluate not called yet */
            if (!quiet) {
                fprintf(stderr, "SecTrustEvaluate result: kSecTrustResultInvalid\n");
            }
            ourRtn = 1;
            break;
        case kSecTrustResultRecoverableTrustFailure:
            /* Failure, can be user-overridden */
            if (!quiet) {
                fprintf(stderr, "SecTrustEvaluate result: kSecTrustResultRecoverableTrustFailure\n");
            }
            ourRtn = 1;
            break;
        case kSecTrustResultFatalTrustFailure:
            /* Complete failure */
            if (!quiet) {
                fprintf(stderr, "SecTrustEvaluate result: kSecTrustResultFatalTrustFailure\n");
            }
            ourRtn = 1;
            break;
        case kSecTrustResultOtherError:
            /* Failure unrelated to trust evaluation */
            if (!quiet) {
                fprintf(stderr, "SecTrustEvaluate result: kSecTrustResultOtherError\n");
            }
            ourRtn = 1;
            break;
		default:
            /* Error is not a defined SecTrustResultType */
			if (!quiet) {
				fprintf(stderr, "Cert Verify Result: %u\n", resultType);
			}
            ourRtn = 1;
			break;
	}

	if ((ourRtn == 0) && !quiet) {
		printf("...certificate verification successful.\n");
	}
errOut:
	/* Cleanup */
	CFRELEASE(certs);
	CFRELEASE(roots);
    CFRELEASE(dateRef);
    CFRELEASE(dict);
	CFRELEASE(policyRef);
	CFRELEASE(trustRef);
    CFRELEASE(name);
	return ourRtn;
}
