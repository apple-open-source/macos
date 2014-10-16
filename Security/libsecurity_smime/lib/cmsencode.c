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
 * CMS encoding.
 */

#include <Security/SecCmsEncoder.h>
#include <Security/SecCmsContentInfo.h>
#include <Security/SecCmsDigestContext.h>
#include <Security/SecCmsMessage.h>

#include "cmslocal.h"

#include "secoid.h"
#include "SecAsn1Item.h"

#include <security_asn1/secasn1.h>
#include <security_asn1/secerr.h>
#include <security_asn1/secport.h>

#include <Security/SecBase.h>

#include <limits.h>

struct nss_cms_encoder_output {
    SecCmsContentCallback outputfn;
    void *outputarg;
    CFMutableDataRef berData;
};

struct SecCmsEncoderStr {
    SEC_ASN1EncoderContext *	ecx;		/* ASN.1 encoder context */
    Boolean			ecxupdated;	/* true if data was handed in */
    SecCmsMessageRef 		cmsg;		/* pointer to the root message */
    SECOidTag			type;		/* type tag of the current content */
    SecCmsContent		content;	/* pointer to current content */
    struct nss_cms_encoder_output output;	/* output function */
    int				error;		/* error code */
    SecCmsEncoderRef 	childp7ecx;	/* link to child encoder context */
};

static OSStatus nss_cms_before_data(SecCmsEncoderRef p7ecx);
static OSStatus nss_cms_after_data(SecCmsEncoderRef p7ecx);
static void nss_cms_encoder_update(void *arg, const char *data, size_t len);
static OSStatus nss_cms_encoder_work_data(SecCmsEncoderRef p7ecx, SecAsn1Item * dest,
			     const unsigned char *data, size_t len,
			     Boolean final, Boolean innermost);

extern const SecAsn1Template SecCmsMessageTemplate[];

/*
 * The little output function that the ASN.1 encoder calls to hand
 * us bytes which we in turn hand back to our caller (via the callback
 * they gave us).
 */
static void
nss_cms_encoder_out(void *arg, const char *buf, size_t len,
		      int depth, SEC_ASN1EncodingPart data_kind)
{
    struct nss_cms_encoder_output *output = (struct nss_cms_encoder_output *)arg;

#ifdef CMSDEBUG
    size_t i;

    fprintf(stderr, "kind = %d, depth = %d, len = %d\n", data_kind, depth, len);
    for (i=0; i < len; i++) {
	fprintf(stderr, " %02x%s", (unsigned int)buf[i] & 0xff, ((i % 16) == 15) ? "\n" : "");
    }
    if ((i % 16) != 0)
	fprintf(stderr, "\n");
#endif

    if (output->outputfn != NULL)
	/* call output callback with DER data */
	output->outputfn(output->outputarg, buf, len);

    if (output->berData != NULL) {
	/* store DER data in output->dest */
	CFDataAppendBytes(output->berData, (const UInt8 *)buf, len);
    }
}

/*
 * nss_cms_encoder_notify - ASN.1 encoder callback
 *
 * this function is called by the ASN.1 encoder before and after the encoding of
 * every object. here, it is used to keep track of data structures, set up
 * encryption and/or digesting and possibly set up child encoders.
 */
