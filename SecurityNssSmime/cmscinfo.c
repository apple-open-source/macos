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
 * CMS contentInfo methods.
 */

#include "cmslocal.h"

//#include "pk11func.h"
#include "secoid.h"
#include "secerr.h"
#include "secitem.h"

/*
 * SecCmsContentInfoCreate - create a content info
 *
 * version is set in the _Finalize procedures for each content type
 */

/*
 * SecCmsContentInfoDestroy - destroy a CMS contentInfo and all of its sub-pieces.
 */
void
SecCmsContentInfoDestroy(SecCmsContentInfo *cinfo)
{
    SECOidTag kind;

    kind = SecCmsContentInfoGetContentTypeTag(cinfo);
    switch (kind) {
    case SEC_OID_PKCS7_ENVELOPED_DATA:
	SecCmsEnvelopedDataDestroy(cinfo->content.envelopedData);
	break;
      case SEC_OID_PKCS7_SIGNED_DATA:
	SecCmsSignedDataDestroy(cinfo->content.signedData);
	break;
      case SEC_OID_PKCS7_ENCRYPTED_DATA:
	SecCmsEncryptedDataDestroy(cinfo->content.encryptedData);
	break;
      case SEC_OID_PKCS7_DIGESTED_DATA:
	SecCmsDigestedDataDestroy(cinfo->content.digestedData);
	break;
      default:
	/* XXX Anything else that needs to be "manually" freed/destroyed? */
	break;
    }
    if (cinfo->bulkkey)
	CFRelease(cinfo->bulkkey);
    /* @@@ private key is only here as a workaround for 3401088.  Note this *must* be released after bulkkey */
    if (cinfo->privkey)
	CFRelease(cinfo->privkey);

    if (cinfo->ciphcx) {
	SecCmsCipherContextDestroy(cinfo->ciphcx);
	cinfo->ciphcx = NULL;
    }
    
    /* we live in a pool, so no need to worry about storage */
}

/*
 * SecCmsContentInfoGetChildContentInfo - get content's contentInfo (if it exists)
 */
SecCmsContentInfo *
SecCmsContentInfoGetChildContentInfo(SecCmsContentInfo *cinfo)
{
    switch (SecCmsContentInfoGetContentTypeTag(cinfo)) {
    case SEC_OID_PKCS7_DATA:
	return NULL;
    case SEC_OID_PKCS7_SIGNED_DATA:
	return &(cinfo->content.signedData->contentInfo);
    case SEC_OID_PKCS7_ENVELOPED_DATA:
	return &(cinfo->content.envelopedData->contentInfo);
    case SEC_OID_PKCS7_DIGESTED_DATA:
	return &(cinfo->content.digestedData->contentInfo);
    case SEC_OID_PKCS7_ENCRYPTED_DATA:
	return &(cinfo->content.encryptedData->contentInfo);
    default:
	return NULL;
    }
}

/*
 * SecCmsContentInfoSetContent - set content type & content
 */
OSStatus
SecCmsContentInfoSetContent(SecCmsMessage *cmsg, SecCmsContentInfo *cinfo, SECOidTag type, void *ptr)
{
    OSStatus rv;

    cinfo->contentTypeTag = SECOID_FindOIDByTag(type);
    if (cinfo->contentTypeTag == NULL)
	return SECFailure;
    
    /* do not copy the oid, just create a reference */
    rv = SECITEM_CopyItem (cmsg->poolp, &(cinfo->contentType), &(cinfo->contentTypeTag->oid));
    if (rv != SECSuccess)
	return SECFailure;

    cinfo->content.pointer = ptr;

    if (type != SEC_OID_PKCS7_DATA) {
	/* as we always have some inner data,
	 * we need to set it to something, just to fool the encoder enough to work on it
	 * and get us into nss_cms_encoder_notify at that point */
	cinfo->rawContent = SECITEM_AllocItem(cmsg->poolp, NULL, 1);
	if (cinfo->rawContent == NULL) {
	    PORT_SetError(SEC_ERROR_NO_MEMORY);
	    return SECFailure;
	}
    }

    return SECSuccess;
}

