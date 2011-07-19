/*
 * Copyright (c) 1997 - 2003 Kungliga Tekniska Högskolan
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

#include "gsskrb5_locl.h"

static OM_uint32
import_krb5_name(OM_uint32 *minor_status,
		 const gss_OID mech,
		 const gss_buffer_t input_name_buffer,
		 const gss_OID input_name_type,
		 gss_name_t *output_name)
{
    krb5_context context;
    krb5_principal princ;
    krb5_error_code ret;
    char *tmp;

    GSSAPI_KRB5_INIT (&context);

    tmp = malloc (input_name_buffer->length + 1);
    if (tmp == NULL) {
	*minor_status = ENOMEM;
	return GSS_S_FAILURE;
    }
    memcpy (tmp,
	    input_name_buffer->value,
	    input_name_buffer->length);
    tmp[input_name_buffer->length] = '\0';

    ret = krb5_parse_name (context, tmp, &princ);
    if (ret) {
	free(tmp);
	*minor_status = ret;

	if (ret == KRB5_PARSE_ILLCHAR || ret == KRB5_PARSE_MALFORMED)
	    return GSS_S_BAD_NAME;

	return GSS_S_FAILURE;
    }

    if (mech && gss_oid_equal(mech, GSS_PKU2U_MECHANISM) && strchr(tmp, '@') == NULL)
	krb5_principal_set_realm(context, princ, KRB5_PKU2U_REALM_NAME);

    free(tmp);

    if (princ->name.name_string.len == 2 &&
	gss_oid_equal(input_name_type, GSS_KRB5_NT_PRINCIPAL_NAME_REFERRAL))
	krb5_principal_set_type(context, princ, KRB5_NT_GSS_HOSTBASED_SERVICE);

    *output_name = (gss_name_t)princ;
    return GSS_S_COMPLETE;
}

static OM_uint32
import_krb5_principal(OM_uint32 *minor_status,
		      const gss_OID mech,
		      const gss_buffer_t input_name_buffer,
		      const gss_OID input_name_type,
		      gss_name_t *output_name)
{
    krb5_context context;
    krb5_principal *princ, res = NULL;
    OM_uint32 ret;

    GSSAPI_KRB5_INIT (&context);

    princ = (krb5_principal *)input_name_buffer->value;

    ret = krb5_copy_principal(context, *princ, &res);
    if (ret) {
	*minor_status = ret;
	return GSS_S_FAILURE;
    }
    *output_name = (gss_name_t)res;
    return GSS_S_COMPLETE;
}


OM_uint32
_gsskrb5_canon_name(OM_uint32 *minor_status, krb5_context context,
		    int use_dns, krb5_const_principal sourcename, gss_name_t targetname,
		    krb5_principal *out)
{
    krb5_principal p = (krb5_principal)targetname;
    krb5_error_code ret;
    char *hostname = NULL, *service;

    *minor_status = 0;

    /* If its not a hostname */
    if (krb5_principal_get_type(context, p) != KRB5_NT_GSS_HOSTBASED_SERVICE) {
	ret = krb5_copy_principal(context, p, out);
    } else if (!use_dns) {
	ret = krb5_copy_principal(context, p, out);
	if (ret)
	    goto out;
	krb5_principal_set_type(context, *out, KRB5_NT_SRV_HST);
	if (sourcename)
	    ret = krb5_principal_set_realm(context, *out, sourcename->realm);
    } else {
	if (p->name.name_string.len == 0)
	    return GSS_S_BAD_NAME;
	else if (p->name.name_string.len > 1)
	    hostname = p->name.name_string.val[1];
	
	service = p->name.name_string.val[0];
	
	ret = krb5_sname_to_principal(context,
				      hostname,
				      service,
				      KRB5_NT_SRV_HST,
				      out);
    }

 out:
    if (ret) {
	*minor_status = ret;
	return GSS_S_FAILURE;
    }

    return 0;
}


