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

#ifndef HEIM_SCRAM_H
#define HEIM_SCRAM_H

#include <sys/types.h>

#ifndef __HEIM_BASE_DATA__
#define __HEIM_BASE_DATA__ 1
struct heim_base_data {
	size_t length;
	void *data;
};
#endif
typedef struct heim_base_data heim_scram_data;

typedef struct heim_scram heim_scram;

typedef struct heim_scram_method_desc *heim_scram_method;

extern struct heim_scram_method_desc heim_scram_digest_sha1_s;
extern struct heim_scram_method_desc heim_scram_digest_sha256_s;

#define HEIM_SCRAM_DIGEST_SHA1 (&heim_scram_digest_sha1_s)
#define HEIM_SCRAM_DIGEST_SHA256 (&heim_scram_digest_sha256_s)

struct heim_scram_server {
#define SCRAM_SERVER_VERSION_1 1
    int version;
    int (*param)(void *ctx,
		 const heim_scram_data *user,
		 heim_scram_data *salt,
		 unsigned int *iteration,
		 heim_scram_data *servernonce);
    int (*calculate)(void *ctx,
		     heim_scram_method method,
		     const heim_scram_data *user,
		     const heim_scram_data *c1,
		     const heim_scram_data *s1,
		     const heim_scram_data *c2noproof,
		     const heim_scram_data *proof,
		     heim_scram_data *server,
		     heim_scram_data *sessionKey);
};

struct heim_scram_client {
#define SCRAM_CLIENT_VERSION_1 1
    int version;
    int (*calculate)(void *ctx,
		     heim_scram_method method,
		     unsigned int iterations,
		     heim_scram_data *salt,
		     const heim_scram_data *c1,
		     const heim_scram_data *s1,
		     const heim_scram_data *c2noproof,
		     heim_scram_data *proof,
		     heim_scram_data *server,
		     heim_scram_data *sessionKey);
};

extern struct heim_scram_client heim_scram_client_password_procs_s;
#define HEIM_SCRAM_CLIENT_PASSWORD_PROCS (&heim_scram_client_password_procs_s)

#include <heimscram-protos.h>

#endif /* SCRAM_SCRAM_H */
