/*
 * Copyright (c) 2002-2012 Apple Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


/*
 * tpCrlVerify.cpp - routines to verify CRLs and to verify certs against CRLs.
 */

#include "tpCrlVerify.h"
#include "TPCertInfo.h"
#include "TPCrlInfo.h"
#include "tpOcspVerify.h"
#include "tpdebugging.h"
#include "TPNetwork.h"
#include "TPDatabase.h"
#include <CommonCrypto/CommonDigest.h>
#include <Security/oidscert.h>
#include <security_ocspd/ocspdClient.h>
#include <security_utilities/globalizer.h>
#include <security_utilities/threading.h>
#include <security_cdsa_utilities/cssmerrors.h>
#include <sys/stat.h>

/* general purpose, switch to policy-specific code based on TPVerifyContext.policy */
CSSM_RETURN tpRevocationPolicyVerify(
	TPVerifyContext	&tpVerifyContext,
	TPCertGroup 	&certGroup)
{
	switch(tpVerifyContext.policy) {
		case kRevokeNone:
			return CSSM_OK;
		case kRevokeCrlBasic:
			return tpVerifyCertGroupWithCrls(tpVerifyContext, certGroup);
		case kRevokeOcsp:
			return tpVerifyCertGroupWithOCSP(tpVerifyContext, certGroup);
		default:
			assert(0);
			return CSSMERR_TP_INTERNAL_ERROR;
	}
}

/*
 * For now, a process-wide memory resident CRL cache. 
 * We are responsible for deleting the CRLs which get added to this
 * cache. Currently the only time we add a CRL to this cache is
 * when we fetch one from the net. We ref count CRLs in this cache
 * to allow multi-threaded access.
 * Entries do not persist past the tpVerifyCertGroupWithCrls() in
 * which they were created unless another thread in the same 
 * process snags a refcount (also from tpVerifyCertGroupWithCrls()). 
 * I.e. when cert verification is complete the cache will be empty. 
 * This is a change from Tiger and previous. CRLs get pretty big, 
 * up to a megabyte or so, and it's just not worth it to keep those 
 * around in memory. (OCSP responses, which are much smaller than 
 * CRLs, are indeed cached in memory. See tpOcspCache.cpp.)
 */
class TPCRLCache : private TPCrlGroup
{
public:
	TPCRLCache();
	~TPCRLCache() { }
	TPCrlInfo *search(
		TPCertInfo 			&cert,
		TPVerifyContext		&vfyCtx);
	void add(
		TPCrlInfo 			&crl);
	void remove(
		TPCrlInfo 			&crl);
	void release(
		TPCrlInfo			&crl);
		
private:
	/* Protects ref count of all members of the cache */
	Mutex 				mLock;
};

TPCRLCache::TPCRLCache()
	: TPCrlGroup(Allocator::standard(), TGO_Group)
{
	
}

TPCrlInfo *TPCRLCache::search(
	TPCertInfo 			&cert,
	TPVerifyContext		&vfyCtx)
{
	StLock<Mutex> _(mLock);
	TPCrlInfo *crl = findCrlForCert(cert);
	if(crl) {
		/* reevaluate validity */
		crl->calculateCurrent(vfyCtx.verifyTime);
		crl->mRefCount++;
		tpCrlDebug("TPCRLCache hit");
	}
	else {
		tpCrlDebug("TPCRLCache miss");
	}
	return crl;
}

/* bumps ref count - caller is going to be using the CRL */
void TPCRLCache::add(
	TPCrlInfo 			&crl)
{
	StLock<Mutex> _(mLock);
	tpCrlDebug("TPCRLCache add");
	crl.mRefCount++;
	appendCrl(crl);
}

/* delete and remove from cache if refCount zero */
void TPCRLCache::release(
	TPCrlInfo 			&crl)
{
	StLock<Mutex> _(mLock);
	assert(crl.mRefCount > 0);
	crl.mRefCount--;
	if(crl.mRefCount == 0) {
		tpCrlDebug("TPCRLCache release; deleting");
		removeCrl(crl);
		delete &crl;
	}
	else {
		tpCrlDebug("TPCRLCache release; in use");
	}
}

static ModuleNexus<TPCRLCache> tpGlobalCrlCache;

