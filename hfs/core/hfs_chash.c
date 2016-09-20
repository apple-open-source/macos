/*
 * Copyright (c) 2002-2015 Apple Inc. All rights reserved.
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
 * Copyright (c) 1982, 1986, 1989, 1991, 1993, 1995
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *	  notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *	  notice, this list of conditions and the following disclaimer in the
 *	  documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *	  must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *	  may be used to endorse or promote products derived from this software
 *	  without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.	IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)hfs_chash.c
 *	derived from @(#)ufs_ihash.c	8.7 (Berkeley) 5/17/95
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/queue.h>

#include "hfs.h"	/* XXX bringup */
#include "hfs_cnode.h"

extern lck_attr_t *  hfs_lock_attr;
extern lck_grp_t *  hfs_mutex_group;
extern lck_grp_t *  hfs_rwlock_group;

lck_grp_t * chash_lck_grp;
lck_grp_attr_t * chash_lck_grp_attr;
lck_attr_t * chash_lck_attr;

static cnode_t *hfs_cnode_alloc(hfsmount_t *hfsmp, bool *unlocked);

#define CNODEHASH(hfsmp, inum) (&hfsmp->hfs_cnodehashtbl[(inum) & hfsmp->hfs_cnodehash])

static void hfs_cnode_zone_init(hfsmount_t *hfsmp);
static void hfs_cnode_zone_free(hfsmount_t *hfsmp);

/*
 * Initialize cnode hash table.
 */
void
hfs_chashinit()
{
	chash_lck_grp_attr= lck_grp_attr_alloc_init();
	chash_lck_grp  = lck_grp_alloc_init("cnode_hash", chash_lck_grp_attr);
	chash_lck_attr = lck_attr_alloc_init();
}

static void hfs_chash_lock(struct hfsmount *hfsmp) 
{
	lck_mtx_lock(&hfsmp->hfs_chash_mutex);
}

static void hfs_chash_lock_spin(struct hfsmount *hfsmp) 
{
	lck_mtx_lock_spin(&hfsmp->hfs_chash_mutex);
}

static void hfs_chash_lock_convert(struct hfsmount *hfsmp)
{
	lck_mtx_convert_spin(&hfsmp->hfs_chash_mutex);
}

static void hfs_chash_unlock(struct hfsmount *hfsmp) 
{
	lck_mtx_unlock(&hfsmp->hfs_chash_mutex);
}

void
hfs_chashinit_finish(struct hfsmount *hfsmp)
{
	lck_mtx_init(&hfsmp->hfs_chash_mutex, chash_lck_grp, chash_lck_attr);

	hfsmp->hfs_cnodehashtbl = hashinit(desiredvnodes / 4, M_TEMP, &hfsmp->hfs_cnodehash);

	hfs_cnode_zone_init(hfsmp);
}

void
hfs_delete_chash(struct hfsmount *hfsmp)
{
	lck_mtx_destroy(&hfsmp->hfs_chash_mutex, chash_lck_grp);

	FREE(hfsmp->hfs_cnodehashtbl, M_TEMP);

	hfs_cnode_zone_free(hfsmp);
}


/*
 * Use the device, inum pair to find the incore cnode.
 *
 * If it is in core, but locked, wait for it.
 */
