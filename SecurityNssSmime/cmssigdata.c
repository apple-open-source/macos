/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

/*
 * CMS signedData methods.
 */

#include "cmslocal.h"

#include "cert.h"
/*#include "cdbhdl.h"*/
#include "secasn1.h"
#include "secitem.h"
#include "secoid.h"
//#include "pk11func.h"
#include "secerr.h"

SecCmsSignedData *
SecCmsSignedDataCreate(SecCmsMessage *cmsg)
{
    void *mark;
    SecCmsSignedData *sigd;
    PLArenaPool *poolp;

    poolp = cmsg->poolp;

    mark = PORT_ArenaMark(poolp);

    sigd = (SecCmsSignedData *)PORT_ArenaZAlloc (poolp, sizeof(SecCmsSignedData));
    if (sigd == NULL)
	goto loser;

    sigd->cmsg = cmsg;

    /* signerInfos, certs, certlists, crls are all empty */
    /* version is set in SecCmsSignedDataFinalize() */

    PORT_ArenaUnmark(poolp, mark);
    return sigd;

loser:
    PORT_ArenaRelease(poolp, mark);
    return NULL;
}

void
SecCmsSignedDataDestroy(SecCmsSignedData *sigd)
{
    SecCmsSignerInfo **signerinfos, *si;

    if (sigd == NULL)
	return;

    if (sigd->certs != NULL)
	CFRelease(sigd->certs);

    signerinfos = sigd->signerInfos;
    if (signerinfos != NULL) {
	while ((si = *signerinfos++) != NULL)
	    SecCmsSignerInfoDestroy(si);
    }

    /* everything's in a pool, so don't worry about the storage */
   SecCmsContentInfoDestroy(&(sigd->contentInfo));
}

/*
 * SecCmsSignedDataEncodeBeforeStart - do all the necessary things to a SignedData
 *     before start of encoding.
 *
 * In detail:
 *  - find out about the right value to put into sigd->version
 *  - come up with a list of digestAlgorithms (which should be the union of the algorithms
 *         in the signerinfos).
 *         If we happen to have a pre-set list of algorithms (and digest values!), we
 *         check if we have all the signerinfos' algorithms. If not, this is an error.
 */
OSStatus
SecCmsSignedDataEncodeBeforeStart(SecCmsSignedData *sigd)
{
    SecCmsSignerInfo *signerinfo;
    SECOidTag digestalgtag;
    CSSM_DATA *dummy;
    int version;
    OSStatus rv;
    PRBool haveDigests = PR_FALSE;
    int n, i;
    PLArenaPool *poolp;

    poolp = sigd->cmsg->poolp;

    /* we assume that we have precomputed digests if there is a list of algorithms, and */
    /* a chunk of data for each of those algorithms */
    if (sigd->digestAlgorithms != NULL && sigd->digests != NULL) {
	for (i=0; sigd->digestAlgorithms[i] != NULL; i++) {
	    if (sigd->digests[i] == NULL)
		break;
	}
	if (sigd->digestAlgorithms[i] == NULL)	/* reached the end of the array? */
	    haveDigests = PR_TRUE;		/* yes: we must have all the digests */
    }
	    
    version = SEC_CMS_SIGNED_DATA_VERSION_BASIC;

    /* RFC2630 5.1 "version is the syntax version number..." */
    if (SecCmsContentInfoGetContentTypeTag(&(sigd->contentInfo)) != SEC_OID_PKCS7_DATA)
	version = SEC_CMS_SIGNED_DATA_VERSION_EXT;

    /* prepare all the SignerInfos (there may be none) */
    for (i=0; i < SecCmsSignedDataSignerInfoCount(sigd); i++) {
	signerinfo = SecCmsSignedDataGetSignerInfo(sigd, i);

	/* RFC2630 5.1 "version is the syntax version number..." */
	if (SecCmsSignerInfoGetVersion(signerinfo) != SEC_CMS_SIGNER_INFO_VERSION_ISSUERSN)
	    version = SEC_CMS_SIGNED_DATA_VERSION_EXT;
	
	/* collect digestAlgorithms from SignerInfos */
	/* (we need to know which algorithms we have when the content comes in) */
	/* do not overwrite any existing digestAlgorithms (and digest) */
	digestalgtag = SecCmsSignerInfoGetDigestAlgTag(signerinfo);
	n = SecCmsAlgArrayGetIndexByAlgTag(sigd->digestAlgorithms, digestalgtag);
	if (n < 0 && haveDigests) {
	    /* oops, there is a digestalg we do not have a digest for */
	    /* but we were supposed to have all the digests already... */
	    goto loser;
	} else if (n < 0) {
	    /* add the digestAlgorithm & a NULL digest */
	    rv = SecCmsSignedDataAddDigest(poolp, sigd, digestalgtag, NULL);
	    if (rv != SECSuccess)
		goto loser;
	} else {
	    /* found it, nothing to do */
	}
    }

    dummy = SEC_ASN1EncodeInteger(poolp, &(sigd->version), (long)version);
    if (dummy == NULL)
	return SECFailure;

    /* this is a SET OF, so we need to sort them guys */
    rv = SecCmsArraySortByDER((void **)sigd->digestAlgorithms, 
                                SEC_ASN1_GET(SECOID_AlgorithmIDTemplate),
				(void **)sigd->digests);
    if (rv != SECSuccess)
	return SECFailure;
    
    return SECSuccess;

loser:
    return SECFailure;
}

