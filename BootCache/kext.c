/*
 * Boot-time performance cache.
 *
 * We implement a shim layer for a block device which can be advised ahead
 * of time of the likely read pattern for the block device.
 *
 * Armed with a sorted version of this read pattern (the "playlist"), we
 * cluster the reads into a private cache, taking advantage of the
 * sequential I/O pattern. Incoming read requests are then satisfied from
 * the cache (if possible), or by forwarding to the original strategy routine.
 *
 * We also record the pattern of incoming read requests (the "history
 * list"), in order to provide for adaptation to changes in behaviour by
 * the calling layer.
 *
 * The hit rate of the cache is also measured, and caching will be disabled
 * preemptively if the hit rate is too low.
 *
 * The cache is controlled by a sysctl interface.  Typical usage is as
 * follows:
 *
 * - Load a sorted read pattern.
 * - Wait for the operation(s) of interest to be performed.
 * - Fetch the new (unsorted) read pattern.
 *
 * Loading a read pattern initialises the cache; upon successful loading
 * the cache is active and readhead is being performed.  Fetching the
 * recorded read pattern disables the cache and frees all memory in use.
 *
 * Limitations:
 *
 * - only one copy of the cache may be active at a time
 * - the cache only works on the root device
 */

/*
 * Build options. 
 */
/*#define DEBUG*/
/*#define EXTRA_DEBUG*/

/*
 * Emulate readahead, don't actually perform it.
 */
/*#define EMULATE_ONLY*/

/*
 * Don't collect request history.
 */
/*#define NO_HISTORY*/

/*
 * Don't use history cluster chains (seems to be a malloc problem with these
 * enabled)
 */
/*#define STATIC_HISTORY*/

/*
 * Keep a log of read history to aid in debugging.
 */
/*#define READ_HISTORY_BUFFER	1000*/
/*#define READ_HISTORY_ALL*/

/*
 * Wire the cache buffer.
 */
/*#define WIRE_BUFFER*/

/*
 * Ignore the batches on playlist entries.
 */
/*#define IGNORE_BATCH */

/*
 * Tunable parameters.
 */

/*
 * Minimum required physical memory before BootCache will
 * activate.  Insufficient physical memory leads to paging
 * activity during boot, which is not repeatable and can't
 * be distinguished from the repeatable read patterns.
 *
 * Size in megabytes.
 */
static int BC_minimum_mem_size = 256;

/*
 * Number of seconds to wait in BC_strategy for readahead to catch up
 * with our request.   Tuninig issues:
 *  - Waiting too long may hold up the system behind this I/O/.
 *  - Bypassing an I/O too early will reduce the hit rate and adversely
 *    affect performance on slow media.
 */
#define BC_READAHEAD_WAIT_DEFAULT	10
#define BC_READAHEAD_WAIT_CDROM		45
static int BC_wait_for_readahead = BC_READAHEAD_WAIT_DEFAULT;

/*
 * Cache hit ratio monitoring.
 *
 * We do not start to watch the cache hit rate until we have seen
 * BC_chit_check_start requests, after which we will shut the cache
 * down if the hit rate falls below BC_chit_low_level (percent).
 */
static int BC_chit_check_start = 500;
static int BC_chit_low_level = 50;

/*
 * Cache timeout.
 *
 * If the cache is not turned off, the history buffer will grow to
 * its maximum size, and blocks may remain tied up in the cache
 * indefinitely.  This timeout will kill the cache after the
 * specified interval (in seconds).
 */
static int BC_cache_timeout = 120;

/*
 * Maximum read size.
 *
 * Due to platform hardware limitations, this must never result in
 * a request that would require more than 32 entries in a scatter-gather
 * list.
 */
#define BC_MAX_READ	(64 * 1024)

/*
 * Trace macros
 */
#define DBG_BOOTCACHE	6
#define	DBG_BC_CUT	1


#ifdef DEBUG
# define MACH_DEBUG
# define debug(fmt, args...)	printf("**** %s: " fmt "\n", __FUNCTION__ , ##args)
extern void Debugger(char *);
#else
# define debug(fmt, args...)
#endif
#define message(fmt, args...)	printf("BootCache: " fmt "\n" , ##args)

#ifdef EXTRA_DEBUG
# define xdebug(fmt, args...)	printf("+++ %s: " fmt "\n", __FUNCTION__ , ##args)
#else
# define xdebug(fmt, args...)
#endif

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/kdebug.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/ubc.h>

#include <ipc/ipc_types.h>
#include <mach/host_priv.h>
#include <mach/kmod.h>
#include <mach/vm_map.h>
#include <mach/vm_param.h>	/* for mem_size */
#include <vm/vm_kern.h>

#include <libkern/libkern.h>

#include <mach/mach_types.h>
#include <kern/assert.h>
#include <kern/host.h>
#include <kern/thread_call.h>

#include <IOKit/storage/IOMediaBSDClient.h>

#include <pexpert/pexpert.h>

#include "BootCache.h"

#ifdef STATIC_PLAYLIST
# include "playlist.h"
#endif

/*
 * A cache extent.
 *
 * Each extent represents a contiguous range of disk blocks which are
 * held in the cache.  All values in bytes.
 */
struct BC_cache_extent {
	lck_mtx_t	ce_lock;
	u_int64_t	ce_offset;	/* physical offset on device */
	u_int64_t	ce_length;	/* data length */
	caddr_t		ce_data;	/* pointer to base of data in buffer */
	int		ce_flags;	/* low 8 bits mark the batch */ 
#define CE_ABORTED	(1 << 9)	/* extent no longer has data */
#define CE_IODONE	(1 << 10)	/* extent has been read */
};

/*
 * A history list cluster.
 */
struct BC_history_cluster {
	struct BC_history_cluster *hc_link;
	int		hc_entries;	/* number of entries in list */
	struct BC_history_entry hc_data[0];
};

#ifdef READ_HISTORY_BUFFER
struct BC_read_history_entry {
	struct BC_cache_extent	*rh_extent;	/* extent this read is for */
	u_int64_t		rh_blkno;	/* offset of actual read */
	u_int64_t		rh_bcount;	/* length in bytes */
	int			rh_result;
# define BC_RHISTORY_OK		0
# define BC_RHISTORY_FAIL	1
};
#endif

#ifdef STATIC_HISTORY
# define BC_HISTORY_ALLOC	128000
# define BC_HISTORY_ENTRIES						\
	(((BC_HISTORY_ALLOC - sizeof(struct BC_history_cluster)) /	\
	    sizeof(struct BC_history_entry)) - 1)
# define BC_HISTORY_MAXCLUSTERS	1
#else
# define BC_HISTORY_ALLOC	16000
# define BC_HISTORY_ENTRIES						\
	(((BC_HISTORY_ALLOC - sizeof(struct BC_history_cluster)) /	\
	    sizeof(struct BC_history_entry)) - 1)
# define BC_HISTORY_MAXCLUSTERS	((BC_MAXENTRIES / BC_HISTORY_ENTRIES) + 1)
#endif


/*
 * The cache control structure.
 */
struct BC_cache_control {
	/* the device we are covering */
	dev_t		c_dev;
        struct vnode    *c_devvp;
	struct bdevsw	*c_devsw;

	/* saved strategy routine */
	strategy_fcn_t	*c_strategy;

	/* device block size and total size */
	u_int64_t	c_blocksize;		/* keep 64-bit for promotion */
	u_int64_t	c_devsize;		/* range checking */
	
	/* flags */
	int		c_flags;
#define	BC_FLAG_SHUTDOWN	(1<<0)		/* cache shutting down */
#define BC_FLAG_CACHEACTIVE	(1<<1)		/* cache is active, owns memory */
#define BC_FLAG_HTRUNCATED	(1<<2)		/* history list truncated */
#define BC_FLAG_IOBUSY		(1<<3)		/* readahead in progress */
#define BC_FLAG_STARTED		(1<<4)		/* cache started by user */
	int		c_strategycalls;	/* count of busy strategy calls */
	int		c_bypasscalls;		/* count of busy strategy bypasses */

	int		c_batch;		/* current batch being read */
	int		c_batch_count;		/* number of extent batches */
	
	/*
	 * The cache buffer contains c_buffer_count blocks of disk data.
	 * The c_blockmap field indicates which blocks are still valid
	 * within the buffer; blocks are invalidated when read, and pages
	 * released when all blocks in the page are invalid.
	 * The c_pagemap field indicates which pages are still present.
	 *
	 * It is assumed that PAGE_SIZE is a multiple of c_blocksize.
	 */
	struct BC_cache_extent *c_extents; /* extent list */
	int		c_extent_count;	/* total number of extents */
	struct BC_cache_extent *c_extent_tail; /* next extent to be filled */
	caddr_t		c_buffer;	/* base of buffer in memory */
	int		c_buffer_pages;	/* total size of buffer in pages */
	int		c_buffer_blocks;/* total size of buffer in blocks */
	u_int32_t	*c_blockmap;	/* blocks present in buffer */
	u_int32_t	*c_pagemap;	/* pages present in buffer */
	u_int32_t	*c_iopagemap;	/* pages with outstanding I/O */
	lck_grp_t	*c_lckgrp;

	/* history list, in reverse order */
	struct BC_history_cluster *c_history;
	u_int64_t	c_history_bytes; /* total read size in bytes */

	/* statistics */
	struct BC_statistics c_stats;

	/* Mach VM-related housekeeping */
	mach_port_t	c_map_port;
	vm_map_t	c_map;
	vm_address_t	c_mapbase;
	vm_size_t	c_mapsize;
	mach_port_t	c_entry_port;

#ifdef READ_HISTORY_BUFFER
	int		c_rhistory_idx;
	struct BC_read_history_entry *c_rhistory;
#endif
};

/*
 * The number of blocks per page; assumes that a page
 * will always be >= the size of a disk block.
 */
#define CB_PAGEBLOCKS(c)		(PAGE_SIZE / (c)->c_blocksize)

/*
 * Convert block offset to page offset and vice versa.
 */
#define CB_BLOCK_TO_PAGE(c, block)	((block) / CB_PAGEBLOCKS(c))
#define CB_PAGE_TO_BLOCK(c, page)	((page) * CB_PAGEBLOCKS(c))
#define CB_BLOCK_TO_BYTE(c, block)	((block) * (c)->c_blocksize)
#define CB_BYTE_TO_BLOCK(c, byte)	((byte) / (c)->c_blocksize)

/*
 * The size of an addressable field in the block/page maps.
 * This reflects the u_int32_t type for the block/pagemap types.
 */
#define CB_MAPFIELDBITS			32
#define CB_MAPFIELDBYTES		4

/*
 * The index into an array of addressable fields, and the bit within
 * the addressable field corresponding to an index in the map.
 */
#define CB_MAPIDX(x)			((x) / CB_MAPFIELDBITS)
#define CB_MAPBIT(x)			(1 << ((x) % CB_MAPFIELDBITS))

/*
 * Blockmap management.
 */
