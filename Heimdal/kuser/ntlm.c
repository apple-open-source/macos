/*
 * Copyright (c) 1997-2008 Kungliga Tekniska Högskolan
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
#include <krb5.h>
#include <kcm.h>


static void
add_cred(krb5_context context)
{
    krb5_error_code ret;
    krb5_storage *request, *response;
    krb5_data response_data;
    krb5_data data;

#if 0
    char password[512];

    if (UI_UTIL_read_pw_string(password, sizeof(password),
			       "Password:", 0) != 1)
       errx(1, "failed reading password");
#endif
       
    ret = krb5_kcm_storage_request(context, KCM_OP_ADD_NTLM_CRED, &request);
    if (ret)
	krb5_err(context, 1, ret, "krb5_kcm_storage_request");

    krb5_store_stringz(request, "lha");
    krb5_store_stringz(request, "BUILTIN");
    data.data = "\xac\x8e\x65\x7f\x83\xdf\x82\xbe\xea\x5d\x43\xbd\xaf\x78\x0\xcc"; /* foo */
    data.length = 16;
    krb5_store_data(request, data);

    ret = krb5_kcm_call(context, request, &response, &response_data);
    if (ret)
	krb5_err(context, 1, ret, "krb5_kcm_call");

    krb5_storage_free(request);
    krb5_storage_free(response);
    krb5_data_free(&response_data);
}


static void
list_cred(krb5_context context)
{
    krb5_error_code ret;
    krb5_storage *request, *response;
    krb5_data response_data;
    
    ret = krb5_kcm_storage_request(context, KCM_OP_GET_NTLM_USER_LIST, &request);
    if (ret)
	krb5_err(context, 1, ret, "krb5_kcm_storage_request");

    ret = krb5_kcm_call(context, request, &response, &response_data);
    if (ret)
	krb5_err(context, 1, ret, "krb5_kcm_call");

    while (1) {
	uint32_t morep;
	char *user = NULL, *domain = NULL;

	ret = krb5_ret_uint32(response, &morep);
	if (ret)
	    krb5_err(context, ret, 1, "ret: morep");

	if (morep == 0)
	    break;

	ret = krb5_ret_stringz(response, &user);
	if (ret)
	    krb5_err(context, ret, 1, "ret: user");
	ret = krb5_ret_stringz(response, &domain);
	if (ret)
	    krb5_err(context, ret, 1, "ret: domain");


	printf("user: %s domain: %s\n", user, domain);
    }

    krb5_storage_free(request);
    krb5_storage_free(response);
    krb5_data_free(&response_data);
}



int
main(int argc, char **argv)
{
    krb5_error_code ret;
    krb5_context context;
    
    ret = krb5_init_context(&context);
    if (ret)
	errx(1, "krb5_init_context");

    list_cred(context);

    add_cred(context);

    krb5_free_context(context);

    return 0;
}
