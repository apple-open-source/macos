/*
 * Copyright (c) 1997 - 2005 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2009 - 2010 Apple Inc. All rights reserved.
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
#include <heim_threads.h>
#include <gssapi_spi.h>
#include <pkinit_asn1.h>
#include <hex.h>

OM_uint32
__gsskrb5_ccache_lifetime(OM_uint32 *minor_status,
			  krb5_context context,
			  krb5_ccache id,
			  krb5_principal principal,
			  time_t *endtime)
{
    krb5_creds in_cred, out_cred;
    krb5_const_realm realm;
    krb5_error_code kret;

    memset(&in_cred, 0, sizeof(in_cred));
    in_cred.client = principal;

    realm = krb5_principal_get_realm(context,  principal);
    if (realm == NULL) {
	_gsskrb5_clear_status ();
	*minor_status = KRB5_PRINC_NOMATCH; /* XXX */
	return GSS_S_FAILURE;
    }

    kret = krb5_make_principal(context, &in_cred.server,
			       realm, KRB5_TGS_NAME, realm, NULL);
    if (kret) {
	*minor_status = kret;
	return GSS_S_FAILURE;
    }

    kret = krb5_cc_retrieve_cred(context, id, 0, &in_cred, &out_cred);
    krb5_free_principal(context, in_cred.server);
    if (kret) {
	*minor_status = 0;
	*endtime = 0;
	return GSS_S_COMPLETE;
    }

    *endtime = out_cred.times.endtime;
    krb5_free_cred_contents(context, &out_cred);

    return GSS_S_COMPLETE;
}

/*
 * Check if there is at least one entry in the keytab before
 * declaring it as an useful keytab.
 */

static int
check_keytab(krb5_context context,
	     gsskrb5_cred handle,
	     const char *service,
	     int require_lkdc)
{
    krb5_keytab_entry tmp;
    krb5_error_code ret;
    krb5_kt_cursor c;
    int found = 0;

    ret = krb5_kt_start_seq_get (context, handle->keytab, &c);
    if (ret)
	return 0;
    while (!found && krb5_kt_next_entry(context, handle->keytab, &tmp, &c) == 0) {
	krb5_principal principal = tmp.principal;

	if (service) {
	    if (principal->name.name_string.len < 1
		|| strcmp(principal->name.name_string.val[0], service) != 0)
		goto next;
	}
	if (require_lkdc) {
	    if (krb5_principal_is_lkdc(context, principal))
		found = 1;
	    if (krb5_principal_is_pku2u(context, principal))
		found = 1;
	} else
	    found = 1;
      next:
	krb5_kt_free_entry(context, &tmp);
    }
    krb5_kt_end_seq_get (context, handle->keytab, &c);

    return found;
}

/*
 *
 */

static krb5_error_code
get_keytab(krb5_context context, gsskrb5_cred handle, int require_lkdc)
{
    krb5_error_code kret;

    HEIMDAL_MUTEX_lock(&gssapi_keytab_mutex);

    if (_gsskrb5_keytab != NULL) {
	char *name = NULL;

	kret = krb5_kt_get_full_name(context, _gsskrb5_keytab, &name);
	if (kret == 0) {
	    kret = krb5_kt_resolve(context, name, &handle->keytab);
	    krb5_xfree(name);
	}
    } else
	kret = krb5_kt_default(context, &handle->keytab);

    if (kret)
	goto out;

    /*
     * If caller requested, check that we have the user in the keytab.
     */

    if (handle->principal) {
	krb5_keytab_entry entry;

	if (krb5_principal_is_gss_hostbased_service(context, handle->principal)) {
	    /* 
	     * check if we have a service in the keytab
	     */
	    const char *service = handle->principal->name.name_string.val[0];

	    if (!check_keytab(context, handle, service, require_lkdc)) {
		kret = KRB5_KT_NOTFOUND;
		krb5_set_error_message(context, kret,
				       "Didn't find service %s in keytab", service);
		goto out;
	    }
	} else {
	    kret = krb5_kt_get_entry(context, handle->keytab, handle->principal,
				     0, 0, &entry);
	    if (kret)
		goto out;

	    /*
	     * Update the name with the entry from the keytab in case we
	     * have a gss hostname service name principal
	     */
	    krb5_free_principal(context, handle->principal);
	    kret = krb5_copy_principal(context, entry.principal, &handle->principal);
	    krb5_kt_free_entry(context, &entry);
	    if (kret)
		goto out;
	}

    } else {
	if (!check_keytab(context, handle, NULL, require_lkdc)) {
	    kret = KRB5_KT_NOTFOUND;
	    goto out;
	}
    }

 out:
    if (kret && handle->keytab) {
	krb5_kt_close(context, handle->keytab);
	handle->keytab = NULL;
    }

    HEIMDAL_MUTEX_unlock(&gssapi_keytab_mutex);

    return (kret);
}

