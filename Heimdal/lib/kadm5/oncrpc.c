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

#define LARGETIME 0x1fffffff

#define CHECK(x)							\
	do {								\
		int __r;						\
		if ((__r = (x))) {					\
			abort();					\
		}							\
	} while(0)

#define INSIST(x) CHECK(!(x))


krb5_error_code
_kadm5_xdr_store_data_xdr(krb5_storage *sp, krb5_data data)
{
    krb5_error_code ret;
    ssize_t sret;
    size_t res;

    ret = krb5_store_data(sp, data);
    if (ret)
	return ret;
    res = 4 - (data.length % 4);
    if (res != 4) {
	static const char zero[4] = { 0, 0, 0, 0 };

	sret = krb5_storage_write(sp, zero, res);
	if(sret < 0 || (size_t)sret != res)
	    return (sret < 0)? errno : krb5_storage_get_eof_code(sp);
    }
    return 0;
}

krb5_error_code
_kadm5_xdr_ret_data_xdr(krb5_storage *sp, krb5_data *data)
{
    krb5_error_code ret;
    ssize_t sret;

    ret = krb5_ret_data(sp, data);
    if (ret)
	return ret;

    if ((data->length % 4) != 0) {
	char buf[4];
	size_t res;

	res = 4 - (data->length % 4);
	if (res != 4) {
	    sret = krb5_storage_read(sp, buf, res);
	    if(sret < 0 || (size_t)sret != res)
		return (sret < 0)? errno : krb5_storage_get_eof_code(sp);
	}
    }
    return 0;
}

krb5_error_code
_kadm5_xdr_ret_auth_opaque(krb5_storage *msg, struct _kadm5_xdr_opaque_auth *ao)
{
    krb5_error_code ret;
    ret = krb5_ret_uint32(msg, &ao->flavor);
    if (ret) return ret;
    ret = _kadm5_xdr_ret_data_xdr(msg, &ao->data);
    return ret;
}

krb5_error_code
_kadm5_xdr_store_auth_opaque(krb5_storage *msg, struct _kadm5_xdr_opaque_auth *ao)
{
    krb5_error_code ret;
    ret = krb5_store_uint32(msg, ao->flavor);
    if (ret) return ret;
    ret = _kadm5_xdr_store_data_xdr(msg, ao->data);
    return ret;
}

krb5_error_code
_kadm5_xdr_ret_gcred(krb5_data *data, struct _kadm5_xdr_gcred *gcred)
{
    krb5_storage *sp;

    memset(gcred, 0, sizeof(*gcred));

    sp = krb5_storage_from_data(data);
    INSIST(sp != NULL);

    CHECK(krb5_ret_uint32(sp, &gcred->version));
    CHECK(krb5_ret_uint32(sp, &gcred->proc));
    CHECK(krb5_ret_uint32(sp, &gcred->seq_num));
    CHECK(krb5_ret_uint32(sp, &gcred->service));
    CHECK(_kadm5_xdr_ret_data_xdr(sp, &gcred->handle));

    krb5_storage_free(sp);

    return 0;
}

krb5_error_code
_kadm5_xdr_store_gcred(struct _kadm5_xdr_gcred *gcred, krb5_data *data)
{
    krb5_error_code ret;
    krb5_storage *sp;

    krb5_data_zero(data);

    sp = krb5_storage_emem();
    INSIST(sp != NULL);

    CHECK(krb5_store_uint32(sp, gcred->version));
    CHECK(krb5_store_uint32(sp, gcred->proc));
    CHECK(krb5_store_uint32(sp, gcred->seq_num));
    CHECK(krb5_store_uint32(sp, gcred->service));
    CHECK(_kadm5_xdr_store_data_xdr(sp, gcred->handle));

    ret = krb5_storage_to_data(sp, data);
    krb5_storage_free(sp);

    return ret;
}


krb5_error_code
_kadm5_xdr_store_gss_init_res(krb5_storage *sp, krb5_data handle,
		   OM_uint32 maj_stat, OM_uint32 min_stat,
		   uint32_t seq_window, gss_buffer_t gout)
{
    krb5_error_code ret;
    krb5_data out;

    out.data = gout->value;
    out.length = gout->length;

    ret = _kadm5_xdr_store_data_xdr(sp, handle);
    if (ret) return ret;
    ret = krb5_store_uint32(sp, maj_stat);
    if (ret) return ret;
    ret = krb5_store_uint32(sp, min_stat);
    if (ret) return ret;
    ret = krb5_store_uint32(sp, seq_window);
    if (ret) return ret;
    ret = _kadm5_xdr_store_data_xdr(sp, out);
    return ret;
}

