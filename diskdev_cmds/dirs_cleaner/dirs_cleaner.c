/*
 * Copyright (c) 2021 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#include <removefile.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>
#include <sysexits.h>
#include <fcntl.h>
#include <sys/attr.h>
#include <sys/vnode.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/time.h>
#include <stdbool.h>
#include <pthread.h>

// TBD integrate with CacheDelete
//#include <CacheDelete/CacheDeletePrivate.h>
#define CACHE_DELETE_AUTO_PURGE_DIRECTORY	"/private/var/dirs_cleaner/"

#if defined(DEBUG) || defined(DIRS_CLEANER_TRACK)
#define DIRS_CLEANER_ERROR(res, fmt, ...) \
	(void)fprintf(stderr, "%s: " fmt " failed with errno=%d: %s\n", \
					__FUNCTION__, ##__VA_ARGS__, (res), strerror(res))
#else
#define DIRS_CLEANER_ERROR(res, fmt, ...)
#endif

#ifdef DIRS_CLEANER_TRACK
#define DIRS_CLEANER_MSG(res, ct, fmt, ...) \
	do { \
		if (!res) { \
			const dir_thread_ctx_t *tct = &ct->dc_thread; \
			(void)fprintf(stdout, "%18s:%14p dc_Xpos=%2u dtc_state=%2u dtc_rf_state=%14p" \
					" dtc_thread=%14p tct_dtc_step=%u " fmt "\n", \
					__FUNCTION__, pthread_self(), ct->dc_Xpos, ct->dc_state, \
					tct->dtc_rf_state, tct->dtc_thread, tct->dtc_step, ##__VA_ARGS__); \
		} \
	} while (0)
#else
#define DIRS_CLEANER_MSG(res, ct, fmt, ...)
#endif

#define DTC_ERR_EXIT(res)	dtc_err_exit(res, __FUNCTION__)

#define DC_TEMP_PATH_LEN		64	// Maximum template path length
#define DC_TEMP_PATH_NUMX		2	// Number of 'X' characters in template path
#define DC_MAX_SYNC_CLEAN_DUR	20	// Maximum duration of synchronous cleaning in seconds
#define DC_SYNC_CLEAN_STEP_DUR	4	// Interval in seconds to check if synchronous cleaning still needed
#define DC_MIN_FREESPACE_PERC	5	// Free space percentage
#define MIN_FREESPACE_LB		((uint32_t)1 << 28)	// Free Space lower bound in bytes
#define MIN_FREESPACE_UB		((uint32_t)1 << 30)	// Free Space upper bound in bytes

// Input directory attributes
typedef struct dir_gattrs {
	u_int32_t		dga_len;
	fsobj_type_t	dga_type;
	uid_t			dga_uid;
	gid_t			dga_gid;
	u_int32_t		dga_mode;
	u_int32_t		dga_prot_class;
} dir_gattrs_t;

// Temporary directory attributes to set
typedef struct dir_sattrs {
	uid_t			dsa_uid;
	gid_t			dsa_gid;
	u_int32_t		dsa_mode;
	u_int32_t		dsa_prot_class;
} dir_sattrs_t;

// Directory context mini-state bit positions
typedef enum {
	DC_STATE_DIRS_SYNC = 0,	// clean all input directories synchronously
	DC_STATE_DIR_SYNC,		// clean current directory synchronously
	DC_STATE_THREAD_INIT,	// thread part of directory cleaning context is initialized
	DC_STATE_THREAD_STOP	// stop partially synchronous cleaning timer thread
} dir_ctx_state_bit_t;

// Thread part of directory cleaning context
typedef struct dir_thread_ctx {
	struct timespec		dtc_sync_lim;	/* synchronous cleaning deadline */
	struct timespec		dtc_next_wakeup;/* next wakeup time */
	removefile_state_t	dtc_rf_state;	/* the only purpose is to cancel ongoing remove */
	pthread_t			dtc_thread;		/* synchronous cleaning timer thread */
	pthread_mutex_t		dtc_mutex;		/* synchronization mutex */
	pthread_cond_t		dtc_cond;		/* condition to wait on */
	uint32_t			dtc_step;		/* next wakeup granularity */
} dir_thread_ctx_t;