static OM_uint32
import_hostbased_name (OM_uint32 *minor_status,
		       const gss_OID mech,
		       const gss_buffer_t input_name_buffer,
		       const gss_OID input_name_type,
		       gss_name_t *output_name)
{
    krb5_context context;
    krb5_principal princ = NULL;
    krb5_error_code kerr;
    char *tmp, *p, *host = NULL, *realm = NULL;

    if (gss_oid_equal(mech, GSS_PKU2U_MECHANISM))
	realm = KRB5_PKU2U_REALM_NAME;
    else
	realm = "WELLKNOWN:ORG.H5L.REFERALS-REALM"; /* should never hit the network */

    GSSAPI_KRB5_INIT (&context);

    tmp = malloc (input_name_buffer->length + 1);
    if (tmp == NULL) {
	*minor_status = ENOMEM;
	return GSS_S_FAILURE;
    }
    memcpy (tmp,
	    input_name_buffer->value,
	    input_name_buffer->length);
    tmp[input_name_buffer->length] = '\0';

    p = strchr (tmp, '@');
    if (p != NULL) {
	size_t len;

	*p = '\0';
	host = p + 1;

	/*
	 * Squash any trailing . on the hostname since that is jolly
	 * good to have when looking up a DNS name (qualified), but
	 * its no good to have in the kerberos principal since those
	 * are supposed to be in qualified format already.
	 */

	len = strlen(host);
	if (len > 0 && host[len - 1] == '.')
	    host[len - 1] = '\0';
    }

    kerr = krb5_make_principal(context, &princ, realm, tmp, host, NULL);
    free (tmp);
    *minor_status = kerr;
    if (kerr == KRB5_PARSE_ILLCHAR || kerr == KRB5_PARSE_MALFORMED)
	return GSS_S_BAD_NAME;
    else if (kerr)
	return GSS_S_FAILURE;

    krb5_principal_set_type(context, princ, KRB5_NT_GSS_HOSTBASED_SERVICE);
    *output_name = (gss_name_t)princ;

    return 0;
}

static OM_uint32
import_dn_name(OM_uint32 *minor_status,
	       const gss_OID mech,
	       const gss_buffer_t input_name_buffer,
	       const gss_OID input_name_type,
	       gss_name_t *output_name)
{
    /* XXX implement me */
    *output_name = NULL;
    *minor_status = 0;
    return GSS_S_FAILURE;
}

static OM_uint32
import_pku2u_export_name(OM_uint32 *minor_status,
			 const gss_OID mech,
			 const gss_buffer_t input_name_buffer,
			 const gss_OID input_name_type,
			 gss_name_t *output_name)
{
    /* XXX implement me */
    *output_name = NULL;
    *minor_status = 0;
    return GSS_S_FAILURE;
}

static OM_uint32
import_uuid_name(OM_uint32 *minor_status,
		 const gss_OID mech,
		 const gss_buffer_t input_name_buffer,
		 const gss_OID input_name_type,
		 gss_name_t *output_name)
{
    krb5_context context;
    krb5_error_code ret;
    krb5_principal princ;
    char uuid[32 + 1];

    GSSAPI_KRB5_INIT(&context);
    
    if (input_name_buffer->length < sizeof(uuid) - 1) {
	*minor_status = 0;
	return GSS_S_BAD_NAME;
    }
    
    memcpy(uuid, input_name_buffer->value, sizeof(uuid) - 1);
    uuid[sizeof(uuid) - 1] = '\0';
    
    /* validate that uuid is only hex chars */
    if (strspn(uuid, "0123456789abcdefABCDEF") != 32) {
	*minor_status = 0;
	return GSS_S_BAD_NAME;
    }
    
    ret = krb5_make_principal(context, &princ, "UUID", uuid, NULL);
    if (ret) {
	*minor_status = ret;
	return GSS_S_FAILURE;
    }
    krb5_principal_set_type(context, princ, KRB5_NT_CACHE_UUID);
    
    *output_name = (gss_name_t)princ;
    *minor_status = 0;

    return GSS_S_COMPLETE;
}

