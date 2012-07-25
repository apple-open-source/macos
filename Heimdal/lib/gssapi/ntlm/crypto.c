/*
 * Copyright (c) 2006 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "ntlm.h"

uint32_t
_krb5_crc_update (const char *p, size_t len, uint32_t res);
void
_krb5_crc_init_table(void);

/*
 *
 */

const char a2i_signmagic[] =
    "session key to server-to-client signing key magic constant";
const char a2i_sealmagic[] =
    "session key to server-to-client sealing key magic constant";
const char i2a_signmagic[] =
    "session key to client-to-server signing key magic constant";
const char i2a_sealmagic[] =
    "session key to client-to-server sealing key magic constant";


static void
_gss_ntlm_set_key(struct ntlmv2_key *key, int acceptor, int sealsign,
		  unsigned char *data, size_t len)
{
    unsigned char out[16];
    CCDigestRef ctx;
    const char *signmagic;
    const char *sealmagic;

    if (acceptor) {
	signmagic = a2i_signmagic;
	sealmagic = a2i_sealmagic;
    } else {
	signmagic = i2a_signmagic;
	sealmagic = i2a_sealmagic;
    }

    key->seq = 0;

    ctx = CCDigestCreate(kCCDigestMD5);
    CCDigestUpdate(ctx, data, len);
    CCDigestUpdate(ctx, signmagic, strlen(signmagic) + 1);
    CCDigestFinal(ctx, key->signkey);

    CCDigestReset(ctx);
    CCDigestUpdate(ctx, data, len);
    CCDigestUpdate(ctx, sealmagic, strlen(sealmagic) + 1);
    CCDigestFinal(ctx, out);
    CCDigestDestroy(ctx);

    EVP_CIPHER_CTX_cleanup(&key->sealkey);

    EVP_CipherInit_ex(&key->sealkey, EVP_rc4(), NULL, out, NULL, 1);
    if (sealsign) {
	key->signsealkey = &key->sealkey;
    }
}

/*
 * Set (or reset) keys
 */

void
_gss_ntlm_set_keys(ntlm_ctx ctx)
{
    int acceptor = (ctx->status & STATUS_CLIENT) ? 0 : 1;

    if (ctx->sessionkey.length == 0)
	return;

    ctx->status |= STATUS_SESSIONKEY;
	
    if (ctx->flags & NTLM_NEG_SEAL)
	ctx->gssflags |= GSS_C_CONF_FLAG;
    if (ctx->flags & (NTLM_NEG_SIGN|NTLM_NEG_ALWAYS_SIGN))
	ctx->gssflags |= GSS_C_INTEG_FLAG;

    if (ctx->flags & NTLM_NEG_NTLM2_SESSION) {
	_gss_ntlm_set_key(&ctx->u.v2.send, acceptor,
			  (ctx->flags & NTLM_NEG_KEYEX),
			  ctx->sessionkey.data,
			  ctx->sessionkey.length);
	_gss_ntlm_set_key(&ctx->u.v2.recv, !acceptor,
			  (ctx->flags & NTLM_NEG_KEYEX),
			  ctx->sessionkey.data,
			  ctx->sessionkey.length);
    } else {
	EVP_CIPHER_CTX_cleanup(&ctx->u.v1.crypto_send.key);
	EVP_CIPHER_CTX_cleanup(&ctx->u.v1.crypto_recv.key);
	
	EVP_CipherInit_ex(&ctx->u.v1.crypto_send.key, EVP_rc4(), NULL,
			  ctx->sessionkey.data, NULL, 1);
	EVP_CipherInit_ex(&ctx->u.v1.crypto_recv.key, EVP_rc4(), NULL,
			  ctx->sessionkey.data, NULL, 1);
    }
}

void
_gss_ntlm_destroy_crypto(ntlm_ctx ctx)
{
    if ((ctx->status & STATUS_SESSIONKEY) == 0)
	return;

    if (ctx->flags & NTLM_NEG_NTLM2_SESSION) {
	EVP_CIPHER_CTX_cleanup(&ctx->u.v2.send.sealkey);
	EVP_CIPHER_CTX_cleanup(&ctx->u.v2.recv.sealkey);
    } else {
	EVP_CIPHER_CTX_cleanup(&ctx->u.v1.crypto_send.key);
	EVP_CIPHER_CTX_cleanup(&ctx->u.v1.crypto_recv.key);
    }
}


