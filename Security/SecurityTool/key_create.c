/*
 * Copyright (c) 2003-2004,2006,2008,2012,2014 Apple Inc. All Rights Reserved.
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
 * key_create.c
 */

#include "key_create.h"

#include "keychain_utilities.h"
#include "security.h"

#include <CoreFoundation/CFDateFormatter.h>
#include <CoreFoundation/CFString.h>
#include <Security/SecAccess.h>
#include <Security/SecKey.h>
#include <Security/SecKeychainItem.h>
#include <Security/SecTrustedApplication.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


static int
do_key_create_pair(const char *keychainName, SecAccessRef access, CSSM_ALGORITHMS algorithm, uint32 keySizeInBits, CFAbsoluteTime from_time, CFAbsoluteTime to_time, Boolean print_hash)
{
	SecKeychainRef keychain = NULL;
	OSStatus status;
	int result = 0;
	CSSM_CC_HANDLE contextHandle = 0;
	CSSM_KEYUSE publicKeyUsage = CSSM_KEYUSE_ENCRYPT | CSSM_KEYUSE_VERIFY | CSSM_KEYUSE_WRAP | CSSM_KEYUSE_DERIVE;
	uint32 publicKeyAttr = CSSM_KEYATTR_PERMANENT | CSSM_KEYATTR_EXTRACTABLE;
	CSSM_KEYUSE privateKeyUsage = CSSM_KEYUSE_DECRYPT | CSSM_KEYUSE_SIGN | CSSM_KEYUSE_UNWRAP | CSSM_KEYUSE_DERIVE;
	uint32 privateKeyAttr = CSSM_KEYATTR_PERMANENT | CSSM_KEYATTR_SENSITIVE | CSSM_KEYATTR_EXTRACTABLE;
	SecKeyRef publicKey = NULL;
	SecKeyRef privateKey = NULL;
	SecKeychainAttributeList *attrList = NULL;

	if (keychainName)
	{
		keychain = keychain_open(keychainName);
		if (!keychain)
		{
			result = 1;
			goto loser;
		}
	}

	status = SecKeyCreatePair(keychain, algorithm, keySizeInBits, contextHandle,
        publicKeyUsage,
        publicKeyAttr,
        privateKeyUsage,
        privateKeyAttr,
        access,
        &publicKey,
        &privateKey);
	if (status)
	{
		sec_error("SecKeyCreatePair %s: %s", keychainName ? keychainName : "<NULL>", sec_errstr(status));
		result = 1;
		goto loser;
	}

	if (print_hash)
	{
		SecItemClass itemClass = 0;
		UInt32 tag = 0x00000006;
		UInt32 format = CSSM_DB_ATTRIBUTE_FORMAT_BLOB;
		SecKeychainAttributeInfo info = { 1, &tag, &format };

		status = SecKeychainItemCopyAttributesAndData((SecKeychainItemRef)privateKey, &info, &itemClass, &attrList, NULL, NULL);
		if (status)
		{
			sec_perror("SecKeychainItemCopyAttributesAndData", status);
			result = 1;
			goto loser;
		}

		if (info.count != attrList->count)
		{
			sec_error("info count: %ld != attribute count: %ld", info.count, attrList->count);
			result = 1;
			goto loser;
		}

		if (tag != attrList->attr[0].tag)
		{
			sec_error("attribute info tag: %ld != attribute tag: %ld", tag, attrList->attr[0].tag);
			result = 1;
			goto loser;
		}

		print_buffer_pem(stdout, "PUBLIC KEY HASH", attrList->attr[0].length, attrList->attr[0].data);
	}

loser:
	if (attrList)
	{
		status = SecKeychainItemFreeAttributesAndData(attrList, NULL);
		if (status)
			sec_perror("SecKeychainItemFreeAttributesAndData", status);
	}

	if (keychain)
		CFRelease(keychain);
	if (publicKey)
		CFRelease(publicKey);
	if (privateKey)
		CFRelease(privateKey);

	return result;
}

static int
parse_algorithm(const char *name, CSSM_ALGORITHMS *algorithm)
{
	size_t len = strlen(name);

	if (!strncmp("rsa", name, len))
		*algorithm = CSSM_ALGID_RSA;
	else if (!strncmp("dsa", name, len))
		*algorithm = CSSM_ALGID_DSA;
	else if (!strncmp("dh", name, len))
		*algorithm = CSSM_ALGID_DH;
	else if (!strncmp("fee", name, len))
		*algorithm = CSSM_ALGID_FEE;
	else
	{
		sec_error("Invalid algorithm: %s", name);
		return 2;
	}

	return 0;
}

