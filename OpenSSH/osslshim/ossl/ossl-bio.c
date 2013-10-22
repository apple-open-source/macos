/*
 * Copyright (c) 2011-12 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
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
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "ossl-bio.h"
#include "ossl-buffer.h"
#include "ossl-bn.h"

static int mem_write(BIO *h, const char *buf, int num);
static int mem_read(BIO *h, char *buf, int size);
static int mem_puts(BIO *h, const char *str);
static int mem_gets(BIO *h, char *str, int size);
static long mem_ctrl(BIO *h, int cmd, long arg1, void *arg2);
static int mem_new(BIO *h);
static int mem_free(BIO *data);

static BIO_METHOD mem_method =
{
	.type = BIO_TYPE_MEM,
	.name = "memory buffer",
	.bwrite = mem_write,
	.bread = mem_read,
	.bputs = mem_puts,
	.bgets = mem_gets,
	.ctrl = mem_ctrl,
	.create = mem_new,
	.destroy = mem_free,
	.callback_ctrl = NULL,
};

BIO_METHOD *
BIO_s_mem(void)
{
	return (&mem_method);
}


void
BIO_set_flags(BIO *b, int flags)
{
	b->flags |= flags;
}


void
BIO_clear_flags(BIO *b, int flags)
{
	b->flags &= ~flags;
}


BIO *
BIO_new_mem_buf(void *buf, int len)
{
	BIO *ret;
	BUF_MEM *b;

	if (!buf) {
		return (NULL);
	}

	if (len == -1) {
		len = strlen(buf);
	}

	if (!(ret = BIO_new(BIO_s_mem()))) {
		return (NULL);
	}

	b = (BUF_MEM *)ret->ptr;
	b->data = buf;
	b->length = len;
	b->max = len;

	ret->flags |= BIO_FLAGS_MEM_RDONLY;
	ret->num = 0;

	return (ret);
}


static int
mem_new(BIO *bi)
{
	BUF_MEM *b;

	if ((b = BUF_MEM_new()) == NULL) {
		return (0);
	}

	bi->shutdown = 1;
	bi->init = 1;
	bi->num = -1;
	bi->ptr = (char *)b;

	return (1);
}


static int
mem_free(BIO *a)
{
	if (a == NULL) {
		return (0);
	}

	if (a->shutdown) {
		if ((a->init) && (a->ptr != NULL)) {
			BUF_MEM *b;

			b = (BUF_MEM *)a->ptr;
			if (a->flags & BIO_FLAGS_MEM_RDONLY) {
				b->data = NULL;
			}
			BUF_MEM_free(b);
			a->ptr = NULL;
		}
	}

	return (1);
}


static int
mem_read(BIO *b, char *out, int outl)
{
	int ret = -1;
	BUF_MEM *bm;
	int i;
	char *from, *to;

	bm = (BUF_MEM *)b->ptr;
	BIO_clear_flags(b, (BIO_FLAGS_RWS|BIO_FLAGS_SHOULD_RETRY));

	ret = (outl > bm->length) ? bm->length : outl;

	if ((out != NULL) && (ret > 0)) {
		memcpy(out, bm->data, ret);
		bm->length -= ret;
		if (b->flags & BIO_FLAGS_MEM_RDONLY) {
			bm->data += ret;
		} else {
			from = (char *)&(bm->data[ret]);
			to = (char *)&(bm->data[0]);
			for (i = 0; i < bm->length; i++) {
				to[i] = from[i];
			}
		}
	} else if (bm->length == 0) {
		ret = b->num;

		if (ret != 0) {
			BIO_set_flags(b, (BIO_FLAGS_READ|BIO_FLAGS_SHOULD_RETRY));
		}
	}
	return (ret);
}


static int
mem_write(BIO *b, const char *in, int inl)
{
	int ret = -1;
	int blen;
	BUF_MEM *bm;

	bm = (BUF_MEM *)b->ptr;
	if (in == NULL) {
		goto end;
	}

	if (b->flags & BIO_FLAGS_MEM_RDONLY) {
		goto end;
	}

	BIO_clear_flags(b, (BIO_FLAGS_RWS|BIO_FLAGS_SHOULD_RETRY));

	blen = bm->length;

	if (BUF_MEM_grow_clean(bm, blen+inl) != (blen+inl)) {
		goto end;
	}
	memcpy(&(bm->data[blen]), in, inl);
	ret = inl;
end:
	return (ret);
}


static long
mem_ctrl(BIO *b, int cmd, long num, void *ptr)
{
	long ret = 1;
	char **pptr;

	BUF_MEM *bm = (BUF_MEM *)b->ptr;

	switch (cmd) {
	case BIO_CTRL_RESET:
		if (bm->data != NULL) {
			if (b->flags & BIO_FLAGS_MEM_RDONLY) {
				bm->data -= bm->max - bm->length;
				bm->length = bm->max;
			} else {
				memset(bm->data, 0, bm->max);
				bm->length = 0;
			}
		}
		break;

	case BIO_CTRL_EOF:
		ret = (long)(bm->length == 0);
		break;

	case BIO_C_SET_BUF_MEM_EOF_RETURN:
		b->num = (int)num;
		break;

	case BIO_CTRL_INFO:
		ret = (long)bm->length;
		if (ptr != NULL) {
			pptr = (char **)ptr;
			*pptr = (char *)&(bm->data[0]);
		}
		break;

	case BIO_C_SET_BUF_MEM:
		mem_free(b);
		b->shutdown = (int)num;
		b->ptr = ptr;
		break;

	case BIO_C_GET_BUF_MEM_PTR:
		if (ptr != NULL) {
			pptr = (char **)ptr;
			*pptr = (char *)bm;
		}
		break;

	case BIO_CTRL_GET_CLOSE:
		ret = (long)b->shutdown;
		break;

	case BIO_CTRL_SET_CLOSE:
		b->shutdown = (int)num;
		break;

	case BIO_CTRL_WPENDING:
		ret = 0L;
		break;

	case BIO_CTRL_PENDING:
		ret = (long)bm->length;
		break;

	case BIO_CTRL_DUP:
	case BIO_CTRL_FLUSH:
		ret = 1;
		break;

	case BIO_CTRL_PUSH:
	case BIO_CTRL_POP:
	default:
		ret = 0;
		break;
	}

	return (ret);
}


static int
mem_gets(BIO *bp, char *buf, int size)
{
	int i, j;
	int ret = -1;
	char *p;
	BUF_MEM *bm = (BUF_MEM *)bp->ptr;

	BIO_clear_flags(bp, (BIO_FLAGS_RWS|BIO_FLAGS_SHOULD_RETRY));
	j = bm->length;
	if ((size-1) < j) {
		j = size-1;
	}
	if (j <= 0) {
		*buf = '\0';
		return (0);
	}
	p = bm->data;
	for (i = 0; i < j; i++) {
		if (p[i] == '\n') {
			i++;
			break;
		}
	}

	i = mem_read(bp, buf, i);
	if (i > 0) {
		buf[i] = '\0';
	}
	ret = i;

	return (ret);
}


static int
mem_puts(BIO *bp, const char *str)
{
	int n, ret;

	n = strlen(str);
	ret = mem_write(bp, str, n);
	return (ret);
}


/*
 * File pointer BIO method
 */

