#include <uuid/uuid.h>
#include <sys/syslimits.h>
#ifndef KERNEL
#include <sys/time.h> /* Cause the apfs_fsctl.h doesn't include it */
#include <apfs/apfs_fsctl.h>
#include <stdio.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

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
#define BC_BOOT_DISABLEFILE	"/var/db/BootCache.disable"
#define BC_BOOT_STATFILE	"/tmp/BootCache.statistics"
#define BC_BOOT_HISTFILE	"/tmp/BootCache.history"
#define BC_BOOT_OMAPHISTFILE	"/tmp/BootCache.omaphistory"
#define BC_BOOT_LOGFILE		"/tmp/BootCache.log"
#define BC_CONTROL_TOOL		"/usr/sbin/BootCacheControl"
#define BC_KEXTUNLOAD		"/sbin/kextunload"
#define BC_BUNDLE_ID		"com.apple.BootCache"

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
#define	BC_MAGIC_4	0x10102023	/* add pid/shared flag to history */
#define	BC_MAGIC_5	0x10102024	/* added crypto offset extents */
#define	BC_MAGIC	0x10102025	/* added groups for sorting */

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
#define BC_OP_SET_USER_TIMESTAMPS 0x09
#define BC_OP_SET_FUSION_OPTIMIZATION_STATS 0x0A
#define BC_OP_SET_HDD_OPTIMIZATION_STATS 0x0B
#define BC_OP_SET_HDD_OPTIMIZATION_STATE 0x0C
#define BC_OP_RESET    0x0D
#define BC_OP_START_NORECORDING 0x0E
#define BC_OP_SET_USER_OVERSIZE 0x0F
#define BC_OP_DEBUG_BUFFER 0x10
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
 * On-disk playlist header
 */
struct BC_playlist_header {
	int	ph_magic;
#define PH_MAGIC_0	0xa1b2c3d4	/* old format with no blocksize */
#define PH_MAGIC_1	0xa1b2c3d5	/* blocksize, offsets in bytes */
#define PH_MAGIC_2	0xa1b2c3d6	/* added flags field */
#define PH_MAGIC_3	0xa1b2c3d7	/* added non-root mounts */
#define PH_MAGIC_4	0xa1b2c3d8	/* added low-priority extents */
#define PH_MAGIC_5	0xa1b2c3d9	/* added crypto offset extents */
#define PH_MAGIC_6  0xa1b2c3e2  /* added OMAPs */
#define PH_MAGIC_7  0xa1b2c3e3  /* added groups for sorting */
#define PH_MAGIC    0xa1b2c3e4  /* add batch to omaps */
	uint ph_nmounts;
	uint ph_nentries;
	uint ph_nomaps;
};

/*
 * filesystem flags
 */
#define BC_FS_APFS                    (1<<0)    /* mount is apfs */
#define BC_FS_APFS_ENCRYPTED          (1<<1)    /* mount volume I/O is encrypted (non-CoreStorage) */
#define BC_FS_APFS_ENCRYPTION_ROLLING (1<<2)    /* mount volume in transition to/from encrypted */
#define BC_FS_APFS_CONTAINER          (1<<3)    /* mount is an apfs container */
#define BC_FS_APFS_FUSION             (1<<4)    /* mount is (or is contained by) an apfs fusion container */
#define BC_FS_CS                      (1<<5)    /* mount is a CoreStorage volume */
#define BC_FS_CS_FUSION               (1<<6)    /* mount is a CoreStorage Fusion (CPDK) volume */
#define BC_FS_APFS_VOLUME             (1<<7)    /* mount is an apfs volume */
#define BC_FS_APFS_SNAPSHOT           (1<<8)    /* mount is an apfs snapshot */

/*
 * Playlist mounts represent mount points to be cached.
 */
struct BC_playlist_mount {
	uuid_t pm_uuid;         /* The UUID of the mount */
	uuid_t pm_group_uuid;   /* If non-0, use this identifier for grouping extents when sorting rather than by mount */
	uint   pm_fs_flags;     /* filesystem flags */
	uint   pm_nentries;     /* The number of playlist entries that refer to this mount */
	uint   pm_nomaps;       /* Number of omap records that refer to this mount */
};

/*
 * Playlist entries represent regions of the mount points to be cached.
 */
