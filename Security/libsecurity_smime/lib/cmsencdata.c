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
 * CMS encryptedData methods.
 */

#include <Security/SecCmsEncryptedData.h>

#include <Security/SecCmsContentInfo.h>

#include "cmslocal.h"

#include "secitem.h"
#include "secoid.h"
#include <security_asn1/secasn1.h>
#include <security_asn1/secerr.h>

/*
 * SecCmsEncryptedDataCreate - create an empty encryptedData object.
 *
 * "algorithm" specifies the bulk encryption algorithm to use.
 * "keysize" is the key size.
 * 
 * An error results in a return value of NULL and an error set.
 * (Retrieve specific errors via PORT_GetError()/XP_GetError().)
 */
SecCmsEncryptedDataRef
SecCmsEncryptedDataCreate(SecCmsMessageRef cmsg, SECOidTag algorithm, int keysize)
{
    void *mark;
    SecCmsEncryptedDataRef encd;
    PLArenaPool *poolp;
    SECAlgorithmID *pbe_algid;
    OSStatus rv;

    poolp = cmsg->poolp;

    mark = PORT_ArenaMark(poolp);

    encd = (SecCmsEncryptedDataRef)PORT_ArenaZAlloc(poolp, sizeof(SecCmsEncryptedData));
    if (encd == NULL)
	goto loser;

    encd->cmsg = cmsg;

    /* version is set in SecCmsEncryptedDataEncodeBeforeStart() */

    switch (algorithm) {
    /* XXX hmmm... hardcoded algorithms? */
    case SEC_OID_RC2_CBC:
    case SEC_OID_DES_EDE3_CBC:
    case SEC_OID_DES_CBC:
	rv = SecCmsContentInfoSetContentEncAlg((SecArenaPoolRef)poolp, &(encd->contentInfo), algorithm, NULL, keysize);
	break;
    default:
	/* Assume password-based-encryption.  At least, try that. */
#if 1
	// @@@ Fix me
	pbe_algid = NULL;
#else
	pbe_algid = PK11_CreatePBEAlgorithmID(algorithm, 1, NULL);
#endif
	if (pbe_algid == NULL) {
	    rv = SECFailure;
	    break;
	}
	rv = SecCmsContentInfoSetContentEncAlgID((SecArenaPoolRef)poolp, &(encd->contentInfo), pbe_algid, keysize);
	SECOID_DestroyAlgorithmID (pbe_algid, PR_TRUE);
	break;
    }
    if (rv != SECSuccess)
	goto loser;

    PORT_ArenaUnmark(poolp, mark);
    return encd;

loser:
    PORT_ArenaRelease(poolp, mark);
    return NULL;
}

/*
 * SecCmsEncryptedDataDestroy - destroy an encryptedData object
 */
void
SecCmsEncryptedDataDestroy(SecCmsEncryptedDataRef encd)
{
    /* everything's in a pool, so don't worry about the storage */
    SecCmsContentInfoDestroy(&(encd->contentInfo));
    return;
}

/*
 * SecCmsEncryptedDataGetContentInfo - return pointer to encryptedData object's contentInfo
 */
SecCmsContentInfoRef
SecCmsEncryptedDataGetContentInfo(SecCmsEncryptedDataRef encd)
{
    return &(encd->contentInfo);
}

/*
 * SecCmsEncryptedDataEncodeBeforeStart - do all the necessary things to a EncryptedData
 *     before encoding begins.
 *
 * In particular:
 *  - set the correct version value.
 *  - get the encryption key
 */
OSStatus
SecCmsEncryptedDataEncodeBeforeStart(SecCmsEncryptedDataRef encd)
{
    int version;
    SecSymmetricKeyRef bulkkey = NULL;
    CSSM_DATA_PTR dummy;
    SecCmsContentInfoRef cinfo = &(encd->contentInfo);

    if (SecCmsArrayIsEmpty((void **)encd->unprotectedAttr))
	version = SEC_CMS_ENCRYPTED_DATA_VERSION;
    else
	version = SEC_CMS_ENCRYPTED_DATA_VERSION_UPATTR;
    
    dummy = SEC_ASN1EncodeInteger (encd->cmsg->poolp, &(encd->version), version);
    if (dummy == NULL)
	return SECFailure;

    /* now get content encryption key (bulk key) by using our cmsg callback */
    if (encd->cmsg->decrypt_key_cb)
	bulkkey = (*encd->cmsg->decrypt_key_cb)(encd->cmsg->decrypt_key_cb_arg, 
		    SecCmsContentInfoGetContentEncAlg(cinfo));
    if (bulkkey == NULL)
	return SECFailure;

    /* store the bulk key in the contentInfo so that the encoder can find it */
    SecCmsContentInfoSetBulkKey(cinfo, bulkkey);
    CFRelease(bulkkey); /* This assumes the decrypt_key_cb hands us a copy of the key --mb */

    return SECSuccess;
}

