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
#include <pwd.h>

krb5_error_code
kcm_ccache_resolve_client(krb5_context context,
			  kcm_client *client,
			  kcm_operation opcode,
			  const char *name,
			  kcm_ccache *ccache)
{
    krb5_error_code ret;

    ret = kcm_ccache_resolve_by_name(context, name, ccache);
    if (ret) {
	kcm_log(1, "Failed to resolve cache %s", name);
	return ret;
    }

    ret = kcm_access(context, client, opcode, *ccache);
    if (ret) {
	ret = KRB5_FCC_NOFILE; /* don't disclose */
	kcm_release_ccache(context, *ccache);
    }

    return ret;
}

krb5_error_code
kcm_ccache_destroy_client(krb5_context context,
			  kcm_client *client,
			  const char *name)
{
    krb5_error_code ret;
    kcm_ccache ccache;

    ret = kcm_ccache_resolve_by_name(context, name, &ccache);
    if (ret) {
	kcm_log(1, "Failed to resolve cache %s", name);
	return ret;
    }

    ret = kcm_access(context, client, KCM_OP_DESTROY, ccache);
    kcm_release_ccache(context, ccache);
    if (ret)
	return ret;

    return kcm_ccache_destroy(context, name);
}

krb5_error_code
kcm_ccache_new_client(krb5_context context,
		      kcm_client *client,
		      const char *name,
		      kcm_ccache *ccache_p)
{
    krb5_error_code ret;
    kcm_ccache ccache;

    ret = kcm_ccache_resolve_by_name(context, name, &ccache);
    if (ret == 0) {
	if ((ccache->uid != client->uid) && !CLIENT_IS_ROOT(client))
	    return KRB5_FCC_PERM;
    } else if (ret != KRB5_FCC_NOFILE && !(CLIENT_IS_ROOT(client) && ret == KRB5_FCC_PERM)) {
		return ret;
    }

    if (ret == KRB5_FCC_NOFILE) {
	ret = kcm_ccache_new(context, name, &ccache);
	if (ret) {
	    kcm_log(1, "Failed to initialize cache %s", name);
	    return ret;
	}

	/* bind to current client */
	ccache->uid = client->uid;
	ccache->session = client->session;

	/* 
	 * add notification when the session goes away, so we can
	 * remove the credential
	 */
	kcm_session_add(client->session);

    } else {
	ret = kcm_zero_ccache_data(context, ccache);
	if (ret) {
	    kcm_log(1, "Failed to empty cache %s", name);
	    kcm_release_ccache(context, ccache);
	    return ret;
	}
	heim_ipc_event_cancel(ccache->renew_event);
	heim_ipc_event_cancel(ccache->expire_event);
    }

    ret = kcm_access(context, client, KCM_OP_INITIALIZE, ccache);
    if (ret) {
	kcm_release_ccache(context, ccache);
	kcm_ccache_destroy(context, name);
	return ret;
    }

    /*
     * Finally, if the user is root and the cache was created under
     * another user's name, chown the cache to that user.
     */
    if (CLIENT_IS_ROOT(client)) {
	unsigned long uid;
	int matches = sscanf(name,"%ld:",&uid);
	if (matches == 0)
	    matches = sscanf(name,"%ld",&uid);
	if (matches == 1) {
	    kcm_chown(context, client, ccache, (uid_t)uid);
	}
    }

    *ccache_p = ccache;
    return 0;
}

const char *
kcm_client_get_execpath(kcm_client *client)
{
    if (client->execpath[0] == '\0') {
	int ret = proc_pidpath(client->pid, client->execpath, sizeof(client->execpath));
	if (ret != -1)
	    client->execpath[sizeof(client->execpath) - 1] = '\0';
	else {
	    /* failed, lets not try again */
	    client->execpath[0] = 0x01;
	    client->execpath[1] = 0x0;
	}
    }
    if (client->execpath[0] != '/')
	return NULL;

    return client->execpath;
}


