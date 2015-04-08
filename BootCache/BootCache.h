#include <uuid/uuid.h>

/*
 * Files.
 */
#define BC_ROOT_PLAYLIST	"/var/db/BootCache.playlist"
#define BC_ROOT_EXTRA_LOGICAL_PLAYLIST "/var/db/BootCaches/RootExtra.logical_playlist"
#define BC_LOGIN_EXTRA_LOGICAL_PLAYLIST "/var/db/BootCaches/LoginExtra.logical_playlist"
#define BC_ROOT_LOGICAL_PLAYLIST "/var/db/BootCaches/Root.logical_playlist"
#define BC_LOGIN_LOGICAL_PLAYLIST "/var/db/BootCaches/Login.logical_playlist"
#define BC_BOOT_BACKINGFILE	"/var/db/BootCache.data"
#define BC_BOOT_FLAGFILE	"/var/db/BootCache.flag"
#define BC_BOOT_STATFILE	"/tmp/BootCache.statistics"
#define BC_BOOT_HISTFILE	"/tmp/BootCache.history"
#define BC_BOOT_LOGFILE		"/tmp/BootCache.log"
#define BC_CONTROL_TOOL		"/usr/sbin/BootCacheControl"
#define BC_KEXTUNLOAD		"/sbin/kextunload"
#define BC_BUNDLE_ID		"com.apple.BootCache"

/*
 * Files we read in during every boot.
 */
#define BC_DYLD_SHARED_CACHE    "/var/db/dyld/dyld_shared_cache_x86_64"
#define BC_DYLD_SHARED_CACHE_H  "/var/db/dyld/dyld_shared_cache_x86_64h"
#define BC_DYLD_SHARED_CACHE_32 "/var/db/dyld/dyld_shared_cache_i386"

/*
 * If defined, entries/extents are sorted by their location on disk
 * (rather than chronologically or some other sorting).
 */
#define BOOTCACHE_ENTRIES_SORTED_BY_DISK_OFFSET

/*
 * Command stucture, passed via sysctl.
 */
struct BC_command {
	/* magic number identifies the structure */
	int	bc_magic;
#define	BC_MAGIC_0	0x10102021	/* old version with DEV_BSIZE blocks */
#define	BC_MAGIC_1	0x10102021	/* all disk addresses in bytes */
#define	BC_MAGIC_2	0x10102021	/* added flags field to playlist entry */
#define	BC_MAGIC_3	0x10102022	/* major restructure, added mounts */
#define	BC_MAGIC	0x10102023	/* add pid/shared flag to history */

	/* opcode determines function of this command */
	int	bc_opcode;
#define BC_OP_START    0x01
#define BC_OP_STOP     0x02
#define BC_OP_HISTORY  0x03
#define BC_OP_STATS    0x04
#define BC_OP_TAG      0x05
#define BC_OP_JETTISON 0x06
#define BC_OP_MOUNT    0x07
#define BC_OP_TEST     0x08
	
	/* user-space data buffers, use varies with opcode */
	unsigned int bc_data1_size;
	unsigned int bc_data2_size;
	u_int64_t    bc_data1;
	u_int64_t    bc_data2;
};

#define BC_SYSCTL	"kern.BootCache"

/*
 * BC_OP_START - start the cache engine with an optional playlist.
 *
 * If bc_data1 is not NULL, it points to bc_data1_size bytes of
 * BC_playlist_mount structures.
 *
 * If bc_data2 is not NULL, it points to bc_data2_size bytes of
 * BC_playlist_entry structures.
 */

/*
 * On-disk playlist header, also used in the preload case.
 */
struct BC_playlist_header {
	int	ph_magic;
#define PH_MAGIC_0	0xa1b2c3d4	/* old format with no blocksize */
#define PH_MAGIC_1	0xa1b2c3d5	/* blocksize, offsets in bytes */
#define PH_MAGIC_2	0xa1b2c3d6	/* added flags field */
#define PH_MAGIC_3	0xa1b2c3d7	/* added non-root mounts */
#define PH_MAGIC	0xa1b2c3d8	/* added low-priority extents */
	int	ph_nmounts;
	int ph_nentries;
};

/*
 * Playlist mounts represent mount points to be cached.
 */
