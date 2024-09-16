/*
 * Copyright (c) 2000-2014 Apple Inc. All rights reserved.
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
/*
 * Copyright (c) 1992,7 NeXT Computer, Inc.
 *
 * Unix data structure initialization.
 *
 */

#include <mach/mach_types.h>

#include <kern/startup.h>
#include <vm/vm_kern_xnu.h>
#include <mach/vm_prot.h>

#include <sys/param.h>
#include <sys/buf_internal.h>
#include <sys/file_internal.h>
#include <sys/proc_internal.h>
#include <sys/mcache.h>
#include <sys/mbuf.h>
#include <sys/systm.h>
#include <sys/tty.h>
#include <sys/vnode.h>
#include <sys/sysctl.h>
#include <machine/cons.h>
#include <pexpert/pexpert.h>
#include <sys/socketvar.h>
#include <pexpert/pexpert.h>
#include <netinet/tcp_var.h>

extern uint32_t kern_maxvnodes;
#if CONFIG_MBUF_MCACHE
extern vm_map_t mb_map;
#endif /* CONFIG_MBUF_MCACHE */

#if INET
extern uint32_t   tcp_sendspace;
extern uint32_t   tcp_recvspace;
#endif

void            bsd_bufferinit(void);

unsigned int    bsd_mbuf_cluster_reserve(boolean_t *);
void bsd_scale_setup(int);
void bsd_exec_setup(int);

/*
 * Declare these as initialized data so we can patch them.
 */

#ifdef  NBUF
int             max_nbuf_headers = NBUF;
int             niobuf_headers = (NBUF / 2) + 2048;
int             nbuf_hashelements = NBUF;
int             nbuf_headers = NBUF;
#else
int             max_nbuf_headers = 0;
int             niobuf_headers = 0;
int             nbuf_hashelements = 0;
int             nbuf_headers = 0;
#endif

SYSCTL_INT(_kern, OID_AUTO, nbuf, CTLFLAG_RD | CTLFLAG_LOCKED, &nbuf_headers, 0, "");
SYSCTL_INT(_kern, OID_AUTO, maxnbuf, CTLFLAG_RW | CTLFLAG_LOCKED | CTLFLAG_KERN, &max_nbuf_headers, 0, "");

__private_extern__ int customnbuf = 0;

#if SOCKETS
static unsigned int mbuf_poolsz;
#endif

vm_map_t        buffer_map;
vm_map_t        bufferhdr_map;
static int vnodes_sized = 0;

extern void     bsd_startupearly(void);

static vm_map_size_t    bufferhdr_map_size;
SECURITY_READ_ONLY_LATE(struct mach_vm_range)  bufferhdr_range = {};

static vm_map_size_t
bsd_get_bufferhdr_map_size(void)
{
	vm_size_t       size;

	/* clip the number of buf headers upto 16k */
	if (max_nbuf_headers == 0) {
		max_nbuf_headers = (int)atop_kernel(sane_size / 50); /* Get 2% of ram, but no more than we can map */
	}
	if ((customnbuf == 0) && ((unsigned int)max_nbuf_headers > 16384)) {
		max_nbuf_headers = 16384;
	}
	if (max_nbuf_headers < CONFIG_MIN_NBUF) {
		max_nbuf_headers = CONFIG_MIN_NBUF;
	}

	if (niobuf_headers == 0) {
		if (max_nbuf_headers < 4096) {
			niobuf_headers = max_nbuf_headers;
		} else {
			niobuf_headers = (max_nbuf_headers / 2) + 2048;
		}
	}
	if (niobuf_headers < CONFIG_MIN_NIOBUF) {
		niobuf_headers = CONFIG_MIN_NIOBUF;
	}

	size = (max_nbuf_headers + niobuf_headers) * sizeof(struct buf);
	size = round_page(size);

	return size;
}

KMEM_RANGE_REGISTER_DYNAMIC(bufferhdr, &bufferhdr_range, ^() {
	return bufferhdr_map_size = bsd_get_bufferhdr_map_size();
});