#define CB_BLOCK_PRESENT(c, block) \
	((c)->c_blockmap[CB_MAPIDX(block)] &   CB_MAPBIT(block))
#define CB_MARK_BLOCK_PRESENT(c, block) \
	((c)->c_blockmap[CB_MAPIDX(block)] |=  CB_MAPBIT(block))
#define CB_MARK_BLOCK_VACANT(c, block) \
	((c)->c_blockmap[CB_MAPIDX(block)] &= ~CB_MAPBIT(block))

/*
 * Convert a pointer to a block offset in the buffer and vice versa.
 */
#define CB_PTR_TO_BLOCK(c, ptr) \
	((vm_offset_t)((caddr_t)(ptr) - (c)->c_buffer) / (c)->c_blocksize)
#define CB_BLOCK_TO_PTR(c, block) \
	((c)->c_buffer + ((block) * (c)->c_blocksize))

/*
 * Pagemap management.
 */
#define CB_PAGE_PRESENT(c, page)	((c)->c_pagemap[CB_MAPIDX(page)] &   CB_MAPBIT(page))
#define CB_MARK_PAGE_PRESENT(c, page)	((c)->c_pagemap[CB_MAPIDX(page)] |=  CB_MAPBIT(page))
#define CB_MARK_PAGE_VACANT(c, page)	((c)->c_pagemap[CB_MAPIDX(page)] &= ~CB_MAPBIT(page))

#define CB_IOPAGE_BUSY(c, page)		((c)->c_iopagemap[CB_MAPIDX(page)] &   CB_MAPBIT(page))
#define CB_MARK_IOPAGE_BUSY(c, page)	((c)->c_iopagemap[CB_MAPIDX(page)] |=  CB_MAPBIT(page))
#define CB_MARK_IOPAGE_UNBUSY(c, page)	((c)->c_iopagemap[CB_MAPIDX(page)] &= ~CB_MAPBIT(page))


/*
 * Convert a pointer to a page offset in the buffer and vice versa.
 */
#define CB_PTR_TO_PAGE(c, ptr)		(((ptr) - (c)->c_buffer) / PAGE_SIZE)
#define CB_PAGE_TO_PTR(c, page)		((c)->c_buffer + ((page) * PAGE_SIZE))

/*
 * Determine whether a given page is vacant (all blocks are gone).
 * This takes advantage of the expectation that a page's blocks
 * will never cross the boundary of a field in the blockmap.
 */
#define CB_PAGE_VACANT(c, page)							\
	(!((c)->c_blockmap[CB_MAPIDX(CB_PAGE_TO_BLOCK((c), (page)))] &		\
	 (((1 << CB_PAGEBLOCKS(c)) - 1) << (CB_PAGE_TO_BLOCK((c), (page)) %	\
	     CB_MAPFIELDBITS))))

/*
 * Sanity macro, frees and zeroes a pointer if it is not already zeroed.
 */
#define _FREE_ZERO(p, c)						\
	do {								\
		if (p != NULL) {					\
			_FREE(p, c);					\
			p = NULL;					\
		}							\
	} while (0);

#define LOCK_EXTENT(e)		lck_mtx_lock(&(e)->ce_lock)
#define UNLOCK_EXTENT(e)	lck_mtx_unlock(&(e)->ce_lock)
/*
 * Only one instance of the cache is supported.
 */
static struct buf *BC_private_bp = NULL;
static struct BC_cache_control BC_cache_softc;
static struct BC_cache_control *BC_cache = &BC_cache_softc;

/*
 * We support preloaded cache data by checking for a Mach-O
 * segment/section which can be populated with a playlist by the
 * linker.  This is particularly useful in the CD mastering process,
 * as reading the playlist from CD is very slow and rebuilding the
 * kext at mastering time is not practical.
 */
extern kmod_info_t kmod_info;
static struct BC_playlist_header *BC_preloaded_playlist;
static int	BC_preloaded_playlist_size;

static int	BC_check_intersection(struct BC_cache_extent *ce, u_int64_t offset, u_int64_t length);
static struct BC_cache_extent *BC_find_extent(u_int64_t offset, u_int64_t length, int contained);
static int	BC_discard_blocks(struct BC_cache_extent *ce, u_int64_t offset, u_int64_t length);
static int	BC_blocks_present(int base, int nblk);
static void	BC_reader_thread(void *param0, wait_result_t param1);
static void	BC_strategy_bypass(struct buf *bp);
static void	BC_strategy(struct buf *bp);
static void	BC_handle_write(struct buf *bp);
static int	BC_terminate_readahead(void);
static int	BC_terminate_cache(void);
static int	BC_terminate_history(void);
static int	BC_copyin_playlist(size_t length, void *uptr);
static int	BC_init_cache(size_t length, caddr_t uptr, u_int64_t blocksize);
static void	BC_timeout_cache(void *);
static void	BC_add_history(u_int64_t offset, u_int64_t length, int flags);
static size_t	BC_size_history(void);
static int	BC_copyout_history(void *uptr);
static void	BC_discard_history(void);
static void	BC_auto_start(void);

/*
 * Sysctl interface.
 */
static int	BC_sysctl SYSCTL_HANDLER_ARGS;

SYSCTL_PROC(_kern, OID_AUTO, BootCache, CTLFLAG_RW,
    0, 0, BC_sysctl,
    "S,BC_command",
    "BootCache command interface");

/*
 * Netboot exports this hook so that we can determine
 * whether we were netbooted.
 */
extern int	netboot_root(void);

/*
 * bsd_init exports this hook so that we can register for notification
 * once the root filesystem is mounted.
 */
extern void (* mountroot_post_hook)(void);

/*
 * Mach VM-specific functions.
 */
static int	BC_alloc_pagebuffer(size_t size);
static void	BC_free_pagebuffer();
static void	BC_free_page(int page);
/* Convert from a map entry port to a map (not prototyped elsewhere) */
extern vm_map_t convert_port_entry_to_map(ipc_port_t port);
extern vm_object_t convert_port_entry_to_object(ipc_port_t port);
extern void	ipc_port_release_send(ipc_port_t port);
extern kern_return_t memory_object_page_op(
	memory_object_control_t control,
	memory_object_offset_t  offset,
	int                     ops,
	vm_offset_t             *phys_entry,
	int                     *flags);

#ifdef __ppc__
extern void	mapping_prealloc(unsigned int);
extern void	mapping_relpre(void);
#endif

/*
 * Mach-o functions.
 */
#define MACH_KERNEL 1
#include <mach-o/loader.h>
extern void *getsectdatafromheader(struct mach_header *, char *, char *, int *);


/*
 * Test whether a given byte range on disk intersects with an extent's range.
 */
static inline int
BC_check_intersection(struct BC_cache_extent *ce, u_int64_t offset, u_int64_t length)
{
	if ((offset < (ce->ce_offset + ce->ce_length)) &&
			((offset + length) > ce->ce_offset))
			return 1;
	return 0;
}

/*
 * Find a cache extent containing or intersecting the offset/length.
 *
 * We need to be able to find both containing and intersecting
 * extents.  Rather than handle each seperately, we take advantage
 * of the fact that any containing extent will also be an intersecting
 * extent.
 *
 * Thus, a search for a containing extent should be a search for an
 * intersecting extent followed by a test for containment.
 */
static struct BC_cache_extent *
BC_find_extent(u_int64_t offset, u_int64_t length, int contained)
{
	struct BC_cache_extent *p, *base, *last;
	size_t lim;

	/* invariants */
	assert(length > 0);
	
	base = BC_cache->c_extents;
	last = base + BC_cache->c_extent_count - 1;

	/*
	 * If the cache is inactive, exit.
	 */
	if (!(BC_cache->c_flags & BC_FLAG_CACHEACTIVE) || (base == NULL))
		return(NULL);

	/*
	 * Range-check against the cache first.
	 *
	 * Note that we check for possible intersection, rather than
	 * containment.
	 */
	if (((offset + length) <= base->ce_offset) ||
	    (offset >= (last->ce_offset + last->ce_length)))
		return(NULL);

	/*
	 * Perform a binary search for an extent intersecting
	 * the offset and length.
	 *
	 * This code is based on bsearch(3), and the description following
	 * is taken directly from it.
	 *
	 * The code below is a bit sneaky.  After a comparison fails, we
	 * divide the work in half by moving either left or right. If lim
	 * is odd, moving left simply involves halving lim: e.g., when lim
	 * is 5 we look at item 2, so we change lim to 2 so that we will
	 * look at items 0 & 1.  If lim is even, the same applies.  If lim
	 * is odd, moving right again involes halving lim, this time moving
	 * the base up one item past p: e.g., when lim is 5 we change base
	 * to item 3 and make lim 2 so that we will look at items 3 and 4.
	 * If lim is even, however, we have to shrink it by one before
	 * halving: e.g., when lim is 4, we still looked at item 2, so we
	 * have to make lim 3, then halve, obtaining 1, so that we will only
	 * look at item 3.
	 */
	for (lim = BC_cache->c_extent_count; lim != 0; lim >>= 1) {
		p = base + (lim >> 1);

		if (BC_check_intersection(p, offset, length)) {
			/*
			 * If we are checking for containment, verify that
			 * here.
			 * Return NULL directly if we overlap an end of the
			 * extent, as there's no other we will be contained by.
			 */
			if ((contained != 0) &&
			    ((offset < p->ce_offset) ||
			     ((offset + length) > (p->ce_offset + p->ce_length))))
				return(NULL);
			return(p);
		}
		
		if (offset > p->ce_offset) {	/* move right */
			base = p + 1;
			lim--;
		}				/* else move left */
	}
	/* found nothing */
	return(NULL);
}

/*
 * Discard cache blocks and free pages which are no longer required.
 *
 * This should be locked against anyone testing for !vacant pages.
 *
 * We return the number of blocks we marked unused. If the value is
 * zero, the offset/length did not overap this extent.
 *
 * Called with extent lock held.
 */
