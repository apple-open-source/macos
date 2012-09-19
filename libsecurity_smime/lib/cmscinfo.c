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

#include <Security/SecCmsContentInfo.h>

#include <Security/SecCmsDigestContext.h>
#include <Security/SecCmsDigestedData.h>
#include <Security/SecCmsEncryptedData.h>
#include <Security/SecCmsEnvelopedData.h>
#include <Security/SecCmsSignedData.h>

#include "cmslocal.h"

//#include "pk11func.h"
#include "secoid.h"
#include "secitem.h"

#include <security_asn1/secerr.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>

/*
 * SecCmsContentInfoCreate - create a content info
 *
 * version is set in the _Finalize procedures for each content type
 */

/*
 * SecCmsContentInfoDestroy - destroy a CMS contentInfo and all of its sub-pieces.
 */
void
SecCmsContentInfoDestroy(SecCmsContentInfoRef cinfo)
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
    if (cinfo->digcx) {
	/* must destroy digest objects */
	SecCmsDigestContextCancel(cinfo->digcx);
	cinfo->digcx = NULL;
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
SecCmsContentInfoRef
SecCmsContentInfoGetChildContentInfo(SecCmsContentInfoRef cinfo)
{
    void *ptr = NULL;
    SecCmsContentInfoRef ccinfo = NULL;
    SECOidTag tag = SecCmsContentInfoGetContentTypeTag(cinfo);
    switch (tag) {
    case SEC_OID_PKCS7_SIGNED_DATA:
	ptr = (void *)cinfo->content.signedData;
	ccinfo = &(cinfo->content.signedData->contentInfo);
	break;
    case SEC_OID_PKCS7_ENVELOPED_DATA:
	ptr = (void *)cinfo->content.envelopedData;
	ccinfo = &(cinfo->content.envelopedData->contentInfo);
	break;
    case SEC_OID_PKCS7_DIGESTED_DATA:
	ptr = (void *)cinfo->content.digestedData;
	ccinfo = &(cinfo->content.digestedData->contentInfo);
	break;
    case SEC_OID_PKCS7_ENCRYPTED_DATA:
	ptr = (void *)cinfo->content.encryptedData;
	ccinfo = &(cinfo->content.encryptedData->contentInfo);
	break;
    case SEC_OID_PKCS7_DATA:
    case SEC_OID_OTHER:
    default:
	break;
    }
    return (ptr ? ccinfo : NULL);
}

/*
 * SecCmsContentInfoSetContent - set content type & content
 */
OSStatus
SecCmsContentInfoSetContent(SecCmsMessageRef cmsg, SecCmsContentInfoRef cinfo, SECOidTag type, void *ptr)
{
    OSStatus rv;

    cinfo->contentTypeTag = SECOID_FindOIDByTag(type);
    if (cinfo->contentTypeTag == NULL)
	return paramErr;
    
    /* do not copy the oid, just create a reference */
    rv = SECITEM_CopyItem (cmsg->poolp, &(cinfo->contentType), &(cinfo->contentTypeTag->oid));
    if (rv != SECSuccess)
	return memFullErr;

    cinfo->content.pointer = ptr;

    if (type != SEC_OID_PKCS7_DATA) {
	/* as we always have some inner data,
	 * we need to set it to something, just to fool the encoder enough to work on it
	 * and get us into nss_cms_encoder_notify at that point */
	cinfo->rawContent = SECITEM_AllocItem(cmsg->poolp, NULL, 1);
	if (cinfo->rawContent == NULL) {
	    PORT_SetError(SEC_ERROR_NO_MEMORY);
	    return memFullErr;
	}
    }

    return noErr;
}

/*
 * SecCmsContentInfoSetContentXXXX - typesafe wrappers for SecCmsContentInfoSetContent
 */

/*
 * data == NULL -> pass in data via SecCmsEncoderUpdate
 * data != NULL -> take this data
 */
OSStatus
SecCmsContentInfoSetContentData(SecCmsMessageRef cmsg, SecCmsContentInfoRef cinfo, CSSM_DATA_PTR data, Boolean detached)
{
    if (SecCmsContentInfoSetContent(cmsg, cinfo, SEC_OID_PKCS7_DATA, (void *)data) != SECSuccess)
	return PORT_GetError();
    cinfo->rawContent = (detached) ? 
			    NULL : (data) ? 
				data : SECITEM_AllocItem(cmsg->poolp, NULL, 1);
    return noErr;
}

OSStatus
SecCmsContentInfoSetContentSignedData(SecCmsMessageRef cmsg, SecCmsContentInfoRef cinfo, SecCmsSignedDataRef sigd)
{
    return SecCmsContentInfoSetContent(cmsg, cinfo, SEC_OID_PKCS7_SIGNED_DATA, (void *)sigd);
}

OSStatus
SecCmsContentInfoSetContentEnvelopedData(SecCmsMessageRef cmsg, SecCmsContentInfoRef cinfo, SecCmsEnvelopedDataRef envd)
{
    return SecCmsContentInfoSetContent(cmsg, cinfo, SEC_OID_PKCS7_ENVELOPED_DATA, (void *)envd);
}

OSStatus
SecCmsContentInfoSetContentDigestedData(SecCmsMessageRef cmsg, SecCmsContentInfoRef cinfo, SecCmsDigestedDataRef digd)
{
    return SecCmsContentInfoSetContent(cmsg, cinfo, SEC_OID_PKCS7_DIGESTED_DATA, (void *)digd);
}

OSStatus
SecCmsContentInfoSetContentEncryptedData(SecCmsMessageRef cmsg, SecCmsContentInfoRef cinfo, SecCmsEncryptedDataRef encd)
{
    return SecCmsContentInfoSetContent(cmsg, cinfo, SEC_OID_PKCS7_ENCRYPTED_DATA, (void *)encd);
}

OSStatus
SecCmsContentInfoSetContentOther(SecCmsMessageRef cmsg, SecCmsContentInfoRef cinfo, CSSM_DATA_PTR data, Boolean detached, const CSSM_OID *eContentType)
{
    SECStatus srtn;
    SECOidData *tmpOidData;
    
    /* just like SecCmsContentInfoSetContentData, except override the contentType and
     * contentTypeTag. This OID is for encoding... */
    srtn = SECITEM_CopyItem (cmsg->poolp, &(cinfo->contentType), eContentType);
    if (srtn != SECSuccess) {
	return memFullErr;
    }
    
    /* this serves up a contentTypeTag with an empty OID */
    tmpOidData = SECOID_FindOIDByTag(SEC_OID_OTHER);
    /* but that's const: cook up a new one we can write to */
    cinfo->contentTypeTag = (SECOidData *)PORT_ArenaZAlloc(cmsg->poolp, sizeof(SECOidData));
    *cinfo->contentTypeTag = *tmpOidData;
    /* now fill in the OID */
    srtn = SECITEM_CopyItem (cmsg->poolp, &(cinfo->contentTypeTag->oid), eContentType);
    if (srtn != SECSuccess) {
	return memFullErr;
    }
    cinfo->content.pointer = data;
    cinfo->rawContent = (detached) ? 
			    NULL : (data) ? 
				data : SECITEM_AllocItem(cmsg->poolp, NULL, 1);
    return noErr;
}


/*
 * SecCmsContentInfoGetContent - get pointer to inner content
 *
 * needs to be casted...
 */
void *
SecCmsContentInfoGetContent(SecCmsContentInfoRef cinfo)
{
    SECOidTag tag = (cinfo && cinfo->contentTypeTag) 
		     ? cinfo->contentTypeTag->offset 
		     : cinfo->contentType.Data ? SEC_OID_OTHER : SEC_OID_UNKNOWN;
    switch (tag) {
    case SEC_OID_PKCS7_DATA:
    case SEC_OID_PKCS7_SIGNED_DATA:
    case SEC_OID_PKCS7_ENVELOPED_DATA:
    case SEC_OID_PKCS7_DIGESTED_DATA:
    case SEC_OID_PKCS7_ENCRYPTED_DATA:
    case SEC_OID_OTHER:
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
CSSM_DATA_PTR
SecCmsContentInfoGetInnerContent(SecCmsContentInfoRef cinfo)
{
    SECOidTag tag;

    for(;;) {
	tag = SecCmsContentInfoGetContentTypeTag(cinfo);
	switch (tag) {
	case SEC_OID_PKCS7_DATA:
	case SEC_OID_OTHER:
	    /* end of recursion - every message has to have a data cinfo */
	    return cinfo->content.data; 
	case SEC_OID_PKCS7_DIGESTED_DATA:
	case SEC_OID_PKCS7_ENCRYPTED_DATA:
	case SEC_OID_PKCS7_ENVELOPED_DATA:
	case SEC_OID_PKCS7_SIGNED_DATA:
            cinfo = SecCmsContentInfoGetChildContentInfo(cinfo);
	    if (cinfo == NULL) {
		return NULL;
	    }
	    /* else recurse */
	    break;
        case SEC_OID_PKCS9_ID_CT_TSTInfo:
	    /* end of recursion - every message has to have a data cinfo */
	    return cinfo->rawContent; 
	default:
	    PORT_Assert(0);
	    return NULL;
	}
    }
    /* NOT REACHED */
    return NULL;
}

/*
 * SecCmsContentInfoGetContentType{Tag,OID} - find out (saving pointer to lookup result
 * for future reference) and return the inner content type.
 */
SECOidTag
SecCmsContentInfoGetContentTypeTag(SecCmsContentInfoRef cinfo)
{
    if (cinfo->contentTypeTag == NULL)
	cinfo->contentTypeTag = SECOID_FindOID(&(cinfo->contentType));

    if (cinfo->contentTypeTag == NULL)
	return SEC_OID_OTHER;	// was...SEC_OID_UNKNOWN OK?

    return cinfo->contentTypeTag->offset;
}

CSSM_DATA_PTR
SecCmsContentInfoGetContentTypeOID(SecCmsContentInfoRef cinfo)
{
    if (cinfo->contentTypeTag == NULL)
	cinfo->contentTypeTag = SECOID_FindOID(&(cinfo->contentType));

    if (cinfo->contentTypeTag == NULL) {
	/* if we have an OID but we just don't recognize it, return that */
	if(cinfo->contentType.Data != NULL) {
	    return &cinfo->contentType;
	}
	else {
	    return NULL;
	}
    }
    return &(cinfo->contentTypeTag->oid);
}

/*
 * SecCmsContentInfoGetContentEncAlgTag - find out (saving pointer to lookup result
 * for future reference) and return the content encryption algorithm tag.
 */
SECOidTag
SecCmsContentInfoGetContentEncAlgTag(SecCmsContentInfoRef cinfo)
{
    if (cinfo->contentEncAlgTag == SEC_OID_UNKNOWN)
	cinfo->contentEncAlgTag = SECOID_GetAlgorithmTag(&(cinfo->contentEncAlg));

    return cinfo->contentEncAlgTag;
}

/*
 * SecCmsContentInfoGetContentEncAlg - find out and return the content encryption algorithm tag.
 */
SECAlgorithmID *
SecCmsContentInfoGetContentEncAlg(SecCmsContentInfoRef cinfo)
{
    return &(cinfo->contentEncAlg);
}

OSStatus
SecCmsContentInfoSetContentEncAlg(SecArenaPoolRef pool, SecCmsContentInfoRef cinfo,
				    SECOidTag bulkalgtag, CSSM_DATA_PTR parameters, int keysize)
{
    PLArenaPool *poolp = (PLArenaPool *)pool;
    OSStatus rv;

    rv = SECOID_SetAlgorithmID(poolp, &(cinfo->contentEncAlg), bulkalgtag, parameters);
    if (rv != SECSuccess)
	return SECFailure;
    cinfo->keysize = keysize;
    return SECSuccess;
}

OSStatus
SecCmsContentInfoSetContentEncAlgID(SecArenaPoolRef pool, SecCmsContentInfoRef cinfo,
				    SECAlgorithmID *algid, int keysize)
{
    PLArenaPool *poolp = (PLArenaPool *)pool;
    OSStatus rv;

    rv = SECOID_CopyAlgorithmID(poolp, &(cinfo->contentEncAlg), algid);
    if (rv != SECSuccess)
	return SECFailure;
    if (keysize >= 0)
	cinfo->keysize = keysize;
    return SECSuccess;
}

void
SecCmsContentInfoSetBulkKey(SecCmsContentInfoRef cinfo, SecSymmetricKeyRef bulkkey)
{
    const CSSM_KEY *cssmKey = NULL;

    cinfo->bulkkey = bulkkey;
    CFRetain(cinfo->bulkkey);
    SecKeyGetCSSMKey(cinfo->bulkkey, &cssmKey);
    cinfo->keysize = cssmKey ? cssmKey->KeyHeader.LogicalKeySizeInBits : 0;
}

SecSymmetricKeyRef
SecCmsContentInfoGetBulkKey(SecCmsContentInfoRef cinfo)
{
    if (cinfo->bulkkey == NULL)
	return NULL;

    CFRetain(cinfo->bulkkey);
    return cinfo->bulkkey;
}

int
SecCmsContentInfoGetBulkKeySize(SecCmsContentInfoRef cinfo)
{
    return cinfo->keysize;
}
