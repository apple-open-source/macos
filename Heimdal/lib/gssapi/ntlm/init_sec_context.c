/*
 * Copyright (c) 2006 - 2008 Kungliga Tekniska Högskolan
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
#include <fnmatch.h>

static int
validate_hostname(ntlm_name name)
{
    CFArrayRef array;
    CFStringRef el;
    CFIndex n, max;
    char *str;
    int res = 0;

    if (name->domain == NULL)
	return 0;

    array = _gss_mg_copy_key(CFSTR("com.apple.GSS.NTLM"), CFSTR("AllowedHosts"));
    if (array == NULL)
	return 1;

    if (CFGetTypeID(array) != CFArrayGetTypeID()) {
	_gss_mg_log(1, "NTLM: invalid type of AllowedHosts");
	CFRelease(array);
	return 0;
    }

    max = CFArrayGetCount(array);

    for (n = 0, res = 0; n < max && !res; n++) {
	el = CFArrayGetValueAtIndex(array, n);
	if (el == NULL || CFGetTypeID(el) != CFStringGetTypeID())
	    continue;
	str = rk_cfstring2cstring(el);
	if (str == NULL)
	    continue;
	res = (fnmatch(str, name->domain, FNM_CASEFOLD) == 0);
	free(str);
    }

    CFRelease(array);

    return res;
}


static OM_uint32
_gss_ntlm_copy_cred(OM_uint32 * minor, ntlm_cred from, ntlm_cred *to)
{
    return _gss_ntlm_duplicate_name(minor, (gss_name_t)from, (gss_name_t *)to);
}

OM_uint32
_gss_ntlm_init_sec_context(OM_uint32 * minor_status,
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
			   OM_uint32 * time_rec)
{
    OM_uint32 major;
    ntlm_ctx ctx;
    ntlm_name name = (ntlm_name) target_name;
    
    *minor_status = 0;
    
    if (ret_flags)
	*ret_flags = 0;
    if (time_rec)
	*time_rec = 0;
    if (actual_mech_type)
	*actual_mech_type = GSS_C_NO_OID;
    if (name == NULL)
	return GSS_S_BAD_NAME;
    
    /*
     * Check that NTLM is enabled before going forward
     */

    if (!gss_mo_get(GSS_NTLM_MECHANISM, GSS_C_NTLM_V1, NULL) &&
	!gss_mo_get(GSS_NTLM_MECHANISM, GSS_C_NTLM_V2, NULL))
    {
	*minor_status = 0;
	return GSS_S_UNAVAILABLE;
    }

    ctx = (ntlm_ctx) *context_handle;

    if (actual_mech_type)
	*actual_mech_type = GSS_NTLM_MECHANISM;
    if (ret_flags)
	*ret_flags = 0;

    if (ctx == NULL) {
	struct ntlm_type1 type1;
	struct ntlm_buf data;
	uint32_t flags = 0;
	int ret;
	
	if (!validate_hostname(name))
	    return gss_mg_set_error_string(GSS_NTLM_MECHANISM, GSS_S_FAILURE,
					   (*minor_status = EAUTH),
					   "Not allowed to use NTLM to host %s",
					   name->domain ? name->domain : "???");

	ctx = calloc(1, sizeof(*ctx));
	if (ctx == NULL) {
	    *minor_status = ENOMEM;
	    return gss_mg_set_error_string(GSS_NTLM_MECHANISM, GSS_S_FAILURE,
					   ENOMEM, "out of memory");
	}
	*context_handle = (gss_ctx_id_t) ctx;
	
	ctx->status |= STATUS_CLIENT;

	if (initiator_cred_handle != GSS_C_NO_CREDENTIAL) {
	    ntlm_cred cred = (ntlm_cred) initiator_cred_handle;
	    major = _gss_ntlm_copy_cred(minor_status, cred, &ctx->client);
	} else {
	    ntlm_name_desc client_name;
	    client_name.user = "";
	    client_name.domain = "";
	    client_name.flags = 0;

	    major = _gss_ntlm_have_cred(minor_status, &client_name, &ctx->client);
	}

	if (major) {
	    OM_uint32 junk;
	    _gss_ntlm_delete_sec_context(&junk, context_handle, NULL);
	    return major;
	}

	major = _gss_ntlm_duplicate_name(minor_status, (gss_name_t)name, &ctx->targetname);
	if (major) {
	    OM_uint32 junk;
	    _gss_ntlm_delete_sec_context(&junk, context_handle, NULL);
	    return major;
	}

	ret = asprintf(&ctx->clientsuppliedtargetname, "%s/%s", name->user, name->domain);
	if (ret < 0 || ctx->clientsuppliedtargetname == NULL) {
	    OM_uint32 junk;
	    _gss_ntlm_delete_sec_context(&junk, context_handle, NULL);
	    return major;
	}

	flags |= NTLM_NEG_UNICODE;
	flags |= NTLM_NEG_NTLM;
	flags |= NTLM_NEG_TARGET;
	flags |= NTLM_NEG_VERSION;

	if (req_flags & GSS_C_ANON_FLAG) {
	    flags |= NTLM_NEG_ANONYMOUS;
	} else {
	    /* can't work with anon cred, just stop now */
	    if ((ctx->client->flags & NTLM_ANON_NAME) != 0) {
		OM_uint32 junk;
		_gss_ntlm_delete_sec_context(&junk, context_handle, NULL);
		gss_mg_set_error_string(GSS_NTLM_MECHANISM, GSS_S_NO_CRED, 0,
					"Cant authenticate with anon name");
		*minor_status = 0;
		return GSS_S_NO_CRED;
	    }

	    if (req_flags & GSS_C_CONF_FLAG) {
		flags |= NTLM_NEG_SEAL;
		flags |= NTLM_NEG_SIGN;
		flags |= NTLM_NEG_ALWAYS_SIGN;
	    }
	    if (req_flags & GSS_C_INTEG_FLAG) {
		flags |= NTLM_NEG_SIGN;
		flags |= NTLM_NEG_ALWAYS_SIGN;
	    }
	    

	    flags |= NTLM_NEG_NTLM2_SESSION;
	    flags |= NTLM_ENC_128;
	    flags |= NTLM_NEG_KEYEX;
	    flags |= NTLM_NEG_TARGET_INFO;
	}
	
	memset(&type1, 0, sizeof(type1));
	
	type1.flags = flags;
	type1.domain = NULL;
	type1.hostname = NULL;
	type1.os[0] = 0x0601b01d;
	type1.os[1] = 0x0000000f;
	
	ret = heim_ntlm_encode_type1(&type1, &data);
	if (ret) {
	    _gss_ntlm_delete_sec_context(minor_status, context_handle, NULL);
	    *minor_status = ret;
	    return GSS_S_FAILURE;
	}
	output_token->value = data.data;
	output_token->length = data.length;
	
	ret = krb5_data_copy(&ctx->type1, data.data, data.length);
	if (ret) {
	    _gss_ntlm_delete_sec_context(minor_status, context_handle, NULL);
	    *minor_status = ret;
	    return GSS_S_FAILURE;
	}

	ctx->flags = flags;

	_gss_mg_log(1, "ntlm-isc-type1: %s\\%s",
		    ctx->client->domain, ctx->client->user);

	return GSS_S_CONTINUE_NEEDED;

    } else if (ctx->flags & NTLM_NEG_ANONYMOUS) {
	struct ntlm_type2 type2;
	struct ntlm_type3 type3;
	struct ntlm_buf ndata;
	int ret;

	memset(&type2, 0, sizeof(type2));
	memset(&type3, 0, sizeof(type3));

	if (input_token == GSS_C_NO_BUFFER)
	    return GSS_S_FAILURE;

	ndata.data = input_token->value;
	ndata.length = input_token->length;

	ret = heim_ntlm_decode_type2(&ndata, &type2);
	if (ret) {
	    _gss_ntlm_delete_sec_context(minor_status, context_handle, NULL);
	    *minor_status = ret;
	    return GSS_S_FAILURE;
	}

	type3.username = "";
	type3.flags = ctx->flags;
	type3.targetname = "";
	type3.ws = "";

	/* This is crazy, but apperently needed ? */
	type3.lm.data = "\x00";
	type3.lm.length = 1;

	ret = heim_ntlm_encode_type3(&type3, &ndata, NULL);
	heim_ntlm_free_type2(&type2);
	if (ret) {
	    _gss_ntlm_delete_sec_context(minor_status, context_handle, NULL);
	    *minor_status = ret;
	    return GSS_S_FAILURE;
	}

	output_token->value = ndata.data;
	output_token->length = ndata.length;

	ctx->gssflags |= GSS_C_ANON_FLAG;

	ctx->srcname = _gss_ntlm_create_name(minor_status, "", "", 0);
	if (ctx->srcname == NULL) {
	    _gss_ntlm_delete_sec_context(minor_status, context_handle, NULL);
	    return GSS_S_FAILURE;
	}

	_gss_mg_log(1, "ntlm-isc-type3: anonymous");

    } else {
	krb5_context context;
	krb5_error_code ret;
	krb5_storage *request, *response;
	krb5_data response_data, data, cb;
	char *ruser = NULL, *rdomain = NULL;
	uint8_t channelbinding[16];
	
	data.data = input_token->value;
	data.length = input_token->length;

	if (input_chan_bindings) {
	    
	    major = gss_mg_gen_cb(minor_status, input_chan_bindings,
				  channelbinding, NULL);
	    if (major) {
		OM_uint32 junk;
		_gss_ntlm_delete_sec_context(&junk, context_handle, NULL);
		return major;
	    }

	    cb.data = channelbinding;
	    cb.length = sizeof(channelbinding);
	} else
	    krb5_data_zero(&cb);
	
	ret = krb5_init_context(&context);
	if (ret) {
	    _gss_ntlm_delete_sec_context(minor_status, context_handle, NULL);
	    return ret;
	}
	ret = krb5_kcm_storage_request(context, KCM_OP_DO_NTLM_AUTH, &request);
	if (ret) {
	    krb5_free_context(context);
	    _gss_ntlm_delete_sec_context(minor_status, context_handle, NULL);
	    *minor_status = ret;
	    return GSS_S_FAILURE;
	}
	ret = krb5_store_stringz(request, ctx->client->user);
	if (ret == 0)
	    ret = krb5_store_stringz(request, ctx->client->domain);
	if (ret == 0)
	    ret = krb5_store_data(request, data);
	if (ret == 0)
	    ret = krb5_store_data(request, cb);
	if (ret == 0)
	    ret = krb5_store_data(request, ctx->type1);
	if (ret == 0)
	    ret = krb5_store_stringz(request, ctx->clientsuppliedtargetname);
	if (ret == 0)
	    ret = krb5_store_uint32(request, ctx->flags);
	
	if (ret == 0)
	    ret = krb5_kcm_call(context, request, &response, &response_data);

	krb5_free_context(context);
	krb5_storage_free(request);
	if (ret) {
	    _gss_ntlm_delete_sec_context(minor_status, context_handle, NULL);
	    *minor_status = ret;
	    return gss_mg_set_error_string(GSS_NTLM_MECHANISM, GSS_S_FAILURE, ret,
					   "failed to create ntlm response");
	}
	ret = krb5_ret_data(response, &data);
	if (ret == 0)
	    ret = krb5_ret_uint32(response, &ctx->kcmflags);
	if (ret == 0)
	    ret = krb5_ret_data(response, &ctx->sessionkey);
	if (ret == 0)
	    ret = krb5_ret_string(response, &ruser);
	if (ret == 0)
	    ret = krb5_ret_string(response, &rdomain);
	if (ret == 0)
	    ret = krb5_ret_uint32(response, &ctx->flags);
	if (ret)
	    ret = KRB5_CC_IO;
	
	krb5_storage_free(response);
	krb5_data_free(&response_data);
	
	_gss_mg_log(1, "ntlm-isc-type3: kcm returned %d", ret);

	if (ret) {
	    _gss_ntlm_delete_sec_context(minor_status, context_handle, NULL);
	    krb5_data_free(&data);
	    free(ruser);
	    free(rdomain);
	    *minor_status = ret;
	    return gss_mg_set_error_string(GSS_NTLM_MECHANISM, GSS_S_FAILURE, ret,
					   "failed parse kcm reply");
	}

	_gss_mg_log(1, "ntlm-isc-type3: %s\\%s", rdomain, ruser);

	ctx->srcname = _gss_ntlm_create_name(minor_status, ruser, rdomain, 0);
	free(ruser);
	free(rdomain);
	if (ctx->srcname == NULL) {
	    _gss_ntlm_delete_sec_context(minor_status, context_handle, NULL);
	    krb5_data_free(&data);
	    return GSS_S_FAILURE;
	}

	output_token->length = data.length;
	output_token->value = data.data;

    }

    ctx->status |= STATUS_OPEN;

    /*
     * Now that we have a session key, let setup crypto layer
     */

    _gss_ntlm_set_keys(ctx);

    if (ret_flags)
	*ret_flags = ctx->gssflags;

    if (time_rec)
	*time_rec = GSS_C_INDEFINITE;



    return GSS_S_COMPLETE;
}
