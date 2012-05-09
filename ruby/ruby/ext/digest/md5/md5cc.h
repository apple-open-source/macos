#ifndef MD5CC_H_INCLUDED
#define MD5CC_H_INCLUDED

#include <stddef.h>
#include <CommonCrypto/CommonDigest.h>

#define MD5_CTX		CC_MD5_CTX

#define MD5_DIGEST_LENGTH	CC_MD5_DIGEST_LENGTH
#define MD5_BLOCK_LENGTH	CC_MD5_BLOCK_BYTES

#define MD5_Init CC_MD5_Init
#define MD5_Update CC_MD5_Update
#define MD5_Finish CC_MD5_Finish

void MD5_Finish(MD5_CTX *pctx, unsigned char *digest);

#endif
