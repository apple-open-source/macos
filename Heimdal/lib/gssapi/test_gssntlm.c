/*
 * Copyright (c) 2006 - 2008 Kungliga Tekniska Högskolan
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
 * 3. Neither the name of KTH nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY KTH AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL KTH OR ITS CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include <stdio.h>
#include <gssapi.h>
#include <gssapi_ntlm.h>
#include <err.h>
#include <roken.h>
#include <hex.h>
#include <getarg.h>
#include "test_common.h"

static int use_server_domain = 0;
static int verbose_flag = 0;
static int broken_session_key_flag = 0;

#ifdef ENABLE_NTLM

#include <krb5.h>
#include <heimntlm.h>

#define HC_DEPRECATED_CRYPTO

#include "crypto-headers.h"

static void
dump_packet(const char *name, const void *data, size_t len)
{
    char *p;

    printf("%s\n", name);
    hex_encode(data, len, &p);
    printf("%s\n", p);
    free(p);
}


static void
dump_pac(gss_ctx_id_t ctx)
{
    OM_uint32 min_stat;
    gss_buffer_set_t pac = GSS_C_NO_BUFFER_SET;

    if (gss_inquire_sec_context_by_oid(&min_stat,
				       ctx,
				       GSS_C_INQ_WIN2K_PAC_X,
				       &pac) == GSS_S_COMPLETE &&
	pac->elements != NULL) {

	dump_packet("Win2K PAC", pac->elements[0].value, pac->elements[0].length);
	gss_release_buffer_set(&min_stat, &pac);
    }
}

static void
verify_session_key(gss_ctx_id_t ctx,
		   struct ntlm_buf *sessionkey,
		   const char *version)
{
    OM_uint32 maj_stat, min_stat;
    gss_buffer_set_t key;

    maj_stat = gss_inquire_sec_context_by_oid(&min_stat,
					      ctx,
					      GSS_NTLM_GET_SESSION_KEY_X,
					      &key);
    if (maj_stat != GSS_S_COMPLETE || key->count != 1)
	errx(1, "GSS_NTLM_GET_SESSION_KEY_X: %s", version);
    
    if (key->elements[0].length == 0) {
	warnx("no session not negotiated");
	goto out;
    }

    if (key->elements[0].length != sessionkey->length)
	errx(1, "key length wrong: %d version: %s",
	     (int)key->elements[0].length, version);
    
    if(memcmp(key->elements[0].value,
	      sessionkey->data, sessionkey->length) != 0) {
	dump_packet("AD    session key", key->elements[0].value, key->elements[0].length);
	dump_packet("local session key", sessionkey->data, sessionkey->length);
	if (!broken_session_key_flag)
	    errx(1, "session key wrong: version: %s", version);
    }
    
 out:
    gss_release_buffer_set(&min_stat, &key);
}

static int
test_libntlm_v1(const char *test_name, int flags,
		const char *user, const char *domain, const char *password)
{
    OM_uint32 maj_stat, min_stat;
    gss_ctx_id_t ctx = GSS_C_NO_CONTEXT;
    gss_buffer_desc input, output;
    struct ntlm_type1 type1;
    struct ntlm_type2 type2;
    struct ntlm_type3 type3;
    struct ntlm_buf data;
    krb5_error_code ret;
    gss_name_t src_name = GSS_C_NO_NAME;
    struct ntlm_buf sessionkey;

    memset(&type1, 0, sizeof(type1));
    memset(&type2, 0, sizeof(type2));
    memset(&type3, 0, sizeof(type3));

    type1.flags = 
	NTLM_NEG_UNICODE|NTLM_NEG_TARGET|
	NTLM_NEG_NTLM|NTLM_NEG_VERSION|
	flags;
    type1.domain = strdup(domain);
    type1.hostname = NULL;
    type1.os[0] = 0;
    type1.os[1] = 0;

    ret = heim_ntlm_encode_type1(&type1, &data);
    if (ret)
	errx(1, "heim_ntlm_encode_type1");

    if (verbose_flag)
	dump_packet("type1", data.data, data.length);

    input.value = data.data;
    input.length = data.length;

    output.length = 0;
    output.value = NULL;

    maj_stat = gss_accept_sec_context(&min_stat,
				      &ctx,
				      GSS_C_NO_CREDENTIAL,
				      &input,
				      GSS_C_NO_CHANNEL_BINDINGS,
				      NULL,
				      NULL,
				      &output,
				      NULL,
				      NULL,
				      NULL);
    free(data.data);
    if (GSS_ERROR(maj_stat)) {
	warnx("accept_sec_context 1 %s: %s",
	      test_name, gssapi_err(maj_stat, min_stat, GSS_C_NO_OID));
	return 0;
    }

    if (output.length == 0)
	errx(1, "output.length == 0");

    data.data = output.value;
    data.length = output.length;

    if (verbose_flag)
	dump_packet("type2", data.data, data.length);

    ret = heim_ntlm_decode_type2(&data, &type2);
    if (ret)
	errx(1, "heim_ntlm_decode_type2");

    gss_release_buffer(&min_stat, &output);

    type3.flags = type1.flags & type2.flags;
    type3.username = rk_UNCONST(user);
    if (use_server_domain)
	type3.targetname = type2.targetname;
    else
	type3.targetname = rk_UNCONST(domain);
    type3.ws = rk_UNCONST("workstation");

    {
	struct ntlm_buf key, tempsession;

	heim_ntlm_nt_key(password, &key);

	heim_ntlm_calculate_ntlm1(key.data, key.length,
				  type2.challenge,
				  &type3.ntlm);

	heim_ntlm_v1_base_session(key.data, key.length, &tempsession);
	heim_ntlm_free_buf(&key);

	if (type3.flags & NTLM_NEG_KEYEX) {
	    heim_ntlm_keyex_wrap(&tempsession, &sessionkey, &type3.sessionkey);
	    heim_ntlm_free_buf(&tempsession);
	} else {
	    sessionkey = tempsession;
	}
    }

    ret = heim_ntlm_encode_type3(&type3, &data, NULL);
    if (ret)
	errx(1, "heim_ntlm_encode_type3");

    if (verbose_flag)
	dump_packet("type3", data.data, data.length);

    input.length = data.length;
    input.value = data.data;

    maj_stat = gss_accept_sec_context(&min_stat,
				      &ctx,
				      GSS_C_NO_CREDENTIAL,
				      &input,
				      GSS_C_NO_CHANNEL_BINDINGS,
				      &src_name,
				      NULL,
				      &output,
				      NULL,
				      NULL,
				      NULL);
    free(input.value);
    if (maj_stat != GSS_S_COMPLETE) {
	warnx("accept_sec_context 2 %s: %s",
	      test_name, gssapi_err(maj_stat, min_stat, GSS_C_NO_OID));
	return 1;
    }

    gss_release_buffer(&min_stat, &output);

    verify_session_key(ctx, &sessionkey, 
		       (flags & NTLM_NEG_KEYEX) ? "v1-keyex" : "v1");

    heim_ntlm_free_buf(&sessionkey);

    if (verbose_flag)
	dump_pac(ctx);

    /* check that we have a source name */

    if (src_name == GSS_C_NO_NAME)
	errx(1, "no source name!");

    gss_display_name(&min_stat, src_name, &output, NULL);

    if (verbose_flag)
      printf("src_name: %.*s\n", (int)output.length, (char*)output.value);

    gss_release_name(&min_stat, &src_name);
    gss_release_buffer(&min_stat, &output);

    gss_delete_sec_context(&min_stat, &ctx, NULL);

    printf("done: %s\n", test_name);

    return 0;
}

