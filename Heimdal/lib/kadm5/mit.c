/*
 * Copyright (c) 2008 Kungliga Tekniska Högskolan
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

#include "kadm5_locl.h"

#include <syslog.h>
#include <gssapi_krb5.h>

#define CHECK(x)							\
	do {								\
		int __r;						\
		if ((__r = (x))) {					\
			syslog(LOG_ERR, "kadmin: protocol error:%d", __LINE__);			\
			_exit(1);					\
		}							\
	} while(0)

#define INSIST(x) CHECK(!(x))

#define VERSION2 0x12345702

#define LAST_FRAGMENT 0x80000000

#define RPC_VERSION 2
#define KADM_SERVER 2112
#define VVERSION 2
#define FLAVOR_GSS 6
#define FLAVOR_GSS_VERSION 1

#define PROC_NULL 0
#define PROC_CREATE_PRINCIPAL 1
#define PROC_DELETE_PRINCIPAL 2
#define PROC_MODIFY_PRINCIPAL 3
#define PROC_GET_PRINCIPAL 5
#define PROC_CHRAND_PRINCIPAL 7
#define PROC_CREATE_PRINCIPAL3 18


struct call_header {
    uint32_t xid;
    uint32_t rpcvers;
    uint32_t prog;
    uint32_t vers;
    uint32_t proc;
    struct _kadm5_xdr_opaque_auth cred;
    struct _kadm5_xdr_opaque_auth verf;
};

enum {
    RPG_DATA = 0,
    RPG_INIT = 1,
    RPG_CONTINUE_INIT = 2,
    RPG_DESTROY = 3
};

enum {
    rpg_privacy = 3
};

#if 0
static void
gss_error(krb5_context context,
	  gss_OID mech, OM_uint32 type, OM_uint32 error)
{
    OM_uint32 new_stat;
    OM_uint32 msg_ctx = 0;
    gss_buffer_desc status_string;
    OM_uint32 ret;

    do {
	ret = gss_display_status (&new_stat,
				  error,
				  type,
				  mech,
				  &msg_ctx,
				  &status_string);
	krb5_warnx(context, "%.*s",
		   (int)status_string.length,
		   (char *)status_string.value);
	gss_release_buffer (&new_stat, &status_string);
    } while (!GSS_ERROR(ret) && msg_ctx != 0);
}

static void
gss_print_errors (krb5_context context,
		  OM_uint32 maj_stat, OM_uint32 min_stat)
{
    gss_error(context, GSS_C_NO_OID, GSS_C_GSS_CODE, maj_stat);
    gss_error(context, GSS_C_NO_OID, GSS_C_MECH_CODE, min_stat);
}
#endif

static int
read_data(krb5_storage *sp, krb5_storage *msg, size_t len)
{
    char buf[1024];

    while (len) {
	size_t tlen = len;
	ssize_t slen;

	if (tlen > sizeof(buf))
	    tlen = sizeof(buf);

	slen = krb5_storage_read(sp, buf, tlen);
	INSIST(slen >= 0 && (size_t)slen == tlen);

	slen = krb5_storage_write(msg, buf, tlen);
	INSIST(slen >= 0 || (size_t)slen == tlen);

	len -= tlen;
    }
    return 0;
}

static int
collect_fragments(krb5_storage *sp, krb5_storage *msg)
{
    krb5_error_code ret;
    uint32_t len;
    int last_fragment;
    size_t total_len = 0;

    do {
	ret = krb5_ret_uint32(sp, &len);
	if (ret)
	    return ret;

	last_fragment = (len & LAST_FRAGMENT);
	len &= ~LAST_FRAGMENT;

	CHECK(read_data(sp, msg, len));
	total_len += len;

    } while(!last_fragment || total_len == 0);

    return 0;
}

struct grpc_client {
    krb5_data handle;
    krb5_data last_cred;
    gss_ctx_id_t ctx;
    krb5_storage *sp;
    uint32_t seq_num;
    uint32_t xid;
    int done;
    int inprogress;
};

static krb5_error_code
xdr_close_connection(krb5_context context,
		     struct grpc_client *client)
{
    free(client);
    return 0;
}

static krb5_error_code
xdr_send_request(krb5_context context,
		 struct grpc_client *client,
		 uint32_t xid,
		 uint32_t proc,
		 krb5_data *out)
{
    struct _kadm5_xdr_opaque_auth cred, verf;
    struct _kadm5_xdr_gcred gcred;
    OM_uint32 maj_stat, min_stat;
    gss_buffer_desc gin, gout;
    krb5_storage *msg;
    krb5_data data;
    size_t sret;

    msg = krb5_storage_emem();

    memset(&gcred, 0, sizeof(gcred));
    memset(&cred, 0, sizeof(cred));
    memset(&verf, 0, sizeof(verf));

    cred.flavor = FLAVOR_GSS;
    cred.data.data = NULL;
    cred.data.length = 0;

    gcred.version = FLAVOR_GSS_VERSION;
    if (client->done)
	gcred.proc = RPG_DATA;
    else
	gcred.proc = RPG_INIT;
    gcred.service = rpg_privacy;
    gcred.handle = client->handle;
    gcred.seq_num = client->seq_num;

    CHECK(_kadm5_xdr_store_gcred(&gcred, &cred.data));

    CHECK(krb5_store_uint32(msg, xid));
    CHECK(krb5_store_uint32(msg, 0)); /* mtype ? */
    CHECK(krb5_store_uint32(msg, RPC_VERSION));
    CHECK(krb5_store_uint32(msg, KADM_SERVER));
    CHECK(krb5_store_uint32(msg, VVERSION));
    CHECK(krb5_store_uint32(msg, proc));
    CHECK(_kadm5_xdr_store_auth_opaque(msg, &cred));

    /* create header verf */
    if (client->done) {
	krb5_storage_to_data(msg, &data);

	gin.value = data.data;
	gin.length = data.length;

	maj_stat = gss_get_mic(&min_stat, client->ctx, 0, &gin, &gout);
	krb5_data_free(&data);
	INSIST(maj_stat == GSS_S_COMPLETE);

	verf.flavor = FLAVOR_GSS;
	verf.data.data = gout.value;
	verf.data.length = gout.length;

	CHECK(_kadm5_xdr_store_auth_opaque(msg, &verf));
	gss_release_buffer(&min_stat, &gout);
    } else {
	verf.flavor = FLAVOR_GSS;
	verf.data.data = NULL;
	verf.data.length = 0;
	CHECK(_kadm5_xdr_store_auth_opaque(msg, &verf));
    }

    if (!client->done) {
	sret = krb5_storage_write(msg, out->data, out->length);
	INSIST(sret == out->length);
    } else if (client->inprogress) {
	client->inprogress = 0;
	sret = krb5_storage_write(msg, out->data, out->length);
	INSIST(sret == out->length);
    } else {
	krb5_storage *reply;
	int conf_state;

	reply = krb5_storage_emem();

	krb5_store_uint32(reply, client->seq_num);
	sret = krb5_storage_write(reply, out->data, out->length);
	INSIST(sret == out->length);

	krb5_storage_to_data(reply, &data);
	krb5_storage_free(reply);

	gin.value = data.data;
	gin.length = data.length;

	maj_stat = gss_wrap(&min_stat, client->ctx, 1, 0,
			    &gin, &conf_state, &gout);
	INSIST(maj_stat == GSS_S_COMPLETE);
	INSIST(conf_state != 0);
	krb5_data_free(&data);

	data.data = gout.value;
	data.length = gout.length;

	CHECK(_kadm5_xdr_store_data_xdr(msg, data));
	gss_release_buffer(&min_stat, &gout);
    }

    /* write packet to output stream */
    {
	CHECK(krb5_storage_to_data(msg, &data));
	CHECK(krb5_store_uint32(client->sp, ((uint32_t)data.length) | LAST_FRAGMENT));
	sret = krb5_storage_write(client->sp, data.data, data.length);
	INSIST(sret == data.length);
	krb5_data_free(&data);
    }

    return 0;
}

