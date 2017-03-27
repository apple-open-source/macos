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
 * CMS digesting.
 */

#include "cmslocal.h"

#include "secitem.h"
#include "secoid.h"

#include <security_asn1/secerr.h>
#include <Security/cssmapi.h>

#include <Security/SecCmsDigestContext.h>

/* Return the maximum value between S and T */
#define MAX(S, T) ({__typeof__(S) _max_s = S; __typeof__(T) _max_t = T; _max_s > _max_t ? _max_s : _max_t;})

struct SecCmsDigestContextStr {
    Boolean		saw_contents;
    int			digcnt;
    CSSM_CC_HANDLE *	digobjs;
};

/*
 * SecCmsDigestContextStartMultiple - start digest calculation using all the
 *  digest algorithms in "digestalgs" in parallel.
 */
SecCmsDigestContextRef
SecCmsDigestContextStartMultiple(SECAlgorithmID **digestalgs)
{
    SecCmsDigestContextRef cmsdigcx;
    CSSM_CC_HANDLE digobj;
    int digcnt;
    int i;

    digcnt = (digestalgs == NULL) ? 0 : SecCmsArrayCount((void **)digestalgs);

    cmsdigcx = (SecCmsDigestContextRef)PORT_ZAlloc(sizeof(struct SecCmsDigestContextStr));
    if (cmsdigcx == NULL)
	return NULL;

    if (digcnt > 0) {
        /* Security check to prevent under-allocation */
        if (digcnt >= (int)(INT_MAX/sizeof(CSSM_CC_HANDLE))) {
            goto loser;
        }
	cmsdigcx->digobjs = (CSSM_CC_HANDLE *)PORT_ZAlloc(digcnt * sizeof(CSSM_CC_HANDLE));
	if (cmsdigcx->digobjs == NULL)
	    goto loser;
    }

    cmsdigcx->digcnt = 0;

    /*
     * Create a digest object context for each algorithm.
     */
    for (i = 0; i < digcnt; i++) {
	digobj = SecCmsUtilGetHashObjByAlgID(digestalgs[i]);
	/*
	 * Skip any algorithm we do not even recognize; obviously,
	 * this could be a problem, but if it is critical then the
	 * result will just be that the signature does not verify.
	 * We do not necessarily want to error out here, because
	 * the particular algorithm may not actually be important,
	 * but we cannot know that until later.
	 */
	if (digobj)
        {
            CSSM_RETURN result;
	    result = CSSM_DigestDataInit(digobj);
            if (result != CSSM_OK)
            {
                goto loser;
            }
        }
        
	cmsdigcx->digobjs[cmsdigcx->digcnt] = digobj;
	cmsdigcx->digcnt++;
    }

    cmsdigcx->saw_contents = PR_FALSE;

    return cmsdigcx;

loser:
    if (cmsdigcx) {
        if (cmsdigcx->digobjs) {
	    PORT_Free(cmsdigcx->digobjs);
            cmsdigcx->digobjs = NULL;
            cmsdigcx->digcnt = 0;
        }
    }
    return NULL;
}

/*
 * SecCmsDigestContextStartSingle - same as SecCmsDigestContextStartMultiple, but
 *  only one algorithm.
 */
SecCmsDigestContextRef
SecCmsDigestContextStartSingle(SECAlgorithmID *digestalg)
{
    SECAlgorithmID *digestalgs[] = { NULL, NULL };		/* fake array */

    digestalgs[0] = digestalg;
    return SecCmsDigestContextStartMultiple(digestalgs);
}

/*
 * SecCmsDigestContextUpdate - feed more data into the digest machine
 */
void
SecCmsDigestContextUpdate(SecCmsDigestContextRef cmsdigcx, const unsigned char *data, size_t len)
{
    CSSM_DATA dataBuf;
    int i;

    dataBuf.Length = len;
    dataBuf.Data = (uint8 *)data;
    cmsdigcx->saw_contents = PR_TRUE;
    for (i = 0; i < cmsdigcx->digcnt; i++)
	if (cmsdigcx->digobjs && cmsdigcx->digobjs[i])
	    CSSM_DigestDataUpdate(cmsdigcx->digobjs[i], &dataBuf, 1);
}

/*
 * SecCmsDigestContextCancel - cancel digesting operation
 */
void
SecCmsDigestContextCancel(SecCmsDigestContextRef cmsdigcx)
{
    int i;

    for (i = 0; i < cmsdigcx->digcnt; i++)
        if (cmsdigcx->digobjs && cmsdigcx->digobjs[i]) {
	    CSSM_DeleteContext(cmsdigcx->digobjs[i]);
            cmsdigcx->digobjs[i] = 0;
        }
}

