/*
 * Copyright (c) 2008-2016 Apple Inc. All Rights Reserved.
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
 */

/*
 * SecPolicyServer.c - Trust policies dealing with certificate revocation.
 */

#include <securityd/SecPolicyServer.h>
#include <Security/SecPolicyInternal.h>
#include <Security/SecPolicyPriv.h>
#include <Security/SecTask.h>
#include <utilities/SecIOFormat.h>
#include <securityd/asynchttp.h>
#include <securityd/policytree.h>
#include <securityd/nameconstraints.h>
#include <CoreFoundation/CFTimeZone.h>
#include <wctype.h>
#include <libDER/oidsPriv.h>
#include <CoreFoundation/CFNumber.h>
#include <Security/SecCertificateInternal.h>
#include <AssertMacros.h>
#include <utilities/debugging.h>
#include <utilities/SecInternalReleasePriv.h>
#include <security_asn1/SecAsn1Coder.h>
#include <security_asn1/ocspTemplates.h>
#include <security_asn1/oidsalg.h>
#include <security_asn1/oidsocsp.h>
#include <CommonCrypto/CommonDigest.h>
#include <Security/SecFramework.h>
#include <Security/SecPolicyInternal.h>
#include <Security/SecTrustPriv.h>
#include <Security/SecTrustSettingsPriv.h>
#include <Security/SecInternal.h>
#include <Security/SecKeyPriv.h>
#include <Security/SecTask.h>
#include <CFNetwork/CFHTTPMessage.h>
#include <CFNetwork/CFHTTPStream.h>
#include <SystemConfiguration/SCDynamicStoreCopySpecific.h>
#include <asl.h>
#include <securityd/SecOCSPRequest.h>
#include <securityd/SecOCSPResponse.h>
#include <securityd/asynchttp.h>
#include <securityd/SecTrustServer.h>
#include <securityd/SecOCSPCache.h>
#include <securityd/SecRevocationDb.h>
#include <securityd/SecTrustLoggingServer.h>
#include <utilities/array_size.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/SecAppleAnchorPriv.h>
#include "OTATrustUtilities.h"
#include "personalization.h"
#include <sys/codesign.h>

#if !TARGET_OS_IPHONE
#include <Security/SecTaskPriv.h>
#endif

/* Set this to 1 to dump the ocsp responses received in DER form in /tmp. */
#ifndef DUMP_OCSPRESPONSES
#define DUMP_OCSPRESPONSES  0
#endif

#if DUMP_OCSPRESPONSES

#include <unistd.h>
#include <fcntl.h>

static void secdumpdata(CFDataRef data, const char *name) {
    int fd = open(name, O_CREAT | O_WRONLY | O_TRUNC, 0666);
    write(fd, CFDataGetBytePtr(data), CFDataGetLength(data));
    close(fd);
}

#endif


/********************************************************
 ****************** SecPolicy object ********************
 ********************************************************/

static CFMutableDictionaryRef gSecPolicyLeafCallbacks = NULL;
static CFMutableDictionaryRef gSecPolicyPathCallbacks = NULL;

static CFArrayRef SecPolicyAnchorDigestsForEVPolicy(const DERItem *policyOID)
{
	CFArrayRef result = NULL;
	SecOTAPKIRef otapkiRef = SecOTAPKICopyCurrentOTAPKIRef();
	if (NULL == otapkiRef)
	{
		return result;
	}

	CFDictionaryRef evToPolicyAnchorDigest = SecOTAPKICopyEVPolicyToAnchorMapping(otapkiRef);
	CFRelease(otapkiRef);

    if (NULL == evToPolicyAnchorDigest)
    {
        return result;
    }

    CFArrayRef roots = NULL;
    CFStringRef oid = SecDERItemCopyOIDDecimalRepresentation(kCFAllocatorDefault, policyOID);
    if (oid && evToPolicyAnchorDigest)
	{
        result = (CFArrayRef)CFDictionaryGetValue(evToPolicyAnchorDigest, oid);
		if (roots && CFGetTypeID(result) != CFArrayGetTypeID())
		{
            secerror("EVRoot.plist has non array value");
            result = NULL;
        }
        CFRelease(oid);
    }
    CFReleaseSafe(evToPolicyAnchorDigest);
    return result;
}


static bool SecPolicyIsEVPolicy(const DERItem *policyOID) {
    return SecPolicyAnchorDigestsForEVPolicy(policyOID);
}

static bool SecPolicyRootCACertificateIsEV(SecCertificateRef certificate,
    policy_set_t valid_policies) {
    CFDictionaryRef keySizes = NULL;
    CFNumberRef rsaSize = NULL, ecSize = NULL;
    bool isEV = false;
    /* Ensure that this certificate is a valid anchor for one of the
       certificate policy oids specified in the leaf. */
    CFDataRef digest = SecCertificateGetSHA1Digest(certificate);
    policy_set_t ix;
    bool good_ev_anchor = false;
    for (ix = valid_policies; ix; ix = ix->oid_next) {
        CFArrayRef digests = SecPolicyAnchorDigestsForEVPolicy(&ix->oid);
        if (digests && CFArrayContainsValue(digests,
            CFRangeMake(0, CFArrayGetCount(digests)), digest)) {
            secdebug("ev", "found anchor for policy oid");
            good_ev_anchor = true;
            break;
        }
    }
    require_action_quiet(good_ev_anchor, notEV, secnotice("ev", "anchor not in plist"));

    CFAbsoluteTime october2006 = 178761600;
    if (SecCertificateNotValidBefore(certificate) >= october2006) {
        require_action_quiet(SecCertificateVersion(certificate) >= 3, notEV,
                             secnotice("ev", "Anchor issued after October 2006 and is not v3"));
    }
    if (SecCertificateVersion(certificate) >= 3
        && SecCertificateNotValidBefore(certificate) >= october2006) {
        const SecCEBasicConstraints *bc = SecCertificateGetBasicConstraints(certificate);
        require_action_quiet(bc && bc->isCA == true, notEV,
                             secnotice("ev", "Anchor has invalid basic constraints"));
        SecKeyUsage ku = SecCertificateGetKeyUsage(certificate);
        require_action_quiet((ku & (kSecKeyUsageKeyCertSign | kSecKeyUsageCRLSign))
            == (kSecKeyUsageKeyCertSign | kSecKeyUsageCRLSign), notEV,
                             secnotice("ev", "Anchor has invalid key usage %u", ku));
    }

    /* At least RSA 2048 or ECC NIST P-256. */
    require_quiet(rsaSize = CFNumberCreateWithCFIndex(NULL, 2048), notEV);
    require_quiet(ecSize = CFNumberCreateWithCFIndex(NULL, 256), notEV);
    const void *keys[] = { kSecAttrKeyTypeRSA, kSecAttrKeyTypeEC };
    const void *values[] = { rsaSize, ecSize };
    require_quiet(keySizes = CFDictionaryCreate(NULL, keys, values, 2,
                                          &kCFTypeDictionaryKeyCallBacks,
                                          &kCFTypeDictionaryValueCallBacks), notEV);
    require_action_quiet(SecCertificateIsAtLeastMinKeySize(certificate, keySizes), notEV,
                         secnotice("ev", "Anchor's public key is too weak for EV"));

    isEV = true;

notEV:
    CFReleaseNull(rsaSize);
    CFReleaseNull(ecSize);
    CFReleaseNull(keySizes);
    return isEV;
}

static bool SecPolicySubordinateCACertificateCouldBeEV(SecCertificateRef certificate) {
    CFMutableDictionaryRef keySizes = NULL;
    CFNumberRef rsaSize = NULL, ecSize = NULL;
    bool isEV = false;

    const SecCECertificatePolicies *cp;
    cp = SecCertificateGetCertificatePolicies(certificate);
    require_action_quiet(cp && cp->numPolicies > 0, notEV,
                         secnotice("ev", "SubCA missing certificate policies"));
    CFArrayRef cdp = SecCertificateGetCRLDistributionPoints(certificate);
    require_action_quiet(cdp && CFArrayGetCount(cdp) > 0, notEV,
                         secnotice("ev", "SubCA missing CRLDP"));
    const SecCEBasicConstraints *bc = SecCertificateGetBasicConstraints(certificate);
    require_action_quiet(bc && bc->isCA == true, notEV,
                         secnotice("ev", "SubCA has invalid basic constraints"));
    SecKeyUsage ku = SecCertificateGetKeyUsage(certificate);
    require_action_quiet((ku & (kSecKeyUsageKeyCertSign | kSecKeyUsageCRLSign))
        == (kSecKeyUsageKeyCertSign | kSecKeyUsageCRLSign), notEV,
                         secnotice("ev", "SubCA has invalid key usage %u", ku));

    /* 6.1.5 Key Sizes */
    CFAbsoluteTime jan2011 = 315532800;
    CFAbsoluteTime jan2014 = 410227200;
    require_quiet(ecSize = CFNumberCreateWithCFIndex(NULL, 256), notEV);
    require_quiet(keySizes = CFDictionaryCreateMutable(NULL, 2, &kCFTypeDictionaryKeyCallBacks,
                                                 &kCFTypeDictionaryValueCallBacks), notEV);
    CFDictionaryAddValue(keySizes, kSecAttrKeyTypeEC, ecSize);
    if (SecCertificateNotValidBefore(certificate) < jan2011 ||
        SecCertificateNotValidAfter(certificate) < jan2014) {
        /* At least RSA 1024 or ECC NIST P-256. */
        require_quiet(rsaSize = CFNumberCreateWithCFIndex(NULL, 1024), notEV);
        CFDictionaryAddValue(keySizes, kSecAttrKeyTypeRSA, rsaSize);
        require_action_quiet(SecCertificateIsAtLeastMinKeySize(certificate, keySizes), notEV,
                             secnotice("ev", "SubCA's public key is too small for issuance before 2011 or expiration before 2014"));
    } else {
        /* At least RSA 2028 or ECC NIST P-256. */
        require_quiet(rsaSize = CFNumberCreateWithCFIndex(NULL, 2048), notEV);
        CFDictionaryAddValue(keySizes, kSecAttrKeyTypeRSA, rsaSize);
        require_action_quiet(SecCertificateIsAtLeastMinKeySize(certificate, keySizes), notEV,
                             secnotice("ev", "SubCA's public key is too small for issuance after 2010 or expiration after 2013"));
    }

    /* 7.1.3 Algorithm Object Identifiers */
    CFAbsoluteTime jan2016 = 473299200;
    if (SecCertificateNotValidBefore(certificate) > jan2016) {
        /* SHA-2 only */
        require_action_quiet(SecCertificateGetSignatureHashAlgorithm(certificate) > kSecSignatureHashAlgorithmSHA1,
                             notEV, secnotice("ev", "SubCA was issued with SHA-1 after 2015"));
    }

    isEV = true;

notEV:
    CFReleaseNull(rsaSize);
    CFReleaseNull(ecSize);
    CFReleaseNull(keySizes);
    return isEV;
}

bool SecPolicySubscriberCertificateCouldBeEV(SecCertificateRef certificate) {
    CFMutableDictionaryRef keySizes = NULL;
    CFNumberRef rsaSize = NULL, ecSize = NULL;
    bool isEV = false;

    /* 3. Subscriber Certificate. */

    /* (a) certificate Policies */
    const SecCECertificatePolicies *cp;
    cp = SecCertificateGetCertificatePolicies(certificate);
    require_quiet(cp && cp->numPolicies > 0, notEV);
    /* Now find at least one policy in here that has a qualifierID of id-qt 2
       and a policyQualifier that is a URI to the CPS and an EV policy OID. */
    uint32_t ix = 0;
    bool found_ev_anchor_for_leaf_policy = false;
    for (ix = 0; ix < cp->numPolicies; ++ix) {
        if (SecPolicyIsEVPolicy(&cp->policies[ix].policyIdentifier)) {
            found_ev_anchor_for_leaf_policy = true;
        }
    }
    require_quiet(found_ev_anchor_for_leaf_policy, notEV);

    /* (b) cRLDistributionPoint
       (c) authorityInformationAccess
            BRv1.3.4: MUST be present with OCSP Responder unless stapled response.
     */

    /* (d) basicConstraints
       If present, the cA field MUST be set false. */
    const SecCEBasicConstraints *bc = SecCertificateGetBasicConstraints(certificate);
    if (bc) {
        require_action_quiet(bc->isCA == false, notEV,
                             secnotice("ev", "Leaf has invalid basic constraints"));
    }

    /* (e) keyUsage. */
    SecKeyUsage ku = SecCertificateGetKeyUsage(certificate);
    if (ku) {
        require_action_quiet((ku & (kSecKeyUsageKeyCertSign | kSecKeyUsageCRLSign)) == 0, notEV,
                             secnotice("ev", "Leaf has invalid key usage %u", ku));
    }

#if 0
    /* The EV Cert Spec errata specifies this, though this is a check for SSL
       not specifically EV. */

    /* (e) extKeyUsage

Either the value id-kp-serverAuth [RFC5280] or id-kp-clientAuth [RFC5280] or both values MUST be present. Other values SHOULD NOT be present. */
    SecCertificateCopyExtendedKeyUsage(certificate);
#endif

    /* 6.1.5 Key Sizes */
    CFAbsoluteTime jan2014 = 410227200;
    require_quiet(ecSize = CFNumberCreateWithCFIndex(NULL, 256), notEV);
    require_quiet(keySizes = CFDictionaryCreateMutable(NULL, 2, &kCFTypeDictionaryKeyCallBacks,
                                                 &kCFTypeDictionaryValueCallBacks), notEV);
    CFDictionaryAddValue(keySizes, kSecAttrKeyTypeEC, ecSize);
    if (SecCertificateNotValidBefore(certificate) < jan2014) {
        /* At least RSA 1024 or ECC NIST P-256. */
        require_quiet(rsaSize = CFNumberCreateWithCFIndex(NULL, 1024), notEV);
        CFDictionaryAddValue(keySizes, kSecAttrKeyTypeRSA, rsaSize);
        require_action_quiet(SecCertificateIsAtLeastMinKeySize(certificate, keySizes), notEV,
                             secnotice("ev", "Leaf's public key is too small for issuance before 2014"));
    } else {
        /* At least RSA 2028 or ECC NIST P-256. */
        require_quiet(rsaSize = CFNumberCreateWithCFIndex(NULL, 2048), notEV);
        CFDictionaryAddValue(keySizes, kSecAttrKeyTypeRSA, rsaSize);
        require_action_quiet(SecCertificateIsAtLeastMinKeySize(certificate, keySizes), notEV,
                             secnotice("ev", "Leaf's public key is too small for issuance after 2013"));
    }

    /* 6.3.2 Validity Periods */
    CFAbsoluteTime jul2016 = 489024000;
    CFAbsoluteTime notAfter = SecCertificateNotValidAfter(certificate);
    CFAbsoluteTime notBefore = SecCertificateNotValidBefore(certificate);
    if (SecCertificateNotValidBefore(certificate) < jul2016) {
        /* Validity Period no greater than 60 months.
           60 months is no more than 5 years and 2 leap days. */
        CFAbsoluteTime maxPeriod = 60*60*24*(365*5+2);
        require_action_quiet(notAfter - notBefore <= maxPeriod, notEV,
                             secnotice("ev", "Leaf's validity period is more than 60 months"));
    } else {
        /* Validity Period no greater than 39 months.
            39 months is no more than 3 years, 2 31-day months,
            1 30-day month, and 1 leap day */
        CFAbsoluteTime maxPeriod = 60*60*24*(365*3+2*31+30+1);
        require_action_quiet(notAfter - notBefore <= maxPeriod, notEV,
                             secnotice("ev", "Leaf has validity period longer than 39 months and issued after 30 June 2016"));
    }

    /* 7.1.3 Algorithm Object Identifiers */
    CFAbsoluteTime jan2016 = 473299200;
    if (SecCertificateNotValidBefore(certificate) > jan2016) {
        /* SHA-2 only */
        require_action_quiet(SecCertificateGetSignatureHashAlgorithm(certificate) > kSecSignatureHashAlgorithmSHA1,
                notEV, secnotice("ev", "Leaf was issued with SHA-1 after 2015"));
    }

    isEV = true;

notEV:
    CFReleaseNull(rsaSize);
    CFReleaseNull(ecSize);
    CFReleaseNull(keySizes);
    return isEV;
}

/********************************************************
 **************** SecPolicy Callbacks *******************
 ********************************************************/
static void SecPolicyCheckCriticalExtensions(SecPVCRef pvc,
	CFStringRef key) {
}

static void SecPolicyCheckIdLinkage(SecPVCRef pvc,
	CFStringRef key) {
	CFIndex ix, count = SecPVCGetCertificateCount(pvc);
	CFDataRef parentSubjectKeyID = NULL;
	for (ix = count - 1; ix >= 0; --ix) {
		SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, ix);
		/* If the previous certificate in the chain had a SubjectKeyID,
		   make sure it matches the current certificates AuthorityKeyID. */
		if (parentSubjectKeyID) {
			/* @@@ According to RFC 2459 neither AuthorityKeyID nor
			   SubjectKeyID can be critical.  Currenty we don't check
			   for this. */
			CFDataRef authorityKeyID = SecCertificateGetAuthorityKeyID(cert);
			if (authorityKeyID) {
				if (!CFEqual(parentSubjectKeyID, authorityKeyID)) {
					/* AuthorityKeyID doesn't match issuers SubjectKeyID. */
					if (!SecPVCSetResult(pvc, key, ix, kCFBooleanFalse))
						return;
				}
			}
		}

		parentSubjectKeyID = SecCertificateGetSubjectKeyID(cert);
	}
}

static void SecPolicyCheckKeyUsage(SecPVCRef pvc,
	CFStringRef key) {
    SecCertificateRef leaf = SecPVCGetCertificateAtIndex(pvc, 0);
    SecPolicyRef policy = SecPVCGetPolicy(pvc);
    CFTypeRef xku = CFDictionaryGetValue(policy->_options, key);
    if (!SecPolicyCheckCertKeyUsage(leaf, xku)) {
        SecPVCSetResult(pvc, key, 0, kCFBooleanFalse);
    }
}

/* AUDIT[securityd](done):
   policy->_options is a caller provided dictionary, only its cf type has
   been checked.
 */
static void SecPolicyCheckExtendedKeyUsage(SecPVCRef pvc, CFStringRef key) {
    SecCertificateRef leaf = SecPVCGetCertificateAtIndex(pvc, 0);
    SecPolicyRef policy = SecPVCGetPolicy(pvc);
    CFTypeRef xeku = CFDictionaryGetValue(policy->_options, key);
    if (!SecPolicyCheckCertExtendedKeyUsage(leaf, xeku)){
        SecPVCSetResult(pvc, key, 0, kCFBooleanFalse);
    }
}

#if 0
static void SecPolicyCheckBasicContraintsCommon(SecPVCRef pvc,
	CFStringRef key, bool strict) {
	CFIndex ix, count = SecPVCGetCertificateCount(pvc);
	for (ix = 0; ix < count; ++ix) {
		SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, ix);
		const SecCEBasicConstraints *bc =
			SecCertificateGetBasicConstraints(cert);
		if (bc) {
			if (strict) {
				if (ix == 0) {
					/* Leaf certificate has basic constraints extension. */
					if (!SecPVCSetResult(pvc, key, ix, kCFBooleanFalse))
						return;
				} else if (!bc->critical) {
					/* Basic constraints extension is not marked critical. */
					if (!SecPVCSetResult(pvc, key, ix, kCFBooleanFalse))
						return;
				}
			}

			if (ix > 0 || count == 1) {
				if (!bc->isCA) {
					/* Non leaf certificate marked as isCA false. */
					if (!SecPVCSetResult(pvc, key, ix, kCFBooleanFalse))
						return;
				}

				if (bc->pathLenConstraintPresent) {
					if (bc->pathLenConstraint < (uint32_t)(ix - 1)) {
#if 0
						/* @@@ If a self signed certificate is issued by
						   another cert that is trusted, then we are supposed
						   to treat the self signed cert itself as the anchor
						   for path length purposes. */
						CFIndex ssix = SecCertificatePathSelfSignedIndex(path);
						if (ssix >= 0 && ix >= ssix) {
							/* It's ok if the pathLenConstraint isn't met for
							   certificates signing a self signed cert in the
							   chain. */
						} else
#endif
						{
							/* Path Length Constraint Exceeded. */
							if (!SecPVCSetResult(pvc, key, ix,
								kCFBooleanFalse))
								return;
						}
					}
				}
			}
		} else if (strict && ix > 0) {
			/* In strict mode all CA certificates *MUST* have a critical
			   basic constraints extension and the leaf certificate
			   *MUST NOT* have a basic constraints extension. */
			/* CA certificate is missing basicConstraints extension. */
			if (!SecPVCSetResult(pvc, key, ix, kCFBooleanFalse))
				return;
		}
	}
}
#endif

static void SecPolicyCheckBasicConstraints(SecPVCRef pvc,
	CFStringRef key) {
	//SecPolicyCheckBasicContraintsCommon(pvc, key, false);
}

static void SecPolicyCheckNonEmptySubject(SecPVCRef pvc,
	CFStringRef key) {
	CFIndex ix, count = SecPVCGetCertificateCount(pvc);
	for (ix = 0; ix < count; ++ix) {
		SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, ix);
		/* If the certificate has a subject, or
		   if it doesn't, and it's the leaf and not self signed,
		   and also has a critical subjectAltName extension it's valid. */
		if (!SecCertificateHasSubject(cert)) {
			if (ix == 0 && count > 1) {
				if (!SecCertificateHasCriticalSubjectAltName(cert)) {
					/* Leaf certificate with empty subject does not have
					   a critical subject alt name extension. */
					if (!SecPVCSetResult(pvc, key, ix, kCFBooleanFalse))
						return;
				}
			} else {
				/* CA certificate has empty subject. */
				if (!SecPVCSetResult(pvc, key, ix, kCFBooleanFalse))
					return;
			}
		}
	}
}

static void SecPolicyCheckQualifiedCertStatements(SecPVCRef pvc,
	CFStringRef key) {
}

/* Compare hostname suffix to domain name.
   This function does not process wildcards, and allows hostname to match
   any subdomain level of the provided domain.

   To match, the last domain length chars of hostname must equal domain,
   and the character immediately preceding domain in hostname (if any)
   must be a dot. This means that domain 'bar.com' will match hostname
   values 'host.bar.com' or 'host.sub.bar.com', but not 'host.foobar.com'.

   Characters in each string are converted to lowercase for the comparison.
   Trailing '.' characters in both names will be ignored.

   Returns true on match, else false.
 */
static bool SecDomainSuffixMatch(CFStringRef hostname, CFStringRef domain) {
    CFStringInlineBuffer hbuf = {}, dbuf = {};
	UniChar hch, dch;
	CFIndex hix, dix,
		hlength = CFStringGetLength(hostname),
		dlength = CFStringGetLength(domain);
	CFRange hrange = { 0, hlength }, drange = { 0, dlength };
	CFStringInitInlineBuffer(hostname, &hbuf, hrange);
	CFStringInitInlineBuffer(domain, &dbuf, drange);

	if((hlength == 0) || (dlength == 0)) {
		/* trivial case with at least one empty name */
		return (hlength == dlength) ? true : false;
	}

	/* trim off trailing dots */
	hch = CFStringGetCharacterFromInlineBuffer(&hbuf, hlength-1);
	dch = CFStringGetCharacterFromInlineBuffer(&dbuf, dlength-1);
	if(hch == '.') {
		hrange.length = --hlength;
	}
	if(dch == '.') {
		drange.length = --dlength;
	}

	/* trim off leading dot in suffix, if present */
	dch = CFStringGetCharacterFromInlineBuffer(&dbuf, 0);
	if((dlength > 0) && (dch == '.')) {
		drange.location++;
		drange.length = --dlength;
	}

	if(hlength < dlength) {
		return false;
	}

	/* perform case-insensitive comparison of domain suffix */
	for (hix = (hlength-dlength),
		 dix = drange.location; dix < drange.length; dix++) {
		hch = CFStringGetCharacterFromInlineBuffer(&hbuf, hix);
		dch = CFStringGetCharacterFromInlineBuffer(&dbuf, dix);
		if (towlower(hch) != towlower(dch)) {
			return false;
		}
	}

	/* require a dot prior to domain suffix, unless hostname == domain */
	if(hlength > dlength) {
		hch = CFStringGetCharacterFromInlineBuffer(&hbuf, (hlength-(dlength+1)));
		if(hch != '.') {
			return false;
		}
	}

	return true;
}

#define kSecPolicySHA1Size 20
static const UInt8 kAppleCorpCASHA1[kSecPolicySHA1Size] = {
    0xA1, 0x71, 0xDC, 0xDE, 0xE0, 0x8B, 0x1B, 0xAE, 0x30, 0xA1,
    0xAE, 0x6C, 0xC6, 0xD4, 0x03, 0x3B, 0xFD, 0xEF, 0x91, 0xCE
};

/* Check whether hostname is in a particular set of allowed domains.
   Returns true if OK, false if not allowed.
 */
static bool SecPolicyCheckDomain(SecPVCRef pvc, CFStringRef hostname)
{
	CFIndex count = SecPVCGetCertificateCount(pvc);
	SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, count - 1);
	CFDataRef anchorSHA1 = SecCertificateGetSHA1Digest(cert);

	/* is this chain anchored by kAppleCorpCASHA1? */
	CFDataRef corpSHA1 = CFDataCreateWithBytesNoCopy(NULL,
	    kAppleCorpCASHA1, kSecPolicySHA1Size, kCFAllocatorNull);
	bool isCorpSHA1 = (corpSHA1 && CFEqual(anchorSHA1, corpSHA1));
	CFReleaseSafe(corpSHA1);
	if (isCorpSHA1) {
		/* limit hostname to specified domains */
		const CFStringRef dnlist[] = {
		    CFSTR("apple.com"),
			CFSTR("icloud.com"),
		};
		unsigned int idx, dncount=2;
		for (idx = 0; idx < dncount; idx++) {
			if (SecDomainSuffixMatch(hostname, dnlist[idx])) {
				return true;
			}
		}
		return false;
	}
	/* %%% other CA pinning checks TBA */

	return true;
}

