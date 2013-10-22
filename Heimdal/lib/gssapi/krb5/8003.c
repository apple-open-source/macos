/*
 * Copyright (c) 1997 - 2003 Kungliga Tekniska Högskolan
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

#include "gsskrb5_locl.h"

krb5_error_code
_gsskrb5_decode_be_om_uint32(const void *ptr, OM_uint32 *n)
{
    const u_char *p = ptr;
    *n = (p[0] <<24) | (p[1] << 16) | (p[2] << 8) | (p[3] << 0);
    return 0;
}

/*
 *
 */

static OM_uint32
store_ext(krb5_storage *sp, uint16_t type, krb5_data *data)
{
    krb5_error_code ret;
    krb5_ssize_t sret;

    ret = krb5_store_uint16(sp, type);
    if (ret) return ret;
    ret = krb5_store_uint16(sp, data->length);
    if (ret) return ret;
    sret = krb5_storage_write(sp, data->data, data->length);
    if (sret != data->length)
	return ENOMEM;
    return 0;
}

/*
 * create a checksum over the chanel bindings in
 * `input_chan_bindings', `flags' and `fwd_data' and return it in
 * `result'
 */

OM_uint32
_gsskrb5_create_8003_checksum(OM_uint32 *minor_status,
			      krb5_context context,
			      krb5_crypto crypto,
			      const gss_channel_bindings_t input_chan_bindings,
			      OM_uint32 flags,
			      krb5_data *fwd_data,
			      krb5_data *pkt_cksum,
			      Checksum *result)
{
    uint8_t channelbindings[16];
    gss_buffer_desc cksum;
    krb5_error_code ret;
    krb5_storage *sp;
    OM_uint32 maj_stat, junk;
    krb5_ssize_t sret;

    cksum.value = NULL;
    cksum.length = 0;

    sp = krb5_storage_emem();
    if (sp == NULL) {
	ret = ENOMEM;
	goto out;
    }
    krb5_storage_set_byteorder(sp, KRB5_STORAGE_BYTEORDER_LE);

    ret = krb5_store_uint32(sp, 16);
    if (ret) goto out;
    
    if (input_chan_bindings == GSS_C_NO_CHANNEL_BINDINGS) {
	memset (channelbindings, 0, 16);
    } else {
	maj_stat = gss_mg_gen_cb(minor_status, input_chan_bindings, channelbindings, &cksum);
	if (maj_stat) {
	    ret = *minor_status; 
	    goto out;
	}
    }
    sret = krb5_storage_write(sp, channelbindings, sizeof(channelbindings));
    if (sret != sizeof(channelbindings)) {
	ret = ENOMEM;
	goto out;
    }
    ret = krb5_store_uint32(sp, flags);
    if (ret) goto out;

    if (flags & GSS_C_DELEG_FLAG) {
	ret = store_ext(sp, 1, fwd_data);
	if (ret) goto out;
    }
    if (pkt_cksum->length > 0) {
	ret = store_ext(sp, 2, pkt_cksum);
	if (ret) goto out;
    }

    if (crypto && input_chan_bindings && cksum.length) {
	Checksum checksum;
	krb5_data data;
	size_t size;

	memset(&checksum, 0, sizeof(checksum));
	ret = krb5_create_checksum(context, crypto, KRB5_KU_GSSAPI_EXTS, 0,
				   cksum.value, cksum.length, &checksum);
	if (ret) goto out;

	ASN1_MALLOC_ENCODE(Checksum, data.data, data.length,
			   &checksum, &size, ret);
	if (ret)
	    goto out;
	if (data.length != size)
	    krb5_abortx(context, "internal error in ASN.1 encoder");

	ret = store_ext(sp, 0, &data);
	krb5_data_free(&data);
	if (ret) goto out;
    }

    /*
     * see rfc1964 (section 1.1.1 (Initial Token), and the checksum value
     * field's format)
     */
    result->cksumtype = CKSUMTYPE_GSSAPI;
    ret = krb5_storage_to_data(sp, &result->checksum);
 out:
    gss_release_buffer(&junk, &cksum);
    if (sp)
	krb5_storage_free(sp);
    if (ret) {
	*minor_status = ret;
	return GSS_S_FAILURE;
    }

    return GSS_S_COMPLETE;
}

/*
 *
 */