void
bsd_startupearly(void)
{
	vm_size_t size = bufferhdr_map_size;

	assert(size);

	/* clip the number of hash elements  to 200000 */
	if ((customnbuf == 0) && nbuf_hashelements == 0) {
		nbuf_hashelements = (int)atop_kernel(sane_size / 50);
		if ((unsigned int)nbuf_hashelements > 200000) {
			nbuf_hashelements = 200000;
		}
	} else {
		nbuf_hashelements = max_nbuf_headers;
	}

	bufferhdr_map = kmem_suballoc(kernel_map,
	    &bufferhdr_range.min_address,
	    size,
	    VM_MAP_CREATE_NEVER_FAULTS,
	    VM_FLAGS_FIXED | VM_FLAGS_OVERWRITE,
	    KMS_PERMANENT | KMS_NOFAIL,
	    VM_KERN_MEMORY_FILE).kmr_submap;

	kmem_alloc(bufferhdr_map,
	    &(vm_offset_t){ bufferhdr_range.min_address },
	    size,
	    KMA_NOFAIL | KMA_PERMANENT | KMA_ZERO | KMA_KOBJECT,
	    VM_KERN_MEMORY_FILE);

	buf_headers = (struct buf *)bufferhdr_range.min_address;

#if SOCKETS
	{
		static const unsigned int       maxspace = 128 * 1024;
		int             scale;

#if INET
		if ((scale = nmbclusters / NMBCLUSTERS) > 1) {
			tcp_sendspace *= scale;
			tcp_recvspace *= scale;

			if (tcp_sendspace > maxspace) {
				tcp_sendspace = maxspace;
			}
			if (tcp_recvspace > maxspace) {
				tcp_recvspace = maxspace;
			}
		}
#endif /* INET */
	}
#endif /* SOCKETS */

	if (vnodes_sized == 0) {
		if (!PE_get_default("kern.maxvnodes", &desiredvnodes, sizeof(desiredvnodes))) {
			/*
			 * Size vnodes based on memory
			 * Number vnodes  is (memsize/64k) + 1024
			 * This is the calculation that is used by launchd in tiger
			 * we are clipping the max based on 16G
			 * ie ((16*1024*1024*1024)/(64 *1024)) + 1024 = 263168;
			 * CONFIG_VNODES is set to 263168 for "medium" configurations (the default)
			 * but can be smaller or larger.
			 */
			desiredvnodes  = (int)(sane_size / 65536) + 1024;
#ifdef CONFIG_VNODES
			if (desiredvnodes > CONFIG_VNODES) {
				desiredvnodes = CONFIG_VNODES;
			}
#endif
		}
		vnodes_sized = 1;
	}
}

#if SOCKETS
SECURITY_READ_ONLY_LATE(struct mach_vm_range) mb_range = {};
KMEM_RANGE_REGISTER_DYNAMIC(mb, &mb_range, ^() {
	nmbclusters = bsd_mbuf_cluster_reserve(NULL) / MCLBYTES;
	return (vm_map_size_t)(nmbclusters * MCLBYTES);
});
#endif /* SOCKETS */

void
bsd_bufferinit(void)
{
	/*
	 * Note: Console device initialized in kminit() from bsd_autoconf()
	 * prior to call to us in bsd_init().
	 */

	bsd_startupearly();

#if CONFIG_MBUF_MCACHE
	mb_map = kmem_suballoc(kernel_map,
	    &mb_range.min_address,
	    (vm_size_t) (nmbclusters * MCLBYTES),
	    FALSE,
	    VM_FLAGS_FIXED | VM_FLAGS_OVERWRITE,
	    KMS_PERMANENT | KMS_NOFAIL,
	    VM_KERN_MEMORY_MBUF).kmr_submap;
	mbutl = (unsigned char *)mb_range.min_address;
#endif /* CONFIG_MBUF_MCACHE */

	/*
	 * Set up buffers, so they can be used to read disk labels.
	 */
	bufinit();
}

/* 512 MB (K32) or 2 GB (K64) hard limit on size of the mbuf pool */
#if !defined(__LP64__)
#define MAX_MBUF_POOL   (512 << MBSHIFT)
#else
#define MAX_MBUF_POOL   (2ULL << GBSHIFT)
#endif /* !__LP64__ */
#define MAX_NCL         (MAX_MBUF_POOL >> MCLSHIFT)

