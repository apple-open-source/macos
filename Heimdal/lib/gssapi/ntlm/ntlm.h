/*
 * Copyright (c) 2006 - 2007 Kungliga Tekniska Högskolan
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

/* $Id$ */

#ifndef NTLM_NTLM_H
#define NTLM_NTLM_H

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

#include <roken.h>

#include <gssapi.h>
#include <gssapi_ntlm.h>
#include <gssapi_spi.h>
#include <gssapi_mech.h>
#include <gssapi_oid.h>

#include <krb5.h>
#include <heim_threads.h>

#include <kcm.h>

#include <heimntlm.h>

#define HC_DEPRECATED_CRYPTO
#include "crypto-headers.h"

typedef struct {
    char *user;
    char *domain;
    int flags;
#define NTLM_UUID	1
#define NTLM_ANON_NAME	2
#define NTLM_DS_UUID	4
    unsigned char ds_uuid[16];
    unsigned char uuid[16];
} *ntlm_name;

struct ntlm_ctx;

typedef ntlm_name ntlm_cred;

typedef OM_uint32
(*ntlm_interface_init)(OM_uint32 *, void **);

typedef OM_uint32
(*ntlm_interface_destroy)(OM_uint32 *, void *);

typedef int
(*ntlm_interface_probe)(OM_uint32 *, void *, const char *, unsigned int *flags);

typedef OM_uint32
(*ntlm_interface_type3)(OM_uint32 *, struct ntlm_ctx *, void *, const struct ntlm_type3 *,
			ntlm_cred, uint32_t *, uint32_t *, struct ntlm_buf *,
			ntlm_name *, struct ntlm_buf *, struct ntlm_buf *);

typedef OM_uint32
(*ntlm_interface_targetinfo)(OM_uint32 *,
			     struct ntlm_ctx *,
			     void *,
			     const char *,
			     const char *,
			     uint32_t *);


typedef void
(*ntlm_interface_free_buffer)(struct ntlm_buf *);

struct ntlm_server_interface {
    const char *nsi_name;
    ntlm_interface_init nsi_init;
    ntlm_interface_destroy nsi_destroy;
    ntlm_interface_probe nsi_probe;
    ntlm_interface_type3 nsi_type3;
    ntlm_interface_free_buffer nsi_free_buffer;
    ntlm_interface_targetinfo nsi_ti;
};


struct ntlmv2_key {
    uint32_t seq;
    EVP_CIPHER_CTX sealkey;
    EVP_CIPHER_CTX *signsealkey;
    unsigned char signkey[16];
};

extern struct ntlm_server_interface ntlmsspi_kdc_digest;
extern struct ntlm_server_interface ntlmsspi_dstg_digest;
extern struct ntlm_server_interface ntlmsspi_netr_digest;
extern struct ntlm_server_interface ntlmsspi_od_digest;


struct ntlm_backend {
    struct ntlm_server_interface *interface;
    void *ctx;
};


typedef struct ntlm_ctx {
    struct ntlm_backend *backends;
    size_t num_backends;
    ntlm_cred client;

    unsigned int probe_flags;
#define NSI_NO_SIGNING 1

    OM_uint32 gssflags;
    uint32_t kcmflags;
    uint32_t flags;
    uint32_t status;
#define STATUS_OPEN 1
#define STATUS_CLIENT 2
#define STATUS_SESSIONKEY 4
    krb5_data sessionkey;
    krb5_data type1;
    krb5_data type2;
    krb5_data type3;

    uint8_t challenge[8];

    struct ntlm_targetinfo ti;
    struct ntlm_buf targetinfo;

    gss_name_t srcname;
    gss_name_t targetname;
    char *clientsuppliedtargetname;

    char uuid[16];
    gss_buffer_desc pac;

    union {
	struct {
	    struct {
		uint32_t seq;
		EVP_CIPHER_CTX key;
	    } crypto_send, crypto_recv;
	} v1;
	struct {
	    struct ntlmv2_key send, recv;
	} v2;
    } u;
} *ntlm_ctx;

#include <ntlm-private.h>


#endif /* NTLM_NTLM_H */