static int file_write(BIO *h, const char *buf, int num);
static int file_read(BIO *h, char *buf, int size);
static int file_puts(BIO *h, const char *str);
static int file_gets(BIO *h, char *str, int size);
static long file_ctrl(BIO *h, int cmd, long arg1, void *arg2);
static int file_new(BIO *h);
static int file_free(BIO *data);

static BIO_METHOD methods_filep =
{
	.type = BIO_TYPE_FILE,
	.name = "FILE pointer",
	.bwrite = file_write,
	.bread = file_read,
	.bputs = file_puts,
	.bgets = file_gets,
	.ctrl = file_ctrl,
	.create = file_new,
	.destroy = file_free,
	.callback_ctrl = NULL
};

static int
file_new(BIO *bi)
{
	bi->init = 0;
	bi->num = 0;
	bi->ptr = NULL;
	bi->flags = BIO_FLAGS_UPLINK;

	return (1);
}


static int
file_free(BIO *a)
{
	if (NULL == a) {
		return (0);
	}
	if (a->shutdown) {
		if ((a->init) && (a->ptr != NULL)) {
			fclose((FILE *)a->ptr);
			a->ptr = NULL;
			a->flags = BIO_FLAGS_UPLINK;
		}
		a->init = 0;
	}

	return (1);
}


static int
file_read(BIO *b, char *out, int outl)
{
	int ret = 0;

	if (b->init && (out != NULL)) {
		ret = fread(out, 1, (int)outl, (FILE *)b->ptr);
		if ((ret == 0) && ferror((FILE *)b->ptr)) {
#if 0
			SYSerr(SYS_F_FREAD, get_last_sys_error());
			BIOerr(BIO_F_FILE_READ, ERR_R_SYS_LIB);
#endif
			ret = -1;
		}
	}
	return (ret);
}


static int
file_write(BIO *b, const char *in, int inl)
{
	int ret = 0;

	if (b->init && (in != NULL)) {
		ret = fwrite(in, (int)inl, 1, (FILE *)b->ptr);
		if (ret) {
			ret = inl;
		}
	}
	return (ret);
}


