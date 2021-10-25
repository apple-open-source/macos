/*
 * Copyright (c) 1997-2004 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2009, 2020 Apple Inc. All rights reserved.
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

#include "krb5_locl.h"
#include "heimqueue.h"

struct mcred {
    krb5_creds cred;
    HEIM_TAILQ_ENTRY(mcred) next;
};

typedef struct krb5_mcache {
    char *name;  //not protected by mutex, never changes
    unsigned int refcnt;
    int dead;
    krb5_principal primary_principal;
    HEIM_TAILQ_HEAD(, mcred) creds;
    HEIM_TAILQ_ENTRY(krb5_mcache) next;
    time_t mtime;
    krb5_deltat kdc_offset;
    krb5_uuid uuid; //not protected by mutex, never changes
    HEIMDAL_MUTEX mutex; //mutex for protecting the mutable contents of this specific cache:" refcnt, dead, mtime, primary principal, creds, kdc_offset
} krb5_mcache;

static HEIMDAL_MUTEX mcc_mutex = HEIMDAL_MUTEX_INITIALIZER;  //mutex for adding, removing cred caches, using mcc_head
HEIM_TAILQ_HEAD(, krb5_mcache) mcc_head = HEIM_TAILQ_HEAD_INITIALIZER(mcc_head);

#define	MCACHE(X)	((krb5_mcache *)(X)->data.data)

#define MISDEAD(X)	((X)->dead)

static void
drop_content_locked(krb5_context context, krb5_mcache *m)
{
    struct mcred *l, *temp = NULL;
    HEIM_TAILQ_FOREACH_SAFE(l, &m->creds, next, temp) {
	krb5_free_cred_contents (context, &l->cred);
	HEIM_TAILQ_REMOVE(&m->creds, l, next);
	free(l);
    }
}

static const char*
mcc_get_name(krb5_context context,
	     krb5_ccache id)
{
    return MCACHE(id)->name;
}

static krb5_mcache * KRB5_CALLCONV
mcc_alloc_locked(const char *name)
{
    krb5_mcache *m, *m_c = NULL, *temp = NULL;
    int ret = 0;

    ALLOC(m, 1);
    if (m == NULL)
	return NULL;
    if (name == NULL) {
	ret = asprintf(&m->name, "%p", m);
    } else {
	m->name = strdup(name);
    }
    
    if(ret < 0 || m->name == NULL) {
	free(m);
	return NULL;
    }
    /* check for dups first */
    HEIM_TAILQ_FOREACH_SAFE(m_c, &mcc_head, next, temp) {
	if (strcmp(m->name, m_c->name) == 0) {
	    break;
	}
    }
    if (m_c) {
	free(m->name);
	free(m);
	return NULL;
    }
    
    m->dead = 0;
    m->refcnt = 1;
    m->primary_principal = NULL;
    HEIM_TAILQ_INIT(&m->creds);
    m->mtime = time(NULL);
    m->kdc_offset = 0;
    krb5_generate_random_block(m->uuid, sizeof(m->uuid));
    HEIMDAL_MUTEX_init(&m->mutex);

    HEIM_TAILQ_INSERT_HEAD(&mcc_head, m, next);
    return m;
}

static krb5_error_code KRB5_CALLCONV
mcc_resolve(krb5_context context, krb5_ccache *id, const char *res)
{
    krb5_mcache *m = NULL, *temp = NULL;

    HEIMDAL_MUTEX_lock(&mcc_mutex);
    HEIM_TAILQ_FOREACH_SAFE(m, &mcc_head, next, temp) {
	if (strcmp(m->name, res) == 0) {
	    break;
	}
    }

    if (m != NULL) {
	_krb5_debugx(context, 20, "mcc_resolve, found: %s\n", m->name);
	HEIMDAL_MUTEX_lock(&m->mutex);
	m->refcnt++;
	HEIMDAL_MUTEX_unlock(&m->mutex);
	HEIMDAL_MUTEX_unlock(&mcc_mutex);
	(*id)->data.data = m;
	(*id)->data.length = sizeof(*m);
	return 0;
    }

    m = mcc_alloc_locked(res);
    _krb5_debugx(context, 20, "mcc_resolve, allocated: %s\n", m->name);
    HEIMDAL_MUTEX_unlock(&mcc_mutex);
    if (m == NULL) {
	
	krb5_set_error_message(context, KRB5_CC_NOMEM,
			       N_("malloc: out of memory", ""));
	return KRB5_CC_NOMEM;
    }

    (*id)->data.data = m;
    (*id)->data.length = sizeof(*m);

    return 0;
}