/* AUDIT[securityd](done):
   policy->_options is a caller provided dictionary, only its cf type has
   been checked.
 */
static void SecPolicyCheckSSLHostname(SecPVCRef pvc,
	CFStringRef key) {
	/* @@@ Consider what to do if the caller passes in no hostname.  Should
	   we then still fail if the leaf has no dnsNames or IPAddresses at all? */
	SecPolicyRef policy = SecPVCGetPolicy(pvc);
	CFStringRef hostName = (CFStringRef)
		CFDictionaryGetValue(policy->_options, key);
    if (!isString(hostName)) {
        /* @@@ We can't return an error here and making the evaluation fail
           won't help much either. */
        return;
    }

	SecCertificateRef leaf = SecPVCGetCertificateAtIndex(pvc, 0);
    bool dnsMatch = SecPolicyCheckCertSSLHostname(leaf, hostName);

	if (!dnsMatch) {
		/* Hostname mismatch or no hostnames found in certificate. */
		SecPVCSetResult(pvc, key, 0, kCFBooleanFalse);
	}
	else if (!SecPolicyCheckDomain(pvc, hostName)) {
		/* Hostname match, but domain not allowed for this CA */
		SecPVCSetResult(pvc, key, 0, kCFBooleanFalse);
	}

    if ((dnsMatch || pvc->details)
        && SecPolicySubscriberCertificateCouldBeEV(leaf)) {
        secdebug("policy", "enabling optionally_ev");
        pvc->optionally_ev = true;
    }

}

/* AUDIT[securityd](done):
 policy->_options is a caller provided dictionary, only its cf type has
 been checked.
 */
static void SecPolicyCheckEmail(SecPVCRef pvc, CFStringRef key) {
	SecPolicyRef policy = SecPVCGetPolicy(pvc);
	CFStringRef email = (CFStringRef)CFDictionaryGetValue(policy->_options, key);
    if (!isString(email)) {
        /* We can't return an error here and making the evaluation fail
         won't help much either. */
        return;
    }

	SecCertificateRef leaf = SecPVCGetCertificateAtIndex(pvc, 0);

	if (!SecPolicyCheckCertEmail(leaf, email)) {
		/* Hostname mismatch or no hostnames found in certificate. */
		SecPVCSetResult(pvc, key, 0, kCFBooleanFalse);
    }
}

static void SecPolicyCheckValidIntermediates(SecPVCRef pvc,
	CFStringRef key) {
	CFIndex ix, count = SecPVCGetCertificateCount(pvc);
	CFAbsoluteTime verifyTime = SecPVCGetVerifyTime(pvc);
	for (ix = 1; ix < count - 1; ++ix) {
		SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, ix);
		if (!SecCertificateIsValid(cert, verifyTime)) {
			/* Intermediate certificate has expired. */
			if (!SecPVCSetResult(pvc, key, ix, kCFBooleanFalse))
				return;
		}
	}
}

static void SecPolicyCheckValidLeaf(SecPVCRef pvc,
	CFStringRef key) {
	CFAbsoluteTime verifyTime = SecPVCGetVerifyTime(pvc);
	SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, 0);
	if (!SecCertificateIsValid(cert, verifyTime)) {
		/* Leaf certificate has expired. */
		if (!SecPVCSetResult(pvc, key, 0, kCFBooleanFalse))
			return;
	}
}

static void SecPolicyCheckValidRoot(SecPVCRef pvc,
	CFStringRef key) {
	CFIndex ix, count = SecPVCGetCertificateCount(pvc);
	CFAbsoluteTime verifyTime = SecPVCGetVerifyTime(pvc);
	ix = count - 1;
	SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, ix);
	if (!SecCertificateIsValid(cert, verifyTime)) {
		/* Root certificate has expired. */
		if (!SecPVCSetResult(pvc, key, ix, kCFBooleanFalse))
			return;
	}
}

/* AUDIT[securityd](done):
   policy->_options is a caller provided dictionary, only its cf type has
   been checked.
 */
static void SecPolicyCheckIssuerCommonName(SecPVCRef pvc,
	CFStringRef key) {
    CFIndex count = SecPVCGetCertificateCount(pvc);
    if (count < 2) {
		/* Can't check intermediates common name if there is no intermediate. */
		SecPVCSetResult(pvc, key, 0, kCFBooleanFalse);
        return;
    }

	SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, 1);
	SecPolicyRef policy = SecPVCGetPolicy(pvc);
    CFStringRef commonName =
        (CFStringRef)CFDictionaryGetValue(policy->_options, key);
    if (!isString(commonName)) {
        /* @@@ We can't return an error here and making the evaluation fail
           won't help much either. */
        return;
    }
    if (!SecPolicyCheckCertSubjectCommonName(cert, commonName)) {
        SecPVCSetResult(pvc, key, 0, kCFBooleanFalse);
    }
}

/* AUDIT[securityd](done):
   policy->_options is a caller provided dictionary, only its cf type has
   been checked.
 */
static void SecPolicyCheckSubjectCommonName(SecPVCRef pvc,
	CFStringRef key) {
	SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, 0);
	SecPolicyRef policy = SecPVCGetPolicy(pvc);
	CFStringRef common_name = (CFStringRef)CFDictionaryGetValue(policy->_options,
		key);
    if (!isString(common_name)) {
        /* @@@ We can't return an error here and making the evaluation fail
           won't help much either. */
        return;
    }
    if (!SecPolicyCheckCertSubjectCommonName(cert, common_name)) {
		SecPVCSetResult(pvc, key, 0, kCFBooleanFalse);
    }
}

/* AUDIT[securityd](done):
   policy->_options is a caller provided dictionary, only its cf type has
   been checked.
 */
static void SecPolicyCheckSubjectCommonNamePrefix(SecPVCRef pvc,
	CFStringRef key) {
	SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, 0);
	SecPolicyRef policy = SecPVCGetPolicy(pvc);
    CFStringRef prefix = (CFStringRef)CFDictionaryGetValue(policy->_options,
		key);
    if (!isString(prefix)) {
        /* @@@ We can't return an error here and making the evaluation fail
           won't help much either. */
        return;
    }
    if (!SecPolicyCheckCertSubjectCommonNamePrefix(cert, prefix)) {
        SecPVCSetResult(pvc, key, 0, kCFBooleanFalse);
    }
}

/* AUDIT[securityd](done):
   policy->_options is a caller provided dictionary, only its cf type has
   been checked.
 */
static void SecPolicyCheckSubjectCommonNameTEST(SecPVCRef pvc,
	CFStringRef key) {
	SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, 0);
	SecPolicyRef policy = SecPVCGetPolicy(pvc);
	CFStringRef common_name = (CFStringRef)CFDictionaryGetValue(policy->_options,
		key);
    if (!isString(common_name)) {
        /* @@@ We can't return an error here and making the evaluation fail
           won't help much either. */
        return;
    }
    if (!SecPolicyCheckCertSubjectCommonNameTEST(cert, common_name)) {
        SecPVCSetResult(pvc, key, 0, kCFBooleanFalse);
    }
}

/* AUDIT[securityd](done):
   policy->_options is a caller provided dictionary, only its cf type has
   been checked.
 */
static void SecPolicyCheckNotValidBefore(SecPVCRef pvc,
	CFStringRef key) {
	SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, 0);
	SecPolicyRef policy = SecPVCGetPolicy(pvc);
    CFDateRef date = (CFDateRef)CFDictionaryGetValue(policy->_options, key);
    if (!isDate(date)) {
        /* @@@ We can't return an error here and making the evaluation fail
           won't help much either. */
        return;
    }
    if (!SecPolicyCheckCertNotValidBefore(cert, date)) {
		if (!SecPVCSetResult(pvc, key, 0, kCFBooleanFalse))
			return;
	}
}

/* AUDIT[securityd](done):
   policy->_options is a caller provided dictionary, only its cf type has
   been checked.
 */
static void SecPolicyCheckChainLength(SecPVCRef pvc,
	CFStringRef key) {
	CFIndex count = SecPVCGetCertificateCount(pvc);
	SecPolicyRef policy = SecPVCGetPolicy(pvc);
    CFNumberRef chainLength =
        (CFNumberRef)CFDictionaryGetValue(policy->_options, key);
    CFIndex value;
    if (!chainLength || CFGetTypeID(chainLength) != CFNumberGetTypeID() ||
        !CFNumberGetValue(chainLength, kCFNumberCFIndexType, &value)) {
        /* @@@ We can't return an error here and making the evaluation fail
           won't help much either. */
        return;
    }
    if (value != count) {
		/* Chain length doesn't match policy requirement. */
		if (!SecPVCSetResult(pvc, key, 0, kCFBooleanFalse))
			return;
    }
}

static bool isDigestInPolicy(SecPVCRef pvc, CFStringRef key, CFDataRef digest) {
    SecPolicyRef policy = SecPVCGetPolicy(pvc);
    CFTypeRef value = CFDictionaryGetValue(policy->_options, key);

    bool foundMatch = false;
    if (isData(value))
        foundMatch = CFEqual(digest, value);
    else if (isArray(value))
        foundMatch = CFArrayContainsValue((CFArrayRef) value, CFRangeMake(0, CFArrayGetCount((CFArrayRef) value)), digest);
    else {
        /* @@@ We only support Data and Array but we can't return an error here so.
         we let the evaluation fail (not much help) and assert in debug. */
        assert(false);
    }

    return foundMatch;
}

static void SecPolicyCheckAnchorSHA256(SecPVCRef pvc, CFStringRef key) {
    CFIndex count = SecPVCGetCertificateCount(pvc);
    SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, count - 1);
    CFDataRef anchorSHA256 = NULL;
    anchorSHA256 = SecCertificateCopySHA256Digest(cert);

    if (!isDigestInPolicy(pvc, key, anchorSHA256)) {
        SecPVCSetResult(pvc, kSecPolicyCheckAnchorSHA256, count-1, kCFBooleanFalse);
    }

    CFReleaseNull(anchorSHA256);
    return;
}


/* AUDIT[securityd](done):
   policy->_options is a caller provided dictionary, only its cf type has
   been checked.
 */
static void SecPolicyCheckAnchorSHA1(SecPVCRef pvc,
	CFStringRef key) {
    CFIndex count = SecPVCGetCertificateCount(pvc);
	SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, count - 1);
    CFDataRef anchorSHA1 = SecCertificateGetSHA1Digest(cert);

    if (!isDigestInPolicy(pvc, key, anchorSHA1))
        if (!SecPVCSetResult(pvc, kSecPolicyCheckAnchorSHA1, count-1, kCFBooleanFalse))
            return;

    return;
}

/*
   Check the SHA256 of SPKI of the first intermediate CA certificate in the path
   policy->_options is a caller provided dictionary, only its cf type has
   been checked.
 */
static void SecPolicyCheckIntermediateSPKISHA256(SecPVCRef pvc,
                                                 CFStringRef key) {
    SecCertificateRef cert = NULL;
    CFDataRef digest = NULL;

    if (SecPVCGetCertificateCount(pvc) < 2) {
        SecPVCSetResult(pvc, kSecPolicyCheckIntermediateSPKISHA256, 0, kCFBooleanFalse);
        return;
    }

    cert = SecPVCGetCertificateAtIndex(pvc, 1);
    digest = SecCertificateCopySubjectPublicKeyInfoSHA256Digest(cert);

    if (!isDigestInPolicy(pvc, key, digest)) {
        SecPVCSetResult(pvc, kSecPolicyCheckIntermediateSPKISHA256, 1, kCFBooleanFalse);
    }
    CFReleaseNull(digest);
}

/*
   policy->_options is a caller provided dictionary, only its cf type has
   been checked.
 */
static void SecPolicyCheckAnchorApple(SecPVCRef pvc,
                                      CFStringRef key) {
    CFIndex count = SecPVCGetCertificateCount(pvc);
    SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, count - 1);
    SecPolicyRef policy = SecPVCGetPolicy(pvc);
    CFTypeRef value = CFDictionaryGetValue(policy->_options, key);
    SecAppleTrustAnchorFlags flags = 0;

    if (isDictionary(value)) {
        if (CFDictionaryGetValue(value, kSecPolicyAppleAnchorIncludeTestRoots)) {
            flags |= kSecAppleTrustAnchorFlagsIncludeTestAnchors;
        }
    }

    bool foundMatch = SecIsAppleTrustAnchor(cert, flags);

    if (!foundMatch)
        if (!SecPVCSetResult(pvc, kSecPolicyCheckAnchorApple, 0, kCFBooleanFalse))
            return;

    return;
}


/* AUDIT[securityd](done):
   policy->_options is a caller provided dictionary, only its cf type has
   been checked.
 */
static void SecPolicyCheckSubjectOrganization(SecPVCRef pvc,
	CFStringRef key) {
	SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, 0);
	SecPolicyRef policy = SecPVCGetPolicy(pvc);
	CFStringRef org = (CFStringRef)CFDictionaryGetValue(policy->_options,
		key);
    if (!isString(org)) {
        /* @@@ We can't return an error here and making the evaluation fail
           won't help much either. */
        return;
    }
    if (!SecPolicyCheckCertSubjectOrganization(cert, org)) {
		/* Leaf Subject Organization mismatch. */
		SecPVCSetResult(pvc, key, 0, kCFBooleanFalse);
	}
}

static void SecPolicyCheckSubjectOrganizationalUnit(SecPVCRef pvc,
	CFStringRef key) {
	SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, 0);
	SecPolicyRef policy = SecPVCGetPolicy(pvc);
	CFStringRef orgUnit = (CFStringRef)CFDictionaryGetValue(policy->_options,
		key);
    if (!isString(orgUnit)) {
        /* @@@ We can't return an error here and making the evaluation fail
           won't help much either. */
        return;
    }
    if (!SecPolicyCheckCertSubjectOrganizationalUnit(cert, orgUnit)) {
        /* Leaf Subject Organization mismatch. */
        SecPVCSetResult(pvc, key, 0, kCFBooleanFalse);
    }
}

/* AUDIT[securityd](done):
   policy->_options is a caller provided dictionary, only its cf type has
   been checked.
 */
static void SecPolicyCheckEAPTrustedServerNames(SecPVCRef pvc,
	CFStringRef key) {
	SecPolicyRef policy = SecPVCGetPolicy(pvc);
	CFArrayRef trustedServerNames = (CFArrayRef)
		CFDictionaryGetValue(policy->_options, key);
    /* No names specified means we accept any name. */
    if (!trustedServerNames)
        return;
    if (!isArray(trustedServerNames)) {
        /* @@@ We can't return an error here and making the evaluation fail
           won't help much either. */
        return;
    }

	SecCertificateRef leaf = SecPVCGetCertificateAtIndex(pvc, 0);
    if (!SecPolicyCheckCertEAPTrustedServerNames(leaf, trustedServerNames)) {
		/* Hostname mismatch or no hostnames found in certificate. */
		SecPVCSetResult(pvc, key, 0, kCFBooleanFalse);
	}
}

static const unsigned char UTN_USERFirst_Hardware_Serial[][16] = {
{ 0xd8, 0xf3, 0x5f, 0x4e, 0xb7, 0x87, 0x2b, 0x2d, 0xab, 0x06, 0x92, 0xe3, 0x15, 0x38, 0x2f, 0xb0 },
{ 0x92, 0x39, 0xd5, 0x34, 0x8f, 0x40, 0xd1, 0x69, 0x5a, 0x74, 0x54, 0x70, 0xe1, 0xf2, 0x3f, 0x43 },
{ 0xb0, 0xb7, 0x13, 0x3e, 0xd0, 0x96, 0xf9, 0xb5, 0x6f, 0xae, 0x91, 0xc8, 0x74, 0xbd, 0x3a, 0xc0 },
{ 0xe9, 0x02, 0x8b, 0x95, 0x78, 0xe4, 0x15, 0xdc, 0x1a, 0x71, 0x0a, 0x2b, 0x88, 0x15, 0x44, 0x47 },
{ 0x39, 0x2a, 0x43, 0x4f, 0x0e, 0x07, 0xdf, 0x1f, 0x8a, 0xa3, 0x05, 0xde, 0x34, 0xe0, 0xc2, 0x29 },
{ 0x3e, 0x75, 0xce, 0xd4, 0x6b, 0x69, 0x30, 0x21, 0x21, 0x88, 0x30, 0xae, 0x86, 0xa8, 0x2a, 0x71 },
{ 0xd7, 0x55, 0x8f, 0xda, 0xf5, 0xf1, 0x10, 0x5b, 0xb2, 0x13, 0x28, 0x2b, 0x70, 0x77, 0x29, 0xa3 },
{ 0x04, 0x7e, 0xcb, 0xe9, 0xfc, 0xa5, 0x5f, 0x7b, 0xd0, 0x9e, 0xae, 0x36, 0xe1, 0x0c, 0xae, 0x1e },
{ 0xf5, 0xc8, 0x6a, 0xf3, 0x61, 0x62, 0xf1, 0x3a, 0x64, 0xf5, 0x4f, 0x6d, 0xc9, 0x58, 0x7c, 0x06 } };

static const unsigned char UTN_USERFirst_Hardware_Normalized_Issuer[] = {
  0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13, 0x02, 0x55,
  0x53, 0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x08, 0x13, 0x02,
  0x55, 0x54, 0x31, 0x17, 0x30, 0x15, 0x06, 0x03, 0x55, 0x04, 0x07, 0x13,
  0x0e, 0x53, 0x41, 0x4c, 0x54, 0x20, 0x4c, 0x41, 0x4b, 0x45, 0x20, 0x43,
  0x49, 0x54, 0x59, 0x31, 0x1e, 0x30, 0x1c, 0x06, 0x03, 0x55, 0x04, 0x0a,
  0x13, 0x15, 0x54, 0x48, 0x45, 0x20, 0x55, 0x53, 0x45, 0x52, 0x54, 0x52,
  0x55, 0x53, 0x54, 0x20, 0x4e, 0x45, 0x54, 0x57, 0x4f, 0x52, 0x4b, 0x31,
  0x21, 0x30, 0x1f, 0x06, 0x03, 0x55, 0x04, 0x0b, 0x13, 0x18, 0x48, 0x54,
  0x54, 0x50, 0x3a, 0x2f, 0x2f, 0x57, 0x57, 0x57, 0x2e, 0x55, 0x53, 0x45,
  0x52, 0x54, 0x52, 0x55, 0x53, 0x54, 0x2e, 0x43, 0x4f, 0x4d, 0x31, 0x1f,
  0x30, 0x1d, 0x06, 0x03, 0x55, 0x04, 0x03, 0x13, 0x16, 0x55, 0x54, 0x4e,
  0x2d, 0x55, 0x53, 0x45, 0x52, 0x46, 0x49, 0x52, 0x53, 0x54, 0x2d, 0x48,
  0x41, 0x52, 0x44, 0x57, 0x41, 0x52, 0x45
};
static const unsigned int UTN_USERFirst_Hardware_Normalized_Issuer_len = 151;


static void SecPolicyCheckBlackListedLeaf(SecPVCRef pvc,
	CFStringRef key) {
	SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, 0);
    CFDataRef issuer = cert ? SecCertificateGetNormalizedIssuerContent(cert) : NULL;

    if (issuer && (CFDataGetLength(issuer) == (CFIndex)UTN_USERFirst_Hardware_Normalized_Issuer_len) &&
        (0 == memcmp(UTN_USERFirst_Hardware_Normalized_Issuer, CFDataGetBytePtr(issuer),
            UTN_USERFirst_Hardware_Normalized_Issuer_len)))
    {
    #if (TARGET_OS_MAC && !(TARGET_OS_EMBEDDED || TARGET_OS_IPHONE))
        CFDataRef serial = SecCertificateCopySerialNumber(cert, NULL);
    #else
        CFDataRef serial = SecCertificateCopySerialNumber(cert);
    #endif

        if (serial) {
            CFIndex serial_length = CFDataGetLength(serial);
            const uint8_t *serial_ptr = CFDataGetBytePtr(serial);

            while ((serial_length > 0) && (*serial_ptr == 0)) {
                serial_ptr++;
                serial_length--;
            }

            if (serial_length == (CFIndex)sizeof(*UTN_USERFirst_Hardware_Serial)) {
                unsigned int i;
                for (i = 0; i < array_size(UTN_USERFirst_Hardware_Serial); i++)
                {
                    if (0 == memcmp(UTN_USERFirst_Hardware_Serial[i],
                        serial_ptr, sizeof(*UTN_USERFirst_Hardware_Serial)))
                    {
                        SecPVCSetResult(pvc, key, 0, kCFBooleanFalse);
                        CFReleaseSafe(serial);
                        return;
                    }
                }
            }
            CFRelease(serial);
        }
    }

	SecOTAPKIRef otapkiRef = SecOTAPKICopyCurrentOTAPKIRef();
	if (NULL != otapkiRef)
	{
		CFSetRef blackListedKeys = SecOTAPKICopyBlackListSet(otapkiRef);
		CFRelease(otapkiRef);
		if (NULL != blackListedKeys)
		{
			/* Check for blacklisted intermediates keys. */
			CFDataRef dgst = SecCertificateCopyPublicKeySHA1Digest(cert);
			if (dgst)
			{
				/* Check dgst against blacklist. */
				if (CFSetContainsValue(blackListedKeys, dgst))
				{
					SecPVCSetResult(pvc, key, 0, kCFBooleanFalse);
				}
				CFRelease(dgst);
			}
			CFRelease(blackListedKeys);
		}
	}
}

static void SecPolicyCheckGrayListedLeaf(SecPVCRef pvc, CFStringRef key)
{
	SecOTAPKIRef otapkiRef = SecOTAPKICopyCurrentOTAPKIRef();
	if (NULL != otapkiRef)
	{
		CFSetRef grayListedKeys = SecOTAPKICopyGrayList(otapkiRef);
		CFRelease(otapkiRef);
		if (NULL != grayListedKeys)
		{
			SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, 0);

			CFDataRef dgst = SecCertificateCopyPublicKeySHA1Digest(cert);
			if (dgst)
			{
				/* Check dgst against gray. */
				if (CFSetContainsValue(grayListedKeys, dgst))
				{
					SecPVCSetResult(pvc, key, 0, kCFBooleanFalse);
				}
				CFRelease(dgst);
			}
			CFRelease(grayListedKeys);
		}
	}
}

static void SecPolicyCheckLeafMarkerOid(SecPVCRef pvc, CFStringRef key)
{
	SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, 0);
    SecPolicyRef policy = SecPVCGetPolicy(pvc);
    CFTypeRef value = CFDictionaryGetValue(policy->_options, key);

    if (!SecPolicyCheckCertLeafMarkerOid(cert, value)) {
        SecPVCSetResult(pvc, key, 0, kCFBooleanFalse);
    }
}

static void SecPolicyCheckLeafMarkerOidWithoutValueCheck(SecPVCRef pvc, CFStringRef key)
{
    SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, 0);
    SecPolicyRef policy = SecPVCGetPolicy(pvc);
    CFTypeRef value = CFDictionaryGetValue(policy->_options, key);

    if (!SecPolicyCheckCertLeafMarkerOidWithoutValueCheck(cert, value)) {
        SecPVCSetResult(pvc, key, 0, kCFBooleanFalse);
    }
}

/*
 * The value is a dictionary. The dictionary contains keys indicating
 * whether the value is for Prod or QA. The values are the same as
 * in the options dictionary for SecPolicyCheckLeafMarkerOid.
 */
static void SecPolicyCheckLeafMarkersProdAndQA(SecPVCRef pvc, CFStringRef key)
{
    SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, 0);
    SecPolicyRef policy = SecPVCGetPolicy(pvc);
    CFDictionaryRef value = CFDictionaryGetValue(policy->_options, key);
    CFTypeRef prodValue = CFDictionaryGetValue(value, kSecPolicyLeafMarkerProd);

    if (!SecPolicyCheckCertLeafMarkerOid(cert, prodValue)) {
        bool result = false;
        if (!result) {
            SecPVCSetResult(pvc, key, 0, kCFBooleanFalse);
        }
    }
}

static void SecPolicyCheckIntermediateMarkerOid(SecPVCRef pvc, CFStringRef key)
{
    CFIndex ix, count = SecPVCGetCertificateCount(pvc);
    SecPolicyRef policy = SecPVCGetPolicy(pvc);
    CFTypeRef value = CFDictionaryGetValue(policy->_options, key);

    for (ix = 1; ix < count - 1; ix++) {
        SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, ix);
        if (SecCertificateHasMarkerExtension(cert, value))
            return;
    }
    SecPVCSetResult(pvc, key, 0, kCFBooleanFalse);
}

static void SecPolicyCheckIntermediateEKU(SecPVCRef pvc, CFStringRef key)
{
	CFIndex ix, count = SecPVCGetCertificateCount(pvc);
	SecPolicyRef policy = SecPVCGetPolicy(pvc);
	CFTypeRef peku = CFDictionaryGetValue(policy->_options, key);

	for (ix = 1; ix < count - 1; ix++) {
		SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, ix);
        if (!SecPolicyCheckCertExtendedKeyUsage(cert, peku)) {
			SecPVCSetResult(pvc, key, ix, kCFBooleanFalse);
		}
	}
}

static void SecPolicyCheckIntermediateOrganization(SecPVCRef pvc, CFStringRef key)
{
    CFIndex ix, count = SecPVCGetCertificateCount(pvc);
    SecPolicyRef policy = SecPVCGetPolicy(pvc);
    CFTypeRef organization = CFDictionaryGetValue(policy->_options, key);

    for (ix = 1; ix < count - 1; ix++) {
        SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, ix);
        if (!SecPolicyCheckCertSubjectOrganization(cert, organization)) {
            SecPVCSetResult(pvc, key, ix, kCFBooleanFalse);
        }
    }
}

