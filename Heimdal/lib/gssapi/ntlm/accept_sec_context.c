/*
 * Copyright (c) 2006 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2009 Apple Inc. All rights reserved.
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

/*
 *
 */

static struct ntlm_server_interface * _ntlm_interfaces[] = {
#if !defined(__APPLE_TARGET_EMBEDDED__)
    &ntlmsspi_dstg_digest
#endif
};

/*
 * Allocate a NTLM context handle for the first provider that
 * is up and running.
 */
OM_uint32
_gss_ntlm_allocate_ctx(OM_uint32 *minor_status, const char *domain, ntlm_ctx *ctx)
{
    ntlm_ctx c;
    OM_uint32 maj_stat;
    size_t i;
    int found = 0;

    *ctx = NULL;

    /*
     * Check that NTLM is enabled before going forward
     */

    if (!gss_mo_get(GSS_NTLM_MECHANISM, GSS_C_NTLM_V1, NULL) &&
	!gss_mo_get(GSS_NTLM_MECHANISM, GSS_C_NTLM_V2, NULL))
    {
	*minor_status = 0;
	return GSS_S_UNAVAILABLE;
    }

    c = calloc(1, sizeof(*c));
    if (c == NULL) {
	*minor_status = ENOMEM;
	return GSS_S_FAILURE;
    }

    c->num_backends = sizeof(_ntlm_interfaces)/sizeof(_ntlm_interfaces[0]);
    if (c->num_backends == 0) {
	free(c);
	*minor_status = EINVAL;
	return GSS_S_FAILURE;
    }

    c->backends = calloc(c->num_backends, sizeof(c->backends[0]));
    if (c->backends == NULL) {
	free(c);
	*minor_status = ENOMEM;
	return GSS_S_FAILURE;
    }

    for (i = 0; i < c->num_backends; i++) {
	struct ntlm_server_interface *iface = _ntlm_interfaces[i];

	maj_stat = (*iface->nsi_init)(minor_status, &c->backends[i].ctx);
	if (GSS_ERROR(maj_stat))
	    continue;

	maj_stat = (*iface->nsi_probe)(minor_status, c->backends[i].ctx, domain, &c->probe_flags);
	if (GSS_ERROR(maj_stat)) {
	    OM_uint32 junk;
	    (*iface->nsi_destroy)(&junk, c->backends[i].ctx);
	    c->backends[i].ctx = NULL;
	} else {
	    _gss_mg_log(1, "ntlm-asc-probe: %s supported", iface->nsi_name);
	}

	c->backends[i].interface = iface;
	found = 1;
    }

    if (!found) {
	OM_uint32 junk;
	gss_ctx_id_t gctx = (gss_ctx_id_t)c;
	_gss_ntlm_delete_sec_context(&junk, &gctx, NULL);
	return GSS_S_UNAVAILABLE;
    }

    *ctx = c;

    return GSS_S_COMPLETE;

}

	
static OM_uint32
build_type2_packet(OM_uint32 *minor_status,
		   ntlm_ctx c,
		   uint32_t flags,
		   const char *hostname,
		   const char *domain,
		   uint32_t *ret_flags,
		   struct ntlm_buf *out)
{
    struct ntlm_type2 type2;
    struct ntlm_buf data;
    size_t i;
    int ret;
    
    _gss_mg_log(1, "ntlm-asc-type2");

    memset(&type2, 0, sizeof(type2));

    type2.flags = NTLM_NEG_UNICODE | NTLM_NEG_NTLM;

    if ((flags & NTLM_NEG_UNICODE) == 0) {
	*minor_status = HNTLM_ERR_OEM;
	return gss_mg_set_error_string(GSS_NTLM_MECHANISM, GSS_S_FAILURE,
				       HNTLM_ERR_OEM, "ntlm: ntlm");
    }

    memcpy(type2.challenge, c->challenge, sizeof(type2.challenge));

    type2.flags |=
	NTLM_NEG_TARGET |
	NTLM_NEG_VERSION |
	NTLM_TARGET_DOMAIN |
	NTLM_ENC_128 |
	NTLM_NEG_TARGET_INFO |
	NTLM_NEG_SIGN |
	NTLM_NEG_SEAL |
	NTLM_NEG_ALWAYS_SIGN |
	NTLM_NEG_NTLM2_SESSION |
	NTLM_NEG_KEYEX;