struct vnode *
hfs_chash_getvnode(struct hfsmount *hfsmp, ino_t inum, int wantrsrc, int skiplock, int allow_deleted)
{
	struct cnode *cp;
	struct vnode *vp;
	int error;
	u_int32_t vid;

	/* 
	 * Go through the hash list
	 * If a cnode is in the process of being cleaned out or being
	 * allocated, wait for it to be finished and then try again.
	 */
loop:
	hfs_chash_lock_spin(hfsmp);

	for (cp = CNODEHASH(hfsmp, inum)->lh_first; cp; cp = cp->c_hash.le_next) {
		if (cp->c_fileid != inum)
			continue;
		/* Wait if cnode is being created or reclaimed. */
		if (ISSET(cp->c_hflag, H_ALLOC | H_TRANSIT | H_ATTACH)) {
		        SET(cp->c_hflag, H_WAITING);

			(void) msleep(cp, &hfsmp->hfs_chash_mutex, PDROP | PINOD,
			              "hfs_chash_getvnode", 0);
			goto loop;
		}
		/* Obtain the desired vnode. */
		vp = wantrsrc ? cp->c_rsrc_vp : cp->c_vp;
		if (vp == NULLVP)
			goto exit;

		vid = vnode_vid(vp);
		hfs_chash_unlock(hfsmp);

		if ((error = vnode_getwithvid(vp, vid))) {
		        /*
			 * If vnode is being reclaimed, or has
			 * already changed identity, no need to wait
			 */
		        return (NULL);
		}
		if (!skiplock && hfs_lock(cp, HFS_EXCLUSIVE_LOCK, HFS_LOCK_DEFAULT) != 0) {
			vnode_put(vp);
			return (NULL);
		}

		/*
		 * Skip cnodes that are not in the name space anymore
		 * we need to check with the cnode lock held because
		 * we may have blocked acquiring the vnode ref or the
		 * lock on the cnode which would allow the node to be
		 * unlinked
		 */
		if (!allow_deleted) {
			if (cp->c_flag & (C_NOEXISTS | C_DELETED)) {
				if (!skiplock) {
					hfs_unlock(cp);
				}
				vnode_put(vp);
				return (NULL);
			}
		}
		return (vp);
	}
exit:
	hfs_chash_unlock(hfsmp);
	return (NULL);
}


/*
 * Use the device, fileid pair to snoop an incore cnode.
 *
 * A cnode can exists in chash even after it has been 
 * deleted from the catalog, so this function returns 
 * ENOENT if C_NOEXIST is set in the cnode's flag.
 * 
 */
int
hfs_chash_snoop(struct hfsmount *hfsmp, ino_t inum, int existence_only, 
				int (*callout)(const cnode_t *cp, void *), void * arg)
{
	struct cnode *cp;
	int result = ENOENT;

	/* 
	 * Go through the hash list
	 * If a cnode is in the process of being cleaned out or being
	 * allocated, wait for it to be finished and then try again.
	 */
	hfs_chash_lock(hfsmp);

	for (cp = CNODEHASH(hfsmp, inum)->lh_first; cp; cp = cp->c_hash.le_next) {
		if (cp->c_fileid != inum)
			continue;
	
		/*
		 * Under normal circumstances, we would want to return ENOENT if a cnode is in
		 * the hash and it is marked C_NOEXISTS or C_DELETED.  However, if the CNID
		 * namespace has wrapped around, then we have the possibility of collisions.  
		 * In that case, we may use this function to validate whether or not we 
		 * should trust the nextCNID value in the hfs mount point.  
		 * 
		 * If we didn't do this, then it would be possible for a cnode that is no longer backed
		 * by anything on-disk (C_NOEXISTS) to still exist in the hash along with its
		 * vnode.  The cat_create routine could then create a new entry in the catalog
		 * re-using that CNID.  Then subsequent hfs_getnewvnode calls will repeatedly fail
		 * trying to look it up/validate it because it is marked C_NOEXISTS.  So we want
		 * to prevent that from happening as much as possible.
		 */
		if (existence_only) {
			result = 0;
			break;
		}

		/* Skip cnodes that have been removed from the catalog */
		if (cp->c_flag & (C_NOEXISTS | C_DELETED)) {
			result = EACCES;
			break;
		}

		/* Skip cnodes being created or reclaimed. */
		if (!ISSET(cp->c_hflag, H_ALLOC | H_TRANSIT | H_ATTACH)) {
			result = callout(cp, arg);
		}
		break;
	}
	hfs_chash_unlock(hfsmp);

	return (result);
}

/*
 * Use the device, fileid pair to find the incore cnode.
 * If no cnode if found one is created
 *
 * If it is in core, but locked, wait for it.
 *
 * If the cnode is C_DELETED, then return NULL since that 
 * inum is no longer valid for lookups (open-unlinked file).
 *
 * If the cnode is C_DELETED but also marked C_RENAMED, then that means
 * the cnode was renamed over and a new entry exists in its place.  The caller
 * should re-drive the lookup to get the newer entry.  In that case, we'll still
 * return NULL for the cnode, but also return GNV_CHASH_RENAMED in the output flags
 * of this function to indicate the caller that they should re-drive.
 */
