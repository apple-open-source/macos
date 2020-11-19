/*
 * Copyright (c) 2003-2010,2012,2014-2019 Apple Inc. All Rights Reserved.
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
 * identity_find.c
 */

#import <Foundation/Foundation.h>

#include "identity_find.h"
#include "keychain_utilities.h"
#include "trusted_cert_utils.h"
#include "security_tool.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <Security/cssmtype.h>
#include <Security/oidsalg.h>
#include <Security/SecCertificate.h>
#include <Security/SecIdentity.h>
#include <Security/SecIdentitySearch.h>
#include <Security/SecPolicySearch.h>
#include <Security/SecTrust.h>
#include <Security/SecItem.h>
#include <CommonCrypto/CommonDigest.h>
#include <utilities/SecCFWrappers.h>

// cssmErrorString
#include <Security/SecBasePriv.h>
// SecCertificateInferLabel, SecDigestGetData, SecCertificateCopyPublicKeySHA{1,256}Digest
#include <Security/SecCertificatePriv.h>
// SecIdentitySearchCreateWithPolicy
#include <Security/SecIdentitySearchPriv.h>
// kSecAttrCanSignRecover, kSecAttrCanVerifyRecover
#include <Security/SecItemPriv.h>

static bool checkKeyUsageOperation(CSSM_KEYUSE keyUse, CSSM_KEYUSE keyOp, NSDictionary *keyAttrs, id secKeyOp) {
    if (keyUse & keyOp) {
        return [keyAttrs[secKeyOp] isEqual:@YES];
    }
    return true;
}

static bool checkKeyUsage(CSSM_KEYUSE keyUse, NSDictionary *keyAttrs) {
    CSSM_KEYUSE keyOps[] = { CSSM_KEYUSE_ENCRYPT, CSSM_KEYUSE_DECRYPT, CSSM_KEYUSE_SIGN, CSSM_KEYUSE_VERIFY, CSSM_KEYUSE_SIGN_RECOVER, CSSM_KEYUSE_VERIFY_RECOVER, CSSM_KEYUSE_WRAP, CSSM_KEYUSE_UNWRAP, CSSM_KEYUSE_DERIVE };
    id secKeyOps[] = { (id)kSecAttrCanEncrypt, (id)kSecAttrCanDecrypt, (id)kSecAttrCanSign, (id)kSecAttrCanVerify, (id)kSecAttrCanSignRecover, (id)kSecAttrCanVerifyRecover, (id)kSecAttrCanWrap, (id)kSecAttrCanUnwrap, (id)kSecAttrCanDerive };

    if (keyUse & CSSM_KEYUSE_ANY) {
        for (size_t i = 0; i < sizeof(keyOps) / sizeof(CSSM_KEYUSE); ++i) {
            keyUse |= keyOps[i];
        }
    }
    for (size_t i = 0; i < sizeof(keyOps) / sizeof(CSSM_KEYUSE); ++i) {
        if (!checkKeyUsageOperation(keyUse, keyOps[i], keyAttrs, secKeyOps[i]))
            return false;
    }
    return true;
}

