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
 * CMS envelopedData methods.
 */

#include <Security/SecCmsEnvelopedData.h>

#include <Security/SecCmsContentInfo.h>
#include <Security/SecCmsRecipientInfo.h>

#include "cmslocal.h"

#include "secitem.h"
#include "secoid.h"
#include "cryptohi.h"

#include <security_asn1/secasn1.h>
#include <security_asn1/secerr.h>
#include <Security/SecKeyPriv.h>

/*
 * SecCmsEnvelopedDataCreate - create an enveloped data message
 */
SecCmsEnvelopedDataRef
SecCmsEnvelopedDataCreate(SecCmsMessageRef cmsg, SECOidTag algorithm, int keysize)
{
    void *mark;
    SecCmsEnvelopedDataRef envd;
    PLArenaPool *poolp;
    OSStatus rv;

    poolp = cmsg->poolp;

    mark = PORT_ArenaMark(poolp);

    envd = (SecCmsEnvelopedDataRef)PORT_ArenaZAlloc(poolp, sizeof(SecCmsEnvelopedData));
    if (envd == NULL)
	goto loser;

    envd->cmsg = cmsg;

    /* version is set in SecCmsEnvelopedDataEncodeBeforeStart() */

    rv = SecCmsContentInfoSetContentEncAlg((SecArenaPoolRef)poolp, &(envd->contentInfo), algorithm, NULL, keysize);
    if (rv != SECSuccess)
	goto loser;

    PORT_ArenaUnmark(poolp, mark);
    return envd;

loser:
    PORT_ArenaRelease(poolp, mark);
    return NULL;
}

/*
 * SecCmsEnvelopedDataDestroy - destroy an enveloped data message
 */
void
SecCmsEnvelopedDataDestroy(SecCmsEnvelopedDataRef edp)
{
    SecCmsRecipientInfoRef *recipientinfos;
    SecCmsRecipientInfoRef ri;

    if (edp == NULL)
	return;

    recipientinfos = edp->recipientInfos;
    if (recipientinfos == NULL)
	return;

    while ((ri = *recipientinfos++) != NULL)
	SecCmsRecipientInfoDestroy(ri);

   SecCmsContentInfoDestroy(&(edp->contentInfo));

}

/*
 * SecCmsEnvelopedDataGetContentInfo - return pointer to this envelopedData's contentinfo
 */
SecCmsContentInfoRef
SecCmsEnvelopedDataGetContentInfo(SecCmsEnvelopedDataRef envd)
{
    return &(envd->contentInfo);
}

/*
 * SecCmsEnvelopedDataAddRecipient - add a recipientinfo to the enveloped data msg
 *
 * rip must be created on the same pool as edp - this is not enforced, though.
 */
OSStatus
SecCmsEnvelopedDataAddRecipient(SecCmsEnvelopedDataRef edp, SecCmsRecipientInfoRef rip)
{
    void *mark;
    OSStatus rv;

    /* XXX compare pools, if not same, copy rip into edp's pool */

    PR_ASSERT(edp != NULL);
    PR_ASSERT(rip != NULL);

    mark = PORT_ArenaMark(edp->cmsg->poolp);

    rv = SecCmsArrayAdd(edp->cmsg->poolp, (void ***)&(edp->recipientInfos), (void *)rip);
    if (rv != SECSuccess) {
	PORT_ArenaRelease(edp->cmsg->poolp, mark);
	return SECFailure;
    }

    PORT_ArenaUnmark (edp->cmsg->poolp, mark);
    return SECSuccess;
}

/*
 * SecCmsEnvelopedDataEncodeBeforeStart - prepare this envelopedData for encoding
 *
 * at this point, we need
 * - recipientinfos set up with recipient's certificates
 * - a content encryption algorithm (if none, 3DES will be used)
 *
 * this function will generate a random content encryption key (aka bulk key),
 * initialize the recipientinfos with certificate identification and wrap the bulk key
 * using the proper algorithm for every certificiate.
 * it will finally set the bulk algorithm and key so that the encode step can find it.
 */
