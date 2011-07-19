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


#include <Kerberos/Kerberos.h>
#include <string.h>
#include <err.h>


int
main(int argc, char **argv)
{
    krb5_principal p1, p2;
    krb5_context context;
    krb5_error_code ret;

    ret = krb5_init_context(&context);
    if (ret)
	errx(1, "krb5_init_context");

    ret = krb5_parse_name(context, "lha@OD.APPLE.COM", &p1);
    if (ret)
	errx(1, "krb5_parse_name");
    ret = krb5_parse_name(context, "foo@OD.APPLE.COM", &p2);
    if (ret)
	errx(1, "krb5_parse_name");

    ret = krb5_realm_compare(context, p1, p2);
    if (ret == 0)
	errx(1, "krb5_realm_compare");
	
    ret = krb5_principal_compare(context, p1, p2);
    if (ret != 0)
	errx(1, "krb5_principal_compare");

    ret = krb5_cc_end_seq_get(context, NULL, NULL);
    if (ret)
	errx(1, "krb5_cc_end_seq_get");

    krb5_free_context(context);

    return 0;
}        