static void
nss_cms_encoder_notify(void *arg, Boolean before, void *dest, int depth)
{
    SecCmsEncoderRef p7ecx;
    SecCmsContentInfoRef rootcinfo, cinfo;
    Boolean after = !before;
    PLArenaPool *poolp;
    SECOidTag childtype;
    SecAsn1Item * item;

    p7ecx = (SecCmsEncoderRef)arg;
    PORT_Assert(p7ecx != NULL);

    rootcinfo = &(p7ecx->cmsg->contentInfo);
    poolp = p7ecx->cmsg->poolp;

#ifdef CMSDEBUG
    fprintf(stderr, "%6.6s, dest = 0x%08x, depth = %d\n", before ? "before" : "after", dest, depth);
#endif

    /*
     * Watch for the content field, at which point we want to instruct
     * the ASN.1 encoder to start taking bytes from the buffer.
     */
    switch (p7ecx->type) {
    default:
    case SEC_OID_UNKNOWN:
	/* we're still in the root message */
	if (after && dest == &(rootcinfo->contentType)) {
	    /* got the content type OID now - so find out the type tag */
	    p7ecx->type = SecCmsContentInfoGetContentTypeTag(rootcinfo);
	    /* set up a pointer to our current content */
	    p7ecx->content = rootcinfo->content;
	}
	break;

    case SEC_OID_PKCS7_DATA:
	if (before && dest == &(rootcinfo->rawContent)) {
	    /* just set up encoder to grab from user - no encryption or digesting */
	    if ((item = rootcinfo->content.data) != NULL)
		(void)nss_cms_encoder_work_data(p7ecx, NULL, item->Data, item->Length, PR_TRUE, PR_TRUE);
	    else
		SEC_ASN1EncoderSetTakeFromBuf(p7ecx->ecx);
	    SEC_ASN1EncoderClearNotifyProc(p7ecx->ecx);	/* no need to get notified anymore */
	}
	break;

    case SEC_OID_PKCS7_SIGNED_DATA:
    case SEC_OID_PKCS7_ENVELOPED_DATA:
    case SEC_OID_PKCS7_DIGESTED_DATA:
    case SEC_OID_PKCS7_ENCRYPTED_DATA:

	/* when we know what the content is, we encode happily until we reach the inner content */
	cinfo = SecCmsContentGetContentInfo(p7ecx->content.pointer, p7ecx->type);
	childtype = SecCmsContentInfoGetContentTypeTag(cinfo);

	if (after && dest == &(cinfo->contentType)) {
	    /* we're right before encoding the data (if we have some or not) */
	    /* (for encrypted data, we're right before the contentEncAlg which may change */
	    /*  in nss_cms_before_data because of IV calculation when setting up encryption) */
	    if (nss_cms_before_data(p7ecx) != SECSuccess)
		p7ecx->error = PORT_GetError();
	}
	if (before && dest == &(cinfo->rawContent)) {
	    if (childtype == SEC_OID_PKCS7_DATA && (item = cinfo->content.data) != NULL)
		/* we have data - feed it in */
		(void)nss_cms_encoder_work_data(p7ecx, NULL, item->Data, item->Length, PR_TRUE, PR_TRUE);
	    else
		/* else try to get it from user */
		SEC_ASN1EncoderSetTakeFromBuf(p7ecx->ecx);
	}
	if (after && dest == &(cinfo->rawContent)) {
	    if (nss_cms_after_data(p7ecx) != SECSuccess)
		p7ecx->error = PORT_GetError();
	    SEC_ASN1EncoderClearNotifyProc(p7ecx->ecx);	/* no need to get notified anymore */
	}
	break;
    }
}

/*
 * nss_cms_before_data - setup the current encoder to receive data
 */