static OM_uint32 acquire_initiator_cred
		  (OM_uint32 * minor_status,
		   krb5_context context,
		   const gss_name_t desired_name,
		   OM_uint32 time_req,
		   gss_cred_usage_t cred_usage,
		   gsskrb5_cred handle
		  )
{
    OM_uint32 ret = GSS_S_FAILURE;
    krb5_creds cred;
    krb5_principal def_princ = NULL;
    krb5_get_init_creds_opt *opt;
    krb5_ccache ccache = NULL;
    krb5_error_code kret;

    memset(&cred, 0, sizeof(cred));

    /*
     * If we have a preferred principal, lets try to find it in all
     * caches, otherwise, fall back to default cache, ignore all
     * errors while searching.
     */

    if (handle->principal) {
	kret = krb5_cc_cache_match (context,
				    handle->principal,
				    &ccache);
	if (kret == 0) {
	    goto found;
	}
    }

    if (ccache == NULL) {
	kret = krb5_cc_default(context, &ccache);
	if (kret)
	    goto end;
    }
    kret = krb5_cc_get_principal(context, ccache, &def_princ);
    if (kret != 0) {
	/* we'll try to use a keytab below */
	krb5_cc_close(context, ccache);
	def_princ = NULL;
	kret = 0;
    } else if (handle->principal == NULL)  {
	kret = krb5_copy_principal(context, def_princ, &handle->principal);
	if (kret)
	    goto end;
    } else if (handle->principal != NULL &&
	       krb5_principal_compare(context, handle->principal,
				      def_princ) == FALSE) {
	krb5_free_principal(context, def_princ);
	def_princ = NULL;
	krb5_cc_close(context, ccache);
	ccache = NULL;
    }
    if (def_princ == NULL) {
	/* We have no existing credentials cache,
	 * so attempt to get a TGT using a keytab.
	 */
	if (handle->principal == NULL) {
	    kret = krb5_get_default_principal(context, &handle->principal);
	    if (kret)
		goto end;
	}
	/*
	 * Require user is in the keytab before trying to talk to
	 * the KDC.
	 */
	kret = get_keytab(context, handle, 0);
	if (kret)
	    goto end;
	/* since the name might have changed, let double check the credential cache */
	kret = krb5_cc_cache_match(context, handle->principal, &ccache);
	if (kret == 0)
	    goto found;
	kret = krb5_get_init_creds_opt_alloc(context, &opt);
	if (kret)
	    goto end;
	kret = krb5_get_init_creds_keytab(context, &cred,
					  handle->principal, handle->keytab,
					  0, NULL, opt);
	krb5_get_init_creds_opt_free(context, opt);
	if (kret)
	    goto end;
	kret = krb5_cc_new_unique(context, krb5_cc_type_memory, NULL, &ccache);
	if (kret)
	    goto end;
	kret = krb5_cc_initialize(context, ccache, cred.client);
	if (kret) {
	    krb5_cc_destroy(context, ccache);
	    goto end;
	}
	kret = krb5_cc_store_cred(context, ccache, &cred);
	if (kret) {
	    krb5_cc_destroy(context, ccache);
	    goto end;
	}
	handle->endtime = cred.times.endtime;
	handle->cred_flags |= GSS_CF_DESTROY_CRED_ON_RELEASE;

    } else {
    found:
	ret = __gsskrb5_ccache_lifetime(minor_status,
					context,
					ccache,
					handle->principal,
					&handle->endtime);
	if (ret != GSS_S_COMPLETE) {
	    krb5_cc_close(context, ccache);
	    goto end;
	}
	kret = 0;
    }

    handle->ccache = ccache;
    ret = GSS_S_COMPLETE;

end:
    if (cred.client != NULL)
	krb5_free_cred_contents(context, &cred);
    if (def_princ != NULL)
	krb5_free_principal(context, def_princ);
    if (ret != GSS_S_COMPLETE && kret != 0)
	*minor_status = kret;
    return (ret);
}

