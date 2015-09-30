/*
 * Copyright (c) 1997-2001 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
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
#include <vis.h>
#include <corecrypto/ccdigest.h>
#include <corecrypto/ccsha2.h>
#include "heimqueue.h"

typedef struct krb5_rc_type {
    const char *type;
    size_t size;
    krb5_error_code (*rc_resolve)(krb5_context, const char *, krb5_rcache);
    krb5_error_code (*rc_init)(krb5_context, krb5_deltat, krb5_rcache);
    krb5_error_code (*rc_get_lifespan)(krb5_context, krb5_rcache, krb5_deltat *);
    krb5_error_code (*rc_store)(krb5_context, krb5_rcache, krb5_donot_replay *);
    krb5_error_code (*rc_expunge)(krb5_context, krb5_rcache);
    krb5_error_code (*rc_destroy)(krb5_context, krb5_rcache);
    void            (*rc_release)(krb5_context, krb5_rcache);
} *krb5_rc_type;

struct krb5_rcache_data {
    krb5_rc_type type;
    char *name;
    char typedata[];
};

#define RCACHE_CTX(_rcache) ((void *)(&(_rcache)->typedata[0])

struct mem_entry{
    krb5_timestamp time;
    uint8_t digest[CCSHA256_OUTPUT_SIZE];
    HEIM_TAILQ_ENTRY(mem_entry) entries;
};

static struct {
    krb5_timestamp time;
    HEIM_TAILQ_HEAD(, mem_entry) list;
    HEIMDAL_MUTEX mutex;
} memhead = {
    5 * 60,
    HEIM_TAILQ_HEAD_INITIALIZER(memhead.list),
    HEIMDAL_MUTEX_INITIALIZER,
};

static void
checksum_entry(krb5_donot_replay *auth, uint8_t digest[CCSHA256_OUTPUT_SIZE])
{
    const struct ccdigest_info *di = ccsha256_di();
    ccdigest_di_decl(di, ctx);
    unsigned n;

    ccdigest_init(di, ctx);
    ccdigest_update(di, ctx, strlen(auth->crealm), auth->crealm);
    for(n = 0; n < auth->cname.name_string.len; n++) {
	ccdigest_update(di, ctx, strlen(auth->cname.name_string.val[n]), auth->cname.name_string.val[n]);
    }

    ccdigest_update(di, ctx, sizeof(auth->ctime), &auth->ctime);
    ccdigest_update(di, ctx, sizeof(auth->cusec), &auth->cusec);

    ccdigest_final(di, ctx, digest);
}

/*
 * Memory backend
 */

static krb5_error_code
mem_init(krb5_context context, krb5_deltat time, krb5_rcache id)
{

    (void)id;
    HEIMDAL_MUTEX_lock(&memhead.mutex);
    if (time != memhead.time)
	memhead.time = time;
    HEIMDAL_MUTEX_unlock(&memhead.mutex);
    return 0;
}

static krb5_error_code
mem_resolve(krb5_context context, const char *name, krb5_rcache id)
{
    return 0;
}

static krb5_error_code
mem_get_lifespan(krb5_context context, krb5_rcache id, krb5_deltat *lifespan)
{
    HEIMDAL_MUTEX_lock(&memhead.mutex);
    *lifespan = memhead.time;
    HEIMDAL_MUTEX_unlock(&memhead.mutex);
    return 0;
}

static krb5_error_code
mem_store(krb5_context context, krb5_rcache id, krb5_donot_replay *replay_data)
{
    struct mem_entry *entry, *e = NULL, *temp = NULL;
    time_t expire;

    entry = malloc(sizeof(*entry));
    if (entry == NULL)
	return krb5_enomem(context);
    entry->time = replay_data->ctime;
    checksum_entry(replay_data, entry->digest);

    expire = time(NULL) - memhead.time;

    HEIMDAL_MUTEX_lock(&memhead.mutex);
    /* first check for dup (and clean out expired entries) */
    HEIM_TAILQ_FOREACH_SAFE(e, &memhead.list, entries, temp) {
	if (e->time == replay_data->ctime && memcmp(e->digest, entry->digest, sizeof(entry->digest)) == 0) {
	    HEIMDAL_MUTEX_unlock(&memhead.mutex);
	    krb5_set_error_message(context, KRB5_RC_REPLAY,
				   N_("replay detected", ""));
	    free(entry);
	    return KRB5_RC_REPLAY;
	}
	if (e->time < expire)
	    HEIM_TAILQ_REMOVE(&memhead.list, e, entries);
    }

    HEIM_TAILQ_INSERT_HEAD(&memhead.list, entry, entries);
    HEIMDAL_MUTEX_unlock(&memhead.mutex);

    return 0;
}

static krb5_error_code
mem_expunge(krb5_context context, krb5_rcache id)
{
    struct mem_entry *e, *temp;
    time_t expire;

    HEIMDAL_MUTEX_lock(&memhead.mutex);
    expire = time(NULL) - memhead.time;

    HEIM_TAILQ_FOREACH_SAFE(e, &memhead.list, entries, temp) {
	if (e->time < expire)
	    HEIM_TAILQ_REMOVE(&memhead.list, e, entries);
    }
    HEIMDAL_MUTEX_unlock(&memhead.mutex);
    return 0;
}

