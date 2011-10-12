/*
 * Copyright (c) 2005, PADL Software Pty Ltd.
 * All rights reserved.
 *
 * Portions Copyright (c) 2009 - 2011 Apple Inc. All rights reserved.
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
#include <uuid/uuid.h>
#include <bsm/libbsm.h>
#include <vproc.h>
#include <vproc_priv.h>

static void kcm_release_ccache_locked(krb5_context, kcm_ccache);



HEIMDAL_MUTEX ccache_mutex = HEIMDAL_MUTEX_INITIALIZER;
TAILQ_HEAD(ccache_head, kcm_ccache_data);
static struct ccache_head ccache_head = TAILQ_HEAD_INITIALIZER(ccache_head);
static uint32_t ccache_nextid = 0;

char *
kcm_ccache_nextid(pid_t pid, uid_t uid)
{
    kcm_ccache c;
    char *name = NULL;
    unsigned n;

    HEIMDAL_MUTEX_lock(&ccache_mutex);
    while (name == NULL) {
	n = ++ccache_nextid;
	asprintf(&name, "%ld:%u", (long)uid, n);

	/* find dups */
	TAILQ_FOREACH(c, &ccache_head, members) {
	    if (strcmp(c->name, name) == 0) {
		free(name);
		name = NULL;
		break;
	    }
	}
    }
    HEIMDAL_MUTEX_unlock(&ccache_mutex);

    return name;
}

krb5_error_code
kcm_ccache_resolve(krb5_context context,
		   const char *name,
		   kcm_ccache *ccache)
{
    kcm_ccache p;
    krb5_error_code ret;

    *ccache = NULL;

    ret = KRB5_FCC_NOFILE;

    HEIMDAL_MUTEX_lock(&ccache_mutex);

    TAILQ_FOREACH(p, &ccache_head, members) {
	if (strcmp(p->name, name) == 0) {
	    ret = 0;
	    break;
	}
    }

    if (ret == 0) {
	kcm_retain_ccache(context, p);
	*ccache = p;
    }

    HEIMDAL_MUTEX_unlock(&ccache_mutex);

    return ret;
}

krb5_error_code
kcm_ccache_resolve_by_uuid(krb5_context context,
			   kcmuuid_t uuid,
			   kcm_ccache *ccache)
{
    kcm_ccache p;
    krb5_error_code ret;

    *ccache = NULL;

    ret = KRB5_FCC_NOFILE;

    HEIMDAL_MUTEX_lock(&ccache_mutex);

    TAILQ_FOREACH(p, &ccache_head, members) {
	if (memcmp(p->uuid, uuid, sizeof(kcmuuid_t)) == 0) {
	    ret = 0;
	    break;
	}
    }

    if (ret == 0) {
	kcm_retain_ccache(context, p);
	*ccache = p;
    }

    HEIMDAL_MUTEX_unlock(&ccache_mutex);

    return ret;
}

krb5_error_code
kcm_ccache_get_uuids(krb5_context context,
		     kcm_client *client,
		     kcm_operation opcode,
		     krb5_storage *sp)
{
    kcm_ccache p;

    HEIMDAL_MUTEX_lock(&ccache_mutex);

    TAILQ_FOREACH(p, &ccache_head, members) {
	krb5_error_code ret;
	ret = kcm_access(context, client, opcode, p);
	if (ret)
	    continue;
	krb5_storage_write(sp, p->uuid, sizeof(p->uuid));
    }

    HEIMDAL_MUTEX_unlock(&ccache_mutex);

    return 0;
}


krb5_error_code
kcm_debug_ccache(krb5_context context)
{
    kcm_ccache p;

    TAILQ_FOREACH(p, &ccache_head, members) {
	char *cpn = NULL, *spn = NULL;
	int ncreds = 0;
	struct kcm_creds *k;

	KCM_ASSERT_VALID(p);

	for (k = p->creds; k != NULL; k = k->next)
	    ncreds++;

	if (p->client != NULL)
	    krb5_unparse_name(context, p->client, &cpn);
	if (p->server != NULL)
	    krb5_unparse_name(context, p->server, &spn);
	
	kcm_log(7, "cache name %s refcnt %d flags %04x"
		"uid %d client %s server %s ncreds %d",
		p->name, p->refcnt, p->flags, p->uid,
		(cpn == NULL) ? "<none>" : cpn,
		(spn == NULL) ? "<none>" : spn,
		ncreds);

	if (cpn != NULL)
	    free(cpn);
	if (spn != NULL)
	    free(spn);
    }

    return 0;
}