struct BC_playlist_mount {
	uuid_t pm_uuid;         /* The UUID of the mount */
	int    pm_nentries;     /* The number of playlist entries that refer to this mount */
};

/*
 * Playlist entries represent regions of the mount points to be cached.
 */
struct BC_playlist_entry {
	u_int64_t	pe_offset;    /* block address */
	u_int64_t	pe_length;    /* size */
	u_int16_t	pe_batch;     /* batch number */
	u_int16_t	pe_flags;     /* flags */
#define BC_PE_LOWPRIORITY 0x1
#define BC_PE_SHARED      0x2
	u_int32_t   pe_mount_idx; /* index of mount in mount array */
};

/* number of entries we copyin at a time */
#define BC_PLC_CHUNK	512

/* sanity check for the upper bound on number of entries and batches */
#define BC_MAXENTRIES	100000
#define BC_MAXBATCHES   255

/*
 * BC_OP_STOP - stop the cache engine.
 *
 * The history list mount size is returned in bc_data1_size and
 * entry list size is returned in bc_data2_size (in bytes).  If the history
 * list was truncated, bc_data1_size will be 0 and BC_OP_HISTORY must still be
 * called to clear the buffer.
 */

/*
 * BC_OP_JETTISON - jettison the cache.
 *
 * No parameters
 */

/*
 * BC_OP_HISTORY - copy out history and clear.
 *
 * The kernel's copy of the mount and entry history is copied into the array of
 * BC_history_mount and BC_history_entry structures at bc_data1 and bc_data2,
 * respectively. The cache must previously have been stopped before calling this
 * interface out.
 */

/*
 * Playlist mounts represent mount points to be cached.
 */
struct BC_history_mount {
	uuid_t hm_uuid;         /* The UUID of the mount */
	int hm_nentries;        /* The number of history entries that refer to this mount */
};

struct BC_history_entry {
	u_int64_t	he_offset;	  /* data offset on device */
	u_int64_t	he_length;	  /* length of data */
	u_int32_t	he_pid;       /* pid of the process that issued this IO */
	u_int16_t	he_flags;
#define	BC_HE_HIT    0x1		  /* read was satisfied from cache */
#define BC_HE_WRITE  0x2		  /* write-through operation */
#define BC_HE_TAG    0x4		  /* userland-set tag */
#define BC_HE_SHARED 0x8		  /* IO was from the shared cache */
	u_int16_t   he_mount_idx; /* index of the mount this read was on */
};


/* Number of disks and batches we can collect statistics on */
#define STAT_DISKMAX	8
#define STAT_MOUNTMAX	32
#define STAT_BATCHMAX	16

/*
 * BC_OP_STATS - return statistics.
 *
 * The kernel's copy of the statistics is copied into
 * the BC_statictics structure at bc_data1
 */
struct BC_statistics {
	/* readahead */
	u_int   ss_readahead_threads; /* number of readahead threads */
	u_int	ss_initiated_reads;	/* read operations we initiated */
	u_int	ss_read_blocks;		/* number of blocks read */
	u_int	ss_read_bytes;		/* number of bytes read */
	u_int	ss_read_errors;		/* read errors encountered */
	u_int	ss_read_errors_bytes;	/* bytes discarded due to read errors */
	u_int	ss_batch_bytes[STAT_DISKMAX][STAT_BATCHMAX+1];	/* number of bytes in read per batch, +1 for sum of extra batches */
	u_int	ss_batch_late_bytes[STAT_DISKMAX][STAT_BATCHMAX+1];	/* number of bytes read during this batch
																	that were scheduled for an earlier batch */
	u_int	ss_batch_initiated_reads[STAT_DISKMAX][STAT_BATCHMAX+1];	/* read operations we initiated per batch */
	u_int	ss_batch_time[STAT_DISKMAX][STAT_BATCHMAX+1];	/* msecs per batch, +1 for sum of extra batches */
	u_int	ss_cache_time;		/* msecs cache was alive */
	u_int	ss_preload_time;	/* msecs before cache started */

	u_int	ss_read_bytes_lowpri;                         /* number of bytes read */
	u_int	ss_batch_bytes_lowpri[STAT_DISKMAX];          /* number of bytes in read per disk */
	u_int	ss_batch_time_lowpri[STAT_DISKMAX];           /* msecs per disk */
	u_int	ss_batch_initiated_reads_lowpri[STAT_DISKMAX]; /* read operations we initiated per disk */