static krb5_error_code
xdr_recv_reply(krb5_context context,
	       struct grpc_client *client,
	       uint32_t *xid,
	       krb5_storage **out)
{
    OM_uint32 maj_stat, min_stat;
    gss_buffer_desc gin, gout;
    krb5_error_code ret;
    krb5_storage *reply;
    krb5_data data;
    int conf_state;
    uint32_t tmp;

    reply = krb5_storage_emem();
    INSIST(reply != NULL);

    ret = collect_fragments(client->sp, reply);
    INSIST(ret == 0);

    krb5_storage_seek(reply, 0, SEEK_SET);

    CHECK(krb5_ret_uint32(reply, xid));
    CHECK(krb5_ret_uint32(reply, &tmp)); /* REPLY */
    INSIST(tmp == 1);
    CHECK(krb5_ret_uint32(reply, &tmp)); /* MSG_ACCEPTED */
    INSIST(tmp == 0);


    CHECK(krb5_ret_uint32(reply, &tmp)); /* flavor_gss */
    INSIST(tmp == FLAVOR_GSS);

    CHECK(_kadm5_xdr_ret_data_xdr(reply, &data));

    if (client->done) {
	uint32_t seqnum = htonl(client->seq_num);

	gin.value = &seqnum;
	gin.length = sizeof(seqnum);
	gout.value = data.data;
	gout.length = data.length;

	maj_stat = gss_verify_mic(&min_stat, client->ctx, &gin, &gout, NULL);
	krb5_data_free(&data);
	INSIST(maj_stat == GSS_S_COMPLETE);
    } else {
	krb5_data_free(&client->last_cred);
	client->last_cred = data;
    }
    CHECK(krb5_ret_uint32(reply, &tmp)); /* SUCCESS */
    INSIST(tmp == 0);

    if (client->done) {
	/* read body */
	CHECK(_kadm5_xdr_ret_data_xdr(reply, &data));

	krb5_storage_free(reply);

	gin.value = data.data;
	gin.length = data.length;

	maj_stat = gss_unwrap(&min_stat, client->ctx, &gin, &gout,
			      &conf_state, NULL);
	krb5_data_free(&data);
	INSIST(maj_stat == GSS_S_COMPLETE);
	INSIST(conf_state != 0);

	/* make reply cleartext */
	*out = krb5_storage_from_mem_copy(gout.value, gout.length);
	INSIST(*out != NULL);

	/* check seq num */
	CHECK(krb5_ret_uint32(*out, &tmp));
	INSIST(tmp == client->seq_num);

	gss_release_buffer(&min_stat, &gout);
    } else {
	*out = reply;
    }

    return 0;
}

