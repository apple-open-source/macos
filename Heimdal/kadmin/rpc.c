/*
 * Copyright (c) 2008 Kungliga Tekniska Högskolan
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

#include "kadmin_locl.h"
#include <kadm5/private.h>

#include <gssapi.h>
#include <gssapi_krb5.h>
#include <gssapi_spnego.h>

#define CHECK(x)						\
    do {							\
	int __r;						\
	if ((__r = (x))) {					\
	    krb5_errx(dcontext, 1, "Failed (%d) on %s:%d",	\
		      __r, __FILE__, __LINE__);			\
	}							\
    } while(0)

#define EXPECT(x,expected)						\
    do {								\
	if ((x) != (expected)) {					\
	    krb5_errx(dcontext, 1,					\
		      "Got %d, was not the expected %d at %s:%d",	\
		      x, expected, __FILE__, __LINE__);			\
	}								\
    } while(0)

#define EXPECT_EGT(x,expected)				\
    do {						\
	if ((x) < (expected)) {				\
	    krb5_errx(dcontext, 1,			\
		      "Got %d that is < %d at %s:%d",	\
		      x, expected, __FILE__, __LINE__);	\
	}						\
    } while(0)

static krb5_context dcontext;

#define INSIST(x) CHECK(!(x))

#define VERSION2 0x12345702

#define LAST_FRAGMENT 0x80000000

#define RPC_VERSION 2
#define KADM_SERVER 2112
#define VVERSION 2
#define FLAVOR_GSS 6
#define 	FLAVOR_GSS_VERSION 1
#define 	SEQ_WINDOW_SIZE		1
#define FLAVOR_GSS_OLD 300001
#define 	FLAVOR_GSS_OLD_MIN_VERSION 3

enum {
    RPG_DATA = 0,
    RPG_INIT = 1,
    RPG_CONTINUE_INIT = 2,
    RPG_DESTROY = 3
};

enum {
    rpg_privacy = 3
};

/*
  struct chrand_ret {
  krb5_ui_4 api_version;
  kadm5_ret_t ret;
  int n_keys;
  krb5_keyblock *keys;
  };
*/


static int
parse_name(const unsigned char *p, size_t len,
	   const gss_OID oid, char **name)
{
    size_t l;
    
    if (len < 4)
	return 1;
    
    /* TOK_ID */
    if (memcmp(p, "\x04\x01", 2) != 0)
	return 1;
    len -= 2;
    p += 2;
    
    /* MECH_LEN */
    l = (p[0] << 8) | p[1];
    len -= 2;
    p += 2;
    if (l < 2 || len < l)
	return 1;
    
    /* oid wrapping */
    if (p[0] != 6 || p[1] != l - 2)
	return 1;
    p += 2;
    l -= 2;
    len -= 2;
    
    /* MECH */
    if (l != oid->length || memcmp(p, oid->elements, oid->length) != 0)
	return 1;
    len -= l;
    p += l;
    
    /* MECHNAME_LEN */
    if (len < 4)
	return 1;
    l = p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3];
    len -= 4;
    p += 4;
    
    /* MECH NAME */
    if (len != l)
	return 1;
    
    *name = malloc(l + 1);
    INSIST(*name != NULL);
    memcpy(*name, p, l);
    (*name)[l] = '\0';

    return 0;
}



static void
gss_error(krb5_context lcontext,
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
	krb5_warnx(lcontext, "%.*s",
		   (int)status_string.length,
		   (char *)status_string.value);
	gss_release_buffer (&new_stat, &status_string);
    } while (!GSS_ERROR(ret) && msg_ctx != 0);
}

static void
gss_print_errors (krb5_context lcontext, 
		  OM_uint32 maj_stat, OM_uint32 min_stat)
{
    gss_error(lcontext, GSS_C_NO_OID, GSS_C_GSS_CODE, maj_stat);
    gss_error(lcontext, GSS_C_NO_OID, GSS_C_MECH_CODE, min_stat);
}

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
	INSIST(slen == tlen);
	
	slen = krb5_storage_write(msg, buf, tlen);
	INSIST(slen == tlen);
	
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

/*
 *
 */

static void
proc_create_principal(kadm5_server_context *lcontext,
		      krb5_storage *in,
		      krb5_storage *out)
{
    uint32_t version, mask;
    kadm5_principal_ent_rec ent;
    krb5_error_code ret;
    char *password, *princ = NULL;

    memset(&ent, 0, sizeof(ent));

    CHECK(krb5_ret_uint32(in, &version));
    EXPECT_EGT(version, VERSION2);
    CHECK(_kadm5_xdr_ret_principal_ent(lcontext->context, in, &ent));
    INSIST(ent.principal != NULL);
    CHECK(krb5_unparse_name(lcontext->context, ent.principal, &princ));
    CHECK(krb5_ret_uint32(in, &mask));
    CHECK(_kadm5_xdr_ret_string_xdr(in, &password));

    INSIST(ent.principal);


    ret = _kadm5_acl_check_permission(lcontext, KADM5_PRIV_ADD, ent.principal);
    if (ret)
	goto fail;

    ret = kadm5_create_principal(lcontext, &ent, mask, password);

 fail:
    krb5_warn(lcontext->context, ret, "create principal: %s", princ ? princ : "<noprinc>");
    CHECK(krb5_store_uint32(out, VERSION2)); /* api version */
    CHECK(krb5_store_uint32(out, ret)); /* code */

    free(password);
    free(princ);
    kadm5_free_principal_ent(context, &ent);
}

