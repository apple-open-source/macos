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
 * specified interval (in hz).
 *
 * User DVDs don't issue a "BootCacheControl stop", so they depend
 * on the cache timeout.
 */
static int BC_cache_timeout = 240 * 100;

/*
 * Trace macros
 */
#ifndef DBG_BOOTCACHE
#define DBG_BOOTCACHE	7
#endif

#define	DBG_BC_TAG	1
#define	DBG_BC_BATCH	2
static int dbg_tag_count = 0;

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

#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/kdebug.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/ubc.h>
#include <sys/uio.h>

#include <mach/kmod.h>
#include <mach/mach_types.h>
#include <mach/vm_param.h>	/* for max_mem */

#include <vm/vm_kern.h>

#include <libkern/libkern.h>
#include <libkern/OSAtomic.h>

#include <kern/assert.h>
#include <kern/host.h>
#include <kern/kalloc.h>
#include <kern/thread_call.h>

#include <IOKit/storage/IOMediaBSDClient.h>

#include <pexpert/pexpert.h>

#include "BootCache.h"


/*
 * A cache extent.
 *
 * Each extent represents a contiguous range of disk blocks which are
 * held in the cache.  All values in bytes.
 */
struct BC_cache_extent {
	lck_mtx_t	ce_lock;
	u_int64_t	ce_diskoffset;	/* physical offset on device */
	u_int64_t	ce_length;	/* data length */
	off_t		ce_cacheoffset;	/* offset into cache backing store */
	int		ce_flags;	/* low 8 bits mark the batch */ 
#define CE_ABORTED	(1 << 9)	/* extent no longer has data */
#define CE_IODONE	(1 << 10)	/* extent has been read */
	u_int32_t	*ce_blockmap;	/* track valid blocks for this extent */
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
	u_int64_t	c_cachesize;		/* total number of bytes in cache */
	u_int64_t	c_maxread;		/* largest read the dev will support */

	/*
	 * Each extent tracks the pages it owns in the buffer.
	 * It is assumed that PAGE_SIZE is a multiple of c_blocksize.
	 */
	struct BC_cache_extent *c_extents; /* extent list */
	int		c_extent_count;	/* total number of extents */
	int		c_batch_count;		/* number of extent batches */
	lck_grp_t	*c_lckgrp;
	vnode_t		c_vp;
	uint32_t	c_vid;

	/* history list, in reverse order */
	struct BC_history_cluster *c_history;
	u_int64_t	c_history_bytes; /* total read size in bytes */

	/* lock protects all the above fields; fields below are accessed atomically */
	lck_mtx_t	c_histlock;
	
	/* flags */
	UInt32		c_flags;
#define	BC_FLAG_SHUTDOWN	(1 << 0)	/* cache shutting down */
#define	BC_FLAG_CACHEACTIVE	(1 << 1)	/* cache is active, owns memory */
#define	BC_FLAG_HTRUNCATED	(1 << 2)	/* history list truncated */
#define	BC_FLAG_IOBUSY		(1 << 3)	/* readahead in progress */
#define	BC_FLAG_STARTED		(1 << 4)	/* cache started by user */
	SInt32		c_strategycalls;	/* count of busy strategy calls */

	/* statistics */
	struct BC_statistics c_stats;

	/* time-keeping */
	struct timeval c_starttime;
	struct timeval c_endtime;
	struct timeval c_loadtime;

#ifdef READ_HISTORY_BUFFER
	int		c_rhistory_idx;
	struct BC_read_history_entry *c_rhistory;
#endif
};

/*
 * The number of blocks per page; assumes that a page
 * will always be >= the size of a disk block.
 */
#define CB_PAGEBLOCKS			(PAGE_SIZE / BC_cache->c_blocksize)

/*
 * Convert block offset to page offset and vice versa.
 */
#define CB_BLOCK_TO_PAGE(block)		((block) / CB_PAGEBLOCKS)
#define CB_PAGE_TO_BLOCK(page)		((page) * CB_PAGEBLOCKS)
#define CB_BLOCK_TO_BYTE(block)		((block) * BC_cache->c_blocksize)
#define CB_BYTE_TO_BLOCK(byte)		((byte) / BC_cache->c_blocksize)

/*
 * The size of an addressable field in the block map.
 * This reflects the u_int32_t type for the blockmap type.
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
#define CB_BLOCK_PRESENT(ce, block) \
	((ce)->ce_blockmap[CB_MAPIDX(block)] &   CB_MAPBIT(block))
#define CB_MARK_BLOCK_PRESENT(ce, block) \
	((ce)->ce_blockmap[CB_MAPIDX(block)] |=  CB_MAPBIT(block))
#define CB_MARK_BLOCK_VACANT(ce, block) \
	((ce)->ce_blockmap[CB_MAPIDX(block)] &= ~CB_MAPBIT(block))

/*
 * Determine whether a given page is vacant (all blocks are gone).
 * This takes advantage of the expectation that a page's blocks
 * will never cross the boundary of a field in the blockmap.
 */