SecIdentityRef
CopyMatchingIdentity(CFTypeRef keychainOrArray,
                     const char *identity,
                     const char *hash,
                     CSSM_KEYUSE keyUsage)
{
	id identityRef;
	// check input hash string and convert to data
	NSData *hashData = nil;
	if (hash) {
        hashData = (__bridge_transfer NSData *)CFDataCreateFromHexString(kCFAllocatorDefault, (__bridge CFStringRef)[NSString stringWithUTF8String:hash]);
	}

	// filter candidates against the hash (or the name, if no hash provided)
	NSString *matchName = (identity) ? [NSString stringWithUTF8String:identity] : nil;
	Boolean exactMatch = FALSE;

    NSDictionary *query;
    if (keychainOrArray) {
        id keychainArray;
        if (![(__bridge id)keychainOrArray isKindOfClass:[NSArray class]]) {
            keychainArray = @[(__bridge id)keychainOrArray];
        } else {
            keychainArray = (__bridge id)keychainOrArray;
        }
        query = @{ (id)kSecClass : (id)kSecClassIdentity,
                   (id)kSecMatchLimit : (id)kSecMatchLimitAll,
                   (id)kSecMatchSearchList : keychainArray,
                   (id)kSecReturnRef : @YES };
    } else {
        query = @{ (id)kSecClass : (id)kSecClassIdentity,
                   (id)kSecMatchLimit : (id)kSecMatchLimitAll,
                   (id)kSecReturnRef : @YES };
    }
    NSArray *identities;
    if (SecItemCopyMatching((CFDictionaryRef)query, (void *)&identities) != errSecSuccess) {
        return NULL;
    }

    for (id candidate in identities) {
		id cert;
		if (SecIdentityCopyCertificate((__bridge SecIdentityRef)candidate, (void *)&cert) != errSecSuccess)
			continue;
        if (keyUsage) {
            NSData *pubKeyHash = (__bridge_transfer NSData*)SecCertificateCopyPublicKeySHA1Digest((__bridge SecCertificateRef)cert);
            NSDictionary *keyAttrs;
            if (SecItemCopyMatching((__bridge CFDictionaryRef)@{ (id)kSecClass : (id)kSecClassKey, (id)kSecAttrApplicationLabel : pubKeyHash, (id)kSecReturnAttributes : @YES }, (void *)&keyAttrs) != errSecSuccess)
                continue;
            if (!(checkKeyUsage(keyUsage, keyAttrs))) {
                continue;
            }
        }
		if (hashData) {
			NSData *certDataHash = nil;
			NSUInteger hashLength = [hashData length];
			if (hashLength == CC_SHA256_DIGEST_LENGTH) {
				certDataHash = (__bridge NSData *)SecCertificateCopySHA256Digest((__bridge SecCertificateRef)cert);
			} else if (hashLength == CC_SHA1_DIGEST_LENGTH) {
				certDataHash = (__bridge NSData *)SecCertificateGetSHA1Digest((__bridge SecCertificateRef)cert);
			}
			BOOL matched = [hashData isEqual:certDataHash];
			certDataHash = nil; // ARC releases the hash
			if (matched) {
				identityRef = candidate;
				break;
			}
		} else {
			// copy certificate name
			NSString *commonName;
			if ((SecCertificateCopyCommonName((__bridge SecCertificateRef)cert, (void *)&commonName) != errSecSuccess) || commonName == nil) {
				continue; // no name, so no match is possible
			}

			if (identity && !strcmp(identity, "*")) {	// special case: means "just take the first one"
				sec_error("Using identity \"%s\"", [commonName UTF8String]);
				identityRef = candidate;
				break;
			}
			CFRange find = { kCFNotFound, 0 };
			if (commonName && matchName)
				find = CFStringFind((__bridge CFStringRef)commonName, (__bridge CFStringRef)matchName, kCFCompareCaseInsensitive | kCFCompareNonliteral);
			Boolean isExact = (find.location == 0 && find.length == (CFIndex)commonName.length);
			if (find.location == kCFNotFound) {
				continue; // no match
			}
			if (identityRef) {	// got two matches
				if (exactMatch && !isExact)	{	// prior is better; ignore this one
					continue;
				}
				if (exactMatch == isExact) {	// same class of match
					if ([identityRef isEqual:candidate]) {	// identities have same cert
						continue;
					}
					// ambiguity - must fail
					sec_error("\"%s\" is ambiguous, matches more than one certificate", identity);
                    identityRef = nil;
					break;
				}
			}
			identityRef = candidate;
			exactMatch = isExact;
		}
	}

    return identityRef?(__bridge_retained SecIdentityRef)identityRef:NULL;
}