static long
file_ctrl(BIO *b, int cmd, long num, void *ptr)
{
	long ret = 1;
	FILE *fp = (FILE *)b->ptr;
	FILE **fpp;
	char p[4];

	switch (cmd) {
	case BIO_C_FILE_SEEK:
	case BIO_CTRL_RESET:
		ret = (long)fseek(fp, num, 0);
		break;

	case BIO_CTRL_EOF:
		ret = (long)feof(fp);
		break;

	case BIO_C_FILE_TELL:
	case BIO_CTRL_INFO:
		ret = ftell(fp);
		break;

	case BIO_C_SET_FILE_PTR:
		file_free(b);
		b->shutdown = (int)num & BIO_CLOSE;
		b->ptr = ptr;
		b->init = 1;
		break;

	case BIO_C_SET_FILENAME:
		file_free(b);
		b->shutdown = (int)num & BIO_CLOSE;
		if (num & BIO_FP_APPEND) {
			if (num & BIO_FP_READ) {
				BUF_strlcpy(p, "a+", sizeof p);
			} else{
				BUF_strlcpy(p, "a", sizeof p);
			}
		} else if ((num & BIO_FP_READ) && (num & BIO_FP_WRITE)) {
			BUF_strlcpy(p, "r+", sizeof p);
		} else if (num & BIO_FP_WRITE) {
			BUF_strlcpy(p, "w", sizeof p);
		} else if (num & BIO_FP_READ) {
			BUF_strlcpy(p, "r", sizeof p);
		} else{
			/* BIOerr(BIO_F_FILE_CTRL,BIO_R_BAD_FOPEN_MODE); */
			ret = 0;
			break;
		}

		fp = fopen(ptr, p);
		if (fp == NULL) {
#if 0
			SYSerr(SYS_F_FOPEN, get_last_sys_error());
			ERR_add_error_data(5, "fopen('", ptr, "','", p, "')");
			BIOerr(BIO_F_FILE_CTRL, ERR_R_SYS_LIB);
#endif
			ret = 0;
			break;
		}
		b->ptr = fp;
		b->init = 1;
		BIO_clear_flags(b, BIO_FLAGS_UPLINK);
		break;

	case BIO_C_GET_FILE_PTR:
		if (ptr != NULL) {
			fpp = (FILE **)ptr;
			*fpp = (FILE *)b->ptr;
		}
		break;

	case BIO_CTRL_GET_CLOSE:
		ret = (long)b->shutdown;
		break;

	case BIO_CTRL_SET_CLOSE:
		b->shutdown = (int)num;
		break;

	case BIO_CTRL_FLUSH:
		fflush((FILE *)b->ptr);
		break;

	case BIO_CTRL_DUP:
		ret = 1;
		break;

	case BIO_CTRL_WPENDING:
	case BIO_CTRL_PENDING:
	case BIO_CTRL_PUSH:
	case BIO_CTRL_POP:
	default:
		ret = 0;
		break;
	}

	return (ret);
}


static int
file_gets(BIO *bp, char *buf, int size)
{
	int ret = 0;

	buf[0] = '\0';

	if (!fgets(buf, size, (FILE *)bp->ptr)) {
		goto err;
	}

	if (buf[0] != '\0') {
		ret = strlen(buf);
	}
err:
	return (ret);
}


static int
file_puts(BIO *bp, const char *str)
{
	int n, ret;

	n = strlen(str);
	ret = file_write(bp, str, n);
	return (ret);
}


BIO *
BIO_new_file(const char *filename, const char *mode)
{
	BIO *ret;
	FILE *file;

	if ((file = fopen(filename, mode)) == NULL) {
		/* SYSerr(SYS_F_FOPEN,get_last_sys_error()); */
		/* ERR_add_error_data(5,"fopen('",filename,"','",mode,"')"); */
#if 0
		if (errno == ENOENT) {
			BIOerr(BIO_F_BIO_NEW_FILE, BIO_R_NO_SUCH_FILE);
		} else{
			BIOerr(BIO_F_BIO_NEW_FILE, ERR_R_SYS_LIB);
		}
#endif
		return (NULL);
	}

	if ((ret = BIO_new(BIO_s_file())) == NULL) {
		fclose(file);
		return (NULL);
	}
	BIO_clear_flags(ret, BIO_FLAGS_UPLINK);
	BIO_set_fp(ret, file, BIO_CLOSE);
	return (ret);
}


BIO *
BIO_new_fp(FILE *stream, int close_flag)
{
	BIO *ret;

	if ((ret = BIO_new(BIO_s_file())) == NULL) {
		return (NULL);
	}

	BIO_set_flags(ret, BIO_FLAGS_UPLINK);
	BIO_set_fp(ret, stream, close_flag);

	return (ret);
}


BIO_METHOD *
BIO_s_file(void)
{
	return (&methods_filep);
}


int
BIO_set(BIO *bio, BIO_METHOD *method)
{
	bio->method = method;
	bio->callback = NULL;
	bio->cb_arg = NULL;
	bio->init = 0;
	bio->shutdown = 1;
	bio->flags = 0;
	bio->retry_reason = 0;
	bio->num = 0;
	bio->ptr = NULL;
	bio->prev_bio = NULL;
	bio->next_bio = NULL;
	bio->references = 1;
	bio->num_read = 0L;
	bio->num_write = 0L;
#if 0
	CRYPTO_new_ex_data(CRYPTO_EX_INDEX_BIO, bio, &bio->ex_data);
#endif
	if (method->create != NULL) {
		if (!method->create(bio)) {
#if 0
			CRYPTO_free_ex_data(CRYPTO_EX_INDEX_BIO, bio,
			    &bio->ex_data);
#endif
			return (0);
		}
	}

	return (1);
}


