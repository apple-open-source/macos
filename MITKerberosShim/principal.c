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

#include "heim.h"
#include <string.h>

static void
map_mit_principal(struct comb_principal *p)
{
    unsigned long i;

    p->mit.magic = MIT_KV5M_PRINCIPAL;
    p->mit.type = p->heim->name.name_type;
    p->mit.realm.magic = MIT_KV5M_DATA;
    p->mit.realm.data = p->heim->realm;
    p->mit.realm.length = strlen(p->heim->realm);
    p->mit.data = calloc(p->heim->name.name_string.len, sizeof(*p->mit.data));
    for (i = 0; i < p->heim->name.name_string.len; i++) {
	p->mit.data[i].magic = MIT_KV5M_DATA;
	p->mit.data[i].data = p->heim->name.name_string.val[i];
	p->mit.data[i].length = strlen(p->heim->name.name_string.val[i]);
    }
    p->mit.length = p->heim->name.name_string.len;
}

mit_krb5_principal
mshim_hprinc2mprinc(krb5_context context, krb5_principal princ)
{
    struct comb_principal *p;
    p = calloc(1, sizeof(*p));
    heim_krb5_copy_principal(context, princ, &p->heim);
    map_mit_principal(p);
    return (mit_krb5_principal)p;
}

mit_krb5_error_code KRB5_CALLCONV
krb5_parse_name(mit_krb5_context context, const char *str, mit_krb5_principal *principal)
{
    struct comb_principal *p;
    krb5_error_code ret;

    LOG_ENTRY();

    p = calloc(1, sizeof(*p));
    ret = heim_krb5_parse_name((krb5_context)context, str, &p->heim);
    if (ret) {
	free(p);
	return ret;
    }
    map_mit_principal(p);
    *principal = (mit_krb5_principal)p;
    return 0;
}

mit_krb5_error_code KRB5_CALLCONV_C
krb5_build_principal_ext(mit_krb5_context context, mit_krb5_principal *principal, unsigned int rlen, const char *realm, ...)
{
    struct comb_principal *p;
    krb5_error_code ret;
    va_list ap;

    LOG_ENTRY();

    va_start(ap, realm);
    p = calloc(1, sizeof(*p));
    ret = heim_krb5_build_principal_va_ext((krb5_context)context, &p->heim, rlen, realm, ap);
    va_end(ap);
    if (ret) {
	free(p);
	return ret;
    }
    map_mit_principal(p);
    *principal = (mit_krb5_principal)p;
    return ret;
}

mit_krb5_error_code KRB5_CALLCONV_C
krb5_build_principal(mit_krb5_context context, mit_krb5_principal *principal, unsigned int rlen, const char *realm, ...)
{
    struct comb_principal *p;
    krb5_error_code ret;
    va_list ap;

    LOG_ENTRY();

    va_start(ap, realm);
    p = calloc(1, sizeof(*p));
    ret = heim_krb5_build_principal_va((krb5_context)context, &p->heim, rlen, realm, ap);
    va_end(ap);
    if (ret) {
	free(p);
	return ret;
    }
    map_mit_principal(p);
    *principal = (mit_krb5_principal)p;
    return ret;
}

mit_krb5_error_code KRB5_CALLCONV
krb5_unparse_name(mit_krb5_context context, mit_krb5_const_principal principal, char **str)
{
    struct comb_principal *p = (struct comb_principal *)principal;
    LOG_ENTRY();
    return heim_krb5_unparse_name((krb5_context)context, p->heim, str);
}

void KRB5_CALLCONV
krb5_free_unparsed_name(mit_krb5_context context, char *str)
{
    LOG_ENTRY();
    heim_krb5_xfree(str);
}

mit_krb5_error_code KRB5_CALLCONV
krb5_copy_principal(mit_krb5_context context,
		    mit_krb5_const_principal from,
		    mit_krb5_principal *to)
{
    struct comb_principal *p = (struct comb_principal *)from;
    LOG_ENTRY();
    *to = mshim_hprinc2mprinc(HC(context), p->heim);
    return 0;
}

void KRB5_CALLCONV
krb5_free_principal(mit_krb5_context context, mit_krb5_principal principal)
{
    struct comb_principal *p = (struct comb_principal *)principal;
    LOG_ENTRY();
    if (p) {
	heim_krb5_free_principal(HC(context), p->heim);
	free(p->mit.data);
	free(p);
    }
}

void KRB5_CALLCONV
krb5_free_default_realm(mit_krb5_context context, char *str)
{
    LOG_ENTRY();
    free(str);
}

mit_krb5_error_code KRB5_CALLCONV
krb5_sname_to_principal(mit_krb5_context context,
			const char *hostname, const char *service, 
			mit_krb5_int32 type,
			mit_krb5_principal *principal)
{
    krb5_error_code ret;
    krb5_principal p;

    LOG_ENTRY();

    *principal = NULL;

    ret = heim_krb5_sname_to_principal(HC(context), hostname, service, type, &p);
    if (ret)
	return ret;

    *principal = mshim_hprinc2mprinc(HC(context), p);
    heim_krb5_free_principal(HC(context), p);
    return 0;
}

mit_krb5_boolean KRB5_CALLCONV
krb5_principal_compare(mit_krb5_context context,
		       mit_krb5_const_principal p1,
		       mit_krb5_const_principal p2)
{
    struct comb_principal *c1 = (struct comb_principal *)p1;
    struct comb_principal *c2 = (struct comb_principal *)p2;

    return heim_krb5_principal_compare(HC(context), c1->heim, c2->heim);
}

mit_krb5_boolean KRB5_CALLCONV
krb5_realm_compare(mit_krb5_context context,
		   mit_krb5_const_principal p1,
		   mit_krb5_const_principal p2)
{
    struct comb_principal *c1 = (struct comb_principal *)p1;
    struct comb_principal *c2 = (struct comb_principal *)p2;

    return heim_krb5_realm_compare(HC(context), c1->heim, c2->heim);
}

mit_krb5_error_code KRB5_CALLCONV
krb5_get_realm_domain(mit_krb5_context, const char *, char **);


mit_krb5_error_code KRB5_CALLCONV
krb5_get_realm_domain(mit_krb5_context context, const char *realm, char **domain)
{
    const char *d;

    d = heim_krb5_config_get_string(HC(context), NULL, "realms", realm,
				    "default_realm", NULL);
    if (d == NULL) {
	*domain = NULL;
	return (-1429577726L); /* PROF_NO_SECTION */
    }
    *domain = strdup(d);
    return 0;
}
