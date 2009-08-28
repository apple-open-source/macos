/* -*- Mode: C; c-file-style: "bsd" -*- */

#ifndef YHASH_H
#define YHASH_H

/* hash function interface */

/* default to SHA1 for yarrow 160 */

#include <CommonCrypto/CommonDigest.h>



#define HASH_CTX CC_SHA1_CTX
#define HASH_Init(x) CC_SHA1_Init(x)
#define HASH_Update(x, buf, sz) CC_SHA1_Update(x, (const void*)buf, sz)
#define HASH_Final(x, tdigest)  CC_SHA1_Final(tdigest, x)

#define HASH_DIGEST_SIZE CC_SHA1_DIGEST_LENGTH

#endif /* YHASH_H */
