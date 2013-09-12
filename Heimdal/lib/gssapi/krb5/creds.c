/*
 * Copyright (c) 2009 Kungliga Tekniska Högskolan
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

#include "gsskrb5_locl.h"

OM_uint32 GSSAPI_CALLCONV
_gsskrb5_export_cred(OM_uint32 *minor_status,
		     gss_cred_id_t cred_handle,
		     gss_buffer_t cred_token)
{
    gsskrb5_cred handle = (gsskrb5_cred)cred_handle;
    krb5_context context;
    krb5_error_code ret;
    krb5_storage *sp;
    krb5_data data, mech;
    const char *type;
    char *str;

    GSSAPI_KRB5_INIT (&context);

    if (handle->usage != GSS_C_INITIATE && handle->usage != GSS_C_BOTH) {
	*minor_status = GSS_KRB5_S_G_BAD_USAGE;
	return GSS_S_FAILURE;
    }

    sp = krb5_storage_emem();
    if (sp == NULL) {
	*minor_status = ENOMEM;
	return GSS_S_FAILURE;
    }

    type = krb5_cc_get_type(context, handle->ccache);
    /*
     * XXX Always use reference keys since that makes it easier to
     * transport between processing in seprate authentication domain.
     *
     * We should encrypt credentials in KCM though using the kcm
     * session key.
     */
    if (1 /*strcmp(type, "MEMORY") == 0 */) {
	krb5_creds *creds;
	ret = krb5_store_uint32(sp, 0);
	if (ret) {
	    krb5_storage_free(sp);
	    *minor_status = ret;
	    return GSS_S_FAILURE;
	}

	ret = _krb5_get_krbtgt(context, handle->ccache,
			       handle->principal->realm,
			       &creds);
	if (ret) {
	    krb5_storage_free(sp);
	    *minor_status = ret;
	    return GSS_S_FAILURE;
	}

	ret = krb5_store_creds(sp, creds);
	krb5_free_creds(context, creds);
	if (ret) {
	    krb5_storage_free(sp);
	    *minor_status = ret;
	    return GSS_S_FAILURE;
	}

    } else {
	ret = krb5_store_uint32(sp, 1);
	if (ret) {
	    krb5_storage_free(sp);
	    *minor_status = ret;
	    return GSS_S_FAILURE;
	}

	ret = krb5_cc_get_full_name(context, handle->ccache, &str);
	if (ret) {
	    krb5_storage_free(sp);
	    *minor_status = ret;
	    return GSS_S_FAILURE;
	}

	ret = krb5_store_string(sp, str);
	free(str);
	if (ret) {
	    krb5_storage_free(sp);
	    *minor_status = ret;
	    return GSS_S_FAILURE;
	}
    }
    ret = krb5_storage_to_data(sp, &data);
    krb5_storage_free(sp);
    if (ret) {
	*minor_status = ret;
	return GSS_S_FAILURE;
    }
    sp = krb5_storage_emem();
    if (sp == NULL) {
	krb5_data_free(&data);
	*minor_status = ENOMEM;
	return GSS_S_FAILURE;
    }

    mech.data = GSS_KRB5_MECHANISM->elements;
    mech.length = GSS_KRB5_MECHANISM->length;

    ret = krb5_store_data(sp, mech);
    if (ret) {
	krb5_data_free(&data);
	krb5_storage_free(sp);
	*minor_status = ret;
	return GSS_S_FAILURE;
    }

    ret = krb5_store_data(sp, data);
    krb5_data_free(&data);
    if (ret) {
	krb5_storage_free(sp);
	*minor_status = ret;
	return GSS_S_FAILURE;
    }

    ret = krb5_storage_to_data(sp, &data);
    krb5_storage_free(sp);
    if (ret) {
	*minor_status = ret;
	return GSS_S_FAILURE;
    }

    cred_token->value = data.data;
    cred_token->length = data.length;

    return GSS_S_COMPLETE;
}

