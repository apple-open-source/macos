/*
 * Copyright (c) 1999-2016 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 *
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * SDKROOT=macosx.internal cc -I`xcrun -sdk macosx.internal --show-sdk-path`/System/Library/Frameworks/System.framework/Versions/B/PrivateHeaders -arch x86_64 -Os -lktrace -lutil -o fs_usage fs_usage.c
 */

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <strings.h>
#include <fcntl.h>
#include <aio.h>
#include <string.h>
#include <dirent.h>
#include <libc.h>
#include <termios.h>
#include <errno.h>
#include <err.h>
#include <libutil.h>

#include <ktrace/session.h>
#include <System/sys/kdebug.h>
#include <assert.h>

#include <sys/disk.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/syslimits.h>
#include <sys/time.h>
#include <sys/types.h>

#import <mach/clock_types.h>
#import <mach/mach_time.h>

/*
 * MAXCOLS controls when extra data kicks in.
 * MAX_WIDE_MODE_COLS controls -w mode to get even wider data in path.
 */
#define MAXCOLS 132
#define MAX_WIDE_MODE_COLS 264
#define MAXWIDTH MAX_WIDE_MODE_COLS + 64

typedef struct th_info {
	struct th_info *next;
	uint64_t thread;

	/* this is needed for execve()/posix_spawn(), because the command name at the end probe is the new name, which we don't want */
	char command[MAXCOMLEN + 1];

	/*
	 * sometimes a syscall can cause multiple VFS_LOOKUPs of the same vnode with multiple paths
	 * (e.g., one absolute, one relative).  traditional fs_usage behavior was to display the
	 * *first* lookup, so we need to save it off once we see it.
	 */
	uint64_t vnodeid; /* the vp of the VFS_LOOKUP we're currently in, 0 if we are not in one */
	char pathname[MAXPATHLEN];
	char pathname2[MAXPATHLEN];
	char *newest_pathname; /* points to pathname2 if it's filled, otherwise pathname if it's filled, otherwise NULL */

	int pid;
	int type;
	uint64_t arg1;
	uint64_t arg2;
	uint64_t arg3;
	uint64_t arg4;
	uint64_t arg5;
	uint64_t arg6;
	uint64_t arg7;
	uint64_t arg8;
	int waited;
	uint64_t stime;
} *th_info_t;

struct diskio {
	struct diskio *next;
	struct diskio *prev;
	uint64_t  type;
	uint64_t  bp;
	uint64_t  dev;
	uint64_t  blkno;
	uint64_t  iosize;
	uint64_t  io_errno;
	uint64_t  is_meta;
	uint64_t   vnodeid;
	uint64_t  issuing_thread;
	pid_t      issuing_pid;
	uint64_t  completion_thread;
	char issuing_command[MAXCOMLEN + 1];
	uint64_t issued_time;
	uint64_t completed_time;
	struct timeval completed_walltime;
	uint32_t bc_info;
};

#define HASH_SIZE       1024
#define HASH_MASK       (HASH_SIZE - 1)

void setup_ktrace_callbacks(void);
void extend_syscall(uint64_t thread, int type, ktrace_event_t event);

/* printing routines */
bool check_filter_mode(pid_t pid, th_info_t ti, uint64_t type, int error, int retval, char *sc_name);
void format_print(th_info_t ti, char *sc_name, ktrace_event_t event, uint64_t type, int format, uint64_t now, uint64_t stime, int waited, const char *pathname, struct diskio *dio);
int print_open(ktrace_event_t event, uint64_t flags);

/* metadata info hash routines */
void meta_add_name(uint64_t blockno, const char *pathname);
const char *meta_find_name(uint64_t blockno);
void meta_delete_all(void);

/* event ("thread info") routines */
void event_enter(int type, ktrace_event_t event);
void event_exit(char *sc_name, int type, ktrace_event_t event, int format);
th_info_t event_find(uint64_t thread, int type);
void event_delete(th_info_t ti_to_delete);
void event_delete_all(void);
void event_mark_thread_waited(uint64_t);

/* network fd set routines */
void fd_set_is_network(pid_t pid, uint64_t fd, bool set);
bool fd_is_network(pid_t pid, uint64_t fd);
void fd_clear_pid(pid_t pid);
void fd_clear_all(void);

/* shared region address lookup routines */
void init_shared_cache_mapping(void);
void lookup_name(uint64_t user_addr, char **type, char **name);

/* disk I/O tracking routines */
struct diskio *diskio_start(uint64_t type, uint64_t bp, uint64_t dev, uint64_t blkno, uint64_t iosize, ktrace_event_t event);
struct diskio *diskio_find(uint64_t bp);
struct diskio *diskio_complete(uint64_t bp, uint64_t io_errno, uint64_t resid, uint64_t thread, uint64_t curtime, struct timeval curtime_wall);
void diskio_print(struct diskio *dio);
void diskio_free(struct diskio *dio);

/* disk name routines */
#define NFS_DEV -1
#define CS_DEV	-2
char *generate_cs_disk_name(uint64_t dev, char *s);
char *find_disk_name(uint64_t dev);
void cache_disk_names(void);

#define CLASS_MASK	0xff000000
#define CSC_MASK	0xffff0000
#define BSC_INDEX(type)	((type >> 2) & 0x3fff)

#define MACH_vmfault    0x01300008
#define MACH_pageout    0x01300004
#define MACH_sched      0x01400000
#define MACH_stkhandoff 0x01400008
#define MACH_idle	0x01400024

#define BSC_thread_terminate    0x040c05a4

#define HFS_update	     0x3018000
#define HFS_modify_block_end 0x3018004

#define Throttled	0x3010184
#define SPEC_ioctl	0x3060000
#define SPEC_unmap_info	0x3060004
#define proc_exit	0x4010004

#define BC_IO_HIT				0x03070010
#define BC_IO_HIT_STALLED		0x03070020
#define BC_IO_MISS				0x03070040
#define BC_IO_MISS_CUT_THROUGH	0x03070080
#define BC_PLAYBACK_IO			0x03070100
#define BC_STR(s)	( \
	(s == BC_IO_HIT) ? "HIT" : \
	(s == BC_IO_HIT_STALLED) ? "STALL" : \
	(s == BC_IO_MISS) ? "MISS" : \
	(s == BC_IO_MISS_CUT_THROUGH) ? "CUT" : \
	(s == BC_PLAYBACK_IO) ? "PLBK" : \
	(s == 0x0) ? "NONE" : "UNKN" )

#define P_DISKIO_READ	  (DKIO_READ << 2)
#define P_DISKIO_ASYNC	  (DKIO_ASYNC << 2)
#define P_DISKIO_META	  (DKIO_META << 2)
#define P_DISKIO_PAGING   (DKIO_PAGING << 2)
#define P_DISKIO_THROTTLE (DKIO_THROTTLE << 2)
#define P_DISKIO_PASSIVE  (DKIO_PASSIVE << 2)
#define P_DISKIO_NOCACHE  (DKIO_NOCACHE << 2)
#define P_DISKIO_TIER_MASK  (DKIO_TIER_MASK << 2)
#define P_DISKIO_TIER_SHIFT (DKIO_TIER_SHIFT + 2)
#define P_DISKIO_TIER_UPGRADE (DKIO_TIER_UPGRADE << 2)

#define P_DISKIO	(FSDBG_CODE(DBG_DKRW, 0))
#define P_DISKIO_DONE	(P_DISKIO | (DKIO_DONE << 2))
#define P_DISKIO_TYPE	(P_DISKIO | P_DISKIO_READ | P_DISKIO_META | P_DISKIO_PAGING)
#define P_DISKIO_MASK	(CSC_MASK | 0x4)

#define P_WrData	(P_DISKIO)
#define P_RdData	(P_DISKIO | P_DISKIO_READ)
#define P_WrMeta	(P_DISKIO | P_DISKIO_META)
#define P_RdMeta	(P_DISKIO | P_DISKIO_META | P_DISKIO_READ)
#define P_PgOut		(P_DISKIO | P_DISKIO_PAGING)
#define P_PgIn		(P_DISKIO | P_DISKIO_PAGING | P_DISKIO_READ)

#define P_CS_Class		0x0a000000	// DBG_CORESTORAGE
#define P_CS_Type_Mask		0xfffffff0
#define P_CS_IO_Done		0x00000004

#define P_CS_ReadChunk		0x0a000200	// chopped up request
#define P_CS_WriteChunk		0x0a000210
#define P_CS_MetaRead		0x0a000300	// meta data
#define P_CS_MetaWrite		0x0a000310
#define P_CS_TransformRead	0x0a000500	// background transform
#define P_CS_TransformWrite	0x0a000510
#define P_CS_MigrationRead	0x0a000600	// composite disk block migration
#define P_CS_MigrationWrite	0x0a000610
#define P_CS_SYNC_DISK		0x0a010000

#define MSC_map_fd   0x010c00ac

#define BSC_BASE     0x040C0000

// Network related codes
#define	BSC_recvmsg		0x040C006C
#define	BSC_sendmsg		0x040C0070
#define	BSC_recvfrom		0x040C0074
#define BSC_accept		0x040C0078
#define BSC_select		0x040C0174
#define BSC_socket		0x040C0184
#define BSC_connect		0x040C0188
#define BSC_bind		0x040C01A0
#define BSC_listen		0x040C01A8
#define	BSC_sendto		0x040C0214
#define BSC_socketpair		0x040C021C
#define BSC_recvmsg_nocancel	0x040c0644
#define BSC_sendmsg_nocancel	0x040c0648
#define BSC_recvfrom_nocancel	0x040c064c
#define BSC_accept_nocancel	0x040c0650
#define BSC_connect_nocancel	0x040c0664
#define BSC_sendto_nocancel	0x040c0674

#define BSC_exit		0x040C0004
#define BSC_read		0x040C000C
#define BSC_write		0x040C0010
#define BSC_open		0x040C0014
#define BSC_close		0x040C0018
#define BSC_link		0x040C0024
#define BSC_unlink		0x040C0028
#define BSC_chdir		0x040c0030
#define BSC_fchdir		0x040c0034
#define BSC_mknod		0x040C0038
#define BSC_chmod		0x040C003C
#define BSC_chown		0x040C0040
#define BSC_getfsstat		0x040C0048
#define BSC_access		0x040C0084
#define BSC_chflags		0x040C0088
#define BSC_fchflags		0x040C008C
#define BSC_sync		0x040C0090
#define BSC_dup			0x040C00A4
#define BSC_ioctl		0x040C00D8
#define BSC_revoke		0x040C00E0
#define BSC_symlink		0x040C00E4
#define BSC_readlink		0x040C00E8
#define BSC_execve		0x040C00EC
#define BSC_umask		0x040C00F0
#define BSC_chroot		0x040C00F4
#define BSC_msync		0x040C0104
#define BSC_dup2		0x040C0168
#define BSC_fcntl		0x040C0170
#define BSC_fsync		0x040C017C
#define BSC_readv		0x040C01E0
#define BSC_writev		0x040C01E4
#define BSC_fchown		0x040C01EC
#define BSC_fchmod		0x040C01F0
#define BSC_rename		0x040C0200
#define BSC_flock		0x040C020C
#define BSC_mkfifo		0x040C0210
#define BSC_mkdir		0x040C0220
#define BSC_rmdir		0x040C0224
#define BSC_utimes		0x040C0228
#define BSC_futimes		0x040C022C
#define BSC_pread		0x040C0264
#define BSC_pwrite		0x040C0268
#define BSC_statfs		0x040C0274
#define BSC_fstatfs		0x040C0278
#define BSC_unmount	        0x040C027C
#define BSC_mount	        0x040C029C
#define BSC_fdatasync		0x040C02EC
#define BSC_stat		0x040C02F0
#define BSC_fstat		0x040C02F4
#define BSC_lstat		0x040C02F8
#define BSC_pathconf		0x040C02FC
#define BSC_fpathconf		0x040C0300
#define BSC_getdirentries	0x040C0310
#define BSC_mmap		0x040c0314
#define BSC_lseek		0x040c031c
#define BSC_truncate		0x040C0320
#define BSC_ftruncate		0x040C0324
#define BSC_undelete		0x040C0334
#define BSC_open_dprotected_np	0x040C0360
#define BSC_getattrlist		0x040C0370
#define BSC_setattrlist		0x040C0374
#define BSC_getdirentriesattr	0x040C0378
#define BSC_exchangedata	0x040C037C
#define BSC_checkuseraccess	0x040C0380
#define BSC_searchfs		0x040C0384
#define BSC_delete		0x040C0388
#define BSC_copyfile		0x040C038C
#define BSC_fgetattrlist	0x040C0390
#define BSC_fsetattrlist	0x040C0394
#define BSC_getxattr		0x040C03A8
#define BSC_fgetxattr		0x040C03AC
#define BSC_setxattr		0x040C03B0
#define BSC_fsetxattr		0x040C03B4
#define BSC_removexattr		0x040C03B8
#define BSC_fremovexattr	0x040C03BC
#define BSC_listxattr		0x040C03C0
#define BSC_flistxattr		0x040C03C4
#define BSC_fsctl		0x040C03C8
#define BSC_posix_spawn		0x040C03D0
#define BSC_ffsctl		0x040C03D4
#define BSC_open_extended	0x040C0454
#define BSC_umask_extended	0x040C0458
#define BSC_stat_extended	0x040C045C
#define BSC_lstat_extended	0x040C0460
#define BSC_fstat_extended	0x040C0464
#define BSC_chmod_extended	0x040C0468
#define BSC_fchmod_extended	0x040C046C
#define BSC_access_extended	0x040C0470
#define BSC_mkfifo_extended	0x040C048C
#define BSC_mkdir_extended	0x040C0490
#define BSC_aio_fsync		0x040C04E4
#define	BSC_aio_return		0x040C04E8
#define BSC_aio_suspend		0x040C04EC
#define BSC_aio_cancel		0x040C04F0
#define BSC_aio_error		0x040C04F4
#define BSC_aio_read		0x040C04F8
#define BSC_aio_write		0x040C04FC
#define BSC_lio_listio		0x040C0500
#define BSC_sendfile		0x040C0544
#define BSC_stat64		0x040C0548
#define BSC_fstat64		0x040C054C
#define BSC_lstat64		0x040C0550
#define BSC_stat64_extended	0x040C0554
#define BSC_lstat64_extended	0x040C0558
#define BSC_fstat64_extended	0x040C055C
#define BSC_getdirentries64	0x040C0560
#define BSC_statfs64		0x040C0564
#define BSC_fstatfs64		0x040C0568
#define BSC_getfsstat64		0x040C056C
#define BSC_pthread_chdir	0x040C0570
#define BSC_pthread_fchdir	0x040C0574
#define BSC_lchown		0x040C05B0

