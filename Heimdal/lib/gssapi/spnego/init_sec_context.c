/*
 * Copyright (c) 1997 - 2004 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * Portions Copyright (c) 2004 PADL Software Pty Ltd.
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

#include "spnego_locl.h"

/*
 * Is target_name an sane target for `mech´.
 */

struct mech_selection {
    OM_uint32 req_flags;
    gss_name_t target_name;
    OM_uint32 time_req;
    gss_channel_bindings_t input_chan_bindings;
    /* out */
    gss_OID preferred_mech_type;
    gss_OID negotiated_mech_type;
    gss_buffer_desc optimistic_token;
    OM_uint32 optimistic_flags;
    gss_ctx_id_t gssctx;
    int complete;
};


static OM_uint32
initiator_approved(void *userptr,
		   gss_name_t target_name,
		   const gss_cred_id_t cred,
		   gss_OID mech)
{
    OM_uint32 min_stat, maj_stat;
    gss_ctx_id_t ctx = GSS_C_NO_CONTEXT;
    gss_buffer_desc out;
    struct mech_selection *sel = userptr;
    gss_OID negotiated_mech_type = GSS_C_NO_OID;
    OM_uint32 flags = 0;

    maj_stat = gss_init_sec_context(&min_stat,
				    cred,
				    &ctx,
				    sel->target_name,
				    mech,
				    sel->req_flags,
				    sel->time_req,
				    sel->input_chan_bindings,
				    GSS_C_NO_BUFFER,
				    &negotiated_mech_type,
				    &out,
				    &flags,
				    NULL);
    if (GSS_ERROR(maj_stat)) {
	gss_mg_collect_error(mech, GSS_S_BAD_MECH, min_stat);
	return GSS_S_BAD_MECH;
    }
    if (sel->preferred_mech_type == NULL) {
	sel->preferred_mech_type = mech;
	sel->negotiated_mech_type = negotiated_mech_type;
	sel->optimistic_token = out;
	sel->optimistic_flags = flags;
	sel->gssctx = ctx;
	if (maj_stat == GSS_S_COMPLETE)
	    sel->complete = 1;
    } else {
	gss_release_buffer(&min_stat, &out);
	gss_delete_sec_context(&min_stat, &ctx, NULL);
    }

    return GSS_S_COMPLETE;
}

/*
 * Send a reply. Note that we only need to send a reply if we
 * need to send a MIC or a mechanism token. Otherwise, we can
 * return an empty buffer.
 *
 * The return value of this will be returned to the API, so it
 * must return GSS_S_CONTINUE_NEEDED if a token was generated.
 */
static OM_uint32
spnego_reply_internal(OM_uint32 *minor_status,
		      gssspnego_ctx ctx,
		      const gss_buffer_t mech_buf,
		      gss_buffer_t mech_token,
		      gss_buffer_t output_token)
{
    NegotiationToken nt;
    gss_buffer_desc mic_buf;
    OM_uint32 ret;
    int accept_completed_set = 0;
    size_t size;

    if (mech_buf == GSS_C_NO_BUFFER && mech_token->length == 0) {
	output_token->length = 0;
	output_token->value = NULL;

	return ctx->open ? GSS_S_COMPLETE : GSS_S_FAILURE;
    }

    memset(&nt, 0, sizeof(nt));

    nt.element = choice_NegotiationToken_negTokenResp;
    nt.u.negTokenResp.negResult = NULL;
    nt.u.negTokenResp.supportedMech = NULL;

    output_token->length = 0;
    output_token->value = NULL;

    if (mech_token->length == 0) {
	nt.u.negTokenResp.responseToken = NULL;
	accept_completed_set = 1;
    } else {
	ALLOC(nt.u.negTokenResp.responseToken, 1);
	if (nt.u.negTokenResp.responseToken == NULL) {
	    free_NegotiationToken(&nt);
	    *minor_status = ENOMEM;
	    return GSS_S_FAILURE;
	}
	nt.u.negTokenResp.responseToken->length = mech_token->length;
	nt.u.negTokenResp.responseToken->data   = mech_token->value;
	mech_token->length = 0;
	mech_token->value  = NULL;
    }

    if (mech_buf != GSS_C_NO_BUFFER) {

	ret = gss_get_mic(minor_status,
			  ctx->negotiated_ctx_id,
			  0,
			  mech_buf,
			  &mic_buf);
	if (ret == GSS_S_COMPLETE) {
	    ALLOC(nt.u.negTokenResp.mechListMIC, 1);
	    if (nt.u.negTokenResp.mechListMIC == NULL) {
		gss_release_buffer(minor_status, &mic_buf);
		free_NegotiationToken(&nt);
		*minor_status = ENOMEM;
		return GSS_S_FAILURE;
	    }

	    nt.u.negTokenResp.mechListMIC->length = mic_buf.length;
	    nt.u.negTokenResp.mechListMIC->data   = mic_buf.value;
	} else if (ret == GSS_S_UNAVAILABLE) {
	    nt.u.negTokenResp.mechListMIC = NULL;
	} if (ret) {
	    free_NegotiationToken(&nt);
	    *minor_status = ENOMEM;
	    return GSS_S_FAILURE;
	}
    } else {
	nt.u.negTokenResp.mechListMIC = NULL;
    }

    /*
     * If this is NTLM we have negotiated, don't include the negResult
     * since Samba 3.0.28a (Mac OS X SnowLeopard) can't handle them.
     */
    
    if (gss_oid_equal(ctx->negotiated_mech_type, GSS_NTLM_MECHANISM) == 0) {
	ALLOC(nt.u.negTokenResp.negResult, 1);
	if (nt.u.negTokenResp.negResult == NULL) {
	    free_NegotiationToken(&nt);
	    *minor_status = ENOMEM;
	    return GSS_S_FAILURE;
	}
	*nt.u.negTokenResp.negResult =
	    (accept_completed_set) ? accept_completed : accept_incomplete;
    }

    ASN1_MALLOC_ENCODE(NegotiationToken,
		       output_token->value, output_token->length,
		       &nt, &size, ret);
    free_NegotiationToken(&nt);
    if (ret) {
	*minor_status = ret;
	return GSS_S_FAILURE;
    }

    if (!accept_completed_set)
	return GSS_S_CONTINUE_NEEDED;

    return GSS_S_COMPLETE;
}

