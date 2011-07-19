/*
 * Copyright (c) 2010 Kungliga Tekniska Högskolan
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
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

#include "krb5_locl.h"
#include <getarg.h>

/*
 *
 */

static char *realm_string = NULL;
static int use_large_flag = 0;
static int version_flag = 0;
static int help_flag	= 0;

static struct getargs args[] = {
    {"realm",		0,	arg_string,	&realm_string,
     "realm", NULL },
    {"use-large",	0,	arg_flag,	&use_large_flag,
     "use transport suitable for large packets", NULL },
    {"version",		0,	arg_flag,	&version_flag,
     "print version", NULL },
    {"help",		0,	arg_flag,	&help_flag,
     NULL, NULL }
};

static void
usage (int ret)
{
    arg_printusage (args,
		    sizeof(args)/sizeof(*args),
		    NULL,
		    "");
    exit (ret);
}

int
main(int argc, char **argv)
{
    krb5_context context;
    krb5_error_code ret;
    int optidx = 0;
    krb5_sendto_ctx ctx = NULL;

    setprogname(argv[0]);

    if(getarg(args, sizeof(args) / sizeof(args[0]), argc, argv, &optidx))
	usage(1);

    if (help_flag)
	usage (0);

    if(version_flag){
	print_version(NULL);
	exit(0);
    }

    ret = krb5_init_context (&context);
    if (ret)
	errx (1, "krb5_init_context failed: %d", ret);


    if (realm_string == NULL)
	errx(1, "missing realm");

    krb5_data send = {
	.data =  /* lha@WELLKNOWN:COM.APPLE.LKDC:localhost */
	"\x6a\x81\xb8\x30\x81\xb5\xa1\x03\x02\x01\x05\xa2\x03\x02\x01\x0a"
	"\xa4\x81\xa8\x30\x81\xa5\xa0\x07\x03\x05\x00\x40\x01\x00\x00\xa1"
	"\x10\x30\x0e\xa0\x03\x02\x01\x01\xa1\x07\x30\x05\x1b\x03\x6c\x68"
	"\x61\xa2\x24\x1b\x22\x57\x45\x4c\x4c\x4b\x4e\x4f\x57\x4e\x3a\x43"
	"\x4f\x4d\x2e\x41\x50\x50\x4c\x45\x2e\x4c\x4b\x44\x43\x3a\x6c\x6f"
	"\x63\x61\x6c\x68\x6f\x73\x74\xa3\x37\x30\x35\xa0\x03\x02\x01\x01"
	"\xa1\x2e\x30\x2c\x1b\x06\x6b\x72\x62\x74\x67\x74\x1b\x22\x57\x45"
	"\x4c\x4c\x4b\x4e\x4f\x57\x4e\x3a\x43\x4f\x4d\x2e\x41\x50\x50\x4c"
	"\x45\x2e\x4c\x4b\x44\x43\x3a\x6c\x6f\x63\x61\x6c\x68\x6f\x73\x74"
	"\xa5\x11\x18\x0f\x32\x30\x31\x30\x30\x37\x33\x30\x31\x30\x33\x35"
	"\x31\x38\x5a\xa7\x06\x02\x04\x3a\xcc\xb8\xba\xa8\x0e\x30\x0c\x02"
	"\x01\x12\x02\x01\x11\x02\x01\x10\x02\x01\x17",
	.length = 187
    }, recv = { .data = NULL, .length = 0 };

    ret = krb5_sendto_ctx_alloc(context, &ctx);
    if (ret)
	krb5_err(context, 1, ret, "krb5_sendto_ctx_alloc");

    if (use_large_flag)
	krb5_sendto_ctx_add_flags(ctx, KRB5_KRBHST_FLAGS_LARGE_MSG);

    ret = krb5_sendto_context(context, ctx, &send, realm_string, &recv);
    if (ret)
	krb5_err(context, 1, ret, "krb5_sendto_context");

    krb5_sendto_ctx_free(context, ctx);

    krb5_free_context(context);

    return 0;
}