static void SecPolicyCheckIntermediateCountry(SecPVCRef pvc, CFStringRef key)
{
    CFIndex ix, count = SecPVCGetCertificateCount(pvc);
    SecPolicyRef policy = SecPVCGetPolicy(pvc);
    CFTypeRef country = CFDictionaryGetValue(policy->_options, key);

    for (ix = 1; ix < count - 1; ix++) {
        SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, ix);
        if (!SecPolicyCheckCertSubjectCountry(cert, country)) {
            SecPVCSetResult(pvc, key, ix, kCFBooleanFalse);
        }
    }
}

/* Returns true if path is on the allow list for the authority key of the
   certificate at certix, false otherwise.
 */
static bool SecPVCCheckCertificateAllowList(SecPVCRef pvc, CFIndex certix)
{
    bool result = false;
    CFIndex ix = 0, count = SecPVCGetCertificateCount(pvc);
    CFStringRef authKey = NULL;
    CFArrayRef allowedCerts = NULL;
    SecOTAPKIRef otapkiRef = NULL;

    if (certix < 0 || certix >= count) {
        return result;
    }

    //get authKeyID from the specified cert in the chain
    SecCertificateRef issuedCert = SecPVCGetCertificateAtIndex(pvc, certix);
    CFDataRef authKeyID = SecCertificateGetAuthorityKeyID(issuedCert);
    if (NULL == authKeyID) {
        return result;
    }
    authKey = CFDataCopyHexString(authKeyID);
    if (NULL == authKey) {
        goto errout;
    }

    otapkiRef = SecOTAPKICopyCurrentOTAPKIRef();
    if (NULL == otapkiRef) {
        goto errout;
    }

    allowedCerts = SecOTAPKICopyAllowListForAuthKeyID(otapkiRef, authKey);
    if (NULL == allowedCerts || !CFArrayGetCount(allowedCerts)) {
        goto errout;
    }

    //search sorted array for the SHA256 hash of a cert in the chain
    CFRange range = CFRangeMake(0, CFArrayGetCount(allowedCerts));
    for (ix = 0; ix <= certix; ix++) {
        SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, ix);
        if (!cert) {
            goto errout;
        }

        CFDataRef certHash = SecCertificateCopySHA256Digest(cert);
        if (!certHash) {
            goto errout;
        }

        CFIndex position = CFArrayBSearchValues(allowedCerts, range, certHash,
                                                (CFComparatorFunction)CFDataCompare, NULL);
        if (position < CFArrayGetCount(allowedCerts)) {
            CFDataRef possibleMatch = CFArrayGetValueAtIndex(allowedCerts, position);
            if (!CFDataCompare(certHash, possibleMatch)) {
                //this cert is in the allowlist
                result = true;
            }
        }

        CFRelease(certHash);
    }

errout:
    CFReleaseNull(authKey);
    CFReleaseNull(otapkiRef);
    CFReleaseNull(allowedCerts);
    return result;
}

#define DCMP(_idx_) memcmp(data+(8*_idx_), digest, 8)

/* Returns true if leaf is on the CT whitelist */
static bool SecPVCCheckCTWhiteListedLeaf(SecPVCRef pvc)
{
    SecOTAPKIRef otapkiRef = NULL;
    CFDataRef whiteList = NULL;
    SecCertificateRef cert = NULL;
    CFDataRef dgst = NULL;
    bool result = false;
    const uint8_t *digest = NULL;
    const uint8_t *data = NULL;
    require(otapkiRef = SecOTAPKICopyCurrentOTAPKIRef(), out);
    require(whiteList = SecOTAPKICopyCTWhiteList(otapkiRef), out);
    require(cert = SecPVCGetCertificateAtIndex(pvc, 0), out);
    require(dgst = SecCertificateCopySHA256Digest(cert), out);

    digest = CFDataGetBytePtr(dgst);
    data = CFDataGetBytePtr(whiteList);
    CFIndex l = 0;
    CFIndex h = CFDataGetLength(whiteList)/8-1;

    if(DCMP(l)==0 || DCMP(h)==0) {
        result = true;
        goto out;
    }

    if(DCMP(l)>0 || DCMP(h)<0) {
        goto out;
    }

    while((h-l)>1) {
        CFIndex i = (h+l)/
        2;
        int s = DCMP(i);
        if(s == 0) {
            result = true;
            goto out;
        } else if(s < 0) {
            l = i;
        } else {
            h = i;
        }
    }

out:
    CFReleaseSafe(dgst);
    CFReleaseSafe(whiteList);
    CFReleaseSafe(otapkiRef);
    return result;
}

/****************************************************************************
 *********************** New rfc5280 Chain Validation ***********************
 ****************************************************************************/

#define POLICY_MAPPING 1
#define POLICY_SUBTREES 1

struct policy_tree_add_ctx {
    oid_t p_oid;
    policy_qualifier_t p_q;
};

/* For each node of depth i-1 in the valid_policy_tree where P-OID is in the expected_policy_set, create a child node as follows: set the valid_policy to P-OID, set the qualifier_set to P-Q, and set the expected_policy_set to {P-OID}. */
static bool policy_tree_add_if_match(policy_tree_t node, void *ctx) {
    struct policy_tree_add_ctx *info = (struct policy_tree_add_ctx *)ctx;
    policy_set_t policy_set;
    for (policy_set = node->expected_policy_set;
        policy_set;
        policy_set = policy_set->oid_next) {
        if (oid_equal(policy_set->oid, info->p_oid)) {
            policy_tree_add_child(node, &info->p_oid, info->p_q);
            return true;
        }
    }
    return false;
}

/* If the valid_policy_tree includes a node of depth i-1 with the valid_policy anyPolicy, generate a child node with the following values: set the valid_policy to P-OID, set the qualifier_set to P-Q, and set the expected_policy_set to {P-OID}. */
static bool policy_tree_add_if_any(policy_tree_t node, void *ctx) {
    struct policy_tree_add_ctx *info = (struct policy_tree_add_ctx *)ctx;
    if (oid_equal(node->valid_policy, oidAnyPolicy)) {
        policy_tree_add_child(node, &info->p_oid, info->p_q);
        return true;
    }
    return false;
}

/* Return true iff node has a child with a valid_policy equal to oid. */
static bool policy_tree_has_child_with_oid(policy_tree_t node,
    const oid_t *oid) {
    policy_tree_t child;
    for (child = node->children; child; child = child->siblings) {
        if (oid_equal(child->valid_policy, (*oid))) {
            return true;
        }
    }
    return false;
}

/* For each node in the valid_policy_tree of depth i-1, for each value in the expected_policy_set (including anyPolicy) that does not appear in a child node, create a child node with the following values: set the valid_policy to the value from the expected_policy_set in the parent node, set the qualifier_set to AP-Q, and set the expected_policy_set to the value in the valid_policy from this node. */
static bool policy_tree_add_expected(policy_tree_t node, void *ctx) {
    policy_qualifier_t p_q = (policy_qualifier_t)ctx;
    policy_set_t policy_set;
    bool added_node = false;
    for (policy_set = node->expected_policy_set;
        policy_set;
        policy_set = policy_set->oid_next) {
        if (!policy_tree_has_child_with_oid(node, &policy_set->oid)) {
            policy_tree_add_child(node, &policy_set->oid, p_q);
            added_node = true;
        }
    }
    return added_node;
}

#if POLICY_MAPPING
/* For each node where ID-P is the valid_policy, set expected_policy_set to the set of subjectDomainPolicy values that are specified as equivalent to ID-P by the policy mappings extension. */
static bool policy_tree_map_if_match(policy_tree_t node, void *ctx) {
    /* Can't map oidAnyPolicy. */
    if (oid_equal(node->valid_policy, oidAnyPolicy))
        return false;

    const SecCEPolicyMappings *pm = (const SecCEPolicyMappings *)ctx;
    size_t mapping_ix, mapping_count = pm->numMappings;
    policy_set_t policy_set = NULL;
    /* Generate the policy_set of sdps for matching idp */
    for (mapping_ix = 0; mapping_ix < mapping_count; ++mapping_ix) {
        const SecCEPolicyMapping *mapping = &pm->mappings[mapping_ix];
        if (oid_equal(node->valid_policy, mapping->issuerDomainPolicy)) {
            policy_set_t p_node = (policy_set_t)malloc(sizeof(*policy_set));
            p_node->oid = mapping->subjectDomainPolicy;
            p_node->oid_next = policy_set ? policy_set : NULL;
            policy_set = p_node;
        }
    }
    if (policy_set) {
        policy_tree_set_expected_policy(node, policy_set);
        return true;
    }
    return false;
}

/* If no node of depth i in the valid_policy_tree has a valid_policy of ID-P but there is a node of depth i with a valid_policy of anyPolicy, then generate a child node of the node of depth i-1 that has a valid_policy of anyPolicy as follows:
        (i)   set the valid_policy to ID-P;
        (ii)  set the qualifier_set to the qualifier set of the policy anyPolicy in the certificate policies extension of certificate i; and
        (iii) set the expected_policy_set to the set of subjectDomainPolicy values that are specified as equivalent to ID-P by the policy mappings extension. */
static bool policy_tree_map_if_any(policy_tree_t node, void *ctx) {
    if (!oid_equal(node->valid_policy, oidAnyPolicy)) {
        return false;
    }

    const SecCEPolicyMappings *pm = (const SecCEPolicyMappings *)ctx;
    size_t mapping_ix, mapping_count = pm->numMappings;
    CFMutableDictionaryRef mappings = NULL;
    CFDataRef idp = NULL;
    CFDataRef sdp = NULL;
    require_quiet(mappings = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
                                                                &kCFTypeDictionaryValueCallBacks),
                   errOut);
    /* First we need to walk the mappings to generate the dictionary idp->sdps */
    for (mapping_ix = 0; mapping_ix < mapping_count; mapping_ix++) {
        oid_t issuerDomainPolicy = pm->mappings[mapping_ix].issuerDomainPolicy;
        oid_t subjectDomainPolicy = pm->mappings[mapping_ix].subjectDomainPolicy;
        idp = CFDataCreateWithBytesNoCopy(NULL, issuerDomainPolicy.data, issuerDomainPolicy.length, kCFAllocatorNull);
        sdp = CFDataCreateWithBytesNoCopy(NULL, subjectDomainPolicy.data, subjectDomainPolicy.length, kCFAllocatorNull);
        CFMutableArrayRef sdps = (CFMutableArrayRef)CFDictionaryGetValue(mappings, idp);
        if (sdps) {
            CFArrayAppendValue(sdps, sdp);
        } else {
            require_quiet(sdps = CFArrayCreateMutable(kCFAllocatorDefault, 0,
                                                      &kCFTypeArrayCallBacks), errOut);
            CFArrayAppendValue(sdps, sdp);
            CFDictionarySetValue(mappings, idp, sdps);
            CFRelease(sdps);
        }
        CFReleaseNull(idp);
        CFReleaseNull(sdp);
    }

    /* Now we use the dictionary to generate the new nodes */
    CFDictionaryForEach(mappings, ^(const void *key, const void *value) {
        CFDataRef idp = key;
        CFArrayRef sdps = value;

        /* (i)   set the valid_policy to ID-P; */
        oid_t p_oid;
        p_oid.data = (uint8_t *)CFDataGetBytePtr(idp);
        p_oid.length = CFDataGetLength(idp);

        /* (ii)  set the qualifier_set to the qualifier set of the policy anyPolicy in the certificate policies extension of certificate i */
        policy_qualifier_t p_q = node->qualifier_set;

        /* (iii) set the expected_policy_set to the set of subjectDomainPolicy values that are specified as equivalent to ID-P by the policy mappings extension.  */
        __block policy_set_t p_expected = NULL;
        CFArrayForEach(sdps, ^(const void *value) {
            policy_set_t p_node = (policy_set_t)malloc(sizeof(*p_expected));
            p_node->oid.data = (void *)CFDataGetBytePtr(value);
            p_node->oid.length = CFDataGetLength(value);
            p_node->oid_next = p_expected ? p_expected : NULL;
            p_expected = p_node;
        });

        policy_tree_add_sibling(node, &p_oid, p_q, p_expected);
    });
    CFReleaseNull(mappings);
    return true;

errOut:
    CFReleaseNull(mappings);
    CFReleaseNull(idp);
    CFReleaseNull(sdp);
    return false;
}

static bool policy_tree_map_delete_if_match(policy_tree_t node, void *ctx) {
    /* Can't map oidAnyPolicy. */
    if (oid_equal(node->valid_policy, oidAnyPolicy))
        return false;

    const SecCEPolicyMappings *pm = (const SecCEPolicyMappings *)ctx;
    size_t mapping_ix, mapping_count = pm->numMappings;
    /* If this node matches any of the idps, delete it. */
    for (mapping_ix = 0; mapping_ix < mapping_count; ++mapping_ix) {
        const SecCEPolicyMapping *mapping = &pm->mappings[mapping_ix];
        if (oid_equal(node->valid_policy, mapping->issuerDomainPolicy)) {
            policy_tree_remove_node(&node);
            break;
        }
    }
    return true;
}
#endif  /* POLICY_MAPPINGS */

/* rfc5280 basic cert processing. */
static void SecPolicyCheckBasicCertificateProcessing(SecPVCRef pvc,
	CFStringRef key) {
    /* Inputs */
    //cert_path_t path;
    CFIndex count = SecPVCGetCertificateCount(pvc);
    /* 64 bits cast: worst case here is we truncate the number of cert, and the validation may fail */
    assert((unsigned long)count<=UINT32_MAX); /* Debug check. Correct as long as CFIndex is long */
    uint32_t n = (uint32_t)count;
    bool is_anchored = SecPVCIsAnchored(pvc);
    if (is_anchored) {
        /* If the anchor is trusted we don't process the last cert in the
           chain (root). */
        n--;
    } else {
        /* trust may be restored for a path with an untrusted root that matches the allow list */
        pvc->is_allowlisted = SecPVCCheckCertificateAllowList(pvc, n - 1);
        if (!pvc->is_allowlisted) {
            /* Add a detail for the root not being trusted. */
            if (!SecPVCSetResultForced(pvc, kSecPolicyCheckAnchorTrusted,
                                      n - 1, kCFBooleanFalse, true))
                return;
        }
    }

    CFAbsoluteTime verify_time = SecPVCGetVerifyTime(pvc);
    //policy_set_t user_initial_policy_set = NULL;
    //trust_anchor_t anchor;
    bool initial_policy_mapping_inhibit = false;
    bool initial_explicit_policy = false;
    bool initial_any_policy_inhibit = false;

    /* Initialization */
    pvc->valid_policy_tree = policy_tree_create(&oidAnyPolicy, NULL);
#if POLICY_SUBTREES
    CFMutableArrayRef permitted_subtrees = NULL;
    CFMutableArrayRef excluded_subtrees = NULL;
    permitted_subtrees = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    excluded_subtrees = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    require_action_quiet(permitted_subtrees != NULL, errOut,
                         SecPVCSetResultForced(pvc, key, 0, kCFBooleanFalse, true));
    require_action_quiet(excluded_subtrees != NULL, errOut,
                         SecPVCSetResultForced(pvc, key, 0, kCFBooleanFalse, true));
#endif
    uint32_t explicit_policy = initial_explicit_policy ? 0 : n + 1;
    uint32_t inhibit_any_policy = initial_any_policy_inhibit ? 0 : n + 1;
    uint32_t policy_mapping = initial_policy_mapping_inhibit ? 0 : n + 1;

#if 0
    /* Path builder ensures we only get cert chains with proper issuer
       chaining with valid signatures along the way. */
    algorithm_id_t working_public_key_algorithm = anchor->public_key_algorithm;
    SecKeyRef working_public_key = anchor->public_key;
    x500_name_t working_issuer_name = anchor->issuer_name;
#endif
    uint32_t i, max_path_length = n;
    SecCertificateRef cert = NULL;
    for (i = 1; i <= n; ++i) {
        /* Process Cert */
        cert = SecPVCGetCertificateAtIndex(pvc, n - i);
        bool is_self_issued = SecPVCIsCertificateAtIndexSelfIssued(pvc, n - i);

        /* (a) Verify the basic certificate information. */
        /* @@@ Ensure that cert was signed with working_public_key_algorithm
           using the working_public_key and the working_public_key_parameters. */
#if 1
        /* Already done by chain builder. */
        if (!SecCertificateIsValid(cert, verify_time)) {
            CFStringRef fail_key = i == n ? kSecPolicyCheckValidLeaf : kSecPolicyCheckValidIntermediates;
            if (!SecPVCSetResult(pvc, fail_key, n - i, kCFBooleanFalse)) {
                goto errOut;
            }
        }
        if (SecCertificateIsWeakKey(cert)) {
            CFStringRef fail_key = i == n ? kSecPolicyCheckWeakLeaf : kSecPolicyCheckWeakIntermediates;
            if (!SecPVCSetResult(pvc, fail_key, n - i, kCFBooleanFalse)) {
                goto errOut;
            }
        }
#endif
        /* @@@ cert.issuer == working_issuer_name. */

#if POLICY_SUBTREES
        /* (b) (c) */
        if (!is_self_issued || i == n) {
            bool found = false;
            /* Verify certificate Subject Name and SubjectAltNames are not within any of the excluded_subtrees */
            if(excluded_subtrees && CFArrayGetCount(excluded_subtrees)) {
                if ((errSecSuccess != SecNameContraintsMatchSubtrees(cert, excluded_subtrees, &found, false)) || found) {
                    secnotice("policy", "name in excluded subtrees");
                    if(!SecPVCSetResultForced(pvc, key, n - i, kCFBooleanFalse, true)) { goto errOut; }
                }
            }
            /* Verify certificate Subject Name and SubjectAltNames are within the permitted_subtrees */
            if(permitted_subtrees && CFArrayGetCount(permitted_subtrees)) {
               if ((errSecSuccess != SecNameContraintsMatchSubtrees(cert, permitted_subtrees, &found, true)) || !found) {
                   secnotice("policy", "name not in permitted subtrees");
                   if(!SecPVCSetResultForced(pvc, key, n - i, kCFBooleanFalse, true)) { goto errOut; }
               }
            }
        }
#endif
        /* (d) */
        if (pvc->valid_policy_tree) {
            const SecCECertificatePolicies *cp =
                SecCertificateGetCertificatePolicies(cert);
            size_t policy_ix, policy_count = cp ? cp->numPolicies : 0;
            for (policy_ix = 0; policy_ix < policy_count; ++policy_ix) {
                const SecCEPolicyInformation *policy = &cp->policies[policy_ix];
                oid_t p_oid = policy->policyIdentifier;
                policy_qualifier_t p_q = &policy->policyQualifiers;
                struct policy_tree_add_ctx ctx = { p_oid, p_q };
                if (!oid_equal(p_oid, oidAnyPolicy)) {
                    if (!policy_tree_walk_depth(pvc->valid_policy_tree, i - 1,
                        policy_tree_add_if_match, &ctx)) {
                        policy_tree_walk_depth(pvc->valid_policy_tree, i - 1,
                        policy_tree_add_if_any, &ctx);
                    }
                }
            }
            /* The certificate policies extension includes the policy
               anyPolicy with the qualifier set AP-Q and either
               (a) inhibit_anyPolicy is greater than 0 or
               (b) i < n and the certificate is self-issued. */
            if (inhibit_any_policy > 0 || (i < n && is_self_issued)) {
                for (policy_ix = 0; policy_ix < policy_count; ++policy_ix) {
                    const SecCEPolicyInformation *policy = &cp->policies[policy_ix];
                    oid_t p_oid = policy->policyIdentifier;
                    policy_qualifier_t p_q = &policy->policyQualifiers;
                    if (oid_equal(p_oid, oidAnyPolicy)) {
                        policy_tree_walk_depth(pvc->valid_policy_tree, i - 1,
                            policy_tree_add_expected, (void *)p_q);
                    }
                }
            }
            policy_tree_prune_childless(&pvc->valid_policy_tree, i - 1);
            /* (e) */
            if (!cp) {
                if (pvc->valid_policy_tree)
                    policy_tree_prune(&pvc->valid_policy_tree);
            }
        }
        /* (f) Verify that either explicit_policy is greater than 0 or the
           valid_policy_tree is not equal to NULL. */
        if (!pvc->valid_policy_tree && explicit_policy == 0) {
            /* valid_policy_tree is empty and explicit policy is 0, illegal. */
            secnotice("policy", "policy tree failure");
            if (!SecPVCSetResultForced(pvc, key /* @@@ Need custom key */, n - i, kCFBooleanFalse, true)) {
                goto errOut;
            }
        }
        /* If Last Cert in Path */
        if (i == n)
            break;

        /* Prepare for Next Cert */
#if POLICY_MAPPING
        /* (a) verify that anyPolicy does not appear as an
           issuerDomainPolicy or a subjectDomainPolicy */
        const SecCEPolicyMappings *pm = SecCertificateGetPolicyMappings(cert);
        if (pm && pm->present) {
            size_t mapping_ix, mapping_count = pm->numMappings;
            for (mapping_ix = 0; mapping_ix < mapping_count; ++mapping_ix) {
                const SecCEPolicyMapping *mapping = &pm->mappings[mapping_ix];
                if (oid_equal(mapping->issuerDomainPolicy, oidAnyPolicy)
                    || oid_equal(mapping->subjectDomainPolicy, oidAnyPolicy)) {
                    /* Policy mapping uses anyPolicy, illegal. */
                    if (!SecPVCSetResultForced(pvc, key /* @@@ Need custom key */, n - i, kCFBooleanFalse, true)) {
                        goto errOut;
                    }
                }
            }

            /* (b) */
            /* (1) If the policy_mapping variable is greater than 0 */
            if (policy_mapping > 0 && pvc->valid_policy_tree) {
                if (!policy_tree_walk_depth(pvc->valid_policy_tree, i,
                    policy_tree_map_if_match, (void *)pm)) {
                    /* If no node of depth i in the valid_policy_tree has a valid_policy of ID-P but there is a node of depth i with a valid_policy of anyPolicy, then generate a child node of the node of depth i-1. */
                    policy_tree_walk_depth(pvc->valid_policy_tree, i, policy_tree_map_if_any, (void *)pm);
                }
            } else if (pvc->valid_policy_tree) {
                /* (i)    delete each node of depth i in the valid_policy_tree
                   where ID-P is the valid_policy. */
                policy_tree_walk_depth(pvc->valid_policy_tree, i,
                    policy_tree_map_delete_if_match, (void *)pm);
                /* (ii)   If there is a node in the valid_policy_tree of depth
                   i-1 or less without any child nodes, delete that
                   node.  Repeat this step until there are no nodes of
                   depth i-1 or less without children. */
                policy_tree_prune_childless(&pvc->valid_policy_tree, i - 1);
            }
        }
#endif /* POLICY_MAPPING */
        /* (c)(d)(e)(f) */
        //working_issuer_name = SecCertificateGetNormalizedSubjectContent(cert);
        //working_public_key = SecCertificateCopyPublicKey(cert);
        //working_public_key_parameters = SecCertificateCopyPublicKeyParameters(cert);
        //working_public_key_algorithm = SecCertificateCopyPublicKeyAlgorithm(cert);
#if POLICY_SUBTREES
        /* (g) If a name constraints extension is included in the certificate, modify the permitted_subtrees and excluded_subtrees state variables.
         */
        CFArrayRef permitted_subtrees_in_cert = SecCertificateGetPermittedSubtrees(cert);
        if (permitted_subtrees_in_cert) {
            SecNameConstraintsIntersectSubtrees(permitted_subtrees, permitted_subtrees_in_cert);
        }

        // could do something smart here to avoid inserting the exact same constraint
        CFArrayRef excluded_subtrees_in_cert = SecCertificateGetExcludedSubtrees(cert);
        if (excluded_subtrees_in_cert) {
            CFIndex num_trees = CFArrayGetCount(excluded_subtrees_in_cert);
            CFRange range = { 0, num_trees };
            CFArrayAppendArray(excluded_subtrees, excluded_subtrees_in_cert, range);
        }
#endif
        /* (h) */
        if (!is_self_issued) {
            if (explicit_policy)
                explicit_policy--;
            if (policy_mapping)
                policy_mapping--;
            if (inhibit_any_policy)
                inhibit_any_policy--;
        }
        /* (i) */
        const SecCEPolicyConstraints *pc =
            SecCertificateGetPolicyConstraints(cert);
        if (pc) {
            if (pc->requireExplicitPolicyPresent
                && pc->requireExplicitPolicy < explicit_policy) {
                explicit_policy = pc->requireExplicitPolicy;
            }
            if (pc->inhibitPolicyMappingPresent
                && pc->inhibitPolicyMapping < policy_mapping) {
                policy_mapping = pc->inhibitPolicyMapping;
            }
        }
        /* (j) */
        const SecCEInhibitAnyPolicy *iap = SecCertificateGetInhibitAnyPolicySkipCerts(cert);
        if (iap && iap->skipCerts < inhibit_any_policy) {
            inhibit_any_policy = iap->skipCerts;
        }
        /* (k) */
		const SecCEBasicConstraints *bc =
			SecCertificateGetBasicConstraints(cert);
#if 0 /* Checked in chain builder pre signature verify already. */
        if (!bc || !bc->isCA) {
            /* Basic constraints not present or not marked as isCA, illegal. */
            if (!SecPVCSetResult(pvc, kSecPolicyCheckBasicConstraints,
                                 n - i, kCFBooleanFalse)) {
                goto errOut;
            }
        }
#endif
        /* (l) */
        if (!is_self_issued) {
            if (max_path_length > 0) {
                max_path_length--;
            } else {
                /* max_path_len exceeded, illegal. */
                if (!SecPVCSetResult(pvc, kSecPolicyCheckBasicConstraints,
                                     n - i, kCFBooleanFalse)) {
                    goto errOut;
                }
            }
        }
        /* (m) */
        if (bc && bc->pathLenConstraintPresent
            && bc->pathLenConstraint < max_path_length) {
            max_path_length = bc->pathLenConstraint;
        }
#if 0 /* Checked in chain builder pre signature verify already. */
        /* (n) If a key usage extension is present, verify that the keyCertSign bit is set. */
        SecKeyUsage keyUsage = SecCertificateGetKeyUsage(cert);
        if (keyUsage && !(keyUsage & kSecKeyUsageKeyCertSign)) {
            if (!SecPVCSetResultForced(pvc, kSecPolicyCheckKeyUsage,
                                       n - i, kCFBooleanFalse, true)) {
                goto errOut;
            }
        }
#endif
        /* (o) Recognize and process any other critical extension present in the certificate. Process any other recognized non-critical extension present in the certificate that is relevant to path processing. */
        if (SecCertificateHasUnknownCriticalExtension(cert)) {
			/* Certificate contains one or more unknown critical extensions. */
			if (!SecPVCSetResult(pvc, kSecPolicyCheckCriticalExtensions,
                                 n - i, kCFBooleanFalse)) {
                goto errOut;
            }
		}
    } /* end loop over certs in path */
    /* Wrap up */
    cert = SecPVCGetCertificateAtIndex(pvc, 0);
    /* (a) */
    if (explicit_policy)
        explicit_policy--;
    /* (b) */
    const SecCEPolicyConstraints *pc = SecCertificateGetPolicyConstraints(cert);
    if (pc) {
        if (pc->requireExplicitPolicyPresent
            && pc->requireExplicitPolicy == 0) {
            explicit_policy = 0;
        }
    }
    /* (c) */
    //working_public_key = SecCertificateCopyPublicKey(cert);
    /* (d) */
    /* If the subjectPublicKeyInfo field of the certificate contains an algorithm field with null parameters or parameters are omitted, compare the certificate subjectPublicKey algorithm to the working_public_key_algorithm. If the certificate subjectPublicKey algorithm and the
working_public_key_algorithm are different, set the working_public_key_parameters to null. */
    //working_public_key_parameters = SecCertificateCopyPublicKeyParameters(cert);
    /* (e) */
    //working_public_key_algorithm = SecCertificateCopyPublicKeyAlgorithm(cert);
    /* (f) Recognize and process any other critical extension present in the certificate n. Process any other recognized non-critical extension present in certificate n that is relevant to path processing. */
    if (SecCertificateHasUnknownCriticalExtension(cert)) {
        /* Certificate contains one or more unknown critical extensions. */
        if (!SecPVCSetResult(pvc, kSecPolicyCheckCriticalExtensions,
                             0, kCFBooleanFalse)) {
            goto errOut;
        }
    }
    /* (g) Calculate the intersection of the valid_policy_tree and the user-initial-policy-set, as follows */

    if (pvc->valid_policy_tree) {
#if !defined(NDEBUG)
        policy_tree_dump(pvc->valid_policy_tree);
#endif
        /* (g3c4) */
        //policy_tree_prune_childless(&pvc->valid_policy_tree, n - 1);
    }

    /* If either (1) the value of explicit_policy variable is greater than
       zero or (2) the valid_policy_tree is not NULL, then path processing
       has succeeded. */
    if (!pvc->valid_policy_tree && explicit_policy == 0) {
        /* valid_policy_tree is empty and explicit policy is 0, illegal. */
        secnotice("policy", "policy tree failure");
        if (!SecPVCSetResultForced(pvc, key /* @@@ Need custom key */, 0, kCFBooleanFalse, true)) {
            goto errOut;
        }
    }

errOut:
    CFReleaseNull(permitted_subtrees);
    CFReleaseNull(excluded_subtrees);
}