// Directory cleaning context
typedef struct dir_ctx {
	char				dc_res_path[PATH_MAX];			/* path the input path resolved to */
	char				dc_temp_path[DC_TEMP_PATH_LEN];	/* template path */
	struct attrlist		dc_attrs_list;	/* attributes to get/set */
	dir_thread_ctx_t	dc_thread;		/* directory cleaning thread context */
	dir_gattrs_t		dc_gattrs;		/* input directory attributes */
	uint32_t			dc_Xpos;		/* position of first 'X' character in dc_temp_path */
	uint32_t			dc_state;		/* combination of bits in dir_ctx_state_bit_t positions */
} dir_ctx_t;


// dtc - directory thread context
static inline void
dtc_err_exit(int res, const char *tag)
{
	if (res) {
		fprintf(stderr, "dirs_cleaner: tag=%s err=%d err_str=%s\n", tag, res, strerror(res));
		exit(EX_OSERR);
	}
}

// Group of dtc pthread wrapper routines
static inline void
dtc_lock(dir_thread_ctx_t *tctx)
{
	int res = pthread_mutex_lock(&tctx->dtc_mutex);

	DTC_ERR_EXIT(res);
}

static inline void
dtc_unlock(dir_thread_ctx_t *tctx)
{
	int res = pthread_mutex_unlock(&tctx->dtc_mutex);

	DTC_ERR_EXIT(res);
}

static inline int
dtc_timedwait(dir_thread_ctx_t *tctx)
{
	int res = pthread_cond_timedwait(&tctx->dtc_cond, &tctx->dtc_mutex, &tctx->dtc_next_wakeup);

	if (res != ETIMEDOUT)
		DTC_ERR_EXIT(res);

	return res;
}

static inline void
dtc_signal(dir_thread_ctx_t *tctx)
{
	int res = pthread_cond_signal(&tctx->dtc_cond);

	DTC_ERR_EXIT(res);
}

// Translate timespan "num_sec from now" into absolute timespec ts. Take
// absolute time limit tl into consideration, so ts cannot be greater than tl.
// Return true if ts was successfully calculated, false otherwise
static bool
dtc_timespan2timespec(struct timespec *ts, uint32_t num_sec, const struct timespec *tl)
{
	struct timeval tv;

	if (gettimeofday(&tv, NULL)) {
		__unused int res = errno;

		DIRS_CLEANER_ERROR(res, "gettimeofday(...)");
		ts->tv_sec = 0;
	}
	else {
		if (tl)
			ts->tv_sec = (tv.tv_sec < tl->tv_sec) ?
							tv.tv_sec + MIN(num_sec, tl->tv_sec - tv.tv_sec) : 0;
		else
			ts->tv_sec = tv.tv_sec + num_sec;
	}

	return (ts->tv_sec);
}

static bool dc_should_reclaim(dir_ctx_t *ctx);
static void dc_state_set_thread_stop(dir_ctx_t *ctx);
static bool dc_state_is_thread_stop(const dir_ctx_t *ctx);

// Limit duration of the synchronous cleaning. This is main function of
// partially synchronous cleaning timer thread. Synchronous cleaning duration is
// limited by DC_MAX_SYNC_CLEAN_DUR seconds. Cleaning stops even earlier if enough
// used space is reclaimed.
static void *
dtc_timer(void *arg)
{
	dir_ctx_t *ctx = (dir_ctx_t *)arg;
	dir_thread_ctx_t *tctx = &ctx->dc_thread;
	int res = 0;

	dtc_lock(tctx);
	while (!dc_state_is_thread_stop(ctx))
		if ((res = dtc_timedwait(tctx)) == ETIMEDOUT && !dc_should_reclaim(ctx)) {
			if (removefile_cancel(tctx->dtc_rf_state)) {
				res = errno;
				DIRS_CLEANER_ERROR(res, "removefile_cancel(...)");
			}
			dc_state_set_thread_stop(ctx);
		}
	dtc_unlock(tctx);

	DIRS_CLEANER_MSG(0, ctx, "res=%d", res);

	return NULL;
}

// dc - directory context
// Group of directory context state management functions
static inline void
dc_state_set(dir_ctx_t *ctx, dir_ctx_state_bit_t bit)
{
	ctx->dc_state |= (1u << bit);
}