static krb5_error_code
xdr_process_request(krb5_context context,
		    struct grpc_client *client,
		    uint32_t proc,
		    krb5_data *in,
		    krb5_storage **out)
{
    krb5_error_code ret;
    uint32_t xid;

    client->xid++;
    client->seq_num++;

    ret = xdr_send_request(context, client, client->xid, proc, in);
    if (ret)
	return ret;
    ret = xdr_recv_reply(context, client, &xid, out);
    if (ret == 0) {
	INSIST(client->xid == xid);
    }
    return ret;
}

static kadm5_ret_t
kadm5_mit_chpass_principal(void *server_handle,
			   krb5_principal principal,
			   int keepold,
			   const char *password,
			   int n_ks_tuple,
			   krb5_key_salt_tuple *ks_tuple)
{
    kadm5_mit_context *context = server_handle;
    krb5_set_error_message(context->context, KADM5_RPC_ERROR,
			   "Function not implemented");
    return KADM5_RPC_ERROR;
}

static kadm5_ret_t
kadm5_mit_chpass_principal_with_key(void *server_handle,
				    krb5_principal princ,
				    int keepold,
				    int n_key_data,
				    krb5_key_data *key_data)
{
    kadm5_mit_context *context = server_handle;
    krb5_set_error_message(context->context, KADM5_RPC_ERROR,
			   "Function not implemented");
    return KADM5_RPC_ERROR;
}

static kadm5_ret_t
kadm5_mit_create_principal(void *server_handle,
			   kadm5_principal_ent_t entry,
			   uint32_t mask,
			   const char *password,
			   int n_ks_tuple,
			   krb5_key_salt_tuple *ks_tuple)
{
    kadm5_mit_context *context = server_handle;
    krb5_storage *sp = NULL;
    krb5_error_code ret;
    uint32_t retcode;
    krb5_data in;

    /* Build request */

    sp = krb5_storage_emem();

    CHECK(krb5_store_uint32(sp, VERSION2));
    CHECK(_kadm5_xdr_store_principal_ent(context->context, sp, entry));
    CHECK(krb5_store_uint32(sp, mask));
    CHECK(_kadm5_xdr_store_string_xdr(sp, password));

    krb5_storage_to_data(sp, &in);
    krb5_storage_free(sp);

    ret = xdr_process_request(context->context, context->gsscontext,
			      PROC_CREATE_PRINCIPAL, &in, &sp);
    krb5_data_free(&in);
    if (ret)
	return ret;

    /* Read reply */

    CHECK(krb5_ret_uint32(sp, &retcode)); /* api version */
    INSIST(retcode == VERSION2);
    CHECK(krb5_ret_uint32(sp, &retcode)); /* code */
    if (retcode == 0) {
#if 0
	CHECK(_kadm5_xdr_ret_principal_ent(context->context, out, ent));
#endif
    }
    krb5_storage_free(sp);

    return (krb5_error_code)retcode;
}

