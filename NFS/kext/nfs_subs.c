/*
 * Copyright (c) 2000-2020 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */
/* Copyright (c) 1995 NeXT Computer, Inc. All Rights Reserved */
/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Macklem at The University of Guelph.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)nfs_subs.c	8.8 (Berkeley) 5/22/95
 * FreeBSD-Id: nfs_subs.c,v 1.47 1997/11/07 08:53:24 phk Exp $
 */

#include "nfs_client.h"
#include "nfs_kdebug.h"

/*
 * These functions support the macros and help fiddle mbuf chains for
 * the nfs op functions. They do things like create the rpc header and
 * copy data between mbuf chains and uio lists.
 */
#include <sys/systm.h>
#include <sys/kpi_mbuf.h>
#include <sys/un.h>
#include <sys/ubc.h>
#include <sys/fcntl.h>
#include <sys/utfconv.h>

#include <libkern/OSAtomic.h>
#include <kern/kalloc.h>
#include <netinet/in.h>
#include <net/kpi_interface.h>

#include <nfs/nfsnode.h>
#define _NFS_XDR_SUBS_FUNCS_ /* define this to get xdrbuf function definitions */
#include <nfs/xdr_subs.h>
#include <nfs/nfsm_subs.h>
#include <nfs/nfs_gss.h>
#include <nfs/nfsmount.h>

/*
 * NFS globals
 */
struct nfsclntstats __attribute__((aligned(8))) nfsclntstats;
struct nfsrvstats __attribute__((aligned(8))) nfsrvstats;
size_t nfs_mbuf_mhlen = 0, nfs_mbuf_minclsize = 0;

/* NFS debugging support */
uint32_t nfsclnt_debug_ctl;
uint32_t nfsrv_debug_ctl;

#include <libkern/libkern.h>
#include <stdarg.h>

void
nfs_printf(unsigned int debug_control, unsigned int facility, unsigned int level, const char *fmt, ...)
{
	va_list ap;

	if (__NFS_IS_DBG(debug_control, facility, level)) {
		va_start(ap, fmt);
		vprintf(fmt, ap);
		va_end(ap);
	}
}


#define DISPLAYLEN 16

static bool
isprint(int ch)
{
	return ch >= 0x20 && ch <= 0x7e;
}

static void
hexdump(void *data, size_t len)
{
	size_t i, j;
	unsigned char *d = data;
	char *p, disbuf[3 * DISPLAYLEN + 1];

	for (i = 0; i < len; i += DISPLAYLEN) {
		for (p = disbuf, j = 0; (j + i) < len && j < DISPLAYLEN; j++, p += 3) {
			snprintf(p, 4, "%2.2x ", d[i + j]);
		}
		for (; j < DISPLAYLEN; j++, p += 3) {
			snprintf(p, 4, "   ");
		}
		printf("%s    ", disbuf);
		for (p = disbuf, j = 0; (j + i) < len && j < DISPLAYLEN; j++, p++) {
			snprintf(p, 2, "%c", isprint(d[i + j]) ? d[i + j] : '.');
		}
		printf("%s\n", disbuf);
	}
}

void
nfs_dump_mbuf(const char *func, int lineno, const char *msg, mbuf_t mb)
{
	mbuf_t m;

	printf("%s:%d %s\n", func, lineno, msg);
	for (m = mb; m; m = mbuf_next(m)) {
		hexdump(mbuf_data(m), mbuf_len(m));
	}
}

int
nfs_maperr(const char *func, int error)
{
	if (error < NFSERR_BADHANDLE || error > NFSERR_DIRBUFDROPPED) {
		return error;
	}
	switch (error) {
	case NFSERR_BADOWNER:
		printf("%s: No name and/or group mapping err=%d\n", func, error);
		return EPERM;
	case NFSERR_BADNAME:
	case NFSERR_BADCHAR:
		printf("%s: nfs char/name not handled by server err=%d\n", func, error);
		return ENOENT;
	case NFSERR_STALE_CLIENTID:
	case NFSERR_STALE_STATEID:
	case NFSERR_EXPIRED:
	case NFSERR_BAD_STATEID:
		printf("%s: nfs recover err returned %d\n", func, error);
		return EIO;
	case NFSERR_BADHANDLE:
	case NFSERR_SERVERFAULT:
	case NFSERR_BADTYPE:
	case NFSERR_FHEXPIRED:
	case NFSERR_RESOURCE:
	case NFSERR_MOVED:
	case NFSERR_NOFILEHANDLE:
	case NFSERR_MINOR_VERS_MISMATCH:
	case NFSERR_OLD_STATEID:
	case NFSERR_BAD_SEQID:
	case NFSERR_LEASE_MOVED:
	case NFSERR_RECLAIM_BAD:
	case NFSERR_BADXDR:
	case NFSERR_OP_ILLEGAL:
		printf("%s: nfs client/server protocol prob err=%d\n", func, error);
		return EIO;
	default:
		printf("%s: nfs err=%d\n", func, error);
		return EIO;
	}
}

/*
 * functions to convert between NFS and VFS types
 */
nfstype
vtonfs_type(enum vtype vtype, int nfsvers)
{
	switch (vtype) {
	case VNON:
		return NFNON;
	case VREG:
		return NFREG;
	case VDIR:
		return NFDIR;
	case VBLK:
		return NFBLK;
	case VCHR:
		return NFCHR;
	case VLNK:
		return NFLNK;
	case VSOCK:
		if (nfsvers > NFS_VER2) {
			return NFSOCK;
		}
		return NFNON;
	case VFIFO:
		if (nfsvers > NFS_VER2) {
			return NFFIFO;
		}
		return NFNON;
	case VBAD:
	case VSTR:
	case VCPLX:
	default:
		return NFNON;
	}
}

enum vtype
nfstov_type(nfstype nvtype, int nfsvers)
{
	switch (nvtype) {
	case NFNON:
		return VNON;
	case NFREG:
		return VREG;
	case NFDIR:
		return VDIR;
	case NFBLK:
		return VBLK;
	case NFCHR:
		return VCHR;
	case NFLNK:
		return VLNK;
	case NFSOCK:
		if (nfsvers > NFS_VER2) {
			return VSOCK;
		}
		OS_FALLTHROUGH;
	case NFFIFO:
		if (nfsvers > NFS_VER2) {
			return VFIFO;
		}
		OS_FALLTHROUGH;
	case NFATTRDIR:
		if (nfsvers > NFS_VER3) {
			return VDIR;
		}
		OS_FALLTHROUGH;
	case NFNAMEDATTR:
		if (nfsvers > NFS_VER3) {
			return VREG;
		}
		OS_FALLTHROUGH;
	default:
		return VNON;
	}
}

int
vtonfsv2_mode(enum vtype vtype, mode_t m)
{
	switch (vtype) {
	case VNON:
	case VREG:
	case VDIR:
	case VBLK:
	case VCHR:
	case VLNK:
	case VSOCK:
		return MAKEIMODE(vtype, m);
	case VFIFO:
		return MAKEIMODE(VCHR, m);
	case VBAD:
	case VSTR:
	case VCPLX:
	default:
		return MAKEIMODE(VNON, m);
	}
}

/*
 * and the reverse mapping from generic to Version 2 procedure numbers
 */
int nfsv2_procid[NFS_NPROCS] = {
	NFSV2PROC_NULL,
	NFSV2PROC_GETATTR,
	NFSV2PROC_SETATTR,
	NFSV2PROC_LOOKUP,
	NFSV2PROC_NOOP,
	NFSV2PROC_READLINK,
	NFSV2PROC_READ,
	NFSV2PROC_WRITE,
	NFSV2PROC_CREATE,
	NFSV2PROC_MKDIR,
	NFSV2PROC_SYMLINK,
	NFSV2PROC_CREATE,
	NFSV2PROC_REMOVE,
	NFSV2PROC_RMDIR,
	NFSV2PROC_RENAME,
	NFSV2PROC_LINK,
	NFSV2PROC_READDIR,
	NFSV2PROC_NOOP,
	NFSV2PROC_STATFS,
	NFSV2PROC_NOOP,
	NFSV2PROC_NOOP,
	NFSV2PROC_NOOP,
	NFSV2PROC_NOOP
};


/*
 * initialize NFS's cache of mbuf constants
 */
void
nfs_mbuf_init(void)
{
	struct mbuf_stat ms;

	mbuf_stats(&ms);
	nfs_mbuf_mhlen = ms.mhlen;
	nfs_mbuf_minclsize = ms.minclsize;
}

/*
 * nfsm_chain_new_mbuf()
 *
 * Add a new mbuf to the given chain.
 */
int
nfsm_chain_new_mbuf(struct nfsm_chain *nmc, size_t sizehint)
{
	mbuf_t mb;
	int error = 0;

	if (nmc->nmc_flags & NFSM_CHAIN_FLAG_ADD_CLUSTERS) {
		sizehint = nfs_mbuf_minclsize;
	}

	/* allocate a new mbuf */
	nfsm_mbuf_getcluster(error, &mb, sizehint);
	if (error) {
		return error;
	}
	if (mb == NULL) {
		panic("got NULL mbuf?");
	}

	/* do we have a current mbuf? */
	if (nmc->nmc_mcur) {
		/* first cap off current mbuf */
		mbuf_setlen(nmc->nmc_mcur, nmc->nmc_ptr - (caddr_t)mbuf_data(nmc->nmc_mcur));
		/* then append the new mbuf */
		error = mbuf_setnext(nmc->nmc_mcur, mb);
		if (error) {
			mbuf_free(mb);
			return error;
		}
	}

	/* set up for using the new mbuf */
	nmc->nmc_mcur = mb;
	nmc->nmc_ptr = mbuf_data(mb);
	nmc->nmc_left = mbuf_trailingspace(mb);

	return 0;
}

/*
 * nfsm_chain_add_opaque_f()
 *
 * Add "len" bytes of opaque data pointed to by "buf" to the given chain.
 */