static int
BC_discard_blocks(struct BC_cache_extent *ce, u_int64_t offset, u_int64_t length)
{
	u_int64_t estart, eend, dstart, dend;
	int base, i, page, discarded, reallyfree, nblk;

	/* invariants */
	assert(length > 0);
#ifdef DEBUG
	lck_mtx_assert(&ce->ce_lock, LCK_MTX_ASSERT_OWNED);
#endif
	
	/*
	 * Constrain offset and length to fit this extent, return immediately if
	 * there is no overlap.
	 */
	dstart = offset;			/* convert to start/end format */
	dend = offset + length;
	assert(dend > dstart);
	estart = ce->ce_offset;
	eend = ce->ce_offset + ce->ce_length;
	assert(eend > estart);

	if (dend <= estart)			/* check for overlap */
		return(0);
	if (eend <= dstart)
		return(0);

	if (dstart < estart)			/* clip to extent */
		dstart = estart;
	if (dend > eend)
		dend = eend;

	xdebug("discard %lu-%lu within %lu-%lu",
	    (unsigned long)dstart, (unsigned long)dend, (unsigned long)estart, (unsigned long)eend);

	/*
	 * Convert length in bytes to a block count.
	 */
	nblk = CB_BYTE_TO_BLOCK(BC_cache, dend - dstart);
	
	/*
	 * Don't free pages if we have not yet read ahead into them.  This
	 * can occur if we are invalidating blocks due to a write while
	 * readahead is still taking place.
	 */
	reallyfree = 1;
	if (!(ce->ce_flags & (CE_IODONE | CE_ABORTED))) {
		reallyfree = 0;
	}
	
	/*
	 * Mark blocks vacant.  For each vacated block, check whether the page
	 * holding it can be freed.
	 *
	 * Note that this could be optimised to reduce the number of
	 * page-freeable checks, but the logic would be more complex.
	 */
	base = CB_PTR_TO_BLOCK(BC_cache, ce->ce_data + (dstart - ce->ce_offset));
	assert(base >= 0);
	discarded = 0;
	for (i = 0; i < nblk; i++) {
		assert((base + i) < BC_cache->c_buffer_blocks);
		
		/* this is a no-op if the block is already gone */
		if (CB_BLOCK_PRESENT(BC_cache, base + i)) {

			/* mark the block as vacant */
			CB_MARK_BLOCK_VACANT(BC_cache, base + i);
			discarded++;

			/* don't try to update the pagemap if we are not freeing */
			if (!reallyfree)
				continue;
			
			/* find the containing page, which must be present */
			page = CB_BLOCK_TO_PAGE(BC_cache, base + i);
			assert(CB_PAGE_PRESENT(BC_cache, page));

			/*
			 * If the page this block is in is now vacant, free the
			 * page.  Only *really* free the page if we can be
			 * certain we'll never try to touch it again.
			 */
			if ((CB_PAGE_PRESENT(BC_cache, page)) && CB_PAGE_VACANT(BC_cache, page)) {
				CB_MARK_PAGE_VACANT(BC_cache, page);

				BC_free_page(page);
			}
		}
	}
	return(discarded);
}

/*
 * Test for the presence of a range of blocks in the cache.
 */
static int
BC_blocks_present(int base, int nblk)
{
	int	blk;
	
	for (blk = 0; blk < nblk; blk++) {
		assert((base + blk) < BC_cache->c_buffer_blocks);
		
		if (!CB_BLOCK_PRESENT(BC_cache, base + blk)) {
			BC_cache->c_stats.ss_hit_blkmissing++;
			/*
			 * Note that we could optionally flush blocks that *are*
			 * present here, on the assumption that this was a read
			 * overlapping another read, and nobody else is going to
			 * subsequently want them.
			 */
			return(0);
		}
	}
	return(1);
}

/*
 * Readahead thread.
 */
static void
BC_reader_thread(void *param0, wait_result_t param1)
{
	struct BC_cache_extent *ce = NULL;
	boolean_t funnel_state;
	struct buf *bp;
	u_int64_t bytesdone;
	int count;
	int	i, x;
	int	bcount;
	uintptr_t bptr;
	uintptr_t offset_in_map;
	kern_return_t kret;
			  
	/* we run under the kernel funnel */
	funnel_state = thread_funnel_set(kernel_flock, TRUE);

	BC_private_bp = buf_alloc(BC_cache->c_devvp);
	bp = BC_private_bp;
	
	debug("reader thread started");

	while (BC_cache->c_batch <= BC_cache->c_batch_count) {
		debug("starting batch %d", BC_cache->c_batch);

		/* iterate over extents to populate */
		for (ce = BC_cache->c_extents;
				ce < (BC_cache->c_extents + BC_cache->c_extent_count);
				ce++) {

			/* Only read extents marked for this batch. */
			if ((ce->ce_flags & CE_BATCH_MASK) != BC_cache->c_batch)
				continue;

			/* loop reading to fill this extent */
			buf_setcount(bp, 0);

			BC_cache->c_extent_tail = ce;
			LOCK_EXTENT(ce);

			for (;;) {

				/* requested shutdown */
				if (BC_cache->c_flags & BC_FLAG_SHUTDOWN)
					goto out;

				/*
				 * Fill the buf to perform the read.
				 *
				 * Note that in the case of a partial read, our buf will
				 * still contain state from the previous read, which we
				 * use to detect this condition.
				 */
				if (buf_count(bp) != 0) {
					/* continuing a partial read */
					daddr64_t blkno;

					blkno = buf_blkno(bp) + CB_BYTE_TO_BLOCK(BC_cache, buf_count(bp));
					buf_setblkno(bp, blkno);
					bytesdone = CB_BLOCK_TO_BYTE(BC_cache, blkno) - ce->ce_offset;
					buf_setcount(bp, MIN(ce->ce_length - bytesdone, BC_MAX_READ));
					buf_setdataptr(bp, (uintptr_t)(ce->ce_data + bytesdone));
				} else {
					/* starting a new extent */
					buf_setblkno(bp, (daddr64_t)CB_BYTE_TO_BLOCK(BC_cache, ce->ce_offset));
					buf_setcount(bp, MIN(ce->ce_length, BC_MAX_READ));
					buf_setdataptr(bp, (uintptr_t)(ce->ce_data));
				}

				bcount = buf_count(bp);
				bptr   = buf_dataptr(bp);

				for (i = 0; i < bcount; ) {
					CB_MARK_IOPAGE_BUSY(BC_cache, CB_PTR_TO_PAGE(BC_cache, (caddr_t)bptr));

					x = 4096 - ((int)bptr & 4095);
					bptr += x;
					bcount -= x;
				}

				buf_setresid(bp, buf_count(bp));	/* ask for residual indication */
				buf_reset(bp, B_READ);


				offset_in_map = (uintptr_t)(((unsigned char *)buf_dataptr(bp) - (unsigned char *)BC_cache->c_buffer) +
							    (unsigned char *)BC_cache->c_mapbase);

	                        kret = vm_map_wire(BC_cache->c_map, (vm_map_offset_t)trunc_page(offset_in_map),
						  (vm_map_offset_t)round_page(offset_in_map+buf_count(bp)),
						   VM_PROT_READ | VM_PROT_WRITE, FALSE);

				/* give the buf to the underlying strategy routine */
				KERNEL_DEBUG_CONSTANT(FSDBG_CODE(DBG_DKRW, DKIO_READ) | DBG_FUNC_NONE,
						(unsigned int)bp, buf_device(bp), (int)buf_blkno(bp), buf_count(bp), 0);

				BC_cache->c_stats.ss_initiated_reads++;
				BC_cache->c_strategy(bp);

				/* wait for the bio to complete */
				buf_biowait(bp);

				if (kret == KERN_SUCCESS) {
					kret = vm_map_unwire(BC_cache->c_map, (vm_map_offset_t)trunc_page(offset_in_map),
							     (vm_map_offset_t)round_page(offset_in_map+buf_count(bp)), FALSE);
					if (kret != KERN_SUCCESS)
						panic("BootCache: vm_map_unwire returned %d\n", kret);
				}

				bcount = buf_count(bp);
				bptr   = buf_dataptr(bp);

				for (i = 0; i < bcount; ) {
					CB_MARK_IOPAGE_UNBUSY(BC_cache, CB_PTR_TO_PAGE(BC_cache, (caddr_t)bptr));

					x = 4096 - ((int)bptr & 4095);
					bptr += x;
					bcount -= x;
				}
				wakeup(&BC_cache->c_iopagemap);

				/*
				 * If the read returned an error, invalidate the blocks
				 * covered by the read (on a residual, we could avoid invalidating
				 * blocks that are claimed to be read as a minor optimisation, but we do
				 * not expect errors as a matter of course).
				 */
				if (buf_error(bp) || (buf_resid(bp) != 0)) {
					debug("read error: extent %d %lu/%lu "
							"(error buf %ld/%ld flags %08x resid %d)",
							ce - BC_cache->c_extents,
							(unsigned long)ce->ce_offset, (unsigned long)ce->ce_length,
							(long)buf_blkno(bp), (long)buf_count(bp),
							buf_flags(bp), buf_resid(bp));

					count = BC_discard_blocks(ce, CB_BLOCK_TO_BYTE(BC_cache, buf_blkno(bp)),
							buf_count(bp));
					debug("read error: discarded %d blocks", count);
					BC_cache->c_stats.ss_read_errors++;
					BC_cache->c_stats.ss_error_discards += count;
				}
#ifdef READ_HISTORY_BUFFER
				if (BC_cache->c_rhistory_idx < READ_HISTORY_BUFFER) {
					BC_cache->c_rhistory[BC_cache->c_rhistory_idx].rh_extent = ce;
					BC_cache->c_rhistory[BC_cache->c_rhistory_idx].rh_blkno = buf_blkno(bp);
					BC_cache->c_rhistory[BC_cache->c_rhistory_idx].rh_bcount = buf_count(bp);
					if (buf_error(bp) || (buf_resid(bp) != 0)) {
						BC_cache->c_rhistory[BC_cache->c_rhistory_idx].rh_result = BC_RHISTORY_FAIL;
						BC_cache->c_rhistory_idx++;
					} else {
# ifdef READ_HISTORY_ALL
						BC_cache->c_rhistory[BC_cache->c_rhistory_idx].rh_result = BC_RHISTORY_OK;
						BC_cache->c_rhistory_idx++;
# endif
					}
				}
#endif
				/*
				 * Test whether we have completed reading this extent's data.
				 */
				if (((CB_BLOCK_TO_BYTE(BC_cache, buf_blkno(bp)) - ce->ce_offset) + buf_count(bp)) >= ce->ce_length)
					break;

			}

			/* update stats */
			BC_cache->c_stats.ss_read_blocks += CB_BYTE_TO_BLOCK(BC_cache, ce->ce_length);

			/*
			 * Wake up anyone wanting this extent, as it is now ready for
			 * use.
			 */
			ce->ce_flags |= CE_IODONE;
			UNLOCK_EXTENT(ce);
			wakeup(ce);
		}
		debug("batch %d done", BC_cache->c_batch);
		/* Reset cache to read the next batch in */
		BC_cache->c_extent_tail = BC_cache->c_extents;	
		BC_cache->c_batch++;
		/* Measure times for the first 4 batches separately */
		microtime(&BC_cache->c_stats.ss_batch_time[MIN(BC_cache->c_batch, STAT_BATCHMAX)]);

		ce = NULL;
	}

out:
	/*
	 * If ce != NULL we have bailed out, but we cannot free memory beyond
	 * the bailout point as it may have been read in a previous batch and 
	 * there may be a sleeping strategy routine assuming that it will 
	 * remain valid.  Since we are typically killed immediately before a 
	 * complete cache termination, this does not represent a significant 
	 * problem.
	 *
	 * However, to prevent readers blocking waiting for readahead to
	 * complete for extents that will never be read, we mark all the
	 * extents we have given up on as aborted.
	 */
	if (ce != NULL) {
		struct BC_cache_extent *tmp = BC_cache->c_extents;
		UNLOCK_EXTENT(ce);
		while ((tmp - BC_cache->c_extents) < BC_cache->c_extent_count) {
			LOCK_EXTENT(tmp);
			/* abort any extent in a batch we haven't hit yet */
			if (BC_cache->c_batch < (tmp->ce_flags & CE_BATCH_MASK)) {
				tmp->ce_flags |= CE_ABORTED;
			}
			/* abort extents in this batch that we haven't reached */
			if ((tmp >= ce) && (BC_cache->c_batch == (tmp->ce_flags & CE_BATCH_MASK))) {
				tmp->ce_flags |= CE_ABORTED;
			}
			/* wake up anyone asleep on this extent */
			UNLOCK_EXTENT(tmp);
			wakeup(tmp);
			tmp++;
		}
	}

	/* finalise accounting */
	microtime(&BC_cache->c_stats.ss_batch_time[STAT_BATCHMAX]);

	/* wake up someone that might be waiting for us to exit */
	BC_cache->c_flags &= ~BC_FLAG_IOBUSY;
	wakeup(&BC_cache->c_flags);
	debug("reader thread done in %d sec",
			(int) BC_cache->c_stats.ss_batch_time[STAT_BATCHMAX].tv_sec - 
			(int) BC_cache->c_stats.ss_batch_time[0].tv_sec);

	BC_private_bp = NULL;
	buf_free(bp);

	(void) thread_funnel_set(kernel_flock, funnel_state);
}