long
BIO_ctrl(BIO *b, int cmd, long larg, void *parg)
{
	long ret;

	long (*cb)(BIO *, int, const char *, int, long, long);

	if (NULL == b) {
		return (0);
	}

	if ((b->method == NULL) || (b->method->ctrl == NULL)) {
		return (-2);
	}

	cb = b->callback;

	if ((cb != NULL) && ((ret = cb(b, BIO_CB_CTRL, parg, cmd, larg, 1L)) <= 0)) {
		return (ret);
	}

	ret = b->method->ctrl(b, cmd, larg, parg);

	if (cb != NULL) {
		ret = cb(b, BIO_CB_CTRL|BIO_CB_RETURN, parg, cmd, larg, ret);
	}

	return (ret);
}


long
BIO_get_mem_data(BIO *b, void *pp)
{
	return (BIO_ctrl(b, BIO_CTRL_INFO, 0, pp));
}


long
BIO_set_fp(BIO *b, FILE *fp, long c)
{
	return (BIO_ctrl(b, BIO_C_SET_FILE_PTR, c, (void *)fp));
}


BIO *
BIO_new(BIO_METHOD *method)
{
	BIO *ret = NULL;

	ret = (BIO *)malloc(sizeof(BIO));
	if (NULL == ret) {
		return (NULL);
	}
	if (!BIO_set(ret, method)) {
		free(ret);
		ret = NULL;
	}

	return (ret);
}


int
BIO_free(BIO *a)
{
	int i;

	if (NULL == a) {
		return (0);
	}

	if ((a->callback != NULL) &&
	    ((i = (int)a->callback(a, BIO_CB_FREE, NULL, 0, 0L, 1L)) <= 0)) {
		return (i);
	}

#if 0
	CRYPTO_free_ex_data(CRYPTO_EX_INDEX_BIO, a, &a->ex_data);
#endif

	if ((a->method == NULL) || (a->method->destroy == NULL)) {
		return (1);
	}

	a->method->destroy(a);
	free(a);

	return (1);
}


int
BIO_write(BIO *b, const void *in, int inl)
{
	int i;

	long (*cb)(BIO *, int, const char *, int, long, long);

	if (b == NULL) {
		return (0);
	}

	cb = b->callback;
	if ((NULL == b->method) || (NULL == b->method->bwrite)) {
		/* BIOerr(BIO_F_BIO_WRITE,BIO_R_UNSUPPORTED_METHOD); */
		return (-2);
	}

	if ((cb != NULL) && ((i = (int)cb(b, BIO_CB_WRITE, in, inl, 0L, 1L)) <= 0)) {
		return (i);
	}

	if (!b->init) {
		/* BIOerr(BIO_F_BIO_WRITE,BIO_R_UNINITIALIZED); */
		return (-2);
	}

	i = b->method->bwrite(b, in, inl);

	if (i > 0) {
		b->num_write += (unsigned long)i;
	}

	if (cb != NULL) {
		i = (int)cb(b, BIO_CB_WRITE | BIO_CB_RETURN, in, inl, 0L, (long)i);
	}

	return (i);
}


int
BIO_gets(BIO *b, char *in, int inl)
{
	int i;

	long (*cb)(BIO *, int, const char *, int, long, long);
	if ((NULL == b) || (NULL == b->method) || (NULL == b->method->bgets)) {
		/* BIOerr(BIO_F_BIO_GETS,BIO_R_UNSUPPORTED_METHOD); */
		return (-2);
	}

	cb = b->callback;
	if ((cb != NULL) && ((i = (int)cb(b, BIO_CB_GETS, in, inl, 0L, 1L)) <= 0)) {
		return (i);
	}

	if (!b->init) {
		/* BIOerr(BIO_F_BIO_GETS,BIO_R_UNINITIALIZED); */
		return (-2);
	}

	i = b->method->bgets(b, in, inl);

	if (cb != NULL) {
		i = (int)cb(b, BIO_CB_GETS | BIO_CB_RETURN, in, inl, 0L, (long)i);
	}

	return (i);
}


/* disable assert() unless BIO_DEBUG has been defined */
#ifndef BIO_DEBUG
# ifndef NDEBUG
#  define NDEBUG
# endif
#endif

#if defined(BN_LLONG) || defined(SIXTY_FOUR_BIT)
# ifndef HAVE_LONG_LONG
#  define HAVE_LONG_LONG    1
# endif
#endif

/***************************************************************************/

/*
 * Copyright Patrick Powell 1995
 * This code is based on code written by Patrick Powell <papowell@astart.com>
 * It may be used for any purpose as long as this notice remains intact
 * on all source code distributions.
 */

/*
 * This code contains numerious changes and enhancements which were
 * made by lots of contributors over the last years to Patrick Powell's
 * original code:
 *
 * o Patrick Powell <papowell@astart.com>      (1995)
 * o Brandon Long <blong@fiction.net>          (1996, for Mutt)
 * o Thomas Roessler <roessler@guug.de>        (1998, for Mutt)
 * o Michael Elkins <me@cs.hmc.edu>            (1998, for Mutt)
 * o Andrew Tridgell <tridge@samba.org>        (1998, for Samba)
 * o Luke Mewburn <lukem@netbsd.org>           (1999, for LukemFTP)
 * o Ralf S. Engelschall <rse@engelschall.com> (1999, for Pth)
 * o ...                                       (for OpenSSL)
 */

