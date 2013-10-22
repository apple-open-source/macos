/*
 * Copyright (c) 2006 - 2010 Kungliga Tekniska Högskolan
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
#include <gssapi_scram.h>
#include <gssapi_spi.h>
#include <err.h>
#include <roken.h>
#include <hex.h>
#include <getarg.h>
#include "test_common.h"

static char *user_string = NULL;
static char *password_string = NULL;

#ifdef ENABLE_SCRAM

#include <heimscram.h>

/*
 *
 */

static unsigned int giterations = 1000;
static heim_scram_data gsalt = {
    .data = rk_UNCONST("salt"),
    .length = 4
};

static int
param(void *ctx,
      const heim_scram_data *user,
      heim_scram_data *salt,
      unsigned int *iteration,
      heim_scram_data *servernonce)
{
    if (user->length != strlen(user_string) && memcmp(user->data, user_string, user->length) != 0)
	return ENOENT;

    *iteration = giterations;

    salt->data = malloc(gsalt.length);
    memcpy(salt->data, gsalt.data, gsalt.length);
    salt->length = gsalt.length;

    servernonce->data = NULL;
    servernonce->length = 0;

    return 0;
}

static int
calculate(void *ctx,
	  heim_scram_method method,
	  const heim_scram_data *user,
	  const heim_scram_data *c1,
	  const heim_scram_data *s1,
	  const heim_scram_data *c2noproof,
	  const heim_scram_data *proof,
	  heim_scram_data *server,
	  heim_scram_data *sessionKey)
{
    heim_scram_data client_key, client_key2, stored_key, server_key, clientSig;
    int ret;

    memset(&client_key2, 0, sizeof(client_key2));

    ret = heim_scram_stored_key(method,
				password_string, giterations, &gsalt,
				&client_key, &stored_key, &server_key);
    if (ret)
	return ret;

    ret = heim_scram_generate(method, &stored_key, &server_key,
			      c1, s1, c2noproof, &clientSig, server);
    heim_scram_data_free(&server_key);
    if (ret)
	goto out;

    ret = heim_scram_validate_client_signature(method,
					       &stored_key,
					       &clientSig,
					       proof,
					       &client_key2);
    if (ret)
	goto out;


    /* extra check since we know the client key */
    if (client_key2.length != client_key.length ||
	memcmp(client_key.data, client_key2.data, client_key.length) != 0) {
	ret = EINVAL;
	goto out;
    }

    ret = heim_scram_session_key(method,
				 &stored_key,
				 &client_key,
				 c1, s1, c2noproof, sessionKey);
    if (ret)
	goto out;

 out:
    heim_scram_data_free(&stored_key);
    heim_scram_data_free(&client_key);
    heim_scram_data_free(&client_key2);

    return ret;
}

static struct heim_scram_server server_proc = {
    .version = SCRAM_SERVER_VERSION_1,
    .param = param,
    .calculate = calculate
};

static gss_cred_id_t client_cred = GSS_C_NO_CREDENTIAL;

static void
ac_complete(void *ctx, OM_uint32 major, gss_status_id_t status,
	    gss_cred_id_t cred, gss_OID_set oids, OM_uint32 time_rec)
{
    OM_uint32 junk;

    if (major) {
	fprintf(stderr, "error: %d", (int)major);
	gss_release_cred(&junk, &cred);
	goto out;
    }

    client_cred = cred;

 out:
    gss_release_oid_set(&junk, &oids);
}