static OM_uint32 acquire_acceptor_cred
		  (OM_uint32 * minor_status,
		   krb5_context context,
		   const gss_name_t desired_name,
		   OM_uint32 time_req,
		   gss_cred_usage_t cred_usage,
		   gsskrb5_cred handle
		  )
{
    krb5_error_code kret;

    kret = get_keytab(context, handle, 0);

    if (kret) {
	if (handle->keytab != NULL) {
	    krb5_kt_close(context, handle->keytab);
	    handle->keytab = NULL;
	}
	*minor_status = kret;
	return GSS_S_FAILURE;
    }

    handle->endtime = INT_MAX;

    return GSS_S_COMPLETE;
}

OM_uint32 GSSAPI_CALLCONV
_gsskrb5_acquire_cred(OM_uint32 * minor_status,
		      const gss_name_t desired_name,
		      OM_uint32 time_req,
		      const gss_OID_set desired_mechs,
		      gss_cred_usage_t cred_usage,
		      gss_cred_id_t * output_cred_handle,
		      gss_OID_set * actual_mechs,
		      OM_uint32 * time_rec)
{
    krb5_context context;
    gsskrb5_cred handle;
    OM_uint32 ret, junk;

    cred_usage &= GSS_C_OPTION_MASK;

    if (cred_usage != GSS_C_ACCEPT && cred_usage != GSS_C_INITIATE && cred_usage != GSS_C_BOTH) {
	*minor_status = GSS_KRB5_S_G_BAD_USAGE;
	return GSS_S_FAILURE;
    }

    GSSAPI_KRB5_INIT(&context);

    *output_cred_handle = NULL;

    handle = calloc(1, sizeof(*handle));
    if (handle == NULL) {
	*minor_status = ENOMEM;
        return (GSS_S_FAILURE);
    }

    HEIMDAL_MUTEX_init(&handle->cred_id_mutex);

    if (desired_name != GSS_C_NO_NAME) {
	krb5_error_code kret;
	
	kret = krb5_copy_principal(context,
				   (krb5_const_principal)desired_name,
				   &handle->principal);
	if (kret) {
	    _gsskrb5_release_cred(&junk, (gss_cred_id_t *)&handle);
	    *minor_status = kret;
	    return GSS_S_FAILURE;
	}
    }
    if (cred_usage == GSS_C_INITIATE || cred_usage == GSS_C_BOTH) {
	ret = acquire_initiator_cred(minor_status, context,
				     desired_name, time_req,
				     cred_usage, handle);
    	if (ret != GSS_S_COMPLETE) {
	    HEIMDAL_MUTEX_destroy(&handle->cred_id_mutex);
	    krb5_free_principal(context, handle->principal);
	    free(handle);
	    return (ret);
	}
    }
    if (cred_usage == GSS_C_ACCEPT || cred_usage == GSS_C_BOTH) {
	ret = acquire_acceptor_cred(minor_status, context,
				    desired_name, time_req,
				    cred_usage, handle);
	if (ret != GSS_S_COMPLETE) {
	    HEIMDAL_MUTEX_destroy(&handle->cred_id_mutex);
	    krb5_free_principal(context, handle->principal);
	    free(handle);
	    return (ret);
	}
    }

    handle->usage = cred_usage;
    *minor_status = 0;
    *output_cred_handle = (gss_cred_id_t)handle;

    ret = _gsskrb5_inquire_cred(minor_status, *output_cred_handle,
				NULL, time_rec, NULL, actual_mechs);
    if (ret) {
	_gsskrb5_release_cred(&junk, output_cred_handle);
	return ret;
    }

    return (GSS_S_COMPLETE);
}