	/* inbound strategy calls (while we're recording) */
	u_int	ss_strategy_calls;			/* total strategy calls we received */
	u_int	ss_strategy_bypassed;		/* strategy calls we bypassed */
	u_int	ss_strategy_nonread;		/* strategy calls that were not reads */
	u_int	ss_strategy_throttled;		/* strategy calls marked low priority */
	u_int	ss_strategy_noncached_mount;	/* strategy calls from non-cached mounts */
	u_int	ss_strategy_unknown;		/* strategy calls that were unidentifiable */
	u_int	ss_strategy_unknown_bytes;	/* bytes of unidentifiable strategy calls */
	u_int	ss_strategy_nonblocksize;	/* strategy calls of non-blocksize-multiple size */
	
	/* non-cached IOs */
	u_int	ss_strategy_hit_nocache;	/* cache hits that were IOs we won't cache for next boot */
	u_int	ss_strategy_bypass_nocache;	/* cache misses that were IOs we won't cache for next boot */
	u_int	ss_strategy_bypass_duringio_nocache;	/* cache misses that were IOs we won't cache for next boot */
	u_int	ss_hit_nocache_bytes;		/* bytes hit by IOs we won't cache for next boot */
	u_int	ss_bypass_nocache_bytes;	/* bytes missed by IOs we won't cache for next boot */
	u_int	ss_bypass_nocache_discards;	/* bytes discarded by IOs we won't cache for next boot */

	/* io during readahead */
	u_int	ss_strategy_duringio;			/* total strategy calls */
	u_int	ss_strategy_bypass_duringio;	/* strategy calls we bypassed */
	u_int	ss_strategy_bypass_duringio_rootdisk_read;		/* read strategy calls we missed for the root disk */
	u_int	ss_strategy_bypass_duringio_rootdisk_failure;	/* read strategy calls we hit but failed to fulfil for the root disk */
	u_int	ss_strategy_bypass_duringio_rootdisk_nonread;	/* nonread strategy calls we bypassed for the root disk */
	u_int	ss_strategy_forced_throttled;			/* strategy calls we forced to throttle due to cutting through our readahead */
	u_int	ss_strategy_nonthrottled;				/* strategy calls that cut through our readahead but we did not throttle */
	u_int	ss_hit_duringio;				/* cache hits during active readahead */
	u_int   ss_strategy_bypass_duringio_unfilled;           /* strategy calls that hit an unfilled extent (for SSDs) */
	u_int   ss_strategy_unfilled_lowpri;    /* strategy calls that hit an unfilled low priority extent */
	u_int	ss_strategy_blocked;			/* strategy calls that blocked on future readhead */
	u_int	ss_strategy_timedout;			/* strategy calls that timed out */
	u_int	ss_strategy_time_longest_blocked;		/* longest time a strategy calls spent blocked on our cache (in milliseconds) */
	u_int	ss_strategy_time_blocked;		/* milliseconds strategy calls spent blocked on our cache */
	
	/* extents */
	u_int	ss_total_extents;	/* number of extents in the cache */
	u_int	ss_extents_clipped;	/* number of extents clipped due to page boundaries */
	u_int	ss_extent_lookups;	/* number of extents searched for */
	u_int	ss_extent_hits;		/* number of extents matched (cache hits) */
	u_int	ss_hit_multiple;	/* cache hits that touched more than one extent */
	u_int	ss_hit_aborted;		/* cache hits not filled due to aborted extents */
	u_int	ss_hit_blkmissing;	/* cache hits not filled due to missing blocks */
	u_int	ss_hit_stolen;		/* cache hits not filled due to stolen pages */
	u_int	ss_hit_failure;		/* cache hits not filled due to other failures */

	/* mounts */
	u_int	ss_total_mounts;	/* number of mounts in the cache */
	u_int	ss_history_mount_no_uuid;	/* number of mounts seen without a uuid */
	
