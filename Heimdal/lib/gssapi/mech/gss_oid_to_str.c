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

/**
 * Turn an mech OID into an name
 *
 * Try to turn a OID into a mechanism name. If a matching OID can't be
 * found, this function will return NULL.
 *
 * The caller must free the oid_str buffer with gss_release_buffer()
 * when done with the string.
 *	  
 * @param minor_status an minor status code
 * @param oid an oid
 * @param oid_str buffer that will point to a NUL terminated string that is the numreric OID
 *
 * @returns a gss major status code
 *
 * @ingroup gssapi
 */

GSSAPI_LIB_FUNCTION OM_uint32 GSSAPI_LIB_CALL
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

/**
 * Turn an mech OID into an name
 *
 * Try to turn a OID into a mechanism name. If a matching OID can't be
 * found, this function will return NULL.
 *
 * @param oid an mechanism oid
 *
 * @returns pointer a static strng to a name or NULL when not found.
 *	  
 * @ingroup gssapi
 */

GSSAPI_LIB_FUNCTION const char * GSSAPI_LIB_CALL
gss_oid_to_name(gss_const_OID oid)
{
    size_t i;

    for (i = 0; _gss_ont_mech[i].oid; i++) {
	if (gss_oid_equal(oid, _gss_ont_mech[i].oid)) {
	    if (_gss_ont_mech[i].short_desc)
		return _gss_ont_mech[i].short_desc;
	    return _gss_ont_mech[i].name;
	}
    }
    return NULL;
}

/**
 * Turn an mech name into an OID
 *
 * Try to turn a string/name into an OID. If a static OID can't be
 * found, this function will return NULL.
 * 
 * WARNING: do _NOT_ call gss_release_oid() on the OID returned.
 *
 * @param name name of a OID
 *
 * @returns pointer a static OID or GSS_C_NO_OID when not found.
 *
 *	  
 * @ingroup gssapi
 */

GSSAPI_LIB_FUNCTION gss_const_OID GSSAPI_LIB_CALL
gss_name_to_oid(const char *name)
{
    size_t i, partial = (size_t)-1;

    for (i = 0; _gss_ont_mech[i].oid; i++) {
	if (strcasecmp(name, _gss_ont_mech[i].short_desc) == 0)
	    return _gss_ont_mech[i].oid;
	if (strncasecmp(name, _gss_ont_mech[i].short_desc, strlen(name)) == 0) {
	    if (partial != (size_t)-1)
		return NULL;
	    partial = i;
	}
    }
    if (partial != (size_t)-1)
	return _gss_ont_mech[partial].oid;

    return NULL;
}