/* get array of salt tuples */

static krb5_key_salt_tuple *
parse_ks_tuple(krb5_context lcontext, krb5_storage *in, uint32_t *n_ks_tuple)
{
    krb5_key_salt_tuple *tuples;
    uint32_t n;

    CHECK(krb5_ret_uint32(in, n_ks_tuple));
    INSIST(*n_ks_tuple < 1000);

    if (*n_ks_tuple == 0)
	return NULL;

    tuples = calloc(*n_ks_tuple, sizeof(tuples[0]));
    INSIST(tuples != NULL);

    for (n = 0; n < *n_ks_tuple; n++) {
	int32_t enctype, salttype;
	CHECK(krb5_ret_int32(in, &enctype));
	CHECK(krb5_ret_int32(in, &salttype));

	tuples[n].ks_enctype = (krb5_enctype)enctype;
	tuples[n].ks_salttype = (krb5_enctype)salttype;
    }
    return tuples;
}

static void
proc_create_principal3(kadm5_server_context *lcontext,
		       krb5_storage *in,
		       krb5_storage *out)
{
    uint32_t version, mask, n_ks_tuple;
    kadm5_principal_ent_rec ent;
    krb5_error_code ret;
    char *password, *princ = NULL;
    krb5_key_salt_tuple *ks_tuple;

    memset(&ent, 0, sizeof(ent));

    CHECK(krb5_ret_uint32(in, &version));
    EXPECT_EGT(version, VERSION2);
    CHECK(_kadm5_xdr_ret_principal_ent(lcontext->context, in, &ent));
    INSIST(ent.principal != NULL);
    CHECK(krb5_unparse_name(lcontext->context, ent.principal, &princ));
    CHECK(krb5_ret_uint32(in, &mask));
    ks_tuple = parse_ks_tuple(lcontext->context, in, &n_ks_tuple);
    CHECK(_kadm5_xdr_ret_string_xdr(in, &password));

    INSIST(ent.principal);

    ret = _kadm5_acl_check_permission(lcontext, KADM5_PRIV_ADD, ent.principal);
    if (ret)
	goto fail;

    ret = kadm5_create_principal_2(context, &ent, mask, n_ks_tuple, ks_tuple, password);

 fail:
    krb5_warn(lcontext->context, ret, "create principal: %s", princ ? princ : "<noprinc>");
    CHECK(krb5_store_uint32(out, VERSION2)); /* api version */
    CHECK(krb5_store_uint32(out, ret)); /* code */

    free(password);
    kadm5_free_principal_ent(lcontext, &ent);

    free(princ);
    if (ks_tuple)
	free(ks_tuple);
    kadm5_free_principal_ent(context, &ent);
}

static void
proc_delete_principal(kadm5_server_context *lcontext,
		      krb5_storage *in,
		      krb5_storage *out)
{
    uint32_t version;
    krb5_principal principal;
    krb5_error_code ret;
    char *princ = NULL;

    CHECK(krb5_ret_uint32(in, &version));
    EXPECT_EGT(version, VERSION2);
    CHECK(_kadm5_xdr_ret_principal_xdr(lcontext->context, in, &principal));
    CHECK(krb5_unparse_name(lcontext->context, principal, &princ));

    ret = _kadm5_acl_check_permission(lcontext, KADM5_PRIV_DELETE, principal);
    if (ret)
	goto fail;

    ret = kadm5_delete_principal(lcontext, principal);

 fail:
    krb5_warn(lcontext->context, ret, "delete principal: %s", princ ? princ : "<noprinc>");
    CHECK(krb5_store_uint32(out, VERSION2)); /* api version */
    CHECK(krb5_store_uint32(out, ret)); /* code */

    free(princ);
    krb5_free_principal(lcontext->context, principal);
}

static void
proc_modify_principal(kadm5_server_context *lcontext,
		      krb5_storage *in,
		      krb5_storage *out)
{
    uint32_t version, mask;
    kadm5_principal_ent_rec ent;
    krb5_error_code ret;

    CHECK(krb5_ret_uint32(in, &version));
    EXPECT_EGT(version, VERSION2);
    CHECK(_kadm5_xdr_ret_principal_ent(lcontext->context, in, &ent));
    INSIST(ent.principal != NULL);
    CHECK(krb5_ret_uint32(in, &mask));

    ret = _kadm5_acl_check_permission(lcontext, KADM5_PRIV_MODIFY,
				      ent.principal);
    if (ret)
	goto fail;

    ret = kadm5_modify_principal(lcontext, &ent, mask);

 fail:
    krb5_warn(lcontext->context, ret, "modify principal");
    CHECK(krb5_store_uint32(out, VERSION2)); /* api version */
    CHECK(krb5_store_uint32(out, ret)); /* code */

    kadm5_free_principal_ent(lcontext, &ent);
}