static void printIdentity(SecIdentityRef identity, SecPolicyRef policy, int ordinalValue)
{
	OSStatus status;
	Boolean printHash = TRUE, printName = TRUE;
	SecCertificateRef cert = NULL;

	status = SecIdentityCopyCertificate(identity, &cert);
	if (!status)
	{
		CSSM_DATA certData = { 0, nil };
		(void) SecCertificateGetData(cert, &certData);
		fprintf(stdout, "%3d) ", ordinalValue);
		if (printHash) {
			uint8 sha1_hash[20];
			CSSM_DATA digest;
			digest.Length = sizeof(sha1_hash);
			digest.Data = sha1_hash;
			if (SecDigestGetData(CSSM_ALGID_SHA1, &digest, &certData) == CSSM_OK) {
				unsigned int i;
				size_t len = digest.Length;
				uint8 *cp = digest.Data;
				for(i=0; i<len; i++) {
					fprintf(stdout, "%02X", ((unsigned char *)cp)[i]);
				}
			} else {
				fprintf(stdout, "!----- unable to get SHA-1 digest -----!");
			}
		}
		if (printName) {
			char *nameBuf = NULL;
			CFStringRef nameRef = NULL;
			status = SecCertificateInferLabel(cert, &nameRef);
			CFIndex nameLen = (nameRef) ? CFStringGetLength(nameRef) : 0;
			if (nameLen > 0) {
				CFIndex bufLen = 1 + CFStringGetMaximumSizeForEncoding(nameLen, kCFStringEncodingUTF8);
				nameBuf = (char *)malloc(bufLen);
				if (!CFStringGetCString(nameRef, nameBuf, bufLen-1, kCFStringEncodingUTF8))
					nameBuf[0]=0;
			}
			fprintf(stdout, " \"%s\"", (nameBuf && nameBuf[0] != 0) ? nameBuf : "<unknown>");
			if (nameBuf)
				free(nameBuf);
			safe_CFRelease(&nameRef);
		}

		// Default to X.509 Basic if no policy was specified
		if (!policy) {
			SecPolicySearchRef policySearch = NULL;
			if (SecPolicySearchCreate(CSSM_CERT_X_509v3, &CSSMOID_APPLE_X509_BASIC, NULL, &policySearch)==noErr) {
				SecPolicySearchCopyNext(policySearch, &policy);
			}
			safe_CFRelease(&policySearch);
		} else {
			CFRetain(policy);
		}

		// Create the trust reference, given policy and certificates
		SecTrustRef trust = nil;
		SecTrustResultType trustResult = kSecTrustResultInvalid;
		OSStatus trustResultCode = noErr;
		CFMutableArrayRef certificates = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		if (certificates) {
			CFArrayAppendValue(certificates, cert);
		}
		status = SecTrustCreateWithCertificates((CFArrayRef)certificates, policy, &trust);
		if (!status) {
			status = SecTrustEvaluate(trust, &trustResult);
		}
		if (trustResult != kSecTrustResultInvalid) {
			status = SecTrustGetCssmResultCode(trust, &trustResultCode);
		}
		if (trustResultCode != noErr) {
			fprintf(stdout, " (%s)\n", 	cssmErrorString(trustResultCode));
		} else {
			fprintf(stdout, "\n");
		}
		safe_CFRelease(&trust);
		safe_CFRelease(&policy);
		safe_CFRelease(&certificates);
	}
	safe_CFRelease(&cert);
    (void) status;
}

