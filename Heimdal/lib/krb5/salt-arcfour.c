/*
 * Copyright (c) 1997 - 2008 Kungliga Tekniska Högskolan
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

#ifdef HEIM_KRB5_ARCFOUR

int krb5_heim_use_broken_arcfour_string2key = 0;

static krb5_error_code
ARCFOUR_string_to_key_broken(krb5_context context,
                             krb5_enctype enctype,
                             krb5_data password,
                             krb5_salt salt,
                             krb5_data opaque,
                             krb5_keyblock *key)
{
    krb5_error_code ret;
    size_t i;
    CCDigestRef m;

    m = CCDigestCreate(kCCDigestMD4);
    if (m == NULL) {
        ret = ENOMEM;
        krb5_set_error_message(context, ret, N_("malloc: out of memory", ""));
        goto out;
    }

    /* LE encoding */
    for (i = 0; i < password.length; i++) {
        unsigned char p;

        p = ((const uint8_t *)password.data)[i] & 0xff;
        CCDigestUpdate(m, &p, 1);
        p = 0;
        CCDigestUpdate(m, &p, 1);
    }

    key->keytype = enctype;
    ret = krb5_data_alloc (&key->keyvalue, 16);
    if (ret) {
        krb5_set_error_message (context, ENOMEM, N_("malloc: out of memory", ""));
        goto out;
    }
    CCDigestFinal(m, key->keyvalue.data);

 out:
    CCDigestDestroy(m);
    return ret;
}

static krb5_error_code
ARCFOUR_string_to_key(krb5_context context,
		      krb5_enctype enctype,
		      krb5_data password,
		      krb5_salt salt,
		      krb5_data opaque,
		      krb5_keyblock *key)
{
    krb5_error_code ret;
    uint16_t *s = NULL;
    size_t len, i;
    CCDigestRef m;

    if (getenv("KRB5_USE_BROKEN_ARCFOUR_STRING2KEY") || krb5_heim_use_broken_arcfour_string2key)
	return ARCFOUR_string_to_key_broken(context, enctype, password,
					    salt, opaque, key);

    m = CCDigestCreate(kCCDigestMD4);
    if (m == NULL) {
	ret = ENOMEM;
	krb5_set_error_message(context, ret, N_("malloc: out of memory", ""));
	goto out;
    }

    ret = wind_utf8ucs2_length(password.data, &len);
    if (ret) {
	krb5_set_error_message (context, ret,
				N_("Password not an UCS2 string", ""));
	goto out;
    }

    s = malloc (len * sizeof(s[0]));
    if (len != 0 && s == NULL) {
	krb5_set_error_message (context, ENOMEM,
				N_("malloc: out of memory", ""));
	ret = ENOMEM;
	goto out;
    }

    ret = wind_utf8ucs2(password.data, s, &len);
    if (ret) {
	krb5_set_error_message (context, ret,
				N_("Password not an UCS2 string", ""));
	goto out;
    }

    /* LE encoding */
    for (i = 0; i < len; i++) {
	unsigned char p;
	p = (s[i] & 0xff);
	CCDigestUpdate(m, &p, 1);
	p = (s[i] >> 8) & 0xff;
	CCDigestUpdate(m, &p, 1);
    }

    key->keytype = enctype;
    ret = krb5_data_alloc (&key->keyvalue, 16);
    if (ret) {
	krb5_set_error_message (context, ENOMEM, N_("malloc: out of memory", ""));
	goto out;
    }
    CCDigestFinal(m, key->keyvalue.data);

 out:
    CCDigestDestroy(m);
    if (s)
	memset (s, 0, len);
    free (s);
    return ret;
}

struct salt_type _krb5_arcfour_salt[] = {
    {
	KRB5_PW_SALT,
	"pw-salt",
	ARCFOUR_string_to_key
    },
    { 0 }
};

#endif /* HEIM_KRB5_ARCFOUR */