/*
 * SecCmsContentInfoSetContentXXXX - typesafe wrappers for SecCmsContentInfoSetContent
 */

/*
 * data == NULL -> pass in data via SecCmsEncoderUpdate
 * data != NULL -> take this data
 */
OSStatus
SecCmsContentInfoSetContentData(SecCmsMessage *cmsg, SecCmsContentInfo *cinfo, CSSM_DATA *data, PRBool detached)
{
    if (SecCmsContentInfoSetContent(cmsg, cinfo, SEC_OID_PKCS7_DATA, (void *)data) != SECSuccess)
	return SECFailure;
    cinfo->rawContent = (detached) ? 
			    NULL : (data) ? 
				data : SECITEM_AllocItem(cmsg->poolp, NULL, 1);
    return SECSuccess;
}

OSStatus
SecCmsContentInfoSetContentSignedData(SecCmsMessage *cmsg, SecCmsContentInfo *cinfo, SecCmsSignedData *sigd)
{
    return SecCmsContentInfoSetContent(cmsg, cinfo, SEC_OID_PKCS7_SIGNED_DATA, (void *)sigd);
}

OSStatus
SecCmsContentInfoSetContentEnvelopedData(SecCmsMessage *cmsg, SecCmsContentInfo *cinfo, SecCmsEnvelopedData *envd)
{
    return SecCmsContentInfoSetContent(cmsg, cinfo, SEC_OID_PKCS7_ENVELOPED_DATA, (void *)envd);
}

OSStatus
SecCmsContentInfoSetContentDigestedData(SecCmsMessage *cmsg, SecCmsContentInfo *cinfo, SecCmsDigestedData *digd)
{
    return SecCmsContentInfoSetContent(cmsg, cinfo, SEC_OID_PKCS7_DIGESTED_DATA, (void *)digd);
}

OSStatus
SecCmsContentInfoSetContentEncryptedData(SecCmsMessage *cmsg, SecCmsContentInfo *cinfo, SecCmsEncryptedData *encd)
{
    return SecCmsContentInfoSetContent(cmsg, cinfo, SEC_OID_PKCS7_ENCRYPTED_DATA, (void *)encd);
}

/*
 * SecCmsContentInfoGetContent - get pointer to inner content
 *
 * needs to be casted...
 */
void *
SecCmsContentInfoGetContent(SecCmsContentInfo *cinfo)
{
    switch (cinfo->contentTypeTag->offset) {
    case SEC_OID_PKCS7_DATA:
    case SEC_OID_PKCS7_SIGNED_DATA:
    case SEC_OID_PKCS7_ENVELOPED_DATA:
    case SEC_OID_PKCS7_DIGESTED_DATA:
    case SEC_OID_PKCS7_ENCRYPTED_DATA:
	return cinfo->content.pointer;
    default:
	return NULL;
    }
}

/* 
 * SecCmsContentInfoGetInnerContent - get pointer to innermost content
 *
 * this is typically only called by SecCmsMessageGetContent()
 */
CSSM_DATA *
SecCmsContentInfoGetInnerContent(SecCmsContentInfo *cinfo)
{
    SecCmsContentInfo *ccinfo;

    switch (SecCmsContentInfoGetContentTypeTag(cinfo)) {
    case SEC_OID_PKCS7_DATA:
	return cinfo->content.data;	/* end of recursion - every message has to have a data cinfo */
    case SEC_OID_PKCS7_DIGESTED_DATA:
    case SEC_OID_PKCS7_ENCRYPTED_DATA:
    case SEC_OID_PKCS7_ENVELOPED_DATA:
    case SEC_OID_PKCS7_SIGNED_DATA:
	if ((ccinfo = SecCmsContentInfoGetChildContentInfo(cinfo)) == NULL)
	    break;
	return SecCmsContentInfoGetContent(ccinfo);
    default:
	PORT_Assert(0);
	break;
    }
    return NULL;
}

