/*
 * Copyright (c) 2008-2015 Apple Inc. All Rights Reserved.
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
#include <security_asn1/SecAsn1Coder.h>
#include <security_asn1/ocspTemplates.h>
#include <security_asn1/oidsalg.h>
#include <security_asn1/oidsocsp.h>
#include <CommonCrypto/CommonDigest.h>
#include <Security/SecFramework.h>
#include <Security/SecPolicyInternal.h>
#include <Security/SecTrustPriv.h>
#include <Security/SecInternal.h>
#include <Security/SecKeyPriv.h>
#include <CFNetwork/CFHTTPMessage.h>
#include <CFNetwork/CFHTTPStream.h>
#include <SystemConfiguration/SCDynamicStoreCopySpecific.h>
#include <asl.h>
#include <securityd/SecOCSPRequest.h>
#include <securityd/SecOCSPResponse.h>
#include <securityd/asynchttp.h>
#include <securityd/SecTrustServer.h>
#include <securityd/SecOCSPCache.h>
#include <utilities/array_size.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/SecAppleAnchorPriv.h>
#include "OTATrustUtilities.h"

#define ocspdErrorLog(args...)     asl_log(NULL, NULL, ASL_LEVEL_ERR, ## args)

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
            ocspdErrorLog("EVRoot.plist has non array value");
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
    require_quiet(good_ev_anchor, notEV);

    CFAbsoluteTime october2006 = 178761600;
    if (SecCertificateVersion(certificate) >= 3
        && SecCertificateNotValidBefore(certificate) >= october2006) {
        const SecCEBasicConstraints *bc = SecCertificateGetBasicConstraints(certificate);
        require_quiet(bc && bc->isCA == true, notEV);
        SecKeyUsage ku = SecCertificateGetKeyUsage(certificate);
        require_quiet((ku & (kSecKeyUsageKeyCertSign | kSecKeyUsageCRLSign))
            == (kSecKeyUsageKeyCertSign | kSecKeyUsageCRLSign), notEV);
    }

    CFAbsoluteTime jan2011 = 315532800;
    if (SecCertificateNotValidBefore(certificate) < jan2011) {
        /* At least MD5, SHA-1 with RSA 2048 or ECC NIST P-256. */
    } else {
        /* At least SHA-1, SHA-256, SHA-384 or SHA-512 with RSA 2048 or
           ECC NIST P-256. */
    }

    return true;
notEV:
    return false;
}

static bool SecPolicySubordinateCACertificateCouldBeEV(SecCertificateRef certificate) {
    const SecCECertificatePolicies *cp;
    cp = SecCertificateGetCertificatePolicies(certificate);
    require_quiet(cp && cp->numPolicies > 0, notEV);
    /* SecCertificateGetCRLDistributionPoints() is a noop right now */
#if 0
    CFArrayRef cdp = SecCertificateGetCRLDistributionPoints(certificate);
    require_quiet(cdp && CFArrayGetCount(cdp) > 0, notEV);
#endif
    const SecCEBasicConstraints *bc = SecCertificateGetBasicConstraints(certificate);
    require_quiet(bc && bc->isCA == true, notEV);
    SecKeyUsage ku = SecCertificateGetKeyUsage(certificate);
    require_quiet((ku & (kSecKeyUsageKeyCertSign | kSecKeyUsageCRLSign))
        == (kSecKeyUsageKeyCertSign | kSecKeyUsageCRLSign), notEV);
    CFAbsoluteTime jan2011 = 315532800;
    if (SecCertificateNotValidBefore(certificate) < jan2011) {
        /* At least SHA-1 with RSA 1024 or ECC NIST P-256. */
    } else {
        /* At least SHA-1, SHA-256, SHA-284 or SHA-512 with RSA 2028 or
           ECC NIST P-256. */
    }

    return true;
notEV:
    return false;
}