struct BC_playlist_entry {
	u_int64_t	pe_offset;    /* byte address */
	u_int64_t	pe_length;    /* size in bytes */
	u_int16_t	pe_batch;     /* batch number */
	u_int16_t	pe_flags;     /* flags */
#define BC_PE_LOWPRIORITY 0x1
#define BC_PE_SHARED      0x2
	u_int32_t	pe_mount_idx; /* index of mount in mount array */
	u_int64_t	pe_crypto_offset; /* opaque crypto offset */
};

#ifndef KERNEL

struct BC_playlist_omap {
	apfs_omap_track_record_v2_t po_omap;      /* apfs omap track record */
	uint32_t                    po_mount_idx;  /* index of mount in mount array */
	
	/* XXX: For low pri metadata
	 *
	 * uint32_t                po_flags;
	 */
};

#endif

/* number of entries we copyin at a time */
#define BC_PLC_CHUNK	512

/* sanity check for the upper bound on number of entries and batches */
#define BC_MAXENTRIES	1000000
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
	uuid_t hm_group_uuid;   /* If non-0, use this identifier for grouping extents when sorting rather than by mount */
	uint   hm_fs_flags;     /* filesystem flags */
	uint   hm_nentries;     /* The number of history entries that refer to this mount */
};

struct BC_history_entry {
	u_int64_t   he_inode;     /* inode of the file */
	u_int64_t	he_offset;	  /* data offset on device */
	u_int64_t	he_length;	  /* length of data */
	u_int32_t	he_pid;       /* pid of the process that issued this IO */
	u_int16_t	he_flags;
#define	BC_HE_HIT        0x1 /* read was satisfied from cache */
#define BC_HE_WRITE      0x2 /* write-through operation */
#define BC_HE_TAG        0x4 /* userland-set tag */
#define BC_HE_SHARED     0x8 /* IO was from the shared cache */
#define BC_HE_OPTIMIZED 0x10 /* extent was provided by APFS after optimizations, not a raw I/O we caught */
	u_int16_t   he_mount_idx; /* index of the mount this read was on */
	u_int64_t	he_crypto_offset; /* opaque crypto offset */
};


/* Number of disks and batches we can collect statistics on */
#define STAT_DISKMAX	2
#define STAT_MOUNTMAX	16
#define STAT_BATCHMAX	8

/*
 * BC_OP_STATS - return statistics.
 *
 * The kernel's copy of the statistics is copied into
 * the BC_statictics structure at bc_data1
 */
struct BC_statistics {
	/* userspace timestamps, filled in by BootCacheControl */
	struct BC_userspace_timestamps {
		u_int64_t	ssup_launch_timestamp;	/* machabstime when userspace process launched */
		u_int64_t	ssup_oid_timestamp;		/* machabstime when userspace process OID lookup started */
	} userspace_timestamps;

	struct BC_userspace_fusion_optimizations {
		/* See fusion_history_* to calculate reads requested and reads already optimized */
		u_int64_t	ssup_reads_optimized;       /* number of reads this boot that were optimized */
		u_int64_t	ssup_inodes_requested;      /* number of inodes requested during boot */
		u_int64_t	ssup_inodes_optimized;      /* number of inodes optimized for next boot */
		u_int64_t	ssup_inodes_already_optimized;	/* number of inodes read this boot that were previously optimized */
		/* See fusion_history_* to calculate bytes requested and bytes already optimized */
		u_int64_t   ssup_bytes_requested;       /* number of bytes requested  during boot */
		u_int64_t	ssup_bytes_optimized;       /* number of bytes optimized for next boot */
	} userspace_fusion_optimizations;
	
