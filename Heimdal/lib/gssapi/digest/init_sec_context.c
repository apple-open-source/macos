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

#include "gssdigest.h"
#include "heimcred.h"
#include "scram.h"

#ifdef HAVE_KCM
static int
calculate(void *ptr,
	  heim_scram_method method,
	  unsigned int iterations,
	  heim_scram_data *salt,
	  const heim_scram_data *c1,
	  const heim_scram_data *s1,
	  const heim_scram_data *c2noproof,
	  heim_scram_data *proof,
	  heim_scram_data *server,
	  heim_scram_data *sessionKey)
{
    krb5_error_code ret;
    krb5_context context;
    krb5_storage *request, *response = NULL;
    krb5_data response_data;
    scram_id_t ctx = ptr;

    ret = krb5_init_context(&context);
    if (ret) {
	return ret;
    }
    ret = krb5_kcm_storage_request(context, KCM_OP_DO_SCRAM_AUTH, &request);
    if (ret) {
	krb5_free_context(context);
	return GSS_S_FAILURE;
    }
    ret = krb5_store_stringz(request, ctx->client);
    if (ret == 0)
	ret = krb5_store_uint32(request, iterations);
    if (ret == 0)
	ret = krb5_store_data(request, *salt);
    if (ret == 0)
	ret = krb5_store_data(request, *c1);
    if (ret == 0)
	ret = krb5_store_data(request, *s1);
    if (ret == 0)
	ret = krb5_store_data(request, *c2noproof);
    if (ret == 0)
	ret = krb5_kcm_call(context, request, &response, &response_data);
    
    krb5_free_context(context);
    krb5_storage_free(request);
    if (ret == 0)
    ret = krb5_ret_data(response, proof);
    if (ret == 0)
	ret = krb5_ret_data(response, server);
    if (ret == 0)
	ret = krb5_ret_data(response, sessionKey);
    if (ret)
	ret = KRB5_CC_IO;
    
    if (response)
	krb5_storage_free(response);
    krb5_data_free(&response_data);

    return ret;
    
}

static struct heim_scram_client scram_client = {
	SCRAM_CLIENT_VERSION_1,
	calculate
};

#else /* !HAVE_KCM */