static policy_set_t policies_for_cert(SecCertificateRef cert) {
    policy_set_t policies = NULL;
    const SecCECertificatePolicies *cp =
        SecCertificateGetCertificatePolicies(cert);
    size_t policy_ix, policy_count = cp ? cp->numPolicies : 0;
    for (policy_ix = 0; policy_ix < policy_count; ++policy_ix) {
        policy_set_add(&policies, &cp->policies[policy_ix].policyIdentifier);
    }
    return policies;
}

static void SecPolicyCheckEV(SecPVCRef pvc,
	CFStringRef key) {
	CFIndex ix, count = SecPVCGetCertificateCount(pvc);
    policy_set_t valid_policies = NULL;

    /* 6.1.7. Key Usage Purposes */
    if (count) {
        CFAbsoluteTime jul2016 = 489024000;
        SecCertificateRef leaf = SecPVCGetCertificateAtIndex(pvc, 0);
        if (SecCertificateNotValidBefore(leaf) > jul2016 && count < 3) {
            /* Root CAs may not sign subscriber certificates after 30 June 2016. */
            if (SecPVCSetResultForced(pvc, key,
                    0, kCFBooleanFalse, true)) {
                return;
            }
        }
    }

	for (ix = 0; ix < count; ++ix) {
		SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, ix);
        policy_set_t policies = policies_for_cert(cert);
        if (ix == 0) {
            /* Subscriber */
            /* anyPolicy in the leaf isn't allowed for EV, so only init
               valid_policies if we have real policies. */
            if (!policy_set_contains(policies, &oidAnyPolicy)) {
                valid_policies = policies;
                policies = NULL;
            }
        } else if (ix < count - 1) {
            /* Subordinate CA */
            if (!SecPolicySubordinateCACertificateCouldBeEV(cert)) {
                secnotice("ev", "subordinate certificate is not ev");
                if (SecPVCSetResultForced(pvc, key,
                    ix, kCFBooleanFalse, true)) {
                    policy_set_free(valid_policies);
                    policy_set_free(policies);
                    return;
                }
            }
            policy_set_intersect(&valid_policies, policies);
        } else {
            /* Root CA */
            if (!SecPolicyRootCACertificateIsEV(cert, valid_policies)) {
                secnotice("ev", "anchor certificate is not ev");
                if (SecPVCSetResultForced(pvc, key,
                    ix, kCFBooleanFalse, true)) {
                    policy_set_free(valid_policies);
                    policy_set_free(policies);
                    return;
                }
            }
        }
        policy_set_free(policies);
        if (!valid_policies) {
            secnotice("ev", "valid_policies set is empty: chain not ev");
            /* If we ever get into a state where no policies are valid anymore
               this can't be an ev chain. */
            if (SecPVCSetResultForced(pvc, key,
                ix, kCFBooleanFalse, true)) {
                return;
            }
        }
	}

    policy_set_free(valid_policies);

    /* (a) EV Subscriber Certificates   Each EV Certificate issued by the CA to a
Subscriber MUST contain an OID defined by the CA in the certificate’s
certificatePolicies extension that: (i) indicates which CA policy statement relates
to that certificate, (ii) asserts the CA’s adherence to and compliance with these
Guidelines, and (iii), by pre-agreement with the Application Software Vendor,
marks the certificate as being an EV Certificate.
(b) EV Subordinate CA Certificates
(1) Certificates issued to Subordinate CAs that are not controlled by the issuing
CA MUST contain one or more OIDs defined by the issuing CA that
explicitly identify the EV Policies that are implemented by the Subordinate
CA;
(2) Certificates issued to Subordinate CAs that are controlled by the Root CA
MAY contain the special anyPolicy OID (2.5.29.32.0).
(c) Root CA Certificates  Root CA Certificates SHOULD NOT contain the
certificatePolicies or extendedKeyUsage extensions.
*/
}


/*
 * MARK: Certificate Transparency support
 */

/***

struct {
    Version sct_version;        // 1 byte
    LogID id;                   // 32 bytes
    uint64 timestamp;           // 8 bytes
    CtExtensions extensions;    // 2 bytes len field, + n bytes data
    digitally-signed struct {   // 1 byte hash alg, 1 byte sig alg, n bytes signature
        Version sct_version;
        SignatureType signature_type = certificate_timestamp;
        uint64 timestamp;
        LogEntryType entry_type;
        select(entry_type) {
        case x509_entry: ASN.1Cert;
        case precert_entry: PreCert;
        } signed_entry;
        CtExtensions extensions;
    };
} SignedCertificateTimestamp;

***/

#include <Security/SecureTransportPriv.h>

static const
SecAsn1Oid *oidForSigAlg(SSL_HashAlgorithm hash, SSL_SignatureAlgorithm alg)
{
    switch(alg) {
        case SSL_SignatureAlgorithmRSA:
            switch (hash) {
                case SSL_HashAlgorithmSHA1:
                    return &CSSMOID_SHA1WithRSA;
                case SSL_HashAlgorithmSHA256:
                    return &CSSMOID_SHA256WithRSA;
                case SSL_HashAlgorithmSHA384:
                    return &CSSMOID_SHA384WithRSA;
                default:
                    break;
            }
        case SSL_SignatureAlgorithmECDSA:
            switch (hash) {
                case SSL_HashAlgorithmSHA1:
                    return &CSSMOID_ECDSA_WithSHA1;
                case SSL_HashAlgorithmSHA256:
                    return &CSSMOID_ECDSA_WithSHA256;
                case SSL_HashAlgorithmSHA384:
                    return &CSSMOID_ECDSA_WithSHA384;
                default:
                    break;
            }
        default:
            break;
    }

    return NULL;
}


static size_t SSLDecodeUint16(const uint8_t *p)
{
    return (p[0]<<8 | p[1]);
}

static uint8_t *SSLEncodeUint16(uint8_t *p, size_t len)
{
    p[0] = (len >> 8)&0xff;
    p[1] = (len & 0xff);
    return p+2;
}

static uint8_t *SSLEncodeUint24(uint8_t *p, size_t len)
{
    p[0] = (len >> 16)&0xff;
    p[1] = (len >> 8)&0xff;
    p[2] = (len & 0xff);
    return p+3;
}


static
uint64_t SSLDecodeUint64(const uint8_t *p)
{
    uint64_t u = 0;
    for(int i=0; i<8; i++) {
        u=(u<<8)|p[0];
        p++;
    }
    return u;
}

#include <libDER/DER_CertCrl.h>
#include <libDER/DER_Encode.h>
#include <libDER/asn1Types.h>


static CFDataRef copy_x509_entry_from_chain(SecPVCRef pvc)
{
    SecCertificateRef leafCert = SecPVCGetCertificateAtIndex(pvc, 0);

    CFMutableDataRef data = CFDataCreateMutable(kCFAllocatorDefault, 3+SecCertificateGetLength(leafCert));

    CFDataSetLength(data, 3+SecCertificateGetLength(leafCert));

    uint8_t *q = CFDataGetMutableBytePtr(data);
    q = SSLEncodeUint24(q, SecCertificateGetLength(leafCert));
    memcpy(q, SecCertificateGetBytePtr(leafCert), SecCertificateGetLength(leafCert));

    return data;
}


static CFDataRef copy_precert_entry_from_chain(SecPVCRef pvc)
{
    SecCertificateRef leafCert = NULL;
    SecCertificateRef issuer = NULL;
    CFDataRef issuerKeyHash = NULL;
    CFDataRef tbs_precert = NULL;
    CFMutableDataRef data= NULL;

    require_quiet(SecPVCGetCertificateCount(pvc)>=2, out); //we need the issuer key for precerts.
    leafCert = SecPVCGetCertificateAtIndex(pvc, 0);
    issuer = SecPVCGetCertificateAtIndex(pvc, 1);

    require(leafCert, out);
    require(issuer, out); // Those two would likely indicate an internal error, since we already checked the chain length above.
    issuerKeyHash = SecCertificateCopySubjectPublicKeyInfoSHA256Digest(issuer);
    tbs_precert = SecCertificateCopyPrecertTBS(leafCert);

    require(issuerKeyHash, out);
    require(tbs_precert, out);
    data = CFDataCreateMutable(kCFAllocatorDefault, CFDataGetLength(issuerKeyHash) + 3 + CFDataGetLength(tbs_precert));
    CFDataSetLength(data, CFDataGetLength(issuerKeyHash) + 3 + CFDataGetLength(tbs_precert));

    uint8_t *q = CFDataGetMutableBytePtr(data);
    memcpy(q, CFDataGetBytePtr(issuerKeyHash), CFDataGetLength(issuerKeyHash)); q += CFDataGetLength(issuerKeyHash); // issuer key hash
    q = SSLEncodeUint24(q, CFDataGetLength(tbs_precert));
    memcpy(q, CFDataGetBytePtr(tbs_precert), CFDataGetLength(tbs_precert));

out:
    CFReleaseSafe(issuerKeyHash);
    CFReleaseSafe(tbs_precert);
    return data;
}

static
CFAbsoluteTime TimestampToCFAbsoluteTime(uint64_t ts)
{
    return (ts / 1000) - kCFAbsoluteTimeIntervalSince1970;
}

static
uint64_t TimestampFromCFAbsoluteTime(CFAbsoluteTime at)
{
    return (uint64_t)(at + kCFAbsoluteTimeIntervalSince1970) * 1000;
}




/*
   If the 'sct' is valid, add it to the validatingLogs dictionary.

   Inputs:
    - validatingLogs: mutable dictionary to which to add the log that validate this SCT.
    - sct: the SCT date
    - entry_type: 0 for x509 cert, 1 for precert.
    - entry: the cert or precert data.
    - vt: verification time timestamp (as used in SCTs: ms since 1970 Epoch)
    - trustedLog: Dictionary contain the Trusted Logs.

   The SCT is valid if:
    - It decodes properly.
    - Its timestamp is less than 'verifyTime'.
    - It is signed by a log in 'trustedLogs'.
    - If entry_type = 0, the log must be currently qualified.
    - If entry_type = 1, the log may be expired.

   If the SCT is valid, it's added to the validatinLogs dictionary using the log dictionary as the key, and the timestamp as value.
   If an entry for the same log already existing in the dictionary, the entry is replaced only if the timestamp of this SCT is earlier.

 */


static CFDictionaryRef getSCTValidatingLog(CFDataRef sct, int entry_type, CFDataRef entry, uint64_t vt, CFArrayRef trustedLogs, CFAbsoluteTime *sct_at)
{
    uint8_t version;
    const uint8_t *logID;
    const uint8_t *timestampData;
    uint64_t timestamp;
    size_t extensionsLen;
    const uint8_t *extensionsData;
    uint8_t hashAlg;
    uint8_t sigAlg;
    size_t signatureLen;
    const uint8_t *signatureData;
    SecKeyRef pubKey = NULL;
    uint8_t *signed_data = NULL;
    const SecAsn1Oid *oid = NULL;
    SecAsn1AlgId algId;
    CFDataRef logIDData = NULL;
    CFDictionaryRef result = 0;

    const uint8_t *p = CFDataGetBytePtr(sct);
    size_t len = CFDataGetLength(sct);

    require(len>=43, out);

    version = p[0]; p++; len--;
    logID = p; p+=32; len-=32;
    timestampData = p; p+=8; len-=8;
    extensionsLen = SSLDecodeUint16(p); p+=2; len-=2;

    require(len>=extensionsLen, out);
    extensionsData = p; p+=extensionsLen; len-=extensionsLen;

    require(len>=4, out);
    hashAlg=p[0]; p++; len--;
    sigAlg=p[0]; p++; len--;
    signatureLen = SSLDecodeUint16(p); p+=2; len-=2;
    require(len==signatureLen, out); /* We do not tolerate any extra data after the signature */
    signatureData = p;

    /* verify version: only v1(0) is supported */
    if(version!=0) {
        secerror("SCT version unsupported: %d\n", version);
        goto out;
    }

    /* verify timestamp not in the future */
    timestamp = SSLDecodeUint64(timestampData);
    if(timestamp > vt) {
        secerror("SCT is in the future: %llu > %llu\n", timestamp, vt);
        goto out;
    }

    uint8_t *q;

    /* signed entry */
    size_t signed_data_len = 12 + CFDataGetLength(entry) + 2 + extensionsLen ;
    signed_data = malloc(signed_data_len);
    require(signed_data, out);
    q = signed_data;
    *q++ = version;
    *q++ = 0; // certificate_timestamp
    memcpy(q, timestampData, 8); q+=8;
    q = SSLEncodeUint16(q, entry_type); // logentry type: 0=cert 1=precert
    memcpy(q, CFDataGetBytePtr(entry), CFDataGetLength(entry)); q += CFDataGetLength(entry);
    q = SSLEncodeUint16(q, extensionsLen);
    memcpy(q, extensionsData, extensionsLen);

    logIDData = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, logID, 32, kCFAllocatorNull);

    CFDictionaryRef logData = CFArrayGetValueMatching(trustedLogs, ^bool(const void *dict) {
        const void *key_data;
        if(!isDictionary(dict)) return false;
        if(!CFDictionaryGetValueIfPresent(dict, CFSTR("key"), &key_data)) return false;
        if(!isData(key_data)) return false;
        CFDataRef valueID = SecSHA256DigestCreateFromData(kCFAllocatorDefault, (CFDataRef)key_data);
        bool result = (bool)(CFDataCompare(logIDData, valueID)==kCFCompareEqualTo);
        CFReleaseSafe(valueID);
        return result;
    });
    require(logData, out);

    if(entry_type==0) {
        // For external SCTs, only keep SCTs from currently valid logs.
        require(!CFDictionaryContainsKey(logData, CFSTR("expiry")), out);
    }

    CFDataRef logKeyData = CFDictionaryGetValue(logData, CFSTR("key"));
    require(logKeyData, out); // This failing would be an internal logic error
    pubKey = SecKeyCreateFromSubjectPublicKeyInfoData(kCFAllocatorDefault, logKeyData);
    require(pubKey, out);

    oid = oidForSigAlg(hashAlg, sigAlg);
    require(oid, out);

    algId.algorithm = *oid;
    algId.parameters.Data = NULL;
    algId.parameters.Length = 0;

    if(SecKeyDigestAndVerify(pubKey, &algId, signed_data, signed_data_len, signatureData, signatureLen)==0) {
        *sct_at = TimestampToCFAbsoluteTime(timestamp);
        result = logData;
    } else {
        secerror("SCT signature failed (log=%@)\n", logData);
    }

out:
    CFReleaseSafe(logIDData);
    CFReleaseSafe(pubKey);
    free(signed_data);
    return result;
}


static void addValidatingLog(CFMutableDictionaryRef validatingLogs, CFDictionaryRef log, CFAbsoluteTime sct_at)
{
    CFDateRef validated_time = CFDictionaryGetValue(validatingLogs, log);

    if(validated_time==NULL || (sct_at < CFDateGetAbsoluteTime(validated_time))) {
        CFDateRef sct_time = CFDateCreate(kCFAllocatorDefault, sct_at);
        CFDictionarySetValue(validatingLogs, log, sct_time);
        CFReleaseSafe(sct_time);
    }
}

static CFArrayRef copy_ocsp_scts(SecPVCRef pvc)
{
    CFMutableArrayRef SCTs = NULL;
    SecCertificateRef leafCert = NULL;
    SecCertificateRef issuer = NULL;
    CFArrayRef ocspResponsesData = NULL;
    SecOCSPRequestRef ocspRequest = NULL;

    ocspResponsesData = SecPathBuilderCopyOCSPResponses(pvc->builder);
    require_quiet(ocspResponsesData, out);

    require_quiet(SecPVCGetCertificateCount(pvc)>=2, out); //we need the issuer key for precerts.
    leafCert = SecPVCGetCertificateAtIndex(pvc, 0);
    issuer = SecPVCGetCertificateAtIndex(pvc, 1);

    require(leafCert, out);
    require(issuer, out); // not quiet: Those two would likely indicate an internal error, since we already checked the chain length above.
    ocspRequest = SecOCSPRequestCreate(leafCert, issuer);

    SCTs = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    require(SCTs, out);

    CFArrayForEach(ocspResponsesData, ^(const void *value) {
        /* TODO: Should the builder already have the appropriate SecOCSPResponseRef ? */
        SecOCSPResponseRef ocspResponse = SecOCSPResponseCreate(value);
        if(ocspResponse && SecOCSPGetResponseStatus(ocspResponse)==kSecOCSPSuccess) {
            SecOCSPSingleResponseRef ocspSingleResponse = SecOCSPResponseCopySingleResponse(ocspResponse, ocspRequest);
            if(ocspSingleResponse) {
                CFArrayRef singleResponseSCTs = SecOCSPSingleResponseCopySCTs(ocspSingleResponse);
                if(singleResponseSCTs) {
                    CFArrayAppendArray(SCTs, singleResponseSCTs, CFRangeMake(0, CFArrayGetCount(singleResponseSCTs)));
                    CFRelease(singleResponseSCTs);
                }
                SecOCSPSingleResponseDestroy(ocspSingleResponse);
            }
        }
        if(ocspResponse) SecOCSPResponseFinalize(ocspResponse);
    });

    if(CFArrayGetCount(SCTs)==0) {
        CFReleaseNull(SCTs);
    }

out:
    CFReleaseSafe(ocspResponsesData);
    if(ocspRequest)
        SecOCSPRequestFinalize(ocspRequest);

    return SCTs;
}

static void SecPolicyCheckCT(SecPVCRef pvc, CFStringRef key)
{
    SecCertificateRef leafCert = SecPVCGetCertificateAtIndex(pvc, 0);
    CFArrayRef embeddedScts = SecCertificateCopySignedCertificateTimestamps(leafCert);
    CFArrayRef builderScts = SecPathBuilderCopySignedCertificateTimestamps(pvc->builder);
    CFArrayRef trustedLogs = SecPathBuilderCopyTrustedLogs(pvc->builder);
    CFArrayRef ocspScts = copy_ocsp_scts(pvc);
    CFDataRef precertEntry = copy_precert_entry_from_chain(pvc);
    CFDataRef x509Entry = copy_x509_entry_from_chain(pvc);

    // This eventually contain list of logs who validated the SCT.
    CFMutableDictionaryRef currentLogsValidatingScts = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFMutableDictionaryRef logsValidatingEmbeddedScts = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    uint64_t vt = TimestampFromCFAbsoluteTime(pvc->verifyTime);

    __block bool at_least_one_currently_valid_external = 0;
    __block bool at_least_one_currently_valid_embedded = 0;

    require(logsValidatingEmbeddedScts, out);
    require(currentLogsValidatingScts, out);

    if(trustedLogs) { // Don't bother trying to validate SCTs if we don't have any trusted logs.
        if(embeddedScts && precertEntry) { // Don't bother if we could not get the precert.
            CFArrayForEach(embeddedScts, ^(const void *value){
                CFAbsoluteTime sct_at;
                CFDictionaryRef log = getSCTValidatingLog(value, 1, precertEntry, vt, trustedLogs, &sct_at);
                if(log) {
                    addValidatingLog(logsValidatingEmbeddedScts, log, sct_at);
                    if(!CFDictionaryContainsKey(log, CFSTR("expiry"))) {
                        addValidatingLog(currentLogsValidatingScts, log, sct_at);
                        at_least_one_currently_valid_embedded = true;
                    }
                }
            });
        }

        if(builderScts && x509Entry) { // Don't bother if we could not get the cert.
            CFArrayForEach(builderScts, ^(const void *value){
                CFAbsoluteTime sct_at;
                CFDictionaryRef log = getSCTValidatingLog(value, 0, x509Entry, vt, trustedLogs, &sct_at);
                if(log) {
                    addValidatingLog(currentLogsValidatingScts, log, sct_at);
                    at_least_one_currently_valid_external = true;
                }
            });
        }

        if(ocspScts && x509Entry) {
            CFArrayForEach(ocspScts, ^(const void *value){
                CFAbsoluteTime sct_at;
                CFDictionaryRef log = getSCTValidatingLog(value, 0, x509Entry, vt, trustedLogs, &sct_at);
                if(log) {
                    addValidatingLog(currentLogsValidatingScts, log, sct_at);
                    at_least_one_currently_valid_external = true;
                }
            });
        }
    }


    /* We now have 2 sets of logs that validated those SCTS, count them and make a final decision.

     Current Policy:
     is_ct = (A1 AND A2) OR (B1 AND B2).

     A1: embedded SCTs from 2+ to 5+ logs valid at issuance time
     A2: At least one embedded SCT from a currently valid log.

     B1: SCTs from 2 currently valid logs (from any source)
     B2: At least 1 external SCT from a currently valid log.

     */

    pvc->is_ct = false;

    if(at_least_one_currently_valid_external && CFDictionaryGetCount(currentLogsValidatingScts)>=2) {
        pvc->is_ct = true;
    } else if(at_least_one_currently_valid_embedded) {
        __block CFAbsoluteTime issuanceTime = pvc->verifyTime;
        __block int lifetime; // in Months
        __block unsigned once_or_current_qualified_embedded = 0;

        /* Calculate issuance time base on timestamp of SCTs from current logs */
        CFDictionaryForEach(currentLogsValidatingScts, ^(const void *key, const void *value) {
            CFDictionaryRef log = key;
            if(!CFDictionaryContainsKey(log, CFSTR("expiry"))) {
                // Log is still qualified
                CFDateRef ts = (CFDateRef) value;
                CFAbsoluteTime timestamp = CFDateGetAbsoluteTime(ts);
                if(timestamp < issuanceTime) {
                    issuanceTime = timestamp;
                }
            }
        });

        /* Count Logs */
        CFDictionaryForEach(logsValidatingEmbeddedScts, ^(const void *key, const void *value) {
            CFDictionaryRef log = key;
            CFDateRef ts = value;
            CFDateRef expiry = CFDictionaryGetValue(log, CFSTR("expiry"));
            if(expiry == NULL || CFDateCompare(ts, expiry, NULL) == kCFCompareLessThan) {
                once_or_current_qualified_embedded++;
            }
        });

        SecCFCalendarDoWithZuluCalendar(^(CFCalendarRef zuluCalendar) {
            int _lifetime;
            CFCalendarGetComponentDifference(zuluCalendar,
                                             SecCertificateNotValidBefore(leafCert),
                                             SecCertificateNotValidAfter(leafCert),
                                             0, "M", &_lifetime);
            lifetime = _lifetime;
        });

        unsigned requiredEmbeddedSctsCount;

        if (lifetime < 15) {
            requiredEmbeddedSctsCount = 2;
        } else if (lifetime <= 27) {
            requiredEmbeddedSctsCount = 3;
        } else if (lifetime <= 39) {
            requiredEmbeddedSctsCount = 4;
        } else {
            requiredEmbeddedSctsCount = 5;
        }

        if(once_or_current_qualified_embedded >= requiredEmbeddedSctsCount){
            pvc->is_ct = true;
        }
    }

out:
    CFReleaseSafe(logsValidatingEmbeddedScts);
    CFReleaseSafe(currentLogsValidatingScts);
    CFReleaseSafe(builderScts);
    CFReleaseSafe(embeddedScts);
    CFReleaseSafe(ocspScts);
    CFReleaseSafe(precertEntry);
    CFReleaseSafe(trustedLogs);
    CFReleaseSafe(x509Entry);
}