/*
 *
 */

static OM_uint32
v1_sign_message(EVP_CIPHER_CTX *signkey,
		uint32_t seq,
		gss_iov_buffer_t trailer,
                gss_iov_buffer_desc *iov,
                int iov_count)
{
    unsigned char *out = trailer->buffer.value;
    unsigned char signature[12];
    uint32_t crc = 0;
    int i;

    _krb5_crc_init_table();

    for (i = 0; i < iov_count; i++) {
        gss_iov_buffer_t iovp = &iov[i];

        switch (GSS_IOV_BUFFER_TYPE(iovp->type)) {
        case GSS_IOV_BUFFER_TYPE_DATA:
        case GSS_IOV_BUFFER_TYPE_PADDING:
        case GSS_IOV_BUFFER_TYPE_SIGN_ONLY:
            crc = _krb5_crc_update(iovp->buffer.value, iovp->buffer.length, crc);
            break;
        default:
            break;
        }
    }

    _gss_mg_encode_le_uint32(0, &signature[0]);
    _gss_mg_encode_le_uint32(crc, &signature[4]);
    _gss_mg_encode_le_uint32(seq, &signature[8]);

    _gss_mg_encode_le_uint32(1, out); /* version */
    
    EVP_Cipher(signkey, out + 4, signature, sizeof(signature));

    if (CCRandomCopyBytes(kCCRandomDefault, out + 4, 4))
	return GSS_S_UNAVAILABLE;

    return 0;
}


static OM_uint32
v2_sign_message(unsigned char signkey[16],
		EVP_CIPHER_CTX *sealkey,
		uint32_t seq,
		gss_iov_buffer_t trailer,
                gss_iov_buffer_desc *iov,
                int iov_count)
{
    unsigned char *out = trailer->buffer.value;
    unsigned char hmac[16];
    CCHmacContext c;
    int i;

    assert(trailer->buffer.length == 16);

    CCHmacInit(&c, kCCHmacAlgMD5, signkey, 16);

    _gss_mg_encode_le_uint32(seq, hmac);
    CCHmacUpdate(&c, hmac, 4);
    for (i = 0; i < iov_count; i++) {
        gss_iov_buffer_t iovp = &iov[i];

        /*
         * We include empty buffers because NTLM2 always does
         * DCE RPC header signing regardless of whether it was
         * negotiated at bind time. The DCE RPC runtime will
         * submit EMPTY header buffers when signing is disabled.
         */
        switch (GSS_IOV_BUFFER_TYPE(iovp->type)) {
        case GSS_IOV_BUFFER_TYPE_EMPTY:
        case GSS_IOV_BUFFER_TYPE_DATA:
        case GSS_IOV_BUFFER_TYPE_PADDING:
        case GSS_IOV_BUFFER_TYPE_SIGN_ONLY:
            CCHmacUpdate(&c, iovp->buffer.value, iovp->buffer.length);
            break;
        default:
            break;
        }
    }
    CCHmacFinal(&c, hmac);
    memset(&c, 0, sizeof(c));

    _gss_mg_encode_le_uint32(1, &out[0]);
    if (sealkey)
	EVP_Cipher(sealkey, out + 4, hmac, 8);
    else
	memcpy(&out[4], hmac, 8);

    memset(&out[12], 0, 4);

    return GSS_S_COMPLETE;
}

static OM_uint32
v2_verify_message(unsigned char signkey[16],
		  EVP_CIPHER_CTX *sealkey,
		  uint32_t seq,
		  gss_iov_buffer_t trailer,
                  gss_iov_buffer_desc *iov,
                  int iov_count)
{
    OM_uint32 ret;
    unsigned char outbuf[16];
    gss_iov_buffer_desc out;

    if (trailer->buffer.length != 16)
	return GSS_S_BAD_MIC;

    _gss_mg_decode_be_uint32((uint8_t *)trailer->buffer.value + 12, &seq);

    out.type = GSS_IOV_BUFFER_TYPE_TRAILER;
    out.buffer.length = sizeof(outbuf);
    out.buffer.value = outbuf;

    ret = v2_sign_message(signkey, sealkey, seq, &out, iov, iov_count);
    if (ret)
	return ret;

    if (ct_memcmp(trailer->buffer.value, outbuf, 16))
	return GSS_S_BAD_MIC;

    return GSS_S_COMPLETE;
}