/*
 * SecCmsContentInfoGetContentType{Tag,OID} - find out (saving pointer to lookup result
 * for future reference) and return the inner content type.
 */
SECOidTag
SecCmsContentInfoGetContentTypeTag(SecCmsContentInfo *cinfo)
{
    if (cinfo->contentTypeTag == NULL)
	cinfo->contentTypeTag = SECOID_FindOID(&(cinfo->contentType));

    if (cinfo->contentTypeTag == NULL)
	return SEC_OID_UNKNOWN;

    return cinfo->contentTypeTag->offset;
}

CSSM_DATA *
SecCmsContentInfoGetContentTypeOID(SecCmsContentInfo *cinfo)
{
    if (cinfo->contentTypeTag == NULL)
	cinfo->contentTypeTag = SECOID_FindOID(&(cinfo->contentType));

    if (cinfo->contentTypeTag == NULL)
	return NULL;

    return &(cinfo->contentTypeTag->oid);
}

/*
 * SecCmsContentInfoGetContentEncAlgTag - find out (saving pointer to lookup result
 * for future reference) and return the content encryption algorithm tag.
 */
SECOidTag
SecCmsContentInfoGetContentEncAlgTag(SecCmsContentInfo *cinfo)
{
    if (cinfo->contentEncAlgTag == SEC_OID_UNKNOWN)
	cinfo->contentEncAlgTag = SECOID_GetAlgorithmTag(&(cinfo->contentEncAlg));

    return cinfo->contentEncAlgTag;
}

/*
 * SecCmsContentInfoGetContentEncAlg - find out and return the content encryption algorithm tag.
 */
SECAlgorithmID *
SecCmsContentInfoGetContentEncAlg(SecCmsContentInfo *cinfo)
{
    return &(cinfo->contentEncAlg);
}

OSStatus
SecCmsContentInfoSetContentEncAlg(PLArenaPool *poolp, SecCmsContentInfo *cinfo,
				    SECOidTag bulkalgtag, CSSM_DATA *parameters, int keysize)
{
    OSStatus rv;

    rv = SECOID_SetAlgorithmID(poolp, &(cinfo->contentEncAlg), bulkalgtag, parameters);
    if (rv != SECSuccess)
	return SECFailure;
    cinfo->keysize = keysize;
    return SECSuccess;
}

OSStatus
SecCmsContentInfoSetContentEncAlgID(PLArenaPool *poolp, SecCmsContentInfo *cinfo,
				    SECAlgorithmID *algid, int keysize)
{
    OSStatus rv;

    rv = SECOID_CopyAlgorithmID(poolp, &(cinfo->contentEncAlg), algid);
    if (rv != SECSuccess)
	return SECFailure;
    if (keysize >= 0)
	cinfo->keysize = keysize;
    return SECSuccess;
}

void
SecCmsContentInfoSetBulkKey(SecCmsContentInfo *cinfo, SecSymmetricKeyRef bulkkey)
{
    const CSSM_KEY *cssmKey = NULL;

    cinfo->bulkkey = bulkkey;
    CFRetain(cinfo->bulkkey);
    SecKeyGetCSSMKey(cinfo->bulkkey, &cssmKey);
    cinfo->keysize = cssmKey ? cssmKey->KeyHeader.LogicalKeySizeInBits : 0;
}

SecSymmetricKeyRef
SecCmsContentInfoGetBulkKey(SecCmsContentInfo *cinfo)
{
    if (cinfo->bulkkey == NULL)
	return NULL;

    CFRetain(cinfo->bulkkey);
    return cinfo->bulkkey;
}

int
SecCmsContentInfoGetBulkKeySize(SecCmsContentInfo *cinfo)
{
    return cinfo->keysize;
}
