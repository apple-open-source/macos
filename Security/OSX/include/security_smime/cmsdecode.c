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
 * CMS decoding.
 */

#include <Security/SecCmsDecoder.h>
#include <Security/SecCmsContentInfo.h>
#include <Security/SecCmsDigestContext.h>
#include <Security/SecCmsMessage.h>

#include "cmslocal.h"

#include "secitem.h"
#include "secoid.h"
#include <security_asn1/secasn1.h>
#include <security_asn1/secerr.h>

struct SecCmsDecoderStr {
    SEC_ASN1DecoderContext *		dcx;		/* ASN.1 decoder context */
    SecCmsMessageRef 			cmsg;		/* backpointer to the root message */
    SECOidTag				type;		/* type of message */
    SecCmsContent			content;	/* pointer to message */
    SecCmsDecoderRef 			childp7dcx;	/* inner CMS decoder context */
    Boolean				saw_contents;
    int					error;
    SecCmsContentCallback		cb;
    void *				cb_arg;
};

static void nss_cms_decoder_update_filter (void *arg, const char *data, size_t len,
                          int depth, SEC_ASN1EncodingPart data_kind);
static OSStatus nss_cms_before_data(SecCmsDecoderRef p7dcx);
static OSStatus nss_cms_after_data(SecCmsDecoderRef p7dcx);
static OSStatus nss_cms_after_end(SecCmsDecoderRef p7dcx);
static void nss_cms_decoder_work_data(SecCmsDecoderRef p7dcx, 
			     const unsigned char *data, size_t len, Boolean final);

extern const SecAsn1Template SecCmsMessageTemplate[];

/* 
 * nss_cms_decoder_notify -
 *  this is the driver of the decoding process. It gets called by the ASN.1
 *  decoder before and after an object is decoded.
 *  at various points in the decoding process, we intercept to set up and do
 *  further processing.
 */