static inline void
dc_state_clear(dir_ctx_t *ctx, dir_ctx_state_bit_t bit)
{
	ctx->dc_state &= ~(1u << bit);
}

static inline bool
dc_state_is_set(const dir_ctx_t *ctx, dir_ctx_state_bit_t bit)
{
	return (ctx->dc_state & (1u << bit));
}

static inline void
dc_state_set_dirs_sync(dir_ctx_t *ctx)
{
	dc_state_set(ctx, DC_STATE_DIRS_SYNC);
}

static inline bool
dc_state_is_dirs_sync(const dir_ctx_t *ctx)
{
	return dc_state_is_set(ctx, DC_STATE_DIRS_SYNC);
}

static inline void
dc_state_set_dir_sync(dir_ctx_t *ctx)
{
	dc_state_set(ctx, DC_STATE_DIR_SYNC);
}

static inline void
dc_state_clear_dir_sync(dir_ctx_t *ctx)
{
	dc_state_clear(ctx, DC_STATE_DIR_SYNC);
}

static inline bool
dc_state_is_dir_sync(const dir_ctx_t *ctx)
{
	return dc_state_is_set(ctx, DC_STATE_DIR_SYNC);
}

static inline void
dc_state_set_thread_init(dir_ctx_t *ctx)
{
	dc_state_set(ctx, DC_STATE_THREAD_INIT);
}

static inline bool
dc_state_is_thread_init(const dir_ctx_t *ctx)
{
	return dc_state_is_set(ctx, DC_STATE_THREAD_INIT);
}

static inline void
dc_state_set_thread_stop(dir_ctx_t *ctx)
{
	dc_state_set(ctx, DC_STATE_THREAD_STOP);
}

static inline bool
dc_state_is_thread_stop(const dir_ctx_t *ctx)
{
	return dc_state_is_set(ctx, DC_STATE_THREAD_STOP);
}

// Swap input and temporary directories
static void
dc_swap_dirs(dir_ctx_t *ctx)
{
	dir_sattrs_t dir_sattrs;
	const dir_gattrs_t *dir_gattrs = &ctx->dc_gattrs;
	const char *swap_path = ctx->dc_temp_path;
	int res = 0;

	// Preserve certain input directory attributes.
	ctx->dc_attrs_list.commonattr = ATTR_CMN_OWNERID | ATTR_CMN_GRPID | ATTR_CMN_ACCESSMASK | ATTR_CMN_DATA_PROTECT_FLAGS;
	dir_sattrs.dsa_uid = dir_gattrs->dga_uid;
	dir_sattrs.dsa_gid = dir_gattrs->dga_gid;
	dir_sattrs.dsa_mode = dir_gattrs->dga_mode;
	dir_sattrs.dsa_prot_class = dir_gattrs->dga_prot_class;

	if (setattrlist(swap_path, &ctx->dc_attrs_list, &dir_sattrs, sizeof(dir_sattrs), 0)) {
		res = errno;
		DIRS_CLEANER_ERROR(res, "setattrlist(%s, ...)", swap_path);
	} else if (renamex_np(ctx->dc_res_path, swap_path, RENAME_SWAP)) {
		res = errno;
		DIRS_CLEANER_ERROR(res, "renamex_np(%s, %s, RENAME_SWAP)", ctx->dc_res_path, swap_path);
	}

	if (res) {
		// Cannot set attrs or swap, switch to synchronous cleaning and remove swap dir
		dc_state_set_dir_sync(ctx);
		if (rmdir(swap_path)) {
			res = errno;
			DIRS_CLEANER_ERROR(res, "rmdir(%s)", swap_path);
		}
	}

	DIRS_CLEANER_MSG(res, ctx, "dc_res_path=%s swap_path=%s", ctx->dc_res_path, swap_path);
}

/*
 * Reset directory cleaning context:
 * - resolve input path
 * - get input directory attributes
 * - create template root directory
 * - create temporary directory
 * - swap input and temporary directories
 */
