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

#include <sys/types.h>
#include <sys/smb_byte_order.h>
#include <sys/smb_apple.h>

#ifdef KERNEL
#include <sys/kpi_mbuf.h>
#include <netsmb/smb_subr.h>
#else // KERNEL
#include <netsmb/upi_mbuf.h>
#include <netsmb/smb_lib.h>
#endif // KERNEL

#include <sys/mchain.h>

/*
 * Various helper functions
 */
size_t m_fixhdr(mbuf_t m0)
{
	mbuf_t m = m0;
	size_t len = 0;

	while (m) {
		len += mbuf_len(m);
		m = mbuf_next(m);
	}
	mbuf_pkthdr_setlen(m0, len);
	return mbuf_pkthdr_len(m0);
}

#ifdef KERNEL

/*
 * There is no KPI call for m_cat. Josh gave me the following
 * code to replace m_cat.
 */
void
mbuf_cat_internal(mbuf_t md_top, mbuf_t m0)
{
	mbuf_t m;
	
	for (m = md_top; mbuf_next(m) != NULL; m = mbuf_next(m))
		;
	mbuf_setnext(m, m0);
}

/*
 * The only way to get min cluster size is to make a 
 * mbuf_stat call. We really only need to do this once
 * since minclsize is a compile time option.
 */
static uint32_t minclsize = 0;

static uint32_t mbuf_minclsize() 
{
	struct mbuf_stat stats;
	
	if (! minclsize) {
		mbuf_stats(&stats);
		minclsize = stats.minclsize;
	}
	return minclsize;
}

/*
 * This will allocate len-worth of mbufs and/or mbuf clusters (whatever fits
 * best) and return a pointer to the top of the allocated chain. If m is
 * non-null, then we assume that it is a single mbuf or an mbuf chain to
 * which we want len bytes worth of mbufs and/or clusters attached, and so
 * if we succeed in allocating it, we will just return a pointer to m.
 *
 * If we happen to fail at any point during the allocation, we will free
 * up everything we have already allocated and return NULL.
 *
 */
static
mbuf_t smb_mbuf_getm(mbuf_t m, size_t len, int how, int type)
{
	size_t mbuf_space;
	mbuf_t top, tail, mp = NULL, mtail = NULL;
	
	KASSERT((ssize_t)len >= 0, ("len is < 0 in smb_mbuf_getm"));
	
	if (mbuf_get(how, type, &mp))
		return (NULL);
	else if (len > mbuf_minclsize()) {
		if ((mbuf_mclget(how, type, &mp)) ||
			((mbuf_flags(mp) & MBUF_EXT) == 0)) {
			mbuf_free(mp);			
			return (NULL);
		}
	}
	mbuf_setlen(mp, 0);
	mbuf_space = mbuf_trailingspace(mp);
	/* len is a size_t so it can't go negative */
	if (mbuf_space > len)
		len = 0;	/* Done */
	else
		len -= mbuf_space;	/* Need another mbuf or more */
	
	if (m != NULL)
		for (mtail = m; mbuf_next(mtail) != NULL; mtail = mbuf_next(mtail));
	else
		m = mp;
	
	top = tail = mp;
	while (len > 0) {
		if (mbuf_get(how, type, &mp))
			goto failed;
		
		mbuf_setnext(tail, mp);
		tail = mp;
		if (len > mbuf_minclsize()) {
			if ((mbuf_mclget(how, type, &mp)) ||
				((mbuf_flags(mp) & MBUF_EXT) == 0))
				goto failed;
		}
		mbuf_setlen(mp, 0);
		mbuf_space = mbuf_trailingspace(mp);
		/* len is a size_t so it can't go negative */
		if (mbuf_space > len)
			len = 0;	/* Done */
		else
			len -= mbuf_space;	/* Need another mbuf or more */
	}
	
	if (mtail != NULL)
		mbuf_setnext(mtail, top);
	return (m);
	
failed:
	mbuf_freem(top);
	return (NULL);
}

#else // KERNEL

/* 
 * We handle this routine differently in userland, than the kernel. See the 
 * above kernel code for more details.
 */
