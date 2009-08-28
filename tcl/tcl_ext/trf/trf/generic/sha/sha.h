#ifndef SHA_H
#define SHA_H

#include <tcl.h>

/* NIST Secure Hash Algorithm */
/* heavily modified from Peter C. Gutmann's implementation */

/* Useful defines & typedefs */

#ifndef _WIN32
typedef unsigned char BYTE;
#endif
#if defined(__alpha) || defined(__LP64__)
typedef unsigned int  UINT32;
#else
#ifndef UINT32
#ifdef _WIN32
#	pragma warning ( disable : 4142 )
#endif
typedef unsigned long UINT32;
#endif
#endif


#define SHA_BLOCKSIZE		64
#define SHA_DIGESTSIZE		20

typedef struct {
    UINT32 digest[5];		/* message digest */
    UINT32 count_lo, count_hi;	/* 64-bit bit count */
    UINT32 data[16];		/* SHA data buffer */
} SHA_INFO;

void sha_init   _ANSI_ARGS_ ((SHA_INFO *));
void sha_update _ANSI_ARGS_ ((SHA_INFO *, BYTE *, int));
void sha_final  _ANSI_ARGS_ ((SHA_INFO *));
void sha_stream _ANSI_ARGS_ ((SHA_INFO *, FILE *));
void sha_print  _ANSI_ARGS_ ((SHA_INFO *));

#endif /* SHA_H */