#define BSC_read_nocancel	0x040c0630
#define BSC_write_nocancel	0x040c0634
#define BSC_open_nocancel	0x040c0638
#define BSC_close_nocancel      0x040c063c
#define BSC_msync_nocancel	0x040c0654
#define BSC_fcntl_nocancel	0x040c0658
#define BSC_select_nocancel	0x040c065c
#define BSC_fsync_nocancel	0x040c0660
#define BSC_readv_nocancel	0x040c066c
#define BSC_writev_nocancel	0x040c0670
#define BSC_pread_nocancel	0x040c0678
#define BSC_pwrite_nocancel	0x040c067c
#define BSC_aio_suspend_nocancel 0x40c0694
#define BSC_guarded_open_np	0x040c06e4
#define BSC_guarded_open_dprotected_np	0x040c0790
#define BSC_guarded_close_np	0x040c06e8
#define BSC_guarded_write_np	0x040c0794
#define BSC_guarded_pwrite_np	0x040c0798
#define BSC_guarded_writev_np	0x040c079c

#define BSC_fsgetpath		0x040c06ac

#define	BSC_getattrlistbulk 0x040c0734

#define BSC_openat		0x040c073c
#define BSC_openat_nocancel	0x040c0740
#define BSC_renameat		0x040c0744
#define BSC_chmodat		0x040c074c
#define BSC_chownat		0x040c0750
#define BSC_fstatat		0x040c0754
#define BSC_fstatat64		0x040c0758
#define BSC_linkat		0x040c075c
#define BSC_unlinkat		0x040c0760
#define BSC_readlinkat		0x040c0764
#define BSC_symlinkat		0x040c0768
#define BSC_mkdirat		0x040c076c
#define BSC_getattrlistat	0x040c0770

#define BSC_msync_extended	0x040e0104
#define BSC_pread_extended	0x040e0264
#define BSC_pwrite_extended	0x040e0268
#define BSC_mmap_extended	0x040e0314
#define BSC_mmap_extended2	0x040f0314

#define FMT_NOTHING -1
#define	FMT_DEFAULT	0
#define FMT_FD		1
#define FMT_FD_IO	2
#define FMT_FD_2	3
#define FMT_SOCKET	4
#define	FMT_PGIN	5
#define	FMT_PGOUT	6
#define	FMT_CACHEHIT	7
#define FMT_DISKIO	8
#define FMT_LSEEK	9
#define FMT_PREAD	10
#define FMT_FTRUNC	11
#define FMT_TRUNC	12
#define FMT_SELECT	13
#define FMT_OPEN	14
#define	FMT_AIO_FSYNC	15
#define	FMT_AIO_RETURN	16
#define	FMT_AIO_SUSPEND	17
#define	FMT_AIO_CANCEL	18
#define	FMT_AIO		19
#define	FMT_LIO_LISTIO	20
#define FMT_MSYNC	21
#define	FMT_FCNTL	22
#define FMT_ACCESS	23
#define FMT_CHMOD	24
#define FMT_FCHMOD	25
#define	FMT_CHMOD_EXT	26
#define	FMT_FCHMOD_EXT	27
#define FMT_CHFLAGS	28
#define FMT_FCHFLAGS	29
#define	FMT_IOCTL	30
#define FMT_MMAP	31
#define FMT_UMASK	32
#define FMT_SENDFILE	33
#define FMT_IOCTL_SYNC	34
#define FMT_MOUNT	35
#define FMT_UNMOUNT	36
#define FMT_DISKIO_CS	37
#define FMT_SYNC_DISK_CS	38
#define FMT_IOCTL_UNMAP	39
#define FMT_UNMAP_INFO	40
#define FMT_HFS_update	41
#define FMT_FLOCK	42
#define FMT_AT		43
#define FMT_CHMODAT	44
#define FMT_OPENAT  45
#define FMT_RENAMEAT	46
#define FMT_IOCTL_SYNCCACHE 47
#define FMT_GUARDED_OPEN 48

#define DBG_FUNC_ALL	(DBG_FUNC_START | DBG_FUNC_END)

#pragma mark global state

ktrace_session_t s;
bool BC_flag = false;
bool RAW_flag = false;
bool wideflag = false;
bool include_waited_flag = false;
bool want_kernel_task = true;
dispatch_source_t stop_timer, sigquit_source, sigpipe_source, sighup_source, sigterm_source, sigwinch_source;
uint64_t mach_time_of_first_event;
uint64_t start_time_ns = 0;
uint64_t end_time_ns = UINT64_MAX;
unsigned int columns = 0;

/*
 * Network only or filesystem only output filter
 * Default of zero means report all activity - no filtering
 */
#define FILESYS_FILTER    0x01
#define NETWORK_FILTER    0x02
#define EXEC_FILTER	  0x08
#define PATHNAME_FILTER	  0x10
#define DISKIO_FILTER	0x20
#define DEFAULT_DO_NOT_FILTER  0x00

int filter_mode = DEFAULT_DO_NOT_FILTER;
bool show_cachehits = false;

#pragma mark syscall lookup table

#define MAX_BSD_SYSCALL	526

struct bsd_syscall {
	char *sc_name;
	int	sc_format;
};

