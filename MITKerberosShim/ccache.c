/*
 * Copyright (c) 2004 - 2007 Kungliga Tekniska Högskolan
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

#include "mit-CredentialsCache.h"
#include <string.h>
#include <syslog.h>
#include <stdlib.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>

#include "heim.h"
#include "heim-sym.h"

static cc_time_t context_change_time = 0;

void
update_time(cc_time_t *change_time)
{
    cc_time_t now = time(NULL);
    if (*change_time >= now)
	*change_time += 1;
    else
	*change_time = now;
}

static cc_int32 
string_release(cc_string_t io_string)
{
    free((char *)io_string->data);
    free(io_string);
    return ccNoError;
}


cc_string_f string_functions =  {
    .release = string_release
};

static cc_string_t
create_string(const char *string)
{
    cc_string_t s;
    s = mshim_malloc(sizeof(*s));
    s->functions = &string_functions;
    s->data = strdup(string);
    return s;
}


#define	KRB5_CCAPI_TKT_FLG_FORWARDABLE			0x40000000
#define	KRB5_CCAPI_TKT_FLG_FORWARDED			0x20000000
#define	KRB5_CCAPI_TKT_FLG_PROXIABLE			0x10000000
#define	KRB5_CCAPI_TKT_FLG_PROXY			0x08000000
#define	KRB5_CCAPI_TKT_FLG_MAY_POSTDATE			0x04000000
#define	KRB5_CCAPI_TKT_FLG_POSTDATED			0x02000000
#define	KRB5_CCAPI_TKT_FLG_INVALID			0x01000000
#define	KRB5_CCAPI_TKT_FLG_RENEWABLE			0x00800000
#define	KRB5_CCAPI_TKT_FLG_INITIAL			0x00400000
#define	KRB5_CCAPI_TKT_FLG_PRE_AUTH			0x00200000
#define	KRB5_CCAPI_TKT_FLG_HW_AUTH			0x00100000
#define	KRB5_CCAPI_TKT_FLG_TRANSIT_POLICY_CHECKED	0x00080000
#define	KRB5_CCAPI_TKT_FLG_OK_AS_DELEGATE		0x00040000
#define	KRB5_CCAPI_TKT_FLG_ANONYMOUS			0x00020000

static krb5_error_code
make_cred_from_ccred(krb5_context context,
		     const cc_credentials_v5_t *incred,
		     krb5_creds *cred)
{
    krb5_error_code ret;
    unsigned int i;

    memset(cred, 0, sizeof(*cred));

    ret = heim_krb5_parse_name(context, incred->client, &cred->client);
    if (ret)
	goto fail;

    ret = heim_krb5_parse_name(context, incred->server, &cred->server);
    if (ret)
	goto fail;

    cred->session.keytype = incred->keyblock.type;
    cred->session.keyvalue.length = incred->keyblock.length;
    cred->session.keyvalue.data = malloc(incred->keyblock.length);
    if (cred->session.keyvalue.data == NULL)
	goto nomem;
    memcpy(cred->session.keyvalue.data, incred->keyblock.data,
	   incred->keyblock.length);

    cred->times.authtime = incred->authtime;
    cred->times.starttime = incred->starttime;
    cred->times.endtime = incred->endtime;
    cred->times.renew_till = incred->renew_till;

    ret = heim_krb5_data_copy(&cred->ticket,
			      incred->ticket.data,
			      incred->ticket.length);
    if (ret)
	goto nomem;

    ret = heim_krb5_data_copy(&cred->second_ticket,
			      incred->second_ticket.data,
			      incred->second_ticket.length);
    if (ret)
	goto nomem;

    cred->authdata.val = NULL;
    cred->authdata.len = 0;

    cred->addresses.val = NULL;
    cred->addresses.len = 0;

    for (i = 0; incred->authdata && incred->authdata[i]; i++)
	;

    if (i) {
	cred->authdata.val = calloc(i, sizeof(cred->authdata.val[0]));
	if (cred->authdata.val == NULL)
	    goto nomem;
	cred->authdata.len = i;
	for (i = 0; i < cred->authdata.len; i++) {
	    cred->authdata.val[i].ad_type = incred->authdata[i]->type;
	    ret = heim_krb5_data_copy(&cred->authdata.val[i].ad_data,
				      incred->authdata[i]->data,
				      incred->authdata[i]->length);
	    if (ret)
		goto nomem;
	}
    }

    for (i = 0; incred->addresses && incred->addresses[i]; i++)
	;

    if (i) {
	cred->addresses.val = calloc(i, sizeof(cred->addresses.val[0]));
	if (cred->addresses.val == NULL)
	    goto nomem;
	cred->addresses.len = i;
	
	for (i = 0; i < cred->addresses.len; i++) {
	    cred->addresses.val[i].addr_type = incred->addresses[i]->type;
	    ret = heim_krb5_data_copy(&cred->addresses.val[i].address,
				      incred->addresses[i]->data,
				      incred->addresses[i]->length);
	    if (ret)
		goto nomem;
	}
    }

    cred->flags.i = 0;
    if (incred->ticket_flags & KRB5_CCAPI_TKT_FLG_FORWARDABLE)
	cred->flags.b.forwardable = 1;
    if (incred->ticket_flags & KRB5_CCAPI_TKT_FLG_FORWARDED)
	cred->flags.b.forwarded = 1;
    if (incred->ticket_flags & KRB5_CCAPI_TKT_FLG_PROXIABLE)
	cred->flags.b.proxiable = 1;
    if (incred->ticket_flags & KRB5_CCAPI_TKT_FLG_PROXY)
	cred->flags.b.proxy = 1;
    if (incred->ticket_flags & KRB5_CCAPI_TKT_FLG_MAY_POSTDATE)
	cred->flags.b.may_postdate = 1;
    if (incred->ticket_flags & KRB5_CCAPI_TKT_FLG_POSTDATED)
	cred->flags.b.postdated = 1;
    if (incred->ticket_flags & KRB5_CCAPI_TKT_FLG_INVALID)
	cred->flags.b.invalid = 1;
    if (incred->ticket_flags & KRB5_CCAPI_TKT_FLG_RENEWABLE)
	cred->flags.b.renewable = 1;
    if (incred->ticket_flags & KRB5_CCAPI_TKT_FLG_INITIAL)
	cred->flags.b.initial = 1;
    if (incred->ticket_flags & KRB5_CCAPI_TKT_FLG_PRE_AUTH)
	cred->flags.b.pre_authent = 1;
    if (incred->ticket_flags & KRB5_CCAPI_TKT_FLG_HW_AUTH)
	cred->flags.b.hw_authent = 1;
    if (incred->ticket_flags & KRB5_CCAPI_TKT_FLG_TRANSIT_POLICY_CHECKED)
	cred->flags.b.transited_policy_checked = 1;
    if (incred->ticket_flags & KRB5_CCAPI_TKT_FLG_OK_AS_DELEGATE)
	cred->flags.b.ok_as_delegate = 1;
    if (incred->ticket_flags & KRB5_CCAPI_TKT_FLG_ANONYMOUS)
	cred->flags.b.anonymous = 1;

    return 0;

nomem:
    ret = ENOMEM;
    krb5_set_error_message((mit_krb5_context)context, ret, "malloc: out of memory");

fail:
    heim_krb5_free_cred_contents(context, cred);
    return ret;
}

static void
free_ccred(cc_credentials_v5_t *cred)
{
    int i;

    if (cred->addresses) {
	for (i = 0; cred->addresses[i] != 0; i++) {
	    if (cred->addresses[i]->data)
		free(cred->addresses[i]->data);
	    free(cred->addresses[i]);
	}
	free(cred->addresses);
    }
    if (cred->server)
	free(cred->server);
    if (cred->client)
	free(cred->client);
    memset(cred, 0, sizeof(*cred));
}

static krb5_error_code
make_ccred_from_cred(krb5_context context,
		     const krb5_creds *incred,
		     cc_credentials_v5_t *cred)
{
    krb5_error_code ret;
    int i;

    memset(cred, 0, sizeof(*cred));

    ret = heim_krb5_unparse_name(context, incred->client, &cred->client);
    if (ret)
	goto fail;

    ret = heim_krb5_unparse_name(context, incred->server, &cred->server);
    if (ret)
	goto fail;

    cred->keyblock.type = incred->session.keytype;
    cred->keyblock.length = incred->session.keyvalue.length;
    cred->keyblock.data = incred->session.keyvalue.data;

    cred->authtime = incred->times.authtime;
    cred->starttime = incred->times.starttime;
    cred->endtime = incred->times.endtime;
    cred->renew_till = incred->times.renew_till;

    cred->ticket.length = incred->ticket.length;
    cred->ticket.data = incred->ticket.data;

    cred->second_ticket.length = incred->second_ticket.length;
    cred->second_ticket.data = incred->second_ticket.data;

    /* XXX this one should also be filled in */
    cred->authdata = NULL;

    cred->addresses = calloc(incred->addresses.len + 1,
			     sizeof(cred->addresses[0]));
    if (cred->addresses == NULL) {

	ret = ENOMEM;
	goto fail;
    }

    for (i = 0; i < incred->addresses.len; i++) {
	cc_data *addr;
	addr = malloc(sizeof(*addr));
	if (addr == NULL) {
	    ret = ENOMEM;
	    goto fail;
	}
	addr->type = incred->addresses.val[i].addr_type;
	addr->length = incred->addresses.val[i].address.length;
	addr->data = malloc(addr->length);
	if (addr->data == NULL) {
	    free(addr);
	    ret = ENOMEM;
	    goto fail;
	}
	memcpy(addr->data, incred->addresses.val[i].address.data,
	       addr->length);
	cred->addresses[i] = addr;
    }
    cred->addresses[i] = NULL;

    cred->ticket_flags = 0;
    if (incred->flags.b.forwardable)
	cred->ticket_flags |= KRB5_CCAPI_TKT_FLG_FORWARDABLE;
    if (incred->flags.b.forwarded)
	cred->ticket_flags |= KRB5_CCAPI_TKT_FLG_FORWARDED;
    if (incred->flags.b.proxiable)
	cred->ticket_flags |= KRB5_CCAPI_TKT_FLG_PROXIABLE;
    if (incred->flags.b.proxy)
	cred->ticket_flags |= KRB5_CCAPI_TKT_FLG_PROXY;
    if (incred->flags.b.may_postdate)
	cred->ticket_flags |= KRB5_CCAPI_TKT_FLG_MAY_POSTDATE;
    if (incred->flags.b.postdated)
	cred->ticket_flags |= KRB5_CCAPI_TKT_FLG_POSTDATED;
    if (incred->flags.b.invalid)
	cred->ticket_flags |= KRB5_CCAPI_TKT_FLG_INVALID;
    if (incred->flags.b.renewable)
	cred->ticket_flags |= KRB5_CCAPI_TKT_FLG_RENEWABLE;
    if (incred->flags.b.initial)
	cred->ticket_flags |= KRB5_CCAPI_TKT_FLG_INITIAL;
    if (incred->flags.b.pre_authent)
	cred->ticket_flags |= KRB5_CCAPI_TKT_FLG_PRE_AUTH;
    if (incred->flags.b.hw_authent)
	cred->ticket_flags |= KRB5_CCAPI_TKT_FLG_HW_AUTH;
    if (incred->flags.b.transited_policy_checked)
	cred->ticket_flags |= KRB5_CCAPI_TKT_FLG_TRANSIT_POLICY_CHECKED;
    if (incred->flags.b.ok_as_delegate)
	cred->ticket_flags |= KRB5_CCAPI_TKT_FLG_OK_AS_DELEGATE;
    if (incred->flags.b.anonymous)
	cred->ticket_flags |= KRB5_CCAPI_TKT_FLG_ANONYMOUS;

    return 0;