/*
 * Find CRL for specified cert. Only returns a fully verified CRL. 
 * Cert-specific errors such as CSSMERR_APPLETP_CRL_NOT_FOUND will be added
 * to cert's return codes. 
 */
static CSSM_RETURN tpFindCrlForCert(
	TPCertInfo						&subject,
	TPCrlInfo						*&foundCrl,		// RETURNED
	TPVerifyContext					&vfyCtx)
{
	
	tpCrlDebug("tpFindCrlForCert top");
	TPCrlInfo *crl = NULL;
	foundCrl = NULL;
	CSSM_APPLE_TP_CRL_OPT_FLAGS crlOptFlags = 0;
	
	if(vfyCtx.crlOpts) {
		crlOptFlags = vfyCtx.crlOpts->CrlFlags;
	}
	
	/* Search inputCrls for a CRL for subject cert */
	if(vfyCtx.inputCrls != NULL) {
		crl = vfyCtx.inputCrls->findCrlForCert(subject);
		if(crl && (crl->verifyWithContextNow(vfyCtx, &subject) == CSSM_OK)) {
			foundCrl = crl;
			crl->mFromWhere = CFW_InGroup;
			tpCrlDebug("   ...CRL found in CrlGroup");
			return CSSM_OK;
		}
	}

	/* local process-wide cache */
	crl = tpGlobalCrlCache().search(subject, vfyCtx);
	if(crl) {
		tpCrlDebug("...tpFindCrlForCert found CRL in cache, calling verifyWithContext");
		if(crl->verifyWithContextNow(vfyCtx, &subject) == CSSM_OK) {
			foundCrl = crl;
			crl->mFromWhere = CFW_LocalCache;
			tpCrlDebug("   ...CRL found in local cache");
			return CSSM_OK;
		}
		else {
			tpGlobalCrlCache().release(*crl);
		}
	}
	
	/* 
	 * Try DL/DB.
	 * Note tpDbFindIssuerCrl() returns a verified CRL.
	 */
	crl = tpDbFindIssuerCrl(vfyCtx, *subject.issuerName(), subject);
	if(crl) {
		foundCrl = crl;
		crl->mFromWhere = CFW_DlDb;
		tpCrlDebug("   ...CRL found in DlDb");
		return CSSM_OK;
	}
	
	/* Last resort: try net if enabled */
	CSSM_RETURN crtn = CSSMERR_APPLETP_CRL_NOT_FOUND;
	crl = NULL;
	if(crlOptFlags & CSSM_TP_ACTION_FETCH_CRL_FROM_NET) {
		crtn = tpFetchCrlFromNet(subject, vfyCtx, crl);
	}
	
	if(crtn) {
		tpCrlDebug("   ...tpFindCrlForCert: CRL not found");
		if(subject.addStatusCode(crtn)) {
			return crtn;
		}
		else {
			return CSSM_OK;
		}
	}
	
	/* got one from net - add to global cache */
	assert(crl != NULL);
	tpGlobalCrlCache().add(*crl);
	crl->mFromWhere = CFW_Net;
	tpCrlDebug("   ...CRL found from net");
	
	foundCrl = crl;
	return CSSM_OK;
}

/* 
 * Dispose of a CRL obtained from tpFindCrlForCert().
 */
static void tpDisposeCrl(
	TPCrlInfo			&crl,
	TPVerifyContext		&vfyCtx)
{
	switch(crl.mFromWhere) {
		case CFW_Nowhere:
		default:
			assert(0);
			CssmError::throwMe(CSSMERR_TP_INTERNAL_ERROR);
		case CFW_InGroup:
			/* nothing to do, handled by TPCrlGroup */
			return;
		case CFW_DlDb:
			/* cooked up specially for this call */
			delete &crl;
			return;
		case CFW_LocalCache:		// cache hit 
		case CFW_Net:				// fetched from net & added to cache
			tpGlobalCrlCache().release(crl);
			return;
		/* probably others */
	}
}

/* 
 * Does this cert have a CrlDistributionPoints extension? We don't parse it, we
 * just tell the caller whether or not it has one.
 */
