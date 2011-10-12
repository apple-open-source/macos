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

mit_krb5_error_code KRB5_CALLCONV
krb5_set_password_using_ccache(mit_krb5_context context,
			       mit_krb5_ccache ccache,
			       char *newpw,
			       mit_krb5_principal change_password_for,
			       int *result_code,
			       mit_krb5_data *result_code_string,
			       mit_krb5_data *result_string)
{
    krb5_error_code ret;
    krb5_principal target = NULL;
    krb5_data code_string, string;

    LOG_ENTRY();

    if (change_password_for) {
	struct comb_principal *p;
	p = (struct comb_principal *)change_password_for;
	target = p->heim;
    }	

    memset(&code_string, 0, sizeof(code_string));
    memset(&string, 0, sizeof(string));

    ret = heim_krb5_set_password_using_ccache(HC(context),
					      (krb5_ccache)ccache,
					      newpw,
					      target,
					      result_code,
					      &code_string,
					      &string);
    if (ret) {
	LOG_FAILURE(ret, "krb5_set_password_using_ccache");
	return ret;
    }

    if (result_code_string)
	mshim_hdata2mdata(&code_string, result_code_string);
    else
	heim_krb5_data_free(&code_string);

    if (result_string)
	mshim_hdata2mdata(&string, result_string);
    else
	heim_krb5_data_free(&string);
    
    return 0;
}

mit_krb5_error_code KRB5_CALLCONV
krb5_set_password(mit_krb5_context context,
		  mit_krb5_creds *creds,
		  char *newpw,
		  mit_krb5_principal change_password_for,
		  int *result_code,
		  mit_krb5_data *result_code_string,
		  mit_krb5_data *result_string)
{
    krb5_error_code ret;
    krb5_principal target = NULL;
    krb5_data code_string, string;
    krb5_creds hcred;

    LOG_ENTRY();

    if (change_password_for) {
	struct comb_principal *p;
	p = (struct comb_principal *)change_password_for;
	target = p->heim;
    }	

    memset(&code_string, 0, sizeof(code_string));
    memset(&string, 0, sizeof(string));

    mshim_mcred2hcred(HC(context), creds, &hcred);

    ret = heim_krb5_set_password(HC(context),
				 &hcred,
				 newpw,
				 target,
				 result_code,
				 &code_string,
				 &string);
    heim_krb5_free_cred_contents(HC(context), &hcred);
    if (ret) {
	LOG_FAILURE(ret, "krb5_set_password");
	return ret;
    }

    if (result_code_string)
	mshim_hdata2mdata(&code_string, result_code_string);
    else
	heim_krb5_data_free(&code_string);

    if (result_string)
	mshim_hdata2mdata(&string, result_string);
    else
	heim_krb5_data_free(&string);
    
    return 0;
}