struct cnode *
hfs_chash_getcnode(struct hfsmount *hfsmp, ino_t inum, struct vnode **vpp, 
				   int wantrsrc, int skiplock, int *out_flags, int *hflags)
{
	struct cnode	*cp;
	struct cnode	*ncp = NULL;
	vnode_t		vp;
	u_int32_t	vid;

	/* 
	 * Go through the hash list
	 * If a cnode is in the process of being cleaned out or being
	 * allocated, wait for it to be finished and then try again.
	 */
loop:
	hfs_chash_lock_spin(hfsmp);

loop_with_lock:
	for (cp = CNODEHASH(hfsmp, inum)->lh_first; cp; cp = cp->c_hash.le_next) {
		if (cp->c_fileid != inum)
			continue;
		/*
		 * Wait if cnode is being created, attached to or reclaimed.
		 */
		if (ISSET(cp->c_hflag, H_ALLOC | H_ATTACH | H_TRANSIT)) {
		        SET(cp->c_hflag, H_WAITING);

			(void) msleep(cp, &hfsmp->hfs_chash_mutex, PINOD,
			              "hfs_chash_getcnode", 0);
			goto loop_with_lock;
		}
		vp = wantrsrc ? cp->c_rsrc_vp : cp->c_vp;
		if (vp == NULL) {
			/*
			 * The desired vnode isn't there so tag the cnode.
			 */
			SET(cp->c_hflag, H_ATTACH);
			*hflags |= H_ATTACH;

			hfs_chash_unlock(hfsmp);
		} else {
			vid = vnode_vid(vp);

			hfs_chash_unlock(hfsmp);

			if (vnode_getwithvid(vp, vid))
		        	goto loop;
		}
		if (ncp) {
			/*
			 * someone else won the race to create
			 * this cnode and add it to the hash
			 * just dump our allocation
			 */
			hfs_cnode_free(hfsmp, ncp);
			ncp = NULL;
		}

		if (!skiplock) {
			hfs_lock(cp, HFS_EXCLUSIVE_LOCK, HFS_LOCK_ALLOW_NOEXISTS);
		}

		/*
		 * Skip cnodes that are not in the name space anymore
		 * we need to check with the cnode lock held because
		 * we may have blocked acquiring the vnode ref or the
		 * lock on the cnode which would allow the node to be
		 * unlinked.
		 *
		 * Don't return a cnode in this case since the inum
		 * is no longer valid for lookups.
		 */
		if ((cp->c_flag & (C_NOEXISTS | C_DELETED)) && !wantrsrc) {
			int renamed = 0;
			if (cp->c_flag & C_RENAMED) {
				renamed = 1;
			}
			if (!skiplock)
				hfs_unlock(cp);
			if (vp != NULLVP) {
				vnode_put(vp);
			} else {
				hfs_chash_lock_spin(hfsmp);
				CLR(cp->c_hflag, H_ATTACH);
				*hflags &= ~H_ATTACH;
				if (ISSET(cp->c_hflag, H_WAITING)) {
					CLR(cp->c_hflag, H_WAITING);
					wakeup((caddr_t)cp);
				}
				hfs_chash_unlock(hfsmp);
			}
			vp = NULL;
			cp = NULL;
			if (renamed) {
				*out_flags = GNV_CHASH_RENAMED;
			}
		}
		*vpp = vp;
		return (cp);
	}

	/* 
	 * Allocate a new cnode
	 */
	if (skiplock && !wantrsrc)
		panic("%s - should never get here when skiplock is set \n", __FUNCTION__);

	if (ncp == NULL) {
		bool unlocked;

		ncp = hfs_cnode_alloc(hfsmp, &unlocked);

		if (unlocked)
			goto loop_with_lock;
	}
	hfs_chash_lock_convert(hfsmp);

#if HFS_MALLOC_DEBUG
	bzero(ncp, __builtin_offsetof(struct cnode, magic));
#else
	bzero(ncp, sizeof(*ncp));
#endif

	SET(ncp->c_hflag, H_ALLOC);
	*hflags |= H_ALLOC;
	ncp->c_fileid = inum;
	TAILQ_INIT(&ncp->c_hintlist); /* make the list empty */
	TAILQ_INIT(&ncp->c_originlist);

	lck_rw_init(&ncp->c_rwlock, hfs_rwlock_group, hfs_lock_attr);
	if (!skiplock)
		(void) hfs_lock(ncp, HFS_EXCLUSIVE_LOCK, HFS_LOCK_DEFAULT);

	/* Insert the new cnode with it's H_ALLOC flag set */
	LIST_INSERT_HEAD(CNODEHASH(hfsmp, inum), ncp, c_hash);
	hfs_chash_unlock(hfsmp);

	*vpp = NULL;
	return (ncp);
}