krb5_error_code
_kadm5_xdr_ret_gss_init_res(krb5_storage *sp, krb5_data *handle,
			    OM_uint32 *maj_stat, OM_uint32 *min_stat,
			    uint32_t *seq_window, krb5_data *out)
{
    krb5_error_code ret;

    ret = _kadm5_xdr_ret_data_xdr(sp, handle);
    if (ret) return ret;
    ret = krb5_ret_uint32(sp, maj_stat);
    if (ret) return ret;
    ret = krb5_ret_uint32(sp, min_stat);
    if (ret) return ret;
    ret = krb5_ret_uint32(sp, seq_window);
    if (ret) return ret;
    ret = _kadm5_xdr_ret_data_xdr(sp, out);
    return ret;
}


krb5_error_code
_kadm5_xdr_ret_gacred(krb5_data *data, struct _kadm5_xdr_gacred *gacred)
{
    krb5_storage *sp;

    memset(gacred, 0, sizeof(*gacred));

    sp = krb5_storage_from_data(data);
    INSIST(sp != NULL);

    CHECK(krb5_ret_uint32(sp, &gacred->version));
    CHECK(krb5_ret_uint32(sp, &gacred->auth_msg));
    CHECK(_kadm5_xdr_ret_data_xdr(sp, &gacred->handle));

    krb5_storage_free(sp);

    return 0;
}


krb5_error_code
_kadm5_xdr_store_string_xdr(krb5_storage *sp, const char *str)
{
    krb5_data c;
    if (str) {
	c.data = rk_UNCONST(str);
	c.length = strlen(str) + 1;
    } else
	krb5_data_zero(&c);
	
    return _kadm5_xdr_store_data_xdr(sp, c);
}

krb5_error_code
_kadm5_xdr_ret_string_xdr(krb5_storage *sp, char **str)
{
    krb5_data c;
    *str = NULL;
    CHECK(_kadm5_xdr_ret_data_xdr(sp, &c));
    if (c.length) {
	*str = malloc(c.length + 1);
	INSIST(*str != NULL);
	memcpy(*str, c.data, c.length);
	(*str)[c.length] = '\0';
    }
    krb5_data_free(&c);
    return 0;
}

krb5_error_code
_kadm5_xdr_store_principal_xdr(krb5_context context,
		    krb5_storage *sp,
		    krb5_principal p)
{
    char *str;
    CHECK(krb5_unparse_name(context, p, &str));
    CHECK(_kadm5_xdr_store_string_xdr(sp, str));
    free(str);
    return 0;
}

krb5_error_code
_kadm5_xdr_ret_principal_xdr(krb5_context context,
		  krb5_storage *sp,
		  krb5_principal *p)
{
    char *str;
    *p = NULL;
    CHECK(_kadm5_xdr_ret_string_xdr(sp, &str));
    if (str) {
	CHECK(krb5_parse_name(context, str, p));
	free(str);
    }
    return 0;
}

krb5_error_code
_kadm5_xdr_store_principal_ent(krb5_context context,
		    krb5_storage *sp,
		    kadm5_principal_ent_rec *ent)
{
    int32_t t;
    size_t i;

    CHECK(_kadm5_xdr_store_principal_xdr(context, sp, ent->principal));
    CHECK(krb5_store_uint32(sp, (uint32_t)ent->princ_expire_time));
    CHECK(krb5_store_uint32(sp, (uint32_t)ent->pw_expiration));
    CHECK(krb5_store_uint32(sp, (uint32_t)ent->last_pwd_change));
    t = (int32_t)ent->max_life;
    if (t == 0)
	t = LARGETIME;
    CHECK(krb5_store_uint32(sp, t));
    CHECK(krb5_store_int32(sp, ent->mod_name == NULL));
    if (ent->mod_name)
	CHECK(_kadm5_xdr_store_principal_xdr(context, sp, ent->mod_name));
    CHECK(krb5_store_uint32(sp, (uint32_t)ent->mod_date));
    CHECK(krb5_store_uint32(sp, ent->attributes));
    CHECK(krb5_store_uint32(sp, ent->kvno));
    CHECK(krb5_store_uint32(sp, ent->mkvno));
    CHECK(_kadm5_xdr_store_string_xdr(sp, ent->policy));
    CHECK(krb5_store_int32(sp, ent->aux_attributes));
    t = (int32_t)ent->max_renewable_life;
    if (t == 0)
	t = LARGETIME;
    CHECK(krb5_store_int32(sp, t));
    CHECK(krb5_store_int32(sp, (int32_t)ent->last_success));
    CHECK(krb5_store_int32(sp, (int32_t)ent->last_failed));
    CHECK(krb5_store_int32(sp, ent->fail_auth_count));
    CHECK(krb5_store_int32(sp, ent->n_key_data));
    CHECK(krb5_store_int32(sp, ent->n_tl_data));
    CHECK(krb5_store_int32(sp, ent->n_tl_data == 0));
    if (ent->n_tl_data) {
	krb5_tl_data *tp;

	for (tp = ent->tl_data; tp; tp = tp->tl_data_next) {
	    krb5_data c;
	    c.length = tp->tl_data_length;
	    c.data = tp->tl_data_contents;

	    CHECK(krb5_store_int32(sp, 0)); /* last item */
	    CHECK(krb5_store_int32(sp, tp->tl_data_type));
	    CHECK(_kadm5_xdr_store_data_xdr(sp, c));
	}
	CHECK(krb5_store_int32(sp, 1)); /* last item */
    }

    CHECK(krb5_store_int32(sp, ent->n_key_data));
    for (i = 0; i < (size_t)ent->n_key_data; i++) {
	CHECK(krb5_store_uint32(sp, 2));
	CHECK(krb5_store_uint32(sp, ent->kvno));
	CHECK(krb5_store_uint32(sp, ent->key_data[i].key_data_type[0]));
	CHECK(krb5_store_uint32(sp, ent->key_data[i].key_data_type[1]));
    }

    return 0;
}