static kadm5_ret_t
kadm5_mit_delete_principal(void *server_handle, krb5_principal principal)
{
    kadm5_mit_context *context = server_handle;
    krb5_storage *sp = NULL;
    krb5_error_code ret;
    uint32_t retcode;
    krb5_data in;

    /* Build request */

    sp = krb5_storage_emem();

    CHECK(krb5_store_uint32(sp, VERSION2));
    CHECK(_kadm5_xdr_store_principal_xdr(context->context, sp, principal));

    krb5_storage_to_data(sp, &in);
    krb5_storage_free(sp);

    ret = xdr_process_request(context->context, context->gsscontext,
			      PROC_DELETE_PRINCIPAL, &in, &sp);
    krb5_data_free(&in);
    if (ret)
	return ret;

    /* Read reply */

    CHECK(krb5_ret_uint32(sp, &retcode)); /* api version */
    INSIST(retcode == VERSION2);
    CHECK(krb5_ret_uint32(sp, &retcode)); /* code */
    krb5_storage_free(sp);

    return (krb5_error_code)retcode;
}

static kadm5_ret_t
kadm5_mit_flush(void *server_handle)
{
    kadm5_mit_context *context = server_handle;
    krb5_set_error_message(context->context, KADM5_RPC_ERROR, "Function not implemented");
    return KADM5_RPC_ERROR;
}

static kadm5_ret_t
kadm5_mit_destroy(void *server_handle)
{
    kadm5_mit_context *context = server_handle;

    krb5_free_principal(context->context, context->caller);
    xdr_close_connection(context->context, context->gsscontext);
    if(context->my_context)
	krb5_free_context(context->context);
    return 0;
}

static kadm5_ret_t
kadm5_mit_get_principal(void *server_handle,
			krb5_principal principal,
			kadm5_principal_ent_t entry,
			uint32_t mask)
{
    kadm5_mit_context *context = server_handle;
    krb5_storage *sp = NULL;
    krb5_error_code ret;
    uint32_t retcode;
    krb5_data in;

    /* Build request */

    sp = krb5_storage_emem();

    CHECK(krb5_store_uint32(sp, VERSION2));
    CHECK(_kadm5_xdr_store_principal_xdr(context->context, sp, principal));
    CHECK(krb5_store_uint32(sp, mask));

    krb5_storage_to_data(sp, &in);
    krb5_storage_free(sp);

    ret = xdr_process_request(context->context, context->gsscontext,
			      PROC_GET_PRINCIPAL, &in, &sp);
    krb5_data_free(&in);
    if (ret)
	return ret;

    /* Read reply */

    CHECK(krb5_ret_uint32(sp, &retcode)); /* api version */
    INSIST(retcode == VERSION2);
    CHECK(krb5_ret_uint32(sp, &retcode)); /* code */
    if (retcode == 0) {
	CHECK(_kadm5_xdr_ret_principal_ent(context->context, sp, entry));
    }
    krb5_storage_free(sp);

    return (krb5_error_code)retcode;
}

static kadm5_ret_t
kadm5_mit_get_principals(void *server_handle,
			 const char *expression,
			 char ***principals,
			 int *count)
{
    kadm5_mit_context *context = server_handle;
    krb5_set_error_message(context->context, KADM5_RPC_ERROR,
			   "Function not implemented");
    return KADM5_RPC_ERROR;
}

static kadm5_ret_t
kadm5_mit_get_privs(void *server_handle, uint32_t*privs)
{
    kadm5_mit_context *context = server_handle;
    krb5_set_error_message(context->context, KADM5_RPC_ERROR,
			   "Function not implemented");
    return KADM5_RPC_ERROR;
}

