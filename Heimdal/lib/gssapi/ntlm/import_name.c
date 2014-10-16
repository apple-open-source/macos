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

#include "ntlm.h"

gss_name_t
_gss_ntlm_create_name(OM_uint32 *minor_status,
		      const char *user, const char *domain, int flags)
{
    ntlm_name n;
    n = calloc(1, sizeof(*n));
    if (n == NULL) {
	*minor_status = ENOMEM;
	return NULL;
    }

    n->user = strdup(user);
    n->domain = strdup(domain);
    n->flags = flags;

    if (n->user == NULL || n->domain == NULL) {
	free(n->user);
	free(n->domain);
	free(n);
	*minor_status = ENOMEM;
	return NULL;
    }

    return (gss_name_t)n;
}

static OM_uint32
anon_name(OM_uint32 *minor_status,
	  gss_const_OID mech,
	  const gss_buffer_t input_name_buffer,
	  gss_const_OID input_name_type,
	  gss_name_t *output_name)
{
    *output_name = _gss_ntlm_create_name(minor_status, "", "", NTLM_ANON_NAME);
    if (*output_name == NULL)
	return GSS_S_FAILURE;
    return GSS_S_COMPLETE;
}

static OM_uint32
hostbased_name(OM_uint32 *minor_status,
	       gss_const_OID mech,
	       const gss_buffer_t input_name_buffer,
	       gss_const_OID input_name_type,
	       gss_name_t *output_name)
{
    char *name, *p;

    name = malloc(input_name_buffer->length + 1);
    if (name == NULL) {
	*minor_status = ENOMEM;
	return GSS_S_FAILURE;
    }
    memcpy(name, input_name_buffer->value, input_name_buffer->length);
    name[input_name_buffer->length] = '\0';

    /* find "domain" part of the name and uppercase it */
    p = strchr(name, '@');
    if (p) {
	p[0] = '\0';
	p++;
    } else {
	p = "";
    }

    *output_name = _gss_ntlm_create_name(minor_status, name, p, 0);
    free(name);
    if (*output_name == NULL)
	return GSS_S_FAILURE;

    return GSS_S_COMPLETE;
}

static OM_uint32
parse_name(OM_uint32 *minor_status,
	   gss_const_OID mech,
	   int domain_required,
	   const gss_buffer_t input_name_buffer,
	   gss_const_OID input_name_type,
	   gss_name_t *output_name)
{
    char *name, *p, *user, *domain;

    if (memchr(input_name_buffer->value, '@', input_name_buffer->length) != NULL)
	return hostbased_name(minor_status, mech, input_name_buffer,
			      input_name_type, output_name);

    name = malloc(input_name_buffer->length + 1);
    if (name == NULL) {
	*minor_status = ENOMEM;
	return GSS_S_FAILURE;
    }
    memcpy(name, input_name_buffer->value, input_name_buffer->length);
    name[input_name_buffer->length] = '\0';

    /* find "domain" part of the name and uppercase it */
    p = strchr(name, '\\');
    if (p) {
	p[0] = '\0';
	user = p + 1;
	domain = name;
	strupr(domain);
    } else if (!domain_required) {
	user = name;
	domain = ""; /* no domain */
    } else {
	free(name);
	*minor_status = HNTLM_ERR_MISSING_NAME_SEPARATOR;
	return gss_mg_set_error_string(GSS_NTLM_MECHANISM, GSS_S_BAD_NAME,
				       HNTLM_ERR_MISSING_NAME_SEPARATOR,
				       "domain requested but missing name");
    }

    *output_name = _gss_ntlm_create_name(minor_status, user, domain, 0);
    free(name);
    if (*output_name == NULL)
	return GSS_S_FAILURE;

    return GSS_S_COMPLETE;
}

static OM_uint32
user_name(OM_uint32 *minor_status,
	  gss_const_OID mech,
	  const gss_buffer_t input_name_buffer,
	  gss_const_OID input_name_type,
	  gss_name_t *output_name)
{
    return parse_name(minor_status, mech, 0, input_name_buffer, input_name_type, output_name);
}

static OM_uint32
parse_ntlm_name(OM_uint32 *minor_status,
		gss_const_OID mech,
		const gss_buffer_t input_name_buffer,
		gss_const_OID input_name_type,
		gss_name_t *output_name)
{
    return parse_name(minor_status, mech, 1, input_name_buffer, input_name_type, output_name);
}

static OM_uint32
export_name(OM_uint32 *minor_status,
	    gss_const_OID mech,
	    const gss_buffer_t input_name_buffer,
	    gss_const_OID input_name_type,
	    gss_name_t *output_name)
{
    return parse_name(minor_status, mech, 1, input_name_buffer, input_name_type, output_name);
}

static struct _gss_name_type ntlm_names[] = {
    { GSS_C_NT_ANONYMOUS, anon_name},
    { GSS_C_NT_HOSTBASED_SERVICE, hostbased_name},
    { GSS_C_NT_USER_NAME, user_name },
    { GSS_C_NT_NTLM, parse_ntlm_name },
    { GSS_C_NT_EXPORT_NAME, export_name },
    { NULL }
};


OM_uint32 _gss_ntlm_import_name
           (OM_uint32 * minor_status,
            const gss_buffer_t input_name_buffer,
            gss_const_OID input_name_type,
            gss_name_t * output_name
           )
{
    return _gss_mech_import_name(minor_status, GSS_NTLM_MECHANISM,
				 ntlm_names, input_name_buffer,
				 input_name_type, output_name);
}

OM_uint32 _gss_ntlm_inquire_names_for_mech (
            OM_uint32 * minor_status,
            gss_const_OID mechanism,
            gss_OID_set * name_types
           )
{
    return _gss_mech_inquire_names_for_mech(minor_status, ntlm_names,
					    name_types);
}