static void
kcm_free_ccache_data_internal(krb5_context context,
			      kcm_ccache cache)
{
    KCM_ASSERT_VALID(cache);

    if (cache->name != NULL) {
	free(cache->name);
	cache->name = NULL;
    }

    if (cache->flags & KCM_FLAGS_USE_KEYTAB) {
	krb5_kt_close(context, cache->keytab);
	cache->keytab = NULL;
    } else if (cache->flags & KCM_FLAGS_USE_PASSWORD) {
	memset(cache->password, 0, strlen(cache->password));
	free(cache->password);
    }

    cache->flags = 0;
    cache->uid = -1;
    cache->session = -1;

    kcm_zero_ccache_data_internal(context, cache);

    cache->tkt_life = 0;
    cache->renew_life = 0;

    cache->refcnt = 0;
}


krb5_error_code
kcm_ccache_destroy(krb5_context context, const char *name)
{
    kcm_ccache p;

    HEIMDAL_MUTEX_lock(&ccache_mutex);
    TAILQ_FOREACH(p, &ccache_head, members) {
	if (strcmp(p->name, name) == 0) {
	    HEIMDAL_MUTEX_lock(&p->mutex);
	    TAILQ_REMOVE(&ccache_head, p, members);
	    break;
	}
    }
    HEIMDAL_MUTEX_unlock(&ccache_mutex);
    if (p == NULL)
	return KRB5_FCC_NOFILE;

    /* XXX blocking */
    heim_ipc_event_cancel(p->renew_event);
    heim_ipc_event_free(p->renew_event);
    p->renew_event = NULL;
    
    heim_ipc_event_cancel(p->expire_event);
    heim_ipc_event_free(p->expire_event);
    p->expire_event = NULL;

    kcm_release_ccache_locked(context, p);

    return 0;
}

#define KCM_EVENT_QUEUE_INTERVAL 60

void
kcm_update_expire_time(kcm_ccache ccache)
{
    time_t renewtime = time(NULL) + 3600 * 2;
    time_t expire = ccache->expire;

    /* if the ticket is about to expire in less then QUEUE_INTERVAL,
     * don't bother */
    if (time(NULL) + KCM_EVENT_QUEUE_INTERVAL > expire)
	return;

    if (renewtime > expire - KCM_EVENT_QUEUE_INTERVAL)
	renewtime = expire - KCM_EVENT_QUEUE_INTERVAL;

    kcm_log(1, "%s: will try to renew credentals in %d seconds",
	    ccache->name, (int)(renewtime - time(NULL)));

    heim_ipc_event_set_time(ccache->renew_event, renewtime);
    ccache->renew_time = renewtime;

#ifdef HAVE_NOTIFY_H
    notify_post(KRB5_KCM_NOTIFY_CACHE_CHANGED);
#endif
}

static void
renew_func(heim_event_t event, void *ptr)
{
    kcm_ccache cache = ptr;
    krb5_error_code ret;
    time_t expire;

    kcm_log(0, "cache: %s renewing", cache->name);

    HEIMDAL_MUTEX_lock(&cache->mutex);

    if (cache->flags & KCM_MASK_KEY_PRESENT) {
	ret = kcm_ccache_acquire(kcm_context, cache, &expire);
    } else {
	ret = kcm_ccache_refresh(kcm_context, cache, &expire);
    }
    switch (ret) {
    case KRB5KRB_AP_ERR_BAD_INTEGRITY:
    case KRB5KRB_AP_ERR_MODIFIED:
    case KRB5KDC_ERR_PREAUTH_FAILED:
	/* bad password, drop it like dead */
	kcm_log(0, "cache: %s got bad password, stop renewing",
		cache->name);
	break;
    case 0:
	cache->expire = expire;
	heim_ipc_event_set_time(cache->expire_event, cache->expire);
	break;
    default: {
	const char *msg = krb5_get_error_message(kcm_context, ret);
	kcm_log(0, "failed to renew: %s: %d", msg, ret);
	krb5_free_error_message(kcm_context, msg);
    }
    }
    kcm_update_expire_time(cache);
    HEIMDAL_MUTEX_unlock(&cache->mutex);
}

