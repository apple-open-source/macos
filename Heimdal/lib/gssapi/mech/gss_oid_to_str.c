/*
 * Copyright (c) 2006 Kungliga Tekniska Högskolan
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

#include "mech_locl.h"

OM_uint32 GSSAPI_LIB_FUNCTION
gss_oid_to_str(OM_uint32 *minor_status, gss_OID oid, gss_buffer_t oid_str)
{
    int ret;
    size_t size;
    heim_oid o;
    char *p;

    _mg_buffer_zero(oid_str);

    if (oid == GSS_C_NULL_OID)
	return GSS_S_FAILURE;

    ret = der_get_oid (oid->elements, oid->length, &o, &size);
    if (ret) {
	*minor_status = ret;
	return GSS_S_FAILURE;
    }

    ret = der_print_heim_oid(&o, ' ', &p);
    der_free_oid(&o);
    if (ret) {
	*minor_status = ret;
	return GSS_S_FAILURE;
    }

    oid_str->value = p;
    oid_str->length = strlen(p);

    *minor_status = 0;
    return GSS_S_COMPLETE;
}

struct {
    gss_OID oid;
    const char *name;
} mechs[] = {
    { GSS_KRB5_MECHANISM, "Kerberos" },
    { GSS_KRB5_MECHANISM, "Kerberos 5" },
    { GSS_KRB5_MECHANISM, "krb5" },
    { GSS_IAKERB_MECHANISM, "IAKERB" },
    { GSS_SPNEGO_MECHANISM, "SPNEGO" },
#ifdef ENABLE_NTLM
    { GSS_NTLM_MECHANISM, "NTLM" },
#endif
#ifdef ENABLE_SCRAM
    { GSS_SCRAM_MECHANISM, "SCRAM-SHA1" },
#endif
#ifdef PKINIT
    { GSS_PKU2U_MECHANISM, "PKU2U" },
#endif
    { GSS_NETLOGON_MECHANISM, "NETLOGON" },
    { GSS_SASL_DIGEST_MD5_MECHANISM, "SASL digest-md5" },
    { GSS_SASL_DIGEST_MD5_MECHANISM, "sasl-digest-md5" }
};

const char *
gss_oid_to_name(gss_OID oid)
{
    size_t j;
    for (j = 0; j < sizeof(mechs)/sizeof(mechs[0]); j++)
	if (gss_oid_equal(oid, mechs[j].oid))
	    return mechs[j].name;
    return NULL;
}

gss_OID
gss_name_to_oid(const char *name)
{
    size_t i, partial = (size_t)-1;

    for (i = 0; i < sizeof(mechs)/sizeof(mechs[0]); i++) {
	if (strcasecmp(name, mechs[i].name) == 0)
	    return mechs[i].oid;
	if (strncasecmp(name, mechs[i].name, strlen(name)) == 0) {
	    if (partial != (size_t)-1)
		return NULL;
	    partial = i;
	}
    }
    if (partial != (size_t)-1)
	return mechs[partial].oid;
    return NULL;
}