OM_uint32 GSSAPI_CALLCONV
_gsskrb5_import_cred(OM_uint32 * minor_status,
		     gss_buffer_t cred_token,
		     gss_cred_id_t * cred_handle)
{
    krb5_context context;
    krb5_error_code ret;
    gsskrb5_cred handle;
    krb5_ccache id;
    krb5_storage *sp;
    char *str;
    uint32_t type;
    int flags = 0;

    *cred_handle = GSS_C_NO_CREDENTIAL;

    GSSAPI_KRB5_INIT (&context);

    sp = krb5_storage_from_mem(cred_token->value, cred_token->length);
    if (sp == NULL) {
	*minor_status = ENOMEM;
	return GSS_S_FAILURE;
    }

    ret = krb5_ret_uint32(sp, &type);
    if (ret) {
	krb5_storage_free(sp);
	*minor_status = ret;
	return GSS_S_FAILURE;
    }
    switch (type) {
    case 0: {
	krb5_creds creds;

	ret = krb5_ret_creds(sp, &creds);
	krb5_storage_free(sp);
	if (ret) {
	    *minor_status = ret;
	    return GSS_S_FAILURE;
	}

	ret = krb5_cc_new_unique(context, "API", NULL, &id);
	if (ret) {
	    *minor_status = ret;
	    return GSS_S_FAILURE;
	}

	ret = krb5_cc_initialize(context, id, creds.client);
	if (ret) {
	    krb5_cc_destroy(context, id);
	    *minor_status = ret;
	    return GSS_S_FAILURE;
	}

	ret = krb5_cc_store_cred(context, id, &creds);
	krb5_free_cred_contents(context, &creds);
	if (ret) {
	    krb5_cc_destroy(context, id);
	    *minor_status = ret;
	    return GSS_S_FAILURE;
	}

	flags |= GSS_CF_DESTROY_CRED_ON_RELEASE;

	break;
    }
    case 1:
	ret = krb5_ret_string(sp, &str);
	krb5_storage_free(sp);
	if (ret) {
	    *minor_status = ret;
	    return GSS_S_FAILURE;
	}

	ret = krb5_cc_resolve(context, str, &id);
	krb5_xfree(str);
	if (ret) {
	    *minor_status = ret;
	    return GSS_S_FAILURE;
	}
	break;

    default:
	krb5_storage_free(sp);
	*minor_status = 0;
	return GSS_S_NO_CRED;
    }

    handle = calloc(1, sizeof(*handle));
    if (handle == NULL) {
	krb5_cc_close(context, id);
	*minor_status = ENOMEM;
	return GSS_S_FAILURE;
    }

    handle->usage = GSS_C_INITIATE;
    krb5_cc_get_principal(context, id, &handle->principal);
    handle->ccache = id;
    handle->cred_flags = flags;

    if (handle->principal)
      __gsskrb5_ccache_lifetime(minor_status, context,
				id, handle->principal,
				&handle->endtime);

    *cred_handle = (gss_cred_id_t)handle;

    return GSS_S_COMPLETE;
}

OM_uint32
_gsskrb5_destroy_cred(OM_uint32 *minor_status,
		      gss_cred_id_t *cred_handle)
{
    gsskrb5_cred cred = (gsskrb5_cred)*cred_handle;
    cred->cred_flags |= GSS_CF_DESTROY_CRED_ON_RELEASE;
    return _gsskrb5_release_cred(minor_status, cred_handle);
}

static OM_uint32
change_hold(OM_uint32 *minor_status, gsskrb5_cred cred,
	    krb5_error_code (*func)(krb5_context, krb5_ccache))
{
    krb5_error_code ret;
    krb5_context context;
    krb5_data data;

    *minor_status = 0;
    krb5_data_zero(&data);

    GSSAPI_KRB5_INIT (&context);

    if (cred == NULL)
	return GSS_S_COMPLETE;

    if (cred->usage != GSS_C_INITIATE && cred->usage != GSS_C_BOTH) {
	*minor_status = GSS_KRB5_S_G_BAD_USAGE;
	return GSS_S_FAILURE;
    }

    /* XXX only refcount nah-created credentials */
    ret = krb5_cc_get_config(context, cred->ccache, NULL, "nah-created", &data);
    if (ret) {
	*minor_status = ret;
	return GSS_S_FAILURE;
    }
    krb5_data_free(&data);

    ret = func(context, cred->ccache);

    if (ret) {
	*minor_status = ret;
	return GSS_S_FAILURE;
    }

    return GSS_S_COMPLETE;
}

