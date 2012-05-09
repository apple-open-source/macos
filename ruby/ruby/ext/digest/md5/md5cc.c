#include "md5cc.h"

void
MD5_Finish(MD5_CTX *pctx, unsigned char *digest)
{
	CC_MD5_Final(digest, pctx);
}