static OSStatus
nss_cms_before_data(SecCmsEncoderRef p7ecx)
{
    OSStatus rv;
    SECOidTag childtype;
    SecCmsContentInfoRef cinfo;
    PLArenaPool *poolp;
    SecCmsEncoderRef childp7ecx;
    const SecAsn1Template *template;

    poolp = p7ecx->cmsg->poolp;

    /* call _Encode_BeforeData handlers */
    switch (p7ecx->type) {
    case SEC_OID_PKCS7_SIGNED_DATA:
	/* we're encoding a signedData, so set up the digests */
	rv = SecCmsSignedDataEncodeBeforeData(p7ecx->content.signedData);
	break;
    case SEC_OID_PKCS7_DIGESTED_DATA:
	/* we're encoding a digestedData, so set up the digest */
	rv = SecCmsDigestedDataEncodeBeforeData(p7ecx->content.digestedData);
	break;
    case SEC_OID_PKCS7_ENVELOPED_DATA:
	rv = SecCmsEnvelopedDataEncodeBeforeData(p7ecx->content.envelopedData);
	break;
    case SEC_OID_PKCS7_ENCRYPTED_DATA:
	rv = SecCmsEncryptedDataEncodeBeforeData(p7ecx->content.encryptedData);
	break;
    default:
	rv = SECFailure;
    }
    if (rv != SECSuccess)
	return SECFailure;

    /* ok, now we have a pointer to cinfo */
    /* find out what kind of data is encapsulated */
    
    cinfo = SecCmsContentGetContentInfo(p7ecx->content.pointer, p7ecx->type);
    childtype = SecCmsContentInfoGetContentTypeTag(cinfo);

    switch (childtype) {
    case SEC_OID_PKCS7_SIGNED_DATA:
    case SEC_OID_PKCS7_ENVELOPED_DATA:
    case SEC_OID_PKCS7_ENCRYPTED_DATA:
    case SEC_OID_PKCS7_DIGESTED_DATA:
#if 0
    case SEC_OID_PKCS7_DATA:		/* XXX here also??? maybe yes! */
#endif
	/* in these cases, we need to set up a child encoder! */
	/* create new encoder context */
	childp7ecx = PORT_ZAlloc(sizeof(struct SecCmsEncoderStr));
	if (childp7ecx == NULL)
	    return SECFailure;

	/* the CHILD encoder needs to hand its encoded data to the CURRENT encoder
	 * (which will encrypt and/or digest it)
	 * this needs to route back into our update function
	 * which finds the lowest encoding context & encrypts and computes digests */
	childp7ecx->type = childtype;
	childp7ecx->content = cinfo->content;
	/* use the non-recursive update function here, of course */
	childp7ecx->output.outputfn = nss_cms_encoder_update;
	childp7ecx->output.outputarg = p7ecx;
	childp7ecx->output.berData = NULL;
	childp7ecx->cmsg = p7ecx->cmsg;

	template = SecCmsUtilGetTemplateByTypeTag(childtype);
	if (template == NULL)
	    goto loser;		/* cannot happen */

	/* now initialize the data for encoding the first third */
	switch (childp7ecx->type) {
	case SEC_OID_PKCS7_SIGNED_DATA:
	    rv = SecCmsSignedDataEncodeBeforeStart(cinfo->content.signedData);
	    break;
	case SEC_OID_PKCS7_ENVELOPED_DATA:
	    rv = SecCmsEnvelopedDataEncodeBeforeStart(cinfo->content.envelopedData);
	    break;
	case SEC_OID_PKCS7_DIGESTED_DATA:
	    rv = SecCmsDigestedDataEncodeBeforeStart(cinfo->content.digestedData);
	    break;
	case SEC_OID_PKCS7_ENCRYPTED_DATA:
	    rv = SecCmsEncryptedDataEncodeBeforeStart(cinfo->content.encryptedData);
	    break;
	case SEC_OID_PKCS7_DATA:
	    rv = SECSuccess;
	    break;
	default:
	    PORT_Assert(0);
	    break;
	}
	if (rv != SECSuccess)
	    goto loser;

	/*
	 * Initialize the BER encoder.
	 */
	childp7ecx->ecx = SEC_ASN1EncoderStart(cinfo->content.pointer, template,
					   nss_cms_encoder_out, &(childp7ecx->output));
	if (childp7ecx->ecx == NULL)
	    goto loser;

	childp7ecx->ecxupdated = PR_FALSE;

	/*
	 * Indicate that we are streaming.  We will be streaming until we
	 * get past the contents bytes.
	 */
	SEC_ASN1EncoderSetStreaming(childp7ecx->ecx);

	/*
	 * The notify function will watch for the contents field.
	 */
	SEC_ASN1EncoderSetNotifyProc(childp7ecx->ecx, nss_cms_encoder_notify, childp7ecx);

	/* please note that we are NOT calling SEC_ASN1EncoderUpdate here to kick off the */
	/* encoding process - we'll do that from the update function instead */
	/* otherwise we'd be encoding data from a call of the notify function of the */
	/* parent encoder (which would not work) */

	/* this will kick off the encoding process & encode everything up to the content bytes,
	 * at which point the notify function sets streaming mode (and possibly creates
	 * another child encoder). */
	if (SEC_ASN1EncoderUpdate(childp7ecx->ecx, NULL, 0) != SECSuccess)
	    goto loser;

	p7ecx->childp7ecx = childp7ecx;
	break;

    case SEC_OID_PKCS7_DATA:
	p7ecx->childp7ecx = NULL;
	break;
    default:
	/* we do not know this type */
	p7ecx->error = SEC_ERROR_BAD_DER;
	break;
    }

    return SECSuccess;

loser:
    if (childp7ecx) {
	if (childp7ecx->ecx)
	    SEC_ASN1EncoderFinish(childp7ecx->ecx);
	PORT_Free(childp7ecx);
    }
    return SECFailure;
}

