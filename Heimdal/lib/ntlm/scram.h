/*
 * Copyright (c) 2008 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2009 - 2010 Apple Inc. All rights reserved.
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
 * 3. Neither the name of KTH nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY KTH AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL KTH OR ITS CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <krb5-types.h>

#include <wind.h>
#include <roken.h>
#include <base64.h>

#include <heimbase.h>

#include "heimscram.h"
#include "crypto-headers.h"

#ifndef __APPLE_TARGET_EMBEDDED__
#include <CommonCrypto/CommonKeyDerivation.h>
#endif


struct heim_scram_pair {
    char type;
    heim_scram_data data;
};

struct heim_scram_pairs {
    int flags;
#define SCRAM_PAIR_ALLOCATED 1
#define SCRAM_ARRAY_ALLOCATED 2
#define SCRAM_BINDINGS_YES 4
#define SCRAM_BINDINGS_NO 8
    struct heim_scram_pair *val;
    size_t len;
};

typedef struct heim_scram_pairs heim_scram_pairs;

struct heim_scram {
    struct heim_scram_method_desc *method;
    enum { CLIENT, SERVER } type;
    heim_scram_data client1;
    heim_scram_data server1;
    /* generated */
    heim_scram_data nonce;

    /* server */
    struct heim_scram_server *server;
    void *ctx;

    heim_scram_data user;

    /* output */
    heim_scram_data ClientProof;
    heim_scram_data ServerSignature;
    heim_scram_data SessionKey;
};

#include "heimscram-protos.h"

int
_heim_scram_parse(heim_scram_data *data, heim_scram_pairs **pd);

int
_heim_scram_unparse (
	heim_scram_pairs */*d*/,
	heim_scram_data */*out*/);

void
_heim_scram_pairs_free (heim_scram_pairs */*d*/);