static void
proc_get_principal(kadm5_server_context *lcontext,
		   krb5_storage *in,
		   krb5_storage *out)
{
    uint32_t version, mask;
    krb5_principal principal;
    kadm5_principal_ent_rec ent;
    krb5_error_code ret;
    char *princ = NULL;

    memset(&ent, 0, sizeof(ent));

    CHECK(krb5_ret_uint32(in, &version));
    EXPECT_EGT(version, VERSION2);
    CHECK(_kadm5_xdr_ret_principal_xdr(lcontext->context, in, &principal));
    CHECK(krb5_unparse_name(lcontext->context, principal, &princ));
    CHECK(krb5_ret_uint32(in, &mask));

    ret = _kadm5_acl_check_permission(lcontext, KADM5_PRIV_GET, principal);
    if(ret)
	goto fail;

    mask |= KADM5_KVNO | KADM5_PRINCIPAL;

    ret = kadm5_get_principal(lcontext, principal, &ent, mask);

 fail:
    krb5_warn(lcontext->context, ret, "get principal: %s kvno %d",
	      princ ? princ : "<unknown>", (int)ent.kvno);

    CHECK(krb5_store_uint32(out, VERSION2)); /* api version */
    CHECK(krb5_store_uint32(out, ret)); /* code */
    if (ret == 0) {
	CHECK(_kadm5_xdr_store_principal_ent(lcontext->context, out, &ent));
    }
    krb5_free_principal(lcontext->context, principal);
    kadm5_free_principal_ent(lcontext, &ent);
}

static void
proc_chrand_principal_v2(kadm5_server_context *lcontext,
			 krb5_storage *in, 
			 krb5_storage *out)
{
    krb5_error_code ret;
    krb5_principal principal;
    uint32_t version;
    krb5_keyblock *new_keys;
    int n_keys;
    char *princ = NULL;

    CHECK(krb5_ret_uint32(in, &version));
    EXPECT_EGT(version, VERSION2);
    CHECK(_kadm5_xdr_ret_principal_xdr(lcontext->context, in, &principal));
    CHECK(krb5_unparse_name(lcontext->context, principal, &princ));

    ret = _kadm5_acl_check_permission(lcontext, KADM5_PRIV_CPW, principal);
    if(ret)
	goto fail;

    ret = kadm5_randkey_principal(lcontext, principal,
				  &new_keys, &n_keys);

 fail:
    krb5_warn(lcontext->context, ret, "rand key principal v2: %s",
	      princ ? princ : "<unknown>");

    CHECK(krb5_store_uint32(out, VERSION2)); /* api version */
    CHECK(krb5_store_uint32(out, ret));
    if (ret == 0) {
	size_t i;
	CHECK(krb5_store_int32(out, n_keys));

	for(i = 0; i < n_keys; i++){
	    CHECK(krb5_store_uint32(out, new_keys[i].keytype));
	    CHECK(_kadm5_xdr_store_data_xdr(out, new_keys[i].keyvalue));
	    krb5_free_keyblock_contents(lcontext->context, &new_keys[i]);
	}
	free(new_keys);
    }
    krb5_free_principal(lcontext->context, principal);
    if (princ)
	free(princ);
}

static void
proc_chrand_principal_v3(kadm5_server_context *lcontext,
			 krb5_storage *in, 
			 krb5_storage *out)
{
    krb5_error_code ret;
    krb5_principal principal;
    uint32_t version, keepold, n_ks_tuple;
    krb5_keyblock *new_keys;
    krb5_key_salt_tuple *ks_tuple;
    char *princ = NULL;
    int n_keys;

    CHECK(krb5_ret_uint32(in, &version));
    EXPECT_EGT(version, VERSION2);
    CHECK(_kadm5_xdr_ret_principal_xdr(lcontext->context, in, &principal));
    CHECK(krb5_unparse_name(lcontext->context, principal, &princ));
    CHECK(krb5_ret_uint32(in, &keepold));
    ks_tuple = parse_ks_tuple(lcontext->context, in, &n_ks_tuple);

    ret = _kadm5_acl_check_permission(lcontext, KADM5_PRIV_CPW, principal);
    if(ret)
	goto fail;

    ret = kadm5_randkey_principal_3(context, principal, keepold, n_ks_tuple, ks_tuple,
				    &new_keys, &n_keys);

 fail:
    krb5_warn(lcontext->context, ret, "rand key principal v3: %s",
	      princ ? princ : "<unknown>");

    CHECK(krb5_store_uint32(out, VERSION2)); /* api version */
    CHECK(krb5_store_uint32(out, ret));
    if (ret == 0) {
	size_t i;
	CHECK(krb5_store_int32(out, n_keys));

	for(i = 0; i < n_keys; i++){
	    CHECK(krb5_store_uint32(out, new_keys[i].keytype));
	    CHECK(_kadm5_xdr_store_data_xdr(out, new_keys[i].keyvalue));
	    krb5_free_keyblock_contents(lcontext->context, &new_keys[i]);
	}
	free(new_keys);
    }
    if (ks_tuple)
	free(ks_tuple);
    krb5_free_principal(lcontext->context, principal);
    if (princ)
	free(princ);
}


