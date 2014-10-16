/*
 * Copyright (c) 2006-2010,2012,2014 Apple Inc. All Rights Reserved.
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
 * trust_cert_add.c
 */

/*
 * This command is fairly versatile and hence the usage might be a bit confusing.
 * The standard usage of this command is to add one or more certs to a Trust
 * Settings domain, along with optional usage constraints. Often, but not
 * necessarily, you'd also add the cert to a keychain while you're adding
 * it to Trust Settings.
 *
 * -- To add someRoot.cer to your login keychain and to your Trust Settings as
 *    an unrestricted root cert:
 *
 *    % security add-trusted-cert -k login.keychain someRoot.cer
 *
 * -- To add anotherRoot.cer to the local admin trust settings, only for policy
 *    ssl, without adding it to a keychain (presumably because it's already in
 *    a keychain somewhere else):
 *
 *    % security add-trusted-cert -p ssl -d anotherRoot.cer
 *
 * The more obscure uses involve default settings and trust settings files.
 *
 * Specifying a default trust setting precludes specifying a cert. Other
 * options apply as usual; note that if the domain for which you are
 * specifying a default setting already has a default setting, the old default
 * will be replaced by the new one you specify.
 *
 * -- To specify a default of "deny" for policy SMIME for the admin domain:
 *
 *    % security add-trusted-cert -p smime -r deny -D
 *
 * This command can also operate on trust settings as files instead of
 * modifying an actual on-disk Trust Settings record. One standard use for
 * this function is in the creation of the system Trust Settings, which
 * are immutable at runtime via the SecTrustSettings API. You provide a
 * file name for this option via -f settingsFile. If the file does not
 * exist, a new empty Trust Settings will be created, and certs and/or
 * a default will be added to that record, and the record will be written
 * out to the filename you provide (infile = outfile, always).
 *
 * -- To create Trust Settings record with one cert in it, restricted to
 *    policy SSL:
 *
 *    % security add-trusted-cert -p ssl -f someTrustSettingsFile.plist -r someRoot.cer
 *
 * You can also use the -f option and specify no certs, in which case an empty
 * Trust Settings record will be created. This can be useful if you want to
 * quickly reset the Trust Settings in a given domain to "empty"; the
 * empty Trust Settings record can be imported via the trust-settings-import
 * command.
 *
 * -- To reset the admin trust settings to "empty":
 *
 *    % security add-trusted-cert -f emptySettingsFile.plist
 *    % security trust-settings-import -d emptySettingsFile.plist
 */

#include "trusted_cert_add.h"
#include "trusted_cert_utils.h"
#include "security.h"
#include "keychain_utilities.h"
#include <Security/Security.h>
#include <Security/SecTrust.h>
#include <Security/SecTrustSettings.h>
#include <Security/SecTrustSettingsPriv.h>
#include <Security/oidsalg.h>
#include <errno.h>
#include <unistd.h>
#include <security_cdsa_utils/cuFileIo.h>
#include <CoreFoundation/CoreFoundation.h>

/* r/w files as CFData */
static CFDataRef readFileData(
	const char *fileName)
{
	unsigned char *d;
	unsigned dLen;

	if(readFile(fileName, &d, &dLen)) {
		return NULL;
	}
	CFDataRef cfd = CFDataCreate(NULL, (const UInt8 *)d, dLen);
	free(d);
	return cfd;
}

static int writeFileData(
	const char *fileName,
	CFDataRef cfd)
{
	unsigned long l = (unsigned long)CFDataGetLength(cfd);
	int rtn = writeFile(fileName, CFDataGetBytePtr(cfd), l);
	if(rtn) {
		fprintf(stderr, "Error %d writing to %s\n", rtn, fileName);
	}
	else if(!do_quiet) {
		fprintf(stdout, "...wrote %ld bytes to %s\n", l, fileName);
	}
	return rtn;
}

