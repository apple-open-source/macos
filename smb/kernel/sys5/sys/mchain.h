/*
 * Copyright (c) 2000, 2001 Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2001 - 2012 Apple Inc. All rights reserved.
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
 
/*
 * Type of copy for mb_{put|get}_mem()
 */
#define	MB_MSYSTEM	0		/* use bcopy() */
#define MB_MINLINE	2		/* use an inline copy loop */
#define	MB_MZERO	3		/* bzero(), mb_put_mem only */
#define	MB_MCUSTOM	4		/* use an user defined function */


struct mbchain {
	mbuf_t		mb_top;		/* head of mbufs chain */
	mbuf_t		mb_cur;		/* current mbuf */
	size_t		mb_mleft;	/* free space in the current mbuf */
	size_t		mb_count;	/* used for byte counting */
	size_t		mb_len;     /* nbr bytes added in SMB2 compound requests */
};

struct mdchain {
	mbuf_t		md_top;		/* head of mbufs chain */
	mbuf_t		md_cur;		/* current mbuf */
	u_char *	md_pos;		/* offset in the current mbuf */
	size_t		md_len;     /* nbr bytes parsed in SMB2 compound replies */
};

typedef	struct mbchain* mbchain_t;
typedef	struct mdchain* mdchain_t;


size_t  m_fixhdr(mbuf_t );

int  mb_init(mbchain_t );
void mb_done(mbchain_t );

mbuf_t mb_detach(mbchain_t );				/* KERNEL */
int mb_pullup(mbchain_t);					/* USERLAND */
void * mb_getbuffer(mbchain_t , size_t );	/* USERLAND */
void mb_consume(mbchain_t , size_t );		/* USERLAND */

size_t  mb_fixhdr(mbchain_t );
void * mb_reserve(mbchain_t , size_t size);

int  mb_put_padbyte(mbchain_t );	/* KERENL */
int  mb_put_uint8(mbchain_t , uint8_t );
int  mb_put_uint16be(mbchain_t , uint16_t );
int  mb_put_uint16le(mbchain_t , uint16_t );
int  mb_put_uint32be(mbchain_t , uint32_t );
int  mb_put_uint32le(mbchain_t , uint32_t );
int  mb_put_uint64be(mbchain_t , uint64_t );
int  mb_put_uint64le(mbchain_t , uint64_t );

int  mb_put_mem(mbchain_t , const char * , size_t , int );
int  mb_put_mbuf(mbchain_t , mbuf_t );

#ifdef KERNEL
void mbuf_cat_internal(mbuf_t md_top, mbuf_t m0);
int  mb_put_uio(mbchain_t mbp, uio_t uiop, size_t size);
int  mb_put_user_mem(mbchain_t mbp, user_addr_t bufp, int size, off_t offset, vfs_context_t context);
#endif // KERNEL

int  md_init(mdchain_t mdp);
#ifndef KERNEL
/* Non kernel special verson when the receive buffer is greater than page size */
int  md_init_rcvsize(mdchain_t, size_t);
#endif // KERNEL
void md_shadow_copy(const mdchain_t mdp, mdchain_t shadow);

void md_initm(mdchain_t mbp, mbuf_t m);	/* KERNEL */
void md_done(mdchain_t mdp);

void md_append_record(mdchain_t mdp, mbuf_t top);
int  md_next_record(mdchain_t mdp);

int  md_get_uint8(mdchain_t mdp, uint8_t *x);
int  md_get_uint16(mdchain_t mdp, uint16_t *x);
int  md_get_uint16le(mdchain_t mdp, uint16_t *x);
int  md_get_uint16be(mdchain_t mdp, uint16_t *x);
int  md_get_uint32(mdchain_t mdp, uint32_t *x);
int  md_get_uint32be(mdchain_t mdp, uint32_t *x);
int  md_get_uint32le(mdchain_t mdp, uint32_t *x);
int  md_get_uint64(mdchain_t mdp, uint64_t *x);
int  md_get_uint64be(mdchain_t mdp, uint64_t *x);
int  md_get_uint64le(mdchain_t mdp, uint64_t *x);

size_t md_get_utf16_strlen(mdchain_t mdp);
size_t md_get_size(mdchain_t mdp);
int  md_get_mem(mdchain_t mdp, caddr_t target, size_t size, int type);

#ifdef KERNEL
int  md_get_mbuf(mdchain_t mdp, size_t size, mbuf_t *m);
int  md_get_uio(mdchain_t mdp, uio_t uiop, int32_t size);
int  md_get_user_mem(mdchain_t mbp, user_addr_t bufp, int size, off_t offset, vfs_context_t context);
#endif // KERNEL


#endif	/* !_SYS_MCHAIN_H_ */