static bool checkPolicyOidData(SecPVCRef pvc, CFDataRef oid) {
	CFIndex ix, count = SecPVCGetCertificateCount(pvc);
    DERItem	key_value;
    key_value.data = (DERByte *)CFDataGetBytePtr(oid);
    key_value.length = (DERSize)CFDataGetLength(oid);

    for (ix = 0; ix < count; ix++) {
        SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, ix);
        policy_set_t policies = policies_for_cert(cert);

        if (policy_set_contains(policies, &key_value)) {
            return true;
        }
    }
    return false;
}

static void SecPolicyCheckCertificatePolicyOid(SecPVCRef pvc, CFStringRef key)
{
    SecPolicyRef policy = SecPVCGetPolicy(pvc);
    CFTypeRef value = CFDictionaryGetValue(policy->_options, key);
    bool result = false;

	if (CFGetTypeID(value) == CFDataGetTypeID())
	{
        result = checkPolicyOidData(pvc, value);
    } else if (CFGetTypeID(value) == CFStringGetTypeID()) {
        CFDataRef dataOid = SecCertificateCreateOidDataFromString(NULL, value);
        if (dataOid) {
            result = checkPolicyOidData(pvc, dataOid);
            CFRelease(dataOid);
        }
    }
    if(!result) {
		SecPVCSetResult(pvc, key, 0, kCFBooleanFalse);
    }
}


static void SecPolicyCheckRevocation(SecPVCRef pvc,
	CFStringRef key) {
    SecPolicyRef policy = SecPVCGetPolicy(pvc);
    CFTypeRef value = CFDictionaryGetValue(policy->_options, key);
    if (isString(value)) {
        SecPVCSetCheckRevocation(pvc, value);
    }
}

static void SecPolicyCheckRevocationResponseRequired(SecPVCRef pvc,
	CFStringRef key) {
    SecPVCSetCheckRevocationResponseRequired(pvc);
}

static void SecPolicyCheckRevocationOnline(SecPVCRef pvc, CFStringRef key) {
    SecPVCSetCheckRevocationOnline(pvc);
}

static void SecPolicyCheckNoNetworkAccess(SecPVCRef pvc,
    CFStringRef key) {
    SecPolicyRef policy = SecPVCGetPolicy(pvc);
    CFTypeRef value = CFDictionaryGetValue(policy->_options, key);
    if (value == kCFBooleanTrue) {
        SecPathBuilderSetCanAccessNetwork(pvc->builder, false);
    } else {
        SecPathBuilderSetCanAccessNetwork(pvc->builder, true);
    }
}

static void SecPolicyCheckWeakIntermediates(SecPVCRef pvc,
    CFStringRef key) {
    CFIndex ix, count = SecPVCGetCertificateCount(pvc);
    for (ix = 1; ix < count - 1; ++ix) {
        SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, ix);
        if (cert && SecCertificateIsWeakKey(cert)) {
            /* Intermediate certificate has a weak key. */
            if (!SecPVCSetResult(pvc, key, ix, kCFBooleanFalse))
                return;
        }
    }
}

static void SecPolicyCheckWeakLeaf(SecPVCRef pvc,
    CFStringRef key) {
    SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, 0);
    if (cert && SecCertificateIsWeakKey(cert)) {
        /* Leaf certificate has a weak key. */
        if (!SecPVCSetResult(pvc, key, 0, kCFBooleanFalse))
            return;
    }
}

static void SecPolicyCheckWeakRoot(SecPVCRef pvc,
    CFStringRef key) {
    CFIndex ix, count = SecPVCGetCertificateCount(pvc);
    ix = count - 1;
    SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, ix);
    if (cert && SecCertificateIsWeakKey(cert)) {
        /* Root certificate has a weak key. */
        if (!SecPVCSetResult(pvc, key, ix, kCFBooleanFalse))
            return;
    }
}

static void SecPolicyCheckKeySize(SecPVCRef pvc, CFStringRef key) {
    CFIndex ix, count = SecPVCGetCertificateCount(pvc);
    SecPolicyRef policy = SecPVCGetPolicy(pvc);
    CFDictionaryRef keySizes = CFDictionaryGetValue(policy->_options, key);
    for (ix = 0; ix < count; ++ix) {
        SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, ix);
        if (!SecCertificateIsAtLeastMinKeySize(cert, keySizes)) {
            if (!SecPVCSetResult(pvc, key, ix, kCFBooleanFalse))
                return;
        }
    }
}

static void SecPolicyCheckSignatureHashAlgorithms(SecPVCRef pvc,
    CFStringRef key) {
    CFIndex ix, count = SecPVCGetCertificateCount(pvc);
    SecPolicyRef policy = SecPVCGetPolicy(pvc);
    CFSetRef disallowedHashAlgorithms = CFDictionaryGetValue(policy->_options, key);
    for (ix = 0; ix < count; ++ix) {
        SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, ix);
        if (!SecPolicyCheckCertSignatureHashAlgorithms(cert, disallowedHashAlgorithms)) {
            if (!SecPVCSetResult(pvc, key, ix, kCFBooleanFalse))
                return;
        }
    }
}

static bool leaf_is_on_weak_hash_whitelist(SecPVCRef pvc) {
    SecCertificateRef leaf = SecPVCGetCertificateAtIndex(pvc, 0);
    require_quiet(leaf, out);

    /* Leaf certificates that expire before Jan 3 2017 can get a pass.
     * They must be updated before this goes live. */
    if (SecCertificateNotValidAfter(leaf) < 505200000.0) {
        return true;
    }

    /* And now a few special snowflakes */

    /* subject:/C=UK/O=Vodafone Group/CN=Vodafone (Corporate Domain 2009) */
    /* issuer :/C=IE/O=Baltimore/OU=CyberTrust/CN=Baltimore CyberTrust Root */
    /* Not After : Dec 19 17:25:36 2019 GMT */
    static const uint8_t vodafone[] = {
        0xC5, 0x0E, 0x88, 0xE5, 0x20, 0xA8, 0x10, 0x41, 0x1D, 0x63,
        0x4C, 0xB8, 0xF9, 0xCC, 0x93, 0x9B, 0xFD, 0x76, 0x93, 0x99
    };

    CFIndex intermediate_ix = SecPVCGetCertificateCount(pvc) - 2;
    require_quiet(intermediate_ix > 0, out);
    SecCertificateRef intermediate = SecPVCGetCertificateAtIndex(pvc, intermediate_ix);
    CFDataRef fingerprint = SecCertificateGetSHA1Digest(intermediate);
    require_quiet(fingerprint, out);
    const unsigned int len = 20;
    const uint8_t *dp = CFDataGetBytePtr(fingerprint);
    if (dp && (!memcmp(vodafone, dp, len))) {
        return true;
    }

out:
    return false;
}

static bool SecPVCKeyIsConstraintPolicyOption(SecPVCRef pvc, CFStringRef key);

static void SecPolicyCheckSystemTrustedWeakHash(SecPVCRef pvc,
    CFStringRef key) {
    CFIndex ix, count = SecPVCGetCertificateCount(pvc);
#if !NO_SERVER
    CFDataRef clientAuditToken = NULL;
    SecTaskRef task = NULL;
#endif
    CFStringRef signingIdentifier = NULL;

    /* Only for Safari and WebKit. */
#if NO_SERVER
    require_quiet(signingIdentifier = CFRetainSafe(CFBundleGetIdentifier(CFBundleGetMainBundle())), out);
#else
    require_quiet(clientAuditToken = SecPathBuilderCopyClientAuditToken(pvc->builder), out);
    audit_token_t auditToken = {};
    require(sizeof(auditToken) == CFDataGetLength(clientAuditToken), out);
    CFDataGetBytes(clientAuditToken, CFRangeMake(0, sizeof(auditToken)), (uint8_t *)&auditToken);
    require_quiet(task = SecTaskCreateWithAuditToken(NULL, auditToken), out);
    require_quiet(signingIdentifier = SecTaskCopySigningIdentifier(task, NULL), out);
#endif
    require_quiet(CFStringHasPrefix(signingIdentifier, CFSTR("com.apple.Safari")) ||
                  CFStringHasPrefix(signingIdentifier, CFSTR("com.apple.mobilesafari")) ||
                  CFStringHasPrefix(signingIdentifier, CFSTR("com.apple.WebKit.Networking")) ||
                  /* Or one of our test apps */
                  CFStringHasPrefix(signingIdentifier, CFSTR("com.apple.security.SecurityTests")) ||
                  CFStringHasPrefix(signingIdentifier, CFSTR("com.apple.security.SecurityDevTests")), out);

    Boolean keyInPolicy = false;
    CFArrayRef policies = pvc->policies;
    CFIndex policyIX, policyCount = CFArrayGetCount(policies);
    for (policyIX = 0; policyIX < policyCount; ++policyIX) {
		SecPolicyRef policy = (SecPolicyRef)CFArrayGetValueAtIndex(policies, policyIX);
        if (policy && CFDictionaryContainsKey(policy->_options, key)) {
            keyInPolicy = true;
        }
    }

    /* We only enforce this check when *both* of the following are true:
     *  1. One of the certs in the path has this usage constraint, and
     *  2. One of the policies in the PVC has this key
     * (As compared to normal policy options which require only one to be true..) */
    require_quiet(SecPVCKeyIsConstraintPolicyOption(pvc, key) &&
                  keyInPolicy, out);

    /* Ignore the anchor if it's trusted */
    if (SecCertificatePathIsAnchored(pvc->path)) {
        count--;
    }
    for (ix = 0; ix < count; ++ix) {
        SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, ix);
        if (SecCertificateIsWeakHash(cert)) {
            if (!leaf_is_on_weak_hash_whitelist(pvc)) {
                if (!SecPVCSetResult(pvc, key, ix, kCFBooleanFalse)) {
                    goto out;
                }
            }
        }
    }
out:
#if !NO_SERVER
    CFReleaseNull(clientAuditToken);
    CFReleaseNull(task);
#endif
    CFReleaseNull(signingIdentifier);
    return;
}

#define ENABLE_CRLS (TARGET_OS_MAC && !TARGET_OS_IPHONE)

// MARK: -
// MARK: SecRVCRef
/********************************************************
 ****************** SecRVCRef Functions *****************
 ********************************************************/
typedef struct OpaqueSecORVC *SecORVCRef;
#if ENABLE_CRLS
typedef struct OpaqueSecCRVC *SecCRVCRef;
#endif

/* Revocation verification context. */
struct OpaqueSecRVC {
    /* Pointer to the pvc for this revocation check */
    SecPVCRef pvc;

    /* Index of cert in pvc that this RVC is for 0 = leaf, etc. */
    CFIndex certIX;

    /* The OCSP Revocation verification context */
    SecORVCRef orvc;

#if ENABLE_CRLS
    SecCRVCRef crvc;
#endif

    /* Valid database info for this revocation check */
    SecValidInfoRef valid_info;

    bool done;
};
typedef struct OpaqueSecRVC *SecRVCRef;

// MARK: SecORVCRef
/********************************************************
 ****************** OCSP RVC Functions ******************
 ********************************************************/
const CFAbsoluteTime kSecDefaultOCSPResponseTTL = 24.0 * 60.0 * 60.0;
const CFAbsoluteTime kSecOCSPResponseOnlineTTL = 5.0 * 60.0;
#define OCSP_RESPONSE_TIMEOUT       (3 * NSEC_PER_SEC)

/* OCSP Revocation verification context. */
struct OpaqueSecORVC {
    /* Will contain the response data. */
    asynchttp_t http;

    /* Pointer to the pvc for this revocation check. */
    SecPVCRef pvc;

    /* Pointer to the generic rvc for this revocation check */
    SecRVCRef rvc;

    /* The ocsp request we send to each responder. */
    SecOCSPRequestRef ocspRequest;

    /* The freshest response we received so far, from stapling or cache or responder. */
    SecOCSPResponseRef ocspResponse;

    /* The best validated candidate single response we received so far, from stapling or cache or responder. */
    SecOCSPSingleResponseRef ocspSingleResponse;

    /* Index of cert in pvc that this RVC is for 0 = leaf, etc. */
    CFIndex certIX;

    /* Index in array returned by SecCertificateGetOCSPResponders() for current
       responder. */
    CFIndex responderIX;

    /* URL of current responder. */
    CFURLRef responder;

    /* Date until which this revocation status is valid. */
    CFAbsoluteTime nextUpdate;

    bool done;
};

static void SecORVCFinish(SecORVCRef orvc) {
    secdebug("alloc", "%p", orvc);
    asynchttp_free(&orvc->http);
    if (orvc->ocspRequest) {
        SecOCSPRequestFinalize(orvc->ocspRequest);
        orvc->ocspRequest = NULL;
    }
    if (orvc->ocspResponse) {
        SecOCSPResponseFinalize(orvc->ocspResponse);
        orvc->ocspResponse = NULL;
        if (orvc->ocspSingleResponse) {
            SecOCSPSingleResponseDestroy(orvc->ocspSingleResponse);
            orvc->ocspSingleResponse = NULL;
        }
    }
}

#define MAX_OCSP_RESPONDERS 3
#define OCSP_REQUEST_THRESHOLD 10

/* Return the next responder we should contact for this rvc or NULL if we
 exhausted them all. */
static CFURLRef SecORVCGetNextResponder(SecORVCRef rvc) {
    SecCertificateRef cert = SecPVCGetCertificateAtIndex(rvc->pvc, rvc->certIX);
    CFArrayRef ocspResponders = SecCertificateGetOCSPResponders(cert);
    if (ocspResponders) {
        CFIndex responderCount = CFArrayGetCount(ocspResponders);
        if (responderCount >= OCSP_REQUEST_THRESHOLD) {
            secnotice("rvc", "too many ocsp responders (%ld)", (long)responderCount);
            return NULL;
        }
        while (rvc->responderIX < responderCount && rvc->responderIX < MAX_OCSP_RESPONDERS) {
            CFURLRef responder = CFArrayGetValueAtIndex(ocspResponders, rvc->responderIX);
            rvc->responderIX++;
            CFStringRef scheme = CFURLCopyScheme(responder);
            if (scheme) {
                /* We only support http and https responders currently. */
                bool valid_responder = (CFEqual(CFSTR("http"), scheme) ||
                                        CFEqual(CFSTR("https"), scheme));
                CFRelease(scheme);
                if (valid_responder)
                    return responder;
            }
        }
    }
    return NULL;
}

/* Fire off an async http request for this certs revocation status, return
 false if request was queued, true if we're done. */
static bool SecORVCFetchNext(SecORVCRef rvc) {
    while ((rvc->responder = SecORVCGetNextResponder(rvc))) {
        CFDataRef request = SecOCSPRequestGetDER(rvc->ocspRequest);
        if (!request)
            goto errOut;

        secinfo("rvc", "Sending http ocsp request for cert %ld", rvc->certIX);
        if (!asyncHttpPost(rvc->responder, request, OCSP_RESPONSE_TIMEOUT, &rvc->http)) {
            /* Async request was posted, wait for reply. */
            return false;
        }
    }

errOut:
    rvc->done = true;
    return true;
}

/* Process a verified ocsp response for a given cert. Return true if the
 certificate status was obtained. */
static bool SecOCSPSingleResponseProcess(SecOCSPSingleResponseRef this,
                                         SecORVCRef rvc) {
    bool processed;
    switch (this->certStatus) {
        case CS_Good:
            secdebug("ocsp", "CS_Good for cert %" PRIdCFIndex, rvc->certIX);
            /* @@@ Mark cert as valid until a given date (nextUpdate if we have one)
             in the info dictionary. */
            //cert.revokeCheckGood(true);
            rvc->nextUpdate = this->nextUpdate == NULL_TIME ? this->thisUpdate + kSecDefaultOCSPResponseTTL : this->nextUpdate;
            processed = true;
            break;
        case CS_Revoked:
            secdebug("ocsp", "CS_Revoked for cert %" PRIdCFIndex, rvc->certIX);
            /* @@@ Mark cert as revoked (with reason) at revocation date in
             the info dictionary, or perhaps we should use a different key per
             reason?   That way a client using exceptions can ignore some but
             not all reasons. */
            SInt32 reason = this->crlReason;
            CFNumberRef cfreason = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &reason);
            SecPVCSetResultForced(rvc->pvc, kSecPolicyCheckRevocation, rvc->certIX,
                                  cfreason, true);
            if (rvc->pvc && rvc->pvc->info) {
                /* make the revocation reason available in the trust result */
                CFDictionarySetValue(rvc->pvc->info, kSecTrustRevocationReason, cfreason);
            }
            CFRelease(cfreason);
            processed = true;
            break;
        case CS_Unknown:
            /* not an error, no per-cert status, nothing here */
            secdebug("ocsp", "CS_Unknown for cert %" PRIdCFIndex, rvc->certIX);
            processed = false;
            break;
        default:
            secnotice("ocsp", "BAD certStatus (%d) for cert %" PRIdCFIndex,
                     (int)this->certStatus, rvc->certIX);
            processed = false;
            break;
    }

    return processed;
}

static void SecORVCUpdatePVC(SecORVCRef rvc) {
    if (rvc->ocspSingleResponse) {
        SecOCSPSingleResponseProcess(rvc->ocspSingleResponse, rvc);
    }
    if (rvc->ocspResponse) {
        rvc->nextUpdate = SecOCSPResponseGetExpirationTime(rvc->ocspResponse);
    }
}

typedef void (^SecOCSPEvaluationCompleted)(SecTrustResultType tr);

static void
SecOCSPEvaluateCompleted(const void *userData,
                                SecCertificatePathRef chain, CFArrayRef details, CFDictionaryRef info,
                                SecTrustResultType result) {
    SecOCSPEvaluationCompleted evaluated = (SecOCSPEvaluationCompleted)userData;
    evaluated(result);
    Block_release(evaluated);

}

static bool SecOCSPResponseEvaluateSigner(SecORVCRef rvc, CFArrayRef signers, CFArrayRef issuers, CFAbsoluteTime verifyTime) {
    __block bool evaluated = false;
    bool trusted = false;
    if (!signers || !issuers) {
        return trusted;
    }

    /* Verify the signer chain against the OCSPSigner policy, using the issuer chain as anchors. */
    const void *ocspSigner = SecPolicyCreateOCSPSigner();
    CFArrayRef policies = CFArrayCreate(kCFAllocatorDefault,
                                        &ocspSigner, 1, &kCFTypeArrayCallBacks);
    CFRelease(ocspSigner);

    SecOCSPEvaluationCompleted completed = Block_copy(^(SecTrustResultType result) {
        if (result == kSecTrustResultProceed || result == kSecTrustResultUnspecified) {
            evaluated = true;
        }
    });

    CFDataRef clientAuditToken = SecPathBuilderCopyClientAuditToken(rvc->pvc->builder);
    SecPathBuilderRef oBuilder = SecPathBuilderCreate(clientAuditToken,
                                                      signers, issuers, true, false,
                                                      policies, NULL, NULL,  NULL,
                                                      verifyTime, NULL,
                                                      SecOCSPEvaluateCompleted, completed);
    /* Build the chain(s), evaluate them, call the completed block, free the block and builder */
    SecPathBuilderStep(oBuilder);
    CFReleaseNull(clientAuditToken);
    CFReleaseNull(policies);

    /* verify the public key of the issuer signed the OCSP signer */
    if (evaluated) {
        SecCertificateRef issuer = NULL, signer = NULL;
        SecKeyRef issuerPubKey = NULL;

        issuer = (SecCertificateRef)CFArrayGetValueAtIndex(issuers, 0);
        signer = (SecCertificateRef)CFArrayGetValueAtIndex(signers, 0);

        if (issuer) {
#if TARGET_OS_IPHONE
            issuerPubKey = SecCertificateCopyPublicKey(issuer);
#else
            issuerPubKey = SecCertificateCopyPublicKey_ios(issuer);
#endif
        }
        if (signer && issuerPubKey && (errSecSuccess == SecCertificateIsSignedBy(signer, issuerPubKey))) {
            trusted = true;
        } else {
            secnotice("ocsp", "ocsp signer cert not signed by issuer");
        }
        CFReleaseNull(issuerPubKey);
    }

    return trusted;
}

static bool SecOCSPResponseVerify(SecOCSPResponseRef ocspResponse, SecORVCRef rvc, CFAbsoluteTime verifyTime) {
    bool trusted;
    SecCertificatePathRef issuers = SecCertificatePathCopyFromParent(rvc->pvc->path, rvc->certIX + 1);
    SecCertificateRef issuer = issuers ? CFRetainSafe(SecCertificatePathGetCertificateAtIndex(issuers, 0)) : NULL;
    CFArrayRef signers = SecOCSPResponseCopySigners(ocspResponse);
    SecCertificateRef signer = SecOCSPResponseCopySigner(ocspResponse, issuer);

    if (signer && signers) {
        if (issuer && CFEqual(signer, issuer)) {
            /* We already know we trust issuer since it's the issuer of the
             * cert we are verifying. */
            secinfo("ocsp", "ocsp responder: %@ response signed by issuer",
                     rvc->responder);
            trusted = true;
        } else {
            secinfo("ocsp", "ocsp responder: %@ response signed by cert issued by issuer",
                     rvc->responder);
            CFMutableArrayRef signerCerts = NULL;
            CFArrayRef issuerCerts = NULL;

            /* Ensure the signer cert is the 0th cert for trust evaluation */
            signerCerts = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
            CFArrayAppendValue(signerCerts, signer);
            CFArrayAppendArray(signerCerts, signers, CFRangeMake(0, CFArrayGetCount(signers)));

            if (issuers) {
                issuerCerts = SecCertificatePathCopyCertificates(issuers, NULL);
            }

            if (SecOCSPResponseEvaluateSigner(rvc, signerCerts, issuerCerts, verifyTime)) {
                secdebug("ocsp", "response satisfies ocspSigner policy (%@)",
                         rvc->responder);
                trusted = true;
            } else {
                /* @@@ We don't trust the cert so don't use this response. */
                secnotice("ocsp", "ocsp response signed by certificate which "
                              "does not satisfy ocspSigner policy");
                trusted = false;
            }
            CFReleaseNull(signerCerts);
            CFReleaseNull(issuerCerts);
        }
    } else {
        /* @@@ No signer found for this ocsp response, discard it. */
        secnotice("ocsp", "ocsp responder: %@ no signer found for response",
                 rvc->responder);
        trusted = false;
    }

#if DUMP_OCSPRESPONSES
    char buf[40];
    snprintf(buf, 40, "/tmp/ocspresponse%ld%s.der",
             rvc->certIX, (trusted ? "t" : "u"));
    secdumpdata(ocspResponse->data, buf);
#endif
    CFReleaseNull(issuers);
    CFReleaseNull(issuer);
    CFReleaseNull(signers);
    CFReleaseNull(signer);
    return trusted;
}

static void SecORVCConsumeOCSPResponse(SecORVCRef rvc, SecOCSPResponseRef ocspResponse /*CF_CONSUMED*/, CFTimeInterval maxAge, bool updateCache) {
    SecOCSPSingleResponseRef sr = NULL;
    require_quiet(ocspResponse, errOut);
    SecOCSPResponseStatus orStatus = SecOCSPGetResponseStatus(ocspResponse);
    require_action_quiet(orStatus == kSecOCSPSuccess, errOut,
                         secnotice("ocsp", "responder: %@ returned status: %d",  rvc->responder, orStatus));
    require_action_quiet(sr = SecOCSPResponseCopySingleResponse(ocspResponse, rvc->ocspRequest), errOut,
                         secnotice("ocsp",  "ocsp responder: %@ did not include status of requested cert", rvc->responder));
    // Check if this response is fresher than any (cached) response we might still have in the rvc.
    require_quiet(!rvc->ocspSingleResponse || rvc->ocspSingleResponse->thisUpdate < sr->thisUpdate, errOut);

    CFAbsoluteTime verifyTime = CFAbsoluteTimeGetCurrent();
    /* TODO: If the responder doesn't have the ocsp-nocheck extension we should
     check whether the leaf was revoked (we are already checking the rest of
     the chain). */
    /* Check the OCSP response signature and verify the response. */
    require_quiet(SecOCSPResponseVerify(ocspResponse, rvc,
                                        sr->certStatus == CS_Revoked ? SecOCSPResponseProducedAt(ocspResponse) : verifyTime), errOut);

    // If we get here, we have a properly signed ocsp response
    // but we haven't checked dates yet.

    bool sr_valid = SecOCSPSingleResponseCalculateValidity(sr, kSecDefaultOCSPResponseTTL, verifyTime);
    if (sr->certStatus == CS_Good) {
        // Side effect of SecOCSPResponseCalculateValidity sets ocspResponse->expireTime
        require_quiet(sr_valid && SecOCSPResponseCalculateValidity(ocspResponse, maxAge, kSecDefaultOCSPResponseTTL, verifyTime), errOut);
    } else if (sr->certStatus == CS_Revoked) {
        // Expire revoked responses when the subject certificate itself expires.
        ocspResponse->expireTime = SecCertificateNotValidAfter(SecPVCGetCertificateAtIndex(rvc->pvc, rvc->certIX));
    }

    // Ok we like the new response, let's toss the old one.
    if (updateCache)
        SecOCSPCacheReplaceResponse(rvc->ocspResponse, ocspResponse, rvc->responder, verifyTime);

    if (rvc->ocspResponse) SecOCSPResponseFinalize(rvc->ocspResponse);
    rvc->ocspResponse = ocspResponse;
    ocspResponse = NULL;

    if (rvc->ocspSingleResponse) SecOCSPSingleResponseDestroy(rvc->ocspSingleResponse);
    rvc->ocspSingleResponse = sr;
    sr = NULL;

    rvc->done = sr_valid;

errOut:
    if (sr) SecOCSPSingleResponseDestroy(sr);
    if (ocspResponse) SecOCSPResponseFinalize(ocspResponse);
}

