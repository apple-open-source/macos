/*
 * Copyright (c) 2011-12 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Copyright (c) 2006 - 2007 Kungliga Tekniska Högskolan
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

#include "ossl-config.h"

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include <sys/time.h>

#include "ossl-engine.h"
#include "ossl-evp.h"

#ifdef __APPLE_TARGET_EMBEDDED__
#include <CommonCrypto/CommonRandom.h>
#else
#include <CommonCrypto/CommonRandomSPI.h>
#endif


/*
 *
 */

static int verbose = 1;
static int time_keygen;
static char *time_key;
static int key_blinding = 1;
static char *rsa_key;
static char *id_flag;
static int loops = 10;

static int total = 0;
static int pass = 0;
static int fail = 0;

/*
static struct getargs args[] = {
    { "loops",		0,	arg_integer,	&loops,
      "number of loops", 	"loops" },
    { "id",		0,	arg_string,	&id_flag,
      "selects the engine id", 	"engine-id" },
    { "time-keygen",	0,	arg_flag,	&time_keygen,
      "time rsa generation", NULL },
    { "time-key",	0,	arg_string,	&time_key,
      "rsa key file", NULL },
    { "key-blinding",	0,	arg_negative_flag, &key_blinding,
      "key blinding", NULL },
    { "key",	0,	arg_string,	&rsa_key,
      "rsa key file", NULL },
    { "version",	0,	arg_flag,	&version_flag,
      "print version", NULL },
    { "help",		0,	arg_flag,	&help_flag,
      NULL, 	NULL }
};
*/

static struct option args[] = {
	{ "loops", 	required_argument,	NULL,		'l' },
	{ "quite",	no_argument,		NULL,		'q' },
	{ "help",	no_argument,		NULL,		'h' },
};

/*
 *
 */

static void
timevalfix(struct timeval *t1)
{
	if (t1->tv_usec < 0) {
		t1->tv_sec--;
		t1->tv_usec += 1000000;
	}
	if (t1->tv_usec >= 1000000) {
		 t1->tv_sec++;
		 t1->tv_usec -= 1000000;
	}
}

static void
timevalsub(struct timeval *t1, const struct timeval *t2)
{
	t1->tv_sec  -= t2->tv_sec;
	t1->tv_usec -= t2->tv_usec;
	timevalfix(t1);
}


static int
check_rsa(const unsigned char *in, size_t len, RSA *rsa, int padding)
{
    unsigned char *res, *res2;
    unsigned int len2;
    int keylen;

    res = malloc(RSA_size(rsa));
    if (res == NULL) {
	fprintf(stderr, "malloc(): failed to allocate %d bytes\n", (int)RSA_size(rsa));
	fail++;
	return (1);
    }

    res2 = malloc(RSA_size(rsa));
    if (res2 == NULL) {
	fprintf(stderr, "malloc(): failed to allocate %d bytes\n", (int)RSA_size(rsa));
	fail++;
	return (1);
    }

    /* signing */

    keylen = RSA_private_encrypt(len, in, res, rsa, padding);
    if (keylen <= 0) {
	fprintf(stderr, "failed to private encrypt: %d %d", (int)len, (int)keylen);
	fail++;
	return (1);
    }

    if (keylen > RSA_size(rsa)) {
	fprintf(stderr, "keylen (%d) > RSA_size(rsa) (%d)", (int)keylen, (int)RSA_size(rsa));
	fail++;
	return (1);
    }

    keylen = RSA_public_decrypt(keylen, res, res2, rsa, padding);
    if (keylen <= 0) {
	fprintf(stderr, "failed to public decrypt: %d", (int)keylen);
	fail++;
	return (1);
    }

    if (keylen != len) {
	fprintf(stderr, "output buffer not same length: %d", (int)keylen);
	fail++;
	return (1);
    }

    if (memcmp(res2, in, len) != 0) {
	fprintf(stderr, "string not the same after decryption");
	fail++;
	return (1);
    }

    /* encryption */

    keylen = RSA_public_encrypt(len, in, res, rsa, padding);
    if (keylen <= 0) {
	fprintf(stderr, "failed to public encrypt: %d", (int)keylen);
	fail++;
	return (1);
    }

    if (keylen > RSA_size(rsa)) {
	fprintf(stderr, "keylen > RSA_size(rsa)");
    	fail++;
	return (1);
    }

    keylen = RSA_private_decrypt(keylen, res, res2, rsa, padding);
    if (keylen <= 0) {
	fprintf(stderr,  "failed to private decrypt: %d", (int)keylen);
	fail++;
	return (1);
    }

    if (keylen != len) {
	fprintf(stderr, "output buffer not same length: %d", (int)keylen);
	fail++;
	return (1);
    }

    if (memcmp(res2, in, len) != 0) {
	fprintf(stderr, "string not the same after decryption");
	fail++;
	return (1);
    }

    len2 = keylen;

    if (RSA_sign(NID_sha1, in, len, res, &len2, rsa) != 1) {
	fprintf(stderr, "RSA_sign failed");
	fail++;
	return (1);
    }

    if (RSA_verify(NID_sha1, in, len, res, len2, rsa) != 1) {
	fprintf(stderr, "RSA_verify failed");
	fail++;
	return (1);
    }

    free(res);
    free(res2);
    return (0);
}