int
nfsm_chain_add_opaque_f(struct nfsm_chain *nmc, const u_char *buf, size_t len)
{
	size_t paddedlen, tlen;
	int error;

	paddedlen = nfsm_rndup(len);

	while (paddedlen) {
		if (!nmc->nmc_left) {
			error = nfsm_chain_new_mbuf(nmc, paddedlen);
			if (error) {
				return error;
			}
		}
		tlen = MIN(nmc->nmc_left, paddedlen);
		if (tlen) {
			if (len) {
				if (tlen > len) {
					tlen = len;
				}
				bcopy(buf, nmc->nmc_ptr, tlen);
			} else {
				bzero(nmc->nmc_ptr, tlen);
			}
			nmc->nmc_ptr += tlen;
			nmc->nmc_left -= tlen;
			paddedlen -= tlen;
			if (len) {
				buf += tlen;
				len -= tlen;
			}
		}
	}
	return 0;
}

/*
 * nfsm_chain_add_opaque_nopad_f()
 *
 * Add "len" bytes of opaque data pointed to by "buf" to the given chain.
 * Do not XDR pad.
 */
int
nfsm_chain_add_opaque_nopad_f(struct nfsm_chain *nmc, const u_char *buf, size_t len)
{
	size_t tlen;
	int error;

	while (len > 0) {
		if (nmc->nmc_left <= 0) {
			error = nfsm_chain_new_mbuf(nmc, len);
			if (error) {
				return error;
			}
		}
		tlen = MIN(nmc->nmc_left, len);
		bcopy(buf, nmc->nmc_ptr, tlen);
		nmc->nmc_ptr += tlen;
		nmc->nmc_left -= tlen;
		len -= tlen;
		buf += tlen;
	}
	return 0;
}

/*
 * nfsm_chain_add_uio()
 *
 * Add "len" bytes of data from "uio" to the given chain.
 */
int
nfsm_chain_add_uio(struct nfsm_chain *nmc, uio_t uio, size_t len)
{
	size_t paddedlen, tlen;
	int error;

	paddedlen = nfsm_rndup(len);

	while (paddedlen) {
		if (!nmc->nmc_left) {
			error = nfsm_chain_new_mbuf(nmc, paddedlen);
			if (error) {
				return error;
			}
		}
		tlen = MIN(nmc->nmc_left, paddedlen);
		if (tlen) {
			if (len) {
				tlen = MIN(INT32_MAX, MIN(tlen, len));
				uiomove(nmc->nmc_ptr, (int)tlen, uio);
			} else {
				bzero(nmc->nmc_ptr, tlen);
			}
			nmc->nmc_ptr += tlen;
			nmc->nmc_left -= tlen;
			paddedlen -= tlen;
			if (len) {
				len -= tlen;
			}
		}
	}
	return 0;
}

/*
 * Find the length of the NFS mbuf chain
 * up to the current encoding/decoding offset.
 */
size_t
nfsm_chain_offset(struct nfsm_chain *nmc)
{
	mbuf_t mb;
	size_t len = 0;

	for (mb = nmc->nmc_mhead; mb; mb = mbuf_next(mb)) {
		if (mb == nmc->nmc_mcur) {
			return len + (nmc->nmc_ptr - (caddr_t) mbuf_data(mb));
		}
		len += mbuf_len(mb);
	}

	return len;
}

/*
 * nfsm_chain_advance()
 *
 * Advance an nfsm_chain by "len" bytes.
 */
int
nfsm_chain_advance(struct nfsm_chain *nmc, size_t len)
{
	mbuf_t mb;

	while (len) {
		if (nmc->nmc_left >= len) {
			nmc->nmc_left -= len;
			nmc->nmc_ptr += len;
			return 0;
		}
		len -= nmc->nmc_left;
		nmc->nmc_mcur = mb = mbuf_next(nmc->nmc_mcur);
		if (!mb) {
			return EBADRPC;
		}
		nmc->nmc_ptr = mbuf_data(mb);
		nmc->nmc_left = mbuf_len(mb);
	}

	return 0;
}

/*
 * nfsm_chain_reverse()
 *
 * Reverse decode offset in an nfsm_chain by "len" bytes.
 */
int
nfsm_chain_reverse(struct nfsm_chain *nmc, size_t len)
{
	size_t mlen, new_offset;
	int error = 0;

	mlen = nmc->nmc_ptr - (caddr_t) mbuf_data(nmc->nmc_mcur);
	if (len <= mlen) {
		nmc->nmc_ptr -= len;
		nmc->nmc_left += len;
		return 0;
	}

	new_offset = nfsm_chain_offset(nmc) - len;
	nfsm_chain_dissect_init(error, nmc, nmc->nmc_mhead);
	if (error) {
		return error;
	}

	return nfsm_chain_advance(nmc, new_offset);
}

/*
 * nfsm_chain_get_opaque_pointer_f()
 *
 * Return a pointer to the next "len" bytes of contiguous data in
 * the mbuf chain.  If the next "len" bytes are not contiguous, we
 * try to manipulate the mbuf chain so that it is.
 *
 * The nfsm_chain is advanced by nfsm_rndup("len") bytes.
 */
int
nfsm_chain_get_opaque_pointer_f(struct nfsm_chain *nmc, uint32_t len, u_char **pptr)
{
	mbuf_t mbcur, mb;
	uint32_t padlen;
	size_t mblen, cplen, need, left;
	u_char *ptr;
	int error = 0;

	/* move to next mbuf with data */
	while (nmc->nmc_mcur && (nmc->nmc_left == 0)) {
		mb = mbuf_next(nmc->nmc_mcur);
		nmc->nmc_mcur = mb;
		if (!mb) {
			break;
		}
		nmc->nmc_ptr = mbuf_data(mb);
		nmc->nmc_left = mbuf_len(mb);
	}
	/* check if we've run out of data */
	if (!nmc->nmc_mcur) {
		return EBADRPC;
	}

	/* do we already have a contiguous buffer? */
	if (nmc->nmc_left >= len) {
		/* the returned pointer will be the current pointer */
		*pptr = (u_char*)nmc->nmc_ptr;
		error = nfsm_chain_advance(nmc, nfsm_rndup(len));
		return error;
	}

	padlen = nfsm_rndup(len) - len;

	/* we need (len - left) more bytes */
	mbcur = nmc->nmc_mcur;
	left = nmc->nmc_left;
	need = len - left;

	if (need > mbuf_trailingspace(mbcur)) {
		/*
		 * The needed bytes won't fit in the current mbuf so we'll
		 * allocate a new mbuf to hold the contiguous range of data.
		 */
		nfsm_mbuf_getcluster(error, &mb, len);
		if (error) {
			return error;
		}
		/* double check that this mbuf can hold all the data */
		if (mbuf_maxlen(mb) < len) {
			mbuf_free(mb);
			return EOVERFLOW;
		}

		/* the returned pointer will be the new mbuf's data pointer */
		*pptr = ptr = mbuf_data(mb);

		/* copy "left" bytes to the new mbuf */
		bcopy(nmc->nmc_ptr, ptr, left);
		ptr += left;
		mbuf_setlen(mb, left);

		/* insert the new mbuf between the current and next mbufs */
		error = mbuf_setnext(mb, mbuf_next(mbcur));
		if (!error) {
			error = mbuf_setnext(mbcur, mb);
		}
		if (error) {
			mbuf_free(mb);
			return error;
		}

		/* reduce current mbuf's length by "left" */
		mbuf_setlen(mbcur, mbuf_len(mbcur) - left);

		/*
		 * update nmc's state to point at the end of the mbuf
		 * where the needed data will be copied to.
		 */
		nmc->nmc_mcur = mbcur = mb;
		nmc->nmc_left = 0;
		nmc->nmc_ptr = (caddr_t)ptr;
	} else {
		/* The rest of the data will fit in this mbuf. */

		/* the returned pointer will be the current pointer */
		*pptr = (u_char*)nmc->nmc_ptr;

		/*
		 * update nmc's state to point at the end of the mbuf
		 * where the needed data will be copied to.
		 */
		nmc->nmc_ptr += left;
		nmc->nmc_left = 0;
	}

	/*
	 * move the next "need" bytes into the current
	 * mbuf from the mbufs that follow
	 */

	/* extend current mbuf length */
	mbuf_setlen(mbcur, mbuf_len(mbcur) + need);

	/* mb follows mbufs we're copying/compacting data from */
	mb = mbuf_next(mbcur);

	while (need && mb) {
		/* copy as much as we need/can */
		ptr = mbuf_data(mb);
		mblen = mbuf_len(mb);
		cplen = MIN(mblen, need);
		if (cplen) {
			bcopy(ptr, nmc->nmc_ptr, cplen);
			/*
			 * update the mbuf's pointer and length to reflect that
			 * the data was shifted to an earlier mbuf in the chain
			 */
			error = mbuf_setdata(mb, ptr + cplen, mblen - cplen);
			if (error) {
				mbuf_setlen(mbcur, mbuf_len(mbcur) - need);
				return error;
			}
			/* update pointer/need */
			nmc->nmc_ptr += cplen;
			need -= cplen;
		}
		/* if more needed, go to next mbuf */
		if (need) {
			mb = mbuf_next(mb);
		}
	}

	/* did we run out of data in the mbuf chain? */
	if (need) {
		mbuf_setlen(mbcur, mbuf_len(mbcur) - need);
		return EBADRPC;
	}

	/*
	 * update nmc's state to point after this contiguous data
	 *
	 * "mb" points to the last mbuf we copied data from so we
	 * just set nmc to point at whatever remains in that mbuf.
	 */
	nmc->nmc_mcur = mb;
	nmc->nmc_ptr = mbuf_data(mb);
	nmc->nmc_left = mbuf_len(mb);

	/* move past any padding */
	if (padlen) {
		error = nfsm_chain_advance(nmc, padlen);
	}

	return error;
}

/*
 * nfsm_chain_get_opaque_f()
 *
 * Read the next "len" bytes in the chain into "buf".
 * The nfsm_chain is advanced by nfsm_rndup("len") bytes.
 */