static 
mbuf_t smb_mbuf_getm(mbuf_t mbuf, size_t size, uint32_t how, uint32_t type)
{
	mbuf_t nm = NULL, mtail = NULL;
	
	if (mbuf_getcluster( how, type, size, &nm))
		return NULL;
	if (mbuf != NULL) {
		for (mtail = mbuf; mbuf_next(mtail) != NULL; mtail = mbuf_next(mtail));
	} else {
		mbuf = nm;
	}
	
	if (mtail != NULL)
		mbuf_setnext(mtail, nm);
	return mbuf;
}

int mb_pullup(mbchain_t mbp)
{
	mbuf_t nm;
	size_t size;
	int error;
	
	/* Its all in one mbuf, nothing to do here */
	if (mbuf_next(mbp->mb_top) == NULL) {
		return 0;
	}
	/* We have a chain, assume that we need to reallocate the buffer */
	size = mb_fixhdr(mbp);
	error = mbuf_getcluster(MBUF_WAITOK, MBUF_TYPE_DATA, size, &nm);
	if (error) {
		return error;
	}
	error =  mbuf_copydata(mbp->mb_top, 0, size, mbuf_data(nm));
	if (error) {
		mbuf_freem(nm);
		return error;
	}
	mbuf_pkthdr_setlen(nm, size);
	mbuf_setlen(nm, size);
	mbuf_freem(mbp->mb_top);
	mbp->mb_top = nm;
	mbp->mb_cur = nm;
	return 0;
}

/*
 * Return a buffer of size from the  mbuf chain, the buffer must be contiguous 
 * and fit in one mbuf. If not enough room in this mbuf create an mbuf that
 * has enough room and add it to the chain.
 */
void * mb_getbuffer(mbchain_t mbp, size_t size)
{
	while (mbp->mb_mleft < size) {
		mbuf_t nm;
		
		if (mbuf_getcluster(MBUF_WAITOK, MBUF_TYPE_DATA, mbp->mb_mleft+size, &nm))
			return NULL;
		mbuf_setlen(nm, 0);
		mbuf_setnext(mbp->mb_cur, nm);
		mbp->mb_cur = nm;
		mbp->mb_mleft += mbuf_trailingspace(mbp->mb_cur);
	}
	return (void *)((uint8_t *)mbuf_data(mbp->mb_cur) + mbuf_len(mbp->mb_cur));
}

/*
 * Consume size number of bytes.
 */
void mb_consume(mbchain_t mbp, size_t size)
{
	mbp->mb_mleft -= size;
	mbp->mb_count += size;
	mbp->mb_len += size;
	mbuf_setlen(mbp->mb_cur, mbuf_len(mbp->mb_cur)+size);
}

#endif // KERNEL

/*
 * Routines for putting data into an mbuf chain
 */

static
void mb_initm(mbchain_t mbp, mbuf_t m)
{
	bzero(mbp, sizeof(*mbp));
	mbp->mb_top = mbp->mb_cur = m;
	mbp->mb_mleft = mbuf_trailingspace(m);
}

int
mb_init(mbchain_t mbp)
{
	mbuf_t m = NULL;
	
	/* mbuf_gethdr now intialize all of the fields */
	if (mbuf_gethdr(MBUF_WAITOK, MBUF_TYPE_DATA, &m))
		return ENOBUFS;
	mb_initm(mbp, m);
	return 0;
}

void
mb_done(mbchain_t mbp)
{
	if (mbp->mb_top) {
		mbuf_freem(mbp->mb_top);
		mbp->mb_top = NULL;
	}
}

mbuf_t mb_detach(mbchain_t mbp)
{
	mbuf_t m;

	m = mbp->mb_top;
	mbp->mb_top = NULL;
	return m;
}

size_t mb_fixhdr(mbchain_t mbp)
{
	return m_fixhdr(mbp->mb_top);
}

/*
 * Check if object of size 'size' fit to the current position and
 * allocate new mbuf if not. Advance pointers and increase length of mbuf(s).
 * Return pointer to the object placeholder or NULL if any error occured.
 * Note: size should be <= MLEN if in kernel code
 */
