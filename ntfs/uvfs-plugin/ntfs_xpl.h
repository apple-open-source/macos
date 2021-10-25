//
//  ntfs_xpl.h
//  NTFSUVFSPlugIn
//
//  Created by Erik Larsson on 2020-01-01.
//  Copyright © 2020 Tuxera Inc. All rights reserved.
//

#ifndef NTFS_XPL_H
#define NTFS_XPL_H

#include "ntfs_types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>

#include <fcntl.h>
#include <pthread.h>

#include <sys/kernel_types.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/uio.h>
#include <mach/mach_types.h>

#include <os/log.h>

#undef PAGE_SIZE
#undef PAGE_MASK
#undef PAGE_SHIFT

#define PAGE_SIZE   16384
#define PAGE_MASK   (PAGE_SIZE - 1)
#define PAGE_SHIFT  14
/*
 * XPL - XNU Porting Layer
 *
 * This layer implements the XNU interfaces needed for the NTFS kernel driver
 * (in the future possibly others as well) to make it possible for the userspace
 * VFS port to reuse as much existing code as possible.
 */

#if 0
#define xpl_trace_enter(fmt, ...) \
	os_log_debug(OS_LOG_DEFAULT, "%s: Entering%s" fmt "...\n", __FUNCTION__, (fmt)[0] ? " with " : "", ##__VA_ARGS__)
#else
#define xpl_trace_enter(fmt, ...) do {} while(0)
#endif

#if 0
#define xpl_debug(fmt, ...) \
	os_log_debug(OS_LOG_DEFAULT, "%s:%s: " fmt, "ntfs", __FUNCTION__, ##__VA_ARGS__)
#else
#define xpl_debug(fmt, ...) do {} while(0)
#endif

#define xpl_info(fmt, ...) \
	os_log_info(OS_LOG_DEFAULT, "%s:%s: " fmt, "ntfs", __FUNCTION__, ##__VA_ARGS__)
#define xpl_perror(err, fmt, ...) \
	os_log_error(OS_LOG_DEFAULT, "%s:%s: " fmt ": %s (%d)", "ntfs", __FUNCTION__, ##__VA_ARGS__, strerror(err), (err))

#define xpl_error(fmt, ...) \
	os_log_error(OS_LOG_DEFAULT, "%s:%s: " fmt, "ntfs", __FUNCTION__, ##__VA_ARGS__)

#if SIMPLE_RWLOCK
#define lck_rw_t pthread_mutex_t
#else
#define lck_rw_t pthread_rwlock_t
#endif

__attribute__((visibility("hidden"))) extern const char ntfs_dev_email[];
__attribute__((visibility("hidden"))) extern const char ntfs_please_email[];

typedef struct {
	int (*vnop_access)(void *);
	int (*vnop_advlock)(void *);
	int (*vnop_allocate)(void *);
	int (*vnop_blktooff)(void *);
	int (*vnop_blockmap)(void *);
	int (*vnop_bwrite)(void *);
	int (*vnop_close)(void *);
	int (*vnop_copyfile)(void *);
	int (*vnop_create)(void *);
	int (*vnop_default)(void *);
	int (*vnop_exchange)(void *);
	int (*vnop_fsync)(void *);
	int (*vnop_getattr)(void *);
	int (*vnop_getnamedstream)(void *);
	int (*vnop_getxattr)(void *);
	int (*vnop_inactive)(void *);
	int (*vnop_ioctl)(void *);
	int (*vnop_link)(void *);
	int (*vnop_listxattr)(void *);
	int (*vnop_lookup)(void *);
	int (*vnop_makenamedstream)(void *);
	int (*vnop_mkdir)(void *);
	int (*vnop_mknod)(void *);
	int (*vnop_mmap)(void *);
	int (*vnop_mnomap)(void *);
	int (*vnop_offtoblk)(void *);
	int (*vnop_open)(void *);
	int (*vnop_pagein)(void *);
	int (*vnop_pageout)(void *);
	int (*vnop_pathconf)(void *);
	int (*vnop_read)(void *);
	int (*vnop_readdir)(void *);
	int (*vnop_readdirattr)(void *);
	int (*vnop_readlink)(void *);
	int (*vnop_reclaim)(void *);
	int (*vnop_remove)(void *);
	int (*vnop_removenamedstream)(void *);
	int (*vnop_removexattr)(void *);
	int (*vnop_rename)(void *);
	int (*vnop_revoke)(void *);
	int (*vnop_rmdir)(void *);
	int (*vnop_searchfs)(void *);
	int (*vnop_select)(void *);
	int (*vnop_setattr)(void *);
	int (*vnop_setxattr)(void *);
	int (*vnop_strategy)(void *);
	int (*vnop_symlink)(void *);
	int (*vnop_write)(void *);
} xpl_vnop_table;

__attribute__((visibility("hidden"))) extern xpl_vnop_table *xpl_vnops;

__attribute__((visibility("hidden"))) extern int fsadded;
__attribute__((visibility("hidden"))) extern struct vfs_fsentry fstable;
__attribute__((visibility("hidden"))) extern vfstable_t vfshandles;

struct vnode {
	unsigned int hasdirtyblks : 1;
	unsigned int issystem : 1;
	unsigned int isreg : 1;
	unsigned int isblk : 1;
	unsigned int ischr : 1;
	unsigned int isnocache : 1;
	unsigned int isnoreadahead : 1;
	unsigned int vnode_external : 1;
	enum vtype vtype;
	mount_t mp;
	void *fsnode;
	proc_t proc;
	uint64_t iocount;
	uint64_t usecount;
	uint64_t size;
	off_t datasize;
	char *nodename;
	vnode_t parent;
	lck_rw_t vnode_lock;
};

struct vfs_context {
	proc_t proc;
	kauth_cred_t ucred;
};

#define upl_t xpl_upl_t
typedef struct xpl_upl *xpl_upl_t;

/* Compatibility defines. */

typedef void* OSMallocTag;
#define OSMalloc_Tagalloc(tag, policy) ((OSMallocTag*) 1)
#define OSMalloc_Tagfree(tag) do {} while(0)

#ifdef XPL_MEMORY_DEBUG
extern size_t xpl_allocated;
extern size_t xpl_allocations;

extern OSMallocTag ntfs_malloc_tag;

extern void* _OSMalloc(const char *file, int line, size_t size,
		OSMallocTag tag);

extern void _OSFree(const char *file, int line, void *ptr, size_t size,
		OSMallocTag tag);

#define OSMalloc(size, tag) _OSMalloc(__FILE__, __LINE__, size, tag)
#define OSFree(ptr, size, tag) _OSFree(__FILE__, __LINE__, ptr, size, tag)
#else
#define OSMalloc(size, tag) malloc(size)
#define OSFree(ptr, size, tag) free(ptr)
#endif

/* These are obvious no-ops in userspace, only defined to make the code compile
 * with minimal changes. */
#define OSKextRetainKextWithLoadTag(tag) do {} while(0)
#define OSKextReleaseKextWithLoadTag(tag) do {} while(0)
#define OSKextGetCurrentLoadTag() do {} while(0)

/* For our purposes we can assume that whoever is accessing the filesystem
 * operations has got superuser access. */
static inline boolean_t kauth_cred_issuser(kauth_cred_t cred)
{
	(void)cred;
	return TRUE;
}

static inline uid_t kauth_cred_getuid(kauth_cred_t cred)
{
	(void)cred;
	return 0;
}

/* Simple rwlock is an initial implementation with a simple mutex, meaning
 * multiple readers isn't possible. This of course hurts performance under
 * highly parallel workloads. */
#define SIMPLE_RWLOCK 0

struct lck_grp_attr_t;
typedef struct lck_grp_attr_t lck_grp_attr_t;

#define lck_grp_attr_alloc_init() ((lck_grp_attr_t*) 1)
#define lck_grp_attr_setstat(...) do {} while(0)
#define lck_grp_attr_free(attr) do {} while(0)

struct lck_grp_t;
typedef struct lck_grp_t lck_grp_t;

#define lck_grp_alloc_init(tag, attr) ((lck_grp_t*) 1)
#define lck_grp_free(grp) do {} while(0)

#define lck_mtx_t pthread_mutex_t
#define lck_attr_t pthread_mutexattr_t
#if SIMPLE_RWLOCK
#define lck_rwattr_t pthread_mutexattr_t
#else
#define lck_rwattr_t pthread_rwlockattr_t
#endif

static inline lck_attr_t* lck_attr_alloc_init(void)
{
	lck_attr_t *lck_alloc;
	int err;

	xpl_trace_enter("");
	lck_alloc = malloc(sizeof(lck_attr_t));
	if (!lck_alloc) {
		xpl_perror(errno, "Error while allocating lck_attr_t");
		return NULL;
	}

	err = pthread_mutexattr_init(lck_alloc);
	if (err) {
		xpl_perror(err, "Error while initializating "
			 "pthread_mutexattr_t");
		free(lck_alloc);
	}

	return err ? NULL : lck_alloc;
}

#define lck_attr_setdebug(attr) do {} while(0)

static inline void lck_attr_free(lck_attr_t *attr)
{
	xpl_trace_enter("attr=%p", attr);
	pthread_mutexattr_destroy(attr);
	free(attr);
}

static inline lck_rwattr_t* lck_rwattr_alloc_init(void)
{
	lck_rwattr_t *lck_alloc;
	int err;

	xpl_trace_enter("");

	lck_alloc = malloc(sizeof(lck_rwattr_t));
	if (!lck_alloc) {
		xpl_perror(errno, "Error while allocating lck_rwattr_t");
		return NULL;
	}

#if SIMPLE_RWLOCK
	err = pthread_mutexattr_init(lck_alloc);
#else
	err = pthread_rwlockattr_init(lck_alloc);
#endif
	if (err) {
		xpl_perror(err, "Error while initializing lck_rwattr_t");
		free(lck_alloc);
	}

	return err ? NULL : lck_alloc;
}

static inline void lck_rwattr_free(lck_rwattr_t *attr)
{
	int err;

	xpl_trace_enter("attr=%p", attr);

#if SIMPLE_RWLOCK
	err = pthread_mutexattr_destroy(attr);
#else
	err = pthread_rwlockattr_destroy(attr);
#endif
	if (err) {
		xpl_perror(err, "Error while destroying lck_rwattr_t");
	}

	free(attr);
}

#define lck_attr_free(attr) pthread_mutexattr_destroy(attr)

__attribute__((visibility("hidden"))) extern lck_attr_t *ntfs_lock_attr;

__attribute__((visibility("hidden"))) extern lck_rwattr_t *ntfs_lock_attr_rw;

#define lck_mtx_init(lock, group, attr) pthread_mutex_init(lock, attr)
#define lck_mtx_lock(lck) pthread_mutex_lock(lck)
#define lck_mtx_unlock(lck) pthread_mutex_unlock(lck)
#define lck_mtx_destroy(lock, group) pthread_mutex_destroy(lock)

typedef unsigned int     lck_rw_type_t;

#define LCK_RW_TYPE_SHARED                      0x01
#define LCK_RW_TYPE_EXCLUSIVE           0x02

#define lck_rw_init(lock, group, attr) _lck_rw_init(lock, attr##_rw)
static inline void _lck_rw_init(lck_rw_t *lck, lck_rwattr_t *attr)
{
	int err;

	xpl_trace_enter("lck=%p attr=%p", lck, attr);

#if SIMPLE_RWLOCK
	err = pthread_mutex_init(lck, attr);
#else
	err = pthread_rwlock_init(lck, attr);
#endif
	if (err) {
		xpl_perror(err, "Error while initializating lck_rw_t");
	}
}

static inline void lck_rw_lock_shared(lck_rw_t *lck)
{
	int err;

	xpl_trace_enter("lck=%p", lck);

#if SIMPLE_RWLOCK
	err = pthread_mutex_lock(lck);
#else
	err = pthread_rwlock_rdlock(lck);
#endif
	if (err) {
		xpl_perror(err, "Error while locking lck_rw_t in shared mode");
	}
}
static inline void lck_rw_lock_exclusive(lck_rw_t *lck)
{
	int err;

	xpl_trace_enter("lck=%p", lck);

#if SIMPLE_RWLOCK
	err = pthread_mutex_lock(lck);
#else
	err = pthread_rwlock_wrlock(lck);
#endif
	if (err) {
		xpl_perror(err, "Error while locking lck_rw_t in exclusive "
				"mode");
	}
}
static inline boolean_t lck_rw_try_lock(lck_rw_t *lck, lck_rw_type_t lck_rw_type)
{
	int err;

	xpl_trace_enter("lck=%p", lck);

#if SIMPLE_RWLOCK
	err = pthread_mutex_trylock(lck);
#else
	err = (lck_rw_type == LCK_RW_TYPE_SHARED) ?
		pthread_rwlock_tryrdlock(lck) :
		pthread_rwlock_trywrlock(lck);
#endif
	if (err && err != EBUSY) {
		xpl_perror(err, "Error while trylocking lck_rw_t in %s mode",
				(lck_rw_type == LCK_RW_TYPE_SHARED) ? "shared" :
				"exclusive");
	}

	return err ? FALSE : TRUE;
}

static inline boolean_t lck_rw_lock_shared_to_exclusive(lck_rw_t *lck)
{
	xpl_trace_enter("lck=%p", lck);

#if SIMPLE_RWLOCK
	/* Simple implementation doesn't allow multiple readers. It's always
	 * exclusive. */
	return TRUE;
#else
	/* Using a pthread_rwlock_t doesn't allow us to upgrade a lock, so
	 * unlock the lock, return FALSE and force the caller to re-lock. */
	pthread_rwlock_unlock(lck);
	return FALSE;
#endif
}

static inline void lck_rw_lock_exclusive_to_shared(lck_rw_t *lck)
{
	xpl_trace_enter("lck=%p", lck);

#if SIMPLE_RWLOCK
	/* Simple implementation doesn't allow multiple readers. It's always
	 * exclusive. */
#else
	/* Using a pthread_rwlock_t doesn't allow us to downgrade a lock, so
	 * just leave it locked in exclusive mode. This may hurt performance in
	 * some cases... */
#endif
}

static inline void lck_rw_unlock_shared(lck_rw_t *lck)
{
	int err;

	xpl_trace_enter("lck=%p", lck);

#if SIMPLE_RWLOCK
	err = pthread_mutex_unlock(lck);
#else
	err = pthread_rwlock_unlock(lck);
#endif
	if (err) {
		xpl_perror(err, "Error while unlocking lck_rw_t in shared "
				"mode");
	}
}

static inline void lck_rw_unlock_exclusive(lck_rw_t *lck)
{
	int err;

	xpl_trace_enter("lck=%p", lck);

#if SIMPLE_RWLOCK
	err = pthread_mutex_unlock(lck);
#else
	err = pthread_rwlock_unlock(lck);
#endif
	if (err) {
		xpl_perror(err, "Error while unlocking lck_rw_t in exclusive "
				"mode");
	}
}

#define lck_rw_destroy(lock, group) _lck_rw_destroy(lock)
static inline void _lck_rw_destroy(lck_rw_t *lck)
{
	int err;

	xpl_trace_enter("lck=%p", lck);
#if SIMPLE_RWLOCK
	err = pthread_mutex_destroy(lck);
#else
	err = pthread_rwlock_destroy(lck);
#endif
	if (err) {
		xpl_perror(err, "Error while destroying lck_rw_t");
	}
}


/* We simulate spinlocks with mutexes for now. */
#define lck_spin_t pthread_mutex_t

#define lck_spin_init(lock, group, attr) pthread_mutex_init(lock, attr)
#define lck_spin_lock(lck) pthread_mutex_lock(lck)
#define lck_spin_unlock(lck) pthread_mutex_unlock(lck)
#define lck_spin_destroy(lock, group) pthread_mutex_destroy(lock)

#define panic(fmt, ...) \
	do { \
		fprintf(stderr, fmt "\n", ##__VA_ARGS__); \
		abort(); \
		*((volatile long*) 0) = 1; \
	} while(0)

#define OSBitOrAtomic(mask, address) __sync_fetch_and_or(address, mask)
#define OSBitAndAtomic(mask, address) __sync_fetch_and_and(address, mask)
#define OSIncrementAtomic(address) __sync_fetch_and_add(address, 1)
#define OSDecrementAtomic(address) __sync_fetch_and_sub(address, 1)

/* pseudo-errors returned inside kernel to modify return to process */
//#define    ERESTART    (-1)        /* restart syscall */
#define    EJUSTRETURN    (-2)        /* don't modify regs, just return */

/* Header: kern/kern_types.h */
typedef int wait_result_t;

typedef void (*thread_continue_t)(void *, wait_result_t);
#define    THREAD_CONTINUE_NULL    ((thread_continue_t) 0)

/* <Header: sys/kernel_types.h> */
typedef int64_t daddr64_t;

struct uio;
typedef struct uio * uio_t;

/* </Header: sys/kernel_types.h> */

/* <Header: mach/memory_object_types.h> */
//#define MAX_UPL_TRANSFER_BYTES	(1024 * 1024)
#define MAX_UPL_SIZE_BYTES	(1024 * 1024 * 64)

typedef struct upl_page_info	upl_page_info_t;
typedef upl_page_info_t		*upl_page_info_array_t;
typedef upl_page_info_array_t	upl_page_list_ptr_t;

typedef uint32_t	upl_offset_t;	/* page-aligned byte offset */
typedef uint32_t        upl_size_t;     /* page-aligned byte size */

//#define UPL_FLAGS_NONE        0x00000000ULL
//#define UPL_COPYOUT_FROM    0x00000001ULL
//#define UPL_PRECIOUS        0x00000002ULL
//#define UPL_NO_SYNC        0x00000004ULL
//#define UPL_CLEAN_IN_PLACE    0x00000008ULL
//#define UPL_NOBLOCK        0x00000010ULL
//#define UPL_RET_ONLY_DIRTY    0x00000020ULL
//#define UPL_SET_INTERNAL    0x00000040ULL
//#define UPL_QUERY_OBJECT_TYPE    0x00000080ULL
//#define UPL_RET_ONLY_ABSENT    0x00000100ULL /* used only for COPY_FROM = FALSE */
//#define UPL_FILE_IO             0x00000200ULL
#define UPL_SET_LITE        0x00000400ULL
//#define UPL_SET_INTERRUPTIBLE    0x00000800ULL
//#define UPL_SET_IO_WIRE        0x00001000ULL
//#define UPL_FOR_PAGEOUT        0x00002000ULL
//#define UPL_WILL_BE_DUMPED      0x00004000ULL
//#define UPL_FORCE_DATA_SYNC    0x00008000ULL
/* continued after the ticket bits... */

//#define UPL_PAGE_TICKET_MASK    0x000F0000ULL
//#define UPL_PAGE_TICKET_SHIFT   16

/* ... flags resume here */
//#define UPL_BLOCK_ACCESS    0x00100000ULL
//#define UPL_ENCRYPT        0x00200000ULL
//#define UPL_NOZEROFILL        0x00400000ULL
#define UPL_WILL_MODIFY        0x00800000ULL /* caller will modify the pages */

//#define UPL_NEED_32BIT_ADDR    0x01000000ULL
//#define UPL_UBC_MSYNC        0x02000000ULL
//#define UPL_UBC_PAGEOUT        0x04000000ULL
//#define UPL_UBC_PAGEIN        0x08000000ULL
//#define UPL_REQUEST_SET_DIRTY    0x10000000ULL
//#define UPL_REQUEST_NO_FAULT    0x20000000ULL /* fail if pages not all resident */
//#define UPL_NOZEROFILLIO    0x40000000ULL /* allow non zerofill pages present */
//#define UPL_REQUEST_FORCE_COHERENCY    0x80000000ULL

/* UPL flags known by this kernel */
//#define UPL_VALID_FLAGS        0xFFFFFFFFFFULL

/* upl abort error flags */
//#define UPL_ABORT_RESTART        0x1
//#define UPL_ABORT_UNAVAILABLE    0x2
#define UPL_ABORT_ERROR        0x4
#define UPL_ABORT_FREE_ON_EMPTY    0x8  /* only implemented in wrappers */
#define UPL_ABORT_DUMP_PAGES    0x10
//#define UPL_ABORT_NOTIFY_EMPTY    0x20
/* deprecated: #define UPL_ABORT_ALLOW_ACCESS    0x40 */
//#define UPL_ABORT_REFERENCE    0x80

/* upl commit flags */
#define UPL_COMMIT_FREE_ON_EMPTY    0x1 /* only implemented in wrappers */
#define UPL_COMMIT_CLEAR_DIRTY        0x2
#define UPL_COMMIT_SET_DIRTY        0x4
#define UPL_COMMIT_INACTIVATE        0x8
//#define UPL_COMMIT_NOTIFY_EMPTY        0x10
/* deprecated: #define UPL_COMMIT_ALLOW_ACCESS        0x20 */
//#define UPL_COMMIT_CS_VALIDATED        0x40
//#define UPL_COMMIT_CLEAR_PRECIOUS    0x80
//#define UPL_COMMIT_SPECULATE        0x100
//#define UPL_COMMIT_FREE_ABSENT        0x200
//#define UPL_COMMIT_WRITTEN_BY_KERNEL    0x400

//#define UPL_COMMIT_KERNEL_ONLY_FLAGS    (UPL_COMMIT_CS_VALIDATED | UPL_COMMIT_FREE_ABSENT)

#define UPL_IOSYNC    0x1
#define UPL_NOCOMMIT    0x2
#define UPL_NORDAHEAD    0x4
#define UPL_VNODE_PAGER    0x8
#define UPL_MSYNC        0x10
#define UPL_PAGING_ENCRYPTED    0x20
#define UPL_KEEPCACHED        0x40
#define UPL_NESTED_PAGEOUT    0x80
#define UPL_IOSTREAMING        0x100
#define UPL_IGNORE_VALID_PAGE_CHECK    0x200

//extern boolean_t        upl_page_present(upl_page_info_t *upl, int index);
extern boolean_t        upl_dirty_page(upl_page_info_t *upl, int index);
extern boolean_t        upl_valid_page(upl_page_info_t *upl, int index);
//extern void             upl_deallocate(upl_t upl);
//extern void             upl_mark_decmp(upl_t upl);
//extern void             upl_unmark_decmp(upl_t upl);

/* </Header: mach/memory_object_types.h> */

/* <Header: mach/vm_param.h> */
#define PAGE_MASK_64 ((unsigned long long) (PAGE_SIZE - 1))
/* </Header: mach/vm_param.h> */

/* <Header: sys/_types/_uuid_t.h> */

typedef __darwin_uuid_t    uuid_t;

/* </Header: sys/_types/_uuid_t.h> */

/* <Header: sys/mount.h> */

//#define VFSATTR_INIT(s)			((s)->f_supported = (s)->f_active = 0LL)
#define VFSATTR_SET_SUPPORTED(s, a)	((s)->f_supported |= VFSATTR_ ## a)
#define VFSATTR_IS_SUPPORTED(s, a)	((s)->f_supported & VFSATTR_ ## a)
//#define VFSATTR_CLEAR_ACTIVE(s, a)	((s)->f_active &= ~VFSATTR_ ## a)
#define VFSATTR_IS_ACTIVE(s, a)		((s)->f_active & VFSATTR_ ## a)
//#define VFSATTR_ALL_SUPPORTED(s)	(((s)->f_active & (s)->f_supported) == (s)->f_active)
#define VFSATTR_WANTED(s, a)		((s)->f_active |= VFSATTR_ ## a)
#define VFSATTR_RETURN(s, a, x)		do { (s)-> a = (x); VFSATTR_SET_SUPPORTED(s, a);} while(0)

#define VFSATTR_f_objcount		(1LL<<  0)
#define VFSATTR_f_filecount		(1LL<<  1)
#define VFSATTR_f_dircount		(1LL<<  2)
#define VFSATTR_f_maxobjcount		(1LL<<  3)
#define VFSATTR_f_bsize			(1LL<< 4)
#define VFSATTR_f_iosize		(1LL<<  5)
#define VFSATTR_f_blocks		(1LL<<  6)
#define VFSATTR_f_bfree			(1LL<<  7)
#define VFSATTR_f_bavail		(1LL<<  8)
#define VFSATTR_f_bused			(1LL<<  9)
#define VFSATTR_f_files			(1LL<< 10)
#define VFSATTR_f_ffree			(1LL<< 11)
#define VFSATTR_f_fsid			(1LL<< 12)
#define VFSATTR_f_owner			(1LL<< 13)
#define VFSATTR_f_capabilities		(1LL<< 14)
#define VFSATTR_f_attributes		(1LL<< 15)
#define VFSATTR_f_create_time		(1LL<< 16)
#define VFSATTR_f_modify_time		(1LL<< 17)
#define VFSATTR_f_access_time		(1LL<< 18)
#define VFSATTR_f_backup_time		(1LL<< 19)
#define VFSATTR_f_fssubtype		(1LL<< 20)
#define VFSATTR_f_vol_name		(1LL<< 21)
#define VFSATTR_f_signature		(1LL<< 22)
#define VFSATTR_f_carbon_fsid		(1LL<< 23)
#define VFSATTR_f_uuid			(1LL<< 24)
//#define VFSATTR_f_quota		(1LL<< 25)
//#define VFSATTR_f_reserved		(1LL<< 26)

struct vfs_attr {
	uint64_t	f_supported;
	uint64_t	f_active;

	uint64_t	f_objcount;	/* number of filesystem objects in volume */
	uint64_t	f_filecount;	/* ... files */
	uint64_t	f_dircount;	/* ... directories */
	uint64_t	f_maxobjcount;	/* maximum number of filesystem objects */
	
	uint32_t	f_bsize;	/* block size for the below size values */
	size_t		f_iosize;	/* optimal transfer block size */
	uint64_t	f_blocks;	/* total data blocks in file system */
	uint64_t	f_bfree;	/* free blocks in fs */
	uint64_t	f_bavail;	/* free blocks avail to non-superuser */
	uint64_t	f_bused;	/* blocks in use */
	uint64_t	f_files;	/* total file nodes in file system */
	uint64_t	f_ffree;	/* free file nodes in fs */
	fsid_t		f_fsid;		/* file system id */
	uid_t		f_owner;	/* user that mounted the filesystem */

	vol_capabilities_attr_t f_capabilities;
	vol_attributes_attr_t f_attributes;

	struct timespec	f_create_time;	/* creation time */
	struct timespec	f_modify_time;	/* last modification time */
	struct timespec f_access_time;	/* time of last access */
	struct timespec	f_backup_time;	/* last backup time */

	uint32_t	f_fssubtype;	/* filesystem subtype */

	char		*f_vol_name;	/* volume name */

	uint16_t	f_signature;	/* used for ATTR_VOL_SIGNATURE, Carbon's FSVolumeInfo.signature */
	uint16_t	f_carbon_fsid;	/* same as Carbon's FSVolumeInfo.filesystemID */
	uuid_t		f_uuid;		/* file system UUID (version 3 or 5), available in 10.6 and later */
	//uint64_t	f_quota;	/* total quota data blocks in file system */
	//uint64_t	f_reserved;	/* total reserved data blocks in file system */
};

struct vfsioattr {
	//u_int32_t       io_maxreadcnt;          /* Max. byte count for read */
	//u_int32_t       io_maxwritecnt;         /* Max. byte count for write */
	//u_int32_t       io_segreadcnt;          /* Max. segment count for read */
	//u_int32_t       io_segwritecnt;         /* Max. segment count for write */
	//u_int32_t       io_maxsegreadsize;      /* Max. segment read size  */
	//u_int32_t       io_maxsegwritesize;     /* Max. segment write size */
	u_int32_t       io_devblocksize;        /* the underlying device block size */
	//u_int32_t       io_flags;                       /* flags for underlying device */
	//union {
	//	int64_t io_max_swappin_available;
	//	// On 32 bit architectures, we don't have any spare
	//	void *io_reserved[2];
	//};
};

#define VFS_TBLTHREADSAFE		0x0001	/* Only threadsafe filesystems are supported */
#define VFS_TBLFSNODELOCK		0x0002	/* Only threadsafe filesystems are supported */
#define VFS_TBLNOTYPENUM		0x0008
#define VFS_TBLLOCALVOL			0x0010
#define VFS_TBL64BITREADY		0x0020
#define VFS_TBLNATIVEXATTR		0x0040
//#define VFS_TBLDIRLINKS			0x0080
//#define VFS_TBLUNMOUNT_PREFLIGHT	0x0100	/* does a preflight check before unmounting */
//#define VFS_TBLGENERICMNTARGS		0x0200  /* force generic mount args for local fs */
//#define VFS_TBLREADDIR_EXTENDED		0x0400  /* fs supports VNODE_READDIR_EXTENDED */
//#define	VFS_TBLNOMACLABEL		0x1000
//#define VFS_TBLVNOP_PAGEINV2		0x2000
//#define VFS_TBLVNOP_PAGEOUTV2		0x4000
//#define VFS_TBLVNOP_NOUPDATEID_RENAME	0x8000	/* vfs should not call vnode_update_ident on rename */
//#define	VFS_TBLVNOP_SECLUDE_RENAME 	0x10000
//#define VFS_TBLCANMOUNTROOT		0x20000

struct vfs_fsentry {
	struct vfsops * vfe_vfsops;	/* vfs operations */
	int		vfe_vopcnt;	/* # of vnodeopv_desc being registered (reg, spec, fifo ...) */
	struct vnodeopv_desc ** vfe_opvdescs; /* null terminated;  */
	//int			vfe_fstypenum;	/* historic filesystem type number */
	char		vfe_fsname[MFSNAMELEN];	/* filesystem type name */
	uint32_t	vfe_flags;		/* defines the FS capabilities */
    //void *		vfe_reserv[2];	/* reserved for future use; set this to zero*/
 };

struct vfsops {
	int  (*vfs_mount)(struct mount *mp, vnode_t devvp, user_addr_t data, vfs_context_t context);
#if 0
	int  (*vfs_start)(struct mount *mp, int flags, vfs_context_t context);
#endif
    int  (*vfs_unmount)(struct mount *mp, int mntflags, vfs_context_t context);
	int  (*vfs_root)(struct mount *mp, struct vnode **vpp, vfs_context_t context);
#if 0
	int  (*vfs_quotactl)(struct mount *mp, int cmds, uid_t uid, caddr_t arg, vfs_context_t context);
#endif
	int  (*vfs_getattr)(struct mount *mp, struct vfs_attr *, vfs_context_t context);
	int  (*vfs_sync)(struct mount *mp, int waitfor, vfs_context_t context);
	int  (*vfs_vget)(struct mount *mp, ino64_t ino, struct vnode **vpp, vfs_context_t context);
#if 0
	int  (*vfs_fhtovp)(struct mount *mp, int fhlen, unsigned char *fhp, struct vnode **vpp,
	                   vfs_context_t context);
	int  (*vfs_vptofh)(struct vnode *vp, int *fhlen, unsigned char *fhp, vfs_context_t context);
	int  (*vfs_init)(struct vfsconf *);
	int  (*vfs_sysctl)(int *, u_int, user_addr_t, size_t *, user_addr_t, size_t, vfs_context_t context);
#endif
	int  (*vfs_setattr)(struct mount *mp, struct vfs_attr *, vfs_context_t context);
#if 0
	int  (*vfs_ioctl)(struct mount *mp, u_long command, caddr_t data,
					  int flags, vfs_context_t context);
	int  (*vfs_vget_snapdir)(struct mount *mp, struct vnode **vpp, vfs_context_t context);
	void *vfs_reserved5;
	void *vfs_reserved4;
	void *vfs_reserved3;
	void *vfs_reserved2;
	void *vfs_reserved1;
#endif
};

int vfs_fsadd(struct vfs_fsentry *vfe, vfstable_t *handle);
int vfs_fsremove(vfstable_t handle);
//int     vfs_iterate(int flags, int (*callout)(struct mount *, void *), void *arg);
//int     vfs_init_io_attributes(vnode_t devvp, mount_t mp);
uint64_t vfs_flags(mount_t mp);
void    vfs_setflags(mount_t mp, uint64_t flags);
//void    vfs_clearflags(mount_t mp, uint64_t flags);
//int     vfs_issynchronous(mount_t mp);
int     vfs_iswriteupgrade(mount_t mp);
int     vfs_isupdate(mount_t mp);
int     vfs_isreload(mount_t mp);
//int     vfs_isforce(mount_t mp);
//int     vfs_isunmount(mount_t mp);
int     vfs_isrdonly(mount_t mp);
int     vfs_isrdwr(mount_t mp);
//int     vfs_authopaque(mount_t mp);
//int     vfs_authopaqueaccess(mount_t mp);
//void    vfs_setauthopaque(mount_t mp);
//void    vfs_setauthopaqueaccess(mount_t mp);
//void    vfs_clearauthopaque(mount_t mp);
//void    vfs_clearauthopaqueaccess(mount_t mp);
//void    vfs_setextendedsecurity(mount_t mp);
//void    vfs_clearextendedsecurity(mount_t mp);
//void    vfs_setnoswap(mount_t mp);
//void    vfs_clearnoswap(mount_t mp);
void    vfs_setlocklocal(mount_t mp);
//int     vfs_authcache_ttl(mount_t mp);
//void    vfs_setauthcache_ttl(mount_t mp, int ttl);
//void    vfs_clearauthcache_ttl(mount_t mp);
//#define CACHED_RIGHT_INFINITE_TTL       ~0
//uint32_t vfs_maxsymlen(mount_t mp);
//void    vfs_setmaxsymlen(mount_t mp, uint32_t symlen);
void *  vfs_fsprivate(mount_t mp);
void    vfs_setfsprivate(mount_t mp, void *mntdata);
struct vfsstatfs *      vfs_statfs(mount_t mp);
//#define VFS_USER_EVENT          0
//#define VFS_KERNEL_EVENT        1
//int     vfs_update_vfsstat(mount_t mp, vfs_context_t ctx, int eventtype);
int     vfs_typenum(mount_t mp);
//void    vfs_name(mount_t mp, char *buffer);
int     vfs_devblocksize(mount_t mp);
void    vfs_ioattr(mount_t mp, struct vfsioattr *ioattrp);
void    vfs_setioattr(mount_t mp, struct vfsioattr *ioattrp);
//int     vfs_64bitready(mount_t mp);
//#define LK_NOWAIT 1
//int     vfs_busy(mount_t mp, int flags);
//void    vfs_unbusy(mount_t mp);
//void    vfs_getnewfsid(struct mount *mp);
//mount_t vfs_getvfs(fsid_t *fsid);
//int     vfs_mountedon(struct vnode *vp);
//int     vfs_unmountbyfsid(fsid_t *fsid, int flags, vfs_context_t ctx);
//void    vfs_event_signal(fsid_t *fsid, u_int32_t event, intptr_t data);
//void    vfs_event_init(void); /* XXX We should not export this */
//void vfs_set_root_unmounted_cleanly(void);

/* </Header: sys/mount.h> */

/* <Header: sys/ubc.h> */

#define	UBC_PUSHDIRTY	0x01	/* clean any dirty pages in the specified range to the backing store */
//#define	UBC_PUSHALL	0x02	/* push both dirty and precious pages to the backing store */
#define	UBC_INVALIDATE	0x04	/* invalidate pages in the specified range... may be used with UBC_PUSHDIRTY/ALL */
#define	UBC_SYNC	0x08	/* wait for I/Os generated by UBC_PUSHDIRTY to complete */

//off_t           ubc_blktooff(struct vnode *, daddr64_t);
//daddr64_t       ubc_offtoblk(struct vnode *, off_t);
off_t   ubc_getsize(struct vnode *);
int     ubc_setsize(struct vnode *, off_t);


//kauth_cred_t ubc_getcred(struct vnode *);
//struct thread;
//int     ubc_setthreadcred(struct vnode *, struct proc *, struct thread *);

errno_t ubc_msync(vnode_t, off_t, off_t, off_t *, int);
//int     ubc_pages_resident(vnode_t);
//int     ubc_page_op(vnode_t, off_t, int, ppnum_t *, int *);
//int     ubc_range_op(vnode_t, off_t, off_t, int, int *);


/* cluster IO routines */
//void    cluster_update_state(vnode_t, vm_object_offset_t, vm_object_offset_t, boolean_t);

//int     advisory_read(vnode_t, off_t, off_t, int);
//int     advisory_read_ext(vnode_t, off_t, off_t, int, int (*)(buf_t, void *), void *, int);

int     cluster_read(vnode_t, struct uio *, off_t, int);
int     cluster_read_ext(vnode_t, struct uio *, off_t, int, int (*)(buf_t, void *), void *);

//int     cluster_write(vnode_t, struct uio *, off_t, off_t, off_t, off_t, int);
int     cluster_write_ext(vnode_t, struct uio *, off_t, off_t, off_t, off_t, int, int (*)(buf_t, void *), void *);

//int     cluster_pageout(vnode_t, upl_t, upl_offset_t, off_t, int, off_t, int);
int     cluster_pageout_ext(vnode_t, upl_t, upl_offset_t, off_t, int, off_t, int, int (*)(buf_t, void *), void *);

//int     cluster_pagein(vnode_t, upl_t, upl_offset_t, off_t, int, off_t, int);
int     cluster_pagein_ext(vnode_t, upl_t, upl_offset_t, off_t, int, off_t, int, int (*)(buf_t, void *), void *);

//int     cluster_push(vnode_t, int);
int     cluster_push_ext(vnode_t, int, int (*)(buf_t, void *), void *);
//int     cluster_push_err(vnode_t, int, int (*)(buf_t, void *), void *, int *);

//int     cluster_bp(buf_t);
//int     cluster_bp_ext(buf_t, int (*)(buf_t, void *), void *);

//void    cluster_zero(upl_t, upl_offset_t, int, buf_t);

//int     cluster_copy_upl_data(uio_t, upl_t, int, int *);
int     cluster_copy_ubc_data(vnode_t, uio_t, int *, int);

//typedef struct cl_direct_read_lock cl_direct_read_lock_t;
//cl_direct_read_lock_t *cluster_lock_direct_read(vnode_t vp, lck_rw_type_t exclusive);
//void cluster_unlock_direct_read(cl_direct_read_lock_t *lck);

/* UPL routines */
int     ubc_create_upl(vnode_t, off_t, int, upl_t *, upl_page_info_t **, int);
int     ubc_upl_map(upl_t, vm_offset_t *);
int     ubc_upl_unmap(upl_t);
//int     ubc_upl_commit(upl_t);
int     ubc_upl_commit_range(upl_t, upl_offset_t, upl_size_t, int);
//int     ubc_upl_abort(upl_t, int);
int     ubc_upl_abort_range(upl_t, upl_offset_t, upl_size_t, int);
//void    ubc_upl_range_needed(upl_t, int, int);

//upl_page_info_t *ubc_upl_pageinfo(upl_t);
upl_size_t ubc_upl_maxbufsize(void);

//int     is_file_clean(vnode_t, off_t);

//errno_t mach_to_bsd_errno(kern_return_t mach_err);

/* </Header: sys/ubc.h> */

/* <Header: sys/uio.h> */

enum uio_seg {
    UIO_USERSPACE         = 0,    /* kernel address is virtual,  to/from user virtual */
    UIO_SYSSPACE         = 2,    /* kernel address is virtual,  to/from system virtual */
    UIO_USERSPACE32     = 5,    /* kernel address is virtual,  to/from user 32-bit virtual */
    UIO_USERSPACE64     = 8,    /* kernel address is virtual,  to/from user 64-bit virtual */
    UIO_SYSSPACE32         = 11    /* deprecated */
};

uio_t uio_create( int a_iovcount,               /* max number of iovecs */
    off_t a_offset,                                             /* current offset */
    int a_spacetype,                                            /* type of address space */
    int a_iodirection );                                /* read or write flag */
void uio_reset( uio_t a_uio,
    off_t a_offset,                                             /* current offset */
    int a_spacetype,                                            /* type of address space */
    int a_iodirection );                                /* read or write flag */
//uio_t uio_duplicate( uio_t a_uio );
void uio_free( uio_t a_uio );
int uio_addiov( uio_t a_uio, user_addr_t a_baseaddr, user_size_t a_length );
//int uio_getiov( uio_t a_uio,
//    int a_index,
//    user_addr_t * a_baseaddr_p,
//    user_size_t * a_length_p );
//void uio_update( uio_t a_uio, user_size_t a_count );
user_ssize_t uio_resid( uio_t a_uio );
void uio_setresid( uio_t a_uio, user_ssize_t a_value );
//int uio_iovcnt( uio_t a_uio );
off_t uio_offset( uio_t a_uio );
void uio_setoffset( uio_t a_uio, off_t a_offset );
//int uio_rw( uio_t a_uio );
//void uio_setrw( uio_t a_uio, int a_value );
//int uio_isuserspace( uio_t a_uio );
//user_addr_t uio_curriovbase( uio_t a_uio );
//user_size_t uio_curriovlen( uio_t a_uio );

/*
 * Limits
 */
//#define UIO_MAXIOV      1024            /* max 1K of iov's */
//#define UIO_SMALLIOV    8               /* 8 on stack, else malloc */

extern int uiomove(const char * cp, int n, struct uio *uio);
//extern int uiomove64(const __uint64_t cp, int n, struct uio *uio);

/* </Header: sys/uio.h> */

/* <Header: sys/utfconv.h> */

#define UTF_REVERSE_ENDIAN   0x0001   /* reverse UCS-2 byte order */
//#define UTF_NO_NULL_TERM     0x0002   /* do not add null termination */
#define UTF_DECOMPOSED       0x0004   /* generate fully decomposed UCS-2 */
#define UTF_PRECOMPOSED      0x0008   /* generate precomposed UCS-2 */
//#define UTF_ESCAPE_ILLEGAL   0x0010   /* escape illegal UTF-8 */
#define UTF_SFM_CONVERSIONS  0x0020   /* Use SFM mappings for illegal NTFS chars */

/*
#define UTF_BIG_ENDIAN       \
        ((BYTE_ORDER == BIG_ENDIAN) ? 0 : UTF_REVERSE_ENDIAN)
*/

#define UTF_LITTLE_ENDIAN    \
        ((BYTE_ORDER == LITTLE_ENDIAN) ? 0 : UTF_REVERSE_ENDIAN)

size_t
utf8_encodelen(const u_int16_t * ucsp, size_t ucslen, u_int16_t altslash,
               int flags);

int
utf8_encodestr(const u_int16_t * ucsp, size_t ucslen, u_int8_t * utf8p,
               size_t * utf8len, size_t buflen, u_int16_t altslash, int flags);

int
utf8_decodestr(const u_int8_t* utf8p, size_t utf8len, u_int16_t* ucsp,
               size_t *ucslen, size_t buflen, u_int16_t altslash, int flags);

/* </Header: sys/utfconv.h> */

/* <Header: sys/vnode_if.h> */

extern struct vnodeop_desc vnop_default_desc;
extern struct vnodeop_desc vnop_lookup_desc;
extern struct vnodeop_desc vnop_create_desc;
//extern struct vnodeop_desc vnop_whiteout_desc; // obsolete
extern struct vnodeop_desc vnop_mknod_desc;
extern struct vnodeop_desc vnop_open_desc;
extern struct vnodeop_desc vnop_close_desc;
extern struct vnodeop_desc vnop_access_desc;
extern struct vnodeop_desc vnop_getattr_desc;
extern struct vnodeop_desc vnop_setattr_desc;
extern struct vnodeop_desc vnop_read_desc;
extern struct vnodeop_desc vnop_write_desc;
extern struct vnodeop_desc vnop_ioctl_desc;
extern struct vnodeop_desc vnop_select_desc;
extern struct vnodeop_desc vnop_exchange_desc;
extern struct vnodeop_desc vnop_revoke_desc;
extern struct vnodeop_desc vnop_mmap_desc;
extern struct vnodeop_desc vnop_mnomap_desc;
extern struct vnodeop_desc vnop_fsync_desc;
extern struct vnodeop_desc vnop_remove_desc;
extern struct vnodeop_desc vnop_link_desc;
extern struct vnodeop_desc vnop_rename_desc;
//extern struct vnodeop_desc vnop_renamex_desc;
extern struct vnodeop_desc vnop_mkdir_desc;
extern struct vnodeop_desc vnop_rmdir_desc;
extern struct vnodeop_desc vnop_symlink_desc;
extern struct vnodeop_desc vnop_readdir_desc;
extern struct vnodeop_desc vnop_readdirattr_desc;
//extern struct vnodeop_desc vnop_getattrlistbulk_desc;
extern struct vnodeop_desc vnop_readlink_desc;
extern struct vnodeop_desc vnop_inactive_desc;
extern struct vnodeop_desc vnop_reclaim_desc;
//extern struct vnodeop_desc vnop_print_desc;
extern struct vnodeop_desc vnop_pathconf_desc;
extern struct vnodeop_desc vnop_advlock_desc;
//extern struct vnodeop_desc vnop_truncate_desc;
extern struct vnodeop_desc vnop_allocate_desc;
extern struct vnodeop_desc vnop_pagein_desc;
extern struct vnodeop_desc vnop_pageout_desc;
extern struct vnodeop_desc vnop_searchfs_desc;
extern struct vnodeop_desc vnop_copyfile_desc;
//extern struct vnodeop_desc vnop_clonefile_desc;
extern struct vnodeop_desc vnop_blktooff_desc;
extern struct vnodeop_desc vnop_offtoblk_desc;
extern struct vnodeop_desc vnop_blockmap_desc;
extern struct vnodeop_desc vnop_strategy_desc;
extern struct vnodeop_desc vnop_bwrite_desc;

extern struct vnodeop_desc vnop_getnamedstream_desc;
extern struct vnodeop_desc vnop_makenamedstream_desc;
extern struct vnodeop_desc vnop_removenamedstream_desc;

extern struct vnodeop_desc vnop_getxattr_desc;
extern struct vnodeop_desc vnop_setxattr_desc;
extern struct vnodeop_desc vnop_removexattr_desc;
extern struct vnodeop_desc vnop_listxattr_desc;


struct vnop_lookup_args {
    struct vnodeop_desc *a_desc;
    vnode_t a_dvp;
    vnode_t *a_vpp;
    struct componentname *a_cnp;
    //vfs_context_t a_context;
};

struct vnop_create_args {
    //struct vnodeop_desc *a_desc;
    vnode_t a_dvp;
    vnode_t *a_vpp;
    struct componentname *a_cnp;
    struct vnode_attr *a_vap;
    //vfs_context_t a_context;
};

#if 0
struct vnop_whiteout_args {
    struct vnodeop_desc *a_desc;
    vnode_t a_dvp;
    struct componentname *a_cnp;
    int a_flags;
    vfs_context_t a_context;
};
#endif

struct vnop_mknod_args {
    //struct vnodeop_desc *a_desc;
    vnode_t a_dvp;
    vnode_t *a_vpp;
    struct componentname *a_cnp;
    struct vnode_attr *a_vap;
    //vfs_context_t a_context;
};

struct vnop_open_args {
    //struct vnodeop_desc *a_desc;
    vnode_t a_vp;
    int a_mode;
    //vfs_context_t a_context;
};

struct vnop_close_args {
    //struct vnodeop_desc *a_desc;
    vnode_t a_vp;
    int a_fflag;
    //vfs_context_t a_context;
};

struct vnop_access_args {
    //struct vnodeop_desc *a_desc;
    //vnode_t a_vp;
    //int a_action;
    //vfs_context_t a_context;
};

struct vnop_getattr_args {
    //struct vnodeop_desc *a_desc;
    vnode_t a_vp;
    struct vnode_attr *a_vap;
    //vfs_context_t a_context;
};

struct vnop_setattr_args {
    //struct vnodeop_desc *a_desc;
    vnode_t a_vp;
    struct vnode_attr *a_vap;
    //vfs_context_t a_context;
};

struct vnop_read_args {
    //struct vnodeop_desc *a_desc;
    vnode_t a_vp;
    struct uio *a_uio;
    int a_ioflag;
    //vfs_context_t a_context;
};

struct vnop_write_args {
    //struct vnodeop_desc *a_desc;
    vnode_t a_vp;
    struct uio *a_uio;
    int a_ioflag;
    //vfs_context_t a_context;
};

struct vnop_ioctl_args {
    //struct vnodeop_desc *a_desc;
    //vnode_t a_vp;
    //u_long a_command;
    //caddr_t a_data;
    //int a_fflag;
    //vfs_context_t a_context;
};

extern errno_t VNOP_IOCTL(vnode_t vp, u_long command, caddr_t data, int fflag, vfs_context_t ctx);

struct vnop_select_args {
    //struct vnodeop_desc *a_desc;
    //vnode_t a_vp;
    //int a_which;
    //int a_fflags;
    //void *a_wql;
    //vfs_context_t a_context;
};

struct vnop_exchange_args {
    //struct vnodeop_desc *a_desc;
    //vnode_t a_fvp;
    //vnode_t a_tvp;
    //int a_options;
    //vfs_context_t a_context;
};

struct vnop_revoke_args {
    //struct vnodeop_desc *a_desc;
    //vnode_t a_vp;
    //int a_flags;
    //vfs_context_t a_context;
};

struct vnop_mmap_args {
    //struct vnodeop_desc *a_desc;
    vnode_t a_vp;
    int a_fflags;
    //vfs_context_t a_context;
};

struct vnop_mnomap_args {
    //struct vnodeop_desc *a_desc;
    vnode_t a_vp;
    //vfs_context_t a_context;
};

struct vnop_fsync_args {
    //struct vnodeop_desc *a_desc;
    vnode_t a_vp;
    int a_waitfor;
    //vfs_context_t a_context;
};

struct vnop_remove_args {
    //struct vnodeop_desc *a_desc;
    vnode_t a_dvp;
    vnode_t a_vp;
    struct componentname *a_cnp;
    int a_flags;
    //vfs_context_t a_context;
};

struct vnop_link_args {
    //struct vnodeop_desc *a_desc;
    vnode_t a_vp;
    vnode_t a_tdvp;
    struct componentname *a_cnp;
    //vfs_context_t a_context;
};

struct vnop_rename_args {
    //struct vnodeop_desc *a_desc;
    vnode_t a_fdvp;
    vnode_t a_fvp;
    struct componentname *a_fcnp;
    vnode_t a_tdvp;
    vnode_t a_tvp;
    struct componentname *a_tcnp;
    //vfs_context_t a_context;
};

#if 0
typedef unsigned int vfs_rename_flags_t;

// Must match sys/stdio.h
enum {
    VFS_RENAME_SECLUDE         = 0x00000001,
    VFS_RENAME_SWAP            = 0x00000002,
    VFS_RENAME_EXCL            = 0x00000004,
    
    VFS_RENAME_FLAGS_MASK    = (VFS_RENAME_SECLUDE | VFS_RENAME_SWAP
                                | VFS_RENAME_EXCL),
};

struct vnop_renamex_args {
    struct vnodeop_desc *a_desc;
    vnode_t a_fdvp;
    vnode_t a_fvp;
    struct componentname *a_fcnp;
    vnode_t a_tdvp;
    vnode_t a_tvp;
    struct componentname *a_tcnp;
    struct vnode_attr *a_vap;        // Reserved for future use
    vfs_rename_flags_t a_flags;
    vfs_context_t a_context;
};
#endif

struct vnop_mkdir_args {
    //struct vnodeop_desc *a_desc;
    vnode_t a_dvp;
    vnode_t *a_vpp;
    struct componentname *a_cnp;
    struct vnode_attr *a_vap;
    //vfs_context_t a_context;
};

struct vnop_rmdir_args {
    //struct vnodeop_desc *a_desc;
    vnode_t a_dvp;
    vnode_t a_vp;
    struct componentname *a_cnp;
    //vfs_context_t a_context;
};

struct vnop_symlink_args {
    //struct vnodeop_desc *a_desc;
    vnode_t a_dvp;
    vnode_t *a_vpp;
    struct componentname *a_cnp;
    struct vnode_attr *a_vap;
    char *a_target;
    //vfs_context_t a_context;
};

struct vnop_readdir_args {
    //struct vnodeop_desc *a_desc;
    vnode_t a_vp;
    struct uio *a_uio;
    int a_flags;
    int *a_eofflag;
    int *a_numdirent;
    //vfs_context_t a_context;
};

struct vnop_readdirattr_args {
    //struct vnodeop_desc *a_desc;
    //vnode_t a_vp;
    //struct attrlist *a_alist;
    //struct uio *a_uio;
    //uint32_t a_maxcount;
    //uint32_t a_options;
    //uint32_t *a_newstate;
    //int *a_eofflag;
    //uint32_t *a_actualcount;
    //vfs_context_t a_context;
};

#if 0
struct vnop_getattrlistbulk_args {
    struct vnodeop_desc *a_desc;
    vnode_t a_vp;
    struct attrlist *a_alist;
    struct vnode_attr *a_vap;
    struct uio *a_uio;
    void *a_private;
    uint64_t a_options;
    int32_t *a_eofflag;
    int32_t *a_actualcount;
    vfs_context_t a_context;
};
#endif

struct vnop_readlink_args {
    //struct vnodeop_desc *a_desc;
    vnode_t a_vp;
    struct uio *a_uio;
    //vfs_context_t a_context;
};

struct vnop_inactive_args {
    //struct vnodeop_desc *a_desc;
    vnode_t a_vp;
    //vfs_context_t a_context;
};

struct vnop_reclaim_args {
    //struct vnodeop_desc *a_desc;
    vnode_t a_vp;
    //vfs_context_t a_context;
};

struct vnop_pathconf_args {
    //struct vnodeop_desc *a_desc;
    vnode_t a_vp;
    int a_name;
    int32_t *a_retval;
    //vfs_context_t a_context;
};

struct vnop_advlock_args {
    //struct vnodeop_desc *a_desc;
    //vnode_t a_vp;
    //caddr_t a_id;
    //int a_op;
    //struct flock *a_fl;
    //int a_flags;
    //vfs_context_t a_context;
    //struct timespec *a_timeout;
};

struct vnop_allocate_args {
    //struct vnodeop_desc *a_desc;
    //vnode_t a_vp;
    //off_t a_length;
    //u_int32_t a_flags;
    //off_t *a_bytesallocated;
    //off_t a_offset;
    //vfs_context_t a_context;
};

struct vnop_pagein_args {
    //struct vnodeop_desc *a_desc;
    vnode_t a_vp;
    upl_t a_pl;
    upl_offset_t a_pl_offset;
    off_t a_f_offset;
    size_t a_size;
    int a_flags;
    //vfs_context_t a_context;
};

struct vnop_pageout_args {
    //struct vnodeop_desc *a_desc;
    vnode_t a_vp;
    upl_t a_pl;
    upl_offset_t a_pl_offset;
    off_t a_f_offset;
    size_t a_size;
    int a_flags;
    //vfs_context_t a_context;
};

struct vnop_searchfs_args {
    //struct vnodeop_desc *a_desc;
    //vnode_t a_vp;
    //void *a_searchparams1;
    //void *a_searchparams2;
    //struct attrlist *a_searchattrs;
    //uint32_t a_maxmatches;
    //struct timeval *a_timelimit;
    //struct attrlist *a_returnattrs;
    //uint32_t *a_nummatches;
    //uint32_t a_scriptcode;
    //uint32_t a_options;
    //struct uio *a_uio;
    //struct searchstate *a_searchstate;
    //vfs_context_t a_context;
};

struct vnop_copyfile_args {
    //struct vnodeop_desc *a_desc;
    //vnode_t a_fvp;
    //vnode_t a_tdvp;
    //vnode_t a_tvp;
    //struct componentname *a_tcnp;
    //int a_mode;
    //int a_flags;
    //vfs_context_t a_context;
};


#if 0
typedef enum dir_clone_authorizer_op {
    OP_AUTHORIZE = 0,           /* request authorization of action */
    OP_VATTR_SETUP = 1,         /* query for attributes that are required for OP_AUTHORIZE */
    OP_VATTR_CLEANUP = 2        /* request to cleanup any state or free any memory allocated in OP_AUTHORIZE */
} dir_clone_authorizer_op_t;

struct vnop_clonefile_args {
    struct vnodeop_desc *a_desc;
    vnode_t a_fvp;
    vnode_t a_dvp;
    vnode_t *a_vpp;
    struct componentname *a_cnp;
    struct vnode_attr *a_vap;
    uint32_t a_flags;
    vfs_context_t a_context;
    int (*a_dir_clone_authorizer)(    /* Authorization callback */
                                  struct vnode_attr *vap, /* attribute to be authorized */
                                  kauth_action_t action, /* action for which attribute is to be authorized */
                                  struct vnode_attr *dvap, /* target directory attributes */
                                  vnode_t sdvp, /* source directory vnode pointer (optional) */
                                  mount_t mp, /* mount point of filesystem */
                                  dir_clone_authorizer_op_t vattr_op, /* specific operation requested : setup, authorization or cleanup  */
                                  uint32_t flags, /* needs to have the value passed to a_flags */
                                  vfs_context_t ctx,         /* As passed to VNOP */
                                  void *reserved);        /* Always NULL */
    void *a_reserved;        /* Currently unused */
};
#endif

struct vnop_getxattr_args {
    //struct vnodeop_desc *a_desc;
    vnode_t a_vp;
    const char * a_name;
    uio_t a_uio;
    size_t *a_size;
    int a_options;
    //vfs_context_t a_context;
};

#if 0
extern struct vnodeop_desc vnop_getxattr_desc;

extern errno_t VNOP_GETXATTR(vnode_t vp, const char *name, uio_t uio, size_t *size, int options, vfs_context_t ctx);
#endif

struct vnop_setxattr_args {
    //struct vnodeop_desc *a_desc;
    vnode_t a_vp;
    const char * a_name;
    uio_t a_uio;
    int a_options;
    //vfs_context_t a_context;
};
#if 0
extern struct vnodeop_desc vnop_setxattr_desc;

extern errno_t VNOP_SETXATTR(vnode_t vp, const char *name, uio_t uio, int options, vfs_context_t ctx);
#endif

struct vnop_removexattr_args {
    //struct vnodeop_desc *a_desc;
    vnode_t a_vp;
    const char * a_name;
    int a_options;
    //vfs_context_t a_context;
};
#if 0
extern struct vnodeop_desc vnop_removexattr_desc;
#endif

struct vnop_listxattr_args {
    //struct vnodeop_desc *a_desc;
    vnode_t a_vp;
    uio_t a_uio;
    size_t *a_size;
    //int a_options;
    //vfs_context_t a_context;
};
#if 0
extern struct vnodeop_desc vnop_listxattr_desc;
#endif

struct vnop_blktooff_args {
    //struct vnodeop_desc *a_desc;
    vnode_t a_vp;
    daddr64_t a_lblkno;
    off_t *a_offset;
};

struct vnop_offtoblk_args {
    //struct vnodeop_desc *a_desc;
    vnode_t a_vp;
    off_t a_offset;
    daddr64_t *a_lblkno;
};

struct vnop_blockmap_args {
    //struct vnodeop_desc *a_desc;
    vnode_t a_vp;
    off_t a_foffset;
    size_t a_size;
    daddr64_t *a_bpn;
    size_t *a_run;
    void *a_poff;
    int a_flags;
    //vfs_context_t a_context;
};

struct vnop_strategy_args {
    //struct vnodeop_desc *a_desc;
    struct buf *a_bp;
};

#if 0
extern errno_t VNOP_STRATEGY(struct buf *bp);
#endif

struct vnop_bwrite_args {
    //struct vnodeop_desc *a_desc;
    //buf_t a_bp;
};

#if 0
extern errno_t VNOP_BWRITE(buf_t bp);

struct vnop_kqfilt_add_args {
    struct vnodeop_desc *a_desc;
    struct vnode *a_vp;
    struct knote *a_kn;
    vfs_context_t a_context;
};
extern struct vnodeop_desc vnop_kqfilt_add_desc;


struct vnop_kqfilt_remove_args {
    struct vnodeop_desc *a_desc;
    struct vnode *a_vp;
    uintptr_t a_ident;
    vfs_context_t a_context;
};
extern struct vnodeop_desc vnop_kqfilt_remove_desc;





struct label;
struct vnop_setlabel_args {
    struct vnodeop_desc *a_desc;
    struct vnode *a_vp;
    struct label *a_vl;
    vfs_context_t a_context;
};
extern struct vnodeop_desc vnop_setlabel_desc;
#endif

enum nsoperation	{ NS_OPEN, NS_CREATE, NS_DELETE };

struct vnop_getnamedstream_args {
	//struct vnodeop_desc *a_desc;
	vnode_t a_vp;
	vnode_t *a_svpp;
	const char *a_name;
	enum nsoperation a_operation;
	int a_flags;
	//vfs_context_t a_context;
};

struct vnop_makenamedstream_args {
	//struct vnodeop_desc *a_desc;
	vnode_t *a_svpp;
	vnode_t a_vp;
	const char *a_name;
	int a_flags;
	//vfs_context_t a_context;
};

struct vnop_removenamedstream_args {
	//struct vnodeop_desc *a_desc;
	vnode_t a_vp;
	vnode_t a_svp;
	const char *a_name;
	int a_flags;
	//vfs_context_t a_context;
};

/* </Header: sys/vnode_if.h> */

/* <Header: sys/vnode.h> */

/*
 * Flags for ioflag.
 */
#define IO_UNIT         0x0001          /* do I/O as atomic unit */
#define IO_APPEND       0x0002          /* append write to end */
#define IO_SYNC         0x0004          /* do I/O synchronously */
//#define IO_NODELOCKED   0x0008          /* underlying node already locked */
//#define IO_NDELAY       0x0010          /* FNDELAY flag set in file table */
#define IO_NOZEROFILL   0x0020          /* F_SETSIZE fcntl uses to prevent zero filling */
#define IO_TAILZEROFILL 0x0040          /* zero fills at the tail of write */
#define IO_HEADZEROFILL 0x0080          /* zero fills at the head of write */
#define IO_NOZEROVALID  0x0100          /* do not zero fill if valid page */
#define IO_NOZERODIRTY  0x0200          /* do not zero fill if page is dirty */
#define IO_CLOSE        0x0400          /* I/O issued from close path */
#define IO_NOCACHE      0x0800          /* same effect as VNOCACHE_DATA, but only for this 1 I/O */
#define IO_RAOFF        0x1000          /* same effect as VRAOFF, but only for this 1 I/O */
#define IO_DEFWRITE     0x2000          /* defer write if vfs.defwrite is set */
//#define IO_PASSIVE      0x4000          /* this I/O is marked as background I/O so it won't throttle Throttleable I/O */
//#define IO_BACKGROUND IO_PASSIVE /* used for backward compatibility.  to be removed after IO_BACKGROUND is no longer
//	                          * used by DiskImages in-kernel mode */
//#define IO_NOAUTH       0x8000          /* No authorization checks. */
//#define IO_NODIRECT     0x10000         /* don't use direct synchronous writes if IO_NOCACHE is specified */
//#define IO_ENCRYPTED    0x20000         /* Retrieve encrypted blocks from the filesystem */
//#define IO_RETURN_ON_THROTTLE   0x40000
//#define IO_SINGLE_WRITER        0x80000
//#define IO_SYSCALL_DISPATCH             0x100000        /* I/O was originated from a file table syscall */
//#define IO_SWAP_DISPATCH                0x200000        /* I/O was originated from the swap layer */
//#define IO_SKIP_ENCRYPTION              0x400000        /* Skips en(de)cryption on the IO. Must be initiated from kernel */
//#define IO_EVTONLY                      0x800000        /* the i/o is being done on an fd that's marked O_EVTONLY */

/*
 * Component Name: this structure describes the pathname
 * information that is passed through the VNOP interface.
 */
struct componentname {
	/*
	 * Arguments to lookup.
	 */
	uint32_t        cn_nameiop;     /* lookup operation */
	uint32_t        cn_flags;       /* flags (see below) */
	//void * cn_reserved1;    /* use vfs_context_t */
	//void * cn_reserved2;    /* use vfs_context_t */
	/*
	 * Shared between lookup and commit routines.
	 */
	//char    *cn_pnbuf;      /* pathname buffer */
	//int     cn_pnlen;       /* length of allocated buffer */
	char    *cn_nameptr;    /* pointer to looked up name */
	int     cn_namelen;     /* length of looked up component */
	uint32_t        cn_hash;        /* hash value of looked up name */
	//uint32_t        cn_consume;     /* chars to consume in lookup() */
};

/*
 * component name operations (for VNOP_LOOKUP)
 */
#define LOOKUP          0       /* perform name lookup only */
#define CREATE          1       /* setup for file creation */
#define DELETE          2       /* setup for file deletion */
#define RENAME          3       /* setup for file renaming */
//#define OPMASK          3       /* mask for operation */

/*
 * component name operational modifier flags
 */
//#define FOLLOW          0x00000040 /* follow symbolic links */

/*
 * component name parameter descriptors.
 */
#define ISDOTDOT        0x00002000 /* current component name is .. */
#define MAKEENTRY       0x00004000 /* entry is to be added to name cache */
#define ISLASTCN        0x00008000 /* this is last component of pathname */

struct vnode_fsparam {
	struct mount * vnfs_mp;         /* mount point to which this vnode_t is part of */
	enum vtype      vnfs_vtype;             /* vnode type */
	const char * vnfs_str;          /* File system Debug aid */
	struct vnode * vnfs_dvp;                        /* The parent vnode */
	void * vnfs_fsnode;                     /* inode */
	int(**vnfs_vops)(void *);               /* vnode dispatch table */
	int vnfs_markroot;                      /* is this a root vnode in FS (not a system wide one) */
	int vnfs_marksystem;            /* is  a system vnode */
	dev_t vnfs_rdev;                        /* dev_t  for block or char vnodes */
	off_t vnfs_filesize;            /* that way no need for getattr in UBC */
	struct componentname * vnfs_cnp; /* component name to add to namecache */
	uint32_t vnfs_flags;            /* flags */
};

#define VNFS_NOCACHE    0x01    /* do not add to name cache at this time */
//#define VNFS_CANTCACHE  0x02    /* never add this instance to the name cache */
#define VNFS_ADDFSREF   0x04    /* take fs (named) reference */

#define VNCREATE_FLAVOR 0
#define VCREATESIZE sizeof(struct vnode_fsparam)

//#define VATTR_INIT(v)                   do {(v)->va_supported = (v)->va_active = 0ll; (v)->va_vaflags = 0; } while(0)
#define VATTR_SET_ACTIVE(v, a)          ((v)->va_active |= VNODE_ATTR_ ## a)
#define VATTR_SET_SUPPORTED(v, a)       ((v)->va_supported |= VNODE_ATTR_ ## a)
#define VATTR_IS_SUPPORTED(v, a)        ((v)->va_supported & VNODE_ATTR_ ## a)
//#define VATTR_CLEAR_ACTIVE(v, a)        ((v)->va_active &= ~VNODE_ATTR_ ## a)
//#define VATTR_CLEAR_SUPPORTED(v, a)     ((v)->va_supported &= ~VNODE_ATTR_ ## a)
//#define VATTR_CLEAR_SUPPORTED_ALL(v)    ((v)->va_supported = 0)
#define VATTR_IS_ACTIVE(v, a)           ((v)->va_active & VNODE_ATTR_ ## a)
//#define VATTR_ALL_SUPPORTED(v)          (((v)->va_active & (v)->va_supported) == (v)->va_active)
//#define VATTR_INACTIVE_SUPPORTED(v)     do {(v)->va_active &= ~(v)->va_supported; (v)->va_supported = 0;} while(0)
//#define VATTR_SET(v, a, x)              do { (v)-> a = (x); VATTR_SET_ACTIVE(v, a);} while(0)
//#define VATTR_WANTED(v, a)              VATTR_SET_ACTIVE(v, a)
#define VATTR_RETURN(v, a, x)           do { (v)-> a = (x); VATTR_SET_SUPPORTED(v, a);} while(0)
//#define VATTR_NOT_RETURNED(v, a)        (VATTR_IS_ACTIVE(v, a) && !VATTR_IS_SUPPORTED(v, a))

/*
 * Two macros to simplify conditional checking in kernel code.
 */
//#define VATTR_IS(v, a, x)               (VATTR_IS_SUPPORTED(v, a) && (v)-> a == (x))
//#define VATTR_IS_NOT(v, a, x)           (VATTR_IS_SUPPORTED(v, a) && (v)-> a != (x))

#define VNODE_ATTR_va_rdev		(1LL<< 0)	/* 00000001 */
#define VNODE_ATTR_va_nlink		(1LL<< 1)	/* 00000002 */
#define VNODE_ATTR_va_total_size	(1LL<< 2)	/* 00000004 */
#define VNODE_ATTR_va_total_alloc	(1LL<< 3)	/* 00000008 */
#define VNODE_ATTR_va_data_size		(1LL<< 4)	/* 00000010 */
#define VNODE_ATTR_va_data_alloc	(1LL<< 5)	/* 00000020 */
#define VNODE_ATTR_va_iosize		(1LL<< 6)	/* 00000040 */
#define VNODE_ATTR_va_uid		(1LL<< 7)	/* 00000080 */
#define VNODE_ATTR_va_gid		(1LL<< 8)	/* 00000100 */
#define VNODE_ATTR_va_mode		(1LL<< 9)	/* 00000200 */
#define VNODE_ATTR_va_flags		(1LL<<10)	/* 00000400 */
//#define VNODE_ATTR_va_acl		(1LL<<11)	/* 00000800 */
#define VNODE_ATTR_va_create_time	(1LL<<12)	/* 00001000 */
#define VNODE_ATTR_va_access_time	(1LL<<13)	/* 00002000 */
#define VNODE_ATTR_va_modify_time	(1LL<<14)	/* 00004000 */
#define VNODE_ATTR_va_change_time	(1LL<<15)	/* 00008000 */
#define VNODE_ATTR_va_backup_time	(1LL<<16)	/* 00010000 */
#define VNODE_ATTR_va_fileid		(1LL<<17)	/* 00020000 */
//#define VNODE_ATTR_va_linkid		(1LL<<18)	/* 00040000 */
#define VNODE_ATTR_va_parentid		(1LL<<19)	/* 00080000 */
#define VNODE_ATTR_va_fsid		(1LL<<20)	/* 00100000 */
#define VNODE_ATTR_va_filerev		(1LL<<21)	/* 00200000 */
#define VNODE_ATTR_va_gen		(1LL<<22)	/* 00400000 */
#define VNODE_ATTR_va_encoding		(1LL<<23)	/* 00800000 */
#define VNODE_ATTR_va_type		(1LL<<24)	/* 01000000 */
#define VNODE_ATTR_va_name		(1LL<<25)       /* 02000000 */
//#define VNODE_ATTR_va_uuuid		(1LL<<26)	/* 04000000 */
//#define VNODE_ATTR_va_guuid		(1LL<<27)	/* 08000000 */
//#define VNODE_ATTR_va_nchildren		(1LL<<28)       /* 10000000 */
//#define VNODE_ATTR_va_dirlinkcount	(1LL<<29)       /* 20000000 */
//#define VNODE_ATTR_va_addedtime		(1LL<<30)	/* 40000000 */
//#define VNODE_ATTR_va_dataprotect_class	(1LL<<31)	/* 80000000 */
//#define VNODE_ATTR_va_dataprotect_flags	(1LL<<32)	/* 100000000 */
//#define VNODE_ATTR_va_document_id	(1LL<<33)	/* 200000000 */
//#define VNODE_ATTR_va_devid		(1LL<<34)	/* 400000000 */
//#define VNODE_ATTR_va_objtype		(1LL<<35)	/* 800000000 */
//#define VNODE_ATTR_va_objtag		(1LL<<36)	/* 1000000000 */
//#define VNODE_ATTR_va_user_access	(1LL<<37)	/* 2000000000 */
//#define VNODE_ATTR_va_finderinfo	(1LL<<38)	/* 4000000000 */
//#define VNODE_ATTR_va_rsrc_length	(1LL<<39)	/* 8000000000 */
//#define VNODE_ATTR_va_rsrc_alloc	(1LL<<40)	/* 10000000000 */
//#define VNODE_ATTR_va_fsid64		(1LL<<41)	/* 20000000000 */
//#define VNODE_ATTR_va_write_gencount    (1LL<<42)	/* 40000000000 */
//#define VNODE_ATTR_va_private_size	(1LL<<43)	/* 80000000000 */

#define VNODE_ATTR_BIT(n)    (VNODE_ATTR_ ## n)

struct vnode_attr {
	/* bitfields */
	uint64_t	va_supported;
	uint64_t	va_active;

	/*
	 * Control flags.  The low 16 bits are reserved for the
	 * ioflags being passed for truncation operations.
	 */
	int		va_vaflags;
	
	/* traditional stat(2) parameter fields */
	dev_t		va_rdev;	/* device id (device nodes only) */
	uint64_t	va_nlink;	/* number of references to this file */
	uint64_t	va_total_size;	/* size in bytes of all forks */
	uint64_t	va_total_alloc;	/* disk space used by all forks */
	uint64_t	va_data_size;	/* size in bytes of the fork managed by current vnode */
	uint64_t	va_data_alloc;	/* disk space used by the fork managed by current vnode */
	uint32_t	va_iosize;	/* optimal I/O blocksize */

	/* file security information */
	uid_t		va_uid;		/* owner UID */
	gid_t		va_gid;		/* owner GID */
	mode_t		va_mode;	/* posix permissions */
	uint32_t	va_flags;	/* file flags */
	//struct kauth_acl *va_acl;	/* access control list */

	/* timestamps */
	struct timespec	va_create_time;	/* time of creation */
	struct timespec	va_access_time;	/* time of last access */
	struct timespec	va_modify_time;	/* time of last data modification */
	struct timespec	va_change_time;	/* time of last metadata change */
	struct timespec	va_backup_time;	/* time of last backup */
	
	/* file parameters */
	uint64_t	va_fileid;	/* file unique ID in filesystem */
	//uint64_t	va_linkid;	/* file link unique ID */
	uint64_t	va_parentid;	/* parent ID */
	uint32_t	va_fsid;	/* filesystem ID */
	uint64_t	va_filerev;	/* file revision counter */	/* XXX */
	uint32_t	va_gen;		/* file generation count */	/* XXX - relationship of
									* these two? */
	/* misc parameters */
	uint32_t	va_encoding;	/* filename encoding script */

	enum vtype	va_type;	/* file type */
	char *		va_name;	/* Name for ATTR_CMN_NAME; MAXPATHLEN bytes */
	//guid_t		va_uuuid;	/* file owner UUID */
	//guid_t		va_guuid;	/* file group UUID */
	
	/* Meaningful for directories only */
	//uint64_t	va_nchildren;     /* Number of items in a directory */
	//uint64_t	va_dirlinkcount;  /* Real references to dir (i.e. excluding "." and ".." refs) */

	//void * 		va_reserved1;
	//struct timespec va_addedtime;	/* timestamp when item was added to parent directory */
		
	/* Data Protection fields */
	//uint32_t va_dataprotect_class;	/* class specified for this file if it didn't exist */
	//uint32_t va_dataprotect_flags;	/* flags from NP open(2) to the filesystem */

	/* Document revision tracking */
	//uint32_t va_document_id;

	/* Fields for Bulk args */
	//uint32_t 	va_devid;	/* devid of filesystem */
	//uint32_t 	va_objtype;	/* type of object */
	//uint32_t 	va_objtag;	/* vnode tag of filesystem */
	//uint32_t 	va_user_access;	/* access for user */
	//uint8_t  	va_finderinfo[32];	/* Finder Info */
	//uint64_t 	va_rsrc_length;	/* Resource Fork length */
	//uint64_t 	va_rsrc_alloc;	/* Resource Fork allocation size */
	//fsid_t 		va_fsid64;	/* fsid, of the correct type  */

	//uint32_t va_write_gencount;     /* counter that increments each time the file changes */

	//uint64_t va_private_size; /* If the file were deleted, how many bytes would be freed immediately */

	/* add new fields here only */
};

/*
 * Flags for va_vaflags.
 */
//#define VA_UTIMES_NULL          0x010000        /* utimes argument was NULL */
#define VA_EXCLUSIVE            0x020000        /* exclusive create request */
//#define VA_NOINHERIT            0x040000        /* Don't inherit ACLs from parent */
//#define VA_NOAUTH               0x080000
//#define VA_64BITOBJIDS          0x100000        /* fileid/linkid/parentid are 64 bit */
//#define VA_REALFSID             0x200000        /* Return real fsid */
//#define VA_USEFSID              0x400000        /* Use fsid from filesystem  */

/*
 *  Modes.  Some values same as Ixxx entries from inode.h for now.
 */
//#define VSUID   0x800 /*04000*/	/* set user id on execution */
//#define VSGID   0x400 /*02000*/	/* set group id on execution */
//#define VSVTX   0x200 /*01000*/	/* save swapped text even after use */
//#define VREAD   0x100 /*00400*/	/* read, write, execute permissions */
//#define VWRITE  0x080 /*00200*/
//#define VEXEC   0x040 /*00100*/

/*
 * Convert between vnode types and inode formats (since POSIX.1
 * defines mode word of stat structure in terms of inode formats).
 */
//extern enum vtype       iftovt_tab[];
//extern int              vttoif_tab[];
//#define IFTOVT(mode)    (iftovt_tab[((mode) & S_IFMT) >> 12])
//#define VTTOIF(indx)    (vttoif_tab[(int)(indx)])
//#define MAKEIMODE(indx, mode)   (int)(VTTOIF(indx) | (mode))

/*
 * Flags to various vnode functions.
 */
//#define SKIPSYSTEM      0x0001          /* vflush: skip vnodes marked VSYSTEM */
//#define FORCECLOSE      0x0002          /* vflush: force file closeure */
//#define WRITECLOSE      0x0004          /* vflush: only close writeable files */
//#define SKIPSWAP        0x0008          /* vflush: skip vnodes marked VSWAP */
//#define SKIPROOT        0x0010          /* vflush: skip root vnodes marked VROOT */

//#define DOCLOSE         0x0008          /* vclean: close active files */

//#define V_SAVE          0x0001          /* vinvalbuf: sync file first */
//#define V_SAVEMETA      0x0002          /* vinvalbuf: leave indirect blocks */

//#define REVOKEALL       0x0001          /* vnop_revoke: revoke all aliases */

/* VNOP_REMOVE/unlink flags */
#define VNODE_REMOVE_NODELETEBUSY                       0x0001 /* Don't delete busy files (Carbon) */
//#define VNODE_REMOVE_SKIP_NAMESPACE_EVENT       0x0002 /* Do not upcall to userland handlers */
//#define VNODE_REMOVE_NO_AUDIT_PATH              0x0004 /* Do not audit the path */
//#define VNODE_REMOVE_DATALESS_DIR               0x0008 /* Special handling for removing a dataless directory without materialization */

/* VNOP_READDIR flags: */
//#define VNODE_READDIR_EXTENDED    0x0001   /* use extended directory entries */
//#define VNODE_READDIR_REQSEEKOFF  0x0002   /* requires seek offset (cookies) */
//#define VNODE_READDIR_SEEKOFF32   0x0004   /* seek offset values should fit in 32 bits */
//#define VNODE_READDIR_NAMEMAX     0x0008   /* For extended readdir, try to limit names to NAME_MAX bytes */

/* VNOP_CLONEFILE flags: */
//#define VNODE_CLONEFILE_DEFAULT       0x0000
//#define VNODE_CLONEFILE_NOOWNERCOPY   0x0001 /* Don't copy ownership information */


#define	NULLVP	((struct vnode *)NULL)

struct vnodeop_desc;

struct vnodeopv_entry_desc {
	struct vnodeop_desc *opve_op;   /* which operation this is */
	int (*opve_impl)(void *);		/* code implementing this operation */
};
struct vnodeopv_desc {
			/* ptr to the ptr to the vector where op should go */
	int (***opv_desc_vector_p)(void *);
	struct vnodeopv_entry_desc *opv_desc_ops;   /* null terminated list */
};

#define	SKIPSYSTEM	0x0001		/* vflush: skip vnodes marked VSYSTEM */
#define	FORCECLOSE	0x0002		/* vflush: force file closeure */
//#define	WRITECLOSE	0x0004		/* vflush: only close writeable files */
//#define SKIPSWAP	0x0008		/* vflush: skip vnodes marked VSWAP */
#define SKIPROOT	0x0010		/* vflush: skip root vnodes marked VROOT */

//#define	DOCLOSE		0x0008		/* vclean: close active files */

#define VNODE_RETURNED		0	/* done with vnode, reference can be dropped */
//#define VNODE_RETURNED_DONE	1	/* done with vnode, reference can be dropped, terminate iteration */
//#define VNODE_CLAIMED		2	/* don't drop reference */
//#define VNODE_CLAIMED_DONE	3	/* don't drop reference, terminate iteration */

int vn_default_error(void);

errno_t vnode_create(uint32_t flavor, uint32_t size, void  *data, vnode_t *vpp);
//int     vnode_addfsref(vnode_t vp);
int     vnode_removefsref(vnode_t vp);
int     vnode_hasdirtyblks(vnode_t vp);
//int     vnode_hascleanblks(vnode_t vp);

//#define VNODE_ASYNC_THROTTLE    15

//int     vnode_waitforwrites(vnode_t vp, int output_target, int slpflag, int slptimeout, const char *msg);
//void    vnode_startwrite(vnode_t vp);
//void    vnode_writedone(vnode_t vp);
enum vtype      vnode_vtype(vnode_t vp);
uint32_t        vnode_vid(vnode_t vp);
//boolean_t vnode_isonexternalstorage(vnode_t vp);
//mount_t vnode_mountedhere(vnode_t vp);
mount_t vnode_mount(vnode_t vp);
dev_t   vnode_specrdev(vnode_t vp);
void *  vnode_fsnode(vnode_t vp);
void    vnode_clearfsnode(vnode_t vp);
//int     vnode_isvroot(vnode_t vp);
int     vnode_issystem(vnode_t vp);
//int     vnode_ismount(vnode_t vp);
int     vnode_isreg(vnode_t vp);
//int     vnode_isdir(vnode_t vp);
//int     vnode_islnk(vnode_t vp);
//int     vnode_isfifo(vnode_t vp);
int     vnode_isblk(vnode_t vp);
int     vnode_ischr(vnode_t vp);
//int     vnode_isswap(vnode_t vp);
//int     vnode_isnamedstream(vnode_t vp);
//int     vnode_ismountedon(vnode_t vp);
//void    vnode_setmountedon(vnode_t vp);
//void    vnode_clearmountedon(vnode_t vp);
//int     vnode_isrecycled(vnode_t vp);
int     vnode_isnocache(vnode_t vp);
//int     vnode_israge(vnode_t vp);
//int     vnode_needssnapshots(vnode_t vp);
//void    vnode_setnocache(vnode_t vp);
//void    vnode_clearnocache(vnode_t vp);
int     vnode_isnoreadahead(vnode_t vp);
//void    vnode_setnoreadahead(vnode_t vp);
//void    vnode_clearnoreadahead(vnode_t vp);
//int     vnode_isfastdevicecandidate(vnode_t vp);
//void    vnode_setfastdevicecandidate(vnode_t vp);
//void    vnode_clearfastdevicecandidate(vnode_t vp);
//int     vnode_isautocandidate(vnode_t vp);
//void    vnode_setautocandidate(vnode_t vp);
//void    vnode_clearautocandidate(vnode_t vp);
void    vnode_settag(vnode_t vp, int tag);
//int     vnode_tag(vnode_t vp);
//int     vnode_getattr(vnode_t vp, struct vnode_attr *vap, vfs_context_t ctx);
//extern uint64_t vnode_get_va_fsid(struct vnode_attr *vap);
//int     vnode_setattr(vnode_t vp, struct vnode_attr *vap, vfs_context_t ctx);
//vnode_t vfs_rootvnode(void);
//void    vnode_uncache_credentials(vnode_t vp);
//void    vnode_setmultipath(vnode_t vp);
//uint32_t  vnode_vfsmaxsymlen(vnode_t vp);
//int     vnode_vfsisrdonly(vnode_t vp);
//int     vnode_vfstypenum(vnode_t vp);
//void    vnode_vfsname(vnode_t vp, char *buf);
//int     vnode_vfs64bitready(vnode_t vp);
//int     vfs_context_get_special_port(vfs_context_t, int, ipc_port_t *);
//int     vfs_context_set_special_port(vfs_context_t, int, ipc_port_t);
proc_t  vfs_context_proc(vfs_context_t ctx);
kauth_cred_t    vfs_context_ucred(vfs_context_t ctx);
//int     vfs_context_pid(vfs_context_t ctx);
//int     vfs_context_issignal(vfs_context_t ctx, sigset_t mask);
//int     vfs_context_suser(vfs_context_t ctx);
//int     vfs_context_is64bit(vfs_context_t ctx);
//vfs_context_t vfs_context_create(vfs_context_t ctx);
//int vfs_context_rele(vfs_context_t ctx);
//vfs_context_t vfs_context_current(void);
int     vflush(struct mount *mp, struct vnode *skipvp, int flags);
int     vnode_get(vnode_t);
int     vnode_getwithvid(vnode_t, uint32_t);
//int     vnode_getwithref(vnode_t vp);
int     vnode_put(vnode_t vp);
int     vnode_ref(vnode_t vp);
void    vnode_rele(vnode_t vp);
int     vnode_isinuse(vnode_t vp, int refcnt);
int     vnode_recycle(vnode_t vp);

#define VNODE_UPDATE_PARENT     0x01
//#define VNODE_UPDATE_NAMEDSTREAM_PARENT VNODE_UPDATE_PARENT
#define VNODE_UPDATE_NAME       0x02
#define VNODE_UPDATE_CACHE      0x04
//#define VNODE_UPDATE_PURGE      0x08

void    vnode_update_identity(vnode_t vp, vnode_t dvp, const char *name, int name_len, uint32_t name_hashval, int flags);
int     vn_bwrite(struct vnop_bwrite_args *ap);
//int     vnode_authorize(vnode_t vp, vnode_t dvp, kauth_action_t action, vfs_context_t ctx);
//int     vnode_authattr(vnode_t vp, struct vnode_attr *vap, kauth_action_t *actionp, vfs_context_t ctx);
//int     vnode_authattr_new(vnode_t dvp, struct vnode_attr *vap, int noauth, vfs_context_t ctx);
//errno_t vnode_close(vnode_t vp, int flags, vfs_context_t ctx);
//int vn_getpath(struct vnode *vp, char *pathbuf, int *len);
//int     vnode_notify(vnode_t vp, uint32_t events, struct vnode_attr *vap);
//int     vfs_get_notify_attributes(struct vnode_attr *vap);

//#define VNODE_LOOKUP_NOFOLLOW           0x01
//#define VNODE_LOOKUP_NOCROSSMOUNT       0x02
//#define VNODE_LOOKUP_CROSSMOUNTNOWAIT   0x04

//errno_t vnode_lookup(const char *path, int flags, vnode_t *vpp, vfs_context_t ctx);
//errno_t vnode_open(const char *path, int fmode, int cmode, int flags, vnode_t *vpp, vfs_context_t ctx);
int     vnode_iterate(struct mount *mp, int flags, int (*callout)(struct vnode *, void *), void *arg);

//#define VNODE_RELOAD                    0x01
//#define VNODE_WAIT                              0x02
//#define VNODE_WRITEABLE                 0x04
//#define VNODE_WITHID                    0x08
//#define VNODE_NOLOCK_INTERNAL   0x10
//#define VNODE_NODEAD                    0x20
//#define VNODE_NOSUSPEND                 0x40
//#define VNODE_ITERATE_ALL               0x80
//#define VNODE_ITERATE_ACTIVE    0x100
//#define VNODE_ITERATE_INACTIVE  0x200

//#define VNODE_RETURNED          0       /* done with vnode, reference can be dropped */
//#define VNODE_RETURNED_DONE     1       /* done with vnode, reference can be dropped, terminate iteration */
//#define VNODE_CLAIMED           2       /* don't drop reference */
//#define VNODE_CLAIMED_DONE      3       /* don't drop reference, terminate iteration */

//int     vn_revoke(vnode_t vp, int flags, vfs_context_t ctx);
int     cache_lookup(vnode_t dvp, vnode_t *vpp, struct componentname *cnp);
void    cache_enter(vnode_t dvp, vnode_t vp, struct componentname *cnp);
void    cache_purge(vnode_t vp);
void    cache_purge_negatives(vnode_t vp);
//const char *vfs_addname(const char *name, uint32_t len, uint32_t nc_hash, uint32_t flags);
//int   vfs_removename(const char *name);
//int     vcount(vnode_t vp);
//int vn_path_package_check(vnode_t vp, char *path, int pathlen, int *component);
//int     vn_rdwr(enum uio_rw rw, struct vnode *vp, caddr_t base, int len, off_t offset, enum uio_seg segflg, int ioflg, kauth_cred_t cred, int *aresid, proc_t p);
const char      *vnode_getname(vnode_t vp);
void    vnode_putname(const char *name);
vnode_t vnode_getparent(vnode_t vp);
//int     vnode_setdirty(vnode_t vp);
//int     vnode_cleardirty(vnode_t vp);
//int     vnode_isdirty(vnode_t vp);
//int vnode_getbackingvnode(vnode_t in_vp, vnode_t* out_vpp);
//errno_t vfs_setup_vattr_from_attrlist(struct attrlist *alp, struct vnode_attr *vap, enum vtype obj_vtype, ssize_t *attr_fixed_sizep, vfs_context_t ctx);
//errno_t vfs_attr_pack(vnode_t vp, uio_t uio, struct attrlist *alp, uint64_t options, struct vnode_attr *vap, void *fndesc, vfs_context_t ctx);
//errno_t vfs_attr_pack_ext(mount_t mp, vnode_t vp, uio_t uio, struct attrlist *alp, uint64_t options, struct vnode_attr *vap, void *fndesc, vfs_context_t ctx);

/* </Header: sys/vnode.h> */

/* <Header: vfs/vfs_support.h> */

extern int nop_revoke(struct vnop_revoke_args *ap);

extern int nop_readdirattr(struct vnop_readdirattr_args *ap);

extern int nop_allocate(struct vnop_allocate_args *ap);

extern int err_advlock(struct vnop_advlock_args *ap);

extern int err_searchfs(struct vnop_searchfs_args *ap);

extern int err_copyfile(struct vnop_copyfile_args *ap);

/* </Header: vfs/vfs_support.h> */

/* <Header: sys/buf.h> */

//void	buf_markaged(buf_t bp);
//void	buf_markinvalid(buf_t bp);
//void	buf_markdelayed(buf_t bp);
//void	buf_markclean(buf_t);
//void	buf_markeintr(buf_t bp);
//void	buf_markfua(buf_t bp);
//int	buf_fua(buf_t bp);
//int	buf_valid(buf_t bp);
//int	buf_fromcache(buf_t bp);
//void *	buf_upl(buf_t bp);
//uint32_t buf_uploffset(buf_t bp);
//kauth_cred_t buf_rcred(buf_t bp);
//kauth_cred_t buf_wcred(buf_t bp);
//proc_t	buf_proc(buf_t bp);
//uint32_t buf_dirtyoff(buf_t bp);
//uint32_t buf_dirtyend(buf_t bp);
//void	buf_setdirtyoff(buf_t bp, uint32_t);
//void	buf_setdirtyend(buf_t bp, uint32_t);
errno_t	buf_error(buf_t bp);
void	buf_seterror(buf_t bp, errno_t);
void	buf_setflags(buf_t bp, int32_t flags);
//void	buf_clearflags(buf_t bp, int32_t flags);
int32_t	buf_flags(buf_t bp);
//void	buf_reset(buf_t bp, int32_t flags);
errno_t	buf_map(buf_t bp, caddr_t *io_addr);
errno_t	buf_unmap(buf_t bp);
//void 	buf_setdrvdata(buf_t bp, void *drvdata);
//void *	buf_drvdata(buf_t bp);
//void 	buf_setfsprivate(buf_t bp, void *fsprivate);
//void *	buf_fsprivate(buf_t bp);
//daddr64_t buf_blkno(buf_t bp);
daddr64_t buf_lblkno(buf_t bp);
//void	buf_setblkno(buf_t bp, daddr64_t blkno);
//void	buf_setlblkno(buf_t bp, daddr64_t lblkno);
uint32_t buf_count(buf_t bp);
uint32_t buf_size(buf_t bp);
//uint32_t buf_resid(buf_t bp);
//void	buf_setcount(buf_t bp, uint32_t bcount);
//void	buf_setsize(buf_t bp, uint32_t);
//void	buf_setresid(buf_t bp, uint32_t resid);
//void	buf_setdataptr(buf_t bp, uintptr_t data);
//uintptr_t buf_dataptr(buf_t bp);
vnode_t	buf_vnode(buf_t bp);
//void	buf_setvnode(buf_t bp, vnode_t vp);
//dev_t	buf_device(buf_t bp);
//errno_t	buf_setdevice(buf_t bp, vnode_t vp);
errno_t	buf_strategy(vnode_t devvp, void *ap);
//#define	BUF_WAIT	0x01
errno_t	buf_invalblkno(vnode_t vp, daddr64_t lblkno, int flags);
//void * buf_callback(buf_t bp);
//errno_t	buf_setcallback(buf_t bp, void (*callback)(buf_t, void *), void *transaction);
//errno_t	buf_setupl(buf_t bp, upl_t upl, uint32_t offset);
//buf_t	buf_clone(buf_t bp, int io_offset, int io_size, void (*iodone)(buf_t, void *), void *arg);
//buf_t	buf_create_shadow(buf_t bp, boolean_t force_copy, uintptr_t external_storage, void (*iodone)(buf_t, void *), void *arg);
//int	buf_shadow(buf_t bp);
//buf_t 	buf_alloc(vnode_t vp);
//void	buf_free(buf_t bp);
//#define	BUF_WRITE_DATA	0x0001		/* write data blocks first */
//#define	BUF_SKIP_META	0x0002		/* skip over metadata blocks */
//#define BUF_INVALIDATE_LOCKED	0x0004	/* force B_LOCKED blocks to be invalidated */
int	buf_invalidateblks(vnode_t vp, int flags, int slpflag, int slptimeo);
//#define BUF_SKIP_NONLOCKED	0x01
//#define BUF_SKIP_LOCKED		0x02
//#define BUF_SCAN_CLEAN		0x04	/* scan the clean buffers */
//#define BUF_SCAN_DIRTY		0x08	/* scan the dirty buffers */
//#define BUF_NOTIFY_BUSY		0x10	/* notify the caller about the busy pages during the scan */
//#define	BUF_RETURNED		0
//#define BUF_RETURNED_DONE	1
//#define BUF_CLAIMED		2
//#define	BUF_CLAIMED_DONE	3
void	buf_flushdirtyblks(vnode_t vp, int wait, int flags, const char *msg);
//void	buf_iterate(vnode_t vp, int (*callout)(buf_t, void *), int flags, void *arg);
void	buf_clear(buf_t bp);
errno_t	buf_bawrite(buf_t bp);
errno_t	buf_bdwrite(buf_t bp);
errno_t	buf_bwrite(buf_t bp);
void	buf_biodone(buf_t bp);
//errno_t	buf_biowait(buf_t bp);
void	buf_brelse(buf_t bp);
//errno_t	buf_bread(vnode_t vp, daddr64_t blkno, int size, kauth_cred_t cred, buf_t *bpp);
//errno_t	buf_breadn(vnode_t vp, daddr64_t blkno, int size, daddr64_t *rablks, int *rasizes, int nrablks, kauth_cred_t cred, buf_t *bpp);
errno_t	buf_meta_bread(vnode_t vp, daddr64_t blkno, int size, kauth_cred_t cred, buf_t *bpp);
//errno_t	buf_meta_breadn(vnode_t vp, daddr64_t blkno, int size, daddr64_t *rablks, int *rasizes, int nrablks, kauth_cred_t cred, buf_t *bpp);
//u_int	minphys(buf_t bp);
//int	physio(void (*f_strategy)(buf_t), buf_t bp, dev_t dev, int flags, u_int (*f_minphys)(buf_t), struct uio *uio, int blocksize);
//#define	BLK_READ	0x01	/* buffer for read */
//#define	BLK_WRITE	0x02	/* buffer for write */
//#define	BLK_META	0x10	/* buffer for metadata */
//#define BLK_ONLYVALID	0x80000000
buf_t	buf_getblk(vnode_t vp, daddr64_t blkno, int size, int slpflag, int slptimeo, int operation);
//buf_t	buf_geteblk(int size);
//void	buf_clear_redundancy_flags(buf_t bp, uint32_t flags);
//uint32_t	buf_redundancy_flags(buf_t bp);
//void	buf_set_redundancy_flags(buf_t bp, uint32_t flags);
//bufattr_t buf_attr(buf_t bp);
//int	buf_static(buf_t bp);
//#ifdef KERNEL_PRIVATE
void	buf_setfilter(buf_t, void (*)(buf_t, void *), void *, void (**)(buf_t, void *), void **);
///* bufattr allocation/duplication/deallocation functions */
//bufattr_t bufattr_alloc(void);
//bufattr_t bufattr_dup (bufattr_t bap);
//void bufattr_free(bufattr_t bap);
//struct cpx *bufattr_cpx(bufattr_t bap);
//void bufattr_setcpx(bufattr_t bap, struct cpx *cpx);
//uint64_t bufattr_cpoff(bufattr_t bap);
//void bufattr_setcpoff(bufattr_t bap, uint64_t);
//int bufattr_rawencrypted(bufattr_t bap);
//int	bufattr_greedymode(bufattr_t bap);
//int	bufattr_isochronous(bufattr_t bap);
//int bufattr_throttled(bufattr_t bap);
//int bufattr_passive(bufattr_t bap);
//int bufattr_nocache(bufattr_t bap);
//int bufattr_meta(bufattr_t bap);
//void bufattr_markmeta(bufattr_t bap);
//int bufattr_delayidlesleep(bufattr_t bap);
//vm_offset_t buf_kernel_addrperm_addr(void * addr);
//void bufattr_markquickcomplete(bufattr_t bap);
//int bufattr_quickcomplete(bufattr_t bap);
//int	count_lock_queue(void);

/*
 * Flags for buf_acquire
 */
//#define BAC_NOWAIT		0x01	/* Don't wait if buffer is busy */
//#define BAC_REMOVE		0x02	/* Remove from free list once buffer is acquired */
//#define BAC_SKIP_NONLOCKED	0x04	/* Don't return LOCKED buffers */
//#define BAC_SKIP_LOCKED		0x08	/* Only return LOCKED buffers */

//errno_t	buf_acquire(buf_t, int, int, int);
//buf_t	buf_create_shadow_priv(buf_t bp, boolean_t force_copy, uintptr_t external_storage, void (*iodone)(buf_t, void *), void *arg);
//void	buf_drop(buf_t);

/* </Header: sys/buf.h> */

/* <Header: sys/time.h> */
void    microuptime(struct timeval *tv);
void    nanotime(struct timespec *ts);
/* </Header: sys/time.h> */

/* <Header: sys/proc.h> */
extern int proc_selfpid(void);
/* </Header: sys/proc.h> */

/* <Header: sys/xattr.h> */
int  xattr_protected(const char *);
/* </Header: sys/xattr.h> */

/* <Header: sys/systm.h> */
void    set_fsblocksize(struct vnode *);
/* </Header: sys/systm.h> */

/* <Header: sys/disk.h> */
#define DKIOCSETBLOCKSIZE                     _IOW('d', 24, uint32_t)
/* </Header: sys/disk.h> */


int xpl_vfs_mount_alloc_init(mount_t *mp, int fd, uint32_t blocksize,
		vnode_t *out_devvp);

int64_t xpl_vfs_mount_get_mount_time(mount_t mp);

void xpl_vfs_mount_teardown(mount_t mp);

static inline enum vtype IFTOVT(const uint16_t mode)
{
	xpl_trace_enter("mode=0%" PRIo16, mode);
	switch(mode & S_IFMT) {
	case S_IFREG:
		return VREG;
	case S_IFDIR:
		return VDIR;
	case S_IFBLK:
		return VBLK;
	case S_IFCHR:
		return VCHR;
	case S_IFLNK:
		return VLNK;
	case S_IFSOCK:
		return VSOCK;
	case S_IFIFO:
		return VFIFO;
	default:
		return VBAD;
	}
}

static inline uint16_t VTTOIF(const enum vtype type)
{
	xpl_trace_enter("type=%d", type);
	switch(type) {
	case VREG:
		return S_IFREG;
	case VDIR:
		return S_IFDIR;
	case VBLK:
		return S_IFBLK;
	case VCHR:
		return S_IFCHR;
	case VLNK:
		return S_IFLNK;
	case VSOCK:
		return S_IFSOCK;
	case VFIFO:
		return S_IFIFO;
	default:
		return 0;
	}
}

/* Normally declared in libkern/copyio.h with arch-specific defines. */
static inline int copyin(const user_addr_t uaddr, void *kaddr, size_t len)
{
	xpl_trace_enter("uaddr=0x%" PRIX64 " kaddr=%p len=%zu",
		(uint64_t)uaddr, kaddr, len);
	memcpy(kaddr, (const char*)(uintptr_t)uaddr, len);
	return 0;
}

static inline void thread_block(void *continuation)
{
	xpl_trace_enter("continuation=%p", continuation);
	sched_yield();
}

#endif /* NTFS_XPL_H */
