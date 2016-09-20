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
 * CMS digestedData methods.
 */

#include <Security/SecCmsDigestedData.h>

#include <Security/SecCmsContentInfo.h>
#include <Security/SecCmsDigestContext.h>

#include "cmslocal.h"

#include "secitem.h"
#include "secoid.h"
#include <security_asn1/secasn1.h>
#include <security_asn1/secerr.h>

/*
 * SecCmsDigestedDataCreate - create a digestedData object (presumably for encoding)
 *
 * version will be set by SecCmsDigestedDataEncodeBeforeStart
 * digestAlg is passed as parameter
 * contentInfo must be filled by the user
 * digest will be calculated while encoding
 */
SecCmsDigestedDataRef
SecCmsDigestedDataCreate(SecCmsMessageRef cmsg, SECAlgorithmID *digestalg)
{
    void *mark;
    SecCmsDigestedDataRef digd;
    PLArenaPool *poolp;

    poolp = cmsg->poolp;

    mark = PORT_ArenaMark(poolp);

    digd = (SecCmsDigestedDataRef)PORT_ArenaZAlloc(poolp, sizeof(SecCmsDigestedData));
    if (digd == NULL)
	goto loser;

    digd->cmsg = cmsg;

    if (SECOID_CopyAlgorithmID (poolp, &(digd->digestAlg), digestalg) != SECSuccess)
	goto loser;

    PORT_ArenaUnmark(poolp, mark);
    return digd;

loser:
    PORT_ArenaRelease(poolp, mark);
    return NULL;
}

/*
 * SecCmsDigestedDataDestroy - destroy a digestedData object
 */
void
SecCmsDigestedDataDestroy(SecCmsDigestedDataRef digd)
{
    if (digd == NULL) {
        return;
    }
    /* everything's in a pool, so don't worry about the storage */
    SecCmsContentInfoDestroy(&(digd->contentInfo));
    return;
}

/*
 * SecCmsDigestedDataGetContentInfo - return pointer to digestedData object's contentInfo
 */
SecCmsContentInfoRef
SecCmsDigestedDataGetContentInfo(SecCmsDigestedDataRef digd)
{
    return &(digd->contentInfo);
}

/*
 * SecCmsDigestedDataEncodeBeforeStart - do all the necessary things to a DigestedData
 *     before encoding begins.
 *
 * In particular:
 *  - set the right version number. The contentInfo's content type must be set up already.
 */
OSStatus
SecCmsDigestedDataEncodeBeforeStart(SecCmsDigestedDataRef digd)
{
    unsigned long version;
    CSSM_DATA_PTR dummy;

    version = SEC_CMS_DIGESTED_DATA_VERSION_DATA;
    if (SecCmsContentInfoGetContentTypeTag(&(digd->contentInfo)) != SEC_OID_PKCS7_DATA)
	version = SEC_CMS_DIGESTED_DATA_VERSION_ENCAP;

    dummy = SEC_ASN1EncodeInteger(digd->cmsg->poolp, &(digd->version), version);
    return (dummy == NULL) ? SECFailure : SECSuccess;
}

/*
 * SecCmsDigestedDataEncodeBeforeData - do all the necessary things to a DigestedData
 *     before the encapsulated data is passed through the encoder.
 *
 * In detail:
 *  - set up the digests if necessary
 */
OSStatus
SecCmsDigestedDataEncodeBeforeData(SecCmsDigestedDataRef digd)
{
    /* set up the digests */
    if (digd->digestAlg.algorithm.Length != 0 && digd->digest.Length == 0) {
	/* if digest is already there, do nothing */
	digd->contentInfo.digcx = SecCmsDigestContextStartSingle(&(digd->digestAlg));
	if (digd->contentInfo.digcx == NULL)
	    return SECFailure;
    }
    return SECSuccess;
}

/*
 * SecCmsDigestedDataEncodeAfterData - do all the necessary things to a DigestedData
 *     after all the encapsulated data was passed through the encoder.
 *
 * In detail:
 *  - finish the digests
 */
OSStatus
SecCmsDigestedDataEncodeAfterData(SecCmsDigestedDataRef digd)
{
    OSStatus rv = SECSuccess;
    /* did we have digest calculation going on? */
    if (digd->contentInfo.digcx) {
	rv = SecCmsDigestContextFinishSingle(digd->contentInfo.digcx,
					     (SecArenaPoolRef)digd->cmsg->poolp, &(digd->digest));
	/* error has been set by SecCmsDigestContextFinishSingle */
	digd->contentInfo.digcx = NULL;
    }

    return rv;
}

/*
 * SecCmsDigestedDataDecodeBeforeData - do all the necessary things to a DigestedData
 *     before the encapsulated data is passed through the encoder.
 *
 * In detail:
 *  - set up the digests if necessary
 */
OSStatus
SecCmsDigestedDataDecodeBeforeData(SecCmsDigestedDataRef digd)
{
    /* is there a digest algorithm yet? */
    if (digd->digestAlg.algorithm.Length == 0)
	return SECFailure;

    digd->contentInfo.digcx = SecCmsDigestContextStartSingle(&(digd->digestAlg));
    if (digd->contentInfo.digcx == NULL)
	return SECFailure;

    return SECSuccess;
}

/*
 * SecCmsDigestedDataDecodeAfterData - do all the necessary things to a DigestedData
 *     after all the encapsulated data was passed through the encoder.
 *
 * In detail:
 *  - finish the digests
 */
OSStatus
SecCmsDigestedDataDecodeAfterData(SecCmsDigestedDataRef digd)
{
    OSStatus rv = SECSuccess;
    /* did we have digest calculation going on? */
    if (digd->contentInfo.digcx) {
	rv = SecCmsDigestContextFinishSingle(digd->contentInfo.digcx,
					     (SecArenaPoolRef)digd->cmsg->poolp, &(digd->cdigest));
	/* error has been set by SecCmsDigestContextFinishSingle */
	digd->contentInfo.digcx = NULL;
    }

    return rv;
}

/*
 * SecCmsDigestedDataDecodeAfterEnd - finalize a digestedData.
 *
 * In detail:
 *  - check the digests for equality
 */
OSStatus
SecCmsDigestedDataDecodeAfterEnd(SecCmsDigestedDataRef digd)
{
    if (!digd) {
        return SECFailure;
    }
    /* did we have digest calculation going on? */
    if (digd->cdigest.Length != 0) {
	/* XXX comparision btw digest & cdigest */
	/* XXX set status */
	/* TODO!!!! */
    }

    return SECSuccess;
}