static void
proc_init(kadm5_server_context *lcontext,
	  krb5_storage *in,
	  krb5_storage *out)
{
    CHECK(krb5_store_uint32(out, VERSION2)); /* api version */
    CHECK(krb5_store_uint32(out, 0)); /* code */
    CHECK(krb5_store_uint32(out, 0)); /* code */
}

static void
proc_get_policy(kadm5_server_context *lcontext,
		krb5_storage *in,
		krb5_storage *out)
{
    CHECK(krb5_store_uint32(out, VERSION2)); /* api version */
    CHECK(krb5_store_uint32(out, KADM5_AUTH_GET)); /* code */
}


struct proc {
    char *name;
    void (*func)(kadm5_server_context *, krb5_storage *, krb5_storage *);
} procs[] = {
    { "NULL", NULL },
    { "create principal", proc_create_principal },
    { "delete principal", proc_delete_principal },
    { "modify principal", proc_modify_principal },
    { "rename principal", NULL },
    { "get principal", proc_get_principal },
    { "chpass principal", NULL },
    { "chrand principal v2", proc_chrand_principal_v2 },
    { "create policy", NULL },
    { "delete policy", NULL },
    { "modify policy", NULL },
    { "get policy", proc_get_policy },
    { "get privs", NULL },
    { "init", proc_init },
    { "get principals", NULL },
    { "get polices", NULL },
    { "setkey principal", NULL },
    { "setkey principal v4", NULL },
    { "create principal v3", proc_create_principal3 },
    { "chpass principal v3", NULL },
    { "chrand principal v3", proc_chrand_principal_v3 },
    { "setkey principal v3", NULL }
};

static krb5_error_code
copyheader(krb5_storage *sp, krb5_data *data)
{
    off_t off;
    ssize_t sret;

    off = krb5_storage_seek(sp, 0, SEEK_CUR);

    CHECK(krb5_data_alloc(data, off));
    INSIST(off == data->length);
    krb5_storage_seek(sp, 0, SEEK_SET);
    sret = krb5_storage_read(sp, data->data, data->length);
    INSIST(sret == off);
    INSIST(off == krb5_storage_seek(sp, 0, SEEK_CUR));

    return 0;
}

struct gctx {
    krb5_data handle;
    gss_ctx_id_t ctx;
    uint32_t seq_num;
    uint32_t protocol;
    int done;
    int inprogress;
    void *server_handle;
    void (*verify_header)(struct gctx *, struct _kadm5_xdr_call_header *, krb5_data *);
    void (*handle_protocol)(struct gctx *, struct _kadm5_xdr_call_header *,
			    krb5_storage *, krb5_storage *);
    void (*reply)(struct gctx *, krb5_storage *, krb5_storage *);

};

static void
setup_context(struct gctx *gctx, gss_name_t src_name)
{
    kadm5_config_params realm_params;
    gss_buffer_desc buf;
    OM_uint32 maj_stat, min_stat, junk;
    krb5_error_code ret;
    char *client;
	    
    INSIST(gctx->done);

    memset(&realm_params, 0, sizeof(realm_params));
	    
    maj_stat = gss_export_name(&min_stat, src_name, &buf);
    EXPECT(maj_stat, GSS_S_COMPLETE);
    EXPECT(min_stat, 0);
	    
    CHECK(parse_name(buf.value, buf.length, 
		     GSS_KRB5_MECHANISM, &client));
    
    gss_release_buffer(&junk, &buf);
    
    krb5_warnx(context, "%s connected", client);
    
    ret = kadm5_s_init_with_password_ctx(context,
					 client,
					 NULL,
					 KADM5_ADMIN_SERVICE,
					 &realm_params,
					 0, 0,
					 &gctx->server_handle);
    EXPECT(ret, 0);
}

/*
 * GSS flavor
 */

static void
xpcgss_verify_header(struct gctx *gctx, struct _kadm5_xdr_call_header *chdr, krb5_data *header)
{
    OM_uint32 maj_stat, min_stat;
    gss_buffer_desc gin, gout;

    EXPECT(chdr->verf.flavor, gctx->protocol);

    /* from first byte to last of credential */
    gin.value = header->data;
    gin.length = header->length;
    gout.value = chdr->verf.data.data;
    gout.length = chdr->verf.data.length;
    
    maj_stat = gss_verify_mic(&min_stat, gctx->ctx, &gin, &gout, NULL);
    EXPECT(maj_stat, GSS_S_COMPLETE);
}

