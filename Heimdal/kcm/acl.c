/*
 * Copyright (c) 2005, PADL Software Pty Ltd.
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
 * 3. Neither the name of PADL Software nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PADL SOFTWARE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL PADL SOFTWARE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "kcm_locl.h"

krb5_error_code
kcm_access(krb5_context context,
	   kcm_client *client,
	   kcm_operation opcode,
	   kcm_ccache ccache)
{
    krb5_error_code ret;

    KCM_ASSERT_VALID(ccache);
    
    HEIMDAL_MUTEX_lock(&ccache->mutex);
    if (ccache->flags & KCM_FLAGS_OWNER_IS_SYSTEM) {
	/* Let root always read system caches */
	if (CLIENT_IS_ROOT(client)) {
	    ret = 0;
	} else {
	    ret = KRB5_FCC_PERM;
	}
    } else if (kcm_is_same_session_locked(client, ccache->uid, ccache->session)) {
	/* same session same as owner */
	ret = 0;
    } else {
	ret = KRB5_FCC_PERM;
    }
    HEIMDAL_MUTEX_unlock(&ccache->mutex);

    if (ret) {
	kcm_log(2, "Process %d is not permitted to call %s on cache %s",
		client->pid, kcm_op2string(opcode), ccache->name);
    }

    return ret;
}

krb5_error_code
kcm_principal_access_locked(krb5_context context,
	   kcm_client *client,
	   krb5_principal server,
	   kcm_operation opcode,
	   kcm_ccache ccache)
{
    KCM_ASSERT_VALID(ccache);
    
    if (!(server->name.name_string.len == 2 &&
	strcmp(server->name.name_string.val[0], "krb5_ccache_conf_data") == 0 &&
	strcmp(server->name.name_string.val[1], "password") == 0))
    {
	// we arent concerned with it, exit and allow access
	return 0;
    }
    
    //default to no access
    krb5_error_code ret = KRB5_FCC_PERM;

    switch (client->iakerb_access) {
	case IAKERB_NOT_CHECKED:
	{
	    const char* callingApp = kcm_client_get_execpath(client);
	    kcm_log(1, "kcm_principal_access: calling app: %s", callingApp);
	    
	    // we check for either the presence of the entitlement or the approved apps.  The file path is the first filter to avoid the expensive code signature check until needed.
	    if (krb5_has_entitlement(client->audit_token, CFSTR("com.apple.private.gssapi.iakerb-data-access"))) {
		kcm_log(1, "kcm_principal_access: has entitlement");
		client->iakerb_access = IAKERB_ACCESS_GRANTED;
	    } else if (strcmp(callingApp, "/System/Library/CoreServices/NetAuthAgent.app/Contents/MacOS/NetAuthSysAgent") == 0 &&
		       krb5_applesigned(context, client->audit_token, "com.apple.NetAuthSysAgent")) {
		client->iakerb_access = IAKERB_ACCESS_GRANTED;
	    } else if (strcmp(callingApp, "/usr/sbin/gssd") == 0 &&
		       krb5_applesigned(context, client->audit_token, "com.apple.gssd")) {
		client->iakerb_access = IAKERB_ACCESS_GRANTED;
	    } else {
		client->iakerb_access = IAKERB_ACCESS_DENIED;
	    }
	    
	    if (client->iakerb_access == IAKERB_ACCESS_GRANTED) {
		ret = 0;
	    }
	    break;
	}
	case IAKERB_ACCESS_GRANTED:
	    ret = 0;
	    break;
	    
	case IAKERB_ACCESS_DENIED:
	    ret = KRB5_FCC_PERM;
	    break;
    }

    kcm_log(1, "kcm_principal_access: access %s", (ret==0 ? "allowed" : "denied"));
    return ret;
}

krb5_error_code
kcm_chmod(krb5_context context,
	  kcm_client *client,
	  kcm_ccache ccache,
	  uint16_t mode)
{
    KCM_ASSERT_VALID(ccache);

    HEIMDAL_MUTEX_lock(&ccache->mutex);
    /* System cache mode can only be set at startup */
    if (ccache->flags & KCM_FLAGS_OWNER_IS_SYSTEM) {
	HEIMDAL_MUTEX_unlock(&ccache->mutex);
	return KRB5_FCC_PERM;
    }

    if (ccache->uid != client->uid) {
	HEIMDAL_MUTEX_unlock(&ccache->mutex);
	return KRB5_FCC_PERM;
    }
    HEIMDAL_MUTEX_unlock(&ccache->mutex);
    return 0;
}

krb5_error_code
kcm_chown(krb5_context context,
	  kcm_client *client,
	  kcm_ccache ccache,
	  uid_t uid)
{
    KCM_ASSERT_VALID(ccache);

    HEIMDAL_MUTEX_lock(&ccache->mutex);
    /* System cache mode can only be set at startup */
    if (ccache->flags & KCM_FLAGS_OWNER_IS_SYSTEM) {
	HEIMDAL_MUTEX_unlock(&ccache->mutex);
	return KRB5_FCC_PERM;
    }

    if (ccache->uid != client->uid) {
	HEIMDAL_MUTEX_unlock(&ccache->mutex);
	return KRB5_FCC_PERM;
    }

    ccache->uid = uid;

    HEIMDAL_MUTEX_unlock(&ccache->mutex);

    return 0;
}

