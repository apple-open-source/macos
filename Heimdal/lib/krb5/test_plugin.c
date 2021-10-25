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

#include <krb5_locl.h>
#include "locate_plugin.h"

static krb5_error_code
resolve_init(krb5_context context, void **ctx)
{
    *ctx = NULL;
    return 0;
}

static void
resolve_fini(void *ctx)
{
}

static krb5_error_code
resolve_lookup(void *ctx,
	       unsigned long flags,
	       enum locate_service_type service,
	       const char *realm,
	       int domain,
	       int type,
	       int (*add)(void *,int,struct sockaddr *),
	       void *addctx)
{
    struct sockaddr_in s;

    memset(&s, 0, sizeof(s));

#ifdef HAVE_STRUCT_SOCKADDR_SA_LEN
    s.sin_len = sizeof(s);
#endif
    s.sin_family = AF_INET;
    s.sin_port = htons(88);
    s.sin_addr.s_addr = htonl(0x7f000002);

    if (strcmp(realm, "NOTHERE.H5L.SE") == 0) {
	(*add)(addctx, type, (struct sockaddr *)&s);
    }

    return 0;
}

static krb5_error_code
resolve_lookup_old(void *ctx,
		   enum locate_service_type service,
		   const char *realm,
		   int domain,
		   int type,
		   int (*add)(void *,int,struct sockaddr *),
		   void *addctx)
{
    return resolve_lookup(ctx, KRB5_PLF_ALLOW_HOMEDIR, service,
			  realm, domain, type, add, addctx);
}

static krb5_error_code
resolve_lookup_host_string(void *ctx,
		     unsigned long flags,
		     enum locate_service_type service,
		     const char *realm,
		     int domain,
		     int type,
                     int (*addfunc)(void *,const char *),
		     void *addctx)
{
    if (strcmp(realm, "NOTHERE.H5L.SE") == 0) {
	addfunc(addctx, "tcp/kdc.NOTHERE.H5L.SE:123");
    } else {
	return KRB5_PLUGIN_NO_HANDLE;
    }

    return 0;
}


krb5plugin_service_locate_ftable resolve = {
    KRB5_PLUGIN_LOCATE_VERSION_2,
    resolve_init,
    resolve_fini,
    resolve_lookup_old,
    resolve_lookup,
    resolve_lookup_host_string
};

int
main(int argc, char **argv)
{
    krb5_error_code ret;
    krb5_context context;
    krb5_krbhst_handle handle;
    char host[MAXHOSTNAMELEN];
    int found = 0;

    setprogname(argv[0]);

    ret = krb5_init_context(&context);
    if (ret)
	errx(1, "krb5_init_contex");

    ret = krb5_plugin_register_module(context, "krb5", PLUGIN_TYPE_DATA,
			       KRB5_PLUGIN_LOCATE, &resolve);
    if (ret) {
	krb5_err(context, 1, ret, "krb5_plugin_register");
    }

    ret = krb5_krbhst_init_flags(context,
				 "NOTHERE.H5L.SE",
				 KRB5_KRBHST_KDC,
				 0,
				 &handle);
    if (ret) {
	krb5_err(context, 1, ret, "krb5_krbhst_init_flags");
    }

    while(krb5_krbhst_next_as_string(context, handle, host, sizeof(host)) == 0){
	if (strcmp(host, "127.0.0.2") == 0) {
	    found++;
	}
    }
    if (!found) {
	krb5_errx(context, 1, "failed to find host");
    }

    //reset and check v3 plugins
    krb5_krbhst_free(context, handle);
    resolve.minor_version = KRB5_PLUGIN_LOCATE_VERSION_3;
    found = 0;

    ret = krb5_krbhst_init_flags(context,
				 "NOTHERE.H5L.SE",
				 KRB5_KRBHST_KDC,
				 0,
				 &handle);
    if (ret) {
	krb5_err(context, 1, ret, "krb5_krbhst_init_flags v3");
    }

    while(krb5_krbhst_next_as_string(context, handle, host, sizeof(host)) == 0){
	if (strcmp(host, "tcp/kdc.nothere.h5l.se:123") == 0) {
	    found++;
	}
    }
    if (!found) {
	krb5_errx(context, 1, "failed to find host v3");
    }

    //reset and check v3 plugins without host_string
    krb5_krbhst_free(context, handle);
    resolve.minor_version = KRB5_PLUGIN_LOCATE_VERSION_3;
    resolve.lookup_host_string = NULL;
    found = 0;

    ret = krb5_krbhst_init_flags(context,
				 "NOTHERE.H5L.SE",
				 KRB5_KRBHST_KDC,
				 0,
				 &handle);
    if (ret) {
	krb5_err(context, 1, ret, "krb5_krbhst_init_flags v3");
    }

    while(krb5_krbhst_next_as_string(context, handle, host, sizeof(host)) == 0){
	if (strcmp(host, "127.0.0.2") == 0) {
	    found++;
	}
    }
    if (!found) {
	krb5_errx(context, 1, "failed to find host v3");
    }

    krb5_krbhst_free(context, handle);

    krb5_free_context(context);
    return 0;
}