static int
test_libntlm_v2(const char *test_name, int flags,
		const char *user, const char *domain, const char *password)
{
    OM_uint32 maj_stat, min_stat;
    gss_ctx_id_t ctx = GSS_C_NO_CONTEXT;
    gss_name_t src_name = GSS_C_NO_NAME;
    gss_buffer_desc input, output;
    struct ntlm_type1 type1;
    struct ntlm_type2 type2;
    struct ntlm_type3 type3;
    struct ntlm_buf data;
    krb5_error_code ret;
    struct ntlm_buf sessionkey;

    memset(&type1, 0, sizeof(type1));
    memset(&type2, 0, sizeof(type2));
    memset(&type3, 0, sizeof(type3));

    type1.flags = NTLM_NEG_UNICODE|NTLM_NEG_NTLM|flags;
    type1.domain = strdup(domain);
    type1.hostname = NULL;
    type1.os[0] = 0;
    type1.os[1] = 0;

    ret = heim_ntlm_encode_type1(&type1, &data);
    if (ret)
	errx(1, "heim_ntlm_encode_type1");

    if (verbose_flag)
	dump_packet("type1", data.data, data.length);

    input.value = data.data;
    input.length = data.length;

    output.length = 0;
    output.value = NULL;

    maj_stat = gss_accept_sec_context(&min_stat,
				      &ctx,
				      GSS_C_NO_CREDENTIAL,
				      &input,
				      GSS_C_NO_CHANNEL_BINDINGS,
				      NULL,
				      NULL,
				      &output,
				      NULL,
				      NULL,
				      NULL);
    free(data.data);
    if (GSS_ERROR(maj_stat)) {
	warnx("accept_sec_context 1 %s: %s",
	      test_name, gssapi_err(maj_stat, min_stat, GSS_C_NO_OID));
	return 1;
    }

    if (output.length == 0)
	errx(1, "output.length == 0");

    data.data = output.value;
    data.length = output.length;

    if (verbose_flag)
	dump_packet("type2", data.data, data.length);

    ret = heim_ntlm_decode_type2(&data, &type2);
    if (ret)
	errx(1, "heim_ntlm_decode_type2: %d", ret);

    if (type2.targetinfo.length) {
	struct ntlm_targetinfo ti;

	ret = heim_ntlm_decode_targetinfo(&type2.targetinfo, 1, &ti);
	if (ret)
	    errx(1, "heim_ntlm_decode_targetinfo: %d", ret);
	
	if (ti.domainname == NULL)
	    errx(1, "no domain name, windows clients hates this");
	if (ti.servername == NULL)
	    errx(1, "no servername name, windows clients hates this");
	
	heim_ntlm_free_targetinfo(&ti);
    } else {
	warnx("no targetinfo");
    }

    type3.flags = type1.flags & type2.flags;
    type3.username = rk_UNCONST(user);
    if (use_server_domain)
	type3.targetname = type2.targetname;
    else
	type3.targetname = rk_UNCONST(domain);
    type3.ws = rk_UNCONST("workstation");

    {
	struct ntlm_buf key, tempsession, chal;
	unsigned char ntlmv2[16];

	heim_ntlm_nt_key(password, &key);

	if (verbose_flag)
	    dump_packet("user key", key.data, key.length);

        heim_ntlm_calculate_lm2(key.data, key.length,
                                user,
                                type3.targetname,
                                type2.challenge,
                                ntlmv2,
                                &type3.lm);

        chal.length = 8;
        chal.data = type2.challenge;

	if (verbose_flag)
	    dump_packet("lm", type3.lm.data, type3.lm.length);

	heim_ntlm_calculate_ntlm2(key.data, key.length,
				  user,
				  type3.targetname,
				  type2.challenge,
				  &type2.targetinfo,
				  ntlmv2,
				  &type3.ntlm);

	if (verbose_flag)
	    dump_packet("ntlm", type3.ntlm.data, type3.ntlm.length);

	heim_ntlm_v2_base_session(ntlmv2, sizeof(ntlmv2),
				  &type3.ntlm,
				  &tempsession);
	if (verbose_flag)
	    dump_packet("base session key", tempsession.data, tempsession.length);

	heim_ntlm_free_buf(&key);

	if (type3.flags & NTLM_NEG_KEYEX) {
	    heim_ntlm_keyex_wrap(&tempsession, &sessionkey, &type3.sessionkey);
	    heim_ntlm_free_buf(&tempsession);
	} else {
	    sessionkey = tempsession;
	}
	memset(ntlmv2, 0, sizeof(ntlmv2));

	if (verbose_flag)
	    dump_packet("session key", sessionkey.data, sessionkey.length);
    }

    ret = heim_ntlm_encode_type3(&type3, &data, NULL);
    if (ret)
	errx(1, "heim_ntlm_encode_type3");

    if (verbose_flag)
	dump_packet("type3", data.data, data.length);

    input.length = data.length;
    input.value = data.data;

    maj_stat = gss_accept_sec_context(&min_stat,
				      &ctx,
				      GSS_C_NO_CREDENTIAL,
				      &input,
				      GSS_C_NO_CHANNEL_BINDINGS,
				      &src_name,
				      NULL,
				      &output,
				      NULL,
				      NULL,
				      NULL);
    free(input.value);
    if (maj_stat != GSS_S_COMPLETE) {
	warnx("accept_sec_context 2 %s: %s",
	      test_name, gssapi_err(maj_stat, min_stat, GSS_C_NO_OID));
	return 1;
    }

    gss_release_buffer(&min_stat, &output);

    verify_session_key(ctx, &sessionkey,
		       (flags & NTLM_NEG_KEYEX) ? "v2-keyex" : "v2");

    heim_ntlm_free_buf(&sessionkey);

    if (verbose_flag)
	dump_pac(ctx);

    /* check that we have a source name */

    if (src_name == GSS_C_NO_NAME)
	errx(1, "no source name!");

    gss_display_name(&min_stat, src_name, &output, NULL);

    if (verbose_flag)
      printf("src_name: %.*s\n", (int)output.length, (char*)output.value);

    gss_release_name(&min_stat, &src_name);
    gss_release_buffer(&min_stat, &output);

    gss_delete_sec_context(&min_stat, &ctx, NULL);

    printf("done: %s\n", test_name);

    return 0;
}
#endif