static void
expire_func(heim_event_t event, void *ptr)
{
    kcm_ccache cache = ptr;
    krb5_error_code ret;

    kcm_log(0, "cache: %s expired", cache->name);

    HEIMDAL_MUTEX_lock(&cache->mutex);

    heim_ipc_event_cancel(cache->renew_event);

    if (cache->flags & KCM_MASK_KEY_PRESENT){
	time_t expire;

	ret = kcm_ccache_acquire(kcm_context, cache, &expire);

	switch (ret) {
	case KRB5KRB_AP_ERR_BAD_INTEGRITY:
	case KRB5KRB_AP_ERR_MODIFIED:
	case KRB5KDC_ERR_PREAUTH_FAILED:
	    /* bad password, drop it like dead */
	    kcm_log(0, "cache: %s got bad password, stop renewing",
		    cache->name);
	    break;
	case 0:
	    kcm_log(0, "cache: %s got new tickets (expire in %d seconds)",
		    cache->name, (int)(expire - time(NULL)));

	    cache->expire = expire;
	    heim_ipc_event_set_time(cache->expire_event, cache->expire);
	    kcm_update_expire_time(cache);
	    break;
	default:
	    heim_ipc_event_set_time(cache->expire_event, time(NULL) + 300);
	    break;
	}
    }
    HEIMDAL_MUTEX_unlock(&cache->mutex);
}

static void
release_cache(void *ctx)
{
    kcm_release_ccache(kcm_context, (kcm_ccache)ctx);
}

static krb5_error_code
kcm_ccache_alloc(krb5_context context,
		 const char *name,
		 kcm_ccache *cache)
{
    kcm_ccache p = NULL;
    krb5_error_code ret;

    /* First, check for duplicates */
    HEIMDAL_MUTEX_lock(&ccache_mutex);

    TAILQ_FOREACH(p, &ccache_head, members) {
	if (strcmp(p->name, name) == 0) {
	    ret = KRB5_CC_WRITE;
	    goto out;
	}
    }

    /*
     * Create an enpty cache for us.
     */
    p = calloc(1, sizeof(*p));
    if (p == NULL) {
	ret = KRB5_CC_NOMEM;
	goto out;
    }
    HEIMDAL_MUTEX_init(&p->mutex);

    CCRandomCopyBytes(kCCRandomDefault, p->uuid, sizeof(p->uuid));

    p->name = strdup(name);
    if (p->name == NULL) {
	ret = KRB5_CC_NOMEM;
	goto out;
    }

    p->refcnt = 3; /* on members, and both events */
    p->holdcount = 1;
    p->flags = 0;
    p->uid = -1;
    p->client = NULL;
    p->server = NULL;
    p->creds = NULL;
    p->keytab = NULL;
    p->password = NULL;
    p->tkt_life = 0;
    p->renew_life = 0;

    p->renew_event = heim_ipc_event_create_f(renew_func, p);
    p->expire_event = heim_ipc_event_create_f(expire_func, p);

    heim_ipc_event_set_final_f(p->renew_event, release_cache);
    heim_ipc_event_set_final_f(p->expire_event, release_cache);

    TAILQ_INSERT_HEAD(&ccache_head, p, members);

    *cache = p;

    HEIMDAL_MUTEX_unlock(&ccache_mutex);
    return 0;

out:
    HEIMDAL_MUTEX_unlock(&ccache_mutex);
    if (p != NULL) {
	HEIMDAL_MUTEX_destroy(&p->mutex);
	free(p);
    }
    *cache = NULL;
    return ret;
}

krb5_error_code
kcm_ccache_enqueue_default(krb5_context context,
			   kcm_ccache cache,
			   krb5_creds *newcred)
{
    if (newcred == NULL) {
	heim_ipc_event_set_time(cache->expire_event, 0);
    } else if (cache->flags & KCM_MASK_KEY_PRESENT) {
	cache->expire = newcred->times.endtime;
	kcm_update_expire_time(cache);
    } else if (newcred->flags.b.initial) {
	cache->expire = newcred->times.endtime;
	if (newcred->flags.b.renewable)
	    kcm_update_expire_time(cache);
	heim_ipc_event_set_time(cache->expire_event, newcred->times.endtime);
    }
    return 0;
}


krb5_error_code
kcm_ccache_remove_creds_internal(krb5_context context,
				 kcm_ccache ccache)
{
    struct kcm_creds *k;

    k = ccache->creds;
    while (k != NULL) {
	struct kcm_creds *old;

	krb5_free_cred_contents(context, &k->cred);
	old = k;
	k = k->next;
	free(old);
    }
    ccache->creds = NULL;

#ifdef HAVE_NOTIFY_H
    notify_post(KRB5_KCM_NOTIFY_CACHE_CHANGED);
#endif

    return 0;
}

krb5_error_code
kcm_ccache_remove_creds(krb5_context context,
			kcm_ccache ccache)
{
    krb5_error_code ret;

    KCM_ASSERT_VALID(ccache);

    HEIMDAL_MUTEX_lock(&ccache->mutex);
    ret = kcm_ccache_remove_creds_internal(context, ccache);
    HEIMDAL_MUTEX_unlock(&ccache->mutex);

    return ret;
}

