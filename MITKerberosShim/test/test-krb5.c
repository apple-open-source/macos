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

#include <err.h>
#include <string.h>
#include <Kerberos/krb5.h>
#include "test_collection.h"

int main(int argc, char **argv)
{
	char *realm = "ADS.APPLE.COM";
	const char *ptr = NULL;
	char *nn = NULL;
	krb5_error_code ret = 0;

	krb5_context ctx = NULL;
	krb5_principal princ = NULL, xprinc = NULL;
	krb5_address **addy = NULL;
	krb5_authdata **auth = NULL;

	krb5_ccache ccache = NULL;
	krb5_creds creds;
	krb5_get_init_creds_opt options;
	krb5_cc_cursor cursor;

	test_collection_t *tc = NULL;
	tc = tests_init_and_start("test-krb5");
	tests_set_flags(tc, TC_FLAG_EXIT_ON_FAILURE);
	tests_set_total_count_hint(tc, 16);

	krb5_get_init_creds_opt_init(&options);

	ret = krb5_init_context(&ctx);
	test_evaluate(tc, "krb5_init_context", ret);
	krb5_free_context(ctx);

	ret = krb5_init_secure_context(&ctx);
	test_evaluate(tc, "krb5_init_secure_context", ret);
	ret = krb5_parse_name(ctx, realm, &xprinc);
	test_evaluate(tc, "krb5_parse_name", ret);
	ret = krb5_unparse_name(ctx, xprinc, &nn);
	test_evaluate(tc, "krb5_unparse_name", ret);

	ptr = krb5_cc_default_name(ctx);
	test_evaluate(tc, "krb5_cc_default_name", ptr == NULL);
	ret = krb5_build_principal(ctx, &princ, sizeof(ptr), ptr,
			"client-comp1", "client-comp2", NULL);
	test_evaluate(tc, "krb5_build_principal", ret);

	ret = krb5_cc_resolve(ctx, realm, &ccache);
	test_evaluate(tc, "krb5_cc_resolve", ret);
	ret = krb5_cc_close(ctx, ccache);
	test_evaluate(tc, "krb5_cc_close", ret);
	
	ret = krb5_cc_resolve(ctx, realm, &ccache);
	test_evaluate(tc, "krb5_cc_resolve", ret);
	ret = krb5_cc_initialize(ctx, ccache, princ);
	test_evaluate(tc, "krb5_cc_initialize", ret);

	ptr = krb5_cc_get_name(ctx, ccache);
	test_evaluate(tc, "krb5_cc_get_name", ret);
	ret = krb5_cc_get_principal(ctx, ccache, &princ);
	test_evaluate(tc, "krb5_cc_get_principal1", ret);
	ret = krb5_cc_get_principal(ctx, ccache, &xprinc);
	test_evaluate(tc, "krb5_cc_get_principal2", ret);

	ret = krb5_cc_start_seq_get(ctx, ccache, &cursor);
	test_evaluate(tc, "krb5_cc_start_seq_get", ret);
	while((ret = krb5_cc_next_cred(ctx, ccache, &cursor, &creds)) == 0)
	{
		ret = krb5_cc_store_cred(ctx, ccache, &creds);
		test_evaluate(tc, "krb5_cc_store_cred", ret);
		krb5_free_cred_contents(ctx, &creds);
	}
	ret = krb5_cc_end_seq_get(ctx, ccache, &cursor);
	test_evaluate(tc, "krb5_cc_end_seq_get", ret);

	ret = krb5_cc_destroy(ctx, ccache);
	test_evaluate(tc, "krb5_cc_destroy", ret);

	addy = malloc(sizeof(krb5_address*)*2);
	addy[0] = malloc(sizeof(krb5_address));
	addy[0]->contents = malloc(sizeof(krb5_octet));
	addy[1] = NULL;
	//test_evaluate(tc, "krb5_os_localaddr", ret);
	krb5_free_addresses(ctx, addy);

	auth = calloc(1, sizeof(krb5_authdata));
	krb5_free_authdata(ctx, auth);
	krb5_free_principal(ctx, xprinc);

	krb5_free_context(ctx);

	// test error_message
	test_evaluate(tc, "error_message-asn1", strcmp(error_message(1859794432L), "ASN.1 failed call to system time library"));
	test_evaluate(tc, "error_message-gk5", strcmp(error_message(35224064), "No @ in SERVICE-NAME name string"));
	test_evaluate(tc, "error_message-wind", strcmp(error_message(-969269760), "No error"));
	test_evaluate(tc, "error_message-krb5", strcmp(error_message(-1765328384L), "No error"));
	test_evaluate(tc, "error_message-krb", strcmp(error_message(39525376), "Kerberos 4 successful"));
	test_evaluate(tc, "error_message-k524", strcmp(error_message(-1750206208), "wrong keytype in ticket"));
	test_evaluate(tc, "error_message-heim", strcmp(error_message(-1980176640), "Error parsing log destination"));
	test_evaluate(tc, "error_message-hx", strcmp(error_message(569856), "ASN.1 failed call to system time library"));

	return tests_stop_and_free(tc);
}