void * mb_reserve(mbchain_t mbp, size_t size)
{
	mbuf_t m, nm;
	caddr_t bpos;

	m = mbp->mb_cur;
	if (mbp->mb_mleft < size) {
		if (mbuf_get(MBUF_WAITOK, MBUF_TYPE_DATA, &nm))
			return NULL;
		/* This check was done up above before KPI code */
		if (size > mbuf_maxlen(nm)) {
#ifdef KERNEL
			SMBERROR("mb_reserve: size = %ld\n", size);
#else // KERNEL
			smb_log_info("%s - mb_reserve: size = %ld, syserr = %s",
						 ASL_LEVEL_ERR, __FUNCTION__, size, strerror(EBADRPC));
#endif // KERNEL
			mbuf_freem(nm);
			return NULL;
		}
		mbuf_setnext(m, nm);
		mbp->mb_cur = nm;
		m = nm;
		mbuf_setlen(m, 0);
		mbp->mb_mleft = mbuf_trailingspace(m);
	}
	mbp->mb_mleft -= size;
	mbp->mb_count += size;
	mbp->mb_len += size;
	bpos = (caddr_t)((uint8_t *)mbuf_data(m) + mbuf_len(m));
	mbuf_setlen(m, mbuf_len(m)+size);
	return bpos;
}

/*
 * Userland starts at word cound not the smb header, lucky for us
 * word cound starts at an even boundry. We will need to relook at
 * this when doing SMB2. Hopefully by then we will be using the whole
 * buffer for both userland and kernel.
 */
int mb_put_padbyte(mbchain_t mbp)
{
	uintptr_t dst;
	char x = 0;
	
	dst = (uintptr_t)((uint8_t *)mbuf_data(mbp->mb_cur) + mbuf_len(mbp->mb_cur));
	/* only add padding if address is odd */
	if (dst & 1) {
		return mb_put_mem(mbp, &x, 1, MB_MSYSTEM);
	} else {
		return 0;
	}
}

int mb_put_uint8(mbchain_t mbp, uint8_t x)
{
	return mb_put_mem(mbp, (caddr_t)&x, sizeof(x), MB_MSYSTEM);
}

int mb_put_uint16be(mbchain_t mbp, uint16_t x)
{
	x = htobes(x);
	return mb_put_mem(mbp, (caddr_t)&x, sizeof(x), MB_MSYSTEM);
}

int mb_put_uint16le(mbchain_t mbp, uint16_t x)
{
	x = htoles(x);
	return mb_put_mem(mbp, (caddr_t)&x, sizeof(x), MB_MSYSTEM);
}

int mb_put_uint32be(mbchain_t mbp, uint32_t x)
{
	x = htobel(x);
	return mb_put_mem(mbp, (caddr_t)&x, sizeof(x), MB_MSYSTEM);
}

int mb_put_uint32le(mbchain_t mbp, uint32_t x)
{
	x = htolel(x);
	return mb_put_mem(mbp, (caddr_t)&x, sizeof(x), MB_MSYSTEM);
}

int mb_put_uint64be(mbchain_t mbp, uint64_t x)
{
	x = htobeq(x);
	return mb_put_mem(mbp, (caddr_t)&x, sizeof(x), MB_MSYSTEM);
}

int mb_put_uint64le(mbchain_t mbp, uint64_t x)
{
	x = htoleq(x);
	return mb_put_mem(mbp, (caddr_t)&x, sizeof(x), MB_MSYSTEM);
}

int mb_put_mem(mbchain_t mbp, const char *source, size_t size, int type)
{
	mbuf_t m;
	caddr_t dst;
	const char * src;
	size_t mleft, count, cplen;

	m = mbp->mb_cur;
	mleft = mbp->mb_mleft;

	while (size > 0) {
		if (mleft == 0) {
			if (mbuf_next(m) == NULL) {
				m = smb_mbuf_getm(m, size, MBUF_WAITOK, MBUF_TYPE_DATA);
				if (m == NULL)
					return ENOBUFS;
			}
			m = mbuf_next(m);
			mleft = mbuf_trailingspace(m);
			continue;
		}
		cplen = mleft > size ? size : mleft;
		dst = (caddr_t)((uint8_t *)mbuf_data(m) + mbuf_len(m));
		switch (type) {
		case MB_MINLINE:
			for (src = source, count = cplen; count; count--)
				*dst++ = *src++;
			break;
		case MB_MSYSTEM:
			bcopy(source, dst, cplen);
			break;
		case MB_MZERO:
			bzero(dst, cplen);
			break;
		}
		size -= cplen;
		source += cplen;
		mbuf_setlen(m, mbuf_len(m)+cplen);
		mleft -= cplen;
		mbp->mb_count += cplen;
        mbp->mb_len += cplen;
	}
	mbp->mb_cur = m;
	mbp->mb_mleft = mleft;
	return 0;
}