	struct BC_userspace_hdd_optimizations {
		/* See hdd_history_* for reads requested */
		u_int64_t	ssup_reads_optimized;         /* number of reads this boot that were optimized */
		u_int64_t	ssup_reads_already_optimized; /* number of reads this boot that were previously optimized */
		u_int64_t	ssup_inodes_requested;	/* number of inodes read during boot */
		u_int64_t	ssup_inodes_optimized;	/* number of inodes optimized for next boot */
		u_int64_t	ssup_inodes_already_optimized;	/* number of inodes read this boot that were previously optimized */
		/* See hdd_history_* for bytes requested */
		u_int64_t   ssup_bytes_requested;       /* number of bytes requested during boot */
		u_int64_t	ssup_bytes_optimized;       /* number of bytes optimized for next boot */
		u_int64_t   ssup_bytes_surplus;         /* number of bytes optimized for next boot that wasn't read during boot */
		u_int64_t	ssup_bytes_nonoptimized;    /* number of bytes not optimized for next boot that was read in during boot */
		u_int64_t	ssup_bytes_already_optimized; /* number of bytes read this boot that were previously optimized */
		u_int64_t	ssup_optimization_range_length; /* size of region holding optimized bytes */
	} userspace_hdd_optimizations;

	struct BC_userspace_oversize {
		u_int64_t ssup_highpri_bytes_trimmed;
		u_int64_t ssup_lowpri_bytes_trimmed;
	} userspace_oversize;
	
	struct BC_userspace_hdd_optimization_state {
		u_int32_t   ssup_num_optimizations_attempted; /* number of hdd optimization passes attempted (including in-progress) */
		u_int32_t   ssup_optimization_state; /* 0: pending, 1: in progress, 2: paused, 3: completed,  */
#define	BC_HDD_OPTIMIZATION_STATE_PENDING	0 /* optimization has not yet started */
#define	BC_HDD_OPTIMIZATION_STATE_IN_PROGRESS	1 /* optimization is in progress */
#define	BC_HDD_OPTIMIZATION_STATE_PAUSED	2 /* optimization has been paused (partial completion) */
#define	BC_HDD_OPTIMIZATION_STATE_COMPLETE	3 /* optimization has been completed */
	} userspace_hdd_optimization_state;
	
	struct stat_numbers {
		/* readahead */
		u_int64_t	ss_initiated_reads;	/* read operations we initiated */
		u_int64_t	ss_read_blocks;		/* number of blocks read */
		u_int64_t	ss_read_bytes;		/* number of bytes read */
		u_int64_t	ss_read_errors;		/* read errors encountered */
		u_int64_t	ss_read_errors_bytes;	/* bytes discarded due to read errors */
		u_int64_t	ss_batch_bytes[STAT_DISKMAX][STAT_BATCHMAX+1];	/* number of bytes in read per batch, +1 for sum of extra batches */
		u_int64_t	ss_batch_late_bytes[STAT_DISKMAX][STAT_BATCHMAX+1];	/* number of bytes read during this batch
																	 that were scheduled for an earlier batch */
		u_int64_t	ss_batch_initiated_reads[STAT_DISKMAX][STAT_BATCHMAX+1];	/* read operations we initiated per batch */
		u_int64_t	ss_batch_time[STAT_DISKMAX][STAT_BATCHMAX+1];	/* machabstime per batch, +1 for sum of extra batches */
		
		u_int64_t	ss_read_bytes_lowpri;                         /* number of bytes read */
		u_int64_t	ss_batch_bytes_lowpri[STAT_DISKMAX];          /* number of bytes in read per disk */
		u_int64_t	ss_batch_time_lowpri[STAT_DISKMAX];           /* machabs per disk */
		u_int64_t	ss_batch_initiated_reads_lowpri[STAT_DISKMAX]; /* read operations we initiated per disk */
		
		/* inbound strategy calls (while we're recording) */
		u_int64_t	ss_strategy_calls;			/* total strategy calls we received */
		u_int64_t	ss_strategy_bypassed;		/* strategy calls we bypassed */
		u_int64_t	ss_strategy_nonread;		/* strategy calls that were not reads */
		u_int64_t	ss_strategy_throttled;		/* strategy calls marked low priority */
		u_int64_t	ss_strategy_noncached_mount;	/* strategy calls from non-cached mounts */
		u_int64_t	ss_strategy_unready_mount;	/* strategy calls from non-ready mounts */
		u_int64_t	ss_strategy_unknown;		/* strategy calls that were unidentifiable */
		u_int64_t	ss_strategy_unknown_bytes;	/* bytes of unidentifiable strategy calls */
		u_int64_t	ss_strategy_nonblocksize;	/* strategy calls of non-blocksize-multiple size */
		
