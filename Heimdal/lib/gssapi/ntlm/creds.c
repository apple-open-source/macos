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

OM_uint32 _gss_ntlm_inquire_cred
           (OM_uint32 * minor_status,
            const gss_cred_id_t cred_handle,
            gss_name_t * name,
            OM_uint32 * lifetime,
            gss_cred_usage_t * cred_usage,
            gss_OID_set * mechanisms
           )
{
    OM_uint32 ret, junk;

    *minor_status = 0;

    if (cred_handle == NULL)
	return GSS_S_NO_CRED;

    if (name) {
	ret = _gss_ntlm_duplicate_name(minor_status,
				       (gss_name_t)cred_handle,
				       name);
	if (ret)
	    goto out;
    }
    if (lifetime)
	*lifetime = GSS_C_INDEFINITE;
    if (cred_usage)
	*cred_usage = 0;
    if (mechanisms)
	*mechanisms = GSS_C_NO_OID_SET;

    if (cred_handle == GSS_C_NO_CREDENTIAL)
	return GSS_S_NO_CRED;

    if (mechanisms) {
        ret = gss_create_empty_oid_set(minor_status, mechanisms);
        if (ret)
	    goto out;
	ret = gss_add_oid_set_member(minor_status,
				     GSS_NTLM_MECHANISM,
				     mechanisms);
        if (ret)
	    goto out;
    }

    return GSS_S_COMPLETE;
out:
    gss_release_oid_set(&junk, mechanisms);
    return ret;
}

OM_uint32
_gss_ntlm_destroy_cred(OM_uint32 *minor_status,
		       gss_cred_id_t *cred_handle)
{
    krb5_error_code ret;
    krb5_storage *request = NULL, *response = NULL;
    krb5_data response_data;
    krb5_context context;
    ntlm_cred cred;
    ssize_t sret;

    if (cred_handle == NULL || *cred_handle == GSS_C_NO_CREDENTIAL)
	return GSS_S_COMPLETE;

    cred = (ntlm_cred)*cred_handle;

    if ((cred->flags & NTLM_UUID) == 0)
	return _gss_ntlm_release_cred(minor_status, cred_handle);

    ret = krb5_init_context(&context);
    if (ret) {
	*minor_status = ret;
	return GSS_S_FAILURE;
    }

    ret = krb5_kcm_storage_request(context, KCM_OP_DESTROY_CRED, &request);
    if (ret) {
	request = NULL;
	goto out;
    }

    sret = krb5_storage_write(request, cred->uuid, sizeof(cred->uuid));
    if (sret != sizeof(cred->uuid)) {
	ret = KRB5_CC_IO;
	goto out;
    }

    ret = krb5_kcm_call(context, request, &response, &response_data);

 out:
    if (request)
        krb5_storage_free(request);
    if (response) {
        krb5_storage_free(response);
	krb5_data_free(&response_data);
    }

    krb5_free_context(context);
    if (ret) {
	*minor_status = ret;
	return GSS_S_FAILURE;
    }

    return _gss_ntlm_release_cred(minor_status, cred_handle);
}

static OM_uint32
change_hold(OM_uint32 *minor_status, ntlm_cred cred, int op)
{
    krb5_storage *request = NULL, *response = NULL;
    krb5_data response_data;
    krb5_context context;
    krb5_error_code ret;
    ssize_t sret;

    *minor_status = 0;

    if (cred == NULL)
	return GSS_S_COMPLETE;

    if ((cred->flags & NTLM_UUID) == 0)
	return GSS_S_COMPLETE;

    ret = krb5_init_context(&context);
    if (ret) {
	*minor_status = ret;
	return GSS_S_FAILURE;
    }

    ret = krb5_kcm_storage_request(context, op, &request);
    if (ret) {
	request = NULL;
	goto out;
    }

    sret = krb5_storage_write(request, cred->uuid, sizeof(cred->uuid));
    if (sret != sizeof(cred->uuid)) {
	ret = KRB5_CC_IO;
	goto out;
    }

    ret = krb5_kcm_call(context, request, &response, &response_data);
    if (ret)
	goto out;

 out:
    if (request)
	krb5_storage_free(request);
    if (response) {
	krb5_storage_free(response);
	krb5_data_free(&response_data);
    }
    krb5_free_context(context);
    if (ret) {
	*minor_status = ret;
	return GSS_S_FAILURE;
    }

    return GSS_S_COMPLETE;
}