static OM_uint32
v2_seal_message(unsigned char signkey[16],
		uint32_t seq,
		EVP_CIPHER_CTX *sealkey,
                gss_iov_buffer_t trailer,
		gss_iov_buffer_desc *iov,
                int iov_count)
{
    OM_uint32 ret;
    int i;

    for (i = 0; i < iov_count; i++) {
        gss_iov_buffer_t iovp = &iov[i];

        switch (GSS_IOV_BUFFER_TYPE(iovp->type)) {
        case GSS_IOV_BUFFER_TYPE_DATA:
        case GSS_IOV_BUFFER_TYPE_PADDING:
	    EVP_Cipher(sealkey, iovp->buffer.value, iovp->buffer.value,
		       iovp->buffer.length);
            break;
        default:
            break;
        }
    }

    assert(trailer->buffer.length == 16);

    ret = v2_sign_message(signkey, sealkey, seq, trailer, iov, iov_count);

    return ret;
}

static OM_uint32
v2_unseal_message(unsigned char signkey[16],
		  uint32_t seq,
		  EVP_CIPHER_CTX *sealkey,
                  gss_iov_buffer_t trailer,
                  gss_iov_buffer_desc *iov,
                  int iov_count)
{
    OM_uint32 ret;
    int i;

    for (i = 0; i < iov_count; i++) {
        gss_iov_buffer_t iovp = &iov[i];

        switch (GSS_IOV_BUFFER_TYPE(iovp->type)) {
        case GSS_IOV_BUFFER_TYPE_DATA:
        case GSS_IOV_BUFFER_TYPE_PADDING:
	    EVP_Cipher(sealkey, iovp->buffer.value, iovp->buffer.value,
		       iovp->buffer.length);
            break;
        default:
            break;
        }
    }

    ret = v2_verify_message(signkey, sealkey, seq,
                            trailer, iov, iov_count);

    return ret;
}

/*
 *
 */

#define CTX_FLAGS_ISSET(_ctx,_flags) \
    (((_ctx)->flags & (_flags)) == (_flags))

/*
 *
 */

static OM_uint32 get_mic_iov
           (OM_uint32 * minor_status,
            const gss_ctx_id_t context_handle,
            gss_qop_t qop_req,
            gss_iov_buffer_t trailer,
            gss_iov_buffer_desc *iov,
            int iov_count
           )
{
    ntlm_ctx ctx = (ntlm_ctx)context_handle;

    *minor_status = 0;

    assert(trailer->buffer.length == 16);
    assert(trailer->buffer.value != NULL);

    if (CTX_FLAGS_ISSET(ctx, NTLM_NEG_SIGN|NTLM_NEG_NTLM2_SESSION)) {
	OM_uint32 ret;

	if ((ctx->status & STATUS_SESSIONKEY) == 0)
	    return GSS_S_UNAVAILABLE;

	ret = v2_sign_message(ctx->u.v2.send.signkey,
			      ctx->u.v2.send.signsealkey,
			      ctx->u.v2.send.seq++,
			      trailer, iov, iov_count);
        return ret;

    } else if (CTX_FLAGS_ISSET(ctx, NTLM_NEG_SIGN)) {
	OM_uint32 ret;

	if ((ctx->status & STATUS_SESSIONKEY) == 0)
	    return GSS_S_UNAVAILABLE;

	ret = v1_sign_message(&ctx->u.v1.crypto_send.key,
			      ctx->u.v1.crypto_send.seq++,
			      trailer, iov, iov_count);
        return ret;

    } else if (CTX_FLAGS_ISSET(ctx, NTLM_NEG_ALWAYS_SIGN)) {
	unsigned char *signature;

	signature = trailer->buffer.value;

	_gss_mg_encode_le_uint32(1, &signature[0]); /* version */
	_gss_mg_encode_le_uint32(0, &signature[4]);
	_gss_mg_encode_le_uint32(0, &signature[8]);
	_gss_mg_encode_le_uint32(0, &signature[12]);

        return GSS_S_COMPLETE;
    }

    return GSS_S_UNAVAILABLE;
}