#ifdef HAVE_LONG_DOUBLE
#define LDOUBLE		long double
#else
#define LDOUBLE		double
#endif

#if HAVE_LONG_LONG
# if defined(OPENSSL_SYS_WIN32) && !defined(__GNUC__)
# define LLONG		__int64
# else
# define LLONG		long long
# endif
#else
#define LLONG		long
#endif

static void fmtstr(char **, char **, size_t *, size_t *,
    const char *, int, int, int);
static void fmtint(char **, char **, size_t *, size_t *,
    LLONG, int, int, int, int);
static void fmtfp(char **, char **, size_t *, size_t *,
    LDOUBLE, int, int, int);
static void doapr_outch(char **, char **, size_t *, size_t *, int);
static void _dopr(char **sbuffer, char **buffer,
    size_t *maxlen, size_t *retlen, int *truncated,
    const char *format, va_list args);

/* format read states */
#define DP_S_DEFAULT		0
#define DP_S_FLAGS		1
#define DP_S_MIN		2
#define DP_S_DOT		3
#define DP_S_MAX		4
#define DP_S_MOD		5
#define DP_S_CONV		6
#define DP_S_DONE		7

/* format flags - Bits */
#define DP_F_MINUS		(1 << 0)
#define DP_F_PLUS		(1 << 1)
#define DP_F_SPACE		(1 << 2)
#define DP_F_NUM		(1 << 3)
#define DP_F_ZERO		(1 << 4)
#define DP_F_UP			(1 << 5)
#define DP_F_UNSIGNED		(1 << 6)

/* conversion flags */
#define DP_C_SHORT		1
#define DP_C_LONG		2
#define DP_C_LDOUBLE		3
#define DP_C_LLONG		4

/* some handy macros */
#define char_to_int(p)		(p - '0')
#define OSSL_MAX(p, q)		((p >= q) ? p : q)