void
hfs_chashwakeup(struct hfsmount *hfsmp, struct cnode *cp, int hflags)
{
	hfs_chash_lock_spin(hfsmp);

	CLR(cp->c_hflag, hflags);

	if (ISSET(cp->c_hflag, H_WAITING)) {
	        CLR(cp->c_hflag, H_WAITING);
		wakeup((caddr_t)cp);
	}
	hfs_chash_unlock(hfsmp);
}


/*
 * Re-hash two cnodes in the hash table.
 */
void
hfs_chash_rehash(struct hfsmount *hfsmp, struct cnode *cp1, struct cnode *cp2)
{
	hfs_chash_lock_spin(hfsmp);

	LIST_REMOVE(cp1, c_hash);
	LIST_REMOVE(cp2, c_hash);
	LIST_INSERT_HEAD(CNODEHASH(hfsmp, cp1->c_fileid), cp1, c_hash);
	LIST_INSERT_HEAD(CNODEHASH(hfsmp, cp2->c_fileid), cp2, c_hash);

	hfs_chash_unlock(hfsmp);
}


/*
 * Remove a cnode from the hash table.
 */
int
hfs_chashremove(struct hfsmount *hfsmp, struct cnode *cp)
{
	hfs_chash_lock_spin(hfsmp);

	/* Check if a vnode is getting attached */
	if (ISSET(cp->c_hflag, H_ATTACH)) {
		hfs_chash_unlock(hfsmp);
		return (EBUSY);
	}
	if (cp->c_hash.le_next || cp->c_hash.le_prev) {
	    LIST_REMOVE(cp, c_hash);
	    cp->c_hash.le_next = NULL;
	    cp->c_hash.le_prev = NULL;
	}
	hfs_chash_unlock(hfsmp);

	return (0);
}

/*
 * Remove a cnode from the hash table and wakeup any waiters.
 */
void
hfs_chash_abort(struct hfsmount *hfsmp, struct cnode *cp)
{
	hfs_chash_lock_spin(hfsmp);

	LIST_REMOVE(cp, c_hash);
	cp->c_hash.le_next = NULL;
	cp->c_hash.le_prev = NULL;

	CLR(cp->c_hflag, H_ATTACH | H_ALLOC);
	if (ISSET(cp->c_hflag, H_WAITING)) {
	        CLR(cp->c_hflag, H_WAITING);
		wakeup((caddr_t)cp);
	}
	hfs_chash_unlock(hfsmp);
}


/*
 * mark a cnode as in transition
 */
void
hfs_chash_mark_in_transit(struct hfsmount *hfsmp, struct cnode *cp)
{
	hfs_chash_lock_spin(hfsmp);

        SET(cp->c_hflag, H_TRANSIT);

	hfs_chash_unlock(hfsmp);
}

/* Search a cnode in the hash.  This function does not return cnode which 
 * are getting created, destroyed or in transition.  Note that this function
 * does not acquire the cnode hash mutex, and expects the caller to acquire it.
 * On success, returns pointer to the cnode found.  On failure, returns NULL.
 */
static 
struct cnode *
hfs_chash_search_cnid(struct hfsmount *hfsmp, cnid_t cnid) 
{
	struct cnode *cp;

	for (cp = CNODEHASH(hfsmp, cnid)->lh_first; cp; cp = cp->c_hash.le_next) {
		if (cp->c_fileid == cnid) {
			break;
		}
	}

	/* If cnode is being created or reclaimed, return error. */
	if (cp && ISSET(cp->c_hflag, H_ALLOC | H_TRANSIT | H_ATTACH)) {
		cp = NULL;
	}

	return cp;
}