static void
nss_cms_decoder_notify(void *arg, Boolean before, void *dest, int depth)
{
    SecCmsDecoderRef p7dcx;
    SecCmsContentInfoRef rootcinfo, cinfo;
    Boolean after = !before;

    p7dcx = (SecCmsDecoderRef)arg;
    rootcinfo = &(p7dcx->cmsg->contentInfo);

    /* XXX error handling: need to set p7dcx->error */

#ifdef CMSDEBUG 
    fprintf(stderr, "%6.6s, dest = %p, depth = %d\n", before ? "before" : "after", dest, depth);
#endif

    /* so what are we working on right now? */
    switch (p7dcx->type) {
    case SEC_OID_UNKNOWN:
	/*
	 * right now, we are still decoding the OUTER (root) cinfo
	 * As soon as we know the inner content type, set up the info,
	 * but NO inner decoder or filter. The root decoder handles the first
	 * level children by itself - only for encapsulated contents (which
	 * are encoded as DER inside of an OCTET STRING) we need to set up a
	 * child decoder...
	 */
	if (after && dest == &(rootcinfo->contentType)) {
	    p7dcx->type = SecCmsContentInfoGetContentTypeTag(rootcinfo);
	    p7dcx->content = rootcinfo->content;	/* is this ready already ? need to alloc? */
	    /* XXX yes we need to alloc -- continue here */
	}
	break;
    case SEC_OID_PKCS7_DATA:
    case SEC_OID_OTHER:
	/* this can only happen if the outermost cinfo has DATA in it */
	/* otherwise, we handle this type implicitely in the inner decoders */

	if (before && dest == &(rootcinfo->content)) {
	    /* fake it to cause the filter to put the data in the right place... */
	    /* we want the ASN.1 decoder to deliver the decoded bytes to us from now on */
	    SEC_ASN1DecoderSetFilterProc(p7dcx->dcx,
					  nss_cms_decoder_update_filter,
					  p7dcx,
					  (Boolean)(p7dcx->cb != NULL));
	    break;
	}

	if (after && dest == &(rootcinfo->content.data)) {
	    /* remove the filter */
	    SEC_ASN1DecoderClearFilterProc(p7dcx->dcx);
	}
	break;

    case SEC_OID_PKCS7_SIGNED_DATA:
    case SEC_OID_PKCS7_ENVELOPED_DATA:
    case SEC_OID_PKCS7_DIGESTED_DATA:
    case SEC_OID_PKCS7_ENCRYPTED_DATA:

	if (before && dest == &(rootcinfo->content))
	    break;					/* we're not there yet */

	if (p7dcx->content.pointer == NULL)
	    p7dcx->content = rootcinfo->content;

	/* get this data type's inner contentInfo */
	cinfo = SecCmsContentGetContentInfo(p7dcx->content.pointer, p7dcx->type);

	if (before && dest == &(cinfo->contentType)) {
	    /* at this point, set up the &%$&$ back pointer */
	    /* we cannot do it later, because the content itself is optional! */
	    /* please give me C++ */
	    switch (p7dcx->type) {
	    case SEC_OID_PKCS7_SIGNED_DATA:
		p7dcx->content.signedData->cmsg = p7dcx->cmsg;
		break;
	    case SEC_OID_PKCS7_DIGESTED_DATA:
		p7dcx->content.digestedData->cmsg = p7dcx->cmsg;
		break;
	    case SEC_OID_PKCS7_ENVELOPED_DATA:
		p7dcx->content.envelopedData->cmsg = p7dcx->cmsg;
		break;
	    case SEC_OID_PKCS7_ENCRYPTED_DATA:
		p7dcx->content.encryptedData->cmsg = p7dcx->cmsg;
		break;
	    default:
		PORT_Assert(0);
		break;
	    }
	}

	if (before && dest == &(cinfo->rawContent)) {
	    /* we want the ASN.1 decoder to deliver the decoded bytes to us from now on */
	    SEC_ASN1DecoderSetFilterProc(p7dcx->dcx, nss_cms_decoder_update_filter,
					  p7dcx, (Boolean)(p7dcx->cb != NULL));


	    /* we're right in front of the data */
	    if (nss_cms_before_data(p7dcx) != SECSuccess) {
		SEC_ASN1DecoderClearFilterProc(p7dcx->dcx);	/* stop all processing */
		p7dcx->error = PORT_GetError();
	    }
	}
	if (after && dest == &(cinfo->rawContent)) {
	    /* we're right after of the data */
	    if (nss_cms_after_data(p7dcx) != SECSuccess)
		p7dcx->error = PORT_GetError();

	    /* we don't need to see the contents anymore */
	    SEC_ASN1DecoderClearFilterProc(p7dcx->dcx);
	}
	break;

#if 0 /* NIH */
    case SEC_OID_PKCS7_AUTHENTICATED_DATA:
#endif
    default:
	/* unsupported or unknown message type - fail (more or less) gracefully */
	p7dcx->error = SEC_ERROR_UNSUPPORTED_MESSAGE_TYPE;
	break;
    }
}

/*
 * nss_cms_before_data - set up the current encoder to receive data
 */
