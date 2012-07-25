/*
 * Copyright (c) 1997-2004 Kungliga Tekniska Högskolan
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

#include "kadmin_locl.h"
#include <gssapi.h>
#include <gssapi_krb5.h>
#include <gssapi_spi.h>



#ifdef __APPLE__
#include <sandbox.h>

int sandbox_flag = 1;
#endif

krb5_error_code _gsskrb5_init (krb5_context *);

static char *check_library  = NULL;
static char *check_function = NULL;
static getarg_strings policy_libraries = { 0, NULL };
static char *config_file;
static char sHDB[] = "HDB:";
static char *keytab_str = sHDB;
static int help_flag;
static int version_flag;
static int debug_flag;
static char *port_str;
char *realm;

static struct getargs args[] = {
    {
	"config-file",	'c',	arg_string,	&config_file,
	"location of config file",	"file"
    },
    {
	"keytab",	0,	arg_string, &keytab_str,
	"what keytab to use", "keytab"
    },
    {	"realm",	'r',	arg_string,   &realm,
	"realm to use", "realm"
    },
#ifdef HAVE_DLOPEN
    { "check-library", 0, arg_string, &check_library,
      "library to load password check function from", "library" },
    { "check-function", 0, arg_string, &check_function,
      "password check function to load", "function" },
    { "policy-libraries", 0, arg_strings, &policy_libraries,
      "password check function to load", "function" },
#endif
    {	"debug",	'd',	arg_flag,   &debug_flag,
	"enable debugging", NULL
    },
    {	"ports",	'p',	arg_string, &port_str,
	"ports to listen to", "port" },
#ifdef __APPLE__
    {	"sandbox",	0, 	arg_negative_flag, &sandbox_flag,
	"use sandbox or not"
    },
#endif /* __APPLE__ */
    {	"help",		'h',	arg_flag,   &help_flag },
    {	"version",	'v',	arg_flag,   &version_flag }
};

static int num_args = sizeof(args) / sizeof(args[0]);

krb5_context context;

static void
usage(int ret)
{
    arg_printusage (args, num_args, NULL, "");
    exit (ret);
}

static void
setup_context(krb5_context ctx)
{
    krb5_log_facility *logfacility;
    krb5_error_code ret;
    char **files;

    if (config_file == NULL) {
	asprintf(&config_file, "%s/kdc.conf", hdb_db_dir(ctx));
	if (config_file == NULL)
	    errx(1, "out of memory");
    }

    ret = krb5_prepend_config_files_default(config_file, &files);
    if (ret)
	krb5_err(ctx, 1, ret, "getting configuration files");

    ret = krb5_set_config_files(ctx, files);
    krb5_free_config_files(files);
    if(ret)
	krb5_err(ctx, 1, ret, "reading configuration files");

    ret = krb5_openlog(ctx, "kadmind", &logfacility);
    if (ret)
	krb5_err(ctx, 1, ret, "krb5_openlog");
    ret = krb5_set_warn_dest(ctx, logfacility);
    if (ret)
	krb5_err(ctx, 1, ret, "krb5_set_warn_dest");

    ret = krb5_kt_register(ctx, &hdb_kt_ops);
    if(ret)
	krb5_err(ctx, 1, ret, "krb5_kt_register");
}

int
main(int argc, char **argv)
{
    krb5_error_code ret;
    int optidx = 0;
    int i;
    krb5_keytab keytab;
    krb5_socket_t sfd = rk_INVALID_SOCKET;

    setprogname(argv[0]);

    ret = krb5_init_context(&context);
    if (ret)
	errx (1, "krb5_init_context failed: %d", ret);

    if (getarg(args, num_args, argc, argv, &optidx)) {
	warnx("error at argument `%s'", argv[optidx]);
	usage(1);
    }

    if (help_flag)
	usage (0);

    if (version_flag) {
	print_version(NULL);
	exit(0);
    }

    setup_context(context);

    /*
     * Now, do the same for the gssapi thread we are going to be running in
     */
    {
	krb5_context gssctx;
	ret = _gsskrb5_init(&gssctx);
	if (ret)
	    errx(1, "failed to setup gssapi context");
	setup_context(gssctx);
	krb5_gss_register_acceptor_identity("HDB:");
    }

    ret = krb5_kt_resolve(context, keytab_str, &keytab);
    if(ret)
	krb5_err(context, 1, ret, "krb5_kt_resolve");

    kadm5_setup_passwd_quality_check (context, check_library, check_function);

    for (i = 0; i < policy_libraries.num_strings; i++) {
	ret = kadm5_add_passwd_quality_verifier(context,
						policy_libraries.strings[i]);
	if (ret)
	    krb5_err(context, 1, ret, "kadm5_add_passwd_quality_verifier");
    }
    ret = kadm5_add_passwd_quality_verifier(context, NULL);
    if (ret)
	krb5_err(context, 1, ret, "kadm5_add_passwd_quality_verifier");

#ifdef ___APPLE__
    if (sandbox_flag) {
	char *errorstring;
	ret = sandbox_init("kadmind", SANDBOX_NAMED, &errorstring);
	if (ret)
	    errx(1, "sandbox_init failed: %d: %s", ret, errorstring);
    }
#endif

    if(debug_flag) {
	int debug_port;

	if(port_str == NULL)
	    debug_port = krb5_getportbyname (context, "kerberos-adm",
					     "tcp", 749);
	else
	    debug_port = htons(atoi(port_str));
	mini_inetd(debug_port, &sfd);
    } else {
#ifdef _WIN32
	pidfile(NULL);
	start_server(context, port_str);
#else
	struct sockaddr_storage __ss;
	struct sockaddr *sa = (struct sockaddr *)&__ss;
	socklen_t sa_size = sizeof(__ss);

	/*
	 * Check if we are running inside inetd or not, if not, start
	 * our own server.
	 */

	if(roken_getsockname(STDIN_FILENO, sa, &sa_size) < 0 &&
	   rk_SOCK_ERRNO == ENOTSOCK) {
	    pidfile(NULL);
	    start_server(context, port_str);
	}
#endif /* _WIN32 */
	sfd = STDIN_FILENO;
    }

    if(realm)
	krb5_set_default_realm(context, realm); /* XXX */

    kadmind_loop(context, keytab, sfd);

    return 0;
}