#if SOCKETS
/*
 * this has been broken out into a separate routine that
 * can be called from the x86 early vm initialization to
 * determine how much lo memory to reserve on systems with
 * DMA hardware that can't fully address all of the physical
 * memory that is present.
 */
unsigned int
bsd_mbuf_cluster_reserve(boolean_t *overridden)
{
	int mbuf_pool = 0, ncl = 0;
	static boolean_t was_overridden = FALSE;

	/* If called more than once, return the previously calculated size */
	if (mbuf_poolsz != 0) {
		goto done;
	}

	/*
	 * Some of these are parsed in parse_bsd_args(), but for x86 we get
	 * here early from i386_vm_init() and so we parse them now, in order
	 * to correctly compute the size of the low-memory VM pool.  It is
	 * redundant but rather harmless.
	 */
	(void) PE_parse_boot_argn("ncl", &ncl, sizeof(ncl));
	(void) PE_parse_boot_argn("mbuf_pool", &mbuf_pool, sizeof(mbuf_pool));

	/*
	 * Convert "mbuf_pool" from MB to # of 2KB clusters; it is
	 * equivalent to "ncl", except that it uses different unit.
	 */
	if (mbuf_pool != 0) {
		ncl = (mbuf_pool << MBSHIFT) >> MCLSHIFT;
	}

	if (sane_size > (64 * 1024 * 1024) || ncl != 0) {
		if (ncl || serverperfmode) {
			was_overridden = TRUE;
		}

		if ((nmbclusters = ncl) == 0) {
			/* Auto-configure the mbuf pool size */
			nmbclusters = mbuf_default_ncl(mem_actual);
		} else {
			/* Make sure it's not odd in case ncl is manually set */
			if (nmbclusters & 0x1) {
				--nmbclusters;
			}

			/* And obey the upper limit */
			if (nmbclusters > MAX_NCL) {
				nmbclusters = MAX_NCL;
			}
		}

		/* Round it down to nearest multiple of PAGE_SIZE */
		nmbclusters = (unsigned int)P2ROUNDDOWN(nmbclusters, NCLPG);
	}
	mbuf_poolsz = nmbclusters << MCLSHIFT;
done:
	if (overridden) {
		*overridden = was_overridden;
	}

	return mbuf_poolsz;
}
#endif

#if defined(__LP64__)
extern int tcp_tcbhashsize;
extern int max_cached_sock_count;
#endif

void
bsd_scale_setup(int scale)
{
#if defined(__LP64__)
	if ((scale > 0) && (serverperfmode == 0)) {
		maxproc *= scale;
		maxprocperuid = (maxproc * 2) / 3;
		if (scale > 2) {
			maxfiles *= scale;
			maxfilesperproc = maxfiles / 2;
		}
	}
	/* Apply server scaling rules */
	if ((scale > 0) && (serverperfmode != 0)) {
		maxproc = 2500 * scale;
		hard_maxproc = maxproc;
		/* no fp usage */
		maxprocperuid = (maxproc * 3) / 4;
		maxfiles = (150000 * scale);
		maxfilesperproc = maxfiles / 2;
		desiredvnodes = maxfiles;
		vnodes_sized = 1;
		tcp_tfo_backlog = 100 * scale;
		if (scale > 4) {
			/* clip somaxconn at 32G level */
			somaxconn = 2048;
			/*
			 * For scale > 4 (> 32G), clip
			 * tcp_tcbhashsize to 32K
			 */
			tcp_tcbhashsize = 32 * 1024;

			if (scale > 7) {
				/* clip at 64G level */
				max_cached_sock_count = 165000;
			} else {
				max_cached_sock_count = 60000 + ((scale - 1) * 15000);
			}
		} else {
			somaxconn = 512 * scale;
			tcp_tcbhashsize = 4 * 1024 * scale;
			max_cached_sock_count = 60000 + ((scale - 1) * 15000);
		}
	}

	if (maxproc > hard_maxproc) {
		hard_maxproc = maxproc;
	}
#endif
	bsd_exec_setup(scale);
}