static krb5_error_code
mem_destroy(krb5_context context, krb5_rcache id)
{
    struct mem_entry *e;

    HEIMDAL_MUTEX_lock(&memhead.mutex);

    while ((e = HEIM_TAILQ_FIRST(&memhead.list)) != NULL) {
	HEIM_TAILQ_REMOVE(&memhead.list, e, entries);
    }

    HEIMDAL_MUTEX_unlock(&memhead.mutex);
    return 0;
}

static void
mem_release(krb5_context context, krb5_rcache id)
{
}

/*
 * File backend
 */

struct rc_entry{
    time_t stamp;
    unsigned char data[CCSHA256_OUTPUT_SIZE];
};

static krb5_error_code
file_init(krb5_context context, krb5_deltat auth_lifespan, krb5_rcache id)
{
    FILE *f = fopen(id->name, "w");
    struct rc_entry tmp;
    int ret;

    if(f == NULL) {
	char buf[128];
	ret = errno;
	rk_strerror_r(ret, buf, sizeof(buf));
	krb5_set_error_message(context, ret, "open(%s): %s", id->name, buf);
	return ret;
    }
    tmp.stamp = auth_lifespan;
    fwrite(&tmp, 1, sizeof(tmp), f);
    fclose(f);

    return 0;
}

static krb5_error_code
file_resolve(krb5_context context, const char *name, krb5_rcache id)
{
    return 0;
}


static krb5_error_code
file_get_lifespan(krb5_context context, krb5_rcache id, krb5_deltat *auth_lifespan)
{
    FILE *f = fopen(id->name, "r");
    size_t r;
    struct rc_entry ent;
    r = fread(&ent, sizeof(ent), 1, f);
    fclose(f);
    if(r){
	*auth_lifespan = ent.stamp;
	return 0;
    }
    krb5_clear_error_message (context);
    return KRB5_RC_IO_UNKNOWN;
}

static krb5_error_code
file_store(krb5_context context, krb5_rcache id, krb5_donot_replay *replay_data)
{
    struct rc_entry ent, tmp;
    time_t t;
    FILE *f;
    int ret;

    ent.stamp = time(NULL);
    checksum_entry(replay_data, ent.data);
    f = fopen(id->name, "r");
    if(f == NULL) {
	char buf[128];
	ret = errno;
	rk_strerror_r(ret, buf, sizeof(buf));
	krb5_set_error_message(context, ret, "open(%s): %s", id->name, buf);
	return ret;
    }
    rk_cloexec_file(f);
    fread(&tmp, sizeof(ent), 1, f);
    t = ent.stamp - tmp.stamp;
    while(fread(&tmp, sizeof(ent), 1, f)){
	if(tmp.stamp < t)
	    continue;
	if(memcmp(tmp.data, ent.data, sizeof(ent.data)) == 0){
	    fclose(f);
	    krb5_clear_error_message (context);
	    return KRB5_RC_REPLAY;
	}
    }
    if(ferror(f)){
	char buf[128];
	ret = errno;
	fclose(f);
	rk_strerror_r(ret, buf, sizeof(buf));
	krb5_set_error_message(context, ret, "%s: %s",
			       id->name, buf);
	return ret;
    }
    fclose(f);
    f = fopen(id->name, "a");
    if(f == NULL) {
	char buf[128];
	rk_strerror_r(errno, buf, sizeof(buf));
	krb5_set_error_message(context, KRB5_RC_IO_UNKNOWN,
			       "open(%s): %s", id->name, buf);
	return KRB5_RC_IO_UNKNOWN;
    }
    fwrite(&ent, 1, sizeof(ent), f);
    fclose(f);
    return 0;
}

static krb5_error_code
file_expunge(krb5_context context, krb5_rcache id)
{
    return 0;
}

static krb5_error_code
file_destroy(krb5_context context, krb5_rcache id)
{
    int ret;

    if(remove(id->name) < 0) {
	char buf[128];
	ret = errno;
	rk_strerror_r(ret, buf, sizeof(buf));
	krb5_set_error_message(context, ret, "remove(%s): %s", id->name, buf);
	return ret;
    }
    return 0;
}

static void
file_release(krb5_context context, krb5_rcache id)
{
}