static krb5_error_code KRB5_CALLCONV
mcc_gen_new(krb5_context context, krb5_ccache *id)
{
    krb5_mcache *m;
    HEIMDAL_MUTEX_lock(&mcc_mutex);
    m = mcc_alloc_locked(NULL);
    _krb5_debugx(context, 20, "mcc_gen_new: %s\n", m->name);
    HEIMDAL_MUTEX_unlock(&mcc_mutex);
    if (m == NULL) {
	krb5_set_error_message(context, KRB5_CC_NOMEM,
			       N_("malloc: out of memory", ""));
	return KRB5_CC_NOMEM;
    }

    (*id)->data.data = m;
    (*id)->data.length = sizeof(*m);

    return 0;
}

static krb5_error_code KRB5_CALLCONV
mcc_initialize(krb5_context context,
	       krb5_ccache id,
	       krb5_principal primary_principal)
{
    krb5_mcache *m = MCACHE(id);
    krb5_error_code ret;
    krb5_principal p;
    HEIMDAL_MUTEX_lock(&mcc_mutex);
    HEIMDAL_MUTEX_lock(&m->mutex);
    _krb5_debugx(context, 20, "mcc_initialize: %s\n", m->name);
    bool wasDead = MISDEAD(m);
    m->dead = 0;
    m->mtime = time(NULL);
    ret = krb5_copy_principal(context, primary_principal, &p);
    if (ret) {
	HEIMDAL_MUTEX_unlock(&m->mutex);
	HEIMDAL_MUTEX_unlock(&mcc_mutex);
	return ret;
    }

    if (m->primary_principal) {
	krb5_free_principal(context, m->primary_principal);
    }
    m->primary_principal = p;

    drop_content_locked(context, m);

    //if the cache was previously dead, add it back to the cache list.
    if (wasDead) {
	_krb5_debugx(context, 20, "mcc_initialize was dead: %s\n", m->name);

	krb5_mcache *matching_cache = NULL, *temp = NULL;
	/* check for dups first */
	HEIM_TAILQ_FOREACH_SAFE(matching_cache, &mcc_head, next, temp) {
	    if (strcmp(m->name, matching_cache->name) == 0) {
		break;
	    }
	}
	//if there is a dupe, clear this cache and return an error
	if (matching_cache) {
	    if (m->primary_principal != NULL) {
		krb5_free_principal (context, m->primary_principal);
		m->primary_principal = NULL;
	    }
	    m->dead = 1;
	    HEIMDAL_MUTEX_unlock(&m->mutex);
	    HEIMDAL_MUTEX_unlock(&mcc_mutex);
	    return EEXIST;
	}

	HEIM_TAILQ_INSERT_HEAD(&mcc_head, m, next);
    }
    HEIMDAL_MUTEX_unlock(&m->mutex);
    HEIMDAL_MUTEX_unlock(&mcc_mutex);
    
    return 0;
}

static int
mcc_close_internal(krb5_context context,
		   krb5_mcache *m)
{
    HEIMDAL_MUTEX_lock(&mcc_mutex);  //this lock is in case the cache is dead.  if so, we free it before it can be resolved or allocated again.
    HEIMDAL_MUTEX_lock(&m->mutex);
    _krb5_debugx(context, 20, "mcc_close_internal: %s, %d\n", m->name, m->refcnt);
    if (--m->refcnt == 0 && MISDEAD(m)) {
	_krb5_debugx(context, 20, "mcc_close_internal, dead: %s\n", m->name);
	HEIMDAL_MUTEX_unlock(&m->mutex);
	free(m->name);
	m->name = NULL;
	HEIMDAL_MUTEX_destroy(&m->mutex);
	HEIMDAL_MUTEX_unlock(&mcc_mutex);
	return 1;
    }
    HEIMDAL_MUTEX_unlock(&m->mutex);
    HEIMDAL_MUTEX_unlock(&mcc_mutex);
    return 0;
}