fail:
    free_ccred(cred);

    krb5_clear_error_message((mit_krb5_context)context);
    return ret;
}

/*
 *
 */

struct cred {
    cc_credentials_union *data;
    cc_credentials_f *functions;
    cc_credentials_f *otherFunctions;
    krb5_creds cred;
};



static cc_int32
cred_release(cc_credentials_t io_credentials)
{
    struct cred *c = (struct cred *)io_credentials;
    heim_krb5_free_cred_contents(milcontext, &c->cred);
    free(c->data->credentials.credentials_v5);
    free(c->data);
    free(c);
    return ccNoError;
}
    
static cc_int32
cred_compare (cc_credentials_t  in_credentials,
	      cc_credentials_t  in_compare_to_credentials,
	      cc_uint32        *out_equal)
{
    *out_equal = 1;
    return ccErrNoMem;
}

cc_credentials_f credential_functions = {
    .release = cred_release,
    .compare = cred_compare
};



static cc_credentials_t
create_credentials(krb5_creds *cred)
{
    struct cred *c;

    c = calloc(1, sizeof(*c));
    c->data = calloc(1, sizeof(*c->data));
    c->data->version = cc_credentials_v5;
    c->data->credentials.credentials_v5 = calloc(1, sizeof(*c->data->credentials.credentials_v5));
    c->functions = &credential_functions;
    
    heim_krb5_copy_creds_contents(milcontext, cred, &c->cred);
    make_ccred_from_cred(milcontext, &c->cred, c->data->credentials.credentials_v5);

    return (cc_credentials_t)c;
}


