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


#include "kuser_locl.h"
#include <config.h>
#include "kcc-commands.h"

static const struct { 
    char *name;
    int type;
} types[] = {
    { "kdc", KRB5_KRBHST_KDC },
    { "kadmin", KRB5_KRBHST_ADMIN },
    { "changepw", KRB5_KRBHST_CHANGEPW },
    { "krb524", KRB5_KRBHST_KRB524 },
    { "kkdcp", KRB5_KRBHST_KKDCP }
};

int
kdc(struct kdc_options *opt, int argc, char **argv)
{
    krb5_krbhst_handle handle;
    int type = KRB5_KRBHST_KDC;
    char host[MAXHOSTNAMELEN];
    krb5_error_code ret;
    int first_realm = 1;
    size_t n;
    int i;
    
    if (argc == 0) {
	printf("give at least on realm\n");
	return 1;
    }

    if (opt->type_string) {

	for (n = 0; n < sizeof(types)/sizeof(types[0]); n++) {
	    if (strcasecmp(types[n].name, opt->type_string) == 0)
		type = types[n].type;
	}
	if (n == sizeof(types)/sizeof(types[0])) {
	    printf("unknown type: %s\nAvailaile types are: \n", opt->type_string);
	    for (n = 0; n < sizeof(types)/sizeof(types[0]); n++)
		printf("%s ", types[n].name);
	    printf("\n");
	    return 1;
	}
    }

    if (opt->json_flag)
	printf("{");

    for (i = 0; i < argc; i++) {
	const char *realm = argv[i];

	ret = krb5_krbhst_init(kcc_context, realm, type, &handle);
	if (ret) {
	    krb5_warn(kcc_context, ret, "krb5_krbhst_init");
	    return 1;
	}

	if (opt->json_flag) {
	    int first = 1;
	    printf("%s\n\t\"%s\" = [ ", first_realm ? "" : ",", realm);
	    first_realm = 0;

	    while(krb5_krbhst_next_as_string(kcc_context, handle, host, sizeof(host)) == 0) {
		printf("%s\n\t\t\"%s\"", first ? "" : ",", host);
		first = 0;
	    }

	    printf("\n\t]");
	} else {
	    printf("[realms]\n");
	    printf("\t%s = {\n", realm);

	    while(krb5_krbhst_next_as_string(kcc_context, handle, host, sizeof(host)) == 0)
		printf("\t\tkdc = %s\n", host);
	    
	    printf("\t}\n");
	}

	krb5_krbhst_free(kcc_context, handle);
    }

    if (opt->json_flag)
	printf("\n}\n");

    return 0;
}
