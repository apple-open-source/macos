#ifndef SHA1CC_H_INCLUDED
#define SHA1CC_H_INCLUDED

#include <CommonCrypto/CommonDigest.h>

#define SHA1_CTX	CC_SHA1_CTX

#define SHA1_BLOCK_LENGTH	CC_SHA1_BLOCK_BYTES
#define SHA1_DIGEST_LENGTH	CC_SHA1_DIGEST_LENGTH

#define SHA1_Init CC_SHA1_Init
#define SHA1_Update CC_SHA1_Update_Block
#define SHA1_Finish CC_SHA1_Finish

#define SHA1_STRIDE_SIZE	16384

void SHA1_Update(SHA1_CTX *context, const uint8_t *data, size_t len);
void SHA1_Finish(SHA1_CTX *ctx, char *buf);

#endif