/* Search a cnode corresponding to given device and ID in the hash.  If the 
 * found cnode has kHFSHasChildLinkBit cleared, set it.  If the cnode is not 
 * found, no new cnode is created and error is returned.
 * 
 * Return values - 
 *	-1 : The cnode was not found.
 * 	 0 : The cnode was found, and the kHFSHasChildLinkBit was already set.
 *	 1 : The cnode was found, the kHFSHasChildLinkBit was not set, and the 
 *	     function had to set that bit.
 */
int
hfs_chash_set_childlinkbit(struct hfsmount *hfsmp, cnid_t cnid)
{
	int retval = -1;
	struct cnode *cp;

	hfs_chash_lock_spin(hfsmp);

	cp = hfs_chash_search_cnid(hfsmp, cnid);
	if (cp) {
		if (cp->c_attr.ca_recflags & kHFSHasChildLinkMask) {
			retval = 0;
		} else {
			cp->c_attr.ca_recflags |= kHFSHasChildLinkMask;
			retval = 1;
		}
	}
	hfs_chash_unlock(hfsmp);

	return retval;
}

// -- Cnode Memory Allocation --

/*
 * We allocate chunks but to minimise fragmentation, we store the
 * chunks in a heap ordered such that the chunk with the least number
 * of free elements is at the top.  At the end of each chunk we store
 * a pointer to a header somewhere within the chunk.  When all
 * elements in a chunk are in use, the pointer is NULL.  Given that
 * chunks will always be aligned to a page, we can compute the element
 * index in the chunk using this equation:
 *
 *		y * (uintptr_t)el % PAGE_SIZE / gcd % (PAGE_SIZE / gcd)
 *
 * where gcd is the greatest common divisor of elem_size and
 * PAGE_SIZE, (which we calculate using the Euclidean algorithm) and y
 * also falls out of the Euclidean algorithm -- see
 * hfs_cnode_zone_init below.  The proof for this is left as an
 * exercise for the reader.  Hint: start with the equation:
 *
 *      elem_ndx * elem_size = PAGE_SIZE * elem_pg + elem_addr % PAGE_SIZE
 *
 * Now realise that can be made to look like a Diophantine equation
 * (ax + by = c) which is solvable using the Extended Euclidean
 * algorithm and from there you will arrive at the equation above.
 */

enum {
	CHUNK_HDR_MAGIC		= 0x6b6e6863,			// chnk
	CNODE_MAGIC			= 0x65646f6e63736668,	// hfscnode
	ELEMENT_ALIGNMENT	= 8,

	/*
	 * We store the pointer to the header for a chunk 8 bytes in from
	 * the end of the chunk.
	 */
	CHUNK_TAIL_LEN		= 8,
};

typedef struct chunk_hdr
{
	void *next_free;
	uint32_t magic;						// = CHUNK_HDR_MAGIC
	uint16_t free_count;
	uint16_t heap_ndx;
	struct chunk_hdr **phdr;
} chunk_hdr_t;

static void hfs_cnode_zone_init(hfsmount_t *hfsmp)
{
	struct cnode_zone *z = &hfsmp->z;
	
	int elem_size = sizeof(cnode_t);

	elem_size = roundup(elem_size, ELEMENT_ALIGNMENT);

	z->elem_size = elem_size;

	// Figure out chunk_size
	int npages, chunk_size, waste;
	
	for (npages = 1;; ++npages) {
		chunk_size = npages * page_size;
		waste = (chunk_size - CHUNK_TAIL_LEN) % elem_size;

		// If waste is less than 1%, that's good enough
		if (waste < chunk_size / 100)
			break;
	}

	z->chunk_size = chunk_size;
	z->alloc_count = (chunk_size - CHUNK_TAIL_LEN) / elem_size;

	// Compute the GCD of elem_size and page_size using Euclidean algorithm
	int t = 1, last_t = 0, r = z->elem_size, last_r = PAGE_SIZE;

	while (r != 0) {
		int q = last_r / r;
		int next_r = last_r - q * r;
		last_r = r;
		r = next_r;
		int next_t = last_t - q * t;
		last_t = t;
		t = next_t;
	}

	z->gcd = last_r;
	z->y = last_t;

	z->heap_max_count = 128 / sizeof(void *);
	z->heap = hfs_malloc(z->heap_max_count * sizeof(void *));

#if DEBUG
	printf("hfs: cnode size %d, chunk size %d, # per chunk %d, waste %d\n",
		   elem_size, chunk_size, z->alloc_count, waste);
#endif
}