bool SecPolicySubscriberCertificateCouldBeEV(SecCertificateRef certificate) {
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

    /* SecCertificateGetCRLDistributionPoints() is a noop right now */
#if 0
    /* (b) cRLDistributionPoint
       (c) authorityInformationAccess */
    CFArrayRef cdp = SecCertificateGetCRLDistributionPoints(certificate);
    if (cdp) {
        require_quiet(CFArrayGetCount(cdp) > 0, notEV);
    } else {
        CFArrayRef or = SecCertificateGetOCSPResponders(certificate);
        require_quiet(or && CFArrayGetCount(or) > 0, notEV);
        //CFArrayRef ci = SecCertificateGetCAIssuers(certificate);
    }
#endif

    /* (d) basicConstraints
       If present, the cA field MUST be set false. */
    const SecCEBasicConstraints *bc = SecCertificateGetBasicConstraints(certificate);
    if (bc) {
        require_quiet(bc->isCA == false, notEV);
    }

    /* (e) keyUsage. */
    SecKeyUsage ku = SecCertificateGetKeyUsage(certificate);
    if (ku) {
        require_quiet((ku & (kSecKeyUsageKeyCertSign | kSecKeyUsageCRLSign)) == 0, notEV);
    }

#if 0
    /* The EV Cert Spec errata specifies this, though this is a check for SSL
       not specifically EV. */

    /* (e) extKeyUsage

Either the value id-kp-serverAuth [RFC5280] or id-kp-clientAuth [RFC5280] or both values MUST be present. Other values SHOULD NOT be present. */
    SecCertificateCopyExtendedKeyUsage(certificate);
#endif

    CFAbsoluteTime jan2011 = 315532800;
    if (SecCertificateNotValidAfter(certificate) < jan2011) {
        /* At least SHA-1 with RSA 1024 or ECC NIST P-256. */
    } else {
        /* At least SHA-1, SHA-256, SHA-284 or SHA-512 with RSA 2028 or
           ECC NIST P-256. */
    }

    return true;
notEV:
    return false;
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

static bool keyusage_allows(SecKeyUsage keyUsage, CFTypeRef xku) {
    if (!xku || CFGetTypeID(xku) != CFNumberGetTypeID())
        return false;

    SInt32 dku;
    CFNumberGetValue((CFNumberRef)xku, kCFNumberSInt32Type, &dku);
    SecKeyUsage ku = (SecKeyUsage)dku;
    return (keyUsage & ku) == ku;
}

static void SecPolicyCheckKeyUsage(SecPVCRef pvc,
	CFStringRef key) {
    SecCertificateRef leaf = SecPVCGetCertificateAtIndex(pvc, 0);
    SecKeyUsage keyUsage = SecCertificateGetKeyUsage(leaf);
    bool match = false;
    SecPolicyRef policy = SecPVCGetPolicy(pvc);
    CFTypeRef xku = CFDictionaryGetValue(policy->_options, key);
    if (isArray(xku)) {
        CFIndex ix, count = CFArrayGetCount(xku);
        for (ix = 0; ix < count; ++ix) {
            CFTypeRef ku = CFArrayGetValueAtIndex(xku, ix);
            if (keyusage_allows(keyUsage, ku)) {
                match = true;
                break;
            }
        }
    } else {
        match = keyusage_allows(keyUsage, xku);
    }
    if (!match) {
        SecPVCSetResult(pvc, key, 0, kCFBooleanFalse);
    }
}

static bool extendedkeyusage_allows(CFArrayRef extendedKeyUsage,
                                    CFTypeRef xeku) {
    if (!xeku || CFGetTypeID(xeku) != CFDataGetTypeID())
        return false;
    if (extendedKeyUsage) {
        CFRange all = { 0, CFArrayGetCount(extendedKeyUsage) };
        return CFArrayContainsValue(extendedKeyUsage, all, xeku);
    } else {
        /* Certificate has no extended key usage, only a match if the policy
           contains a 0 length CFDataRef. */
        return CFDataGetLength((CFDataRef)xeku) == 0;
    }
}

/* AUDIT[securityd](done):
   policy->_options is a caller provided dictionary, only its cf type has
   been checked.
 */
static void SecPolicyCheckExtendedKeyUsage(SecPVCRef pvc, CFStringRef key) {
    SecCertificateRef leaf = SecPVCGetCertificateAtIndex(pvc, 0);
    CFArrayRef leafExtendedKeyUsage = SecCertificateCopyExtendedKeyUsage(leaf);
    bool match = false;
    SecPolicyRef policy = SecPVCGetPolicy(pvc);
    CFTypeRef xeku = CFDictionaryGetValue(policy->_options, key);
    if (isArray(xeku)) {
        CFIndex ix, count = CFArrayGetCount(xeku);
        for (ix = 0; ix < count; ix++) {
            CFTypeRef eku = CFArrayGetValueAtIndex(xeku, ix);
            if (extendedkeyusage_allows(leafExtendedKeyUsage, eku)) {
                match = true;
                break;
            }
        }
    } else {
        match = extendedkeyusage_allows(leafExtendedKeyUsage, xeku);
    }
    CFReleaseSafe(leafExtendedKeyUsage);
    if (!match) {
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

static void SecPolicyCheckBasicContraints(SecPVCRef pvc,
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
	CFStringInlineBuffer hbuf, dbuf;
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

/* Compare hostname, to a server name obtained from the server's cert
   Obtained from the SubjectAltName or the CommonName entry in the Subject.
   Limited wildcard checking is performed here as outlined in

   RFC 2818 Section 3.1.  Server Identity

   [...] Names may contain the wildcard
   character * which is considered to match any single domain name
   component or component fragment. E.g., *.a.com matches foo.a.com but
   not bar.foo.a.com. f*.com matches foo.com but not bar.com.
   [...]

   Trailing '.' characters in the hostname will be ignored.

   Returns true on match, else false.

   RFC6125:
 */
bool SecDNSMatch(CFStringRef hostname, CFStringRef servername) {
	CFStringInlineBuffer hbuf, sbuf;
	CFIndex hix, six, tix,
			hlength = CFStringGetLength(hostname),
			slength = CFStringGetLength(servername);
	CFRange hrange = { 0, hlength }, srange = { 0, slength };
	CFStringInitInlineBuffer(hostname, &hbuf, hrange);
	CFStringInitInlineBuffer(servername, &sbuf, srange);
	bool prevLabel=false;

	for (hix = six = 0; six < slength; ++six) {
		UniChar tch, hch, sch = CFStringGetCharacterFromInlineBuffer(&sbuf, six);
		if (sch == '*') {
			if (prevLabel) {
				/* RFC6125: No wildcard after a Previous Label */
				/* INVALID: Means we have something like foo.*.<public_suffix> */
				return false;
			}

			if (six + 1 >= slength) {
				/* Trailing '*' in servername, match until end of hostname or
				   trailing '.'.  */
				do {
					if (hix >= hlength) {
						/* If we reach the end of the hostname we have a
						   match. */
						return true;
					}
					hch = CFStringGetCharacterFromInlineBuffer(&hbuf, hix++);
				} while (hch != '.');
				/* We reached the end of servername and found a '.' in
				   hostname.  Return true if hostname has a single
				   trailing '.' return false if there is anything after it. */
				return hix == hlength;
			}

			/* Grab the character after the '*'. */
			sch = CFStringGetCharacterFromInlineBuffer(&sbuf, ++six);
			if (sch != '.') {
				/* We have something of the form '*foo.com'.  Or '**.com'
				   We don't deal with that yet, since it might require
				   backtracking. Also RFC 2818 doesn't seem to require it. */
				return false;
			}

			/* We're looking at the '.' after the '*' in something of the
			   form 'foo*.com' or '*.com'. Match until next '.' in hostname. */
			if (prevLabel==false) {                 /* RFC6125: Check if *.<tld> */
				tix=six+1;
				do {                                    /* Loop to end of servername */
					if (tix > slength)
						return false;    /* Means we have something like *.com */
					tch = CFStringGetCharacterFromInlineBuffer(&sbuf, tix++);
				} while (tch != '.');
				if (tix > slength)
					return false;        /* In case we have *.com. */
			}

			do {
				/* Since we're not at the end of servername yet (that case
				   was handled above), running out of chars in hostname
				   means we don't have a match. */
				if (hix >= hlength)
					return false;
				hch = CFStringGetCharacterFromInlineBuffer(&hbuf, hix++);
			} while (hch != '.');
		} else {
			/* We're looking at a non wildcard character in the servername.
			   If we reached the end of hostname, it's not a match. */
			if (hix >= hlength)
				return false;

			/* Otherwise make sure the hostname matches the character in the
			   servername, case insensitively. */
			hch = CFStringGetCharacterFromInlineBuffer(&hbuf, hix++);
			if (towlower(hch) != towlower(sch))
				return false;
			if (sch == '.')
				prevLabel=true;   /* Set if a confirmed previous component */
		}
	}

	if (hix < hlength) {
		/* We reached the end of servername but we have one or more characters
		   left to compare against in the hostname. */
		if (hix + 1 == hlength &&
				CFStringGetCharacterFromInlineBuffer(&hbuf, hix) == '.') {
			/* Hostname has a single trailing '.', we're ok with that. */
			return true;
		}
		/* Anything else is not a match. */
		return false;
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
	bool dnsMatch = false;
	CFArrayRef dnsNames = SecCertificateCopyDNSNames(leaf);
	if (dnsNames) {
		CFIndex ix, count = CFArrayGetCount(dnsNames);
		for (ix = 0; ix < count; ++ix) {
			CFStringRef dns = (CFStringRef)CFArrayGetValueAtIndex(dnsNames, ix);
			if (SecDNSMatch(hostName, dns)) {
				dnsMatch = true;
				break;
			}
		}
		CFRelease(dnsNames);
	}

    if (!dnsMatch) {
        /* Maybe hostname is an IPv4 or IPv6 address, let's compare against
           the values returned by SecCertificateCopyIPAddresses() instead. */
        CFArrayRef ipAddresses = SecCertificateCopyIPAddresses(leaf);
        if (ipAddresses) {
            CFIndex ix, count = CFArrayGetCount(ipAddresses);
            for (ix = 0; ix < count; ++ix) {
                CFStringRef ipAddress = (CFStringRef)CFArrayGetValueAtIndex(ipAddresses, ix);
                if (!CFStringCompare(hostName, ipAddress, kCFCompareCaseInsensitive)) {
                    dnsMatch = true;
                    break;
                }
            }
            CFRelease(ipAddresses);
        }
    }

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
        /* optionally_ev => check_revocation, so we don't enable revocation
           checking here, since we don't want it on for non EV ssl certs.  */
#if 0
        /* Check revocation status if the certificate asks for it (and we
           support it) currently we only support ocsp. */
        CFArrayRef ocspResponders = SecCertificateGetOCSPResponders(leaf);
        if (ocspResponders) {
            SecPVCSetCheckRevocation(pvc);
        }
#endif
    }
}

/* AUDIT[securityd](done):
 policy->_options is a caller provided dictionary, only its cf type has
 been checked.
 */
static void SecPolicyCheckEmail(SecPVCRef pvc, CFStringRef key) {
	SecPolicyRef policy = SecPVCGetPolicy(pvc);
	CFStringRef email = (CFStringRef)CFDictionaryGetValue(policy->_options, key);
	bool match = false;
    if (!isString(email)) {
        /* We can't return an error here and making the evaluation fail
         won't help much either. */
        return;
    }

	SecCertificateRef leaf = SecPVCGetCertificateAtIndex(pvc, 0);
	CFArrayRef addrs = SecCertificateCopyRFC822Names(leaf);
	if (addrs) {
		CFIndex ix, count = CFArrayGetCount(addrs);
		for (ix = 0; ix < count; ++ix) {
			CFStringRef addr = (CFStringRef)CFArrayGetValueAtIndex(addrs, ix);
			if (!CFStringCompare(email, addr, kCFCompareCaseInsensitive)) {
				match = true;
				break;
			}
		}
		CFRelease(addrs);
	}

	if (!match) {
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
    CFArrayRef commonNames = SecCertificateCopyCommonNames(cert);
    if (!commonNames || CFArrayGetCount(commonNames) != 1 ||
        !CFEqual(commonName, CFArrayGetValueAtIndex(commonNames, 0))) {
		/* Common Name mismatch. */
		SecPVCSetResult(pvc, key, 0, kCFBooleanFalse);
	}
    CFReleaseSafe(commonNames);
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
	CFArrayRef commonNames = SecCertificateCopyCommonNames(cert);
	if (!commonNames || CFArrayGetCount(commonNames) != 1 ||
		!CFEqual(common_name, CFArrayGetValueAtIndex(commonNames, 0))) {
		/* Common Name mismatch. */
		SecPVCSetResult(pvc, key, 0, kCFBooleanFalse);
	}
	CFReleaseSafe(commonNames);
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
    CFArrayRef commonNames = SecCertificateCopyCommonNames(cert);
    if (!commonNames || CFArrayGetCount(commonNames) != 1 ||
        !CFStringHasPrefix(CFArrayGetValueAtIndex(commonNames, 0), prefix)) {
		/* Common Name prefix mismatch. */
		SecPVCSetResult(pvc, key, 0, kCFBooleanFalse);
	}
    CFReleaseSafe(commonNames);
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
	CFArrayRef commonNames = SecCertificateCopyCommonNames(cert);
	if (!commonNames || CFArrayGetCount(commonNames) != 1) {
        CFStringRef cert_common_name = CFArrayGetValueAtIndex(commonNames, 0);
        CFStringRef test_common_name = common_name ?
            CFStringCreateWithFormat(kCFAllocatorDefault,
                NULL, CFSTR("TEST %@ TEST"), common_name) :
            NULL;
		if (!CFEqual(common_name, cert_common_name) &&
            (!test_common_name || !CFEqual(test_common_name, cert_common_name)))
                /* Common Name mismatch. */
                SecPVCSetResult(pvc, key, 0, kCFBooleanFalse);
        CFReleaseSafe(test_common_name);
	}
	CFReleaseSafe(commonNames);
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
	CFAbsoluteTime at = CFDateGetAbsoluteTime(date);
	if (SecCertificateNotValidBefore(cert) <= at) {
		/* Leaf certificate has not valid before that is too old. */
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

/* AUDIT[securityd](done):
   policy->_options is a caller provided dictionary, only its cf type has
   been checked.
 */
static void SecPolicyCheckAnchorSHA1(SecPVCRef pvc,
	CFStringRef key) {
    CFIndex count = SecPVCGetCertificateCount(pvc);
	SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, count - 1);
	SecPolicyRef policy = SecPVCGetPolicy(pvc);
    CFTypeRef value = CFDictionaryGetValue(policy->_options, key);
    CFDataRef anchorSHA1 = SecCertificateGetSHA1Digest(cert);

    bool foundMatch = false;

    if (isData(value))
        foundMatch = CFEqual(anchorSHA1, value);
    else if (isArray(value))
        foundMatch = CFArrayContainsValue((CFArrayRef) value, CFRangeMake(0, CFArrayGetCount((CFArrayRef) value)), anchorSHA1);
    else {
        /* @@@ We only support Data and Array but we can't return an error here so.
               we let the evaluation fail (not much help) and assert in debug. */
        assert(false);
    }

    if (!foundMatch)
        if (!SecPVCSetResult(pvc, kSecPolicyCheckAnchorSHA1, 0, kCFBooleanFalse))
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
    SecPolicyRef policy = SecPVCGetPolicy(pvc);
    CFTypeRef value = CFDictionaryGetValue(policy->_options, key);
    SecCertificateRef cert = NULL;
    CFDataRef digest = NULL;
    bool foundMatch = false;

    if (SecPVCGetCertificateCount(pvc) < 2) {
        SecPVCSetResult(pvc, kSecPolicyCheckIntermediateSPKISHA256, 0, kCFBooleanFalse);
        return;
    }

    cert = SecPVCGetCertificateAtIndex(pvc, 1);
    digest = SecCertificateCopySubjectPublicKeyInfoSHA256Digest(cert);

    if (isData(value))
        foundMatch = CFEqual(digest, value);
    else if (isArray(value))
        foundMatch = CFArrayContainsValue((CFArrayRef) value, CFRangeMake(0, CFArrayGetCount((CFArrayRef) value)), digest);
    else {
        /* @@@ We only support Data and Array but we can't return an error here so.
         we let the evaluation fail (not much help) and assert in debug. */
        assert(false);
    }

    CFReleaseNull(digest);

    if (!foundMatch) {
        SecPVCSetResult(pvc, kSecPolicyCheckIntermediateSPKISHA256, 0, kCFBooleanFalse);
    }
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
        if (CFDictionaryGetValue(value, kSecPolicyAppleAnchorIncludeTestRoots))
            flags |= kSecAppleTrustAnchorFlagsIncludeTestAnchors;
        if (CFDictionaryGetValue(value, kSecPolicyAppleAnchorAllowTestRootsOnProduction))
            flags |= kSecAppleTrustAnchorFlagsAllowNonProduction;
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
	CFArrayRef organization = SecCertificateCopyOrganization(cert);
	if (!organization || CFArrayGetCount(organization) != 1 ||
		!CFEqual(org, CFArrayGetValueAtIndex(organization, 0))) {
		/* Leaf Subject Organization mismatch. */
		SecPVCSetResult(pvc, key, 0, kCFBooleanFalse);
	}
	CFReleaseSafe(organization);
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
	CFArrayRef organizationalUnit = SecCertificateCopyOrganizationalUnit(cert);
	if (!organizationalUnit || CFArrayGetCount(organizationalUnit) != 1 ||
		!CFEqual(orgUnit, CFArrayGetValueAtIndex(organizationalUnit, 0))) {
		/* Leaf Subject Organizational Unit mismatch. */
		SecPVCSetResult(pvc, key, 0, kCFBooleanFalse);
	}
	CFReleaseSafe(organizationalUnit);
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

    CFIndex tsnCount = CFArrayGetCount(trustedServerNames);
	SecCertificateRef leaf = SecPVCGetCertificateAtIndex(pvc, 0);
	bool dnsMatch = false;
	CFArrayRef dnsNames = SecCertificateCopyDNSNames(leaf);
	if (dnsNames) {
		CFIndex ix, count = CFArrayGetCount(dnsNames);
        // @@@ This is O(N^2) unfortunately we can't do better easily unless
        // we don't do wildcard matching. */
		for (ix = 0; !dnsMatch && ix < count; ++ix) {
			CFStringRef dns = (CFStringRef)CFArrayGetValueAtIndex(dnsNames, ix);
            CFIndex tix;
            for (tix = 0; tix < tsnCount; ++tix) {
                CFStringRef serverName =
                    (CFStringRef)CFArrayGetValueAtIndex(trustedServerNames, tix);
                if (!isString(serverName)) {
                    /* @@@ We can't return an error here and making the
                       evaluation fail won't help much either. */
                    CFReleaseSafe(dnsNames);
                    return;
                }
                /* we purposefully reverse the arguments here such that dns names
                   from the cert are matched against a server name list, where
                   the server names list can contain wildcards and the dns name
                   cannot.  References: http://support.microsoft.com/kb/941123
                   It's easy to find occurrences where people tried to use
                   wildcard certificates and were told that those don't work
                   in this context. */
                if (SecDNSMatch(dns, serverName)) {
                    dnsMatch = true;
                    break;
                }
            }
		}
		CFRelease(dnsNames);
	}

	if (!dnsMatch) {
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

    if (value && SecCertificateHasMarkerExtension(cert, value))
        return;

    SecPVCSetResult(pvc, key, 0, kCFBooleanFalse);
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

/* Returns true if path is on the allow list, false otherwise */
static bool SecPVCCheckCertificateAllowList(SecPVCRef pvc)
{
    bool result = false;
    CFIndex ix = 0, count = SecPVCGetCertificateCount(pvc);
    CFStringRef authKey = NULL;
    SecOTAPKIRef otapkiRef = NULL;

    //get authKeyID from the last chain in the cert
    if (count < 1) {
        return result;
    }
    SecCertificateRef lastCert = SecPVCGetCertificateAtIndex(pvc, count - 1);
    CFDataRef authKeyID = SecCertificateGetAuthorityKeyID(lastCert);
    if (NULL == authKeyID) {
        return result;
    }
    authKey = CFDataCopyHexString(authKeyID);

    //if allowList && key is in allowList, this would have chained up to a now-removed anchor
    otapkiRef = SecOTAPKICopyCurrentOTAPKIRef();
    if (NULL == otapkiRef) {
        goto errout;
    }
    CFDictionaryRef allowList = SecOTAPKICopyAllowList(otapkiRef);
    if (NULL == allowList) {
        goto errout;
    }

    CFArrayRef allowedCerts = CFDictionaryGetValue(allowList, authKey);
    if (!allowedCerts || !CFArrayGetCount(allowedCerts)) {
        goto errout;
    }

    //search sorted array for the SHA256 hash of a cert in the chain
    CFRange range = CFRangeMake(0, CFArrayGetCount(allowedCerts));
    for (ix = 0; ix < count; ix++) {
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
    CFRelease(authKey);
    CFReleaseNull(otapkiRef);
    CFReleaseNull(allowList);
    return result;
}


/****************************************************************************
 *********************** New rfc5280 Chain Validation ***********************
 ****************************************************************************/

#if 0
typedef struct cert_path *cert_path_t;
struct cert_path {
    int length;
};

typedef struct x500_name *x500_name_t;
struct x500_name {
};

typedef struct algorithm_id *algorithm_id_t;
struct algorithm_id {
    oid_t algorithm_oid;
    der_t parameters;
};

typedef struct trust_anchor *trust_anchor_t;
struct trust_anchor {
    x500_name_t issuer_name;
    algorithm_id_t public_key_algorithm; /* includes optional params */
    SecKeyRef public_key;
};

typedef struct certificate_policy *certificate_policy_t;
struct certificate_policy {
    policy_qualifier_t qualifiers;
    oid_t oid;
    SLIST_ENTRY(certificate_policy) policies;
};

typedef struct policy_mapping *policy_mapping_t;
struct policy_mapping {
    SLIST_ENTRY(policy_mapping) mappings;
    oid_t issuer_domain_policy;
    oid_t subject_domain_policy;
};

typedef struct root_name *root_name_t;
struct root_name {
};
#endif

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

#if 0
/* For each node where ID-P is the valid_policy, set expected_policy_set to the set of subjectDomainPolicy values that are specified as equivalent to ID-P by the policy mappings extension. */
static bool policy_tree_map(policy_tree_t node, void *ctx) {
    /* Can't map oidAnyPolicy. */
    if (oid_equal(node->valid_policy, oidAnyPolicy))
        return false;

    const SecCEPolicyMappings *pm = (const SecCEPolicyMappings *)ctx;
    uint32_t mapping_ix, mapping_count = pm->numMappings;
    policy_set_t policy_set = NULL;
    /* First count how many mappings match this nodes valid_policy. */
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
#endif

#define POLICY_MAPPING 0
#define POLICY_SUBTREES 1

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
        /* If the anchor is trusted we don't procces the last cert in the
           chain (root). */
        n--;
    } else {
        /* trust may be restored for a path with an untrusted root that matches the allow list */
        if (!SecPVCCheckCertificateAllowList(pvc)) {
            /* Add a detail for the root not being trusted. */
            if (SecPVCSetResultForced(pvc, kSecPolicyCheckAnchorTrusted,
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
    assert(permitted_subtrees != NULL);
    assert(excluded_subtrees != NULL);
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
        bool is_self_issued = SecPVCIsCertificateAtIndexSelfSigned(pvc, n - i);

        /* (a) Verify the basic certificate information. */
        /* @@@ Ensure that cert was signed with working_public_key_algorithm
           using the working_public_key and the working_public_key_parameters. */
#if 1
        /* Already done by chain builder. */
        if (!SecCertificateIsValid(cert, verify_time)) {
            CFStringRef fail_key = i == n ? kSecPolicyCheckValidLeaf : kSecPolicyCheckValidIntermediates;
            if (!SecPVCSetResult(pvc, fail_key, n - i, kCFBooleanFalse))
                return;
        }
        if (SecCertificateIsWeak(cert)) {
            CFStringRef fail_key = i == n ? kSecPolicyCheckWeakLeaf : kSecPolicyCheckWeakIntermediates;
            if (!SecPVCSetResult(pvc, fail_key, n - i, kCFBooleanFalse))
                return;
        }
#endif
#if 0
        /* Check revocation status if the certificate asks for it. */
        CFArrayRef ocspResponders = SecCertificateGetOCSPResponders(cert);
        if (ocspResponders) {
            SecPVCSetCheckRevocation(pvc);
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
                    if(!SecPVCSetResultForced(pvc, key, n - i, kCFBooleanFalse, true)) return;
                }
            }
            /* Verify certificate Subject Name and SubjectAltNames are within the permitted_subtrees */
            if(permitted_subtrees && CFArrayGetCount(permitted_subtrees)) {
               if ((errSecSuccess != SecNameContraintsMatchSubtrees(cert, permitted_subtrees, &found, true)) || !found) {
                   if(!SecPVCSetResultForced(pvc, key, n - i, kCFBooleanFalse, true)) return;
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
            if (!SecPVCSetResultForced(pvc, key /* @@@ Need custom key */, n - i, kCFBooleanFalse, true))
                return;
        }
        /* If Last Cert in Path */
        if (i == n)
            break;

        /* Prepare for Next Cert */
#if POLICY_MAPPING
        /* (a) verify that anyPolicy does not appear as an
           issuerDomainPolicy or a subjectDomainPolicy */
        CFDictionaryRef pm = SecCertificateGetPolicyMappings(cert);
        if (pm) {
            uint32_t mapping_ix, mapping_count = pm->numMappings;
            for (mapping_ix = 0; mapping_ix < mapping_count; ++mapping_ix) {
                const SecCEPolicyMapping *mapping = &pm->mappings[mapping_ix];
                if (oid_equal(mapping->issuerDomainPolicy, oidAnyPolicy)
                    || oid_equal(mapping->subjectDomainPolicy, oidAnyPolicy)) {
                    /* Policy mapping uses anyPolicy, illegal. */
                    if (!SecPVCSetResultForced(pvc, key /* @@@ Need custom key */, n - i, kCFBooleanFalse))
                        return;
                }
            }
            /* (b) */
            /* (1) If the policy_mapping variable is greater than 0 */
            if (policy_mapping > 0) {
                if (!policy_tree_walk_depth(pvc->valid_policy_tree, i,
                    policy_tree_map, (void *)pm)) {
                        /* If no node of depth i in the valid_policy_tree has a valid_policy of ID-P but there is a node of depth i with a valid_policy of anyPolicy, then generate a child node of the node of depth i-1 that has a valid_policy of anyPolicy as follows:

            (i)    set the valid_policy to ID-P;

            (ii)   set the qualifier_set to the qualifier set of the
                   policy anyPolicy in the certificate policies
                   extension of certificate i; and
    (iii) set the expected_policy_set to the set of subjectDomainPolicy values that are specified as equivalent to ID-P by the policy mappings extension. */
                    }
            } else {
    #if 0
                /* (i)    delete each node of depth i in the valid_policy_tree
                   where ID-P is the valid_policy. */
                struct policy_tree_map_ctx ctx = { idp_oid, sdp_oid };
                policy_tree_walk_depth(pvc->valid_policy_tree, i,
                    policy_tree_delete_if_match, &ctx);
    #endif
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
        uint32_t iap = SecCertificateGetInhibitAnyPolicySkipCerts(cert);
        if (iap < inhibit_any_policy) {
            inhibit_any_policy = iap;
        }
        /* (k) */
		const SecCEBasicConstraints *bc =
			SecCertificateGetBasicConstraints(cert);
#if 0 /* Checked in chain builder pre signature verify already. */
        if (!bc || !bc->isCA) {
            /* Basic constraints not present or not marked as isCA, illegal. */
            if (!SecPVCSetResult(pvc, kSecPolicyCheckBasicContraints,
                n - i, kCFBooleanFalse))
                return;
        }
#endif
        /* (l) */
        if (!is_self_issued) {
            if (max_path_length > 0) {
                max_path_length--;
            } else {
                /* max_path_len exceeded, illegal. */
                if (!SecPVCSetResult(pvc, kSecPolicyCheckBasicContraints,
                    n - i, kCFBooleanFalse))
                    return;
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
                n - i, kCFBooleanFalse, true))
                return;
        }
#endif
        /* (o) Recognize and process any other critical extension present in the certificate. Process any other recognized non-critical extension present in the certificate that is relevant to path processing. */
        if (SecCertificateHasUnknownCriticalExtension(cert)) {
			/* Certificate contains one or more unknown critical extensions. */
			if (!SecPVCSetResult(pvc, kSecPolicyCheckCriticalExtensions,
                n - i, kCFBooleanFalse))
				return;
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
                             0, kCFBooleanFalse))
            return;
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
        if (!SecPVCSetResultForced(pvc, key /* @@@ Need custom key */, 0, kCFBooleanFalse, true))
            return;
    }

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
                secdebug("ev", "subordinate certificate is not ev");
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
                secdebug("ev", "anchor certificate is not ev");
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
            secdebug("ev", "valid_policies set is empty: chain not ev");
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

/* If the 'sct' is valid, return the operator ID of the log that signed this sct.

   The SCT is valid if:
    - It decodes properly.
    - Its timestamp is less than 'verifyTime'.
    - It is signed by a log in 'trustedLogs'.
    - The signing log expiryTime (if any) is less than 'verifyTime' (entry_type==0) or 'issuanceTime' (entry_type==1).

   If the SCT is valid, the returned CFStringRef is the identifier for the log operator. That value is not retained.
   If the SCT is valid, '*validLogAtVerifyTime' is set to true if the log is not expired at 'verifyTime'

   If the SCT is not valid this function return NULL.
 */
static CFStringRef get_valid_sct_operator(CFDataRef sct, int entry_type, CFDataRef entry, CFAbsoluteTime verifyTime, CFAbsoluteTime issuanceTime, CFArrayRef trustedLogs, bool *validLogAtVerifyTime)
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
    CFStringRef result = NULL;
    SecKeyRef pubKey = NULL;
    uint8_t *signed_data = NULL;
    const SecAsn1Oid *oid = NULL;
    SecAsn1AlgId algId;

    const uint8_t *p = CFDataGetBytePtr(sct);
    size_t len = CFDataGetLength(sct);
    uint64_t vt =(uint64_t)( verifyTime + kCFAbsoluteTimeIntervalSince1970) * 1000;

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

    CFDataRef logIDData = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, logID, 32, kCFAllocatorNull);

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
    CFReleaseSafe(logIDData);
    require(logData, out);

    /* If an expiry date is specified, and is a valid CFDate, then we check it against issuanceTime or verifyTime */
    const void *expiry_date;
    if(CFDictionaryGetValueIfPresent(logData, CFSTR("expiry"), &expiry_date) && isDate(expiry_date)) {
        CFAbsoluteTime expiryTime = CFDateGetAbsoluteTime(expiry_date);
        if(entry_type == 1) {/* pre-cert: check the validity of the log at issuanceTime */
            require(issuanceTime<=expiryTime, out);
        } else {
            require(verifyTime<=expiryTime, out);
        }
        *validLogAtVerifyTime = (verifyTime<=expiryTime);
    } else {
        *validLogAtVerifyTime = true;
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
        result = CFDictionaryGetValue(logData, CFSTR("operator"));
    } else {
        secerror("SCT signature failed (log=%@)\n", logData);
    }

out:
    CFReleaseSafe(pubKey);
    free(signed_data);
    return result;
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

    // This eventually contain the list of operators who validated the SCT.
    CFMutableSetRef operatorsValidatingEmbeddedScts = CFSetCreateMutable(kCFAllocatorDefault, 0, &kCFTypeSetCallBacks);
    CFMutableSetRef operatorsValidatingExternalScts = CFSetCreateMutable(kCFAllocatorDefault, 0, &kCFTypeSetCallBacks);

    __block bool atLeastOneValidAtVerifyTime = false;
    __block int lifetime; // in Months

    require(operatorsValidatingEmbeddedScts, out);
    require(operatorsValidatingExternalScts, out);

    if(trustedLogs) { // Don't bother trying to validate SCTs if we don't have any trusted logs.
        if(embeddedScts && precertEntry) { // Don't bother if we could not get the precert.
            CFArrayForEach(embeddedScts, ^(const void *value){
                bool validLogAtVerifyTime = false;
                CFStringRef operator = get_valid_sct_operator(value, 1, precertEntry, pvc->verifyTime, SecCertificateNotValidBefore(leafCert), trustedLogs, &validLogAtVerifyTime);
                if(operator) CFSetAddValue(operatorsValidatingEmbeddedScts, operator);
                if(validLogAtVerifyTime) atLeastOneValidAtVerifyTime = true;
            });
        }

        if(builderScts && x509Entry) { // Don't bother if we could not get the cert.
            CFArrayForEach(builderScts, ^(const void *value){
                bool validLogAtVerifyTime = false;
                CFStringRef operator = get_valid_sct_operator(value, 0, x509Entry, pvc->verifyTime, SecCertificateNotValidBefore(leafCert), trustedLogs, &validLogAtVerifyTime);
                if(operator) CFSetAddValue(operatorsValidatingExternalScts, operator);
                if(validLogAtVerifyTime) atLeastOneValidAtVerifyTime = true;
            });
        }

        if(ocspScts && x509Entry) {
            CFArrayForEach(ocspScts, ^(const void *value){
                bool validLogAtVerifyTime = false;
                CFStringRef operator = get_valid_sct_operator(value, 0, x509Entry, pvc->verifyTime, SecCertificateNotValidBefore(leafCert), trustedLogs, &validLogAtVerifyTime);
                if(operator) CFSetAddValue(operatorsValidatingExternalScts, operator);
                if(validLogAtVerifyTime) atLeastOneValidAtVerifyTime = true;
            });
        }
    }

    /* We now have 2 sets of operators that validated those SCTS, count them and make a final decision.
       Current Policy:
        is_ct = (A1 OR A2) AND B.

       A1: 2+ to 5+ SCTs from the cert from independent logs valid at issuance time
         (operatorsValidatingEmbeddedScts)
       A2: 2+ SCTs from external sources (OCSP stapled response and TLS extension)
         from independent logs valid at verify time. (operatorsValidatingExternalScts)
       B: All least one SCTs from a log valid at verify time.

       Policy is based on: https://docs.google.com/viewer?a=v&pid=sites&srcid=ZGVmYXVsdGRvbWFpbnxjZXJ0aWZpY2F0ZXRyYW5zcGFyZW5jeXxneDo0ODhjNGRlOTIyMzYwNTcz
        with one difference: we consider SCTs from OCSP and TLS extensions as a whole.
       It sounds like this is what Google will eventually do, per:
        https://groups.google.com/forum/?fromgroups#!topic/certificate-transparency/VdXuzA3TLWY

     */

    SecCFCalendarDoWithZuluCalendar(^(CFCalendarRef zuluCalendar) {
        int _lifetime;
        CFCalendarGetComponentDifference(zuluCalendar,
                                         SecCertificateNotValidBefore(leafCert),
                                         SecCertificateNotValidAfter(leafCert),
                                         0, "M", &_lifetime);
        lifetime = _lifetime;
    });

    CFIndex requiredEmbeddedSctsCount;

    if (lifetime < 15) {
        requiredEmbeddedSctsCount = 2;
    } else if (lifetime <= 27) {
        requiredEmbeddedSctsCount = 3;
    } else if (lifetime <= 39) {
        requiredEmbeddedSctsCount = 4;
    } else {
        requiredEmbeddedSctsCount = 5;
    }

    pvc->is_ct = ((CFSetGetCount(operatorsValidatingEmbeddedScts) >= requiredEmbeddedSctsCount) ||
                  (CFSetGetCount(operatorsValidatingExternalScts) >= 2)
                 ) && atLeastOneValidAtVerifyTime;

out:

    CFReleaseSafe(operatorsValidatingEmbeddedScts);
    CFReleaseSafe(operatorsValidatingExternalScts);
    CFReleaseSafe(builderScts);
    CFReleaseSafe(embeddedScts);
    CFReleaseSafe(ocspScts);
    CFReleaseSafe(precertEntry);
    CFReleaseSafe(trustedLogs);
    CFReleaseSafe(x509Entry);
}

static void SecPolicyCheckCertificatePolicyOid(SecPVCRef pvc, CFStringRef key)
{
	CFIndex ix, count = SecPVCGetCertificateCount(pvc);
    SecPolicyRef policy = SecPVCGetPolicy(pvc);
    CFTypeRef value = CFDictionaryGetValue(policy->_options, key);
	DERItem	key_value;
	key_value.data = NULL;
	key_value.length = 0;

	if (CFGetTypeID(value) == CFDataGetTypeID())
	{
		CFDataRef key_data = (CFDataRef)value;
		key_value.data = (DERByte *)CFDataGetBytePtr(key_data);
		key_value.length = (DERSize)CFDataGetLength(key_data);

		for (ix = 0; ix < count; ix++) {
	        SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, ix);
            policy_set_t policies = policies_for_cert(cert);

			if (policy_set_contains(policies, &key_value)) {
				return;
			}
		}
		SecPVCSetResult(pvc, key, 0, kCFBooleanFalse);
	}
}


static void SecPolicyCheckRevocation(SecPVCRef pvc,
	CFStringRef key) {
    SecPVCSetCheckRevocation(pvc);
}

static void SecPolicyCheckRevocationResponseRequired(SecPVCRef pvc,
	CFStringRef key) {
    SecPVCSetCheckRevocationResponseRequired(pvc);
}

static void SecPolicyCheckNoNetworkAccess(SecPVCRef pvc,
	CFStringRef key) {
    SecPathBuilderSetCanAccessNetwork(pvc->builder, false);
}

static void SecPolicyCheckWeakIntermediates(SecPVCRef pvc,
    CFStringRef key) {
    CFIndex ix, count = SecPVCGetCertificateCount(pvc);
    for (ix = 1; ix < count - 1; ++ix) {
        SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, ix);
        if (cert && SecCertificateIsWeak(cert)) {
            /* Intermediate certificate has a weak key. */
            if (!SecPVCSetResult(pvc, key, ix, kCFBooleanFalse))
                return;
        }
    }
}

static void SecPolicyCheckWeakLeaf(SecPVCRef pvc,
    CFStringRef key) {
    SecCertificateRef cert = SecPVCGetCertificateAtIndex(pvc, 0);
    if (cert && SecCertificateIsWeak(cert)) {
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
    if (cert && SecCertificateIsWeak(cert)) {
        /* Root certificate has a weak key. */
        if (!SecPVCSetResult(pvc, key, ix, kCFBooleanFalse))
            return;
    }
}

// MARK: -
// MARK: SecRVCRef
/********************************************************
 ****************** SecRVCRef Functions *****************
 ********************************************************/

const CFAbsoluteTime kSecDefaultOCSPResponseTTL = 24.0 * 60.0 * 60.0;

/* Revocation verification context. */
struct OpaqueSecRVC {
    /* Will contain the response data. */
    asynchttp_t http;

    /* Pointer to the pvc for this revocation check. */
    SecPVCRef pvc;

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
typedef struct OpaqueSecRVC *SecRVCRef;

static void SecRVCDelete(SecRVCRef rvc) {
    secdebug("alloc", "%p", rvc);
    asynchttp_free(&rvc->http);
    SecOCSPRequestFinalize(rvc->ocspRequest);
    if (rvc->ocspResponse) {
        SecOCSPResponseFinalize(rvc->ocspResponse);
        rvc->ocspResponse = NULL;
        if (rvc->ocspSingleResponse) {
            SecOCSPSingleResponseDestroy(rvc->ocspSingleResponse);
            rvc->ocspSingleResponse = NULL;
        }
    }
}

/* Return the next responder we should contact for this rvc or NULL if we
   exhausted them all. */
static CFURLRef SecRVCGetNextResponder(SecRVCRef rvc) {
    SecCertificateRef cert = SecPVCGetCertificateAtIndex(rvc->pvc, rvc->certIX);
    CFArrayRef ocspResponders = SecCertificateGetOCSPResponders(cert);
    if (ocspResponders) {
        CFIndex responderCount = CFArrayGetCount(ocspResponders);
        while (rvc->responderIX < responderCount) {
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
static bool SecRVCFetchNext(SecRVCRef rvc) {
    while ((rvc->responder = SecRVCGetNextResponder(rvc))) {
        CFDataRef request = SecOCSPRequestGetDER(rvc->ocspRequest);
        if (!request)
            goto errOut;

        secdebug("ocsp", "Sending http ocsp request for cert %ld", rvc->certIX);
        if (!asyncHttpPost(rvc->responder, request, &rvc->http)) {
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
    SecRVCRef rvc) {
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
        secdebug("ocsp", "BAD certStatus (%d) for cert %" PRIdCFIndex,
            (int)this->certStatus, rvc->certIX);
        processed = false;
        break;
    }

    return processed;
}

static void SecRVCUpdatePVC(SecRVCRef rvc) {
    if (rvc->ocspSingleResponse) {
        SecOCSPSingleResponseProcess(rvc->ocspSingleResponse, rvc);
    }
    if (rvc->ocspResponse) {
        rvc->nextUpdate = SecOCSPResponseGetExpirationTime(rvc->ocspResponse);
    }
}

static bool SecOCSPResponseVerify(SecOCSPResponseRef ocspResponse, SecRVCRef rvc, CFAbsoluteTime verifyTime) {
    bool trusted;
    SecCertificatePathRef issuer = SecCertificatePathCopyFromParent(rvc->pvc->path, rvc->certIX + 1);
    SecCertificatePathRef signer = SecOCSPResponseCopySigner(ocspResponse, issuer);
    CFRelease(issuer);

    if (signer) {
        if (signer == issuer) {
            /* We already know we trust issuer since it's the path we are
               trying to verify minus the leaf. */
            secdebug("ocsp", "ocsp responder: %@ response signed by issuer",
                rvc->responder);
            trusted = true;
        } else {
            secdebug("ocsp",
                "ocsp responder: %@ response signed by cert issued by issuer",
                rvc->responder);
            /* @@@ Now check that we trust signer. */
            const void *ocspSigner = SecPolicyCreateOCSPSigner();
            CFArrayRef policies = CFArrayCreate(kCFAllocatorDefault,
                &ocspSigner, 1, &kCFTypeArrayCallBacks);
            CFRelease(ocspSigner);
            struct OpaqueSecPVC ospvc;
            SecPVCInit(&ospvc, rvc->pvc->builder, policies, verifyTime);
            CFRelease(policies);
            SecPVCSetPath(&ospvc, signer, NULL);
            SecPVCLeafChecks(&ospvc);
            if (ospvc.result) {
                bool completed = SecPVCPathChecks(&ospvc);
                /* If completed is false we are waiting for a callback, this
                   shouldn't happen since we aren't asking for details, no
                   revocation checking is done. */
                if (!completed) {
                    ocspdErrorLog("SecPVCPathChecks unexpectedly started "
                        "background job!");
                    /* @@@ assert() or abort here perhaps? */
                }
            }
            if (ospvc.result) {
                secdebug("ocsp", "response satisfies ocspSigner policy (%@)",
                    rvc->responder);
                trusted = true;
            } else {
                /* @@@ We don't trust the cert so don't use this response. */
                ocspdErrorLog("ocsp response signed by certificate which "
                    "does not satisfy ocspSigner policy");
                trusted = false;
            }
            SecPVCDelete(&ospvc);
        }

        CFRelease(signer);
    } else {
        /* @@@ No signer found for this ocsp response, discard it. */
        secdebug("ocsp", "ocsp responder: %@ no signer found for response",
            rvc->responder);
        trusted = false;
    }

#if DUMP_OCSPRESPONSES
    char buf[40];
    snprintf(buf, 40, "/tmp/ocspresponse%ld%s.der",
        rvc->certIX, (trusted ? "t" : "u"));
    secdumpdata(ocspResponse->data, buf);
#endif

    return trusted;
}

static void SecRVCConsumeOCSPResponse(SecRVCRef rvc, SecOCSPResponseRef ocspResponse /*CF_CONSUMED*/, CFTimeInterval maxAge, bool updateCache) {
    SecOCSPSingleResponseRef sr = NULL;
    require_quiet(ocspResponse, errOut);
    SecOCSPResponseStatus orStatus = SecOCSPGetResponseStatus(ocspResponse);
    require_action_quiet(orStatus == kSecOCSPSuccess, errOut,
                         secdebug("ocsp", "responder: %@ returned status: %d",  rvc->responder, orStatus));
    require_action_quiet(sr = SecOCSPResponseCopySingleResponse(ocspResponse, rvc->ocspRequest), errOut,
            secdebug("ocsp",  "ocsp responder: %@ did not include status of requested cert", rvc->responder));
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
    SecRVCRef rvc = (SecRVCRef)http->info;
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

    SecRVCConsumeOCSPResponse(rvc, ocspResponse, maxAge, true);
    // TODO: maybe we should set the cache-control: false in the http header and try again if the response is stale

    if (!rvc->done) {
        /* Clear the data for the next response. */
        asynchttp_free(http);
        SecRVCFetchNext(rvc);
    }

    if (rvc->done) {
        SecRVCUpdatePVC(rvc);
        SecRVCDelete(rvc);
        if (!--pvc->asyncJobCount) {
            SecPathBuilderStep(pvc->builder);
        }
    }
}

static void SecRVCInit(SecRVCRef rvc, SecPVCRef pvc, CFIndex certIX) {
    secdebug("alloc", "%p", rvc);
    rvc->pvc = pvc;
    rvc->certIX = certIX;
    rvc->http.queue = SecPathBuilderGetQueue(pvc->builder);
    rvc->http.token = SecPathBuilderCopyClientAuditToken(pvc->builder);
    rvc->http.completed = SecOCSPFetchCompleted;
    rvc->http.info = rvc;
    rvc->ocspRequest = NULL;
    rvc->responderIX = 0;
    rvc->responder = NULL;
    rvc->nextUpdate = NULL_TIME;
    rvc->ocspResponse = NULL;
    rvc->ocspSingleResponse = NULL;
    rvc->done = false;
}


static bool SecPVCCheckRevocation(SecPVCRef pvc) {
    secdebug("ocsp", "checking revocation");
	CFIndex certIX, certCount = SecPVCGetCertificateCount(pvc);
    bool completed = true;
    if (certCount <= 1) {
		/* Can't verify without an issuer; we're done */
        return completed;
    }
    if (!SecPVCIsAnchored(pvc)) {
        /* We can't check revocation for chains without a trusted anchor. */
        return completed;
    }
    certCount--;

#if 0
    /* TODO: Implement getting this value from the client.
       Optional responder passed in though policy. */
    CFURLRef localResponder = NULL;
    /* Generate a nonce in outgoing request if true. */
	bool genNonce = false;
    /* Require a nonce in response if true. */
	bool requireRespNonce = false;
	bool cacheReadDisable = false;
	bool cacheWriteDisable = false;
#endif

    if (pvc->rvcs) {
        /* We have done revocation checking already, we're done. */
        secdebug("ocsp", "Not rechecking revocation");
        return completed;
    }

    /* Setup things so we check revocation status of all certs except the
       anchor. */
    pvc->rvcs = calloc(sizeof(struct OpaqueSecRVC), certCount);

#if 0
    /* Lookup cached revocation data for each certificate. */
	for (certIX = 0; certIX < certCount; ++certIX) {
		SecCertificateRef cert = SecPVCGetCertificateAtIndex(rvc->pvc, rvc->certIX);
        CFArrayRef ocspResponders = SecCertificateGetOCSPResponders(cert);
		if (ocspResponders) {
            /* First look though passed in ocsp responses. */
            //SecPVCGetOCSPResponseForCertificateAtIndex(pvc, ix, singleResponse);

            /* Then look though shared cache (we don't care which responder
               something came from here). */
            CFDataRef ocspResponse = SecOCSPCacheCopyMatching(SecCertIDRef certID, NULL);

            /* Now let's parse the response. */
            if (decodeOCSPResponse(ocspResp)) {
                secdebug("ocsp", "response ok: %@", ocspResp);
            } else {
                secdebug("ocsp", "response bad: %@", ocspResp);
                /* ocsp response not ok. */
                if (!SecPVCSetResultForced(pvc, key, ix, kCFBooleanFalse, true))
                    return completed;
            }
            CFReleaseSafe(ocspResp);
		} else {
            /* Check if certificate has any crl distributionPoints. */
            CFArrayRef distributionPoints = SecCertificateGetCRLDistributionPoints(cert);
            if (distributionPoints) {
                /* Look for a cached CRL and potentially delta CRL for this certificate. */
            }
        }
	}
#endif

    /* Note that if we are multi threaded and a job completes after it
       is started but before we return from this function, we don't want
       a callback to decrement asyncJobCount to zero before we finish issuing
       all the jobs. To avoid this we pretend we issued certCount async jobs,
       and decrement pvc->asyncJobCount for each cert that we don't start a
       background fetch for. */
    pvc->asyncJobCount = (unsigned int) certCount;

    /* Loop though certificates again and issue an ocsp fetch if the
       revocation status checking isn't done yet. */
    for (certIX = 0; certIX < certCount; ++certIX) {
        secdebug("ocsp", "checking revocation for cert: %ld", certIX);
        SecRVCRef rvc = &((SecRVCRef)pvc->rvcs)[certIX];
        SecRVCInit(rvc, pvc, certIX);
        if (rvc->done)
            continue;

        SecCertificateRef cert = SecPVCGetCertificateAtIndex(rvc->pvc,
            rvc->certIX);
        /* The certIX + 1 is ok here since certCount is always at least 1
           less than the actual number of certs. */
        SecCertificateRef issuer = SecPVCGetCertificateAtIndex(rvc->pvc,
            rvc->certIX + 1);

        rvc->ocspRequest = SecOCSPRequestCreate(cert, issuer);

        /* Get stapled OCSP responses */
        CFArrayRef ocspResponsesData = SecPathBuilderCopyOCSPResponses(pvc->builder);

        /* If we have any OCSP stapled responses, check those first */
        if(ocspResponsesData) {
            secdebug("ocsp", "Checking stapled responses for cert %ld", certIX);
            CFArrayForEach(ocspResponsesData, ^(const void *value) {
                /* TODO: Should the builder already have the appropriate SecOCSPResponseRef ? */
                SecOCSPResponseRef ocspResponse = SecOCSPResponseCreate(value);
                SecRVCConsumeOCSPResponse(rvc, ocspResponse, NULL_TIME, false);
            });
            CFRelease(ocspResponsesData);
        }

        /* Then check the cached response */
        secdebug("ocsp", "Checking cached responses for cert %ld", certIX);
        SecRVCConsumeOCSPResponse(rvc, SecOCSPCacheCopyMatching(rvc->ocspRequest, NULL), NULL_TIME, false);

        /* If the cert is EV or if revocation checking was explicitly enabled, attempt to fire off an
           async http request for this cert's revocation status, unless we already successfully checked
           the revocation status of this cert based on the cache or stapled responses,  */
        bool allow_fetch = SecPathBuilderCanAccessNetwork(pvc->builder) && (pvc->is_ev || pvc->check_revocation);
        bool fetch_done = true;
        if (rvc->done || !allow_fetch ||
            (fetch_done = SecRVCFetchNext(rvc))) {
            /* We got a cache hit or we aren't allowed to access the network,
               or the async http post failed. */
            SecRVCUpdatePVC(rvc);
            SecRVCDelete(rvc);
            /* We didn't really start a background job for this cert. */
            pvc->asyncJobCount--;
        } else if (!fetch_done) {
            /* We started at least one background fetch. */
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
		kSecPolicyCheckBasicContraints,
		SecPolicyCheckBasicContraints);
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
		kSecPolicyCheckIntermediateSPKISHA256,
		SecPolicyCheckIntermediateSPKISHA256);
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
}

/* AUDIT[securityd](done):
   array (ok) is a caller provided array, only its cf type has
   been checked.
   The options (ok) field ends up in policy->_options unchecked, so every access
   of policy->_options needs to be validated.
 */
static SecPolicyRef SecPolicyCreateWithArray(CFArrayRef array) {
    SecPolicyRef policy = NULL;
    require_quiet(array && CFArrayGetCount(array) == 2, errOut);
    CFStringRef oid = (CFStringRef)CFArrayGetValueAtIndex(array, 0);
    require_quiet(isString(oid), errOut);
    CFDictionaryRef options = (CFDictionaryRef)CFArrayGetValueAtIndex(array, 1);
    require_quiet(isDictionary(options), errOut);
    policy = SecPolicyCreate(oid, options);
errOut:
    return policy;
}

/* AUDIT[securityd](done):
   value (ok) is an element in a caller provided array.
 */
static void deserializePolicy(const void *value, void *context) {
    CFArrayRef policyArray = (CFArrayRef)value;
    if (isArray(policyArray)) {
        CFTypeRef deserializedPolicy = SecPolicyCreateWithArray(policyArray);
        if (deserializedPolicy) {
            CFArrayAppendValue((CFMutableArrayRef)context, deserializedPolicy);
            CFRelease(deserializedPolicy);
        }
    }
}

/* AUDIT[securityd](done):
   serializedPolicies (ok) is a caller provided array, only its cf type has
   been checked.
 */
CFArrayRef SecPolicyArrayDeserialize(CFArrayRef serializedPolicies) {
    CFMutableArrayRef result = NULL;
    require_quiet(isArray(serializedPolicies), errOut);
    CFIndex count = CFArrayGetCount(serializedPolicies);
    result = CFArrayCreateMutable(kCFAllocatorDefault, count, &kCFTypeArrayCallBacks);
    CFRange all_policies = { 0, count };
    CFArrayApplyFunction(serializedPolicies, all_policies, deserializePolicy, result);
errOut:
    return result;
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
    pvc->builder = builder;
    pvc->policies = policies;
    if (policies)
        CFRetain(policies);
    pvc->verifyTime = verifyTime;
    pvc->path = NULL;
    pvc->details = NULL;
    pvc->info = NULL;
    pvc->valid_policy_tree = NULL;
    pvc->callbacks = NULL;
    pvc->policyIX = 0;
    pvc->rvcs = NULL;
    pvc->asyncJobCount = 0;
    pvc->check_revocation = false;
    pvc->response_required = false;
    pvc->optionally_ev = false;
    pvc->is_ev = false;
    pvc->result = true;
}

static void SecPVCDeleteRVCs(SecPVCRef pvc) {
    secdebug("alloc", "%p", pvc);
    if (pvc->rvcs) {
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
}

void SecPVCSetPath(SecPVCRef pvc, SecCertificatePathRef path,
    CF_CONSUMED CFArrayRef details) {
    secdebug("policy", "%@", path);
    if (pvc->path != path) {
        /* Changing path makes us clear the Revocation Verification Contexts */
        SecPVCDeleteRVCs(pvc);
        pvc->path = path;
    }
    pvc->details = details;
    CFReleaseNull(pvc->info);
    if (pvc->valid_policy_tree) {
        policy_tree_prune(&pvc->valid_policy_tree);
    }
    pvc->policyIX = 0;
	pvc->result = true;
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

bool SecPVCIsCertificateAtIndexSelfSigned(SecPVCRef pvc, CFIndex ix) {
    return SecCertificatePathSelfSignedIndex(pvc->path) == ix;
}

void SecPVCSetCheckRevocation(SecPVCRef pvc) {
    pvc->check_revocation = true;
    secdebug("ocsp", "deferred revocation checking enabled");
}

void SecPVCSetCheckRevocationResponseRequired(SecPVCRef pvc) {
    pvc->response_required = true;
    secdebug("ocsp", "revocation response required");
}

bool SecPVCIsAnchored(SecPVCRef pvc) {
    return SecCertificatePathIsAnchored(pvc->path);
}

CFAbsoluteTime SecPVCGetVerifyTime(SecPVCRef pvc) {
    return pvc->verifyTime;
}

/* AUDIT[securityd](done):
   policy->_options is a caller provided dictionary, only its cf type has
   been checked.
 */
bool SecPVCSetResultForced(SecPVCRef pvc,
	CFStringRef key, CFIndex ix, CFTypeRef result, bool force) {

    secdebug("policy", "cert[%d]: %@ =(%s)[%s]> %@", (int) ix, key,
        (pvc->callbacks == gSecPolicyLeafCallbacks ? "leaf"
            : (pvc->callbacks == gSecPolicyPathCallbacks ? "path"
                : "custom")),
        (force ? "force" : ""), result);

    /* If this is not something the current policy cares about ignore
       this error and return true so our caller continues evaluation. */
    if (!force) {
        /* @@@ The right long term fix might be to check if none of the passed
           in policies contain this key, since not all checks are run for all
           policies. */
        SecPolicyRef policy = SecPVCGetPolicy(pvc);
        if (policy && !CFDictionaryContainsKey(policy->_options, key))
            return true;
    }

	/* @@@ Check to see if the SecTrustSettings for the certificate in question
	   tell us to ignore this error. */
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

    if (SecCertificateIsWeak(cert)) {
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

        /* (k) */
		const SecCEBasicConstraints *bc =
			SecCertificateGetBasicConstraints(cert);
        if (!bc || !bc->isCA) {
            /* Basic constraints not present or not marked as isCA, illegal. */
            if (!SecPVCSetResultForced(pvc, kSecPolicyCheckBasicContraints,
                ix, kCFBooleanFalse, true))
                goto errOut;
        }
        /* Consider adding (l) max_path_length checking here. */

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
		    bool is_anchor = (ix == SecPVCGetCertificateCount(pvc) - 1
		                      && SecPVCIsAnchored(pvc));
		    if (!is_anchor) {
		        /* Check for blacklisted intermediates keys. */
		        CFDataRef dgst = SecCertificateCopyPublicKeySHA1Digest(cert);
		        if (dgst) {
		            /* Check dgst against blacklist. */
		            if (CFSetContainsValue(blackListedKeys, dgst)) {
		                SecPVCSetResultForced(pvc, kSecPolicyCheckBlackListedKey,
		                                      ix, kCFBooleanFalse, true);
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
		    bool is_anchor = (ix == SecPVCGetCertificateCount(pvc) - 1
		                      && SecPVCIsAnchored(pvc));
		    if (!is_anchor) {
		        /* Check for gray listed intermediates keys. */
		        CFDataRef dgst = SecCertificateCopyPublicKeySHA1Digest(cert);
		        if (dgst) {
		            /* Check dgst against gray list. */
		            if (CFSetContainsValue(grayListKeys, dgst)) {
		                SecPVCSetResultForced(pvc, kSecPolicyCheckGrayListedKey,
		                                      ix, kCFBooleanFalse, true);
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
    /* Check revocation only if the chain is valid so far. The revocation will
       only fetch OCSP response over the network if the client asked for revocation
       check explicitly or is_ev is true. */
    if (pvc->result) {
        completed = SecPVCCheckRevocation(pvc);
    }

    /* Check for CT */
    if (pvc->result || pvc->details) {
        /* This call will set the value of pvc->is_ct, but won't change the result (pvc->result) */
        SecPolicyCheckCT(pvc, kSecPolicyCheckCertificateTransparency);
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
    CFAbsoluteTime enu = 0;
    if (certCount <= 1 || !pvc->rvcs) {
        return enu;
    }
    certCount--;

	for (certIX = 0; certIX < certCount; ++certIX) {
        SecRVCRef rvc = &((SecRVCRef)pvc->rvcs)[certIX];
        if (rvc->nextUpdate == 0) {
            if (certIX > 0) {
                /* We allow for CA certs to not be revocation checked if they
                   have no ocspResponders to check against, but the leaf
                   must be checked in order for us to claim we did revocation
                   checking. */
                SecCertificateRef cert =
                    SecPVCGetCertificateAtIndex(rvc->pvc, rvc->certIX);
                CFArrayRef ocspResponders = SecCertificateGetOCSPResponders(cert);
                if (!ocspResponders || CFArrayGetCount(ocspResponders) == 0) {
                    /* We can't check this cert so we don't consider it a soft
                       failure that we didn't. Ideally we should support crl
                       checking and remove this workaround, since that more
                       strict. */
                    continue;
                }
            }
            secdebug("ocsp", "revocation checking soft failure for cert: %ld",
                certIX);
            enu = rvc->nextUpdate;
            break;
        }
        if (enu == 0 || rvc->nextUpdate < enu) {
            enu = rvc->nextUpdate;
        }
#if 0
        /* Perhaps we don't want to do this since some policies might
           ignore the certificate expiration but still use revocation
           checking. */

        /* Earliest certificate expiration date. */
		SecCertificateRef cert = SecPVCGetCertificateAtIndex(rvc->pvc, rvc->certIX);
        CFAbsoluteTime nva = SecCertificateNotValidAfter(cert);
        if (nva && (enu == 0 || nva < enu)
            enu = nva;
#endif
    }

    secdebug("ocsp", "revocation valid until: %lg", enu);
    return enu;
}