krb5_error_code
_kadm5_xdr_ret_principal_ent(krb5_context context,
			       krb5_storage *sp,
			       kadm5_principal_ent_rec *ent)
{
    uint32_t flag, num, dataver;
    size_t i;

    memset(ent, 0, sizeof(*ent));

    CHECK(_kadm5_xdr_ret_principal_xdr(context, sp, &ent->principal));
    CHECK(krb5_ret_uint32(sp, &flag));
    ent->princ_expire_time = flag;
    CHECK(krb5_ret_uint32(sp, &flag));
    ent->pw_expiration = flag;
    CHECK(krb5_ret_uint32(sp, &flag));
    ent->last_pwd_change = flag;
    CHECK(krb5_ret_uint32(sp, &flag));
    ent->max_life = flag;
    if (ent->max_life >= LARGETIME)
	ent->max_life = 0;
    CHECK(krb5_ret_uint32(sp, &flag));
    if (flag == 0) {
	CHECK(_kadm5_xdr_ret_principal_xdr(context, sp, &ent->mod_name));
    }
    CHECK(krb5_ret_uint32(sp, &flag));
    ent->mod_date = flag;
    CHECK(krb5_ret_uint32(sp, &flag));
    ent->attributes = flag;
    CHECK(krb5_ret_uint32(sp, &flag));
    ent->kvno = flag;
    CHECK(krb5_ret_uint32(sp, &flag));
    ent->mkvno = flag;
    CHECK(_kadm5_xdr_ret_string_xdr(sp, &ent->policy));
    CHECK(krb5_ret_uint32(sp, &flag));
    ent->aux_attributes = flag;
    CHECK(krb5_ret_uint32(sp, &flag));
    ent->max_renewable_life = flag;
    if (ent->max_renewable_life >= LARGETIME)
	ent->max_renewable_life = 0;
    CHECK(krb5_ret_uint32(sp, &flag));
    ent->last_success = flag;
    CHECK(krb5_ret_uint32(sp, &flag));
    ent->last_failed = flag;
    CHECK(krb5_ret_uint32(sp, &flag));
    ent->fail_auth_count = flag;
    CHECK(krb5_ret_uint32(sp, &flag));
    ent->n_key_data = flag;
    CHECK(krb5_ret_uint32(sp, &flag));
    ent->n_tl_data = flag;
    CHECK(krb5_ret_uint32(sp, &flag));
    if (flag == 0) {
	krb5_tl_data **tp = &ent->tl_data;
	size_t count = 0;

	while(1) {
	    krb5_data c;
	    CHECK(krb5_ret_uint32(sp, &flag)); /* last item */
	    if (flag)
		break;
	    *tp = calloc(1, sizeof(**tp));
	    INSIST(*tp != NULL);
	    CHECK(krb5_ret_uint32(sp, &flag));
	    (*tp)->tl_data_type = flag;
	    CHECK(_kadm5_xdr_ret_data_xdr(sp, &c));
	    (*tp)->tl_data_length = c.length;
	    (*tp)->tl_data_contents = c.data;
	    tp = &(*tp)->tl_data_next;

	    count++;

	    INSIST(count < 2000);
	}
	INSIST((size_t)ent->n_tl_data == count);
    } else {
	INSIST(ent->n_tl_data == 0);
    }
	  
    CHECK(krb5_ret_uint32(sp, &num));
    INSIST(num == (uint32_t)ent->n_key_data);

    ent->key_data = calloc(num, sizeof(ent->key_data[0]));
    INSIST(ent->key_data != NULL);

    for (i = 0; i < num; i++) {
	CHECK(krb5_ret_uint32(sp, &dataver));
	CHECK(krb5_ret_uint32(sp, &flag));
	ent->kvno = flag;

	CHECK(krb5_ret_uint32(sp, &flag));
	ent->key_data[i].key_data_type[0] = flag;

	if (dataver > 1) {
	    CHECK(krb5_ret_uint32(sp, &flag));
	    ent->key_data[i].key_data_type[1] = flag;
	}
    }

    return 0;
}
