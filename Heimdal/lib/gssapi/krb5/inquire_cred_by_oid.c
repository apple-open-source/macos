/*
 * Copyright (c) 2004, PADL Software Pty Ltd.
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
 * 3. Neither the name of PADL Software nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PADL SOFTWARE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL PADL SOFTWARE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "gsskrb5_locl.h"
#include <hex.h>
#include <rtbl.h>

static char*
printable_time(time_t t)
{
    static char s[128];
    char *p;

    if ((p = ctime(&t)) == NULL)
	strlcpy(s, "?", sizeof(s));
    else
	strlcpy(s, p + 4, sizeof(s));
    s[20] = 0;
    return s;
}


OM_uint32 GSSAPI_CALLCONV _gsskrb5_inquire_cred_by_oid
	   (OM_uint32 * minor_status,
	    const gss_cred_id_t cred_handle,
	    const gss_OID desired_object,
	    gss_buffer_set_t *data_set)
{
    krb5_context context;
    gsskrb5_cred cred = (gsskrb5_cred)cred_handle;
    krb5_error_code ret;
    gss_buffer_desc buffer;

    GSSAPI_KRB5_INIT (&context);

    if (gss_oid_equal(desired_object, GSS_KRB5_COPY_CCACHE_X)) {
	char *str;

	HEIMDAL_MUTEX_lock(&cred->cred_id_mutex);
	
	if (cred->ccache == NULL) {
	    HEIMDAL_MUTEX_unlock(&cred->cred_id_mutex);
	    *minor_status = EINVAL;
	    return GSS_S_FAILURE;
	}
	
	ret = krb5_cc_get_full_name(context, cred->ccache, &str);
	HEIMDAL_MUTEX_unlock(&cred->cred_id_mutex);
	if (ret) {
	    *minor_status = ret;
	    return GSS_S_FAILURE;
	}
	
	buffer.value = str;
	buffer.length = strlen(str);
	
	ret = gss_add_buffer_set_member(minor_status, &buffer, data_set);
	if (ret != GSS_S_COMPLETE)
	    _gsskrb5_clear_status ();
	
	free(str);
	
	*minor_status = 0;
	return GSS_S_COMPLETE;

    } else if (gss_oid_equal(desired_object, GSS_C_CRED_VALIDATE)) {
	krb5_verify_init_creds_opt vopt;
	krb5_creds *kcred = NULL;

	if (cred->ccache == NULL || cred->principal == NULL)
	    return GSS_S_FAILURE;

	krb5_verify_init_creds_opt_init(&vopt);
	krb5_verify_init_creds_opt_set_ap_req_nofail(&vopt, TRUE);

	HEIMDAL_MUTEX_lock(&cred->cred_id_mutex);

	ret = _krb5_get_krbtgt(context, cred->ccache, cred->principal->realm, &kcred);
	if (ret == 0) {
	    ret = krb5_verify_init_creds(context,
					 kcred,
					 NULL,
					 NULL,
					 &cred->ccache,
					 &vopt);
	    krb5_free_creds(context, kcred);
	}
	HEIMDAL_MUTEX_unlock(&cred->cred_id_mutex);

	if (ret) {
	    *minor_status = ret;
	    return GSS_S_FAILURE;
	}

	return GSS_S_COMPLETE;

    } else if (gss_oid_equal(desired_object, GSS_C_NT_UUID)) {
	krb5_uuid uuid;
	char *str;
	
	HEIMDAL_MUTEX_lock(&cred->cred_id_mutex);
	
	if (cred->ccache == NULL) {
	    HEIMDAL_MUTEX_unlock(&cred->cred_id_mutex);
	    *minor_status = EINVAL;
	    return GSS_S_FAILURE;
	}
	
	ret = krb5_cc_get_uuid(context, cred->ccache, uuid);
	HEIMDAL_MUTEX_unlock(&cred->cred_id_mutex);
	if (ret) {
	    *minor_status = ret;
	    return GSS_S_FAILURE;
	}
	
	if (hex_encode(uuid, sizeof(uuid), &str) < 0 || str == NULL) {
	    *minor_status = ENOMEM;
	    return GSS_S_FAILURE;
	}
	
	buffer.value = str;
	buffer.length = strlen(str);
	
	ret = gss_add_buffer_set_member(minor_status, &buffer, data_set);
	free(str);
	if (ret != GSS_S_COMPLETE)
	    _gsskrb5_clear_status ();
	
	return GSS_S_COMPLETE;

    } else if (gss_oid_equal(desired_object, GSS_C_CRED_DIAG)) {
	krb5_cc_cursor cursor;
	krb5_creds creds;
	krb5_data data;
	rtbl_t ct = NULL;
	char *str = NULL;

	if (cred->ccache == NULL) {
	    *minor_status = EINVAL;
	    return GSS_S_FAILURE;
	}

	/*
	 * Cache name
	 */

	ret = krb5_cc_get_full_name(context, cred->ccache, &str);
	HEIMDAL_MUTEX_unlock(&cred->cred_id_mutex);
	if (ret) {
	    *minor_status = ret;
	    return GSS_S_FAILURE;
	}
	
	buffer.value = str;
	buffer.length = strlen(str);
	
	ret = gss_add_buffer_set_member(minor_status, &buffer, data_set);
	free(str);
	if (ret != GSS_S_COMPLETE)
	    _gsskrb5_clear_status ();

	/*
	 * cache list
	 */

	ct = rtbl_create();
	if (ct == NULL) {
	    *minor_status = ENOMEM;
	    return GSS_S_FAILURE;
	}

