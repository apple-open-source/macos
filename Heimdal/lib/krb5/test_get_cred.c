/*
 * Copyright (c) 2008 - 2011 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2011 Apple Inc. All rights reserved.
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

#include "krb5_locl.h"
#include <err.h>
#include <getarg.h>

static char *ccache_string = NULL;
static int verbose_flag = 0;
static int version_flag = 0;
static int help_flag	= 0;

static struct getargs args[] = {
    { "cache",			'c', arg_string, &ccache_string,
	"credentials cache to use", "cache" },
    {"verbose",	0,	arg_flag,	&verbose_flag,
	"verbose debugging", NULL },
    {"version",	0,	arg_flag,	&version_flag,
	"print version", NULL },
    {"help",	0,	arg_flag,	&help_flag,
	NULL, NULL }
};

static void
usage (int ret)
{
    arg_printusage (args,
		    sizeof(args)/sizeof(*args),
		    NULL,
		    "principal ...");
    exit (ret);
}

int
main(int argc, char **argv)
{
    krb5_context context;
    krb5_error_code ret;
    krb5_ccache id;
    int i, optidx = 0;
    
    setprogname (argv[0]);
    
    if(getarg(args, sizeof(args) / sizeof(args[0]), argc, argv, &optidx))
	usage(1);
    
    if (help_flag)
	usage (0);
    
    if(version_flag){
	print_version(NULL);
	exit(0);
    }
    
    argc -= optidx;
    argv += optidx;
    
    if (argc < 1)
	usage(1);
    
    ret = krb5_init_context(&context);
    if (ret)
	errx(1, "krb5_init_context");
    
    if (verbose_flag) {
	krb5_log_facility *log;
	krb5_initlog(context, "libkrb5", &log);
	krb5_addlog_dest(context, log, "0-/STDERR");
	krb5_set_debug_dest(context, log);
    }
    
    if (ccache_string) {
	ret = krb5_cc_resolve(context, ccache_string, &id);
    } else {
	ret = krb5_cc_default(context, &id);
    }
    if (ret)
	krb5_err(context, 1, ret, "krb5_cc_%s",
		 ccache_string ? "resolve" : "default");
    
    for (i = 0; i < argc; i++) {
	krb5_tkt_creds_context ctx = NULL;
	krb5_creds incred, *outcred = NULL;
	krb5_data indata, outdata;
	
	memset(&incred, 0, sizeof(incred));
	krb5_data_zero(&indata);
	krb5_data_zero(&outdata);
	
	/*
	 *
	 */
	
	printf("%s\n", argv[i]);
	
	ret = krb5_cc_get_principal(context, id, &incred.client);
	if (ret)
	    krb5_err(context, 1, ret, "krb5_cc_get_principal");
	
	ret = krb5_parse_name(context, argv[i], &incred.server);
	if (ret)
	    krb5_err(context, 1, ret, "krb5_parse_name");
	
	ret = krb5_tkt_creds_init(context, id, &incred, 0, &ctx);
	if (ret)
	    krb5_err(context, 1, ret, "krb5_tkt_creds_init");
	
	krb5_free_principal(context, incred.server);
	krb5_free_principal(context, incred.client);
	
	/*
	 *
	 */
	
	while (1) {
	    krb5_realm realm = NULL;
	    unsigned int flags = 0;
	    
	    ret = krb5_tkt_creds_step(context, ctx, &indata, &outdata,
				      &realm, &flags);
	    krb5_data_free(&indata);
	    if (ret)
		krb5_err(context, 1, ret, "krb5_tkt_step_step");
	    
	    if ((flags & KRB5_TKT_STATE_CONTINUE) == 0) {
		heim_assert(outdata.length == 0, "data to send to KDC");
		break;
	    }
	    heim_assert(outdata.length != 0, "no data to send to KDC!");
	    
	    ret = krb5_sendto_context(context, NULL,
				      &outdata, realm, &indata);
	    if (ret)
		krb5_err(context, 1, ret, "krb5_sendto_context");
	    krb5_data_free(&outdata);
	}
	
	ret = krb5_tkt_creds_get_creds(context, ctx, &outcred);
	if (ret)
	    krb5_err(context, 1, ret, "krb5_tkt_creds_get_creds");
	
	krb5_tkt_creds_free(context, ctx);
    }
    
    krb5_free_context(context);
    
    return 0;
}