krb5_error_code
kcm_zero_ccache_data_internal(krb5_context context,
			      kcm_ccache cache)
{
    if (cache->client != NULL) {
	krb5_free_principal(context, cache->client);
	cache->client = NULL;
    }

    if (cache->server != NULL) {
	krb5_free_principal(context, cache->server);
	cache->server = NULL;
    }

    kcm_ccache_remove_creds_internal(context, cache);

    return 0;
}

krb5_error_code
kcm_zero_ccache_data(krb5_context context,
		     kcm_ccache cache)
{
    krb5_error_code ret;

    KCM_ASSERT_VALID(cache);

    HEIMDAL_MUTEX_lock(&cache->mutex);
    ret = kcm_zero_ccache_data_internal(context, cache);
    HEIMDAL_MUTEX_unlock(&cache->mutex);

    return ret;
}

krb5_error_code
kcm_retain_ccache(krb5_context context,
		  kcm_ccache ccache)
{
    KCM_ASSERT_VALID(ccache);

    HEIMDAL_MUTEX_lock(&ccache->mutex);
    ccache->refcnt++;
    HEIMDAL_MUTEX_unlock(&ccache->mutex);

    return 0;
}

static void
kcm_release_ccache_locked(krb5_context context, kcm_ccache p)
{
    if (p->refcnt == 1) {
	kcm_free_ccache_data_internal(context, p);
	HEIMDAL_MUTEX_unlock(&p->mutex);
	HEIMDAL_MUTEX_destroy(&p->mutex);
	free(p);
    } else {
	p->refcnt--;
	HEIMDAL_MUTEX_unlock(&p->mutex);
    }
}


krb5_error_code
kcm_release_ccache(krb5_context context, kcm_ccache c)
{
    KCM_ASSERT_VALID(c);

    HEIMDAL_MUTEX_lock(&c->mutex);
    kcm_release_ccache_locked(context, c);

    return 0;
}

krb5_error_code
kcm_ccache_new(krb5_context context,
	       const char *name,
	       kcm_ccache *ccache)
{
    krb5_error_code ret;

    ret = kcm_ccache_alloc(context, name, ccache);
    if (ret == 0) {
	/*
	 * one reference is held by the linked list,
	 * one by the caller
	 */
	kcm_retain_ccache(context, *ccache);
    }

    return ret;
}

krb5_error_code
kcm_ccache_store_cred(krb5_context context,
		      kcm_ccache ccache,
		      krb5_creds *creds,
		      int copy)
{
    krb5_error_code ret;

    KCM_ASSERT_VALID(ccache);

    HEIMDAL_MUTEX_lock(&ccache->mutex);
    ret = kcm_ccache_store_cred_internal(context, ccache, creds, NULL, copy);
    HEIMDAL_MUTEX_unlock(&ccache->mutex);

    return ret;
}

struct kcm_creds *
kcm_ccache_find_cred_uuid(krb5_context context,
			  kcm_ccache ccache,
			  kcmuuid_t uuid)
{
    struct kcm_creds *c;

    for (c = ccache->creds; c != NULL; c = c->next)
	if (memcmp(c->uuid, uuid, sizeof(c->uuid)) == 0)
	    return c;

    return NULL;
}



krb5_error_code
kcm_ccache_store_cred_internal(krb5_context context,
			       kcm_ccache ccache,
			       krb5_creds *creds,
			       kcmuuid_t uuid,
			       int copy)
{
    struct kcm_creds **c;
    krb5_error_code ret;

    /*
     * Remove dup creds and find the end to add new credential.
     */

    c = &ccache->creds;
    while (*c != NULL) {
	if (krb5_compare_creds(context, 0, creds, &(*c)->cred)) {
	    struct kcm_creds *dup_cred = *c;
	    *c = dup_cred->next;
	    krb5_free_cred_contents(context, &dup_cred->cred);
	    free(dup_cred);
	} else {
	    c = &(*c)->next;
	}
    }

    *c = (struct kcm_creds *)calloc(1, sizeof(**c));
    if (*c == NULL)
	return KRB5_CC_NOMEM;

    if (uuid)
	memcpy((*c)->uuid, uuid, sizeof((*c)->uuid));
    else
	CCRandomCopyBytes(kCCRandomDefault, (*c)->uuid, sizeof((*c)->uuid));

    if (copy) {
	ret = krb5_copy_creds_contents(context, creds, &(*c)->cred);
	if (ret) {
	    free(*c);
	    *c = NULL;
	}
    } else {
	(*c)->cred = *creds;
	ret = 0;
    }


#ifdef HAVE_NOTIFY_H
    notify_post(KRB5_KCM_NOTIFY_CACHE_CHANGED);
#endif

    return ret;
}