OM_uint32
_gssiakerb_acquire_cred(OM_uint32 * minor_status,
			const gss_name_t desired_name,
			OM_uint32 time_req,
			const gss_OID_set desired_mechs,
			gss_cred_usage_t cred_usage,
			gss_cred_id_t * output_cred_handle,
			gss_OID_set * actual_mechs,
			OM_uint32 * time_rec)
{
    krb5_principal princ = (krb5_principal)desired_name;
    krb5_context context;
    krb5_error_code ret;
    gsskrb5_cred handle;
    krb5_data pw;
    krb5_uuid uuid;
    
    GSSAPI_KRB5_INIT(&context);

    *minor_status = 0;
    *output_cred_handle = NULL;
    
    if (cred_usage != GSS_C_INITIATE && cred_usage != GSS_C_BOTH)
	return GSS_S_FAILURE;
    if (princ == NULL)
	return GSS_S_FAILURE;
    
    if (krb5_principal_get_type(context, princ) != KRB5_NT_CACHE_UUID)
	return GSS_S_BAD_NAMETYPE;
    
    if (princ->name.name_string.len != 1 || strcmp(princ->realm, "UUID") != 0)
	return GSS_S_BAD_NAME;

    if (strlen(princ->name.name_string.val[0]) != 32)
	return GSS_S_BAD_NAME;
    
    if (hex_decode(princ->name.name_string.val[0], uuid, sizeof(uuid)) != 16)
	return GSS_S_BAD_NAME;
    
    handle = calloc(1, sizeof(*handle));
    if (handle == NULL)
        return (GSS_S_FAILURE);
    
    ret = krb5_cc_resolve_by_uuid(context, krb5_cc_type_api, &handle->ccache, uuid);
    if (ret) {
	free(handle);
	*minor_status = ret;
	return GSS_S_FAILURE;
    }
    
    if ((ret = krb5_cc_get_config(context, handle->ccache, NULL, "password", &pw)) == 0) {

	ret = asprintf(&handle->password, "%.*s", (int)pw.length, (char *)pw.data);
	memset(pw.data, 0, pw.length);
	krb5_data_free(&pw);
	if (ret <= 0 || handle->password == NULL) {
	    krb5_cc_close(context, handle->ccache);
	    free(handle);
	    *minor_status = ENOMEM;
	    return GSS_S_FAILURE;
	}

#ifdef PKINIT
    } else if ((ret = krb5_cc_get_config(context, handle->ccache, NULL, "certificate-ref", &pw)) == 0) {
	hx509_certs certs;
	hx509_query *q;
	
	ret = hx509_certs_init(context->hx509ctx, "KEYCHAIN:", 0, NULL, &certs);
	if (ret) {
	    krb5_data_free(&pw);
	    hx509_certs_free(&certs);
	    krb5_cc_close(context, handle->ccache);
	    free(handle);
	    *minor_status = ret;
	    return GSS_S_FAILURE;
	}

	ret = hx509_query_alloc(context->hx509ctx, &q);
	if (ret) {
	    krb5_data_free(&pw);
	    hx509_certs_free(&certs);
	    krb5_cc_close(context, handle->ccache);
	    free(handle);
	    *minor_status = ret;
	    return GSS_S_FAILURE;
	}
	
	hx509_query_match_option(q, HX509_QUERY_OPTION_PRIVATE_KEY);
	hx509_query_match_option(q, HX509_QUERY_OPTION_KU_DIGITALSIGNATURE);
	hx509_query_match_persistent(q, &pw);

	ret = _krb5_pk_find_cert(context, 1, certs, q, &handle->cert);
	krb5_data_free(&pw);
	hx509_certs_free(&certs);
	hx509_query_free(context->hx509ctx, q);
	if (ret != 0) {
	    _gss_mg_log(1, "gss-krb5: failed to find certificate ref %d", ret);
	    krb5_cc_close(context, handle->ccache);
	    free(handle);
	    *minor_status = ret;
	    return GSS_S_FAILURE;
	}
#endif
    } else {
	krb5_cc_close(context, handle->ccache);
	free(handle);
	*minor_status = 0;
	return GSS_S_FAILURE;
    }
    
    HEIMDAL_MUTEX_init(&handle->cred_id_mutex);
    
    ret = krb5_cc_get_principal(context, handle->ccache, &handle->principal);
    if (ret) {
	gss_cred_id_t h = (gss_cred_id_t)handle;
	OM_uint32 junk;
	gss_release_cred(&junk, &h);
	*minor_status = ret;
	return GSS_S_FAILURE;
    }

    handle->usage = GSS_C_INITIATE;
    handle->endtime = INT_MAX;
    
    *output_cred_handle = (gss_cred_id_t)handle;
    *minor_status = 0;
    return GSS_S_COMPLETE;
}