struct cred_iterator {
    cc_credentials_iterator_d iterator;
    krb5_ccache id;
    krb5_cc_cursor cursor;
};

static cc_int32
cred_iter_release(cc_credentials_iterator_t io_credentials_iterator)
{
    struct cred_iterator *ci = (struct cred_iterator *)io_credentials_iterator;
    LOG_ENTRY();
    if (ci->id)
	krb5_cc_end_seq_get ((mit_krb5_context)milcontext,
			     (mit_krb5_ccache)ci->id,
			     (mit_krb5_cc_cursor *)&ci->cursor);
    free(ci);
    return ccNoError;
}

cc_int32
cred_iter_next(cc_credentials_iterator_t  in_credentials_iterator, cc_credentials_t *out_credentials)
{
    struct cred_iterator *ci = (struct cred_iterator *)in_credentials_iterator;
    krb5_error_code ret;
    krb5_creds cred;
    LOG_ENTRY();

    ret = heim_krb5_cc_next_cred(milcontext, ci->id, &ci->cursor, &cred);
    if (ret == KRB5_CC_END)
	return ccIteratorEnd;
    else if (ret)
	return ret; /* XXX */

    *out_credentials = create_credentials(&cred);
    heim_krb5_free_cred_contents(milcontext, &cred);
    if (*out_credentials == NULL)
	return ccErrNoMem;

    return ccNoError;
}
    
cc_int32
cred_iter_clone (cc_credentials_iterator_t  in_credentials_iterator,
		 cc_credentials_iterator_t *out_credentials_iterator)
{
    LOG_UNIMPLEMENTED();
    return ccErrNoMem;
}


struct cc_credentials_iterator_f cred_iter_functions = {
    .release = cred_iter_release,
    .next = cred_iter_next,
    .clone = cred_iter_clone
};



struct cc_ccache {
    cc_ccache_d ccache;
    krb5_ccache id;
    cc_time_t change_time;
    cc_time_t last_default_time;
};

