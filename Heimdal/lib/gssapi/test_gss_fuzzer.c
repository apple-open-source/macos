/*
 * Copyright (c) 2006 - 2008 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2009 Apple Inc. All rights reserved.
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

#include "krb5/gsskrb5_locl.h"
#include <err.h>
#include <getarg.h>
#include <gssapi.h>
#include <gssapi_krb5.h>
#include <gssapi_spnego.h>
#include <gssapi_ntlm.h>
#include <gssapi_spi.h>
#include "test_common.h"
#include "fuzzer.h"

static char *type_string;
static char *mech_string;
static char *cred_string;
static char *client_name;

static int max_loops = 20;
static int fuzzer_loops = 1000000;
static int fuzzer_failure = 0;
static int fuzzer_slipped_by = 0;
static int version_flag = 0;
static char *dumpfile_string = NULL;
static int verbose_flag = 0;
static int help_flag	= 0;

enum return_value {
    NEXT_ITERATION = 0,
    NEXT_TARGET = 1,
    NEXT_FUZZER = 2
};

static heim_fuzz_type_t fuzzers[] = {
    HEIM_FUZZ_RANDOM,
    HEIM_FUZZ_BITFLIP,
    HEIM_FUZZ_BYTEFLIP,
    HEIM_FUZZ_SHORTFLIP,
    HEIM_FUZZ_WORDFLIP,
    HEIM_FUZZ_INTERESTING8,
    HEIM_FUZZ_INTERESTING16,
    HEIM_FUZZ_INTERESTING32,
#if 0
    HEIM_FUZZ_ASN1,
#endif
    NULL
};

static enum return_value
loop(heim_fuzz_type_t fuzzer, gss_OID mechoid, gss_const_OID nameoid, const char *target, gss_cred_id_t init_cred,
     unsigned long target_loop,
     unsigned long iteration_count,
     void **fuzzer_context)
{
    int server_done = 0, client_done = 0;
    int num_loops = 0;
    unsigned long current_target = 0;
    OM_uint32 maj_stat, min_stat;
    gss_name_t gss_target_name = GSS_C_NO_NAME;
    gss_buffer_desc input_token, output_token;
    OM_uint32 flags = 0, ret_cflags, ret_sflags;
    gss_OID actual_mech_client;
    gss_OID actual_mech_server;
    gss_ctx_id_t cctx = NULL, sctx = NULL;
    int fuzzer_changed = 0;
    enum return_value return_value = NEXT_ITERATION;

    flags |= GSS_C_INTEG_FLAG;
    flags |= GSS_C_CONF_FLAG;
    flags |= GSS_C_MUTUAL_FLAG;

    input_token.value = rk_UNCONST(target);
    input_token.length = strlen(target);

    maj_stat = gss_import_name(&min_stat,
			       &input_token,
			       nameoid,
			       &gss_target_name);
    if (GSS_ERROR(maj_stat))
	err(1, "import name creds failed with: %d", maj_stat);

    input_token.length = 0;
    input_token.value = NULL;

    output_token.length = 0;
    output_token.value = NULL;

    while (!server_done || !client_done) {

	num_loops++;
	
	if (max_loops && max_loops < num_loops) {
	    errx(1, "num loops %d was larger then max loops %d",
		 num_loops, max_loops);
	}

	if (target_loop == current_target) {
	    uint8_t *data = (uint8_t *)input_token.value;

	    fuzzer_changed = 1;

	    if (heim_fuzzer(fuzzer, fuzzer_context, iteration_count,  data, input_token.length)) {
		heim_fuzzer_free(fuzzer, *fuzzer_context);
		*fuzzer_context = NULL;
		return_value = NEXT_TARGET;
		goto out;
	    }

	    if (dumpfile_string)
		rk_dumpdata(dumpfile_string, data, input_token.length);
	}
	current_target++;

	maj_stat = gss_init_sec_context(&min_stat,
					init_cred,
					&cctx,
					gss_target_name,
					mechoid,
					flags,
					0,
					GSS_C_NO_CHANNEL_BINDINGS,
					&input_token,
					&actual_mech_client,
					&output_token,
					&ret_cflags,
					NULL);
	if (dumpfile_string)
	    unlink(dumpfile_string);

	if (GSS_ERROR(maj_stat)) {
	    if (verbose_flag)
		warnx("init_sec_context: %s",
		      gssapi_err(maj_stat, min_stat, mechoid));
	    fuzzer_failure++;
	    goto out;
	}

	if (maj_stat & GSS_S_CONTINUE_NEEDED)
	    ;
	else
	    client_done = 1;


	if (client_done && server_done)
	    break;

	if (input_token.length != 0)
	    gss_release_buffer(&min_stat, &input_token);

	if (target_loop == current_target) {
	    uint8_t *data = (uint8_t *)output_token.value;

	    fuzzer_changed = 1;

	    if (heim_fuzzer(fuzzer, fuzzer_context, iteration_count,  data, output_token.length)) {
		heim_fuzzer_free(fuzzer, *fuzzer_context);
		*fuzzer_context = NULL;
		return_value = NEXT_TARGET;
		goto out;
	    }

	    if (dumpfile_string)
		rk_dumpdata(dumpfile_string, data, input_token.length);
	}
	current_target++;

	maj_stat = gss_accept_sec_context(&min_stat,
					  &sctx,
					  GSS_C_NO_CREDENTIAL,
					  &output_token,
					  GSS_C_NO_CHANNEL_BINDINGS,
					  NULL,
					  &actual_mech_server,
					  &input_token,
					  &ret_sflags,
					  NULL,
					  NULL);
	if (dumpfile_string)
	    unlink(dumpfile_string);

	if (GSS_ERROR(maj_stat)) {
	    if (verbose_flag)
		warnx("accept_sec_context: %s",
		      gssapi_err(maj_stat, min_stat, actual_mech_server));
	    fuzzer_failure++;
	    goto out;
	}


	if (output_token.length != 0)
	    gss_release_buffer(&min_stat, &output_token);

	if (maj_stat & GSS_S_CONTINUE_NEEDED)
	    ;
	else
	    server_done = 1;

    }

    if (client_done && server_done) {
	if (!fuzzer_changed)
	    return_value = NEXT_FUZZER;
	else
	    fuzzer_slipped_by++;
    }

 out:
    gss_delete_sec_context(&min_stat, &cctx, NULL);
    gss_delete_sec_context(&min_stat, &sctx, NULL);

    if (output_token.length != 0)
	gss_release_buffer(&min_stat, &output_token);
    if (input_token.length != 0)
	gss_release_buffer(&min_stat, &input_token);
    gss_release_name(&min_stat, &gss_target_name);

    return return_value;
}


/*
 *
 */

