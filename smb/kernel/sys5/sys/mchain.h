/*
 * Copyright (c) 2000, 2001 Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2001 - 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 */
#ifndef _SYS_MCHAIN_H_
#define _SYS_MCHAIN_H_
 
#include <architecture/byte_order.h>
#include <machine/endian.h>

/*
 * This macros probably belongs to the endian.h
 */
#if (BYTE_ORDER == LITTLE_ENDIAN)

#define htoles(x)	((u_int16_t)(x))
#define letohs(x)	((u_int16_t)(x))
#define	htolel(x)	((u_int32_t)(x))
#define	letohl(x)	((u_int32_t)(x))
#define	htoleq(x)	((u_int64_t)(x))
#define	letohq(x)	((u_int64_t)(x))

#define htobes(x)	(OSSwapInt16(x))
#define betohs(x)	(OSSwapInt16(x))
#define htobel(x)	(OSSwapInt32(x))
#define betohl(x)	(OSSwapInt32(x))
#define	htobeq(x)	(OSSwapInt64(x))
#define	betohq(x)	(OSSwapInt64(x))

#else	/* (BYTE_ORDER == LITTLE_ENDIAN) */

#define htoles(x)	(OSSwapInt16(x))
#define letohs(x)	(OSSwapInt16(x))
#define	htolel(x)	(OSSwapInt32(x))
#define	letohl(x)	(OSSwapInt32(x))
#define	htoleq(x)	(OSSwapInt64(x))
#define	letohq(x)	(OSSwapInt64(x))

#define htobes(x)	((u_int16_t)(x))
#define betohs(x)	((u_int16_t)(x))
#define htobel(x)	((u_int32_t)(x))
#define betohl(x)	((u_int32_t)(x))
#define	htobeq(x)	((u_int64_t)(x))
#define	betohq(x)	((u_int64_t)(x))

#endif	/* (BYTE_ORDER == LITTLE_ENDIAN) */


#ifdef KERNEL

/*
 * Type of copy for mb_{put|get}_mem()
 */
#define	MB_MSYSTEM	0		/* use bcopy() */
#define MB_MINLINE	2		/* use an inline copy loop */
#define	MB_MZERO	3		/* bzero(), mb_put_mem only */
#define	MB_MCUSTOM	4		/* use an user defined function */

struct mbchain;

struct mbchain {
	mbuf_t		mb_top;		/* head of mbufs chain */
	mbuf_t		mb_cur;		/* current mbuf */
	size_t		mb_mleft;	/* free space in the current mbuf */
	size_t		mb_count;	/* total number of bytes */
};

struct mdchain {
	mbuf_t		md_top;		/* head of mbufs chain */
	mbuf_t		md_cur;		/* current mbuf */
	u_char *	md_pos;		/* offset in the current mbuf */
};

size_t  m_fixhdr(mbuf_t m);

int  mb_init(struct mbchain *mbp);
void mb_initm(struct mbchain *mbp, mbuf_t m);
void mb_done(struct mbchain *mbp);
mbuf_t mb_detach(struct mbchain *mbp);
size_t  mb_fixhdr(struct mbchain *mbp);
caddr_t mb_reserve(struct mbchain *mbp, size_t size);

int  mb_put_padbyte(struct mbchain *mbp);
int  mb_put_uint8(struct mbchain *mbp, u_int8_t x);
int  mb_put_uint16be(struct mbchain *mbp, u_int16_t x);
int  mb_put_uint16le(struct mbchain *mbp, u_int16_t x);
int  mb_put_uint32be(struct mbchain *mbp, u_int32_t x);
int  mb_put_uint32le(struct mbchain *mbp, u_int32_t x);
int  mb_put_uint64be(struct mbchain *mbp, u_int64_t x);
int  mb_put_uint64le(struct mbchain *mbp, u_int64_t x);
int  mb_put_mem(struct mbchain *mbp, c_caddr_t source, size_t size, int type);
int  mb_put_mbuf(struct mbchain *mbp, mbuf_t m);
int  mb_put_uio(struct mbchain *mbp, uio_t uiop, size_t size);
int  mb_put_user_mem(struct mbchain *mbp, user_addr_t bufp, int size, off_t offset, vfs_context_t context);

int  md_init(struct mdchain *mdp);
void md_initm(struct mdchain *mbp, mbuf_t m);
void md_done(struct mdchain *mdp);
void md_append_record(struct mdchain *mdp, mbuf_t top);
int  md_next_record(struct mdchain *mdp);
int  md_get_uint8(struct mdchain *mdp, u_int8_t *x);
int  md_get_uint16(struct mdchain *mdp, u_int16_t *x);
int  md_get_uint16le(struct mdchain *mdp, u_int16_t *x);
int  md_get_uint16be(struct mdchain *mdp, u_int16_t *x);
int  md_get_uint32(struct mdchain *mdp, u_int32_t *x);
int  md_get_uint32be(struct mdchain *mdp, u_int32_t *x);
int  md_get_uint32le(struct mdchain *mdp, u_int32_t *x);
int  md_get_uint64(struct mdchain *mdp, u_int64_t *x);
int  md_get_uint64be(struct mdchain *mdp, u_int64_t *x);
int  md_get_uint64le(struct mdchain *mdp, u_int64_t *x);
int  md_get_mem(struct mdchain *mdp, caddr_t target, size_t size, int type);
int  md_get_mbuf(struct mdchain *mdp, size_t size, mbuf_t *m);
int  md_get_uio(struct mdchain *mdp, uio_t uiop, int32_t size);
int  md_get_user_mem(struct mdchain *mbp, user_addr_t bufp, int size, off_t offset, vfs_context_t context);

#endif	/* ifdef KERNEL */

#endif	/* !_SYS_MCHAIN_H_ */