OM_uint32 _gss_ntlm_get_mic
           (OM_uint32 * minor_status,
            const gss_ctx_id_t context_handle,
            gss_qop_t qop_req,
            const gss_buffer_t message_buffer,
            gss_buffer_t message_token
           )
{
    gss_iov_buffer_desc iov[2];
    OM_uint32 ret, junk;

    iov[0].type = GSS_IOV_BUFFER_TYPE_DATA;
    iov[0].buffer = *message_buffer;

    iov[1].type = GSS_IOV_BUFFER_TYPE_TRAILER;
    iov[1].buffer.length = 16;
    iov[1].buffer.value = malloc(iov[1].buffer.length);
    if (iov[1].buffer.value == NULL) {
        *minor_status = ENOMEM;
        return GSS_S_FAILURE;
    }

    ret = get_mic_iov(minor_status, context_handle, qop_req,
                      &iov[1], iov, 1);

    if (ret)
        gss_release_buffer(&junk, &iov[1].buffer);
    else
        *message_token = iov[1].buffer;

    return ret;
}

/*
 *
 */

static OM_uint32
verify_mic_iov
           (OM_uint32 * minor_status,
            const gss_ctx_id_t context_handle,
            gss_iov_buffer_t trailer,
            gss_qop_t * qop_state,
            gss_iov_buffer_desc *iov,
            int iov_count
	    )
{
    ntlm_ctx ctx = (ntlm_ctx)context_handle;

    if (qop_state != NULL)
	*qop_state = GSS_C_QOP_DEFAULT;
    *minor_status = 0;

    if (trailer->buffer.length != 16)
	return GSS_S_BAD_MIC;

    if (CTX_FLAGS_ISSET(ctx, NTLM_NEG_SIGN|NTLM_NEG_NTLM2_SESSION)) {
	OM_uint32 ret;

	if ((ctx->status & STATUS_SESSIONKEY) == 0)
	    return GSS_S_UNAVAILABLE;

	ret = v2_verify_message(ctx->u.v2.recv.signkey,
				ctx->u.v2.recv.signsealkey,
				0,
				trailer, iov, iov_count);
	if (ret)
	    return ret;

	return GSS_S_COMPLETE;
    } else if (CTX_FLAGS_ISSET(ctx, NTLM_NEG_SIGN)) {
	unsigned char signature[12];
	uint32_t crc = 0, num;
        int i;

	if ((ctx->status & STATUS_SESSIONKEY) == 0)
	    return GSS_S_UNAVAILABLE;

	_gss_mg_decode_le_uint32(trailer->buffer.value, &num);
	if (num != 1)
	    return GSS_S_BAD_MIC;

	EVP_Cipher(&ctx->u.v1.crypto_recv.key, signature,
		   ((unsigned char *)trailer->buffer.value) + 4,
		   sizeof(signature));

	_krb5_crc_init_table();

        for (i = 0; i < iov_count; i++) {
            gss_iov_buffer_t iovp = &iov[i];

            switch (GSS_IOV_BUFFER_TYPE(iovp->type)) {
            case GSS_IOV_BUFFER_TYPE_DATA:
            case GSS_IOV_BUFFER_TYPE_PADDING:
            case GSS_IOV_BUFFER_TYPE_SIGN_ONLY:
                crc = _krb5_crc_update(iovp->buffer.value,
                                       iovp->buffer.length, crc);
                break;
            default:
                break;
            }
        }

	/* skip first 4 bytes in the encrypted checksum */
	_gss_mg_decode_le_uint32(&signature[4], &num);
	if (num != crc)
	    return GSS_S_BAD_MIC;
	_gss_mg_decode_le_uint32(&signature[8], &num);
	if (ctx->u.v1.crypto_recv.seq != num)
	    return GSS_S_BAD_MIC;
	ctx->u.v1.crypto_recv.seq++;

        return GSS_S_COMPLETE;
    } else if (ctx->flags & NTLM_NEG_ALWAYS_SIGN) {
	uint32_t num;
	unsigned char *p;

	p = (unsigned char*)(trailer->buffer.value);

	_gss_mg_decode_le_uint32(&p[0], &num); /* version */
	if (num != 1) return GSS_S_BAD_MIC;
	_gss_mg_decode_le_uint32(&p[4], &num);
	if (num != 0) return GSS_S_BAD_MIC;
	_gss_mg_decode_le_uint32(&p[8], &num);
	if (num != 0) return GSS_S_BAD_MIC;
	_gss_mg_decode_le_uint32(&p[12], &num);
	if (num != 0) return GSS_S_BAD_MIC;

        return GSS_S_COMPLETE;
    }

    return GSS_S_UNAVAILABLE;
}