krb5_error_code
kcm_ccache_remove_cred_internal(krb5_context context,
				kcm_ccache ccache,
				krb5_flags whichfields,
				const krb5_creds *mcreds)
{
    krb5_error_code ret;
    struct kcm_creds **c;

    ret = KRB5_CC_NOTFOUND;

    for (c = &ccache->creds; *c != NULL; c = &(*c)->next) {
	if (krb5_compare_creds(context, whichfields, mcreds, &(*c)->cred)) {
	    struct kcm_creds *cred = *c;

	    *c = cred->next;
	    krb5_free_cred_contents(context, &cred->cred);
	    free(cred);
	    ret = 0;
	    if (*c == NULL)
		break;
	}
    }

#ifdef HAVE_NOTIFY_H
    notify_post(KRB5_KCM_NOTIFY_CACHE_CHANGED);
#endif

    return ret;
}

krb5_error_code
kcm_ccache_remove_cred(krb5_context context,
		       kcm_ccache ccache,
		       krb5_flags whichfields,
		       const krb5_creds *mcreds)
{
    krb5_error_code ret;

    KCM_ASSERT_VALID(ccache);

    HEIMDAL_MUTEX_lock(&ccache->mutex);
    ret = kcm_ccache_remove_cred_internal(context, ccache, whichfields, mcreds);
    HEIMDAL_MUTEX_unlock(&ccache->mutex);

    return ret;
}

krb5_error_code
kcm_ccache_retrieve_cred_internal(krb5_context context,
			 	  kcm_ccache ccache,
			 	  krb5_flags whichfields,
			 	  const krb5_creds *mcreds,
			 	  krb5_creds **creds)
{
    krb5_boolean match;
    struct kcm_creds *c;
    krb5_error_code ret;

    memset(creds, 0, sizeof(*creds));

    ret = KRB5_CC_END;

    match = FALSE;
    for (c = ccache->creds; c != NULL; c = c->next) {
	match = krb5_compare_creds(context, whichfields, mcreds, &c->cred);
	if (match)
	    break;
    }

    if (match) {
	ret = 0;
	*creds = &c->cred;
    }

    return ret;
}

krb5_error_code
kcm_ccache_retrieve_cred(krb5_context context,
			 kcm_ccache ccache,
			 krb5_flags whichfields,
			 const krb5_creds *mcreds,
			 krb5_creds **credp)
{
    krb5_error_code ret;

    KCM_ASSERT_VALID(ccache);

    HEIMDAL_MUTEX_lock(&ccache->mutex);
    ret = kcm_ccache_retrieve_cred_internal(context, ccache,
					    whichfields, mcreds, credp);
    HEIMDAL_MUTEX_unlock(&ccache->mutex);

    return ret;
}

char *
kcm_ccache_first_name(kcm_client *client)
{
    kcm_ccache p;
    char *name = NULL;

    HEIMDAL_MUTEX_lock(&ccache_mutex);

    TAILQ_FOREACH(p, &ccache_head, members) {
	if (kcm_is_same_session(client, p->uid, p->session))
	    break;
    }
    if (p)
	name = strdup(p->name);
    HEIMDAL_MUTEX_unlock(&ccache_mutex);
    return name;
}

void
kcm_cache_remove_session(pid_t session)
{
    kcm_ccache p, tempp;

    HEIMDAL_MUTEX_lock(&ccache_mutex);

    TAILQ_FOREACH_SAFE(p, &ccache_head, members, tempp) {
	if (p->session == session) {
	    kcm_log(1, "remove credental %s because session %d went away",
		    p->name, (int)session);

	    TAILQ_REMOVE(&ccache_head, p, members);

	    dispatch_async(dispatch_get_main_queue(), ^{
		    /* XXX blocking */
		    HEIMDAL_MUTEX_lock(&p->mutex);

		    heim_ipc_event_cancel(p->renew_event);
		    heim_ipc_event_free(p->renew_event);
		    p->renew_event = NULL;

		    heim_ipc_event_cancel(p->expire_event);
		    heim_ipc_event_free(p->expire_event);
		    p->expire_event = NULL;

		    kcm_release_ccache_locked(kcm_context, p);
		});
	}
    }
    HEIMDAL_MUTEX_unlock(&ccache_mutex);
}

static bool
session_exists(pid_t asid)
{
    auditinfo_addr_t aia;
    aia.ai_asid = asid;

    if (audit_get_sinfo_addr(&aia, sizeof(aia)) == 0)
	return true;
    return false;
}


#define CHECK(s) do { if ((s)) { goto out; } } while(0)

