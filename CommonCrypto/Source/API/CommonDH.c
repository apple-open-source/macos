/*
 * Copyright (c) 2012 Apple Inc. All Rights Reserved.
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

// #define COMMON_DH_FUNCTIONS
#include "CommonDH.h"
#include "CommonRandomSPI.h"
#include <dispatch/dispatch.h>
#include <dispatch/queue.h>
#include <corecrypto/ccn.h>
#include <corecrypto/ccdh.h>
#include <corecrypto/ccdh_gp.h>
#include "ccMemory.h"
#include "ccErrors.h"
#include "ccdebug.h"

typedef struct CCDHParameters_s {
    ccdh_const_gp_t gp;
    size_t malloced;
} CCDHParmSetstruct, *CCDHParmSet; 

typedef struct DH_ {
	CCDHParmSet parms;
    ccdh_full_ctx_t ctx;
} CCDHstruct, *CCDH;


static struct CCDHParameters_s gp2;
static struct CCDHParameters_s gp5;
CCDHParameters kCCDHRFC2409Group2 = &gp2;
CCDHParameters kCCDHRFC3526Group5 = &gp5;

// = ccdh_gp_rfc3526group05(void);
static void
ccdhInitGPs() {
    static dispatch_once_t dhgpinit;
    dispatch_once(&dhgpinit, ^{
        kCCDHRFC3526Group5->malloced = 0;
        kCCDHRFC3526Group5->gp = ccdh_gp_rfc3526group05();
    });
}


CCDHRef
CCDHCreate(CCDHParameters dhParameter)
{
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    CC_NONULLPARM(dhParameter);
    if(dhParameter == kCCDHRFC2409Group2) return NULL; // no corecrypto group yet for this.
    ccdhInitGPs();
    
    CCDHParmSet CCDHParm = (CCDHParmSet) dhParameter;
    
    CCDH retval = CC_XMALLOC(sizeof(CCDHstruct));
    if(retval == NULL) return retval;
        
    retval->ctx._full = CC_XMALLOC(ccdh_full_ctx_size(ccdh_ccn_size(CCDHParm->gp)));
    
    

    if(retval->ctx._full == NULL) {
        CC_XFREE(retval, sizeof(CCDHstruct));
        return NULL;
    }
    
    ccdh_ctx_init(CCDHParm->gp, retval->ctx);
    retval->parms = CCDHParm;
        
    return retval;
}

void
CCDHRelease(CCDHRef ref)
{
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    if(ref == NULL) return;
    CCDH keyref = (CCDH) ref;
    if(keyref->ctx._full) 
        CC_XFREE(keyref->ctx._full, ccdh_full_ctx_size(ccdh_ccn_size(keyref->parms->gp)));
    keyref->ctx._full = keyref->parms = NULL;
    CC_XFREE(keyref, sizeof(CCDHstruct));
}

int
CCDHGenerateKey(CCDHRef ref, void *output, size_t *outputLength)
{
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    CC_NONULLPARM(ref);
    CC_NONULLPARM(output);
    CC_NONULLPARM(outputLength);
    
    CCDH keyref = (CCDH) ref;
    
    if(ccdh_generate_key(keyref->parms->gp, ccDRBGGetRngState(), keyref->ctx.pub))
        return -1;
    
    size_t size_needed = ccdh_export_pub_size(keyref->ctx);
    if(size_needed > *outputLength) {
        *outputLength = size_needed;
        return -1;
    }
    
    *outputLength = size_needed;
    ccdh_export_pub(keyref->ctx, output);
    return 0;
}


int
CCDHComputeKey(unsigned char *sharedKey, size_t *sharedKeyLen, const void *peerPubKey, size_t peerPubKeyLen, CCDHRef ref)
{
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    CC_NONULLPARM(sharedKey);
    CC_NONULLPARM(sharedKeyLen);
    CC_NONULLPARM(peerPubKey);
    CC_NONULLPARM(ref);
    
    CCDH keyref = (CCDH) ref;
    ccdh_pub_ctx_decl_gp(keyref->parms->gp, peer_pub);
    cc_size n = ccdh_ctx_n(keyref->ctx);
    cc_unit skey[n];
    
    if(ccdh_import_pub(keyref->parms->gp, peerPubKeyLen, peerPubKey,
                    peer_pub))
        return -1;
    
    if(ccdh_compute_key(keyref->ctx, peer_pub, skey))
        return -1;
    
    size_t size_needed = ccn_write_uint_size(n, skey);
    if(size_needed > *sharedKeyLen) {
        *sharedKeyLen = size_needed;
        return -1;
    }
    *sharedKeyLen = size_needed;
    (void) ccn_write_uint_padded(n, skey, *sharedKeyLen, sharedKey);
    
    return 0;

}

CCDHParameters
CCDHParametersCreateFromData(const void *p, size_t pLen, const void *g, size_t gLen, size_t l)
{
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    CC_NONULLPARM(p);
    CC_NONULLPARM(g);
    
    cc_size psize = ccn_nof_size(pLen);
    cc_size gsize = ccn_nof_size(gLen);
    cc_size n = (psize > gsize) ? psize: gsize;
    cc_unit pval[n], gval[n];
    
    CCDHParmSet retval = CC_XMALLOC(sizeof(CCDHParmSetstruct));
    if(retval == NULL) return NULL;
    
    retval->malloced = ccdh_gp_size(n);
    retval->gp.gp = (ccdh_gp *) CC_XMALLOC(retval->malloced);
    if(retval->gp.gp == NULL) {
        retval->malloced = 0;
        CC_XFREE(retval, sizeof(CCDHParmSetstruct));
        return NULL;
    }
    
    if(ccdh_init_gp(retval->gp._ncgp, n, pval, gval, (cc_size) l))
        return NULL;
    return retval;

}

void
CCDHParametersRelease(CCDHParameters parameters)
{
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    if(parameters == NULL) return;
    if(parameters == kCCDHRFC2409Group2) return;
    if(parameters == kCCDHRFC3526Group5) return;

    CCDHParmSet CCDHParm = (CCDHParmSet) parameters;
    if(CCDHParm->malloced) 
        CC_XFREE(CCDHParm->gp.gp, retval->malloced);
    CCDHParm->malloced = 0;
    CCDHParm->gp.gp = NULL;
    CC_XFREE(CCDHParm, sizeof(CCDHParmSetstruct));
}

// TODO - needs PKCS3 in/out
CCDHParameters
CCDHParametersCreateFromPKCS3(const void *data, size_t len)
{
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    CC_NONULLPARM(data);
    return NULL;
}

size_t
CCDHParametersPKCS3EncodeLength(CCDHParameters parms)
{
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    return 0;
}

size_t
CCDHParametersPKCS3Encode(CCDHParameters parms, void *data, size_t dataAvailable)
{
    CC_DEBUG_LOG(ASL_LEVEL_ERR, "Entering\n");
    return 0;
}