OSStatus
SecCmsSignedDataEncodeBeforeData(SecCmsSignedData *sigd)
{
    /* set up the digests */
    if (sigd->digestAlgorithms != NULL) {
	sigd->contentInfo.digcx = SecCmsDigestContextStartMultiple(sigd->digestAlgorithms);
	if (sigd->contentInfo.digcx == NULL)
	    return SECFailure;
    }
    return SECSuccess;
}

/*
 * SecCmsSignedDataEncodeAfterData - do all the necessary things to a SignedData
 *     after all the encapsulated data was passed through the encoder.
 *
 * In detail:
 *  - create the signatures in all the SignerInfos
 *
 * Please note that nothing is done to the Certificates and CRLs in the message - this
 * is entirely the responsibility of our callers.
 */
OSStatus
SecCmsSignedDataEncodeAfterData(SecCmsSignedData *sigd)
{
    SecCmsSignerInfo **signerinfos, *signerinfo;
    SecCmsContentInfo *cinfo;
    SECOidTag digestalgtag;
    OSStatus ret = SECFailure;
    OSStatus rv;
    CSSM_DATA *contentType;
    int certcount;
    int i, ci, n, rci, si;
    PLArenaPool *poolp;
    CFArrayRef certlist;
    extern const SEC_ASN1Template SecCmsSignerInfoTemplate[];

    poolp = sigd->cmsg->poolp;
    cinfo = &(sigd->contentInfo);

    /* did we have digest calculation going on? */
    if (cinfo->digcx) {
	rv = SecCmsDigestContextFinishMultiple(cinfo->digcx, poolp, &(sigd->digests));
	if (rv != SECSuccess)
	    goto loser;		/* error has been set by SecCmsDigestContextFinishMultiple */
	cinfo->digcx = NULL;
    }

    signerinfos = sigd->signerInfos;
    certcount = 0;

    /* prepare all the SignerInfos (there may be none) */
    for (i=0; i < SecCmsSignedDataSignerInfoCount(sigd); i++) {
	signerinfo = SecCmsSignedDataGetSignerInfo(sigd, i);

	/* find correct digest for this signerinfo */
	digestalgtag = SecCmsSignerInfoGetDigestAlgTag(signerinfo);
	n = SecCmsAlgArrayGetIndexByAlgTag(sigd->digestAlgorithms, digestalgtag);
	if (n < 0 || sigd->digests == NULL || sigd->digests[n] == NULL) {
	    /* oops - digest not found */
	    PORT_SetError(SEC_ERROR_DIGEST_NOT_FOUND);
	    goto loser;
	}

	/* XXX if our content is anything else but data, we need to force the
	 * presence of signed attributes (RFC2630 5.3 "signedAttributes is a
	 * collection...") */

	/* pass contentType here as we want a contentType attribute */
	if ((contentType = SecCmsContentInfoGetContentTypeOID(cinfo)) == NULL)
	    goto loser;

	/* sign the thing */
	rv = SecCmsSignerInfoSign(signerinfo, sigd->digests[n], contentType);
	if (rv != SECSuccess)
	    goto loser;

	/* while we're at it, count number of certs in certLists */
	certlist = SecCmsSignerInfoGetCertList(signerinfo);
	if (certlist)
	    certcount += CFArrayGetCount(certlist);
    }

    /* this is a SET OF, so we need to sort them guys */
    rv = SecCmsArraySortByDER((void **)signerinfos, SecCmsSignerInfoTemplate, NULL);
    if (rv != SECSuccess)
	goto loser;

    /*
     * now prepare certs & crls
     */

    /* count the rest of the certs */
    if (sigd->certs != NULL)
	certcount += CFArrayGetCount(sigd->certs);

    if (certcount == 0) {
	sigd->rawCerts = NULL;
    } else {
	/*
	 * Combine all of the certs and cert chains into rawcerts.
	 * Note: certcount is an upper bound; we may not need that many slots
	 * but we will allocate anyway to avoid having to do another pass.
	 * (The temporary space saving is not worth it.)
	 *
	 * XXX ARGH - this NEEDS to be fixed. need to come up with a decent
	 *  SetOfDERcertficates implementation
	 */
	sigd->rawCerts = (CSSM_DATA **)PORT_ArenaAlloc(poolp, (certcount + 1) * sizeof(CSSM_DATA *));
	if (sigd->rawCerts == NULL)
	    return SECFailure;

	/*
	 * XXX Want to check for duplicates and not add *any* cert that is
	 * already in the set.  This will be more important when we start
	 * dealing with larger sets of certs, dual-key certs (signing and
	 * encryption), etc.  For the time being we can slide by...
	 *
	 * XXX ARGH - this NEEDS to be fixed. need to come up with a decent
	 *  SetOfDERcertficates implementation
	 */
	rci = 0;
	if (signerinfos != NULL) {
	    for (si = 0; signerinfos[si] != NULL; si++) {
		signerinfo = signerinfos[si];
		for (ci = 0; ci < CFArrayGetCount(signerinfo->certList); ci++) {
		    sigd->rawCerts[rci] = PORT_ArenaZAlloc(poolp, sizeof(CSSM_DATA));
		    SecCertificateRef cert = (SecCertificateRef)CFArrayGetValueAtIndex(signerinfo->certList, ci);
		    SecCertificateGetData(cert, sigd->rawCerts[rci++]);
		}
	    }
	}

	if (sigd->certs != NULL) {
	    for (ci = 0; ci < CFArrayGetCount(sigd->certs); ci++) {
		sigd->rawCerts[rci] = PORT_ArenaZAlloc(poolp, sizeof(CSSM_DATA));
		SecCertificateRef cert = (SecCertificateRef)CFArrayGetValueAtIndex(sigd->certs, ci);
		SecCertificateGetData(cert, sigd->rawCerts[rci++]);
	    }
	}

	sigd->rawCerts[rci] = NULL;

	/* this is a SET OF, so we need to sort them guys - we have the DER already, though */
	SecCmsArraySort((void **)sigd->rawCerts, SecCmsUtilDERCompare, NULL, NULL);
    }

    ret = SECSuccess;

loser:
    return ret;
}