static krb5_error_code KRB5_CALLCONV
mcc_close(krb5_context context,
	  krb5_ccache id)
{
    krb5_mcache *m = MCACHE(id);
    if (mcc_close_internal(context, m)) {
	krb5_data_free(&id->data);
    }
    return 0;
}

static void KRB5_CALLCONV
mcc_destroy_internal_locked(krb5_context context,
		     krb5_mcache *m)
{
    HEIMDAL_MUTEX_lock(&m->mutex);
    _krb5_debugx(context, 20, "mcc_destroy_internal_locked: %s\n", m->name);
    heim_assert(m->refcnt != 0, "mcc_destroy: refcnt already 0");
    if (!MISDEAD(m)) {
	/* if this is an active mcache, remove it from the linked
	 list, and free all data */
	HEIM_TAILQ_REMOVE(&mcc_head, m, next);

	if (m->primary_principal != NULL) {
	    krb5_free_principal (context, m->primary_principal);
	    m->primary_principal = NULL;
	}
	m->dead = 1;

	drop_content_locked(context, m);
    }
    HEIMDAL_MUTEX_unlock(&m->mutex);
}

static krb5_error_code KRB5_CALLCONV
mcc_destroy(krb5_context context,
	    krb5_ccache id)
{
    krb5_mcache *m = MCACHE(id);
    _krb5_debugx(context, 20, "mcc_destroy: %s\n", m->name);
    HEIMDAL_MUTEX_lock(&mcc_mutex);
    mcc_destroy_internal_locked(context, m);
    HEIMDAL_MUTEX_unlock(&mcc_mutex);
    return 0;
}

static krb5_error_code KRB5_CALLCONV
mcc_store_cred(krb5_context context,
	       krb5_ccache id,
	       krb5_creds *creds)
{
    krb5_mcache *m = MCACHE(id);
    krb5_error_code ret;
    struct mcred *l;

    HEIMDAL_MUTEX_lock(&m->mutex);
    if (MISDEAD(m)) {
	HEIMDAL_MUTEX_unlock(&m->mutex);
	return ENOENT;
    }

    l = calloc(1, sizeof(*l));
    if (l == NULL) {
	HEIMDAL_MUTEX_unlock(&m->mutex);
	krb5_set_error_message(context, KRB5_CC_NOMEM,
			       N_("malloc: out of memory", ""));
	return KRB5_CC_NOMEM;
    }
    HEIM_TAILQ_INSERT_TAIL(&m->creds, l, next);
    memset (&l->cred, 0, sizeof(l->cred));
    ret = krb5_copy_creds_contents (context, creds, &l->cred);
    if (ret) {
	HEIM_TAILQ_REMOVE(&m->creds, l, next);
	free (l);
	HEIMDAL_MUTEX_unlock(&m->mutex);
	return ret;
    }
    m->mtime = time(NULL);
    HEIMDAL_MUTEX_unlock(&m->mutex);
    return 0;
}

static krb5_error_code KRB5_CALLCONV
mcc_get_principal(krb5_context context,
		  krb5_ccache id,
		  krb5_principal *principal)
{
    krb5_mcache *m = MCACHE(id);
    krb5_error_code ret;
    
    HEIMDAL_MUTEX_lock(&m->mutex);
    _krb5_debugx(context, 20, "mcc_get_principal: %p\n", m);
    if (MISDEAD(m) || m->primary_principal == NULL) {
	HEIMDAL_MUTEX_unlock(&m->mutex);
	return ENOENT;
    }
    ret = krb5_copy_principal (context,
				m->primary_principal,
				principal);
    HEIMDAL_MUTEX_unlock(&m->mutex);
    return ret;
}