int
nfsm_chain_get_opaque_f(struct nfsm_chain *nmc, size_t len, u_char *buf)
{
	size_t cplen, padlen;
	int error = 0;

	padlen = nfsm_rndup(len) - len;

	/* loop through mbufs copying all the data we need */
	while (len && nmc->nmc_mcur) {
		/* copy as much as we need/can */
		cplen = MIN(nmc->nmc_left, len);
		if (cplen) {
			bcopy(nmc->nmc_ptr, buf, cplen);
			nmc->nmc_ptr += cplen;
			nmc->nmc_left -= cplen;
			buf += cplen;
			len -= cplen;
		}
		/* if more needed, go to next mbuf */
		if (len) {
			mbuf_t mb = mbuf_next(nmc->nmc_mcur);
			nmc->nmc_mcur = mb;
			nmc->nmc_ptr = mb ? mbuf_data(mb) : NULL;
			nmc->nmc_left = mb ? mbuf_len(mb) : 0;
		}
	}

	/* did we run out of data in the mbuf chain? */
	if (len) {
		return EBADRPC;
	}

	if (padlen) {
		nfsm_chain_adv(error, nmc, padlen);
	}

	return error;
}

/*
 * nfsm_chain_get_uio()
 *
 * Read the next "len" bytes in the chain into the given uio.
 * The nfsm_chain is advanced by nfsm_rndup("len") bytes.
 */
int
nfsm_chain_get_uio(struct nfsm_chain *nmc, size_t len, uio_t uio)
{
	size_t cplen, padlen;
	int error = 0;

	padlen = nfsm_rndup(len) - len;

	/* loop through mbufs copying all the data we need */
	while (len && nmc->nmc_mcur) {
		/* copy as much as we need/can */
		cplen = MIN(nmc->nmc_left, len);
		if (cplen) {
			cplen = MIN(cplen, INT32_MAX);
			error = uiomove(nmc->nmc_ptr, (int)cplen, uio);
			if (error) {
				return error;
			}
			nmc->nmc_ptr += cplen;
			nmc->nmc_left -= cplen;
			len -= cplen;
		}
		/* if more needed, go to next mbuf */
		if (len) {
			mbuf_t mb = mbuf_next(nmc->nmc_mcur);
			nmc->nmc_mcur = mb;
			nmc->nmc_ptr = mb ? mbuf_data(mb) : NULL;
			nmc->nmc_left = mb ? mbuf_len(mb) : 0;
		}
	}

	/* did we run out of data in the mbuf chain? */
	if (len) {
		return EBADRPC;
	}

	if (padlen) {
		nfsm_chain_adv(error, nmc, padlen);
	}

	return error;
}

int
nfsm_chain_add_string_nfc(struct nfsm_chain *nmc, const uint8_t *s, size_t slen)
{
	uint8_t smallbuf[64];
	uint8_t *nfcname = smallbuf;
	size_t buflen = sizeof(smallbuf), nfclen;
	int error;

	error = utf8_normalizestr(s, slen, nfcname, &nfclen, buflen, UTF_PRECOMPOSED | UTF_NO_NULL_TERM);
	if (error == ENAMETOOLONG) {
		buflen = MAXPATHLEN;
		nfcname = zalloc(get_zone(NFS_NAMEI));
		error = utf8_normalizestr(s, slen, nfcname, &nfclen, buflen, UTF_PRECOMPOSED | UTF_NO_NULL_TERM);
	}

	/* if we got an error, just use the original string */
	if (error) {
		nfsm_chain_add_string(error, nmc, s, slen);
	} else {
		nfsm_chain_add_string(error, nmc, nfcname, nfclen);
	}

	if (nfcname && (nfcname != smallbuf)) {
		NFS_ZFREE(get_zone(NFS_NAMEI), nfcname);
	}
	return error;
}

/*
 * Add a verifier that can reasonably be expected to be unique.
 */
int
nfsm_chaim_add_exclusive_create_verifier(int error, struct nfsm_chain *nmreq, struct nfsmount *nmp)
{
	uint32_t val;
	uint64_t xid;
	struct sockaddr ss;

	nfs_get_xid(&xid);
	val = (uint32_t)(xid >> 32);

	if (nmp->nm_nso && !sock_getsockname(nmp->nm_nso->nso_so, (struct sockaddr*)&ss, sizeof(ss))) {
		if (nmp->nm_saddr->sa_family == AF_INET) {
			val = ((struct sockaddr_in*)&ss)->sin_addr.s_addr;
		} else if (nmp->nm_saddr->sa_family == AF_INET6) {
			val = ((struct sockaddr_in6*)&ss)->sin6_addr.__u6_addr.__u6_addr32[3];
		}
	}

	nfsm_chain_add_32(error, nmreq, val);
	nfsm_chain_add_32(error, nmreq, (uint32_t)xid);

	return error;
}

/*
 * Add an NFSv2 "sattr" structure to an mbuf chain
 */
int
nfsm_chain_add_v2sattr_f(struct nfsm_chain *nmc, struct vnode_attr *vap, uint32_t szrdev)
{
	int error = 0;

	nfsm_chain_add_32(error, nmc, vtonfsv2_mode(vap->va_type,
	    (VATTR_IS_ACTIVE(vap, va_mode) ? vap->va_mode : 0600)));
	nfsm_chain_add_32(error, nmc,
	    VATTR_IS_ACTIVE(vap, va_uid) ? vap->va_uid : (uint32_t)-1);
	nfsm_chain_add_32(error, nmc,
	    VATTR_IS_ACTIVE(vap, va_gid) ? vap->va_gid : (uint32_t)-1);
	nfsm_chain_add_32(error, nmc, szrdev);
	nfsm_chain_add_v2time(error, nmc,
	    VATTR_IS_ACTIVE(vap, va_access_time) ?
	    &vap->va_access_time : NULL);
	nfsm_chain_add_v2time(error, nmc,
	    VATTR_IS_ACTIVE(vap, va_modify_time) ?
	    &vap->va_modify_time : NULL);

	return error;
}

/*
 * Add an NFSv3 "sattr" structure to an mbuf chain
 */
int
nfsm_chain_add_v3sattr_f(
	__unused struct nfsmount *nmp,
	struct nfsm_chain *nmc,
	struct vnode_attr *vap)
{
	int error = 0;

	if (VATTR_IS_ACTIVE(vap, va_mode)) {
		nfsm_chain_add_32(error, nmc, TRUE);
		nfsm_chain_add_32(error, nmc, vap->va_mode);
	} else {
		nfsm_chain_add_32(error, nmc, FALSE);
	}
	if (VATTR_IS_ACTIVE(vap, va_uid)) {
		nfsm_chain_add_32(error, nmc, TRUE);
		nfsm_chain_add_32(error, nmc, vap->va_uid);
	} else {
		nfsm_chain_add_32(error, nmc, FALSE);
	}
	if (VATTR_IS_ACTIVE(vap, va_gid)) {
		nfsm_chain_add_32(error, nmc, TRUE);
		nfsm_chain_add_32(error, nmc, vap->va_gid);
	} else {
		nfsm_chain_add_32(error, nmc, FALSE);
	}
	if (VATTR_IS_ACTIVE(vap, va_data_size)) {
		nfsm_chain_add_32(error, nmc, TRUE);
		nfsm_chain_add_64(error, nmc, vap->va_data_size);
	} else {
		nfsm_chain_add_32(error, nmc, FALSE);
	}
	if (vap->va_vaflags & VA_UTIMES_NULL) {
		nfsm_chain_add_32(error, nmc, NFS_TIME_SET_TO_SERVER);
		nfsm_chain_add_32(error, nmc, NFS_TIME_SET_TO_SERVER);
	} else {
		if (VATTR_IS_ACTIVE(vap, va_access_time)) {
			nfsm_chain_add_32(error, nmc, NFS_TIME_SET_TO_CLIENT);
			nfsm_chain_add_32(error, nmc, vap->va_access_time.tv_sec);
			nfsm_chain_add_32(error, nmc, vap->va_access_time.tv_nsec);
		} else {
			nfsm_chain_add_32(error, nmc, NFS_TIME_DONT_CHANGE);
		}
		if (VATTR_IS_ACTIVE(vap, va_modify_time)) {
			nfsm_chain_add_32(error, nmc, NFS_TIME_SET_TO_CLIENT);
			nfsm_chain_add_32(error, nmc, vap->va_modify_time.tv_sec);
			nfsm_chain_add_32(error, nmc, vap->va_modify_time.tv_nsec);
		} else {
			nfsm_chain_add_32(error, nmc, NFS_TIME_DONT_CHANGE);
		}
	}

	return error;
}


/*
 * nfsm_chain_get_fh_attr()
 *
 * Get the file handle and attributes from an mbuf chain. (NFSv2/v3)
 */
int
nfsm_chain_get_fh_attr(
	struct nfsmount *nmp,
	struct nfsm_chain *nmc,
	nfsnode_t dnp,
	vfs_context_t ctx,
	int nfsvers,
	uint64_t *xidp,
	fhandle_t *fhp,
	struct nfs_vattr *nvap)
{
	int error = 0, gotfh, gotattr;

	gotfh = gotattr = 1;

	if (nfsvers == NFS_VER3) { /* check for file handle */
		nfsm_chain_get_32(error, nmc, gotfh);
	}
	if (!error && gotfh) { /* get file handle */
		nfsm_chain_get_fh(error, nmc, nfsvers, fhp);
	} else {
		fhp->fh_len = 0;
	}
	if (nfsvers == NFS_VER3) { /* check for file attributes */
		nfsm_chain_get_32(error, nmc, gotattr);
	}
	nfsmout_if(error);
	if (gotattr) {
		if (!gotfh) { /* skip attributes */
			nfsm_chain_adv(error, nmc, NFSX_V3FATTR);
		} else { /* get attributes */
			error = nfs_parsefattr(nmp, nmc, nfsvers, nvap);
		}
	} else if (gotfh) {
		/* we need valid attributes in order to call nfs_nget() */
		if (nfs3_getattr_rpc(NULL, NFSTOMP(dnp), fhp->fh_data, fhp->fh_len, 0, ctx, nvap, xidp)) {
			gotattr = 0;
			fhp->fh_len = 0;
		}
	}
nfsmout:
	return error;
}