static int appendConstraintsToDict(
	const char *appPath,			/* optional */
	const char *policy,				/* optional - smime, ssl, etc. */
	const char *policyStr,			/* optional policy string */
	SecTrustSettingsResult resultType,
	CSSM_RETURN allowErr,			/* optional allowed error */
	SecTrustSettingsKeyUsage keyUse,/* optional key use */
	CFMutableDictionaryRef *dict)	/* result RETURNED here, created if necessary */
{
	if(*dict == NULL) {
		*dict = CFDictionaryCreateMutable(NULL,
			0,		// capacity
			&kCFTypeDictionaryKeyCallBacks,
			&kCFTypeDictionaryValueCallBacks);
	}

	/* OID string to an OID pointer */
	const CSSM_OID *oid = NULL;
	if(policy != NULL) {
		oid = policyStringToOid(policy);
		if(oid == NULL) {
			return 2;
		}

		/* OID to SecPolicyRef */
		SecPolicyRef policyRef = oidToPolicy(oid);
		if(policyRef == NULL) {
			return 2;
		}
		CFDictionaryAddValue(*dict, kSecTrustSettingsPolicy, policyRef);
		CFRelease(policyRef);
	}

	/* app string to SecTrustedApplicationRef */
	if(appPath != NULL) {
		SecTrustedApplicationRef appRef;
		OSStatus ortn = SecTrustedApplicationCreateFromPath(appPath, &appRef);
		if(ortn) {
			cssmPerror("SecTrustedApplicationCreateFromPath", ortn);
			return -1;
		}
		CFDictionaryAddValue(*dict, kSecTrustSettingsApplication, appRef);
		CFRelease(appRef);
	}

	if(policyStr != NULL) {
		CFStringRef pstr = CFStringCreateWithCString(NULL, policyStr, kCFStringEncodingUTF8);
		CFDictionaryAddValue(*dict, kSecTrustSettingsPolicyString, pstr);
		CFRelease(pstr);
	}

	if(allowErr) {
		SInt32 ae = (SInt32)allowErr;
		CFNumberRef cfNum = CFNumberCreate(NULL, kCFNumberSInt32Type, &ae);
		CFDictionaryAddValue(*dict, kSecTrustSettingsAllowedError, cfNum);
		CFRelease(cfNum);
	}

	if(keyUse != 0) {
		SInt32 ku = (SInt32)keyUse;
		CFNumberRef cfNum = CFNumberCreate(NULL, kCFNumberSInt32Type, &ku);
		CFDictionaryAddValue(*dict, kSecTrustSettingsKeyUsage, cfNum);
		CFRelease(cfNum);
	}

	if(resultType != kSecTrustSettingsResultTrustRoot) {
		SInt32 rt = (SInt32)resultType;
		CFNumberRef cfNum = CFNumberCreate(NULL, kCFNumberSInt32Type, &rt);
		CFDictionaryAddValue(*dict, kSecTrustSettingsResult, cfNum);
		CFRelease(cfNum);
	}

	return 0;
}