OSStatus
SecCmsSignedDataDecodeBeforeData(SecCmsSignedData *sigd)
{
    /* set up the digests */
    if (sigd->digestAlgorithms != NULL && sigd->digests == NULL) {
	/* if digests are already there, do nothing */
	sigd->contentInfo.digcx = SecCmsDigestContextStartMultiple(sigd->digestAlgorithms);
	if (sigd->contentInfo.digcx == NULL)
	    return SECFailure;
    }
    return SECSuccess;
}

/*
 * SecCmsSignedDataDecodeAfterData - do all the necessary things to a SignedData
 *     after all the encapsulated data was passed through the decoder.
 */
OSStatus
SecCmsSignedDataDecodeAfterData(SecCmsSignedData *sigd)
{
    /* did we have digest calculation going on? */
    if (sigd->contentInfo.digcx) {
	if (SecCmsDigestContextFinishMultiple(sigd->contentInfo.digcx, sigd->cmsg->poolp, &(sigd->digests)) != SECSuccess)
	    return SECFailure;	/* error has been set by SecCmsDigestContextFinishMultiple */
	sigd->contentInfo.digcx = NULL;
    }
    return SECSuccess;
}

/*
 * SecCmsSignedDataDecodeAfterEnd - do all the necessary things to a SignedData
 *     after all decoding is finished.
 */