/*
 * Get and process NFSv3 WCC data from an mbuf chain
 */
int
nfsm_chain_get_wcc_data_f(
	struct nfsm_chain *nmc,
	nfsnode_t np,
	struct timespec *premtime,
	int *newpostattr,
	u_int64_t *xidp)
{
	int error = 0;
	uint32_t flag = 0;

	nfsm_chain_get_32(error, nmc, flag);
	if (!error && flag) {
		nfsm_chain_adv(error, nmc, 2 * NFSX_UNSIGNED);
		nfsm_chain_get_32(error, nmc, premtime->tv_sec);
		nfsm_chain_get_32(error, nmc, premtime->tv_nsec);
		nfsm_chain_adv(error, nmc, 2 * NFSX_UNSIGNED);
	} else {
		premtime->tv_sec = 0;
		premtime->tv_nsec = 0;
	}
	nfsm_chain_postop_attr_update_flag(error, nmc, np, *newpostattr, xidp);

	return error;
}

/*
 * Get the next RPC transaction ID (XID)
 */
void
nfs_get_xid(uint64_t *xidp)
{
	struct timeval tv;

	lck_mtx_lock(get_lck_mtx(NLM_XID));
	if (!nfs_xid) {
		/*
		 * Derive initial xid from system time.
		 *
		 * Note: it's OK if this code inits nfs_xid to 0 (for example,
		 * due to a broken clock) because we immediately increment it
		 * and we guarantee to never use xid 0.  So, nfs_xid should only
		 * ever be 0 the first time this function is called.
		 */
		microtime(&tv);
		nfs_xid = tv.tv_sec << 12;
	}
	if (++nfs_xid == 0) {
		/* Skip zero xid if it should ever happen. */
		nfs_xidwrap++;
		nfs_xid++;
	}
	*xidp = nfs_xid + (nfs_xidwrap << 32);
	lck_mtx_unlock(get_lck_mtx(NLM_XID));
}

/*
 * Build the RPC header and fill in the authorization info.
 * Returns the head of the mbuf list and the xid.
 */

int
nfsm_rpchead(
	struct nfsreq *req,
	mbuf_t mrest,
	u_int64_t *xidp,
	mbuf_t *mreqp)
{
	struct nfsmount *nmp = req->r_nmp;
	int nfsvers = nmp->nm_vers;
	int proc = ((nfsvers == NFS_VER2) ? nfsv2_procid[req->r_procnum] : (int)req->r_procnum);

	return nfsm_rpchead2(nmp, nmp->nm_sotype, NFS_PROG, nfsvers, proc,
	           req->r_auth, req->r_cred, req, mrest, xidp, mreqp);
}

/*
 * get_auiliary_groups:	Gets the supplementary groups from a credential.
 *
 * IN:		cred:	credential to get the associated groups from.
 * OUT:		groups:	An array of gids of NGROUPS size.
 * IN:		count:	The number of groups to get; i.e.; the number of groups the server supports
 *
 * returns:	The number of groups found.
 *
 * Just a wrapper around kauth_cred_getgroups to handle the case of a server supporting less
 * than NGROUPS.
 */
static size_t
get_auxiliary_groups(kauth_cred_t cred, gid_t groups[NGROUPS], size_t count)
{
	gid_t pgid;
	size_t maxcount = count < NGROUPS ? count + 1 : NGROUPS;
	size_t i;

	for (i = 0; i < NGROUPS; i++) {
		groups[i] = -2; /* Initialize to the nobody group */
	}
	(void)kauth_cred_getgroups(cred, groups, &maxcount);
	if (maxcount < 1) {
		return maxcount;
	}

	/*
	 * kauth_get_groups returns the primary group followed by the
	 * users auxiliary groups. If the number of groups the server supports
	 * is less than NGROUPS, then we will drop the first group so that
	 * we can send one more group over the wire.
	 */


	if (count < NGROUPS) {
		pgid = kauth_cred_getgid(cred);
		if (pgid == groups[0]) {
			maxcount -= 1;
			for (i = 0; i < maxcount; i++) {
				groups[i] = groups[i + 1];
			}
		}
	}

	return maxcount;
}

int
nfsm_rpchead2(__unused struct nfsmount *nmp, int sotype, int prog, int vers, int proc, int auth_type,
    kauth_cred_t cred, __unused struct nfsreq *req, mbuf_t mrest, u_int64_t *xidp, mbuf_t *mreqp)
{
	mbuf_t mreq, mb;
	size_t i;
	int error, auth_len = 0, authsiz, reqlen;
	size_t headlen;
	struct nfsm_chain nmreq;
	gid_t grouplist[NGROUPS];
	size_t groupcount = 0;

	/* calculate expected auth length */
	switch (auth_type) {
	case RPCAUTH_NONE:
		auth_len = 0;
		break;
	case RPCAUTH_SYS:
	{
		size_t count = nmp->nm_numgrps < NGROUPS ? nmp->nm_numgrps : NGROUPS;

		if (!cred) {
			return EINVAL;
		}
		groupcount = get_auxiliary_groups(cred, grouplist, count);
		auth_len = ((uint32_t)groupcount + 5) * NFSX_UNSIGNED;
		break;
	}
#if CONFIG_NFS_GSS
	case RPCAUTH_KRB5:
	case RPCAUTH_KRB5I:
	case RPCAUTH_KRB5P:
		if (!req || !cred) {
			return EINVAL;
		}
		auth_len = 5 * NFSX_UNSIGNED + 0;         // zero context handle for now
		break;
#endif /* CONFIG_NFS_GSS */
	default:
		return EINVAL;
	}
	authsiz = nfsm_rndup(auth_len);

	/* allocate the packet */
	headlen = authsiz + 10 * NFSX_UNSIGNED;
	if (sotype == SOCK_STREAM) { /* also include room for any RPC Record Mark */
		headlen += NFSX_UNSIGNED;
	}
	if (headlen >= nfs_mbuf_minclsize) {
		error = mbuf_getpacket(MBUF_WAITOK, &mreq);
	} else {
		error = mbuf_gethdr(MBUF_WAITOK, MBUF_TYPE_DATA, &mreq);
		if (!error) {
			if (headlen < nfs_mbuf_mhlen) {
				mbuf_align_32(mreq, headlen);
			} else {
				mbuf_align_32(mreq, 8 * NFSX_UNSIGNED);
			}
		}
	}
	if (error) {
		/* unable to allocate packet */
		/* XXX should we keep statistics for these errors? */
		return error;
	}

	/*
	 * If the caller gave us a non-zero XID then use it because
	 * it may be a higher-level resend with a GSSAPI credential.
	 * Otherwise, allocate a new one.
	 */
	if (*xidp == 0) {
		nfs_get_xid(xidp);
	}

	/* build the header(s) */
	nfsm_chain_init(&nmreq, mreq);

	/* First, if it's a TCP stream insert space for an RPC record mark */
	if (sotype == SOCK_STREAM) {
		nfsm_chain_add_32(error, &nmreq, 0);
	}

	/* Then the RPC header. */
	nfsm_chain_add_32(error, &nmreq, (*xidp & 0xffffffff));
	nfsm_chain_add_32(error, &nmreq, RPC_CALL);
	nfsm_chain_add_32(error, &nmreq, RPC_VER2);
	nfsm_chain_add_32(error, &nmreq, prog);
	nfsm_chain_add_32(error, &nmreq, vers);
	nfsm_chain_add_32(error, &nmreq, proc);

#if CONFIG_NFS_GSS
add_cred:
#endif
	switch (auth_type) {
	case RPCAUTH_NONE:
		nfsm_chain_add_32(error, &nmreq, RPCAUTH_NONE); /* auth */
		nfsm_chain_add_32(error, &nmreq, 0);            /* length */
		nfsm_chain_add_32(error, &nmreq, RPCAUTH_NONE); /* verf */
		nfsm_chain_add_32(error, &nmreq, 0);            /* length */
		nfsm_chain_build_done(error, &nmreq);
		/* Append the args mbufs */
		if (!error) {
			error = mbuf_setnext(nmreq.nmc_mcur, mrest);
		}
		break;
	case RPCAUTH_SYS: {
		nfsm_chain_add_32(error, &nmreq, RPCAUTH_SYS);
		nfsm_chain_add_32(error, &nmreq, authsiz);
		{
			nfsm_chain_add_32(error, &nmreq, 0);    /* stamp */
		}
		nfsm_chain_add_32(error, &nmreq, 0);    /* zero-length hostname */
		nfsm_chain_add_32(error, &nmreq, kauth_cred_getuid(cred));      /* UID */
		nfsm_chain_add_32(error, &nmreq, kauth_cred_getgid(cred));      /* GID */
		nfsm_chain_add_32(error, &nmreq, groupcount);/* additional GIDs */
		for (i = 0; i < groupcount; i++) {
			nfsm_chain_add_32(error, &nmreq, grouplist[i]);
		}

		/* And the verifier... */
		nfsm_chain_add_32(error, &nmreq, RPCAUTH_NONE); /* flavor */
		nfsm_chain_add_32(error, &nmreq, 0);            /* length */
		nfsm_chain_build_done(error, &nmreq);

		/* Append the args mbufs */
		if (!error) {
			error = mbuf_setnext(nmreq.nmc_mcur, mrest);
		}
		break;
	}
#if CONFIG_NFS_GSS
	case RPCAUTH_KRB5:
	case RPCAUTH_KRB5I:
	case RPCAUTH_KRB5P:
		error = nfs_gss_clnt_cred_put(req, &nmreq, mrest);
		if (error == ENEEDAUTH) {
			size_t count = nmp->nm_numgrps < NGROUPS ? nmp->nm_numgrps : NGROUPS;

			/*
			 * Use sec=sys for this user
			 */
			error = 0;
			req->r_auth = auth_type = RPCAUTH_SYS;
			groupcount = get_auxiliary_groups(cred, grouplist, count);
			auth_len = ((uint32_t)groupcount + 5) * NFSX_UNSIGNED;
			authsiz = nfsm_rndup(auth_len);
			goto add_cred;
		}
		break;
#endif /* CONFIG_NFS_GSS */
	}
	;

	/* finish setting up the packet */
	if (!error) {
		error = mbuf_pkthdr_setrcvif(mreq, 0);
	}

	if (error) {
		mbuf_freem(mreq);
		return error;
	}

	/* Calculate the size of the request */
	reqlen = 0;
	for (mb = nmreq.nmc_mhead; mb; mb = mbuf_next(mb)) {
		reqlen += mbuf_len(mb);
	}

	mbuf_pkthdr_setlen(mreq, reqlen);

	/*
	 * If the request goes on a TCP stream,
	 * set its size in the RPC record mark.
	 * The record mark count doesn't include itself
	 * and the last fragment bit is set.
	 */
	if (sotype == SOCK_STREAM) {
		nfsm_chain_set_recmark(error, &nmreq,
		    (reqlen - NFSX_UNSIGNED) | 0x80000000);
	}

	*mreqp = mreq;
	return 0;
}