int
trusted_cert_add(int argc, char * const *argv)
{
	extern char *optarg;
	extern int optind;
	OSStatus ortn;
	int arg;
	SecTrustSettingsDomain domain = kSecTrustSettingsDomainUser;
	int ourRtn = 0;
	SecKeychainRef kcRef = NULL;
	int defaultSetting = 0;
	char *certFile = NULL;
	SecCertificateRef certRef = NULL;

	/* for operating in file-based settings */
	char *settingsFileIn = NULL;
	char *settingsFileOut = NULL;
	CFDataRef settingsIn = NULL;
	CFDataRef settingsOut = NULL;

	/* optional usage constraints */
//	char *policy = NULL;
	char *appPath = NULL;
//	char *policyString = NULL;
	SecTrustSettingsResult resultType = kSecTrustSettingsResultTrustRoot;
	CSSM_RETURN allowErr = CSSM_OK;
	SecTrustSettingsKeyUsage keyUse = 0;
	CFMutableArrayRef trustSettings = NULL;
	int haveConstraints = 0;

	const int maxPolicies = 16; // upper limit on policies that can be set in one invocation
	char *policyNames[maxPolicies];
	char *policyStrings[maxPolicies];
	int allowedErrors[maxPolicies];
	int policyNameCount = 0, policyStringCount = 0, allowedErrorCount = 0;

	if(argc < 2) {
		return 2; /* @@@ Return 2 triggers usage message. */
	}

	optind = 1;
	while ((arg = getopt(argc, argv, "dr:a:p:s:e:u:k:i:o:Dh")) != -1) {
		switch (arg) {
			case 'd':
				domain = kSecTrustSettingsDomainAdmin;
				break;
			case 'r':
				if(!strcmp(optarg, "trustRoot")) {
					resultType = kSecTrustSettingsResultTrustRoot;
				}
				else if(!strcmp(optarg, "trustAsRoot")) {
					resultType = kSecTrustSettingsResultTrustAsRoot;
				}
				else if(!strcmp(optarg, "deny")) {
					resultType = kSecTrustSettingsResultDeny;
				}
				else if(!strcmp(optarg, "unspecified")) {
					resultType = kSecTrustSettingsResultUnspecified;
				}
				else {
					return 2;
				}
				haveConstraints = 1;
				break;
			case 'p':
				if (policyNameCount < maxPolicies) {
					policyNames[policyNameCount++] = optarg;
				} else {
					fprintf(stderr, "Too many policy arguments.\n");
					return 2;
				}
				haveConstraints = 1;
				break;
			case 'a':
				appPath = optarg;
				haveConstraints = 1;
				break;
			case 's':
				if (policyStringCount < maxPolicies) {
					policyStrings[policyStringCount++] = optarg;
				} else {
					fprintf(stderr, "Too many policy string arguments.\n");
					return 2;
				}
				haveConstraints = 1;
				break;
			case 'e':
				if (allowedErrorCount < maxPolicies) {
					if (!strcmp("certExpired", optarg))
						allowErr = -2147409654; // 0x8001210A = CSSMERR_TP_CERT_EXPIRED
					else if (!strcmp("hostnameMismatch", optarg))
						allowErr = -2147408896; // 0x80012400 = CSSMERR_APPLETP_HOSTNAME_MISMATCH
					else
						allowErr = (CSSM_RETURN)atoi(optarg);
					if (!allowErr) {
						fprintf(stderr, "Invalid value for allowed error.\n");
						return 2;
					}
					allowedErrors[allowedErrorCount++] = allowErr;
				} else {
					fprintf(stderr, "Too many \"allowed error\" arguments.\n");
					return 2;
				}
				haveConstraints = 1;
				break;
			case 'u':
				keyUse = (SecTrustSettingsKeyUsage)atoi(optarg);
				haveConstraints = 1;
				break;
			case 'k':
				kcRef = keychain_open(optarg);
				if(kcRef == NULL) {
					return 1;
				}
				break;
			case 'i':
				settingsFileIn = optarg;
				break;
			case 'o':
				settingsFileOut = optarg;
				break;
			case 'D':
				defaultSetting = 1;
				break;
			default:
			case 'h':
				return 2; /* @@@ Return 2 triggers usage message. */
		}
	}
	if(ourRtn) {
		goto errOut;
	}

	switch(argc - optind) {
		case 0:
			/* no certs */
			break;
		case 1:
			certFile = argv[optind];
			break;
		default:
			ourRtn = 2;
			goto errOut;
	}

	/* validate inputs */
	if(defaultSetting && (certFile != NULL)) {
		fprintf(stderr, "Can't specify cert when manipulating default setting.\n");
		ourRtn = 2; /* @@@ Return 2 triggers usage message. */
		goto errOut;
	}
	if((certFile == NULL) && (settingsFileOut == NULL) && !defaultSetting) {
		/* no cert file - only legal for r/w file or for default settings */
		fprintf(stderr, "No cert file specified.\n");
		ourRtn = 2;
		goto errOut;
	}
	if((settingsFileOut != NULL) && (domain != kSecTrustSettingsDomainUser)) {
		fprintf(stderr, "Can't specify both domain and a settingsFile\n");
		ourRtn = 2;
		goto errOut;
	}
	if((settingsFileIn != NULL) && (settingsFileOut == NULL)) {
		/* on the other hand, fileOut with no fileIn is OK */
		fprintf(stderr, "Can't specify settingsFileIn and no settingsFileOut\n");
		ourRtn = 2;
		goto errOut;
	}

	/* build per-policy constraints dictionaries */
	if(haveConstraints) {
		int i, j, k;
		for (i=0; i<policyNameCount; i++) {
			if (!trustSettings) {
				trustSettings = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
			}
			if (policyStringCount) {
				for (j=0; j<policyStringCount; j++) {
					if (allowedErrorCount) {
						for (k=0; k<allowedErrorCount; k++) {
							CFMutableDictionaryRef constraintDict = NULL;
							ourRtn = appendConstraintsToDict(appPath,
															 policyNames[i],
															 policyStrings[j],
															 resultType,
															 allowedErrors[k],
															 keyUse,
															 &constraintDict);
							if (!ourRtn) {
								CFArrayAppendValue(trustSettings, constraintDict);
								CFRelease(constraintDict); // array retains it
							}
						}
					} else { // no allowed errors
						CFMutableDictionaryRef constraintDict = NULL;
						ourRtn = appendConstraintsToDict(appPath,
														 policyNames[i],
														 policyStrings[j],
														 resultType, 0, keyUse,
														 &constraintDict);
						if (!ourRtn) {
							CFArrayAppendValue(trustSettings, constraintDict);
							CFRelease(constraintDict); // array retains it
						}

					}
				}
			} else { // no policy strings
				if (allowedErrorCount) {
					for (k=0; k<allowedErrorCount; k++) {
						CFMutableDictionaryRef constraintDict = NULL;
						ourRtn = appendConstraintsToDict(appPath,
														 policyNames[i],
														 NULL,
														 resultType,
														 allowedErrors[k],
														 keyUse,
														 &constraintDict);
						if (!ourRtn) {
							CFArrayAppendValue(trustSettings, constraintDict);
							CFRelease(constraintDict); // array retains it
						}
					}
				} else { // no allowed errors
					CFMutableDictionaryRef constraintDict = NULL;
					ourRtn = appendConstraintsToDict(appPath,
													 policyNames[i],
													 NULL,
													 resultType, 0, keyUse,
													 &constraintDict);
					if (!ourRtn) {
						CFArrayAppendValue(trustSettings, constraintDict);
						CFRelease(constraintDict); // array retains it
					}
				}
			}
			if(ourRtn) {
				goto errOut;
			}
		}
	}

	/* handle the case where no policies were specified */
	if(haveConstraints && !trustSettings) {
		CFMutableDictionaryRef constraintDict = NULL;
		trustSettings = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		ourRtn = appendConstraintsToDict(appPath, NULL, NULL,
										 resultType, allowErr, keyUse,
										 &constraintDict);
		if (!ourRtn) {
			CFArrayAppendValue(trustSettings, constraintDict);
			CFRelease(constraintDict); // array retains it
		} else {
			goto errOut;
		}
	}

	/* optional settings file */
	if(settingsFileIn) {
		settingsIn = readFileData(settingsFileIn);
		if(settingsIn == NULL) {
			fprintf(stderr, "Error reading file %s\n", settingsFileIn);
			ourRtn = 1;
			goto errOut;
		}
	}
	else if(settingsFileOut) {
		/* output file, no input file - start with empty settings */
		ortn = SecTrustSettingsSetTrustSettingsExternal(NULL,
			NULL, NULL, &settingsIn);
		if(ortn) {
			cssmPerror("SecTrustSettingsSetTrustSettings", ortn);
			ourRtn = 1;
			goto errOut;
		}
	}

	/* optional cert file */
	if(defaultSetting) {
		/* we don't have a cert; use this instead... */
		certRef = kSecTrustSettingsDefaultRootCertSetting;
	}
	else if(certFile != NULL) {
		if(readCertFile(certFile, &certRef)) {
			fprintf(stderr, "Error reading file %s\n", certFile);
			ourRtn = 1;
			goto errOut;
		}
		if(kcRef) {
			/* note we do NOT add by default */
			ortn = SecCertificateAddToKeychain(certRef, kcRef);
			switch(ortn) {
				case noErr:
				case errSecDuplicateItem:		/* that's fine */
					break;
				default:
					cssmPerror("SecCertificateAddToKeychain", ortn);
					ourRtn = 1;
					goto errOut;
			}
		}
	}

	/* now manipulate the Trust Settings */
	if(settingsFileOut) {
		/*
		 * Operating on file data, not actual domain.
		 * At this point settingsIn is the current settings data; it
		 * may be empty but it's valid nonetheless.
		 */
		if(certRef != NULL) {
			ortn = SecTrustSettingsSetTrustSettingsExternal(settingsIn,
				certRef, trustSettings, &settingsOut);
			if(ortn) {
				cssmPerror("SecTrustSettingsSetTrustSettings", ortn);
				ourRtn = 1;
				goto errOut;
			}
		}
		else {
			/* no cert data: output := input, e.g. create empty settings file */
			settingsOut = settingsIn;
			settingsIn = NULL;
		}
		ourRtn = writeFileData(settingsFileOut, settingsOut);
		if(ourRtn) {
			fprintf(stderr, "Error writing to %s\n", settingsFileOut);
			goto errOut;
		}
	}
	else {
		/* normal "Add this cert to Trust Settings" */
		if(certRef == NULL) {
			fprintf(stderr, "Internal error in trusted_cert_add\n");
			ourRtn = 1;
			goto errOut;
		}
		ortn = SecTrustSettingsSetTrustSettings(certRef, domain, trustSettings);
		if(ortn) {
			cssmPerror("SecTrustSettingsSetTrustSettings", ortn);
			ourRtn = 1;
		}
	}
errOut:
	if((certRef != NULL) & (certRef != kSecTrustSettingsDefaultRootCertSetting)) {
		CFRelease(certRef);
	}
	CFRELEASE(trustSettings);
	CFRELEASE(kcRef);
	CFRELEASE(settingsIn);
	CFRELEASE(settingsOut);
	return ourRtn;
}