static void hfs_cnode_zone_free(hfsmount_t *hfsmp)
{
	// Check that all heap chunks are completely free
	struct cnode_zone *z = &hfsmp->z;

	for (int i = 0; i < z->heap_count; ++i) {
#if DEBUG
		hfs_assert(z->heap[i]->free_count == z->alloc_count);
#else
		if (z->heap[i]->free_count != z->alloc_count) {
			printf("hfs: cnodes leaked!\n");
			continue;
		}
#endif
		void *chunk = (void *)((uintptr_t)z->heap[i]->phdr
							   - (z->chunk_size - CHUNK_TAIL_LEN));
		hfs_free(chunk, z->chunk_size);
	}

	hfs_free(z->heap, z->heap_max_count * sizeof(void *));
}

static void *zel(struct cnode_zone *z, void *chunk, int i)
{
	return chunk + i * z->elem_size;
}

static chunk_hdr_t **zphdr(struct cnode_zone *z, void *chunk)
{
	return chunk + z->chunk_size - CHUNK_TAIL_LEN;
}

#if 0
static void hfs_check_heap(hfsmount_t *hfsmp, void *just_removed)
{
	struct cnode_zone *z = &hfsmp->z;
	
	for (int i = 0; i < z->heap_count; ++i) {
		hfs_assert(z->heap[i]->magic == CHUNK_HDR_MAGIC);
		hfs_assert(z->heap[i]->free_count > 0
			   && z->heap[i]->free_count <= z->alloc_count);
		hfs_assert(z->heap[i]->heap_ndx == i);
		void *max_el = z->heap[i]->phdr;
		void *min_el = (void *)((uintptr_t)z->heap[i]->phdr
								+ CHUNK_TAIL_LEN - z->chunk_size);
		int count = 1;
		hfs_assert(z->heap[i] != just_removed);
		void *el = z->heap[i]->next_free;
		size_t bitmap_size = (z->alloc_count + 7) / 8;
		uint8_t bitmap[bitmap_size];
		bzero(bitmap, bitmap_size);
		int ndx = (int)((void *)z->heap[i] - min_el) / z->elem_size;
		bitmap[ndx / 8] |= 0x80 >> ndx % 8;
		while (el) {
			hfs_assert(el != just_removed);
			hfs_assert(el >= min_el
				   && el < max_el && (el - min_el) % z->elem_size == 0);
			int n = (int)(el - min_el) / z->elem_size;
			hfs_assert(!(bitmap[n / 8] & (0x80 >> n % 8)));
			bitmap[n / 8] |= 0x80 >> n % 8;
			el = *(void **)el;
			++count;
		}
		hfs_assert(count == z->heap[i]->free_count);
		if (i)
			hfs_assert(z->heap[(i + 1) / 2 - 1]->free_count <= z->heap[i]->free_count);
	}
}
#else
#define hfs_check_heap(a, b)	(void)0
#endif

static void hfs_cnode_set_magic(cnode_t *cp, uint64_t v)
{
#if HFS_MALLOC_DEBUG
	cp->magic = v;
#endif
}