/*
 * Pass a read on to the default strategy handler.
 */
static void
BC_strategy_bypass(struct buf *bp)
{
	int isread;
	assert(bp != NULL);

	/*
	 * Since the strategy routine may drop the funnel, we need
	 * to protect ourselves from being unloaded so that the return
	 * address is still valid.  This is protected by the kernel
	 * funnel.
	 */
	BC_cache->c_bypasscalls++;

	/* if here, and it's a read, we missed the cache */
	if (buf_flags(bp) & B_READ) {
		BC_add_history(CB_BLOCK_TO_BYTE(BC_cache, buf_blkno(bp)), buf_count(bp), BC_HE_MISS);
		isread = 1;
	} else {
		isread = 0;
	}

	/* not really "bypassed" if the cache is not active */
	if (BC_cache->c_flags & BC_FLAG_CACHEACTIVE) {
		BC_cache->c_stats.ss_strategy_bypassed++;
		if (BC_cache->c_flags & BC_FLAG_IOBUSY)
			BC_cache->c_stats.ss_strategy_bypass_active++;
	}

	/* if this is a read, and we do have a cache, and it's active */
	if (isread &&
	    (BC_cache->c_extents != NULL) &&
	    (BC_cache->c_flags & BC_FLAG_CACHEACTIVE)) {
		/*
		 * Check to make sure that we're not missing the cache
		 * too often.  If we are, it might be stale and the
		 * best thing to do would be to simply give up.
		 *
		 * Don't check until readahead has completed, and we've
		 * seen a significant number of inbound requests, and
		 * our success rate on those requests is poor.
		 */
		if (!(BC_cache->c_flags & BC_FLAG_IOBUSY) &&
		    (BC_cache->c_stats.ss_extent_lookups > BC_chit_check_start) &&
		    (((BC_cache->c_stats.ss_extent_hits * 100) /
		      BC_cache->c_stats.ss_extent_lookups) < BC_chit_low_level)) {

			/*
			 * The hit rate is poor, shut the cache down but leave
			 * history recording active.
			 */
			debug("hit rate below threshold (%d hits on %d lookups)",
			    BC_cache->c_stats.ss_extent_hits,
			    BC_cache->c_stats.ss_extent_lookups);
			if (BC_terminate_readahead()) {
				message("could not stop readahead on bad cache hitrate");
			} else {
				if (BC_terminate_cache()) {
					message("could not terminate cache on bad hitrate");
				}
			}
		}
	}
	
	KERNEL_DEBUG_CONSTANT(FSDBG_CODE(DBG_BOOTCACHE, DBG_BC_CUT),
			(unsigned int)bp, 0, 0, 0, 0);

	/* pass the request on */
	BC_cache->c_strategy(bp);

	/* un-refcount ourselves */
	BC_cache->c_bypasscalls--;
}

/*
 * Handle an incoming read request.
 */
static void
BC_strategy(struct buf *bp)
{
	struct BC_cache_extent *ce = NULL;
	boolean_t funnel_state;
	int base, nblk;
	caddr_t p, s;
	int retry;
	struct timeval blocktime, now, elapsed;
	struct timespec timeout;
	daddr64_t blkno;
	int     bcount;
	caddr_t	vaddr;

	assert(bp != NULL);

	/* we run under the kernel funnel */
	funnel_state = thread_funnel_set(kernel_flock, TRUE);

	blkno = buf_blkno(bp);
	bcount = buf_count(bp);

	/* 1/10th of a second */
	timeout.tv_sec = 0;
	timeout.tv_nsec = 100 * 1000 * 1000;

	/*
	 * If the device doesn't match ours for some reason, pretend
	 * we never saw the request at all.
	 */
	if (buf_device(bp) != BC_cache->c_dev)  {
		BC_cache->c_strategy(bp);
		(void) thread_funnel_set(kernel_flock, funnel_state);
		return;
	}

	/*
	 * If the cache is not active, bypass the request.  We may
	 * not be able to fill it, but we still want to record it.
	 */
	if (!(BC_cache->c_flags & BC_FLAG_CACHEACTIVE)) {
		BC_strategy_bypass(bp);
		(void) thread_funnel_set(kernel_flock, funnel_state);
		return;
	}
	
	/*
	 * In order to prevent the cleanup code from racing with us
	 * when we sleep, we track the number of strategy calls active.
	 * This value is protected by the kernel funnel, and must be
	 * incremented here before we have a chance to sleep and
	 * release the funnel following the check for BC_FLAG_CACHEACTIVE.
	 */
	BC_cache->c_strategycalls++;
	BC_cache->c_stats.ss_strategy_calls++;

	/* if it's not a read, pass it off */
	if ( !(buf_flags(bp) & B_READ)) {
		BC_handle_write(bp);
		BC_add_history(CB_BLOCK_TO_BYTE(BC_cache, blkno), bcount, BC_HE_WRITE);
		BC_cache->c_stats.ss_strategy_nonread++;
		goto bypass;
	}

	BC_cache->c_stats.ss_requested_blocks += CB_BYTE_TO_BLOCK(BC_cache, bcount);
	
	/*
	 * Look for a cache extent containing this request.
	 */
	BC_cache->c_stats.ss_extent_lookups++;
	if ((ce = BC_find_extent(CB_BLOCK_TO_BYTE(BC_cache, blkno), bcount, 1)) == NULL)
		goto bypass;
	BC_cache->c_stats.ss_extent_hits++;

	/*
	 * If the extent hasn't been read yet, sleep waiting for it.
	 */
	LOCK_EXTENT(ce);
	blocktime.tv_sec = 0;
	for (retry = 0; ; retry++) {
		/* has the reader thread made it to this extent? */
		if (ce->ce_flags & (CE_ABORTED | CE_IODONE))
			break;	/* don't go to bypass, record blocked time */

		/* check for timeout */
		if (retry > (BC_wait_for_readahead * 10)) {
			debug("timed out waiting for extent to be read (need %d, tail %d)",
			    ce - BC_cache->c_extents, BC_cache->c_extent_tail - BC_cache->c_extents);
			goto bypass;
		}

		/* account for blocked time, part one */
		if (blocktime.tv_sec == 0) {	/* never true for a real time */
			microtime(&blocktime);
			BC_cache->c_stats.ss_strategy_blocked++;
		}
		
		/* not ready, sleep */
		msleep(ce, &ce->ce_lock, PRIBIO, "BC_strategy", &timeout);
	}

	/*
	 * Record any time we spent blocked.
	 */
	if (blocktime.tv_sec != 0) {
		microtime(&now);
		timersub(&now, &blocktime, &elapsed);
		timeradd(&elapsed, &BC_cache->c_stats.ss_wait_time, &BC_cache->c_stats.ss_wait_time);
	}
	
	/*
	 * Check that the extent wasn't aborted while we were asleep.
	 */
	if (ce->ce_flags & CE_ABORTED)
		goto bypass;

	/*
	 * Check that the requested blocks are in the buffer.
	 */
	/* XXX assumes cache block size == disk block size */
	base = CB_PTR_TO_BLOCK(BC_cache, ce->ce_data) +
	    (blkno - CB_BYTE_TO_BLOCK(BC_cache, ce->ce_offset));
	nblk = CB_BYTE_TO_BLOCK(BC_cache, bcount);
	assert(base >= 0);
	if (!BC_blocks_present(base, nblk))
		goto bypass;

	if ((BC_cache->c_flags & BC_FLAG_IOBUSY))
		BC_cache->c_stats.ss_strategy_duringio++;
#ifdef EMULATE_ONLY
	/* discard blocks we have touched */
	BC_cache->c_stats.ss_hit_blocks +=
	    BC_discard_blocks(ce, CB_BLOCK_TO_BYTE(BC_cache, blkno), bcount);

	/* release the extent */
	UNLOCK_EXTENT(ce);
	
	/* we would have hit this request */
	BC_add_history(CB_BLOCK_TO_BYTE(BC_cache, blkno), bcount, BC_HE_HIT);

	/* bypass directly without updating statistics */
	BC_cache->c_strategy(bp);
#else
	/*
	 * buf_map will do it's best to provide access to this
	 * buffer via a kernel accessible address
	 * if it fails, we can still fall through.
	 */
	if (buf_map(bp, &vaddr)) {
		/* can't map, let someone else worry about it */
	        goto bypass;
	}
	p = vaddr;

	/*
	 * It is possible that buf_map will block, and during that
	 * time we may have our cache blocks invalidated.  This shouldn't
	 * happen, as the system will normally not allow overlapping I/O,
	 * but it has been observed in practice.
	 */
	if (!BC_blocks_present(base, nblk)) {
	        /* buf_unmap will take care of all cases */
	        buf_unmap(bp);
		BC_cache->c_stats.ss_strategy_stolen++;
		goto bypass;
	}
	
	/* find the source in our buffer */
	s = CB_BLOCK_TO_PTR(BC_cache, base);

	/* copy from cache to buf */
	bcopy(s, p, bcount);
	buf_setresid(bp, 0);

	/* buf_unmap will take care of all cases */
	buf_unmap(bp);

	/* complete the request */
	buf_biodone(bp);
	
	/* can't dereference bp after a buf_biodone has been issued */

	/* discard blocks we have touched */
	BC_cache->c_stats.ss_hit_blocks +=
	BC_discard_blocks(ce, CB_BLOCK_TO_BYTE(BC_cache, blkno), bcount);

	UNLOCK_EXTENT(ce);
	/* record successful fulfilment (may block) */
	BC_add_history(CB_BLOCK_TO_BYTE(BC_cache, blkno), bcount, BC_HE_HIT);
#endif

	/* we are not busy anymore */
	BC_cache->c_strategycalls--;
	(void) thread_funnel_set(kernel_flock, funnel_state);
	return;

bypass:
	/*
	 * We have to drop the busy count before bypassing, as the
	 * bypass may try to terminate the cache (which will block and
	 * ultimately fail if the busy count is not zero).
	 */
	if (ce != NULL)
		UNLOCK_EXTENT(ce);
	BC_cache->c_strategycalls--;
	BC_strategy_bypass(bp);
	(void) thread_funnel_set(kernel_flock, funnel_state);
	return;
}