static OSStatus
nss_cms_after_data(SecCmsEncoderRef p7ecx)
{
    OSStatus rv = SECFailure;

    switch (p7ecx->type) {
    case SEC_OID_PKCS7_SIGNED_DATA:
	/* this will finish the digests and sign */
	rv = SecCmsSignedDataEncodeAfterData(p7ecx->content.signedData);
	break;
    case SEC_OID_PKCS7_ENVELOPED_DATA:
	rv = SecCmsEnvelopedDataEncodeAfterData(p7ecx->content.envelopedData);
	break;
    case SEC_OID_PKCS7_DIGESTED_DATA:
	rv = SecCmsDigestedDataEncodeAfterData(p7ecx->content.digestedData);
	break;
    case SEC_OID_PKCS7_ENCRYPTED_DATA:
	rv = SecCmsEncryptedDataEncodeAfterData(p7ecx->content.encryptedData);
	break;
    case SEC_OID_PKCS7_DATA:
	/* do nothing */
	break;
    default:
	rv = SECFailure;
	break;
    }
    return rv;
}

/*
 * nss_cms_encoder_work_data - process incoming data
 *
 * (from the user or the next encoding layer)
 * Here, we need to digest and/or encrypt, then pass it on
 *
 */
static OSStatus
nss_cms_encoder_work_data(SecCmsEncoderRef p7ecx, SecAsn1Item * dest,
			     const unsigned char *data, size_t len,
			     Boolean final, Boolean innermost)
{
    unsigned char *buf = NULL;
    OSStatus rv;
    SecCmsContentInfoRef cinfo;

    rv = SECSuccess;		/* may as well be optimistic */

    /*
     * We should really have data to process, or we should be trying
     * to finish/flush the last block.  (This is an overly paranoid
     * check since all callers are in this file and simple inspection
     * proves they do it right.  But it could find a bug in future
     * modifications/development, that is why it is here.)
     */
    PORT_Assert ((data != NULL && len) || final);
    PORT_Assert (len < UINT_MAX); /* overflow check for later cast */

    /* we got data (either from the caller, or from a lower level encoder) */
    cinfo = SecCmsContentGetContentInfo(p7ecx->content.pointer, p7ecx->type);

    /* Update the running digest. */
    if (len && cinfo->digcx != NULL)
	SecCmsDigestContextUpdate(cinfo->digcx, data, len);

    /* Encrypt this chunk. */
    if (cinfo->ciphcx != NULL) {
	unsigned int inlen;	/* length of data being encrypted */
	unsigned int outlen;	/* length of encrypted data */
	unsigned int buflen;	/* length available for encrypted data */

        /* 64 bits cast: only an issue if unsigned int is smaller than size_t.
           Worst case is you will truncate a CMS blob bigger than 4GB when
           encrypting */
	inlen = (unsigned int)len;

	buflen = SecCmsCipherContextEncryptLength(cinfo->ciphcx, inlen, final);
	if (buflen == 0) {
	    /*
	     * No output is expected, but the input data may be buffered
	     * so we still have to call Encrypt.
	     */
	    rv = SecCmsCipherContextEncrypt(cinfo->ciphcx, NULL, NULL, 0,
				   data, inlen, final);
	    if (final) {
		len = 0;
		goto done;
	    }
	    return rv;
	}

	if (dest != NULL)
	    buf = (unsigned char*)PORT_ArenaAlloc(p7ecx->cmsg->poolp, buflen);
	else
	    buf = (unsigned char*)PORT_Alloc(buflen);

	if (buf == NULL) {
	    rv = SECFailure;
	} else {
	    rv = SecCmsCipherContextEncrypt(cinfo->ciphcx, buf, &outlen, buflen,
				   data, inlen, final);
	    data = buf;
	    len = outlen;
	}
	if (rv != SECSuccess)
	    /* encryption or malloc failed? */
	    return rv;
    }


    /*
     * at this point (data,len) has everything we'd like to give to the CURRENT encoder
     * (which will encode it, then hand it back to the user or the parent encoder)
     * We don't encode the data if we're innermost and we're told not to include the data
     */
    if (p7ecx->ecx != NULL && len && (!innermost || cinfo->rawContent != NULL))
	rv = SEC_ASN1EncoderUpdate(p7ecx->ecx, (const char *)data, len);

done:

    if (cinfo->ciphcx != NULL) {
	if (dest != NULL) {
	    dest->Data = buf;
	    dest->Length = len;
	} else if (buf != NULL) {
	    PORT_Free (buf);
	}
    }
    return rv;
}