/* Callback from async http code after an ocsp response has been received. */
static void SecOCSPFetchCompleted(asynchttp_t *http, CFTimeInterval maxAge) {
    SecORVCRef rvc = (SecORVCRef)http->info;
    SecPVCRef pvc = rvc->pvc;
    SecOCSPResponseRef ocspResponse = NULL;
    if (http->response) {
        CFDataRef data = CFHTTPMessageCopyBody(http->response);
        if (data) {
            /* Parse the returned data as if it's an ocspResponse. */
            ocspResponse = SecOCSPResponseCreate(data);
            CFRelease(data);
        }
    }

    SecORVCConsumeOCSPResponse(rvc, ocspResponse, maxAge, true);
    // TODO: maybe we should set the cache-control: false in the http header and try again if the response is stale

    if (!rvc->done) {
        /* Clear the data for the next response. */
        asynchttp_free(http);
        SecORVCFetchNext(rvc);
    }

    if (rvc->done) {
        secdebug("rvc", "got OCSP response for cert: %ld", rvc->certIX);
        SecORVCUpdatePVC(rvc);
        SecORVCFinish(rvc);
        if (!--pvc->asyncJobCount) {
            secdebug("rvc", "done with all async jobs");
            SecPathBuilderStep(pvc->builder);
        }
    }
}

static SecORVCRef SecORVCCreate(SecRVCRef rvc, SecPVCRef pvc, CFIndex certIX) {
    SecORVCRef orvc = NULL;
    orvc = malloc(sizeof(struct OpaqueSecORVC));
    if (orvc) {
        memset(orvc, 0, sizeof(struct OpaqueSecORVC));
        orvc->pvc = pvc;
        orvc->rvc = rvc;
        orvc->certIX = certIX;
        orvc->http.queue = SecPathBuilderGetQueue(pvc->builder);
        orvc->http.token = SecPathBuilderCopyClientAuditToken(pvc->builder);
        orvc->http.completed = SecOCSPFetchCompleted;
        orvc->http.info = orvc;
        orvc->ocspRequest = NULL;
        orvc->responderIX = 0;
        orvc->responder = NULL;
        orvc->nextUpdate = NULL_TIME;
        orvc->ocspResponse = NULL;
        orvc->ocspSingleResponse = NULL;
        orvc->done = false;

        SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, certIX);
        CFIndex count = SecPVCGetCertificateCount(pvc);
        if (certIX + 1 < count) {
            SecCertificateRef issuer = SecPVCGetCertificateAtIndex(pvc, certIX + 1);
            orvc->ocspRequest = SecOCSPRequestCreate(cert, issuer);
        }
    }
    return orvc;
}

static void SecORVCProcessStapledResponses(SecORVCRef rvc) {
    /* Get stapled OCSP responses */
    CFArrayRef ocspResponsesData = SecPathBuilderCopyOCSPResponses(rvc->pvc->builder);

    if(ocspResponsesData) {
        secdebug("rvc", "Checking stapled responses for cert %ld", rvc->certIX);
        CFArrayForEach(ocspResponsesData, ^(const void *value) {
            SecOCSPResponseRef ocspResponse = SecOCSPResponseCreate(value);
            SecORVCConsumeOCSPResponse(rvc, ocspResponse, NULL_TIME, false);
        });
        CFRelease(ocspResponsesData);
    }
}

// MARK: SecCRVCRef
/********************************************************
 ******************* CRL RVC Functions ******************
 ********************************************************/
#if ENABLE_CRLS
#include <../trustd/SecTrustOSXEntryPoints.h>
#define kSecDefaultCRLTTL kSecDefaultOCSPResponseTTL

/* CRL Revocation verification context. */
struct OpaqueSecCRVC {
    /* Response data from ocspd. Yes, ocspd does CRLs, but not OCSP... */
    async_ocspd_t async_ocspd;

    /* Pointer to the pvc for this revocation check. */
    SecPVCRef pvc;

    /* Pointer to the generic rvc for this revocation check */
    SecRVCRef rvc;

    /* The current CRL status from ocspd. */
    OSStatus status;

    /* Index of cert in pvc that this RVC is for 0 = leaf, etc. */
    CFIndex certIX;

    /* Index in array returned by SecCertificateGetCRLDistributionPoints() for 
     current distribution point. */
    CFIndex distributionPointIX;

    /* URL of current distribution point. */
    CFURLRef distributionPoint;

    /* Date until which this revocation status is valid. */
    CFAbsoluteTime nextUpdate;
    
    bool done;
};

static void SecCRVCFinish(SecCRVCRef crvc) {
    // nothing yet
}

#define MAX_CRL_DPS 3
#define CRL_REQUEST_THRESHOLD 10

static CFURLRef SecCRVCGetNextDistributionPoint(SecCRVCRef rvc) {
    SecCertificateRef cert = SecPVCGetCertificateAtIndex(rvc->pvc, rvc->certIX);
    CFArrayRef crlDPs = SecCertificateGetCRLDistributionPoints(cert);
    if (crlDPs) {
        CFIndex crlDPCount = CFArrayGetCount(crlDPs);
        if (crlDPCount >= CRL_REQUEST_THRESHOLD) {
            secnotice("rvc", "too many CRL DP entries (%ld)", (long)crlDPCount);
            return NULL;
        }
        while (rvc->distributionPointIX < crlDPCount && rvc->distributionPointIX < MAX_CRL_DPS) {
            CFURLRef distributionPoint = CFArrayGetValueAtIndex(crlDPs, rvc->distributionPointIX);
            rvc->distributionPointIX++;
            CFStringRef scheme = CFURLCopyScheme(distributionPoint);
            if (scheme) {
                /* We only support http and https responders currently. */
                bool valid_DP = (CFEqual(CFSTR("http"), scheme) ||
                                 CFEqual(CFSTR("https"), scheme) ||
                                 CFEqual(CFSTR("ldap"), scheme));
                CFRelease(scheme);
                if (valid_DP)
                    return distributionPoint;
            }
        }
    }
    return NULL;
}

static void SecCRVCGetCRLStatus(SecCRVCRef rvc) {
    SecCertificateRef cert = SecPVCGetCertificateAtIndex(rvc->pvc, rvc->certIX);
    SecCertificatePathRef path = rvc->pvc->path;
    CFArrayRef serializedCertPath = SecCertificatePathCreateSerialized(path, NULL);
    secdebug("rvc", "searching CRL cache for cert: %ld", rvc->certIX);
    rvc->status = SecTrustLegacyCRLStatus(cert, serializedCertPath, rvc->distributionPoint);
    CFReleaseNull(serializedCertPath);
    /* we got a response indicating that the CRL was checked */
    if (rvc->status == errSecSuccess || rvc->status == errSecCertificateRevoked) {
        rvc->done = true;
        /* ocspd doesn't give us the nextUpdate time, so set to default */
        rvc->nextUpdate = SecPVCGetVerifyTime(rvc->pvc) + kSecDefaultCRLTTL;
    }
}

static void SecCRVCCheckRevocationCache(SecCRVCRef rvc) {
    while ((rvc->distributionPoint = SecCRVCGetNextDistributionPoint(rvc))) {
        SecCRVCGetCRLStatus(rvc);
        if (rvc->status == errSecCertificateRevoked) {
            return;
        }
    }
}

/* Fire off an async http request for this certs revocation status, return
 false if request was queued, true if we're done. */
static bool SecCRVCFetchNext(SecCRVCRef rvc) {
    while ((rvc->distributionPoint = SecCRVCGetNextDistributionPoint(rvc))) {
        SecCertificateRef cert = SecPVCGetCertificateAtIndex(rvc->pvc, rvc->certIX);
        SecCertificatePathRef path = rvc->pvc->path;
        CFArrayRef serializedCertPath = SecCertificatePathCreateSerialized(path, NULL);
        secinfo("rvc", "fetching CRL for cert: %ld", rvc->certIX);
        if (!SecTrustLegacyCRLFetch(&rvc->async_ocspd, rvc->distributionPoint,
                                    CFAbsoluteTimeGetCurrent(), cert, serializedCertPath)) {
            CFDataRef clientAuditToken = NULL;
            SecTaskRef task = NULL;
            audit_token_t auditToken = {};
            clientAuditToken = SecPathBuilderCopyClientAuditToken(rvc->pvc->builder);
            require(clientAuditToken, out);
            require(sizeof(auditToken) == CFDataGetLength(clientAuditToken), out);
            CFDataGetBytes(clientAuditToken, CFRangeMake(0, sizeof(auditToken)), (uint8_t *)&auditToken);
            require(task = SecTaskCreateWithAuditToken(NULL, auditToken), out);
            secnotice("rvc", "asynchronously fetching CRL (%@) for client (%@)",
                      rvc->distributionPoint, task);

        out:
            CFReleaseNull(clientAuditToken);
            CFReleaseNull(task);
            /* Async request was posted, wait for reply. */
            return false;
        }
    }
    rvc->done = true;
    return true;
}

static void SecCRVCUpdatePVC(SecCRVCRef rvc) {
    if (rvc->status == errSecCertificateRevoked) {
        secdebug("rvc", "CRL revoked cert %" PRIdCFIndex, rvc->certIX);
        SInt32 reason = 0; // unspecified, since ocspd didn't tell us
        CFNumberRef cfreason = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &reason);
        SecPVCSetResultForced(rvc->pvc, kSecPolicyCheckRevocation, rvc->certIX,
                              cfreason, true);
        if (rvc->pvc && rvc->pvc->info) {
            /* make the revocation reason available in the trust result */
            CFDictionarySetValue(rvc->pvc->info, kSecTrustRevocationReason, cfreason);
        }
        CFReleaseNull(cfreason);
    }
}

static void SecCRVCFetchCompleted(async_ocspd_t *ocspd) {
    SecCRVCRef rvc = ocspd->info;
    SecPVCRef pvc = rvc->pvc;
    /* we got a response indicating that the CRL was checked */
    if (ocspd->response == errSecSuccess || ocspd->response == errSecCertificateRevoked) {
        rvc->status = ocspd->response;
        rvc->done = true;
        /* ocspd doesn't give us the nextUpdate time, so set to default */
        rvc->nextUpdate = SecPVCGetVerifyTime(rvc->pvc) + kSecDefaultCRLTTL;
        secdebug("rvc", "got CRL response for cert: %ld", rvc->certIX);
        SecCRVCUpdatePVC(rvc);
        SecCRVCFinish(rvc);
        if (!--pvc->asyncJobCount) {
            secdebug("rvc", "done with all async jobs");
            SecPathBuilderStep(pvc->builder);
        }
    } else {
        if(SecCRVCFetchNext(rvc)) {
            if (!--pvc->asyncJobCount) {
                secdebug("rvc", "done with all async jobs");
                SecPathBuilderStep(pvc->builder);
            }
        }
    }
}

static SecCRVCRef SecCRVCCreate(SecRVCRef rvc, SecPVCRef pvc, CFIndex certIX) {
    SecCRVCRef crvc = NULL;
    crvc = malloc(sizeof(struct OpaqueSecCRVC));
    if (crvc) {
        memset(crvc, 0, sizeof(struct OpaqueSecCRVC));
        crvc->pvc = pvc;
        crvc->rvc = rvc;
        crvc->certIX = certIX;
        crvc->status = errSecInternal;
        crvc->distributionPointIX = 0;
        crvc->distributionPoint = NULL;
        crvc->nextUpdate = NULL_TIME;
        crvc->async_ocspd.queue = SecPathBuilderGetQueue(pvc->builder);
        crvc->async_ocspd.completed = SecCRVCFetchCompleted;
        crvc->async_ocspd.response = errSecInternal;
        crvc->async_ocspd.info = crvc;
        crvc->done = false;
    }
    return crvc;
}

static bool SecRVCShouldCheckCRL(SecRVCRef rvc) {
    if (rvc->pvc->check_revocation &&
        CFEqual(kSecPolicyCheckRevocationCRL, rvc->pvc->check_revocation)) {
        /* Our client insists on CRLs */
        secinfo("rvc", "client told us to check CRL");
        return true;
    }
    SecCertificateRef cert = SecPVCGetCertificateAtIndex(rvc->pvc, rvc->certIX);
    CFArrayRef ocspResponders = SecCertificateGetOCSPResponders(cert);
    if ((!ocspResponders || CFArrayGetCount(ocspResponders) == 0) &&
        (rvc->pvc->check_revocation && !CFEqual(kSecPolicyCheckRevocationOCSP, rvc->pvc->check_revocation))) {
        /* The cert doesn't have OCSP responders and the client didn't specifically ask for OCSP.
         * This logic will skip the CRL cache check if the client didn't ask for revocation checking */
        secinfo("rvc", "client told us to check revocation and CRL is only option for cert: %ld", rvc->certIX);
        return true;
    }
    return false;
}
#endif /* ENABLE_CRLS */

static void SecRVCFinish(SecRVCRef rvc) {
    if (rvc->orvc) {
        SecORVCFinish(rvc->orvc);
    }
#if ENABLE_CRLS
    if (rvc->crvc) {
        SecCRVCFinish(rvc->crvc);
    }
#endif
}

static void SecRVCDelete(SecRVCRef rvc) {
    if (rvc->orvc) {
        SecORVCFinish(rvc->orvc);
        free(rvc->orvc);
    }
#if ENABLE_CRLS
    if (rvc->crvc) {
        SecCRVCFinish(rvc->crvc);
        free(rvc->crvc);
    }
#endif
    if (rvc->valid_info) {
        SecValidInfoRelease(rvc->valid_info);
    }
}

static void SecRVCInit(SecRVCRef rvc, SecPVCRef pvc, CFIndex certIX) {
    secdebug("alloc", "%p", rvc);
    rvc->pvc = pvc;
    rvc->certIX = certIX;
    rvc->orvc = SecORVCCreate(rvc, pvc, certIX);
#if ENABLE_CRLS
    rvc->crvc = SecCRVCCreate(rvc, pvc, certIX);
#endif
    rvc->done = false;
}

static void SecRVCUpdatePVC(SecRVCRef rvc) {
    SecORVCUpdatePVC(rvc->orvc);
#if ENABLE_CRLS
    SecCRVCUpdatePVC(rvc->crvc);
#endif
}

#if ENABLE_CRLS
static bool SecRVCShouldCheckOCSP(SecRVCRef rvc) {
    if (!rvc->pvc->check_revocation
        || !CFEqual(rvc->pvc->check_revocation, kSecPolicyCheckRevocationCRL)) {
        return true;
    }
    return false;
}
#else
static bool SecRVCShouldCheckOCSP(SecRVCRef rvc) {
    return true;
}
#endif

static void SecRVCProcessValidInfoResults(SecRVCRef rvc) {
    if (!rvc || !rvc->valid_info || !rvc->pvc) {
        return;
    }
    /* Handle definitive revocations.
    */
    bool valid = rvc->valid_info->valid;
    SecValidInfoFormat format = rvc->valid_info->format;
    if (!valid && (format == kSecValidInfoFormatSerial || format == kSecValidInfoFormatSHA256)) {
        secdebug("validupdate", "rvc: revoked cert %" PRIdCFIndex, rvc->certIX);
        SInt32 reason = 0; // unspecified, since the Valid db doesn't tell us
        CFNumberRef cfreason = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &reason);
        SecPVCSetResultForced(rvc->pvc, kSecPolicyCheckRevocation, rvc->certIX,
                              cfreason, true);
        if (rvc->pvc->info) {
            /* make the revocation reason available in the trust result */
            CFDictionarySetValue(rvc->pvc->info, kSecTrustRevocationReason, cfreason);
        }
        CFReleaseNull(cfreason);

        rvc->done = true;
        return;
    }

    /* Handle non-definitive information.
       We set rvc->done = true above ONLY if the result was definitive;
       otherwise we require a revocation check for SSL usage.
    */
    if (format == kSecValidInfoFormatNto1) {
        /* matched the filter */
        CFIndex count = SecPVCGetCertificateCount(rvc->pvc);
        CFIndex issuerIX = rvc->certIX + 1;
        if (issuerIX >= count) {
            /* cannot perform a revocation check on the last cert in the
               chain, since we don't have its issuer. */
            return;
        }
        SecPolicyRef policy = SecPVCGetPolicy(rvc->pvc);
        CFStringRef policyName = (policy) ? SecPolicyGetName(policy) : NULL;
        if (policyName && CFEqual(CFSTR("sslServer"), policyName)) {
            /* perform revocation check for SSL policy;
               require for leaf if an OCSP responder is present. */
            if (0 == rvc->certIX) {
                SecCertificateRef cert = SecPVCGetCertificateAtIndex(rvc->pvc, rvc->certIX);
                CFArrayRef resps = (cert) ? SecCertificateGetOCSPResponders(cert) : NULL;
                CFIndex rcount = (resps) ? CFArrayGetCount(resps) : 0;
                if (rcount > 0) {
                    rvc->pvc->response_required = true;
                }
            }
            rvc->pvc->check_revocation = kSecPolicyCheckRevocationAny;
        }
    }

}

static bool SecRVCCheckValidInfoDatabase(SecRVCRef rvc) {
    /* If the valid database is enabled... */
#if (__MAC_OS_X_VERSION_MIN_REQUIRED >= 101300 || __IPHONE_OS_VERSION_MIN_REQUIRED >= 110000)
    /* Make sure revocation db info is up-to-date,
       if we are allowed to access the network */
#if !TARGET_OS_BRIDGE
    SecPathBuilderRef builder = rvc->pvc->builder;
    if (SecPathBuilderCanAccessNetwork(builder)) {
        SecRevocationDbCheckNextUpdate();
    }
#endif
    /* Check whether we have valid db info for this cert,
       given the cert and its issuer */
    SecValidInfoRef info = NULL;
    CFIndex count = SecPVCGetCertificateCount(rvc->pvc);
    if (count) {
        SecCertificateRef cert = NULL;
        SecCertificateRef issuer = NULL;
        CFIndex issuerIX = rvc->certIX + 1;
        if (count > issuerIX) {
            issuer = SecPVCGetCertificateAtIndex(rvc->pvc, issuerIX);
        } else if (count == issuerIX) {
            CFIndex rootIX = SecCertificatePathSelfSignedIndex(rvc->pvc->path);
            if (rootIX == rvc->certIX) {
                issuer = SecPVCGetCertificateAtIndex(rvc->pvc, rootIX);
            }
        }
        cert = SecPVCGetCertificateAtIndex(rvc->pvc, rvc->certIX);
        info = SecRevocationDbCopyMatching(cert, issuer);
    }
    if (info) {
        SecValidInfoRef old_info = rvc->valid_info;
        rvc->valid_info = info;
        if (old_info) {
            SecValidInfoRelease(old_info);
        }
        return true;
    }
#endif
    return false;
}

static void SecRVCCheckRevocationCaches(SecRVCRef rvc) {
    /* Don't check OCSP cache if CRLs enabled and policy requested CRL only */
    if (SecRVCShouldCheckOCSP(rvc)) {
        secdebug("ocsp", "Checking cached responses for cert %ld", rvc->certIX);
        SecOCSPResponseRef response = NULL;
        if (rvc->pvc->online_revocation) {
            CFAbsoluteTime now = CFAbsoluteTimeGetCurrent();
            response = SecOCSPCacheCopyMatchingWithMinInsertTime(rvc->orvc->ocspRequest, NULL, now - kSecOCSPResponseOnlineTTL);
        } else {
            response = SecOCSPCacheCopyMatching(rvc->orvc->ocspRequest, NULL);
        }
        SecORVCConsumeOCSPResponse(rvc->orvc,
                                   response,
                                   NULL_TIME, false);
    }
#if ENABLE_CRLS
    /* Don't check CRL cache if policy requested OCSP only */
    if (SecRVCShouldCheckCRL(rvc)) {
        SecCRVCCheckRevocationCache(rvc->crvc);
    }
#endif
}

static bool SecRVCFetchNext(SecRVCRef rvc) {
    bool OCSP_fetch_finished = true;
    /* Don't send OCSP request only if CRLs enabled and policy requested CRL only */
    if (SecRVCShouldCheckOCSP(rvc)) {
        OCSP_fetch_finished &= SecORVCFetchNext(rvc->orvc);
    }
    if (OCSP_fetch_finished) {
        /* we didn't start an OCSP background job for this cert */
        rvc->pvc->asyncJobCount--;
    }

#if ENABLE_CRLS
    bool CRL_fetch_finished = true;
    /* Don't check CRL cache if policy requested OCSP only */
    if (SecRVCShouldCheckCRL(rvc)) {
        /* reset the distributionPointIX because we already iterated through the CRLDPs
         * in SecCRVCCheckRevocationCache */
        rvc->crvc->distributionPointIX = 0;
        CRL_fetch_finished &= SecCRVCFetchNext(rvc->crvc);
    }
    if (CRL_fetch_finished) {
        /* we didn't start a CRL background job for this cert */
        rvc->pvc->asyncJobCount--;
    }
    OCSP_fetch_finished &= CRL_fetch_finished;
#endif

    return OCSP_fetch_finished;
}

static bool SecPVCCheckRevocation(SecPVCRef pvc) {
    secdebug("rvc", "checking revocation");
    CFIndex certIX, certCount = SecPVCGetCertificateCount(pvc);
    bool completed = true;
    if (certCount <= 1) {
        /* Can't verify without an issuer; we're done */
        return completed;
    }

    /*
     * Don't need to call SecPVCIsAnchored; having an issuer is sufficient here.
     *
     * Note: we can't check revocation for the last certificate in the chain
     * via OCSP or CRL methods, since there isn't a separate issuer cert to
     * sign those responses. However, since a self-signed root has an implied
     * issuer of itself, we can check for it in the valid database.
     */

    if (pvc->rvcs) {
        /* We have done revocation checking already, we're done. */
        secdebug("rvc", "Not rechecking revocation");
        return completed;
    }

    /* Setup things so we check revocation status of all certs. */
    pvc->rvcs = calloc(sizeof(struct OpaqueSecRVC), certCount);

    /* Note that if we are multi threaded and a job completes after it
     is started but before we return from this function, we don't want
     a callback to decrement asyncJobCount to zero before we finish issuing
     all the jobs. To avoid this we pretend we issued certCount-1 async jobs,
     and decrement pvc->asyncJobCount for each cert that we don't start a
     background fetch for. (We will never start an async job for the final
     cert in the chain.) */
#if !ENABLE_CRLS
    pvc->asyncJobCount = (unsigned int)(certCount-1);
#else
    /* If we enable CRLS, we may end up with two async jobs per cert: one
     * for OCSP and one for fetching the CRL */
    pvc->asyncJobCount =  2 * (unsigned int)(certCount-1);
#endif
    secdebug("rvc", "set asyncJobCount to %d", pvc->asyncJobCount);

    /* Loop though certificates again and issue an ocsp fetch if the
     * revocation status checking isn't done yet (and we have an issuer!) */
    for (certIX = 0; certIX < certCount; ++certIX) {
        secdebug("rvc", "checking revocation for cert: %ld", certIX);
        SecRVCRef rvc = &((SecRVCRef)pvc->rvcs)[certIX];
        SecRVCInit(rvc, pvc, certIX);
        if (rvc->done){
            continue;
        }

#if !TARGET_OS_BRIDGE
        /* Check valid database first (separate from OCSP response cache) */
        if (SecRVCCheckValidInfoDatabase(rvc)) {
            SecRVCProcessValidInfoResults(rvc);
        }
#endif
        /* Any other revocation method requires an issuer certificate;
         * skip the last cert in the chain since it doesn't have one. */
        if (certIX+1 >= certCount) {
            continue;
        }

        /* Ignore stapled OCSP responses only if CRLs are enabled and the
         * policy specifically requested CRLs only. */
        if (SecRVCShouldCheckOCSP(rvc)) {
            /*  If we have any OCSP stapled responses, check those first */
            SecORVCProcessStapledResponses(rvc->orvc);
        }

#if TARGET_OS_BRIDGE
        /* The bridge has no writeable storage and no network. Nothing else we can
         * do here. */
        rvc->done = true;
        return completed;
#endif

        /* Then check the caches for revocation results. */
        SecRVCCheckRevocationCaches(rvc);

        /* The check is done if we found cached responses from either method. */
        if (rvc->orvc->done
#if ENABLE_CRLS
            || rvc->orvc->done
#endif
            ) {
            secdebug("rvc", "found cached response for cert: %ld", certIX);
            rvc->done = true;
        }

        /* If we got a cached response that is no longer valid (which can only be true for
         * revoked responses), let's try to get a fresher response even if no one asked.
         * This check resolves unrevocation events after the nextUpdate time. */
        bool old_cached_response = (!rvc->done && rvc->orvc->ocspResponse);

        /* If the cert is EV or if revocation checking was explicitly enabled, attempt to fire off an
         async http request for this cert's revocation status, unless we already successfully checked
         the revocation status of this cert based on the cache or stapled responses.  */
        bool allow_fetch = SecPathBuilderCanAccessNetwork(pvc->builder) &&
                           (pvc->is_ev || pvc->check_revocation || old_cached_response);
        bool fetch_done = true;
        if (rvc->done || !allow_fetch) {
            /* We got a cache hit or we aren't allowed to access the network */
            SecRVCUpdatePVC(rvc);
            SecRVCFinish(rvc);
            /* We didn't really start any background jobs for this cert. */
            pvc->asyncJobCount--;
#if ENABLE_CRLS
            pvc->asyncJobCount--;
#endif
            secdebug("rvc", "not fetching and job count is %d for cert %ld", pvc->asyncJobCount, certIX);
        } else {
            fetch_done = SecRVCFetchNext(rvc);
        }
        if (!fetch_done) {
            /* We started at least one background fetch. */
            secdebug("rvc", "waiting on background fetch for cert %ld", certIX);
            completed = false;
        }
    }

    /* Return false if we started any background jobs. */
    /* We can't just return !pvc->asyncJobCount here, since if we started any
     jobs the completion callback will be called eventually and it will call
     SecPathBuilderStep(). If for some reason everything completed before we
     get here we still want the outer SecPathBuilderStep() to terminate so we
     keep track of whether we started any jobs and return false if so. */
    return completed;
}