int mb_put_mbuf(mbchain_t mbp, mbuf_t m)
{
	mbuf_setnext(mbp->mb_cur, m);
	while (m) {
		mbp->mb_count += mbuf_len(m);
        mbp->mb_len += mbuf_len(m);
		if (mbuf_next(m) == NULL)
			break;
		m = mbuf_next(m);
	}
	mbp->mb_mleft = mbuf_trailingspace(m);
	mbp->mb_cur = m;
	return 0;
}

#ifdef KERNEL
/*
 * copies a uio scatter/gather list to an mbuf chain.
 */
int mb_put_uio(mbchain_t mbp, uio_t uiop, size_t size)
{
	int error;
	size_t mleft, cplen;
	void  *dst;
	mbuf_t m;
	
	m = mbp->mb_cur;	/* Mbuf to start copying the data into */
	mleft = mbp->mb_mleft;	/* How much space is left in this mbuf */
	
	while ((size > 0) && (uio_resid(uiop))) {
		/* Do we need another mbuf, is this one full */
		if (mleft == 0) {
			if (mbuf_next(m) == NULL) {
				m = smb_mbuf_getm(m, size, MBUF_WAITOK, MBUF_TYPE_DATA);
				if (m == NULL)
					return ENOBUFS;
			}
			m = mbuf_next(m);
			mleft = mbuf_trailingspace(m);
			continue;
		}
		/* Get the amount of data to copy and a pointer to the mbuf location */
		cplen = mleft > size ? size : mleft;
		dst = (uint8_t *)mbuf_data(m) + mbuf_len(m);
		/* Copy the data into the mbuf */
		error = uiomove(dst, (int)cplen, uiop);
		if (error)
			return error;
		
		size -= cplen;
		mbuf_setlen(m, mbuf_len(m)+cplen);
		mbp->mb_count += cplen;
        mbp->mb_len += cplen;
		mleft -= cplen;
	}
	mbp->mb_cur = m;
	mbp->mb_mleft = mleft;
	return 0;
}

/*
 * Given a user land pointer place the data in a mbuf chain.
 */
int mb_put_user_mem(mbchain_t mbp, user_addr_t bufp, int size, off_t offset, vfs_context_t context)
{
	user_size_t nbyte = size;
	uio_t auio;
	int error;

	if (vfs_context_is64bit(context))
		auio = uio_create(1, offset, UIO_USERSPACE64, UIO_WRITE);
	else
		auio = uio_create(1, offset, UIO_USERSPACE32, UIO_WRITE);

	if (! auio )
		return ENOMEM;
		
	uio_addiov(auio, bufp, nbyte);
	error = mb_put_uio(mbp, auio, size);
	uio_free(auio);
	return error;
}
#endif // KERNEL

/*
 * Routines for fetching data from an mbuf chain
 */
void md_initm(mdchain_t mdp, mbuf_t m)
{
	bzero(mdp, sizeof(*mdp));
	mdp->md_top = mdp->md_cur = m;
	mdp->md_pos = mbuf_data(m);
    mdp->md_len = 0;
}

int md_init(mdchain_t mdp)
{
	mbuf_t m = NULL;

	/* mbuf_gethdr now intialize all of the fields */
	if (mbuf_gethdr(MBUF_WAITOK, MBUF_TYPE_DATA, &m))
		return ENOBUFS;
	md_initm(mdp, m);
	return 0;
}

#ifndef KERNEL
int md_init_rcvsize(mdchain_t mdp, size_t size)
{
	mbuf_t	m = NULL;
	
	if (size <= (size_t)getpagesize()) {
		if (mbuf_gethdr(MBUF_WAITOK, MBUF_TYPE_DATA, &m))
			return ENOBUFS;		
	} else if ((mbuf_getcluster(MBUF_WAITOK, MBUF_TYPE_DATA, size, &m)) != 0)
		return ENOBUFS;
	md_initm(mdp, m);
	return 0;
}
#endif // KERNEL

void md_shadow_copy(const mdchain_t mdp, mdchain_t shadow)
{
	shadow->md_top = mdp->md_top;		/* head of mbufs chain */
	shadow->md_cur = mdp->md_cur;		/* current mbuf */
	shadow->md_pos = mdp->md_pos;		/* offset in the current mbuf */
}

