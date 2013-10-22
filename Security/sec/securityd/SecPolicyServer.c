/*
 * Copyright (c) 2008-2012 Apple Inc. All Rights Reserved.
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
#include <CoreFoundation/CFTimeZone.h>
#include <wctype.h>
#include <libDER/oids.h>
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
 */
static bool SecDNSMatch(CFStringRef hostname, CFStringRef servername) {
	CFStringInlineBuffer hbuf, sbuf;
	CFIndex hix, six,
		hlength = CFStringGetLength(hostname),
		slength = CFStringGetLength(servername);
	CFRange hrange = { 0, hlength }, srange = { 0, slength };
	CFStringInitInlineBuffer(hostname, &hbuf, hrange);
	CFStringInitInlineBuffer(servername, &sbuf, srange);

	for (hix = six = 0; six < slength; ++six) {
		UniChar hch, sch = CFStringGetCharacterFromInlineBuffer(&sbuf, six);
		if (sch == '*') {
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
			do {
				/* Since we're not at the end of servername yet (that case
				   was handeled above), running out of chars in hostname
				   means we don't have a match. */
				if (hix >= hlength)
					return false;
				hch = CFStringGetCharacterFromInlineBuffer(&hbuf, hix++);
			} while (hch != '.');
		} else {
			/* We're looking at a non wildcard character in the servername.
			   If we reached the end of hostname it's not a match. */
			if (hix >= hlength)
				return false;

			/* Otherwise make sure the hostname matches the character in the
			   servername, case insensitively. */
			hch = CFStringGetCharacterFromInlineBuffer(&hbuf, hix++);
			if (towlower(hch) != towlower(sch))
				return false;
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
        CFDataRef serial = SecCertificateCopySerialNumber(cert);
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
#define POLICY_SUBTREES 0

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
        /* Add a detail for the root not being trusted. */
        if (SecPVCSetResultForced(pvc, kSecPolicyCheckAnchorTrusted,
            n - 1, kCFBooleanFalse, true))
            return;
    }

    CFAbsoluteTime verify_time = SecPVCGetVerifyTime(pvc);
    //policy_set_t user_initial_policy_set = NULL;
    //trust_anchor_t anchor;
    bool initial_policy_mapping_inhibit = false;
    bool initial_explicit_policy = false;
    bool initial_any_policy_inhibit = false;
#if POLICY_SUBTREES
    root_name_t initial_permitted_subtrees = NULL;
    root_name_t initial_excluded_subtrees = NULL;
#endif

    /* Initialization */
    pvc->valid_policy_tree = policy_tree_create(&oidAnyPolicy, NULL);
#if POLICY_SUBTREES
    root_name_t permitted_subtrees = initial_permitted_subtrees;
    root_name_t excluded_subtrees = initial_excluded_subtrees;
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
            /* Verify that the subject name is within one of the permitted_subtrees for X.500 distinguished names, and verify that each of the alternative names in the subjectAltName extension (critical or non-critical) is within one of the permitted_subtrees for that name type. */
            /* Verify that the subject name is not within any of the excluded_subtrees for X.500 distinguished names, and verify that each of the alternative names in the subjectAltName extension (critical or non-critical) is not within any of the excluded_subtrees for that name type. */
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
        /* (g) If a name constraints extension is included in the certificate, modify the permitted_subtrees and excluded_subtrees state variables as follows:
 */
        /* @@@ handle name constraints. */
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
    }
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

static void SecPolicyCheckNoNetworkAccess(SecPVCRef pvc,
	CFStringRef key) {
    SecPathBuilderSetCanAccessNetwork(pvc->builder, false);
}

// MARK: -
// MARK: SecRVCRef
/********************************************************
 ****************** SecRVCRef Functions *****************
 ********************************************************/

/* Revocation verification context. */
struct OpaqueSecRVC {
    /* Will contain the response data. */
    asynchttp_t http;

    /* Pointer to the pvc for this revocation check. */
    SecPVCRef pvc;

    /* The ocsp request we send to each responder. */
    SecOCSPRequestRef ocspRequest;

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

        if (!asyncHttpPost(rvc->responder, request, &rvc->http)) {
            /* Async request was posted, wait for reply. */
            return false;
        }
    }

errOut:
    rvc->done = true;
    return true;
}

/* Proccess a verified ocsp response for a given cert. Return true if the
   certificate status was obtained. */