OSStatus
SecCmsSignedDataDecodeAfterEnd(SecCmsSignedData *sigd)
{
    SecCmsSignerInfo **signerinfos;
    int i;

    /* set cmsg for all the signerinfos */
    signerinfos = sigd->signerInfos;

    /* set cmsg for all the signerinfos */
    if (signerinfos) {
	for (i = 0; signerinfos[i] != NULL; i++)
	    signerinfos[i]->cmsg = sigd->cmsg;
    }

    return SECSuccess;
}

/* 
 * SecCmsSignedDataGetSignerInfos - retrieve the SignedData's signer list
 */
SecCmsSignerInfo **
SecCmsSignedDataGetSignerInfos(SecCmsSignedData *sigd)
{
    return sigd->signerInfos;
}

int
SecCmsSignedDataSignerInfoCount(SecCmsSignedData *sigd)
{
    return SecCmsArrayCount((void **)sigd->signerInfos);
}

SecCmsSignerInfo *
SecCmsSignedDataGetSignerInfo(SecCmsSignedData *sigd, int i)
{
    return sigd->signerInfos[i];
}

/* 
 * SecCmsSignedDataGetDigestAlgs - retrieve the SignedData's digest algorithm list
 */
SECAlgorithmID **
SecCmsSignedDataGetDigestAlgs(SecCmsSignedData *sigd)
{
    return sigd->digestAlgorithms;
}

/*
 * SecCmsSignedDataGetContentInfo - return pointer to this signedData's contentinfo
 */
SecCmsContentInfo *
SecCmsSignedDataGetContentInfo(SecCmsSignedData *sigd)
{
    return &(sigd->contentInfo);
}

/* 
 * SecCmsSignedDataGetCertificateList - retrieve the SignedData's certificate list
 */
CSSM_DATA **
SecCmsSignedDataGetCertificateList(SecCmsSignedData *sigd)
{
    return sigd->rawCerts;
}

OSStatus
SecCmsSignedDataImportCerts(SecCmsSignedData *sigd, SecKeychainRef keychain,
				SECCertUsage certusage, PRBool keepcerts)
{
    int certcount;
    OSStatus rv;
    int i;

    certcount = SecCmsArrayCount((void **)sigd->rawCerts);

    rv = CERT_ImportCerts(keychain, certusage, certcount, sigd->rawCerts, NULL,
			  keepcerts, PR_FALSE, NULL);

    /* XXX CRL handling */

    if (sigd->signerInfos != NULL) {
	/* fill in all signerinfo's certs */
	for (i = 0; sigd->signerInfos[i] != NULL; i++)
	    (void)SecCmsSignerInfoGetSigningCertificate(sigd->signerInfos[i], keychain);
    }

    return rv;
}

/*
 * XXX the digests need to be passed in BETWEEN the decoding and the verification in case
 *     of external signatures!
 */

/*
 * SecCmsSignedDataVerifySignerInfo - check the signatures.
 *
 * The digests were either calculated during decoding (and are stored in the
 * signedData itself) or set after decoding using SecCmsSignedDataSetDigests.
 *
 * The verification checks if the signing cert is valid and has a trusted chain
 * for the purpose specified by "policies".
 *
 * If trustRef is NULL the cert chain is verified and the VerificationStatus is set accordingly.
 * Otherwise a SecTrust object is returned for the caller to evaluate using SecTrustEvaluate().
 */
OSStatus
SecCmsSignedDataVerifySignerInfo(SecCmsSignedData *sigd, int i, 
			    SecKeychainRef keychainOrArray, CFTypeRef policies, SecTrustRef *trustRef)
{
    SecCmsSignerInfo *signerinfo;
    SecCmsContentInfo *cinfo;
    SECOidData *algiddata;
    CSSM_DATA *contentType, *digest;

    cinfo = &(sigd->contentInfo);

    signerinfo = sigd->signerInfos[i];

    /* verify certificate */
    if (SecCmsSignerInfoVerifyCertificate(signerinfo, keychainOrArray, policies, trustRef) != SECSuccess)
	return SECFailure;		/* error is set by SecCmsSignerInfoVerifyCertificate */

    /* find digest and contentType for signerinfo */
    algiddata = SecCmsSignerInfoGetDigestAlg(signerinfo);
    digest = SecCmsSignedDataGetDigestByAlgTag(sigd, algiddata->offset);
    contentType = SecCmsContentInfoGetContentTypeOID(cinfo);

    /* now verify signature */
    return SecCmsSignerInfoVerify(signerinfo, digest, contentType);
}

