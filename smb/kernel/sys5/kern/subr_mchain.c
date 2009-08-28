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


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/kpi_mbuf.h>
#include <sys/uio.h>

#include <sys/smb_apple.h>
#include <sys/mchain.h>

#include <netsmb/smb_subr.h>

#include <netsmb/smb_compat4.h>

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
	return len;
}

int
mb_init(struct mbchain *mbp)
{
	mbuf_t m;
	
	/* mbuf_gethdr now intialize all of the fields */
	if (mbuf_gethdr(MBUF_WAITOK, MBUF_TYPE_DATA, &m))
		return ENOBUFS;
	mb_initm(mbp, m);
	return 0;
}

void
mb_initm(struct mbchain *mbp, mbuf_t m)
{
	bzero(mbp, sizeof(*mbp));
	mbp->mb_top = mbp->mb_cur = m;
	mbp->mb_mleft = mbuf_trailingspace(m);
}

void
mb_done(struct mbchain *mbp)
{
	if (mbp->mb_top) {
		mbuf_freem(mbp->mb_top);
		mbp->mb_top = NULL;
	}
}

mbuf_t 
mb_detach(struct mbchain *mbp)
{
	mbuf_t m;

	m = mbp->mb_top;
	mbp->mb_top = NULL;
	return m;
}

size_t mb_fixhdr(struct mbchain *mbp)
{
	mbuf_pkthdr_setlen(mbp->mb_top, m_fixhdr(mbp->mb_top));
	
	return mbuf_pkthdr_len(mbp->mb_top);
}

/*
 * Check if object of size 'size' fit to the current position and
 * allocate new mbuf if not. Advance pointers and increase length of mbuf(s).
 * Return pointer to the object placeholder or NULL if any error occured.
 * Note: size should be <= MLEN 
 */
caddr_t mb_reserve(struct mbchain *mbp, size_t size)
{
	mbuf_t m, mn;
	caddr_t bpos;

	m = mbp->mb_cur;
	if (mbp->mb_mleft < size) {
		if (mbuf_get(MBUF_WAITOK, MBUF_TYPE_DATA, &mn))
			return NULL;
		/* This check was done up above before KPI code */
		if (size > mbuf_maxlen(mn))
			panic("mb_reserve: size = %ld\n", size);
		mbuf_setnext(m, mn);
		mbp->mb_cur = mn;
		m = mn;
		mbuf_setlen(m, 0);
		mbp->mb_mleft = mbuf_trailingspace(m);
	}
	mbp->mb_mleft -= size;
	mbp->mb_count += size;
	bpos = (caddr_t)((u_int8_t *)mbuf_data(m) + mbuf_len(m));
	mbuf_setlen(m, mbuf_len(m)+size);
	return bpos;
}

int
mb_put_padbyte(struct mbchain *mbp)
{
	caddr_t dst;
	char x = 0;

	dst = (caddr_t)((u_int8_t *)mbuf_data(mbp->mb_cur) + mbuf_len(mbp->mb_cur));

	/* only add padding if address is odd */
	if ((long)dst & 1)
		return mb_put_mem(mbp, (caddr_t)&x, 1, MB_MSYSTEM);
	else
		return 0;
}

int
mb_put_uint8(struct mbchain *mbp, u_int8_t x)
{
	return mb_put_mem(mbp, (caddr_t)&x, sizeof(x), MB_MSYSTEM);
}

int
mb_put_uint16be(struct mbchain *mbp, u_int16_t x)
{
	x = htobes(x);
	return mb_put_mem(mbp, (caddr_t)&x, sizeof(x), MB_MSYSTEM);
}

int
mb_put_uint16le(struct mbchain *mbp, u_int16_t x)
{
	x = htoles(x);
	return mb_put_mem(mbp, (caddr_t)&x, sizeof(x), MB_MSYSTEM);
}