static int
calculate(void *ptr,
	  heim_scram_method method,
	  unsigned int iterations,
	  heim_scram_data *salt,
	  const heim_scram_data *c1,
	  const heim_scram_data *s1,
	  const heim_scram_data *c2noproof,
	  heim_scram_data *proof,
	  heim_scram_data *server,
	  heim_scram_data *sessionKey)
{
    krb5_error_code ret = 0;
    scram_id_t ctx = ptr;

    HeimCredRef credential_cfcred = NULL;
    CFDictionaryRef attrs = NULL;
    CFUUIDRef uuid_cfuuid = NULL;
    CFDictionaryRef response_cfdict = NULL;
    CFErrorRef cferr = NULL;

    CFStringRef userName = NULL;
    CFNumberRef iterations_number = NULL;
    CFDataRef salt_data = NULL;
    CFDataRef c1_data = NULL;
    CFDataRef s1_data = NULL;
    CFDataRef c2noproof_data = NULL;

    CFDataRef proof_data = NULL;
    CFDataRef server_data = NULL;
    CFDataRef sessionkey_data = NULL;

    scram_data_zero(proof);
    scram_data_zero(server);
    scram_data_zero(sessionKey);

    CFUUIDBytes cfuuid;
    memcpy(&cfuuid, &ctx->client->uuid, sizeof(ctx->client->uuid));
    uuid_cfuuid = CFUUIDCreateFromUUIDBytes(kCFAllocatorDefault, cfuuid );
    credential_cfcred = HeimCredCopyFromUUID(uuid_cfuuid);

    userName = CFStringCreateWithCString(NULL, ctx->client->name, kCFStringEncodingUTF8);
    if (!userName) {
	ret = KRB5_CC_IO;
	goto out;
    }

    iterations_number = CFNumberCreate(NULL, kCFNumberIntType, &iterations);
    if (!iterations_number) {
	ret = KRB5_CC_IO;
	goto out;
    }

    salt_data = CFDataCreate(kCFAllocatorDefault, (const UInt8 *)salt->data, salt->length );
    if (!salt_data) {
	ret = KRB5_CC_IO;
	goto out;
    }

    c1_data = CFDataCreate(kCFAllocatorDefault, (const UInt8 *)c1->data, c1->length );
    if (!c1_data) {
	ret = KRB5_CC_IO;
	goto out;
    }

    s1_data = CFDataCreate(kCFAllocatorDefault, (const UInt8 *)s1->data, s1->length );
    if (!s1_data) {
	ret = KRB5_CC_IO;
	goto out;
    }

    c2noproof_data = CFDataCreate(kCFAllocatorDefault, (const UInt8 *)c2noproof->data, c2noproof->length );
    if (!c2noproof_data) {
	ret = KRB5_CC_IO;
	goto out;
    }

    const void *auth_keys[] = {
    (void *)kHEIMObjectType,
	    kHEIMAttrType,
	    kHEIMAttrSCRAMUsername,
	    kHEIMAttrSCRAMIterations,
	    kHEIMAttrSCRAMSalt,
	    kHEIMAttrSCRAMC1,
	    kHEIMAttrSCRAMS1,
	    kHEIMAttrSCRAMC2NoProof,
    };
    const void *auth_values[] = {
    (void *)kHEIMObjectSCRAM,
	    kHEIMTypeSCRAM,
	    userName,
	    iterations_number,
	    salt_data,
	    c1_data,
	    s1_data,
	    c2noproof_data,
    };

    attrs = CFDictionaryCreate(NULL, auth_keys, auth_values, sizeof(auth_keys) / sizeof(auth_keys[0]), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    response_cfdict = HeimCredDoSCRAM(credential_cfcred, attrs, &cferr);
    CFRELEASE_NULL(attrs);

    if (response_cfdict) {

	proof_data = CFDictionaryGetValue(response_cfdict, kHEIMAttrSCRAMProof);
	if (proof_data) {
	    proof->length = CFDataGetLength(proof_data);
	    proof->data = calloc(1, proof->length);
	    if (proof->data == NULL) {
		ret = ENOMEM;
		goto out;
	    }
	    CFDataGetBytes(proof_data, CFRangeMake(0, CFDataGetLength(proof_data)), proof->data);
	} else {
	    ret = KRB5_CC_IO;
	    goto out;
	}

	server_data = CFDictionaryGetValue(response_cfdict, kHEIMAttrSCRAMServer);
	if (server_data) {
	    server->length = CFDataGetLength(server_data);
	    server->data = calloc(1, server->length);
	    if (server->data == NULL) {
		ret = ENOMEM;
		goto out;
	    }
	    CFDataGetBytes(server_data, CFRangeMake(0, CFDataGetLength(server_data)), server->data);
	} else {
	    ret = KRB5_CC_IO;
	    goto out;
	}

	sessionkey_data = CFDictionaryGetValue(response_cfdict, kHEIMAttrSCRAMSessionKey);
	if (sessionkey_data) {
	    sessionKey->length = CFDataGetLength(sessionkey_data);
	    sessionKey->data = calloc(1, sessionKey->length);
	    if (sessionKey->data == NULL) {
		ret = ENOMEM;
		goto out;
	    }
	    CFDataGetBytes(sessionkey_data, CFRangeMake(0, CFDataGetLength(sessionkey_data)), sessionKey->data);
	} else {
	    ret = KRB5_CC_IO;
	    goto out;
	}
	
	ret = 0;
    }

out:

    CFRELEASE_NULL(userName);
    CFRELEASE_NULL(iterations_number);
    CFRELEASE_NULL(salt_data);
    CFRELEASE_NULL(c1_data);
    CFRELEASE_NULL(s1_data);
    CFRELEASE_NULL(c2noproof_data);

    CFRELEASE_NULL(credential_cfcred);
    CFRELEASE_NULL(attrs);
    CFRELEASE_NULL(response_cfdict);

    return ret;

}

static struct heim_scram_client scram_client = {
	SCRAM_CLIENT_VERSION_1,
	calculate
};

#endif  /* HAVE_KCM */

OM_uint32
_gss_scram_init_sec_context(OM_uint32 * minor_status,
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
    heim_scram_data in, out;
    scram_id_t ctx;
    int ret;
    
    *minor_status = 0;
    
    if (ret_flags)
	*ret_flags = 0;
    if (time_rec)
	*time_rec = 0;
    if (actual_mech_type)
	*actual_mech_type = GSS_C_NO_OID;
    
    ctx = (scram_id_t) *context_handle;

    if (actual_mech_type)
	*actual_mech_type = GSS_SCRAM_MECHANISM;
    if (ret_flags)
	*ret_flags = 0;

    if (ctx == NULL) {
        gss_buffer_desc outraw;

        if (initiator_cred_handle == GSS_C_NO_CREDENTIAL)
	    return GSS_S_FAILURE;
#ifdef HAVE_KCM
	major = _gss_scram_have_cred(minor_status, (char *)initiator_cred_handle, NULL);
#else
	scram_cred cred = (scram_cred)initiator_cred_handle;

	major = _gss_scram_have_cred(minor_status, cred->name, NULL);
#endif
	if (major != GSS_S_COMPLETE)
	    return major;

	ctx = calloc(1, sizeof(*ctx));
	if (ctx == NULL) {
	    *minor_status = ENOMEM;
	    return gss_mg_set_error_string(GSS_SCRAM_MECHANISM, GSS_S_FAILURE,
					   ENOMEM, "out of memory");
	}
	*context_handle = (gss_ctx_id_t) ctx;

#ifdef HAVE_KCM
        ctx->client = strdup((char *)initiator_cred_handle);
	ret = heim_scram_client1(ctx->client, NULL, HEIM_SCRAM_DIGEST_SHA1,
				 &ctx->scram, &out);
#else
	ctx->client = cred;
	ret = heim_scram_client1(ctx->client->name, NULL, HEIM_SCRAM_DIGEST_SHA1,
				 &ctx->scram, &out);
#endif
	if (ret) {
	  _gss_scram_delete_sec_context(minor_status, context_handle, NULL);
	  *minor_status = ret;
	  return GSS_S_FAILURE;
	}

	outraw.value = out.data;
	outraw.length = out.length;

	major = gss_encapsulate_token(&outraw,
				      GSS_SCRAM_MECHANISM,
				      output_token);
	if (major) {
	    _gss_scram_delete_sec_context(minor_status, context_handle, NULL);
	    *minor_status = ENOMEM;
	    return GSS_S_FAILURE;
	}

	ctx->flags = req_flags;

#ifdef HAVE_KCM
	_gss_mg_log(1, "scram-isc-client1: %s",
		    ctx->client);
#else
	_gss_mg_log(1, "scram-isc-client1: %s",
		    ctx->client->name);
#endif
	ctx->state = CLIENT2;
	
	major = GSS_S_CONTINUE_NEEDED;

    } else if (ctx->state == CLIENT2) {

	in.data = input_token->value;
	in.length = input_token->length;
	
	ret = heim_scram_client2(&in, &scram_client, ctx, ctx->scram, &out);
	if (ret) {
	    _gss_scram_delete_sec_context(minor_status, context_handle, NULL);
	    *minor_status = ret;
	    return gss_mg_set_error_string(GSS_SCRAM_MECHANISM, GSS_S_FAILURE, ret,
					   "failed to create scram response");
	}

	output_token->length = out.length;
	output_token->value = calloc(1, out.length);
	
	memcpy(output_token->value, out.data, out.length);

	ctx->state = CLIENT3;

	/*
	 * Here we have session key, pull it out and set prot ready
	 */

	major = GSS_S_CONTINUE_NEEDED;

    } else if (ctx->state == CLIENT3) {

	in.data = input_token->value;
	in.length = input_token->length;
	
	ret = heim_scram_client3(&in, ctx->scram);
	if (ret) {
	    _gss_scram_delete_sec_context(minor_status, context_handle, NULL);
	    *minor_status = ret;
	    return gss_mg_set_error_string(GSS_SCRAM_MECHANISM, GSS_S_FAILURE, ret,
					   "failed to verify server response");
	}

	output_token->length = 0;
	output_token->value = NULL;

	ctx->status |= STATUS_OPEN;

	major = GSS_S_COMPLETE;

    } else {
	abort();
    }

    if (ret_flags)
	*ret_flags = ctx->flags;

    if (time_rec)
	*time_rec = GSS_C_INDEFINITE;

    return major;
}