cc_int32
ccache_release(cc_ccache_t io_ccache)
{
    struct cc_ccache *c = (struct cc_ccache *)io_ccache;
    LOG_ENTRY();
    if (c->id)
	heim_krb5_cc_close(milcontext, c->id);
    free(c);
    return ccNoError;
}

static cc_int32
ccache_destroy(cc_ccache_t io_ccache)
{
    struct cc_ccache *c = (struct cc_ccache *)io_ccache;
    LOG_ENTRY();
    update_time(&context_change_time);
    krb5_cc_destroy((mit_krb5_context)milcontext, (mit_krb5_ccache)c->id);
    c->id = NULL;
    return ccNoError;
}

static cc_int32
ccache_set_default(cc_ccache_t io_ccache)
{
    struct cc_ccache *c = (struct cc_ccache *)io_ccache;
    LOG_ENTRY();
    if (io_ccache == NULL || c->id == NULL)
	return ccErrBadParam;
    heim_krb5_cc_switch(milcontext, c->id);
    update_time(&c->change_time);
    update_time(&c->last_default_time);
    return ccNoError;
}

static cc_int32
ccache_get_credentials_version(cc_ccache_t  in_ccache, cc_uint32   *out_credentials_version)
{
    if (out_credentials_version == NULL)
	return ccErrBadParam;
    *out_credentials_version = cc_credentials_v5;
    return ccNoError;
}

static cc_int32
ccache_get_name(cc_ccache_t  in_ccache, cc_string_t *out_name)
{
    struct cc_ccache *c = (struct cc_ccache *)in_ccache;
    const char *name;
    LOG_ENTRY();

    if (out_name == NULL)
	return ccErrBadParam;
    if (c->id == NULL)
	return ccErrInvalidCCache;

    name = heim_krb5_cc_get_name(milcontext, c->id);
    if (name == NULL)
	return ccErrInvalidCCache;
    *out_name = create_string(name);

    return ccNoError;
}

static cc_int32
ccache_get_principal(cc_ccache_t  in_ccache, cc_uint32    in_credentials_version, cc_string_t *out_principal)
{
    struct cc_ccache *c = (struct cc_ccache *)in_ccache;
    krb5_principal princ;
    krb5_error_code ret;
    char *name;
    
    LOG_ENTRY();

    if (out_principal == NULL)
	return ccErrBadParam;
    if (in_credentials_version != cc_credentials_v5)
	return LOG_FAILURE(ccErrBadCredentialsVersion, "wrong version");
    if (c->id == NULL)
	return ccErrInvalidCCache;

    ret = heim_krb5_cc_get_principal(milcontext, c->id, &princ);
    if (ret)
	return LOG_FAILURE(ret, "get principal");
    ret = heim_krb5_unparse_name(milcontext, princ, &name);
    heim_krb5_free_principal(milcontext, princ);
    if (ret)
	return LOG_FAILURE(ret, "unparse name");
    *out_principal = create_string(name);
    free(name);

    return ccNoError;
}

static cc_int32
ccache_set_principal(cc_ccache_t  io_ccache, cc_uint32    in_credentials_version, const char  *in_principal)
{
    struct cc_ccache *c = (struct cc_ccache *)io_ccache;
    krb5_error_code ret;
    krb5_principal p;
    LOG_ENTRY();

    if (in_principal == NULL)
	return ccErrBadParam;
    if (in_credentials_version != cc_credentials_v5)
	return LOG_FAILURE(ccErrBadCredentialsVersion, "wrong version");

    update_time(&c->change_time);
    update_time(&context_change_time);

    ret = heim_krb5_parse_name(milcontext, in_principal, &p);
    if (ret)
	return LOG_FAILURE(ccErrBadParam, "parse name");
	
    ret = heim_krb5_cc_initialize(milcontext, c->id, p);
    heim_krb5_free_principal(milcontext, p);
    if (ret)
	return LOG_FAILURE(ccErrInvalidCCache, "init cache");
    
    return ccNoError;
}

static cc_int32
ccache_store_credentials(cc_ccache_t io_ccache, const cc_credentials_union *in_credentials_union)
{
    struct cc_ccache *c = (struct cc_ccache *)io_ccache;
    krb5_error_code ret;
    krb5_creds hcred;
    LOG_ENTRY();

    if (in_credentials_union == NULL)
	return ccErrBadParam;
    if (in_credentials_union->version != cc_credentials_v5)
	return LOG_FAILURE(ccErrBadCredentialsVersion, "wrong version");
    if (in_credentials_union->credentials.credentials_v5->client  == NULL)
	return ccErrBadParam;

    update_time(&c->change_time);
    update_time(&context_change_time);

    make_cred_from_ccred(milcontext, in_credentials_union->credentials.credentials_v5, &hcred);

    ret = heim_krb5_cc_store_cred(milcontext, c->id, &hcred);
    heim_krb5_free_cred_contents(milcontext, &hcred);
    if (ret)
	return LOG_FAILURE(ccErrInvalidCCache, "store cred");

    return ccNoError;
}