static kadm5_ret_t
kadm5_mit_modify_principal(void *server_handle,
			  kadm5_principal_ent_t entry,
			  uint32_t mask)
{
    kadm5_mit_context *context = server_handle;
    krb5_storage *sp = NULL;
    krb5_error_code ret;
    uint32_t retcode;
    krb5_data in;

    /* Build request */

    sp = krb5_storage_emem();

    CHECK(krb5_store_uint32(sp, VERSION2));
    CHECK(_kadm5_xdr_store_principal_ent(context->context, sp, entry));
    CHECK(krb5_store_uint32(sp, mask));

    krb5_storage_to_data(sp, &in);
    krb5_storage_free(sp);

    ret = xdr_process_request(context->context, context->gsscontext,
			      PROC_MODIFY_PRINCIPAL, &in, &sp);
    krb5_data_free(&in);
    if (ret)
	return ret;

    /* Read reply */

    CHECK(krb5_ret_uint32(sp, &retcode)); /* api version */
#if 0 /* modify doesn't set api version */
    INSIST(retcode == VERSION2);
#endif
    CHECK(krb5_ret_uint32(sp, &retcode)); /* code */
    krb5_storage_free(sp);

    return (krb5_error_code)retcode;
}

static kadm5_ret_t
kadm5_mit_rename_principal(void *server_handle,
			  krb5_principal from,
			  krb5_principal to)
{
    kadm5_mit_context *context = server_handle;
    krb5_set_error_message(context->context, KADM5_RPC_ERROR,
			   "Function not implemented");
    return KADM5_RPC_ERROR;
}

static kadm5_ret_t
kadm5_mit_randkey_principal(void *server_handle,
			    krb5_principal principal,
			    krb5_boolean keepold,
			    int n_ks_tuple,
			    krb5_key_salt_tuple *ks_tuple,
			    krb5_keyblock **keys,
			    int *n_keys)
{
    kadm5_mit_context *context = server_handle;
    krb5_storage *sp = NULL;
    krb5_error_code ret;
    uint32_t retcode;
    krb5_data in;

    /* Build request */

    sp = krb5_storage_emem();

    CHECK(krb5_store_uint32(sp, VERSION2));
    CHECK(_kadm5_xdr_store_principal_xdr(context->context, sp, principal));

    krb5_storage_to_data(sp, &in);
    krb5_storage_free(sp);

    ret = xdr_process_request(context->context, context->gsscontext,
			      PROC_CHRAND_PRINCIPAL, &in, &sp);
    krb5_data_free(&in);
    if (ret)
	return ret;

    /* Read reply */

    CHECK(krb5_ret_uint32(sp, &retcode)); /* api version */
    INSIST(retcode == VERSION2);
    CHECK(krb5_ret_uint32(sp, &retcode)); /* code */
    if (retcode == 0) {
	uint32_t enctype, num;
	int i;

	CHECK(krb5_ret_uint32(sp, &num));
	CHECK(num < 2000);

	*n_keys = num;

	*keys = calloc(num, sizeof((*keys)[0]));
	for (i = 0; i < *n_keys; i++) {
	    CHECK(krb5_ret_uint32(sp, &enctype));
	    (*keys)[i].keytype = enctype;
	    CHECK(_kadm5_xdr_ret_data_xdr(sp, &(*keys)[i].keyvalue));
	}
    }
    krb5_storage_free(sp);

    return (krb5_error_code)retcode;
}

/*
 *
 */

static void
verify_seq_num(struct grpc_client *c, uint32_t seq, krb5_data *cred)
{
    OM_uint32 maj_stat, min_stat;
    gss_buffer_desc gin, gout;

    seq = htonl(seq);

    gin.value = &seq;
    gin.length = sizeof(seq);
    gout.value = cred->data;
    gout.length = cred->length;

    INSIST(cred->length != 0);

    maj_stat = gss_verify_mic(&min_stat, c->ctx, &gin, &gout, NULL);
    krb5_data_free(&c->last_cred);
    INSIST(maj_stat == GSS_S_COMPLETE);
}


static krb5_error_code
xdr_setup_connection(krb5_context context,
		     const char *service,
		     const char *hostname,
		     unsigned port,
		     struct grpc_client **client)
{
    struct addrinfo *ai, *a, hints;
    char portstr[NI_MAXSERV];
    struct grpc_client *c;
    krb5_error_code ret;
    int error, s = -1;
    OM_uint32 maj_stat, maj_stat2, min_stat, ret_flags;
    gss_buffer_desc gin, gout;
    gss_name_t target;
    int try_kadmin_admin = 0;
    uint32_t seq_window = 0;