static int
dc_reset(dir_ctx_t *ctx, const char *path)
{
	int res = 0;

	dc_state_clear_dir_sync(ctx);

	if (realpath(path, ctx->dc_res_path)) {
		ctx->dc_attrs_list.commonattr  = ATTR_CMN_OBJTYPE | ATTR_CMN_OWNERID |
				ATTR_CMN_GRPID | ATTR_CMN_ACCESSMASK | ATTR_CMN_DATA_PROTECT_FLAGS;
		if (getattrlist(ctx->dc_res_path, &ctx->dc_attrs_list, &ctx->dc_gattrs, sizeof(ctx->dc_gattrs), 0))
			res = errno;
		else if (ctx->dc_gattrs.dga_type == VDIR)
			ctx->dc_gattrs.dga_mode &= ~S_IFMT;
		else
			res = errno = ENOTDIR;
	} else {
		res = errno;
	}

	DIRS_CLEANER_MSG(res, ctx, "st_mode=%u st_uid=%u st_gid=%u da_prot_class=%d path=%s dc_res_path=%s",
					 ctx->dc_gattrs.dga_mode,ctx->dc_gattrs.dga_uid, ctx->dc_gattrs.dga_gid,
					 ctx->dc_gattrs.dga_prot_class, path, ctx->dc_res_path);

	if (!res) {
		if (dc_state_is_dirs_sync(ctx)) {
			dc_state_set_dir_sync(ctx);
		} else {
			memset(&ctx->dc_temp_path[ctx->dc_Xpos], 'X', DC_TEMP_PATH_NUMX);

			if (mkdtemp(ctx->dc_temp_path)) {
				dc_swap_dirs(ctx);
			} else {
				res = errno;
				DIRS_CLEANER_ERROR(res, "mkdtemp(%s)", ctx->dc_temp_path);
				res = 0;
				dc_state_set_dir_sync(ctx);
			}
		}
	}

	return res;
}

_Static_assert(DC_TEMP_PATH_LEN > DC_TEMP_PATH_NUMX + 1, "Template path must be longer");

// Prepare template path
static bool
dc_prep_temp_dir(dir_ctx_t *ctx, const char *temp_path)
{
	size_t temp_len = DC_TEMP_PATH_LEN - DC_TEMP_PATH_NUMX - 1;
	size_t len = strlcpy(ctx->dc_temp_path, temp_path, temp_len);

	if (len && len < temp_len) {
		if (ctx->dc_temp_path[len - 1] != '/') {
			ctx->dc_temp_path[len++] = '/';
			ctx->dc_temp_path[len] = 0;
		}

		if (mkdir(ctx->dc_temp_path, 0700)) {
			int res = errno;

			if (res != EEXIST) {
				DIRS_CLEANER_ERROR(res, "mkdir(%s, 0700)", ctx->dc_temp_path);
				len = 0;
			}
		}
	} else {
		DIRS_CLEANER_ERROR(EINVAL, "strlcpy(%s, %s, %zu)", ctx->dc_temp_path, temp_path, len);
		len = 0;
	}

	if (len) {
		ctx->dc_temp_path[len + DC_TEMP_PATH_NUMX] = 0;
		ctx->dc_Xpos = (uint32_t)len;
		return false;
	}

	ctx->dc_temp_path[0] = 0;
	return true;
}

// Construct directory cleaning context
static void
dc_construct(dir_ctx_t *ctx, const char *temp_path, bool init)
{
	memset(ctx, 0, sizeof(*ctx));

	if (dc_prep_temp_dir(ctx, temp_path) || (init &&
		!dtc_timespan2timespec(&ctx->dc_thread.dtc_sync_lim, DC_MAX_SYNC_CLEAN_DUR, NULL)))
		dc_state_set_dirs_sync(ctx);

	ctx->dc_attrs_list.bitmapcount = ATTR_BIT_MAP_COUNT;

	DIRS_CLEANER_MSG(0, ctx, "dc_temp_path=%s", ctx->dc_temp_path);
}