static cc_int32
ccache_remove_credentials(cc_ccache_t io_ccache, cc_credentials_t in_credentials)
{
    struct cc_ccache *c = (struct cc_ccache *)io_ccache;
    const cc_credentials_v5_t *incred;
    krb5_creds cred;
    krb5_error_code ret;

    LOG_ENTRY();

    update_time(&c->change_time);
    update_time(&context_change_time);

    memset(&cred, 0, sizeof(cred));

    if (c->id == NULL)
	return LOG_FAILURE(ccErrBadParam, "bad argument");

    if (in_credentials == NULL || in_credentials->data == NULL)
	return LOG_FAILURE(ccErrBadParam, "remove with no cred?");
    if (in_credentials->data->version != cc_credentials_v5)
	return LOG_FAILURE(ccErrBadParam, "wrong version");

    incred = in_credentials->data->credentials.credentials_v5;
    if (incred->client == NULL)
	return LOG_FAILURE(ccErrBadParam, "no client to remove");
    if (incred->server  == NULL)
	return LOG_FAILURE(ccErrBadParam, "no server to remove");

    ret = heim_krb5_parse_name(milcontext, incred->client, &cred.client);
    if (ret)
	goto fail;
    ret = heim_krb5_parse_name(milcontext, incred->server, &cred.server);
    if (ret)
	goto fail;

    ret = heim_krb5_cc_remove_cred(milcontext, c->id, 0, &cred);

    update_time(&context_change_time);
 fail:
    heim_krb5_free_cred_contents(milcontext, &cred);
    if (ret)
	return ccErrCredentialsNotFound;
    return ccNoError;
}

static cc_int32
ccache_new_credentials_iterator(cc_ccache_t in_ccache, cc_credentials_iterator_t *out_credentials_iterator)
{
    struct cc_ccache *c = (struct cc_ccache *)in_ccache;
    struct cred_iterator *ci;
    krb5_error_code ret;
    LOG_ENTRY();

    if (c == NULL || c->id == NULL)
	return ccErrInvalidCCache;
    if (out_credentials_iterator == NULL)
	return ccErrBadParam;

    ci = calloc(1, sizeof(*c));
    ci->iterator.functions = &cred_iter_functions;
    ci->id = c->id;
    ret = krb5_cc_start_seq_get((mit_krb5_context)milcontext, 
				(mit_krb5_ccache)c->id,
				(mit_krb5_cc_cursor *)&ci->cursor);
    if (ret) {
	free(ci);
	return LOG_FAILURE(ccErrInvalidCCache, "start seq");
    }
    *out_credentials_iterator = (cc_credentials_iterator_t)ci;
    return ccNoError;
}

static cc_int32
ccache_move(cc_ccache_t io_source_ccache, cc_ccache_t io_destination_ccache)
{
    struct cc_ccache *s = (struct cc_ccache *)io_source_ccache;
    struct cc_ccache *d = (struct cc_ccache *)io_destination_ccache;
    krb5_error_code ret;

    if (s->id == NULL)
	return ccErrInvalidCCache;
    if (d == NULL)
	return ccErrBadParam;

    if (d->id == NULL) {
	ret = heim_krb5_cc_new_unique(milcontext,
				      heim_krb5_cc_get_type(milcontext, s->id),
				      NULL, &d->id);
	if (ret)
	    return ccErrInvalidCCache;
    }

    ret = heim_krb5_cc_move(milcontext, s->id, d->id);
    if (ret)
	return LOG_FAILURE(ret, "move cache");
    s->id = NULL;

    return ccNoError;
}

static cc_int32
ccache_lock(cc_ccache_t io_ccache, cc_uint32   in_lock_type, cc_uint32   in_block)
{
    LOG_ENTRY();
    return ccNoError;
}

static cc_int32
ccache_unlock(cc_ccache_t io_ccache)
{
    LOG_ENTRY();
    return ccNoError;
}

static cc_int32
ccache_get_last_default_time(cc_ccache_t  in_ccache, cc_time_t   *out_last_default_time)
{
    struct cc_ccache *s = (struct cc_ccache *)in_ccache;
    LOG_ENTRY();

    if (out_last_default_time == NULL)
	return ccErrBadParam;
    if (s->id == NULL)
	return ccErrInvalidCCache;
    if (s->last_default_time == 0)
	return ccErrNeverDefault;

    *out_last_default_time = s->last_default_time;
    return ccNoError;
}

static cc_int32
ccache_get_change_time(cc_ccache_t  in_ccache, cc_time_t   *out_change_time)
{
    struct cc_ccache *s = (struct cc_ccache *)in_ccache;
    LOG_ENTRY();

    if (out_change_time == NULL)
	return ccErrBadParam;
    *out_change_time = s->change_time;
    return ccNoError;
}

static cc_int32
ccache_compare(cc_ccache_t  in_ccache, cc_ccache_t  in_compare_to_ccache, cc_uint32   *out_equal)
{
    struct cc_ccache *s1 = (struct cc_ccache *)in_ccache;
    struct cc_ccache *s2 = (struct cc_ccache *)in_compare_to_ccache;
    krb5_error_code ret;
    char *n1, *n2;

    LOG_ENTRY();

    if (out_equal == NULL || s2 == NULL)
	return ccErrBadParam;
    if (s1 == s2) {
	*out_equal = 1;
	return ccNoError;
    }
    if (s1->id == NULL || s2->id == NULL)
	return ccErrInvalidCCache;

    ret = heim_krb5_cc_get_full_name(milcontext, s1->id, &n1);
    if (ret)
	return ccErrInvalidCCache;
    ret = heim_krb5_cc_get_full_name(milcontext, s2->id, &n2);
    if (ret) {
	free(n1);
	return ccErrInvalidCCache;
    }

    *out_equal = (strcmp(n1, n2) == 0);
    
    free(n1);
    free(n2);

    return ccNoError;
}