#define CB_PAGE_VACANT(ce, page)					\
	(!((ce)->ce_blockmap[CB_MAPIDX(CB_PAGE_TO_BLOCK(page))] &	\
	 (((1 << CB_PAGEBLOCKS) - 1) << (CB_PAGE_TO_BLOCK(page) %	\
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

/*
 * Synchronization macros
 */
#define LOCK_EXTENT(e)		lck_mtx_lock(&(e)->ce_lock)
#define UNLOCK_EXTENT(e)	lck_mtx_unlock(&(e)->ce_lock)
#define LOCK_HISTORY()		lck_mtx_lock(&BC_cache->c_histlock)
#define UNLOCK_HISTORY()	lck_mtx_unlock(&BC_cache->c_histlock)
#define BC_ADD_STAT(stat, inc)	OSAddAtomic((inc), ((SInt32 *)&BC_cache->c_stats.ss_##stat))

/*
 * Only one instance of the cache is supported.
 */
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
static int	BC_blocks_present(struct BC_cache_extent *ce, int base, int nblk);
static void	BC_reader_thread(void *param0, wait_result_t param1);
static void	BC_strategy_bypass(struct buf *bp);
static void	BC_strategy(struct buf *bp);
static void	BC_handle_write(struct buf *bp);
static int	BC_terminate_readahead(void);
static int	BC_terminate_cache(void);
static int	BC_terminate_history(void);
static void	BC_next_valid_range(struct BC_cache_extent *ce, uint32_t from, 
		uint32_t *nextpage, uint32_t *nextoffset, uint32_t *nextlength);
static u_int64_t BC_setup_extent(struct BC_cache_extent *ce, struct BC_playlist_entry *pce);
static void	BC_teardown_extent(struct BC_cache_extent *ce);
static int	BC_copyin_playlist(size_t length, user_addr_t uptr);
static int	BC_init_cache(size_t length, user_addr_t uptr, u_int64_t blocksize);
static void	BC_timeout_cache(void *);
static void	BC_add_history(u_int64_t offset, u_int64_t length, int flags);
static int	BC_size_history(void);
static int	BC_copyout_history(user_addr_t uptr);
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
extern void (* unmountroot_pre_hook)(void);

/*
 * Mach VM-specific functions.
 */
static int	BC_alloc_pagebuffer();
static void	BC_free_pagebuffer();
static void	BC_free_page(struct BC_cache_extent *ce, int page);

/* Functions not prototyped elsewhere */
int     	ubc_range_op(vnode_t, off_t, off_t, int, int *); /* in sys/ubc_internal.h */

/*
 * Mach-o functions.
 */
#define MACH_KERNEL 1
#include <mach-o/loader.h>
extern void *getsectdatafromheader(struct mach_header *, char *, char *, int *);

/*
 * Set or clear a flag atomically and return the previous value of that bit.
 */
static inline UInt32
BC_set_flag(UInt32 flag)
{
	return flag & OSBitOrAtomic(flag, &BC_cache->c_flags);
}

static inline UInt32
BC_clear_flag(UInt32 flag)
{
	return flag & OSBitAndAtomic(~(flag), &BC_cache->c_flags);
}

/*
 * Test whether a given byte range on disk intersects with an extent's range.
 */
static inline int
BC_check_intersection(struct BC_cache_extent *ce, u_int64_t offset, u_int64_t length)
{
	if ((offset < (ce->ce_diskoffset + ce->ce_length)) &&
			((offset + length) > ce->ce_diskoffset))
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
	if (((offset + length) <= base->ce_diskoffset) ||
	    (offset >= (last->ce_diskoffset + last->ce_length)))
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
			    ((offset < p->ce_diskoffset) ||
			     ((offset + length) > (p->ce_diskoffset + p->ce_length))))
				return(NULL);
			return(p);
		}
		
		if (offset > p->ce_diskoffset) {	/* move right */
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
	u_int64_t estart, eend, dstart, dend, nblk, base, page;
	int i, discarded;

	/* invariants */
	assert(length > 0);
#ifdef DEBUG
	lck_mtx_assert(&ce->ce_lock, LCK_MTX_ASSERT_OWNED);
#endif

	/*
	 * Extent has been terminated: blockmap may no longer be valid.
	 */
	if (ce->ce_flags & CE_ABORTED)
		return 0;
	
	/*
	 * Constrain offset and length to fit this extent, return immediately if
	 * there is no overlap.
	 */
	dstart = offset;			/* convert to start/end format */
	dend = offset + length;
	assert(dend > dstart);
	estart = ce->ce_diskoffset;
	eend = ce->ce_diskoffset + ce->ce_length;
	assert(eend > estart);

	if (dend <= estart)			/* check for overlap */
		return(0);
	if (eend <= dstart)
		return(0);

	if (dstart < estart)			/* clip to extent */
		dstart = estart;
	if (dend > eend)
		dend = eend;

	/*
	 * Convert length in bytes to a block count.
	 */
	nblk = CB_BYTE_TO_BLOCK(dend - dstart);
	
	/*
	 * Mark blocks vacant.  For each vacated block, check whether the page
	 * holding it can be freed.
	 *
	 * Note that this could be optimised to reduce the number of
	 * page-freeable checks, but the logic would be more complex.
	 */
	base = CB_BYTE_TO_BLOCK(dstart - ce->ce_diskoffset);
	assert(base >= 0);
	discarded = 0;
	for (i = 0; i < nblk; i++) {
		assert((base + i) < howmany(ce->ce_length, BC_cache->c_blocksize));

		/* this is a no-op if the block is already gone */
		if (CB_BLOCK_PRESENT(ce, base + i)) {

			/* mark the block as vacant */
			CB_MARK_BLOCK_VACANT(ce, base + i);
			discarded++;
			
			page = CB_BLOCK_TO_PAGE(base + i);
			if (CB_PAGE_VACANT(ce, page))
				BC_free_page(ce, (int) page);
		}
	}
	return(discarded);
}

/*
 * Blocks in an extent can be invalidated, e.g. by a write, before the
 * reader thread fills the extent. Search for the next range of viable blocks
 * in the extent, starting from offset 'fromoffset'.
 *
 * Values are returned by out parameters:
 * 	'nextpage' takes the page containing the start of the next run, or
 * 		-1 if no more runs are to be found.
 * 	'nextoffset' takes the offset into that page that the run begins.
 * 	'nextlength' takes the length in bytes of the range, capped at maxread.
 *
 * In other words, for this blockmap, if
 * 	    *fromoffset = 3 * blocksize,
 * 	1 1 0 0 0 1 1 1   1 1 1 1 1 1 0 0
 * 	*nextpage = 0
 * 	          *nextoffset = 5 * blocksize
 * 	                            *nextlength = 6 * blocksize
 */
static void 
BC_next_valid_range(struct BC_cache_extent *ce, uint32_t fromoffset, 
		uint32_t *nextpage, uint32_t *nextoffset, uint32_t *nextlength)
{
	int maxblks, i, nextblk = 0;
	int found = 0;

	maxblks = (int) howmany(ce->ce_length, BC_cache->c_blocksize);
	i = CB_BYTE_TO_BLOCK(fromoffset);
	/* scan for the next range of valid blocks in the extent */
	for (; i < maxblks; i++) {
		if (CB_BLOCK_PRESENT(ce, i)) {
			if (found == 0) {
				nextblk = i;
			}
			found++;
		} else {
			if (found != 0)
				break;
		}
	}

	if (found) {
		/* found a valid range, so convert to (page, offset, length) */
		*nextpage = CB_BLOCK_TO_PAGE(nextblk);
		*nextoffset = CB_BLOCK_TO_BYTE(nextblk) % PAGE_SIZE;
		*nextlength = MIN(CB_BLOCK_TO_BYTE(found), BC_cache->c_maxread);
	} else {
		*nextpage = -1;
	}

	return;
}

/*
 * Test for the presence of a range of blocks in the cache.
 */
static int
BC_blocks_present(struct BC_cache_extent *ce, int base, int nblk)
{
	int	blk;
	
	assert((base + nblk) < howmany(ce->ce_length, BC_cache->c_blocksize));

	for (blk = 0; blk < nblk; blk++) {
		
		if (!CB_BLOCK_PRESENT(ce, base + blk)) {
			BC_ADD_STAT(hit_blkmissing, 1);
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
	struct buf *bp;
	int count, batch;
	upl_t	upl;
	kern_return_t kret;
	struct timeval batchstart, batchend;
			  
	bp = buf_alloc(BC_cache->c_devvp);
	
	debug("reader thread started");

	for (batch = 0; batch <= BC_cache->c_batch_count; batch++) {
		debug("starting batch %d", batch);
		KERNEL_DEBUG_CONSTANT(FSDBG_CODE(DBG_BOOTCACHE, DBG_BC_BATCH) | 
				DBG_FUNC_START,
				batch, 0, 0, 0, 0);
		microtime(&batchstart);

		/* iterate over extents to populate */
		for (ce = BC_cache->c_extents;
			ce < (BC_cache->c_extents + BC_cache->c_extent_count);
			ce++) {

			LOCK_EXTENT(ce);

			/* requested shutdown */
			if (BC_cache->c_flags & BC_FLAG_SHUTDOWN)
				goto out;

			/* Only read extents marked for this batch. */
			if ((ce->ce_flags & CE_BATCH_MASK) != batch) {
				UNLOCK_EXTENT(ce);
				continue;
			}

			/* Check for early extent termination */
			if (ce->ce_flags & CE_ABORTED) {
				UNLOCK_EXTENT(ce);
				continue;
			}

			/* loop reading to fill this extent */
			buf_setcount(bp, 0);
			uint32_t fromoffset = 0;

			for (;;) {
				uint32_t nextpage;
				uint32_t nextoffset;
				uint32_t nextlength;

				/*
				 * Find the next set of blocks that haven't been invalidated
				 * for this extent.
				 */
				BC_next_valid_range(ce, fromoffset, &nextpage, &nextoffset, &nextlength);
				/* no more blocks to be read */
				if (nextpage == -1)
					break;

				/* set up fromoffset to read the next segment of the extent */
				fromoffset = (nextpage * PAGE_SIZE) + nextoffset + nextlength;

				kret = vnode_getwithvid(BC_cache->c_vp, BC_cache->c_vid);
				if (kret != KERN_SUCCESS) {
					debug("reader thread: vnode_getwithvid failed - %d\n", kret);
					goto out;
				}

				kret = ubc_create_upl(BC_cache->c_vp, 
						ce->ce_cacheoffset + (nextpage * PAGE_SIZE), 
						(int) roundup(nextoffset + nextlength, PAGE_SIZE), 
						&upl, 
						NULL, 
						UPL_SET_LITE|UPL_FILE_IO);
				if (kret != KERN_SUCCESS) {
					debug("ubc_create_upl returned %d\n", kret);
					(void) vnode_put(BC_cache->c_vp);
					goto out;
				}

				/* set buf to fill the requested subset of the upl */
				buf_setblkno(bp, CB_BYTE_TO_BLOCK(ce->ce_diskoffset + nextoffset) + CB_PAGE_TO_BLOCK(nextpage));
				buf_setcount(bp, (unsigned int) nextlength);
				buf_setupl(bp, upl, (unsigned int) nextoffset);
				buf_setresid(bp, buf_count(bp));	/* ask for residual indication */
				buf_reset(bp, B_READ);

				/* give the buf to the underlying strategy routine */
				KERNEL_DEBUG_CONSTANT(FSDBG_CODE(DBG_DKRW, DKIO_READ) | DBG_FUNC_NONE,
						(uintptr_t) bp, buf_device(bp), (int)buf_blkno(bp), buf_count(bp), 0);

				BC_ADD_STAT(initiated_reads, 1);
				BC_cache->c_strategy(bp);

				/* wait for the bio to complete */
				buf_biowait(bp);

				kret = ubc_upl_commit(upl);
				(void) vnode_put(BC_cache->c_vp);

				/*
				 * If the read or commit returned an error, invalidate the blocks
				 * covered by the read (on a residual, we could avoid invalidating
				 * blocks that are claimed to be read as a minor optimisation, but we do
				 * not expect errors as a matter of course).
				 */
				if (kret != KERN_SUCCESS || buf_error(bp) || (buf_resid(bp) != 0)) {
					debug("read error: extent %lu %lu/%lu "
							"(error buf %ld/%ld flags %08x resid %d)",
							(unsigned long) (ce - BC_cache->c_extents),
							(unsigned long)ce->ce_diskoffset, (unsigned long)ce->ce_length,
							(long)buf_blkno(bp), (long)buf_count(bp),
							buf_flags(bp), buf_resid(bp));

					count = BC_discard_blocks(ce, CB_BLOCK_TO_BYTE(buf_blkno(bp)),
							buf_count(bp));
					debug("read error: discarded %d blocks", count);
					BC_ADD_STAT(read_errors, 1);
					BC_ADD_STAT(error_discards, count);
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
			}

			/* update stats */
			BC_ADD_STAT(read_blocks, (u_int) CB_BYTE_TO_BLOCK(ce->ce_length));
			BC_ADD_STAT(batch_size[batch], (u_int) CB_BYTE_TO_BLOCK(ce->ce_length));

			/*
			 * Wake up anyone wanting this extent, as it is now ready for
			 * use.
			 */
			ce->ce_flags |= CE_IODONE;
			UNLOCK_EXTENT(ce);
			wakeup(ce);
		}
		/* Measure times for the first 4 batches separately */
		microtime(&batchend);
		timersub(&batchend, &batchstart, &batchend);
		BC_cache->c_stats.ss_batch_time[MIN(batch, STAT_BATCHMAX)] = 
			(u_int) batchend.tv_sec * 1000 + 
			(u_int) batchend.tv_usec / 1000;
		debug("batch %d done", batch);
		KERNEL_DEBUG_CONSTANT(FSDBG_CODE(DBG_BOOTCACHE, DBG_BC_BATCH) | 
				DBG_FUNC_END,
				batch, 0, 0, 0, 0);

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
			if (batch < (tmp->ce_flags & CE_BATCH_MASK)) {
				tmp->ce_flags |= CE_ABORTED;
			}
			/* abort extents in this batch that we haven't reached */
			if ((tmp >= ce) && (batch == (tmp->ce_flags & CE_BATCH_MASK))) {
				tmp->ce_flags |= CE_ABORTED;
			}
			/* wake up anyone asleep on this extent */
			UNLOCK_EXTENT(tmp);
			wakeup(tmp);
			tmp++;
		}
	}

	/* wake up someone that might be waiting for us to exit */
	BC_clear_flag(BC_FLAG_IOBUSY);
	wakeup(&BC_cache->c_flags);
	debug("reader thread done");

	buf_free(bp);
}


/*
 * Pass a read on to the default strategy handler.
 */
static void
BC_strategy_bypass(struct buf *bp)
{
	int isread = 0;
	assert(bp != NULL);

	/* if here, and it's a read, we missed the cache */
	if (buf_flags(bp) & B_THROTTLED_IO) {
		BC_ADD_STAT(strategy_throttled, 1);
	} else if (buf_flags(bp) & B_READ) {
		BC_add_history(CB_BLOCK_TO_BYTE(buf_blkno(bp)), buf_count(bp), BC_HE_MISS);
		isread = 1;
	}

	/* not really "bypassed" if the cache is not active */
	if (BC_cache->c_flags & BC_FLAG_CACHEACTIVE) {
		BC_ADD_STAT(strategy_bypassed, 1);
		if (BC_cache->c_flags & BC_FLAG_IOBUSY)
			BC_ADD_STAT(strategy_bypass_active, 1);
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
			message("hit rate below threshold (%d hits on %d lookups)",
			    BC_cache->c_stats.ss_extent_hits,
			    BC_cache->c_stats.ss_extent_lookups);
			if (BC_terminate_readahead()) {
				message("could not stop readahead on bad cache hitrate");
			} else {
				OSAddAtomic(-1, &BC_cache->c_strategycalls);
				if (BC_terminate_cache()) {
					message("could not terminate cache on bad hitrate");
					OSAddAtomic(1, &BC_cache->c_strategycalls);
				}
			}
		}
	}
	
	/* pass the request on */
	BC_cache->c_strategy(bp);
}

/*
 * Handle an incoming read request.
 */
static void
BC_strategy(struct buf *bp)
{
	struct BC_cache_extent *ce = NULL;
	uio_t uio = NULL;
	int base, nblk, retry, bcount, resid, discards;
	struct timespec timeout;
	daddr64_t blkno;
	caddr_t p;
	off_t offset;
	kern_return_t kret;

	assert(bp != NULL);

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
		return;
	}

	/*
	 * In order to prevent the cleanup code from racing with us
	 * when we sleep, we track the number of strategy calls active.
	 */
	OSAddAtomic(1, &BC_cache->c_strategycalls);
	BC_ADD_STAT(strategy_calls, 1);

	/*
	 * If the cache is not active, bypass the request.  We may
	 * not be able to fill it, but we still want to record it.
	 */
	if (!(BC_cache->c_flags & BC_FLAG_CACHEACTIVE)) {
		goto bypass;
	}

	/* if it's not a read, pass it off */
	if ( !(buf_flags(bp) & B_READ)) {
		BC_handle_write(bp);
		BC_add_history(CB_BLOCK_TO_BYTE(blkno), bcount, BC_HE_WRITE);
		BC_ADD_STAT(strategy_nonread, 1);
		goto bypass;
	}

	/* ignore low-priority IO */
	if (buf_flags(bp) & B_THROTTLED_IO) {
		goto bypass;
	}

	BC_ADD_STAT(requested_blocks, (u_int) CB_BYTE_TO_BLOCK(bcount));
	
	/*
	 * Look for a cache extent containing this request.
	 */
	BC_ADD_STAT(extent_lookups, 1);
	if ((ce = BC_find_extent(CB_BLOCK_TO_BYTE(blkno), bcount, 1)) == NULL)
		goto bypass;
	BC_ADD_STAT(extent_hits, 1);

	/*
	 * If the extent hasn't been read yet, sleep waiting for it.
	 */
	LOCK_EXTENT(ce);
	for (retry = 0; ; retry++) {
		/* has the reader thread made it to this extent? */
		if (ce->ce_flags & (CE_ABORTED | CE_IODONE))
			break;	/* don't go to bypass, record blocked time */

		/* check for timeout */
		if (retry > (BC_wait_for_readahead * 10)) {
			debug("timed out waiting for extent %p to be read", ce);
			goto bypass;
		}

		/* account for blocked callers */
		if (retry == 1)
			BC_ADD_STAT(strategy_blocked, 1);
		
		/* not ready, sleep */
		msleep(ce, &ce->ce_lock, PRIBIO, "BC_strategy", &timeout);
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
	base = (int) (blkno - CB_BYTE_TO_BLOCK(ce->ce_diskoffset));
	nblk = (int) CB_BYTE_TO_BLOCK(bcount);
	assert(base >= 0);
	if (!BC_blocks_present(ce, base, nblk))
		goto bypass;

	if (BC_cache->c_flags & BC_FLAG_IOBUSY)
		BC_ADD_STAT(strategy_duringio, 1);
#ifdef EMULATE_ONLY
	/* discard blocks we have touched */
	discards = BC_discard_blocks(ce, CB_BLOCK_TO_BYTE(blkno), bcount);
	BC_ADD_STAT(hit_blocks, discards);

	/* release the extent */
	UNLOCK_EXTENT(ce);
	
	/* we would have hit this request */
	BC_add_history(CB_BLOCK_TO_BYTE(blkno), bcount, BC_HE_HIT);

	/* bypass directly without updating statistics */
	BC_cache->c_strategy(bp);
#else

	/*
	 * buf_map will do its best to provide access to this
	 * buffer via a kernel accessible address
	 * if it fails, we can still fall through.
	 */
	if (buf_map(bp, &p)) {
		/* can't map, let someone else worry about it */
	        goto bypass;
	}

	offset = ce->ce_cacheoffset + CB_BLOCK_TO_BYTE(base);
	resid = bcount;
	
	uio = uio_create(1, offset, UIO_SYSSPACE, UIO_READ);
	if (uio == NULL) {
		debug("couldn't allocate uio");
		buf_unmap(bp);
		goto bypass;
	}

	kret = uio_addiov(uio, CAST_USER_ADDR_T(p), bcount);
	if (kret != KERN_SUCCESS) {
		debug("couldn't add iov to uio - %d", kret);
		buf_unmap(bp);
		goto bypass;
	}

	kret = vnode_getwithvid(BC_cache->c_vp, BC_cache->c_vid);
	if (kret != KERN_SUCCESS) {
		debug("vnode_getwithvid failed - %d", kret);
		buf_unmap(bp);
		goto bypass;
	}

	kret = cluster_copy_ubc_data(BC_cache->c_vp, uio, &resid, 1); 
	if (kret != KERN_SUCCESS) {
		debug("couldn't copy ubc data - %d", kret);
		buf_unmap(bp);
		(void) vnode_put(BC_cache->c_vp);
		goto bypass;
	}

	(void) vnode_put(BC_cache->c_vp);

	/* make sure we didn't hit a missing page in the cache */
	if (resid != 0) {
		/* blocks have been stolen for pageout or contiguous memory */
		discards = BC_discard_blocks(ce, CB_BLOCK_TO_BYTE(blkno), bcount);
		BC_ADD_STAT(lost_blocks, discards);

		buf_unmap(bp);
		goto bypass;
	} 	

	/* copy was successful */
	uio_free(uio);

	buf_setresid(bp, 0);

	/* buf_unmap will take care of all cases */
	buf_unmap(bp);

	/* complete the request */
	buf_biodone(bp);
	
	/* can't dereference bp after a buf_biodone has been issued */

	/* discard blocks we have touched */
	discards = BC_discard_blocks(ce, CB_BLOCK_TO_BYTE(blkno), bcount);
	BC_ADD_STAT(hit_blocks, discards);

	UNLOCK_EXTENT(ce);
	/* record successful fulfilment (may block) */
	BC_add_history(CB_BLOCK_TO_BYTE(blkno), bcount, BC_HE_HIT);
#endif

	/* we are not busy anymore */
	OSAddAtomic(-1, &BC_cache->c_strategycalls);
	return;

bypass:
	/*
	 * We have to drop the busy count before bypassing, as the
	 * bypass may try to terminate the cache (which will block and
	 * ultimately fail if the busy count is not zero).
	 */
	if (ce != NULL)
		UNLOCK_EXTENT(ce);
	if (uio != NULL)
		uio_free(uio);
	BC_strategy_bypass(bp);
	OSAddAtomic(-1, &BC_cache->c_strategycalls);
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
	offset = CB_BLOCK_TO_BYTE(blkno);

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
	BC_ADD_STAT(write_discards, count);
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
		BC_ADD_STAT(write_discards, count);
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
		BC_ADD_STAT(write_discards, count);
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
		if (!BC_set_flag(BC_FLAG_SHUTDOWN)) { 
			/* lost the race, so we're done here */
			debug("readahead already shut down");
			return 0;
		}

		error = tsleep(&BC_cache->c_flags, PRIBIO, "BC_terminate_readahead", 1000);
		if (error == EWOULDBLOCK) {
		
			debug("timed out waiting for I/O to stop");
			if (!(BC_cache->c_flags & BC_FLAG_IOBUSY)) {
				debug("but I/O has stopped!");
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
	int retry, i, j;
	
	/* can't shut down if readahead is still active */
	if (BC_cache->c_flags & BC_FLAG_IOBUSY) {
		debug("cannot terminate cache while readahead is in progress");
		return(EBUSY);
	}
	
	if (!BC_clear_flag(BC_FLAG_CACHEACTIVE)) { 
		/* can't shut down if we're not active */
		debug("cannot terminate cache when not active");
		return(ENXIO);
	}

	debug("terminating cache...");

	/*
	 * Mark all extents as FREED. This will notify any sleepers in the 
	 * strategy routine that the extent won't have data for them.
	 */
	for (i = 0; i < BC_cache->c_extent_count; i++) {
		struct BC_cache_extent *ce = BC_cache->c_extents + i;
		LOCK_EXTENT(ce);
		ce->ce_flags |= CE_ABORTED;
		UNLOCK_EXTENT(ce);
		wakeup(ce);

		/* 
		 * Track unused blocks 
		 */
		for (j = 0; j < howmany(ce->ce_length, BC_cache->c_blocksize); j++) {
			if (CB_BLOCK_PRESENT(ce, j))
				BC_ADD_STAT(spurious_blocks, 1);
		}
			
		BC_teardown_extent(ce);
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
		tsleep(BC_cache, PRIBIO, "BC_terminate_cache", 10);
		if (retry++ > 50) {
			debug("could not terminate cache, timed out with %d caller%s in BC_strategy",
			    (int) BC_cache->c_strategycalls,
			    BC_cache->c_strategycalls == 1 ? "" : "s");

			return(EBUSY);	/* again really EWEDGED */
		}
	}

	BC_free_pagebuffer();
	
	/* free memory held by extents */
	_FREE_ZERO(BC_cache->c_extents, M_TEMP);

	return(0);
}

/*
 * Terminate history recording.
 *
 * This stops us recording any further history events, as well
 * as disabling the cache.
 *
 * Only called if BC_terminate_cache was successful.
 */
static int
BC_terminate_history(void)
{
	debug("terminating history collection...");
	/* disconnect the strategy routine */
	if ((BC_cache->c_devsw != NULL) &&
	    (BC_cache->c_strategy != NULL))
		BC_cache->c_devsw->d_strategy = BC_cache->c_strategy;

	/* record stop time */
	microtime(&BC_cache->c_endtime);
	timersub(&BC_cache->c_endtime,
			&BC_cache->c_starttime,
			&BC_cache->c_endtime);
	BC_ADD_STAT(active_time, (u_int) BC_cache->c_endtime.tv_sec * 1000 + 
			(u_int) BC_cache->c_endtime.tv_usec / 1000);

	timersub(&BC_cache->c_starttime,
			&BC_cache->c_loadtime,
			&BC_cache->c_starttime);
	BC_ADD_STAT(preload_time, (u_int) BC_cache->c_starttime.tv_sec * 1000 + 
			(u_int) BC_cache->c_starttime.tv_usec / 1000);

	
	return(0);
}

static u_int64_t 
BC_setup_extent(struct BC_cache_extent *ce, struct BC_playlist_entry *pce)
{
	u_int64_t roundsize;
	int numblocks;

	roundsize = roundup(pce->pce_length, PAGE_SIZE);
	numblocks = (int) howmany(pce->pce_length, BC_cache->c_blocksize);

	lck_mtx_init(&ce->ce_lock, BC_cache->c_lckgrp,
			LCK_ATTR_NULL);
	ce->ce_diskoffset = pce->pce_offset;
	ce->ce_length = pce->pce_length;
	ce->ce_flags = (int) (pce->pce_batch & CE_BATCH_MASK);
#ifdef IGNORE_BATCH
	ce->ce_flags &= ~CE_BATCH_MASK;
#endif
	ce->ce_cacheoffset = 0;
	ce->ce_blockmap = _MALLOC(
			howmany(numblocks, (CB_MAPFIELDBITS / CB_MAPFIELDBYTES)),
			M_TEMP, M_WAITOK | M_ZERO);
	if (!ce->ce_blockmap)
		return 0;

	/* track highest batch number for this playlist */
	if (ce->ce_flags > BC_cache->c_batch_count) {
		BC_cache->c_batch_count = ce->ce_flags;
	}

	return roundsize;
}

static void
BC_teardown_extent(struct BC_cache_extent *ce)
{
	ce->ce_flags |= CE_ABORTED;
	_FREE_ZERO(ce->ce_blockmap, M_TEMP);
}

/*
 * Fetch the playlist from userspace or the Mach-O segment where it
 * was preloaded.   
 */
static int
BC_copyin_playlist(size_t length, user_addr_t uptr)
{
	struct BC_playlist_entry *pce;
	struct BC_cache_extent *ce;
	int error, idx;
	off_t p;
	u_int64_t size, esize;
	int entries;
	int actual;

	error = 0;
	pce = NULL;
	
	/* allocate playlist storage */
	entries = (int) (length / sizeof(*pce));
	BC_cache->c_extents = _MALLOC(entries * sizeof(struct BC_cache_extent),
	    M_TEMP, M_WAITOK | M_ZERO);
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
		pce = CAST_DOWN(struct BC_playlist_entry *, uptr);
		for (idx = 0; idx < entries; idx++) {
			esize = BC_setup_extent(ce + idx, pce + idx);
			if (esize == 0) {
				error = ENOMEM;
				goto out;
			}
			size += esize;
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
			if ((error = copyin(uptr, pce,
					    actual * sizeof(struct BC_playlist_entry))) != 0) {
				message("copyin from 0x%llx to %p failed", uptr, pce);
				_FREE(pce, M_TEMP);
				goto out;
			}

			/* unpack into our array */
			for (idx = 0; idx < actual; idx++) {
				esize = BC_setup_extent(ce + idx, pce + idx);
				if (esize == 0) {
					error = ENOMEM;
					goto out;
				}
				size += esize;
			}
			entries -= actual;
			uptr += actual * sizeof(struct BC_playlist_entry);
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
	 * max_mem and size are in bytes.  All calculations are done in page
	 * units to avoid overflow/sign issues.
	 *
	 * If the cache is too large, we discard it and go to record-only mode.
	 * This lets us survive the case where the playlist is corrupted or
	 * manually made too large, but the "real" bootstrap footprint
	 * would be OK.
	 */
	if (size > (max_mem / 2)) {
		message("cache size (%lu bytes) too large for physical memory (%llu bytes)",
				(unsigned long)size, max_mem);
		goto out;
	}

	BC_cache->c_cachesize = size;

	/*
	 * Allocate the pagebuffer.
	 */
	if (BC_alloc_pagebuffer() != 0) {
		message("can't allocate %lld bytes for cache buffer", size);
		error = ENOMEM;
		goto out;
	}

	/*
	 * Fix up the extent cache offsets.
	 */
	p = 0; 
	for (idx = 0; idx < BC_cache->c_extent_count; idx++) {
		u_int64_t length = BC_cache->c_extents[idx].ce_length;
		int j;
		(BC_cache->c_extents + idx)->ce_cacheoffset = p;
		p += roundup(length, PAGE_SIZE);

		for (j = 0; j < howmany(length, BC_cache->c_blocksize); j++)
			CB_MARK_BLOCK_PRESENT((BC_cache->c_extents + idx), j);
	}

	/* all done */
out:
	if (error != 0) {
		debug("cache setup failed, aborting");
		if (BC_cache->c_vp != NULL)
			BC_free_pagebuffer();
		for (idx = 0; idx < BC_cache->c_extent_count; idx++)
			BC_teardown_extent(BC_cache->c_extents + idx);
		_FREE_ZERO(BC_cache->c_extents, M_TEMP);
		BC_cache->c_extent_count = 0;
		BC_cache->c_stats.ss_total_extents = 0;
	}
	return(error);
}

/*
 * Initialise the cache.  Fetch the playlist from userspace and
 * find and attach to our block device.
 */
static int
BC_init_cache(size_t length, user_addr_t uptr, u_int64_t blocksize)
{
	u_int32_t blksize;
	u_int64_t blkcount;
	int error;
	unsigned int boot_arg;
	thread_t rthread;

	error = 0;

	/*
	 * Check that we're not already running.
	 */
	if (BC_set_flag(BC_FLAG_STARTED)) {
		debug("cache already started");
		return(EBUSY);
	}

	/*
	 * If we have less than the minimum supported physical memory,
	 * bail immediately.  With too little memory, the cache will
	 * become polluted with nondeterministic pagein activity, leaving
	 * large amounts of wired memory around which further exacerbates
	 * the problem.
	 */
	if ((max_mem / (1024 * 1024)) < BC_minimum_mem_size) {
		debug("insufficient physical memory (%dMB < %dMB required)",
		    (int)(max_mem / (1024 * 1024)), BC_minimum_mem_size);
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
			(!PE_parse_boot_argn("BootCacheOverride", &boot_arg, sizeof(boot_arg)) ||
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
	    DKIOCGETMAXSEGMENTBYTECOUNTREAD,		/* cmd */
	    (caddr_t)&BC_cache->c_maxread,		/* data */
	    0,						/* fflag */
	    kernproc);					/* proc */
	if (BC_cache->c_maxread < PAGE_SIZE) {
		debug("can't determine device read size; using default");
		BC_cache->c_maxread = MAXPHYS;
	}
	BC_cache->c_devsw->d_ioctl(BC_cache->c_dev,	/* device */
	    DKIOCGETBLOCKSIZE,				/* cmd */
	    (caddr_t)&blksize,				/* data */
	    0,						/* fflag */
	    kernproc);					/* proc */
	if (blksize == 0) {
		message("can't determine device block size, defaulting to 512 bytes");
		blksize = 512;
	}
	BC_cache->c_devsw->d_ioctl(BC_cache->c_dev,	/* device */
	    DKIOCGETBLOCKCOUNT,				/* cmd */
	    (caddr_t)&blkcount,				/* data */
	    0,						/* fflag */
	    kernproc);					/* proc */
	if (blkcount == 0) {
		message("can't determine device size, not checking");
	}
	BC_cache->c_blocksize = blksize;
	BC_cache->c_devsize = BC_cache->c_blocksize * blkcount;
	BC_cache->c_stats.ss_blocksize = (u_int) BC_cache->c_blocksize;
	debug("blocksize %lu bytes, filesystem %lu bytes",
	    (unsigned long)BC_cache->c_blocksize, (unsigned long)BC_cache->c_devsize);
	BC_cache->c_lckgrp = lck_grp_alloc_init("BootCache", LCK_GRP_ATTR_NULL);
	lck_mtx_init(&BC_cache->c_histlock, BC_cache->c_lckgrp, LCK_ATTR_NULL);

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

	microtime(&BC_cache->c_starttime);
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

#ifdef EMULATE_ONLY
	{
		int i;
		for (i = 0; i < BC_cache->c_extent_count; i++)
			BC_cache->c_extents[i].ce_flags |= CE_IODONE;
	}
#else
	/*
	 * Start the reader thread.
	 */
	if (BC_cache->c_extents != NULL) {
		debug("starting readahead");
		BC_set_flag(BC_FLAG_IOBUSY);
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
	BC_set_flag(BC_FLAG_CACHEACTIVE);

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
	/* shut the cache down */
	BC_terminate_readahead();
	if (BC_terminate_cache())
		return;
	BC_terminate_history();
}

/*
 * Record an incoming read in the history list.
 *
 * Note that this function is not reentrant.
 */
static void
BC_add_history(u_int64_t offset, u_int64_t length, int flags)
{
	struct BC_history_cluster *hc, *tmphc = NULL;
	struct BC_history_entry *he;

	LOCK_HISTORY();
	/* don't do anything if the history list has been truncated */
	if (BC_cache->c_flags & BC_FLAG_HTRUNCATED)
		goto out;

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
	if ((BC_cache->c_history_bytes + length) > (max_mem / 2))
		goto out;

	BC_cache->c_history_bytes += length;
	
#ifdef NO_HISTORY
	BC_set_flag(BC_FLAG_HTRUNCATED);
	debug("history disabled, truncating");
	goto out;
#endif
	
	/*
	 * Get the current cluster.
	 */
	hc = BC_cache->c_history;
	if ((hc == NULL) || (hc->hc_entries >= BC_HISTORY_ENTRIES)) {
		BC_ADD_STAT(history_clusters, 1);

		/* limit the number of clusters we will allocate */
		if (BC_cache->c_stats.ss_history_clusters >= BC_HISTORY_MAXCLUSTERS) {
			message("too many history clusters (%d, limit %ld)",
			    BC_cache->c_stats.ss_history_clusters,
			    (long)BC_HISTORY_MAXCLUSTERS);
			BC_set_flag(BC_FLAG_HTRUNCATED);
			BC_ADD_STAT(history_clusters, -1);
			goto out;
		}
		UNLOCK_HISTORY();
		tmphc = kalloc(BC_HISTORY_ALLOC);
		LOCK_HISTORY();
		if (tmphc == NULL) {
			message("could not allocate %d bytes for history cluster",
			    BC_HISTORY_ALLOC);
			BC_set_flag(BC_FLAG_HTRUNCATED);
			goto out;
		}

		/* verify that we still need a new cluster */
		hc = BC_cache->c_history;
		if ((hc == NULL) || (hc->hc_entries >= BC_HISTORY_ENTRIES)) {
			tmphc->hc_entries = 0;
			tmphc->hc_link = BC_cache->c_history;
			BC_cache->c_history = tmphc;
			hc = tmphc;
			tmphc = NULL;
		}
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
out:
	UNLOCK_HISTORY();
	if (tmphc != NULL)
		kfree(tmphc, BC_HISTORY_ALLOC);
	return;
}

/*
 * Return the size of the history buffer.
 */
static int 
BC_size_history(void)
{
	struct BC_history_cluster *hc;
	int nentries;

	/* walk the list of clusters and count the number of entries */
	nentries = 0;
	for (hc = BC_cache->c_history; hc != NULL; hc = hc->hc_link)
		nentries += hc->hc_entries;

	return(nentries * (int) sizeof(struct BC_history_entry));
}

/*
 * Copy out either the history buffer size, or the entire history buffer.
 *
 * Note that the cache must be shut down before calling this function in
 * order to avoid a race around the entry count/buffer size.
 */
static int
BC_copyout_history(user_addr_t uptr)
{
	struct BC_history_cluster *hc, *ohc;
	int error, nentries;

	assert(uptr != 0);

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
		if ((error = copyout(hc->hc_data, uptr,
			 hc->hc_entries * sizeof(struct BC_history_entry))) != 0)
			return(error);
		uptr += hc->hc_entries * sizeof(struct BC_history_entry);

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
		kfree(hc, BC_HISTORY_ALLOC);
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
	error = BC_init_cache(BC_preloaded_playlist->ph_entries * sizeof(struct BC_playlist_entry), 
			      CAST_USER_ADDR_T((uintptr_t)BC_preloaded_playlist + sizeof(struct BC_playlist_header)), 
			      BC_preloaded_playlist->ph_blocksize);
	if (error != 0)
		debug("BootCache autostart failed: %d", error);
}

/*
 * Called from the root-unmount hook.
 */
static void
BC_unmount_hook(void)
{
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
	if (!BC_terminate_cache())
		BC_terminate_history();

}


/*
 * Handle the command sysctl.
 */
static int
BC_sysctl SYSCTL_HANDLER_ARGS
{
	struct BC_command bc;
	int error;

	/* get the commande structure and validate */
	if ((error = SYSCTL_IN(req, &bc, sizeof(bc))) != 0) {
		debug("couldn't get command");
		return(error);
	}
	if (bc.bc_magic != BC_MAGIC) {
		debug("bad command magic");
		return(EINVAL);
	}

	switch (bc.bc_opcode) {
	case BC_OP_START:
		if (BC_preloaded_playlist) {
			error = EINVAL;
			break;
		}
		debug("BC_OP_START(%d)", bc.bc_length);
		error = BC_init_cache(bc.bc_length, (user_addr_t) bc.bc_data, (u_int64_t)bc.bc_param);
		if (error != 0) {
			message("cache init failed");
		} else {
			/* start the timeout handler */
			timeout(BC_timeout_cache, NULL, BC_cache_timeout);
		}
		break;

	case BC_OP_STOP:
		debug("BC_OP_STOP");
		unmountroot_pre_hook = NULL;

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
		if (!BC_terminate_cache())
			BC_terminate_history();

		/* return the size of the history buffer */
		LOCK_HISTORY();
		if (BC_cache->c_flags & BC_FLAG_HTRUNCATED) {
			bc.bc_length = 0;
		} else {
			bc.bc_length = BC_size_history();
		}
		if ((error = SYSCTL_OUT(req, &bc, sizeof(bc))) != 0)
			debug("could not return history size");
		UNLOCK_HISTORY();
		break;
		
	case BC_OP_HISTORY:
		debug("BC_OP_HISTORY");
		/* if there's a user buffer, verify the size and copy out */
		LOCK_HISTORY();
		if (bc.bc_data != 0) {
			if (bc.bc_length < BC_size_history()) {
				debug("supplied history buffer too small");
				error = EINVAL;
				UNLOCK_HISTORY();
				break;
			}
			if ((error = BC_copyout_history(bc.bc_data)) != 0)
				debug("could not copy out history");
			
		}
		/*
		 * Release the last of the memory we own.
		 */
		BC_discard_history();
		UNLOCK_HISTORY();
		BC_clear_flag(BC_FLAG_STARTED);
		break;
				    
	case BC_OP_STATS:
		debug("BC_OP_STATS");
		/* check buffer size and copy out */
		if (bc.bc_length != sizeof(BC_cache->c_stats)) {
			debug("stats structure wrong size");
			error = ENOMEM;
		} else {
			BC_cache->c_stats.ss_cache_flags = BC_cache->c_flags;
			if ((error = copyout(&BC_cache->c_stats, bc.bc_data, bc.bc_length)) != 0)
				debug("could not copy out statistics");
		}
		break;

	case BC_OP_TAG:
		debug("BC_OP_TAG");
		KERNEL_DEBUG_CONSTANT(FSDBG_CODE(DBG_BOOTCACHE, DBG_BC_TAG) | 
				DBG_FUNC_NONE, proc_selfppid(), dbg_tag_count, 
				0, 0, 0);
		BC_add_history((u_int64_t)bc.bc_param, 0, BC_HE_TAG);
		dbg_tag_count++;
		break;
		
	default:
		error = EINVAL;
	}
	return(error);
}


/*
 * Initialise the block of pages we use to back our data.
 *
 */
static int
BC_alloc_pagebuffer()
{
	kern_return_t kret;

	kret = vnode_open(BC_BOOT_BACKINGFILE, (O_CREAT | O_NOFOLLOW | FREAD), 
			0400, 0, &BC_cache->c_vp, vfs_context_current());
	if (kret != KERN_SUCCESS) {
		debug("vnode_open failed - %d", kret);
		return -1;
	}

	BC_cache->c_vid = vnode_vid(BC_cache->c_vp);

	assert(BC_cache->c_cachesize != 0);
	kret = ubc_setsize(BC_cache->c_vp, BC_cache->c_cachesize);
	if (kret != 1) {
		debug("ubc_setsize failed - %d", kret);
		return -1;
	}
	
	(void) vnode_put(BC_cache->c_vp);

	return(0);
}

/*
 * Release all resources associated with our pagebuffer.
 */
static void
BC_free_pagebuffer(void)
{
	kern_return_t kret;
	if (BC_cache->c_vp != NULL) {
		/* Can handle a vnode without ubc info */
		kret = vnode_getwithref(BC_cache->c_vp);
		if (kret != KERN_SUCCESS) {
			debug("vnode_getwithref failed - %d", kret);
			return;
		}

		kret = ubc_range_op(BC_cache->c_vp, 0, BC_cache->c_cachesize, 
				UPL_ROP_DUMP, NULL);
		if (kret != KERN_SUCCESS) {
			debug("ubc_range_op failed - %d", kret);
		}

#if 0
		/* need this interface to be exported... will eliminate need
		 * for the ubc_range_op above */
		kret = vnode_delete(BC_BOOT_BACKINGFILE, vfs_context_current());
		if (kret) {
			debug("vnode_delete failed - %d", error);
		}
#endif
		kret = vnode_close(BC_cache->c_vp, 0, vfs_context_current());
		if (kret != KERN_SUCCESS) {
			debug("vnode_close failed - %d", kret);
		}
		
		BC_cache->c_vp = NULL;
	}
}

/*
 * Release one page from the pagebuffer.
 */
static void
BC_free_page(struct BC_cache_extent *ce, int page)
{
	off_t vpoffset;
	kern_return_t kret;

	vpoffset = (page * PAGE_SIZE) + ce->ce_cacheoffset;
	kret = vnode_getwithvid(BC_cache->c_vp, BC_cache->c_vid);
	if (kret != KERN_SUCCESS) {
		debug("vnode_getwithvid failed - %d", kret);
		return;
	}

	kret = ubc_range_op(BC_cache->c_vp, vpoffset, vpoffset + PAGE_SIZE,
			UPL_ROP_DUMP, NULL);
	if (kret != KERN_SUCCESS) {
		debug("ubc_range_op failed - %d", kret);
	}

	(void) vnode_put(BC_cache->c_vp);
}

void
BC_hook(void)
{
	microtime(&BC_cache->c_loadtime);
}

/*
 * Cache start/stop functions.
 */
void
BC_load(void)
{
	struct mach_header	*mh;
	long			xsize;
	int			has64bitness, error;
	size_t			sysctlsize = sizeof(int);
	char			*plsection = "playlist";

	/* register our control interface */
	sysctl_register_oid(&sysctl__kern_BootCache);

	/* clear the control structure */
	bzero(BC_cache, sizeof(*BC_cache));

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

	/*
	 * If booting on a 32-bit only machine, use the "playlist32" section.
	 * Otherwise, fall through to the default "playlist" section.
	 */
	error = sysctlbyname("hw.cpu64bit_capable", &has64bitness, &sysctlsize, 
			NULL, 0);
	if (error == 0 && has64bitness == 0) {
		plsection = "playlist32";
	}
	BC_preloaded_playlist = getsectdatafromheader(mh, "BootCache", 
			plsection, &BC_preloaded_playlist_size);
	debug("preload section %s: %d @ %p", 
	      plsection, BC_preloaded_playlist_size, BC_preloaded_playlist);

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
			message("preloaded playlist too small (%d, expected at least %ld)",
				BC_preloaded_playlist_size, xsize);
			return;
		}
		BC_wait_for_readahead = BC_READAHEAD_WAIT_CDROM;
		mountroot_post_hook = BC_auto_start;
		unmountroot_pre_hook = BC_unmount_hook;
		microtime(&BC_cache->c_loadtime);
		
		debug("waiting for root mount...");
	} else {
		/* no preload, normal operation */
		BC_preloaded_playlist = NULL;
		mountroot_post_hook = BC_hook;
		debug("waiting for playlist...");
	}
}

int
BC_unload(void)
{
	int error;

	error = 1;

	debug("preparing to unload...");
	unmountroot_pre_hook = NULL;
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
	if (BC_terminate_cache())
		goto out;
	BC_terminate_history();
	LOCK_HISTORY();
	BC_discard_history();
	UNLOCK_HISTORY();

	sysctl_unregister_oid(&sysctl__kern_BootCache);
	error = 0;

out:
	return(error);
}