// Decide if used space need to be synchronously reclaimed
static bool
dc_should_reclaim(dir_ctx_t *ctx)
{
	dir_thread_ctx_t *tctx = &ctx->dc_thread;
	bool bres = dtc_timespan2timespec(&tctx->dtc_next_wakeup, tctx->dtc_step, &tctx->dtc_sync_lim);

	if (bres) {
		struct statfs statfs_buf;

		if (statfs(ctx->dc_res_path, &statfs_buf)) {
			__unused int res = errno;

			DIRS_CLEANER_ERROR(res, "statfs(%s, ...)", ctx->dc_res_path);
			bres = false;
		} else {
			uint64_t min_free_space = (statfs_buf.f_blocks * DC_MIN_FREESPACE_PERC) / 100;
			const uint32_t min_free_space_lb = MIN_FREESPACE_LB / statfs_buf.f_bsize;
			const uint32_t min_free_space_ub = MIN_FREESPACE_UB / statfs_buf.f_bsize;

			if (min_free_space < min_free_space_lb)
				min_free_space = min_free_space_lb;
			else if (min_free_space_ub < min_free_space)
				min_free_space = min_free_space_ub;

			if (min_free_space <= statfs_buf.f_bfree)
				bres = false;

			DIRS_CLEANER_MSG(0, ctx, "min_free_space=%llu statfs_buf.f_bfree=%llu bres=%d",
							 min_free_space, statfs_buf.f_bfree, bres);
		}
	}

	DIRS_CLEANER_MSG(0, ctx, "dc_temp_path=%s bres=%d tv_sec=%ld lim.tv_sec",
					 ctx->dc_temp_path, bres, tctx->dtc_next_wakeup.tv_sec, tctx->dtc_sync_lim.tv_sec);

	return bres;
}

// Finalize thread part of the directory cleaning context
static void
dc_fini_thread_ctx(dir_ctx_t *ctx)
{
	dir_thread_ctx_t *tctx = &ctx->dc_thread;

	if (dc_state_is_thread_init(ctx)) {
		dtc_lock(tctx);
		if (!dc_state_is_thread_stop(ctx)) {
			dc_state_set_thread_stop(ctx);
			dtc_signal(tctx);
		}
		dtc_unlock(tctx);
		(void)pthread_join(tctx->dtc_thread, NULL);
		(void)pthread_cond_destroy(&tctx->dtc_cond);
		(void)pthread_mutex_destroy(&tctx->dtc_mutex);
	}

	(void)removefile_state_free(tctx->dtc_rf_state);
}

// Initialize thread part of the directory cleaning context
static bool
dc_init_thread_ctx(dir_ctx_t *ctx)
{
	int res;
	dir_thread_ctx_t *tctx = &ctx->dc_thread;

	ctx->dc_temp_path[ctx->dc_Xpos] = 0;

	if (!(tctx->dtc_rf_state = removefile_state_alloc())) {
		res = ENOMEM;
		DIRS_CLEANER_ERROR(res, "removefile_state_alloc(...)");
		return false;
	}

	if ((res = pthread_mutex_init(&tctx->dtc_mutex, NULL))) {
		DIRS_CLEANER_ERROR(res, "pthread_mutex_init(...)");
	} else if ((res = pthread_cond_init(&tctx->dtc_cond, NULL))) {
		DIRS_CLEANER_ERROR(res, "pthread_cond_init(...)");
		(void)pthread_mutex_destroy(&tctx->dtc_mutex);
	} else {
		dtc_lock(tctx);
		if ((res = pthread_create(&tctx->dtc_thread, NULL, dtc_timer, ctx))) {
			DIRS_CLEANER_ERROR(res, "pthread_create(...)");
		} else {
			tctx->dtc_step = DC_SYNC_CLEAN_STEP_DUR;
			dc_state_set_thread_init(ctx);

			DIRS_CLEANER_MSG(res, ctx, "dc_temp_path=%s", ctx->dc_temp_path);
		}
		dtc_unlock(tctx);
		if (res) {
			(void)pthread_cond_destroy(&tctx->dtc_cond);
			(void)pthread_mutex_destroy(&tctx->dtc_mutex);
		}
	}

	return (!res);
}

// Used temporarily to avoid dependency issues against removefile rev'ing an enum
// at the same time. Delete this #define and replace DC_LONGPATHS with
// REMOVEFILE_ALLOW_LONG_PATHS once removefile is safely in a build.
// See rdar://76152973
#define DC_LONGPATHS (1 << 8)

// Partially synchronously clean temporary directories
static void
dc_clean_part_sync(dir_ctx_t *ctx)
{
	dir_thread_ctx_t *tctx = &ctx->dc_thread;

	if(dc_init_thread_ctx(ctx) &&
	   removefile(ctx->dc_temp_path, tctx->dtc_rf_state, REMOVEFILE_RECURSIVE |
				  REMOVEFILE_KEEP_PARENT | DC_LONGPATHS)) {
		__unused int res = errno;

		if (res == ECANCELED)
			DIRS_CLEANER_MSG(0, ctx, "dc_temp_path=%s res=ECANCELED", ctx->dc_temp_path);
		else
			DIRS_CLEANER_ERROR(res, "removefile(%s, ...)", ctx->dc_res_path);
	}

	dc_fini_thread_ctx(ctx);
}

