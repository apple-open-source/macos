/*
 * Copyright (c) 1997 - 2004 Kungliga Tekniska Högskolan
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
#include <gssapi_spi.h>

static OM_uint32
_gss_ntlm_have_cred(OM_uint32 *minor,
		    const ntlm_name target_name,
		    ntlm_cred *rcred)
{
    krb5_context context;
    krb5_error_code ret;
    krb5_storage *request, *response;
    krb5_data response_data;
    OM_uint32 major;
    ntlm_name cred;
    kcmuuid_t uuid;
    ssize_t sret;

    ret = krb5_init_context(&context);
    if (ret) {
	*minor = ret;
	return GSS_S_FAILURE;
    }

    ret = krb5_kcm_storage_request(context, KCM_OP_HAVE_NTLM_CRED, &request);
    if (ret)
	goto out;

    ret = krb5_store_stringz(request, target_name->user);
    if (ret)
	goto out;

    ret = krb5_store_stringz(request, target_name->domain);
    if (ret)
	goto out;

    ret = krb5_kcm_call(context, request, &response, &response_data);
    krb5_storage_free(request);
    if (ret)
	goto out;

    sret = krb5_storage_read(response, uuid, sizeof(uuid));

    krb5_storage_free(response);
    krb5_data_free(&response_data);

    if (sret != sizeof(uuid)) {
	krb5_clear_error_message(context);
	return KRB5_CC_IO;
    }

    major = _gss_ntlm_duplicate_name(minor, (gss_name_t)target_name,
				     (gss_name_t *)&cred);
    if (major)
	return major;

    cred->flags |= NTLM_UUID;
    memcpy(cred->uuid, uuid, sizeof(cred->uuid));

    *rcred = (ntlm_cred)cred;
 out:
    krb5_free_context(context);
    if (ret) {
	*minor = ret;
	major = GSS_S_FAILURE;
    }

    return major;
}

OM_uint32
_gss_ntlm_acquire_cred(OM_uint32 * min_stat,
		       const gss_name_t desired_name,
		       OM_uint32 time_req,
		       const gss_OID_set desired_mechs,
		       gss_cred_usage_t cred_usage,
		       gss_cred_id_t * output_cred_handle,
		       gss_OID_set * actual_mechs,
		       OM_uint32 * time_rec)
{
    ntlm_name name = (ntlm_name) desired_name;
    OM_uint32 maj_stat, junk;
    ntlm_ctx ctx;

    *min_stat = 0;
    *output_cred_handle = GSS_C_NO_CREDENTIAL;
    if (actual_mechs)
	*actual_mechs = GSS_C_NO_OID_SET;
    if (time_rec)
	*time_rec = GSS_C_INDEFINITE;

    if (desired_name == NULL)
	return GSS_S_NO_CRED;

    if (cred_usage == GSS_C_BOTH || cred_usage == GSS_C_ACCEPT) {
	gss_ctx_id_t gctx;

	maj_stat = _gss_ntlm_allocate_ctx(min_stat, name->domain, &ctx);
	if (maj_stat != GSS_S_COMPLETE)
	    return maj_stat;

	gctx = (gss_ctx_id_t)ctx;
	_gss_ntlm_delete_sec_context(&junk, &gctx, NULL);
    }	
    if (cred_usage == GSS_C_BOTH || cred_usage == GSS_C_INITIATE) {
	ntlm_cred cred;

	/* if we have a anon name, lets dup it directly */
	if ((name->flags & NTLM_ANON_NAME) != 0) {
	    maj_stat = _gss_ntlm_duplicate_name(min_stat,
						(gss_name_t)name,
						(gss_name_t *)&cred);
	    if (maj_stat)
		return maj_stat;
	} else {
	    maj_stat = _gss_ntlm_have_cred(min_stat, name, &cred);
	    if (maj_stat)
		return maj_stat;
	}
	*output_cred_handle = (gss_cred_id_t)cred;
    }

    return (GSS_S_COMPLETE);
}

OM_uint32
_gss_ntlm_acquire_cred_ext(OM_uint32 * minor_status,
			   const gss_name_t desired_name,
			   gss_const_OID credential_type,
			   const void *credential_data,
			   OM_uint32 time_req,
			   gss_const_OID desired_mech,
			   gss_cred_usage_t cred_usage,
			   gss_cred_id_t * output_cred_handle)
{
    ntlm_name name = (ntlm_name) desired_name;
    OM_uint32 major;
    krb5_storage *request, *response;
    krb5_data response_data;
    krb5_context context;
    krb5_error_code ret;
    struct ntlm_buf buf;
    krb5_data data;
    ntlm_cred dn;
    kcmuuid_t uuid;
    ssize_t sret;

    if (credential_data == NULL)
	return GSS_S_FAILURE;

    if (!gss_oid_equal(credential_type, GSS_C_CRED_PASSWORD))
	return GSS_S_FAILURE;

    if (name == NULL)
	return GSS_S_FAILURE;
	
    ret = krb5_init_context(&context);
    if (ret)
	return GSS_S_FAILURE;

    {
	gss_buffer_t buffer;
	char *password;
	
	buffer = (gss_buffer_t)credential_data;
	password = malloc(buffer->length + 1);
	if (password == NULL) {
	    ret = ENOMEM;
	    goto out;
	}
	memcpy(password, buffer->value, buffer->length);
	password[buffer->length] = '\0';
	
	heim_ntlm_nt_key(password, &buf);
	memset(password, 0, strlen(password));
	free(password);
    }

    data.data = buf.data;
    data.length = buf.length;
	
    krb5_kcm_storage_request(context, KCM_OP_ADD_NTLM_CRED, &request);
	
    krb5_store_stringz(request, name->user);
    krb5_store_stringz(request, name->domain);
    krb5_store_data(request, data);
    
    ret = krb5_kcm_call(context, request, &response, &response_data);
    krb5_storage_free(request);
    if (ret)
	goto out;
    
    sret = krb5_storage_read(response, &uuid, sizeof(uuid));

    krb5_storage_free(response);
    krb5_data_free(&response_data);

    if (sret != sizeof(uuid)) {
	ret = KRB5_CC_IO;
	goto out;
    }

    heim_ntlm_free_buf(&buf);

    dn = calloc(1, sizeof(*dn));
    if (dn == NULL) {
	major = GSS_S_FAILURE;
	goto out;
    }

    dn->user = strdup(name->user);
    dn->domain = strdup(name->domain);
    dn->flags = NTLM_UUID;
    memcpy(dn->uuid, uuid, sizeof(dn->uuid));

    *output_cred_handle = (gss_cred_id_t)dn;
    
    major = GSS_S_COMPLETE;
 out:

    krb5_free_context(context);
    if (ret)
	major = GSS_S_FAILURE;

    return major;
}