static OSStatus
nss_cms_before_data(SecCmsDecoderRef p7dcx)
{
    OSStatus rv;
    SECOidTag childtype;
    PLArenaPool *poolp;
    SecCmsDecoderRef childp7dcx;
    SecCmsContentInfoRef cinfo;
    const SecAsn1Template *template;
    void *mark = NULL;
    size_t size;
    
    poolp = p7dcx->cmsg->poolp;

    /* call _Decode_BeforeData handlers */
    switch (p7dcx->type) {
    case SEC_OID_PKCS7_SIGNED_DATA:
	/* we're decoding a signedData, so set up the digests */
	rv = SecCmsSignedDataDecodeBeforeData(p7dcx->content.signedData);
	if (rv != SECSuccess)
	    return SECFailure;
	break;
    case SEC_OID_PKCS7_DIGESTED_DATA:
	/* we're encoding a digestedData, so set up the digest */
	rv = SecCmsDigestedDataDecodeBeforeData(p7dcx->content.digestedData);
	if (rv != SECSuccess)
	    return SECFailure;
	break;
    case SEC_OID_PKCS7_ENVELOPED_DATA:
	rv = SecCmsEnvelopedDataDecodeBeforeData(p7dcx->content.envelopedData);
	if (rv != SECSuccess)
	    return SECFailure;
	break;
    case SEC_OID_PKCS7_ENCRYPTED_DATA:
	rv = SecCmsEncryptedDataDecodeBeforeData(p7dcx->content.encryptedData);
	if (rv != SECSuccess)
	    return SECFailure;
	break;
    default:
	return SECFailure;
    }

    /* ok, now we have a pointer to cinfo */
    /* find out what kind of data is encapsulated */
    
    cinfo = SecCmsContentGetContentInfo(p7dcx->content.pointer, p7dcx->type);
    childtype = SecCmsContentInfoGetContentTypeTag(cinfo);
    
    /* special case for SignedData: "unknown" child type maps to SEC_OID_OTHER */
    if((childtype == SEC_OID_UNKNOWN) && (p7dcx->type == SEC_OID_PKCS7_SIGNED_DATA)) {
	childtype = SEC_OID_OTHER;
    }
    
    if ((childtype == SEC_OID_PKCS7_DATA) || (childtype == SEC_OID_OTHER)){
	cinfo->content.data = SECITEM_AllocItem(poolp, NULL, 0);
	if (cinfo->content.data == NULL)
	    /* set memory error */
	    return SECFailure;

	p7dcx->childp7dcx = NULL;
	return SECSuccess;
    }

    /* set up inner decoder */

    if ((template = SecCmsUtilGetTemplateByTypeTag(childtype)) == NULL)
	return SECFailure;

    childp7dcx = (SecCmsDecoderRef)PORT_ZAlloc(sizeof(struct SecCmsDecoderStr));
    if (childp7dcx == NULL)
	return SECFailure;

    mark = PORT_ArenaMark(poolp);

    /* allocate space for the stuff we're creating */
    size = SecCmsUtilGetSizeByTypeTag(childtype);
    childp7dcx->content.pointer = (void *)PORT_ArenaZAlloc(poolp, size);
    if (childp7dcx->content.pointer == NULL)
	goto loser;

    /* Apple: link the new content to parent ContentInfo */
    cinfo->content.pointer = childp7dcx->content.pointer;
    
    /* start the child decoder */
    childp7dcx->dcx = SEC_ASN1DecoderStart(poolp, childp7dcx->content.pointer, template, NULL);
    if (childp7dcx->dcx == NULL)
	goto loser;

    /* the new decoder needs to notify, too */
    SEC_ASN1DecoderSetNotifyProc(childp7dcx->dcx, nss_cms_decoder_notify, childp7dcx);

    /* tell the parent decoder that it needs to feed us the content data */
    p7dcx->childp7dcx = childp7dcx;

    childp7dcx->type = childtype;	/* our type */

    childp7dcx->cmsg = p7dcx->cmsg;	/* backpointer to root message */

    /* should the child decoder encounter real data, it needs to give it to the caller */
    childp7dcx->cb = p7dcx->cb;
    childp7dcx->cb_arg = p7dcx->cb_arg;

    /* now set up the parent to hand decoded data to the next level decoder */
    p7dcx->cb = (SecCmsContentCallback)SecCmsDecoderUpdate;
    p7dcx->cb_arg = childp7dcx;

    PORT_ArenaUnmark(poolp, mark);

    return SECSuccess;

loser:
    if (mark)
	PORT_ArenaRelease(poolp, mark);
    if (childp7dcx)
	PORT_Free(childp7dcx);
    p7dcx->childp7dcx = NULL;
    return SECFailure;
}

static OSStatus
nss_cms_after_data(SecCmsDecoderRef p7dcx)
{
    PLArenaPool *poolp;
    SecCmsDecoderRef childp7dcx;
    OSStatus rv = SECFailure;

    poolp = p7dcx->cmsg->poolp;

    /* Handle last block. This is necessary to flush out the last bytes
     * of a possibly incomplete block */
    nss_cms_decoder_work_data(p7dcx, NULL, 0, PR_TRUE);

    /* finish any "inner" decoders - there's no more data coming... */
    if (p7dcx->childp7dcx != NULL) {
	childp7dcx = p7dcx->childp7dcx;
	if (childp7dcx->dcx != NULL) {
	    if (SEC_ASN1DecoderFinish(childp7dcx->dcx) != SECSuccess) {
		/* do what? free content? */
		rv = SECFailure;
	    } else {
		rv = nss_cms_after_end(childp7dcx);
	    }
	    if (rv != SECSuccess)
		goto done;
	}
	PORT_Free(p7dcx->childp7dcx);
	p7dcx->childp7dcx = NULL;
    }

    switch (p7dcx->type) {
    case SEC_OID_PKCS7_SIGNED_DATA:
	/* this will finish the digests and verify */
	rv = SecCmsSignedDataDecodeAfterData(p7dcx->content.signedData);
	break;
    case SEC_OID_PKCS7_ENVELOPED_DATA:
	rv = SecCmsEnvelopedDataDecodeAfterData(p7dcx->content.envelopedData);
	break;
    case SEC_OID_PKCS7_DIGESTED_DATA:
	rv = SecCmsDigestedDataDecodeAfterData(p7dcx->content.digestedData);
	break;
    case SEC_OID_PKCS7_ENCRYPTED_DATA:
	rv = SecCmsEncryptedDataDecodeAfterData(p7dcx->content.encryptedData);
	break;
    case SEC_OID_PKCS7_DATA:
	/* do nothing */
	break;
    default:
	rv = SECFailure;
	break;
    }
done:
    return rv;
}