static void
_dopr(
	char **sbuffer,
	char **buffer,
	size_t *maxlen,
	size_t *retlen,
	int *truncated,
	const char *format,
	va_list args)
{
	char ch;
	LLONG value;
	LDOUBLE fvalue;
	char *strvalue;
	int min;
	int max;
	int state;
	int flags;
	int cflags;
	size_t currlen;

	state = DP_S_DEFAULT;
	flags = currlen = cflags = min = 0;
	max = -1;
	ch = *format++;

	while (state != DP_S_DONE) {
		if ((ch == '\0') || ((buffer == NULL) && (currlen >= *maxlen))) {
			state = DP_S_DONE;
		}

		switch (state) {
		case DP_S_DEFAULT:
			if (ch == '%') {
				state = DP_S_FLAGS;
			} else{
				doapr_outch(sbuffer, buffer, &currlen, maxlen, ch);
			}
			ch = *format++;
			break;

		case DP_S_FLAGS:
			switch (ch) {
			case '-':
				flags |= DP_F_MINUS;
				ch = *format++;
				break;

			case '+':
				flags |= DP_F_PLUS;
				ch = *format++;
				break;

			case ' ':
				flags |= DP_F_SPACE;
				ch = *format++;
				break;

			case '#':
				flags |= DP_F_NUM;
				ch = *format++;
				break;

			case '0':
				flags |= DP_F_ZERO;
				ch = *format++;
				break;

			default:
				state = DP_S_MIN;
				break;
			}
			break;

		case DP_S_MIN:
			if (isdigit((unsigned char)ch)) {
				min = 10 * min + char_to_int(ch);
				ch = *format++;
			} else if (ch == '*') {
				min = va_arg(args, int);
				ch = *format++;
				state = DP_S_DOT;
			} else{
				state = DP_S_DOT;
			}
			break;

		case DP_S_DOT:
			if (ch == '.') {
				state = DP_S_MAX;
				ch = *format++;
			} else{
				state = DP_S_MOD;
			}
			break;

		case DP_S_MAX:
			if (isdigit((unsigned char)ch)) {
				if (max < 0) {
					max = 0;
				}
				max = 10 * max + char_to_int(ch);
				ch = *format++;
			} else if (ch == '*') {
				max = va_arg(args, int);
				ch = *format++;
				state = DP_S_MOD;
			} else{
				state = DP_S_MOD;
			}
			break;

		case DP_S_MOD:
			switch (ch) {
			case 'h':
				cflags = DP_C_SHORT;
				ch = *format++;
				break;

			case 'l':
				if (*format == 'l') {
					cflags = DP_C_LLONG;
					format++;
				} else{
					cflags = DP_C_LONG;
				}
				ch = *format++;
				break;

			case 'q':
				cflags = DP_C_LLONG;
				ch = *format++;
				break;

			case 'L':
				cflags = DP_C_LDOUBLE;
				ch = *format++;
				break;

			default:
				break;
			}
			state = DP_S_CONV;
			break;

		case DP_S_CONV:
			switch (ch) {
			case 'd':
			case 'i':
				switch (cflags) {
				case DP_C_SHORT:
					value = (short int)va_arg(args, int);
					break;

				case DP_C_LONG:
					value = va_arg(args, long int);
					break;

				case DP_C_LLONG:
					value = va_arg(args, LLONG);
					break;

				default:
					value = va_arg(args, int);
					break;
				}
				fmtint(sbuffer, buffer, &currlen, maxlen,
				    value, 10, min, max, flags);
				break;

			case 'X':
				flags |= DP_F_UP;

			/* FALLTHROUGH */
			case 'x':
			case 'o':
			case 'u':
				flags |= DP_F_UNSIGNED;
				switch (cflags) {
				case DP_C_SHORT:
					value = (unsigned short int)va_arg(args, unsigned int);
					break;

				case DP_C_LONG:
					value = (LLONG)va_arg(args,
						unsigned long int);
					break;

				case DP_C_LLONG:
					value = va_arg(args, unsigned LLONG);
					break;

				default:
					value = (LLONG)va_arg(args,
						unsigned int);
					break;
				}
				fmtint(sbuffer, buffer, &currlen, maxlen, value,
				    ch == 'o' ? 8 : (ch == 'u' ? 10 : 16),
				    min, max, flags);
				break;

			case 'f':
				if (cflags == DP_C_LDOUBLE) {
					fvalue = va_arg(args, LDOUBLE);
				} else{
					fvalue = va_arg(args, double);
				}
				fmtfp(sbuffer, buffer, &currlen, maxlen,
				    fvalue, min, max, flags);
				break;

			case 'E':
				flags |= DP_F_UP;

			case 'e':
				if (cflags == DP_C_LDOUBLE) {
					fvalue = va_arg(args, LDOUBLE);
				} else{
					fvalue = va_arg(args, double);
				}
				break;

			case 'G':
				flags |= DP_F_UP;

			case 'g':
				if (cflags == DP_C_LDOUBLE) {
					fvalue = va_arg(args, LDOUBLE);
				} else{
					fvalue = va_arg(args, double);
				}
				break;

			case 'c':
				doapr_outch(sbuffer, buffer, &currlen, maxlen,
				    va_arg(args, int));
				break;

			case 's':
				strvalue = va_arg(args, char *);
				if (max < 0) {
					if (buffer) {
						max = INT_MAX;
					} else{
						max = *maxlen;
					}
				}
				fmtstr(sbuffer, buffer, &currlen, maxlen, strvalue,
				    flags, min, max);
				break;

			case 'p':
				value = (long)va_arg(args, void *);
				fmtint(sbuffer, buffer, &currlen, maxlen,
				    value, 16, min, max, flags|DP_F_NUM);
				break;

			case 'n': /* XXX */
				if (cflags == DP_C_SHORT) {
					short int *num;
					num = va_arg(args, short int *);
					*num = currlen;
				} else if (cflags == DP_C_LONG) { /* XXX */
					long int *num;
					num = va_arg(args, long int *);
					*num = (long int)currlen;
				} else if (cflags == DP_C_LLONG) { /* XXX */
					LLONG *num;
					num = va_arg(args, LLONG *);
					*num = (LLONG)currlen;
				} else {
					int *num;
					num = va_arg(args, int *);
					*num = currlen;
				}
				break;

			case '%':
				doapr_outch(sbuffer, buffer, &currlen, maxlen, ch);
				break;

			case 'w':
				/* not supported yet, treat as next char */
				ch = *format++;
				break;

			default:
				/* unknown, skip */
				break;
			}
			ch = *format++;
			state = DP_S_DEFAULT;
			flags = cflags = min = 0;
			max = -1;
			break;

		case DP_S_DONE:
			break;

		default:
			break;
		}
	}
	*truncated = (currlen > *maxlen - 1);
	if (*truncated) {
		currlen = *maxlen - 1;
	}
	doapr_outch(sbuffer, buffer, &currlen, maxlen, '\0');
	*retlen = currlen - 1;
}


static void
fmtstr(
	char **sbuffer,
	char **buffer,
	size_t *currlen,
	size_t *maxlen,
	const char *value,
	int flags,
	int min,
	int max)
{
	int padlen, strln;
	int cnt = 0;

	if (value == 0) {
		value = "<NULL>";
	}
	for (strln = 0; value[strln]; ++strln) {
	}
	padlen = min - strln;
	if (padlen < 0) {
		padlen = 0;
	}
	if (flags & DP_F_MINUS) {
		padlen = -padlen;
	}

	while ((padlen > 0) && (cnt < max)) {
		doapr_outch(sbuffer, buffer, currlen, maxlen, ' ');
		--padlen;
		++cnt;
	}
	while (*value && (cnt < max)) {
		doapr_outch(sbuffer, buffer, currlen, maxlen, *value++);
		++cnt;
	}
	while ((padlen < 0) && (cnt < max)) {
		doapr_outch(sbuffer, buffer, currlen, maxlen, ' ');
		++padlen;
		++cnt;
	}
}


#ifndef DECIMAL_SIZE
#define DECIMAL_SIZE(type)    ((sizeof(type)*8+2)/3+1)
#endif