static void
do_identity_search_with_policy(CFTypeRef keychainOrArray,
							   const char *name,
							   const CSSM_OID* oidPtr,
							   CSSM_KEYUSE keyUsage,
							   Boolean client,
							   Boolean validOnly)
{
	// set up SMIME options with provided data
	CE_KeyUsage ceKeyUsage = 0;
	if (keyUsage & CSSM_KEYUSE_SIGN) ceKeyUsage |= CE_KU_DigitalSignature;
	if (keyUsage & CSSM_KEYUSE_ENCRYPT) ceKeyUsage |= CE_KU_KeyEncipherment;
	CSSM_APPLE_TP_SMIME_OPTIONS smimeOpts = {
		CSSM_APPLE_TP_SMIME_OPTS_VERSION,				// Version
		ceKeyUsage,										// IntendedUsage
		name ? (uint32) strlen(name) : 0,				// SenderEmailLen
		name											// SenderEmail
	};
	CSSM_DATA smimeValue = { sizeof(smimeOpts), (uint8*)&smimeOpts };

	// set up SSL options with provided data
	CSSM_APPLE_TP_SSL_OPTIONS sslOpts = {
		CSSM_APPLE_TP_SSL_OPTS_VERSION,					// Version
		(name && !client) ? (uint32) strlen(name) : 0,	// ServerNameLen
		(client) ? NULL : name,							// ServerName
		(client) ? CSSM_APPLE_TP_SSL_CLIENT : 0			// Flags
	};
	CSSM_DATA sslValue = { sizeof(sslOpts), (uint8*)&sslOpts };

	// get a policy ref for the specified policy OID
	OSStatus status = noErr;
	SecPolicyRef policy = NULL;
	SecPolicySearchRef policySearch = NULL;
	status = SecPolicySearchCreate(CSSM_CERT_X_509v3, oidPtr, NULL, &policySearch);
	if (!status)
		status = SecPolicySearchCopyNext(policySearch, &policy);

	CSSM_DATA *policyValue = NULL;
	const char *policyName = "<unknown>";

	if (compareOids(oidPtr, &CSSMOID_APPLE_TP_SMIME)) {
		policyName = "S/MIME";
		policyValue = &smimeValue;
	}
	else if (compareOids(oidPtr, &CSSMOID_APPLE_TP_SSL)) {
		if (client)
			policyName = "SSL (client)";
		else
			policyName = "SSL (server)";
		policyValue = &sslValue;
	}
	else if (compareOids(oidPtr, &CSSMOID_APPLE_TP_EAP)) {
		policyName = "EAP";
	}
	else if (compareOids(oidPtr, &CSSMOID_APPLE_TP_IP_SEC)) {
		policyName = "IPsec";
	}
	else if (compareOids(oidPtr, &CSSMOID_APPLE_TP_ICHAT)) {
		policyName = "iChat";
	}
	else if (compareOids(oidPtr, &CSSMOID_APPLE_TP_CODE_SIGNING)) {
		policyName = "Code Signing";
	}
	else if (compareOids(oidPtr, &CSSMOID_APPLE_X509_BASIC)) {
		policyName = "X.509 Basic";
	}
	else if (compareOids(oidPtr, &CSSMOID_APPLE_TP_MACAPPSTORE_RECEIPT)) {
		policyName = "Mac App Store Receipt";
	}
	else if (compareOids(oidPtr, &CSSMOID_APPLE_TP_APPLEID_SHARING)) {
		policyName = "AppleID Sharing";
	}

	// set the policy's value, if there is one (this is specific to certain policies)
	if (policy && policyValue)
		status = SecPolicySetValue(policy, policyValue);

	CFStringRef idStr = (name) ? CFStringCreateWithCStringNoCopy(NULL, name, kCFStringEncodingUTF8, kCFAllocatorNull) : NULL;
	SecIdentitySearchRef searchRef = NULL;
	int identityCount = 0;

	if (!validOnly) {
		// create an identity search, specifying all identities (i.e. returnOnlyValidIdentities=FALSE)
		// this should return all identities which match the policy and key usage, regardless of validity
		fprintf(stdout, "\nPolicy: %s\n", policyName);
		fprintf(stdout, "  Matching identities\n");
		status = SecIdentitySearchCreateWithPolicy(policy, idStr, keyUsage, keychainOrArray, FALSE, &searchRef);
		if (!status)
		{
			SecIdentityRef identityRef = NULL;
			while (SecIdentitySearchCopyNext(searchRef, &identityRef) == noErr)
			{
				identityCount++;
				printIdentity(identityRef, policy, identityCount);
				safe_CFRelease(&identityRef);
			}
			safe_CFRelease(&searchRef);
		}
		fprintf(stdout, "     %d identities found\n\n", identityCount);
	}

	// create a second identity search, specifying only valid identities (i.e. returnOnlyValidIdentities=TRUE)
	// this should return only valid identities for the policy.
	identityCount = 0;
	if (!validOnly) {
		fprintf(stdout, "  Valid identities only\n");
	}
	status = SecIdentitySearchCreateWithPolicy(policy, idStr, keyUsage, keychainOrArray, TRUE, &searchRef);
	if (!status)
	{
		SecIdentityRef identityRef = NULL;
		while (SecIdentitySearchCopyNext(searchRef, &identityRef) == noErr)
		{
			identityCount++;
			printIdentity(identityRef, policy, identityCount);
			safe_CFRelease(&identityRef);
		}
		safe_CFRelease(&searchRef);
	}
	fprintf(stdout, "     %d valid identities found\n", identityCount);

	safe_CFRelease(&idStr);
	safe_CFRelease(&policy);
	safe_CFRelease(&policySearch);
}