int
mb_put_uint32be(struct mbchain *mbp, u_int32_t x)
{
	x = htobel(x);
	return mb_put_mem(mbp, (caddr_t)&x, sizeof(x), MB_MSYSTEM);
}

int
mb_put_uint32le(struct mbchain *mbp, u_int32_t x)
{
	x = htolel(x);
	return mb_put_mem(mbp, (caddr_t)&x, sizeof(x), MB_MSYSTEM);
}

int mb_put_uint64be(struct mbchain *mbp, u_int64_t x)
{
	x = htobeq(x);
	return mb_put_mem(mbp, (caddr_t)&x, sizeof(x), MB_MSYSTEM);
}

int
mb_put_uint64le(struct mbchain *mbp, u_int64_t x)
{
	x = htoleq(x);
	return mb_put_mem(mbp, (caddr_t)&x, sizeof(x), MB_MSYSTEM);
}

int mb_put_mem(struct mbchain *mbp, c_caddr_t source, size_t size, int type)
{
	mbuf_t m;
	caddr_t dst;
	c_caddr_t src;
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
		dst = (caddr_t)((u_int8_t *)mbuf_data(m) + mbuf_len(m));
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
	}
	mbp->mb_cur = m;
	mbp->mb_mleft = mleft;
	return 0;
}

int
mb_put_mbuf(struct mbchain *mbp, mbuf_t m)
{
	mbuf_setnext(mbp->mb_cur, m);
	while (m) {
		mbp->mb_count += mbuf_len(m);
		if (mbuf_next(m) == NULL)
			break;
		m = mbuf_next(m);
	}
	mbp->mb_mleft = mbuf_trailingspace(m);
	mbp->mb_cur = m;
	return 0;
}

/*
 * copies a uio scatter/gather list to an mbuf chain.
 */
int mb_put_uio(struct mbchain *mbp, uio_t uiop, size_t size)
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
		dst = (u_int8_t *)mbuf_data(m) + mbuf_len(m);
		/* Copy the data into the mbuf */
		error = uiomove(dst, (int)cplen, uiop);
		if (error)
			return error;
		
		size -= cplen;
		mbuf_setlen(m, mbuf_len(m)+cplen);
		mbp->mb_count += cplen;
		mleft -= cplen;
	}
	mbp->mb_cur = m;
	mbp->mb_mleft = mleft;
	return 0;
}

/*
 * Given a user land pointer place the data in a mbuf chain.
 */
int
mb_put_user_mem(struct mbchain *mbp, user_addr_t bufp, int size, off_t offset, vfs_context_t context)
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

/*
 * Routines for fetching data from an mbuf chain
 */
int
md_init(struct mdchain *mdp)
{
	mbuf_t m;

	/* mbuf_gethdr now intialize all of the fields */
	if (mbuf_gethdr(MBUF_WAITOK, MBUF_TYPE_DATA, &m))
		return ENOBUFS;
	md_initm(mdp, m);
	return 0;
}

void
md_initm(struct mdchain *mdp, mbuf_t m)
{
	bzero(mdp, sizeof(*mdp));
	mdp->md_top = mdp->md_cur = m;
	mdp->md_pos = mbuf_data(m);
}

void
md_done(struct mdchain *mdp)
{
	if (mdp->md_top) {
		mbuf_freem(mdp->md_top);
		mdp->md_top = NULL;
	}
}

/*
 * Append a separate mbuf chain. It is caller responsibility to prevent
 * multiple calls to fetch/record routines.
 */
void
md_append_record(struct mdchain *mdp, mbuf_t top)
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
int
md_next_record(struct mdchain *mdp)
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

int
md_get_uint8(struct mdchain *mdp, u_int8_t *x)
{
	return md_get_mem(mdp, (caddr_t)x, 1, MB_MINLINE);
}

int
md_get_uint16(struct mdchain *mdp, u_int16_t *x)
{
	return md_get_mem(mdp, (caddr_t)x, 2, MB_MINLINE);
}