static OSStatus
nss_cms_after_end(SecCmsDecoderRef p7dcx)
{
    OSStatus rv;
    PLArenaPool *poolp;

    poolp = p7dcx->cmsg->poolp;

    switch (p7dcx->type) {
    case SEC_OID_PKCS7_SIGNED_DATA:
	rv = SecCmsSignedDataDecodeAfterEnd(p7dcx->content.signedData);
	break;
    case SEC_OID_PKCS7_ENVELOPED_DATA:
	rv = SecCmsEnvelopedDataDecodeAfterEnd(p7dcx->content.envelopedData);
	break;
    case SEC_OID_PKCS7_DIGESTED_DATA:
	rv = SecCmsDigestedDataDecodeAfterEnd(p7dcx->content.digestedData);
	break;
    case SEC_OID_PKCS7_ENCRYPTED_DATA:
	rv = SecCmsEncryptedDataDecodeAfterEnd(p7dcx->content.encryptedData);
	break;
    case SEC_OID_PKCS7_DATA:
	rv = SECSuccess;
	break;
    default:
	rv = SECFailure;	/* we should not have got that far... */
	break;
    }
    return rv;
}

/*
 * nss_cms_decoder_work_data - handle decoded data bytes.
 *
 * This function either decrypts the data if needed, and/or calculates digests
 * on it, then either stores it or passes it on to the next level decoder.
 */