static char *user_string = NULL;
static char *domain_string = NULL;
static char *password_string = NULL;
static int version_flag = 0;
static int help_flag	= 0;

static int ntlmv1 = 1;
static int ntlmv2 = 1;

static struct getargs args[] = {
    {"user",	0,	arg_string,	&user_string, "user name", "user" },
    {"domain",	0,	arg_string,	&domain_string, "domain", "domain" },
    {"use-server-domain",0,arg_flag,	&use_server_domain, "use server domain" },
    {"password",0,	arg_string,	&password_string, "password", "password" },
    {"ntlm1", 0,	arg_negative_flag, &ntlmv1, "don't test NTLMv1", NULL},
    {"ntlm2", 0,	arg_negative_flag, &ntlmv2, "don't test NTLMv2", NULL},
    {"session-key-broken",0,arg_flag,	&broken_session_key_flag, "session key is broken, we know", NULL },
    {"verbose",	0,	arg_flag,	&verbose_flag, "verbose debug output", NULL },
    {"version",	0,	arg_flag,	&version_flag, "print version", NULL },
    {"help",	0,	arg_flag,	&help_flag,  NULL, NULL }
};

static void
usage (int ret)
{
    arg_printusage (args, sizeof(args)/sizeof(*args),
		    NULL, "");
    exit (ret);
}

int
main(int argc, char **argv)
{
    int ret = 0, optidx = 0;

    setprogname(argv[0]);

    if(getarg(args, sizeof(args) / sizeof(args[0]), argc, argv, &optidx))
	usage(1);

    if (help_flag)
	usage (0);

    if(version_flag){
	print_version(NULL);
	exit(0);
    }

#ifdef ENABLE_NTLM
    if (user_string == NULL)
	errx(1, "no username");
    if (domain_string == NULL)
	domain_string = "";
    if (password_string == NULL)
	errx(1, "no password");

    if (ntlmv1) {
	ret += test_libntlm_v1("v1", 0, user_string, domain_string, password_string);
	ret += test_libntlm_v1("v1 kex", NTLM_NEG_KEYEX, user_string, domain_string, password_string);
    }

    if (ntlmv2) {
	ret += test_libntlm_v2("v2", 0, user_string, domain_string, password_string);
	ret += test_libntlm_v2("v2 kex", NTLM_NEG_KEYEX, user_string, domain_string, password_string);
    }

#endif
    return (ret != 0) ? 1 : 0;
}