/*
 * SecCmsSignedDataVerifyCertsOnly - verify the certs in a certs-only message
 */
OSStatus
SecCmsSignedDataVerifyCertsOnly(SecCmsSignedData *sigd, 
                                  SecKeychainRef keychainOrArray, 
                                  CFTypeRef policies)
{
    SecCertificateRef cert;
    OSStatus rv = SECSuccess;
    int i;
    int count;

    if (!sigd || !keychainOrArray || !sigd->rawCerts) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }

    count = SecCmsArrayCount((void**)sigd->rawCerts);
    for (i=0; i < count; i++) {
	if (sigd->certs && CFArrayGetCount(sigd->certs) > i) {
	    cert = (SecCertificateRef)CFArrayGetValueAtIndex(sigd->certs, i);
	    CFRetain(cert);
	} else {
	    cert = CERT_FindCertByDERCert(keychainOrArray, sigd->rawCerts[i]);
	    if (!cert) {
		rv = SECFailure;
		break;
	    }
	}
	rv |= CERT_VerifyCert(keychainOrArray, cert, policies, CFAbsoluteTimeGetCurrent(), NULL);
	CFRelease(cert);
    }

    return rv;
}

/*
 * SecCmsSignedDataHasDigests - see if we have digests in place
 */
PRBool
SecCmsSignedDataHasDigests(SecCmsSignedData *sigd)
{
    return (sigd->digests != NULL);
}

OSStatus
SecCmsSignedDataAddCertList(SecCmsSignedData *sigd, CFArrayRef certlist)
{
    PORT_Assert(certlist != NULL);

    if (certlist == NULL)
	return SECFailure;

    if (!sigd->certs)
	sigd->certs = CFArrayCreateMutableCopy(NULL, 0, certlist);
    else
    {
	CFRange certlistRange = { 0, CFArrayGetCount(certlist) };
	CFArrayAppendArray(sigd->certs, certlist, certlistRange);
    }

    return SECSuccess;
}

/*
 * SecCmsSignedDataAddCertChain - add cert and its entire chain to the set of certs 
 */
OSStatus
SecCmsSignedDataAddCertChain(SecCmsSignedData *sigd, SecCertificateRef cert)
{
    CFArrayRef certlist;
    SECCertUsage usage;
    OSStatus rv;

    usage = certUsageEmailSigner;

    /* do not include root */
    certlist = CERT_CertChainFromCert(cert, usage, PR_FALSE);
    if (certlist == NULL)
	return SECFailure;

    rv = SecCmsSignedDataAddCertList(sigd, certlist);
    CFRelease(certlist);

    return rv;
}

OSStatus
SecCmsSignedDataAddCertificate(SecCmsSignedData *sigd, SecCertificateRef cert)
{
    PORT_Assert(cert != NULL);

    if (cert == NULL)
	return SECFailure;

    if (!sigd->certs)
	sigd->certs = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

    CFArrayAppendValue(sigd->certs, cert);

    return SECSuccess;
}

PRBool
SecCmsSignedDataContainsCertsOrCrls(SecCmsSignedData *sigd)
{
    if (sigd->rawCerts != NULL && sigd->rawCerts[0] != NULL)
	return PR_TRUE;
    else if (sigd->rawCrls != NULL && sigd->rawCrls[0] != NULL)
	return PR_TRUE;
    else
	return PR_FALSE;
}

OSStatus
SecCmsSignedDataAddSignerInfo(SecCmsSignedData *sigd,
				SecCmsSignerInfo *signerinfo)
{
    void *mark;
    OSStatus rv;
    SECOidTag digestalgtag;
    PLArenaPool *poolp;

    poolp = sigd->cmsg->poolp;

    mark = PORT_ArenaMark(poolp);

    /* add signerinfo */
    rv = SecCmsArrayAdd(poolp, (void ***)&(sigd->signerInfos), (void *)signerinfo);
    if (rv != SECSuccess)
	goto loser;

    /*
     * add empty digest
     * Empty because we don't have it yet. Either it gets created during encoding
     * (if the data is present) or has to be set externally.
     * XXX maybe pass it in optionally?
     */
    digestalgtag = SecCmsSignerInfoGetDigestAlgTag(signerinfo);
    rv = SecCmsSignedDataSetDigestValue(sigd, digestalgtag, NULL);
    if (rv != SECSuccess)
	goto loser;

    /*
     * The last thing to get consistency would be adding the digest.
     */

    PORT_ArenaUnmark(poolp, mark);
    return SECSuccess;

loser:
    PORT_ArenaRelease (poolp, mark);
    return SECFailure;
}