static OM_uint32
spnego_initial
           (OM_uint32 * minor_status,
	    gss_cred_id_t cred,
            gss_ctx_id_t * context_handle,
            const gss_name_t target_name,
            const gss_OID mech_type,
            OM_uint32 req_flags,
            OM_uint32 time_req,
            const gss_channel_bindings_t input_chan_bindings,
            const gss_buffer_t input_token,
            gss_OID * actual_mech_type,
            gss_buffer_t output_token,
            OM_uint32 * ret_flags,
            OM_uint32 * time_rec
    )
{
    NegotiationToken nt;
    int ret;
    OM_uint32 sub, minor;
    gss_buffer_desc mech_token;
    size_t size;
    gss_buffer_desc data;
    gss_ctx_id_t context;
    gssspnego_ctx ctx;
    spnego_name name = (spnego_name)target_name;
    struct mech_selection sel;

    *minor_status = 0;

    memset (&nt, 0, sizeof(nt));

    *context_handle = GSS_C_NO_CONTEXT;

    if (target_name == GSS_C_NO_NAME)
	return GSS_S_BAD_NAME;

    sub = _gss_spnego_alloc_sec_context(&minor, &context);
    if (GSS_ERROR(sub)) {
	*minor_status = minor;
	return sub;
    }
    ctx = (gssspnego_ctx)context;

    HEIMDAL_MUTEX_lock(&ctx->ctx_id_mutex);

    ctx->local = 1;

    sub = gss_import_name(&minor, &name->value, &name->type, &ctx->target_name);
    if (GSS_ERROR(sub)) {
	*minor_status = minor;
	_gss_spnego_internal_delete_sec_context(&minor, &context, GSS_C_NO_BUFFER);
	return sub;
    }

    nt.element = choice_NegotiationToken_negTokenInit;

    memset(&sel, 0, sizeof(sel));

    sel.target_name = ctx->target_name;
    sel.preferred_mech_type = GSS_C_NO_OID;
    sel.req_flags = req_flags;
    sel.time_req = time_req;
    sel.input_chan_bindings = (gss_channel_bindings_t)input_chan_bindings;

    sub = _gss_spnego_indicate_mechtypelist(&minor,
					    ctx->target_name,
					    initiator_approved,
					    &sel,
					    0,
					    cred,
					    &nt.u.negTokenInit.mechTypes,
					    &ctx->preferred_mech_type);
    if (GSS_ERROR(sub)) {
	*minor_status = minor;
	_gss_spnego_internal_delete_sec_context(&minor, &context, GSS_C_NO_BUFFER);
	return sub;
    }

    nt.u.negTokenInit.reqFlags = NULL;

    if (sel.gssctx == NULL) {
	free_NegotiationToken(&nt);
	_gss_spnego_internal_delete_sec_context(&minor, &context, GSS_C_NO_BUFFER);
	*minor_status = 0;
	return gss_mg_set_error_string(GSS_C_NO_OID, GSS_S_NO_CONTEXT, 0,
				       "SPNEGO could not find a prefered mechanism");
    }

    /* optimistic token from selection context */
    mech_token = sel.optimistic_token;
    ctx->mech_flags = sel.optimistic_flags;
    ctx->negotiated_mech_type = sel.negotiated_mech_type;
    ctx->negotiated_ctx_id = sel.gssctx;
    ctx->maybe_open = sel.complete;

    if (mech_token.length != 0) {
	ALLOC(nt.u.negTokenInit.mechToken, 1);
	if (nt.u.negTokenInit.mechToken == NULL) {
	    free_NegotiationToken(&nt);
	    gss_release_buffer(&minor, &mech_token);
	    _gss_spnego_internal_delete_sec_context(&minor, &context, GSS_C_NO_BUFFER);
	    *minor_status = ENOMEM;
	    return GSS_S_FAILURE;
	}
	nt.u.negTokenInit.mechToken->length = mech_token.length;
	nt.u.negTokenInit.mechToken->data = malloc(mech_token.length);
	if (nt.u.negTokenInit.mechToken->data == NULL && mech_token.length != 0) {
	    free_NegotiationToken(&nt);
	    gss_release_buffer(&minor, &mech_token);
	    *minor_status = ENOMEM;
	    _gss_spnego_internal_delete_sec_context(&minor, &context, GSS_C_NO_BUFFER);
	    return GSS_S_FAILURE;
	}
	memcpy(nt.u.negTokenInit.mechToken->data, mech_token.value, mech_token.length);
	gss_release_buffer(&minor, &mech_token);
    } else
	nt.u.negTokenInit.mechToken = NULL;

    nt.u.negTokenInit.mechListMIC = NULL;

    ctx->initiator_mech_types.len = nt.u.negTokenInit.mechTypes.len;
    ctx->initiator_mech_types.val = nt.u.negTokenInit.mechTypes.val;

    ASN1_MALLOC_ENCODE(NegotiationToken, data.value, data.length, &nt, &size, ret);
    nt.u.negTokenInit.mechTypes.len = 0;
    nt.u.negTokenInit.mechTypes.val = NULL;
    free_NegotiationToken(&nt);
    if (ret) {
	_gss_spnego_internal_delete_sec_context(&minor, &context, GSS_C_NO_BUFFER);
	return GSS_S_FAILURE;
    }
    if (data.length != size)
	abort();

    sub = gss_encapsulate_token(&data,
				GSS_SPNEGO_MECHANISM,
				output_token);
    free (data.value);

    if (sub) {
	_gss_spnego_internal_delete_sec_context(&minor, &context, GSS_C_NO_BUFFER);
	return sub;
    }

    if (actual_mech_type)
	*actual_mech_type = ctx->negotiated_mech_type;
    if (ret_flags)
	*ret_flags = ctx->mech_flags;
    if (time_rec)
	*time_rec = ctx->mech_time_rec;

    HEIMDAL_MUTEX_unlock(&ctx->ctx_id_mutex);

    *context_handle = context;

    return GSS_S_CONTINUE_NEEDED;
}

