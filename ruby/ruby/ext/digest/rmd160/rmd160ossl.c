/* $Id: rmd160ossl.c 11905 2007-02-27 10:38:32Z knu $ */

#include "defs.h"
#include "rmd160ossl.h"

void RMD160_Finish(RMD160_CTX *ctx, char *buf) {
	RIPEMD160_Final((unsigned char *)buf, ctx);
}