    type2.os[0] = 0x0601b01d;
    type2.os[1] = 0x0000000f;

    /* Go over backends and find best match, if not found use empty targetinfo */
    for (i = 0; i < c->num_backends; i++) {
	uint32_t negNtlmFlags = 0;
	if ( c->backends[i].ctx == NULL)
	    continue;
	c->backends[i].interface->nsi_ti(minor_status, c,
					 c->backends[i].ctx,
					 hostname, domain, &negNtlmFlags);
	/* figure out flags that the plugin doesn't support */
	type2.flags &= ~negNtlmFlags;
    }

    if (c->ti.domainname) {
	struct ntlm_buf d;

	ret = heim_ntlm_encode_targetinfo(&c->ti, 1, &d);
	if (ret) {
	    *minor_status = ENOMEM;
	    return gss_mg_set_error_string(GSS_NTLM_MECHANISM, GSS_S_FAILURE,
					   *minor_status, "out of memory");
	}
	c->targetinfo.data = d.data;
	c->targetinfo.length = d.length;

    } else {
	/* Send end of seq of targetinfo */
	c->targetinfo.data = malloc(4);
	c->targetinfo.length = 4;
	memcpy(c->targetinfo.data, "\x00\x00\x00\x00", 4);
    }

    type2.targetinfo = c->targetinfo;
    type2.targetname = c->ti.domainname;

    /* If we can't find a targetname, provide one, not having one make heim_ntlm_encode_type2 crash */
    if (type2.targetname == NULL)
	type2.targetname = "BUILTIN";

    ret = heim_ntlm_encode_type2(&type2, &data);
    if (ret) {
	*minor_status = ret;
	return GSS_S_FAILURE;
    }
	
    out->data = data.data;
    out->length = data.length;

    *ret_flags = type2.flags;

    return GSS_S_COMPLETE;
}



/*
 *
 */