/*
 * Parse an NFS file attribute structure out of an mbuf chain.
 */
int
nfs_parsefattr(
	__unused struct nfsmount *nmp,
	struct nfsm_chain *nmc,
	int nfsvers,
	struct nfs_vattr *nvap)
{
	int error = 0;
	enum vtype vtype;
	nfstype nvtype;
	uint32_t vmode, val, val2;
	dev_t rdev;

	val = val2 = 0;
	NVATTR_INIT(nvap);

	NFS_BITMAP_SET(nvap->nva_bitmap, NFS_FATTR_TYPE);
	NFS_BITMAP_SET(nvap->nva_bitmap, NFS_FATTR_MODE);
	NFS_BITMAP_SET(nvap->nva_bitmap, NFS_FATTR_NUMLINKS);
	NFS_BITMAP_SET(nvap->nva_bitmap, NFS_FATTR_OWNER);
	NFS_BITMAP_SET(nvap->nva_bitmap, NFS_FATTR_OWNER_GROUP);
	NFS_BITMAP_SET(nvap->nva_bitmap, NFS_FATTR_SIZE);
	NFS_BITMAP_SET(nvap->nva_bitmap, NFS_FATTR_SPACE_USED);
	NFS_BITMAP_SET(nvap->nva_bitmap, NFS_FATTR_RAWDEV);
	NFS_BITMAP_SET(nvap->nva_bitmap, NFS_FATTR_FSID);
	NFS_BITMAP_SET(nvap->nva_bitmap, NFS_FATTR_FILEID);
	NFS_BITMAP_SET(nvap->nva_bitmap, NFS_FATTR_TIME_ACCESS);
	NFS_BITMAP_SET(nvap->nva_bitmap, NFS_FATTR_TIME_MODIFY);
	NFS_BITMAP_SET(nvap->nva_bitmap, NFS_FATTR_TIME_METADATA);

	nfsm_chain_get_32(error, nmc, nvtype);
	nfsm_chain_get_32(error, nmc, vmode);
	nfsmout_if(error);

	if (nfsvers == NFS_VER3) {
		nvap->nva_type = vtype = nfstov_type(nvtype, nfsvers);
	} else {
		/*
		 * The duplicate information returned in fa_type and fa_mode
		 * is an ambiguity in the NFS version 2 protocol.
		 *
		 * VREG should be taken literally as a regular file.  If a
		 * server intends to return some type information differently
		 * in the upper bits of the mode field (e.g. for sockets, or
		 * FIFOs), NFSv2 mandates fa_type to be VNON.  Anyway, we
		 * leave the examination of the mode bits even in the VREG
		 * case to avoid breakage for bogus servers, but we make sure
		 * that there are actually type bits set in the upper part of
		 * fa_mode (and failing that, trust the va_type field).
		 *
		 * NFSv3 cleared the issue, and requires fa_mode to not
		 * contain any type information (while also introducing
		 * sockets and FIFOs for fa_type).
		 */
		vtype = nfstov_type(nvtype, nfsvers);
		if ((vtype == VNON) || ((vtype == VREG) && ((vmode & S_IFMT) != 0))) {
			vtype = IFTOVT(vmode);
		}
		nvap->nva_type = vtype;
	}

	nvap->nva_mode = (vmode & 07777);

	nfsm_chain_get_32(error, nmc, nvap->nva_nlink);
	nfsm_chain_get_32(error, nmc, nvap->nva_uid);
	nfsm_chain_get_32(error, nmc, nvap->nva_gid);

	if (nfsvers == NFS_VER3) {
		nfsm_chain_get_64(error, nmc, nvap->nva_size);
		nfsm_chain_get_64(error, nmc, nvap->nva_bytes);
		nfsm_chain_get_32(error, nmc, nvap->nva_rawdev.specdata1);
		nfsm_chain_get_32(error, nmc, nvap->nva_rawdev.specdata2);
		nfsmout_if(error);
		nfsm_chain_get_64(error, nmc, nvap->nva_fsid.major);
		nvap->nva_fsid.minor = 0;
		nfsm_chain_get_64(error, nmc, nvap->nva_fileid);
	} else {
		nfsm_chain_get_32(error, nmc, nvap->nva_size);
		nfsm_chain_adv(error, nmc, NFSX_UNSIGNED);
		nfsm_chain_get_32(error, nmc, rdev);
		nfsmout_if(error);
		nvap->nva_rawdev.specdata1 = major(rdev);
		nvap->nva_rawdev.specdata2 = minor(rdev);
		nfsm_chain_get_32(error, nmc, val); /* blocks */
		nfsmout_if(error);
		nvap->nva_bytes = val * NFS_FABLKSIZE;
		nfsm_chain_get_32(error, nmc, val);
		nfsmout_if(error);
		nvap->nva_fsid.major = (uint64_t)val;
		nvap->nva_fsid.minor = 0;
		nfsm_chain_get_32(error, nmc, val);
		nfsmout_if(error);
		nvap->nva_fileid = (uint64_t)val;
		/* Really ugly NFSv2 kludge. */
		if ((vtype == VCHR) && (rdev == (dev_t)0xffffffff)) {
			nvap->nva_type = VFIFO;
		}
	}
	nfsm_chain_get_time(error, nmc, nfsvers,
	    nvap->nva_timesec[NFSTIME_ACCESS],
	    nvap->nva_timensec[NFSTIME_ACCESS]);
	nfsm_chain_get_time(error, nmc, nfsvers,
	    nvap->nva_timesec[NFSTIME_MODIFY],
	    nvap->nva_timensec[NFSTIME_MODIFY]);
	nfsm_chain_get_time(error, nmc, nfsvers,
	    nvap->nva_timesec[NFSTIME_CHANGE],
	    nvap->nva_timensec[NFSTIME_CHANGE]);

nfsmout:
	return error;
}

/*
 * Load the attribute cache (that lives in the nfsnode entry) with
 * the value pointed to by nvap, unless the file type in the attribute
 * cache doesn't match the file type in the nvap, in which case log a
 * warning and return ESTALE.
 *
 * If the dontshrink flag is set, then it's not safe to call ubc_setsize()
 * to shrink the size of the file.
 */