static CFAbsoluteTime SecRVCGetEarliestNextUpdate(SecRVCRef rvc) {
    CFAbsoluteTime enu = NULL_TIME;
    enu = rvc->orvc->nextUpdate;
#if ENABLE_CRLS
    CFAbsoluteTime crlNextUpdate = rvc->crvc->nextUpdate;
    if (enu == NULL_TIME ||
        ((crlNextUpdate > NULL_TIME) && (enu > crlNextUpdate))) {
        /* We didn't check OCSP or CRL next update time was sooner */
        enu = crlNextUpdate;
    }
#endif
    return enu;
}


void SecPolicyServerInitalize(void) {
	gSecPolicyLeafCallbacks = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
		&kCFTypeDictionaryKeyCallBacks, NULL);
	gSecPolicyPathCallbacks = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
		&kCFTypeDictionaryKeyCallBacks, NULL);

	CFDictionaryAddValue(gSecPolicyPathCallbacks,
		kSecPolicyCheckBasicCertificateProcessing,
		SecPolicyCheckBasicCertificateProcessing);
	CFDictionaryAddValue(gSecPolicyPathCallbacks,
		kSecPolicyCheckCriticalExtensions,
		SecPolicyCheckCriticalExtensions);
	CFDictionaryAddValue(gSecPolicyPathCallbacks,
		kSecPolicyCheckIdLinkage,
		SecPolicyCheckIdLinkage);
	CFDictionaryAddValue(gSecPolicyLeafCallbacks,
		kSecPolicyCheckKeyUsage,
		SecPolicyCheckKeyUsage);
	CFDictionaryAddValue(gSecPolicyLeafCallbacks,
		kSecPolicyCheckExtendedKeyUsage,
		SecPolicyCheckExtendedKeyUsage);
	CFDictionaryAddValue(gSecPolicyLeafCallbacks,
		kSecPolicyCheckBasicConstraints,
		SecPolicyCheckBasicConstraints);
	CFDictionaryAddValue(gSecPolicyLeafCallbacks,
		kSecPolicyCheckNonEmptySubject,
		SecPolicyCheckNonEmptySubject);
	CFDictionaryAddValue(gSecPolicyLeafCallbacks,
		kSecPolicyCheckQualifiedCertStatements,
		SecPolicyCheckQualifiedCertStatements);
	CFDictionaryAddValue(gSecPolicyLeafCallbacks,
		kSecPolicyCheckSSLHostname,
		SecPolicyCheckSSLHostname);
	CFDictionaryAddValue(gSecPolicyLeafCallbacks,
		kSecPolicyCheckEmail,
		SecPolicyCheckEmail);
	CFDictionaryAddValue(gSecPolicyPathCallbacks,
		kSecPolicyCheckValidIntermediates,
		SecPolicyCheckValidIntermediates);
	CFDictionaryAddValue(gSecPolicyLeafCallbacks,
		kSecPolicyCheckValidLeaf,
		SecPolicyCheckValidLeaf);
	CFDictionaryAddValue(gSecPolicyPathCallbacks,
		kSecPolicyCheckValidRoot,
		SecPolicyCheckValidRoot);
	CFDictionaryAddValue(gSecPolicyPathCallbacks,
		kSecPolicyCheckIssuerCommonName,
		SecPolicyCheckIssuerCommonName);
	CFDictionaryAddValue(gSecPolicyLeafCallbacks,
		kSecPolicyCheckSubjectCommonNamePrefix,
		SecPolicyCheckSubjectCommonNamePrefix);
	CFDictionaryAddValue(gSecPolicyLeafCallbacks,
		kSecPolicyCheckSubjectCommonName,
		SecPolicyCheckSubjectCommonName);
	CFDictionaryAddValue(gSecPolicyLeafCallbacks,
		kSecPolicyCheckNotValidBefore,
		SecPolicyCheckNotValidBefore);
	CFDictionaryAddValue(gSecPolicyPathCallbacks,
		kSecPolicyCheckChainLength,
		SecPolicyCheckChainLength);
	CFDictionaryAddValue(gSecPolicyPathCallbacks,
		kSecPolicyCheckAnchorSHA1,
		SecPolicyCheckAnchorSHA1);
    CFDictionaryAddValue(gSecPolicyPathCallbacks,
        kSecPolicyCheckAnchorSHA256,
        SecPolicyCheckAnchorSHA256);
	CFDictionaryAddValue(gSecPolicyPathCallbacks,
		kSecPolicyCheckAnchorApple,
		SecPolicyCheckAnchorApple);
	CFDictionaryAddValue(gSecPolicyLeafCallbacks,
		kSecPolicyCheckSubjectOrganization,
		SecPolicyCheckSubjectOrganization);
	CFDictionaryAddValue(gSecPolicyLeafCallbacks,
		kSecPolicyCheckSubjectOrganizationalUnit,
		SecPolicyCheckSubjectOrganizationalUnit);
	CFDictionaryAddValue(gSecPolicyLeafCallbacks,
		kSecPolicyCheckEAPTrustedServerNames,
		SecPolicyCheckEAPTrustedServerNames);
	CFDictionaryAddValue(gSecPolicyLeafCallbacks,
		kSecPolicyCheckSubjectCommonNameTEST,
		SecPolicyCheckSubjectCommonNameTEST);
	CFDictionaryAddValue(gSecPolicyLeafCallbacks,
		kSecPolicyCheckRevocation,
		SecPolicyCheckRevocation);
	CFDictionaryAddValue(gSecPolicyLeafCallbacks,
		kSecPolicyCheckRevocationResponseRequired,
		SecPolicyCheckRevocationResponseRequired);
    CFDictionaryAddValue(gSecPolicyLeafCallbacks,
        kSecPolicyCheckRevocationOnline,
        SecPolicyCheckRevocationOnline);
	CFDictionaryAddValue(gSecPolicyLeafCallbacks,
		kSecPolicyCheckNoNetworkAccess,
		SecPolicyCheckNoNetworkAccess);
	CFDictionaryAddValue(gSecPolicyLeafCallbacks,
		kSecPolicyCheckBlackListedLeaf,
		SecPolicyCheckBlackListedLeaf);
	CFDictionaryAddValue(gSecPolicyLeafCallbacks,
		kSecPolicyCheckGrayListedLeaf,
		SecPolicyCheckGrayListedLeaf);
	CFDictionaryAddValue(gSecPolicyLeafCallbacks,
		kSecPolicyCheckLeafMarkerOid,
		SecPolicyCheckLeafMarkerOid);
    CFDictionaryAddValue(gSecPolicyLeafCallbacks,
        kSecPolicyCheckLeafMarkerOidWithoutValueCheck,
        SecPolicyCheckLeafMarkerOidWithoutValueCheck);
    CFDictionaryAddValue(gSecPolicyLeafCallbacks,
        kSecPolicyCheckLeafMarkersProdAndQA,
        SecPolicyCheckLeafMarkersProdAndQA);
	CFDictionaryAddValue(gSecPolicyPathCallbacks,
		kSecPolicyCheckIntermediateSPKISHA256,
		SecPolicyCheckIntermediateSPKISHA256);
	CFDictionaryAddValue(gSecPolicyPathCallbacks,
		kSecPolicyCheckIntermediateEKU,
		SecPolicyCheckIntermediateEKU);
	CFDictionaryAddValue(gSecPolicyPathCallbacks,
		kSecPolicyCheckIntermediateMarkerOid,
		SecPolicyCheckIntermediateMarkerOid);
	CFDictionaryAddValue(gSecPolicyLeafCallbacks,
		kSecPolicyCheckCertificatePolicy,
		SecPolicyCheckCertificatePolicyOid);
    CFDictionaryAddValue(gSecPolicyPathCallbacks,
        kSecPolicyCheckWeakIntermediates,
        SecPolicyCheckWeakIntermediates);
    CFDictionaryAddValue(gSecPolicyLeafCallbacks,
        kSecPolicyCheckWeakLeaf,
        SecPolicyCheckWeakLeaf);
    CFDictionaryAddValue(gSecPolicyPathCallbacks,
        kSecPolicyCheckWeakRoot,
        SecPolicyCheckWeakRoot);
    CFDictionaryAddValue(gSecPolicyPathCallbacks,
        kSecPolicyCheckKeySize,
        SecPolicyCheckKeySize);
    CFDictionaryAddValue(gSecPolicyPathCallbacks,
        kSecPolicyCheckSignatureHashAlgorithms,
        SecPolicyCheckSignatureHashAlgorithms);
    CFDictionaryAddValue(gSecPolicyPathCallbacks,
        kSecPolicyCheckSystemTrustedWeakHash,
        SecPolicyCheckSystemTrustedWeakHash);
    CFDictionaryAddValue(gSecPolicyPathCallbacks,
        kSecPolicyCheckIntermediateOrganization,
        SecPolicyCheckIntermediateOrganization);
    CFDictionaryAddValue(gSecPolicyPathCallbacks,
        kSecPolicyCheckIntermediateCountry,
        SecPolicyCheckIntermediateCountry);
}

// MARK: -
// MARK: SecPVCRef
/********************************************************
 ****************** SecPVCRef Functions *****************
 ********************************************************/

void SecPVCInit(SecPVCRef pvc, SecPathBuilderRef builder, CFArrayRef policies,
    CFAbsoluteTime verifyTime) {
    secdebug("alloc", "%p", pvc);
    // Weird logging policies crashes.
    //secdebug("policy", "%@", policies);

    // Zero the pvc struct so only non-zero fields need to be explicitly set
    memset(pvc, 0, sizeof(struct OpaqueSecPVC));
    pvc->builder = builder;
    pvc->policies = policies;
    if (policies)
        CFRetain(policies);
    pvc->verifyTime = verifyTime;
    pvc->result = true;
}

static void SecPVCDeleteRVCs(SecPVCRef pvc) {
    secdebug("alloc", "%p", pvc);
    if (pvc->rvcs) {
        CFIndex certIX, certCount = SecPVCGetCertificateCount(pvc);
        for (certIX = 0; certIX < certCount; ++certIX) {
            SecRVCRef rvc = &((SecRVCRef)pvc->rvcs)[certIX];
            SecRVCDelete(rvc);
        }
        free(pvc->rvcs);
        pvc->rvcs = NULL;
    }
}

void SecPVCDelete(SecPVCRef pvc) {
    secdebug("alloc", "%p", pvc);
    CFReleaseNull(pvc->policies);
    CFReleaseNull(pvc->details);
    CFReleaseNull(pvc->info);
    if (pvc->valid_policy_tree) {
        policy_tree_prune(&pvc->valid_policy_tree);
    }
    SecPVCDeleteRVCs(pvc);
    CFReleaseNull(pvc->path);
}

void SecPVCSetPath(SecPVCRef pvc, SecCertificatePathRef path,
    CF_CONSUMED CFArrayRef details) {
    secdebug("policy", "%@", path);
    bool samePath = ((!path && !pvc->path) || (path && pvc->path && CFEqual(path, pvc->path)));
    if (!samePath) {
        /* Changing path makes us clear the Revocation Verification Contexts */
        SecPVCDeleteRVCs(pvc);
        CFReleaseSafe(pvc->path);
        pvc->path = CFRetainSafe(path);
    }
    pvc->details = details;
    CFReleaseNull(pvc->info);
    if (pvc->valid_policy_tree) {
        policy_tree_prune(&pvc->valid_policy_tree);
    }
    pvc->policyIX = 0;

    /* Since we don't run the LeafChecks again, we need to preserve the
     * result the leaf had. */
    pvc->result = (details) ? (CFDictionaryGetCount(CFArrayGetValueAtIndex(details, 0)) == 0)
                            : true;
}

SecPolicyRef SecPVCGetPolicy(SecPVCRef pvc) {
	return (SecPolicyRef)CFArrayGetValueAtIndex(pvc->policies, pvc->policyIX);
}

CFIndex SecPVCGetCertificateCount(SecPVCRef pvc) {
	return SecCertificatePathGetCount(pvc->path);
}

SecCertificateRef SecPVCGetCertificateAtIndex(SecPVCRef pvc, CFIndex ix) {
	return SecCertificatePathGetCertificateAtIndex(pvc->path, ix);
}

bool SecPVCIsCertificateAtIndexSelfIssued(SecPVCRef pvc, CFIndex ix) {
    /* The SecCertificatePath only tells us the last self-issued cert.
     * The chain may have more than one self-issued cert, so we need to
     * do the comparison. */
    bool result = false;
    SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, ix);
    CFDataRef issuer = SecCertificateCopyNormalizedIssuerSequence(cert);
    CFDataRef subject = SecCertificateCopyNormalizedSubjectSequence(cert);
    if (issuer && subject && CFEqual(issuer, subject)) {
        result = true;
    }
    CFReleaseNull(issuer);
    CFReleaseNull(subject);
    return result;
}

void SecPVCSetCheckRevocation(SecPVCRef pvc, CFStringRef method) {
    pvc->check_revocation = method;
    secdebug("rvc", "deferred revocation checking enabled using %@ method", method);
}

void SecPVCSetCheckRevocationResponseRequired(SecPVCRef pvc) {
    pvc->response_required = true;
    secdebug("rvc", "revocation response required");
}

void SecPVCSetCheckRevocationOnline(SecPVCRef pvc) {
    pvc->online_revocation = true;
    secdebug("rvc", "revocation force online check");
}

bool SecPVCIsAnchored(SecPVCRef pvc) {
    return SecCertificatePathIsAnchored(pvc->path);
}

CFAbsoluteTime SecPVCGetVerifyTime(SecPVCRef pvc) {
    return pvc->verifyTime;
}

static int32_t detailKeyToCssmErr(CFStringRef key) {
    int32_t result = 0;

    if (CFEqual(key, kSecPolicyCheckSSLHostname)) {
        result = -2147408896; // CSSMERR_APPLETP_HOSTNAME_MISMATCH
    }
    else if (CFEqual(key, kSecPolicyCheckEmail)) {
        result = -2147408872; // CSSMERR_APPLETP_SMIME_EMAIL_ADDRS_NOT_FOUND
    }
    else if (CFEqual(key, kSecPolicyCheckValidLeaf) ||
             CFEqual(key, kSecPolicyCheckValidIntermediates) ||
             CFEqual(key, kSecPolicyCheckValidRoot)) {
        result = -2147409654; // CSSMERR_TP_CERT_EXPIRED
    }

    return result;
}

static bool SecPVCMeetsConstraint(SecPVCRef pvc, SecCertificateRef certificate, CFDictionaryRef constraint);

static bool SecPVCIsAllowedError(SecPVCRef pvc, CFIndex ix, CFStringRef key) {
    bool result = false;
    CFArrayRef constraints = SecCertificatePathGetUsageConstraintsAtIndex(pvc->path, ix);
    SecCertificateRef cert = SecCertificatePathGetCertificateAtIndex(pvc->path, ix);
    CFIndex constraintIX, constraintCount = CFArrayGetCount(constraints);

    for (constraintIX = 0; constraintIX < constraintCount; constraintIX++) {
        CFDictionaryRef constraint = (CFDictionaryRef)CFArrayGetValueAtIndex(constraints, constraintIX);
        CFNumberRef allowedErrorNumber = NULL;
        if (!isDictionary(constraint)) {
            continue;
        }
        allowedErrorNumber = (CFNumberRef)CFDictionaryGetValue(constraint, kSecTrustSettingsAllowedError);
        int32_t allowedErrorValue = 0;
        if (!isNumber(allowedErrorNumber) || !CFNumberGetValue(allowedErrorNumber, kCFNumberSInt32Type, &allowedErrorValue)) {
            continue;
        }

        if (SecPVCMeetsConstraint(pvc, cert, constraint)) {
            if (allowedErrorValue == detailKeyToCssmErr(key)) {
                result = true;
                break;
            }
        }
    }
    return result;
}

static bool SecPVCKeyIsConstraintPolicyOption(SecPVCRef pvc, CFStringRef key) {
    CFIndex certIX, certCount = SecCertificatePathGetCount(pvc->path);
    for (certIX = 0; certIX < certCount; certIX++) {
        CFArrayRef constraints = SecCertificatePathGetUsageConstraintsAtIndex(pvc->path, certIX);
        CFIndex constraintIX, constraintCount = CFArrayGetCount(constraints);
        for (constraintIX = 0; constraintIX < constraintCount; constraintIX++) {
            CFDictionaryRef constraint = (CFDictionaryRef)CFArrayGetValueAtIndex(constraints, constraintIX);
            if (!isDictionary(constraint)) {
                continue;
            }

            CFDictionaryRef policyOptions = NULL;
            policyOptions = (CFDictionaryRef)CFDictionaryGetValue(constraint, kSecTrustSettingsPolicyOptions);
            if (policyOptions && isDictionary(policyOptions) &&
                CFDictionaryContainsKey(policyOptions, key)) {
                return true;
            }
        }
    }
    return false;
}

/* AUDIT[securityd](done):
   policy->_options is a caller provided dictionary, only its cf type has
   been checked.
 */
bool SecPVCSetResultForced(SecPVCRef pvc,
	CFStringRef key, CFIndex ix, CFTypeRef result, bool force) {

    secnotice("policy", "cert[%d]: %@ =(%s)[%s]> %@", (int) ix, key,
        (pvc->callbacks == gSecPolicyLeafCallbacks ? "leaf"
            : (pvc->callbacks == gSecPolicyPathCallbacks ? "path"
                : "custom")),
        (force ? "force" : ""), result);

    /* If this is not something the current policy cares about ignore
       this error and return true so our caller continues evaluation. */
    if (!force) {
        /* Either the policy or the usage constraints have to have this key */
        SecPolicyRef policy = SecPVCGetPolicy(pvc);
        if (!(SecPVCKeyIsConstraintPolicyOption(pvc, key) ||
            (policy && CFDictionaryContainsKey(policy->_options, key)))) {
            return true;
        }
    }

	/* Check to see if the SecTrustSettings for the certificate in question
	   tell us to ignore this error. */
	if (SecPVCIsAllowedError(pvc, ix, key)) {
        secinfo("policy", "cert[%d]: skipped allowed error %@", (int) ix, key);
		return true;
	}

	pvc->result = false;
	if (!pvc->details)
		return false;

	CFMutableDictionaryRef detail =
		(CFMutableDictionaryRef)CFArrayGetValueAtIndex(pvc->details, ix);

	/* Perhaps detail should have an array of results per key?  As it stands
       in the case of multiple policy failures the last failure stands.  */
	CFDictionarySetValue(detail, key, result);

	return true;
}

bool SecPVCSetResult(SecPVCRef pvc,
	CFStringRef key, CFIndex ix, CFTypeRef result) {
    return SecPVCSetResultForced(pvc, key, ix, result, false);
}

/* AUDIT[securityd](done):
   key(ok) is a caller provided.
   value(ok, unused) is a caller provided.
 */
static void SecPVCValidateKey(const void *key, const void *value,
	void *context) {
	SecPVCRef pvc = (SecPVCRef)context;

	/* If our caller doesn't want full details and we failed earlier there is
	   no point in doing additional checks. */
	if (!pvc->result && !pvc->details)
		return;

	SecPolicyCheckFunction fcn = (SecPolicyCheckFunction)
		CFDictionaryGetValue(pvc->callbacks, key);

	if (!fcn) {
#if 0
    /* Why not to have optional policy checks rant:
       Not all keys are in all dictionaries anymore, so why not make checks
       optional?  This way a client can ask for something and the server will
       do a best effort based on the supported flags.  It works since they are
       synchronized now, but we need some debug checking here for now. */
		pvc->result = false;
#endif
        if (pvc->callbacks == gSecPolicyLeafCallbacks) {
            if (!CFDictionaryContainsKey(gSecPolicyPathCallbacks, key)) {
                pvc->result = false;
            }
        } else if (pvc->callbacks == gSecPolicyPathCallbacks) {
            if (!CFDictionaryContainsKey(gSecPolicyLeafCallbacks, key)) {
                pvc->result = false;
            }
        } else {
            /* Non standard validation phase, nothing is optional. */
            pvc->result = false;
        }
		return;
	}

	fcn(pvc, (CFStringRef)key);
}

/* AUDIT[securityd](done):
   policy->_options is a caller provided dictionary, only its cf type has
   been checked.
 */
bool SecPVCLeafChecks(SecPVCRef pvc) {
    pvc->result = true;
    CFArrayRef policies = pvc->policies;
	CFIndex ix, count = CFArrayGetCount(policies);
	for (ix = 0; ix < count; ++ix) {
		SecPolicyRef policy = (SecPolicyRef)CFArrayGetValueAtIndex(policies, ix);
        pvc->policyIX = ix;
        /* Validate all keys for all policies. */
        pvc->callbacks = gSecPolicyLeafCallbacks;
        CFDictionaryApplyFunction(policy->_options, SecPVCValidateKey, pvc);
        if (!pvc->result && !pvc->details)
            return pvc->result;
	}

    return pvc->result;
}

bool SecPVCParentCertificateChecks(SecPVCRef pvc, CFIndex ix) {
    /* Check stuff common to intermediate and anchors. */
	CFAbsoluteTime verifyTime = SecPVCGetVerifyTime(pvc);
	SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, ix);
    bool is_anchor = (ix == SecPVCGetCertificateCount(pvc) - 1
        && SecPVCIsAnchored(pvc));
	if (!SecCertificateIsValid(cert, verifyTime)) {
		/* Certificate has expired. */
		if (!SecPVCSetResult(pvc, is_anchor ? kSecPolicyCheckValidRoot
            : kSecPolicyCheckValidIntermediates, ix, kCFBooleanFalse))
            goto errOut;
	}

    if (SecCertificateIsWeakKey(cert)) {
        /* Certificate uses weak key. */
        if (!SecPVCSetResult(pvc, is_anchor ? kSecPolicyCheckWeakRoot
            : kSecPolicyCheckWeakIntermediates, ix, kCFBooleanFalse))
            goto errOut;
    }

    if (is_anchor) {
        /* Perform anchor specific checks. */
        /* Don't think we have any of these. */
    } else {
        /* Perform intermediate specific checks. */

		/* (k) Basic constraints only relevant for v3 and later. */
		if (SecCertificateVersion(cert) >= 3) {
			const SecCEBasicConstraints *bc =
				SecCertificateGetBasicConstraints(cert);
			if (!bc || !bc->isCA) {
				/* Basic constraints not present or not marked as isCA, illegal. */
				if (!SecPVCSetResultForced(pvc, kSecPolicyCheckBasicConstraints,
							ix, kCFBooleanFalse, true))
					goto errOut;
			}
		}
		/* (l) max_path_length is checked elsewhere. */

        /* (n) If a key usage extension is present, verify that the keyCertSign bit is set. */
        SecKeyUsage keyUsage = SecCertificateGetKeyUsage(cert);
        if (keyUsage && !(keyUsage & kSecKeyUsageKeyCertSign)) {
            if (!SecPVCSetResultForced(pvc, kSecPolicyCheckKeyUsage,
                ix, kCFBooleanFalse, true))
                goto errOut;
        }
    }

errOut:
    return pvc->result;
}

bool SecPVCBlackListedKeyChecks(SecPVCRef pvc, CFIndex ix) {
    /* Check stuff common to intermediate and anchors. */

	SecOTAPKIRef otapkiRef = SecOTAPKICopyCurrentOTAPKIRef();
	if (NULL != otapkiRef)
	{
		CFSetRef blackListedKeys = SecOTAPKICopyBlackListSet(otapkiRef);
		CFRelease(otapkiRef);
		if (NULL != blackListedKeys)
		{
			SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, ix);
			CFIndex count = SecPVCGetCertificateCount(pvc);
			bool is_last = (ix == count - 1);
			bool is_anchor = (is_last && SecPVCIsAnchored(pvc));
			if (!is_anchor) {
				/* Check for blacklisted intermediate issuer keys. */
				CFDataRef dgst = SecCertificateCopyPublicKeySHA1Digest(cert);
				if (dgst) {
					/* Check dgst against blacklist. */
					if (CFSetContainsValue(blackListedKeys, dgst)) {
						/* Check allow list for this blacklisted issuer key,
						   which is the authority key of the issued cert at ix-1.
						   If ix is the last cert, the root is missing, so we
						   also check our own authority key in that case.
						*/
						bool allowed = ((ix && SecPVCCheckCertificateAllowList(pvc, ix - 1)) ||
						                (is_last && SecPVCCheckCertificateAllowList(pvc, ix)));
						if (!allowed) {
							SecPVCSetResultForced(pvc, kSecPolicyCheckBlackListedKey,
							                      ix, kCFBooleanFalse, true);
						}
						pvc->is_allowlisted = allowed;
					}
					CFRelease(dgst);
				}
			}
			CFRelease(blackListedKeys);
			return pvc->result;
		}
	}
	// Assume OK
	return true;
}