OM_uint32
_gsskrb5_cred_hold(OM_uint32 *minor_status, gss_cred_id_t cred)
{
    return change_hold(minor_status, (gsskrb5_cred)cred, krb5_cc_hold);
}

OM_uint32
_gsskrb5_cred_unhold(OM_uint32 *minor_status, gss_cred_id_t cred)
{
    return change_hold(minor_status, (gsskrb5_cred)cred, krb5_cc_unhold);
}

OM_uint32
_gsskrb5_cred_label_get(OM_uint32 *minor_status, gss_cred_id_t cred_handle,
			const char *label, gss_buffer_t value)
{
    gsskrb5_cred cred = (gsskrb5_cred)cred_handle;
    krb5_context context;
    krb5_error_code ret;
    krb5_data data;

    GSSAPI_KRB5_INIT (&context);

    if (cred == NULL)
	return GSS_S_COMPLETE;

    if (cred->ccache == NULL) {
	*minor_status = GSS_KRB5_S_G_BAD_USAGE;
	return GSS_S_FAILURE;
    }

    ret = krb5_cc_get_config(context, cred->ccache, NULL, label, &data);
    if (ret) {
	*minor_status = ret;
	return GSS_S_FAILURE;
    }

    value->value = data.data;
    value->length = data.length;

    return GSS_S_COMPLETE;
}

OM_uint32
_gsskrb5_cred_label_set(OM_uint32 *minor_status, gss_cred_id_t cred_handle,
			const char *label, gss_buffer_t value)
{
    gsskrb5_cred cred = (gsskrb5_cred)cred_handle;
    krb5_context context;
    krb5_error_code ret;
    krb5_data data, *datap = NULL;

    GSSAPI_KRB5_INIT (&context);

    if (cred == NULL)
	return GSS_S_COMPLETE;

    if (cred->ccache == NULL) {
	*minor_status = GSS_KRB5_S_G_BAD_USAGE;
	return GSS_S_FAILURE;
    }

    if (value) {
	data.data = value->value;
	data.length = value->length;
	datap = &data;
    }

    ret = krb5_cc_set_config(context, cred->ccache, NULL, label, datap);
    if (ret) {
	*minor_status = ret;
	return GSS_S_FAILURE;
    }

    return GSS_S_COMPLETE;
}


OM_uint32
_gsskrb5_appl_change_password(OM_uint32 *minor_status,
			      gss_name_t name,
			      const char *oldpw,
			      const char *newpw)
{
    krb5_data result_code_string, result_string;
    krb5_get_init_creds_opt *opt = NULL;
    krb5_context context;
    krb5_principal principal = (krb5_principal)name;
    krb5_creds kcred;
    krb5_error_code ret;
    int result_code;

    GSSAPI_KRB5_INIT (&context);

    memset(&kcred, 0, sizeof(kcred));

    ret = krb5_get_init_creds_opt_alloc(context, &opt);
    if (ret)
	goto out;

    krb5_get_init_creds_opt_set_tkt_life(opt, 300);
    krb5_get_init_creds_opt_set_forwardable(opt, FALSE);
    krb5_get_init_creds_opt_set_proxiable(opt, FALSE);

    ret = krb5_get_init_creds_password(context,
				       &kcred,
				       principal,
				       oldpw,
				       NULL,
				       NULL,
				       0,
				       "kadmin/changepw",
				       opt);
    if (ret)
	goto out;
    
    ret = krb5_set_password(context, &kcred, newpw, NULL,
			    &result_code, &result_code_string, &result_string);
    if (ret)
	goto out;

    krb5_data_free(&result_string);
    krb5_data_free(&result_code_string);

    if (result_code) {
	krb5_set_error_message(context, KRB5KRB_AP_ERR_BAD_INTEGRITY, 
			       "Failed to change invalid password: %d", result_code);
	ret = KRB5KRB_AP_ERR_BAD_INTEGRITY;
	goto out;
    }

 out:
    if (opt)
	krb5_get_init_creds_opt_free(context, opt);
    krb5_free_cred_contents(context, &kcred);

    *minor_status = ret;

    return ret ? GSS_S_FAILURE : GSS_S_COMPLETE;
}