int
nfs_loadattrcache(
	nfsnode_t np,
	struct nfs_vattr *nvap,
	u_int64_t *xidp,
	int dontshrink)
{
	mount_t mp;
	vnode_t vp;
	struct timeval now;
	struct nfs_vattr *npnvap;
	int xattr = np->n_vattr.nva_flags & NFS_FFLAG_IS_ATTR;
	int referral = np->n_vattr.nva_flags & NFS_FFLAG_TRIGGER_REFERRAL;
	int aclbit, monitored, error = 0;
	kauth_acl_t acl;
	struct nfsmount *nmp;
	uint32_t events = np->n_events;

	if (np->n_hflag & NHINIT) {
		vp = NULL;
		mp = np->n_mount;
	} else {
		vp = NFSTOV(np);
		mp = vnode_mount(vp);
	}
	monitored = vp ? vnode_ismonitored(vp) : 0;

	NFS_KDBG_ENTRY(NFSDBG_OP_LOADATTRCACHE, np, vp, *xidp >> 32, *xidp);

	if (!((nmp = VFSTONFS(mp)))) {
		NFS_KDBG_INFO(NFSDBG_OP_LOADATTRCACHE, 0xabc001, np, *xidp, ENXIO);
		error = ENXIO;
		goto out_return;
	}

	if (*xidp < np->n_xid) {
		/*
		 * We have already updated attributes with a response from
		 * a later request.  The attributes we have here are probably
		 * stale so we drop them (just return).  However, our
		 * out-of-order receipt could be correct - if the requests were
		 * processed out of order at the server.  Given the uncertainty
		 * we invalidate our cached attributes.  *xidp is zeroed here
		 * to indicate the attributes were dropped - only getattr
		 * cares - it needs to retry the rpc.
		 */
		NATTRINVALIDATE(np);
		NFS_KDBG_INFO(NFSDBG_OP_LOADATTRCACHE, 0xabc002, np, np->n_xid, *xidp);
		*xidp = 0;
		error = 0;
		goto out_return;
	}

	if (vp && (nvap->nva_type != vnode_vtype(vp))) {
		/*
		 * The filehandle has changed type on us.  This can be
		 * caused by either the server not having unique filehandles
		 * or because another client has removed the previous
		 * filehandle and a new object (of a different type)
		 * has been created with the same filehandle.
		 *
		 * We can't simply switch the type on the vnode because
		 * there may be type-specific fields that need to be
		 * cleaned up or set up.
		 *
		 * So, what should we do with this vnode?
		 *
		 * About the best we can do is log a warning and return
		 * an error.  ESTALE is about the closest error, but it
		 * is a little strange that we come up with this error
		 * internally instead of simply passing it through from
		 * the server.  Hopefully, the vnode will be reclaimed
		 * soon so the filehandle can be reincarnated as the new
		 * object type.
		 */
		printf("nfs loadattrcache vnode changed type, was %d now %d\n",
		    vnode_vtype(vp), nvap->nva_type);
		error = ESTALE;
		if (monitored) {
			events |= VNODE_EVENT_DELETE;
		}
		goto out;
	}

	npnvap = &np->n_vattr;

	/*
	 * The ACL cache needs special handling because it is not
	 * always updated.  Save current ACL cache state so it can
	 * be restored after copying the new attributes into place.
	 */
	aclbit = NFS_BITMAP_ISSET(npnvap->nva_bitmap, NFS_FATTR_ACL);
	acl = npnvap->nva_acl;

	if (monitored) {
		/*
		 * For monitored nodes, check for attribute changes that should generate events.
		 */
		if (NFS_BITMAP_ISSET(nvap->nva_bitmap, NFS_FATTR_NUMLINKS) &&
		    (nvap->nva_nlink != npnvap->nva_nlink)) {
			events |= VNODE_EVENT_ATTRIB | VNODE_EVENT_LINK;
		}
		if (events & VNODE_EVENT_PERMS) {
			/* no need to do all the checking if it's already set */;
		} else if (NFS_BITMAP_ISSET(nvap->nva_bitmap, NFS_FATTR_MODE) &&
		    (nvap->nva_mode != npnvap->nva_mode)) {
			events |= VNODE_EVENT_ATTRIB | VNODE_EVENT_PERMS;
		} else if (NFS_BITMAP_ISSET(nvap->nva_bitmap, NFS_FATTR_OWNER) &&
		    (nvap->nva_uid != npnvap->nva_uid)) {
			events |= VNODE_EVENT_ATTRIB | VNODE_EVENT_PERMS;
		} else if (NFS_BITMAP_ISSET(nvap->nva_bitmap, NFS_FATTR_OWNER_GROUP) &&
		    (nvap->nva_gid != npnvap->nva_gid)) {
			events |= VNODE_EVENT_ATTRIB | VNODE_EVENT_PERMS;
#if CONFIG_NFS4
		} else if (nmp->nm_vers >= NFS_VER4) {
			if (NFS_BITMAP_ISSET(nvap->nva_bitmap, NFS_FATTR_OWNER) &&
			    !kauth_guid_equal(&nvap->nva_uuuid, &npnvap->nva_uuuid)) {
				events |= VNODE_EVENT_ATTRIB | VNODE_EVENT_PERMS;
			} else if (NFS_BITMAP_ISSET(nvap->nva_bitmap, NFS_FATTR_OWNER_GROUP) &&
			    !kauth_guid_equal(&nvap->nva_guuid, &npnvap->nva_guuid)) {
				events |= VNODE_EVENT_ATTRIB | VNODE_EVENT_PERMS;
			} else if ((NFS_BITMAP_ISSET(nvap->nva_bitmap, NFS_FATTR_ACL) &&
			    nvap->nva_acl && npnvap->nva_acl &&
			    ((nvap->nva_acl->acl_entrycount != npnvap->nva_acl->acl_entrycount) ||
			    bcmp(nvap->nva_acl, npnvap->nva_acl, KAUTH_ACL_COPYSIZE(nvap->nva_acl))))) {
				events |= VNODE_EVENT_ATTRIB | VNODE_EVENT_PERMS;
			}
#endif
		}
		if (/* Oh, C... */
#if CONFIG_NFS4
			((nmp->nm_vers >= NFS_VER4) && NFS_BITMAP_ISSET(nvap->nva_bitmap, NFS_FATTR_CHANGE) && (nvap->nva_change != npnvap->nva_change)) ||
#endif
			(NFS_BITMAP_ISSET(nvap->nva_bitmap, NFS_FATTR_TIME_MODIFY) &&
			((nvap->nva_timesec[NFSTIME_MODIFY] != npnvap->nva_timesec[NFSTIME_MODIFY]) ||
			(nvap->nva_timensec[NFSTIME_MODIFY] != npnvap->nva_timensec[NFSTIME_MODIFY])))) {
			events |= VNODE_EVENT_ATTRIB | VNODE_EVENT_WRITE;
		}
		if (!events && NFS_BITMAP_ISSET(nvap->nva_bitmap, NFS_FATTR_RAWDEV) &&
		    ((nvap->nva_rawdev.specdata1 != npnvap->nva_rawdev.specdata1) ||
		    (nvap->nva_rawdev.specdata2 != npnvap->nva_rawdev.specdata2))) {
			events |= VNODE_EVENT_ATTRIB;
		}
		if (!events && NFS_BITMAP_ISSET(nvap->nva_bitmap, NFS_FATTR_FILEID) &&
		    (nvap->nva_fileid != npnvap->nva_fileid)) {
			events |= VNODE_EVENT_ATTRIB;
		}
		if (!events && NFS_BITMAP_ISSET(nvap->nva_bitmap, NFS_FATTR_ARCHIVE) &&
		    ((nvap->nva_flags & NFS_FFLAG_ARCHIVED) != (npnvap->nva_flags & NFS_FFLAG_ARCHIVED))) {
			events |= VNODE_EVENT_ATTRIB;
		}
		if (!events && NFS_BITMAP_ISSET(nvap->nva_bitmap, NFS_FATTR_HIDDEN) &&
		    ((nvap->nva_flags & NFS_FFLAG_HIDDEN) != (npnvap->nva_flags & NFS_FFLAG_HIDDEN))) {
			events |= VNODE_EVENT_ATTRIB;
		}
		if (!events && NFS_BITMAP_ISSET(nvap->nva_bitmap, NFS_FATTR_TIME_CREATE) &&
		    ((nvap->nva_timesec[NFSTIME_CREATE] != npnvap->nva_timesec[NFSTIME_CREATE]) ||
		    (nvap->nva_timensec[NFSTIME_CREATE] != npnvap->nva_timensec[NFSTIME_CREATE]))) {
			events |= VNODE_EVENT_ATTRIB;
		}
		if (!events && NFS_BITMAP_ISSET(nvap->nva_bitmap, NFS_FATTR_TIME_BACKUP) &&
		    ((nvap->nva_timesec[NFSTIME_BACKUP] != npnvap->nva_timesec[NFSTIME_BACKUP]) ||
		    (nvap->nva_timensec[NFSTIME_BACKUP] != npnvap->nva_timensec[NFSTIME_BACKUP]))) {
			events |= VNODE_EVENT_ATTRIB;
		}
	}

#if CONFIG_NFS4
	/* Copy the attributes to the attribute cache */
	if (nmp->nm_vers >= NFS_VER4 && npnvap->nva_flags & NFS_FFLAG_PARTIAL_WRITE) {
		/*
		 * NFSv4 WRITE RPCs contain partial GETATTR requests - only type, change, size, metadatatime and modifytime are requested.
		 * In such cases,  we do not update the time stamp - but the requested attributes.
		 */
		NFS_BITMAP_COPY_ATTR(nvap, npnvap, TYPE, type);
		NFS_BITMAP_COPY_ATTR(nvap, npnvap, CHANGE, change);
		NFS_BITMAP_COPY_ATTR(nvap, npnvap, SIZE, size);
		NFS_BITMAP_COPY_TIME(nvap, npnvap, METADATA, CHANGE);
		NFS_BITMAP_COPY_TIME(nvap, npnvap, MODIFY, MODIFY);
	} else
#endif /* CONFIG_NFS4 */
	{
		bcopy((caddr_t)nvap, (caddr_t)npnvap, sizeof(*nvap));
		microuptime(&now);
		np->n_attrstamp = now.tv_sec;
	}

	np->n_xid = *xidp;
	/* NFS_FFLAG_IS_ATTR and NFS_FFLAG_TRIGGER_REFERRAL need to be sticky... */
	if (vp && xattr) {
		nvap->nva_flags |= xattr;
	}
	if (vp && referral) {
		nvap->nva_flags |= referral;
	}

	if (NFS_BITMAP_ISSET(nvap->nva_bitmap, NFS_FATTR_ACL)) {
		/* we're updating the ACL */
		if (nvap->nva_acl) {
			/* make a copy of the acl for the cache */
			npnvap->nva_acl = kauth_acl_alloc(nvap->nva_acl->acl_entrycount);
			if (npnvap->nva_acl) {
				bcopy(nvap->nva_acl, npnvap->nva_acl, KAUTH_ACL_COPYSIZE(nvap->nva_acl));
			} else {
				/* can't make a copy to cache, invalidate ACL cache */
				NFS_BITMAP_CLR(npnvap->nva_bitmap, NFS_FATTR_ACL);
				NACLINVALIDATE(np);
				aclbit = 0;
			}
		}
		if (acl) {
			kauth_acl_free(acl);
			acl = NULL;
		}
	}
	if (NFS_BITMAP_ISSET(nvap->nva_bitmap, NFS_FATTR_ACL)) {
		/* update the ACL timestamp */
		microuptime(&now);
		np->n_aclstamp = now.tv_sec;
	} else {
		/* we aren't updating the ACL, so restore original values */
		if (aclbit) {
			NFS_BITMAP_SET(npnvap->nva_bitmap, NFS_FATTR_ACL);
		}
		npnvap->nva_acl = acl;
	}

#if CONFIG_TRIGGERS
#if CONFIG_NFS4
	/*
	 * For NFSv4, if the fsid doesn't match the fsid for the mount, then
	 * this node is for a different file system on the server.  So we mark
	 * this node as a trigger node that will trigger the mirror mount.
	 */
	if ((nmp->nm_vers >= NFS_VER4) && (nvap->nva_type == VDIR) &&
	    ((np->n_vattr.nva_fsid.major != nmp->nm_fsid.major) ||
	    (np->n_vattr.nva_fsid.minor != nmp->nm_fsid.minor))) {
		np->n_vattr.nva_flags |= NFS_FFLAG_TRIGGER;
	}
#endif /* CONFIG_NFS4 */
#endif /* CONFIG_TRIGGERS */

	if (!vp || (nvap->nva_type != VREG)) {
		np->n_size = nvap->nva_size;
	} else if (nvap->nva_size != np->n_size) {
		NFS_KDBG_INFO(NFSDBG_OP_LOADATTRCACHE, 0xabc003, np, nvap->nva_size, np->n_size);
		if (!UBCINFOEXISTS(vp) || (dontshrink && (nvap->nva_size < np->n_size))) {
			/* asked not to shrink, so stick with current size */
			NFS_KDBG_INFO(NFSDBG_OP_LOADATTRCACHE, 0xabc004, np, np->n_size, np->n_vattr.nva_size);
			nvap->nva_size = np->n_size;
			NATTRINVALIDATE(np);
		} else if ((np->n_flag & NMODIFIED) && (nvap->nva_size < np->n_size)) {
			/* if we've modified, stick with larger size */
			NFS_KDBG_INFO(NFSDBG_OP_LOADATTRCACHE, 0xabc005, np, np->n_size, np->n_vattr.nva_size);
			nvap->nva_size = np->n_size;
			npnvap->nva_size = np->n_size;
		} else {
			/*
			 * n_size is protected by the data lock, so we need to
			 * defer updating it until it's safe.  We save the new size
			 * and set a flag and it'll get updated the next time we get/drop
			 * the data lock or the next time we do a getattr.
			 */
			np->n_newsize = nvap->nva_size;
			SET(np->n_flag, NUPDATESIZE);
			if (monitored) {
				events |= VNODE_EVENT_ATTRIB | VNODE_EVENT_EXTEND;
			}
		}
	}

	if (np->n_flag & NCHG) {
		if (np->n_flag & NACC) {
			nvap->nva_timesec[NFSTIME_ACCESS] = np->n_atim.tv_sec;
			nvap->nva_timensec[NFSTIME_ACCESS] = np->n_atim.tv_nsec;
		}
		if (np->n_flag & NUPD) {
			nvap->nva_timesec[NFSTIME_MODIFY] = np->n_mtim.tv_sec;
			nvap->nva_timensec[NFSTIME_MODIFY] = np->n_mtim.tv_nsec;
		}
	}

out:
	if (monitored && events) {
		nfs_vnode_notify(np, events);
	}

out_return:
	NFS_KDBG_EXIT(NFSDBG_OP_LOADATTRCACHE, np, np->n_size, *xidp, error);
	return error;
}