CSSM_DATA *
SecCmsSignedDataGetDigestByAlgTag(SecCmsSignedData *sigd, SECOidTag algtag)
{
    int idx;

    idx = SecCmsAlgArrayGetIndexByAlgTag(sigd->digestAlgorithms, algtag);
    return sigd->digests[idx];
}

/*
 * SecCmsSignedDataSetDigests - set a signedData's digests member
 *
 * "digestalgs" - array of digest algorithm IDs
 * "digests"    - array of digests corresponding to the digest algorithms
 */
OSStatus
SecCmsSignedDataSetDigests(SecCmsSignedData *sigd,
				SECAlgorithmID **digestalgs,
				CSSM_DATA **digests)
{
    int cnt, i, idx;

    if (sigd->digestAlgorithms == NULL) {
	PORT_SetError(SEC_ERROR_INVALID_ARGS);
	return SECFailure;
    }

    /* we assume that the digests array is just not there yet */
    PORT_Assert(sigd->digests == NULL);
    if (sigd->digests != NULL) {
	PORT_SetError(SEC_ERROR_LIBRARY_FAILURE);
	return SECFailure;
    }

    /* now allocate one (same size as digestAlgorithms) */
    cnt = SecCmsArrayCount((void **)sigd->digestAlgorithms);
    sigd->digests = PORT_ArenaZAlloc(sigd->cmsg->poolp, (cnt + 1) * sizeof(CSSM_DATA *));
    if (sigd->digests == NULL) {
	PORT_SetError(SEC_ERROR_NO_MEMORY);
	return SECFailure;
    }

    for (i = 0; sigd->digestAlgorithms[i] != NULL; i++) {
	/* try to find the sigd's i'th digest algorithm in the array we passed in */
	idx = SecCmsAlgArrayGetIndexByAlgID(digestalgs, sigd->digestAlgorithms[i]);
	if (idx < 0) {
	    PORT_SetError(SEC_ERROR_DIGEST_NOT_FOUND);
	    return SECFailure;
	}

	/* found it - now set it */
	if ((sigd->digests[i] = SECITEM_AllocItem(sigd->cmsg->poolp, NULL, 0)) == NULL ||
	    SECITEM_CopyItem(sigd->cmsg->poolp, sigd->digests[i], digests[idx]) != SECSuccess)
	{
	    PORT_SetError(SEC_ERROR_NO_MEMORY);
	    return SECFailure;
	}
    }
    return SECSuccess;
}

OSStatus
SecCmsSignedDataSetDigestValue(SecCmsSignedData *sigd,
				SECOidTag digestalgtag,
				CSSM_DATA *digestdata)
{
    CSSM_DATA *digest = NULL;
    PLArenaPool *poolp;
    void *mark;
    int n, cnt;

    poolp = sigd->cmsg->poolp;

    mark = PORT_ArenaMark(poolp);

   
    if (digestdata) {
        digest = (CSSM_DATA *) PORT_ArenaZAlloc(poolp,sizeof(CSSM_DATA));

	/* copy digestdata item to arena (in case we have it and are not only making room) */
	if (SECITEM_CopyItem(poolp, digest, digestdata) != SECSuccess)
	    goto loser;
    }

    /* now allocate one (same size as digestAlgorithms) */
    if (sigd->digests == NULL) {
        cnt = SecCmsArrayCount((void **)sigd->digestAlgorithms);
        sigd->digests = PORT_ArenaZAlloc(sigd->cmsg->poolp, (cnt + 1) * sizeof(CSSM_DATA *));
        if (sigd->digests == NULL) {
	        PORT_SetError(SEC_ERROR_NO_MEMORY);
	        return SECFailure;
        }
    }

    n = -1;
    if (sigd->digestAlgorithms != NULL)
	n = SecCmsAlgArrayGetIndexByAlgTag(sigd->digestAlgorithms, digestalgtag);

    /* if not found, add a digest */
    if (n < 0) {
	if (SecCmsSignedDataAddDigest(poolp, sigd, digestalgtag, digest) != SECSuccess)
	    goto loser;
    } else {
	/* replace NULL pointer with digest item (and leak previous value) */
	sigd->digests[n] = digest;
    }

    PORT_ArenaUnmark(poolp, mark);
    return SECSuccess;

loser:
    PORT_ArenaRelease(poolp, mark);
    return SECFailure;
}