OM_uint32
_gss_iakerb_acquire_cred_ext(OM_uint32 * minor_status,
			     const gss_name_t desired_name,
			     gss_const_OID credential_type,
			     const void *credential_data,
			     OM_uint32 time_req,
			     gss_const_OID desired_mech,
			     gss_cred_usage_t cred_usage,
			     gss_cred_id_t * output_cred_handle)
{
    krb5_context context;
    gsskrb5_cred handle;
    krb5_error_code ret;
    krb5_creds cred;
    gss_buffer_t credential_buffer = NULL;
#ifdef PKINIT
    hx509_cert cert = NULL;
#endif
    
    memset(&cred, 0, sizeof(cred));
    
    if (cred_usage != GSS_C_INITIATE && cred_usage != GSS_C_BOTH)
	return GSS_S_FAILURE;

    GSSAPI_KRB5_INIT_STATUS(&context, status);


    /* pick up the credential */

    if (gss_oid_equal(credential_type, GSS_C_CRED_PASSWORD)) {

	credential_buffer = (gss_buffer_t)credential_data;

	if (credential_buffer->length + 1 < credential_buffer->length)
	    return GSS_S_FAILURE;

#ifdef PKINIT
    } else if (gss_oid_equal(credential_type, GSS_C_CRED_CERTIFICATE)) {

	cert = (hx509_cert)credential_data;

    } else if (gss_oid_equal(credential_type, GSS_C_CRED_SecIdentity)) {

	ret = hx509_cert_init_SecFramework(context->hx509ctx, rk_UNCONST(credential_data), &cert);
	if (ret) {
	    *minor_status = ret;
	    return GSS_S_FAILURE;
	}
#endif
    } else {
	*minor_status = KRB5_NOCREDS_SUPPLIED;
	return GSS_S_FAILURE;
    }


    if (desired_name == GSS_C_NO_NAME)
	return GSS_S_FAILURE;
    
    handle = calloc(1, sizeof(*handle));
    if (handle == NULL)
        return (GSS_S_FAILURE);
    
    HEIMDAL_MUTEX_init(&handle->cred_id_mutex);
    
    handle->usage = GSS_C_INITIATE;
    
    {
	krb5_principal princ = (krb5_principal)desired_name;
	
	ret = krb5_copy_principal(context, princ, &handle->principal);
	if (ret) {
	    HEIMDAL_MUTEX_destroy(&handle->cred_id_mutex);
	    free(handle);
	    *minor_status = ret;
	    return GSS_S_FAILURE;
	}
    }

    if (credential_buffer) {

	handle->password = malloc(credential_buffer->length + 1);
	if (handle->password == NULL) {
	    krb5_free_principal(context, handle->principal);
	    HEIMDAL_MUTEX_destroy(&handle->cred_id_mutex);
	    free(handle);
	    *minor_status = ENOMEM;
	    return GSS_S_FAILURE;
	}
	
	memcpy(handle->password, credential_buffer->value, credential_buffer->length);
	handle->password[credential_buffer->length] = '\0';
    }
#ifdef PKINIT
    if (cert)
	handle->cert = heim_retain(cert);
#endif

    handle->keytab = NULL;
    handle->ccache = NULL;
    handle->endtime = INT_MAX;
    
    /*
     * Lets overwrite the same credentials if we already have it
     */
    ret = krb5_cc_cache_match(context, handle->principal, &handle->ccache);
    if (ret) {
	ret = krb5_cc_new_unique(context, krb5_cc_type_api, NULL, &handle->ccache);
	if (ret)
	    goto out;
    }

    ret = krb5_cc_initialize(context, handle->ccache, handle->principal);
    if (ret)
	goto out;

    if (handle->password) {
	krb5_data pw;
	pw.data = handle->password;
	pw.length = strlen(handle->password);
	ret = krb5_cc_set_config(context, handle->ccache, NULL, "password", &pw);
	if (ret)
	    goto out;
    }
#ifdef PKINIT
    if (handle->cert) {
	krb5_data pd;
	ret = hx509_cert_get_persistent(handle->cert, &pd);
	if (ret)
	    goto out;
	ret = krb5_cc_set_config(context, handle->ccache, NULL, "certificate-ref", &pd);
	der_free_octet_string(&pd);
	if (ret)
	    goto out;
    }
#endif

    *output_cred_handle = (gss_cred_id_t) handle;

    *minor_status = 0;

    return GSS_S_COMPLETE;

 out:

    krb5_free_principal(context, handle->principal);
    if (handle->password) {
	memset(handle->password, 0, strlen(handle->password));
	free(handle->password);
    }
#ifdef PKINIT
    if (handle->cert)
	hx509_cert_free(handle->cert);
#endif
    if (handle->ccache)
	krb5_cc_destroy(context, handle->ccache);
    HEIMDAL_MUTEX_destroy(&handle->cred_id_mutex);
    free(handle);
    *minor_status = ret;
    return GSS_S_FAILURE;
}