static void
rpcgss_handle_protocol(struct gctx *gctx,
		       struct _kadm5_xdr_call_header *chdr,
		       krb5_storage *msg,
		       krb5_storage *dreply)
{
    OM_uint32 maj_stat, min_stat, junk;
    gss_buffer_desc gin, gout;
    struct _kadm5_xdr_gcred gcred;

    memset(&gcred, 0, sizeof(gcred));

    CHECK(_kadm5_xdr_ret_gcred(&chdr->cred.data, &gcred));
    EXPECT(gcred.version, FLAVOR_GSS_VERSION);

    switch(gcred.proc) {
    case RPG_DATA: {
	krb5_data data;
	int conf_state;
	uint32_t seq;
	krb5_storage *sp;
	
	EXPECT(gcred.service, rpg_privacy);
	
	INSIST(gctx->done);
	
	INSIST(krb5_data_cmp(&gcred.handle, &gctx->handle) == 0);
	
	CHECK(_kadm5_xdr_ret_data_xdr(msg, &data));
	
	gin.value = data.data;
	gin.length = data.length;
	
	maj_stat = gss_unwrap(&min_stat, gctx->ctx, &gin, &gout,
			      &conf_state, NULL);
	krb5_data_free(&data);
	INSIST(maj_stat == GSS_S_COMPLETE);
	INSIST(conf_state != 0);
	
	sp = krb5_storage_from_mem(gout.value, gout.length);
	INSIST(sp != NULL);
	
	CHECK(krb5_ret_uint32(sp, &seq));
	EXPECT(seq, gcred.seq_num);
	
	/*
	 * Check sequence number
	 */
	INSIST(seq > gctx->seq_num);
	gctx->seq_num = seq;
	
	/* 
	 * If context is setup, priv data have the seq_num stored
	 * first in the block, so add it here before users data is
	 * added.
	 */
	CHECK(krb5_store_uint32(dreply, gctx->seq_num));
	
	if (chdr->proc >= sizeof(procs)/sizeof(procs[0])) {
	    krb5_warnx(context, "proc number out of array");
	} else if (procs[chdr->proc].func == NULL) {
	    krb5_warnx(context, "proc '%s' never implemented", 
		       procs[chdr->proc].name);
	} else {
	    krb5_warnx(context, "proc %s", procs[chdr->proc].name);
	    INSIST(gctx->server_handle != NULL);
	    (*procs[chdr->proc].func)(gctx->server_handle, sp, dreply);
	}
	krb5_storage_free(sp);
	gss_release_buffer(&min_stat, &gout);
	
	break;
    }
    case RPG_INIT:
	INSIST(gctx->inprogress == 0);
	INSIST(gctx->ctx == NULL);
	
	gctx->inprogress = 1;
	/* FALL THOUGH */
    case RPG_CONTINUE_INIT: {
	gss_name_t src_name = GSS_C_NO_NAME;
	krb5_data in;
	
	INSIST(gctx->inprogress);
	
	CHECK(_kadm5_xdr_ret_data_xdr(msg, &in));
	
	gin.value = in.data;
	gin.length = in.length;
	gout.value = NULL;
	gout.length = 0;
	
	maj_stat = gss_accept_sec_context(&min_stat,
					  &gctx->ctx, 
					  GSS_C_NO_CREDENTIAL,
					  &gin,
					  GSS_C_NO_CHANNEL_BINDINGS,
					  &src_name,
					  NULL,
					  &gout,
					  NULL,
					  NULL,
					  NULL);
	if (GSS_ERROR(maj_stat)) {
	    gss_print_errors(context, maj_stat, min_stat);
	    krb5_errx(context, 1, "gss error, exit");
	}
	if ((maj_stat & GSS_S_CONTINUE_NEEDED) == 0) {
	    
	    gctx->done = 1;
	    gctx->verify_header = xpcgss_verify_header;
	    
	    setup_context(gctx, src_name);
	}
	
	INSIST(gctx->ctx != GSS_C_NO_CONTEXT);
	
	CHECK(_kadm5_xdr_store_gss_init_res(dreply, gctx->handle,
					    maj_stat, min_stat, SEQ_WINDOW_SIZE, &gout));
	if (gout.value)
	    gss_release_buffer(&junk, &gout);
	if (src_name)
	    gss_release_name(&junk, &src_name);
	
	break;
    }
    case RPG_DESTROY:
	krb5_errx(context, 1, "client destroyed gss context");
    default:
	krb5_errx(context, 1, "client sent unknown gsscode %d", 
		  (int)gcred.proc);
    }
    
    krb5_data_free(&gcred.handle);
}

static void
rpcgss_reply(struct gctx *gctx, krb5_storage *dreply, krb5_storage *reply)
{
    uint32_t seqnum = htonl(gctx->seq_num);
    gss_buffer_desc gin, gout;
    OM_uint32 maj_stat, min_stat, junk;
    krb5_data data;

    /*
     * The first checksum is really the checksum of the
     * seq_window in the rpc_gss_init_res packet, lets agree
     * with that.
     */
    if (gctx->seq_num == 0)
	seqnum = htonl(SEQ_WINDOW_SIZE);

    gin.value = &seqnum;
    gin.length = sizeof(seqnum);

    maj_stat = gss_get_mic(&min_stat, gctx->ctx, 0, &gin, &gout);
    INSIST(maj_stat == GSS_S_COMPLETE);

    data.data = gout.value;
    data.length = gout.length;

    CHECK(krb5_store_uint32(reply, FLAVOR_GSS));
    CHECK(_kadm5_xdr_store_data_xdr(reply, data));
    gss_release_buffer(&junk, &gout);

    CHECK(krb5_store_uint32(reply, 0)); /* SUCCESS */

    CHECK(krb5_storage_to_data(dreply, &data));

    if (gctx->inprogress) {
	ssize_t sret;
	gctx->inprogress = 0;
	sret = krb5_storage_write(reply, data.data, data.length);
	INSIST(sret == data.length);
	krb5_data_free(&data);
    } else {
	int conf_state;

	gin.value = data.data;
	gin.length = data.length;
		
	maj_stat = gss_wrap(&min_stat, gctx->ctx, 1, 0,
			    &gin, &conf_state, &gout);
	INSIST(maj_stat == GSS_S_COMPLETE);
	INSIST(conf_state != 0);
	krb5_data_free(&data);
	
	data.data = gout.value;
	data.length = gout.length;
	
	_kadm5_xdr_store_data_xdr(reply, data);
	gss_release_buffer(&min_stat, &gout);
    }
}