		/* non-cached IOs */
		u_int64_t	ss_strategy_hit_nocache;	/* cache hits that were IOs we won't cache for next boot */
		u_int64_t	ss_strategy_bypass_nocache;	/* cache misses that were IOs we won't cache for next boot */
		u_int64_t	ss_strategy_bypass_duringio_nocache;	/* cache misses that were IOs we won't cache for next boot */
		u_int64_t	ss_hit_nocache_bytes;		/* bytes hit by IOs we won't cache for next boot */
		u_int64_t	ss_bypass_nocache_bytes;	/* bytes missed by IOs we won't cache for next boot */
		u_int64_t	ss_bypass_nocache_discards;	/* bytes discarded by IOs we won't cache for next boot */
		u_int64_t	ss_bypass_nocache_unread;	/* bytes unread because of IOs we won't cache for next boot */
		
		/* io during readahead */
		u_int64_t	ss_strategy_duringio;			/* total strategy calls */
		u_int64_t	ss_strategy_bypass_duringio;	/* strategy calls we bypassed */
		u_int64_t	ss_strategy_bypass_duringio_rootdisk_read;		/* read strategy calls we missed for the root disk */
		u_int64_t	ss_strategy_bypass_duringio_rootdisk_read_crypto_mismatch;		/* read strategy calls we missed due to crypto mismatch */
		u_int64_t	ss_strategy_bypass_duringio_rootdisk_read_partial_hits;		/* read strategy calls we missed due to only partial intersection */
		u_int64_t	ss_strategy_bypass_duringio_rootdisk_failure;	/* read strategy calls we hit but failed to fulfill for the root disk */
		u_int64_t	ss_strategy_bypass_duringio_rootdisk_nonread;	/* nonread strategy calls we bypassed for the root disk */
		u_int64_t	ss_strategy_forced_throttled;			/* strategy calls we forced to throttle due to cutting through our readahead */
		u_int64_t	ss_strategy_nonthrottled;				/* strategy calls that cut through our readahead but we did not throttle */
		u_int64_t	ss_hit_duringio;				/* cache hits during active readahead */
		u_int64_t   ss_strategy_bypass_duringio_unfilled;           /* strategy calls that hit an unfilled extent (for SSDs) */
		u_int64_t   ss_strategy_unfilled_lowpri;    /* strategy calls that hit an unfilled low priority extent */
		u_int64_t	ss_strategy_blocked;			/* strategy calls that blocked on future readhead */
		u_int64_t	ss_strategy_timedout;			/* strategy calls that timed out */
		u_int64_t	ss_strategy_time_longest_blocked;		/* longest time a strategy calls spent blocked on our cache (in milliseconds) */
		u_int64_t	ss_strategy_time_blocked;		/* milliseconds strategy calls spent blocked on our cache */
		
		/* extents */
		u_int64_t	ss_total_extents;	/* number of extents in the cache */
		u_int64_t	ss_extents_clipped;	/* number of extents clipped due to page boundaries */
		u_int64_t	ss_extent_lookups;	/* number of extents searched for */
		u_int64_t	ss_extent_crypto_mismatches;		/* number of extents missed due to crypto mismatch */
		u_int64_t	ss_extent_partial_hits;		/* number of extents missed due to only partial intersection */
		u_int64_t	ss_extent_hits;		/* number of extents matched (cache hits) */
		u_int64_t	ss_hit_multiple;	/* cache hits that touched more than one extent */
		u_int64_t	ss_hit_aborted;		/* cache hits not filled due to aborted extents */
		u_int64_t	ss_hit_blkmissing;	/* cache hits not filled due to missing blocks */
		u_int64_t	ss_hit_stolen;		/* cache hits not filled due to stolen pages */
		u_int64_t	ss_hit_failure;		/* cache hits not filled due to other failures */
		