static cnode_t *hfs_cnode_alloc(hfsmount_t *hfsmp, bool *unlocked)
{
	hfs_check_heap(hfsmp, NULL);

	*unlocked = false;

	struct cnode_zone *z = &hfsmp->z;
	void *el;

	while (!z->heap_count) {
		if (z->spare) {
			/*
			 * We have a spare chunk we can use so initialise it, add
			 * it to the heap and return one element from it.
			 */
			chunk_hdr_t *chunk_hdr = z->spare;
			z->spare = NULL;
			void *last = NULL;
			for (int i = z->alloc_count - 2; i > 0; --i) {
				void *p = zel(z, chunk_hdr, i);
				*(void **)p = last;
				hfs_cnode_set_magic(p, 0);
				last = p;
			}
			hfs_cnode_set_magic((void *)chunk_hdr, 0);
			chunk_hdr->phdr = zphdr(z, chunk_hdr);
			chunk_hdr->next_free = last;
			chunk_hdr->free_count = z->alloc_count - 1;
			chunk_hdr->heap_ndx = 0;
			// Set the tail to the index of the chunk_hdr
			*zphdr(z, chunk_hdr) = chunk_hdr;
			z->heap[0] = chunk_hdr;
			z->heap_count = 1;
			el = zel(z, chunk_hdr, z->alloc_count - 1);
			hfs_cnode_set_magic(el, 0);
			goto exit;
		}

		*unlocked = true;

		if (z->allocating) {
			z->waiting = true;
			msleep(z, &hfsmp->hfs_chash_mutex, PINOD | PSPIN,
				   "hfs_cnode_alloc", NULL);
		} else {
			z->allocating = true;
			lck_mtx_unlock(&hfsmp->hfs_chash_mutex);
			chunk_hdr_t *chunk_hdr = hfs_malloc(z->chunk_size);
			chunk_hdr->magic = CHUNK_HDR_MAGIC;
			hfs_assert(!((uintptr_t)chunk_hdr & PAGE_MASK));
			lck_mtx_lock_spin(&hfsmp->hfs_chash_mutex);
			z->allocating = false;
			if (z->waiting) {
				wakeup(z);
				z->waiting = false;
			}
			hfs_assert(!z->spare);
			z->spare = chunk_hdr;
		}
	}

	chunk_hdr_t *chunk_hdr = z->heap[0];
	if (chunk_hdr->next_free) {
		/*
		 * Not the last element in this chunk, so just pull an element
		 * off and return it.  This chunk should remain at the top of
		 * the heap.
		 */
		el = chunk_hdr->next_free;
		chunk_hdr->next_free = *(void **)chunk_hdr->next_free;
		--chunk_hdr->free_count;
		
		goto exit;
	}

	/*
	 * This is the last element in the chunk so we need to fix up the
	 * heap.
	 */
	el = chunk_hdr;

	*chunk_hdr->phdr = NULL;
	chunk_hdr->magic = ~CHUNK_HDR_MAGIC;

	if (!--z->heap_count)
		goto exit;

	chunk_hdr_t *v = z->heap[z->heap_count];

	// Fix heap downwards; offset by one to make easier
	int k = 1;
	chunk_hdr_t **a = &z->heap[0] - 1;
	int N = z->heap_count;
	
	for (;;) {
		// Look at the next level down
		int j = k * 2;
		if (j > N)
			break;
		// Pick the smaller of the two that we're looking at
		if (j < N && a[j]->free_count > a[j+1]->free_count)
			++j;
		if (v->free_count <= a[j]->free_count)
			break;
		// Shift element at j to k
		a[k] = a[j];
		a[k]->heap_ndx = k - 1;
		
		// Descend
		k = j;
	}
	
	a[k] = v;
	a[k]->heap_ndx = k - 1;
	
exit:
	
	hfs_check_heap(hfsmp, el);

#if HFS_MALLOC_DEBUG
	if (((cnode_t *)el)->magic == CNODE_MAGIC) {
#if __x86_64__
		asm("int $3\n");
#elif __arm64__
		asm("svc 0\n");
#else
		asm("trap\n");
#endif
	}
#endif // HFS_MALLOC_DEBUG

	hfs_cnode_set_magic(el, CNODE_MAGIC);

	return el;
}