/*
 * GSSAPI flavor
 */

enum {
    RPGA_INIT = 1,
    RPGA_CONTINUE_INIT = 2,
    RPGA_MSG = 3,
    RPGA_DESTORY = 4
};

static void
xpcgssapi_verify_header(struct gctx *gctx, struct _kadm5_xdr_call_header *chdr, krb5_data *header)
{
#if 0
    OM_uint32 maj_stat, min_stat;
    gss_buffer_desc gin, gout;

    EXPECT(chdr->verf.flavor, gctx->protocol);

    /* from first byte to last of credential */
    gin.value = header->data;
    gin.length = header->length;
    gout.value = chdr->verf.data.data;
    gout.length = chdr->verf.data.length;
    
    maj_stat = gss_verify_mic(&min_stat, gctx->ctx, &gin, &gout, NULL);
    EXPECT(maj_stat, GSS_S_COMPLETE);
#endif
}

static void
rpcgssapi_handle_protocol(struct gctx *gctx,
			  struct _kadm5_xdr_call_header *chdr,
			  krb5_storage *msg,
			  krb5_storage *dreply)
{
    struct _kadm5_xdr_gacred gacred;
    OM_uint32 maj_stat, min_stat, junk;
    gss_buffer_desc gin, gout, gseq;

    CHECK(_kadm5_xdr_ret_gacred(&chdr->cred.data, &gacred));
    gseq.length = 0;
    gseq.value = NULL;

    if (gctx->done == 0 && chdr->proc == RPGA_INIT) {
	INSIST(gacred.handle.length == 0);
	INSIST(gctx->handle.length == 0);

	CHECK(krb5_data_alloc(&gctx->handle, 16));
	CCRandomCopyBytes(kCCRandomDefault, gctx->handle.data, gctx->handle.length);

    } else {
	INSIST(gacred.handle.length != 0);
	INSIST(krb5_data_cmp(&gacred.handle, &gctx->handle) == 0);
    }


    if (gctx->done == 0) {
	uint32_t version;
	krb5_data token, out;

	CHECK(krb5_ret_uint32(msg, &version));
	CHECK(_kadm5_xdr_ret_data_xdr(msg, &token));

	switch (chdr->proc) {
	case RPGA_INIT:
	    INSIST(gctx->inprogress == 0);
	    INSIST(gctx->ctx == NULL);
	
	    INSIST(version == 3 || version == 4);

	    gctx->inprogress = 1;

	    /* FALL THOUGH */
	case RPGA_CONTINUE_INIT: {
	    gss_name_t src_name = GSS_C_NO_NAME;
	
	    INSIST(gctx->inprogress);
	
	    gin.value = token.data;
	    gin.length = token.length;
	    gout.value = NULL;
	    gout.length = 0;
	    
	    maj_stat = gss_accept_sec_context(&min_stat,
					      &gctx->ctx, 
					      GSS_C_NO_CREDENTIAL,
					      &gin,
					      GSS_C_NO_CHANNEL_BINDINGS,
					      &src_name,
					      NULL,
					      &gout,
					      NULL,
					      NULL,
					      NULL);
	    if (GSS_ERROR(maj_stat)) {
		gss_print_errors(context, maj_stat, min_stat);
		krb5_errx(context, 1, "gss error, exit");
	    }
	    if ((maj_stat & GSS_S_CONTINUE_NEEDED) == 0) {
		uint32_t netseqnum;

		gctx->done = 1;
		gctx->verify_header = xpcgssapi_verify_header;

		setup_context(gctx, src_name);

		CCRandomCopyBytes(kCCRandomDefault, 
				  &gctx->seq_num, sizeof(gctx->seq_num));

		netseqnum = htonl(gctx->seq_num);
		gin.value = &netseqnum;
		gin.length = sizeof(netseqnum);

		CHECK(gss_wrap(&junk, gctx->ctx, 0, GSS_C_QOP_DEFAULT, &gin, NULL, &gseq));
	    }

	    INSIST(gctx->ctx != GSS_C_NO_CONTEXT);

	    /* reply argument */

	    CHECK(krb5_store_uint32(dreply, version));
	    CHECK(_kadm5_xdr_store_data_xdr(dreply, gctx->handle));
	    CHECK(krb5_store_uint32(dreply, maj_stat));
	    CHECK(krb5_store_uint32(dreply, min_stat));

	    out.data = gout.value;
	    out.length = gout.length;
	    CHECK(_kadm5_xdr_store_data_xdr(dreply, out));

	    out.data = gseq.value;
	    out.length = gseq.length;
	    CHECK(_kadm5_xdr_store_data_xdr(dreply, out));

	    if (gout.value)
		gss_release_buffer(&junk, &gout);
	    if (gseq.value)
		gss_release_buffer(&junk, &gseq);
	    if (src_name)
		gss_release_name(&junk, &src_name);

	    break;
	}
	default:
	    krb5_errx(context, 1, "unsupported init message %d", (int)chdr->proc);
	}
	krb5_data_free(&token);

    } else if (gacred.auth_msg) {
	if (chdr->proc == RPGA_MSG)
	    krb5_warnx(context, "auth message MSG not supported");
	else if (chdr->proc == RPGA_DESTORY)
	    krb5_warnx(context, "auth message DESTROY not supported");
	else
	    krb5_errx(context, 1, "auth message not supported: %d", (int)chdr->proc);
    } else {
	krb5_storage *sp;
	krb5_data data;
	int conf_state = 0;
	uint32_t seq;

	INSIST(gctx->done);
	
	CHECK(_kadm5_xdr_ret_data_xdr(msg, &data));
	
	gin.value = data.data;
	gin.length = data.length;
	
	maj_stat = gss_unwrap(&min_stat, gctx->ctx, &gin, &gout,
			      &conf_state, NULL);
	krb5_data_free(&data);
	INSIST(maj_stat == GSS_S_COMPLETE);
	INSIST(conf_state != 0);
	
	sp = krb5_storage_from_mem(gout.value, gout.length);
	INSIST(sp != NULL);
	
	CHECK(krb5_ret_uint32(sp, &seq));
	
	/*
	 * Check sequence number
	 */
	INSIST(seq == gctx->seq_num + 1);
	gctx->seq_num = seq + 1;
	
	/* 
	 * If context is setup, priv data have the seq_num stored
	 * first in the block, so add it here before users data is
	 * added.
	 */
	CHECK(krb5_store_uint32(dreply, gctx->seq_num));
	
	if (chdr->proc >= sizeof(procs)/sizeof(procs[0])) {
	    krb5_warnx(context, "proc number out of array");
	} else if (procs[chdr->proc].func == NULL) {
	    krb5_warnx(context, "proc '%s' never implemented", 
		       procs[chdr->proc].name);
	} else {
	    krb5_warnx(context, "proc %s", procs[chdr->proc].name);
	    INSIST(gctx->server_handle != NULL);
	    (*procs[chdr->proc].func)(gctx->server_handle, sp, dreply);
	}
	krb5_storage_free(sp);
	gss_release_buffer(&min_stat, &gout);
    }

    krb5_data_free(&gacred.handle);
}

