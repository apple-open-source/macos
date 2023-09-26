/*-
 * Copyright 2005 Colin Percival
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _SHA256_H_
#define _SHA256_H_

#include <CommonCrypto/CommonDigest.h>

#define SHA256_BLOCK_LENGTH		CC_SHA256_BLOCK_BYTES
#define SHA256_DIGEST_LENGTH		CC_SHA256_DIGEST_LENGTH
#define SHA256_DIGEST_STRING_LENGTH	(SHA256_DIGEST_LENGTH * 2 + 1)

#define	SHA256_CTX	CC_SHA256_CTX

#include <sys/cdefs.h>
#include <sys/types.h>

#define SHA256_Init	CC_SHA256_Init
#define SHA256_Update	CC_SHA256_Update
#define SHA256_Final	CC_SHA256_Final

__BEGIN_DECLS
char   *SHA256_End(SHA256_CTX *, char *);
char   *SHA256_Data(const void *, unsigned int, char *);
char   *SHA256_Fd(int, char *);
char   *SHA256_FdChunk(int, char *, off_t, off_t);
char   *SHA256_File(const char *, char *);
char   *SHA256_FileChunk(const char *, char *, off_t, off_t);
__END_DECLS

#endif /* !_SHA256_H_ */