static bool SecOCSPSingleResponseProccess(SecOCSPSingleResponseRef this,
    SecRVCRef rvc) {
    bool proccessed;
	switch (this->certStatus) {
    case CS_Good:
        secdebug("ocsp", "CS_Good for cert %" PRIdCFIndex, rvc->certIX);
        /* @@@ Mark cert as valid until a given date (nextUpdate if we have one)
           in the info dictionary. */
        //cert.revokeCheckGood(true);
        rvc->nextUpdate = this->nextUpdate;
        proccessed = true;
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
        CFRelease(cfreason);
        proccessed = true;
        break;
    case CS_Unknown:
        /* not an error, no per-cert status, nothing here */
        secdebug("ocsp", "CS_Unknown for cert %" PRIdCFIndex, rvc->certIX);
        proccessed = false;
        break;
    default:
        secdebug("ocsp", "BAD certStatus (%d) for cert %" PRIdCFIndex,
            (int)this->certStatus, rvc->certIX);
        proccessed = false;
        break;
	}

	return proccessed;
}

static bool SecOCSPResponseVerify(SecOCSPResponseRef ocspResponse, SecRVCRef rvc) {
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
            CFAbsoluteTime verifyTime = SecOCSPResponseVerifyTime(ocspResponse);
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

/* Callback from async http code after an ocsp response has been received. */
static void SecOCSPFetchCompleted(asynchttp_t *http, CFTimeInterval maxAge) {
    SecRVCRef rvc = (SecRVCRef)http->info;
    SecPVCRef pvc = rvc->pvc;
    SecOCSPResponseRef ocspResponse = NULL;
    if (http->response) {
        CFDataRef data = CFHTTPMessageCopyBody(http->response);
        if (data) {
            /* Parse the returned data as if it's an ocspResponse. */
            ocspResponse = SecOCSPResponseCreate(data, maxAge);
            CFRelease(data);
        }
    }

    if (ocspResponse) {
        SecOCSPResponseStatus orStatus = SecOCSPGetResponseStatus(ocspResponse);
        if (orStatus == kSecOCSPSuccess) {
            SecOCSPSingleResponseRef sr =
                SecOCSPResponseCopySingleResponse(ocspResponse, rvc->ocspRequest);
            if (!sr) {
                /* The ocsp response didn't have a singleResponse for the cert
                   we are looking for, let's try the next responder. */
                secdebug("ocsp",
                    "ocsp responder: %@ did not include status of requested cert",
                    rvc->responder);
            } else {
                /* We got a singleResponse for the cert we are interested in,
                   let's proccess it. */
                /* @@@ If the responder doesn't have the ocsp-nocheck extension
                   we should check whether the leaf was revoked (we are
                   already checking the rest of the chain). */
                /* Check the OCSP response signature and verify the
                   response. */
                if (SecOCSPResponseVerify(ocspResponse, rvc)) {
                    secdebug("ocsp","responder: %@ sent proper response",
                        rvc->responder);

                    if (SecOCSPSingleResponseProccess(sr, rvc)) {
                        if (rvc->nextUpdate == 0) {
                            rvc->nextUpdate =
                                SecOCSPResponseGetExpirationTime(ocspResponse);
                        }
                        /* If the singleResponse had meaningful information, we
                           cache the response. */
                        SecOCSPCacheAddResponse(ocspResponse, rvc->responder);
                        rvc->done = true;
                    }
                }
                SecOCSPSingleResponseDestroy(sr);
            }
        } else {
            /* ocsp response not ok.  Let's try next responder. */
            secdebug("ocsp", "responder: %@ returned status: %d",
                rvc->responder, orStatus);
#if 0
            if (!SecPVCSetResultForced(pvc, kSecPolicyCheckRevocation,
                rvc->certIX, kCFBooleanFalse, true))
                return;
#endif
        }
        SecOCSPResponseFinalize(ocspResponse);
    }

    if (!rvc->done) {
        /* Clear the data for the next response. */
        asynchttp_free(http);
        SecRVCFetchNext(rvc);
    }

    if (rvc->done) {
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
    rvc->http.completed = SecOCSPFetchCompleted;
    rvc->http.info = rvc;
    rvc->ocspRequest = NULL;
    rvc->responderIX = 0;
    rvc->responder = NULL;
    rvc->nextUpdate = 0;
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
    /* @@@ Implement getting this value from the client.
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
        SecOCSPResponseRef ocspResponse;
        ocspResponse = SecOCSPCacheCopyMatching(rvc->ocspRequest, NULL);
        if (ocspResponse) {
            SecOCSPSingleResponseRef sr =
                SecOCSPResponseCopySingleResponse(ocspResponse, rvc->ocspRequest);
            if (!sr) {
                /* The cached ocsp response didn't have a singleResponse for
                   the cert we are looking for, it's shouldn't be in the cache. */
                secdebug("ocsp", "cached ocsp response did not include status"
                    " of requested cert");
            } else {
                /* We got a singleResponse for the cert we are interested in,
                   let's proccess it. */

                /* @@@ If the responder doesn't have the ocsp-nocheck extension
                   we should check whether the leaf was revoked (we are
                   already checking the rest of the chain). */
                /* Recheck the OCSP response signature and verify the
                   response. */
                if (SecOCSPResponseVerify(ocspResponse, rvc)) {
                    secdebug("ocsp","cached response still has valid signature");

                    if (SecOCSPSingleResponseProccess(sr, rvc)) {
                        CFAbsoluteTime expTime =
                            SecOCSPResponseGetExpirationTime(ocspResponse);
                        if (rvc->nextUpdate == 0 || expTime < rvc->nextUpdate)
                            rvc->nextUpdate = expTime;
                        rvc->done = true;
                    }
                }
                SecOCSPSingleResponseDestroy(sr);
            }
            SecOCSPResponseFinalize(ocspResponse);
        }

        /* Unless we succefully checked the revocation status of this cert
           based on the cache, Attempt to fire off an async http request
           for this certs revocation status. */
        bool fetch_done = true;
        if (rvc->done || !SecPathBuilderCanAccessNetwork(pvc->builder) ||
            (fetch_done = SecRVCFetchNext(rvc))) {
            /* We got a cache hit or we aren't allowed to access the network,
               or the async http post failed. */
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
		kSecPolicyCheckCriticalExtensions, SecPolicyCheckCriticalExtensions);
	CFDictionaryAddValue(gSecPolicyPathCallbacks,
		kSecPolicyCheckIdLinkage, SecPolicyCheckIdLinkage);
	CFDictionaryAddValue(gSecPolicyLeafCallbacks,
		kSecPolicyCheckKeyUsage, SecPolicyCheckKeyUsage);
	CFDictionaryAddValue(gSecPolicyLeafCallbacks,
		kSecPolicyCheckExtendedKeyUsage, SecPolicyCheckExtendedKeyUsage);
	CFDictionaryAddValue(gSecPolicyLeafCallbacks,
		kSecPolicyCheckBasicContraints, SecPolicyCheckBasicContraints);
	CFDictionaryAddValue(gSecPolicyLeafCallbacks,
		kSecPolicyCheckNonEmptySubject, SecPolicyCheckNonEmptySubject);
	CFDictionaryAddValue(gSecPolicyLeafCallbacks,
		kSecPolicyCheckQualifiedCertStatements,
		SecPolicyCheckQualifiedCertStatements);
	CFDictionaryAddValue(gSecPolicyLeafCallbacks,
		kSecPolicyCheckSSLHostname, SecPolicyCheckSSLHostname);
	CFDictionaryAddValue(gSecPolicyLeafCallbacks,
        kSecPolicyCheckEmail, SecPolicyCheckEmail);
	CFDictionaryAddValue(gSecPolicyPathCallbacks,
		kSecPolicyCheckValidIntermediates, SecPolicyCheckValidIntermediates);
	CFDictionaryAddValue(gSecPolicyLeafCallbacks,
		kSecPolicyCheckValidLeaf, SecPolicyCheckValidLeaf);
	CFDictionaryAddValue(gSecPolicyPathCallbacks,
		kSecPolicyCheckValidRoot, SecPolicyCheckValidRoot);
	CFDictionaryAddValue(gSecPolicyPathCallbacks,
		kSecPolicyCheckIssuerCommonName, SecPolicyCheckIssuerCommonName);
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
		kSecPolicyCheckChainLength, SecPolicyCheckChainLength);
	CFDictionaryAddValue(gSecPolicyPathCallbacks,
		kSecPolicyCheckAnchorSHA1, SecPolicyCheckAnchorSHA1);
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
    CFArrayRef details) {
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
            /* Non standard valdation phase, nothing is optional. */
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
    /* Check revocation only if the chain is valid so far.  Then only check
       revocation if the client asked for it explicitly or is_ev is
       true. */
    if (pvc->result && (pvc->is_ev || pvc->check_revocation)) {
        completed = SecPVCCheckRevocation(pvc);
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
           ignore the certificate experation but still use revocation
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