static void
do_system_identity_search(CFStringRef domain)
{
    SecIdentityRef identity = NULL;
    OSStatus status = SecIdentityCopySystemIdentity(domain, &identity, NULL);
    if (CFEqual(domain, kSecIdentityDomainDefault)) {
    	fprintf(stdout, "\n  System default identity\n");
    } else if (CFEqual(domain, kSecIdentityDomainKerberosKDC)) {
    	fprintf(stdout, "\n  System Kerberos KDC identity\n");
    }
    if (!status && identity) {
        SecPolicyRef policy = NULL;
        SecPolicySearchRef policySearch = NULL;
        if (SecPolicySearchCreate(CSSM_CERT_X_509v3, &CSSMOID_APPLE_X509_BASIC, NULL, &policySearch) == noErr) {
            if (SecPolicySearchCopyNext(policySearch, &policy) == noErr) {
                printIdentity(identity, policy, 1);
                CFRelease(policy);
            }
            safe_CFRelease(&policySearch);
        }
        safe_CFRelease(&identity);
    }
}

static int
do_find_identities(CFTypeRef keychainOrArray, const char *name, unsigned int policyFlags, Boolean validOnly)
{
	int result = 0;

	if (name) {
		fprintf(stdout, "Looking for identities matching \"%s\"\n", name);
    }
	if (policyFlags & (1 << 0))
		do_identity_search_with_policy(keychainOrArray, name, &CSSMOID_APPLE_TP_SSL, CSSM_KEYUSE_SIGN, TRUE, validOnly);
	if (policyFlags & (1 << 1))
		do_identity_search_with_policy(keychainOrArray, name, &CSSMOID_APPLE_TP_SSL, CSSM_KEYUSE_SIGN, FALSE, validOnly);
	if (policyFlags & (1 << 2))
		do_identity_search_with_policy(keychainOrArray, name, &CSSMOID_APPLE_TP_SMIME, CSSM_KEYUSE_SIGN, TRUE, validOnly);
	if (policyFlags & (1 << 3))
		do_identity_search_with_policy(keychainOrArray, name, &CSSMOID_APPLE_TP_EAP, CSSM_KEYUSE_SIGN, TRUE, validOnly);
	if (policyFlags & (1 << 4))
		do_identity_search_with_policy(keychainOrArray, name, &CSSMOID_APPLE_TP_IP_SEC, CSSM_KEYUSE_SIGN, TRUE, validOnly);
	if (policyFlags & (1 << 5))
		do_identity_search_with_policy(keychainOrArray, name, &CSSMOID_APPLE_TP_ICHAT, CSSM_KEYUSE_SIGN, TRUE, validOnly);
	if (policyFlags & (1 << 6))
		do_identity_search_with_policy(keychainOrArray, name, &CSSMOID_APPLE_TP_CODE_SIGNING, CSSM_KEYUSE_SIGN, TRUE, validOnly);
	if (policyFlags & (1 << 7))
		do_identity_search_with_policy(keychainOrArray, name, &CSSMOID_APPLE_X509_BASIC, CSSM_KEYUSE_SIGN, TRUE, validOnly);

	if (policyFlags & (1 << 8))
		do_system_identity_search(kSecIdentityDomainDefault);
	if (policyFlags & (1 << 9))
		do_system_identity_search(kSecIdentityDomainKerberosKDC);
	if (policyFlags & (1 << 10))
		do_identity_search_with_policy(keychainOrArray, name, &CSSMOID_APPLE_TP_APPLEID_SHARING, CSSM_KEYUSE_SIGN, FALSE, validOnly);
	if (policyFlags & (1 << 11))
		do_identity_search_with_policy(keychainOrArray, name, &CSSMOID_APPLE_TP_MACAPPSTORE_RECEIPT, CSSM_KEYUSE_SIGN, TRUE, validOnly);

	return result;
}