struct mcc_cursor {
    HEIM_TAILQ_HEAD(, mcred) creds;
};

static void
free_cred_cursor(krb5_context context, struct mcc_cursor *l)
{
    struct mcred *p, *temp = NULL;
    HEIM_TAILQ_FOREACH_SAFE(p, &l->creds, next, temp) {
	krb5_free_cred_contents (context, &p->cred);
	HEIM_TAILQ_REMOVE(&l->creds, p, next);
	free(p);
    }
}

static krb5_error_code KRB5_CALLCONV
mcc_get_first (krb5_context context,
	       krb5_ccache id,
	       krb5_cc_cursor *cursor)
{
    krb5_mcache *m = MCACHE(id);
    krb5_error_code ret;
    
    HEIMDAL_MUTEX_lock(&m->mutex);
    if (MISDEAD(m)) {
	HEIMDAL_MUTEX_unlock(&m->mutex);
	return ENOENT;
    }
    
    struct mcc_cursor *c = calloc(1, sizeof(*c));
    if (c == NULL) {
	HEIMDAL_MUTEX_unlock(&m->mutex);
	krb5_set_error_message(context, KRB5_CC_NOMEM,
			       N_("malloc: out of memory", ""));
	return KRB5_CC_NOMEM;
    }
    HEIM_TAILQ_INIT(&c->creds);
    struct mcred *i, *temp = NULL;
    HEIM_TAILQ_FOREACH_SAFE(i, &m->creds, next, temp) {
	struct mcred *entry = NULL;
	entry = calloc(1 ,sizeof(*entry));
	if (entry == NULL) {
	    krb5_set_error_message(context, KRB5_CC_NOMEM,
				   N_("malloc: out of memory", ""));
	    
	    free_cred_cursor(context, c);
	    free(c);
	    HEIMDAL_MUTEX_unlock(&m->mutex);
	    
	    return KRB5_CC_NOMEM;
	}
	
	ret = krb5_copy_creds_contents(context,
				       &i->cred,
				       &entry->cred);
	if (ret) {
	    free(entry);
	    HEIMDAL_MUTEX_unlock(&m->mutex);
	    return ret;
	}

	HEIM_TAILQ_INSERT_TAIL(&c->creds, entry, next);
    }
    HEIMDAL_MUTEX_unlock(&m->mutex);
    *cursor = c;
    return 0;
}

static krb5_error_code KRB5_CALLCONV
mcc_get_next (krb5_context context,
	      krb5_ccache id,
	      krb5_cc_cursor *cursor,
	      krb5_creds *creds)
{
    krb5_mcache *m = MCACHE(id);
    struct mcc_cursor *c = *cursor;

    HEIMDAL_MUTEX_lock(&m->mutex);
    if (MISDEAD(m)) {
	HEIMDAL_MUTEX_unlock(&m->mutex);
	return ENOENT;
    }
    HEIMDAL_MUTEX_unlock(&m->mutex);
    
    //transfer the copy of the credential to the caller.  The memory allocated should be freed by the caller.
    struct mcred *current = HEIM_TAILQ_FIRST(&c->creds);
    if (current != NULL) {
	*creds = current->cred;
	//remove the current entry and cleanup
	HEIM_TAILQ_REMOVE(&c->creds, current, next);
	free(current);
	return 0;
    }

    return KRB5_CC_END;
}

static krb5_error_code KRB5_CALLCONV
mcc_end_get (krb5_context context,
	     krb5_ccache id,
	     krb5_cc_cursor *cursor)
{
    struct mcc_cursor *l = *cursor;
    *cursor = NULL;
    
    free_cred_cursor(context, l);
    
    free(l);
    return 0;
}