OM_uint32
_gss_ntlm_verify_mic
           (OM_uint32 * minor_status,
            const gss_ctx_id_t context_handle,
            const gss_buffer_t message_buffer,
            const gss_buffer_t token_buffer,
            gss_qop_t * qop_state
	    )
{
    gss_iov_buffer_desc iov[2];

    iov[0].type = GSS_IOV_BUFFER_TYPE_DATA;
    iov[0].buffer = *message_buffer;

    iov[1].type = GSS_IOV_BUFFER_TYPE_TRAILER;
    iov[1].buffer = *token_buffer;

    return verify_mic_iov(minor_status, context_handle,
                          &iov[1], qop_state, iov, 1);
}

/*
 *
 */

OM_uint32
_gss_ntlm_wrap_size_limit (
            OM_uint32 * minor_status,
            const gss_ctx_id_t context_handle,
            int conf_req_flag,
            gss_qop_t qop_req,
            OM_uint32 req_output_size,
            OM_uint32 * max_input_size
           )
{
    ntlm_ctx ctx = (ntlm_ctx)context_handle;

    *minor_status = 0;

    if(ctx->flags & NTLM_NEG_SEAL) {

	if (req_output_size < 16)
	    *max_input_size = 0;
	else
	    *max_input_size = req_output_size - 16;

	return GSS_S_COMPLETE;
    }

    return GSS_S_UNAVAILABLE;
}

/*
 *
 */

OM_uint32 _gss_ntlm_wrap_iov
(OM_uint32 * minor_status,
 const gss_ctx_id_t context_handle,
 int conf_req_flag,
 gss_qop_t qop_req,
 int * conf_state,
 gss_iov_buffer_desc *iov,
 int iov_count
    )
{
    ntlm_ctx ctx = (ntlm_ctx)context_handle;
    OM_uint32 ret;
    gss_iov_buffer_t trailer;

    *minor_status = 0;
    if (conf_state)
	*conf_state = 0;
    if (iov == GSS_C_NO_IOV_BUFFER)
	return GSS_S_FAILURE;

    /* TRAILER for normal protocols, HEADER for DCE */
    trailer = _gss_mg_find_buffer(iov, iov_count, GSS_IOV_BUFFER_TYPE_TRAILER);
    if (trailer == NULL) {
        trailer = _gss_mg_find_buffer(iov, iov_count, GSS_IOV_BUFFER_TYPE_HEADER);
        if (trailer == NULL) {
	    *minor_status = HNTLM_ERR_MISSING_BUFFER;
	    return gss_mg_set_error_string(GSS_NTLM_MECHANISM, GSS_S_FAILURE,
					   HNTLM_ERR_MISSING_BUFFER,
					   "iov header buffer missing");
        }
    }
    if (GSS_IOV_BUFFER_FLAGS(trailer->type) & GSS_IOV_BUFFER_TYPE_FLAG_ALLOCATE) {
        ret = _gss_mg_allocate_buffer(minor_status, trailer, 16);
        if (ret)
            return ret;
    } else if (trailer->buffer.length < 16) {
        *minor_status = KRB5_BAD_MSIZE;
        return GSS_S_FAILURE;
    } else {
        trailer->buffer.length = 16;
    }

    if (CTX_FLAGS_ISSET(ctx, NTLM_NEG_SEAL|NTLM_NEG_NTLM2_SESSION)) {

	return v2_seal_message(ctx->u.v2.send.signkey,
			       ctx->u.v2.send.seq++,
			       &ctx->u.v2.send.sealkey,
			       trailer, iov, iov_count);

    } else if (CTX_FLAGS_ISSET(ctx, NTLM_NEG_SEAL)) {
        int i;

        for (i = 0; i < iov_count; i++) {
            gss_iov_buffer_t iovp = &iov[i];

            switch (GSS_IOV_BUFFER_TYPE(iovp->type)) {
            case GSS_IOV_BUFFER_TYPE_DATA:
            case GSS_IOV_BUFFER_TYPE_PADDING:
		EVP_Cipher(&ctx->u.v1.crypto_send.key,
			   iovp->buffer.value, iovp->buffer.value,
			   iovp->buffer.length);
                break;
            default:
                break;
            }
        }

	ret = get_mic_iov(minor_status, context_handle,
				0, trailer, iov, iov_count);

	return ret;
    }

    return GSS_S_UNAVAILABLE;
}