static void
nss_cms_decoder_work_data(SecCmsDecoderRef p7dcx, 
			     const unsigned char *data, size_t len,
			     Boolean final)
{
    SecCmsContentInfoRef cinfo;
    unsigned char *buf = NULL;
    unsigned char *dest;
    CSSM_SIZE offset;
    OSStatus rv;
    CSSM_DATA_PTR storage;
    
    /*
     * We should really have data to process, or we should be trying
     * to finish/flush the last block.  (This is an overly paranoid
     * check since all callers are in this file and simple inspection
     * proves they do it right.  But it could find a bug in future
     * modifications/development, that is why it is here.)
     */
    PORT_Assert ((data != NULL && len) || final);

    if (!p7dcx->content.pointer)	// might be ExContent??
        return;
        
    cinfo = SecCmsContentGetContentInfo(p7dcx->content.pointer, p7dcx->type);

    if (cinfo->ciphcx != NULL) {
	/*
	 * we are decrypting.
	 * 
	 * XXX If we get an error, we do not want to do the digest or callback,
	 * but we want to keep decoding.  Or maybe we want to stop decoding
	 * altogether if there is a callback, because obviously we are not
	 * sending the data back and they want to know that.
	 */

	CSSM_SIZE outlen = 0;	/* length of decrypted data */
	CSSM_SIZE buflen;		/* length available for decrypted data */

	/* find out about the length of decrypted data */
	buflen = SecCmsCipherContextDecryptLength(cinfo->ciphcx, len, final);

	/*
	 * it might happen that we did not provide enough data for a full
	 * block (decryption unit), and that there is no output available
	 */

	/* no output available, AND no input? */
	if (buflen == 0 && len == 0)
	    goto loser;	/* bail out */

	/*
	 * have inner decoder: pass the data on (means inner content type is NOT data)
	 * no inner decoder: we have DATA in here: either call callback or store
	 */
	if (buflen != 0) {
	    /* there will be some output - need to make room for it */
	    /* allocate buffer from the heap */
	    buf = (unsigned char *)PORT_Alloc(buflen);
	    if (buf == NULL) {
		p7dcx->error = SEC_ERROR_NO_MEMORY;
		goto loser;
	    }
	}

	/*
	 * decrypt incoming data
	 * buf can still be NULL here (and buflen == 0) here if we don't expect
	 * any output (see above), but we still need to call SecCmsCipherContextDecrypt to
	 * keep track of incoming data
	 */
	rv = SecCmsCipherContextDecrypt(cinfo->ciphcx, buf, &outlen, buflen,
			       data, len, final);
	if (rv != SECSuccess) {
	    p7dcx->error = PORT_GetError();
	    goto loser;
	}

	PORT_Assert (final || outlen == buflen);
	
	/* swap decrypted data in */
	data = buf;
	len = outlen;
    }

    if (len == 0)
	goto done;		/* nothing more to do */

    /*
     * Update the running digests with plaintext bytes (if we need to).
     */
    if (cinfo->digcx)
	SecCmsDigestContextUpdate(cinfo->digcx, data, len);

    /* at this point, we have the plain decoded & decrypted data */
    /* which is either more encoded DER which we need to hand to the child decoder */
    /*              or data we need to hand back to our caller */

    /* pass the content back to our caller or */
    /* feed our freshly decrypted and decoded data into child decoder */
    if (p7dcx->cb != NULL) {
	(*p7dcx->cb)(p7dcx->cb_arg, (const char *)data, len);
    }
#if 1
    else
#endif
    switch(SecCmsContentInfoGetContentTypeTag(cinfo)) {
	default:
	    break;
	case SEC_OID_PKCS7_DATA:
	case SEC_OID_OTHER:
	/* store it in "inner" data item as well */
	/* find the DATA item in the encapsulated cinfo and store it there */
	storage = cinfo->content.data;

	offset = storage->Length;

	/* check for potential overflow */
	if (len >= (size_t)(INT_MAX - storage->Length)) {
	  p7dcx->error = SEC_ERROR_NO_MEMORY;
	  goto loser;
	}

	if (storage->Length == 0) {
	    dest = (unsigned char *)PORT_ArenaAlloc(p7dcx->cmsg->poolp, len);
	} else {
	    dest = (unsigned char *)PORT_ArenaGrow(p7dcx->cmsg->poolp, 
				  storage->Data,
				  storage->Length,
				  storage->Length + len);
	}
	if (dest == NULL) {
	    p7dcx->error = SEC_ERROR_NO_MEMORY;
	    goto loser;
	}

	storage->Data = dest;
	storage->Length += len;

	/* copy it in */
	PORT_Memcpy(storage->Data + offset, data, len);
    }

done:
loser:
    if (buf)
	PORT_Free (buf);
}

/*
 * nss_cms_decoder_update_filter - process ASN.1 data
 *
 * once we have set up a filter in nss_cms_decoder_notify(),
 * all data processed by the ASN.1 decoder is also passed through here.
 * we pass the content bytes (as opposed to length and tag bytes) on to
 * nss_cms_decoder_work_data().
 */
static void
nss_cms_decoder_update_filter (void *arg, const char *data, size_t len,
			  int depth, SEC_ASN1EncodingPart data_kind)
{
    SecCmsDecoderRef p7dcx;

    PORT_Assert (len);	/* paranoia */
    if (len == 0)
	return;

    p7dcx = (SecCmsDecoderRef)arg;

    p7dcx->saw_contents = PR_TRUE;

    /* pass on the content bytes only */
    if (data_kind == SEC_ASN1_Contents)
	nss_cms_decoder_work_data(p7dcx, (const unsigned char *) data, len, PR_FALSE);
}

/*
 * SecCmsDecoderCreate - set up decoding of a BER-encoded CMS message
 */