static cc_int32
ccache_get_kdc_time_offset(cc_ccache_t in_ccache,
			   cc_uint32 in_credentials_version,
			   cc_time_t *out_time_offset)
{
    struct cc_ccache *c = (struct cc_ccache *)in_ccache;
    krb5_deltat sec = 0;

    LOG_ENTRY();

    if (c->id == NULL)
	return LOG_FAILURE(ccErrBadParam, "bad credential");
    if (in_credentials_version != cc_credentials_v5)
	return LOG_FAILURE(ccErrBadCredentialsVersion, "wrong version");
    if (out_time_offset == NULL)
	return LOG_FAILURE(ccErrBadParam, "bad argument");

    heim_krb5_cc_get_kdc_offset(milcontext, c->id, &sec);
    *out_time_offset = sec;

    return ccNoError;
}

static cc_int32
ccache_set_kdc_time_offset(cc_ccache_t io_ccache, cc_uint32   in_credentials_version, cc_time_t   in_time_offset)
{
    struct cc_ccache *c = (struct cc_ccache *)io_ccache;
    LOG_ENTRY();

    if (c->id == NULL)
	return LOG_FAILURE(ccErrBadParam, "bad credential");

    if (in_credentials_version != cc_credentials_v5)
	return LOG_FAILURE(ccErrBadCredentialsVersion, "wrong version");

    heim_krb5_cc_set_kdc_offset(milcontext, c->id, in_time_offset);

    return ccNoError;
}

static cc_int32
ccache_clear_kdc_time_offset(cc_ccache_t io_ccache, cc_uint32   in_credentials_version)
{
    struct cc_ccache *c = (struct cc_ccache *)io_ccache;
    LOG_ENTRY();

    if (c->id == NULL)
	return LOG_FAILURE(ccErrBadParam, "bad credential");

    heim_krb5_cc_set_kdc_offset(milcontext, c->id, 0);

    return ccNoError;
}

static cc_int32
ccache_wait_for_change(cc_ccache_t in_ccache)
{
    LOG_UNIMPLEMENTED();
    return ccErrNoMem;
}

static cc_ccache_f ccache_functions = {
    .release = ccache_release,
    .destroy = ccache_destroy,
    .set_default = ccache_set_default,
    .get_credentials_version = ccache_get_credentials_version,
    .get_name = ccache_get_name,
    .get_principal = ccache_get_principal,
    .set_principal = ccache_set_principal,
    .store_credentials = ccache_store_credentials,
    .remove_credentials = ccache_remove_credentials,
    .new_credentials_iterator = ccache_new_credentials_iterator,
    .move = ccache_move,
    .lock = ccache_lock,
    .unlock = ccache_unlock,
    .get_last_default_time = ccache_get_last_default_time,
    .get_change_time = ccache_get_change_time,
    .compare = ccache_compare,
    .get_kdc_time_offset = ccache_get_kdc_time_offset,
    .set_kdc_time_offset = ccache_set_kdc_time_offset,
    .clear_kdc_time_offset = ccache_clear_kdc_time_offset,
    .wait_for_change = ccache_wait_for_change
};

static cc_ccache_t
create_ccache(krb5_ccache id)
{
    struct cc_ccache *c;

    c = mshim_malloc(sizeof(*c));
    c->ccache.functions = &ccache_functions;
    c->id = id;
    update_time(&c->change_time);
    return (cc_ccache_t)c;
}

struct cc_iter {
    cc_ccache_iterator_d iterator;
    mit_krb5_cccol_cursor cursor;
};

static cc_int32
cc_iterator_release(cc_ccache_iterator_t io_ccache_iterator)
{
    struct cc_iter *c = (struct cc_iter *)io_ccache_iterator;
    LOG_ENTRY();
    krb5_cccol_cursor_free((mit_krb5_context)milcontext, &c->cursor);
    free(c);
    return ccNoError;
}

cc_int32
cc_iterator_next(cc_ccache_iterator_t  in_ccache_iterator,
		 cc_ccache_t *out_ccache)
{
    struct cc_iter *c = (struct cc_iter *)in_ccache_iterator;
    krb5_error_code ret;
    krb5_ccache id;

    LOG_ENTRY();

    if (out_ccache == NULL)
	return ccErrBadParam;


    while (1) {
	ret = krb5_cccol_cursor_next((mit_krb5_context)milcontext, c->cursor, (mit_krb5_ccache *)&id);
	if (ret == KRB5_CC_END || id == NULL)
	    return ccIteratorEnd;
	else if (ret)
	    return LOG_FAILURE(ret, "ccol next cursor");

	const char *type = heim_krb5_cc_get_type(milcontext, id);
	if (strcmp(type, "API") == 0 || strcmp(type, "KCM") == 0)
	    break;
	heim_krb5_cc_close(milcontext, id);
    }
    *out_ccache = create_ccache(id);

    return ccNoError;
}