static void
rpcgssapi_reply(struct gctx *gctx, krb5_storage *dreply, krb5_storage *reply)
{
    uint32_t seqnum = htonl(gctx->seq_num);
    gss_buffer_desc gin, gout;
    OM_uint32 maj_stat, min_stat, junk;
    krb5_data data;

    gin.value = &seqnum;
    gin.length = sizeof(seqnum);

    maj_stat = gss_wrap(&min_stat, gctx->ctx, 0, GSS_C_QOP_DEFAULT, &gin, NULL, &gout);
    INSIST(maj_stat == GSS_S_COMPLETE);

    data.data = gout.value;
    data.length = gout.length;

    CHECK(krb5_store_uint32(reply, FLAVOR_GSS_OLD));
    CHECK(_kadm5_xdr_store_data_xdr(reply, data));
    gss_release_buffer(&junk, &gout);

    CHECK(krb5_store_uint32(reply, 0)); /* SUCCESS */

    CHECK(krb5_storage_to_data(dreply, &data));

    if (gctx->inprogress) {
	ssize_t sret;
	gctx->inprogress = 0;
	sret = krb5_storage_write(reply, data.data, data.length);
	INSIST(sret == data.length);
	krb5_data_free(&data);
    } else {
	int conf_state;

	gin.value = data.data;
	gin.length = data.length;
		
	maj_stat = gss_wrap(&min_stat, gctx->ctx, 1, 0,
			    &gin, &conf_state, &gout);
	INSIST(maj_stat == GSS_S_COMPLETE);
	INSIST(conf_state != 0);
	krb5_data_free(&data);
	
	data.data = gout.value;
	data.length = gout.length;
	
	_kadm5_xdr_store_data_xdr(reply, data);
	gss_release_buffer(&min_stat, &gout);
    }
}


/*
 *
 */