/*
 *
 */

OM_uint32 GSSAPI_CALLCONV
_gss_krb5_acquire_cred_ext(OM_uint32 * minor_status,
			   const gss_name_t desired_name,
			   gss_const_OID credential_type,
			   const void *credential_data,
			   OM_uint32 time_req,
			   gss_const_OID desired_mech,
			   gss_cred_usage_t cred_usage,
			   gss_cred_id_t * output_cred_handle)
{
    krb5_get_init_creds_opt *opt;
    krb5_principal principal;
    gss_buffer_t password;
    krb5_context context;
    krb5_error_code kret;
    gsskrb5_cred handle;
    krb5_ccache ccache = NULL, ccacheold = NULL;
    krb5_creds cred;
    char *str;

    memset(&cred, 0, sizeof(cred));

    cred_usage &= GSS_C_OPTION_MASK;

    if (cred_usage != GSS_C_INITIATE && cred_usage != GSS_C_BOTH) {
	*minor_status = GSS_KRB5_S_G_BAD_USAGE;
	return GSS_S_FAILURE;
    }

    if (!gss_oid_equal(credential_type, GSS_C_CRED_PASSWORD)) {
	*minor_status = KRB5_NOCREDS_SUPPLIED; /* XXX */
	return GSS_S_FAILURE;
    }
    if (desired_name == GSS_C_NO_NAME)
	return GSS_S_FAILURE;

    GSSAPI_KRB5_INIT(&context);

    *output_cred_handle = NULL;

    handle = calloc(1, sizeof(*handle));
    if (handle == NULL) {
	*minor_status = ENOMEM;
        return (GSS_S_FAILURE);
    }

    principal = (krb5_principal)desired_name;

    /*
     * check if there an existing cache to overwrite before we lay
     * down the new cache
     */
    (void)krb5_cc_cache_match(context, principal, &ccacheold);

    HEIMDAL_MUTEX_init(&handle->cred_id_mutex);

    kret = krb5_copy_principal(context, principal, &handle->principal);
    if (kret)
	goto out;

    kret = krb5_cc_new_unique(context, NULL, NULL, &ccache);
    if (kret)
	goto out;

    kret = krb5_get_init_creds_opt_alloc(context, &opt);
    if (kret)
	goto out;

    krb5_get_init_creds_opt_set_forwardable(opt, 1);
    krb5_get_init_creds_opt_set_proxiable(opt, 1);
    krb5_get_init_creds_opt_set_renew_life(opt, 3600 * 24 * 30); /* 1 month */

    password = (gss_buffer_t)credential_data;

    str = malloc(password->length + 1);
    if (str == NULL) {
	kret = ENOMEM;
	goto out;
    }

    memcpy(str, password->value, password->length);
    str[password->length] = '\0';

    kret = krb5_get_init_creds_password(context, &cred,
					handle->principal,
					str,
					NULL, NULL, 0, NULL, opt);
    memset(str, 0, strlen(str));
    free(str);

    krb5_get_init_creds_opt_free(context, opt);
    if (kret) {
	krb5_free_cred_contents(context, &cred);
	goto out;
    }

    (void)krb5_cc_cache_match(context, cred.client, &ccacheold);

    kret = krb5_cc_initialize(context, ccache, cred.client);
    if (kret) {
	krb5_free_cred_contents(context, &cred);
	goto out;
    }
    kret = krb5_cc_store_cred(context, ccache, &cred);
    if (kret == 0)
	handle->endtime = cred.times.endtime;
    krb5_free_cred_contents(context, &cred);
    if (kret)
	goto out;
    
    /*
     * If we have a credential with the same naame, lets overwrite it
     */

    if (ccacheold) {
	kret = krb5_cc_move(context, ccache, ccacheold);
	if (kret)
	    goto out;
	handle->ccache = ccacheold;
	ccacheold = NULL;
    } else {
	handle->ccache = ccache;
    }

    handle->usage = cred_usage;
    *minor_status = 0;
    *output_cred_handle = (gss_cred_id_t)handle;

    return GSS_S_COMPLETE;

 out:
    if (ccacheold)
	krb5_cc_close(context, ccacheold);
    if (ccache)
	krb5_cc_destroy(context, ccache);
    if (handle->principal)
	krb5_free_principal(context, handle->principal);

    HEIMDAL_MUTEX_destroy(&handle->cred_id_mutex);
    free(handle);

    *minor_status = kret;
    return GSS_S_FAILURE;
}