/*
 * Handle a write request.  If it touches any blocks we are caching, we have
 * to discard the blocks.
 */
static void
BC_handle_write(struct buf *bp)
{
	struct BC_cache_extent *ce, *p;
	int count;
	daddr64_t blkno;
	u_int64_t offset;
	int     bcount;

	assert(bp != NULL);

	blkno = buf_blkno(bp);
	bcount = buf_count(bp);
	offset = CB_BLOCK_TO_BYTE(BC_cache, blkno);

	/*
	 * Look for an extent that we overlap.
	 */
	if ((ce = BC_find_extent(offset, bcount, 0)) == NULL)
		return;		/* doesn't affect us */

	/*
	 * Discard blocks in the matched extent.
	 */
	LOCK_EXTENT(ce);
	count = BC_discard_blocks(ce, offset, bcount);
	UNLOCK_EXTENT(ce);
	BC_cache->c_stats.ss_write_discards += count;
	assert(count != 0);

	/*
	 * Scan adjacent extents for possible overlap and discard there as well.
	 */
	p = ce - 1;
	while (p >= BC_cache->c_extents && 
			BC_check_intersection(p, offset, bcount)) {
		LOCK_EXTENT(p);
		count = BC_discard_blocks(p, offset, bcount);
		UNLOCK_EXTENT(p);
		if (count == 0)
			break;
		BC_cache->c_stats.ss_write_discards += count;
		p--;
	}
	p = ce + 1;
	while (p < (BC_cache->c_extents + BC_cache->c_extent_count) && 
			BC_check_intersection(p, offset, bcount)) {
		LOCK_EXTENT(p);
		count = BC_discard_blocks(p, offset, bcount);
		UNLOCK_EXTENT(p);
		if (count == 0)
			break;
		BC_cache->c_stats.ss_write_discards += count;
		p++;
	}
	
}

/*
 * Shut down readahead operations.
 */
static int
BC_terminate_readahead(void)
{
	int error;

	/*
	 * If readahead is still in progress, we have to shut it down
	 * cleanly.  This is an expensive process, but since readahead
	 * will normally complete long before the reads it tries to serve
	 * complete, it will typically only be invoked when the playlist
	 * is out of synch and the cache hitrate falls below the acceptable
	 * threshold.
	 *
	 * Note that if readahead is aborted, the reader thread will mark
	 * the aborted extents and wake up any strategy callers waiting
	 * on them, so we don't have to worry about them here.
	 */
	if (BC_cache->c_flags & BC_FLAG_IOBUSY) {
		debug("terminating active readahead");

		/*
		 * Signal the readahead thread to terminate, and wait for
		 * it to comply.  If this takes > 10 seconds, give up.
		 */
		BC_cache->c_flags |= BC_FLAG_SHUTDOWN;
		error = tsleep(&BC_cache->c_flags, PRIBIO, "BC_terminate_readahead", hz * 10);
		if (error == EWOULDBLOCK) {
		
			debug("timed out waiting for I/O to stop");
			if (!(BC_cache->c_flags & BC_FLAG_IOBUSY)) {
				debug("but I/O has stopped!");
			} else {
				debug("doing extent %d of %d  %lu/%lu @ %p",
				    BC_cache->c_extent_tail - BC_cache->c_extents,
				    BC_cache->c_extent_count,
				    (unsigned long)BC_cache->c_extent_tail->ce_offset,
				    (unsigned long)BC_cache->c_extent_tail->ce_length,
				    BC_cache->c_extent_tail->ce_data);
				debug("current buf:");
				if (BC_private_bp) {
				  debug(" blkno %qd  bcount %d  resid %d  flags 0x%x  dataptr %p",
				    buf_blkno(BC_private_bp),
				    buf_count(BC_private_bp),
				    buf_resid(BC_private_bp),
				    buf_flags(BC_private_bp),
				    (void *) buf_dataptr(BC_private_bp));
				} else
				  debug("NULL pointer");

			}
#ifdef DEBUG
 			Debugger("I/O Kit wedged on us");
#endif
			/*
			 * It might be nice to free all the pages that
			 * aren't actually referenced by the outstanding
			 * region, since otherwise we may be camped on a
			 * very large amount of physical memory.
			 *
			 * Ideally though, this will never happen.
			 */
			return(EBUSY);	/* really EWEDGED */
		}
	}

	return(0);
}

/*
 * Terminate the cache.
 *
 * This prevents any further requests from being satisfied from the cache
 * and releases all the resources owned by it.
 */
static int
BC_terminate_cache(void)
{
	int retry, i;
	
	/* can't shut down if readahead is still active */
	if (BC_cache->c_flags & BC_FLAG_IOBUSY) {
		debug("cannot terminate cache while readahead is in progress");
		return(EBUSY);
	}
	
	/* can't shut down if we're not active */
	if (!(BC_cache->c_flags & BC_FLAG_CACHEACTIVE)) {
		debug("cannot terminate cache when not active");
		return(ENXIO);
	}
	BC_cache->c_flags &= ~BC_FLAG_CACHEACTIVE;
	debug("terminating cache...");

	/*
	 * Mark all extents as FREED. This will notify any sleepers in the 
	 * strategy routine that the extent won't have data for them.
	 */
	for (i = 0; i < BC_cache->c_extent_count; i++) {
		LOCK_EXTENT(BC_cache->c_extents + i);
		BC_cache->c_extents[i].ce_flags |= CE_ABORTED;
		UNLOCK_EXTENT(BC_cache->c_extents + i);
		wakeup(BC_cache->c_extents + i);
	}

	/*
	 * It is possible that one or more callers are asleep in the
	 * strategy routine (eg. if readahead was terminated early,
	 * or if we are called off the timeout).
	 * Check the count of callers in the strategy code, and sleep
	 * until there are none left (or we time out here).  Note that
	 * by clearing BC_FLAG_CACHEACTIVE above we prevent any new
	 * strategy callers from touching the cache, so the count
	 * must eventually drain to zero.
	 */
	retry = 0;
	while (BC_cache->c_strategycalls > 0) {
		tsleep(BC_cache, PRIBIO, "BC_terminate_cache", hz / 10);
		if (retry++ > 50) {
			debug("could not terminate cache, timed out with %d caller%s in BC_strategy",
			    BC_cache->c_strategycalls,
			    BC_cache->c_strategycalls == 1 ? "" : "s");

			/* mark active again, since we failed */
			BC_cache->c_flags |= BC_FLAG_CACHEACTIVE;

			return(EBUSY);	/* again really EWEDGED */
		}
	}
	
	/*
	 * Check the block and page maps and report what's left.
	 * Release any pages that we still hold.
	 */
	BC_cache->c_stats.ss_spurious_blocks = 0;
	for (i = 0; i < BC_cache->c_buffer_blocks; i++)
		if (CB_BLOCK_PRESENT(BC_cache, i))
			BC_cache->c_stats.ss_spurious_blocks++;
	BC_cache->c_stats.ss_spurious_pages = 0;
	for (i = 0; i < BC_cache->c_buffer_pages; i++)
		if (CB_PAGE_PRESENT(BC_cache, i)) {
			BC_cache->c_stats.ss_spurious_pages++;

			BC_free_page(i);
		}
	BC_free_pagebuffer();
	
	/* free memory held by extents, the pagemap and the blockmap */
	_FREE_ZERO(BC_cache->c_extents, M_TEMP);
	_FREE_ZERO(BC_cache->c_pagemap, M_TEMP);
	_FREE_ZERO(BC_cache->c_iopagemap, M_TEMP);
	_FREE_ZERO(BC_cache->c_blockmap, M_TEMP);

	return(0);
}

/*
 * Terminate history recording.
 *
 * This stops us recording any further history events, as well
 * as disabling the cache.
 */
static int
BC_terminate_history(void)
{
	int retry;

	debug("terminating history collection...");
	/* disconnect the strategy routine */
	if ((BC_cache->c_devsw != NULL) &&
	    (BC_cache->c_strategy != NULL))
		BC_cache->c_devsw->d_strategy = BC_cache->c_strategy;

	/*
	 * It is possible that one or more callers are asleep in the
	 * real strategy routine (as it may drop the funnel).  Check
	 * the count of callers in the bypass code, and sleep until
	 * there are none left (or we time out here).  Note that by
	 * removing ourselves from the bdev switch above we prevent
	 * any new callers from entering the bypass code, so the count
	 * must eventually drain to zero.
	 */
	retry = 0;
	while (BC_cache->c_bypasscalls > 0) {
		tsleep(BC_cache, PRIBIO, "BC_terminate_history", hz / 10);
		if (retry++ > 50) {
			debug("could not terminate history, timed out with %d caller%s in BC_strategy_bypass",
			    BC_cache->c_bypasscalls,
			    BC_cache->c_bypasscalls == 1 ? "" : "s");

			return(EBUSY);	/* again really EWEDGED */
		}
	}

	/* record stop time */
	microtime(&BC_cache->c_stats.ss_cache_stop);
	
	return(0);
}

/*
 * Fetch the playlist from userspace or the Mach-O segment where it
 * was preloaded.  Note that in the STATIC_PLAYLIST case the playlist
 * is already in memory.
 */