void hfs_cnode_free(hfsmount_t *hfsmp, cnode_t *cp)
{
	void *el = cp;
	void *old_heap = NULL;
	size_t old_heap_size = 0;
	struct cnode_zone *z = &hfsmp->z;

	int ndx_in_chunk = (z->y * (uintptr_t)el % PAGE_SIZE
						/ z->gcd % (PAGE_SIZE / z->gcd));

	void *free_chunk = NULL, *chunk = el - ndx_in_chunk * z->elem_size;

	lck_mtx_lock_spin(&hfsmp->hfs_chash_mutex);

#if HFS_MALLOC_DEBUG
	hfs_assert(cp->magic == CNODE_MAGIC);
	cp->magic = ~CNODE_MAGIC;
#endif

loop:
	
	hfs_check_heap(hfsmp, NULL);

	chunk_hdr_t *hdr = *zphdr(z, chunk);

	int k, orig_k;

	if (hdr) {
		/*
		 * This chunk already has some free elements in it so we chain this
		 * element on and then fix up the heap.
		 */
		hfs_assert(hdr->magic == CHUNK_HDR_MAGIC);

		*(void **)el = hdr->next_free;
		hdr->next_free = el;
		hfs_assert(hdr->next_free != hdr);
		
		chunk_hdr_t *v;
		orig_k = k = hdr->heap_ndx + 1;
		
		chunk_hdr_t **a = &z->heap[0] - 1;
		
		if (++hdr->free_count == z->alloc_count) {
			// Chunk is totally free

			// Always keep at least one chunk on the heap
			if (z->heap_count == 1)
				goto exit;

			// Remove the chunk
			free_chunk = chunk;
			--z->heap_count;
			v = z->heap[z->heap_count];
			
			if (k > 1 && a[k / 2]->free_count > v->free_count) {
				do {
					a[k] = a[k / 2];
					a[k]->heap_ndx = k - 1;
					k = k / 2;
				} while (k > 1 && a[k / 2]->free_count > v->free_count);
				a[k] = v;
				a[k]->heap_ndx = k - 1;
				goto exit;
			}
		} else
			v = hdr;

		// Fix up the heap
		int N = z->heap_count;

		for (;;) {
			// Look at the next level down
			int j = k * 2;
			if (j > N)
				break;
			// Pick the smaller of the two that we're looking at
			if (j < N && a[j]->free_count > a[j+1]->free_count)
				++j;
			if (v->free_count <= a[j]->free_count)
				break;
			// Shift element at j to k
			a[k] = a[j];
			a[k]->heap_ndx = k - 1;
			
			// Descend
			k = j;
		}
		
		a[k] = v;
		a[k]->heap_ndx = k - 1;
	} else {
		// This element is the first that's free in this chunk

		if (z->heap_count == z->heap_max_count) {
			// We need to resize the heap
			int new_max_count = z->heap_max_count * 2;
			lck_mtx_unlock(&hfsmp->hfs_chash_mutex);
			if (old_heap)
				hfs_free(old_heap, old_heap_size);
			struct chunk_hdr **new_heap = hfs_malloc(new_max_count * sizeof(void *));
			lck_mtx_lock_spin(&hfsmp->hfs_chash_mutex);
			// Check to see if someone beat us to it
			if (new_max_count > z->heap_max_count) {
				memcpy(new_heap, z->heap, z->heap_count * sizeof(void *));
				old_heap_size = z->heap_max_count * sizeof(void *);
				z->heap_max_count = new_max_count;
				old_heap = z->heap;
				z->heap = new_heap;
			} else {
				old_heap = new_heap;
				old_heap_size = new_max_count * sizeof(void *);
			}
			// Lock was dropped so we have to go around again
			goto loop;
		}

		hdr = (chunk_hdr_t *)el;
		*zphdr(z, chunk) = hdr;
		hdr->free_count = 1;
		hdr->next_free = NULL;
		hdr->heap_ndx = 0;
		hdr->phdr = zphdr(z, chunk);
		hdr->magic = CHUNK_HDR_MAGIC;

		// Fix heap upwards; offset by one to make easier
		chunk_hdr_t **a = &z->heap[0] - 1;

		if (z->heap_count++) {
			k = z->heap_count;
			chunk_hdr_t *v = z->heap[0];
			while (k > 3 && a[k / 2]->free_count > v->free_count) {
				a[k] = a[k / 2];
				a[k]->heap_ndx = k - 1;
				k = k / 2;
			}
			a[k] = v;
			a[k]->heap_ndx = k - 1;
		}

		z->heap[0] = hdr;
	}

exit:

	hfs_check_heap(hfsmp, NULL);
	lck_mtx_unlock(&hfsmp->hfs_chash_mutex);

	if (old_heap)
		hfs_free(old_heap, old_heap_size);
	if (free_chunk)
		hfs_free(free_chunk, z->chunk_size);
}