	/* byte/page activity */
	u_int	ss_requested_bytes;
	u_int   ss_requested_bytes_m[STAT_MOUNTMAX];
	u_int	ss_hit_bytes;		/* number of bytes vacated due to read hits */
	u_int	ss_hit_bytes_m[STAT_MOUNTMAX];		/* number of bytes vacated due to read hits */
	u_int	ss_shared_bytes;		/* number of bytes read from the shared cache */
	u_int	ss_hit_shared_bytes;	/* number of bytes hit IOs in the shared cache */
	u_int	ss_stolen_bytes;		/* number of bytes lost to pageout or contig mem */
	u_int	ss_write_discards;	/* bytes discarded due to overwriting */
	u_int	ss_read_discards;	/* bytes discarded due to incoming reads */
	u_int	ss_error_discards;	/* bytes discarded due to failure to fulfil a cache hit */
	u_int	ss_unable_to_discard_bytes;	/* bytes not discarded due to lock contention (high estimate) */
	u_int	ss_unable_to_discard_count;	/* number of cut-through IOs with bytes not discarded due to lock contention */
	u_int	ss_spurious_bytes;	/* number of btyes not consumed */
	u_int	ss_hit_bytes_afterhistory;		/* bytes fulfilled after history recording was complete */
	u_int	ss_lost_bytes_afterhistory;	/* bytes lost after history recording was complete */

	/* history activity */
	u_int	ss_history_bytes;			/* number of bytes contained in the history we've seen for this boot */
	u_int	ss_history_time;			/* msecs hisotry was active */
	u_int	ss_history_reads;			/* number of reads we saw during initial boot */
	u_int	ss_history_writes;			/* number of writes we saw during initial boot */
	u_int	ss_history_entries;			/* number of history entries we've created this boot */
	u_int	ss_history_mounts;			/* number of allocated history mounts we had for the last recording */
	u_int	ss_history_unknown;			/* history calls we couldn't find a mount for */
	u_int	ss_history_unknown_bytes;	/* bytes history calls we couldn't find a mount for */
	u_int	ss_history_num_recordings;	/* number of recordings we've has this boot */

	/* current status */
	u_int	ss_cache_flags;		/* current cache flags */
};

/*
 * In-memory BootCache playlist structure
 */
struct BC_playlist {
	int                       p_nmounts;  /* number of mounts */
	int                       p_nentries; /* number of entries */
	struct BC_playlist_mount *p_mounts;   /* array of mounts */
	struct BC_playlist_entry *p_entries;  /* array of entries */
};

/*
 * In-memory BootCache history structure
 */
struct BC_history {
	int                      h_nmounts;  /* number of mounts */
	int                      h_nentries; /* number of entries */
	struct BC_history_mount *h_mounts;   /* array of mounts */
	struct BC_history_entry *h_entries;  /* array of entries */
};

#ifndef KERNEL
/*
 * Support library functions.
 */
extern int	BC_read_playlist(const char *, struct BC_playlist **);
extern int	BC_write_playlist(const char *, const struct BC_playlist *);
extern int	BC_merge_playlists(struct BC_playlist *, const struct BC_playlist *);
extern void	BC_sort_playlist(struct BC_playlist *);
extern int	BC_coalesce_playlist(struct BC_playlist *);
extern int	BC_playlist_for_file(int fd, struct BC_playlist** ppc);
extern int  BC_playlist_for_filename(int fd, const char *fname, off_t maxsize, struct BC_playlist** ppc);
extern int	BC_verify_playlist(const struct BC_playlist *);
extern void BC_free_playlist(struct BC_playlist *);
#define PC_FREE_ZERO(pc) do { if (pc) { BC_free_playlist(pc); (pc) = NULL; } } while (0)
extern void BC_free_history(struct BC_history *);
#define HC_FREE_ZERO(hc) do { if (hc) { BC_free_history(hc); (hc) = NULL; } } while (0)
extern int	BC_fetch_statistics(struct BC_statistics **);
extern int	BC_convert_history(const struct BC_history *, struct BC_playlist **);
extern int	BC_start(struct BC_playlist *);
extern int	BC_stop(struct BC_history **);
extern int	BC_notify_mount(void);
extern int	BC_test(void);
extern int	BC_jettison(void);
extern int	BC_print_statistics(char *, struct BC_statistics *);
extern int	BC_print_history(char *, struct BC_history *);
extern int	BC_tag_history(void);
extern int	BC_unload(void);
#endif
