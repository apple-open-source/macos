/*
 * Copyright (c) 2008-2010 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2008-2010 Apple Inc. All rights reserved.
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

#include <stdio.h>
#include <err.h>
#include <string.h>
#include <Kerberos/krb5.h>
#include "test_collection.h"

int main(int argc, char **argv)
{
	krb5_error_code ret = 0;

	krb5_context context = NULL;
	krb5_principal princ = NULL;
	krb5_get_init_creds_opt opt;
	krb5_creds cred;
	int result_code;
	krb5_data result_code_string;
	krb5_data result_string;

	test_collection_t *tc = NULL;

	memset(&cred, 0, sizeof(cred));
	memset(&result_code_string, 0, sizeof(result_code_string));
	memset(&result_string, 0, sizeof(result_string));

	tc = tests_init_and_start("test-krb5");
	tests_set_flags(tc, TC_FLAG_EXIT_ON_FAILURE);
	tests_set_total_count_hint(tc, 5);

	ret = krb5_init_context(&context);
	test_evaluate(tc, "krb5_init_context", ret);

	ret = krb5_parse_name(context, argv[1], &princ);
	test_evaluate(tc, "krb5_parse_name", ret);

	krb5_get_init_creds_opt_init(&opt);

	ret = krb5_get_init_creds_password (context,
					    &cred,
					    princ,
					    argv[2],
					    krb5_prompter_posix,
					    NULL,
					    0,
					    "kadmin/changepw",
					    &opt);
	test_evaluate(tc, "krb5_get_init_creds_password", ret);

	ret = krb5_set_password(context,
				&cred,
				argv[3],
				NULL,
				&result_code,
				&result_code_string,
				&result_string);
	test_evaluate(tc, "krb5_set_password", ret);

	printf("result code: %d result_code_string %.*s result_string: %*.s\n",
	       result_code,
	       (int)result_code_string.length,
	       (char *)result_code_string.data,
	       (int)result_string.length,
	       (char *)result_string.data);

	test_evaluate(tc, "krb5_set_password result code", result_code);

	krb5_free_context(context);

	return tests_stop_and_free(tc);
}
