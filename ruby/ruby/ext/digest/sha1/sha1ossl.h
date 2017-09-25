/* $Id: sha1ossl.h 55918 2016-08-16 03:13:24Z nagachika $ */

#ifndef SHA1OSSL_H_INCLUDED
#define SHA1OSSL_H_INCLUDED

#include <stddef.h>
#include <openssl/sha.h>

#define SHA1_CTX	SHA_CTX

#ifdef SHA_BLOCK_LENGTH
#define SHA1_BLOCK_LENGTH	SHA_BLOCK_LENGTH
#else
#define SHA1_BLOCK_LENGTH	SHA_CBLOCK
#endif
#define SHA1_DIGEST_LENGTH	SHA_DIGEST_LENGTH

static DEFINE_FINISH_FUNC_FROM_FINAL(SHA1)
#undef SHA1_Finish
#define SHA1_Finish rb_digest_SHA1_finish

#endif
