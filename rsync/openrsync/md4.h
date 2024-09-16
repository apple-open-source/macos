/*
 * This is an OpenSSL-compatible implementation of the RSA Data Security, Inc.
 * MD4 Message-Digest Algorithm (RFC 1320).
 *
 * Homepage:
 * http://openwall.info/wiki/people/solar/software/public-domain-source-code/md4
 *
 * Author:
 * Alexander Peslyak, better known as Solar Designer <solar at openwall.com>
 *
 * This software was written by Alexander Peslyak in 2001.  No copyright is
 * claimed, and the software is hereby placed in the public domain.
 * In case this attempt to disclaim copyright and place the software in the
 * public domain is deemed null and void, then the software is
 * Copyright (c) 2001 Alexander Peslyak and it is hereby released to the
 * general public under the following terms:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted.
 *
 * There's ABSOLUTELY NO WARRANTY, express or implied.
 *
 * See md4.c for more information.
 */

#ifndef MD4_H
#define MD4_H

#define	MD4_DIGEST_LENGTH		16

#ifdef __APPLE__
#include <sys/param.h>

/*
 * Shim the MD4 implementation out to libmd, which in turn is largely just
 * a CommonDigest shim.  The intermediate hop is just some macro definitions
 * that are guaranteed to match what openrsync / openbsd expect and we won't
 * actually need to link against libmd, so the indirection costs us  nothing.
 */
#include <md4.h>
#include <limits.h>

#define MD4_Init(ctx)			MD4Init(ctx)

/*
 * Shim MD4_Update out manually, because we have a slight size mismatch.
 * CommonCrypto wants a CC_LONG, which is actually a uint32_t on all platforms,
 * but we use size_t everywhere.
 */
static inline void
MD4_Update(MD4_CTX *ctx, const void *data, size_t len)
{
	size_t resid;

	while (len != 0) {
		resid = MIN(len, UINT_MAX);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
		MD4Update(ctx, data, resid);
#pragma clang diagnostic pop
		len -= resid;
		data += resid;
	}
}

#define	MD4_Final(digest, ctx)		MD4Final(digest, ctx)
#else
/* Any 32-bit or wider unsigned integer data type will do */
typedef unsigned int MD4_u32plus;

typedef struct {
	MD4_u32plus lo, hi;
	MD4_u32plus a, b, c, d;
	unsigned char buffer[64];
	MD4_u32plus block[16];
} MD4_CTX;

extern void MD4_Init(MD4_CTX *ctx);
extern void MD4_Update(MD4_CTX *ctx, const void *data, unsigned long size);
extern void MD4_Final(unsigned char *result, MD4_CTX *ctx);
#endif /* __APPLE__ */

#endif