static int
process_stream(krb5_context lcontext, 
	       unsigned char *buf, size_t ilen,
	       krb5_storage *sp)
{
    krb5_error_code ret;
    krb5_storage *msg, *reply, *dreply;
    struct gctx gctx;

    memset(&gctx, 0, sizeof(gctx));

    msg = krb5_storage_emem();
    reply = krb5_storage_emem();
    dreply = krb5_storage_emem();

    /*
     * First packet comes partly from the caller
     */

    INSIST(ilen >= 4);

    while (1) {
	struct _kadm5_xdr_call_header chdr;
	uint32_t mtype;
	krb5_data headercopy;

	krb5_storage_truncate(dreply, 0);
	krb5_storage_truncate(reply, 0);
	krb5_storage_truncate(msg, 0);

	krb5_data_zero(&headercopy);
	memset(&chdr, 0, sizeof(chdr));

	/*
	 * This is very icky to handle the the auto-detection between
	 * the Heimdal protocol and the MIT ONC-RPC based protocol.
	 */

	if (ilen) {
	    int last_fragment;
	    unsigned long len;
	    ssize_t slen;
	    unsigned char tmp[4];

	    if (ilen < 4) {
		memcpy(tmp, buf, ilen);
		slen = krb5_storage_read(sp, tmp + ilen, sizeof(tmp) - ilen);
		INSIST(slen == sizeof(tmp) - ilen);

		ilen = sizeof(tmp);
		buf = tmp;
	    }
	    INSIST(ilen >= 4);
	    
	    _krb5_get_int(buf, &len, 4);
	    last_fragment = (len & LAST_FRAGMENT) != 0;
	    len &= ~LAST_FRAGMENT;
	    
	    ilen -= 4;
	    buf += 4;

	    if (ilen) {
		if (len < ilen) {
		    slen = krb5_storage_write(msg, buf, len);
		    INSIST(slen == len);
		    ilen -= len;
		    len = 0;
		} else {
		    slen = krb5_storage_write(msg, buf, ilen);
		    INSIST(slen == ilen);
		    len -= ilen;
		}
	    }

	    CHECK(read_data(sp, msg, len));
	    
	    if (!last_fragment) {
		ret = collect_fragments(sp, msg);
		if (ret == HEIM_ERR_EOF)
		    krb5_errx(lcontext, 0, "client disconnected");
		INSIST(ret == 0);
	    }
	} else {

	    ret = collect_fragments(sp, msg);
	    if (ret == HEIM_ERR_EOF)
		krb5_errx(lcontext, 0, "client disconnected");
	    INSIST(ret == 0);
	}
	krb5_storage_seek(msg, 0, SEEK_SET);

	CHECK(krb5_ret_uint32(msg, &chdr.xid));
	CHECK(krb5_ret_uint32(msg, &mtype));
	CHECK(krb5_ret_uint32(msg, &chdr.rpcvers));
	CHECK(krb5_ret_uint32(msg, &chdr.prog));
	CHECK(krb5_ret_uint32(msg, &chdr.vers));
	CHECK(krb5_ret_uint32(msg, &chdr.proc));
	CHECK(_kadm5_xdr_ret_auth_opaque(msg, &chdr.cred));
	CHECK(copyheader(msg, &headercopy));
	CHECK(_kadm5_xdr_ret_auth_opaque(msg, &chdr.verf));

	EXPECT(chdr.rpcvers, RPC_VERSION);
	EXPECT(chdr.prog, KADM_SERVER);
	EXPECT(chdr.vers, VVERSION);
	if (gctx.protocol == 0) {
	    gctx.protocol = chdr.cred.flavor;

	    INSIST(gctx.handle_protocol == NULL);

	    switch(gctx.protocol) {
	    case FLAVOR_GSS:
		gctx.handle_protocol = rpcgss_handle_protocol;
		gctx.reply = rpcgss_reply;
		break;
	    case FLAVOR_GSS_OLD:
		gctx.handle_protocol = rpcgssapi_handle_protocol;
		gctx.reply = rpcgssapi_reply;
		break;
	    default:
		krb5_errx(lcontext, 0, "unsupported protocol version: %d", (int)gctx.protocol);
	    }

	} else {
	    EXPECT(chdr.cred.flavor, gctx.protocol);
	}

	if (gctx.verify_header)
	    gctx.verify_header(&gctx, &chdr, &headercopy);

	gctx.handle_protocol(&gctx, &chdr, msg, dreply);

	krb5_data_free(&chdr.cred.data);
	krb5_data_free(&chdr.verf.data);
	krb5_data_free(&headercopy);

	CHECK(krb5_store_uint32(reply, chdr.xid));
	CHECK(krb5_store_uint32(reply, 1)); /* REPLY */
	CHECK(krb5_store_uint32(reply, 0)); /* MSG_ACCEPTED */

	if (!gctx.done) {
	    krb5_data data;

	    CHECK(krb5_store_uint32(reply, 0)); /* flavor_none */
	    CHECK(krb5_store_uint32(reply, 0)); /* length */

	    CHECK(krb5_store_uint32(reply, 0)); /* SUCCESS */

	    CHECK(krb5_storage_to_data(dreply, &data));
	    INSIST(krb5_storage_write(reply, data.data, data.length) == data.length);
	    krb5_data_free(&data);
	} else {
	    INSIST(gctx.reply != NULL);

	    gctx.reply(&gctx, dreply, reply);
	}

	{
	    krb5_data data;
	    ssize_t sret;
	    CHECK(krb5_storage_to_data(reply, &data));
	    CHECK(krb5_store_uint32(sp, ((uint32_t)data.length) | LAST_FRAGMENT));
	    sret = krb5_storage_write(sp, data.data, data.length);
	    INSIST(sret == data.length);
	    krb5_data_free(&data);
	}

    }
}

int
handle_mit(krb5_context lcontext, void *buf, size_t len, krb5_socket_t sock)
{
    krb5_storage *sp;

    dcontext = context;

    sp = krb5_storage_from_fd(sock);
    INSIST(sp != NULL);
    
    process_stream(lcontext, buf, len, sp);

    return 0;
}