/*
 * Calculate the attribute timeout based on
 * how recently the file has been modified.
 */
long
nfs_attrcachetimeout(nfsnode_t np)
{
	struct nfsmount *nmp;
	struct timeval now;
	int isdir;
	long timeo;

	nmp = NFSTONMP(np);
	if (nfs_mount_gone(nmp)) {
		return 0;
	}

	isdir = vnode_isdir(NFSTOV(np));
#if CONFIG_NFS4
	if ((nmp->nm_vers >= NFS_VER4) && (np->n_openflags & N_DELEG_MASK)) {
		/* If we have a delegation, we always use the max timeout. */
		timeo = isdir ? nmp->nm_acdirmax : nmp->nm_acregmax;
	} else
#endif
	if ((np)->n_flag & NMODIFIED) {
		/* If we have modifications, we always use the min timeout. */
		timeo = isdir ? nmp->nm_acdirmin : nmp->nm_acregmin;
	} else {
		/* Otherwise, we base the timeout on how old the file seems. */
		/* Note that if the client and server clocks are way out of sync, */
		/* timeout will probably get clamped to a min or max value */
		microtime(&now);
		timeo = (now.tv_sec - (np)->n_vattr.nva_timesec[NFSTIME_MODIFY]) / 10;
		if (isdir) {
			if (timeo < nmp->nm_acdirmin) {
				timeo = nmp->nm_acdirmin;
			} else if (timeo > nmp->nm_acdirmax) {
				timeo = nmp->nm_acdirmax;
			}
		} else {
			if (timeo < nmp->nm_acregmin) {
				timeo = nmp->nm_acregmin;
			} else if (timeo > nmp->nm_acregmax) {
				timeo = nmp->nm_acregmax;
			}
		}
	}

	return timeo;
}

/*
 * Check the attribute cache time stamp.
 * If the cache is valid, copy contents to *nvaper and return 0
 * otherwise return an error.
 * Must be called with the node locked.
 */
int
nfs_getattrcache(nfsnode_t np, struct nfs_vattr *nvaper, int flags)
{
	struct nfs_vattr *nvap;
	struct timeval nowup;
	long timeo;
	struct nfsmount *nmp;

	/* Check if the attributes are valid. */
	if (!NATTRVALID(np) || ((flags & NGA_ACL) && !NACLVALID(np))) {
		NFS_KDBG_INFO(NFSDBG_OP_GETATTRCACHE, 0xabc001, np, flags, ENOENT);
		OSAddAtomic64(1, &nfsclntstats.attrcache_misses);
		return ENOENT;
	}

	nmp = NFSTONMP(np);
	if (nfs_mount_gone(nmp)) {
		return ENXIO;
	}
	/*
	 * Verify the cached attributes haven't timed out.
	 * If the server isn't responding, skip the check
	 * and return cached attributes.
	 */
	if (!nfs_use_cache(nmp)) {
		microuptime(&nowup);
		if (np->n_attrstamp > nowup.tv_sec) {
			printf("NFS: Attribute time stamp is in the future by %ld seconds. Invalidating cache\n",
			    np->n_attrstamp - nowup.tv_sec);
			NATTRINVALIDATE(np);
			NACCESSINVALIDATE(np);
			return ENOENT;
		}
		timeo = nfs_attrcachetimeout(np);
		if ((nowup.tv_sec - np->n_attrstamp) >= timeo) {
			NFS_KDBG_INFO(NFSDBG_OP_GETATTRCACHE, 0xabc002, np, flags, ENOENT);
			OSAddAtomic64(1, &nfsclntstats.attrcache_misses);
			return ENOENT;
		}
		if ((flags & NGA_ACL) && ((nowup.tv_sec - np->n_aclstamp) >= timeo)) {
			NFS_KDBG_INFO(NFSDBG_OP_GETATTRCACHE, 0xabc003, np, flags, ENOENT);
			OSAddAtomic64(1, &nfsclntstats.attrcache_misses);
			return ENOENT;
		}
	}

	nvap = &np->n_vattr;
	NFS_KDBG_INFO(NFSDBG_OP_GETATTRCACHE, 0xabc004, np, nvap->nva_size, np->n_size);
	OSAddAtomic64(1, &nfsclntstats.attrcache_hits);

	if (nvap->nva_type != VREG) {
		np->n_size = nvap->nva_size;
	} else if (nvap->nva_size != np->n_size) {
		NFS_KDBG_INFO(NFSDBG_OP_GETATTRCACHE, 0xabc005, np, nvap->nva_size, np->n_size);
		if ((np->n_flag & NMODIFIED) && (nvap->nva_size < np->n_size)) {
			/* if we've modified, stick with larger size */
			nvap->nva_size = np->n_size;
		} else {
			/*
			 * n_size is protected by the data lock, so we need to
			 * defer updating it until it's safe.  We save the new size
			 * and set a flag and it'll get updated the next time we get/drop
			 * the data lock or the next time we do a getattr.
			 */
			np->n_newsize = nvap->nva_size;
			SET(np->n_flag, NUPDATESIZE);
		}
	}

	bcopy((caddr_t)nvap, (caddr_t)nvaper, sizeof(struct nfs_vattr));
	if (np->n_flag & NCHG) {
		if (np->n_flag & NACC) {
			nvaper->nva_timesec[NFSTIME_ACCESS] = np->n_atim.tv_sec;
			nvaper->nva_timensec[NFSTIME_ACCESS] = np->n_atim.tv_nsec;
		}
		if (np->n_flag & NUPD) {
			nvaper->nva_timesec[NFSTIME_MODIFY] = np->n_mtim.tv_sec;
			nvaper->nva_timensec[NFSTIME_MODIFY] = np->n_mtim.tv_nsec;
		}
	}
	if (nvap->nva_acl) {
		if (flags & NGA_ACL) {
			nvaper->nva_acl = kauth_acl_alloc(nvap->nva_acl->acl_entrycount);
			if (!nvaper->nva_acl) {
				return ENOMEM;
			}
			bcopy(nvap->nva_acl, nvaper->nva_acl, KAUTH_ACL_COPYSIZE(nvap->nva_acl));
		} else {
			nvaper->nva_acl = NULL;
		}
	}
	return 0;
}

/*
 * When creating file system objects:
 * Don't bother setting UID if it's the same as the credential performing the create.
 * Don't bother setting GID if it's the same as the directory or credential.
 */
void
nfs_avoid_needless_id_setting_on_create(nfsnode_t dnp, struct vnode_attr *vap, vfs_context_t ctx)
{
	if (VATTR_IS_ACTIVE(vap, va_uid)) {
		if (kauth_cred_getuid(vfs_context_ucred(ctx)) == vap->va_uid) {
			VATTR_CLEAR_ACTIVE(vap, va_uid);
			VATTR_CLEAR_ACTIVE(vap, va_uuuid);
		}
	}
	if (VATTR_IS_ACTIVE(vap, va_gid)) {
		if ((vap->va_gid == dnp->n_vattr.nva_gid) ||
		    (kauth_cred_getgid(vfs_context_ucred(ctx)) == vap->va_gid)) {
			VATTR_CLEAR_ACTIVE(vap, va_gid);
			VATTR_CLEAR_ACTIVE(vap, va_guuid);
		}
	}
}

/*
 * Convert a universal address string to a sockaddr structure.
 *
 * Universal addresses can be in the following formats:
 *
 * d = decimal (IPv4)
 * x = hexadecimal (IPv6)
 * p = port (decimal)
 *
 * d.d.d.d
 * d.d.d.d.p.p
 * x:x:x:x:x:x:x:x
 * x:x:x:x:x:x:x:x.p.p
 * x:x:x:x:x:x:d.d.d.d
 * x:x:x:x:x:x:d.d.d.d.p.p
 *
 * IPv6 strings can also have a series of zeroes elided
 * IPv6 strings can also have a %scope suffix at the end (after any port)
 *
 * rules & exceptions:
 * - value before : is hex
 * - value before . is dec
 * - once . hit, all values are dec
 * - hex+port case means value before first dot is actually hex
 * - . is always preceded by digits except if last hex was double-colon
 *
 * scan, converting #s to bytes
 * first time a . is encountered, scan the rest to count them.
 * 2 dots = just port
 * 3 dots = just IPv4 no port
 * 5 dots = IPv4 and port
 */

