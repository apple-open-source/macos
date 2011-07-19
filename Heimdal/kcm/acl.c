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

    if (ccache->flags & KCM_FLAGS_OWNER_IS_SYSTEM) {
	/* Let root always read system caches */
	if (CLIENT_IS_ROOT(client)) {
	    ret = 0;
	} else {
	    ret = KRB5_FCC_PERM;
	}
    } else if (kcm_is_same_session(client, ccache->uid, ccache->session)) {
	/* same session same as owner */
	ret = 0;
    } else {
	ret = KRB5_FCC_PERM;
    }

    if (ret) {
	kcm_log(2, "Process %d is not permitted to call %s on cache %s",
		client->pid, kcm_op2string(opcode), ccache->name);
    }

    return ret;
}

krb5_error_code
kcm_chmod(krb5_context context,
	  kcm_client *client,
	  kcm_ccache ccache,
	  uint16_t mode)
{
    KCM_ASSERT_VALID(ccache);

    /* System cache mode can only be set at startup */
    if (ccache->flags & KCM_FLAGS_OWNER_IS_SYSTEM)
	return KRB5_FCC_PERM;

    if (ccache->uid != client->uid)
	return KRB5_FCC_PERM;

    return 0;
}

krb5_error_code
kcm_chown(krb5_context context,
	  kcm_client *client,
	  kcm_ccache ccache,
	  uid_t uid)
{
    KCM_ASSERT_VALID(ccache);

    /* System cache owner can only be set at startup */
    if (ccache->flags & KCM_FLAGS_OWNER_IS_SYSTEM)
	return KRB5_FCC_PERM;

    if (ccache->uid != client->uid && client->uid != 0)
	return KRB5_FCC_PERM;

    HEIMDAL_MUTEX_lock(&ccache->mutex);

    ccache->uid = uid;

    HEIMDAL_MUTEX_unlock(&ccache->mutex);

    return 0;
}