static bool tpCertHasCrlDistPt(
	TPCertInfo &cert)
{
	CSSM_DATA_PTR fieldValue;		
	CSSM_RETURN crtn = cert.fetchField(&CSSMOID_CrlDistributionPoints, &fieldValue);
	if(crtn) {
		return false;
	}
	else {
		cert.freeField(&CSSMOID_CrlDistributionPoints,	fieldValue);
		return true;
	}
}

/*
 * Get current CRL status for a certificate and its issuers.
 *
 * Possible results:
 *
 * CSSM_OK (we have a valid CRL; certificate is not revoked)
 * CSSMERR_TP_CERT_REVOKED (we have a valid CRL; certificate is revoked)
 * CSSMERR_APPLETP_NETWORK_FAILURE (CRL not available, download in progress)
 * CSSMERR_APPLETP_CRL_NOT_FOUND (CRL not available, and not being fetched)
 * CSSMERR_TP_INTERNAL_ERROR (unexpected error)
 *
 * Note that ocspdCRLStatus does NOT wait for the CRL to be downloaded before
 * returning, nor does it initiate a CRL download.
 */
static
CSSM_RETURN tpGetCrlStatusForCert(
	TPCertInfo						&subject,
	const CSSM_DATA					&issuers)
{
	CSSM_DATA *serialNumber=NULL;
	CSSM_RETURN crtn = subject.fetchField(&CSSMOID_X509V1SerialNumber, &serialNumber);
	if(crtn || !serialNumber) {
		return CSSMERR_TP_INTERNAL_ERROR;
	}
	crtn = ocspdCRLStatus(*serialNumber, issuers, subject.issuerName(), NULL);
	subject.freeField(&CSSMOID_X509V1SerialNumber, serialNumber);
	return crtn;
}

/*
 * Perform CRL verification on a cert group.
 * The cert group has already passed basic issuer/subject and signature
 * verification. The status of the incoming CRLs is completely unknown. 
 * 
 * FIXME - No mechanism to get CRLs from net with non-NULL verifyTime.
 * How are we supposed to get the CRL which was valid at a specified 
 * time in the past?
 */
