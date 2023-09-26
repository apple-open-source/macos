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

#ifndef _SHA384_H_
#define _SHA384_H_

#include <CommonCrypto/CommonDigest.h>

#define SHA384_BLOCK_LENGTH		CC_SHA384_BLOCK_BYTES
#define SHA384_DIGEST_LENGTH		CC_SHA384_DIGEST_LENGTH
#define SHA384_DIGEST_STRING_LENGTH	(SHA384_DIGEST_LENGTH * 2 + 1)

#define	SHA384_CTX	CC_SHA512_CTX

#include <sys/cdefs.h>
#include <sys/types.h>

#define SHA384_Init	CC_SHA384_Init
#define SHA384_Update	CC_SHA384_Update
#define SHA384_Final	CC_SHA384_Final

__BEGIN_DECLS
char   *SHA384_End(SHA384_CTX *, char *);
char   *SHA384_Data(const void *, unsigned int, char *);
char   *SHA384_Fd(int, char *);
char   *SHA384_FdChunk(int, char *, off_t, off_t);
char   *SHA384_File(const char *, char *);
char   *SHA384_FileChunk(const char *, char *, off_t, off_t);
__END_DECLS

#endif /* !_SHA384_H_ */