void md_done(mdchain_t mdp)
{
	if (mdp->md_top) {
		mbuf_freem(mdp->md_top);
		mdp->md_top = NULL;
	}
}

#ifdef KERNEL
/*
 * Append a separate mbuf chain. It is caller responsibility to prevent
 * multiple calls to fetch/record routines.
 */
void md_append_record(mdchain_t mdp, mbuf_t top)
{
	mbuf_t m;

	if (mdp->md_top == NULL) {
		md_initm(mdp, top);
		return;
	}
	m = mdp->md_top;
	while (mbuf_nextpkt(m))
		m = mbuf_nextpkt(m);
	mbuf_setnextpkt(m, top);
	mbuf_setnextpkt(top, NULL);
	return;
}

/*
 * Put next record in place of existing
 */
int md_next_record(mdchain_t mdp)
{
	mbuf_t m;

	if (mdp->md_top == NULL)
		return ENOENT;
	m = mbuf_nextpkt(mdp->md_top);
	md_done(mdp);
	if (m == NULL)
		return ENOENT;
	md_initm(mdp, m);
	return 0;
}
#endif // KERNEL

int md_get_uint8(mdchain_t mdp, uint8_t *x)
{
	return md_get_mem(mdp, (caddr_t)x, 1, MB_MINLINE);
}

int md_get_uint16(mdchain_t mdp, uint16_t *x)
{
	return md_get_mem(mdp, (caddr_t)x, 2, MB_MINLINE);
}

int md_get_uint16le(mdchain_t mdp, uint16_t *x)
{
	uint16_t v;
	int error = md_get_uint16(mdp, &v);

	if (x && (error == 0))
		*x = letohs(v);
	return error;
}

int md_get_uint16be(mdchain_t mdp, uint16_t *x) 
{
	uint16_t v;
	int error = md_get_uint16(mdp, &v);

	if (x && (error == 0))
		*x = betohs(v);
	return error;
}

int md_get_uint32(mdchain_t mdp, uint32_t *x)
{
	return md_get_mem(mdp, (caddr_t)x, 4, MB_MINLINE);
}

int md_get_uint32be(mdchain_t mdp, uint32_t *x)
{
	uint32_t v;
	int error;

	error = md_get_uint32(mdp, &v);
	if (x && (error == 0))
		*x = betohl(v);
	return error;
}

int md_get_uint32le(mdchain_t mdp, uint32_t *x)
{
	uint32_t v;
	int error;

	error = md_get_uint32(mdp, &v);
	if (x && (error == 0))
		*x = letohl(v);
	return error;
}

int md_get_uint64(mdchain_t mdp, uint64_t *x)
{
	return md_get_mem(mdp, (caddr_t)x, 8, MB_MINLINE);
}

int md_get_uint64be(mdchain_t mdp, uint64_t *x)
{
	uint64_t v;
	int error;

	error = md_get_uint64(mdp, &v);
	if (x && (error == 0))
		*x = betohq(v);
	return error;
}

int md_get_uint64le(mdchain_t mdp, uint64_t *x)
{
	uint64_t v;
	int error;

	error = md_get_uint64(mdp, &v);
	if (x && (error == 0))
		*x = letohq(v);
	return error;
}

size_t md_get_size(mdchain_t mdp)
{
	mbuf_t m = mdp->md_cur;
	size_t start_pos = (size_t)mdp->md_pos;
	size_t len = 0;
	
	while (m) {
		if (start_pos) {
			len += (size_t)mbuf_data(m) + mbuf_len(m) - start_pos;
			start_pos = 0; /* only care the first time through */
		} else {
			len += mbuf_len(m);
		}
		m = mbuf_next(m);
	}
	return len;
}

/*
 * This routine relies on the fact that we are looking for the length of a UTF16
 * string that must start on an even boundry.
 */