CSSM_RETURN tpVerifyCertGroupWithCrls(
	TPVerifyContext					&vfyCtx,
	TPCertGroup 					&certGroup)		// to be verified 
{
	CSSM_RETURN 	crtn;
	CSSM_RETURN		ourRtn = CSSM_OK;

	assert(vfyCtx.clHand != 0);
	assert(vfyCtx.policy == kRevokeCrlBasic);
	tpCrlDebug("tpVerifyCertGroupWithCrls numCerts %u", certGroup.numCerts());
	CSSM_DATA issuers = { 0, NULL };
	CSSM_APPLE_TP_CRL_OPT_FLAGS optFlags = 0;
	if(vfyCtx.crlOpts != NULL) {
		optFlags = vfyCtx.crlOpts->CrlFlags;
	}
	
	/* found & verified CRLs we need to release */
	TPCrlGroup foundCrls(vfyCtx.alloc, TGO_Caller);
	
	try {
		
		unsigned certDex;
		TPCrlInfo *crl = NULL;
		
		/* get issuers as PEM-encoded data blob; we need to release */
		certGroup.encodeIssuers(issuers);

		/* main loop, verify each cert */
		for(certDex=0; certDex<certGroup.numCerts(); certDex++) {
			TPCertInfo *cert = certGroup.certAtIndex(certDex);

			tpCrlDebug("...verifying %s cert %u", 
				cert->isAnchor() ? "anchor " : "", cert->index());
			if(cert->isSelfSigned() || cert->trustSettingsFound()) {
				/* CRL meaningless for a root or trusted cert */
				continue;
			}
			if(cert->revokeCheckComplete()) {
				/* Another revocation policy claimed that this cert is good to go */
				tpCrlDebug("   ...cert at index %u revokeCheckComplete; skipping", 
					cert->index());
				continue;
			}
			crl = NULL;
			do {
				/* first, see if we have CRL status available for this cert */
				crtn = tpGetCrlStatusForCert(*cert, issuers);
				tpCrlDebug("...tpGetCrlStatusForCert: %u", crtn);
				if(crtn == CSSM_OK) {
					tpCrlDebug("tpVerifyCertGroupWithCrls: cert %u verified by local .crl\n",
								cert->index());
					cert->revokeCheckGood(true);
					if(optFlags & CSSM_TP_ACTION_CRL_SUFFICIENT) {
						/* no more revocation checking necessary for this cert */
						cert->revokeCheckComplete(true);
					}
					break;
				}
				if(crtn == CSSMERR_TP_CERT_REVOKED) {
					tpCrlDebug("tpVerifyCertGroupWithCrls: cert %u revoked in local .crl\n",
								cert->index());
					cert->addStatusCode(crtn);
					break;
				}
				if(crtn == CSSMERR_APPLETP_NETWORK_FAILURE) {
					/* crl is being fetched from net, but we don't have it yet */
					if((optFlags & CSSM_TP_ACTION_REQUIRE_CRL_IF_PRESENT) &&
								tpCertHasCrlDistPt(*cert)) {
						/* crl is required; we don't have it yet, so we fail */
						tpCrlDebug("   ...cert %u: REQUIRE_CRL_IF_PRESENT abort",
								cert->index());
						break;
					}
					/* "Best Attempt" case, so give the cert a pass for now */
					tpCrlDebug("   ...cert %u: no CRL; tolerating", cert->index());
					crtn = CSSM_OK;
					break;
				}
				/* all other CRL status results: try to fetch the CRL */

				/* find a CRL for this cert by hook or crook */
				crtn = tpFindCrlForCert(*cert, crl, vfyCtx);
				if(crtn) {
					/* tpFindCrlForCert may have simply caused ocspd to start
					 * downloading a CRL asynchronously; depending on the speed
					 * of the network and the CRL size, this may return 0 bytes
					 * of data with a CSSMERR_APPLETP_NETWORK_FAILURE result.
					 * We won't know the actual revocation result until the
					 * next time we call tpGetCrlStatusForCert after the full
					 * CRL has been downloaded successfully.
					 */
					if(optFlags & CSSM_TP_ACTION_REQUIRE_CRL_PER_CERT) {
						tpCrlDebug("   ...cert %u: REQUIRE_CRL_PER_CERT abort",
								cert->index());
						break;
					}
					if((optFlags & CSSM_TP_ACTION_REQUIRE_CRL_IF_PRESENT) && 
								tpCertHasCrlDistPt(*cert)) {
						tpCrlDebug("   ...cert %u: REQUIRE_CRL_IF_PRESENT abort",
								cert->index());
						break;
					}
					/* 
					 * This is the only place where "Best Attempt" tolerates an error
					 */
					tpCrlDebug("   ...cert %u: no CRL; tolerating", cert->index());
					crtn = CSSM_OK;
					assert(crl == NULL);
					break;
				}
				
				/* Keep track; we'll release all when done. */
				assert(crl != NULL);
				foundCrls.appendCrl(*crl);
				
				/* revoked? */
				crtn = crl->isCertRevoked(*cert, vfyCtx.verifyTime);
				if(crtn) {
					break;
				}
				tpCrlDebug("   ...cert %u VERIFIED by CRL", cert->index());
				cert->revokeCheckGood(true);
				if(optFlags & CSSM_TP_ACTION_CRL_SUFFICIENT) {
					/* no more revocation checking necessary for this cert */
					cert->revokeCheckComplete(true);
				}
			} while(0);
			
			/* done processing one cert */
			if(crtn) {
				tpCrlDebug("   ...cert at index %u FAILED crl vfy", 
					cert->index());
				if(ourRtn == CSSM_OK) {
					ourRtn = crtn;
				}
				/* continue on to next cert */
			}	/* error on one cert */
		}		/* for each cert */
	}
	catch(const CssmError &cerr) {
		if(ourRtn == CSSM_OK) {
			ourRtn = cerr.error;
		}
	}
	/* other exceptions fatal */

	/* release all found CRLs */
	for(unsigned dex=0; dex<foundCrls.numCrls(); dex++) {
		TPCrlInfo *crl = foundCrls.crlAtIndex(dex);
		assert(crl != NULL);
		tpDisposeCrl(*crl, vfyCtx);
	}
	/* release issuers */
	if(issuers.Data) {
		free(issuers.Data);
	}
	return ourRtn;
}

