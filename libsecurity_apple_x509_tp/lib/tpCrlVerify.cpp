/*
 * Copyright (c) 2002 Apple Computer, Inc. All Rights Reserved.
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
 *
 * Written 9/26/02 by Doug Mitchell.
 */

#include "tpCrlVerify.h"
#include "TPCertInfo.h"
#include "TPCrlInfo.h"
#include "tpOcspVerify.h"
#include "tpdebugging.h"
#include "TPNetwork.h"
#include "TPDatabase.h"
#include <Security/oidscert.h>
#include <security_utilities/globalizer.h>
#include <security_utilities/threading.h>
#include <security_cdsa_utilities/cssmerrors.h>

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
	}
	return crl;
}

/* bumps ref count - caller is going to be using the CRL */
void TPCRLCache::add(
	TPCrlInfo 			&crl)
{
	StLock<Mutex> _(mLock);
	crl.mRefCount++;
	appendCrl(crl);
}

/* we delete on this one if --refCount == 0 */
void TPCRLCache::remove(
	TPCrlInfo 			&crl)
{
	StLock<Mutex> _(mLock);
	removeCrl(crl);
	assert(crl.mRefCount > 0);
	crl.mRefCount--;
	if(crl.mRefCount == 0) {
		delete &crl;
	}
	else {
		/* in use, flag for future delete */
		crl.mToBeDeleted = true;
	}
}

/* only delete if refCount zero AND flagged for deletion  */
void TPCRLCache::release(
	TPCrlInfo 			&crl)
{
	StLock<Mutex> _(mLock);
	assert(crl.mRefCount > 0);
	crl.mRefCount--;
	if(crl.mToBeDeleted & (crl.mRefCount == 0)) {
		delete &crl;
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
			tpGlobalCrlCache().remove(*crl);
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
		subject.addStatusCode(crtn);
		tpCrlDebug("   ...tpFindCrlForCert: CRL not found");
		return crtn;
	}
	
	/* got one from net - add to global cache */
	assert(crl != NULL);
	tpGlobalCrlCache().add(*crl);
	crl->mFromWhere = CFW_Net;
	tpCrlDebug("   ...CRL found from net");
	
#if 	WRITE_FETCHED_CRLS_TO_DB
	/* and to DLDB if enabled */
	if((vfyCtx.crlOpts != NULL) && (vfyCtx.crlOpts->crlStore != NULL)) {
		crtn = tpDbStoreCrl(*crl, *vfyCtx.crlOpts->crlStore);
		if(crtn) {
			/* let's not let this affect the CRL verification...just log
			 * the per-cert error. */
			subject.addStatusCode(crtn);
		}
		else {
			tpCrlDebug("   ...CRL written to DB");
		}
	}
#endif	/* WRITE_FETCHED_CRLS_TO_DB */

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
	CSSM_APPLE_TP_CRL_OPT_FLAGS optFlags = 0;
	if(vfyCtx.crlOpts != NULL) {
		optFlags = vfyCtx.crlOpts->CrlFlags;
	}
	
	/* found & verified CRLs we need to release */
	TPCrlGroup foundCrls(vfyCtx.alloc, TGO_Caller);
	
	try {
		
		unsigned certDex;
		TPCrlInfo *crl = NULL;
		
		/* main loop, verify each cert */
		for(certDex=0; certDex<certGroup.numCerts(); certDex++) {
			TPCertInfo *cert = certGroup.certAtIndex(certDex);

			tpCrlDebug("...verifying %s cert %u", 
				cert->isAnchor() ? "anchor " : "", cert->index());
			if(cert->isSelfSigned()) {
				/* CRL meaningless for a root cert */
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
				/* find a CRL for this cert by hook or crook */
				crtn = tpFindCrlForCert(*cert, crl, vfyCtx);
				if(crtn) {
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
	return ourRtn;
}