static void
fmtint(
	char **sbuffer,
	char **buffer,
	size_t *currlen,
	size_t *maxlen,
	LLONG value,
	int base,
	int min,
	int max,
	int flags)
{
	int signvalue = 0;
	const char *prefix = "";
	unsigned LLONG uvalue;
	char convert[DECIMAL_SIZE(value)+3];
	int place = 0;
	int spadlen = 0;
	int zpadlen = 0;
	int caps = 0;

	if (max < 0) {
		max = 0;
	}
	uvalue = value;
	if (!(flags & DP_F_UNSIGNED)) {
		if (value < 0) {
			signvalue = '-';
			uvalue = -value;
		} else if (flags & DP_F_PLUS) {
			signvalue = '+';
		} else if (flags & DP_F_SPACE) {
			signvalue = ' ';
		}
	}
	if (flags & DP_F_NUM) {
		if (base == 8) {
			prefix = "0";
		}
		if (base == 16) {
			prefix = "0x";
		}
	}
	if (flags & DP_F_UP) {
		caps = 1;
	}
	do {
		convert[place++] =
		    (caps ? "0123456789ABCDEF" : "0123456789abcdef")
		    [uvalue % (unsigned)base];
		uvalue = (uvalue / (unsigned)base);
	} while (uvalue && (place < (int)sizeof(convert)));
	if (place == sizeof(convert)) {
		place--;
	}
	convert[place] = 0;

	zpadlen = max - place;
	spadlen = min - OSSL_MAX(max, place) - (signvalue ? 1 : 0) - strlen(prefix);
	if (zpadlen < 0) {
		zpadlen = 0;
	}
	if (spadlen < 0) {
		spadlen = 0;
	}
	if (flags & DP_F_ZERO) {
		zpadlen = OSSL_MAX(zpadlen, spadlen);
		spadlen = 0;
	}
	if (flags & DP_F_MINUS) {
		spadlen = -spadlen;
	}

	/* spaces */
	while (spadlen > 0) {
		doapr_outch(sbuffer, buffer, currlen, maxlen, ' ');
		--spadlen;
	}

	/* sign */
	if (signvalue) {
		doapr_outch(sbuffer, buffer, currlen, maxlen, signvalue);
	}

	/* prefix */
	while (*prefix) {
		doapr_outch(sbuffer, buffer, currlen, maxlen, *prefix);
		prefix++;
	}

	/* zeros */
	if (zpadlen > 0) {
		while (zpadlen > 0) {
			doapr_outch(sbuffer, buffer, currlen, maxlen, '0');
			--zpadlen;
		}
	}
	/* digits */
	while (place > 0) {
		doapr_outch(sbuffer, buffer, currlen, maxlen, convert[--place]);
	}

	/* left justified spaces */
	while (spadlen < 0) {
		doapr_outch(sbuffer, buffer, currlen, maxlen, ' ');
		++spadlen;
	}
}


static LDOUBLE
abs_val(LDOUBLE value)
{
	LDOUBLE result = value;

	if (value < 0) {
		result = -value;
	}
	return (result);
}


static LDOUBLE
pow_10(int in_exp)
{
	LDOUBLE result = 1;

	while (in_exp) {
		result *= 10;
		in_exp--;
	}
	return (result);
}


static long
roundv(LDOUBLE value)
{
	long intpart;

	intpart = (long)value;
	value = value - intpart;
	if (value >= 0.5) {
		intpart++;
	}
	return (intpart);
}


static void
fmtfp(
	char **sbuffer,
	char **buffer,
	size_t *currlen,
	size_t *maxlen,
	LDOUBLE fvalue,
	int min,
	int max,
	int flags)
{
	int signvalue = 0;
	LDOUBLE ufvalue;
	char iconvert[20];
	char fconvert[20];
	int iplace = 0;
	int fplace = 0;
	int padlen = 0;
	int zpadlen = 0;
	int caps = 0;
	long intpart;
	long fracpart;
	long max10;

	if (max < 0) {
		max = 6;
	}
	ufvalue = abs_val(fvalue);
	if (fvalue < 0) {
		signvalue = '-';
	} else if (flags & DP_F_PLUS) {
		signvalue = '+';
	} else if (flags & DP_F_SPACE) {
		signvalue = ' ';
	}

	intpart = (long)ufvalue;

	/* sorry, we only support 9 digits past the decimal because of our
	 * conversion method */
	if (max > 9) {
		max = 9;
	}

	/* we "cheat" by converting the fractional part to integer by
	 * multiplying by a factor of 10 */
	max10 = roundv(pow_10(max));
	fracpart = roundv(pow_10(max) * (ufvalue - intpart));

	if (fracpart >= max10) {
		intpart++;
		fracpart -= max10;
	}

	/* convert integer part */
	do {
		iconvert[iplace++] =
		    (caps ? "0123456789ABCDEF"
		    : "0123456789abcdef")[intpart % 10];
		intpart = (intpart / 10);
	} while (intpart && (iplace < (int)sizeof(iconvert)));
	if (iplace == sizeof iconvert) {
		iplace--;
	}
	iconvert[iplace] = 0;

	/* convert fractional part */
	do {
		fconvert[fplace++] =
		    (caps ? "0123456789ABCDEF"
		    : "0123456789abcdef")[fracpart % 10];
		fracpart = (fracpart / 10);
	} while (fplace < max);
	if (fplace == sizeof fconvert) {
		fplace--;
	}
	fconvert[fplace] = 0;

	/* -1 for decimal point, another -1 if we are printing a sign */
	padlen = min - iplace - max - 1 - ((signvalue) ? 1 : 0);
	zpadlen = max - fplace;
	if (zpadlen < 0) {
		zpadlen = 0;
	}
	if (padlen < 0) {
		padlen = 0;
	}
	if (flags & DP_F_MINUS) {
		padlen = -padlen;
	}

	if ((flags & DP_F_ZERO) && (padlen > 0)) {
		if (signvalue) {
			doapr_outch(sbuffer, buffer, currlen, maxlen, signvalue);
			--padlen;
			signvalue = 0;
		}
		while (padlen > 0) {
			doapr_outch(sbuffer, buffer, currlen, maxlen, '0');
			--padlen;
		}
	}
	while (padlen > 0) {
		doapr_outch(sbuffer, buffer, currlen, maxlen, ' ');
		--padlen;
	}
	if (signvalue) {
		doapr_outch(sbuffer, buffer, currlen, maxlen, signvalue);
	}

	while (iplace > 0) {
		doapr_outch(sbuffer, buffer, currlen, maxlen, iconvert[--iplace]);
	}

	/*
	 * Decimal point. This should probably use locale to find the correct
	 * char to print out.
	 */
	if ((max > 0) || (flags & DP_F_NUM)) {
		doapr_outch(sbuffer, buffer, currlen, maxlen, '.');

		while (fplace > 0) {
			doapr_outch(sbuffer, buffer, currlen, maxlen, fconvert[--fplace]);
		}
	}
	while (zpadlen > 0) {
		doapr_outch(sbuffer, buffer, currlen, maxlen, '0');
		--zpadlen;
	}

	while (padlen < 0) {
		doapr_outch(sbuffer, buffer, currlen, maxlen, ' ');
		++padlen;
	}
}