#ifdef PKINIT

krb5_error_code
_gsspku2u_principal(krb5_context context,
		    struct hx509_cert_data *cert,
		    krb5_principal *principal)
{
    hx509_octet_string_list list;
    krb5_error_code ret;
    int found = 0;
    unsigned i;
    char *name;

    *principal = NULL;

    /*
     * First try to map PKINIT SAN to a Kerberos principal
     */

    ret = hx509_cert_find_subjectAltName_otherName(context->hx509ctx, cert,
						   &asn1_oid_id_pkinit_san,
						   &list);
    if (ret == 0) {
	for (i = 0; !found && i < list.len; i++) {
	    KRB5PrincipalName r;

	    ret = decode_KRB5PrincipalName(list.val[i].data,
					   list.val[i].length,
					   &r, NULL);
	    if (ret)
		continue;
	    
	    ret = _krb5_principalname2krb5_principal(context, principal,
						     r.principalName,
						     KRB5_PKU2U_REALM_NAME);
	    free_KRB5PrincipalName(&r);
	    if (ret == 0)
		found = 1;
	}
	hx509_free_octet_string_list(&list);
    }
    if (found)
	return 0;

    /*
     *
     */

    ret = hx509_cert_get_appleid(context->hx509ctx, cert, &name);
    if (ret == 0) {
	ret = krb5_make_principal(context, principal,
				  KRB5_PKU2U_REALM_NAME,
				  name, NULL);
	hx509_xfree(name);
	if (ret == 0) {
	    (*principal)->name.name_type = KRB5_NT_ENTERPRISE_PRINCIPAL;
	    return 0;
	}
    }
    
    /*
     * Give up and just WELLKNOWN and assertion instead
     */

    ret = krb5_make_principal(context, principal, KRB5_PKU2U_REALM_NAME,
			      KRB5_WELLKNOWN_NAME, KRB5_NULL_NAME, NULL);
    if (ret == 0)
	(*principal)->name.name_type = KRB5_NT_WELLKNOWN;
    return ret;
}




