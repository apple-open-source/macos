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

#ifndef _OSSL_BIO_H_
#define _OSSL_BIO_H_

/* symbol renaming */
#define BIO_new				ossl_BIO_new
#define BIO_s_mem			ossl_BIO_s_mem
#define BIO_free			ossl_BIO_free
#define BIO_new_mem_buf			ossl_BIO_new_mem_buf
#define BIO_get_mem_data		ossl_BIO_get_mem_data
#define BIO_set_flags			ossl_BIO_set_flags
#define BIO_clear_flags			ossl_BIO_clear_flags
#define BIO_set				ossl_BIO_set
#define BIO_ctrl			ossl_BIO_ctrl
#define BIO_gets			ossl_BIO_gets
#define BIO_write			ossl_BIO_write

#define BIO_new_file			ossl_BIO_new_file
#define BIO_new_fp			ossl_BIO_new_fp
#define BIO_printf			ossl_BIO_printf
#define BIO_s_file			ossl_BIO_s_file
#define BIO_set_fp			ossl_BIO_set_fp
#define BIO_snprintf			ossl_BIO_snprintf
#define BIO_vprintf			ossl_BIO_vprintf
#define BIO_vsnprinf			ossl_BIO_vsnprintf

/* BIO_METHOD types */
#define BIO_TYPE_NONE			0
#define BIO_TYPE_MEM			(1|0x0400)
#define BIO_TYPE_FILE			(2|0x0400)

/* BIO flags */
#define BIO_FLAGS_UPLINK		0
#define BIO_FLAGS_READ			0x01
#define BIO_FLAGS_WRITE			0x02
#define BIO_FLAGS_IO_SPECIAL		0x04
#define BIO_FLAGS_RWS			(BIO_FLAGS_READ|BIO_FLAGS_WRITE|BIO_FLAGS_IO_SPECIAL)
#define BIO_FLAGS_SHOULD_RETRY		0x08

/* BIO_ctrl() commands */
#define BIO_CTRL_RESET			1       /* opt - rewind/zero etc */
#define BIO_CTRL_EOF			2       /* opt - are we at the eof */
#define BIO_CTRL_INFO			3       /* opt - extra tit-bits */
#define BIO_CTRL_SET			4       /* man - set the 'IO' type */
#define BIO_CTRL_GET			5       /* man - get the 'IO' type */
#define BIO_CTRL_PUSH			6       /* opt - internal, used to signify change */
#define BIO_CTRL_POP			7       /* opt - internal, used to signify change */
#define BIO_CTRL_GET_CLOSE		8       /* man - set the 'close' on free */
#define BIO_CTRL_SET_CLOSE		9       /* man - set the 'close' on free */
#define BIO_CTRL_PENDING		10      /* opt - is their more data buffered */
#define BIO_CTRL_FLUSH			11      /* opt - 'flush' buffered output */
#define BIO_CTRL_DUP			12      /* man - extra stuff for 'duped' BIO */
#define BIO_CTRL_WPENDING		13      /* opt - number of bytes still to write */


#define BIO_FLAGS_MEM_RDONLY		0x200

#define BIO_C_SET_FD			104
#define BIO_C_GET_FD			105
#define BIO_C_SET_FILE_PTR		106
#define BIO_C_GET_FILE_PTR		107
#define BIO_C_SET_FILENAME		108
#define BIO_C_FILE_SEEK			128
#define BIO_C_FILE_TELL			133

#define BIO_C_SET_BUF_MEM		114
#define BIO_C_GET_BUF_MEM_PTR		115
#define BIO_C_SET_BUF_MEM_EOF_RETURN	130

/* BIO callback */
#define BIO_CB_FREE			0x01
#define BIO_CB_READ			0x02
#define BIO_CB_WRITE			0x03
#define BIO_CB_PUTS			0x04
#define BIO_CB_GETS			0x05
#define BIO_CB_CTRL			0x06
#define BIO_CB_RETURN			0x80

#define BIO_NOCLOSE			0x00
#define BIO_CLOSE			0x01
#define BIO_FP_READ			0x02
#define BIO_FP_WRITE			0x04
#define BIO_FP_APPEND			0x08
#define BIO_FP_TEXT			0x10

struct bio_st;
typedef struct bio_st	BIO;

typedef void		bio_info_cb (struct bio_st *, int, const char *, int, long, long);

typedef struct bio_method_st {
	int		type;
	const char *	name;
	int (*bwrite)(BIO *, const char *, int);
	int (*bread)(BIO *, char *, int);
	int (*bputs)(BIO *, const char *);
	int (*bgets)(BIO *, char *, int);
	long (*ctrl)(BIO *, int, long, void *);
	int (*create)(BIO *);
	int (*destroy)(BIO *);
	long (*callback_ctrl)(BIO *, int, bio_info_cb *);
} BIO_METHOD;

struct bio_st {
	BIO_METHOD *	method;
	/* bio, mode, argp, argi, argl, ret */
	long		(*callback)(struct bio_st *, int, const char *, int, long, long);
	char *		cb_arg; /* first argument for the callback */

	int		init;
	int		shutdown;
	int		flags; /* extra storage */
	int		retry_reason;
	int		num;
	void *		ptr;
	struct bio_st * next_bio;       /* used by filter BIOs */
	struct bio_st * prev_bio;       /* used by filter BIOs */
	int		references;
	unsigned long	num_read;
	unsigned long	num_write;
#if 0
	CRYPTO_EX_DATA	ex_data;
#endif
};

BIO *BIO_new(BIO_METHOD *);
BIO_METHOD *BIO_s_mem(void);
int BIO_free(BIO *a);
BIO *BIO_new_mem_buf(void *buf, int len);
long BIO_get_mem_data(BIO *b, void *p);
void BIO_set_flags(BIO *b, int flags);
void BIO_clear_flags(BIO *b, int flags);
int BIO_set(BIO *bio, BIO_METHOD *method);
long BIO_ctrl(BIO *b, int cmd, long larg, void *parg);
int BIO_gets(BIO *bp, char *buf, int size);
int BIO_write(BIO *b, const void *in, int inl);

BIO_METHOD *BIO_s_file(void);
BIO *BIO_new_fp(FILE *stream, int close_flag);
BIO *BIO_new_file(const char *filename, const char *mode);
long BIO_set_fp(BIO *b, FILE *fp, long c);

int BIO_printf(BIO *bio, const char *format, ...);
int BIO_snprintf(char *buf, size_t n, const char *format, ...);
int BIO_vprintf(BIO *bio, const char *format, va_list args);
int BIO_vsnprintf(char *buf, size_t n, const char *format, va_list args);

#endif /* _OSSL_BIO_H_ */