int
keychain_find_identity(int argc, char * const *argv)
{
	int ch, result = 0;
	unsigned int policyFlags = 0;
	const char *name = NULL;
	Boolean validOnly = FALSE;
	CFTypeRef keychainOrArray = NULL;

	/*
	 *	"    -p  Specify policy to evaluate (multiple -p options are allowed)\n"
	 *	"    -s  Specify optional policy-specific string (e.g. a DNS hostname for SSL,\n"
	 *  "        or RFC822 email address for S/MIME)\n"
	 *	"    -v  Show valid identities only (default is to show all identities)\n"
	 */

	while ((ch = getopt(argc, argv, "hp:s:v")) != -1)
	{
		switch  (ch)
		{
			case 'p':
				if (optarg != NULL) {
					if      (!strcmp(optarg, "ssl-client"))
						policyFlags |= 1 << 0;
					else if (!strcmp(optarg, "ssl-server"))
						policyFlags |= 1 << 1;
					else if (!strcmp(optarg, "smime"))
						policyFlags |= 1 << 2;
					else if (!strcmp(optarg, "eap"))
						policyFlags |= 1 << 3;
					else if (!strcmp(optarg, "ipsec"))
						policyFlags |= 1 << 4;
					else if (!strcmp(optarg, "ichat"))
						policyFlags |= 1 << 5;
					else if (!strcmp(optarg, "codesigning"))
						policyFlags |= 1 << 6;
					else if (!strcmp(optarg, "basic"))
						policyFlags |= 1 << 7;
					else if (!strcmp(optarg, "sys-default"))
						policyFlags |= 1 << 8;
					else if (!strcmp(optarg, "sys-kerberos-kdc"))
						policyFlags |= 1 << 9;
					else if (!strcmp(optarg, "appleID"))
						policyFlags |= 1 << 10;
					else if (!strcmp(optarg, "macappstore"))
						policyFlags |= 1 << 11;
					else {
						result = SHOW_USAGE_MESSAGE;
						goto cleanup;
					}
				}
				break;
			case 's':
				name = optarg;
				break;
			case 'v':
				validOnly = TRUE;
				break;
			case '?':
			default:
				result = 2; /* @@@ Return 2 triggers usage message. */
				goto cleanup;
		}
	}

	if (!policyFlags)
		policyFlags |= 1 << 7; /* default to basic policy if none specified */

	argc -= optind;
	argv += optind;

    keychainOrArray = keychain_create_array(argc, argv);

	result = do_find_identities(keychainOrArray, name, policyFlags, validOnly);

cleanup:
	safe_CFRelease(&keychainOrArray);

	return result;
}