#define DUMP_F_SERVER	1
#define DUMP_F_PASSWORD	2
#define DUMP_F_KEYTAB	4


static krb5_error_code
parse_krb5_cache(krb5_context context, krb5_storage *sp)
{
    krb5_error_code ret;
    char *name = NULL;
    kcm_ccache c;
    uint32_t u32;
    int32_t s32;
    uint8_t u8;
    time_t renew_time;

    CHECK(ret = krb5_ret_stringz(sp, &name));
    ret = kcm_ccache_new(context, name, &c);
    free(name);
    CHECK(ret);
    CHECK(ret = krb5_ret_uuid(sp, c->uuid));
    CHECK(ret = krb5_ret_uint32(sp, &u32));
    c->renew_time = renew_time = u32;
    CHECK(ret = krb5_ret_uint32(sp, &u32));
    c->holdcount = u32;
    CHECK(ret = krb5_ret_uint32(sp, &u32));
    c->flags = u32;
    CHECK(ret = krb5_ret_int32(sp, &s32));
    c->uid = s32;
    CHECK(ret = krb5_ret_int32(sp, &s32));
    c->session = s32;

    CHECK(ret = krb5_ret_principal(sp, &c->client));

    CHECK(ret = krb5_ret_uint32(sp, &u32));

    if (u32 & DUMP_F_SERVER)
	CHECK(ret = krb5_ret_principal(sp, &c->server));

    if (u32 & DUMP_F_PASSWORD)
	CHECK(ret = krb5_ret_stringz(sp, &c->password));

    if (u32 & DUMP_F_KEYTAB) {
	char *keytab;
	CHECK(ret = krb5_ret_stringz(sp, &keytab));
	CHECK(ret = krb5_kt_resolve(context, keytab, &c->keytab));
	free(keytab);
    }

    while (1) {
	krb5_creds cred;
	kcmuuid_t uuid;

	CHECK(ret = krb5_ret_uint8(sp, &u8));
	if (u8 == 0)
	    break;

	CHECK(ret = krb5_ret_uuid(sp, uuid));
	CHECK(ret = krb5_ret_creds(sp, &cred));
	ret = kcm_ccache_store_cred_internal(context, c, &cred, uuid, 1);
	if (cred.flags.b.initial)
	    kcm_ccache_enqueue_default(context, c, &cred);

	krb5_free_cred_contents(context, &cred);
	CHECK(ret);
    }

    /* if we have a renew time and its not too far in the paste, kick off rewtime again */
    if (renew_time && renew_time > time(NULL) - 60) {
	kcm_log(1, "re-setting renew time to: %ds (original renew time)", (int)(renew_time - time(NULL)));
	heim_ipc_event_set_time(c->renew_event, renew_time);
    }
 out:
    /* in case of failure, abandon memory (and broken cache) */
    if (ret) {
	TAILQ_REMOVE(&ccache_head, c, members);
    }

    return ret;
}

static krb5_error_code
unparse_krb5_cache(krb5_context context, krb5_storage *sp, kcm_ccache c, time_t *nextwakeup)
{
    struct kcm_creds *cred;
    krb5_error_code ret;
    uint32_t sflags;

    if (c->renew_time && (*nextwakeup == 0 || *nextwakeup > c->renew_time))
	*nextwakeup = c->renew_time;

    CHECK(ret = krb5_store_stringz(sp, c->name));
    CHECK(ret = krb5_store_uuid(sp, c->uuid));
    CHECK(ret = krb5_store_uint32(sp, c->renew_time));
    CHECK(ret = krb5_store_uint32(sp, c->holdcount));
    CHECK(ret = krb5_store_uint32(sp, c->flags));
    CHECK(ret = krb5_store_int32(sp, c->uid));
    CHECK(ret = krb5_store_int32(sp, c->session));

    CHECK(ret = krb5_store_principal(sp, c->client));

    sflags = 0;
    if (c->server) sflags |= DUMP_F_SERVER;
    if (c->password) sflags |= DUMP_F_PASSWORD;
    if (c->keytab) sflags |= DUMP_F_KEYTAB;
    CHECK(ret = krb5_store_uint32(sp, sflags));

    if (c->server)
	CHECK(ret = krb5_store_principal(sp, c->server));
    if (c->password)
	CHECK(ret = krb5_store_stringz(sp, c->password));
    if (c->keytab) {
	char *str;
	CHECK(ret = krb5_kt_get_full_name(context, c->keytab, &str));
	CHECK(ret = krb5_store_stringz(sp, str));
	krb5_xfree(str);
    }
	
    for (cred = c->creds; cred != NULL; cred = cred->next) {
	CHECK(ret = krb5_store_uint8(sp, 1));
	CHECK(ret = krb5_store_uuid(sp, cred->uuid));
	CHECK(ret = krb5_store_creds(sp, &cred->cred));
    }
    CHECK(ret = krb5_store_uint8(sp, 0));
 out:
    return ret;
}

