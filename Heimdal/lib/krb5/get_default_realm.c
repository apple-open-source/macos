/*
 * Copyright (c) 1997 - 2001, 2004 Kungliga Tekniska Högskolan
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

#include "krb5_locl.h"

krb5_error_code
_krb5_array_to_realms(krb5_context context, heim_array_t array, krb5_realm **realms)
{
    size_t n, len;

    len = heim_array_get_length(array);
    
    *realms = calloc(len + 1, sizeof((*realms)[0]));
    for (n = 0; n < len; n++) {
	heim_string_t s = heim_array_copy_value(array, n);
	if (s) {
	    (*realms)[n] = strdup(heim_string_get_utf8(s));
	    heim_release(s);
	}
	if ((*realms)[n] == NULL) {
	    krb5_free_host_realm(context, *realms);
	    krb5_set_error_message(context, ENOMEM,
				   N_("malloc: out of memory", ""));
	    *realms = NULL;
	    return ENOMEM;
	}
    }
    (*realms)[n] = NULL;

    return 0;
}

/*
 * Get the list of default realms and make sure there is at least
 * one realm configured.
 */

static krb5_error_code
get_default_realms(krb5_context context)
{
    if (context->default_realms == NULL ||
	heim_array_get_length(context->default_realms) == 0)
    {
	krb5_error_code ret = krb5_set_default_realm(context, NULL);
	if (ret)
	    return KRB5_CONFIG_NODEFREALM;
    }
    
    if (context->default_realms == NULL ||
	heim_array_get_length(context->default_realms) == 0)
    {
	krb5_set_error_message(context, KRB5_CONFIG_NODEFREALM,
			       N_("No default realm found", ""));
	return KRB5_CONFIG_NODEFREALM;
    }

    return 0;
}

/*
 * Return a NULL-terminated list of default realms in `realms'.
 * Free this memory with krb5_free_host_realm.
 */

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_get_default_realms (krb5_context context,
			 krb5_realm **realms)
{
    krb5_error_code ret;

    ret = get_default_realms(context);
    if (ret)
	return ret;
    
    return _krb5_array_to_realms(context, context->default_realms, realms);
}

/*
 * Return the first default realm.  For compatibility.
 */

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_get_default_realm(krb5_context context,
		       krb5_realm *realm)
{
    krb5_error_code ret;
    heim_string_t s;
    
    ret = get_default_realms(context);
    if (ret)
	return ret;
    
    s = heim_array_copy_value(context->default_realms, 0);
    if (s) {
	*realm = strdup(heim_string_get_utf8(s));
	heim_release(s);
    }
    if (s == NULL || *realm == NULL) {
	krb5_set_error_message(context, ENOMEM,
			       N_("malloc: out of memory", ""));
	return ENOMEM;
    }
    
    return 0;
}
