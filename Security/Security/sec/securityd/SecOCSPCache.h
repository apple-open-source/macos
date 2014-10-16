/*
 * Copyright (c) 2009-2010,2012-2014 Apple Inc. All Rights Reserved.
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
 */

/*!
    @header SecOCSPCache
    The functions provided in SecOCSPCache.h provide an interface to
    an OCSP caching module.
*/

#ifndef _SECURITY_SECOCSPCACHE_H_
#define _SECURITY_SECOCSPCACHE_H_

#include <securityd/SecOCSPRequest.h>
#include <securityd/SecOCSPResponse.h>
#include <CoreFoundation/CFURL.h>

__BEGIN_DECLS


void SecOCSPCacheAddResponse(SecOCSPResponseRef response,
    CFURLRef localResponderURI);

SecOCSPResponseRef SecOCSPCacheCopyMatching(SecOCSPRequestRef request,
    CFURLRef localResponderURI /* may be NULL */);

/* This should be called on a normal non emergency exit. */
void SecOCSPCacheGC(void);

/* Call this periodically or perhaps when we are exiting due to low memory. */
void SecOCSPCacheFlush(void);

__END_DECLS

#endif /* _SECURITY_SECOCSPCACHE_H_ */

#if 0
/*
Experation policy assumptions:
- We never check revocation status of anchors, whether they be system anchors,
  passed in anchors or anchors hardcoded in a policy.
- Revocation information is cached for positive reponses for a limited time.
- Revocation information can be cached for negative reponses for an unlimited time.
- Revocation information need never be kept around after the certificate has expired (unless we still check after the cert has expired like we were talking about for EERI).
- Revocation information records that are used and still valid should be kept longer.
- We can set an upper limit in number of records (or certificates) in the cache.
- We can set an upper limit on total space consumed by the cache.
Questions:
- Remember bad server responses too?  some ocsp responders required signed requests which we don't support, so we could consider caching the 6 (Not Authorized or something) response.

Data needed per type of revocation record to implement this policy.

Caching policy:
- Deleting cache should not be user option.
- Cache should surrvive backups.
- Negative caching as long as possible.

CRL certificate stati:
unspecified, keyCompromise, cACompromise,
affiliationChanged, superseded, cessationOfOperation,
certificateHold, removeFromCRL, privilegeWithdrawn,
aACompromise, the special value UNREVOKED, or the special
value UNDETERMINED.  This variable is initialized to the
special value UNREVOKED.

CRL Timestamp values:
- thisUpdate
- nextUpdate (optional but not really 5280 says CAs must provide it even though ASN.1 is optional)
(no producedAt in CRLs, that's what thisUpdate is by definition it seems).


OCSP Timestamp values:
      thisUpdate = May 1, 2005  01:00:00 GMT
      nextUpdate = May 3, 2005 01:00:00 GMT (optional abscence means update available any time)
      productedAt = May 1, 2005 01:00:00 GMT

PER CERTIFICATE RETURN in INFO

Revocation object used: OCSP Response, mapping from
reasons-> (CRL + most current delta CRL), Error Object (with status code).
   -- good
   -- revoked
   -- unknown

other exceptions (unsigned responses):
   -- malformedRequest
   -- internalError
   -- tryLater
   -- sigRequired
   -- unauthorized (5019 The response "unauthorized" is returned in cases where the client
      is not authorized to make this query to this server or the server
      is not capable of responding authoritatively.  (Expired certs might get this answer too))


CRL signer chain rules:
1) Must use same anchor as cert itself.
This implies that we can only cache the validity of a leaf or intermediate certificate for CRL checking based on the mapping:
(certificate, path anchor, use_deltas) -> Revocation_status (unspecified, keyCompromise, cACompromise,
affiliationChanged, superseded, cessationOfOperation,certificateHold, removeFromCRL, privilegeWithdrawn,aACompromise, UNREVOKED, UNDETERMINED).

OCSP signer chain rules:
(Wikipedia confirmed in rfc): The key that signs a response need not be the same key that signed the certificate. The certificate's issuer may delegate another authority to be the OCSP responder. In this case, the responder's certificate (the one that is used to sign the response) must be issued by the issuer of the certificate in question, and must include a certain extension that marks it as an OCSP signing authority (more precisely, an extended key usage extension with the OID {iso(1) identified-organization(3) dod(6) internet(1) security(5) mechanisms(5) pkix(7) keyPurpose(3) ocspSigning(9)})

rfc text of the wikipedia: Therefore, a certificate's issuer MUST either sign the OCSP
   responses itself or it MUST explicitly designate this authority to
   another entity.  OCSP signing delegation SHALL be designated by the
   inclusion of id-kp-OCSPSigning in an extendedKeyUsage certificate
   extension included in the OCSP response signer's certificate.  This
   certificate MUST be issued directly by the CA that issued the
   certificate in question.

rfc: If ocsp signing cert has id-pkix-ocsp-nocheck extension we don't check it's revocation status.

(certificate, direct issuer certificate) -> Revocation_status good (UNREVOKED) revoked revocationTime, CRLReason (unspecified, keyCompromise, cACompromise,affiliationChanged, superseded, cessationOfOperation,certificateHold, removeFromCRL, privilegeWithdrawn,aACompromise) unknown (UNDETERMINED).

ocsp CertID ::= SEQUENCE {
       hashAlgorithm       AlgorithmIdentifier,
       issuerNameHash      OCTET STRING, -- Hash of Issuer's DN
       issuerKeyHash       OCTET STRING, -- Hash of Issuers public key
       serialNumber        CertificateSerialNumber }
)

In order to accomadate the responder using a different hashAlgorithm than we used in the request we need to recalc these from the cert itself.

If all we have is a list of ocspresponses without knowing where they came from, we have to calculate the hashes of our issuerName and issuerKey for each hashAlgorithm we have cached ocsp responses for (optionally after limiting our candidates to those with matching serialNumbers first).

SELECT from ocsp_cache hashAlgorithm WHERE serialNumber = <SERIAL>

for hix = 0 hix < hashAlgorithms.count
  ALG(hix).id = hashAlgorithms(hix)

SELECT from ocsp_cache response WHERE serialNumber = <SERIAL> hashAlgorithm = ALG(hix).id issuerNameHash = ALG(hix).hash(issuer) issuerKeyHash = ALG(hix).hash(key)






Notes for Matt:
- ttl in amfi cache (to force recheck when ocsp response is invalid)?
- Periodic check before launch to remove in band waiting for ocsp response?

Notes on Nonces in ocsp request and responses.  Only ask for nonce if we think server supports it (no way to know today). Fall back on time based validity checking if reponse has no nonce, even if we asked for one

Note on CRL checking and experation and retries of OCSP checking.
Clients MAY attempt to retrieve the CRL if no
   OCSPResponse is received from the responder after a locally
   configured timeout and number of retries..



CRL/OCSP cache design idea:

revocation status table:

rowid certhash issuer-rowid lastUsed thisUpdate producedAt nextUpdate revocationTime revocationStatus

cacheAddOCSP(path, index_of_cert_resp_is_for, ocspResp)
cacheAddCRLStatus(path, index_of_cert_in_path, nextUpdate, revocationTime, revocationStatus)
(revocationTime, revocationStatus) = cacheLookupStatus(path, ix)

Return a list of parent certificate hashes for the current leaf.  If a result is returned, we have a candiate path leading up to an anchor, for which we already trust the signature in the chain and revocation information has been checked.

CFArrayRef cacheSuggestParentsHashesFor(cert)

for crl based status root must match root of path.  For ocsp status issuer must match issuer of leaf in path

presence in the cache means cert chain leading to an anchor is valid, and signed properly and trusted by the ocsp or crl policy, revocation status for cert is valid until the time indicated by nextUpdate.  Cert chain itself may or may not be valid but that's checked by the policy engine.

If a chain isn't properly signed or fails to satisfy the crl policy, it should not be in the cache.

ocsp cache

rowid ocspResponse (responder) lastUsed nextUpdate

hashAlgorithm->(issuerNameHash,issuerKeyHash,serialNumber)->response


crl cache ()

crlDistributionPoint (reasons) crl thisUpdate nextUpdate isDelta


crlEntry cache table
(certHash anchorHash) crlIssuer revocationStatus revocationTime expires lastUsed
crlTable
(crlIssuer anchorHash distributionPointURL?) crl sigVerified expires
ocspEntry cache table
(certHash parentHash ocspReponderID) hashAlg revocationStatus revocationTime expires lastUsed
ocspTable
((hashAlg, pubKeyHash, issuerHash, serialNum) anchorHash) ocspResponse sigVerified expires

or
cert cache table
(certHash parentHash anchorHash) crlEntryID ocspID

crlEntry cache table
(crlEntryID anchorHash) crlIssuer revocationStatus revocationTime

crlIssuerTable
(crlIssuer anchorHash) crl sigVerified

ocsp table
(ocspID) ocspResponse


but so does caching the raw response as a link to a blob table containing crls
and ocsp-responses
But also cache the revocationStatus for a (cert,parent) or (cert,anchor) via
a link to a cached ocspResponse or revocationStatus and revocationTime entry from crl
*/

#endif