static void
doapr_outch(
	char **sbuffer,
	char **buffer,
	size_t *currlen,
	size_t *maxlen,
	int c)
{
	/* If we haven't at least one buffer, someone has doe a big booboo */
	assert(*sbuffer != NULL || buffer != NULL);

	if (buffer) {
		while (*currlen >= *maxlen) {
			if (*buffer == NULL) {
				if (*maxlen == 0) {
					*maxlen = 1024;
				}
				*buffer = malloc(*maxlen);
				if (*currlen > 0) {
					assert(*sbuffer != NULL);
					memcpy(*buffer, *sbuffer, *currlen);
				}
				*sbuffer = NULL;
			} else {
				*maxlen += 1024;
				*buffer = realloc(*buffer, *maxlen);
			}
		}
		/* What to do if *buffer is NULL? */
		assert(*sbuffer != NULL || *buffer != NULL);
	}

	if (*currlen < *maxlen) {
		if (*sbuffer) {
			(*sbuffer)[(*currlen)++] = (char)c;
		} else{
			(*buffer)[(*currlen)++] = (char)c;
		}
	}
}


/***************************************************************************/

int
BIO_printf(BIO *bio, const char *format, ...)
{
	va_list args;
	int ret;

	va_start(args, format);

	ret = BIO_vprintf(bio, format, args);

	va_end(args);
	return (ret);
}


int
BIO_vprintf(BIO *bio, const char *format, va_list args)
{
	int ret;
	size_t retlen;
	char hugebuf[1024*2];   /* Was previously 10k, which is unreasonable
	                         * in small-stack environments, like threads
	                         * or DOS programs. */
	char *hugebufp = hugebuf;
	size_t hugebufsize = sizeof(hugebuf);
	char *dynbuf = NULL;
	int ignored;

	dynbuf = NULL;
	/* CRYPTO_push_info("doapr()"); */
	_dopr(&hugebufp, &dynbuf, &hugebufsize,
	    &retlen, &ignored, format, args);
	if (dynbuf) {
		ret = BIO_write(bio, dynbuf, (int)retlen);
		free(dynbuf);
	} else {
		ret = BIO_write(bio, hugebuf, (int)retlen);
	}
	/* CRYPTO_pop_info(); */
	return (ret);
}


/* As snprintf is not available everywhere, we provide our own implementation.
 * This function has nothing to do with BIOs, but it's closely related
 * to BIO_printf, and we need *some* name prefix ...
 * (XXX  the function should be renamed, but to what?) */
int BIO_snprintf(char *buf, size_t n, const char *format, ...)
{
	va_list args;
	int ret;

	va_start(args, format);

	ret = BIO_vsnprintf(buf, n, format, args);

	va_end(args);
	return (ret);
}


int BIO_vsnprintf(char *buf, size_t n, const char *format, va_list args)
{
	size_t retlen;
	int truncated;

	_dopr(&buf, NULL, &n, &retlen, &truncated, format, args);

	if (truncated) {
		/* In case of truncation, return -1 like traditional snprintf.
		 * (Current drafts for ISO/IEC 9899 say snprintf should return
		 * the number of characters that would have been written,
		 * had the buffer been large enough.) */
		return (-1);
	} else{
		return ((retlen <= INT_MAX) ? (int)retlen : -1);
	}
}
