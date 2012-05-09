#include "defs.h"
#include "sha1cc.h"

void
SHA1_Update(SHA1_CTX *ctx, const uint8_t *data, size_t len)
{
	uint8_t *ptr = data;
	size_t i;

	for (i = 0, ptr = data; i < (len / SHA1_STRIDE_SIZE); i++, ptr += SHA1_STRIDE_SIZE) {
		CC_SHA1_Update(ctx, ptr, SHA1_STRIDE_SIZE);
	}
	CC_SHA1_Update(ctx, ptr, len % SHA1_STRIDE_SIZE);
}

void
SHA1_Finish(SHA1_CTX *ctx, char *buf)
{
	CC_SHA1_Final((unsigned char *)buf, ctx);
}