size_t md_get_utf16_strlen(mdchain_t mdp) 
{
	mbuf_t m = mdp->md_cur;
	u_char *s = mdp->md_pos; /* Points to the start of the utf16 string in the mbuf data */
	size_t size;
	size_t max_count, count, ii;
	uint16_t *ustr;
	
	size = 0;
	while (m) {
		/* Max amount of data we can scan in this mbuf */
		max_count = count = (size_t)mbuf_data(m) + mbuf_len(m) - (size_t)s;
		/* Trail byte in this mbuf ignore it for now */ 
		max_count &= ~1;
		/* Scan the mbuf counting the bytes */
		ustr = (uint16_t *)((void *)s);
		for (ii = 0; ii < max_count; ii += 2) {
			if (*ustr++ == 0) {
				/* Found the end we are done */
				goto done;
			}
			size += 2;
		}
		/* Get the next mbuf to scan */
		m = mbuf_next(m);
		if (m) {
			s = mbuf_data(m);
			/* Did the previous mbuf have an odd length */
			if (count & 1) {
				/* Check the last byte in that mbuf and the first byte in this one */
				if ((*((u_char *)ustr) == 0) && (*s == 0)) {
					/* Found the end we are done */
					goto done;
				}
				s += 1;
			}
		}
	}
done:
	return size;
}

int md_get_mem(mdchain_t mdp, caddr_t target, size_t size, int type)
{
	size_t size_request = size;
	mbuf_t m = mdp->md_cur;
	size_t count;
	u_char *s;
	
	while (size > 0) {
		if (m == NULL) {
			/* Note some calls expect this to happen, see notify change */
#ifdef KERNEL
			SMBWARNING("WARNING: Incomplete copy original size = %ld size = %ld\n", size_request, size);
#else // KERNEL
			smb_log_info("%s - WARNING: Incomplete copy original size = %ld size = %ld, syserr = %s",
						 ASL_LEVEL_DEBUG, __FUNCTION__, size_request, size, strerror(EBADRPC));
#endif // KERNEL
			return EBADRPC;
		}
		s = mdp->md_pos;
		count = (size_t)mbuf_data(m) + mbuf_len(m) - (size_t)s;
		if (count == 0) {
			mdp->md_cur = m = mbuf_next(m);
			if (m)
				mdp->md_pos = mbuf_data(m);
			continue;
		}
		if (count > size)
			count = size;
		size -= count;
		mdp->md_pos += count;
		if (target == NULL)
			continue;
		switch (type) {
		case MB_MSYSTEM:
			bcopy(s, target, count);
			break;
		case MB_MINLINE:
			while (count--)
				*target++ = *s++;
			continue;
		}
		target += count;
	}
    
    mdp->md_len += size_request;
	return 0;
}

#ifdef KERNEL
int md_get_mbuf(mdchain_t mdp, size_t size, mbuf_t *ret)
{
	mbuf_t m = mdp->md_cur, rm;
	size_t offset = (size_t)mdp->md_pos - (size_t)mbuf_data(m);

	if (mbuf_copym(m, offset, size, MBUF_WAITOK, &rm))
		return EBADRPC;
	md_get_mem(mdp, NULL, size, MB_MZERO);
	*ret = rm;
	return 0;
}

int md_get_uio(mdchain_t mdp, uio_t uiop, int32_t size)
{
	size_t size_request = size;
	int32_t count;
	int error;
	mbuf_t m = mdp->md_cur;
	uint8_t *src;
	
	/* Read in the data into the the uio */
	while ((size > 0) && (uio_resid(uiop))) {
		
		if (m == NULL) {
			SMBERROR("UIO incomplete copy\n");
			return EBADRPC;
		}
		/* Get a pointer to the mbuf data */
		src = mdp->md_pos;
		count = (int)((uint8_t *)mbuf_data(m) + mbuf_len(m) - src);
		if (count == 0) {
			mdp->md_cur = m = mbuf_next(m);
			if (m)
				mdp->md_pos = mbuf_data(m);
			continue;
		}
		if (count > size)
			count = size;
		size -= count;
		mdp->md_pos += count;
		error = uiomove((void *)src, count, uiop);
		if (error)
			return error;
	}
    
    mdp->md_len += size_request;
	return 0;
}


/*
 * Given a user land pointer place the data in a mbuf chain.
 */
int md_get_user_mem(mdchain_t mdp, user_addr_t bufp, int size, off_t offset, 
					vfs_context_t context)
{
	user_size_t nbyte = size;
	uio_t auio;
	int error;
	
	if (vfs_context_is64bit(context))
		auio = uio_create(1, offset, UIO_USERSPACE64, UIO_READ);
	else
		auio = uio_create(1, offset, UIO_USERSPACE32, UIO_READ);
	
	if (! auio )
		return ENOMEM;
	
	uio_addiov(auio, bufp, nbyte);
	error = md_get_uio(mdp, auio, size);
	uio_free(auio);
	return error;
}
#endif // KERNEL