int
md_get_uint16le(struct mdchain *mdp, u_int16_t *x)
{
	u_int16_t v;
	int error = md_get_uint16(mdp, &v);

	if (x && (error == 0))
		*x = letohs(v);
	return error;
}

int
md_get_uint16be(struct mdchain *mdp, u_int16_t *x) {
	u_int16_t v;
	int error = md_get_uint16(mdp, &v);

	if (x && (error == 0))
		*x = betohs(v);
	return error;
}

int
md_get_uint32(struct mdchain *mdp, u_int32_t *x)
{
	return md_get_mem(mdp, (caddr_t)x, 4, MB_MINLINE);
}

int
md_get_uint32be(struct mdchain *mdp, u_int32_t *x)
{
	u_int32_t v;
	int error;

	error = md_get_uint32(mdp, &v);
	if (x && (error == 0))
		*x = betohl(v);
	return error;
}

int
md_get_uint32le(struct mdchain *mdp, u_int32_t *x)
{
	u_int32_t v;
	int error;

	error = md_get_uint32(mdp, &v);
	if (x && (error == 0))
		*x = letohl(v);
	return error;
}

int
md_get_uint64(struct mdchain *mdp, u_int64_t *x)
{
	return md_get_mem(mdp, (caddr_t)x, 8, MB_MINLINE);
}

int
md_get_uint64be(struct mdchain *mdp, u_int64_t *x)
{
	u_int64_t v;
	int error;

	error = md_get_uint64(mdp, &v);
	if (x && (error == 0))
		*x = betohq(v);
	return error;
}

int
md_get_uint64le(struct mdchain *mdp, u_int64_t *x)
{
	u_int64_t v;
	int error;

	error = md_get_uint64(mdp, &v);
	if (x && (error == 0))
		*x = letohq(v);
	return error;
}

int md_get_mem(struct mdchain *mdp, caddr_t target, size_t size, int type)
{
	size_t size_request = size;
	mbuf_t m = mdp->md_cur;
	size_t count;
	u_char *s;
	
	while (size > 0) {
		if (m == NULL) {
			/* Note some calls expect this to happen, see notify change */
			SMBWARNING("WARNING: Incomplete copy original size = %ld size = %ld\n", size_request, size);
			return EBADRPC;
		}
		s = mdp->md_pos;
		count = (size_t)mbuf_data(m) + mbuf_len(m) - (size_t)s;
		if (count == 0) {
			mdp->md_cur = m = mbuf_next(m);
			if (m)
				s = mdp->md_pos = mbuf_data(m);
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
	return 0;
}

int md_get_mbuf(struct mdchain *mdp, size_t size, mbuf_t *ret)
{
	mbuf_t m = mdp->md_cur, rm;
	size_t offset = (size_t)mdp->md_pos - (size_t)mbuf_data(m);

	if (mbuf_copym(m, offset, size, MBUF_WAITOK, &rm))
		return EBADRPC;
	md_get_mem(mdp, NULL, size, MB_MZERO);
	*ret = rm;
	return 0;
}

int
md_get_uio(struct mdchain *mdp, uio_t uiop, int32_t size)
{
	int32_t count;
	int error;
	mbuf_t m = mdp->md_cur;
	u_int8_t *src;
	
	/* Read in the data into the the uio */
	while ((size > 0) && (uio_resid(uiop))) {
		
		if (m == NULL) {
			SMBERROR("UIO incomplete copy\n");
			return EBADRPC;
		}
		/* Get a pointer to the mbuf data */
		src = mdp->md_pos;
		count = (int)((u_int8_t *)mbuf_data(m) + mbuf_len(m) - src);
		if (count == 0) {
			mdp->md_cur = m = mbuf_next(m);
			if (m)
				src = mdp->md_pos = mbuf_data(m);
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
	return 0;
}


/*
 * Given a user land pointer place the data in a mbuf chain.
 */
int
md_get_user_mem(struct mdchain *mdp, user_addr_t bufp, int size, off_t offset, vfs_context_t context)
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