static krb5_error_code KRB5_CALLCONV
mcc_remove_cred(krb5_context context,
		 krb5_ccache id,
		 krb5_flags which,
		 krb5_creds *mcreds)
{
    krb5_mcache *m = MCACHE(id);
    HEIMDAL_MUTEX_lock(&m->mutex);
    struct mcred *i, *temp = NULL;
    HEIM_TAILQ_FOREACH_SAFE(i, &m->creds, next, temp) {
	if(krb5_compare_creds(context, which, mcreds, &i->cred)) {
	    krb5_free_cred_contents(context, &i->cred);
	    HEIM_TAILQ_REMOVE(&m->creds, i, next);
	    free(i);
	    m->mtime = time(NULL);
	}
    }
    HEIMDAL_MUTEX_unlock(&m->mutex);
    return 0;
}

static krb5_error_code KRB5_CALLCONV
mcc_set_flags(krb5_context context,
	      krb5_ccache id,
	      krb5_flags flags)
{
    return 0; /* XXX */
}

struct mcache_entry {
    krb5_mcache *cache;			 //points to the actual entry, even if destroyed.  This holds a ref count on the cache to prevent freeing it.
    HEIM_TAILQ_ENTRY(mcache_entry) next; //points to the next cache.  this is a copy of the cache list when the cursor is created
};

struct mcache_cursor {
    HEIM_TAILQ_HEAD(, mcache_entry) caches;
};

static void
mcc_free_cache_cursor(krb5_context context, krb5_cc_cursor cursor)
{
    struct mcache_cursor *iter = cursor;
    struct mcache_entry *i = NULL;

    //free any remaining entries
    while(!HEIM_TAILQ_EMPTY(&iter->caches)) {
	i = HEIM_TAILQ_FIRST(&iter->caches);
	_krb5_debugx(context, 20, "mcc_free_cache_cursor, closing cache: %s\n", i->cache->name);
	if (mcc_close_internal(context, i->cache)) {
	    krb5_mcache *c = i->cache;
	    _krb5_debugx(context, 20, "mcc_free_cache_cursor, needs to be freed: %s\n", c->name);
	    free(c);  //we cant call krb5_data_free here because we only have the mcache and not the ccache.
	}
	HEIM_TAILQ_REMOVE(&iter->caches, i, next);
	free(i);  //free the mcache_entry
    }
}

static krb5_error_code KRB5_CALLCONV
mcc_get_cache_first(krb5_context context, krb5_cc_cursor *cursor)
{
    krb5_mcache *i = NULL, *temp = NULL;

    //create cursor struct
    struct mcache_cursor *iter = calloc(1, sizeof(*iter));
    if (iter == NULL) {
	krb5_set_error_message(context, KRB5_CC_NOMEM,
			       N_("malloc: out of memory", ""));
	return KRB5_CC_NOMEM;
    }
    HEIM_TAILQ_INIT(&iter->caches);

    HEIMDAL_MUTEX_lock(&mcc_mutex);
    //make a snapshot of the current set of credential caches to make it thread safe
    HEIM_TAILQ_FOREACH_SAFE(i, &mcc_head, next, temp) {
	struct mcache_entry *entry = calloc(1, sizeof(*entry));
	if (entry == NULL) {
	    HEIMDAL_MUTEX_unlock(&mcc_mutex);
	    krb5_set_error_message(context, ENOMEM,
				   N_("malloc: out of memory", ""));
	    
	    mcc_free_cache_cursor(context, iter);
	    
	    return ENOMEM;
	}
	
	entry->cache = i;
	HEIM_TAILQ_INSERT_TAIL(&iter->caches, entry, next);
	_krb5_debugx(context, 20, "mcc_get_cache_first, adding to cursor cache: %s\n", i->name);
	//increment the reference count on each cache
	if (entry->cache) {
	    HEIMDAL_MUTEX_lock(&(entry->cache->mutex));
	    if (MISDEAD(entry->cache)) {
		HEIMDAL_MUTEX_unlock(&(entry->cache->mutex));
		entry->cache = NULL;
	    } else {
		entry->cache->refcnt++;
		HEIMDAL_MUTEX_unlock(&(entry->cache->mutex));
	    }
	}
	
    }
    HEIMDAL_MUTEX_unlock(&mcc_mutex);
    
    *cursor = iter;
    return 0;
}