static int
parse_time(const char *time, CFAbsoluteTime *ptime)
{
    CFDateFormatterRef formatter = CFDateFormatterCreate(NULL, NULL, kCFDateFormatterShortStyle, kCFDateFormatterShortStyle);
	CFStringRef time_string = CFStringCreateWithCString(NULL, time, kCFStringEncodingUTF8);
	int result = 0;
	if (!CFDateFormatterGetAbsoluteTimeFromString(formatter, time_string, NULL, ptime))
	{
		sec_error("%s is not a valid date", time);
		result = 1;
	}
    if (formatter)
        CFRelease(formatter);
    if (time_string)
        CFRelease(time_string);
	return result;
}

int
key_create_pair(int argc, char * const *argv)
{
	const char *keychainName = NULL;
	CSSM_ALGORITHMS algorithm = CSSM_ALGID_RSA;
	uint32 keySizeInBits = 512;
	int ch, result = 0;
	OSStatus status;
	Boolean always_allow = FALSE;
	Boolean print_hash = FALSE;
	CFAbsoluteTime from_time = 0.0, to_time = 0.0;
	double days = 0.0;
	SecAccessRef access = NULL;
	CFMutableArrayRef trusted_list = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	CFStringRef description = NULL;

/*
    { "create-keypair", key_create_pair,
	  "[-a alg] [-s size] [-f date] [-t date] [-d days] [-k keychain] [-A|-T appPath] description\n"
	  "    -a  Use alg as the algorithm, can be rsa, dh, dsa or fee (default rsa)\n"
	  "    -s  Specify the keysize in bits (default 512)\n"
	  "    -f  Make a key valid from the specified date\n"
	  "    -t  Make a key valid to the specified date\n"
	  "    -d  Make a key valid for the number of days specified from now\n"
	  "    -k  Use the specified keychain rather than the default\n"
	  "    -H  Print the public key hash attribute\n"
	  "    -A  Allow any application to access without warning\n"
	  "    -T  Allow the application specified to access without warning (multiple -T options are allowed)\n"
	  "If no options are provided, ask the user interactively.",
*/

    while ((ch = getopt(argc, argv, "a:s:f:t:d:k:AHT:h")) != -1)
	{
		switch  (ch)
		{
        case 'a':
			result = parse_algorithm(optarg, &algorithm);
			if (result)
				goto loser;
			break;
        case 's':
			keySizeInBits = atoi(optarg);
			break;
		case 'k':
			keychainName = optarg;
			break;
		case 'A':
			always_allow = TRUE;
			break;
		case 'H':
			print_hash = TRUE;
			break;
		case 'T':
		{
			if (optarg[0])
			{
				SecTrustedApplicationRef app = NULL;
				status = SecTrustedApplicationCreateFromPath(optarg, &app);
				if (status)
				{
					sec_error("SecTrustedApplicationCreateFromPath %s: %s", optarg, sec_errstr(status));
					result = 1;
					goto loser;
				}

				CFArrayAppendValue(trusted_list, app);
				CFRelease(app);
			}
			break;
		}
		case 'f':
			 result = parse_time(optarg, &from_time);
			if (result)
				goto loser;
			break;
		case 't':
			 result = parse_time(optarg, &to_time);
			if (result)
				goto loser;
			break;
		case 'd':
			days = atof(optarg);
			if (days < 1)
			{
				result = 2;
				goto loser;
			}
			from_time = CFAbsoluteTimeGetCurrent();
			to_time = from_time + days * 86400.0;
			break;
		case '?':
		default:
			return 2; /* @@@ Return 2 triggers usage message. */
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 1)
	{
		if (*argv[0] == '\0')
		{
			result = 2;
			goto loser;
		}
		description  = CFStringCreateWithCString(NULL, argv[0], kCFStringEncodingUTF8);
	}
	else if (argc != 0)
		return 2;
	else
		description = CFStringCreateWithCString(NULL, "<key>", kCFStringEncodingUTF8);

	if (always_allow)
	{
		status = SecAccessCreate(description, NULL, &access);
		if (status)
		{
			sec_perror("SecAccessCreate", status);
			result = 1;
		}
	}
	else
	{
		status = SecAccessCreate(description, trusted_list, &access);
		if (status)
		{
			sec_perror("SecAccessCreate", status);
			result = 1;
		}
	}

	if (result)
		goto loser;

	result = do_key_create_pair(keychainName, access, algorithm, keySizeInBits, from_time, to_time, print_hash);

loser:
	if (description)
		CFRelease(description);
	if (trusted_list)
		CFRelease(trusted_list);
	if (access)
		CFRelease(access);

	return result;
}

#if 0
static OSStatus
createCertCsr(
	CSSM_TP_HANDLE		tpHand,				// eventually, a SecKeychainRef
	CSSM_CL_HANDLE		clHand,
	CSSM_CSP_HANDLE		cspHand,
	SecKeyRef			subjPubKey,
	SecKeyRef			signerPrivKey,
	CSSM_ALGORITHMS 	sigAlg,
	const CSSM_OID		*sigOid,
	/*
	 * Issuer's RDN is obtained from the issuer cert, if present, or is
	 * assumed to be the same as the subject name (i.e., we're creating
	 * a self-signed root cert).
	 */
	CSSM_BOOL			useAllDefaults,
	CSSM_DATA_PTR		csrData)			// CSR: mallocd and RETURNED
{
	CE_DataAndType 				exts[2];
	CE_DataAndType 				*extp = exts;
	unsigned					numExts;

	CSSM_DATA					refId;		// mallocd by CSSM_TP_SubmitCredRequest
	CSSM_APPLE_TP_CERT_REQUEST	certReq;
	CSSM_TP_REQUEST_SET			reqSet;
	sint32						estTime;
	CSSM_BOOL					confirmRequired;
	CSSM_TP_RESULT_SET_PTR		resultSet;
	CSSM_ENCODED_CERT			*encCert;
	CSSM_APPLE_TP_NAME_OID		subjectNames[MAX_NAMES];
	uint32						numNames;
	CSSM_TP_CALLERAUTH_CONTEXT 	CallerAuthContext;
	CSSM_FIELD					policyId;

	/* Note a lot of the CSSM_APPLE_TP_CERT_REQUEST fields are not
	 * used for the createCsr option, but we'll fill in as much as is practical
	 * for either case.
	 */
	numExts = 0;

	char challengeBuf[400];
	if(useAllDefaults) {
		strcpy(challengeBuf, ZDEF_CHALLENGE);
	}
	else {
		while(1) {
			getStringWithPrompt("Enter challenge string: ",
				challengeBuf, sizeof(challengeBuf));
			if(challengeBuf[0] != '\0') {
				break;
			}
		}
	}
	certReq.challengeString = challengeBuf;

	/* name array, get from user. */
	if(useAllDefaults) {
		subjectNames[0].string 	= ZDEF_COMMON_NAME;
		subjectNames[0].oid 	= &CSSMOID_CommonName;
		subjectNames[1].string	= ZDEF_ORG_NAME;
		subjectNames[1].oid 	= &CSSMOID_OrganizationName;
		subjectNames[2].string	= ZDEF_COUNTRY;
		subjectNames[2].oid 	= &CSSMOID_CountryName;
		subjectNames[3].string	= ZDEF_STATE;
		subjectNames[3].oid 	= &CSSMOID_StateProvinceName;
		numNames = 4;
	}
	else {
		getNameOids(subjectNames, &numNames);
	}

	/* certReq */
	certReq.cspHand = cspHand;
	certReq.clHand = clHand;
	certReq.serialNumber = 0x12345678;		// TBD - random? From user?
	certReq.numSubjectNames = numNames;
	certReq.subjectNames = subjectNames;

	/* TBD - if we're passed in a signing cert, certReq.issuerNameX509 will
	 * be obtained from that cert. For now we specify "self-signed" cert
	 * by not providing an issuer name at all. */
	certReq.numIssuerNames = 0;				// root for now
	certReq.issuerNames = NULL;
	certReq.issuerNameX509 = NULL;
	certReq.certPublicKey = subjPubKey;
	certReq.issuerPrivateKey = signerPrivKey;
	certReq.signatureAlg = sigAlg;
	certReq.signatureOid = *sigOid;
	certReq.notBefore = 0;					// TBD - from user
	certReq.notAfter = 60 * 60 * 24 * 30;	// seconds from now
	certReq.numExtensions = numExts;
	certReq.extensions = exts;

	reqSet.NumberOfRequests = 1;
	reqSet.Requests = &certReq;

	/* a CSSM_TP_CALLERAUTH_CONTEXT to specify an OID */
	memset(&CallerAuthContext, 0, sizeof(CSSM_TP_CALLERAUTH_CONTEXT));
	memset(&policyId, 0, sizeof(CSSM_FIELD));
	policyId.FieldOid = CSSMOID_APPLE_TP_CSR_GEN;
	CallerAuthContext.Policy.NumberOfPolicyIds = 1;
	CallerAuthContext.Policy.PolicyIds = &policyId;

	CSSM_RETURN crtn = CSSM_TP_SubmitCredRequest(tpHand,
		NULL,				// PreferredAuthority
		CSSM_TP_AUTHORITY_REQUEST_CERTISSUE,
		&reqSet,
		&CallerAuthContext,
		&estTime,
		&refId);

	/* before proceeding, free resources allocated thus far */
	if(!useAllDefaults) {
		freeNameOids(subjectNames, numNames);
	}

	if(crtn) {
		printError("***Error submitting credential request","CSSM_TP_SubmitCredRequest",crtn);
		return crtn;
	}
	crtn = CSSM_TP_RetrieveCredResult(tpHand,
		&refId,
		NULL,				// CallerAuthCredentials
		&estTime,
		&confirmRequired,
		&resultSet);
	if(crtn) {
		printError("***Error retreiving credential request","CSSM_TP_RetrieveCredResult",crtn);
		return crtn;
	}
	if(resultSet == NULL) {
		printf("***CSSM_TP_RetrieveCredResult returned NULL result set.\n");
		return ioErr;
	}
	encCert = (CSSM_ENCODED_CERT *)resultSet->Results;
	*csrData = encCert->CertBlob;

	/* free resources allocated by TP */
	APP_FREE(refId.Data);
	APP_FREE(encCert);
	APP_FREE(resultSet);
	return noErr;
}
#endif

#if 0
/* this was added in Michael's integration of PR-3420772, but this is an unimplemented command */

int
csr_create(int argc, char * const *argv)
{
	int result = 0;
	int ch;
	const char *keychainName = NULL;
	CSSM_ALGORITHMS algorithm = CSSM_ALGID_RSA;
	uint32 keySizeInBits = 512;
	OSStatus status;
	Boolean always_allow = FALSE;
	CFAbsoluteTime from_time = 0.0, to_time = 0.0;
	double days = 0.0;
	SecAccessRef access = NULL;
	CFMutableArrayRef trusted_list = NULL;
	CFStringRef description = NULL;

/*
    { "create-keypair", key_create_pair,
	  "[-a alg] [-s size] [-f date] [-t date] [-d days] [-k keychain] [-u sdux] [-c challenge] description\n"
	  " [-k keychain] [-u sewx] description\n"
	  "    -a  Look for key with alg as the algorithm, can be rsa, dh, dsa or fee (default rsa)\n"
	  "    -s  Look for key with keysize in bits\n"
	  "    -c  Add challenge to the key as a challange string\n"
	  "    -f  Look for a key at least valid from the specified date\n"
	  "    -t  Look for a key at least valid to the specified date\n"
	  "    -d  Look for a key at least valid for the number of days specified from now\n"
	  "	   -u  Look for a key with the specified usage flags (s)igning (d)ecryption (u)nwrapping e(x)change\n"
	  "    -k  Look in specified keychain rather than the default search list\n"
	  "If no options are provided ask the user interactively",
*/

    while ((ch = getopt(argc, argv, "a:s:f:t:d:k:AT:h")) != -1)
	{
		switch  (ch)
		{
        case 'a':
			result = parse_algorithm(optarg, &algorithm);
			if (result)
				goto loser;
			break;
        case 's':
			keySizeInBits = atoi(optarg);
			break;
		case 'k':
			keychainName = optarg;
			break;
		case 'A':
			always_allow = TRUE;
			break;
		case 'T':
		{
			if (!trusted_list)
			{
				trusted_list = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
			}

			if (optarg[0])
			{
				SecTrustedApplicationRef app = NULL;
				status = SecTrustedApplicationCreateFromPath(optarg, &app);
				if (status)
				{
					sec_error("SecTrustedApplicationCreateFromPath %s: %s", optarg, sec_errstr(status));
					result = 1;
					goto loser;
				}

				CFArrayAppendValue(trusted_list, app);
				CFRelease(app);
				break;
			}
		}
		case 'f':
			 result = parse_time(optarg, &from_time);
			if (result)
				goto loser;
			break;
		case 't':
			 result = parse_time(optarg, &to_time);
			if (result)
				goto loser;
			break;
		case 'd':
			days = atof(optarg);
			if (days < 1)
			{
				result = 2;
				goto loser;
			}
			from_time = CFAbsoluteTimeGetCurrent();
			to_time = from_time + days * 86400.0;
			break;
		case '?':
		default:
			return 2; /* @@@ Return 2 triggers usage message. */
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 1)
	{
		if (*argv[0] == '\0')
		{
			result = 2;
			goto loser;
		}
		description  = CFStringCreateWithCString(NULL, argv[0], kCFStringEncodingUTF8);
	}
	else if (argc != 0)
		return 2;
	else
		description = CFStringCreateWithCString(NULL, "<key>", kCFStringEncodingUTF8);

	if (always_allow)
	{
		status = SecAccessCreate(description, NULL, &access);
		if (status)
		{
			sec_perror("SecAccessCreate", status);
			result = 1;
		}
		// @@@ Make the acl always allow now.
	}
	else
	{
		status = SecAccessCreate(description, trusted_list, &access);
		if (status)
		{
			sec_perror("SecAccessCreate", status);
			result = 1;
		}
	}

	if (result)
		goto loser;

	result = do_csr_create(keychainName, access, algorithm, keySizeInBits, from_time, to_time);

loser:
	if (description)
		CFRelease(description);
	if (trusted_list)
		CFRelease(trusted_list);
	if (access)
		CFRelease(access);

	return result;
}
#endif