#define NORMAL_SYSCALL(name) \
	[BSC_INDEX(BSC_##name)] = {#name, FMT_DEFAULT}

#define SYSCALL(name, format) \
	[BSC_INDEX(BSC_##name)] = {#name, format}

#define SYSCALL_NAMED(name, displayname, format) \
	[BSC_INDEX(BSC_##name)] = {#displayname, format}

#define SYSCALL_WITH_NOCANCEL(name, format) \
	[BSC_INDEX(BSC_##name)] = {#name, format}, \
	[BSC_INDEX(BSC_##name##_nocancel)] = {#name, format}

const struct bsd_syscall bsd_syscalls[MAX_BSD_SYSCALL] = {
	SYSCALL(sendfile, FMT_FD), /* this should be changed to FMT_SENDFILE once we add an extended info trace event */
	SYSCALL_WITH_NOCANCEL(recvmsg, FMT_FD_IO),
	SYSCALL_WITH_NOCANCEL(sendmsg, FMT_FD_IO),
	SYSCALL_WITH_NOCANCEL(recvfrom, FMT_FD_IO),
	SYSCALL_WITH_NOCANCEL(sendto, FMT_FD_IO),
	SYSCALL_WITH_NOCANCEL(select, FMT_SELECT),
	SYSCALL_WITH_NOCANCEL(accept, FMT_FD_2),
	SYSCALL(socket, FMT_SOCKET),
	SYSCALL_WITH_NOCANCEL(connect, FMT_FD),
	SYSCALL(bind, FMT_FD),
	SYSCALL(listen, FMT_FD),
	SYSCALL(mmap, FMT_MMAP),
	NORMAL_SYSCALL(socketpair),
	NORMAL_SYSCALL(getxattr),
	NORMAL_SYSCALL(setxattr),
	NORMAL_SYSCALL(removexattr),
	NORMAL_SYSCALL(listxattr),
	NORMAL_SYSCALL(stat),
	NORMAL_SYSCALL(stat64),
	NORMAL_SYSCALL(stat_extended),
	SYSCALL_NAMED(stat64_extended, stat_extended64, FMT_DEFAULT), /* should be stat64_extended ? */
	SYSCALL(mount, FMT_MOUNT),
	SYSCALL(unmount, FMT_UNMOUNT),
	NORMAL_SYSCALL(exit),
	NORMAL_SYSCALL(execve),
	NORMAL_SYSCALL(posix_spawn),
	SYSCALL_WITH_NOCANCEL(open, FMT_OPEN),
	SYSCALL(open_extended, FMT_OPEN),
	SYSCALL(guarded_open_np, FMT_GUARDED_OPEN),
	SYSCALL_NAMED(open_dprotected_np, open_dprotected, FMT_OPEN),
	SYSCALL(guarded_open_dprotected_np, FMT_GUARDED_OPEN),
	SYSCALL(dup, FMT_FD_2),
	SYSCALL(dup2, FMT_FD_2),
	SYSCALL_WITH_NOCANCEL(close, FMT_FD),
	SYSCALL(guarded_close_np, FMT_FD),
	SYSCALL_WITH_NOCANCEL(read, FMT_FD_IO),
	SYSCALL_WITH_NOCANCEL(write, FMT_FD_IO),
	SYSCALL(guarded_write_np, FMT_FD_IO),
	SYSCALL(guarded_pwrite_np, FMT_FD_IO),
	SYSCALL(guarded_writev_np, FMT_FD_IO),
	SYSCALL(fgetxattr, FMT_FD),
	SYSCALL(fsetxattr, FMT_FD),
	SYSCALL(fremovexattr, FMT_FD),
	SYSCALL(flistxattr, FMT_FD),
	SYSCALL(fstat, FMT_FD),
	SYSCALL(fstat64, FMT_FD),
	SYSCALL(fstat_extended, FMT_FD),
	SYSCALL(fstat64_extended, FMT_FD),
	NORMAL_SYSCALL(lstat),
	NORMAL_SYSCALL(lstat64),
	NORMAL_SYSCALL(lstat_extended),
	SYSCALL_NAMED(lstat64_extended, lstat_extended64, FMT_DEFAULT),
	NORMAL_SYSCALL(link),
	NORMAL_SYSCALL(unlink),
	NORMAL_SYSCALL(mknod),
	SYSCALL(umask, FMT_UMASK),
	SYSCALL(umask_extended, FMT_UMASK),
	SYSCALL(chmod, FMT_CHMOD),
	SYSCALL(chmod_extended, FMT_CHMOD_EXT),
	SYSCALL(fchmod, FMT_FCHMOD),
	SYSCALL(fchmod_extended, FMT_FCHMOD_EXT),
	NORMAL_SYSCALL(chown),
	NORMAL_SYSCALL(lchown),
	SYSCALL(fchown, FMT_FD),
	SYSCALL(access, FMT_ACCESS),
	NORMAL_SYSCALL(access_extended),
	NORMAL_SYSCALL(chdir),
	NORMAL_SYSCALL(pthread_chdir),
	NORMAL_SYSCALL(chroot),
	NORMAL_SYSCALL(utimes),
	SYSCALL_NAMED(delete, delete-Carbon, FMT_DEFAULT),
	NORMAL_SYSCALL(undelete),
	NORMAL_SYSCALL(revoke),
	NORMAL_SYSCALL(fsctl),
	SYSCALL(ffsctl, FMT_FD),
	SYSCALL(chflags, FMT_CHFLAGS),
	SYSCALL(fchflags, FMT_FCHFLAGS),
	SYSCALL(fchdir, FMT_FD),
	SYSCALL(pthread_fchdir, FMT_FD),
	SYSCALL(futimes, FMT_FD),
	NORMAL_SYSCALL(sync),
	NORMAL_SYSCALL(symlink),
	NORMAL_SYSCALL(readlink),
	SYSCALL_WITH_NOCANCEL(fsync, FMT_FD),
	SYSCALL(fdatasync, FMT_FD),
	SYSCALL_WITH_NOCANCEL(readv, FMT_FD_IO),
	SYSCALL_WITH_NOCANCEL(writev, FMT_FD_IO),
	SYSCALL_WITH_NOCANCEL(pread, FMT_PREAD),
	SYSCALL_WITH_NOCANCEL(pwrite, FMT_PREAD),
	NORMAL_SYSCALL(mkdir),
	NORMAL_SYSCALL(mkdir_extended),
	NORMAL_SYSCALL(mkfifo),
	NORMAL_SYSCALL(mkfifo_extended),
	NORMAL_SYSCALL(rmdir),
	NORMAL_SYSCALL(statfs),
	NORMAL_SYSCALL(statfs64),
	NORMAL_SYSCALL(getfsstat),
	NORMAL_SYSCALL(getfsstat64),
	SYSCALL(fstatfs, FMT_FD),
	SYSCALL(fstatfs64, FMT_FD),
	NORMAL_SYSCALL(pathconf),
	SYSCALL(fpathconf, FMT_FD),
	SYSCALL(getdirentries, FMT_FD_IO),
	SYSCALL(getdirentries64, FMT_FD_IO),
	SYSCALL(lseek, FMT_LSEEK),
	SYSCALL(truncate, FMT_TRUNC),
	SYSCALL(ftruncate, FMT_FTRUNC),
	SYSCALL(flock, FMT_FLOCK),
	NORMAL_SYSCALL(getattrlist),
	NORMAL_SYSCALL(setattrlist),
	SYSCALL(fgetattrlist, FMT_FD),
	SYSCALL(fsetattrlist, FMT_FD),
	SYSCALL(getdirentriesattr, FMT_FD),
	NORMAL_SYSCALL(exchangedata),
	NORMAL_SYSCALL(rename),
	NORMAL_SYSCALL(copyfile),
	NORMAL_SYSCALL(checkuseraccess),
	NORMAL_SYSCALL(searchfs),
	SYSCALL(aio_fsync, FMT_AIO_FSYNC),
	SYSCALL(aio_return, FMT_AIO_RETURN),
	SYSCALL_WITH_NOCANCEL(aio_suspend, FMT_AIO_SUSPEND),
	SYSCALL(aio_cancel, FMT_AIO_CANCEL),
	SYSCALL(aio_error, FMT_AIO),
	SYSCALL(aio_read, FMT_AIO),
	SYSCALL(aio_write, FMT_AIO),
	SYSCALL(lio_listio, FMT_LIO_LISTIO),
	SYSCALL_WITH_NOCANCEL(msync, FMT_MSYNC),
	SYSCALL_WITH_NOCANCEL(fcntl, FMT_FCNTL),
	SYSCALL(ioctl, FMT_IOCTL),
	NORMAL_SYSCALL(fsgetpath),
	NORMAL_SYSCALL(getattrlistbulk),
	SYSCALL_WITH_NOCANCEL(openat, FMT_OPENAT), /* open_nocancel() was previously shown as "open_nocanel" (note spelling) */
	SYSCALL(renameat, FMT_RENAMEAT),
	SYSCALL(chmodat, FMT_CHMODAT),
	SYSCALL(chownat, FMT_AT),
	SYSCALL(fstatat, FMT_AT),
	SYSCALL(fstatat64, FMT_AT),
	SYSCALL(linkat, FMT_AT),
	SYSCALL(unlinkat, FMT_AT),
	SYSCALL(readlinkat, FMT_AT),
	SYSCALL(symlinkat, FMT_AT),
	SYSCALL(mkdirat, FMT_AT),
	SYSCALL(getattrlistat, FMT_AT),
};

static void
get_screenwidth(void)
{
	struct winsize size;

	columns = MAXCOLS;

	if (isatty(STDOUT_FILENO)) {
		if (ioctl(1, TIOCGWINSZ, &size) != -1) {
			columns = size.ws_col;

			if (columns > MAXWIDTH)
				columns = MAXWIDTH;
		}
	}
}

static uint64_t
mach_to_nano(uint64_t mach)
{
	uint64_t nanoseconds = 0;
	assert(ktrace_convert_timestamp_to_nanoseconds(s, mach, &nanoseconds) == 0);

	return nanoseconds;
}

static void
exit_usage(void)
{
	const char *myname;

	myname = getprogname();

	fprintf(stderr, "Usage: %s [-e] [-w] [-f mode] [-b] [-t seconds] [-R rawfile [-S start_time] [-E end_time]] [pid | cmd [pid | cmd] ...]\n", myname);
	fprintf(stderr, "  -e    exclude the specified list of pids from the sample\n");
	fprintf(stderr, "        and exclude fs_usage by default\n");
	fprintf(stderr, "  -w    force wider, detailed, output\n");
	fprintf(stderr, "  -f    output is based on the mode provided\n");
	fprintf(stderr, "          mode = \"network\"  Show network-related events\n");
	fprintf(stderr, "          mode = \"filesys\"  Show filesystem-related events\n");
	fprintf(stderr, "          mode = \"pathname\" Show only pathname-related events\n");
	fprintf(stderr, "          mode = \"exec\"     Show only exec and spawn events\n");
	fprintf(stderr, "          mode = \"diskio\"   Show only disk I/O events\n");
	fprintf(stderr, "          mode = \"cachehit\" In addition, show cache hits\n");
	fprintf(stderr, "  -b    annotate disk I/O events with BootCache info (if available)\n");
	fprintf(stderr, "  -t    specifies timeout in seconds (for use in automated tools)\n");
	fprintf(stderr, "  -R    specifies a raw trace file to process\n");
	fprintf(stderr, "  -S    if -R is specified, selects a start point in microseconds\n");
	fprintf(stderr, "  -E    if -R is specified, selects an end point in microseconds\n");
	fprintf(stderr, "  pid   selects process(s) to sample\n");
	fprintf(stderr, "  cmd   selects process(s) matching command string to sample\n");
	fprintf(stderr, "By default (no options) the following processes are excluded from the output:\n");
	fprintf(stderr, "fs_usage, Terminal, telnetd, sshd, rlogind, tcsh, csh, sh\n\n");

	exit(1);
}

int
main(int argc, char *argv[])
{
	char ch;
	int rv;
	bool exclude_pids = false;
	uint64_t time_limit_ns = 0;

	get_screenwidth();

	s = ktrace_session_create();
	assert(s != NULL);
	(void)ktrace_ignore_process_filter_for_event(s, P_WrData);
	(void)ktrace_ignore_process_filter_for_event(s, P_RdData);
	(void)ktrace_ignore_process_filter_for_event(s, P_WrMeta);
	(void)ktrace_ignore_process_filter_for_event(s, P_RdMeta);
	(void)ktrace_ignore_process_filter_for_event(s, P_PgOut);
	(void)ktrace_ignore_process_filter_for_event(s, P_PgIn);

	while ((ch = getopt(argc, argv, "bewf:R:S:E:t:W")) != -1) {
		switch (ch) {
			case 'e':
				exclude_pids = true;
				break;

			case 'w':
				wideflag = 1;
				columns = MAX_WIDE_MODE_COLS;
				break;

			case 'W':
				include_waited_flag = true;
				break;

			case 'f':
				if (!strcmp(optarg, "network"))
					filter_mode |= NETWORK_FILTER;
				else if (!strcmp(optarg, "filesys"))
					filter_mode |= FILESYS_FILTER;
				else if (!strcmp(optarg, "cachehit"))
					show_cachehits = true;
				else if (!strcmp(optarg, "exec"))
					filter_mode |= EXEC_FILTER;
				else if (!strcmp(optarg, "pathname"))
					filter_mode |= PATHNAME_FILTER;
				else if (!strcmp(optarg, "diskio"))
					filter_mode |= DISKIO_FILTER;

				break;

			case 'b':
				BC_flag = true;
				break;

			case 't':
				time_limit_ns = (uint64_t)(NSEC_PER_SEC * atof(optarg));
				if (time_limit_ns == 0) {
					fprintf(stderr, "ERROR: could not set time limit to %s\n",
							optarg);
					exit(1);
				}
				break;

			case 'R':
				RAW_flag = true;
				rv = ktrace_set_file(s, optarg);
				if (rv) {
					fprintf(stderr, "ERROR: reading trace from '%s' failed (%s)\n", optarg, strerror(errno));
					exit(1);
				}
				break;

			case 'S':
				start_time_ns = NSEC_PER_SEC * atof(optarg);
				break;

			case 'E':
				end_time_ns = NSEC_PER_SEC * atof(optarg);
				break;

			default:
				exit_usage();
		}
	}

	argc -= optind;
	argv += optind;

	if (time_limit_ns > 0 && RAW_flag) {
		fprintf(stderr, "NOTE: time limit ignored when a raw file is specified\n");
		time_limit_ns = 0;
	}

	if (!RAW_flag) {
		if (geteuid() != 0) {
			fprintf(stderr, "'fs_usage' must be run as root...\n");
			exit(1);
		}

		/*
		 * ktrace can't both *in*clude and *ex*clude pids, so: if we are
		 * already excluding pids, or if we are not explicitly including
		 * or excluding any pids, then exclude the defaults.
		 *
		 * if on the other hand we are explicitly including pids, we'll
		 * filter the defaults out naturally.
		 */
		if (exclude_pids || argc == 0) {
			ktrace_exclude_process(s, "fs_usage");
			ktrace_exclude_process(s, "Terminal");
			ktrace_exclude_process(s, "telnetd");
			ktrace_exclude_process(s, "telnet");
			ktrace_exclude_process(s, "sshd");
			ktrace_exclude_process(s, "rlogind");
			ktrace_exclude_process(s, "tcsh");
			ktrace_exclude_process(s, "csh");
			ktrace_exclude_process(s, "sh");
			ktrace_exclude_process(s, "zsh");
#if TARGET_OS_EMBEDDED
			ktrace_exclude_process(s, "dropbear");
#endif /* TARGET_OS_EMBEDDED */
		}
	}

	/*
	 * If we're *in*cluding processes, also *in*clude the kernel_task, which
	 * issues trace points when disk I/Os complete.  But set a flag for us to
	 * avoid showing events attributed to the kernel_task.
	 *
	 * If the user actually wants to those events, we'll change that flag in
	 * the loop below.
	 */
	if (argc > 0 && !exclude_pids) {
		ktrace_filter_pid(s, 0);
		want_kernel_task = false;
	}

	/*
	 * Process the list of specified pids, and in/exclude them as
	 * appropriate.
	 */
	while (argc > 0) {
		pid_t pid;
		char *name;
		char *endptr;

		name = argv[0];
		pid = (pid_t)strtoul(name, &endptr, 10);

		if (*name != '\0' && *endptr == '\0') {
			if (exclude_pids) {
				rv = ktrace_exclude_pid(s, pid);
			} else {
				if (pid == 0)
					want_kernel_task = true;
				else
					rv = ktrace_filter_pid(s, pid);
			}
		} else {
			if (exclude_pids) {
				rv = ktrace_exclude_process(s, name);
			} else {
				if (!strcmp(name, "kernel_task"))
					want_kernel_task = true;
				else
					rv = ktrace_filter_process(s, name);
			}
		}

		if (rv == EINVAL) {
			fprintf(stderr, "ERROR: cannot both include and exclude simultaneously\n");
			exit(1);
		} else {
			assert(!rv);
		}

		argc--;
		argv++;
	}

	/* provides SIGINT, SIGHUP, SIGPIPE, SIGTERM handlers */
	ktrace_set_signal_handler(s);

	ktrace_set_completion_handler(s, ^{
		exit(0);
	});

	signal(SIGWINCH, SIG_IGN);
	sigwinch_source = dispatch_source_create(DISPATCH_SOURCE_TYPE_SIGNAL, SIGWINCH, 0, dispatch_get_main_queue());
	dispatch_source_set_event_handler(sigwinch_source, ^{
		if (!wideflag)
			get_screenwidth();
	});
	dispatch_activate(sigwinch_source);

	init_shared_cache_mapping();

	cache_disk_names();

	setup_ktrace_callbacks();

	ktrace_set_dropped_events_handler(s, ^{
		fprintf(stderr, "fs_usage: buffer overrun, events generated too quickly\n");

		/* clear any state that is now potentially invalid */

		event_delete_all();
		fd_clear_all();
		meta_delete_all();
	});

	ktrace_set_default_event_names_enabled(KTRACE_FEATURE_DISABLED);
	ktrace_set_execnames_enabled(s, KTRACE_FEATURE_LAZY);
	ktrace_set_vnode_paths_enabled(s, true);
	/* no need to symbolicate addresses */
	ktrace_set_uuid_map_enabled(s, KTRACE_FEATURE_DISABLED);

	rv = ktrace_start(s, dispatch_get_main_queue());

	if (rv) {
		perror("ktrace_start");
		exit(1);
	}

	if (time_limit_ns > 0) {
		dispatch_after(dispatch_time(DISPATCH_TIME_NOW, time_limit_ns),
				dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0),
		^{
			ktrace_end(s, 0);
		});
	}

	dispatch_main();

	return 0;
}

void
setup_ktrace_callbacks(void)
{
	ktrace_events_subclass(s, DBG_MACH, DBG_MACH_EXCP_SC, ^(ktrace_event_t event) {
		int type;

		type = event->debugid & KDBG_EVENTID_MASK;

		if (type == MSC_map_fd) {
			if (event->debugid & DBG_FUNC_START) {
				event_enter(type, event);
			} else {
				event_exit("map_fd", type, event, FMT_FD);
			}
		}
	});

	ktrace_events_subclass(s, DBG_MACH, DBG_MACH_VM, ^(ktrace_event_t event) {
		th_info_t ti;
		unsigned int type;

		type = event->debugid & KDBG_EVENTID_MASK;

		if (type != MACH_pageout && type != MACH_vmfault)
			return;

		if (event->debugid & DBG_FUNC_START) {
			event_enter(type, event);
		} else {
			switch (type) {
				case MACH_pageout:
					if (event->arg2)
						event_exit("PAGE_OUT_ANON", type, event, FMT_PGOUT);
					else
						event_exit("PAGE_OUT_FILE", type, event, FMT_PGOUT);

					break;

				case MACH_vmfault:
					if (event->arg4 == DBG_PAGEIN_FAULT)
						event_exit("PAGE_IN", type, event, FMT_PGIN);
					else if (event->arg4 == DBG_PAGEINV_FAULT)
						event_exit("PAGE_IN_FILE", type, event, FMT_PGIN);
					else if (event->arg4 == DBG_PAGEIND_FAULT)
						event_exit("PAGE_IN_ANON", type, event, FMT_PGIN);
					else if (event->arg4 == DBG_CACHE_HIT_FAULT)
						event_exit("CACHE_HIT", type, event, FMT_CACHEHIT);
					else if ((ti = event_find(event->threadid, type)))
						event_delete(ti);

					break;

				default:
					abort();
			}
		}
	});

	if (include_waited_flag || RAW_flag) {
		ktrace_events_subclass(s, DBG_MACH, DBG_MACH_SCHED, ^(ktrace_event_t event) {
			int type;

			type = event->debugid & KDBG_EVENTID_MASK;

			switch (type) {
				case MACH_idle:
				case MACH_sched:
				case MACH_stkhandoff:
					event_mark_thread_waited(event->threadid);
			}
		});
	}

	ktrace_events_subclass(s, DBG_FSYSTEM, DBG_FSRW, ^(ktrace_event_t event) {
		th_info_t ti;
		int type;

		type = event->debugid & KDBG_EVENTID_MASK;

		switch (type) {
			case HFS_modify_block_end:
				/*
				 * the expected path here is as follows:
				 * enter syscall
				 * look up a path, which gets stored in ti->vnode / ti->pathname
				 * modify a metadata block -- we assume the modification has something to do with the path that was looked up
				 * leave syscall
				 * ...
				 * later, someone writes that metadata block; the last path associated with it is attributed
				 */
				if ((ti = event_find(event->threadid, 0))) {
					if (ti->newest_pathname)
						meta_add_name(event->arg2, ti->newest_pathname);
				}

				break;

			case VFS_LOOKUP:
				if (event->debugid & DBG_FUNC_START) {
					if ((ti = event_find(event->threadid, 0)) && !ti->vnodeid) {
						ti->vnodeid = event->arg1;
					}
				}

				/* it can be both start and end */

				if (event->debugid & DBG_FUNC_END) {
					if ((ti = event_find(event->threadid, 0)) && ti->vnodeid) {
						const char *pathname;

						pathname = ktrace_get_path_for_vp(s, ti->vnodeid);

						ti->vnodeid = 0;

						if (pathname) {
							if (ti->pathname[0] == '\0') {
								strncpy(ti->pathname, pathname, MAXPATHLEN);
								ti->newest_pathname = ti->pathname;
							} else if (ti->pathname2[0] == '\0') {
								strncpy(ti->pathname2, pathname, MAXPATHLEN);
								ti->newest_pathname = ti->pathname2;
							}
						}
					}
				}

				break;
		}

		if (type != Throttled && type != HFS_update)
			return;

		if (event->debugid & DBG_FUNC_START) {
			event_enter(type, event);
		} else {
			switch (type) {
				case Throttled:
					event_exit("  THROTTLED", type, event, FMT_NOTHING);
					break;

				case HFS_update:
					event_exit("  HFS_update", type, event, FMT_HFS_update);
					break;

				default:
					abort();
			}
		}
	});

	ktrace_events_subclass(s, DBG_FSYSTEM, DBG_DKRW, ^(ktrace_event_t event) {
		struct diskio *dio;
		unsigned int type;

		type = event->debugid & KDBG_EVENTID_MASK;

		if ((type & P_DISKIO_MASK) == P_DISKIO) {
			diskio_start(type, event->arg1, event->arg2, event->arg3, event->arg4, event);
		} else if ((type & P_DISKIO_MASK) == P_DISKIO_DONE) {
			if ((dio = diskio_complete(event->arg1, event->arg4, event->arg3, event->threadid, event->timestamp, event->walltime))) {
				dio->vnodeid = event->arg2;
				diskio_print(dio);
				diskio_free(dio);
			}
		}
	});

	ktrace_events_subclass(s, DBG_FSYSTEM, DBG_IOCTL, ^(ktrace_event_t event) {
		th_info_t ti;
		int type;
		pid_t pid;

		type = event->debugid & KDBG_EVENTID_MASK;

		switch (type) {
			case SPEC_unmap_info:
				pid = ktrace_get_pid_for_thread(s, event->threadid);

				if (check_filter_mode(pid, NULL, SPEC_unmap_info, 0, 0, "SPEC_unmap_info"))
					format_print(NULL, "  TrimExtent", event, type, FMT_UNMAP_INFO, event->timestamp, event->timestamp, 0, "", NULL);

				break;

			case SPEC_ioctl:
				if (event->debugid & DBG_FUNC_START) {
					event_enter(type, event);
				} else {
					if (event->arg2 == DKIOCSYNCHRONIZECACHE)
						event_exit("IOCTL", type, event, FMT_IOCTL_SYNCCACHE);
					else if (event->arg2 == DKIOCUNMAP)
						event_exit("IOCTL", type, event, FMT_IOCTL_UNMAP);
					else if (event->arg2 == DKIOCSYNCHRONIZE && (event->debugid & DBG_FUNC_ALL) == DBG_FUNC_NONE)
						event_exit("IOCTL", type, event, FMT_IOCTL_SYNC);
					else if ((ti = event_find(event->threadid, type)))
						event_delete(ti);
				}

				break;
		}
	});

	if (BC_flag || RAW_flag) {
		ktrace_events_subclass(s, DBG_FSYSTEM, DBG_BOOTCACHE, ^(ktrace_event_t event) {
			struct diskio *dio;
			unsigned int type;

			type = event->debugid & KDBG_EVENTID_MASK;

			switch (type) {
				case BC_IO_HIT:
				case BC_IO_HIT_STALLED:
				case BC_IO_MISS:
				case BC_IO_MISS_CUT_THROUGH:
				case BC_PLAYBACK_IO:
					if ((dio = diskio_find(event->arg1)) != NULL)
						dio->bc_info = type;
			}
		});
	}

	void (^bsd_sc_proc_cb)(ktrace_event_t event) = ^(ktrace_event_t event) {
		int type, index;
		pid_t pid;

		type = event->debugid & KDBG_EVENTID_MASK;

		switch (type) {
			case BSC_exit: /* see below */
				return;

			case proc_exit:
				event->arg1 = event->arg2 >> 8;
				type = BSC_exit;

				pid = ktrace_get_pid_for_thread(s, event->threadid);
				fd_clear_pid(pid);

				break;

			case BSC_mmap:
				if (event->arg4 & MAP_ANON)
					return;

				break;
		}

		if ((index = BSC_INDEX(type)) >= MAX_BSD_SYSCALL)
			return;

		if (!bsd_syscalls[index].sc_name)
			return;

		if (event->debugid & DBG_FUNC_START) {
			event_enter(type, event);
		} else {
			event_exit(bsd_syscalls[index].sc_name, type, event, bsd_syscalls[index].sc_format);
		}
	};

	ktrace_events_subclass(s, DBG_BSD, DBG_BSD_EXCP_SC, bsd_sc_proc_cb);
	ktrace_events_subclass(s, DBG_BSD, DBG_BSD_PROC, bsd_sc_proc_cb);

	ktrace_events_range(s, KDBG_EVENTID(DBG_BSD, DBG_BSD_SC_EXTENDED_INFO, 0), KDBG_EVENTID(DBG_BSD, DBG_BSD_SC_EXTENDED_INFO2 + 1, 0), ^(ktrace_event_t event) {
		extend_syscall(event->threadid, event->debugid & KDBG_EVENTID_MASK, event);
	});

	ktrace_events_subclass(s, DBG_CORESTORAGE, DBG_CS_IO, ^(ktrace_event_t event) {
		// the usual DBG_FUNC_START/END does not work for i/o since it will
		// return on a different thread, this code uses the P_CS_IO_Done (0x4) bit
		// instead. the trace command doesn't know how handle either method
		// (unmatched start/end or 0x4) but works a little better this way.

		struct diskio *dio;
		int cs_type = event->debugid & P_CS_Type_Mask;   // strip out the done bit
		bool start = (event->debugid & P_CS_IO_Done) != P_CS_IO_Done;

		switch (cs_type) {
			case P_CS_ReadChunk:
			case P_CS_WriteChunk:
			case P_CS_MetaRead:
			case P_CS_MetaWrite:
				if (start) {
					diskio_start(cs_type, event->arg2, event->arg1, event->arg3, event->arg4, event);
				} else {
					if ((dio = diskio_complete(event->arg2, event->arg4, event->arg3, event->threadid, event->timestamp, event->walltime))) {
						diskio_print(dio);
						diskio_free(dio);
					}
				}

				break;
			case P_CS_TransformRead:
			case P_CS_TransformWrite:
			case P_CS_MigrationRead:
			case P_CS_MigrationWrite:
				if (start) {
					diskio_start(cs_type, event->arg2, CS_DEV, event->arg3, event->arg4, event);
				} else {
					if ((dio = diskio_complete(event->arg2, event->arg4, event->arg3, event->threadid, event->timestamp, event->walltime))) {
						diskio_print(dio);
						diskio_free(dio);
					}
				}

				break;
		}
	});

	ktrace_events_subclass(s, DBG_CORESTORAGE, 1 /* DBG_CS_SYNC */, ^(ktrace_event_t event) {
		int cs_type = event->debugid & P_CS_Type_Mask; // strip out the done bit
		bool start = (event->debugid & P_CS_IO_Done) != P_CS_IO_Done;

		if (cs_type == P_CS_SYNC_DISK) {
			if (start) {
				event_enter(cs_type, event);
			} else {
				event_exit("  SyncCacheCS", cs_type, event, FMT_SYNC_DISK_CS);
			}
		}
	});
}

/*
 * Handle system call extended trace data.
 * pread and pwrite:
 *     Wipe out the kd args that were collected upon syscall_entry
 *     because it is the extended info that we really want, and it
 *     is all we really need.
 */
void
extend_syscall(uint64_t thread, int type, ktrace_event_t event)
{
	th_info_t ti;

	switch (type) {
		case BSC_mmap_extended:
			if ((ti = event_find(thread, BSC_mmap)) == NULL)
				return;

			ti->arg8   = ti->arg3;  /* save protection */
			ti->arg1   = event->arg1;  /* the fd */
			ti->arg3   = event->arg2;  /* bottom half address */
			ti->arg5   = event->arg3;  /* bottom half size */
			break;

		case BSC_mmap_extended2:
			if ((ti = event_find(thread, BSC_mmap)) == NULL)
				return;

			ti->arg2   = event->arg1;  /* top half address */
			ti->arg4   = event->arg2;  /* top half size */
			ti->arg6   = event->arg3;  /* top half file offset */
			ti->arg7   = event->arg4;  /* bottom half file offset */
			break;

		case BSC_msync_extended:
			if ((ti = event_find(thread, BSC_msync)) == NULL) {
				if ((ti = event_find(thread, BSC_msync_nocancel)) == NULL)
					return;
			}

			ti->arg4   = event->arg1;  /* top half address */
			ti->arg5   = event->arg2;  /* top half size */
			break;

		case BSC_pread_extended:
			if ((ti = event_find(thread, BSC_pread)) == NULL) {
				if ((ti = event_find(thread, BSC_pread_nocancel)) == NULL)
					return;
			}

			ti->arg1   = event->arg1;  /* the fd */
			ti->arg2   = event->arg2;  /* nbytes */
			ti->arg3   = event->arg3;  /* top half offset */
			ti->arg4   = event->arg4;  /* bottom half offset */
			break;

		case BSC_pwrite_extended:
			if ((ti = event_find(thread, BSC_pwrite)) == NULL) {
				if ((ti = event_find(thread, BSC_pwrite_nocancel)) == NULL)
					return;
			}

			ti->arg1   = event->arg1;  /* the fd */
			ti->arg2   = event->arg2;  /* nbytes */
			ti->arg3   = event->arg3;  /* top half offset */
			ti->arg4   = event->arg4;  /* bottom half offset */
			break;
	}
}

#pragma mark printing routines

static void
get_mode_nibble(char *buf, uint64_t smode, uint64_t special, char x_on, char x_off)
{
	if (smode & 04)
		buf[0] = 'r';

	if (smode & 02)
		buf[1] = 'w';

	if (smode & 01) {
		if (special)
			buf[2] = x_on;
		else
			buf[2] = 'x';
	} else {
		if (special)
			buf[2] = x_off;
	}
}

static void
get_mode_string(uint64_t mode, char *buf)
{
	memset(buf, '-', 9);
	buf[9] = '\0';

	get_mode_nibble(&buf[6], mode, (mode & 01000), 't', 'T');
	get_mode_nibble(&buf[3], (mode>>3), (mode & 02000), 's', 'S');
	get_mode_nibble(&buf[0], (mode>>6), (mode & 04000), 's', 'S');
}

static int
clip_64bit(char *s, uint64_t value)
{
	int clen = 0;

	if ( (value & 0xff00000000000000LL) )
		clen = printf("%s0x%16.16qx", s, value);
	else if ( (value & 0x00ff000000000000LL) )
		clen = printf("%s0x%14.14qx  ", s, value);
	else if ( (value & 0x0000ff0000000000LL) )
		clen = printf("%s0x%12.12qx    ", s, value);
	else if ( (value & 0x000000ff00000000LL) )
		clen = printf("%s0x%10.10qx      ", s, value);
	else
		clen = printf("%s0x%8.8qx        ", s, value);

	return clen;
}

/*
 * ret = 1 means print the entry
 * ret = 0 means don't print the entry
 */

/*
 * meaning of filter flags:
 * cachehit	turn on display of CACHE_HIT events (which are filtered out by default)
 *
 * exec		show exec/posix_spawn
 * pathname	show events with a pathname and close()
 * diskio	show disk I/Os
 * filesys	show filesystem events
 * network	show network events
 *
 * filters may be combined; default is all filters on (except cachehit)
 */
bool
check_filter_mode(pid_t pid, th_info_t ti, uint64_t type, int error, int retval, char *sc_name)
{
	bool ret = false;
	int network_fd_isset = 0;
	uint64_t fd;

	/* cachehit is special -- it's not on by default */
	if (sc_name[0] == 'C' && !strcmp(sc_name, "CACHE_HIT")) {
		return show_cachehits;
	}

	if (filter_mode == DEFAULT_DO_NOT_FILTER)
		return true;

	if (filter_mode & DISKIO_FILTER) {
		if ((type & P_DISKIO_MASK) == P_DISKIO)
			return true;

		if (type == Throttled)
			return true;
	}

	if (filter_mode & EXEC_FILTER) {
		if (type == BSC_execve || type == BSC_posix_spawn)
			return true;
	}

	if (filter_mode & PATHNAME_FILTER) {
		if (ti && ti->pathname[0])
			return true;

		if (type == BSC_close || type == BSC_close_nocancel ||
			type == BSC_guarded_close_np)
			return true;
	}

	if (!ti) {
		if (filter_mode & FILESYS_FILTER)
			return true;

		return 0;
	}

	switch (type) {
		case BSC_close:
		case BSC_close_nocancel:
		case BSC_guarded_close_np:
			fd = ti->arg1;
			network_fd_isset = fd_is_network(pid, fd);

			if (error == 0)
				fd_set_is_network(pid, fd, false);

			if (network_fd_isset) {
				if (filter_mode & NETWORK_FILTER)
					ret = true;
			} else {
				if (filter_mode & FILESYS_FILTER)
					ret = true;
			}

			break;

		case BSC_read:
		case BSC_write:
		case BSC_read_nocancel:
		case BSC_write_nocancel:
			/*
			 * we don't care about error in these cases
			 */
			fd = ti->arg1;

			if (fd_is_network(pid, fd)) {
				if (filter_mode & NETWORK_FILTER)
					ret = true;
			} else if (filter_mode & FILESYS_FILTER) {
				ret = true;
			}

			break;

		case BSC_accept:
		case BSC_accept_nocancel:
		case BSC_socket:
			fd = retval;

			if (error == 0)
				fd_set_is_network(pid, fd, true);

			if (filter_mode & NETWORK_FILTER)
				ret = true;

			break;

		case BSC_recvfrom:
		case BSC_sendto:
		case BSC_recvmsg:
		case BSC_sendmsg:
		case BSC_connect:
		case BSC_bind:
		case BSC_listen:
		case BSC_sendto_nocancel:
		case BSC_recvfrom_nocancel:
		case BSC_recvmsg_nocancel:
		case BSC_sendmsg_nocancel:
		case BSC_connect_nocancel:
			fd = ti->arg1;

			if (error == 0)
				fd_set_is_network(pid, fd, true);

			if (filter_mode & NETWORK_FILTER)
				ret = true;

			break;

		case BSC_select:
		case BSC_select_nocancel:
		case BSC_socketpair:
			/*
			 * Cannot determine info about file descriptors
			 */
			if (filter_mode & NETWORK_FILTER)
				ret = true;

			break;

		case BSC_dup:
		case BSC_dup2:
			/*
			 * We track these cases for fd state only
			 */
			fd = ti->arg1;

			if (error == 0 && fd_is_network(pid, fd)) {
				/*
				 * then we are duping a socket descriptor
				 */
				fd = retval;  /* the new fd */
				fd_set_is_network(pid, fd, true);
			}

			break;

		default:
			if (filter_mode & FILESYS_FILTER)
				ret = true;

			break;
	}

	return ret;
}

int
print_open(ktrace_event_t event, uint64_t flags)
{
	char mode[] = "______";

	if (flags & O_RDWR) {
		mode[0] = 'R';
		mode[1] = 'W';
	} else if (flags & O_WRONLY) {
		mode[1] = 'W';
	} else {
		mode[0] = 'R';
	}

	if (flags & O_CREAT) {
		mode[2] = 'C';
	}
	if (flags & O_APPEND) {
		mode[3] = 'A';
	}
	if (flags & O_TRUNC) {
		mode[4] = 'T';
	}
	if (flags & O_EXCL) {
		mode[5] = 'E';
	}

	if (event->arg1) {
		return printf("      [%3d] (%s) ", (int)event->arg1, mode);
	} else {
		return printf(" F=%-3d      (%s) ", (int)event->arg2, mode);
	}
}

/*
 * called from:
 *
 * exit_event() (syscalls etc.)
 * print_diskio() (disk I/Os)
 * block callback for TrimExtent
 */
void
format_print(th_info_t ti, char *sc_name, ktrace_event_t event,
	     uint64_t type, int format, uint64_t now, uint64_t stime,
	     int waited, const char *pathname, struct diskio *dio)
{
	uint64_t secs, usecs;
	int nopadding = 0;
	static time_t last_walltime_secs = -1;
	const char *command_name;
	pid_t pid;
	int len = 0;
	int clen = 0;
	size_t tlen = 0;
	uint64_t class;
	uint64_t user_addr;
	uint64_t user_size;
	char *framework_name;
	char *framework_type;
	char *p1;
	char *p2;
	char buf[2 * PATH_MAX + 64];
	char cs_diskname[32];
	uint64_t threadid;
	struct timeval now_walltime;

	static char timestamp[32];
	static size_t timestamp_len = 0;

	if (!mach_time_of_first_event)
		mach_time_of_first_event = now;

	if (format == FMT_DISKIO || format == FMT_DISKIO_CS) {
		assert(dio);
	} else {
		assert(event);

		if (format != FMT_UNMAP_INFO)
			assert(ti);
	}

	/* <rdar://problem/19852325> Filter out WindowServer/xcpm ioctls in fs_usage */
	if (type == BSC_ioctl && ti->arg2 == 0xffffffffc030581dUL)
		return;

	/* honor -S and -E */
	if (RAW_flag) {
		uint64_t relative_time_ns;

		relative_time_ns = mach_to_nano(now - mach_time_of_first_event);

		if (relative_time_ns < start_time_ns || relative_time_ns > end_time_ns)
			return;
	}

	class = KDBG_EXTRACT_CLASS(type);

	if (dio) {
		command_name = dio->issuing_command;
		threadid = dio->issuing_thread;
		pid = dio->issuing_pid;
		now_walltime = dio->completed_walltime;
	} else {
		if (ti && ti->command[0] != '\0') {
			command_name = ti->command;
			threadid = ti->thread;
			pid = ti->pid;
		} else {
			command_name = ktrace_get_execname_for_thread(s, event->threadid);
			threadid = event->threadid;
			pid = ktrace_get_pid_for_thread(s, event->threadid);
		}

		now_walltime = event->walltime;
	}

	if (!want_kernel_task && pid == 0)
		return;

	if (!command_name)
		command_name = "";

	assert(now_walltime.tv_sec || now_walltime.tv_usec);

	/* try and reuse the timestamp string */
	if (last_walltime_secs != now_walltime.tv_sec) {
		timestamp_len = strftime(timestamp, sizeof (timestamp), "%H:%M:%S", localtime(&now_walltime.tv_sec));
		last_walltime_secs = now_walltime.tv_sec;
	}

	if (columns > MAXCOLS || wideflag) {
		tlen = timestamp_len;
		nopadding = 0;

		sprintf(&timestamp[tlen], ".%06d", now_walltime.tv_usec);
		tlen += 7;

		timestamp[tlen] = '\0';
	} else {
		nopadding = 1;
	}

	clen = printf("%s  %-17.17s", timestamp, sc_name);

	framework_name = NULL;

	if (columns > MAXCOLS || wideflag) {
		off_t offset_reassembled = 0LL;

		switch (format) {
			case FMT_NOTHING:
				clen += printf("                  ");
				break;

			case FMT_AT:
			case FMT_RENAMEAT:
			case FMT_DEFAULT:
				/*
				 * pathname based system calls or
				 * calls with no fd or pathname (i.e.  sync)
				 */
				if (event->arg1)
					clen += printf("      [%3d]       ", (int)event->arg1);
				else
					clen += printf("                  ");

				break;

			case FMT_FD:
				/*
				 * fd based system call... no I/O
				 */
				if (event->arg1)
					clen += printf(" F=%-3d[%3d]", (int)ti->arg1, (int)event->arg1);
				else
					clen += printf(" F=%-3d", (int)ti->arg1);

				break;

			case FMT_FD_2:
				/*
				 * accept, dup, dup2
				 */
				if (event->arg1)
					clen += printf(" F=%-3d[%3d]", (int)ti->arg1, (int)event->arg1);
				else
					clen += printf(" F=%-3d  F=%-3d", (int)ti->arg1, (int)event->arg2);

				break;

			case FMT_FD_IO:
				/*
				 * system calls with fd's that return an I/O completion count
				 */
				if (event->arg1)
					clen += printf(" F=%-3d[%3d]", (int)ti->arg1, (int)event->arg1);
				else
					clen += printf(" F=%-3d  B=0x%-6" PRIx64, (int)ti->arg1, (uint64_t)event->arg2);

				break;

			case FMT_PGIN:
				/*
				 * pagein
				 */
				user_addr = ((uint64_t)event->arg1 << 32) | (uint32_t)event->arg2;

				lookup_name(user_addr, &framework_type, &framework_name);
				clen += clip_64bit(" A=", user_addr);
				break;

			case FMT_CACHEHIT:
				/*
				 * cache hit
				 */
				user_addr = ((uint64_t)event->arg1 << 32) | (uint32_t)event->arg2;

				lookup_name(user_addr, &framework_type, &framework_name);
				clen += clip_64bit(" A=", user_addr);
				break;

			case FMT_PGOUT:
				/*
				 * pageout
				 */
				clen += printf("      B=0x%-8" PRIx64, (uint64_t)event->arg1);
				break;

			case FMT_HFS_update:
			{
				static const struct {
					int flag;
					char ch;
				} hfsflags[] = {
					{ DBG_HFS_UPDATE_SKIPPED,   'S' },
					{ DBG_HFS_UPDATE_FORCE,     'F' },
					{ DBG_HFS_UPDATE_MODIFIED,  'M' },
					{ DBG_HFS_UPDATE_MINOR,     'N' },
					{ DBG_HFS_UPDATE_DATEADDED, 'd' },
					{ DBG_HFS_UPDATE_CHGTIME,   'c' },
					{ DBG_HFS_UPDATE_ACCTIME,   'a' },
					{ DBG_HFS_UPDATE_MODTIME,   'm' },
				};
				size_t i;
				int flagcount;
				char *sbuf;
				int  sflag = (int)event->arg2;

				flagcount = sizeof (hfsflags) / sizeof (*hfsflags);
				sbuf = malloc(flagcount + 1);

				for (i = 0; i < flagcount; i++) {
					if (sflag & hfsflags[i].flag) {
						sbuf[i] = hfsflags[i].ch;
					} else {
						sbuf[i] = '_';
					}
				}

				sbuf[flagcount] = '\0';

				clen += printf(" %*s(%s) ", 17 - flagcount, "", sbuf);

				free(sbuf);

				pathname = ktrace_get_path_for_vp(s, event->arg1);

				if (!pathname)
					pathname = "";

				nopadding = 1;

				break;
			}

			case FMT_DISKIO:
				/*
				 * physical disk I/O
				 */
				if (dio->io_errno) {
					clen += printf(" D=0x%8.8" PRIx64 "  [%3d]", dio->blkno, (int)dio->io_errno);
				} else {
					if (BC_flag)
						clen += printf(" D=0x%8.8" PRIx64 "  B=0x%-6" PRIx64 " BC:%s /dev/%s ", dio->blkno, dio->iosize, BC_STR(dio->bc_info), find_disk_name(dio->dev));
					else
						clen += printf(" D=0x%8.8" PRIx64 "  B=0x%-6" PRIx64 " /dev/%s ", dio->blkno, dio->iosize, find_disk_name(dio->dev));

					if (dio->is_meta) {
						if (!(type & P_DISKIO_READ)) {
							pathname = meta_find_name(dio->blkno);
						}
					} else {
						pathname = ktrace_get_path_for_vp(s, dio->vnodeid);

						if (!pathname)
							pathname = "";
					}

					nopadding = 1;
				}

				break;

			case FMT_DISKIO_CS:
				/*
				 * physical disk I/O
				 */
				if (dio->io_errno)
					clen += printf(" D=0x%8.8" PRIx64 "  [%3" PRIu64 "]", dio->blkno, dio->io_errno);
				else
					clen += printf(" D=0x%8.8" PRIx64 "  B=0x%-6" PRIx64 " /dev/%s", dio->blkno, dio->iosize, generate_cs_disk_name(dio->dev, cs_diskname));

				break;

			case FMT_SYNC_DISK_CS:
				/*
				 * physical disk sync cache
				 */
				clen += printf("                          /dev/%s", generate_cs_disk_name(event->arg1, cs_diskname));

				break;

			case FMT_MSYNC:
			{
				/*
				 * msync
				 */
				int  mlen = 0;

				buf[0] = '\0';

				if (ti->arg3 & MS_ASYNC)
					mlen += sprintf(&buf[mlen], "MS_ASYNC | ");
				else
					mlen += sprintf(&buf[mlen], "MS_SYNC | ");

				if (ti->arg3 & MS_INVALIDATE)
					mlen += sprintf(&buf[mlen], "MS_INVALIDATE | ");
				if (ti->arg3 & MS_KILLPAGES)
					mlen += sprintf(&buf[mlen], "MS_KILLPAGES | ");
				if (ti->arg3 & MS_DEACTIVATE)
					mlen += sprintf(&buf[mlen], "MS_DEACTIVATE | ");

				if (ti->arg3 & ~(MS_ASYNC | MS_SYNC | MS_INVALIDATE | MS_KILLPAGES | MS_DEACTIVATE))
					mlen += sprintf(&buf[mlen], "UNKNOWN | ");

				if (mlen)
					buf[mlen - 3] = '\0';

				if (event->arg1)
					clen += printf("      [%3d]", (int)event->arg1);

				user_addr = (((off_t)(unsigned int)(ti->arg4)) << 32) | (unsigned int)(ti->arg1);
				clen += clip_64bit(" A=", user_addr);

				user_size = (((off_t)(unsigned int)(ti->arg5)) << 32) | (unsigned int)(ti->arg2);

				clen += printf("  B=0x%-16qx  <%s>", user_size, buf);

				break;
			}

			case FMT_FLOCK:
			{
				/*
				 * flock
				 */
				int  mlen = 0;

				buf[0] = '\0';

				if (ti->arg2 & LOCK_SH)
					mlen += sprintf(&buf[mlen], "LOCK_SH | ");
				if (ti->arg2 & LOCK_EX)
					mlen += sprintf(&buf[mlen], "LOCK_EX | ");
				if (ti->arg2 & LOCK_NB)
					mlen += sprintf(&buf[mlen], "LOCK_NB | ");
				if (ti->arg2 & LOCK_UN)
					mlen += sprintf(&buf[mlen], "LOCK_UN | ");

				if (ti->arg2 & ~(LOCK_SH | LOCK_EX | LOCK_NB | LOCK_UN))
					mlen += sprintf(&buf[mlen], "UNKNOWN | ");

				if (mlen)
					buf[mlen - 3] = '\0';

				if (event->arg1)
					clen += printf(" F=%-3d[%3d]  <%s>", (int)ti->arg1, (int)event->arg1, buf);
				else
					clen += printf(" F=%-3d  <%s>", (int)ti->arg1, buf);

				break;
			}

			case FMT_FCNTL:
			{
				/*
				 * fcntl
				 */
				char *p = NULL;
				int fd = -1;

				if (event->arg1)
					clen += printf(" F=%-3d[%3d]", (int)ti->arg1, (int)event->arg1);
				else
					clen += printf(" F=%-3d", (int)ti->arg1);

				switch(ti->arg2) {
					case F_DUPFD:
						p = "DUPFD";
						break;

					case F_GETFD:
						p = "GETFD";
						break;

					case F_SETFD:
						p = "SETFD";
						break;

					case F_GETFL:
						p = "GETFL";
						break;

					case F_SETFL:
						p = "SETFL";
						break;

					case F_GETOWN:
						p = "GETOWN";
						break;

					case F_SETOWN:
						p = "SETOWN";
						break;

					case F_GETLK:
						p = "GETLK";
						break;

					case F_SETLK:
						p = "SETLK";
						break;

					case F_SETLKW:
						p = "SETLKW";
						break;

					case F_PREALLOCATE:
						p = "PREALLOCATE";
						break;

					case F_SETSIZE:
						p = "SETSIZE";
						break;

					case F_RDADVISE:
						p = "RDADVISE";
						break;

					case F_GETPATH:
						p = "GETPATH";
						break;

					case F_FULLFSYNC:
						p = "FULLFSYNC";
						break;

					case F_PATHPKG_CHECK:
						p = "PATHPKG_CHECK";
						break;

					case F_OPENFROM:
						p = "OPENFROM";

						if (event->arg1 == 0)
							fd = (int)event->arg2;
						break;

					case F_UNLINKFROM:
						p = "UNLINKFROM";
						break;

					case F_CHECK_OPENEVT:
						p = "CHECK_OPENEVT";
						break;

					case F_NOCACHE:
						if (ti->arg3)
							p = "CACHING OFF";
						else
							p = "CACHING ON";
						break;

					case F_GLOBAL_NOCACHE:
						if (ti->arg3)
							p = "CACHING OFF (GLOBAL)";
						else
							p = "CACHING ON (GLOBAL)";
						break;

				}

				if (p) {
					if (fd == -1)
						clen += printf(" <%s>", p);
					else
						clen += printf(" <%s> F=%d", p, fd);
				} else {
					clen += printf(" <CMD=%d>", (int)ti->arg2);
				}

				break;
			}

			case FMT_IOCTL:
			{
				/*
				 * ioctl
				 */
				if (event->arg1)
					clen += printf(" F=%-3d[%3d]", (int)ti->arg1, (int)event->arg1);
				else
					clen += printf(" F=%-3d", (int)ti->arg1);

				clen += printf(" <CMD=0x%x>", (int)ti->arg2);

				break;
			}

			case FMT_IOCTL_SYNC:
			{
				/*
				 * ioctl
				 */
				clen += printf(" <DKIOCSYNCHRONIZE>  B=%" PRIu64 " /dev/%s", (uint64_t)event->arg3, find_disk_name(event->arg1));

				break;
			}

			case FMT_IOCTL_SYNCCACHE:
			{
				/*
				 * ioctl
				 */
				clen += printf(" <DKIOCSYNCHRONIZECACHE>  /dev/%s", find_disk_name(event->arg1));

				break;
			}

			case FMT_IOCTL_UNMAP:
			{
				/*
				 * ioctl
				 */
				clen += printf(" <DKIOCUNMAP>             /dev/%s", find_disk_name(event->arg1));

				break;
			}

			case FMT_UNMAP_INFO:
			{
				clen += printf(" D=0x%8.8" PRIx64 "  B=0x%-6" PRIx64 " /dev/%s", (uint64_t)event->arg2, (uint64_t)event->arg3, find_disk_name(event->arg1));

				break;
			}

			case FMT_SELECT:
				/*
				 * select
				 */
				if (event->arg1)
					clen += printf("      [%3d]", (int)event->arg1);
				else
					clen += printf("        S=%-3d", (int)event->arg2);

				break;

			case FMT_LSEEK:
			case FMT_PREAD:
				/*
				 * pread, pwrite, lseek
				 */
				clen += printf(" F=%-3d", (int)ti->arg1);

				if (event->arg1) {
					clen += printf("[%3d]  ", (int)event->arg1);
				} else {
					if (format == FMT_PREAD)
						clen += printf("  B=0x%-8" PRIx64 " ", (uint64_t)event->arg2);
					else
						clen += printf("  ");
				}

				if (format == FMT_PREAD)
					offset_reassembled = (((off_t)(unsigned int)(ti->arg3)) << 32) | (unsigned int)(ti->arg4);
				else
#ifdef __ppc__
					offset_reassembled = (((off_t)(unsigned int)(arg2)) << 32) | (unsigned int)(arg3);
#else
					offset_reassembled = (((off_t)(unsigned int)(event->arg3)) << 32) | (unsigned int)(event->arg2);
#endif

				clen += clip_64bit("O=", offset_reassembled);

				if (format == FMT_LSEEK) {
					char *mode;

					if (ti->arg3 == SEEK_SET)
						mode = "SEEK_SET";
					else if (ti->arg3 == SEEK_CUR)
						mode = "SEEK_CUR";
					else if (ti->arg3 == SEEK_END)
						mode = "SEEK_END";
					else
						mode = "UNKNOWN";

					clen += printf(" <%s>", mode);
				}

				break;

			case FMT_MMAP:
				/*
				 * mmap
				 */
				clen += printf(" F=%-3d  ", (int)ti->arg1);

				if (event->arg1) {
					clen += printf("[%3d]  ", (int)event->arg1);
				} else {
					user_addr = (((off_t)(unsigned int)(ti->arg2)) << 32) | (unsigned int)(ti->arg3);

					clen += clip_64bit("A=", user_addr);

					offset_reassembled = (((off_t)(unsigned int)(ti->arg6)) << 32) | (unsigned int)(ti->arg7);

					clen += clip_64bit("O=", offset_reassembled);

					user_size = (((off_t)(unsigned int)(ti->arg4)) << 32) | (unsigned int)(ti->arg5);

					clen += printf("B=0x%-16qx", user_size);

					clen += printf(" <");

					if (ti->arg8 & PROT_READ)
						clen += printf("READ");

					if (ti->arg8 & PROT_WRITE)
						clen += printf("|WRITE");

					if (ti->arg8 & PROT_EXEC)
						clen += printf("|EXEC");

					clen += printf(">");
				}

				break;

			case FMT_TRUNC:
			case FMT_FTRUNC:
				/*
				 * ftruncate, truncate
				 */
				if (format == FMT_FTRUNC)
					clen += printf(" F=%-3d", (int)ti->arg1);
				else
					clen += printf("      ");

				if (event->arg1)
					clen += printf("[%3d]", (int)event->arg1);

#ifdef __ppc__
				offset_reassembled = (((off_t)(unsigned int)(ti->arg2)) << 32) | (unsigned int)(ti->arg3);
#else
				offset_reassembled = (((off_t)(unsigned int)(ti->arg3)) << 32) | (unsigned int)(ti->arg2);
#endif
				clen += clip_64bit("  O=", offset_reassembled);

				nopadding = 1;
				break;

			case FMT_FCHFLAGS:
			case FMT_CHFLAGS:
			{
				/*
				 * fchflags, chflags
				 */
				int mlen = 0;

				if (format == FMT_FCHFLAGS) {
					if (event->arg1)
						clen += printf(" F=%-3d[%3d]", (int)ti->arg1, (int)event->arg1);
					else
						clen += printf(" F=%-3d", (int)ti->arg1);
				} else {
					if (event->arg1)
						clen += printf(" [%3d] ", (int)event->arg1);
				}

				buf[mlen++] = ' ';
				buf[mlen++] = '<';

				if (ti->arg2 & UF_NODUMP)
					mlen += sprintf(&buf[mlen], "UF_NODUMP | ");
				if (ti->arg2 & UF_IMMUTABLE)
					mlen += sprintf(&buf[mlen], "UF_IMMUTABLE | ");
				if (ti->arg2 & UF_APPEND)
					mlen += sprintf(&buf[mlen], "UF_APPEND | ");
				if (ti->arg2 & UF_OPAQUE)
					mlen += sprintf(&buf[mlen], "UF_OPAQUE | ");
				if (ti->arg2 & SF_ARCHIVED)
					mlen += sprintf(&buf[mlen], "SF_ARCHIVED | ");
				if (ti->arg2 & SF_IMMUTABLE)
					mlen += sprintf(&buf[mlen], "SF_IMMUTABLE | ");
				if (ti->arg2 & SF_APPEND)
					mlen += sprintf(&buf[mlen], "SF_APPEND | ");

				if (ti->arg2 == 0)
					mlen += sprintf(&buf[mlen], "CLEAR_ALL_FLAGS | ");
				else if (ti->arg2 & ~(UF_NODUMP | UF_IMMUTABLE | UF_APPEND | SF_ARCHIVED | SF_IMMUTABLE | SF_APPEND))
					mlen += sprintf(&buf[mlen], "UNKNOWN | ");

				if (mlen >= 3)
					mlen -= 3;

				buf[mlen++] = '>';
				buf[mlen] = '\0';

				if (mlen < 19) {
					memset(&buf[mlen], ' ', 19 - mlen);
					mlen = 19;
					buf[mlen] = '\0';
				}

				clen += printf("%s", buf);

				nopadding = 1;
				break;
			}

			case FMT_UMASK:
			case FMT_FCHMOD:
			case FMT_FCHMOD_EXT:
			case FMT_CHMOD:
			case FMT_CHMOD_EXT:
			case FMT_CHMODAT:
			{
				/*
				 * fchmod, fchmod_extended, chmod, chmod_extended
				 */
				uint64_t mode;

				if (format == FMT_FCHMOD || format == FMT_FCHMOD_EXT) {
					if (event->arg1)
						clen += printf(" F=%-3d[%3d] ", (int)ti->arg1, (int)event->arg1);
					else
						clen += printf(" F=%-3d ", (int)ti->arg1);
				} else {
					if (event->arg1)
						clen += printf(" [%3d] ", (int)event->arg1);
					else
						clen += printf(" ");
				}

				if (format == FMT_UMASK)
					mode = ti->arg1;
				else if (format == FMT_FCHMOD || format == FMT_CHMOD || format == FMT_CHMODAT)
					mode = ti->arg2;
				else
					mode = ti->arg4;

				get_mode_string(mode, &buf[0]);

				if (event->arg1 == 0)
					clen += printf("<%s>      ", buf);
				else
					clen += printf("<%s>", buf);
				break;
			}

			case FMT_ACCESS:
			{
				/*
				 * access
				 */
				char mode[5];

				memset(mode, '_', 4);
				mode[4] = '\0';

				if (ti->arg2 & R_OK)
					mode[0] = 'R';
				if (ti->arg2 & W_OK)
					mode[1] = 'W';
				if (ti->arg2 & X_OK)
					mode[2] = 'X';
				if (ti->arg2 == F_OK)
					mode[3] = 'F';

				if (event->arg1)
					clen += printf("      [%3d] (%s)   ", (int)event->arg1, mode);
				else
					clen += printf("            (%s)   ", mode);

				nopadding = 1;
				break;
			}

			case FMT_MOUNT:
			{
				if (event->arg1)
					clen += printf("      [%3d] <FLGS=0x%" PRIx64 "> ", (int)event->arg1, ti->arg3);
				else
					clen += printf("     <FLGS=0x%" PRIx64 "> ", ti->arg3);

				nopadding = 1;
				break;
			}

			case FMT_UNMOUNT:
			{
				char *mountflag;

				if (ti->arg2 & MNT_FORCE)
					mountflag = "<FORCE>";
				else
					mountflag = "";

				if (event->arg1)
					clen += printf("      [%3d] %s  ", (int)event->arg1, mountflag);
				else
					clen += printf("     %s         ", mountflag);

				nopadding = 1;
				break;
			}

			case FMT_OPEN:
				clen += print_open(event, ti->arg2);
				nopadding = 1;
				break;

			case FMT_OPENAT:
				clen += print_open(event, ti->arg3);
				nopadding = 1;
				break;

			case FMT_GUARDED_OPEN:
				clen += print_open(event, ti->arg4);
				nopadding = 1;
				break;

			case FMT_SOCKET:
			{
				/*
				 * socket
				 *
				 */
				char *domain;
				char *type;

				switch (ti->arg1) {
					case AF_UNIX:
						domain = "AF_UNIX";
						break;

					case AF_INET:
						domain = "AF_INET";
						break;

					case AF_ISO:
						domain = "AF_ISO";
						break;

					case AF_NS:
						domain = "AF_NS";
						break;

					case AF_IMPLINK:
						domain = "AF_IMPLINK";
						break;

					default:
						domain = "UNKNOWN";
						break;
				}

				switch (ti->arg2) {
					case SOCK_STREAM:
						type = "SOCK_STREAM";
						break;
					case SOCK_DGRAM:
						type = "SOCK_DGRAM";
						break;
					case SOCK_RAW:
						type = "SOCK_RAW";
						break;
					default:
						type = "UNKNOWN";
						break;
				}

				if (event->arg1)
					clen += printf("      [%3d] <%s, %s, 0x%" PRIx64 ">", (int)event->arg1, domain, type, ti->arg3);
				else
					clen += printf(" F=%-3d      <%s, %s, 0x%" PRIx64 ">", (int)event->arg2, domain, type, ti->arg3);

				break;
			}

			case FMT_AIO_FSYNC:
			{
				/*
				 * aio_fsync		[errno]   AIOCBP   OP
				 */
				char *op;

				if (ti->arg1 == O_SYNC || ti->arg1 == 0)
					op = "AIO_FSYNC";
#if O_DSYNC
				else if (ti->arg1 == O_DSYNC)
					op = "AIO_DSYNC";
#endif
				else
					op = "UNKNOWN";

				if (event->arg1)
					clen += printf("      [%3d] P=0x%8.8" PRIx64 "  <%s>", (int)event->arg1, ti->arg2, op);
				else
					clen += printf("            P=0x%8.8" PRIx64 "  <%s>", ti->arg2, op);

				break;
			}

			case FMT_AIO_RETURN:
				/*
				 * aio_return		[errno]   AIOCBP   IOSIZE
				 */
				if (event->arg1)
					clen += printf("      [%3d] P=0x%8.8" PRIx64, (int)event->arg1, ti->arg1);
				else
					clen += printf("            P=0x%8.8" PRIx64 "  B=0x%-8" PRIx64, ti->arg1, (uint64_t)event->arg2);

				break;

			case FMT_AIO_SUSPEND:
				/*
				 * aio_suspend		[errno]   NENTS
				 */
				if (event->arg1)
					clen += printf("      [%3d] N=%d", (int)event->arg1, (int)ti->arg2);
				else
					clen += printf("            N=%d", (int)ti->arg2);

				break;

			case FMT_AIO_CANCEL:
				/*
				 * aio_cancel	  	[errno]   FD or AIOCBP (if non-null)
				 */
				if (ti->arg2) {
					if (event->arg1)
						clen += printf("      [%3d] P=0x%8." PRIx64, (int)event->arg1, ti->arg2);
					else
						clen += printf("            P=0x%8.8" PRIx64, ti->arg2);
				} else {
					if (event->arg1)
						clen += printf(" F=%-3d[%3d]", (int)ti->arg1, (int)event->arg1);
					else
						clen += printf(" F=%-3d", (int)ti->arg1);
				}

				break;

			case FMT_AIO:
				/*
				 * aio_error, aio_read, aio_write	[errno]  AIOCBP
				 */
				if (event->arg1)
					clen += printf("      [%3d] P=0x%8.8" PRIx64, (int)event->arg1, ti->arg1);
				else
					clen += printf("            P=0x%8.8" PRIx64, ti->arg1);

				break;

			case FMT_LIO_LISTIO: {
				/*
				 * lio_listio		[errno]   NENTS  MODE
				 */
				char *op;

				if (ti->arg1 == LIO_NOWAIT)
					op = "LIO_NOWAIT";
				else if (ti->arg1 == LIO_WAIT)
					op = "LIO_WAIT";
				else
					op = "UNKNOWN";

				if (event->arg1)
					clen += printf("      [%3d] N=%d  <%s>", (int)event->arg1, (int)ti->arg3, op);
				else
					clen += printf("            N=%d  <%s>", (int)ti->arg3, op);

				break;
			}
		}
	}

	/*
	 * Calculate space available to print pathname
	 */
	if (columns > MAXCOLS || wideflag)
		clen = columns - (clen + 14 + 20 + 11);
	else
		clen = columns - (clen + 14 + 12);

	if (!nopadding)
		clen -= 3;

	if (framework_name) {
		len = sprintf(&buf[0], " %s %s ", framework_type, framework_name);
	} else if (*pathname != '\0') {
		switch(format) {
			case FMT_AT:
			case FMT_OPENAT:
			case FMT_CHMODAT:
				len = sprintf(&buf[0], " [%d]/%s ", (int)ti->arg1, pathname);
				break;
			case FMT_RENAMEAT:
				len = sprintf(&buf[0], " [%d]/%s ", (int)ti->arg3, pathname);
				break;
			default:
				len = sprintf(&buf[0], " %s ", pathname);
		}

		if (format == FMT_MOUNT && ti->pathname2[0] != '\0') {
			int	len2;

			memset(&buf[len], ' ', 2);

			len2 = sprintf(&buf[len+2], " %s ", ti->pathname2);
			len = len + 2 + len2;
		}
	} else {
		len = 0;
	}

	if (clen > len) {
		/*
		 * Add null padding if column length
		 * is wider than the pathname length.
		 */
		memset(&buf[len], ' ', clen - len);
		buf[clen] = '\0';

		pathname = buf;
	} else if (clen == len) {
		pathname = buf;
	} else if ((clen > 0) && (clen < len)) {
		/*
		 * This prints the tail end of the pathname
		 */
		buf[len-clen] = ' ';

		pathname = &buf[len - clen];
	} else {
		pathname = "";
	}

	/*
	 * fudge some additional system call overhead
	 * that currently isn't tracked... this also
	 * insures that we see a minimum of 1 us for
	 * an elapsed time
	 */
	usecs = (mach_to_nano(now - stime) + (NSEC_PER_USEC - 1)) / NSEC_PER_USEC;
	secs = usecs / USEC_PER_SEC;
	usecs -= secs * USEC_PER_SEC;

	if (!nopadding)
		p1 = "   ";
	else
		p1 = "";

	if (waited)
		p2 = " W";
	else
		p2 = "  ";

	if (columns > MAXCOLS || wideflag)
		printf("%s%s %3llu.%06llu%s %s.%" PRIu64 "\n", p1, pathname, secs, usecs, p2, command_name, threadid);
	else
		printf("%s%s %3llu.%06llu%s %-12.12s\n", p1, pathname, secs, usecs, p2, command_name);

	if (!RAW_flag)
		fflush(stdout);
}

#pragma mark metadata info hash routines

#define VN_HASH_SIZE	16384
#define VN_HASH_MASK	(VN_HASH_SIZE - 1)

typedef struct meta_info {
	struct meta_info *m_next;
	uint64_t m_blkno;
	char m_name[MAXPATHLEN];
} *meta_info_t;

meta_info_t m_info_hash[VN_HASH_SIZE];

void
meta_add_name(uint64_t blockno, const char *pathname)
{
	meta_info_t	mi;
	int hashid;

	hashid = blockno & VN_HASH_MASK;

	for (mi = m_info_hash[hashid]; mi; mi = mi->m_next) {
		if (mi->m_blkno == blockno)
			break;
	}

	if (mi == NULL) {
		mi = malloc(sizeof (struct meta_info));

		mi->m_next = m_info_hash[hashid];
		m_info_hash[hashid] = mi;
		mi->m_blkno = blockno;
	}

	strncpy(mi->m_name, pathname, sizeof (mi->m_name));
}

const char *
meta_find_name(uint64_t blockno)
{
	meta_info_t	mi;
	int		hashid;

	hashid = blockno & VN_HASH_MASK;

	for (mi = m_info_hash[hashid]; mi; mi = mi->m_next) {
		if (mi->m_blkno == blockno)
			return mi->m_name;
	}

	return "";
}

void
meta_delete_all(void)
{
	meta_info_t mi, next;
	int i;

	for (i = 0; i < HASH_MASK; i++) {
		for (mi = m_info_hash[i]; mi; mi = next) {
			next = mi->m_next;

			free(mi);
		}

		m_info_hash[i] = NULL;
	}
}

#pragma mark event ("thread info") routines

th_info_t th_info_hash[HASH_SIZE];
th_info_t th_info_freelist;

static th_info_t
add_event(ktrace_event_t event, int type)
{
	th_info_t ti;
	int hashid;
	uint64_t eventid;

	if ((ti = th_info_freelist))
		th_info_freelist = ti->next;
	else
		ti = malloc(sizeof (struct th_info));

	bzero(ti, sizeof (struct th_info));

	hashid = event->threadid & HASH_MASK;

	ti->next = th_info_hash[hashid];
	th_info_hash[hashid] = ti;

	eventid = event->debugid & KDBG_EVENTID_MASK;

	if (eventid == BSC_execve || eventid == BSC_posix_spawn) {
		const char *command;

		command = ktrace_get_execname_for_thread(s, event->threadid);

		if (!command)
			command = "";

		strncpy(ti->command, command, sizeof (ti->command));
		ti->command[MAXCOMLEN] = '\0';
	}

	ti->thread = event->threadid;
	ti->type = type;

	return ti;
}

th_info_t
event_find(uint64_t thread, int type)
{
	th_info_t ti;
	int hashid;

	hashid = thread & HASH_MASK;

	for (ti = th_info_hash[hashid]; ti; ti = ti->next) {
		if (ti->thread == thread) {
			if (type == ti->type)
				return ti;

			if (type == 0)
				return ti;
		}
	}

	return NULL;
}

void
event_delete(th_info_t ti_to_delete)
{
	th_info_t	ti;
	th_info_t	ti_prev;
	int		hashid;

	hashid = ti_to_delete->thread & HASH_MASK;

	if ((ti = th_info_hash[hashid])) {
		if (ti == ti_to_delete)
			th_info_hash[hashid] = ti->next;
		else {
			ti_prev = ti;

			for (ti = ti->next; ti; ti = ti->next) {
				if (ti == ti_to_delete) {
					ti_prev->next = ti->next;
					break;
				}
				ti_prev = ti;
			}
		}
		if (ti) {
			ti->next = th_info_freelist;
			th_info_freelist = ti;
		}
	}
}

void
event_delete_all(void)
{
	th_info_t	ti = 0;
	th_info_t	ti_next = 0;
	int             i;

	for (i = 0; i < HASH_SIZE; i++) {

		for (ti = th_info_hash[i]; ti; ti = ti_next) {
			ti_next = ti->next;
			ti->next = th_info_freelist;
			th_info_freelist = ti;
		}
		th_info_hash[i] = 0;
	}
}

void
event_enter(int type, ktrace_event_t event)
{
	th_info_t ti;

#if DEBUG
	int index;
	bool found;

	found = false;

	switch (type) {
		case P_CS_SYNC_DISK:
		case MACH_pageout:
		case MACH_vmfault:
		case MSC_map_fd:
		case SPEC_ioctl:
		case Throttled:
		case HFS_update:
			found = true;
	}

	if ((type & CSC_MASK) == BSC_BASE) {
		if ((index = BSC_INDEX(type)) < MAX_BSD_SYSCALL && bsd_syscalls[index].sc_name)
			found = true;
	}

	assert(found);
#endif /* DEBUG */

	if ((ti = add_event(event, type)) == NULL)
		return;

	ti->stime  = event->timestamp;
	ti->arg1   = event->arg1;
	ti->arg2   = event->arg2;
	ti->arg3   = event->arg3;
	ti->arg4   = event->arg4;
}

void
event_exit(char *sc_name, int type, ktrace_event_t event, int format)
{
	th_info_t ti;
	pid_t pid;

	if ((ti = event_find(event->threadid, type)) == NULL)
		return;

	pid = ktrace_get_pid_for_thread(s, event->threadid);

	if (check_filter_mode(pid, ti, type, (int)event->arg1, (int)event->arg2, sc_name)) {
		const char *pathname;

		pathname = NULL;

		/* most things are just interested in the first lookup */
		if (ti->pathname[0] != '\0')
			pathname = ti->pathname;

		if (!pathname)
			pathname = "";

		format_print(ti, sc_name, event, type, format, event->timestamp, ti->stime, ti->waited, pathname, NULL);
	}

	event_delete(ti);
}

void
event_mark_thread_waited(uint64_t thread)
{
	th_info_t	ti;
	int		hashid;

	hashid = thread & HASH_MASK;

	for (ti = th_info_hash[hashid]; ti; ti = ti->next) {
		if (ti->thread == thread)
			ti->waited = 1;
	}
}

#pragma mark network fd set routines

struct pid_fd_set {
	struct pid_fd_set *next;
	pid_t pid;
	char *set;
	size_t setsize; /* number of *bytes*, not bits */
};

struct pid_fd_set *pfs_hash[HASH_SIZE];

static struct pid_fd_set *
pfs_get(pid_t pid)
{
	struct pid_fd_set *pfs;
	int hashid;

	assert(pid >= 0);

	hashid = pid & HASH_MASK;

	for (pfs = pfs_hash[hashid]; pfs; pfs = pfs->next) {
		if (pfs->pid == pid) {
			return pfs;
		}
	}

	pfs = calloc(1, sizeof (struct pid_fd_set));

	pfs->pid = pid;
	pfs->set = NULL;
	pfs->setsize = 0;
	pfs->next = pfs_hash[hashid];
	pfs_hash[hashid] = pfs;

	return pfs;
}

void
fd_clear_pid(pid_t pid)
{
	struct pid_fd_set *pfs, *prev;
	int hashid;

	if (pid < 0)
		return;

	hashid = pid & HASH_MASK;

	pfs = pfs_hash[hashid];
	prev = NULL;

	while (pfs) {
		if (pfs->pid == pid) {
			if (prev) {
				prev->next = pfs->next;
			} else {
				pfs_hash[hashid] = pfs->next;
			}

			free(pfs->set);
			free(pfs);

			break;
		} else {
			prev = pfs;
			pfs = pfs->next;
		}
	}
}

void
fd_clear_all(void)
{
	struct pid_fd_set *pfs, *next;
	int i;

	for (i = 0; i < HASH_SIZE; i++) {
		for (pfs = pfs_hash[i]; pfs; pfs = next) {
			next = pfs->next;

			free(pfs->set);
			free(pfs);
		}

		pfs_hash[i] = NULL;
	}
}

void
fd_set_is_network(pid_t pid, uint64_t fd, bool set)
{
	struct pid_fd_set *pfs;

	if (pid < 0)
		return;
	if (fd > OPEN_MAX)
		return;

	pfs = pfs_get(pid);

	if (fd >= pfs->setsize * CHAR_BIT) {
		size_t newsize;

		if (!set) return;

		newsize = MAX(((size_t)fd + CHAR_BIT) / CHAR_BIT, 2 * pfs->setsize);
		pfs->set = reallocf(pfs->set, newsize);
		assert(pfs->set != NULL);

		bzero(pfs->set + pfs->setsize, newsize - pfs->setsize);
		pfs->setsize = newsize;
	}

	if (set)
		setbit(pfs->set, fd);
	else
		clrbit(pfs->set, fd);
}

bool
fd_is_network(pid_t pid, uint64_t fd)
{
	struct pid_fd_set *pfs;

	if (pid < 0)
		return false;

	pfs = pfs_get(pid);

	if (fd >= pfs->setsize * CHAR_BIT) {
		return false;
	}

	return isset(pfs->set, fd);
}

#pragma mark shared region address lookup routines

#define MAXINDEX 2048

struct library_range {
	uint64_t b_address;
	uint64_t e_address;
};

struct library_info {
	uint64_t b_address;
	uint64_t e_address;
	int	 r_type;
	char     *name;
};

struct library_range framework32 = {0, 0};
struct library_range framework64 = {0, 0};
struct library_range framework64h = {0, 0};

struct library_info library_infos[MAXINDEX];
int num_libraries = 0;

#define	TEXT_R		0
#define DATA_R		1
#define OBJC_R		2
#define IMPORT_R	3
#define UNICODE_R	4
#define IMAGE_R		5
#define LINKEDIT_R	6

static void
sort_library_addresses(void)
{
	library_infos[num_libraries].b_address = library_infos[num_libraries - 1].b_address + 0x800000;
	library_infos[num_libraries].e_address = library_infos[num_libraries].b_address;
	library_infos[num_libraries].name = NULL;

	qsort_b(library_infos, num_libraries, sizeof (struct library_info), ^int(const void *aa, const void *bb) {
		struct library_info *a = (struct library_info *)aa;
		struct library_info *b = (struct library_info *)bb;

		if (a->b_address < b->b_address) return -1;
		if (a->b_address == b->b_address) return 0;
		return 1;
	});
}

static int
scanline(char *inputstring, char **argv, int maxtokens)
{
	int n = 0;
	char **ap = argv, *p, *val;

	for (p = inputstring; n < maxtokens && p != NULL; ) {
		while ((val = strsep(&p, " \t")) != NULL && *val == '\0') ;

		*ap++ = val;
		n++;
	}

	*ap = 0;

	return n;
}

static int
read_shared_cache_map(const char *path, struct library_range *lr, char *linkedit_name)
{
	uint64_t	b_address, e_address;
	char	buf[1024];
	char	*fnp, *fn_tofree;
	FILE	*fd;
	char	frameworkName[256];
	char	*tokens[64];
	int	ntokens;
	int	type;
	int	linkedit_found = 0;
	char	*substring, *ptr;

	bzero(buf, sizeof(buf));
	bzero(tokens, sizeof(tokens));

	lr->b_address = 0;
	lr->e_address = 0;

	if ((fd = fopen(path, "r")) == 0)
		return 0;

	while (fgets(buf, 1023, fd)) {
		if (strncmp(buf, "mapping", 7))
			break;
	}

	buf[strlen(buf)-1] = 0;

	frameworkName[0] = 0;

	for (;;) {
		/*
		 * Extract lib name from path name
		 */
		if ((substring = strrchr(buf, '.'))) {
			/*
			 * There is a ".": name is whatever is between the "/" around the "."
			 */
			while ( *substring != '/')		    /* find "/" before "." */
				substring--;

			substring++;

			strncpy(frameworkName, substring, 256);           /* copy path from "/" */
			frameworkName[255] = 0;
			substring = frameworkName;

			while ( *substring != '/' && *substring)    /* find "/" after "." and stop string there */
				substring++;

			*substring = 0;
		} else {
			/*
			 * No ".": take segment after last "/"
			 */
			ptr = buf;
			substring = ptr;

			while (*ptr)  {
				if (*ptr == '/')
					substring = ptr + 1;
				ptr++;
			}

			strncpy(frameworkName, substring, 256);
			frameworkName[255] = 0;
		}

		fnp = malloc(strlen(frameworkName) + 1);
		fn_tofree = fnp;
		strcpy(fnp, frameworkName);

		while (fgets(buf, 1023, fd) && num_libraries < (MAXINDEX - 2)) {
			/*
			 * Get rid of EOL
			 */
			buf[strlen(buf)-1] = 0;

			ntokens = scanline(buf, tokens, 64);

			if (ntokens < 4)
				continue;

			if (strncmp(tokens[0], "__TEXT", 6) == 0)
				type = TEXT_R;
			else if (strncmp(tokens[0], "__DATA", 6) == 0)
				type = DATA_R;
			else if (strncmp(tokens[0], "__OBJC", 6) == 0)
				type = OBJC_R;
			else if (strncmp(tokens[0], "__IMPORT", 8) == 0)
				type = IMPORT_R;
			else if (strncmp(tokens[0], "__UNICODE", 9) == 0)
				type = UNICODE_R;
			else if (strncmp(tokens[0], "__IMAGE", 7) == 0)
				type = IMAGE_R;
			else if (strncmp(tokens[0], "__LINKEDIT", 10) == 0)
				type = LINKEDIT_R;
			else
				type = -1;

			if (type == LINKEDIT_R && linkedit_found)
				break;

			if (type != -1) {
				b_address = strtoull(tokens[1], 0, 16);
				e_address = strtoull(tokens[3], 0, 16);

				library_infos[num_libraries].b_address	= b_address;
				library_infos[num_libraries].e_address	= e_address;
				library_infos[num_libraries].r_type	= type;

				if (type == LINKEDIT_R) {
					library_infos[num_libraries].name = linkedit_name;
					linkedit_found = 1;
				} else {
					library_infos[num_libraries].name = fnp;
					fn_tofree = NULL;
				}
#if 0
				printf("%s(%d): %qx-%qx\n", frameworkInfo[numFrameworks].name, type, b_address, e_address);
#endif
				if (lr->b_address == 0 || b_address < lr->b_address)
					lr->b_address = b_address;

				if (lr->e_address == 0 || e_address > lr->e_address)
					lr->e_address = e_address;

				num_libraries++;
			}

			if (type == LINKEDIT_R)
				break;
		}

		free(fn_tofree);

		if (fgets(buf, 1023, fd) == 0)
			break;

		buf[strlen(buf)-1] = 0;
	}

	fclose(fd);

#if 0
	printf("%s range, %qx-%qx\n", path, lr->b_address, lr->e_address);
#endif
	return 1;
}

void
init_shared_cache_mapping(void)
{
	read_shared_cache_map("/var/db/dyld/dyld_shared_cache_i386.map", &framework32, "/var/db/dyld/dyld_shared_cache_i386");

	if (0 == read_shared_cache_map("/var/db/dyld/dyld_shared_cache_x86_64h.map", &framework64h, "/var/db/dyld/dyld_shared_cache_x86_64h")) {
		read_shared_cache_map("/var/db/dyld/dyld_shared_cache_x86_64.map", &framework64, "/var/db/dyld/dyld_shared_cache_x86_64");
	}

	sort_library_addresses();
}

void
lookup_name(uint64_t user_addr, char **type, char **name)
{
	int i;
	int start, last;

	static char *frameworkType[] = {
		"<TEXT>    ",
		"<DATA>    ",
		"<OBJC>    ",
		"<IMPORT>  ",
		"<UNICODE> ",
		"<IMAGE>   ",
		"<LINKEDIT>",
	};

	*name = NULL;
	*type = NULL;

	if (num_libraries) {
		if ((user_addr >= framework32.b_address && user_addr < framework32.e_address) ||
			(user_addr >= framework64.b_address && user_addr < framework64.e_address) ||
			(user_addr >= framework64h.b_address && user_addr < framework64h.e_address)) {

			start = 0;
			last  = num_libraries;

			for (i = num_libraries / 2; start < last; i = start + ((last - start) / 2)) {
				if (user_addr > library_infos[i].e_address)
					start = i+1;
				else
					last = i;
			}

			if (start < num_libraries &&
				user_addr >= library_infos[start].b_address && user_addr < library_infos[start].e_address) {
				*type = frameworkType[library_infos[start].r_type];
				*name = library_infos[start].name;
			}
		}
	}
}

#pragma mark disk I/O tracking routines

struct diskio *free_diskios = NULL;
struct diskio *busy_diskios = NULL;

struct diskio *
diskio_start(uint64_t type, uint64_t bp, uint64_t dev,
	     uint64_t blkno, uint64_t iosize, ktrace_event_t event)
{
	const char *command;
	struct diskio *dio;

	if ((dio = free_diskios)) {
		free_diskios = dio->next;
	} else {
		dio = malloc(sizeof (struct diskio));
	}

	dio->prev = NULL;

	dio->type = type;
	dio->bp = bp;
	dio->dev = dev;
	dio->blkno = blkno;
	dio->iosize = iosize;
	dio->issued_time = event->timestamp;
	dio->issuing_thread = event->threadid;
	dio->issuing_pid = ktrace_get_pid_for_thread(s, event->threadid);

	dio->bc_info = 0x0;

	command = ktrace_get_execname_for_thread(s, event->threadid);

	if (!command)
		command = "";

	strncpy(dio->issuing_command, command, MAXCOMLEN);
	dio->issuing_command[MAXCOMLEN] = '\0';

	dio->next = busy_diskios;

	if (dio->next)
		dio->next->prev = dio;

	busy_diskios = dio;

	return dio;
}

struct diskio *
diskio_find(uint64_t bp)
{
	struct diskio *dio;

	for (dio = busy_diskios; dio; dio = dio->next) {
		if (dio->bp == bp)
			return dio;
	}

	return NULL;
}

struct diskio *
diskio_complete(uint64_t bp, uint64_t io_errno, uint64_t resid,
		uint64_t thread, uint64_t curtime, struct timeval curtime_wall)
{
	struct diskio *dio;

	if ((dio = diskio_find(bp)) == NULL) return NULL;

	if (dio == busy_diskios) {
		if ((busy_diskios = dio->next))
			dio->next->prev = NULL;
	} else {
		if (dio->next)
			dio->next->prev = dio->prev;
		dio->prev->next = dio->next;
	}

	dio->iosize -= resid;
	dio->io_errno = io_errno;
	dio->completed_time = curtime;
	dio->completed_walltime = curtime_wall;
	dio->completion_thread = thread;

	return dio;
}

void
diskio_free(struct diskio *dio)
{
	dio->next = free_diskios;
	free_diskios = dio;
}

void
diskio_print(struct diskio *dio)
{
	char  *p = NULL;
	int   len = 0;
	uint64_t type;
	int   format = FMT_DISKIO;
	char  buf[64];

	type = dio->type;
	dio->is_meta = 0;

	if ((type & P_CS_Class) == P_CS_Class) {
		switch (type) {
			case P_CS_ReadChunk:
				p = "    RdChunkCS";
				len = 13;
				format = FMT_DISKIO_CS;
				break;
			case P_CS_WriteChunk:
				p = "    WrChunkCS";
				len = 13;
				format = FMT_DISKIO_CS;
				break;
			case P_CS_MetaRead:
				p = "  RdMetaCS";
				len = 10;
				format = FMT_DISKIO_CS;
				break;
			case P_CS_MetaWrite:
				p = "  WrMetaCS";
				len = 10;
				format = FMT_DISKIO_CS;
				break;
			case P_CS_TransformRead:
				p = "  RdBgTfCS";
				len = 10;
				break;
			case P_CS_TransformWrite:
				p = "  WrBgTfCS";
				len = 10;
				break;
			case P_CS_MigrationRead:
				p = "  RdBgMigrCS";
				len = 12;
				break;
			case P_CS_MigrationWrite:
				p = "  WrBgMigrCS";
				len = 12;
				break;
			default:
				p = "  CS";
				len = 4;
				break;
		}

		strncpy(buf, p, len);
	} else {
		switch (type & P_DISKIO_TYPE) {
			case P_RdMeta:
				dio->is_meta = 1;
				p = "  RdMeta";
				len = 8;
				break;
			case P_WrMeta:
				dio->is_meta = 1;
				p = "  WrMeta";
				len = 8;
				break;
			case P_RdData:
				p = "  RdData";
				len = 8;
				break;
			case P_WrData:
				p = "  WrData";
				len = 8;
				break;
			case P_PgIn:
				p = "  PgIn";
				len = 6;
				break;
			case P_PgOut:
				p = "  PgOut";
				len = 7;
				break;
			default:
				p = "  ";
				len = 2;
				break;
		}

		strncpy(buf, p, len);

		buf[len++] = '[';

		if (type & P_DISKIO_ASYNC)
			buf[len++] = 'A';
		else
			buf[len++] = 'S';

		if (type & P_DISKIO_NOCACHE)
			buf[len++] = 'N';

		int tier = (type & P_DISKIO_TIER_MASK) >> P_DISKIO_TIER_SHIFT;

		if (tier > 0) {
			buf[len++] = 'T';
			if (tier > 0 && tier < 10)
				buf[len++] = '0' + tier;

			if (type & P_DISKIO_TIER_UPGRADE) {
				buf[len++] = 'U';
			}
		}

		if (type & P_DISKIO_PASSIVE)
			buf[len++] = 'P';

		buf[len++] = ']';
	}

	buf[len] = 0;

	if (check_filter_mode(-1, NULL, type, 0, 0, buf)) {
		const char *pathname = ktrace_get_path_for_vp(s, dio->vnodeid);
		format_print(NULL, buf, NULL, type, format, dio->completed_time,
				dio->issued_time, 1, pathname ? pathname : "", dio);
	}
}

#pragma mark disk name routines

struct diskrec {
	struct diskrec *next;
	char *diskname;
	int   dev;
};

struct diskrec *disk_list = NULL;

void
cache_disk_names(void)
{
	struct stat    st;
	DIR            *dirp = NULL;
	struct dirent  *dir;
	struct diskrec *dnp;

	if ((dirp = opendir("/dev")) == NULL)
		return;

	while ((dir = readdir(dirp)) != NULL) {
		char nbuf[MAXPATHLEN];

		if (dir->d_namlen < 5 || strncmp("disk", dir->d_name, 4))
			continue;

		snprintf(nbuf, MAXPATHLEN, "%s/%s", "/dev", dir->d_name);

		if (stat(nbuf, &st) < 0)
			continue;

		if ((dnp = malloc(sizeof(struct diskrec))) == NULL)
			continue;

		if ((dnp->diskname = malloc(dir->d_namlen + 1)) == NULL) {
			free(dnp);
			continue;
		}
		strncpy(dnp->diskname, dir->d_name, dir->d_namlen);
		dnp->diskname[dir->d_namlen] = 0;
		dnp->dev = st.st_rdev;

		dnp->next = disk_list;
		disk_list = dnp;
	}

	closedir(dirp);
}

static void
recache_disk_names(void)
{
	struct diskrec *dnp, *next_dnp;

	for (dnp = disk_list; dnp; dnp = next_dnp) {
		next_dnp = dnp->next;

		free(dnp->diskname);
		free(dnp);
	}

	disk_list = NULL;
	cache_disk_names();
}

char *
find_disk_name(uint64_t dev)
{
	struct diskrec *dnp;
	int	i;

	if (dev == NFS_DEV)
		return ("NFS");

	if (dev == CS_DEV)
		return ("CS");

	for (i = 0; i < 2; i++) {
		for (dnp = disk_list; dnp; dnp = dnp->next) {
			if (dnp->dev == dev)
				return (dnp->diskname);
		}
		recache_disk_names();
	}

	return "NOTFOUND";
}

char *
generate_cs_disk_name(uint64_t dev, char *s)
{
	if (dev == -1)
		return "UNKNOWN";

	sprintf(s, "disk%" PRIu64 "s%" PRIu64, (dev >> 16) & 0xffff, dev & 0xffff);

	return (s);
}