/*
 * nss_cms_encoder_update - deliver encoded data to the next higher level
 *
 * no recursion here because we REALLY want to end up at the next higher encoder!
 */
static void
nss_cms_encoder_update(void *arg, const char *data, size_t len)
{
    /* XXX Error handling needs help.  Return what?  Do "Finish" on failure? */
    SecCmsEncoderRef p7ecx = (SecCmsEncoderRef)arg;

    (void)nss_cms_encoder_work_data (p7ecx, NULL, (const unsigned char *)data, len, PR_FALSE, PR_FALSE);
}

/*
 * SecCmsEncoderCreate - set up encoding of a CMS message
 *
 * "cmsg" - message to encode
 * "outputfn", "outputarg" - callback function for delivery of DER-encoded output
 *                           will not be called if NULL.
 * "dest" - if non-NULL, pointer to SecAsn1Item that will hold the DER-encoded output
 * "destpoolp" - pool to allocate DER-encoded output in
 * "pwfn", pwfn_arg" - callback function for getting token password
 * "decrypt_key_cb", "decrypt_key_cb_arg" - callback function for getting bulk key for encryptedData
 * "detached_digestalgs", "detached_digests" - digests from detached content
 */
OSStatus
SecCmsEncoderCreate(SecCmsMessageRef cmsg,
                    SecCmsContentCallback outputfn, void *outputarg,
                    CFMutableDataRef outBer,
                    PK11PasswordFunc pwfn, void *pwfn_arg,
                    SecCmsGetDecryptKeyCallback decrypt_key_cb, void *decrypt_key_cb_arg,
                    SecCmsEncoderRef *outEncoder)
{
    SecCmsEncoderRef p7ecx;
    OSStatus result;
    SecCmsContentInfoRef cinfo;

    SecCmsMessageSetEncodingParams(cmsg, pwfn, pwfn_arg, decrypt_key_cb, decrypt_key_cb_arg);

    p7ecx = (SecCmsEncoderRef)PORT_ZAlloc(sizeof(struct SecCmsEncoderStr));
    if (p7ecx == NULL) {
        result = errSecAllocate;
        goto loser;
    }

    p7ecx->cmsg = cmsg;
    p7ecx->output.outputfn = outputfn;
    p7ecx->output.outputarg = outputarg;
    p7ecx->output.berData = outBer;

    p7ecx->type = SEC_OID_UNKNOWN;

    cinfo = SecCmsMessageGetContentInfo(cmsg);

    switch (SecCmsContentInfoGetContentTypeTag(cinfo)) {
    case SEC_OID_PKCS7_SIGNED_DATA:
	result = SecCmsSignedDataEncodeBeforeStart(cinfo->content.signedData);
	break;
    case SEC_OID_PKCS7_ENVELOPED_DATA:
	result = SecCmsEnvelopedDataEncodeBeforeStart(cinfo->content.envelopedData);
	break;
    case SEC_OID_PKCS7_DIGESTED_DATA:
	result = SecCmsDigestedDataEncodeBeforeStart(cinfo->content.digestedData);
	break;
    case SEC_OID_PKCS7_ENCRYPTED_DATA:
	result = SecCmsEncryptedDataEncodeBeforeStart(cinfo->content.encryptedData);
	break;
    default:
        /* @@@ We need a better error for unsupported message types. */
	result = errSecParam;
	break;
    }
    if (result)
        goto loser;

    /* Initialize the BER encoder.
     * Note that this will not encode anything until the first call to SEC_ASN1EncoderUpdate */
    p7ecx->ecx = SEC_ASN1EncoderStart(cmsg, SecCmsMessageTemplate,
                                      nss_cms_encoder_out, &(p7ecx->output));
    if (p7ecx->ecx == NULL) {
        result = PORT_GetError();
	PORT_Free (p7ecx);
        goto loser;
    }
    p7ecx->ecxupdated = PR_FALSE;

    /*
     * Indicate that we are streaming.  We will be streaming until we
     * get past the contents bytes.
     */
    SEC_ASN1EncoderSetStreaming(p7ecx->ecx);

    /*
     * The notify function will watch for the contents field.
     */
    SEC_ASN1EncoderSetNotifyProc(p7ecx->ecx, nss_cms_encoder_notify, p7ecx);

    /* this will kick off the encoding process & encode everything up to the content bytes,
     * at which point the notify function sets streaming mode (and possibly creates
     * a child encoder). */
    if (SEC_ASN1EncoderUpdate(p7ecx->ecx, NULL, 0) != SECSuccess) {
        result = PORT_GetError();
	PORT_Free (p7ecx);
        goto loser;
    }

    *outEncoder = p7ecx;
loser:
    return result;
}