static int
BC_copyin_playlist(size_t length, void *uptr)
{
	struct BC_playlist_entry *pce;
	struct BC_cache_extent *ce;
	int error, idx;
	caddr_t p;
	u_int64_t size;
	int entries;
	int actual;

	error = 0;
	pce = NULL;
	
	/* allocate playlist storage */
	entries = length / sizeof(*pce);
	BC_cache->c_extents = _MALLOC(entries * sizeof(struct BC_cache_extent),
	    M_TEMP, M_WAITOK);
	if (BC_cache->c_extents == NULL) {
		message("can't allocate memory for extents");
		error = ENOMEM;
		goto out;
	}
	BC_cache->c_extent_count = entries;
	BC_cache->c_stats.ss_total_extents = entries;
	ce = BC_cache->c_extents;
	size = 0;

	if (BC_preloaded_playlist) {
		/*
		 * Unpack the static control entry array into the extent array.
		 */
		debug("using static playlist with %d entries", entries);
		pce = (struct BC_playlist_entry *)uptr;
		for (idx = 0; idx < entries; idx++) {
			lck_mtx_init(&ce[idx].ce_lock, BC_cache->c_lckgrp,
					LCK_ATTR_NULL);
			ce[idx].ce_offset = pce[idx].pce_offset;
			ce[idx].ce_length = pce[idx].pce_length;
			ce[idx].ce_flags = pce[idx].pce_batch & CE_BATCH_MASK;
#ifdef IGNORE_BATCH
			ce[idx].ce_flags &= ~CE_BATCH_MASK;
#endif
			ce[idx].ce_data = NULL;
			size += pce[idx].pce_length;	/* track total size */

			/* track highest batch number for this playlist */
			if (ce[idx].ce_flags > BC_cache->c_batch_count) {
				BC_cache->c_batch_count = ce[idx].ce_flags;
			}
		}
	} else {
		/*
		 * Since the playlist control entry structure is not the same as the
		 * extent structure we use, we need to copy the control entries in
		 * and unpack them.
		 */
		debug("using supplied playlist with %d entries", entries);
		pce = _MALLOC(BC_PLC_CHUNK * sizeof(struct BC_playlist_entry),
			      M_TEMP, M_WAITOK);
		if (pce == NULL) {
			message("can't allocate unpack buffer");
			goto out;
		}
		while (entries > 0) {

			actual = min(entries, BC_PLC_CHUNK);
			if ((error = copyin(CAST_USER_ADDR_T(uptr), pce,
					    actual * sizeof(struct BC_playlist_entry))) != 0) {
				message("copyin from %p to %p failed", uptr, pce);
				_FREE(pce, M_TEMP);
				goto out;
			}

			/* unpack into our array */
			for (idx = 0; idx < actual; idx++) {
				lck_mtx_init(&ce[idx].ce_lock, BC_cache->c_lckgrp,
						LCK_ATTR_NULL);
				ce[idx].ce_offset = pce[idx].pce_offset;
				ce[idx].ce_length = pce[idx].pce_length;
				ce[idx].ce_flags = pce[idx].pce_batch & CE_BATCH_MASK;
#ifdef IGNORE_BATCH
				ce[idx].ce_flags &= ~CE_BATCH_MASK;
#endif
				ce[idx].ce_data = NULL;
				size += pce[idx].pce_length;	/* track total size */
				/* track highest batch number for this playlist */
				if (ce[idx].ce_flags > BC_cache->c_batch_count) {
					BC_cache->c_batch_count = ce[idx].ce_flags;
				}
			}
			entries -= actual;
			uptr = (struct BC_playlist_entry *)uptr + actual;
			ce += actual;
		}
		_FREE(pce, M_TEMP);
	}	

	/*
	 * In order to simplify allocation of the block and page maps, we round
	 * the size up to a multiple of CB_MAPFIELDBITS pages.  This means we
	 * may waste as many as 31 pages in the worst case.
	 */
	size = roundup(size, PAGE_SIZE * CB_MAPFIELDBITS);

	/*
	 * Verify that the cache is not larger than 50% of physical memory
	 * to avoid forcing pageout activity.
	 *
	 * mem_size and size are in bytes.  All calculations are done in page
	 * units to avoid overflow/sign issues.
	 *
	 * If the cache is too large, we discard it and go to record-only mode.
	 * This lets us survive the case where the playlist is corrupted or
	 * manually made too large, but the "real" bootstrap footprint
	 * would be OK.
	 */
	if (size > (mem_size / 2)) {
		message("cache size (%lu bytes) too large for physical memory (%u bytes)",
		    (unsigned long)size, mem_size);

		/* undo everything we've done so far */
		_FREE_ZERO(BC_cache->c_extents, M_TEMP);
		BC_cache->c_extents = NULL;
		BC_cache->c_extent_count = 0;
		BC_cache->c_stats.ss_total_extents = 0;

		return(0);	/* since this isn't really an error */
	}
	
	/*
	 * Allocate memory for the buffer and page maps.
	 */
	BC_cache->c_buffer_blocks = CB_BYTE_TO_BLOCK(BC_cache, size);
	BC_cache->c_buffer_pages = CB_BLOCK_TO_PAGE(BC_cache, BC_cache->c_buffer_blocks);
	if ((BC_cache->c_blockmap =
		_MALLOC(BC_cache->c_buffer_blocks / (CB_MAPFIELDBITS / CB_MAPFIELDBYTES),
		    M_TEMP, M_WAITOK)) == NULL) {
		message("can't allocate %d bytes for blockmap",
		    BC_cache->c_buffer_blocks / CB_MAPFIELDBYTES);
		error = ENOMEM;
		goto out;
	}
	if ((BC_cache->c_pagemap =
		_MALLOC(BC_cache->c_buffer_pages / (CB_MAPFIELDBITS / CB_MAPFIELDBYTES),
		    M_TEMP, M_WAITOK)) == NULL) {
		message("can't allocate %d bytes for pagemap",
		    BC_cache->c_buffer_pages / CB_MAPFIELDBYTES);
		error = ENOMEM;
		goto out;
	}

	if ((BC_cache->c_iopagemap =
		_MALLOC(BC_cache->c_buffer_pages / (CB_MAPFIELDBITS / CB_MAPFIELDBYTES),
		    M_TEMP, M_WAITOK)) == NULL) {
		message("can't allocate %d bytes for iopagemap",
		    BC_cache->c_buffer_pages / CB_MAPFIELDBYTES);
		error = ENOMEM;
		goto out;
	}
	bzero(BC_cache->c_iopagemap, BC_cache->c_buffer_pages / (CB_MAPFIELDBITS / CB_MAPFIELDBYTES));

	/*
	 * Allocate the pagebuffer.
	 */
	if (BC_alloc_pagebuffer(BC_cache->c_buffer_pages * PAGE_SIZE) != 0) {
		message("can't allocate %d bytes for cache buffer",
		    BC_cache->c_buffer_pages * PAGE_SIZE);
		error = ENOMEM;
		goto out;
	}

	/*
	 * Note that we have to mark both pages and blocks present.
	 * If we fail to mark blocks present, we encounter a race where
	 * an incoming read that has been blocked against a readahead
	 * will wake up and complete, discarding its blocks and then freeing
	 * a page it shares with the following extent for which reahead is
	 * still in progress.
	 */
	for (idx = 0; idx < BC_cache->c_buffer_pages; idx++)
		CB_MARK_PAGE_PRESENT(BC_cache, idx);
	for (idx = 0; idx < BC_cache->c_buffer_blocks; idx++)
		CB_MARK_BLOCK_PRESENT(BC_cache, idx);

	/*
	 * Fix up the extent data pointers.
	 */
	p = BC_cache->c_buffer;
	for (idx = 0; idx < BC_cache->c_extent_count; idx++) {
		(BC_cache->c_extents + idx)->ce_data = p;
		p += BC_cache->c_extents[idx].ce_length;
	}

	/* all done */
	out:
	if (error != 0) {
		debug("cache setup failed, aborting");
		if (BC_cache->c_buffer != NULL)
			BC_free_pagebuffer();
		_FREE_ZERO(BC_cache->c_blockmap, M_TEMP);
		_FREE_ZERO(BC_cache->c_pagemap, M_TEMP);
		_FREE_ZERO(BC_cache->c_iopagemap, M_TEMP);
		_FREE_ZERO(BC_cache->c_extents, M_TEMP);
		BC_cache->c_extent_count = 0;
	}
	return(error);
}

/*
 * Initialise the cache.  Fetch the playlist from userspace and
 * find and attach to our block device.
 */
static int
BC_init_cache(size_t length, caddr_t uptr, u_int64_t blocksize)
{
	u_int32_t blksize;
	u_int64_t blkcount;
	int error;
	unsigned int boot_arg;
	boolean_t funnel_state;
	thread_t rthread;

	error = 0;

	/*
	 * Check that we're not already running.
	 * This is safe first time around because the BSS is guaranteed
	 * to be zeroed.  Must be fixed if we move to dynamically
	 * allocating the cache softc.
	 */
	if (BC_cache->c_flags != 0)
		return(EBUSY);
	
	/* clear the control structure */
	bzero(BC_cache, sizeof(*BC_cache));

	/*
	 * If we have less than the minimum supported physical memory,
	 * bail immediately.  With too little memory, the cache will
	 * become polluted with nondeterministic pagein activity, leaving
	 * large amounts of wired memory around which further exacerbates
	 * the problem.
	 */
	if ((mem_size / (1024 * 1024)) < BC_minimum_mem_size) {
		debug("insufficient physical memory (%dMB < %dMB required)",
		    (int)(mem_size / (1024 * 1024)), BC_minimum_mem_size);
		return(ENOMEM);
	}
	
	/*
	 * Find our block device.
	 *
	 * Note that in the netbooted case, we are loaded but should not
	 * actually run, unless we're explicitly overridden.
	 *
	 * Note also that if the override is set, we'd better have a valid
	 * rootdev...
	 */
	if (netboot_root() && 
	    (!PE_parse_boot_arg("BootCacheOverride", &boot_arg) ||
	     (boot_arg != 1)))                          /* not interesting */
		return(ENXIO);
	BC_cache->c_dev = rootdev;
	BC_cache->c_devvp = rootvp;
	BC_cache->c_devsw = &bdevsw[major(rootdev)];
	assert(BC_cache->c_devsw != NULL);
	BC_cache->c_strategy = BC_cache->c_devsw->d_strategy;
	if (BC_cache->c_strategy == NULL)		/* not configured */
		return(ENXIO);

	BC_cache->c_devsw->d_ioctl(BC_cache->c_dev,	/* device */
#warning SHOULD USE DKIOCGETBLOCKSIZE64
	    DKIOCGETBLOCKSIZE,				/* cmd */
	    (caddr_t)&blksize,				/* data */
	    0,						/* fflag */
	    NULL);					/* proc XXX NULL? */
	if (blksize == 0) {
		message("can't determine device block size, defaulting to 512 bytes");
		blksize = 512;
	}
	BC_cache->c_devsw->d_ioctl(BC_cache->c_dev,	/* device */
	    DKIOCGETBLOCKCOUNT,				/* cmd */
	    (caddr_t)&blkcount,				/* data */
	    0,						/* fflag */
	    NULL);					/* proc XXX NULL? */
	if (blkcount == 0) {
		message("can't determine device size, not checking");
	}
	BC_cache->c_blocksize = blksize;
	BC_cache->c_devsize = BC_cache->c_blocksize * blkcount;
	BC_cache->c_stats.ss_blocksize = BC_cache->c_blocksize;
	debug("blocksize %lu bytes, filesystem %lu bytes",
	    (unsigned long)BC_cache->c_blocksize, (unsigned long)BC_cache->c_devsize);
	BC_cache->c_lckgrp = lck_grp_alloc_init("BootCache", LCK_GRP_ATTR_NULL);

	/*
	 * If we have playlist data, fetch it.
	 */
	if (length > 0) {
		if (BC_cache->c_blocksize > blocksize) {
			message("playlist blocksize %lu not compatible with root device's %lu",
			    (unsigned long)blocksize, (unsigned long)BC_cache->c_blocksize);
			return(EINVAL);
		}
		if ((error = BC_copyin_playlist(length, uptr)) != 0) {
			message("can't load playlist");
			return(error);
		}
	} else {
		debug("no playlist, recording only");
	}

	microtime(&BC_cache->c_stats.ss_batch_time[0]);
#ifndef NO_HISTORY
# ifdef STATIC_HISTORY
	/* initialise the history buffer */
	if ((BC_cache->c_history = _MALLOC(BC_HISTORY_ALLOC, M_TEMP, M_WAITOK)) == NULL) {
		message("can't allocate %d bytes for static history buffer", BC_HISTORY_ALLOC);
		return(ENOMEM);
	}
	bzero(BC_cache->c_history, BC_HISTORY_ALLOC);
	BC_cache->c_stats.ss_history_clusters = 1;
# endif
#endif
#ifdef READ_HISTORY_BUFFER
	/* initialise the read history buffer */
	if ((BC_cache->c_rhistory = _MALLOC(READ_HISTORY_BUFFER * sizeof(struct BC_read_history_entry),
	     M_TEMP, M_WAITOK)) == NULL) {
		message("can't allocate %d bytes for read history buffer",
		    READ_HISTORY_BUFFER * sizeof(struct BC_read_history_entry));
		return(ENOMEM);
	}
	bzero(BC_cache->c_rhistory, READ_HISTORY_BUFFER * sizeof(struct BC_read_history_entry));
#endif


	/* we run under the kernel funnel */
	funnel_state = thread_funnel_set(kernel_flock, TRUE);

#ifdef EMULATE_ONLY
	/* we are emulating the cache but not doing reads */
	BC_cache->c_extent_tail = BC_cache->c_extents + BC_cache->c_extent_count;
	/* XXX need to fill the block map */
	
	debug("emulated complete cache fill");
#else
	/*
	 * Start the reader thread.
	 */
	if (BC_cache->c_extents != NULL) {
		debug("starting readahead");
		BC_cache->c_flags |= BC_FLAG_IOBUSY;
		BC_cache->c_batch = 0;
		kernel_thread_start(BC_reader_thread, NULL, &rthread);
		thread_deallocate(rthread);
	}
#endif

	/*
	 * Take over the strategy routine for our device; we are now
	 * recording read operations and filling them from the cache.
	 */
	BC_cache->c_devsw->d_strategy = BC_strategy;

	/* cache is now active */
	BC_cache->c_flags |= BC_FLAG_CACHEACTIVE;

	(void) thread_funnel_set(kernel_flock, funnel_state);

	return(0);
}