		/* byte/page activity */
		u_int64_t	ss_requested_bytes;
		u_int64_t   ss_requested_bytes_m[STAT_MOUNTMAX];
		u_int64_t	ss_hit_bytes;		/* number of bytes vacated due to read hits */
		u_int64_t	ss_hit_bytes_m[STAT_MOUNTMAX];		/* number of bytes vacated due to read hits */
		u_int64_t	ss_stolen_discards;		/* number of bytes lost to pageout or contig mem */
		u_int64_t	ss_write_discards;	/* bytes discarded due to overwriting */
		u_int64_t	ss_read_discards;	/* bytes discarded due to incoming reads */
		u_int64_t	ss_error_discards;	/* bytes discarded due to error satisfying the I/O */
		u_int64_t	ss_close_discards;	/* bytes discarded due to mount being closed */
		u_int64_t	ss_lowpri_discards;	/* lowpri bytes discarded due to incomplete request */
		u_int64_t	ss_unable_to_discard_bytes;	/* bytes not discarded due to lock contention (high estimate) */
		u_int64_t	ss_unable_to_discard_count;	/* number of cut-through IOs with bytes not discarded due to lock contention */
		u_int64_t	ss_spurious_discards;	/* number of bytes read but not consumed */
		u_int64_t	ss_hit_bytes_afterhistory;		/* bytes fulfilled after history recording was complete */
		u_int64_t	ss_lost_bytes_afterhistory;	/* bytes lost after history recording was complete */
		
		/* cache size and bytes not read in */
		u_int64_t	ss_cache_bytes;		/* number of bytes total in cache (not necessarily read in) */
		u_int64_t	ss_cache_oversize;	/* number of bytes not added to cache due to size limit */
		u_int64_t	ss_write_unread;	/* bytes unread due to overwriting */
		u_int64_t	ss_read_unread;		/* bytes unread due to incoming reads */
		u_int64_t	ss_stolen_unread;		/* number of bytes lost to pageout or contig mem */
		u_int64_t	ss_readerror_unread;	/* bytes unread due to failure to read it */
		u_int64_t	ss_extenterror_unread;	/* bytes unread due to failure to setup/fill in extent */
		u_int64_t	ss_close_unread;	/* bytes unread due to mount being closed */
		u_int64_t	ss_error_unread;	/* bytes unread due to error satisfying I/O */
		u_int64_t	ss_lowpri_unread;	/* lowpri bytes unread due to request before it was available */
		u_int64_t	ss_spurious_unread;	/* number of bytes unread and not consumed */
		u_int64_t	ss_badreader_unread;/* bytes unread due to mount unable to start reader thread */
		u_int64_t	ss_mounterror_unread;	/* bytes unread due to failure to setup/fill in mount */
		u_int64_t	ss_nonroot_unread;	/* bytes unread due to not being on the root disk */
		u_int64_t	ss_unsupported_unread;	/* bytes unread due to unsupported configuration */

		/* history activity */
		u_int64_t	ss_history_writes;			/* number of writes we saw during initial boot */
		u_int64_t	ss_history_writes_bytes;			/* number of writes we saw during initial boot */
		u_int64_t	ss_history_reads;			/* number of reads we saw during initial boot */
		u_int64_t	ss_history_reads_bytes;			/* number of reads we saw during initial boot */
		u_int64_t	ss_history_reads_truncated; /* number of reads we saw but ignored due to truncation */
		u_int64_t	ss_history_reads_truncated_bytes; /* number of reads we saw but ignored due to truncation */
		u_int64_t	ss_history_reads_nomount;   /* number of reads we saw but had no mount */
		u_int64_t	ss_history_reads_nomount_bytes;   /* number of reads we saw but had no mount */
		u_int64_t	ss_history_reads_unknown;			/* history calls we couldn't find a mount for */
		u_int64_t	ss_history_reads_unknown_bytes;	/* bytes history calls we couldn't find a mount for */
		u_int64_t	ss_history_reads_no_blocksize;	/* history calls with 0 blocksize mounts */
		u_int64_t	ss_history_reads_no_blocksize_bytes;	/* bytes history calls with 0 blocksize mounts */
		u_int64_t	ss_history_reads_nonroot;	/* history calls with 0 blocksize mounts */
		u_int64_t	ss_history_reads_nonroot_bytes;	/* bytes history calls with 0 blocksize mounts */
		u_int64_t	ss_history_reads_ssd;	/* history calls with 0 blocksize mounts */
		u_int64_t	ss_history_reads_ssd_bytes;	/* bytes history calls with 0 blocksize mounts */
		u_int64_t	ss_history_entries;			/* number of history entries we've created this boot */
		u_int64_t	ss_history_entries_bytes;			/* number of bytes contained in the history we've seen for this boot */
		u_int64_t	ss_fusion_history_already_optimized_reads;	/* number of reads in the history that were previously optimized on fusion */
		u_int64_t	ss_fusion_history_already_optimized_bytes;	/* number of bytes in the history that were previously optimized on fusion */
		u_int64_t	ss_fusion_history_not_already_optimized_reads;	/* number of reads in the history that were not previously optimized on fusion */
		u_int64_t	ss_fusion_history_not_already_optimized_bytes;	/* number of bytes in the history that were not previously optimized on fusion */
		u_int64_t	ss_hdd_history_reads;	/* number of reads in the history that were read from an hdd */
		u_int64_t	ss_hdd_history_bytes;	/* number of bytes in the history that were read from an hdd */
	} ss_nonsharedcache; // Just non-shared cache