/*
 * SecCmsEncoderUpdate - take content data delivery from the user
 *
 * "p7ecx" - encoder context
 * "data" - content data
 * "len" - length of content data
 *
 * need to find the lowest level (and call SEC_ASN1EncoderUpdate on the way down),
 * then hand the data to the work_data fn
 */
OSStatus
SecCmsEncoderUpdate(SecCmsEncoderRef p7ecx, const void *data, CFIndex len)
{
    OSStatus result;
    SecCmsContentInfoRef cinfo;
    SECOidTag childtype;

    if (p7ecx->error)
	return p7ecx->error;

    /* hand data to the innermost decoder */
    if (p7ecx->childp7ecx) {
	/* recursion here */
	result = SecCmsEncoderUpdate(p7ecx->childp7ecx, data, len);
    } else {
	/* we are at innermost decoder */
	/* find out about our inner content type - must be data */
	cinfo = SecCmsContentGetContentInfo(p7ecx->content.pointer, p7ecx->type);
	childtype = SecCmsContentInfoGetContentTypeTag(cinfo);
	if (childtype != SEC_OID_PKCS7_DATA)
	    return errSecParam; /* @@@ Maybe come up with a better error? */
	/* and we must not have preset data */
	if (cinfo->content.data != NULL)
	    return errSecParam; /* @@@ Maybe come up with a better error? */

	/*  hand it the data so it can encode it (let DER trickle up the chain) */
	result = nss_cms_encoder_work_data(p7ecx, NULL, (const unsigned char *)data, len, PR_FALSE, PR_TRUE);
        if (result)
            result = PORT_GetError();
    }
    return result;
}

/*
 * SecCmsEncoderDestroy - stop all encoding
 *
 * we need to walk down the chain of encoders and the finish them from the innermost out
 */