static krb5_error_code KRB5_CALLCONV
mcc_get_cache_next(krb5_context context, krb5_cc_cursor cursor, krb5_ccache *id)
{
    struct mcache_cursor *iter = cursor;
    krb5_error_code ret;
    krb5_mcache *m = NULL;

    if (iter == NULL) {
	return KRB5_CC_END;
    }
    
    do {
	// set the cache to return
	struct mcache_entry *current = HEIM_TAILQ_FIRST(&iter->caches);
	if (current) {
	    HEIM_TAILQ_REMOVE(&iter->caches, current, next);
	    m = current->cache;
	    free(current);
	    HEIMDAL_MUTEX_lock(&m->mutex);
	    if (MISDEAD(m)) {
		HEIMDAL_MUTEX_unlock(&m->mutex);
		_krb5_debugx(context, 20, "mcc_get_cache_next, not returning, already dead: %s\n", m->name);
		if (mcc_close_internal(context, m)) {
		    krb5_mcache *c = m;
		    _krb5_debugx(context, 20, "mcc_get_cache_next, needs to be freed: %s\n", c->name);
		    free(m);  //we cant call krb5_data_free here because we only have the mcache and not the ccache.
		}
		m = NULL;
	    } else {
		HEIMDAL_MUTEX_unlock(&m->mutex);
		_krb5_debugx(context, 20, "mcc_get_cache_next, returning: %s\n", m->name);
	    }
	}

    } while (m == NULL && !HEIM_TAILQ_EMPTY(&iter->caches));  //if there isnt a cache to return, then move to the next
    
    if (m == NULL) {
	return KRB5_CC_END;
    }
    
    //the ref count that was incremented when the cursor was created is transferred to the caller to close.
    ret = _krb5_cc_allocate(context, &krb5_mcc_ops, id);
    if (ret) {
	return ret;
    }
    (*id)->data.data = m;
    (*id)->data.length = sizeof(*m);
    
    return 0;
}



static krb5_error_code KRB5_CALLCONV
mcc_end_cache_get(krb5_context context, krb5_cc_cursor cursor)
{
    mcc_free_cache_cursor(context, cursor);
    free(cursor);
    return 0;
}

static krb5_error_code KRB5_CALLCONV
mcc_move(krb5_context context, krb5_ccache from, krb5_ccache to)
{
    krb5_mcache *mfrom = MCACHE(from), *mto = MCACHE(to);
    struct mcred *creds, *temp;
    krb5_principal principal;
    _krb5_debugx(context, 20, "mcc_move, from: %s to: %s\n", mfrom->name, mto->name);

    HEIMDAL_MUTEX_lock(&mcc_mutex);
    /* drop the from cache from the linked list to avoid lookups */
    HEIM_TAILQ_REMOVE(&mcc_head, mfrom, next);

    HEIMDAL_MUTEX_lock(&mfrom->mutex);
    HEIMDAL_MUTEX_lock(&mto->mutex);
    /* swap creds */

    //remove all the destination creds
    HEIM_TAILQ_FOREACH_SAFE(creds, &mto->creds, next, temp) {
	krb5_free_cred_contents (context, &creds->cred);
	HEIM_TAILQ_REMOVE(&mto->creds, creds, next);
	free(creds);
    }
    //more the source creds to the destination
    HEIM_TAILQ_FOREACH_SAFE(creds, &mfrom->creds, next, temp) {
	HEIM_TAILQ_REMOVE(&mfrom->creds, creds, next);
	HEIM_TAILQ_INSERT_TAIL(&mto->creds, creds, next);
    }

    /* swap principal */
    principal = mto->primary_principal;
    mto->primary_principal = mfrom->primary_principal;
    mfrom->primary_principal = principal;

    mto->mtime = mfrom->mtime = time(NULL);
    HEIMDAL_MUTEX_unlock(&mfrom->mutex);
    HEIMDAL_MUTEX_unlock(&mto->mutex);
    
    mcc_destroy_internal_locked(context, mfrom);
    HEIMDAL_MUTEX_unlock(&mcc_mutex);
    if (mcc_close_internal(context, mfrom)) {
	krb5_data_free(&from->data);
	_krb5_debugx(context, 20, "mcc_move, from freed\n");
    } else {
	_krb5_debugx(context, 20, "mcc_move, from not freed\n");
    }

    return 0;
}