OM_uint32
_gss_ntlm_cred_hold(OM_uint32 *minor_status, gss_cred_id_t cred)
{
    return change_hold(minor_status, (ntlm_cred)cred, KCM_OP_RETAIN_CRED);
}

OM_uint32
_gss_ntlm_cred_unhold(OM_uint32 *minor_status, gss_cred_id_t cred)
{
    return change_hold(minor_status, (ntlm_cred)cred, KCM_OP_RELEASE_CRED);
}

OM_uint32
_gss_ntlm_cred_label_get(OM_uint32 *minor_status, gss_cred_id_t cred_handle,
			const char *label, gss_buffer_t value)
{
    ntlm_cred cred = (ntlm_cred)cred_handle;
    krb5_storage *request = NULL, *response = NULL;
    krb5_data response_data, data;
    krb5_error_code ret;
    krb5_context context;
    ssize_t sret;

    *minor_status = 0;

    if (cred == NULL)
	return GSS_S_COMPLETE;

    if ((cred->flags & NTLM_UUID) == 0)
	return GSS_S_COMPLETE;

    ret = krb5_init_context(&context);
    if (ret) {
	*minor_status = ret;
	return GSS_S_FAILURE;
    }

    ret = krb5_kcm_storage_request(context, KCM_OP_CRED_LABEL_GET, &request);
    if (ret) {
	request = NULL;
	goto out;
    }

    sret = krb5_storage_write(request, cred->uuid, sizeof(cred->uuid));
    if (sret != sizeof(cred->uuid)) {
	ret = KRB5_CC_IO;
	goto out;
    }

    ret = krb5_store_stringz(request, label);
    if (ret)
	goto out;

    ret = krb5_kcm_call(context, request, &response, &response_data);
    if (ret)
	goto out;

    ret = krb5_ret_data(response, &data);
    if (ret)
	goto out;

    value->value = data.data;
    value->length = data.length;

 out:
    if (request)
	krb5_storage_free(request);
    if (response) {
	krb5_storage_free(response);
	krb5_data_free(&response_data);
    }
    krb5_free_context(context);
    if (ret) {
	*minor_status = ret;
	return GSS_S_FAILURE;
    }

    return GSS_S_COMPLETE;
}

OM_uint32
_gss_ntlm_cred_label_set(OM_uint32 *minor_status, gss_cred_id_t cred_handle,
			 const char *label, gss_buffer_t value)
{
    ntlm_cred cred = (ntlm_cred)cred_handle;
    krb5_storage *request = NULL, *response = NULL;
    krb5_data response_data;
    krb5_context context;
    krb5_error_code ret;
    krb5_data data;
    ssize_t sret;

    *minor_status = 0;

    if (cred == NULL)
	return GSS_S_COMPLETE;

    if ((cred->flags & NTLM_UUID) == 0)
	return GSS_S_COMPLETE;

    ret = krb5_init_context(&context);
    if (ret) {
	*minor_status = ret;
	return GSS_S_FAILURE;
    }

    ret = krb5_kcm_storage_request(context, KCM_OP_CRED_LABEL_SET, &request);
    if (ret) {
	request = NULL;
	goto out;
    }

    sret = krb5_storage_write(request, cred->uuid, sizeof(cred->uuid));
    if (sret != sizeof(cred->uuid)) {
	ret = KRB5_CC_IO;
	goto out;
    }

    ret = krb5_store_stringz(request, label);
    if (ret)
	goto out;

    if (value) {
	data.data = value->value;
	data.length = value->length;
    } else {
	krb5_data_zero(&data);
    }

    ret = krb5_store_data(request, data);
    if (ret)
	goto out;

    ret = krb5_kcm_call(context, request, &response, &response_data);
    if (ret)
	goto out;

 out:
    if (request)
	krb5_storage_free(request);
    if (response) {
	krb5_storage_free(response);
	krb5_data_free(&response_data);
    }

    krb5_free_context(context);
    if (ret) {
	*minor_status = ret;
	return GSS_S_FAILURE;
    }

    return GSS_S_COMPLETE;
}
