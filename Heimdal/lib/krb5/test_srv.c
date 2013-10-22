/*
 * Copyright (c) 2013 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2013 Apple Inc. All rights reserved.
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

static int version_flag = 0;
static int help_flag	= 0;

static struct getargs args[] = {
    {"version",		0,	arg_flag,	&version_flag,
     "print version", NULL },
    {"help",		0,	arg_flag,	&help_flag,
     NULL, NULL }
};

static void
usage(int ret)
{
    arg_printusage(args,
		   sizeof(args)/sizeof(*args),
		   NULL,
		   "");
    exit(ret);
}

#define NUM_ITER 100000
#define MAX_HOSTS 1000

int
main(int argc, char **argv)
{
    struct _krb5_srv_query_ctx ctx;
    krb5_context context;
    krb5_error_code ret;
    int optidx = 0;
    size_t n, m;

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

    memset(&ctx, 0, sizeof(ctx));

    ctx.context = context;
    ctx.domain = rk_UNCONST("domain");
    ctx.array = calloc(MAX_HOSTS, sizeof(ctx.array[0]));
    if (ctx.array == NULL)
	errx(1, "malloc: outo of memory");

#ifdef __APPLE__
    for (n = 0; n < NUM_ITER; n++) {

	if ((n % (NUM_ITER / 10)) == 0) {
	    printf("%d ", (int)n);
	    fflush(stdout);
	}

	if (n < 10) {
	    ctx.len = n;
	} else {
	    ctx.len = (rk_random() % (MAX_HOSTS - 1)) + 1;
	}

	for (m = 0; m < ctx.len; m++) {
	    ctx.array[m].hi = NULL;
	    ctx.array[m].priority = (5 % (rk_random() + 1));
	    ctx.array[m].weight = (4 % (rk_random() + 1));
	}

	_krb5_state_srv_sort(&ctx);
    }
#endif
    printf("\n");


    krb5_free_context(context);

    return 0;
}