static struct _gss_name_type krb5_names[] = {
    { GSS_C_NT_HOSTBASED_SERVICE, import_hostbased_name },
    { GSS_C_NT_HOSTBASED_SERVICE_X, import_hostbased_name },
    { GSS_KRB5_NT_PRINCIPAL, import_krb5_principal},
    { GSS_C_NO_OID, import_krb5_name },
    { GSS_C_NT_USER_NAME, import_krb5_name },
    { GSS_KRB5_NT_PRINCIPAL_NAME, import_krb5_name },
    { GSS_KRB5_NT_PRINCIPAL_NAME_REFERRAL, import_krb5_name },
    { GSS_C_NT_EXPORT_NAME, import_krb5_name },
    { NULL }
};

static struct _gss_name_type pku2u_names[] = {
    { GSS_C_NT_HOSTBASED_SERVICE, import_hostbased_name },
    { GSS_C_NT_HOSTBASED_SERVICE_X, import_hostbased_name },
    { GSS_C_NO_OID, import_krb5_name },
    { GSS_C_NT_USER_NAME, import_krb5_name },
    { GSS_KRB5_NT_PRINCIPAL_NAME, import_krb5_name },
    { GSS_C_NT_DN, import_dn_name },
    { GSS_C_NT_EXPORT_NAME, import_pku2u_export_name },
    { NULL }
};

static struct _gss_name_type iakerb_names[] = {
    { GSS_C_NT_HOSTBASED_SERVICE, import_hostbased_name },
    { GSS_C_NT_HOSTBASED_SERVICE_X, import_hostbased_name },
    { GSS_C_NO_OID, import_krb5_name },
    { GSS_C_NT_USER_NAME, import_krb5_name },
    { GSS_KRB5_NT_PRINCIPAL_NAME, import_krb5_name },
    { GSS_C_NT_EXPORT_NAME, import_krb5_name },
    { GSS_C_NT_UUID, import_uuid_name },
    { NULL }
};

OM_uint32 _gsskrb5_import_name
           (OM_uint32 * minor_status,
            const gss_buffer_t input_name_buffer,
            const gss_OID input_name_type,
            gss_name_t * output_name
           )
{
    return _gss_mech_import_name(minor_status, GSS_KRB5_MECHANISM,
				 krb5_names, input_name_buffer,
				 input_name_type, output_name);
}

OM_uint32 _gsspku2u_import_name
           (OM_uint32 * minor_status,
            const gss_buffer_t input_name_buffer,
            const gss_OID input_name_type,
            gss_name_t * output_name
           )
{
    return _gss_mech_import_name(minor_status, GSS_PKU2U_MECHANISM,
				 pku2u_names, input_name_buffer,
				 input_name_type, output_name);
}

OM_uint32
_gssiakerb_import_name(OM_uint32 * minor_status,
		       const gss_buffer_t input_name_buffer,
		       const gss_OID input_name_type,
		       gss_name_t * output_name)
{
    return _gss_mech_import_name(minor_status, GSS_IAKERB_MECHANISM,
				 iakerb_names, input_name_buffer,
				 input_name_type, output_name);
}


OM_uint32
_gsskrb5_inquire_names_for_mech (OM_uint32 * minor_status,
				 const gss_OID mechanism,
				 gss_OID_set * name_types)
{
    return _gss_mech_inquire_names_for_mech(minor_status, krb5_names,
					    name_types);
}

OM_uint32
_gsspku2u_inquire_names_for_mech (OM_uint32 * minor_status,
				  const gss_OID mechanism,
				  gss_OID_set * name_types)
{
    return _gss_mech_inquire_names_for_mech(minor_status, pku2u_names,
					    name_types);
}

OM_uint32
_gssiakerb_inquire_names_for_mech (OM_uint32 * minor_status,
				   const gss_OID mechanism,
				   gss_OID_set * name_types)
{
    return _gss_mech_inquire_names_for_mech(minor_status, iakerb_names,
					    name_types);
}
