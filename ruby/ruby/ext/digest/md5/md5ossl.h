/* $Id: md5ossl.h,v 1.1 2002/09/26 16:27:23 knu Exp $ */

#ifndef MD5OSSL_H_INCLUDED
#define MD5OSSL_H_INCLUDED

#include <openssl/md5.h>

void MD5_End(MD5_CTX *pctx, unsigned char *hexdigest);
int MD5_Equal(MD5_CTX *pctx1, MD5_CTX *pctx2);

#endif