#define COL_ISSUED "Issued"
#define COL_EXPIRES "Expires"
#define COL_PRINCIPAL "Principal"
#define COL_ENCTYPE "Enctype"

	rtbl_add_column(ct, COL_PRINCIPAL, 0);
	rtbl_add_column(ct, COL_ISSUED, 0);
	rtbl_add_column(ct, COL_EXPIRES, 0);
	rtbl_add_column(ct, COL_ENCTYPE, 0);
	rtbl_set_separator(ct, "  ");

	HEIMDAL_MUTEX_lock(&cred->cred_id_mutex);

	ret = krb5_cc_start_seq_get (context, cred->ccache, &cursor);
	if (ret)
	    goto out;

	while ((ret = krb5_cc_next_cred (context, cred->ccache, &cursor, &creds)) == 0) {

	    ret = krb5_unparse_name(context, creds.server, &str);
	    if (ret)
		goto next;

	    rtbl_add_column_entry(ct, COL_PRINCIPAL, str);
	    free(str);

	    rtbl_add_column_entry(ct, COL_ISSUED,
				  printable_time(creds.times.starttime));
	    if (time(NULL) < creds.times.endtime)
		rtbl_add_column_entry(ct, COL_EXPIRES,
				      printable_time(creds.times.endtime));
	    else
		rtbl_add_column_entry(ct, COL_EXPIRES, "Expired");

	    ret = krb5_enctype_to_string(context, creds.session.keytype, &str);
	    if (ret)
		goto next;
	    rtbl_add_column_entry(ct, COL_ENCTYPE, str);
	    free(str);

	next:
	    krb5_free_cred_contents (context, &creds);
	}
	(void)krb5_cc_end_seq_get (context, cred->ccache, &cursor);

	if (ret == KRB5_CC_END)
	    ret = 0;
	else
	    goto out;


	buffer.value = rtbl_format_str(ct);
	if (buffer.value == NULL) {
	    *minor_status = ENOMEM;
	    return GSS_S_FAILURE;
	}
	buffer.length = strlen((char *)buffer.value);
	
	ret = gss_add_buffer_set_member(minor_status, &buffer, data_set);
	free(buffer.value);
	if (ret != GSS_S_COMPLETE)
	    _gsskrb5_clear_status ();


	/*
	 * kcm status
	 */

	ret = krb5_cc_get_config(context, cred->ccache, NULL, "kcm-status", &data);
	if (ret == 0) {
	    uint32_t num;
	    int status, kcmret;
	    if (data.length >= 8) {
		memcpy(&num, ((int8_t*)data.data) + 4, sizeof(num));
		status = (int)ntohl(num);
	    }
	    if (data.length >= 12) {
		memcpy(&num, ((int8_t*)data.data) + 8, sizeof(num));
		kcmret = (int)ntohl(num);
	    }
	    ret = asprintf(&str, "kcm-status: %s ret: %d",
			   _krb5_kcm_get_status(status), kcmret);
	    krb5_data_free(&data);
	    if (ret < 0)
		goto out;
	    buffer.value = str;
	    buffer.length = strlen(str);

	    ret = gss_add_buffer_set_member(minor_status, &buffer, data_set);
	    free(str);
	    if (ret != GSS_S_COMPLETE)
		_gsskrb5_clear_status ();
	}
	ret = 0;

    out:
	HEIMDAL_MUTEX_unlock(&cred->cred_id_mutex);

	if (ct)
	    rtbl_destroy(ct);

	if (ret) {
	    *minor_status = ret;
	    return GSS_S_FAILURE;
	}

	return GSS_S_COMPLETE;

    } else if (gss_oid_equal(desired_object, GSS_C_CRED_SET_DEFAULT)) {

	HEIMDAL_MUTEX_lock(&cred->cred_id_mutex);

	if (cred->ccache)
	    ret = krb5_cc_switch(context, cred->ccache);
	else
	    ret = EINVAL;

	HEIMDAL_MUTEX_unlock(&cred->cred_id_mutex);

	if (ret) {
	    *minor_status = ret;
	    return GSS_S_FAILURE;
	}

	buffer.value = (void *)"default";
	buffer.length = 7;
	return gss_add_buffer_set_member(minor_status, &buffer, data_set);

    } else if (gss_oid_equal(desired_object, GSS_C_CRED_GET_DEFAULT)) {
	const char *defname;
	char *fn = NULL;

	HEIMDAL_MUTEX_lock(&cred->cred_id_mutex);
	ret = krb5_cc_get_full_name(context, cred->ccache, &fn);
	HEIMDAL_MUTEX_unlock(&cred->cred_id_mutex);

	if (ret) {
	    *minor_status = ret;
	    return GSS_S_FAILURE;
	}

	defname = krb5_cc_default_name(context);
	if (defname && strcmp(fn, defname) == 0)
	    ret = 0;
	else
	    ret = EINVAL;
	free(fn);

	if (ret) {
	    *minor_status = ret;
	    return GSS_S_FAILURE;
	}

	buffer.value = (void *)"default";
	buffer.length = 7;
	return gss_add_buffer_set_member(minor_status, &buffer, data_set);

    } else if (gss_oid_equal(desired_object, GSS_C_CRED_RENEW)) {
	krb5_creds in, *out = NULL;
	krb5_kdc_flags flags;
	const char *realm;

	memset(&in, 0, sizeof(in));

	HEIMDAL_MUTEX_lock(&cred->cred_id_mutex);

	ret = krb5_cc_get_principal(context, cred->ccache, &in.client);
	if(ret) {
	    HEIMDAL_MUTEX_unlock(&cred->cred_id_mutex);
	    *minor_status = ret;
	    return GSS_S_FAILURE;
	}
	realm = krb5_principal_get_realm(context, in.client);
	ret = krb5_make_principal(context, &in.server, realm, KRB5_TGS_NAME, realm, NULL);
	if(ret) {	
	    HEIMDAL_MUTEX_unlock(&cred->cred_id_mutex);
	    krb5_free_cred_contents(context, &in);
	    *minor_status = ret;
	    return GSS_S_FAILURE;
	}

	ret = krb5_get_credentials(context, KRB5_GC_CACHED, cred->ccache, &in, &out);
	if (ret) {
	    HEIMDAL_MUTEX_unlock(&cred->cred_id_mutex);
	    krb5_free_cred_contents(context, &in);
	    *minor_status = ret;
	    return GSS_S_FAILURE;
	}

	if (out->flags.b.renewable == 0) {
	    HEIMDAL_MUTEX_unlock(&cred->cred_id_mutex);
	    krb5_free_cred_contents(context, &in);
	    krb5_free_creds(context, out);
	    krb5_set_error_message(context, GSS_KRB5_S_G_BAD_USAGE, "Credential is not renewable");
	    *minor_status = GSS_KRB5_S_G_BAD_USAGE;
	    return GSS_S_FAILURE;
	}

	flags.i = 0;
	flags.b.renewable         = out->flags.b.renewable;
	flags.b.forwardable       = out->flags.b.forwardable;
	flags.b.proxiable         = out->flags.b.proxiable;

	krb5_free_creds(context, out);
	out = NULL;

	ret = krb5_get_kdc_cred(context,
				cred->ccache,
				flags,
				NULL,
				NULL,
				&in,
				&out);
	if(ret == 0) {
	    (void)krb5_cc_remove_cred(context, cred->ccache, 0, &in);
	    ret = krb5_cc_store_cred(context, cred->ccache, out);
	    krb5_free_creds (context, out);
	}
	krb5_free_cred_contents(context, &in);
	HEIMDAL_MUTEX_unlock(&cred->cred_id_mutex);

	if (ret) {
	    *minor_status = ret;
	    return GSS_S_FAILURE;
	}

	buffer.value = (void *)"renewed";
	buffer.length = 7;
	return gss_add_buffer_set_member(minor_status, &buffer, data_set);

    } else {
	*minor_status = EINVAL;
	return GSS_S_FAILURE;
    }
}

