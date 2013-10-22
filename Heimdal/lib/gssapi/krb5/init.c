/*
 * Copyright (c) 1997 - 2001, 2003, 2006 Kungliga Tekniska Högskolan
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

heim_string_t _gsskrb5_kGSSICPassword;
heim_string_t _gsskrb5_kGSSICKerberosCacheName;
heim_string_t _gsskrb5_kGSSICCertificate;
heim_string_t _gsskrb5_kGSSICLKDCHostname;
heim_string_t _gsskrb5_kGSSICAppIdentifierACL;

static HEIMDAL_thread_key context_key;

static void
destroy_context(void *ptr)
{
    krb5_context context = ptr;

    if (context == NULL)
	return;
    krb5_free_context(context);
}

static void
once_func(void *ctx)
{
    int ret;

    _gsskrb5_kGSSICPassword = heim_string_create("kGSSICPassword");
    _gsskrb5_kGSSICCertificate = heim_string_create("kGSSICCertificate");
    _gsskrb5_kGSSICKerberosCacheName = heim_string_create("kGSSICKerberosCacheName");
    _gsskrb5_kGSSICLKDCHostname = heim_string_create("kGSSICLKDCHostname");
    _gsskrb5_kGSSICAppIdentifierACL = heim_string_create("kGSSICAppIdentifierACL");
    
    HEIMDAL_key_create(&context_key, destroy_context, ret);
}

krb5_error_code
_gsskrb5_init (krb5_context *context)
{
    static heim_base_once_t once;
    krb5_error_code ret = 0;

    heim_base_once_f(&once, NULL, once_func);

    *context = HEIMDAL_getspecific(context_key);
    if (*context == NULL) {

	ret = krb5_init_context(context);
	if (ret == 0) {
	    HEIMDAL_setspecific(context_key, *context, ret);
	    if (ret) {
		krb5_free_context(*context);
		*context = NULL;
	    }
	}
    } else {
	krb5_reload_config(*context, 0, NULL);
    }

    return ret;
}