/*
 * SecCmsEncryptedDataEncodeBeforeData - set up encryption
 */
OSStatus
SecCmsEncryptedDataEncodeBeforeData(SecCmsEncryptedDataRef encd)
{
    SecCmsContentInfoRef cinfo;
    SecSymmetricKeyRef bulkkey;
    SECAlgorithmID *algid;

    cinfo = &(encd->contentInfo);

    /* find bulkkey and algorithm - must have been set by SecCmsEncryptedDataEncodeBeforeStart */
    bulkkey = SecCmsContentInfoGetBulkKey(cinfo);
    if (bulkkey == NULL)
	return SECFailure;
    algid = SecCmsContentInfoGetContentEncAlg(cinfo);
    if (algid == NULL)
	return SECFailure;

    /* this may modify algid (with IVs generated in a token).
     * it is therefore essential that algid is a pointer to the "real" contentEncAlg,
     * not just to a copy */
    cinfo->ciphcx = SecCmsCipherContextStartEncrypt(encd->cmsg->poolp, bulkkey, algid);
    CFRelease(bulkkey);
    if (cinfo->ciphcx == NULL)
	return SECFailure;

    return SECSuccess;
}

/*
 * SecCmsEncryptedDataEncodeAfterData - finalize this encryptedData for encoding
 */
OSStatus
SecCmsEncryptedDataEncodeAfterData(SecCmsEncryptedDataRef encd)
{
    if (encd->contentInfo.ciphcx) {
	SecCmsCipherContextDestroy(encd->contentInfo.ciphcx);
	encd->contentInfo.ciphcx = NULL;
    }

    /* nothing to do after data */
    return SECSuccess;
}


/*
 * SecCmsEncryptedDataDecodeBeforeData - find bulk key & set up decryption
 */
OSStatus
SecCmsEncryptedDataDecodeBeforeData(SecCmsEncryptedDataRef encd)
{
    SecSymmetricKeyRef bulkkey = NULL;
    SecCmsContentInfoRef cinfo;
    SECAlgorithmID *bulkalg;
    OSStatus rv = SECFailure;

    cinfo = &(encd->contentInfo);

    bulkalg = SecCmsContentInfoGetContentEncAlg(cinfo);

    if (encd->cmsg->decrypt_key_cb == NULL)	/* no callback? no key../ */
	goto loser;

    bulkkey = (*encd->cmsg->decrypt_key_cb)(encd->cmsg->decrypt_key_cb_arg, bulkalg);
    if (bulkkey == NULL)
	/* no success finding a bulk key */
	goto loser;

    SecCmsContentInfoSetBulkKey(cinfo, bulkkey);

    cinfo->ciphcx = SecCmsCipherContextStartDecrypt(bulkkey, bulkalg);
    if (cinfo->ciphcx == NULL)
	goto loser;		/* error has been set by SecCmsCipherContextStartDecrypt */

#if 1
    // @@@ Not done yet
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

    /* we are done with (this) bulkkey now. */
    CFRelease(bulkkey);

    rv = SECSuccess;

loser:
    return rv;
}

/*
 * SecCmsEncryptedDataDecodeAfterData - finish decrypting this encryptedData's content
 */
OSStatus
SecCmsEncryptedDataDecodeAfterData(SecCmsEncryptedDataRef encd)
{
    if (encd->contentInfo.ciphcx) {
	SecCmsCipherContextDestroy(encd->contentInfo.ciphcx);
	encd->contentInfo.ciphcx = NULL;
    }

    return SECSuccess;
}

/*
 * SecCmsEncryptedDataDecodeAfterEnd - finish decoding this encryptedData
 */
OSStatus
SecCmsEncryptedDataDecodeAfterEnd(SecCmsEncryptedDataRef encd)
{
    /* apply final touches */
    return SECSuccess;
}