static krb5_error_code
parse_default_one(krb5_context context, krb5_storage *sp)
{
    struct kcm_default_cache *c;
    krb5_error_code ret;
    int32_t s32;
    char *str;

    c = calloc(1, sizeof(*c));
    if (c == NULL)
	return ENOMEM;

    CHECK(ret = krb5_ret_int32(sp, &s32));
    c->uid = s32;
    CHECK(ret = krb5_ret_int32(sp, &s32));
    c->session = s32;
    CHECK(ret = krb5_ret_stringz(sp, &str));
    c->name = str;

    c->next = default_caches;
    default_caches = c;

 out:
    if (ret)
	kcm_log(10, "failed to parse default entry");
    return ret;
}

static krb5_error_code
unparse_default_all(krb5_context context, krb5_storage *sp)
{
    struct kcm_default_cache *c;
    krb5_error_code r = 0;

    for (c = default_caches; r == 0 && c != NULL; c = c->next) {
	r = kcm_unparse_wrap(sp, "default-cache", c->session, ^(krb5_storage *inner) {
		krb5_error_code ret;
		CHECK(ret = krb5_store_int32(inner, c->uid));
		CHECK(ret = krb5_store_int32(inner, c->session));
		CHECK(ret = krb5_store_stringz(inner, c->name));
	    out:
		return ret;
	    });

    }

    return r;
}

#define KCM_DUMP_VERSION 1

void
kcm_parse_cache_data(krb5_context context, krb5_data *data)
{
    krb5_error_code ret; 
    krb5_storage *sp;
    char *str;
    uint8_t u8;

    sp = krb5_storage_from_readonly_mem(data->data, data->length);
    if (sp == NULL)
	return;

    CHECK(ret = krb5_ret_stringz(sp, &str));
    if (strcmp(str, "start-dump") != 0) {
	free(str);
	ret = EINVAL;
	goto out;
    }
    free(str);

    
    CHECK(ret = krb5_ret_uint8(sp, &u8));
    CHECK((u8 == KCM_DUMP_VERSION) ? 0 : 1);
    CHECK(ret = krb5_ret_uint32(sp, &ccache_nextid));

    while(ret == 0) {
	int32_t session;
	krb5_data data;
	krb5_storage *inner;

	CHECK(ret = krb5_ret_stringz(sp, &str));

	kcm_log(10, "dump: reading a %s entry", str);

	if (strcmp(str, "end-dump") == 0) {
	    free(str);
	    break;
	}

	CHECK(ret = krb5_ret_int32(sp, &session));
	CHECK(ret = krb5_ret_data(sp, &data));
	
	inner = krb5_storage_from_data(&data);
	heim_assert(inner, "krb5_storage_from_data");

	if (!session_exists(session)) {
	    /* do nothing */
	} else if (strcmp(str, "krb5-cache") == 0) {
	    ret = parse_krb5_cache(context, inner);
	} else if (strcmp(str, "digest-cache") == 0) {
	    ret = kcm_parse_digest_one(context, inner);
	} else if (strcmp(str, "default-cache") == 0) {
	    ret = parse_default_one(context, inner);
	} else {
	    kcm_log(10, "dump: unknown type: %s", str);
	    ret = 0;
	}
	if (ret)
	    kcm_log(10, "dump: failed to unparse a %s cache with: %d", str, ret);
	free(str);
	krb5_storage_free(inner);
	krb5_data_free(&data);
    }
 out:
    krb5_storage_free(sp);
    if (ret)
	kcm_log(10, "dump: failed to read credential dump: %d", ret);
    return;
}

krb5_error_code
kcm_unparse_wrap(krb5_storage *sp, char *name, int32_t session, int (^wrapped)(krb5_storage *inner))
{
    krb5_error_code ret;
    krb5_storage *inner = krb5_storage_emem();
    krb5_data data;

    CHECK(ret = wrapped(inner));
    CHECK(ret = krb5_store_stringz(sp, name));
    CHECK(ret = krb5_store_int32(sp, session));
    CHECK(ret = krb5_storage_to_data(inner, &data));
    ret = krb5_store_data(sp, data);
    krb5_data_free(&data);

 out:
    if (ret)
	kcm_log(10, "dump: failed to add a %s", name);
    krb5_storage_free(inner);
    return ret;
}

