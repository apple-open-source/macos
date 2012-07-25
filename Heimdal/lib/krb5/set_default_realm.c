/*
 * Copyright (c) 1997 - 2001 Kungliga Tekniska Högskolan
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
#include "config_plugin.h"

static void
add_default_realm(krb5_context context, void *userctx, krb5_const_realm realm)
{
    heim_array_t array = userctx;
    heim_string_t s = heim_string_create(realm);

    if (s) {
	heim_array_append_value(array, s);
	heim_release(s);
    }
}

static krb5_error_code
config_plugin(krb5_context context,
	      const void *plug, void *plugctx, void *userctx)
{
    const krb5plugin_config_ftable *config = plug;
    if (config->get_default_realm == NULL)
	return KRB5_PLUGIN_NO_HANDLE;

    return config->get_default_realm(context, plugctx, userctx, add_default_realm);
}

static krb5_error_code
get_plugin(krb5_context context, heim_array_t array)
{
    return krb5_plugin_run_f(context, "krb5",
			     KRB5_PLUGIN_CONFIGURATION,
			     KRB5_PLUGIN_CONFIGURATION_VERSION_0,
			     0, array, config_plugin);
}

/**
 * Set the knowledge of the default realm(s) in context.
 * If realm is not NULL, that's the new default realm.
 * Otherwise, the realm(s) are figured out from plugin, configuration file or DNS.
 *
 * @param context Kerberos 5 context.
 * @param realm the new default realm or NULL for using configuration.

 * @return Returns 0 to indicate success. Otherwise an kerberos et
 * error code is returned, see krb5_get_error_message().
 *
 * @ingroup krb5
 */

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_set_default_realm(krb5_context context,
		       const char *realm)
{
    krb5_error_code ret = 0;
    krb5_realm *realms = NULL;
    heim_array_t array;
    size_t n;

    array = heim_array_create();

    if (realm == NULL) {
	get_plugin(context, array);

	realms = krb5_config_get_strings (context, NULL,
					  "libdefaults",
					  "default_realm",
					  NULL);
	if (realms == NULL && heim_array_get_length(array) == 0) {
	    ret = krb5_get_host_realm(context, NULL, &realms);
	    if (ret) {
		heim_release(array);
		return ret;
	    }
	}

	if (realms) {
	    for (n = 0; realms[n]; n++)
		add_default_realm(context, array, realms[n]);
	    krb5_free_host_realm(context, realms);
	}
    } else {
	add_default_realm(context, array, realm);
    }

    heim_release(context->default_realms);
    context->default_realms = array;

    return 0;
}