#define IS_DIGIT(C) \
	(((C) >= '0') && ((C) <= '9'))

#define IS_XDIGIT(C) \
	(IS_DIGIT(C) || \
	 (((C) >= 'A') && ((C) <= 'F')) || \
	 (((C) >= 'a') && ((C) <= 'f')))

int
nfs_uaddr2sockaddr(const char *uaddr, struct sockaddr *addr)
{
	const char *p, *pd;     /* pointers to current character in scan */
	const char *pnum;       /* pointer to current number to decode */
	const char *pscope;     /* pointer to IPv6 scope ID */
	uint8_t a[18];          /* octet array to store address bytes */
	int i;                  /* index of next octet to decode */
	int dci;                /* index of octet to insert double-colon zeroes */
	int dcount, xdcount;    /* count of digits in current number */
	int needmore;           /* set when we know we need more input (e.g. after colon, period) */
	int dots;               /* # of dots */
	int hex;                /* contains hex values */
	unsigned long val;      /* decoded value */
	int s;                  /* index used for sliding array to insert elided zeroes */

	/* AF_LOCAL address are paths that start with '/' or are empty */
	if (*uaddr == '/' || *uaddr == '\0') { /* AF_LOCAL address */
		struct sockaddr_un *sun = (struct sockaddr_un *)addr;
		sun->sun_family = AF_LOCAL;
		sun->sun_len = sizeof(struct sockaddr_un);
		strlcpy(sun->sun_path, uaddr, sizeof(sun->sun_path));

		return 1;
	}

#define HEXVALUE        0
#define DECIMALVALUE    1

#define GET(TYPE) \
	do { \
	        if ((dcount <= 0) || (dcount > (((TYPE) == DECIMALVALUE) ? 3 : 4))) \
	                return (0); \
	        if (((TYPE) == DECIMALVALUE) && xdcount) \
	                return (0); \
	        val = strtoul(pnum, NULL, ((TYPE) == DECIMALVALUE) ? 10 : 16); \
	        if (((TYPE) == DECIMALVALUE) && (val >= 256)) \
	                return (0); \
	/* check if there is room left in the array */ \
	        if (i > (int)(sizeof(a) - (((TYPE) == HEXVALUE) ? 2 : 1) - ((dci != -1) ? 2 : 0))) \
	                return (0); \
	        if ((TYPE) == HEXVALUE) \
	                a[i++] = ((val >> 8) & 0xff); \
	        a[i++] = (val & 0xff); \
	} while (0)

	hex = 0;
	dots = 0;
	dci = -1;
	i = dcount = xdcount = 0;
	pnum = p = uaddr;
	pscope = NULL;
	needmore = 1;
	if ((*p == ':') && (*++p != ':')) { /* if it starts with colon, gotta be a double */
		return 0;
	}

	while (*p) {
		if (IS_XDIGIT(*p)) {
			dcount++;
			if (!IS_DIGIT(*p)) {
				xdcount++;
			}
			needmore = 0;
			p++;
		} else if (*p == '.') {
			/* rest is decimal IPv4 dotted quad and/or port */
			if (!dots) {
				/* this is the first, so count them */
				for (pd = p; *pd; pd++) {
					if (*pd == '.') {
						if (++dots > 5) {
							return 0;
						}
					} else if (hex && (*pd == '%')) {
						break;
					} else if ((*pd < '0') || (*pd > '9')) {
						return 0;
					}
				}
				if ((dots != 2) && (dots != 3) && (dots != 5)) {
					return 0;
				}
				if (hex && (dots == 2)) { /* hex+port */
					if (!dcount && needmore) {
						return 0;
					}
					if (dcount) { /* last hex may be elided zero */
						GET(HEXVALUE);
					}
				} else {
					GET(DECIMALVALUE);
				}
			} else {
				GET(DECIMALVALUE);
			}
			dcount = xdcount = 0;
			needmore = 1;
			pnum = ++p;
		} else if (*p == ':') {
			hex = 1;
			if (dots) {
				return 0;
			}
			if (!dcount) { /* missing number, probably double colon */
				if (dci >= 0) { /* can only have one double colon */
					return 0;
				}
				dci = i;
				needmore = 0;
			} else {
				GET(HEXVALUE);
				dcount = xdcount = 0;
				needmore = 1;
			}
			pnum = ++p;
		} else if (*p == '%') { /* scope ID delimiter */
			if (!hex) {
				return 0;
			}
			p++;
			pscope = p;
			break;
		} else { /* unexpected character */
			return 0;
		}
	}
	if (needmore && !dcount) {
		return 0;
	}
	if (dcount) { /* decode trailing number */
		GET(dots ? DECIMALVALUE : HEXVALUE);
	}
	if (dci >= 0) {  /* got a double-colon at i, need to insert a range of zeroes */
		/* if we got a port, slide to end of array */
		/* otherwise, slide to end of address (non-port) values */
		int end = ((dots == 2) || (dots == 5)) ? sizeof(a) : (sizeof(a) - 2);
		if (i % 2) { /* length of zero range must be multiple of 2 */
			return 0;
		}
		if (i >= end) { /* no room? */
			return 0;
		}
		/* slide (i-dci) numbers up from index dci */
		for (s = 0; s < (i - dci); s++) {
			a[end - 1 - s] = a[i - 1 - s];
		}
		/* zero (end-i) numbers at index dci */
		for (s = 0; s < (end - i); s++) {
			a[dci + s] = 0;
		}
		i = end;
	}

	/* copy out resulting socket address */
	if (hex) {
		struct sockaddr_in6 *sin6 = (struct sockaddr_in6*)addr;
		if ((((dots == 0) || (dots == 3)) && (i != (sizeof(a) - 2)))) {
			return 0;
		}
		if ((((dots == 2) || (dots == 5)) && (i != sizeof(a)))) {
			return 0;
		}
		bzero(sin6, sizeof(struct sockaddr_in6));
		sin6->sin6_len = sizeof(struct sockaddr_in6);
		sin6->sin6_family = AF_INET6;
		bcopy(a, &sin6->sin6_addr.s6_addr, sizeof(struct in6_addr));
		if ((dots == 5) || (dots == 2)) {
			sin6->sin6_port = htons((in_port_t)((a[16] << 8) | a[17]));
		}
		if (pscope) {
			for (p = pscope; IS_DIGIT(*p); p++) {
				;
			}
			if (*p && !IS_DIGIT(*p)) { /* name */
				ifnet_t interface = NULL;
				if (ifnet_find_by_name(pscope, &interface) == 0) {
					sin6->sin6_scope_id = ifnet_index(interface);
				}
				if (interface) {
					ifnet_release(interface);
				}
			} else { /* decimal number */
				sin6->sin6_scope_id = (uint32_t)strtoul(pscope, NULL, 10);
			}
			/* XXX should we also embed scope id for linklocal? */
		}
	} else {
		struct sockaddr_in *sin = (struct sockaddr_in*)addr;
		if ((dots != 3) && (dots != 5)) {
			return 0;
		}
		if ((dots == 3) && (i != 4)) {
			return 0;
		}
		if ((dots == 5) && (i != 6)) {
			return 0;
		}
		bzero(sin, sizeof(struct sockaddr_in));
		sin->sin_len = sizeof(struct sockaddr_in);
		sin->sin_family = AF_INET;
		bcopy(a, &sin->sin_addr.s_addr, sizeof(struct in_addr));
		if (dots == 5) {
			sin->sin_port = htons((in_port_t)((a[4] << 8) | a[5]));
		}
	}
	return 1;
}

/* Is a mount gone away? */
int
nfs_mount_gone(struct nfsmount *nmp)
{
	return !nmp || vfs_isforce(nmp->nm_mountp) || (nmp->nm_state & (NFSSTA_FORCE | NFSSTA_DEAD));
}

/*
 * Return some of the more significant mount options
 * as a string, e.g. "'ro,hard,intr,tcp,vers=3,sec=krb5,deadtimeout=0'
 */
int
nfs_mountopts(struct nfsmount *nmp, char *buf, int buflen)
{
	int c;

	c = snprintf(buf, buflen, "%s,%s,%s,%s,vers=%d,sec=%s,%sdeadtimeout=%d",
	    (vfs_flags(nmp->nm_mountp) & MNT_RDONLY) ? "ro" : "rw",
	    NMFLAG(nmp, SOFT) ? "soft" : "hard",
	    NMFLAG(nmp, INTR) ? "intr" : "nointr",
	    nmp->nm_sotype == SOCK_STREAM ? "tcp" : "udp",
	    nmp->nm_vers,
	    nmp->nm_auth == RPCAUTH_KRB5  ? "krb5" :
	    nmp->nm_auth == RPCAUTH_KRB5I ? "krb5i" :
	    nmp->nm_auth == RPCAUTH_KRB5P ? "krb5p" :
	    nmp->nm_auth == RPCAUTH_SYS   ? "sys" : "none",
	    nmp->nm_lockmode == NFS_LOCK_MODE_ENABLED ?  "locks," :
	    nmp->nm_lockmode == NFS_LOCK_MODE_DISABLED ? "nolocks," :
	    nmp->nm_lockmode == NFS_LOCK_MODE_LOCAL ? "locallocks," : "",
	    nmp->nm_deadtimeout);

	return c > buflen ? ENOMEM : 0;
}

/*
 * Schedule a callout thread to run an NFS timer function
 * interval milliseconds in the future.
 */
void
nfs_interval_timer_start(thread_call_t call, time_t interval)
{
	uint64_t deadline;

	clock_interval_to_deadline((int)interval, 1000 * 1000, &deadline);
	thread_call_enter_delayed(call, deadline);
}
