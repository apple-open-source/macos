/*
 * Copyright (c) 1997-2009 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2009 - 2010 Apple Inc. All rights reserved.
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

#include "kuser_locl.h"

#ifdef __APPLE__
#include <Security/Security.h>
#endif

struct krb5_dh_moduli;
struct AlgorithmIdentifier;
struct _krb5_krb_auth_data;
struct _krb5_key_data;
struct _krb5_key_type;
struct _krb5_checksum_type;
struct _krb5_encryption_type;
struct _krb5_srv_query_ctx;
struct krb5_fast_state;
struct _krb5_srp_group;
struct _krb5_srp;

#include <heimbase.h>
#include <hx509.h>
#include <krb5-private.h>

static void usage (int ret) __attribute__((noreturn));


int version_flag	= 0;
int help_flag		= 0;


static struct getargs args[] = {
    { "version", 	0,   arg_flag, &version_flag },
    { "help",		0,   arg_flag, &help_flag }
};

static void
usage (int ret)
{
    arg_printusage_i18n (args,
			 sizeof(args)/sizeof(*args),
			 N_("Usage: ", ""),
			 NULL,
			 "[principal [command]]",
			 getarg_i18n);
    exit (ret);
}

int
main (int argc, char **argv)
{
    krb5_principal client = NULL, server = NULL;
    krb5_error_code ret;
    krb5_context context;
    int optidx = 0;
    krb5_ccache id;
    char password[512] = { 0 } ;

    setprogname (argv[0]);

    ret = krb5_init_context (&context);
    if (ret)
	errx(1, "krb5_init_context failed: %d", ret);

    if(getarg(args, sizeof(args) / sizeof(args[0]), argc, argv, &optidx))
	usage(1);

    if (help_flag)
	usage (0);

    if(version_flag) {
	print_version(NULL);
	exit(0);
    }

    argc -= optidx;
    argv += optidx;

    if (argc != 1)
	krb5_errx(context, 1, "principal missing");

    ret = krb5_parse_name (context, argv[0], &client);
    if (ret)
	krb5_err (context, 1, ret, "krb5_parse_name(%s)", argv[0]);

    ret = krb5_cc_cache_match(context, client, &id);
    if (ret) {
	ret = krb5_cc_new_unique(context, "KCM", NULL, &id);
	if (ret)
	    krb5_err (context, 1, ret, "krb5_cc_new_unique");

	ret = krb5_cc_initialize(context, id, client);
	if (ret)
	    krb5_err (context, 1, ret, "krb5_cc_initialize");
    }

#if defined(__APPLE__) && !defined(__APPLE_TARGET_EMBEDDED__)
    {
	const char *realm;
	OSStatus osret;
	UInt32 length;
	void *buffer;
	char *name;

	realm = krb5_principal_get_realm(context, client);

	ret = krb5_unparse_name_flags(context, client,
				      KRB5_PRINCIPAL_UNPARSE_NO_REALM, &name);
	if (ret)
	    goto nopassword;

	osret = SecKeychainFindGenericPassword(NULL, (UInt32)strlen(realm), realm,
					       (UInt32)strlen(name), name,
					       &length, &buffer, NULL);
	free(name);
	if (osret != noErr)
	    goto nopassword;

	if (length < sizeof(password) - 1) {
	    memcpy(password, buffer, length);
	    password[length] = '\0';
	}
	SecKeychainItemFreeContent(NULL, buffer);

    nopassword:
	do { } while(0);
    }
#endif
    if (password[0] == 0) {
	char *p, *prompt;
	
	krb5_unparse_name (context, client, &p);
	asprintf (&prompt, "%s's Password: ", p);
	free (p);
	
	if (UI_UTIL_read_pw_string(password, sizeof(password)-1, prompt, 0)){
	    memset(password, 0, sizeof(password));
	    krb5_cc_destroy(context, id);
	    exit(1);
	}
	free (prompt);
    }

    ret = _krb5_kcm_get_initial_ticket(context, id, client, server, password);
    memset(password, 0, sizeof(password));
    if (ret) {
	krb5_cc_destroy(context, id);
	krb5_err (context, 1, ret, "_krb5_kcm_get_initial_ticket");
    }

    return 0;
}
