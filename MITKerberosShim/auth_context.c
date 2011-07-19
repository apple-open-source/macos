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

#define MIT_KRB5_DEPRECATED 1

#include "heim.h"
#include "mit-krb5.h"
#include <string.h>
#include <errno.h>
#include <syslog.h>

mit_krb5_error_code KRB5_CALLCONV
krb5_auth_con_setaddrs(mit_krb5_context context,
		       mit_krb5_auth_context ac,
		       mit_krb5_address *caddr,
		       mit_krb5_address *saddr)
{
    LOG_ENTRY();
    return 0;
}

mit_krb5_error_code KRB5_CALLCONV
krb5_auth_con_getaddrs(mit_krb5_context context,
		       mit_krb5_auth_context ac,
		       mit_krb5_address **caddr,
		       mit_krb5_address **saddr)
{
    LOG_ENTRY();
    *caddr = NULL;
    *saddr = NULL;
    return 0;
}


mit_krb5_error_code KRB5_CALLCONV
krb5_auth_con_setports(mit_krb5_context context,
		       mit_krb5_auth_context ac,
		       mit_krb5_address *caddr,
		       mit_krb5_address *saddr)
{
    LOG_ENTRY();
    return 0;
}

mit_krb5_error_code KRB5_CALLCONV
krb5_auth_con_getkey(mit_krb5_context context,
		     mit_krb5_auth_context ac,
		     mit_krb5_keyblock **keyblock)
{
    LOG_ENTRY();
    return 0;
}

mit_krb5_error_code KRB5_CALLCONV
krb5_auth_con_setrcache(mit_krb5_context context,
			mit_krb5_auth_context ac,
			mit_krb5_rcache rcaceh)
{
    LOG_ENTRY();
    return 0;
}

mit_krb5_error_code KRB5_CALLCONV
krb5_auth_con_getrcache(mit_krb5_context context,
			mit_krb5_auth_context ac,
			mit_krb5_rcache *rcache)
{
    LOG_ENTRY();
    *rcache = NULL;
    return 0;
}

mit_krb5_error_code KRB5_CALLCONV
krb5_auth_con_getauthenticator(mit_krb5_context context,
			       mit_krb5_auth_context ac,
			       mit_krb5_authenticator **auth)
{
    LOG_ENTRY();
    *auth = NULL;
    return 0;
}

mit_krb5_error_code KRB5_CALLCONV
krb5_auth_con_getlocalsubkey(mit_krb5_context context,
			     mit_krb5_auth_context ac,
			     mit_krb5_keyblock **key)
{
    LOG_ENTRY();
    krb5_keyblock *hkey = NULL;
    krb5_error_code ret;

    *key = NULL;

    ret = heim_krb5_auth_con_getlocalsubkey(HC(context),
					    (krb5_auth_context)ac,
					    &hkey);
    if (ret)
	return ret;
    if (hkey) {
	*key = mshim_malloc(sizeof(**key));
	mshim_hkeyblock2mkeyblock(hkey, *key);
	heim_krb5_free_keyblock(HC(context), hkey);
    }
    return 0;
}

mit_krb5_error_code KRB5_CALLCONV
krb5_auth_con_getremotesubkey(mit_krb5_context context,
			      mit_krb5_auth_context ac,
			      mit_krb5_keyblock **key)
{
    LOG_ENTRY();
    krb5_keyblock *hkey = NULL;
    krb5_error_code ret;

    *key = NULL;

    ret = heim_krb5_auth_con_getremotesubkey(HC(context),
					     (krb5_auth_context)ac,
					     &hkey);
    if (ret)
	return ret;

    if (hkey) {
	*key = mshim_malloc(sizeof(**key));
	mshim_hkeyblock2mkeyblock(hkey, *key);
	heim_krb5_free_keyblock(HC(context), hkey);
    }
    return 0;
}