static cc_int32
cc_iterator_clone(cc_ccache_iterator_t  in_ccache_iterator,
		  cc_ccache_iterator_t *out_ccache_iterator)
{
    LOG_UNIMPLEMENTED();
    if (out_ccache_iterator == NULL)
	return ccErrBadParam;
    return ccErrNoMem;
}

static cc_ccache_iterator_f ccache_iterator_functions = {
    .release = cc_iterator_release,
    .next = cc_iterator_next,
    .clone = cc_iterator_clone
};




static cc_int32
context_release(cc_context_t io_context)
{
    LOG_ENTRY();
    memset(io_context, 0, sizeof(*io_context));
    free(io_context);

    return ccNoError;
}

static cc_int32
context_get_change_time(cc_context_t  in_context,
			cc_time_t    *out_time)
{
    LOG_ENTRY();
    if (out_time == NULL)
	return ccErrBadParam;
    *out_time = context_change_time;
    return ccNoError;
}

static cc_int32
context_get_default_ccache_name(cc_context_t  in_context,
				cc_string_t  *out_name)
{
    const char *name;
    name = krb5_cc_default_name((mit_krb5_context)milcontext);
    if (name == NULL)
	return ccErrNoMem; /* XXX */
    if (out_name == NULL)
	return ccErrBadParam;
    if (strncmp("API:", name, 4) == 0)
	name += 4;
    
    *out_name = create_string(name);

    return ccNoError;
}

/*
 * Probe for client principal to make sure the cache really
 * exists.
 */

static cc_int32
check_exists(krb5_ccache id)
{
    krb5_principal princ;
    int ret;

    ret = heim_krb5_cc_get_principal(milcontext, id, &princ);
    if (ret)
	return 0;
    heim_krb5_free_principal(milcontext, princ);

    return 1;
}



static cc_int32 
context_open_ccache (cc_context_t  in_context,
		     const char   *in_name,
		     cc_ccache_t  *out_ccache)
{
    char *name;
    krb5_error_code ret;
    krb5_ccache id;

    if (out_ccache == NULL || in_name == NULL || in_context == NULL)
	return ccErrBadParam;

    asprintf(&name, "API:%s", in_name);

    ret = heim_krb5_cc_resolve(milcontext, name, &id);
    free(name);
    if (ret)
	return LOG_FAILURE(ret, "open cache");

    if (!check_exists(id)) {
	heim_krb5_cc_close(milcontext, id);
	return ccErrCCacheNotFound;
    }

    *out_ccache = create_ccache(id);

    return ccNoError;
}
    
static cc_int32
context_open_default_ccache(cc_context_t  in_context,
			    cc_ccache_t  *out_ccache)
{
    krb5_error_code ret;
    krb5_ccache id;

    LOG_ENTRY();

    if (out_ccache == NULL)
	return ccErrBadParam;

    ret = heim_krb5_cc_default(milcontext, &id);
    if (ret)
	return LOG_FAILURE(ret, "cc default");

    if (!check_exists(id)) {
	heim_krb5_cc_close(milcontext, id);
	return ccErrCCacheNotFound;
    }

    *out_ccache = create_ccache(id);

    return ccNoError;
}

static cc_int32 
context_create_ccache(cc_context_t  in_context,
		      const char   *in_name,
		      cc_uint32     in_cred_vers,
		      const char   *in_principal, 
		      cc_ccache_t  *out_ccache)
{
    krb5_principal principal;
    krb5_error_code ret;
    krb5_ccache id;

    if (in_cred_vers != cc_credentials_v5)
	return ccErrBadCredentialsVersion;
    if (out_ccache == NULL || in_name == NULL || in_context == NULL || in_principal == NULL)
	return ccErrBadParam;

    update_time(&context_change_time);

    ret = heim_krb5_parse_name(milcontext, in_principal, &principal);
    if (ret)
	return LOG_FAILURE(ret, "parse name");

    ret = heim_krb5_cc_resolve(milcontext, in_name, &id);
    if (ret) {
	heim_krb5_free_principal(milcontext, principal);
	return LOG_FAILURE(ret, "open cache");
    }

    ret = heim_krb5_cc_initialize(milcontext, id, principal);
    heim_krb5_free_principal(milcontext, principal);
    if (ret) {
	krb5_cc_destroy((mit_krb5_context)milcontext, (mit_krb5_ccache)id);
	return LOG_FAILURE(ret, "cc init");
    }

    *out_ccache = create_ccache(id);

    return ccNoError;
}