void
kcm_unparse_cache_data(krb5_context context, krb5_data *data)
{
    __block time_t nextwakeup = 0;
    krb5_error_code ret;
    krb5_storage *sp;
    kcm_ccache c;

    krb5_data_zero(data);

    sp = krb5_storage_emem();
    if (sp == NULL)
	return;

    CHECK(ret = krb5_store_stringz(sp, "start-dump"));
    CHECK(ret = krb5_store_uint8(sp, KCM_DUMP_VERSION)); 
    CHECK(ret = krb5_store_uint32(sp, ccache_nextid));

    HEIMDAL_MUTEX_lock(&ccache_mutex);
    TAILQ_FOREACH_REVERSE(c, &ccache_head, ccache_head, members) {
	ret = kcm_unparse_wrap(sp, "krb5-cache", c->session, ^(krb5_storage *inner) {
		return unparse_krb5_cache(context, inner, c, &nextwakeup);
	    });
    }
    HEIMDAL_MUTEX_unlock(&ccache_mutex);
    CHECK(ret);

    /* add default cache */
    CHECK(ret = unparse_default_all(context, sp));

    /* add NTLM/SCRAM */
    CHECK(ret = kcm_unparse_digest_all(context, sp));

    CHECK(ret = krb5_store_stringz(sp, "end-dump"));

    if (nextwakeup) {
	int64_t next = nextwakeup - time(NULL);
	if (next > 0) {
	    vproc_swap_integer(NULL, VPROC_GSK_START_INTERVAL, &next, NULL);
	}
	kcm_log(1, "next wakup in: %d", (int)next);
    }

 out:
    if (ret == 0) {
	ret = krb5_storage_to_data(sp, data);
	if (ret)
	    kcm_log(1, "dump: failed to create credential data: %d", ret);
    }
    krb5_storage_free(sp);
}

static int have_uuid_master = 0;
static krb5_uuid uuid_master;
static const char *dumpfile = "/var/db/kcm-dump.bin";
static const char *keyfile = "/var/db/kcm-dump.uuid";

static krb5_error_code
kcm_load_key(krb5_context context)
{
    krb5_error_code ret;
    krb5_data enc;
    size_t len;
    void *p = NULL;

    if (have_uuid_master)
	return 0;

    ret = rk_undumpdata(keyfile, &p, &len);
    if (ret != 0)
	goto nokey;

    if (len != sizeof(uuid_master)) {
	free(p);
	goto nokey;
    }
    
    memcpy(uuid_master, p, sizeof(uuid_master));
    free(p);

    /* check if key is stale */
    ret = kcm_store_io(context, uuid_master, "", 0, &enc, true);
    if (ret)
	goto nokey;

    krb5_data_free(&enc);
    have_uuid_master = 1;

    return 0;
 nokey:

    ret = kcm_create_key(uuid_master);
    if (ret)
	return ret;
    rk_dumpdata(keyfile, uuid_master, sizeof(uuid_master));

    have_uuid_master = 1;

    return ret;
}

void
kcm_write_dump(krb5_context context)
{
    uuid_string_t uuidstr;
    krb5_data data, enc;
    krb5_error_code ret;
    
    ret = kcm_load_key(context);
    if (ret) {
	unlink(keyfile);
	unlink(dumpfile);
	return;
    }

    uuid_unparse(uuid_master, uuidstr);
    kcm_log(10, "dump: [masterkey] %s", uuidstr);

    kcm_unparse_cache_data(context, &data);
    if (data.length == 0)
	return;
    
    ret = kcm_store_io(context, uuid_master, data.data, data.length, &enc, true);
    krb5_data_free(&data);
    if (ret) {
	kcm_log(1, "dump: failed to encrypt credential data %d", ret);
	return;
    }
    
    rk_dumpdata(dumpfile, enc.data, enc.length);

    krb5_data_free(&enc);
}

void
kcm_read_dump(krb5_context context)
{
    uuid_string_t uuidstr;
    krb5_error_code ret;
    krb5_data data;
    size_t len;
    void *p;

    ret = kcm_load_key(context);
    if (ret)
	return;

    uuid_unparse(uuid_master, uuidstr);
    kcm_log(10, "load: [masterkey] %s", uuidstr);

    ret = rk_undumpdata(dumpfile, &p, &len);
    if (ret != 0 || len == 0)
	return;
    
    ret = kcm_store_io(kcm_context, uuid_master, p, len, &data, false);
    if (ret == 0) {
	kcm_parse_cache_data(kcm_context, &data);
	krb5_data_free(&data);
	have_uuid_master = 1;
    } else {
	unlink(dumpfile);
    }
    free(p);
}