// Synchronously clean input or temporary directories
static int
dc_clean_sync(dir_ctx_t *ctx, bool input_path)
{
	int res = 0;
	const char *path = input_path ? ctx->dc_res_path : ctx->dc_temp_path;

	if (removefile(path, NULL, REMOVEFILE_RECURSIVE | REMOVEFILE_KEEP_PARENT | DC_LONGPATHS)) {
		res = errno;
		DIRS_CLEANER_ERROR(res, "removefile(%s, NULL, ...)", path);
	}

	DIRS_CLEANER_MSG(res, ctx, "dc_res_path=%s", path);

	return res;
}

static void
usage()
{
	fprintf(stderr, "usage: dirs_cleaner [--init] directory ...\n");
	exit(EX_USAGE);
}

/*
 * dirs_cleaner wipes out contents of input directories one by one. Input directories
 * themselves are not removed. If argv[1] equal to "--init", cleaner is called as
 * part of the system initialization (boot).
 *
 * If conditions permit, empty temporary directory will be created under dc_temp_path.
 * Input directory is swapped with temporay directory then. Certain input directory
 * attributes are preserved during the swap. When swap is completed, first phase
 * of cleaning is done, as input directory is empty now. Since used space was not
 * actually reclamed this kind of cleaning is asynchronous.
 *
 * In certain cases swap cannot be done. Input directory is cleaned synchronously
 * in place then. Since space is reclaimed before dirs_cleaner returns, this kind
 * of cleaning is synchronous. Generally speaking, dirs_cleaner run is a combination
 * of synchronous and asynchronous directory contents removals.
 *
 * Once all directories are processed, second phase of the cleaning may be needed.
 * If cleaner is called during the boot and at least one directory was cleaned
 * asynchronously, free space in a file system, containing path dc_temp_path, is
 * checked. If available free space does not satisfy minimum free space requirement,
 * part of the used space is reclaimed synchronously in attempt to provide sufficient
 * amount of free space for boot continuation. The reclamation process stops if
 * enough space is reclaimed or reclamation timer expires.
 *
 * As for now, the only use case for dirs_cleaner is to (preferrably) asynchronously
 * clean certain directories during boot process and, therefore, decrease boot time.
 * CacheDelete is expected to actually wipe out entire name space tree rooted at
 * dc_temp_path.
 *
 * dirs_cleaner returns an error when an input directory cannot be cleaned (made empty).
 * Otherwise, it does return success (0). None, part, or all name spaces rooted
 * at input directories were actually removed by time of return.
 */
int
main(int argc, char *argv[])
{
	dir_ctx_t ctx;
	int init, res, comb_res = 0;
	bool clean_async = false;

	if (argc == 1)
		usage();

	init = strcmp(argv[1], "--init") ? 0 : 1;
	if (init == 1 && argc == 2)
		usage();

	dc_construct(&ctx, CACHE_DELETE_AUTO_PURGE_DIRECTORY, init);

	for (int i = 1 + init; i < argc; i++) {
		if ((res = dc_reset(&ctx, argv[i])) || (dc_state_is_dir_sync(&ctx) && (res = dc_clean_sync(&ctx, true)))) {
			fprintf(stderr, "dirs_cleaner: %s: %s\n", argv[i], strerror(res));
			if (!comb_res)
				comb_res = res;
		}
		if (!res && !dc_state_is_dir_sync(&ctx))
			clean_async = true;
	}

	if (init) {
		if (comb_res) {
			if (!dc_state_is_dirs_sync(&ctx)) {
				ctx.dc_temp_path[ctx.dc_Xpos] = 0;
				(void)dc_clean_sync(&ctx, false);
			}
		} else if (clean_async && dc_should_reclaim(&ctx)) {
			dc_clean_part_sync(&ctx);
		}
	}

	if (comb_res)
		comb_res = EX_NOINPUT;

	exit(comb_res);
}