OM_uint32 _gss_ntlm_wrap
(OM_uint32 * minor_status,
 const gss_ctx_id_t context_handle,
 int conf_req_flag,
 gss_qop_t qop_req,
 const gss_buffer_t input_message_buffer,
 int * conf_state,
 gss_buffer_t output_message_buffer)
{
    gss_iov_buffer_desc iov[2];
    OM_uint32 ret;

    output_message_buffer->length = input_message_buffer->length + 16;
    output_message_buffer->value = malloc(output_message_buffer->length);
    if (output_message_buffer->value == NULL) {
        *minor_status = ENOMEM;
        return GSS_S_FAILURE;
    }

    iov[0].type = GSS_IOV_BUFFER_TYPE_DATA;
    iov[0].buffer.length = input_message_buffer->length;
    iov[0].buffer.value = output_message_buffer->value;
    memcpy(iov[0].buffer.value, input_message_buffer->value,
           input_message_buffer->length);

    iov[1].type = GSS_IOV_BUFFER_TYPE_TRAILER;
    iov[1].buffer.length = 16;
    iov[1].buffer.value = (unsigned char *)output_message_buffer->value + 16;

    ret = _gss_ntlm_wrap_iov(minor_status, context_handle,
                             conf_req_flag, qop_req,
                             conf_state, iov, sizeof(iov)/sizeof(iov[0]));
    if (GSS_ERROR(ret)) {
        OM_uint32 tmp;
        gss_release_buffer(&tmp, output_message_buffer);
    }

    return ret;
}
 
/*
 *
 */

OM_uint32 _gss_ntlm_unwrap_iov
           (OM_uint32 * minor_status,
            const gss_ctx_id_t context_handle,
            int * conf_state,
            gss_qop_t * qop_state,
            gss_iov_buffer_desc *iov,
            int iov_count
           )
{
    ntlm_ctx ctx = (ntlm_ctx)context_handle;
    OM_uint32 ret;
    gss_iov_buffer_t trailer;

    *minor_status = 0;

    if (conf_state)
	*conf_state = 0;
    if (qop_state)
	*qop_state = 0;

    /* TRAILER for normal protocols, HEADER for DCE */
    trailer = _gss_mg_find_buffer(iov, iov_count, GSS_IOV_BUFFER_TYPE_TRAILER);
    if (trailer == NULL) {
        trailer = _gss_mg_find_buffer(iov, iov_count, GSS_IOV_BUFFER_TYPE_HEADER);
        if (trailer == NULL) {
	    *minor_status = HNTLM_ERR_MISSING_BUFFER;
	    return gss_mg_set_error_string(GSS_NTLM_MECHANISM, GSS_S_FAILURE,
					   HNTLM_ERR_MISSING_BUFFER,
					   "iov tailer buffer missing");
        }
    }

    if (trailer->buffer.length < 16) 
        return GSS_S_BAD_MIC;

    if (CTX_FLAGS_ISSET(ctx, NTLM_NEG_SEAL|NTLM_NEG_NTLM2_SESSION)) {

	return v2_unseal_message(ctx->u.v2.recv.signkey,
				 ctx->u.v2.recv.seq++,
				 &ctx->u.v2.recv.sealkey,
				 trailer,
				 iov,
				 iov_count);

    } else if (CTX_FLAGS_ISSET(ctx, NTLM_NEG_SEAL)) {

        int i;

        for (i = 0; i < iov_count; i++) {
            gss_iov_buffer_t iovp = &iov[i];

            switch (GSS_IOV_BUFFER_TYPE(iovp->type)) {
            case GSS_IOV_BUFFER_TYPE_DATA:
            case GSS_IOV_BUFFER_TYPE_PADDING:
		EVP_Cipher(&ctx->u.v1.crypto_recv.key,
			   iovp->buffer.value, iovp->buffer.value,
			   iovp->buffer.length);
                break;
            default:
                break;
            }
        }
	
	ret = verify_mic_iov(minor_status, context_handle,
                             trailer, NULL, iov, iov_count);

	return ret;
    }

    return GSS_S_UNAVAILABLE;
}