    c = calloc(1, sizeof(*c));
    if (c == NULL)
	return ENOMEM;

    memset (&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    snprintf (portstr, sizeof(portstr), "%u", ntohs(port));

    error = getaddrinfo (hostname, portstr, &hints, &ai);
    if (error)
	return KADM5_BAD_SERVER_NAME;

    for (a = ai; a != NULL; a = a->ai_next) {
	s = socket (a->ai_family, a->ai_socktype, a->ai_protocol);
	if (s < 0)
	    continue;
	socket_set_nopipe(s, 1);
	if (connect (s, a->ai_addr, a->ai_addrlen) < 0) {
	    close (s);
	    continue;
	}
	break;
    }
    if (a == NULL || s < 0) {
	freeaddrinfo (ai);
	return KADM5_FAILURE;
    }

    c->sp = krb5_storage_from_fd(s);
    close(s);

 try_again:
    {
	char *str;
	if (!try_kadmin_admin)
	    asprintf(&str, "%s@%s", service, hostname);
	else
	    asprintf(&str, "kadmin@admin");
	gin.value = str;
	gin.length = strlen(str);
	maj_stat = gss_import_name(&min_stat, &gin, GSS_C_NT_HOSTBASED_SERVICE, &target);
	if (maj_stat != GSS_S_COMPLETE) {
	    krb5_set_error_message(context, ENOENT,
				   "failed to import name: %s", str);
	    free(str);
	    return ENOENT;
	}
	free(str);
    }


    c->inprogress = 1;
    gin.value = NULL;
    gin.length = 0;

    /* do gss-api negotiation dance here */
    while (1) {
	krb5_storage *out, *in = krb5_storage_emem();
	krb5_data data;

	maj_stat = gss_init_sec_context(&min_stat, GSS_C_NO_CREDENTIAL,
					&c->ctx, target, GSS_KRB5_MECHANISM,
					GSS_C_MUTUAL_FLAG|GSS_C_INTEG_FLAG|GSS_C_CONF_FLAG,
					GSS_C_INDEFINITE, GSS_C_NO_CHANNEL_BINDINGS,
					&gin, NULL, &gout, &ret_flags, NULL);
	if (gin.value) {
	    free(gin.value);
	    gin.value = NULL;
	}

	if (GSS_ERROR(maj_stat)) {
	    krb5_storage_free(in);
	    if (!try_kadmin_admin) {
		try_kadmin_admin = 1;
		goto try_again;
	    }
	    krb5_set_error_message(context, EINVAL,
				   "init_sec_context failed with %d/%d",
				   maj_stat, min_stat);
	    return EINVAL;
	} else if (maj_stat & GSS_S_CONTINUE_NEEDED) {
	    INSIST(!c->done);
	} else {
	    INSIST(maj_stat == GSS_S_COMPLETE);
	    INSIST((ret_flags & GSS_C_MUTUAL_FLAG) != 0);
	    if (c->done) {
		verify_seq_num(c, seq_window, &c->last_cred);
		krb5_data_free(&c->last_cred);
		break;
	    }

	    c->done = 1;
	}

	data.data = gout.value;
	data.length = gout.length;

	CHECK(_kadm5_xdr_store_data_xdr(in, data));
	gss_release_buffer(&min_stat, &gout);
	krb5_storage_to_data(in, &data);
	krb5_storage_free(in);

	ret = xdr_process_request(context, c, PROC_NULL, &data, &out);
	krb5_data_free(&data);
	if (ret)
	    return ret;

	CHECK(_kadm5_xdr_ret_gss_init_res(out, &c->handle,
					  &maj_stat2, &min_stat,
					  &seq_window, &data));
	krb5_storage_free(out);

	gin.length = data.length;
	gin.value = data.data;

	if (GSS_ERROR(maj_stat2)) {
	    krb5_data_free(&data);
	    krb5_set_error_message(context, EINVAL,
				   "server sent a failure code: %d/%d",
				   maj_stat2, min_stat);
	    return EINVAL;
	} else if (maj_stat2 & GSS_S_CONTINUE_NEEDED) {
	    INSIST(!c->done);

	} else {
	    INSIST(maj_stat2 == GSS_S_COMPLETE);
	    if (c->done) {
		verify_seq_num(c, seq_window, &c->last_cred);
		krb5_data_free(&c->last_cred);
		break;
	    }
	    c->done = 1;
	}
    }

    c->inprogress = 0;

    *client = c;
    return 0;
}


static void
set_funcs(kadm5_mit_context *c)
{
#define SET(C, F) (C)->funcs.F = kadm5_mit_ ## F
    SET(c, chpass_principal);
    SET(c, chpass_principal_with_key);
    SET(c, create_principal);
    SET(c, delete_principal);
    SET(c, destroy);
    SET(c, flush);
    SET(c, get_principal);
    SET(c, get_principals);
    SET(c, get_privs);
    SET(c, modify_principal);
    SET(c, randkey_principal);
    SET(c, rename_principal);
}

/*
 *
 */

kadm5_ret_t
kadm5_mit_init_with_password_ctx(krb5_context context,
				 const char *client_name,
				 const char *password,
				 kadm5_config_params *params,
				 unsigned long struct_version,
				 unsigned long api_version,
				 void **server_handle)
{
    kadm5_ret_t ret;
    kadm5_mit_context *ctx;
    struct grpc_client *c = NULL;
    char *colon;

    ctx = malloc(sizeof(*ctx));
    if(ctx == NULL)
	return ENOMEM;
    memset(ctx, 0, sizeof(*ctx));
    set_funcs(ctx);

    ctx->context = context;
    ctx->my_context = 0;
    krb5_add_et_list (context, initialize_kadm5_error_table_r);

    if (client_name) {
	ret = krb5_parse_name(ctx->context, client_name, &ctx->caller);
    } else {
	krb5_ccache id;

	ret = _kadm5_c_get_cache_principal(context, &id, &ctx->caller);
	if (ret) {
	    const char *user;

	    user = get_default_username ();
	    if(user == NULL) {
		krb5_set_error_message(context, KADM5_FAILURE, "Unable to find local user name");
		ret = KADM5_FAILURE;
	    } else {
		ret = krb5_make_principal(context, &ctx->caller,
					  NULL, user, "admin", NULL);
	    }
	} else if (id) {
	    krb5_cc_close(context, id);
	}
    }
    if(ret) {
	free(ctx);
	return ret;
    }

    if(params->mask & KADM5_CONFIG_REALM) {
	ret = 0;
	ctx->realm = strdup(params->realm);
	if (ctx->realm == NULL)
	    ret = ENOMEM;
    } else
	ret = krb5_get_default_realm(ctx->context, &ctx->realm);
    if (ret) {
	free(ctx);
	return ret;
    }

    if(params->mask & KADM5_CONFIG_ADMIN_SERVER)
	ctx->admin_server = strdup(params->admin_server);
    else {
	krb5_krbhst_handle handle = NULL;
	char host[MAXHOSTNAMELEN];

	ret = krb5_krbhst_init(context, ctx->realm, KRB5_KRBHST_ADMIN, &handle);
	if (ret == 0)
	    ret = krb5_krbhst_next_as_string(context, handle, host, sizeof(host));
	krb5_krbhst_free(context, handle);
	if (ret) {
	    free(ctx->realm);
	    free(ctx);
	    return ret;
	}
	ctx->admin_server = strdup(host);
    }

    if (ctx->admin_server == NULL) {
	free(ctx->realm);
	free(ctx);
	return ENOMEM;
    }
    colon = strchr (ctx->admin_server, ':');
    if (colon != NULL)
	*colon++ = '\0';

    ctx->kadmind_port = 0;

    if(params->mask & KADM5_CONFIG_KADMIND_PORT)
	ctx->kadmind_port = params->kadmind_port;
    else if (colon != NULL) {
	char *end;
	ctx->kadmind_port = htons(strtol (colon, &end, 0));
    }
    if (ctx->kadmind_port == 0)
	ctx->kadmind_port = krb5_getportbyname (context, "kerberos-adm",
						   "tcp", 749);

    ret = xdr_setup_connection(context, KADM5_KADMIN_SERVICE,
			       ctx->admin_server, ctx->kadmind_port, &c);
    if (ret) {
	free(ctx->realm);
	free(ctx);
	return ret;
    }

    ctx->gsscontext = c;

    *server_handle = ctx;
    return 0;
}