OSStatus
SecCmsEnvelopedDataEncodeBeforeStart(SecCmsEnvelopedDataRef envd)
{
    int version;
    SecCmsRecipientInfoRef *recipientinfos;
    SecCmsContentInfoRef cinfo;
    SecSymmetricKeyRef bulkkey = NULL;
    CSSM_ALGORITHMS algorithm;
    SECOidTag bulkalgtag;
    //CK_MECHANISM_TYPE type;
    //PK11SlotInfo *slot;
    OSStatus rv;
    CSSM_DATA_PTR dummy;
    PLArenaPool *poolp;
    extern const SecAsn1Template SecCmsRecipientInfoTemplate[];
    void *mark = NULL;
    int i;

    poolp = envd->cmsg->poolp;
    cinfo = &(envd->contentInfo);

    recipientinfos = envd->recipientInfos;
    if (recipientinfos == NULL) {
	PORT_SetError(SEC_ERROR_BAD_DATA);
#if 0
	PORT_SetErrorString("Cannot find recipientinfos to encode.");
#endif
	goto loser;
    }

    version = SEC_CMS_ENVELOPED_DATA_VERSION_REG;
    if (envd->originatorInfo != NULL || envd->unprotectedAttr != NULL) {
	version = SEC_CMS_ENVELOPED_DATA_VERSION_ADV;
    } else {
	for (i = 0; recipientinfos[i] != NULL; i++) {
	    if (SecCmsRecipientInfoGetVersion(recipientinfos[i]) != 0) {
		version = SEC_CMS_ENVELOPED_DATA_VERSION_ADV;
		break;
	    }
	}
    }
    dummy = SEC_ASN1EncodeInteger(poolp, &(envd->version), version);
    if (dummy == NULL)
	goto loser;

    /* now we need to have a proper content encryption algorithm
     * on the SMIME level, we would figure one out by looking at SMIME capabilities
     * we cannot do that on our level, so if none is set already, we'll just go
     * with one of the mandatory algorithms (3DES) */
    if ((bulkalgtag = SecCmsContentInfoGetContentEncAlgTag(cinfo)) == SEC_OID_UNKNOWN) {
	rv = SecCmsContentInfoSetContentEncAlg((SecArenaPoolRef)poolp, cinfo, SEC_OID_DES_EDE3_CBC, NULL, 168);
	if (rv != SECSuccess)
	    goto loser;
	bulkalgtag = SEC_OID_DES_EDE3_CBC;
    }

    algorithm = SECOID_FindyCssmAlgorithmByTag(bulkalgtag);
    if (!algorithm)
	goto loser;
    rv = SecKeyGenerate(NULL,	/* keychainRef */
		algorithm,
		SecCmsContentInfoGetBulkKeySize(cinfo),
		0,		/* contextHandle */
		CSSM_KEYUSE_ENCRYPT | CSSM_KEYUSE_DECRYPT,
		CSSM_KEYATTR_EXTRACTABLE,
		NULL,		/* initialAccess */
		&bulkkey);
    if (rv)
	goto loser;

    mark = PORT_ArenaMark(poolp);

    /* Encrypt the bulk key with the public key of each recipient.  */
    for (i = 0; recipientinfos[i] != NULL; i++) {
	rv = SecCmsRecipientInfoWrapBulkKey(recipientinfos[i], bulkkey, bulkalgtag);
	if (rv != SECSuccess)
	    goto loser;	/* error has been set by SecCmsRecipientInfoEncryptBulkKey */
	    		/* could be: alg not supported etc. */
    }

    /* the recipientinfos are all finished. now sort them by DER for SET OF encoding */
    rv = SecCmsArraySortByDER((void **)envd->recipientInfos, SecCmsRecipientInfoTemplate, NULL);
    if (rv != SECSuccess)
	goto loser;	/* error has been set by SecCmsArraySortByDER */

    /* store the bulk key in the contentInfo so that the encoder can find it */
    SecCmsContentInfoSetBulkKey(cinfo, bulkkey);

    PORT_ArenaUnmark(poolp, mark);

    CFRelease(bulkkey);

    return SECSuccess;

loser:
    if (mark != NULL)
	PORT_ArenaRelease (poolp, mark);
    if (bulkkey)
	CFRelease(bulkkey);

    return SECFailure;
}

/*
 * SecCmsEnvelopedDataEncodeBeforeData - set up encryption
 *
 * it is essential that this is called before the contentEncAlg is encoded, because
 * setting up the encryption may generate IVs and thus change it!
 */
OSStatus
SecCmsEnvelopedDataEncodeBeforeData(SecCmsEnvelopedDataRef envd)
{
    SecCmsContentInfoRef cinfo;
    SecSymmetricKeyRef bulkkey;
    SECAlgorithmID *algid;

    cinfo = &(envd->contentInfo);

    /* find bulkkey and algorithm - must have been set by SecCmsEnvelopedDataEncodeBeforeStart */
    bulkkey = SecCmsContentInfoGetBulkKey(cinfo);
    if (bulkkey == NULL)
	return SECFailure;
    algid = SecCmsContentInfoGetContentEncAlg(cinfo);
    if (algid == NULL)
	return SECFailure;

    /* this may modify algid (with IVs generated in a token).
     * it is essential that algid is a pointer to the contentEncAlg data, not a
     * pointer to a copy! */
    cinfo->ciphcx = SecCmsCipherContextStartEncrypt(envd->cmsg->poolp, bulkkey, algid);
    CFRelease(bulkkey);
    if (cinfo->ciphcx == NULL)
	return SECFailure;

    return SECSuccess;
}

/*
 * SecCmsEnvelopedDataEncodeAfterData - finalize this envelopedData for encoding
 */
OSStatus
SecCmsEnvelopedDataEncodeAfterData(SecCmsEnvelopedDataRef envd)
{
    if (envd->contentInfo.ciphcx) {
	SecCmsCipherContextDestroy(envd->contentInfo.ciphcx);
	envd->contentInfo.ciphcx = NULL;
    }

    /* nothing else to do after data */
    return SECSuccess;
}

/*
 * SecCmsEnvelopedDataDecodeBeforeData - find our recipientinfo, 
 * derive bulk key & set up our contentinfo
 */