static krb5_error_code
read_ext(krb5_storage *sp, uint16_t *type, krb5_data *data)
{
    krb5_error_code ret;
    krb5_ssize_t sret;
    uint16_t len;

    ret = krb5_ret_uint16(sp, type);
    if (ret)
	return ret;
	
    ret = krb5_ret_uint16(sp, &len);
    if (ret)
	return ret;

    ret = krb5_data_alloc(data, len);
    if (ret)
	return ret;

    sret = krb5_storage_read(sp, data->data, data->length);
    if (sret != data->length) {
	krb5_data_free(data);
	return HEIM_ERR_EOF;
    }
    return 0;
}

/*
 * verify the checksum in `cksum' over `input_chan_bindings'
 * returning  `flags' and `fwd_data'
 */

OM_uint32
_gsskrb5_verify_8003_checksum(OM_uint32 *minor_status,
			      krb5_context context,
			      krb5_crypto crypto,
			      const gss_channel_bindings_t input_chan_bindings,
			      const Checksum *cksum,
			      OM_uint32 *flags,
			      krb5_data *fwd_data,
			      krb5_data *finished)
{
    unsigned count = 0, verified_checksum = 0;
    krb5_error_code ret;
    krb5_ssize_t sret;
    krb5_storage *sp;
    unsigned char hash[16], pkthash[16];
    uint32_t length;
    krb5_data data;
    uint16_t type;
    OM_uint32 maj_stat, junk;
    gss_buffer_desc cbdata;

    cbdata.length = 0;
    krb5_data_zero(&data);

    /* XXX should handle checksums > 24 bytes */
    if(cksum->cksumtype != CKSUMTYPE_GSSAPI || cksum->checksum.length < 24) {
	*minor_status = 0;
	return GSS_S_BAD_BINDINGS;
    }

    sp = krb5_storage_from_readonly_mem(cksum->checksum.data,
					cksum->checksum.length);
    if (sp == NULL) {
	*minor_status = ENOMEM;
	return GSS_S_FAILURE;
    }
    krb5_storage_set_byteorder(sp, KRB5_STORAGE_BYTEORDER_LE);

    ret = krb5_ret_uint32(sp, &length);
    if (ret) goto out;
    if(length != sizeof(hash)) {
	ret = ENOMEM;
	goto out;
    }

    sret = krb5_storage_read(sp, pkthash, sizeof(pkthash));
    if (sret != sizeof(pkthash)) {
	ret = ENOMEM;
	goto out;
    }

    maj_stat = gss_mg_validate_cb(minor_status, input_chan_bindings,
				  pkthash, &cbdata);
    if (maj_stat) {
	ret = *minor_status;
	goto out;
    }

    ret = krb5_ret_uint32(sp, flags);
    if (ret) goto out;

    if ((*flags) & GSS_C_DELEG_FLAG) {
	ret = read_ext(sp, &type, fwd_data);
	if (ret) goto out;

	if (type != 1) {
	    ret = EINVAL;
	    goto out;
	}
    }

    while ((ret = read_ext(sp, &type, &data)) == 0) {
	count++;
	if (type == 0 && input_chan_bindings) {
	    Checksum checksum;

	    /* new checksum */

	    if (crypto == NULL) {
		ret = ENOMEM;
		goto out;
	    }

	    ret = decode_Checksum(data.data, data.length, &checksum, NULL);
	    if (ret) goto out;

	    ret = krb5_verify_checksum(context, crypto, KRB5_KU_GSSAPI_EXTS,
				       cbdata.value, cbdata.length,
				       &checksum);
	    free_Checksum(&checksum);
	    krb5_data_free(&data);

	    if (ret) goto out;
	    
	    verified_checksum = 1;

	} else if (type == 2) {
	    *finished = data;
	    krb5_data_zero(&data);
	} else
	    krb5_data_free(&data);
    }
    if (ret != HEIM_ERR_EOF && ret != 0)
	goto out;

    if (input_chan_bindings && count && !verified_checksum) {
	ret = EINVAL;
	goto out;
    }
    ret = 0;

 out:
    gss_release_buffer(&junk, &cbdata);
    krb5_data_free(&data);
    krb5_storage_free(sp);
    if (ret) {
	*minor_status = ret;
	return GSS_S_BAD_BINDINGS;
    }
    return GSS_S_COMPLETE;
}