static int
test_scram(const char *test_name, const char *user, const char *password)
{
    gss_name_t cname, target = GSS_C_NO_NAME;
    OM_uint32 maj_stat, min_stat;
    gss_ctx_id_t ctx = GSS_C_NO_CONTEXT;
    gss_buffer_desc cn, input, output, output2;
    int ret;
    heim_scram *scram = NULL;
    heim_scram_data in, out;
    gss_auth_identity_desc identity;


    memset(&identity, 0, sizeof(identity));

    identity.username = rk_UNCONST(user);
    identity.realm = "";
    identity.password = rk_UNCONST(password);

    cn.value = rk_UNCONST(user);
    cn.length = strlen(user);

    maj_stat = gss_import_name(&min_stat, &cn, GSS_C_NT_USER_NAME, &cname);
    if (maj_stat)
	errx(1, "gss_import_name: %d", (int)maj_stat);

    maj_stat = gss_acquire_cred_ex_f(NULL,
				     cname,
				     0,
				     GSS_C_INDEFINITE,
				     GSS_SCRAM_MECHANISM,
				     GSS_C_INITIATE,
				     &identity,
				     NULL,
				     ac_complete);
    if (maj_stat)
	errx(1, "gss_acquire_cred_ex_f: %d", (int)maj_stat);

    if (client_cred == GSS_C_NO_CREDENTIAL)
	errx(1, "gss_acquire_cred_ex_f");

    cn.value = rk_UNCONST("host@localhost");
    cn.length = strlen((char *)cn.value);

    maj_stat = gss_import_name(&min_stat, &cn,
			       GSS_C_NT_HOSTBASED_SERVICE, &target);
    if (maj_stat)
	errx(1, "gss_import_name: %d", (int)maj_stat);

    maj_stat = gss_init_sec_context(&min_stat, client_cred, &ctx, 
				    target, GSS_SCRAM_MECHANISM, 
				    0, 0, NULL,
				    GSS_C_NO_BUFFER, NULL, 
				    &output, NULL, NULL); 
    if (maj_stat != GSS_S_CONTINUE_NEEDED)
	errx(1, "accept_sec_context %s %s", test_name,
	      gssapi_err(maj_stat, min_stat, GSS_C_NO_OID));

    if (output.length == 0)
	errx(1, "output.length == 0");

    maj_stat = gss_decapsulate_token(&output, GSS_SCRAM_MECHANISM, &output2);
    if (maj_stat)
	errx(1, "decapsulate token");

    in.length = output2.length;
    in.data = output2.value;

    ret = heim_scram_server1(&in, NULL, HEIM_SCRAM_DIGEST_SHA1, &server_proc, NULL, &scram, &out);
    if (ret)
	errx(1, "heim_scram_server1");

    gss_release_buffer(&min_stat, &output);

    input.length = out.length;
    input.value = out.data;

    maj_stat = gss_init_sec_context(&min_stat, client_cred, &ctx,
				    target, GSS_SCRAM_MECHANISM,
				    0, 0, NULL,
				    &input, NULL,
				    &output, NULL, NULL);
    if (maj_stat != GSS_S_CONTINUE_NEEDED) {
	warnx("accept_sec_context v1 2 %s",
	     gssapi_err(maj_stat, min_stat, GSS_C_NO_OID));
	return 1;
    }

    in.length = output.length;
    in.data = output.value;

    ret = heim_scram_server2(&in, scram, &out);
    if (ret)
	errx(1, "heim_scram_server2");

    gss_release_buffer(&min_stat, &output);

    input.length = out.length;
    input.value = out.data;

    maj_stat = gss_init_sec_context(&min_stat, client_cred, &ctx, 
				    target, GSS_SCRAM_MECHANISM, 
				    0, 0, NULL,
				    &input, NULL, 
				    &output, NULL, NULL); 
    if (maj_stat != GSS_S_COMPLETE) {
	warnx("accept_sec_context v1 2 %s",
	     gssapi_err(maj_stat, min_stat, GSS_C_NO_OID));
	return 1;
    }

    heim_scram_free(scram);

    //gss_destroy_cred(NULL, &client_cred);

    printf("done: %s\n", test_name);

    return 0;
}

#endif /* ENABLE_SCRAM */

/*
 *
 */

static int version_flag = 0;
static int help_flag	= 0;

static struct getargs args[] = {
    {"user",	0,	arg_string,	&user_string, "user name", "user" },
    {"password",0,	arg_string,	&password_string, "password", "password" },
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
	usage(0);

    if(version_flag){
	print_version(NULL);
	exit(0);
    }

    if (user_string == NULL)
	errx(1, "no username");
    if (password_string == NULL)
	errx(1, "no password");

#ifdef ENABLE_SCRAM
    ret += test_scram("scram", user_string, password_string);
#endif

    return (ret != 0) ? 1 : 0;
}