int
trusted_cert_remove(int argc, char * const *argv)
{
	OSStatus ortn = noErr;
	int ourRtn = 0;
	SecTrustSettingsDomain domain = kSecTrustSettingsDomainUser;
	int defaultSetting = 0;
	SecCertificateRef certRef = NULL;
	char *certFile = NULL;

	extern char *optarg;
	extern int optind;
	int arg;

	optind = 1;
	while ((arg = getopt(argc, argv, "dDh")) != -1) {
		switch (arg) {
			case 'd':
				domain = kSecTrustSettingsDomainAdmin;
				break;
			case 'D':
				defaultSetting = 1;
				break;
			default:
			case 'h':
				return 2; /* @@@ Return 2 triggers usage message. */
		}
	}

	switch(argc - optind) {
		case 0:
			/* no certs */
			break;
		case 1:
			certFile = argv[optind];
			break;
		default:
			return 2;
	}

	if((certFile == NULL) && !defaultSetting) {
		fprintf(stderr, "No cert file specified.\n");
		return 2;
	}
	if((certFile != NULL) && defaultSetting) {
		fprintf(stderr, "Can't specify cert when manipulating default setting.\n");
		return 2;
	}

	if(defaultSetting) {
		/* we don't have a cert; use this instead... */
		certRef = kSecTrustSettingsDefaultRootCertSetting;
	}
	else {
		if(readCertFile(certFile, &certRef)) {
			fprintf(stderr, "Error reading file %s\n", certFile);
			return 1;
		}
	}

	ortn = SecTrustSettingsRemoveTrustSettings(certRef, domain);
	if(ortn) {
		cssmPerror("SecTrustSettingsRemoveTrustSettings", ortn);
		ourRtn = 1;
	}

	if((certRef != NULL) & (certRef != kSecTrustSettingsDefaultRootCertSetting)) {
		CFRelease(certRef);
	}

	return ourRtn;
}