static struct krb5_rc_type rc_types[] = {
    {
	"FILE",
	0,
	file_resolve,
	file_init,
	file_get_lifespan,
	file_store,
	file_expunge,
	file_destroy,
	file_release,
    }, {
	"MEMORY",
	0,
	mem_resolve,
	mem_init,
	mem_get_lifespan,
	mem_store,
	mem_expunge,
	mem_destroy,
	mem_release,
    }
};

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_rc_resolve(krb5_context context,
		krb5_rcache id,
		const char *name)
{
    if (id->name != NULL)
	krb5_abort(context, EINVAL, "called krb5_rc_resolve more then once");
    id->name = strdup(name);
    return id->type->rc_resolve(context, name, id);
}

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_rc_resolve_type(krb5_context context,
		     krb5_rcache *id,
		     const char *type)
{
    krb5_rc_type t = NULL;
    unsigned n;

    *id = NULL;

    for (n = 0; n < sizeof(rc_types)/sizeof(rc_types[0]); n++) {
	size_t len = strlen(rc_types[n].type);

	if (strncmp(type, rc_types[n].type, len) == 0 && (type[len] == '\0' || type[len] == ':')) {
	    t = &rc_types[n];
	    break;
	}
    }
    if (t == NULL) {
	krb5_set_error_message (context, KRB5_RC_TYPE_NOTFOUND,
				N_("replay cache type %s not supported", ""),
				type);
	return KRB5_RC_TYPE_NOTFOUND;
    }

    *id = calloc(1, sizeof(**id) + t->size);
    if (*id == NULL)
	return krb5_enomem(context);
    
    (*id)->type = t;

    return 0;
}

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_rc_resolve_full(krb5_context context,
		     krb5_rcache *id,
		     const char *string_name)
{
    krb5_error_code ret;
    size_t len;

    *id = NULL;

    ret = krb5_rc_resolve_type(context, id, string_name);
    if(ret)
	return ret;

    len = strlen((*id)->type->type);
    if (string_name[len] != ':') {
	krb5_set_error_message (context, KRB5_RC_TYPE_NOTFOUND,
				N_("replay have bad name: %s", ""),
				string_name);
	return KRB5_RC_TYPE_NOTFOUND;
    }	

    ret = krb5_rc_resolve(context, *id, string_name + len + 1);
    if (ret) {
	krb5_rc_close(context, *id);
	*id = NULL;
    }
    return ret;
}

KRB5_LIB_FUNCTION const char* KRB5_LIB_CALL
krb5_rc_default_name(krb5_context context)
{
    return "MEMORY:";
}

KRB5_LIB_FUNCTION const char* KRB5_LIB_CALL
krb5_rc_default_type(krb5_context context)
{
    return "MEMORY";
}

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_rc_default(krb5_context context,
		krb5_rcache *id)
{
    return krb5_rc_resolve_full(context, id, krb5_rc_default_name(context));
}

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_rc_initialize(krb5_context context,
		   krb5_rcache id,
		   krb5_deltat auth_lifespan)
{
    return id->type->rc_init(context, auth_lifespan, id);
}

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_rc_recover(krb5_context context,
		krb5_rcache id)
{
    return 0;
}

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_rc_destroy(krb5_context context,
		krb5_rcache id)
{
    krb5_error_code ret;
    ret = id->type->rc_destroy(context, id);
    if (ret)
	return ret;
    return krb5_rc_close(context, id);
}

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_rc_close(krb5_context context,
	      krb5_rcache id)
{
    id->type->rc_release(context, id);
    if (id->name)
        free(id->name);
    free(id);
    return 0;
}

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_rc_store(krb5_context context,
	      krb5_rcache id,
	      krb5_donot_replay *rep)
{
    return id->type->rc_store(context, id, rep);
}

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_rc_expunge(krb5_context context,
		krb5_rcache id)
{
    return id->type->rc_expunge(context, id);
}

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_rc_get_lifespan(krb5_context context,
		     krb5_rcache id,
		     krb5_deltat *auth_lifespan)
{
    return id->type->rc_get_lifespan(context, id, auth_lifespan);
}

KRB5_LIB_FUNCTION const char* KRB5_LIB_CALL
krb5_rc_get_name(krb5_context context,
		 krb5_rcache id)
{
    return id->name;
}

KRB5_LIB_FUNCTION const char* KRB5_LIB_CALL
krb5_rc_get_type(krb5_context context,
		 krb5_rcache id)
{
    return id->type->type;
}

KRB5_LIB_FUNCTION krb5_error_code KRB5_LIB_CALL
krb5_get_server_rcache(krb5_context context,
		       const krb5_data *piece,
		       krb5_rcache *id)
{
#if 1
    return krb5_rc_default(context, id);
#else
    krb5_rcache rcache;
    krb5_error_code ret;

    char *tmp = malloc(4 * piece->length + 1);
    char *name;

    if(tmp == NULL) {
	krb5_set_error_message(context, ENOMEM,
			       N_("malloc: out of memory", ""));
	return ENOMEM;
    }
    strvisx(tmp, piece->data, piece->length, VIS_WHITE | VIS_OCTAL);
#ifdef HAVE_GETEUID
    ret = asprintf(&name, "FILE:rc_%s_%u", tmp, (unsigned)geteuid());
#else
    ret = asprintf(&name, "FILE:rc_%s", tmp);
#endif
    free(tmp);
    if(ret < 0 || name == NULL) {
	krb5_set_error_message(context, ENOMEM,
			       N_("malloc: out of memory", ""));
	return ENOMEM;
    }

    ret = krb5_rc_resolve_full(context, &rcache, name);
    free(name);
    if(ret)
	return ret;
    *id = rcache;
    return ret;
#endif
}
