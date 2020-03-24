/*
 * Copyright (c) 2009 Kungliga Tekniska Högskolan
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

#ifndef HEIM_ECDSA_H
#define HEIM_ECDSA_H 1

#define ECDSA_verify hc_ECDSA_verify
#define ECDSA_sign hc_ECDSA_sign
#define ECDSA_size hc_ECDSA_size
#define ECDSA_new hc_ECDSA_new
#define ECDSA_new_method hc_ECDSA_new_method
#define ECDSA_free hc_ECDSA_free
#define ECDSA_public_encrypt hc_ECDSA_public_encrypt
#define ECDSA_public_decrypt hc_ECDSA_public_decrypt
#define ECDSA_private_encrypt hc_ECDSA_private_encrypt
#define ECDSA_private_decrypt hc_ECDSA_private_decrypt
#define ECDSA_set_method hc_ECDSA_set_method
#define ECDSA_get_method hc_ECDSA_get_method

typedef struct ECDSA ECDSA;
typedef struct ECDSA_METHOD ECDSA_METHOD;

#include <hcrypto/ec.h>
#define ECDSA_KEY_SIZE 72

int ECDSA_verify(int, const unsigned char *, unsigned int,
		 unsigned char *, unsigned int, ECDSA *);

int ECDSA_sign(int, const unsigned char *, unsigned int,
	       unsigned char *, unsigned int *, ECDSA *);

int ECDSA_size(EC_KEY *);

int     ECDSA_set_app_data(ECDSA *, void *arg);
void *  ECDSA_get_app_data(const ECDSA *);

struct ECDSA_METHOD {
    const char *name;
    int (*ecdsa_pub_enc)(int,const unsigned char *, unsigned char *, ECDSA *,int);
    int (*ecdsa_pub_dec)(int,const unsigned char *, unsigned char *, ECDSA *,int);
    int (*ecdsa_priv_enc)(int,const unsigned char *, unsigned char *, ECDSA *,int);
    int (*ecdsa_priv_dec)(int,const unsigned char *, unsigned char *, ECDSA *,int);
    int (*init)(ECDSA *ecdsa);
    int (*finish)(ECDSA *ecdsa);
    int flags;
    int (*ecdsa_sign)(int, const unsigned char *, unsigned int,
                    unsigned char *, unsigned int *, const ECDSA *);
    int (*ecdsa_verify)(int, const unsigned char *, unsigned int,
                      unsigned char *, unsigned int, const ECDSA *);
};

struct ECDSA {
    int pad;
    long version;
    const ECDSA_METHOD *meth;
    void *engine;
    struct ecdsa_CRYPTO_EX_DATA {
        void *sk;
        int dummy;
    } ex_data;
    int references;
};

ECDSA *   ECDSA_new(void);
ECDSA *   ECDSA_new_method(ENGINE *);
void    ECDSA_free(ECDSA *);
int     ECDSA_up_ref(ECDSA *);

void    ECDSA_set_default_method(const ECDSA_METHOD *);
const ECDSA_METHOD * ECDSA_get_default_method(void);

const ECDSA_METHOD * ECDSA_get_method(const ECDSA *);
int ECDSA_set_method(ECDSA *, const ECDSA_METHOD *);


#endif /* HEIM_ECDSA_H */