OSStatus
SecCmsSignedDataAddDigest(PRArenaPool *poolp,
				SecCmsSignedData *sigd,
				SECOidTag digestalgtag,
				CSSM_DATA *digest)
{
    SECAlgorithmID *digestalg;
    void *mark;

    mark = PORT_ArenaMark(poolp);

    digestalg = PORT_ArenaZAlloc(poolp, sizeof(SECAlgorithmID));
    if (digestalg == NULL)
	goto loser;

    if (SECOID_SetAlgorithmID (poolp, digestalg, digestalgtag, NULL) != SECSuccess) /* no params */
	goto loser;

    if (SecCmsArrayAdd(poolp, (void ***)&(sigd->digestAlgorithms), (void *)digestalg) != SECSuccess ||
	/* even if digest is NULL, add dummy to have same-size array */
	SecCmsArrayAdd(poolp, (void ***)&(sigd->digests), (void *)digest) != SECSuccess)
    {
	goto loser;
    }

    PORT_ArenaUnmark(poolp, mark);
    return SECSuccess;

loser:
    PORT_ArenaRelease(poolp, mark);
    return SECFailure;
}

CSSM_DATA *
SecCmsSignedDataGetDigestValue(SecCmsSignedData *sigd, SECOidTag digestalgtag)
{
    int n;

    if (sigd->digestAlgorithms == NULL)
	return NULL;

    n = SecCmsAlgArrayGetIndexByAlgTag(sigd->digestAlgorithms, digestalgtag);

    return (n < 0) ? NULL : sigd->digests[n];
}

/* =============================================================================
 * Misc. utility functions
 */

/*
 * SecCmsSignedDataCreateCertsOnly - create a certs-only SignedData.
 *
 * cert          - base certificates that will be included
 * include_chain - if true, include the complete cert chain for cert
 *
 * More certs and chains can be added via AddCertificate and AddCertChain.
 *
 * An error results in a return value of NULL and an error set.
 *
 * XXXX CRLs
 */
SecCmsSignedData *
SecCmsSignedDataCreateCertsOnly(SecCmsMessage *cmsg, SecCertificateRef cert, PRBool include_chain)
{
    SecCmsSignedData *sigd;
    void *mark;
    PLArenaPool *poolp;
    OSStatus rv;

    poolp = cmsg->poolp;
    mark = PORT_ArenaMark(poolp);

    sigd = SecCmsSignedDataCreate(cmsg);
    if (sigd == NULL)
	goto loser;

    /* no signerinfos, thus no digestAlgorithms */

    /* but certs */
    if (include_chain) {
	rv = SecCmsSignedDataAddCertChain(sigd, cert);
    } else {
	rv = SecCmsSignedDataAddCertificate(sigd, cert);
    }
    if (rv != SECSuccess)
	goto loser;

    /* RFC2630 5.2 sez:
     * In the degenerate case where there are no signers, the
     * EncapsulatedContentInfo value being "signed" is irrelevant.  In this
     * case, the content type within the EncapsulatedContentInfo value being
     * "signed" should be id-data (as defined in section 4), and the content
     * field of the EncapsulatedContentInfo value should be omitted.
     */
    rv = SecCmsContentInfoSetContentData(cmsg, &(sigd->contentInfo), NULL, PR_TRUE);
    if (rv != SECSuccess)
	goto loser;

    PORT_ArenaUnmark(poolp, mark);
    return sigd;

loser:
    if (sigd)
	SecCmsSignedDataDestroy(sigd);
    PORT_ArenaRelease(poolp, mark);
    return NULL;
}

/* TODO:
 * SecCmsSignerInfoGetReceiptRequest()
 * SecCmsSignedDataHasReceiptRequest()
 * easy way to iterate over signers
 */