static OM_uint32
spnego_reply
           (OM_uint32 * minor_status,
	    const gss_cred_id_t cred,
            gss_ctx_id_t * context_handle,
            const gss_name_t target_name,
            const gss_OID mech_type,
            OM_uint32 req_flags,
            OM_uint32 time_req,
            const gss_channel_bindings_t input_chan_bindings,
            const gss_buffer_t input_token,
            gss_OID * actual_mech_type,
            gss_buffer_t output_token,
            OM_uint32 * ret_flags,
            OM_uint32 * time_rec
    )
{
    OM_uint32 ret, minor;
    NegotiationToken resp;
    int require_mic;
    size_t buf_len;
    gss_buffer_desc mic_buf, mech_buf;
    gss_buffer_desc mech_output_token;
    gssspnego_ctx ctx;

    *minor_status = 0;

    ctx = (gssspnego_ctx)*context_handle;

    output_token->length = 0;
    output_token->value  = NULL;

    mech_output_token.length = 0;
    mech_output_token.value = NULL;

    mech_buf.value = NULL;
    mech_buf.length = 0;

    ret = decode_NegotiationToken(input_token->value, input_token->length,
				  &resp, NULL);
    if (ret)
      return ret;

    if (resp.element != choice_NegotiationToken_negTokenResp) {
	free_NegotiationToken(&resp);
	*minor_status = 0;
	return GSS_S_BAD_MECH;
    }

    if (resp.u.negTokenResp.negResult == NULL
	|| *(resp.u.negTokenResp.negResult) == reject)
    {
	free_NegotiationToken(&resp);
	return GSS_S_BAD_MECH;
    }

    /*
     * Pick up the mechanism that the acceptor selected, only allow it
     * to be sent in packet.
     */

    HEIMDAL_MUTEX_lock(&ctx->ctx_id_mutex);

    if (resp.u.negTokenResp.supportedMech) {
	gss_OID_desc oid;
	size_t len;

	ctx->seen_supported_mech = 1;

	oid.length = der_length_oid(resp.u.negTokenResp.supportedMech);
	oid.elements = malloc(oid.length);
	if (oid.elements == NULL) {
	    free_NegotiationToken(&resp);
	    HEIMDAL_MUTEX_unlock(&ctx->ctx_id_mutex);
	    return GSS_S_BAD_MECH;
	}
	ret = der_put_oid(((uint8_t *)oid.elements) + oid.length - 1,
			  oid.length,
			  resp.u.negTokenResp.supportedMech,
			  &len);
	if (ret || len != oid.length) {
	    free(oid.elements);
	    free_NegotiationToken(&resp);
	    HEIMDAL_MUTEX_unlock(&ctx->ctx_id_mutex);
	    return GSS_S_BAD_MECH;
	}

	if (gss_oid_equal(GSS_SPNEGO_MECHANISM, &oid)) {
	    free(oid.elements);
	    free_NegotiationToken(&resp);
	    HEIMDAL_MUTEX_unlock(&ctx->ctx_id_mutex);
	    return GSS_S_BAD_MECH;
	}

	/* check if the acceptor took our optimistic token */
	if (!gss_oid_equal(ctx->preferred_mech_type, &oid) &&
	    (!gss_oid_equal(ctx->preferred_mech_type, GSS_KRB5_MECHANISM) ||
	     !gss_oid_equal(&oid, &_gss_spnego_mskrb_mechanism_oid_desc)))
	{
	    free(oid.elements);
	    gss_delete_sec_context(&minor, &ctx->negotiated_ctx_id,
				   GSS_C_NO_BUFFER);
	    ctx->negotiated_ctx_id = GSS_C_NO_CONTEXT;
	} else {
	    gss_duplicate_oid(minor_status, &oid, &ctx->selected_mech_type);
	    free(oid.elements);
	}

    } else if (!ctx->seen_supported_mech) {
	free_NegotiationToken(&resp);
	HEIMDAL_MUTEX_unlock(&ctx->ctx_id_mutex);
	return GSS_S_BAD_MECH;
    }

    /* if a token (of non zero length), or no context, pass to underlaying mech */
    if ((resp.u.negTokenResp.responseToken != NULL && resp.u.negTokenResp.responseToken->length) ||
	ctx->negotiated_ctx_id == GSS_C_NO_CONTEXT) {
	gss_buffer_desc mech_input_token;

	if (resp.u.negTokenResp.responseToken) {
	    mech_input_token.length = resp.u.negTokenResp.responseToken->length;
	    mech_input_token.value  = resp.u.negTokenResp.responseToken->data;
	} else {
	    mech_input_token.length = 0;
	    mech_input_token.value = NULL;
	}

	/* Fall through as if the negotiated mechanism
	   was requested explicitly */
	ret = gss_init_sec_context(&minor,
				   cred,
				   &ctx->negotiated_ctx_id,
				   ctx->target_name,
				   ctx->selected_mech_type,
				   req_flags,
				   time_req,
				   input_chan_bindings,
				   &mech_input_token,
				   &ctx->negotiated_mech_type,
				   &mech_output_token,
				   &ctx->mech_flags,
				   &ctx->mech_time_rec);
	if (GSS_ERROR(ret)) {
	    HEIMDAL_MUTEX_unlock(&ctx->ctx_id_mutex);
	    free_NegotiationToken(&resp);
	    gss_mg_collect_error(ctx->selected_mech_type, ret, minor);
	    *minor_status = minor;
	    return ret;
	}
	if (ret == GSS_S_COMPLETE) {
	    ctx->open = 1;
	}
    } else if (*(resp.u.negTokenResp.negResult) == accept_completed) {
	if (ctx->maybe_open)
	    ctx->open = 1;
    }

    if (*(resp.u.negTokenResp.negResult) == request_mic) {
	ctx->require_mic = 1;
    }

    if (ctx->open) {
	/*
	 * Verify the mechListMIC if CFX was used and a non-preferred
	 * mechanism was selected.
	 */
	require_mic = _gss_spnego_require_mechlist_mic(ctx);
	
	/*
	 * If the peer sent mechListMIC, verify it unless its a
	 * windows 2000 server that sends the responseToken inside the
	 * mechListMIC too. We only accept this condition if
	 * require_mic is not set.
	 */
	if (resp.u.negTokenResp.mechListMIC) {
	    heim_octet_string *m = resp.u.negTokenResp.mechListMIC;
	    
	    if (require_mic == 0 && resp.u.negTokenResp.responseToken && 
		der_heim_octet_string_cmp(m, resp.u.negTokenResp.responseToken) == 0) {
		/* windows 2000 server */
	    } else
		require_mic = 1;
	}
    } else {
	require_mic = 0;
    }

    if (require_mic) {
	ASN1_MALLOC_ENCODE(MechTypeList, mech_buf.value, mech_buf.length,
			   &ctx->initiator_mech_types, &buf_len, ret);
	if (ret) {
	    HEIMDAL_MUTEX_unlock(&ctx->ctx_id_mutex);
	    free_NegotiationToken(&resp);
	    gss_release_buffer(&minor, &mech_output_token);
	    *minor_status = ret;
	    return GSS_S_FAILURE;
	}
	if (mech_buf.length != buf_len)
	    abort();

	if (resp.u.negTokenResp.mechListMIC == NULL) {
	    HEIMDAL_MUTEX_unlock(&ctx->ctx_id_mutex);
	    free(mech_buf.value);
	    free_NegotiationToken(&resp);
	    *minor_status = 0;
	    return gss_mg_set_error_string(GSS_SPNEGO_MECHANISM,
		GSS_S_DEFECTIVE_TOKEN, 0,
	        "SPNEGO Server didn't send mechListMIC when it was required");
	}
	mic_buf.length = resp.u.negTokenResp.mechListMIC->length;
	mic_buf.value  = resp.u.negTokenResp.mechListMIC->data;

	if (mech_output_token.length == 0) {
	    ret = gss_verify_mic(minor_status,
				 ctx->negotiated_ctx_id,
				 &mech_buf,
				 &mic_buf,
				 NULL);
	   if (ret) {
		HEIMDAL_MUTEX_unlock(&ctx->ctx_id_mutex);
		free(mech_buf.value);
		gss_release_buffer(&minor, &mech_output_token);
		free_NegotiationToken(&resp);
		return gss_mg_set_error_string(GSS_SPNEGO_MECHANISM,
		    GSS_S_DEFECTIVE_TOKEN, *minor_status,
	            "SPNEGO server sent invalid mechListMIC");
	    }
	    ctx->verified_mic = 1;
	}
    }

    ret = spnego_reply_internal(minor_status, ctx,
				require_mic ? &mech_buf : NULL,
				&mech_output_token,
				output_token);

    if (mech_buf.value != NULL)
	free(mech_buf.value);

    free_NegotiationToken(&resp);
    gss_release_buffer(&minor, &mech_output_token);

    if (actual_mech_type)
	*actual_mech_type = ctx->negotiated_mech_type;
    if (ret_flags)
	*ret_flags = ctx->mech_flags;
    if (time_rec)
	*time_rec = ctx->mech_time_rec;

    HEIMDAL_MUTEX_unlock(&ctx->ctx_id_mutex);
    return ret;
}

OM_uint32 _gss_spnego_init_sec_context
           (OM_uint32 * minor_status,
            const gss_cred_id_t initiator_cred_handle,
            gss_ctx_id_t * context_handle,
            const gss_name_t target_name,
            const gss_OID mech_type,
            OM_uint32 req_flags,
            OM_uint32 time_req,
            const gss_channel_bindings_t input_chan_bindings,
            const gss_buffer_t input_token,
            gss_OID * actual_mech_type,
            gss_buffer_t output_token,
            OM_uint32 * ret_flags,
            OM_uint32 * time_rec
           )
{
    if (*context_handle == GSS_C_NO_CONTEXT)
	return spnego_initial (minor_status,
			       initiator_cred_handle,
			       context_handle,
			       target_name,
			       mech_type,
			       req_flags,
			       time_req,
			       input_chan_bindings,
			       input_token,
			       actual_mech_type,
			       output_token,
			       ret_flags,
			       time_rec);
    else
	return spnego_reply (minor_status,
			     initiator_cred_handle,
			     context_handle,
			     target_name,
			     mech_type,
			     req_flags,
			     time_req,
			     input_chan_bindings,
			     input_token,
			     actual_mech_type,
			     output_token,
			     ret_flags,
			     time_rec);
}