static cc_int32
context_create_default_ccache(cc_context_t  in_context,
			      cc_uint32     in_cred_vers,
			      const char   *in_principal, 
			      cc_ccache_t  *out_ccache)
{
    krb5_principal principal;
    krb5_error_code ret;
    struct cc_ccache *c;
    krb5_ccache id;

    LOG_ENTRY();

    if (in_cred_vers != cc_credentials_v5)
	return ccErrBadCredentialsVersion;
    if (out_ccache == NULL || in_principal == NULL)
	return ccErrBadParam;

    *out_ccache = NULL;

    update_time(&context_change_time);

    ret = heim_krb5_cc_default(milcontext, &id);
    if (ret)
	return LOG_FAILURE(ret, "cc default");

    ret = heim_krb5_parse_name(milcontext, in_principal, &principal);
    if (ret) {
	heim_krb5_cc_close(milcontext, id);
	return LOG_FAILURE(ret, "parse name");
    }
    
    ret = heim_krb5_cc_initialize(milcontext, id, principal);
    heim_krb5_free_principal(milcontext, principal);
    if (ret) {
	krb5_cc_destroy((mit_krb5_context)milcontext, (mit_krb5_ccache)id);
	return LOG_FAILURE(ret, "cc init");
    }

    c = (struct cc_ccache *)create_ccache(id);

    update_time(&c->last_default_time);

    *out_ccache = (cc_ccache_t)c;

    return ccNoError;
}

static cc_int32
context_create_new_ccache(cc_context_t in_context,
			  cc_uint32    in_cred_vers,
			  const char  *in_principal, 
			  cc_ccache_t *out_ccache)
{
    krb5_principal principal;
    krb5_error_code ret;
    krb5_ccache id;

    LOG_ENTRY();
    
    if (in_cred_vers != cc_credentials_v5)
	return ccErrBadCredentialsVersion;

    if (out_ccache == NULL || in_principal == NULL)
	return ccErrBadParam;

    update_time(&context_change_time);

    ret = heim_krb5_parse_name(milcontext, in_principal, &principal);
    if (ret)
	return LOG_FAILURE(ret, "parse name");

    ret = heim_krb5_cc_new_unique(milcontext, NULL, NULL, &id);
    if (ret) {
	heim_krb5_free_principal(milcontext, principal);
	return LOG_FAILURE(ret, "new unique");
    }
    
    ret = heim_krb5_cc_initialize(milcontext, id, principal);
    heim_krb5_free_principal(milcontext, principal);
    if (ret) {
	krb5_cc_destroy((mit_krb5_context)milcontext, (mit_krb5_ccache)id);
	return LOG_FAILURE(ret, "cc init");
    }

    *out_ccache = create_ccache(id);

    return ccNoError;
}

static cc_int32
context_new_ccache_iterator(cc_context_t in_context,
			    cc_ccache_iterator_t *out_iterator)
{
    LOG_ENTRY();

    krb5_error_code ret;
    struct cc_iter *c;

    if (out_iterator == NULL)
	return ccErrBadParam;

    c = calloc(1, sizeof(*c));
    c->iterator.functions = &ccache_iterator_functions;

    ret = krb5_cccol_cursor_new((mit_krb5_context)milcontext, &c->cursor);
    if (ret) {
	free(c);
	return ccErrNoMem;
    }

    *out_iterator = (cc_ccache_iterator_t)c;

    return ccNoError;
}

static cc_int32
context_lock(cc_context_t in_context,
	     cc_uint32    in_lock_type,
	     cc_uint32    in_block)
{
    LOG_UNIMPLEMENTED();
    return ccNoError;
}
    
static cc_int32
context_unlock(cc_context_t in_context)
{
    LOG_UNIMPLEMENTED();
    return ccNoError;
}

static cc_int32
context_compare(cc_context_t  in_cc_context,
		cc_context_t  in_compare_to_context,
		cc_uint32    *out_equal)
{
    LOG_UNIMPLEMENTED();
    if (out_equal == NULL || in_compare_to_context == NULL)
	return ccErrBadParam;
    *out_equal = (in_cc_context == in_compare_to_context);
    return 0;
}

static cc_int32
context_wait_for_change(cc_context_t in_cc_context)
{
    LOG_UNIMPLEMENTED();
    return ccErrNoMem;
}



cc_context_f cc_functions = {
    .release = context_release,
    .get_change_time = context_get_change_time,
    .get_default_ccache_name = context_get_default_ccache_name,
    .open_ccache = context_open_ccache,
    .open_default_ccache = context_open_default_ccache,
    .create_ccache = context_create_ccache,
    .create_default_ccache = context_create_default_ccache,
    .create_new_ccache = context_create_new_ccache,
    .new_ccache_iterator = context_new_ccache_iterator,
    .lock = context_lock,
    .unlock = context_unlock,
    .compare = context_compare,
    .wait_for_change = context_wait_for_change
};


cc_int32
cc_initialize(cc_context_t  *out_context,
	      cc_int32       in_version,
	      cc_int32      *out_supported_version,
	      char const   **out_vendor)
{
    LOG_ENTRY();

    mshim_init_context();

    update_time(&context_change_time);

    if (in_version < ccapi_version_3 || in_version > ccapi_version_7)
	return ccErrBadAPIVersion;
    if (out_context == NULL)
	return ccErrBadParam;

    *out_context = calloc(1, sizeof(**out_context));
    (*out_context)->functions = &cc_functions;

    if (out_supported_version)
	*out_supported_version = ccapi_version_7;
    if (out_vendor)
	*out_vendor = "Apple Heimdal shim layer";

    return 0;
}