OM_uint32 _gss_ntlm_unwrap
           (OM_uint32 * minor_status,
            const gss_ctx_id_t context_handle,
            const gss_buffer_t input_message_buffer,
            gss_buffer_t output_message_buffer,
            int * conf_state,
            gss_qop_t * qop_state
           )
{
    gss_iov_buffer_desc iov[2];
    OM_uint32 ret;

    if (input_message_buffer->length < 16)
        return GSS_S_DEFECTIVE_TOKEN;

    output_message_buffer->length = input_message_buffer->length - 16;
    output_message_buffer->value = malloc(output_message_buffer->length);
    if (output_message_buffer->value == NULL) {
        *minor_status = ENOMEM;
        return GSS_S_FAILURE;
    }
    memcpy(output_message_buffer->value, input_message_buffer->value,
           output_message_buffer->length);

    iov[0].type = GSS_IOV_BUFFER_TYPE_DATA;
    iov[0].buffer = *output_message_buffer;

    iov[1].type = GSS_IOV_BUFFER_TYPE_TRAILER;
    iov[1].buffer.value = (unsigned char *)input_message_buffer->value +
                          input_message_buffer->length - 16;
    iov[1].buffer.length = 16;

    ret = _gss_ntlm_unwrap_iov(minor_status, context_handle,
                               conf_state, qop_state, iov,
                               sizeof(iov)/sizeof(iov[0]));
    if (GSS_ERROR(ret)) {
        OM_uint32 tmp;
        gss_release_buffer(&tmp, output_message_buffer);
    }

    return ret;
}

OM_uint32
_gss_ntlm_wrap_iov_length(OM_uint32 * minor_status,
                          gss_ctx_id_t context_handle,
                          int conf_req_flag,
                          gss_qop_t qop_req,
                          int *conf_state,
                          gss_iov_buffer_desc *iov,
                          int iov_count)
{
    gss_iov_buffer_t iovp;
    OM_uint32 ctype;

    /* DCE puts the trailer in the HEADER, other protocols in TRAILER. */
    iovp = _gss_mg_find_buffer(iov, iov_count, GSS_IOV_BUFFER_TYPE_TRAILER);
    if (iovp == NULL) {
        iovp = _gss_mg_find_buffer(iov, iov_count, GSS_IOV_BUFFER_TYPE_HEADER);
        if (iovp == NULL) {
	    *minor_status = HNTLM_ERR_MISSING_BUFFER;
	    return gss_mg_set_error_string(GSS_NTLM_MECHANISM, GSS_S_FAILURE,
					   HNTLM_ERR_MISSING_BUFFER,
					   "iov header buffer missing");
        } else
            ctype = GSS_IOV_BUFFER_TYPE_TRAILER;
    } else
        ctype = GSS_IOV_BUFFER_TYPE_HEADER;

    iovp->buffer.length = 16;

    iovp = _gss_mg_find_buffer(iov, iov_count, GSS_IOV_BUFFER_TYPE_PADDING);
    if (iovp != NULL)
        iovp->buffer.length = 0;

    /* No HEADER if we have a TRAILER and vice versa */
    iovp = _gss_mg_find_buffer(iov, iov_count, ctype);
    if (iovp != NULL)
        iovp->buffer.length = 0;

    if (conf_state != NULL)
        *conf_state = conf_req_flag;

    *minor_status = 0;
    return GSS_S_COMPLETE;
}