struct search {
    krb5_context context;
    krb5_principal principal;
};

static int
match_pkinit_san(hx509_context context, hx509_cert cert, void *ctx)
{
    struct search *s = ctx;
    return _krb5_pk_match_cert(s->context, s->principal, cert, 0);
}

OM_uint32
_gsspku2u_acquire_cred(OM_uint32 * minor_status,
		       const gss_name_t desired_name,
		       OM_uint32 time_req,
		       const gss_OID_set desired_mechs,
		       gss_cred_usage_t cred_usage,
		       gss_cred_id_t * output_cred_handle,
		       gss_OID_set * actual_mechs,
		       OM_uint32 * time_rec)
{
    krb5_context context;
    gsskrb5_cred handle;
    hx509_query *q;
    hx509_certs certs = NULL;
    OM_uint32 ret;
    krb5_principal name = (krb5_principal)desired_name;

    /* remove non-options from cred_usage */
    cred_usage = (cred_usage & GSS_C_OPTION_MASK);

    if (cred_usage != GSS_C_ACCEPT && cred_usage != GSS_C_INITIATE && cred_usage != GSS_C_BOTH) {
	*minor_status = GSS_KRB5_S_G_BAD_USAGE;
	return GSS_S_FAILURE;
    }

    GSSAPI_KRB5_INIT(&context);

    *output_cred_handle = NULL;
    if (time_rec)
	*time_rec = GSS_C_INDEFINITE;
    if (actual_mechs)
	*actual_mechs = GSS_C_NO_OID_SET;

    /*
     * We can't acquire credential for specific names that are not
     * PKU2U names, so don't try.
     */

    if (name && !krb5_principal_is_pku2u(context, name)) {
	*minor_status = 0;
	return GSS_S_BAD_NAME;
    }

    handle = calloc(1, sizeof(*handle));
    if (handle == NULL)
	return (GSS_S_FAILURE);

    HEIMDAL_MUTEX_init(&handle->cred_id_mutex);

    handle->usage = cred_usage;

    if ((cred_usage == GSS_C_INITIATE) || (cred_usage == GSS_C_BOTH)) {
	struct search s;

	ret = hx509_certs_init(context->hx509ctx, "KEYCHAIN:", 0, NULL, &certs);
	if (ret) {
	    *minor_status = ret;
	    goto fail;
	}

	ret = hx509_query_alloc(context->hx509ctx, &q);
	if (ret) {
	    *minor_status = ret;
	    goto fail;
	}
	
	hx509_query_match_option(q, HX509_QUERY_OPTION_PRIVATE_KEY);
	hx509_query_match_option(q, HX509_QUERY_OPTION_KU_DIGITALSIGNATURE);
	
	if (name) {
	    s.context = context;
	    s.principal = name;
	    hx509_query_match_cmp_func(q, match_pkinit_san, &s);
	}

	ret = _krb5_pk_find_cert(context, 1, certs, q, &handle->cert);
	hx509_query_free(context->hx509ctx, q);
	if (ret) {
	    *minor_status = ret;
	    goto fail;
	}

	if (name)
	    ret = krb5_copy_principal(context, name, &handle->principal);
	else
	    ret = _gsspku2u_principal(context, handle->cert, &handle->principal);
	if (ret) {
	    *minor_status = ret;
	    goto fail;
	}

    }

    if ((cred_usage == GSS_C_ACCEPT) || (cred_usage == GSS_C_BOTH)) {
	ret = get_keytab(context, handle, 1);
	if (ret) {
	    *minor_status = ret;
	    goto fail;
	}
    }
    if (certs)
	hx509_certs_free(&certs);

    *output_cred_handle = (gss_cred_id_t)handle;
    return GSS_S_COMPLETE;

 fail:
    if (certs)
	hx509_certs_free(&certs);
    if (handle->keytab)
	krb5_kt_close(context, handle->keytab);
    HEIMDAL_MUTEX_destroy(&handle->cred_id_mutex);
    free(handle);

    return GSS_S_FAILURE;
}

#endif
