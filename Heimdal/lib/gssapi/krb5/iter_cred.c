/*
 * Copyright (c) 2009 Kungliga Tekniska Högskolan
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
#include <heim_threads.h>

static void
iter_creds_f(OM_uint32 flags,
	     gss_OID type,
	     void *userctx ,
	     void (*cred_iter)(void *, gss_OID, gss_cred_id_t))
{
    krb5_context context;
    krb5_cccol_cursor cursor;
    krb5_error_code ret;
    krb5_ccache id;

    GSSAPI_KRB5_INIT_GOTO(&context, out);

    ret = krb5_cccol_cursor_new (context, &cursor);
    if (ret)
	goto out;

    while (krb5_cccol_cursor_next (context, cursor, &id) == 0 && id != NULL) {
	gsskrb5_cred handle;
	OM_uint32 junk;
	krb5_principal principal;
	krb5_data data;
	gss_OID resolved_type = NULL;

	ret = krb5_cc_get_principal(context, id, &principal);
	if (ret) {
	    krb5_cc_close(context, id);
	    continue;
	}
	
	if (krb5_principal_is_pku2u(context, principal))
	    resolved_type = GSS_PKU2U_MECHANISM;
	else if (krb5_cc_get_config(context, id, NULL, "iakerb", &data) == 0) {
	    resolved_type = GSS_IAKERB_MECHANISM;
	    krb5_data_free(&data);
	} else {
	    resolved_type = GSS_KRB5_MECHANISM;
	}

	if (!gss_oid_equal(type, resolved_type)) {
	    krb5_free_principal(context, principal);
	    krb5_cc_close(context, id);
	    continue;
	}

	handle = calloc(1, sizeof(*handle));
	if (handle == NULL) {
	    krb5_cc_close(context, id);
	    goto out;
	}

	HEIMDAL_MUTEX_init(&handle->cred_id_mutex);
	
	handle->usage = GSS_C_INITIATE;
	handle->principal = principal;

	__gsskrb5_ccache_lifetime(&junk, context, id,
				  handle->principal, &handle->endtime);
	handle->keytab = NULL;
	handle->ccache = id;

	cred_iter(userctx, type, (gss_cred_id_t)handle);
    }

    krb5_cccol_cursor_free(context, &cursor);

 out:
    cred_iter(userctx, NULL, NULL);
}		 

void
_gss_pku2u_iter_creds_f(OM_uint32 flags,
			void *userctx ,
			void (*cred_iter)(void *, gss_OID, gss_cred_id_t))
{
    iter_creds_f(flags, GSS_PKU2U_MECHANISM, userctx, cred_iter);
}

void
_gss_krb5_iter_creds_f(OM_uint32 flags,
		       void *userctx ,
		       void (*cred_iter)(void *, gss_OID, gss_cred_id_t))
{
    iter_creds_f(flags, GSS_KRB5_MECHANISM, userctx, cred_iter);
}

void
_gss_iakerb_iter_creds_f(OM_uint32 flags,
		       void *userctx ,
		       void (*cred_iter)(void *, gss_OID, gss_cred_id_t))
{
    iter_creds_f(flags, GSS_IAKERB_MECHANISM, userctx, cred_iter);
}