bool SecPVCGrayListedKeyChecks(SecPVCRef pvc, CFIndex ix)
{
	/* Check stuff common to intermediate and anchors. */
	SecOTAPKIRef otapkiRef = SecOTAPKICopyCurrentOTAPKIRef();
	if (NULL != otapkiRef)
	{
		CFSetRef grayListKeys = SecOTAPKICopyGrayList(otapkiRef);
		CFRelease(otapkiRef);
		if (NULL != grayListKeys)
		{
			SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, ix);
			CFIndex count = SecPVCGetCertificateCount(pvc);
			bool is_last = (ix == count - 1);
			bool is_anchor = (is_last && SecPVCIsAnchored(pvc));
			if (!is_anchor) {
				/* Check for gray listed intermediate issuer keys. */
				CFDataRef dgst = SecCertificateCopyPublicKeySHA1Digest(cert);
				if (dgst) {
					/* Check dgst against gray list. */
					if (CFSetContainsValue(grayListKeys, dgst)) {
						/* Check allow list for this graylisted issuer key,
						   which is the authority key of the issued cert at ix-1.
						   If ix is the last cert, the root is missing, so we
						   also check our own authority key in that case.
						*/
						bool allowed = ((ix && SecPVCCheckCertificateAllowList(pvc, ix - 1)) ||
						                (is_last && SecPVCCheckCertificateAllowList(pvc, ix)));
						if (!allowed) {
							SecPVCSetResultForced(pvc, kSecPolicyCheckGrayListedKey,
							                      ix, kCFBooleanFalse, true);
						}
						pvc->is_allowlisted = allowed;
					}
					CFRelease(dgst);
				}
			}
			CFRelease(grayListKeys);
			return pvc->result;
		}
	}
	// Assume ok
	return true;
}

static bool SecPVCContainsPolicy(SecPVCRef pvc, CFStringRef searchOid, CFStringRef searchName, CFIndex *policyIX) {
    if (!isString(searchName) && !isString(searchOid)) {
        return false;
    }
	CFArrayRef policies = pvc->policies;
	CFIndex ix, count = CFArrayGetCount(policies);
	for (ix = 0; ix < count; ++ix) {
		SecPolicyRef policy = (SecPolicyRef)CFArrayGetValueAtIndex(policies, ix);
		CFStringRef policyName = SecPolicyGetName(policy);
        CFStringRef policyOid = SecPolicyGetOidString(policy);
        /* Prefer a match of both name and OID */
        if (searchOid && searchName && policyOid && policyName) {
            if (CFEqual(searchOid, policyOid) &&
                CFEqual(searchName, policyName)) {
                if (policyIX) { *policyIX = ix; }
                return true;
            }
        }
        /* Next best is just OID. */
        if (!searchName && searchOid && policyOid) {
            if (CFEqual(searchOid, policyOid)) {
                if (policyIX) { *policyIX = ix; }
                return true;
            }
        }
        if (!searchOid && searchName && policyName) {
            if (CFEqual(searchName, policyName)) {
                if (policyIX) { *policyIX = ix; }
                return true;
            }
        }
	}
	return false;
}

static bool SecPVCContainsString(SecPVCRef pvc, CFIndex policyIX, CFStringRef stringValue) {
    if (!isString(stringValue)) {
        return false;
    }
    bool result = false;

    CFStringRef tmpStringValue = NULL;
    if (CFStringGetCharacterAtIndex(stringValue, CFStringGetLength(stringValue) -1) == (UniChar)0x0000) {
        tmpStringValue = CFStringCreateTruncatedCopy(stringValue, CFStringGetLength(stringValue) - 1);
    } else {
        tmpStringValue = CFStringCreateCopy(NULL, stringValue);
    }
    if (policyIX >= 0 && policyIX < CFArrayGetCount(pvc->policies)) {
        SecPolicyRef policy = (SecPolicyRef)CFArrayGetValueAtIndex(pvc->policies, policyIX);
        /* Have to look for all the possible locations of name string */
        CFStringRef policyString = NULL;
        policyString = CFDictionaryGetValue(policy->_options, kSecPolicyCheckSSLHostname);
        if (!policyString) {
            policyString = CFDictionaryGetValue(policy->_options, kSecPolicyCheckEmail);
        }
        if (policyString && (CFStringCompare(tmpStringValue, policyString, kCFCompareCaseInsensitive) == kCFCompareEqualTo)) {
            result = true;
            goto out;
        }

        CFArrayRef policyStrings = NULL;
        policyStrings = CFDictionaryGetValue(policy->_options, kSecPolicyCheckEAPTrustedServerNames);
        if (policyStrings && CFArrayContainsValue(policyStrings,
                                                  CFRangeMake(0, CFArrayGetCount(policyStrings)),
                                                  tmpStringValue)) {
            result = true;
            goto out;
        }
    }

out:
    CFReleaseNull(tmpStringValue);
    return result;
}


static uint32_t ts_key_usage_for_kuNumber(CFNumberRef keyUsageNumber) {
    uint32_t ourTSKeyUsage = 0;
    uint32_t keyUsage = 0;
    if (keyUsageNumber &&
        CFNumberGetValue(keyUsageNumber, kCFNumberSInt32Type, &keyUsage)) {
        if (keyUsage & kSecKeyUsageDigitalSignature) {
            ourTSKeyUsage |= kSecTrustSettingsKeyUseSignature;
        }
        if (keyUsage & kSecKeyUsageDataEncipherment) {
            ourTSKeyUsage |= kSecTrustSettingsKeyUseEnDecryptData;
        }
        if (keyUsage & kSecKeyUsageKeyEncipherment) {
            ourTSKeyUsage |= kSecTrustSettingsKeyUseEnDecryptKey;
        }
        if (keyUsage & kSecKeyUsageKeyAgreement) {
            ourTSKeyUsage |= kSecTrustSettingsKeyUseKeyExchange;
        }
        if (keyUsage == kSecKeyUsageAll) {
            ourTSKeyUsage = kSecTrustSettingsKeyUseAny;
        }
    }
    return ourTSKeyUsage;
}

static uint32_t ts_key_usage_for_policy(SecPolicyRef policy) {
    uint32_t ourTSKeyUsage = 0;
    CFTypeRef policyKeyUsageType = NULL;

    policyKeyUsageType = (CFTypeRef)CFDictionaryGetValue(policy->_options, kSecPolicyCheckKeyUsage);
    if (isArray(policyKeyUsageType)) {
        CFIndex ix, count = CFArrayGetCount(policyKeyUsageType);
        for (ix = 0; ix < count; ix++) {
            CFNumberRef policyKeyUsageNumber = NULL;
            policyKeyUsageNumber = (CFNumberRef)CFArrayGetValueAtIndex(policyKeyUsageType, ix);
            ourTSKeyUsage |= ts_key_usage_for_kuNumber(policyKeyUsageNumber);
        }
    } else if (isNumber(policyKeyUsageType)) {
        ourTSKeyUsage |= ts_key_usage_for_kuNumber(policyKeyUsageType);
    }

    return ourTSKeyUsage;
}

static bool SecPVCContainsTrustSettingsKeyUsage(SecPVCRef pvc,
    SecCertificateRef certificate, CFIndex policyIX, CFNumberRef keyUsageNumber) {
    int64_t keyUsageValue = 0;
    uint32_t ourKeyUsage = 0;

    if (!isNumber(keyUsageNumber) || !CFNumberGetValue(keyUsageNumber, kCFNumberSInt64Type, &keyUsageValue)) {
        return false;
    }

    if (keyUsageValue == kSecTrustSettingsKeyUseAny) {
        return true;
    }

    /* We're using the key for revocation if we have the OCSPSigner policy.
     * @@@ If we support CRLs, we'd need to check for that policy here too.
     */
    if (SecPVCContainsPolicy(pvc, kSecPolicyAppleOCSPSigner, NULL, NULL)) {
        ourKeyUsage |= kSecTrustSettingsKeyUseSignRevocation;
    }

    /* We're using the key for verifying a cert if it's a root/intermediate
     * in the chain. If the cert isn't in the path yet, we're about to add it,
     * so it's a root/intermediate. If there is no path, this is the leaf.
     */
    CFIndex pathIndex = -1;
    if (pvc->path) {
        pathIndex = SecCertificatePathGetIndexOfCertificate(pvc->path, certificate);
    } else {
        pathIndex = 0;
    }
    if (pathIndex != 0) {
        ourKeyUsage |= kSecTrustSettingsKeyUseSignCert;
    }

    /* The rest of the key usages may be specified by the policy(ies). */
    if (policyIX >= 0 && policyIX < CFArrayGetCount(pvc->policies)) {
        SecPolicyRef policy = (SecPolicyRef)CFArrayGetValueAtIndex(pvc->policies, policyIX);
        ourKeyUsage |= ts_key_usage_for_policy(policy);
    } else {
        /* Get key usage from ALL policies */
        CFIndex ix, count = CFArrayGetCount(pvc->policies);
        for (ix = 0; ix < count; ix++) {
            SecPolicyRef policy = (SecPolicyRef)CFArrayGetValueAtIndex(pvc->policies, ix);
            ourKeyUsage |= ts_key_usage_for_policy(policy);
        }
    }

    if (ourKeyUsage == (uint32_t)(keyUsageValue & 0x00ffffffff)) {
        return true;
    }

    return false;
}

#if TARGET_OS_MAC && !TARGET_OS_IPHONE

#include <Security/SecTrustedApplicationPriv.h>
#include <bsm/libbsm.h>
#include <libproc.h>

static bool SecPVCCallerIsApplication(CFDataRef clientAuditToken, CFTypeRef appRef) {
    bool result = false;
    audit_token_t auditToken = {};
    char path[MAXPATHLEN];

    require(appRef && clientAuditToken, out);
    require(CFGetTypeID(appRef) == SecTrustedApplicationGetTypeID(), out);

    require(sizeof(auditToken) == CFDataGetLength(clientAuditToken), out);
    CFDataGetBytes(clientAuditToken, CFRangeMake(0, sizeof(auditToken)), (uint8_t *)&auditToken);
    require(proc_pidpath(audit_token_to_pid(auditToken), path, sizeof(path)) > 0, out);

    if(errSecSuccess == SecTrustedApplicationValidateWithPath((SecTrustedApplicationRef)appRef, path)) {
        result = true;
    }

out:
    return result;
}
#endif

static bool SecPVCContainsTrustSettingsPolicyOption(SecPVCRef pvc, CFDictionaryRef options) {
    if (!isDictionary(options)) {
        return false;
    }

    /* Push */
    CFDictionaryRef currentCallbacks = pvc->callbacks;

    /* We need to run the leaf and path checks using these options. */
    pvc->callbacks = gSecPolicyLeafCallbacks;
    CFDictionaryApplyFunction(options, SecPVCValidateKey, pvc);

    pvc->callbacks = gSecPolicyPathCallbacks;
    CFDictionaryApplyFunction(options, SecPVCValidateKey, pvc);

    /* Pop */
    pvc->callbacks = currentCallbacks;

    /* Our work here is done; no need to claim a match */
    return false;
}

static bool SecPVCMeetsConstraint(SecPVCRef pvc, SecCertificateRef certificate, CFDictionaryRef constraint) {
    CFStringRef policyOid = NULL, policyString = NULL, policyName = NULL;
    CFNumberRef keyUsageNumber = NULL;
    CFTypeRef trustedApplicationData = NULL;
    CFDictionaryRef policyOptions = NULL;

    bool policyMatch = false, policyStringMatch = false, applicationMatch = false ,
         keyUsageMatch = false, policyOptionMatch = false;
    bool result = false;

#if TARGET_OS_MAC && !TARGET_OS_IPHONE
    /* OS X returns a SecPolicyRef in the constraints. Convert to the oid string. */
    SecPolicyRef policy = NULL;
    policy = (SecPolicyRef)CFDictionaryGetValue(constraint, kSecTrustSettingsPolicy);
    policyOid = (policy) ? policy->_oid : NULL;
#else
    policyOid = (CFStringRef)CFDictionaryGetValue(constraint, kSecTrustSettingsPolicy);
#endif
    policyName = (CFStringRef)CFDictionaryGetValue(constraint, kSecTrustSettingsPolicyName);
    policyString = (CFStringRef)CFDictionaryGetValue(constraint, kSecTrustSettingsPolicyString);
    keyUsageNumber = (CFNumberRef)CFDictionaryGetValue(constraint, kSecTrustSettingsKeyUsage);
    policyOptions = (CFDictionaryRef)CFDictionaryGetValue(constraint, kSecTrustSettingsPolicyOptions);

    CFIndex policyIX = -1;
    policyMatch = SecPVCContainsPolicy(pvc, policyOid, policyName, &policyIX);
    policyStringMatch = SecPVCContainsString(pvc, policyIX, policyString);
    keyUsageMatch = SecPVCContainsTrustSettingsKeyUsage(pvc, certificate, policyIX, keyUsageNumber);
    policyOptionMatch = SecPVCContainsTrustSettingsPolicyOption(pvc, policyOptions);

#if TARGET_OS_MAC && !TARGET_OS_IPHONE
    trustedApplicationData =  CFDictionaryGetValue(constraint, kSecTrustSettingsApplication);
    CFDataRef clientAuditToken = SecPathBuilderCopyClientAuditToken(pvc->builder);
    applicationMatch = SecPVCCallerIsApplication(clientAuditToken, trustedApplicationData);
    CFReleaseNull(clientAuditToken);
#else
    if(CFDictionaryContainsKey(constraint, kSecTrustSettingsApplication)) {
        secerror("kSecTrustSettingsApplication is not yet supported on this platform");
    }
#endif

    /* If we either didn't find the parameter in the dictionary or we got a match
     * against that parameter, for all possible parameters in the dictionary, then
     * this trust setting result applies to the output. */
    if (((!policyOid && !policyName) || policyMatch) &&
        (!policyString || policyStringMatch) &&
        (!trustedApplicationData || applicationMatch) &&
        (!keyUsageNumber || keyUsageMatch) &&
        (!policyOptions || policyOptionMatch)) {
        result = true;
    }

    return result;
}

SecTrustSettingsResult SecPVCGetTrustSettingsResult(SecPVCRef pvc, SecCertificateRef certificate, CFArrayRef constraints) {
    SecTrustSettingsResult result = kSecTrustSettingsResultInvalid;
    CFIndex constraintIX, constraintCount = CFArrayGetCount(constraints);
    for (constraintIX = 0; constraintIX < constraintCount; constraintIX++) {
        CFDictionaryRef constraint = (CFDictionaryRef)CFArrayGetValueAtIndex(constraints, constraintIX);
        if (!isDictionary(constraint)) {
            continue;
        }

        CFNumberRef resultNumber = NULL;
        resultNumber = (CFNumberRef)CFDictionaryGetValue(constraint, kSecTrustSettingsResult);
        uint32_t resultValue = kSecTrustSettingsResultInvalid;
        if (!isNumber(resultNumber) || !CFNumberGetValue(resultNumber, kCFNumberSInt32Type, &resultValue)) {
            /* no SecTrustSettingsResult entry defaults to TrustRoot*/
            resultValue = kSecTrustSettingsResultTrustRoot;
        }

        if (SecPVCMeetsConstraint(pvc, certificate, constraint)) {
            result = resultValue;
            break;
        }
    }
    return result;
}

bool SecPVCCheckUsageConstraints(SecPVCRef pvc) {
    bool shouldDeny = false;
    CFIndex certIX, certCount = SecCertificatePathGetCount(pvc->path);
    for (certIX = 0; certIX < certCount; certIX++) {
        CFArrayRef constraints = SecCertificatePathGetUsageConstraintsAtIndex(pvc->path, certIX);
        SecCertificateRef cert = SecCertificatePathGetCertificateAtIndex(pvc->path, certIX);
        SecTrustSettingsResult result = SecPVCGetTrustSettingsResult(pvc, cert, constraints);

        if (result == kSecTrustSettingsResultDeny) {
            SecPVCSetResultForced(pvc, kSecPolicyCheckUsageConstraints, certIX, kCFBooleanFalse, true);
            shouldDeny = true;
        }
    }
    return shouldDeny;
}

#define kSecPolicySHA256Size 32
static const UInt8 kTestDateConstraintsRoot[kSecPolicySHA256Size] = {
    0x51,0xA0,0xF3,0x1F,0xC0,0x1D,0xEC,0x87,0x32,0xB6,0xFD,0x13,0x6A,0x43,0x4D,0x6C,
    0x87,0xCD,0x62,0xE0,0x38,0xB4,0xFB,0xD6,0x40,0xB0,0xFD,0x62,0x4D,0x1F,0xCF,0x6D
};
static const UInt8 kWS_CA1_G2[kSecPolicySHA256Size] = {
    0xD4,0x87,0xA5,0x6F,0x83,0xB0,0x74,0x82,0xE8,0x5E,0x96,0x33,0x94,0xC1,0xEC,0xC2,
    0xC9,0xE5,0x1D,0x09,0x03,0xEE,0x94,0x6B,0x02,0xC3,0x01,0x58,0x1E,0xD9,0x9E,0x16
};
static const UInt8 kWS_CA1_NEW[kSecPolicySHA256Size] = {
    0x4B,0x22,0xD5,0xA6,0xAE,0xC9,0x9F,0x3C,0xDB,0x79,0xAA,0x5E,0xC0,0x68,0x38,0x47,
    0x9C,0xD5,0xEC,0xBA,0x71,0x64,0xF7,0xF2,0x2D,0xC1,0xD6,0x5F,0x63,0xD8,0x57,0x08
};
static const UInt8 kWS_CA2_NEW[kSecPolicySHA256Size] = {
    0xD6,0xF0,0x34,0xBD,0x94,0xAA,0x23,0x3F,0x02,0x97,0xEC,0xA4,0x24,0x5B,0x28,0x39,
    0x73,0xE4,0x47,0xAA,0x59,0x0F,0x31,0x0C,0x77,0xF4,0x8F,0xDF,0x83,0x11,0x22,0x54
};
static const UInt8 kWS_ECC[kSecPolicySHA256Size] = {
    0x8B,0x45,0xDA,0x1C,0x06,0xF7,0x91,0xEB,0x0C,0xAB,0xF2,0x6B,0xE5,0x88,0xF5,0xFB,
    0x23,0x16,0x5C,0x2E,0x61,0x4B,0xF8,0x85,0x56,0x2D,0x0D,0xCE,0x50,0xB2,0x9B,0x02
};
static const UInt8 kSC_SFSCA[kSecPolicySHA256Size] = {
    0xC7,0x66,0xA9,0xBE,0xF2,0xD4,0x07,0x1C,0x86,0x3A,0x31,0xAA,0x49,0x20,0xE8,0x13,
    0xB2,0xD1,0x98,0x60,0x8C,0xB7,0xB7,0xCF,0xE2,0x11,0x43,0xB8,0x36,0xDF,0x09,0xEA
};
static const UInt8 kSC_SHA2[kSecPolicySHA256Size] = {
    0xE1,0x78,0x90,0xEE,0x09,0xA3,0xFB,0xF4,0xF4,0x8B,0x9C,0x41,0x4A,0x17,0xD6,0x37,
    0xB7,0xA5,0x06,0x47,0xE9,0xBC,0x75,0x23,0x22,0x72,0x7F,0xCC,0x17,0x42,0xA9,0x11
};
static const UInt8 kSC_G2[kSecPolicySHA256Size] = {
    0xC7,0xBA,0x65,0x67,0xDE,0x93,0xA7,0x98,0xAE,0x1F,0xAA,0x79,0x1E,0x71,0x2D,0x37,
    0x8F,0xAE,0x1F,0x93,0xC4,0x39,0x7F,0xEA,0x44,0x1B,0xB7,0xCB,0xE6,0xFD,0x59,0x95
};

bool SecPVCCheckIssuerDateConstraints(SecPVCRef pvc) {
    static CFSetRef sConstrainedRoots = NULL;
    static dispatch_once_t _t;
    dispatch_once(&_t, ^{
        const UInt8 *v_hashes[] = {
            kWS_CA1_G2, kWS_CA1_NEW, kWS_CA2_NEW, kWS_ECC,
            kSC_SFSCA, kSC_SHA2, kSC_G2, kTestDateConstraintsRoot
        };
        CFMutableSetRef set = CFSetCreateMutable(NULL, 0, &kCFTypeSetCallBacks);
        CFIndex ix, count = sizeof(v_hashes)/sizeof(*v_hashes);
        for (ix=0; ix<count; ix++) {
            CFDataRef hash = CFDataCreateWithBytesNoCopy(NULL, v_hashes[ix],
                kSecPolicySHA256Size, kCFAllocatorNull);
            if (hash) {
                CFSetAddValue(set, hash);
                CFRelease(hash);
            }
        }
        sConstrainedRoots = set;
    });

    bool shouldDeny = false;
    CFIndex certIX, certCount = SecCertificatePathGetCount(pvc->path);
    for (certIX = certCount - 1; certIX >= 0 && !shouldDeny; certIX--) {
        SecCertificateRef cert = SecCertificatePathGetCertificateAtIndex(pvc->path, certIX);
        CFDataRef sha256 = SecCertificateCopySHA256Digest(cert);
        if (sha256 && CFSetContainsValue(sConstrainedRoots, sha256)) {
            /* matched a constrained root; check notBefore dates on all its children. */
            CFIndex childIX = certIX;
            while (--childIX >= 0) {
                SecCertificateRef child = SecCertificatePathGetCertificateAtIndex(pvc->path, childIX);
                /* 1 Dec 2016 00:00:00 GMT */
                if (child && (CFAbsoluteTime)502243200.0 <= SecCertificateNotValidBefore(child)) {
                    SecPVCSetResultForced(pvc, kSecPolicyCheckBlackListedKey, certIX, kCFBooleanFalse, true);
                    shouldDeny = true;
                    break;
                }
            }
        }
        CFReleaseNull(sha256);
    }
    return shouldDeny;
}

/* AUDIT[securityd](done):
   policy->_options is a caller provided dictionary, only its cf type has
   been checked.
 */
bool SecPVCPathChecks(SecPVCRef pvc) {
    secdebug("policy", "begin path: %@", pvc->path);
    bool completed = true;
    /* This needs to be initialized before we call any function that might call
       SecPVCSetResultForced(). */
    pvc->policyIX = 0;
    SecPolicyCheckIdLinkage(pvc, kSecPolicyCheckIdLinkage);
    if (pvc->result || pvc->details) {
        SecPolicyCheckBasicCertificateProcessing(pvc,
            kSecPolicyCheckBasicCertificateProcessing);
    }

    CFArrayRef policies = pvc->policies;
    CFIndex count = CFArrayGetCount(policies);
    for (; pvc->policyIX < count; ++pvc->policyIX) {
        /* Validate all keys for all policies. */
        pvc->callbacks = gSecPolicyPathCallbacks;
        SecPolicyRef policy = SecPVCGetPolicy(pvc);
        CFDictionaryApplyFunction(policy->_options, SecPVCValidateKey, pvc);
        if (!pvc->result && !pvc->details)
            return completed;
    }

    // Reset
    pvc->policyIX = 0;

    /* Check whether the TrustSettings say to deny a cert in the path. */
    (void)SecPVCCheckUsageConstraints(pvc);

    /* Check for issuer date constraints. */
    (void)SecPVCCheckIssuerDateConstraints(pvc);

    /* Check the things we can't check statically for the certificate path. */
    /* Critical Extensions, chainLength. */

    /* Policy tests. */
    pvc->is_ev = false;
    if ((pvc->result || pvc->details) && pvc->optionally_ev) {
        bool pre_ev_check_result = pvc->result;
        SecPolicyCheckEV(pvc, kSecPolicyCheckExtendedValidation);
        pvc->is_ev = pvc->result;
        /* If ev checking failed, we still want to accept this chain
           as a non EV one, if it was valid as such. */
        pvc->result = pre_ev_check_result;
    }

    /* Check revocation always, since we don't want a lesser recoverable result
     * to prevent the check from occurring. */
    completed = SecPVCCheckRevocation(pvc);

    /* Check for CT */
    if (pvc->result || pvc->details) {
        /* This call will set the value of pvc->is_ct, but won't change the result (pvc->result) */
        SecPolicyCheckCT(pvc, kSecPolicyCheckCertificateTransparency);
    }

    if (pvc->is_ev && !pvc->is_ct) {
        pvc->is_ct_whitelisted = SecPVCCheckCTWhiteListedLeaf(pvc);
    } else {
        pvc->is_ct_whitelisted = false;
    }

//errOut:
    secdebug("policy", "end %strusted completed: %d path: %@",
        (pvc->result ? "" : "not "), completed, pvc->path);
    return completed;
}

/* This function returns 0 to indicate revocation checking was not completed
   for this certificate chain, otherwise return to date at which the first
   piece of revocation checking info we used expires.  */
CFAbsoluteTime SecPVCGetEarliestNextUpdate(SecPVCRef pvc) {
	CFIndex certIX, certCount = SecPVCGetCertificateCount(pvc);
    CFAbsoluteTime enu = NULL_TIME;
    if (certCount <= 1 || !pvc->rvcs) {
        return enu;
    }
    certCount--;

	for (certIX = 0; certIX < certCount; ++certIX) {
        SecRVCRef rvc = &((SecRVCRef)pvc->rvcs)[certIX];
        CFAbsoluteTime thisCertNextUpdate = SecRVCGetEarliestNextUpdate(rvc);
        if (thisCertNextUpdate == 0) {
            if (certIX > 0) {
                /* We allow for CA certs to not be revocation checked if they
                   have no ocspResponders nor CRLDPs to check against, but the leaf
                   must be checked in order for us to claim we did revocation
                   checking. */
                SecCertificateRef cert =
                    SecPVCGetCertificateAtIndex(rvc->pvc, rvc->certIX);
                CFArrayRef ocspResponders = NULL;
                ocspResponders = SecCertificateGetOCSPResponders(cert);
#if ENABLE_CRLS
                CFArrayRef crlDPs = NULL;
                crlDPs = SecCertificateGetCRLDistributionPoints(cert);
#endif
                if ((!ocspResponders || CFArrayGetCount(ocspResponders) == 0)
#if ENABLE_CRLS
                    && (!crlDPs || CFArrayGetCount(crlDPs) == 0)
#endif
                    ) {
                    /* We can't check this cert so we don't consider it a soft
                       failure that we didn't. Ideally we should support crl
                       checking and remove this workaround, since that more
                       strict. */
                    continue;
                }
            }
            secdebug("rvc", "revocation checking soft failure for cert: %ld",
                certIX);
            enu = thisCertNextUpdate;
            break;
        }
        if (enu == 0 || thisCertNextUpdate < enu) {
            enu = thisCertNextUpdate;
        }
    }

    secdebug("rvc", "revocation valid until: %lg", enu);
    return enu;
}