static krb5_error_code KRB5_CALLCONV
mcc_default_name(krb5_context context, char **str)
{
    *str = strdup("MEMORY:");
    if (*str == NULL) {
	krb5_set_error_message(context, ENOMEM,
			       N_("malloc: out of memory", ""));
	return ENOMEM;
    }
    return 0;
}

static krb5_error_code KRB5_CALLCONV
mcc_lastchange(krb5_context context, krb5_ccache id, krb5_timestamp *mtime)
{
    krb5_mcache *m = MCACHE(id);
    HEIMDAL_MUTEX_lock(&m->mutex);
    *mtime = m->mtime;
    HEIMDAL_MUTEX_unlock(&m->mutex);
    return 0;
}

static krb5_error_code KRB5_CALLCONV
mcc_set_kdc_offset(krb5_context context, krb5_ccache id, krb5_deltat kdc_offset)
{
    krb5_mcache *m = MCACHE(id);
    HEIMDAL_MUTEX_lock(&m->mutex);
    m->kdc_offset = kdc_offset;
    HEIMDAL_MUTEX_unlock(&m->mutex);
    return 0;
}

static krb5_error_code KRB5_CALLCONV
mcc_get_kdc_offset(krb5_context context, krb5_ccache id, krb5_deltat *kdc_offset)
{
    krb5_mcache *m = MCACHE(id);
    HEIMDAL_MUTEX_lock(&m->mutex);
    *kdc_offset = m->kdc_offset;
    HEIMDAL_MUTEX_unlock(&m->mutex);
    return 0;
}

static krb5_error_code
mcc_get_uuid(krb5_context context, krb5_ccache id, krb5_uuid uuid)
{
    krb5_mcache *m = MCACHE(id);
    memcpy(uuid, m->uuid, sizeof(m->uuid));
    return 0;
}

static krb5_error_code
mcc_resolve_by_uuid(krb5_context context, krb5_ccache id, krb5_uuid uuid)
{
    krb5_mcache *m = NULL, *temp = NULL;
    
    HEIMDAL_MUTEX_lock(&mcc_mutex);

    HEIM_TAILQ_FOREACH_SAFE(m, &mcc_head, next, temp) {
	if (memcmp(m->uuid, uuid, sizeof(m->uuid)) == 0)
	    break;
    }
    
    if (m == NULL) {
	HEIMDAL_MUTEX_unlock(&mcc_mutex);
	krb5_clear_error_message(context);
	return KRB5_CC_END;
    }
    
    HEIMDAL_MUTEX_lock(&m->mutex);
    m->refcnt++;
    HEIMDAL_MUTEX_unlock(&m->mutex);
    HEIMDAL_MUTEX_unlock(&mcc_mutex);
    id->data.data = m;
    id->data.length = sizeof(*m);
    
    return 0;
}


/**
 * Variable containing the MEMORY based credential cache implemention.
 *
 * @ingroup krb5_ccache
 */

KRB5_LIB_VARIABLE const krb5_cc_ops krb5_mcc_ops = {
    KRB5_CC_OPS_VERSION,
    "MEMORY",
    mcc_get_name,
    mcc_resolve,
    mcc_gen_new,
    mcc_initialize,
    mcc_destroy,
    mcc_close,
    mcc_store_cred,
    NULL, /* mcc_retrieve */
    mcc_get_principal,
    mcc_get_first,
    mcc_get_next,
    mcc_end_get,
    mcc_remove_cred,
    mcc_set_flags,
    NULL,
    mcc_get_cache_first,
    mcc_get_cache_next,
    mcc_end_cache_get,
    mcc_move,
    mcc_default_name,
    NULL,
    mcc_lastchange,
    mcc_set_kdc_offset,
    mcc_get_kdc_offset,
    NULL,
    NULL,
    mcc_get_uuid,
    mcc_resolve_by_uuid
};
