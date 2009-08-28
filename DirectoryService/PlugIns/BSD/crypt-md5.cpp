/*
 * Copyright (c) 2003 Poul-Henning Kamp
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
 */

#if 0
#include <sys/cdefs.h>
__FBSDID("$FreeBSD: /repoman/r/ncvs/src/lib/libcrypt/crypt-md5.c,v 1.13 2003/06/02 21:43:14 markm Exp $");
#endif

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <openssl/md5.h>
#include "crypt-md5.h"

static char itoa64[] =		/* 0 ... 63 => ascii - 64 */
	"./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

void
_crypt_to64(char *s, u_long v, int n)
{
	while (--n >= 0) {
		*s++ = itoa64[v&0x3f];
		v >>= 6;
	}
}

char *
crypt_md5(const char *pw, const char *salt)
{
	MD5_CTX	ctx,ctx1;
	u_int32_t l;
	int sl, pl;
	u_int i;
	u_char final[MD5_DIGEST_LENGTH];
	static const char *sp, *ep;
	static char passwd[120], *p;
	static const char *magic = "$1$";

	/* Refine the Salt first */
	sp = salt;

	/* If it starts with the magic string, then skip that */
	if(!strncmp(sp, magic, strlen(magic)))
		sp += strlen(magic);

	/* It stops at the first '$', max 8 chars */
	for(ep = sp; *ep && *ep != '$' && ep < (sp + 8); ep++)
		continue;

	/* get the length of the true salt */
	sl = ep - sp;

	MD5_Init(&ctx);

	/* The password first, since that is what is most unknown */
	MD5_Update(&ctx, (const u_char *)pw, strlen(pw));

	/* Then our magic string */
	MD5_Update(&ctx, (const u_char *)magic, strlen(magic));

	/* Then the raw salt */
	MD5_Update(&ctx, (const u_char *)sp, (u_int)sl);

	/* Then just as many characters of the MD5(pw,salt,pw) */
	MD5_Init(&ctx1);
	MD5_Update(&ctx1, (const u_char *)pw, strlen(pw));
	MD5_Update(&ctx1, (const u_char *)sp, (u_int)sl);
	MD5_Update(&ctx1, (const u_char *)pw, strlen(pw));
	MD5_Final(final, &ctx1);
	for(pl = (int)strlen(pw); pl > 0; pl -= MD5_DIGEST_LENGTH)
		MD5_Update(&ctx, (const u_char *)final,
		    (u_int)(pl > MD5_DIGEST_LENGTH ? MD5_DIGEST_LENGTH : pl));

	/* Don't leave anything around in vm they could use. */
	memset(final, 0, sizeof(final));

	/* Then something really weird... */
	for (i = strlen(pw); i; i >>= 1)
		if(i & 1)
		    MD5_Update(&ctx, (const u_char *)final, 1);
		else
		    MD5_Update(&ctx, (const u_char *)pw, 1);

	/* Now make the output string */
	strcpy(passwd, magic);
	strncat(passwd, sp, (u_int)sl);
	strcat(passwd, "$");

	MD5_Final(final, &ctx);

	/*
	 * and now, just to make sure things don't run too fast
	 * On a 60 Mhz Pentium this takes 34 msec, so you would
	 * need 30 seconds to build a 1000 entry dictionary...
	 */
	for(i = 0; i < 1000; i++) {
		MD5_Init(&ctx1);
		if(i & 1)
			MD5_Update(&ctx1, (const u_char *)pw, strlen(pw));
		else
			MD5_Update(&ctx1, (const u_char *)final, MD5_DIGEST_LENGTH);

		if(i % 3)
			MD5_Update(&ctx1, (const u_char *)sp, (u_int)sl);

		if(i % 7)
			MD5_Update(&ctx1, (const u_char *)pw, strlen(pw));

		if(i & 1)
			MD5_Update(&ctx1, (const u_char *)final, MD5_DIGEST_LENGTH);
		else
			MD5_Update(&ctx1, (const u_char *)pw, strlen(pw));
		MD5_Final(final, &ctx1);
	}

	p = passwd + strlen(passwd);

	l = (final[ 0]<<16) | (final[ 6]<<8) | final[12];
	_crypt_to64(p, l, 4); p += 4;
	l = (final[ 1]<<16) | (final[ 7]<<8) | final[13];
	_crypt_to64(p, l, 4); p += 4;
	l = (final[ 2]<<16) | (final[ 8]<<8) | final[14];
	_crypt_to64(p, l, 4); p += 4;
	l = (final[ 3]<<16) | (final[ 9]<<8) | final[15];
	_crypt_to64(p, l, 4); p += 4;
	l = (final[ 4]<<16) | (final[10]<<8) | final[ 5];
	_crypt_to64(p, l, 4); p += 4;
	l = final[11];
	_crypt_to64(p, l, 2); p += 2;
	*p = '\0';

	/* Don't leave anything around in vm they could use. */
	memset(final, 0, sizeof(final));

	return (passwd);
}