static struct getargs args[] = {
    {"name-type",0,	arg_string, &type_string,  "type of name", NULL },
    {"mech-type",0,	arg_string, &mech_string,  "type of mech", NULL },

    {"cred-type",0,	arg_string,     &cred_string,  "type of cred", NULL },
    {"client-name", 0,  arg_string,     &client_name, "client name", NULL },

    {"max-loops",	0, arg_integer,	&max_loops, "times", NULL },
    {"fuzzer-loops",	0, arg_integer,	&fuzzer_loops, "times", NULL },
    {"dump-data",	0, arg_string,	&dumpfile_string, "file", NULL },
    {"version",	0,	arg_flag,	&version_flag, "print version", NULL },
    {"verbose",	'v',	arg_flag,	&verbose_flag, "verbose", NULL },
    {"help",	0,	arg_flag,	&help_flag,  NULL, NULL }
};

static void
usage (int ret)
{
    arg_printusage (args, sizeof(args)/sizeof(*args),
		    NULL, "service@host");
    exit (ret);
}

int
main(int argc, char **argv)
{
    int optidx = 0;
    OM_uint32 min_stat, maj_stat;
    gss_const_OID nameoid, mechoid;
    gss_cred_id_t client_cred = GSS_C_NO_CREDENTIAL;
    gss_name_t cname = GSS_C_NO_NAME;
    unsigned long current_target, current_iteration, current_fuzzer;
    unsigned long n, divN;
    int end_of_fuzzer = 0;
    int ttyp = isatty(STDIN_FILENO);
    void *fuzzer_context = NULL;

    setprogname(argv[0]);

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

    if (argc != 1)
	usage(1);
    
    if (fuzzer_loops < 0)
	errx(1, "invalid number of loops");

    if (type_string == NULL)
	nameoid = GSS_C_NT_HOSTBASED_SERVICE;
    else if (strcmp(type_string, "hostbased-service") == 0)
	nameoid = GSS_C_NT_HOSTBASED_SERVICE;
    else if (strcmp(type_string, "krb5-principal-name") == 0)
	nameoid = GSS_KRB5_NT_PRINCIPAL_NAME;
    else if (strcmp(type_string, "krb5-principal-name-referral") == 0)
	nameoid = GSS_KRB5_NT_PRINCIPAL_NAME_REFERRAL;
    else
	errx(1, "%s not suppported", type_string);

    if (mech_string == NULL)
	mechoid = GSS_KRB5_MECHANISM;
    else {
	mechoid = gss_name_to_oid(mech_string);
	if (mechoid == GSS_C_NO_OID)
	    errx(1, "failed to find mech oid: %s", mech_string);
    }

    if (client_name) {
	gss_buffer_desc cn;
	gss_const_OID credoid = GSS_C_NO_OID;
	gss_OID_set mechs = GSS_C_NULL_OID_SET;

	if (cred_string) {
	    credoid = gss_name_to_oid(cred_string);
	    if (credoid == GSS_C_NO_OID)
		errx(1, "failed to find cred oid: %s", cred_string);
	}

	cn.value = client_name;
	cn.length = strlen(client_name);

	maj_stat = gss_import_name(&min_stat, &cn, GSS_C_NT_USER_NAME, &cname);
	if (maj_stat)
	    errx(1, "gss_import_name: %s",
		 gssapi_err(maj_stat, min_stat, GSS_C_NO_OID));



	if (credoid != GSS_C_NO_OID) {
	    maj_stat = gss_create_empty_oid_set(&min_stat, &mechs);
	    if (maj_stat != GSS_S_COMPLETE)
		errx(1, "gss_create_empty_oid_set: %s",
		     gssapi_err(maj_stat, min_stat, GSS_C_NO_OID));
		
	    maj_stat = gss_add_oid_set_member(&min_stat, credoid, &mechs);
	    if (maj_stat != GSS_S_COMPLETE)
		errx(1, "gss_add_oid_set_member: %s",
		     gssapi_err(maj_stat, min_stat, GSS_C_NO_OID));
	}

	maj_stat = gss_acquire_cred(&min_stat, cname, 0, mechs,
				    GSS_C_INITIATE, &client_cred, NULL, NULL);
	if (GSS_ERROR(maj_stat))
	    errx(1, "gss_import_name: %s",
		 gssapi_err(maj_stat, min_stat, GSS_C_NO_OID));
	
	if (mechs != GSS_C_NULL_OID_SET)
	    gss_release_oid_set(&min_stat, &mechs);

	gss_release_name(&min_stat, &cname);

    } else {
	maj_stat = gss_acquire_cred(&min_stat,
				    cname,
				    GSS_C_INDEFINITE,
				    GSS_C_NO_OID_SET,
				    GSS_C_INITIATE,
				    &client_cred,
				    NULL,
				    NULL);
	if (GSS_ERROR(maj_stat))
	    errx(1, "gss_acquire_cred: %s",
		 gssapi_err(maj_stat, min_stat, GSS_C_NO_OID));
    }
    
    /*
     *
     */

    divN = fuzzer_loops / 10;
    if (divN == 0)
	divN = 1;

    current_fuzzer = current_target = current_iteration = 0;

    for (n = 1; n <= (unsigned long)fuzzer_loops && !end_of_fuzzer; n++) {
	enum return_value return_value;

	return_value = loop(fuzzers[current_fuzzer], rk_UNCONST(mechoid), nameoid,
			    argv[0], client_cred, current_target, current_iteration, &fuzzer_context);
	switch (return_value) {
	case NEXT_ITERATION:
	    current_iteration++;
	    break;

	case NEXT_TARGET:
	    if (fuzzer_context != NULL)
		errx(1, "fuzzer context not NULL at next target state?");
	    if (ttyp)
		printf("\b");
	    printf("fuzzer %s targets next step (%lu) at: %lu\n",
		   heim_fuzzer_name(fuzzers[current_fuzzer]),
		   current_target + 1,
		   (unsigned long)current_iteration);
	    current_iteration = 0;
	    current_target++;
	    break;

	case NEXT_FUZZER:
	    if (ttyp)
		printf("\b");
	    printf("fuzzer %s done at: %lu\n",
		   heim_fuzzer_name(fuzzers[current_fuzzer]), n);
	    current_target = 0;
	    current_iteration = 0;

	    current_fuzzer++;
	    if (fuzzers[current_fuzzer] == NULL)
		end_of_fuzzer = 1;
	    break;
	}

	if ((n % divN) == 0) {
	    if (ttyp)
		printf("\b");
	    printf(" try %lu\n", (unsigned long)n);
	} else if (ttyp && (n & 0xff) == 0) {
	    printf("\b%d", (int)((n >> 8) % 10));
	}
    }

    if (ttyp)
	printf("\b");

    if (!end_of_fuzzer)
	printf("fuzzer: %s step: %lu iteration: %lu\n",
	       heim_fuzzer_name(fuzzers[current_fuzzer]),
	       current_target,
	       current_iteration);

    printf("fuzzer: tries: %lu failure: %d "
	   "modified-but-non-failure: %d end-of-fuzzer: %d\n",
	   (unsigned long)n,
	   fuzzer_failure,
	   fuzzer_slipped_by,
	   end_of_fuzzer);

    return 0;
}