OSStatus
SecCmsDecoderCreate(SecArenaPoolRef pool,
                    SecCmsContentCallback cb, void *cb_arg,
                    PK11PasswordFunc pwfn, void *pwfn_arg,
                    SecCmsGetDecryptKeyCallback decrypt_key_cb, void *decrypt_key_cb_arg,
                    SecCmsDecoderRef *outDecoder)
{
    SecCmsDecoderRef p7dcx;
    SecCmsMessageRef cmsg;
    OSStatus result;

    cmsg = SecCmsMessageCreate(pool);
    if (cmsg == NULL)
        goto loser;

    SecCmsMessageSetEncodingParams(cmsg, pwfn, pwfn_arg, decrypt_key_cb, decrypt_key_cb_arg,
					NULL, NULL);

    p7dcx = (SecCmsDecoderRef)PORT_ZAlloc(sizeof(struct SecCmsDecoderStr));
    if (p7dcx == NULL) {
	SecCmsMessageDestroy(cmsg);
	goto loser;
    }

    p7dcx->dcx = SEC_ASN1DecoderStart(cmsg->poolp, cmsg, SecCmsMessageTemplate, NULL);
    if (p7dcx->dcx == NULL) {
	PORT_Free (p7dcx);
	SecCmsMessageDestroy(cmsg);
	goto loser;
    }

    SEC_ASN1DecoderSetNotifyProc (p7dcx->dcx, nss_cms_decoder_notify, p7dcx);

    p7dcx->cmsg = cmsg;
    p7dcx->type = SEC_OID_UNKNOWN;

    p7dcx->cb = cb;
    p7dcx->cb_arg = cb_arg;

    *outDecoder = p7dcx;
    return noErr;

loser:
    result = PORT_GetError();
    return result;
}

/*
 * SecCmsDecoderUpdate - feed DER-encoded data to decoder
 */
OSStatus
SecCmsDecoderUpdate(SecCmsDecoderRef p7dcx, const void *buf, CFIndex len)
{
    if (p7dcx->dcx != NULL && p7dcx->error == 0) {	/* if error is set already, don't bother */
	if (SEC_ASN1DecoderUpdate (p7dcx->dcx, buf, len) != SECSuccess) {
	    p7dcx->error = PORT_GetError();
	    PORT_Assert (p7dcx->error);
	    if (p7dcx->error == 0)
		p7dcx->error = -1;
	}
    }

    if (p7dcx->error == 0)
	return 0;

    /* there has been a problem, let's finish the decoder */
    if (p7dcx->dcx != NULL) {
        /* @@@ Change this to SEC_ASN1DecoderAbort()? */
	(void) SEC_ASN1DecoderFinish (p7dcx->dcx);
	p7dcx->dcx = NULL;
    }
    PORT_SetError (p7dcx->error);

    return p7dcx->error;
}

/*
 * SecCmsDecoderDestroy - stop decoding in case of error
 */
void
SecCmsDecoderDestroy(SecCmsDecoderRef p7dcx)
{
    /* XXXX what about inner decoders? running digests? decryption? */
    /* XXXX there's a leak here! */
    SecCmsMessageDestroy(p7dcx->cmsg);
    if (p7dcx->dcx)
        (void)SEC_ASN1DecoderFinish(p7dcx->dcx);
    PORT_Free(p7dcx);
}

/*
 * SecCmsDecoderFinish - mark the end of inner content and finish decoding
 */
OSStatus
SecCmsDecoderFinish(SecCmsDecoderRef p7dcx, SecCmsMessageRef *outMessage)
{
    SecCmsMessageRef cmsg;
    OSStatus result;

    cmsg = p7dcx->cmsg;

    if (p7dcx->dcx == NULL || SEC_ASN1DecoderFinish(p7dcx->dcx) != SECSuccess ||
	nss_cms_after_end(p7dcx) != SECSuccess)
    {
	SecCmsMessageDestroy(cmsg);	/* needs to get rid of pool if it's ours */
        result = PORT_GetError();
        goto loser;
    }

    *outMessage = cmsg;
    result = noErr;

loser:
    PORT_Free(p7dcx);
    return result;
}

OSStatus
SecCmsMessageDecode(const CSSM_DATA *encodedMessage,
                    SecCmsContentCallback cb, void *cb_arg,
                    PK11PasswordFunc pwfn, void *pwfn_arg,
                    SecCmsGetDecryptKeyCallback decrypt_key_cb, void *decrypt_key_cb_arg,
                    SecCmsMessageRef *outMessage)
{
    OSStatus result;
    SecCmsDecoderRef decoder;

    result = SecCmsDecoderCreate(NULL, cb, cb_arg, pwfn, pwfn_arg, decrypt_key_cb, decrypt_key_cb_arg, &decoder);
    if (result)
        goto loser;
    result = SecCmsDecoderUpdate(decoder, encodedMessage->Data, encodedMessage->Length);
    if (result) {
        SecCmsDecoderDestroy(decoder);
        goto loser;
    }

    result = SecCmsDecoderFinish(decoder, outMessage);
loser:
    return result;
}