/*
 * Timeout on collection and caching.
 *
 * We haven't heard from the userland component for too long;
 * release all the cache and extent memory, but keep our history
 * around in case they show up.
 */
static void
BC_timeout_cache(void *junk)
{
	boolean_t       funnel_state;

	funnel_state = thread_funnel_set(kernel_flock, TRUE);

	/* shut the cache down */
	BC_terminate_readahead();
	BC_terminate_cache();
	BC_terminate_history();

	(void) thread_funnel_set(kernel_flock, funnel_state);
}

/*
 * Record an incoming read in the history list.
 *
 * Note that this function is not reentrant.
 */
static void
BC_add_history(u_int64_t offset, u_int64_t length, int flags)
{
	struct BC_history_cluster *hc;
	struct BC_history_entry *he;
	kern_return_t kret;

	/* don't do anything if the history list has been truncated */
	if (BC_cache->c_flags & BC_FLAG_HTRUNCATED)
		return;

	/*
	 * In order to avoid recording a playlist larger than we will
	 * allow to be played back, if the total byte count for recorded
	 * reads reaches 50% of the physical memory in the system, we
	 * start ignoring reads.
	 * This is preferable to dropping entries from the end of the
	 * playlist as we copy it in, since it will result in a clean
	 * cessation of caching at a single point, rather than
	 * introducing sporadic cache misses throughout the cache's run.
	 */
	if ((BC_cache->c_history_bytes + length) > (mem_size / 2))
		return;
	BC_cache->c_history_bytes += length;
	
#ifdef NO_HISTORY
	BC_cache->c_flags |= BC_FLAG_HTRUNCATED;
	debug("history disabled, truncating");
	return;
#endif
	
	/*
	 * Get the current cluster.
	 */
	hc = BC_cache->c_history;
	if ((hc == NULL) || (hc->hc_entries >= BC_HISTORY_ENTRIES)) {
		BC_cache->c_stats.ss_history_clusters++;

		/* limit the number of clusters we will allocate */
		if (BC_cache->c_stats.ss_history_clusters >= BC_HISTORY_MAXCLUSTERS) {
			message("too many history clusters (%d, limit %ld)",
			    BC_cache->c_stats.ss_history_clusters,
			    (long)BC_HISTORY_MAXCLUSTERS);
			BC_cache->c_flags |= BC_FLAG_HTRUNCATED;
			BC_cache->c_stats.ss_history_clusters--;
			return;
		}
		kret = kmem_alloc(kernel_map, (vm_offset_t *)&hc, BC_HISTORY_ALLOC);
		if (kret != KERN_SUCCESS) {
			message("could not allocate %d bytes for history cluster",
			    BC_HISTORY_ALLOC);
			BC_cache->c_flags |= BC_FLAG_HTRUNCATED;
			return;
		}
		hc->hc_entries = 0;
		hc->hc_link = BC_cache->c_history;
		BC_cache->c_history = hc;
	}

	/*
	 * Find the next free entry and update it.
	 */
	he = &(hc->hc_data[hc->hc_entries]);
	assert(he >= &hc->hc_data[0]);
	assert(he < &hc->hc_data[BC_HISTORY_ENTRIES]);
	he->he_offset = offset;
	he->he_length = length;
	he->he_flags = flags;
	hc->hc_entries++;
}

/*
 * Return the size of the history buffer.
 */
static size_t
BC_size_history(void)
{
	struct BC_history_cluster *hc;
	int nentries;

	/* walk the list of clusters and count the number of entries */
	nentries = 0;
	for (hc = BC_cache->c_history; hc != NULL; hc = hc->hc_link)
		nentries += hc->hc_entries;

	return(nentries * sizeof(struct BC_history_entry));
}

/*
 * Copy out either the history buffer size, or the entire history buffer.
 *
 * Note that the cache must be shut down before calling this function in
 * order to avoid a race around the entry count/buffer size.
 */
static int
BC_copyout_history(void *uptr)
{
	struct BC_history_cluster *hc, *ohc;
	int error, nentries;

	assert(uptr != NULL);

	/* walk the list of clusters and count the number of entries */
	nentries = 0;
	for (hc = BC_cache->c_history; hc != NULL; hc = hc->hc_link)
		nentries += hc->hc_entries;

	/*
	 * Copying the history out is a little messy due to the fact that the
	 * clusters are on the list backwards, but we want the entries in order.
	 */
	ohc = NULL;
	for (;;) {
		/* scan the list until we find the last one we haven't copied */
		hc = BC_cache->c_history;
		while (hc->hc_link != ohc)
			hc = hc->hc_link;
		ohc = hc;

		/* copy the cluster out */
		if ((error = copyout(hc->hc_data, CAST_USER_ADDR_T(uptr),
			 hc->hc_entries * sizeof(struct BC_history_entry))) != 0)
			return(error);
		uptr = (caddr_t)uptr + hc->hc_entries * sizeof(struct BC_history_entry);

		/* if this was the last cluster, all done */
		if (hc == BC_cache->c_history)
			break;
	}
        return(0);
}

/*
 * Discard the history list.
 */
static void
BC_discard_history(void)
{
	struct BC_history_cluster *hc;

	while (BC_cache->c_history != NULL) {
		hc = BC_cache->c_history;
		BC_cache->c_history = hc->hc_link;
		kmem_free(kernel_map, (vm_offset_t)hc, BC_HISTORY_ALLOC);
	}
}

/*
 * Called from the root-mount hook.
 */
static void
BC_auto_start(void)
{
	int error;

	/* initialise the cache */
#ifdef STATIC_PLAYLIST
	error = BC_init_cache(sizeof(BC_data), (caddr_t)&BC_data[0], BC_playlist_blocksize);
#else /* !STATIC_PLAYLIST */
	error = BC_init_cache(BC_preloaded_playlist->ph_entries * sizeof(struct BC_playlist_entry), 
			      (caddr_t)BC_preloaded_playlist + sizeof(struct BC_playlist_header), 
			      BC_preloaded_playlist->ph_blocksize);
#endif /* STATIC_PLAYLIST */
	if (error != 0)
		debug("BootCache autostart failed: %d", error);

	/* set 'started' flag (cleared when we get history) */
	BC_cache->c_flags |= BC_FLAG_STARTED;
}


/*
 * Handle the command sysctl.
 */
static int
BC_sysctl SYSCTL_HANDLER_ARGS
{
	struct BC_command bc;
	boolean_t funnel_state;
	int error;

	/* we run under the kernel funnel */
	funnel_state = thread_funnel_set(kernel_flock, TRUE);

	/* get the commande structure and validate */
	if ((error = SYSCTL_IN(req, &bc, sizeof(bc))) != 0) {
		debug("couldn't get command");
		(void) thread_funnel_set(kernel_flock, funnel_state);
		return(error);
	}
	if (bc.bc_magic != BC_MAGIC) {
		debug("bad command magic");
		(void) thread_funnel_set(kernel_flock, funnel_state);
		return(EINVAL);
	}

	switch (bc.bc_opcode) {
#ifndef STATIC_PLAYLIST
	case BC_OP_START:
		if (BC_preloaded_playlist) {
			error = EINVAL;
			break;
		}
		debug("BC_OP_START(%ld)", bc.bc_length);
		error = BC_init_cache(bc.bc_length, bc.bc_data, (u_int64_t)bc.bc_param);
		if (error != 0) {
			message("cache init failed");
		} else {
			/* start the timeout handler */
			timeout(BC_timeout_cache, NULL, hz * BC_cache_timeout);

			/* set 'started' flag (cleared when we get history) */
			BC_cache->c_flags |= BC_FLAG_STARTED;
		}
		break;
#endif /* STATIC_PLAYLIST */

	case BC_OP_STOP:
		debug("BC_OP_STOP");

		/*
		 * Kill the timeout handler; we don't want it going off
		 * now.
		 */
		untimeout(BC_timeout_cache, NULL);
		
		/*
		 * If the cache is running, stop it.  If it's already stopped
		 * (it may have stopped itself), that's OK.
		 */
		BC_terminate_readahead();
		BC_terminate_cache();
		BC_terminate_history();

		/* return the size of the history buffer */
		if (BC_cache->c_flags & BC_FLAG_HTRUNCATED) {
			bc.bc_length = 0;
		} else {
			bc.bc_length = BC_size_history();
		}
		if ((error = SYSCTL_OUT(req, &bc, sizeof(bc))) != 0)
			debug("could not return history size");
		break;
		
	case BC_OP_HISTORY:
		debug("BC_OP_HISTORY");
		/* if there's a user buffer, verify the size and copy out */
		if (bc.bc_data != NULL) {
			if (bc.bc_length < BC_size_history()) {
				debug("supplied history buffer too small");
				error = ENOMEM;
				break;
			}
			if ((error = BC_copyout_history(bc.bc_data)) != 0)
				debug("could not copy out history");
			
		}
		/*
		 * Release the last of the memory we own.
		 */
		BC_discard_history();
		BC_cache->c_flags = 0;
		break;
				    
	case BC_OP_STATS:
		debug("BC_OP_STATS");
		/* check buffer size and copy out */
		if (bc.bc_length != sizeof(BC_cache->c_stats)) {
			debug("stats structure wrong size");
			error = ENOMEM;
		} else {
			BC_cache->c_stats.ss_cache_flags = BC_cache->c_flags;
			if ((error = copyout(&BC_cache->c_stats, CAST_USER_ADDR_T(bc.bc_data), bc.bc_length)) != 0)
				debug("could not copy out statistics");
		}
		break;

	case BC_OP_TAG:
		debug("BC_OP_TAG");
		BC_add_history((u_int64_t)bc.bc_param, 0, BC_HE_TAG);
		break;
		
	default:
		error = EINVAL;
	}
	(void) thread_funnel_set(kernel_flock, funnel_state);
	return(error);
}