	struct stat_numbers ss_sharedcache;
	
	u_int64_t	ss_cache_time;		/* msecs cache was alive */
	u_int64_t	ss_load_timestamp;	/* machabstime when kext was loaded */
	u_int64_t	ss_bc_start_timestamp;	/* machabstime when BC_OP_START was issued */
	u_int64_t	ss_start_timestamp;	/* machabstime when cache started */

	u_int64_t   ss_readahead_threads; /* number of readahead threads */
	u_int64_t	ss_batch_time[STAT_DISKMAX][STAT_BATCHMAX+1];	/* machabs per batch, +1 for sum of extra batches */
	u_int64_t	ss_batch_time_lowpri[STAT_DISKMAX];           /* machabs per disk */

	u_int64_t	ss_cache_size;		/* the amount of memory allocated for the playback buffer */

	/* mounts */
	u_int64_t	ss_total_extents;	/* number of extents in the cache */
	u_int64_t	ss_total_mounts;	/* number of mounts in the cache */
	u_int64_t	ss_history_mount_no_uuid;	/* number of mounts seen without a uuid */
	u_int64_t	ss_history_mount_no_blocksize;	/* number of mounts seen without a blocksize */

	u_int64_t	ss_history_time;			/* msecs hisotry was active */
	u_int64_t	ss_history_num_recordings;	/* number of recordings we've has this boot */
	u_int64_t	ss_history_mounts;			/* number of allocated history mounts we had for the last recording */

	/* current status */
	u_int64_t	ss_cache_flags;		/* current cache flags */
#define	BC_FLAG_SETUP			(1 << 0)	/* cache setup properly during mount */
#define	BC_FLAG_CACHEACTIVE		(1 << 1)	/* cache is active, owns memory */
#define	BC_FLAG_HISTORYACTIVE	(1 << 2)	/* currently recording history */
#define	BC_FLAG_HTRUNCATED		(1 << 3)	/* history list truncated */
#define	BC_FLAG_SHUTDOWN		(1 << 4)	/* readahead shut down */

	uuid_t ss_mount_uuid[STAT_MOUNTMAX]; /* uuid of each mount referred to in stats above */
	
	char ss_playback_end_reason[64];
	char ss_cache_end_reason[64];
	char ss_history_end_reason[64];
};

#ifndef KERNEL

/*
 * In-memory BootCache playlist structure for userspace
 */
struct BC_playlist {
	uint                           p_nmounts;  /* number of mounts */
	uint                           p_nentries; /* number of entries */
	uint                           p_nomaps;   /* number of omap records */
	struct BC_playlist_mount      *p_mounts;   /* array of mounts */
	struct BC_playlist_entry      *p_entries;  /* array of entries */
	struct BC_playlist_omap       *p_omaps;    /* array of omap records */
};

/*
 * In-memory BootCache history structure
 */
struct BC_history {
	uint                     h_nmounts;  /* number of mounts */
	uint                     h_nentries; /* number of entries */
	struct BC_history_mount *h_mounts;   /* array of mounts */
	struct BC_history_entry *h_entries;  /* array of entries */
};

/*
 * In-memory OMAP history structure
 */
struct BC_omap_history_mount {
	uuid_t                       ohm_uuid;
	
	uint                         ohm_nomaps;
	apfs_omap_track_record_v2_t *ohm_omaps; /* All the omaps here point to the same mount above */
};
	
struct BC_omap_history {
	uint                          oh_nmounts;
	struct BC_omap_history_mount* oh_mounts; /* Array of mounts */
};