void
SecCmsEncoderDestroy(SecCmsEncoderRef p7ecx)
{
    /* XXX do this right! */

    /*
     * Finish any inner decoders before us so that all the encoded data is flushed
     * This basically finishes all the decoders from the innermost to the outermost.
     * Finishing an inner decoder may result in data being updated to the outer decoder
     * while we are already in SecCmsEncoderFinish, but that's allright.
     */
    if (p7ecx->childp7ecx)
	SecCmsEncoderDestroy(p7ecx->childp7ecx); /* frees p7ecx->childp7ecx */

    /*
     * On the way back up, there will be no more data (if we had an
     * inner encoder, it is done now!)
     * Flush out any remaining data and/or finish digests.
     */
    if (nss_cms_encoder_work_data(p7ecx, NULL, NULL, 0, PR_TRUE, (p7ecx->childp7ecx == NULL)))
	goto loser;

    p7ecx->childp7ecx = NULL;

    /* kick the encoder back into working mode again.
     * We turn off streaming stuff (which will cause the encoder to continue
     * encoding happily, now that we have all the data (like digests) ready for it).
     */
    SEC_ASN1EncoderClearTakeFromBuf(p7ecx->ecx);
    SEC_ASN1EncoderClearStreaming(p7ecx->ecx);

    /* now that TakeFromBuf is off, this will kick this encoder to finish encoding */
    SEC_ASN1EncoderUpdate(p7ecx->ecx, NULL, 0);

loser:
    SEC_ASN1EncoderFinish(p7ecx->ecx);
    PORT_Free (p7ecx);
}

/*
 * SecCmsEncoderFinish - signal the end of data
 *
 * we need to walk down the chain of encoders and the finish them from the innermost out
 */
OSStatus
SecCmsEncoderFinish(SecCmsEncoderRef p7ecx)
{
    OSStatus result;
    SecCmsContentInfoRef cinfo;
    SECOidTag childtype;

    /*
     * Finish any inner decoders before us so that all the encoded data is flushed
     * This basically finishes all the decoders from the innermost to the outermost.
     * Finishing an inner decoder may result in data being updated to the outer decoder
     * while we are already in SecCmsEncoderFinish, but that's allright.
     */
    if (p7ecx->childp7ecx) {
	result = SecCmsEncoderFinish(p7ecx->childp7ecx); /* frees p7ecx->childp7ecx */
	if (result)
	    goto loser;
    }

    /*
     * On the way back up, there will be no more data (if we had an
     * inner encoder, it is done now!)
     * Flush out any remaining data and/or finish digests.
     */
    result = nss_cms_encoder_work_data(p7ecx, NULL, NULL, 0, PR_TRUE, (p7ecx->childp7ecx == NULL));
    if (result) {
        result = PORT_GetError();
	goto loser;
    }

    p7ecx->childp7ecx = NULL;

    /* find out about our inner content type - must be data */
    cinfo = SecCmsContentGetContentInfo(p7ecx->content.pointer, p7ecx->type);
    childtype = SecCmsContentInfoGetContentTypeTag(cinfo);
    if (childtype == SEC_OID_PKCS7_DATA && cinfo->content.data == NULL) {
	SEC_ASN1EncoderClearTakeFromBuf(p7ecx->ecx);
	/* now that TakeFromBuf is off, this will kick this encoder to finish encoding */
	result = SEC_ASN1EncoderUpdate(p7ecx->ecx, NULL, 0);
        if (result)
            result = PORT_GetError();
    }

    SEC_ASN1EncoderClearStreaming(p7ecx->ecx);

    if (p7ecx->error && !result)
	result = p7ecx->error;

loser:
    SEC_ASN1EncoderFinish(p7ecx->ecx);
    PORT_Free (p7ecx);
    return result;
}

OSStatus
SecCmsMessageEncode(SecCmsMessageRef cmsg, const SecAsn1Item *input,
                    CFMutableDataRef outBer)
{
    SecCmsEncoderRef encoder = NULL;
    OSStatus result;

    if (!cmsg || !outBer) {
        result = errSecParam;
        goto loser;
    }

    result = SecCmsEncoderCreate(cmsg, 0, 0, outBer, 0, 0, 0, 0, &encoder);
    if (result)
	goto loser;

    if (input) {
	result = SecCmsEncoderUpdate(encoder, input->Data, input->Length);
	if (result) {
            SecCmsEncoderDestroy(encoder);
            goto loser;
	}
    }
    result = SecCmsEncoderFinish(encoder);

loser:
    return result;
}