/*
 * Initialise the block of pages we use to back our data.
 *
 */
static int
BC_alloc_pagebuffer(size_t size)
{
	kern_return_t kret;
	vm_object_offset_t s;
	int i;

	BC_cache->c_mapsize = size;

	/*
	 * Create a VM region object (our private map).
	 */
	kret = vm_region_object_create(kernel_map,
	    BC_cache->c_mapsize,
	    &BC_cache->c_map_port);
	if (kret != KERN_SUCCESS) {
		debug("vm_region_object_create failed - %d", kret);
		return(ENOMEM);
	}
	BC_cache->c_map = convert_port_entry_to_map(BC_cache->c_map_port);

	/*
	 * Allocate and wire pages into our submap.
	 */
	BC_cache->c_mapbase = 0;
	kret = vm_allocate(BC_cache->c_map,	/* map to allocate in */
	    &BC_cache->c_mapbase,		/* offset in map */
	    BC_cache->c_mapsize,		/* size in bytes */
	    FALSE);				/* allocate_anywhere */
	if (kret != KERN_SUCCESS) {
		debug("vm_allocate failed - %d", kret);
		return(ENOMEM);
	}
	s = (vm_object_offset_t)size;
	kret = mach_make_memory_entry_64(
		BC_cache->c_map,		/* map */
		&s,				/* size */
		0,				/* offset */
		VM_PROT_READ | VM_PROT_WRITE,	/* default memory protection */
		&BC_cache->c_entry_port,	/* return handle */
		NULL);				/* parent */
	if ((kret != KERN_SUCCESS) || (s != size)) {
		debug("mach_make_memory_entry failed - %d", kret);
		return(ENOMEM);
	}
	kret = vm_protect(BC_cache->c_map,	/* map */
	    BC_cache->c_mapbase,		/* offset */
	    BC_cache->c_mapsize,		/* length */
	    TRUE,				/* set maximum protection */
	    VM_PROT_READ | VM_PROT_WRITE);	/* default memory protection */
	if ((kret != KERN_SUCCESS) || (s != size)) {
		debug("vm_protect failed - %d", kret);
		return(ENOMEM);
	}
	/*
	 * Map our private map into the kernel map as a submap.
	 */
	BC_cache->c_buffer = NULL;		/* start at the bottom of memory */
	kret = vm_map(kernel_map,		/* parent map */
	    (vm_offset_t *)&BC_cache->c_buffer,	/* mapped address */
	    BC_cache->c_mapsize,		/* mapping size */
	    0,					/* mask */
	    1,					/* flags (want VM_MAP_ANYWHERE) */
	    BC_cache->c_map_port,		/* port */
	    BC_cache->c_mapbase,		/* offset in child map */
	    FALSE,				/* copy */
	    VM_PROT_READ | VM_PROT_WRITE,	/* default memory protection */
	    VM_PROT_READ | VM_PROT_WRITE,	/* maximum memory protection */
	    VM_INHERIT_NONE);			/* inheritance */
	if (kret != KERN_SUCCESS) {
		debug("vm_map failed - %d", kret);
		return(ENOMEM);
	}
#ifdef WIRE_BUFFER
	/*
	 * Wire the pages into the kernel map.
	 */
	kret = vm_wire(host_priv_self(),	/* caller privilege */
	    kernel_map,				/* map within which to wire */
	    BC_cache->c_buffer,			/* offset to map from */
	    BC_cache->c_mapsize,		/* size of region to map */
	    VM_PROT_READ | VM_PROT_WRITE);	/* default memory protection */
	if ((kret != KERN_SUCCESS) || (s != size)) {
		debug("vm_wire failed - %d", kret);
		return(ENOMEM);
	}
#endif /* WIRE_BUFFER */

#ifdef __ppc__
	/*
	 * Tell the pmap that we are about to touch a *lot* of new memory.
	 * This is a hack, but the current pmap demands it.
	 */
	mapping_prealloc(BC_cache->c_mapsize);
#endif
	
	/*
	 * Touch every page in the buffer to get it into the pmap.  Some
	 * device drivers will use the pmap to perform virtual-physical
	 * translation and fail if we do not do this.
	 */
	for (i = 0; i < BC_cache->c_mapsize; i += PAGE_SIZE)
		*(char *)(BC_cache->c_buffer + i) = 0;

#ifdef __ppc__
	mapping_relpre();
#endif
	
	return(0);
}

/*
 * Release all resources associated with our pagebuffer.
 */
static void
BC_free_pagebuffer(void)
{

	if (BC_cache->c_buffer != NULL) {
		/*
		 * Remove our submap from the kernel map.
		 */
		vm_deallocate(kernel_map,		/* parent map */
		    (vm_address_t)BC_cache->c_buffer,	/* virtual offset */
		    BC_cache->c_mapsize);		/* size */
		BC_cache->c_buffer = NULL;
		BC_cache->c_mapbase = 0;
		BC_cache->c_mapsize = 0;
	}
	if (BC_cache->c_map != 0) {
		vm_map_deallocate(BC_cache->c_map);	/* release reference on port */
		BC_cache->c_map = 0;
	}

	/*
	 * Kill our map by removing its name, nuke the object's name.
	 */
	if (BC_cache->c_entry_port != 0) {
		ipc_port_release_send(BC_cache->c_entry_port);
		BC_cache->c_entry_port = 0;
	}
	if (BC_cache->c_map_port != 0) {
		ipc_port_release_send(BC_cache->c_map_port);
		BC_cache->c_map_port = 0;
	}
}

/*
 * Release one page from the pagebuffer.
 */
static void
BC_free_page(int page)
{

	while (CB_IOPAGE_BUSY(BC_cache, page))
   		tsleep(&BC_cache->c_iopagemap, PRIBIO, "BC_free_page", hz / 10);

#ifdef WIRE_BUFFER
	/*
	 * Unwire the page.
	 */
	vm_wire(host_priv_self(),			/* caller privilege */
	    BC_cache->c_map,				/* map within which to wire */
	    BC_cache->c_mapbase + (page * PAGE_SIZE),	/* offset to map from */
	    PAGE_SIZE,					/* size of region to map */
	    VM_PROT_NONE);				/* requests unwiring */
#endif
	/*
	 * Deallocate the page from our submap.
	 */
	vm_deallocate(BC_cache->c_map,			/* child map */
	    BC_cache->c_mapbase + (page * PAGE_SIZE),	/* offset */
	    PAGE_SIZE);					/* length */

	/*
	 * Push the page completely out of the object.
	 */
	mach_memory_entry_page_op(
		BC_cache->c_entry_port,			/* handle */
		(page * PAGE_SIZE),			/* offset */
		UPL_POP_DUMP,				/* operation */
		NULL,					/* phys_entry */
		NULL);					/* flags */
}

/*
 * Cache start/stop functions.
 */
void
BC_start(void)
{
#ifndef STATIC_PLAYLIST
	struct mach_header	*mh;
	int			xsize;

	/* register our control interface */
	sysctl_register_oid(&sysctl__kern_BootCache);

	/*
	 * Find the preload section and its size.  If it's not empty
	 * we have a preloaded playlist, so prepare for an early
	 * start.
	 */
	if (kmod_info.info_version != KMOD_INFO_VERSION) {
		message("incompatible kmod_info versions");
		return;
	}
	mh = (struct mach_header *)kmod_info.address;
	BC_preloaded_playlist = getsectdatafromheader(mh, "BootCache", "playlist",
						      &BC_preloaded_playlist_size);
	debug("preload section: %d @ %p", 
	      BC_preloaded_playlist_size, BC_preloaded_playlist);
	if (BC_preloaded_playlist != NULL) {
		if (BC_preloaded_playlist_size < sizeof(struct BC_playlist_header)) {
			message("preloaded playlist too small");
			return;
		}
		if (BC_preloaded_playlist->ph_magic != PH_MAGIC) {
			message("preloaded playlist has invalid magic (%x, expected %x)",
				BC_preloaded_playlist->ph_magic, PH_MAGIC);
			return;
		}
		xsize = sizeof(struct BC_playlist_header) + 
			(BC_preloaded_playlist->ph_entries * sizeof(struct BC_playlist_entry));
		if (xsize > BC_preloaded_playlist_size) {
			message("preloaded playlist too small (%d, expected at least %d)",
				BC_preloaded_playlist_size, xsize);
			return;
		}
		BC_wait_for_readahead = BC_READAHEAD_WAIT_CDROM;
		mountroot_post_hook = BC_auto_start;

		debug("waiting for root mount...");
	} else {
		/* no preload, normal operation */
		BC_preloaded_playlist = NULL;
		debug("waiting for playlist...");
	}
#else /* STATIC_PLAYLIST */
	/* register our control interface */
	sysctl_register_oid(&sysctl__kern_BootCache);

	mountroot_post_hook = BC_auto_start;
	debug("waiting for root mount...");
#endif /* STATIC_PLAYLIST */
}

int
BC_stop(void)
{
	boolean_t funnel_state;
	int error;

	/* we run under the kernel funnel */
	error = 1;
	funnel_state = thread_funnel_set(kernel_flock, TRUE);

	debug("preparing to unload...");
	/*
	 * Kill the timeout handler; we don't want it going off
	 * now.
	 */
	untimeout(BC_timeout_cache, NULL);
		
	/*
	 * If the cache is running, stop it.  If it's already stopped
	 * (it may have stopped itself), that's OK.
	 */
	if (BC_terminate_readahead())
		goto out;
	BC_terminate_cache();
	BC_terminate_history();
	BC_discard_history();
	BC_cache->c_flags = 0;

	sysctl_unregister_oid(&sysctl__kern_BootCache);
	error = 0;

out:
	(void) thread_funnel_set(kernel_flock, funnel_state);
	return(error);
}
