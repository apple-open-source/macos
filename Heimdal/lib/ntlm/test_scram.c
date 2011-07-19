/*
 * Copyright (c) 2008 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2010 Apple Inc. All rights reserved.
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
#include <err.h>
#include <roken.h>
#include <getarg.h>
#include <base64.h>

#include <heimbase.h>

static int verbose_flag = 0;
static int version_flag = 0;
static int help_flag	= 0;

static struct getargs args[] = {
    {"verbose",	0,	arg_flag,	&verbose_flag, "verbose", NULL },
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

#ifdef ENABLE_SCRAM

#include "scram.h"

static int
test_parser(void)
{
#if 0
    heim_scram_pairs *d;
    size_t i;
    int ret;
    struct {
	char *str;
	int ret;
    } strings[] = {
	{ "", EINVAL },
	{ "a", EINVAL },
	{ "a=bar", 0 },
	{ "a=", 0 },
	{ "a=,", EINVAL },
	{ "a", EINVAL },
	{ "aa=", EINVAL },
	{ "a=,b", EINVAL },
	{ "a=,b=", 0 },
	{ "a=,b=,", EINVAL },
	{ "a=", 0 },
	{ "a=aaa,b=b  bb b  b", 0 },
	{ "a=aaa,b=b  bb b  b,c=    c  =", 0 },
	{ "a=a,b=AF,c====", 0 },
	{ "n,a=a,b=AF,c====", 0 },
	{ "y,a=a,b=AF,c====", 0 },
	{ "p=foo,a=a,b=AF,c====", 0 }
    };

    for (i = 0; i < sizeof(strings)/sizeof(strings[0]); i++) {
	heim_scram_data data, data2;

	data.data = strings[i].str;
	data.length = strlen(strings[i].str);

	d = NULL;

	ret = _heim_scram_parse(&data, &d);
	if (verbose_flag)
	    printf("%s -> %d\n", strings[i].str, ret);
	if (ret != strings[i].ret)
	    return 1;
	if (ret)
	    continue;

	ret = _heim_scram_unparse(d, &data2);
	if (ret)
	    return ret;
	if (verbose_flag)
	    printf("unparse %d %s = %.*s\n", ret,
		   strings[i].str,
		   (int)data2.length, (char *)data2.data);
	if (data.length != data2.length ||
	    memcmp(data.data, data2.data, data.length) != 0) {
	    heim_scram_data_free(&data2);
	    return 1;
	}
	heim_scram_data_free(&data2);
	_heim_scram_pairs_free(d);
    }

    printf("parse success\n");
#endif
    return 0;
}

static int
test_storekey(void)
{
    const char *pw = "password";
    const char salt[] = "c2FsdA==";
    char clientkey[20] = "\xdc\x58\xe3\x8a\xf4\xb5\x54\xc6\x95\x2c\xfe\xc6\xff\xe3\xea\x17\x5f\x44\xb6\x0e";
    char storekey[20] = "\xbd\x59\xe9\xd0\x58\x50\x66\x64\x11\x48\xcb\xf0\xf6\x8a\xb5\x2c\x53\x02\x87\xc1";
    char saltinfo[sizeof(salt)];
    heim_scram_data saltdata, key, client;
    int ret, len;

    len = base64_decode(salt, saltinfo);
    if (len < 0)
	return 1;
    
    saltdata.data = saltinfo;
    saltdata.length = len;

    /*
     * store key
     */

    ret = heim_scram_stored_key(HEIM_SCRAM_DIGEST_SHA1, pw, 1, &saltdata,
				&client, &key, NULL);
    if (ret)
	return 1;

    if (key.length != sizeof(storekey) ||
	memcmp(key.data, storekey, sizeof(storekey)) != 0)
	return 1;

    if (client.length != sizeof(clientkey) ||
	memcmp(client.data, clientkey, sizeof(clientkey)) != 0)
	return 1;

    printf("store key success\n");

    heim_scram_data_free(&key);

    heim_scram_data_free(&key);

    return 0;
}

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
    if (user->length != 3 && memcmp(user->data, "lha", 3) != 0)
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
				ctx, giterations, &gsalt,
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

static struct heim_scram_server server_procs = {
    .version = SCRAM_SERVER_VERSION_1,
    .param = param,
    .calculate = calculate
};

static int
test_exchange(void)
{
    int ret;
    heim_scram *cs = NULL, *ss = NULL;
    heim_scram_data cp, sp;

    ret = heim_scram_client1("lha", NULL, HEIM_SCRAM_DIGEST_SHA1, &cs, &cp);
    if (ret)
	goto out;

    printf("c1: %.*s\n", (int)cp.length, (char *)cp.data);

    ret = heim_scram_server1(&cp, NULL, HEIM_SCRAM_DIGEST_SHA1, 
			     &server_procs, "password", &ss, &sp);
    if (ret)
	goto out;

    printf("s1: %.*s\n", (int)sp.length, (char *)sp.data);

    ret = heim_scram_client2(&sp, HEIM_SCRAM_CLIENT_PASSWORD_PROCS, "password", cs, &cp);
    if (ret)
	goto out;
    
    printf("c2: %.*s\n", (int)cp.length, (char *)cp.data);

    ret = heim_scram_server2(&cp, ss, &sp);
    if (ret)
	goto out;

    printf("s2: %.*s\n", (int)sp.length, (char *)sp.data);

    ret = heim_scram_client3(&sp, cs);
    if (ret)
	goto out;

    printf("exchange success\n");

 out:
    heim_scram_free(cs);
    heim_scram_free(ss);

    return ret;
}
#endif /* ENABLE_SCRAM */

int
main(int argc, char **argv)
{
    int ret, optidx = 0;

    setprogname(argv[0]);

    if (getarg(args, sizeof(args) / sizeof(args[0]), argc, argv, &optidx))
	usage(1);

    if (help_flag)
	usage (0);

    if (version_flag){
	print_version(NULL);
	exit(0);
    }

    ret = 0;
#ifdef ENABLE_SCRAM
    ret |= test_parser();
    ret |= test_storekey();
    ret |= test_exchange();
#endif

    return ret;
}
