/*
 * Copyright (C) 2007 Apple Inc.  All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of Apple Inc, nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * <krb5/authdata_plugin.h>
 *
 * AuthorizationData plugin definitions for Kerberos 5.
 *
 */

#ifndef KRB5_AUTHDATA_PLUGIN_H_INCLUDED
#define KRB5_AUTHDATA_PLUGIN_H_INCLUDED
#include <krb5/krb5.h>

/*
 * While arguments of these types are passed-in, for the most part a preauth
 * module can treat them as opaque.  If we need keying data, we can ask for
 * it directly.
 */
struct _krb5_db_entry_new;

/*
 * The function table / structure which a preauth server module must export as
 * "authdata_server_0".  NOTE: replace "0" with "1" for the type and
 * variable names if this gets picked up by upstream.  If the interfaces work
 * correctly, future versions of the table will add either more callbacks or
 * more arguments to callbacks, and in both cases we'll be able to wrap the v0
 * functions.
 */
typedef struct krb5plugin_authdata_ftable_v0 {
    /* Not-usually-visible name. */
    char *name;

    /* Per-plugin initialization/cleanup.  The init function is called by the
     * KDC when the plugin is loaded, and the fini function is called before
     * the plugin is unloaded.  Both are optional. */
    krb5_error_code (*init_proc)(krb5_context, void **);
    void (*fini_proc)(krb5_context, void *);
    krb5_error_code (*authdata_proc)(krb5_context,
   				   struct _krb5_db_entry_new *client,
				   krb5_data *req_pkt,
				   krb5_kdc_req *request,
				   krb5_enc_tkt_part *enc_tkt_reply);
} krb5plugin_authdata_ftable_v0;
#endif /* KRB5_AUTHDATA_PLUGIN_H_INCLUDED */