// Is the native shared cache explicitly added as high priority (vs low priority)
#define BC_ADD_SHARED_CACHE_AT_HIGH_PRIORITY 0

/*
 * Support library functions.
 */

/* For BC_playlist_for_file_extents */
struct bc_file_extent {
	off_t offset;    /* offset into file to start of extent, in bytes */
	off_t length;    /* length of the extent in bytes */
	u_int16_t flags; /* same as pe_flags */
};
	
// If NULL, log messages go to os_log
// If non-NULL, log messages go to the stream provided
// Defaults to NULL (os_log)
extern FILE* bc_log_stream;
	
struct bc_optimization_info;
	
extern int	BC_read_playlist(const char *, struct BC_playlist **);
extern int	BC_write_playlist(const char *, const struct BC_playlist *);
extern int	BC_merge_playlists(struct BC_playlist *, const struct BC_playlist *);
extern int  BC_playlists_intersect(const struct BC_playlist*, const struct BC_playlist*);
extern int  BC_playlist_for_filename(int fd, const char *fname, off_t maxsize, struct BC_playlist** ppc);
extern int  BC_playlist_for_file_extents(int fd, uint nextents, const struct bc_file_extent* extents, struct BC_playlist** ppc);
extern int  BC_sort_and_coalesce_playlist(struct BC_playlist* pc);
extern int  BC_verify_playlist(const struct BC_playlist *);
extern struct BC_playlist *BC_allocate_playlist(uint nmounts, uint nentries, uint nomaps);
extern void BC_reset_playlist(struct BC_playlist *);
extern struct BC_playlist *BC_copy_playlist(const struct BC_playlist *);
extern void BC_free_playlist(struct BC_playlist *);
#define PC_FREE_ZERO(pc) do { if (pc) { BC_free_playlist(pc); (pc) = NULL; } } while (0)
extern void BC_merge_history(struct BC_history *, const struct BC_history *);
extern struct BC_history *BC_copy_history(const struct BC_history *);
extern void BC_free_history(struct BC_history *);
#define HC_FREE_ZERO(hc) do { if (hc) { BC_free_history(hc); (hc) = NULL; } } while (0)
extern void BC_merge_omap_history(struct BC_omap_history *, const struct BC_omap_history *);
extern struct BC_omap_history *BC_copy_omap_history(const struct BC_omap_history *);
extern void BC_free_omap_history(struct BC_omap_history *);
#define OH_FREE_ZERO(oh) do { if (oh) { BC_free_omap_history(oh); (oh) = NULL; } } while (0)
extern int	BC_fetch_statistics(struct BC_statistics **);
extern int	BC_fetch_debug_buffer(char**, size_t *);
extern int	BC_set_userspace_oversize(const struct BC_userspace_oversize *);
extern int	BC_set_userspace_timestamps(const struct BC_userspace_timestamps *);
extern int	BC_set_userspace_fusion_optimization_stats(const struct BC_userspace_fusion_optimizations *);
extern int	BC_set_userspace_hdd_optimization_stats(const struct BC_userspace_hdd_optimizations *);
extern int BC_convert_history_and_omaps(const struct BC_history *, const struct BC_omap_history *, struct BC_playlist **);
extern int BC_optimize_history(struct BC_history *, struct bc_optimization_info**, bool);
extern int BC_pause_optimizations(void);
extern void BC_free_optimization_info(struct bc_optimization_info*);
#define OI_FREE_ZERO(oi) do { if (oi) { BC_free_optimization_info(oi); (oi) = NULL; } } while (0)
extern int BC_start(const struct BC_playlist *);
extern int BC_stop_and_fetch(struct BC_history **, struct BC_omap_history **);
#define BC_stop(hc) BC_stop_and_fetch(hc, NULL)
extern int	BC_notify_mount(void);
extern int	BC_test(void);
extern int	BC_jettison(void);
extern int	BC_print_statistics(char *, const struct BC_statistics *);
extern int	BC_print_history(char *, const struct BC_history *);
extern int	BC_print_omap_history(char *fname, const struct BC_omap_history *);
extern int	BC_print_playlist(const struct BC_playlist *, bool);
extern int	BC_tag_history(void);
extern int	BC_unload(void);
#endif // ifndef KERNEL

#ifdef __cplusplus
}
#endif