static int
cb_func(int a, int b, BN_GENCB *c)
{
    return 1;
}

static RSA *
read_key(ENGINE *engine, const char *rsakey)
{
    unsigned char buf[1024 * 4];
    const unsigned char *p;
    size_t size;
    RSA *rsa;
    FILE *f;

    f = fopen(rsa_key, "rb");
    if (f == NULL) {
	fprintf(stderr, "could not open file %s", rsakey);
	fail++;
	return NULL;
    }
   /* rk_cloexec_file(f); */

    size = fread(buf, 1, sizeof(buf), f);
    fclose(f);
    if (size == 0) {
	fprintf(stderr, "failed to read file %s", rsakey);
	fail++;
	return NULL;
    }
    if (size == sizeof(buf)) {
	fprintf(stderr, "key too long in file %s!", rsakey);
	fail++;
	return NULL;
    }

    p = buf;
    rsa = d2i_RSAPrivateKey(NULL, &p, size);
    if (rsa == NULL) {
	fprintf(stderr, "failed to parse key in file %s", rsakey);
	fail++;
	return NULL;
    }

    RSA_set_method(rsa, ENGINE_get_RSA(engine));

    if (!key_blinding)
	rsa->flags |= RSA_FLAG_NO_BLINDING;

    return rsa;
}

/*
 *
 */

static void
usage (int ret)
{
    fprintf(stderr, "usage: %s [--quite/-q] [--help/-h]\n", getprogname());
    exit (ret);
}

