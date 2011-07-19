#ifndef MD5_H
#define MD5_H

#include "config.h"

#include <sys/types.h>

#ifdef HAVE_MD5_H
#include <md5.h>
#else
#include "fetchmail.h"
#ifndef HEADER_MD5_H
/* Try to avoid clashes with OpenSSL */
#define HEADER_MD5_H 
#endif


#if SIZEOF_INT == 4
typedef unsigned int uint32;
#else
typedef unsigned long int uint32;
#endif

struct MD5Context {
	uint32 buf[4];
	uint32 bits[2];
	unsigned char in[64];
};

void MD5Init(struct MD5Context *context);
void MD5Update(struct MD5Context *context, const void *buf, unsigned len);
void MD5Final(void *digest, struct MD5Context *context);
void MD5Transform(uint32 buf[], uint32 const in[]);

/*
 * This is needed to make RSAREF happy on some MS-DOS compilers.
 */
typedef struct MD5Context MD5_CTX;

#endif
#endif /* !MD5_H */