OSStatus
SecCmsEnvelopedDataDecodeBeforeData(SecCmsEnvelopedDataRef envd)
{
    SecCmsRecipientInfoRef ri;
    SecSymmetricKeyRef bulkkey = NULL;
    SECOidTag bulkalgtag;
    SECAlgorithmID *bulkalg;
    OSStatus rv = SECFailure;
    SecCmsContentInfoRef cinfo;
    SecCmsRecipient **recipient_list = NULL;
    SecCmsRecipient *recipient;
    int rlIndex;

    if (SecCmsArrayCount((void **)envd->recipientInfos) == 0) {
	PORT_SetError(SEC_ERROR_BAD_DATA);
#if 0
	PORT_SetErrorString("No recipient data in envelope.");
#endif
	goto loser;
    }

    /* look if one of OUR cert's issuerSN is on the list of recipients, and if so,  */
    /* get the cert and private key for it right away */
    recipient_list = nss_cms_recipient_list_create(envd->recipientInfos);
    if (recipient_list == NULL)
	goto loser;

    /* what about multiple recipientInfos that match?
     * especially if, for some reason, we could not produce a bulk key with the first match?!
     * we could loop & feed partial recipient_list to PK11_FindCertAndKeyByRecipientList...
     * maybe later... */
    rlIndex = nss_cms_FindCertAndKeyByRecipientList(recipient_list, envd->cmsg->pwfn_arg);

    /* if that fails, then we're not an intended recipient and cannot decrypt */
    if (rlIndex < 0) {
	PORT_SetError(SEC_ERROR_NOT_A_RECIPIENT);
#if 0
	PORT_SetErrorString("Cannot decrypt data because proper key cannot be found.");
#endif
	goto loser;
    }

    recipient = recipient_list[rlIndex];
    if (!recipient->cert || !recipient->privkey) {
	/* XXX should set an error code ?!? */
	goto loser;
    }
    /* get a pointer to "our" recipientinfo */
    ri = envd->recipientInfos[recipient->riIndex];

    cinfo = &(envd->contentInfo);
    bulkalgtag = SecCmsContentInfoGetContentEncAlgTag(cinfo);
    if (bulkalgtag == SEC_OID_UNKNOWN) {
	PORT_SetError(SEC_ERROR_INVALID_ALGORITHM);
    } else
	bulkkey = SecCmsRecipientInfoUnwrapBulkKey(ri,recipient->subIndex,
						    recipient->cert,
						    recipient->privkey,
						    bulkalgtag);
    if (bulkkey == NULL) {
	/* no success finding a bulk key */
	rv = errSecDataNotAvailable;
	goto loser;
    }

    SecCmsContentInfoSetBulkKey(cinfo, bulkkey);
    // @@@ See 3401088 for details.  We need to CFRelease cinfo->bulkkey before recipient->privkey gets CFReleased. It's created with SecKeyCreateWithCSSMKey which is not safe currently.  If the private key's SecKeyRef from which we extracted the CSP gets CFRelease before the builkkey does we crash.  We should really fix SecKeyCreateWithCSSMKey which is a huge hack currently.  To work around this we add recipient->privkey to the cinfo so it gets when cinfo is destroyed.
    CFRetain(recipient->privkey);
    cinfo->privkey = recipient->privkey;

    bulkalg = SecCmsContentInfoGetContentEncAlg(cinfo);

    cinfo->ciphcx = SecCmsCipherContextStartDecrypt(bulkkey, bulkalg);
    if (cinfo->ciphcx == NULL)
	goto loser;		/* error has been set by SecCmsCipherContextStartDecrypt */

#if 1
    // @@@ Fix me
#else
    /* 
     * HACK ALERT!!
     * For PKCS5 Encryption Algorithms, the bulkkey is actually a different
     * structure.  Therefore, we need to set the bulkkey to the actual key 
     * prior to freeing it.
     */
    if (SEC_PKCS5IsAlgorithmPBEAlg(bulkalg)) {
	SEC_PKCS5KeyAndPassword *keyPwd = (SEC_PKCS5KeyAndPassword *)bulkkey;
	bulkkey = keyPwd->key;
    }
#endif

    rv = SECSuccess;

loser:
    if (bulkkey)
	CFRelease(bulkkey);
    if (recipient_list != NULL)
	nss_cms_recipient_list_destroy(recipient_list);
    return rv;
}

/*
 * SecCmsEnvelopedDataDecodeAfterData - finish decrypting this envelopedData's content
 */
OSStatus
SecCmsEnvelopedDataDecodeAfterData(SecCmsEnvelopedDataRef envd)
{
    if (envd && envd->contentInfo.ciphcx) {
	SecCmsCipherContextDestroy(envd->contentInfo.ciphcx);
	envd->contentInfo.ciphcx = NULL;
    }

    return SECSuccess;
}

/*
 * SecCmsEnvelopedDataDecodeAfterEnd - finish decoding this envelopedData
 */
OSStatus
SecCmsEnvelopedDataDecodeAfterEnd(SecCmsEnvelopedDataRef envd)
{
    /* apply final touches */
    return SECSuccess;
}