int
main(int argc, char **argv)
{
    ENGINE *engine = NULL;
    int i, j, idx = 0;
    RSA *rsa;

    setprogname(argv[0]);

    /*
    if(getarg(args, sizeof(args) / sizeof(args[0]), argc, argv, &idx))
	usage(1);

    if (help_flag)
	usage(0);

    if(version_flag){
	print_version(NULL);
	exit(0);
    }
    */

    while(1) {
	    int c = getopt_long(argc, argv, "hq", args, &idx);

	    if (c == -1)
		    break;

	    switch (c) {
	    case 'q':
		    verbose = 0;
		    break;
	    case 'h':
		    usage(0);
		    break;
	    case '?':
	    default:
		    usage(-1);
		    break;
	    }
    }

    /*
    argc -= idx;
    argv += idx;
    */

    if (verbose) printf("[TEST] RSA\n");

    /* OpenSSL_add_all_algorithms(); */
#ifdef OPENSSL
    ENGINE_load_openssl();
#endif
    ENGINE_load_builtin_engines();

    /*
    if (argc == 0) {
	engine = ENGINE_by_id("builtin");
    } else {
	engine = ENGINE_by_id(argv[0]);
	if (engine == NULL)
	    engine = ENGINE_by_dso(argv[0], id_flag);
    }
    if (engine == NULL) {
	fprintf(stderr, "ENGINE_by_dso failed"\n);
	return 76;
    }

    if (ENGINE_get_RSA(engine) == NULL)
	return 77;

    printf("rsa %s\n", ENGINE_get_RSA(engine)->name);
    */

    if (time_keygen) {
	struct timeval tv1, tv2;
	BIGNUM *e;

	rsa = RSA_new_method(engine);
	if (!key_blinding)
	    rsa->flags |= RSA_FLAG_NO_BLINDING;

	e = BN_new();
	BN_set_word(e, 0x10001);

	printf("running keygen with %d loops\n", loops);

	gettimeofday(&tv1, NULL);

	for (i = 0; i < loops; i++) {
	    rsa = RSA_new_method(engine);
	    if (RSA_generate_key_ex(rsa, 1024, e, NULL) != 1) {
		RSA_free(rsa);
		fprintf(stderr, "RSA_generate_key_ex");
		fail++;
		return 1;
	    }
	    RSA_free(rsa);
	}

	gettimeofday(&tv2, NULL);
	timevalsub(&tv2, &tv1);

	printf("time %lu.%06lu\n",
	       (unsigned long)tv2.tv_sec,
	       (unsigned long)tv2.tv_usec);

	BN_free(e);
	/*
	ENGINE_finish(engine);
	*/

	return 0;
    }

/*
    if (time_key) {
	const int size = 20;
	struct timeval tv1, tv2;
	unsigned char *p;

	if (strcmp(time_key, "generate") == 0) {
	    BIGNUM *e;

	    rsa = RSA_new_method(engine);
	    if (!key_blinding)
		rsa->flags |= RSA_FLAG_NO_BLINDING;

	    e = BN_new();
	    BN_set_word(e, 0x10001);

	    if (RSA_generate_key_ex(rsa, 1024, e, NULL) != 1) {
		fprintf(stderr, "RSA_generate_key_ex");
		fail++;
		return (1);
	    }
	} else {
	    rsa = read_key(engine, time_key);
	}

	p = emalloc(loops * size);

	CCRandomCopyBytes(kCCRandomDefault, p, loops * size);

	gettimeofday(&tv1, NULL);
	for (i = 0; i < loops; i++)
	    check_rsa(p + (i * size), size, rsa, RSA_PKCS1_PADDING);
	gettimeofday(&tv2, NULL);

	timevalsub(&tv2, &tv1);

	printf("time %lu.%06lu\n",
	       (unsigned long)tv2.tv_sec,
	       (unsigned long)tv2.tv_usec);

	RSA_free(rsa);
	ENGINE_finish(engine);

	return 0;
    }
*/

    if (rsa_key) {
	rsa = read_key(engine, rsa_key);

	/*
	 * Assuming that you use the RSA key in the distribution, this
	 * test will generate a signature have a starting zero and thus
	 * will generate a checksum that is 127 byte instead of the
	 * checksum that is 128 byte (like the key).
	 */
	{
	    const unsigned char sha1[20] = {
		0x6d, 0x33, 0xf9, 0x40, 0x75, 0x5b, 0x4e, 0xc5, 0x90, 0x35,
		0x48, 0xab, 0x75, 0x02, 0x09, 0x76, 0x9a, 0xb4, 0x7d, 0x6b
	    };

	    check_rsa(sha1, sizeof(sha1), rsa, RSA_PKCS1_PADDING);
	}

	for (i = 0; i < 128; i++) {
	    unsigned char sha1[20];
	
	    CCRandomCopyBytes(kCCRandomDefault, sha1, sizeof(sha1));
	    check_rsa(sha1, sizeof(sha1), rsa, RSA_PKCS1_PADDING);
	}
	for (i = 0; i < 128; i++) {
	    unsigned char des3[21];

	    CCRandomCopyBytes(kCCRandomDefault, des3, sizeof(des3));
	    check_rsa(des3, sizeof(des3), rsa, RSA_PKCS1_PADDING);
	}
	for (i = 0; i < 128; i++) {
	    unsigned char aes[32];

	    CCRandomCopyBytes(kCCRandomDefault, aes, sizeof(aes));
	    check_rsa(aes, sizeof(aes), rsa, RSA_PKCS1_PADDING);
	}

	RSA_free(rsa);
    }

    if (verbose) {
	    printf("[BEGIN] RSA loops\n");
	    printf("Running %d loops\n", loops);
    }
    total++;
    for (i = 0; i < loops; i++) {
	BN_GENCB cb;
	BIGNUM *e;
	unsigned int n;

	rsa = RSA_new_method(engine);
	if (!key_blinding)
	    rsa->flags |= RSA_FLAG_NO_BLINDING;

	e = BN_new();
	BN_set_word(e, 0x10001);

	BN_GENCB_set(&cb, cb_func, NULL);
	
	CCRandomCopyBytes(kCCRandomDefault, &n, sizeof(n));
	n &= 0x1ff;
	n += 1024;

	if (RSA_generate_key_ex(rsa, n, e, &cb) != 1) {
	    fprintf(stderr, "RSA_generate_key_ex");
	    fail++;
	    return 1;
	}

	BN_free(e);

	for (j = 0; j < 8; j++) {
	    unsigned char sha1[20];
	    CCRandomCopyBytes(kCCRandomDefault, sha1, sizeof(sha1));
	    check_rsa(sha1, sizeof(sha1), rsa, RSA_PKCS1_PADDING);
	}

	RSA_free(rsa);
    }

    if (verbose) printf("[PASS] RSA loops\n");
    pass++;


    if (verbose) {
	    printf("[SUMMARY]\n");
	    printf("total: %d\n", total);
	    printf("passed: %d\n", pass);
	    printf("failed: %d\n", fail);
    }

    /* ENGINE_finish(engine); */

    return (fail);
}