OM_uint32
_gss_ntlm_accept_sec_context
(OM_uint32 * minor_status,
 gss_ctx_id_t * context_handle,
 const gss_cred_id_t acceptor_cred_handle,
 const gss_buffer_t input_token_buffer,
 const gss_channel_bindings_t input_chan_bindings,
 gss_name_t * src_name,
 gss_OID * mech_type,
 gss_buffer_t output_token,
 OM_uint32 * ret_flags,
 OM_uint32 * time_rec,
 gss_cred_id_t * delegated_cred_handle
    )
{
    krb5_error_code ret;
    struct ntlm_buf data;
    OM_uint32 junk;
    ntlm_ctx ctx;

    output_token->value = NULL;
    output_token->length = 0;

    *minor_status = 0;

    if (context_handle == NULL)
	return GSS_S_FAILURE;
	
    if (input_token_buffer == GSS_C_NO_BUFFER)
	return GSS_S_FAILURE;

    if (src_name)
	*src_name = GSS_C_NO_NAME;
    if (mech_type)
	*mech_type = GSS_C_NO_OID;
    if (ret_flags)
	*ret_flags = 0;
    if (time_rec)
	*time_rec = 0;
    if (delegated_cred_handle)
	*delegated_cred_handle = GSS_C_NO_CREDENTIAL;

    if (*context_handle == GSS_C_NO_CONTEXT) {
	struct ntlm_type1 type1;
	OM_uint32 major_status;
	OM_uint32 retflags;
	struct ntlm_buf out;

	major_status = _gss_ntlm_allocate_ctx(minor_status, NULL, &ctx);
	if (major_status)
	    return major_status;
	*context_handle = (gss_ctx_id_t)ctx;
	
	if (CCRandomCopyBytes(kCCRandomDefault, ctx->challenge, sizeof(ctx->challenge))) {
	    *minor_status = HNTLM_ERR_RAND;
	    return gss_mg_set_error_string(GSS_NTLM_MECHANISM, GSS_S_FAILURE,
					   HNTLM_ERR_RAND, "rand failed");
	}

	{
	    static krb5_context context;
	    static dispatch_once_t once;
	    dispatch_once(&once, ^{
		    krb5_init_context(&context);
		});
	    if (context)
		krb5_kcm_ntlm_challenge(context, ctx->challenge);
	}

	data.data = input_token_buffer->value;
	data.length = input_token_buffer->length;
	
	ret = heim_ntlm_decode_type1(&data, &type1);
	if (ret) {
	    _gss_ntlm_delete_sec_context(minor_status, context_handle, NULL);
	    *minor_status = ret;
	    return GSS_S_FAILURE;
	}

	if ((type1.flags & NTLM_NEG_UNICODE) == 0) {
	    heim_ntlm_free_type1(&type1);
	    _gss_ntlm_delete_sec_context(minor_status, context_handle, NULL);
	    *minor_status = HNTLM_ERR_OEM;
	    return gss_mg_set_error_string(GSS_NTLM_MECHANISM, GSS_S_FAILURE,
					   HNTLM_ERR_OEM, "not unicode");
	}

	ret = krb5_data_copy(&ctx->type1, data.data, data.length);
	if (ret) {
	    _gss_ntlm_delete_sec_context(minor_status, context_handle, NULL);
	    *minor_status = ret;
	    return GSS_S_FAILURE;
	}

	major_status = build_type2_packet(minor_status,
					  ctx,
					  type1.flags,
					  type1.hostname,
					  type1.domain,
					  &retflags,
					  &out);
	heim_ntlm_free_type1(&type1);
	if (major_status != GSS_S_COMPLETE) {
	    _gss_mg_log(1, "ntlm-asc-type2 failed with: %d", (int)major_status);
	    _gss_ntlm_delete_sec_context(&junk, context_handle, NULL);
	    return major_status;
	}

	output_token->value = malloc(out.length);
	if (output_token->value == NULL && out.length != 0) {
	    heim_ntlm_free_buf(&out);
	    _gss_ntlm_delete_sec_context(&junk, context_handle, NULL);
	    _gss_mg_log(1, "ntlm-asc-type2 failed with no packet");
	    *minor_status = ENOMEM;
	    return GSS_S_FAILURE;
	}
	memcpy(output_token->value, out.data, out.length);
	output_token->length = out.length;

	ret = krb5_data_copy(&ctx->type2, out.data, out.length);
	heim_ntlm_free_buf(&out);
	if (ret) {
	    _gss_ntlm_delete_sec_context(minor_status, context_handle, NULL);
	    _gss_mg_log(1, "ntlm-asc-type2 failed to copy type2 packet");
	    *minor_status = ret;
	    return GSS_S_FAILURE;
	}

	if (mech_type)
	    *mech_type = GSS_NTLM_MECHANISM;

	ctx->flags = retflags;

	return GSS_S_CONTINUE_NEEDED;
    } else {
	ntlm_cred acceptor_cred = (ntlm_name)acceptor_cred_handle;
	struct ntlm_buf session, uuid, pac;
	uint32_t ntlmflags = 0, avflags = 0;
	struct ntlm_type3 type3;
	ntlm_name name = NULL;
	OM_uint32 maj_stat;
	size_t i;

	ctx = (ntlm_ctx)*context_handle;

	_gss_mg_log(1, "ntlm-asc-type3");

	data.data = input_token_buffer->value;
	data.length = input_token_buffer->length;

	ret = heim_ntlm_decode_type3(&data, 
				     (ctx->flags & NTLM_NEG_UNICODE),
				     &type3);
	if (ret) {
	    _gss_ntlm_delete_sec_context(minor_status, context_handle, NULL);
	    *minor_status = ret;
	    return GSS_S_FAILURE;
	}

	ret = krb5_data_copy(&ctx->type3, data.data, data.length);
	if (ret) {
	    _gss_ntlm_delete_sec_context(minor_status, context_handle, NULL);
	    *minor_status = ret;
	    return GSS_S_FAILURE;
	}

	/* Go over backends and check for a success */
	maj_stat = GSS_S_FAILURE;
	*minor_status = 0;
	for (i = 0; i < ctx->num_backends; i++) {
	    if (ctx->backends[i].ctx == NULL)
		continue;

	    maj_stat = ctx->backends[i].interface->nsi_type3(minor_status,
							     ctx,
							     ctx->backends[i].ctx,
							     &type3,
							     acceptor_cred,
							     &ntlmflags,
							     &avflags,
							     &session,
							     &name,
							     &uuid,
							     &pac);
	    _gss_mg_log(10, "ntlm-asc-type3: tried %s -> %d/%d",
			ctx->backends[i].interface->nsi_name, 
			(int)maj_stat, (int)*minor_status);
	    if (maj_stat == GSS_S_COMPLETE)
		break;
	}
	if (i >= ctx->num_backends) {
	    heim_ntlm_free_type3(&type3);
	    _gss_ntlm_delete_sec_context(&junk, context_handle, NULL);
	    return maj_stat;
	}

	if ((avflags & NTLM_TI_AV_FLAG_MIC) && type3.mic_offset == 0) {
	    _gss_mg_log(1, "ntlm-asc-type3 mic missing from reply");
	    *minor_status = EAUTH;
	    return GSS_S_FAILURE;
	}

	_gss_ntlm_debug_key(10, "session key", session.data, session.length);

	if (session.length && type3.mic_offset) {
	    uint8_t *p = (uint8_t *)ctx->type3.data + type3.mic_offset;
	    CCHmacContext c;
	    memset(p, 0, sizeof(type3.mic));

	    CCHmacInit(&c, kCCHmacAlgMD5, session.data, session.length);
	    CCHmacUpdate(&c, ctx->type1.data, ctx->type1.length);
	    CCHmacUpdate(&c, ctx->type2.data, ctx->type2.length);
	    CCHmacUpdate(&c, ctx->type3.data, ctx->type3.length);
	    CCHmacFinal(&c, p);

	    if (memcmp(p, type3.mic, sizeof(type3.mic)) != 0) {
		_gss_ntlm_debug_hex(10, "mic", type3.mic, sizeof(type3.mic));
		_gss_ntlm_debug_hex(10, "ntlm-asc-type3 mic invalid", p, sizeof(type3.mic));
		free(session.data);
		*minor_status = HNTLM_ERR_INVALID_MIC;
		return GSS_S_FAILURE;
	    }
	}

	if (ntlmflags & NTLM_NEG_ANONYMOUS)
	    ctx->gssflags |= GSS_C_ANON_FLAG;

	ctx->pac.value = pac.data;
	ctx->pac.length = pac.length;

	if (name) {
	    _gss_ntlm_release_name(&junk, &ctx->srcname);
	    ctx->srcname = (gss_name_t)name;
	} else {

	    ctx->srcname = _gss_ntlm_create_name(minor_status, type3.username, type3.targetname, 0);
	    if (ctx->srcname == NULL) {
		free(session.data);
		heim_ntlm_free_type3(&type3);
		_gss_ntlm_delete_sec_context(minor_status, context_handle, NULL);
		return maj_stat;
	    }
	}
	/* XXX pull out the uuid and store in name */
	if (uuid.data) {
	    ntlm_name n = (ntlm_name) ctx->srcname;
	    if (uuid.length == 16) {
		memcpy(n->ds_uuid, uuid.data, uuid.length);
		n->flags |= NTLM_DS_UUID;
	    }
	    free(uuid.data);
	}

	/* do some logging */
	{
	    ntlm_name n = (ntlm_name) ctx->srcname;
	    _gss_mg_log(1, "ntlm-asc-type3: %s\\%s", 
			n->domain, n->user);
	}

	if (src_name)
	    _gss_ntlm_duplicate_name(&junk, ctx->srcname, src_name);

	heim_ntlm_free_type3(&type3);

	ret = krb5_data_copy(&ctx->sessionkey,
			     session.data, session.length);
	free(session.data);
	if (ret) {	
	    if (src_name)
		_gss_ntlm_release_name(&junk, src_name);
	    _gss_ntlm_delete_sec_context(minor_status, context_handle, NULL);
	    *minor_status = ret;
	    return GSS_S_FAILURE;
	}
	
	_gss_ntlm_set_keys(ctx);

	if (mech_type)
	    *mech_type = GSS_NTLM_MECHANISM;
	if (time_rec)
	    *time_rec = GSS_C_INDEFINITE;

	ctx->status |= STATUS_OPEN;

	if (ret_flags)
	    *ret_flags = ctx->gssflags;

	return GSS_S_COMPLETE;
    }
}