/*
 * SecCmsDigestContextFinishMultiple - finish the digests and put them
 *  into an array of CSSM_DATAs (allocated on poolp)
 */
OSStatus
SecCmsDigestContextFinishMultiple(SecCmsDigestContextRef cmsdigcx, SecArenaPoolRef poolp,
			    CSSM_DATA_PTR **digestsp)
{
    CSSM_CC_HANDLE digobj;
    CSSM_DATA_PTR *digests, digest;
    int i;
    void *mark;
    OSStatus rv = SECFailure;

    /* no contents? do not update digests */
    if (digestsp == NULL || !cmsdigcx->saw_contents) {
	for (i = 0; i < cmsdigcx->digcnt; i++)
            if (cmsdigcx->digobjs && cmsdigcx->digobjs[i]) {
		CSSM_DeleteContext(cmsdigcx->digobjs[i]);
                cmsdigcx->digobjs[i] = 0;
            }
	rv = SECSuccess;
	if (digestsp)
	    *digestsp = NULL;
	goto cleanup;
    }

    mark = PORT_ArenaMark ((PLArenaPool *)poolp);

    /* Security check to prevent under-allocation */
    if (cmsdigcx->digcnt >= (int)((INT_MAX/(MAX(sizeof(CSSM_DATA_PTR),sizeof(CSSM_DATA))))-1)) {
        goto loser;
    }
    /* allocate digest array & CSSM_DATAs on arena */
    digests = (CSSM_DATA_PTR *)PORT_ArenaAlloc((PLArenaPool *)poolp, (cmsdigcx->digcnt+1) * sizeof(CSSM_DATA_PTR));
    digest = (CSSM_DATA_PTR)PORT_ArenaZAlloc((PLArenaPool *)poolp, cmsdigcx->digcnt * sizeof(CSSM_DATA));
    if (digests == NULL || digest == NULL) {
	goto loser;
    }

    for (i = 0; i < cmsdigcx->digcnt; i++, digest++) {
        if (cmsdigcx->digobjs) {
            digobj = cmsdigcx->digobjs[i];
        } else {
            digobj = 0;
        }

	CSSM_QUERY_SIZE_DATA dataSize;
	rv = CSSM_QuerySize(digobj, CSSM_FALSE, 1, &dataSize);
        if (rv != CSSM_OK)
        {
            goto loser;
        }
        
	int diglength = dataSize.SizeOutputBlock;
	
	if (digobj)
	{
	    digest->Data = (unsigned char*)PORT_ArenaAlloc((PLArenaPool *)poolp, diglength);
	    if (digest->Data == NULL)
		goto loser;
	    digest->Length = diglength;
	    rv = CSSM_DigestDataFinal(digobj, digest);
            if (rv != CSSM_OK)
            {
                goto loser;
            }
            
	    CSSM_DeleteContext(digobj);
            cmsdigcx->digobjs[i] = 0;
	}
	else
	{
	    digest->Data = NULL;
	    digest->Length = 0;
	}
	
	digests[i] = digest;
   }
    digests[i] = NULL;
    *digestsp = digests;

    rv = SECSuccess;

loser:
    if (rv == SECSuccess)
	PORT_ArenaUnmark((PLArenaPool *)poolp, mark);
    else
	PORT_ArenaRelease((PLArenaPool *)poolp, mark);

cleanup:
    if (cmsdigcx->digcnt > 0) {
        SecCmsDigestContextCancel(cmsdigcx);
	PORT_Free(cmsdigcx->digobjs);
        cmsdigcx->digobjs = NULL;
        cmsdigcx->digcnt = 0;
    }
    PORT_Free(cmsdigcx);

    return rv;
}

/*
 * SecCmsDigestContextFinishSingle - same as SecCmsDigestContextFinishMultiple,
 *  but for one digest.
 */
OSStatus
SecCmsDigestContextFinishSingle(SecCmsDigestContextRef cmsdigcx, SecArenaPoolRef poolp,
			    CSSM_DATA_PTR digest)
{
    OSStatus rv = SECFailure;
    CSSM_DATA_PTR *dp;
    PLArenaPool *arena = NULL;

    if ((arena = PORT_NewArena(1024)) == NULL)
	goto loser;

    /* get the digests into arena, then copy the first digest into poolp */
    if (SecCmsDigestContextFinishMultiple(cmsdigcx, (SecArenaPoolRef)arena, &dp) != SECSuccess)
	goto loser;

    /* now copy it into poolp */
    if (SECITEM_CopyItem((PLArenaPool *)poolp, digest, dp[0]) != SECSuccess)
	goto loser;

    rv = SECSuccess;

loser:
    if (arena)
	PORT_FreeArena(arena, PR_FALSE);

    return rv;
}
