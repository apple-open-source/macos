/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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
cc -I. -DPRIVATE -D__APPLE_PRIVATE -O -o fs_usage fs_usage.c
*/

#define	Default_DELAY	1	/* default delay interval */

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <strings.h>
#include <nlist.h>
#include <fcntl.h>
#include <aio.h>
#include <string.h>
#include <dirent.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>

#include <libc.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/mman.h>

#ifndef KERNEL_PRIVATE
#define KERNEL_PRIVATE
#include <sys/kdebug.h>
#undef KERNEL_PRIVATE
#else
#include <sys/kdebug.h>
#endif /*KERNEL_PRIVATE*/

#include <sys/sysctl.h>
#include <errno.h>
#import <mach/clock_types.h>
#import <mach/mach_time.h>
#include <err.h>

extern int errno;



#define MAXINDEX 2048

typedef struct LibraryRange {
     uint64_t b_address;
     uint64_t e_address;
} LibraryRange;

LibraryRange framework32;
LibraryRange framework64;


typedef struct LibraryInfo {
     uint64_t b_address;
     uint64_t e_address;
     char     *name;
} LibraryInfo;

LibraryInfo frameworkInfo[MAXINDEX];
int numFrameworks = 0;

char *lookup_name(uint64_t user_addr);


/* 
   MAXCOLS controls when extra data kicks in.
   MAX_WIDE_MODE_COLS controls -w mode to get even wider data in path.
   If NUMPARMS changes to match the kernel, it will automatically
   get reflected in the -w mode output.
*/
#define NUMPARMS 23
#define PATHLENGTH (NUMPARMS*sizeof(long))
#define MAXCOLS 132
#define MAX_WIDE_MODE_COLS (PATHLENGTH + 80)
#define MAXWIDTH MAX_WIDE_MODE_COLS + 64

struct th_info {
        int  my_index;
        int  in_filemgr;
        int  thread;
        int  pid;
        int  type;
        int  arg1;
        int  arg2;
        int  arg3;
        int  arg4;
        int  arg5;
        int  arg6;
        int  arg7;
        int  arg8;
        int  child_thread;
        int  waited;
        double stime;
        long *pathptr;
        long pathname[NUMPARMS + 1];   /* add room for null terminator */
};

#define MAX_THREADS 512
struct th_info th_state[MAX_THREADS];

kd_threadmap *last_map = NULL;
int	last_thread = 0;
int	map_is_the_same = 0;

int  filemgr_in_progress = 0;
int  execs_in_progress = 0;
int  cur_start = 0;
int  cur_max = 0;
int  need_new_map = 1;
int  bias_secs = 0;
long last_time;
int  wideflag = 0;
int  columns = 0;
int  select_pid_mode = 0;  /* Flag set indicates that output is restricted
			      to selected pids or commands */

int  one_good_pid = 0;    /* Used to fail gracefully when bad pids given */

char	*arguments = 0;
int     argmax = 0;


#define	USLEEP_MIN	1
#define USLEEP_BEHIND	2
#define	USLEEP_MAX	32
int	usleep_ms = USLEEP_MIN;

/*
 * Network only or filesystem only output filter
 * Default of zero means report all activity - no filtering
 */
#define FILESYS_FILTER    0x01
#define NETWORK_FILTER    0x02
#define CACHEHIT_FILTER   0x04
#define EXEC_FILTER	  0x08
#define PATHNAME_FILTER	  0x10
#define DEFAULT_DO_NOT_FILTER  0x00

int filter_mode = CACHEHIT_FILTER;

#define NFS_DEV -1

struct diskrec {
        struct diskrec *next;
        char *diskname;
        int   dev;
};

struct diskio {
        struct diskio *next;
        struct diskio *prev;
        int  type;
        int  bp;
        int  dev;
        int  blkno;
        int  iosize;
        int  io_errno;
        int  issuing_thread;
        int  completion_thread;
        char issuing_command[MAXCOMLEN];
        double issued_time;
        double completed_time;
};

struct diskrec *disk_list = NULL;
struct diskio *free_diskios = NULL;
struct diskio *busy_diskios = NULL;



struct diskio *insert_diskio();
struct diskio *complete_diskio();
void    	free_diskio();
void		print_diskio();
void            format_print(struct th_info *, char *, int, int, int, int, int, int, int, double, double, int, char *, struct diskio *);
void            exit_syscall(char *, int, int, int, int, int, int, int, double);
char           *find_disk_name();
void		cache_disk_names();
int 		ReadSharedCacheMap(const char *, LibraryRange *);
void 		SortFrameworkAddresses();
void		mark_thread_waited(int);
int		check_filter_mode(struct th_info *, int, int, int, char *);
void		fs_usage_fd_set(unsigned int, unsigned int);
int		fs_usage_fd_isset(unsigned int, unsigned int);
void		fs_usage_fd_clear(unsigned int, unsigned int);
void		init_arguments_buffer();
int	        get_real_command_name(int, char *, int);
void            create_map_entry(int, int, char *);

void		enter_syscall();
void		extend_syscall();
void		kill_thread_map();

#define CLASS_MASK	0xff000000
#define CSC_MASK	0xffff0000
#define BSC_INDEX(type)	((type >> 2) & 0x3fff)


#define TRACE_DATA_NEWTHREAD   0x07000004
#define TRACE_DATA_EXEC        0x07000008
#define TRACE_STRING_NEWTHREAD 0x07010004
#define TRACE_STRING_EXEC      0x07010008

#define MACH_vmfault    0x01300008
#define MACH_pageout    0x01300004
#define MACH_sched      0x01400000
#define MACH_stkhandoff 0x01400008
#define VFS_LOOKUP      0x03010090
#define BSC_exit        0x040C0004

#define P_DISKIO	0x03020000
#define P_DISKIO_DONE	0x03020004
#define P_DISKIO_MASK	(CSC_MASK | 0x4)

#define P_WrData	0x03020000
#define P_RdData	0x03020008
#define P_WrMeta	0x03020020
#define P_RdMeta	0x03020028
#define P_PgOut		0x03020040
#define P_PgIn		0x03020048
#define P_WrDataAsync	0x03020010
#define P_RdDataAsync	0x03020018
#define P_WrMetaAsync	0x03020030
#define P_RdMetaAsync	0x03020038
#define P_PgOutAsync	0x03020050
#define P_PgInAsync	0x03020058

#define P_WrDataDone		0x03020004
#define P_RdDataDone		0x0302000C
#define P_WrMetaDone		0x03020024
#define P_RdMetaDone		0x0302002C
#define P_PgOutDone		0x03020044
#define P_PgInDone		0x0302004C
#define P_WrDataAsyncDone	0x03020014
#define P_RdDataAsyncDone	0x0302001C
#define P_WrMetaAsyncDone	0x03020034
#define P_RdMetaAsyncDone	0x0302003C
#define P_PgOutAsyncDone	0x03020054
#define P_PgInAsyncDone		0x0302005C


#define MSC_map_fd   0x010c00ac

#define BSC_BASE     0x040C0000
#define MSC_BASE     0x010C0000

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
#define BSC_mkfifo		0x040c0210	
#define BSC_mkdir		0x040C0220	
#define BSC_rmdir		0x040C0224
#define BSC_utimes		0x040C0228
#define BSC_futimes		0x040C022C
#define BSC_pread		0x040C0264
#define BSC_pwrite		0x040C0268
#define BSC_statfs		0x040C0274	
#define BSC_fstatfs		0x040C0278	
#define BSC_stat		0x040C02F0	
#define BSC_fstat		0x040C02F4	
#define BSC_lstat		0x040C02F8	
#define BSC_pathconf		0x040C02FC	
#define BSC_fpathconf		0x040C0300
#define BSC_getdirentries	0x040C0310
#define BSC_mmap		0x040c0314
#define BSC_lseek		0x040c031c
#define BSC_truncate		0x040C0320
#define BSC_ftruncate   	0x040C0324
#define BSC_undelete		0x040C0334
#define BSC_statv		0x040C0364	
#define BSC_lstatv		0x040C0368	
#define BSC_fstatv		0x040C036C	
#define BSC_mkcomplex   	0x040C0360	
#define BSC_getattrlist 	0x040C0370	
#define BSC_setattrlist 	0x040C0374	
#define BSC_getdirentriesattr	0x040C0378	
#define BSC_exchangedata	0x040C037C	
#define BSC_checkuseraccess	0x040C0380	
#define BSC_searchfs    	0x040C0384
#define BSC_delete      	0x040C0388
#define BSC_copyfile   		0x040C038C
#define BSC_getxattr		0x040C03A8
#define BSC_fgetxattr		0x040C03AC
#define BSC_setxattr		0x040C03B0
#define BSC_fsetxattr		0x040C03B4
#define BSC_removexattr		0x040C03B8
#define BSC_fremovexattr	0x040C03BC
#define BSC_listxattr		0x040C03C0
#define BSC_flistxattr		0x040C03C4
#define BSC_fsctl       	0x040C03C8
#define BSC_open_extended	0x040C0454
#define BSC_stat_extended	0x040C045C
#define BSC_lstat_extended	0x040C0460
#define BSC_fstat_extended	0x040C0464
#define BSC_chmod_extended	0x040C0468
#define BSC_fchmod_extended	0x040C046C
#define BSC_access_extended	0x040C0470
#define BSC_mkfifo_extended	0x040C048C
#define BSC_mkdir_extended	0x040C0490
#define BSC_load_shared_file	0x040C04A0
#define BSC_aio_fsync		0x040C04E4
#define	BSC_aio_return		0x040C04E8
#define BSC_aio_suspend		0x040C04EC
#define BSC_aio_cancel		0x040C04F0
#define BSC_aio_error		0x040C04F4
#define BSC_aio_read		0x040C04F8
#define BSC_aio_write		0x040C04FC
#define BSC_lio_listio		0x040C0500
#define BSC_lchown		0x040C05B0

#define BSC_read_nocancel	0x040c0630
#define BSC_write_nocancel	0x040c0634
#define BSC_open_nocancel	0x040c0638
#define BSC_close_nocancel      0x040c063c
#define BSC_recvmsg_nocancel	0x040c0644
#define BSC_sendmsg_nocancel	0x040c0648
#define BSC_recvfrom_nocancel	0x040c064c
#define BSC_accept_nocancel	0x040c0650
#define BSC_msync_nocancel	0x040c0654
#define BSC_fcntl_nocancel	0x040c0658
#define BSC_select_nocancel	0x040c065c
#define BSC_fsync_nocancel	0x040c0660
#define BSC_connect_nocancel	0x040c0664
#define BSC_readv_nocancel	0x040c066c
#define BSC_writev_nocancel	0x040c0670
#define BSC_sendto_nocancel	0x040c0674
#define BSC_pread_nocancel	0x040c0678
#define BSC_pwrite_nocancel	0x040c067c
#define BSC_aio_suspend_nocancel	0x40c0694

#define BSC_msync_extended	0x040e0104
#define BSC_pread_extended	0x040e0264
#define BSC_pwrite_extended	0x040e0268
#define BSC_mmap_extended	0x040e0314
#define BSC_mmap_extended2	0x040f0314

// Carbon File Manager support
#define FILEMGR_PBGETCATALOGINFO		0x1e000020
#define FILEMGR_PBGETCATALOGINFOBULK		0x1e000024
#define FILEMGR_PBCREATEFILEUNICODE		0x1e000028
#define FILEMGR_PBCREATEDIRECTORYUNICODE	0x1e00002c
#define FILEMGR_PBCREATEFORK			0x1e000030
#define FILEMGR_PBDELETEFORK			0x1e000034
#define FILEMGR_PBITERATEFORK			0x1e000038
#define FILEMGR_PBOPENFORK			0x1e00003c
#define FILEMGR_PBREADFORK			0x1e000040
#define FILEMGR_PBWRITEFORK			0x1e000044
#define FILEMGR_PBALLOCATEFORK			0x1e000048
#define FILEMGR_PBDELETEOBJECT			0x1e00004c
#define FILEMGR_PBEXCHANGEOBJECT		0x1e000050
#define FILEMGR_PBGETFORKCBINFO			0x1e000054
#define FILEMGR_PBGETVOLUMEINFO			0x1e000058
#define FILEMGR_PBMAKEFSREF			0x1e00005c
#define FILEMGR_PBMAKEFSREFUNICODE		0x1e000060
#define FILEMGR_PBMOVEOBJECT			0x1e000064
#define FILEMGR_PBOPENITERATOR			0x1e000068
#define FILEMGR_PBRENAMEUNICODE			0x1e00006c
#define FILEMGR_PBSETCATALOGINFO		0x1e000070
#define FILEMGR_PBSETVOLUMEINFO			0x1e000074
#define FILEMGR_FSREFMAKEPATH			0x1e000078
#define FILEMGR_FSPATHMAKEREF			0x1e00007c

#define FILEMGR_PBGETCATINFO			0x1e010000
#define FILEMGR_PBGETCATINFOLITE		0x1e010004
#define FILEMGR_PBHGETFINFO			0x1e010008
#define FILEMGR_PBXGETVOLINFO			0x1e01000c
#define FILEMGR_PBHCREATE			0x1e010010
#define FILEMGR_PBHOPENDF			0x1e010014
#define FILEMGR_PBHOPENRF			0x1e010018
#define FILEMGR_PBHGETDIRACCESS			0x1e01001c
#define FILEMGR_PBHSETDIRACCESS			0x1e010020
#define FILEMGR_PBHMAPID			0x1e010024
#define FILEMGR_PBHMAPNAME			0x1e010028
#define FILEMGR_PBCLOSE				0x1e01002c
#define FILEMGR_PBFLUSHFILE			0x1e010030
#define FILEMGR_PBGETEOF			0x1e010034
#define FILEMGR_PBSETEOF			0x1e010038
#define FILEMGR_PBGETFPOS			0x1e01003c
#define FILEMGR_PBREAD				0x1e010040
#define FILEMGR_PBWRITE				0x1e010044
#define FILEMGR_PBGETFCBINFO			0x1e010048
#define FILEMGR_PBSETFINFO			0x1e01004c
#define FILEMGR_PBALLOCATE			0x1e010050
#define FILEMGR_PBALLOCCONTIG			0x1e010054
#define FILEMGR_PBSETFPOS			0x1e010058
#define FILEMGR_PBSETCATINFO			0x1e01005c
#define FILEMGR_PBGETVOLPARMS			0x1e010060
#define FILEMGR_PBSETVINFO			0x1e010064
#define FILEMGR_PBMAKEFSSPEC			0x1e010068
#define FILEMGR_PBHGETVINFO			0x1e01006c
#define FILEMGR_PBCREATEFILEIDREF		0x1e010070
#define FILEMGR_PBDELETEFILEIDREF		0x1e010074
#define FILEMGR_PBRESOLVEFILEIDREF		0x1e010078
#define FILEMGR_PBFLUSHVOL			0x1e01007c
#define FILEMGR_PBHRENAME			0x1e010080
#define FILEMGR_PBCATMOVE			0x1e010084
#define FILEMGR_PBEXCHANGEFILES			0x1e010088
#define FILEMGR_PBHDELETE			0x1e01008c
#define FILEMGR_PBDIRCREATE			0x1e010090
#define FILEMGR_PBCATSEARCH			0x1e010094
#define FILEMGR_PBHSETFLOCK			0x1e010098
#define FILEMGR_PBHRSTFLOCK			0x1e01009c
#define FILEMGR_PBLOCKRANGE			0x1e0100a0
#define FILEMGR_PBUNLOCKRANGE			0x1e0100a4


#define FILEMGR_CLASS   0x1e
#define FILEMGR_BASE	0x1e000000

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

#define MAX_BSD_SYSCALL	512

struct bsd_syscall {
        char	*sc_name;
        int	sc_format;
} bsd_syscalls[MAX_BSD_SYSCALL];


int bsd_syscall_types[] = {
        BSC_recvmsg,
        BSC_recvmsg_nocancel,
	BSC_sendmsg,
	BSC_sendmsg_nocancel,
	BSC_recvfrom,
	BSC_recvfrom_nocancel,
	BSC_accept,
	BSC_accept_nocancel,
	BSC_select,
	BSC_select_nocancel,
	BSC_socket,
	BSC_connect,
	BSC_connect_nocancel,
	BSC_bind,
	BSC_listen,
	BSC_sendto,
	BSC_sendto_nocancel,
	BSC_socketpair,
	BSC_read,
	BSC_read_nocancel,
	BSC_write,
	BSC_write_nocancel,
	BSC_open,
	BSC_open_nocancel,
	BSC_close,
	BSC_close_nocancel,
	BSC_link,
	BSC_unlink,
	BSC_chdir,
	BSC_fchdir,
	BSC_mknod,
	BSC_chmod,
	BSC_chown,
	BSC_access,
	BSC_chflags,
	BSC_fchflags,
	BSC_sync,
	BSC_dup,
	BSC_revoke,
	BSC_symlink,
	BSC_readlink,
	BSC_execve,
	BSC_chroot,
	BSC_dup2,
	BSC_fsync,
	BSC_fsync_nocancel,
	BSC_readv,
	BSC_readv_nocancel,
	BSC_writev,
	BSC_writev_nocancel,
	BSC_fchown,
	BSC_fchmod,
	BSC_rename,
	BSC_mkfifo,
	BSC_mkdir,
	BSC_rmdir,
	BSC_utimes,
	BSC_futimes,
	BSC_pread,
	BSC_pread_nocancel,
	BSC_pwrite,
	BSC_pwrite_nocancel,
	BSC_statfs,
	BSC_fstatfs,
	BSC_stat,
	BSC_fstat,
	BSC_lstat,
	BSC_pathconf,
	BSC_fpathconf,
	BSC_getdirentries,
	BSC_mmap,
	BSC_lseek,
	BSC_truncate,
	BSC_ftruncate,
	BSC_undelete,
	BSC_statv,
	BSC_lstatv,
	BSC_fstatv,
	BSC_mkcomplex,
	BSC_getattrlist,
	BSC_setattrlist,
	BSC_getdirentriesattr,
	BSC_exchangedata,
	BSC_checkuseraccess,
	BSC_searchfs,
	BSC_delete,
	BSC_copyfile,
	BSC_getxattr,
	BSC_fgetxattr,
	BSC_setxattr,
	BSC_fsetxattr,
	BSC_removexattr,
	BSC_fremovexattr,
	BSC_listxattr,
	BSC_flistxattr,
	BSC_fsctl,
	BSC_open_extended,
	BSC_stat_extended,
	BSC_lstat_extended,
	BSC_fstat_extended,
	BSC_chmod_extended,
	BSC_fchmod_extended,
	BSC_access_extended,
	BSC_mkfifo_extended,
	BSC_mkdir_extended,
	BSC_load_shared_file,
	BSC_aio_fsync,
	BSC_aio_return,
	BSC_aio_suspend,
	BSC_aio_suspend_nocancel,
	BSC_aio_cancel,
	BSC_aio_error,
	BSC_aio_read,
	BSC_aio_write,
	BSC_lio_listio,
	BSC_lchown,
	BSC_msync,
	BSC_msync_nocancel,
	BSC_fcntl,
	BSC_fcntl_nocancel,
	BSC_ioctl,
	0
};


#define MAX_FILEMGR	512

struct filemgr_call {
        char	*fm_name;
} filemgr_calls[MAX_FILEMGR];


int filemgr_call_types[] = {
        FILEMGR_PBGETCATALOGINFO,
	FILEMGR_PBGETCATALOGINFOBULK,
	FILEMGR_PBCREATEFILEUNICODE,
	FILEMGR_PBCREATEDIRECTORYUNICODE,
	FILEMGR_PBCREATEFORK,
	FILEMGR_PBDELETEFORK,
	FILEMGR_PBITERATEFORK,
	FILEMGR_PBOPENFORK,
	FILEMGR_PBREADFORK,
	FILEMGR_PBWRITEFORK,
	FILEMGR_PBALLOCATEFORK,
	FILEMGR_PBDELETEOBJECT,
	FILEMGR_PBEXCHANGEOBJECT,
	FILEMGR_PBGETFORKCBINFO,
	FILEMGR_PBGETVOLUMEINFO,
	FILEMGR_PBMAKEFSREF,
	FILEMGR_PBMAKEFSREFUNICODE,
	FILEMGR_PBMOVEOBJECT,
	FILEMGR_PBOPENITERATOR,
	FILEMGR_PBRENAMEUNICODE,
	FILEMGR_PBSETCATALOGINFO,
	FILEMGR_PBSETVOLUMEINFO,
	FILEMGR_FSREFMAKEPATH,
	FILEMGR_FSPATHMAKEREF,

	FILEMGR_PBGETCATINFO,
	FILEMGR_PBGETCATINFOLITE,
	FILEMGR_PBHGETFINFO,
	FILEMGR_PBXGETVOLINFO,
	FILEMGR_PBHCREATE,
	FILEMGR_PBHOPENDF,
	FILEMGR_PBHOPENRF,
	FILEMGR_PBHGETDIRACCESS,
	FILEMGR_PBHSETDIRACCESS,
	FILEMGR_PBHMAPID,
	FILEMGR_PBHMAPNAME,
	FILEMGR_PBCLOSE,
	FILEMGR_PBFLUSHFILE,
	FILEMGR_PBGETEOF,
	FILEMGR_PBSETEOF,
	FILEMGR_PBGETFPOS,
	FILEMGR_PBREAD,
	FILEMGR_PBWRITE,
	FILEMGR_PBGETFCBINFO,
	FILEMGR_PBSETFINFO,
	FILEMGR_PBALLOCATE,
	FILEMGR_PBALLOCCONTIG,
	FILEMGR_PBSETFPOS,
	FILEMGR_PBSETCATINFO,
	FILEMGR_PBGETVOLPARMS,
	FILEMGR_PBSETVINFO,
	FILEMGR_PBMAKEFSSPEC,
	FILEMGR_PBHGETVINFO,
	FILEMGR_PBCREATEFILEIDREF,
	FILEMGR_PBDELETEFILEIDREF,
	FILEMGR_PBRESOLVEFILEIDREF,
	FILEMGR_PBFLUSHVOL,
	FILEMGR_PBHRENAME,
	FILEMGR_PBCATMOVE,
	FILEMGR_PBEXCHANGEFILES,
	FILEMGR_PBHDELETE,
	FILEMGR_PBDIRCREATE,
	FILEMGR_PBCATSEARCH,
	FILEMGR_PBHSETFLOCK,
	FILEMGR_PBHRSTFLOCK,
	FILEMGR_PBLOCKRANGE,
	FILEMGR_PBUNLOCKRANGE,
	0
};



#define MAX_PIDS 256
int    pids[MAX_PIDS];

int    num_of_pids = 0;
int    exclude_pids = 0;
int    exclude_default_pids = 1;


struct kinfo_proc *kp_buffer = 0;
int kp_nentries = 0;

#define SAMPLE_SIZE 100000

int num_cpus;

#define EVENT_BASE 60000

int num_events = EVENT_BASE;

#define DBG_ZERO_FILL_FAULT   1
#define DBG_PAGEIN_FAULT      2
#define DBG_COW_FAULT         3
#define DBG_CACHE_HIT_FAULT   4

#define DBG_FUNC_ALL	(DBG_FUNC_START | DBG_FUNC_END)
#define DBG_FUNC_MASK	0xfffffffc

double divisor = 0.0;       /* Trace divisor converts to microseconds */

int mib[6];
size_t needed;
char  *my_buffer;

kbufinfo_t bufinfo = {0, 0, 0, 0, 0};

int total_threads = 0;
kd_threadmap *mapptr = 0;	/* pointer to list of threads */

/* defines for tracking file descriptor state */
#define FS_USAGE_FD_SETSIZE 256		/* Initial number of file descriptors per
					   thread that we will track */

#define FS_USAGE_NFDBITS      (sizeof (unsigned long) * 8)
#define FS_USAGE_NFDBYTES(n)  (((n) / FS_USAGE_NFDBITS) * sizeof (unsigned long))

typedef struct {
    unsigned int   fd_valid;       /* set if this is a valid entry */
    unsigned int   fd_thread;
    unsigned int   fd_setsize;     /* this is a bit count */
    unsigned long  *fd_setptr;     /* file descripter bitmap */
} fd_threadmap;

fd_threadmap *fdmapptr = 0;	/* pointer to list of threads for fd tracking */

int trace_enabled = 0;
int set_remove_flag = 1;

void set_numbufs();
void set_init();
void set_enable();
void sample_sc();
int quit();

/*
 *  signal handlers
 */

void leave()			/* exit under normal conditions -- INT handler */
{
        int i;
	void set_enable();
	void set_pidcheck();
	void set_pidexclude();
	void set_remove();

	fflush(0);

	set_enable(0);

	if (exclude_pids == 0) {
	        for (i = 0; i < num_of_pids; i++)
		        set_pidcheck(pids[i], 0);
	}
	else {
	        for (i = 0; i < num_of_pids; i++)
		        set_pidexclude(pids[i], 0);
	}
	set_remove();
	exit(0);
}


void get_screenwidth()
{
        struct winsize size;

	columns = MAXCOLS;

	if (isatty(1)) {
	        if (ioctl(1, TIOCGWINSZ, &size) != -1) {
		        columns = size.ws_col;

			if (columns > MAXWIDTH)
			        columns = MAXWIDTH;
		}
	}
}


void sigwinch()
{
        if (!wideflag)
	        get_screenwidth();
}

int
exit_usage(char *myname) {

        fprintf(stderr, "Usage: %s [-e] [-w] [-f mode] [pid | cmd [pid | cmd]....]\n", myname);
	fprintf(stderr, "  -e    exclude the specified list of pids from the sample\n");
	fprintf(stderr, "        and exclude fs_usage by default\n");
	fprintf(stderr, "  -w    force wider, detailed, output\n");
	fprintf(stderr, "  -f    Output is based on the mode provided\n");
	fprintf(stderr, "          mode = \"network\"  Show only network related output\n");
	fprintf(stderr, "          mode = \"filesys\"  Show only file system related output\n");
	fprintf(stderr, "          mode = \"pathname\" Show only pathname related output\n");
	fprintf(stderr, "          mode = \"exec\"     Show only execs\n");
	fprintf(stderr, "          mode = \"cachehit\" In addition, show cachehits\n");
	fprintf(stderr, "  pid   selects process(s) to sample\n");
	fprintf(stderr, "  cmd   selects process(s) matching command string to sample\n");
	fprintf(stderr, "\n%s will handle a maximum list of %d pids.\n\n", myname, MAX_PIDS);
	fprintf(stderr, "By default (no options) the following processes are excluded from the output:\n");
	fprintf(stderr, "fs_usage, Terminal, telnetd, sshd, rlogind, tcsh, csh, sh\n\n");

	exit(1);
}


int filemgr_index(type) {

        if (type & 0x10000)
	        return (((type >> 2) & 0x3fff) + 256);

        return (((type >> 2) & 0x3fff));
}


void init_tables(void)
{	int i;
        int type; 
        int code; 

	for (i = 0; i < MAX_THREADS; i++)
	        th_state[i].my_index = i;

	for (i = 0; i < MAX_BSD_SYSCALL; i++) {
	        bsd_syscalls[i].sc_name = NULL;
	        bsd_syscalls[i].sc_format = FMT_DEFAULT;
	}

	for (i = 0; i < MAX_FILEMGR; i++) {
	        filemgr_calls[i].fm_name = NULL;
	}

	for (i = 0; (type = bsd_syscall_types[i]); i++) {

	        code = BSC_INDEX(type);

		if (code >= MAX_BSD_SYSCALL) {
		        printf("BSD syscall init (%x):  type exceeds table size\n", type);
		        continue;
		}
		switch (type) {
		    
		    case BSC_recvmsg:
		    case BSC_recvmsg_nocancel:
		      bsd_syscalls[code].sc_name = "recvmsg";
		      bsd_syscalls[code].sc_format = FMT_FD_IO;
		      break;

		    case BSC_sendmsg:
		    case BSC_sendmsg_nocancel:
		      bsd_syscalls[code].sc_name = "sendmsg";
		      bsd_syscalls[code].sc_format = FMT_FD_IO;
		      break;

		    case BSC_recvfrom:
		    case BSC_recvfrom_nocancel:
		      bsd_syscalls[code].sc_name = "recvfrom";
		      bsd_syscalls[code].sc_format = FMT_FD_IO;
		      break;

		    case BSC_sendto:
		    case BSC_sendto_nocancel:
		      bsd_syscalls[code].sc_name = "sendto";
		      bsd_syscalls[code].sc_format = FMT_FD_IO;
		      break;

		    case BSC_select:
		    case BSC_select_nocancel:
		      bsd_syscalls[code].sc_name = "select";
		      bsd_syscalls[code].sc_format = FMT_SELECT;
		      break;
		    
		    case BSC_accept:
		    case BSC_accept_nocancel:
		      bsd_syscalls[code].sc_name = "accept";
		      bsd_syscalls[code].sc_format = FMT_FD_2;
		      break;
		    
		    case BSC_socket:
		      bsd_syscalls[code].sc_name = "socket";
		      bsd_syscalls[code].sc_format = FMT_SOCKET;
		      break;

		    case BSC_connect:
		    case BSC_connect_nocancel:
		      bsd_syscalls[code].sc_name = "connect";
		      bsd_syscalls[code].sc_format = FMT_FD;
		      break;

		    case BSC_bind:
		      bsd_syscalls[code].sc_name = "bind";
		      bsd_syscalls[code].sc_format = FMT_FD;
		      break;

		    case BSC_listen:
		      bsd_syscalls[code].sc_name = "listen";
		      bsd_syscalls[code].sc_format = FMT_FD;
		      break;

		    case BSC_mmap:
		      bsd_syscalls[code].sc_name = "mmap";
		      bsd_syscalls[code].sc_format = FMT_MMAP;
		      break;
		    
		    case BSC_socketpair:
		      bsd_syscalls[code].sc_name = "socketpair";
		      break;
		    
		    case BSC_getxattr:
		      bsd_syscalls[code].sc_name = "getxattr";
		      break;
                    
		    case BSC_setxattr:
		      bsd_syscalls[code].sc_name = "setxattr";
		      break;
                    
		    case BSC_removexattr:
		      bsd_syscalls[code].sc_name = "removexattr";
		      break;
                    
		    case BSC_listxattr:
		      bsd_syscalls[code].sc_name = "listxattr";
		      break;
                    
		    case BSC_stat:
		      bsd_syscalls[code].sc_name = "stat";
		      break;
                    
		    case BSC_stat_extended:
		      bsd_syscalls[code].sc_name = "stat_extended";
		      break;
                    
		    case BSC_execve:
		      bsd_syscalls[code].sc_name = "execve";
		      break;
                    
		    case BSC_load_shared_file:
		      bsd_syscalls[code].sc_name = "load_sf";
		      break;

		    case BSC_open:
		    case BSC_open_nocancel:
		      bsd_syscalls[code].sc_name = "open";
		      bsd_syscalls[code].sc_format = FMT_OPEN;
		      break;

		    case BSC_open_extended:
		      bsd_syscalls[code].sc_name = "open_extended";
		      bsd_syscalls[code].sc_format = FMT_OPEN;
		      break;

		    case BSC_dup:
		      bsd_syscalls[code].sc_name = "dup";
		      bsd_syscalls[code].sc_format = FMT_FD_2;
		      break;

		    case BSC_dup2:
		      bsd_syscalls[code].sc_name = "dup2";
		      bsd_syscalls[code].sc_format = FMT_FD_2;
		      break;		    

		    case BSC_close:
		    case BSC_close_nocancel:
		      bsd_syscalls[code].sc_name = "close";
		      bsd_syscalls[code].sc_format = FMT_FD;
		      break;

		    case BSC_read:
		    case BSC_read_nocancel:
		      bsd_syscalls[code].sc_name = "read";
		      bsd_syscalls[code].sc_format = FMT_FD_IO;
		      break;

		    case BSC_write:
		    case BSC_write_nocancel:
		      bsd_syscalls[code].sc_name = "write";
		      bsd_syscalls[code].sc_format = FMT_FD_IO;
		      break;

		    case BSC_fgetxattr:
		      bsd_syscalls[code].sc_name = "fgetxattr";
		      bsd_syscalls[code].sc_format = FMT_FD;
		      break;

		    case BSC_fsetxattr:
		      bsd_syscalls[code].sc_name = "fsetxattr";
		      bsd_syscalls[code].sc_format = FMT_FD;
		      break;

		    case BSC_fremovexattr:
		      bsd_syscalls[code].sc_name = "fremovexattr";
		      bsd_syscalls[code].sc_format = FMT_FD;
		      break;

		    case BSC_flistxattr:
		      bsd_syscalls[code].sc_name = "flistxattr";
		      bsd_syscalls[code].sc_format = FMT_FD;
		      break;

		    case BSC_fstat:
		      bsd_syscalls[code].sc_name = "fstat";
		      bsd_syscalls[code].sc_format = FMT_FD;
		      break;

		    case BSC_fstat_extended:
		      bsd_syscalls[code].sc_name = "fstat_extended";
		      bsd_syscalls[code].sc_format = FMT_FD;
		      break;

		    case BSC_lstat:
		      bsd_syscalls[code].sc_name = "lstat";
		      break;

		    case BSC_lstat_extended:
		      bsd_syscalls[code].sc_name = "lstat_extended";
		      break;

		    case BSC_lstatv:
		      bsd_syscalls[code].sc_name = "lstatv";
		      break;

		    case BSC_link:
		      bsd_syscalls[code].sc_name = "link";
		      break;

		    case BSC_unlink:
		      bsd_syscalls[code].sc_name = "unlink";
		      break;

		    case BSC_mknod:
		      bsd_syscalls[code].sc_name = "mknod";
		      break;

		    case BSC_chmod:
		      bsd_syscalls[code].sc_name = "chmod";
		      bsd_syscalls[code].sc_format = FMT_CHMOD;
		      break;

		    case BSC_chmod_extended:
		      bsd_syscalls[code].sc_name = "chmod_extended";
		      bsd_syscalls[code].sc_format = FMT_CHMOD_EXT;
		      break;

		    case BSC_fchmod:
		      bsd_syscalls[code].sc_name = "fchmod";
		      bsd_syscalls[code].sc_format = FMT_FCHMOD;
		      break;

		    case BSC_fchmod_extended:
		      bsd_syscalls[code].sc_name = "fchmod_extended";
		      bsd_syscalls[code].sc_format = FMT_FCHMOD_EXT;
		      break;

		    case BSC_chown:
		      bsd_syscalls[code].sc_name = "chown";
		      break;

		    case BSC_lchown:
		      bsd_syscalls[code].sc_name = "lchown";
		      break;

		    case BSC_fchown:
		      bsd_syscalls[code].sc_name = "fchown";
		      bsd_syscalls[code].sc_format = FMT_FD;
		      break;

		    case BSC_access:
		      bsd_syscalls[code].sc_name = "access";
		      bsd_syscalls[code].sc_format = FMT_ACCESS;
		      break;

		    case BSC_access_extended:
		      bsd_syscalls[code].sc_name = "access_extended";
		      break;

		    case BSC_chdir:
		      bsd_syscalls[code].sc_name = "chdir";
		      break;
                    
		    case BSC_chroot:
		      bsd_syscalls[code].sc_name = "chroot";
		      break;
                    
		    case BSC_utimes:
		      bsd_syscalls[code].sc_name = "utimes";
		      break;
                    
		    case BSC_delete:
		      bsd_syscalls[code].sc_name = "delete";
		      break;
                    
		    case BSC_undelete:
		      bsd_syscalls[code].sc_name = "undelete";
		      break;
                    
		    case BSC_revoke:
		      bsd_syscalls[code].sc_name = "revoke";
		      break;
                    
		    case BSC_fsctl:
		      bsd_syscalls[code].sc_name = "fsctl";
		      break;
                    
		    case BSC_chflags:
		      bsd_syscalls[code].sc_name = "chflags";
		      bsd_syscalls[code].sc_format = FMT_CHFLAGS;
		      break;
                    
		    case BSC_fchflags:
		      bsd_syscalls[code].sc_name = "fchflags";
		      bsd_syscalls[code].sc_format = FMT_FCHFLAGS;
		      break;
                    
		    case BSC_fchdir:
		      bsd_syscalls[code].sc_name = "fchdir";
		      bsd_syscalls[code].sc_format = FMT_FD;
		      break;
                    
		    case BSC_futimes:
		      bsd_syscalls[code].sc_name = "futimes";
		      bsd_syscalls[code].sc_format = FMT_FD;
		      break;

		    case BSC_sync:
		      bsd_syscalls[code].sc_name = "sync";
		      break;

		    case BSC_symlink:
		      bsd_syscalls[code].sc_name = "symlink";
		      break;

		    case BSC_readlink:
		      bsd_syscalls[code].sc_name = "readlink";
		      break;

		    case BSC_fsync:
		    case BSC_fsync_nocancel:
		      bsd_syscalls[code].sc_name = "fsync";
		      bsd_syscalls[code].sc_format = FMT_FD;
		      break;

		    case BSC_readv:
		    case BSC_readv_nocancel:
		      bsd_syscalls[code].sc_name = "readv";
		      bsd_syscalls[code].sc_format = FMT_FD_IO;
		      break;

		    case BSC_writev:
		    case BSC_writev_nocancel:
		      bsd_syscalls[code].sc_name = "writev";
		      bsd_syscalls[code].sc_format = FMT_FD_IO;
		      break;

		    case BSC_pread:
		    case BSC_pread_nocancel:
		      bsd_syscalls[code].sc_name = "pread";
		      bsd_syscalls[code].sc_format = FMT_PREAD;
		      break;

		    case BSC_pwrite:
		    case BSC_pwrite_nocancel:
		      bsd_syscalls[code].sc_name = "pwrite";
		      bsd_syscalls[code].sc_format = FMT_PREAD;
		      break;

		    case BSC_mkdir:
		      bsd_syscalls[code].sc_name = "mkdir";
		      break;
                    
		    case BSC_mkdir_extended:
		      bsd_syscalls[code].sc_name = "mkdir_extended";
		      break;
                    
		    case BSC_mkfifo:
		      bsd_syscalls[code].sc_name = "mkfifo";
		      break;

		    case BSC_mkfifo_extended:
		      bsd_syscalls[code].sc_name = "mkfifo_extended";
		      break;

		    case BSC_rmdir:
		      bsd_syscalls[code].sc_name = "rmdir";
		      break;

		    case BSC_statfs:
		      bsd_syscalls[code].sc_name = "statfs";
		      break;

		    case BSC_fstatfs:
		      bsd_syscalls[code].sc_name = "fstatfs";
		      bsd_syscalls[code].sc_format = FMT_FD;
		      break;

		    case BSC_pathconf:
		      bsd_syscalls[code].sc_name = "pathconf";
		      break;

		    case BSC_fpathconf:
		      bsd_syscalls[code].sc_name = "fpathconf";
		      bsd_syscalls[code].sc_format = FMT_FD;
		      break;

		    case BSC_getdirentries:
		      bsd_syscalls[code].sc_name = "getdirentries";
		      bsd_syscalls[code].sc_format = FMT_FD_IO;
		      break;

		    case BSC_lseek:
		      bsd_syscalls[code].sc_name = "lseek";
		      bsd_syscalls[code].sc_format = FMT_LSEEK;
		      break;

		    case BSC_truncate:
		      bsd_syscalls[code].sc_name = "truncate";
		      bsd_syscalls[code].sc_format = FMT_TRUNC;
		      break;

		    case BSC_ftruncate:
		      bsd_syscalls[code].sc_name = "ftruncate";
		      bsd_syscalls[code].sc_format = FMT_FTRUNC;
		      break;

		    case BSC_statv:
		      bsd_syscalls[code].sc_name = "statv";
		      break;

		    case BSC_fstatv:
		      bsd_syscalls[code].sc_name = "fstatv";
		      bsd_syscalls[code].sc_format = FMT_FD;
		      break;

		    case BSC_mkcomplex:
		      bsd_syscalls[code].sc_name = "mkcomplex";
		      break;

		    case BSC_getattrlist:
		      bsd_syscalls[code].sc_name = "getattrlist";
		      break;

		    case BSC_setattrlist:
		      bsd_syscalls[code].sc_name = "setattrlist";
		      break;

		    case BSC_getdirentriesattr:
		      bsd_syscalls[code].sc_name = "getdirentriesattr";
		      bsd_syscalls[code].sc_format = FMT_FD;
		      break;

		    case BSC_exchangedata:
		      bsd_syscalls[code].sc_name = "exchangedata";
		      break;
                    
		    case BSC_rename:
		      bsd_syscalls[code].sc_name = "rename";
		      break;

		    case BSC_copyfile:
		      bsd_syscalls[code].sc_name = "copyfile";
		      break;

		    case BSC_checkuseraccess:
		      bsd_syscalls[code].sc_name = "checkuseraccess";
		      break;

		    case BSC_searchfs:
		      bsd_syscalls[code].sc_name = "searchfs";
		      break;

		    case BSC_aio_fsync:
		      bsd_syscalls[code].sc_name = "aio_fsync";
		      bsd_syscalls[code].sc_format = FMT_AIO_FSYNC;
		      break;

		    case BSC_aio_return:
		      bsd_syscalls[code].sc_name = "aio_return";
		      bsd_syscalls[code].sc_format = FMT_AIO_RETURN;
		      break;

		    case BSC_aio_suspend:
		    case BSC_aio_suspend_nocancel:
		      bsd_syscalls[code].sc_name = "aio_suspend";
		      bsd_syscalls[code].sc_format = FMT_AIO_SUSPEND;
		      break;

		    case BSC_aio_cancel:
		      bsd_syscalls[code].sc_name = "aio_cancel";
		      bsd_syscalls[code].sc_format = FMT_AIO_CANCEL;
		      break;

		    case BSC_aio_error:
		      bsd_syscalls[code].sc_name = "aio_error";
		      bsd_syscalls[code].sc_format = FMT_AIO;
		      break;

		    case BSC_aio_read:
		      bsd_syscalls[code].sc_name = "aio_read";
		      bsd_syscalls[code].sc_format = FMT_AIO;
		      break;

		    case BSC_aio_write:
		      bsd_syscalls[code].sc_name = "aio_write";
		      bsd_syscalls[code].sc_format = FMT_AIO;
		      break;

		    case BSC_lio_listio:
		      bsd_syscalls[code].sc_name = "lio_listio";
		      bsd_syscalls[code].sc_format = FMT_LIO_LISTIO;
		      break;

		    case BSC_msync:
		    case BSC_msync_nocancel:
		      bsd_syscalls[code].sc_name = "msync";
		      bsd_syscalls[code].sc_format = FMT_MSYNC;
		      break;

		    case BSC_fcntl:
		    case BSC_fcntl_nocancel:
		      bsd_syscalls[code].sc_name = "fcntl";
		      bsd_syscalls[code].sc_format = FMT_FCNTL;
		      break;

		    case BSC_ioctl:
		      bsd_syscalls[code].sc_name = "ioctl";
		      bsd_syscalls[code].sc_format = FMT_IOCTL;
		      break;
		}
	}

	for (i = 0; (type = filemgr_call_types[i]); i++) {
	        char * p;

	        code = filemgr_index(type);

		if (code >= MAX_FILEMGR) {
		        printf("FILEMGR call init (%x):  type exceeds table size\n", type);
		        continue;
		}
		switch (type) {

		    case FILEMGR_PBGETCATALOGINFO:
		      p = "GetCatalogInfo";
		      break;

		    case FILEMGR_PBGETCATALOGINFOBULK:
		      p = "GetCatalogInfoBulk";
		      break;

		    case FILEMGR_PBCREATEFILEUNICODE:
		      p = "CreateFileUnicode";
		      break;

		    case FILEMGR_PBCREATEDIRECTORYUNICODE:
		      p = "CreateDirectoryUnicode";
		      break;

		    case FILEMGR_PBCREATEFORK:
		      p = "PBCreateFork";
		      break;

		    case FILEMGR_PBDELETEFORK:
		      p = "PBDeleteFork";
		      break;

		    case FILEMGR_PBITERATEFORK:
		      p = "PBIterateFork";
		      break;

		    case FILEMGR_PBOPENFORK:
		      p = "PBOpenFork";
		      break;

		    case FILEMGR_PBREADFORK:
		      p = "PBReadFork";
		      break;

		    case FILEMGR_PBWRITEFORK:
		      p = "PBWriteFork";
		      break;

		    case FILEMGR_PBALLOCATEFORK:
		      p = "PBAllocateFork";
		      break;

		    case FILEMGR_PBDELETEOBJECT:
		      p = "PBDeleteObject";
		      break;

		    case FILEMGR_PBEXCHANGEOBJECT:
		      p = "PBExchangeObject";
		      break;

		    case FILEMGR_PBGETFORKCBINFO:
		      p = "PBGetForkCBInfo";
		      break;

		    case FILEMGR_PBGETVOLUMEINFO:
		      p = "PBGetVolumeInfo";
		      break;
		
		    case FILEMGR_PBMAKEFSREF:
		      p = "PBMakeFSRef";
		      break;

		    case FILEMGR_PBMAKEFSREFUNICODE:
		      p = "PBMakeFSRefUnicode";
		      break;

		    case FILEMGR_PBMOVEOBJECT:
		      p = "PBMoveObject";
		      break;

		    case FILEMGR_PBOPENITERATOR:
		      p = "PBOpenIterator";
		      break;

		    case FILEMGR_PBRENAMEUNICODE:
		      p = "PBRenameUnicode";
		      break;

		    case FILEMGR_PBSETCATALOGINFO:
		      p = "SetCatalogInfo";
		      break;

		    case FILEMGR_PBSETVOLUMEINFO:
		      p = "SetVolumeInfo";
		      break;

		    case FILEMGR_FSREFMAKEPATH:
		      p = "FSRefMakePath";
		      break;

		    case FILEMGR_FSPATHMAKEREF:
		      p = "FSPathMakeRef";
		      break;

		    case FILEMGR_PBGETCATINFO:
		      p = "GetCatInfo";
		      break;

		    case FILEMGR_PBGETCATINFOLITE:
		      p = "GetCatInfoLite";
		      break;

		    case FILEMGR_PBHGETFINFO:
		      p = "PBHGetFInfo";
		      break;

		    case FILEMGR_PBXGETVOLINFO:
		      p = "PBXGetVolInfo";
		      break;

		    case FILEMGR_PBHCREATE:
		      p = "PBHCreate";
		      break;

		    case FILEMGR_PBHOPENDF:
		      p = "PBHOpenDF";
		      break;

		    case FILEMGR_PBHOPENRF:
		      p = "PBHOpenRF";
		      break;

		    case FILEMGR_PBHGETDIRACCESS:
		      p = "PBHGetDirAccess";
		      break;

		    case FILEMGR_PBHSETDIRACCESS:
		      p = "PBHSetDirAccess";
		      break;

		    case FILEMGR_PBHMAPID:
		      p = "PBHMapID";
		      break;

		    case FILEMGR_PBHMAPNAME:
		      p = "PBHMapName";
		      break;

		    case FILEMGR_PBCLOSE:
		      p = "PBClose";
		      break;

		    case FILEMGR_PBFLUSHFILE:
		      p = "PBFlushFile";
		      break;

		    case FILEMGR_PBGETEOF:
		      p = "PBGetEOF";
		      break;

		    case FILEMGR_PBSETEOF:
		      p = "PBSetEOF";
		      break;

		    case FILEMGR_PBGETFPOS:
		      p = "PBGetFPos";
		      break;

		    case FILEMGR_PBREAD:
		      p = "PBRead";
		      break;

		    case FILEMGR_PBWRITE:
		      p = "PBWrite";
		      break;

		    case FILEMGR_PBGETFCBINFO:
		      p = "PBGetFCBInfo";
		      break;

		    case FILEMGR_PBSETFINFO:
		      p = "PBSetFInfo";
		      break;

		    case FILEMGR_PBALLOCATE:
		      p = "PBAllocate";
		      break;

		    case FILEMGR_PBALLOCCONTIG:
		      p = "PBAllocContig";
		      break;

		    case FILEMGR_PBSETFPOS:
		      p = "PBSetFPos";
		      break;

		    case FILEMGR_PBSETCATINFO:
		      p = "PBSetCatInfo";
		      break;

		    case FILEMGR_PBGETVOLPARMS:
		      p = "PBGetVolParms";
		      break;

		    case FILEMGR_PBSETVINFO:
		      p = "PBSetVInfo";
		      break;

		    case FILEMGR_PBMAKEFSSPEC:
		      p = "PBMakeFSSpec";
		      break;

		    case FILEMGR_PBHGETVINFO:
		      p = "PBHGetVInfo";
		      break;

		    case FILEMGR_PBCREATEFILEIDREF:
		      p = "PBCreateFileIDRef";
		      break;

		    case FILEMGR_PBDELETEFILEIDREF:
		      p = "PBDeleteFileIDRef";
		      break;

		    case FILEMGR_PBRESOLVEFILEIDREF:
		      p = "PBResolveFileIDRef";
		      break;

		    case FILEMGR_PBFLUSHVOL:
		      p = "PBFlushVol";
		      break;

		    case FILEMGR_PBHRENAME:
		      p = "PBHRename";
		      break;

		    case FILEMGR_PBCATMOVE:
		      p = "PBCatMove";
		      break;

		    case FILEMGR_PBEXCHANGEFILES:
		      p = "PBExchangeFiles";
		      break;

		    case FILEMGR_PBHDELETE:
		      p = "PBHDelete";
		      break;

		    case FILEMGR_PBDIRCREATE:
		      p = "PBDirCreate";
		      break;

		    case FILEMGR_PBCATSEARCH:
		      p = "PBCatSearch";
		      break;

		    case FILEMGR_PBHSETFLOCK:
		      p = "PBHSetFlock";
		      break;

		    case FILEMGR_PBHRSTFLOCK:
		      p = "PBHRstFLock";
		      break;

		    case FILEMGR_PBLOCKRANGE:
		      p = "PBLockRange";
		      break;

		    case FILEMGR_PBUNLOCKRANGE:
		      p = "PBUnlockRange";
		      break;

		    default:
		      p = NULL;
		      break;
		}
		filemgr_calls[code].fm_name = p;
	}
}



int
main(argc, argv)
	int	argc;
	char	*argv[];
{
	char	*myname = "fs_usage";
	int     i;
	char    ch;
	struct sigaction osa;
	void getdivisor();
	void argtopid();
	void set_remove();
	void set_pidcheck();
	void set_pidexclude();
	int quit();

        if ( geteuid() != 0 ) {
            fprintf(stderr, "'fs_usage' must be run as root...\n");
            exit(1);
        }
	get_screenwidth();

	/* get our name */
	if (argc > 0) {
		if ((myname = rindex(argv[0], '/')) == 0) {
			myname = argv[0];
		}
		else {
			myname++;
		}
	}
	
       while ((ch = getopt(argc, argv, "ewf:")) != EOF) {
               switch(ch) {
                case 'e':
		    exclude_pids = 1;
		    exclude_default_pids = 0;
		    break;
                case 'w':
		    wideflag = 1;
		    if ((uint)columns < MAX_WIDE_MODE_COLS)
		      columns = MAX_WIDE_MODE_COLS;
		    break;
	       case 'f':
		   if (!strcmp(optarg, "network"))
		       filter_mode |= NETWORK_FILTER;
		   else if (!strcmp(optarg, "filesys"))
		       filter_mode |= FILESYS_FILTER;
		   else if (!strcmp(optarg, "cachehit"))
		       filter_mode &= ~CACHEHIT_FILTER;   /* turns on CACHE_HIT */
		   else if (!strcmp(optarg, "exec"))
		       filter_mode |= EXEC_FILTER;
		   else if (!strcmp(optarg, "pathname"))
		       filter_mode |= PATHNAME_FILTER;
		   break;
		       
	       default:
		 exit_usage(myname);		 
	       }
       }

        argc -= optind;
        argv += optind;

	/*
	 * when excluding, fs_usage should be the first in line for pids[]
	 * 
	 * the !exclude_pids && argc == 0 catches the exclude_default_pids
	 * case below where exclude_pids is later set and the fs_usage PID
	 * needs to make it into pids[] 
	 */
	if (exclude_pids || (!exclude_pids && argc == 0))
	  {
	    if (num_of_pids < (MAX_PIDS - 1))
	        pids[num_of_pids++] = getpid();
	  }

	/* If we process any list of pids/cmds, then turn off the defaults */
	if (argc > 0)
	  exclude_default_pids = 0;

	while (argc > 0 && num_of_pids < (MAX_PIDS - 1)) {
	  select_pid_mode++;
	  argtopid(argv[0]);
	  argc--;
	  argv++;
	}

	/* Exclude a set of default pids */
	if (exclude_default_pids)
	  {
	    argtopid("Terminal");
	    argtopid("telnetd");
	    argtopid("telnet");
	    argtopid("sshd");
	    argtopid("rlogind");
	    argtopid("tcsh");
	    argtopid("csh");
	    argtopid("sh");
	    exclude_pids = 1;
	  }

#if 0
	for (i = 0; i < num_of_pids; i++)
	  {
	    if (exclude_pids)
	      fprintf(stderr, "exclude pid %d\n", pids[i]);
	    else
	      fprintf(stderr, "pid %d\n", pids[i]);
	  }
#endif

	/* set up signal handlers */
	signal(SIGINT, leave);
	signal(SIGQUIT, leave);

	sigaction(SIGHUP, (struct sigaction *)NULL, &osa);

	if (osa.sa_handler == SIG_DFL)
	        signal(SIGHUP, leave);
	signal(SIGTERM, leave);
	signal(SIGWINCH, sigwinch);

	/* grab the number of cpus */
	size_t len;
	mib[0] = CTL_HW;
	mib[1] = HW_NCPU;
	mib[2] = 0;
	len = sizeof(num_cpus);
	sysctl(mib, 2, &num_cpus, &len, NULL, 0);
	num_events = EVENT_BASE * num_cpus;

	if ((my_buffer = malloc(num_events * sizeof(kd_buf))) == (char *)0)
	    quit("can't allocate memory for tracing info\n");

	if (ReadSharedCacheMap("/var/db/dyld/dyld_shared_cache_i386.map", &framework32))
		ReadSharedCacheMap("/var/db/dyld/dyld_shared_cache_x86_64.map", &framework64);
	else {
		ReadSharedCacheMap("/var/db/dyld/dyld_shared_cache_ppc.map", &framework32);
		ReadSharedCacheMap("/var/db/dyld/dyld_shared_cache_ppc64.map", &framework64);
	}
	SortFrameworkAddresses();

        cache_disk_names();

	set_remove();
	set_numbufs(num_events);
	set_init();

	if (exclude_pids == 0) {
	        for (i = 0; i < num_of_pids; i++)
		        set_pidcheck(pids[i], 1);
	} else {
	        for (i = 0; i < num_of_pids; i++)
		        set_pidexclude(pids[i], 1);
	}

	if (select_pid_mode && !one_good_pid)
	  {
	    /* 
	       An attempt to restrict output to a given
	       pid or command has failed. Exit gracefully
	    */
	    set_remove();
	    exit_usage(myname);
	  }

	set_enable(1);
	getdivisor();
	init_arguments_buffer();

	init_tables();

	/* main loop */

	while (1) {
	        usleep(1000 * usleep_ms);

		sample_sc();

		last_time = time((long *)0);
	}
}

void
find_proc_names()
{
        size_t			bufSize = 0;
	struct kinfo_proc       *kp;
	int quit();

	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC;
	mib[2] = KERN_PROC_ALL;
	mib[3] = 0;

	if (sysctl(mib, 4, NULL, &bufSize, NULL, 0) < 0)
		quit("trace facility failure, KERN_PROC_ALL\n");

	if((kp = (struct kinfo_proc *)malloc(bufSize)) == (struct kinfo_proc *)0)
	    quit("can't allocate memory for proc buffer\n");
	
	if (sysctl(mib, 4, kp, &bufSize, NULL, 0) < 0)
		quit("trace facility failure, KERN_PROC_ALL\n");

        kp_nentries = bufSize/ sizeof(struct kinfo_proc);
	kp_buffer = kp;
}


void destroy_thread(struct th_info *ti) {

	ti->child_thread = 0;
	ti->thread = 0;
	ti->pid = 0;

	if (ti->my_index < cur_start)
	        cur_start = ti->my_index;

        if (ti->my_index == cur_max) {
	        while (ti >= &th_state[0]) {
		        if (ti->thread)
			        break;
			ti--;
		}
		cur_max = ti->my_index;
	}
}


struct th_info *find_empty(void) {
        struct th_info *ti;

	for (ti = &th_state[cur_start]; ti < &th_state[MAX_THREADS]; ti++, cur_start++) {
	        if (ti->thread == 0) {
		        if (cur_start > cur_max)
			        cur_max = cur_start;
			cur_start++;

			return (ti);
		}
	}
	return ((struct th_info *)0);

}


struct th_info *find_thread(int thread, int type) {
        struct th_info *ti;

	for (ti = &th_state[0]; ti <= &th_state[cur_max]; ti++) {
	        if (ti->thread == thread) {
		        if (type == ti->type)
			        return(ti);
			if (ti->in_filemgr) {
			        if (type == -1)
				        return(ti);
				continue;
			}
			if (type == 0)
			        return(ti);
		}
	}
	return ((struct th_info *)0);
}


void
mark_thread_waited(int thread) {
       struct th_info *ti;

       for (ti = th_state; ti <= &th_state[cur_max]; ti++) {
	       if (ti->thread == thread) {
		       ti->waited = 1;
	       }
       }
}


void
set_enable(int val) 
{
	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDENABLE;		/* protocol */
	mib[3] = val;
	mib[4] = 0;
	mib[5] = 0;		        /* no flags */
	if (sysctl(mib, 4, NULL, &needed, NULL, 0) < 0)
		quit("trace facility failure, KERN_KDENABLE\n");

	if (val)
	  trace_enabled = 1;
	else
	  trace_enabled = 0;
}

void
set_numbufs(int nbufs) 
{
	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDSETBUF;
	mib[3] = nbufs;
	mib[4] = 0;
	mib[5] = 0;		        /* no flags */
	if (sysctl(mib, 4, NULL, &needed, NULL, 0) < 0)
		quit("trace facility failure, KERN_KDSETBUF\n");

	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDSETUP;		
	mib[3] = 0;
	mib[4] = 0;
	mib[5] = 0;		        /* no flags */
	if (sysctl(mib, 3, NULL, &needed, NULL, 0) < 0)
		quit("trace facility failure, KERN_KDSETUP\n");
}

void
set_pidcheck(int pid, int on_off) 
{
        kd_regtype kr;

	kr.type = KDBG_TYPENONE;
	kr.value1 = pid;
	kr.value2 = on_off;
	needed = sizeof(kd_regtype);
	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDPIDTR;
	mib[3] = 0;
	mib[4] = 0;
	mib[5] = 0;

	if (sysctl(mib, 3, &kr, &needed, NULL, 0) < 0) {
	        if (on_off == 1)
		        fprintf(stderr, "pid %d does not exist\n", pid);
	}
	else {
	  one_good_pid++;
	}
}

/* 
   on_off == 0 turns off pid exclusion
   on_off == 1 turns on pid exclusion
*/
void
set_pidexclude(int pid, int on_off) 
{
        kd_regtype kr;

	one_good_pid++;

	kr.type = KDBG_TYPENONE;
	kr.value1 = pid;
	kr.value2 = on_off;
	needed = sizeof(kd_regtype);
	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDPIDEX;
	mib[3] = 0;
	mib[4] = 0;
	mib[5] = 0;

	if (sysctl(mib, 3, &kr, &needed, NULL, 0) < 0) {
	        if (on_off == 1)
		          fprintf(stderr, "pid %d does not exist\n", pid);
	}
}

void
get_bufinfo(kbufinfo_t *val)
{
        needed = sizeof (*val);
	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDGETBUF;		
	mib[3] = 0;
	mib[4] = 0;
	mib[5] = 0;		/* no flags */

	if (sysctl(mib, 3, val, &needed, 0, 0) < 0)
		quit("trace facility failure, KERN_KDGETBUF\n");  

}

void
set_remove() 
{
        errno = 0;

	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDREMOVE;		/* protocol */
	mib[3] = 0;
	mib[4] = 0;
	mib[5] = 0;		/* no flags */
	if (sysctl(mib, 3, NULL, &needed, NULL, 0) < 0)
	  {
	    set_remove_flag = 0;

	    if (errno == EBUSY)
		quit("the trace facility is currently in use...\n          fs_usage, sc_usage, and latency use this feature.\n\n");
	    else
		quit("trace facility failure, KERN_KDREMOVE\n");
	  }
}

void
set_init() 
{       kd_regtype kr;

	kr.type = KDBG_RANGETYPE;
	kr.value1 = 0;
	kr.value2 = -1;
	needed = sizeof(kd_regtype);
	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDSETREG;		
	mib[3] = 0;
	mib[4] = 0;
	mib[5] = 0;		/* no flags */

	if (sysctl(mib, 3, &kr, &needed, NULL, 0) < 0)
		quit("trace facility failure, KERN_KDSETREG\n");

	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDSETUP;		
	mib[3] = 0;
	mib[4] = 0;
	mib[5] = 0;		/* no flags */

	if (sysctl(mib, 3, NULL, &needed, NULL, 0) < 0)
		quit("trace facility failure, KERN_KDSETUP\n");
}

void
sample_sc()
{
	kd_buf *kd;
	int i, count;
	size_t needed;
	void read_command_map();
	void create_map_entry();

        /* Get kernel buffer information */
	get_bufinfo(&bufinfo);

	if (need_new_map) {
	        read_command_map();
	        need_new_map = 0;
	}
	needed = bufinfo.nkdbufs * sizeof(kd_buf);
	mib[0] = CTL_KERN;
	mib[1] = KERN_KDEBUG;
	mib[2] = KERN_KDREADTR;		
	mib[3] = 0;
	mib[4] = 0;
	mib[5] = 0;		/* no flags */

	if (sysctl(mib, 3, my_buffer, &needed, NULL, 0) < 0)
                quit("trace facility failure, KERN_KDREADTR\n");
	count = needed;

	if (count > (SAMPLE_SIZE / 8)) {
	        if (usleep_ms > USLEEP_BEHIND)
		        usleep_ms = USLEEP_BEHIND;
	        else if (usleep_ms > USLEEP_MIN)
	                usleep_ms /= 2;

	} else	if (count < (SAMPLE_SIZE / 16)) {
	        if (usleep_ms < USLEEP_MAX)
	                usleep_ms *= 2;
	}

	if (bufinfo.flags & KDBG_WRAPPED) {
	        fprintf(stderr, "fs_usage: buffer overrun, events generated too quickly: %d\n", count);

	        for (i = 0; i <= cur_max; i++) {
			th_state[i].thread = 0;
			th_state[i].pid = 0;
			th_state[i].pathptr = (long *)NULL;
			th_state[i].pathname[0] = 0;
		}
		cur_max = 0;
		cur_start = 0;
		need_new_map = 1;
		map_is_the_same = 0;
		
		set_enable(0);
		set_enable(1);
	}
	kd = (kd_buf *)my_buffer;
#if 0
	fprintf(stderr, "READTR returned %d items\n", count);
#endif
	for (i = 0; i < count; i++) {
	        int debugid, thread;
		int type;
	        int index;
		long *sargptr;
		uint64_t now;
		long long l_usecs;
		int secs;
		long curr_time;
		struct th_info *ti;
                struct diskio  *dio;


		thread  = kd[i].arg5;
		debugid = kd[i].debugid;
		type    = kd[i].debugid & DBG_FUNC_MASK;

                now = kd[i].timestamp & KDBG_TIMESTAMP_MASK;

		if (i == 0)
		{
		    curr_time = time((long *)0);
		    /*
		     * Compute bias seconds after each trace buffer read.
		     * This helps resync timestamps with the system clock
		     * in the event of a system sleep.
		     */
		    if (bias_secs == 0 || curr_time < last_time || curr_time > (last_time + 2)) {
		            l_usecs = (long long)(now / divisor);
			    secs = l_usecs / 1000000;
			    bias_secs = curr_time - secs;
		    }
		}
		
		if ((type & P_DISKIO_MASK) == P_DISKIO) {
		        insert_diskio(type, kd[i].arg1, kd[i].arg2, kd[i].arg3, kd[i].arg4, thread, (double)now);
			continue;
		}
		if ((type & P_DISKIO_MASK) == P_DISKIO_DONE) {
		        if ((dio = complete_diskio(kd[i].arg1, kd[i].arg4, kd[i].arg3, thread, (double)now))) {
			        print_diskio(dio);
				free_diskio(dio);
			}
			continue;
		}

		switch (type) {

		case TRACE_DATA_NEWTHREAD:
		    
		    if ((ti = find_empty()) == NULL)
		            continue;

		    ti->thread = thread;
		    ti->child_thread = kd[i].arg1;
		    ti->pid = kd[i].arg2;
		    continue;

		case TRACE_STRING_NEWTHREAD:
		    if ((ti = find_thread(thread, 0)) == (struct th_info *)0)
		            continue;
		    if (ti->child_thread == 0)
		            continue;
		    create_map_entry(ti->child_thread, ti->pid, (char *)&kd[i].arg1);

		    destroy_thread(ti);

		    continue;
	
		case TRACE_DATA_EXEC:

		    if ((ti = find_empty()) == NULL)
		            continue;

		    ti->thread = thread;
		    ti->pid = kd[i].arg1;
		    continue;	    

		case TRACE_STRING_EXEC:
		    if ((ti = find_thread(thread, 0)) == (struct th_info *)0)
		    {
			/* this is for backwards compatibility */
			create_map_entry(thread, 0, (char *)&kd[i].arg1);
		    }
		    else
		    {
			create_map_entry(thread, ti->pid, (char *)&kd[i].arg1);

			destroy_thread(ti);
		    }
		    continue;

		case BSC_exit:
		    kill_thread_map(thread);
		    continue;

		case BSC_mmap:
		    if (kd[i].arg4 & MAP_ANON)
		        continue;
		    break;

		case MACH_sched:
		case MACH_stkhandoff:
                    mark_thread_waited(thread);
		    continue;

		case VFS_LOOKUP:
		    if ((ti = find_thread(thread, 0)) == (struct th_info *)0)
		            continue;

		    if (ti->pathptr == NULL) {
			    sargptr = ti->pathname;
			    *sargptr++ = kd[i].arg2;
			    *sargptr++ = kd[i].arg3;
			    *sargptr++ = kd[i].arg4;
			    /*
			     * NULL terminate the 'string'
			     */
			    *sargptr = 0;
			    ti->pathptr = sargptr;
		    } else {
		            sargptr = ti->pathptr;

			    /* 
			     * We don't want to overrun our pathname buffer if the
			     * kernel sends us more VFS_LOOKUP entries than we can
			     *  handle.
			     */
			    if (sargptr >= &ti->pathname[NUMPARMS]) {
				continue;
			    }

                            /*
			     * We need to detect consecutive vfslookup entries.
			     * So, if we get here and find a START entry,
			     * fake the pathptr so we can bypass all further
			     * vfslookup entries.
			     */
			    if (debugid & DBG_FUNC_START) {
				ti->pathptr = &ti->pathname[NUMPARMS];
				continue;
			    }
			    *sargptr++ = kd[i].arg1;
			    *sargptr++ = kd[i].arg2;
			    *sargptr++ = kd[i].arg3;
			    *sargptr++ = kd[i].arg4;
			    /*
			     * NULL terminate the 'string'
			     */
			    *sargptr = 0;
			    ti->pathptr = sargptr;
		    }
		    continue;
		}

		if (debugid & DBG_FUNC_START) {
		        char * p;

			if ((type & CLASS_MASK) == FILEMGR_BASE) {

				index = filemgr_index(type);

				if (index >= MAX_FILEMGR)
				        continue;

				if ((p = filemgr_calls[index].fm_name) == NULL)
				        continue;
			} else
			        p = NULL;

			enter_syscall(thread, type, &kd[i], p, (double)now);
			continue;
		}

		switch (type) {

		    case MACH_pageout:
		      if (kd[i].arg2) 
			      exit_syscall("PAGE_OUT_D", thread, type, 0, kd[i].arg1, 0, 0, FMT_PGOUT, (double)now);
		      else
			      exit_syscall("PAGE_OUT_V", thread, type, 0, kd[i].arg1, 0, 0, FMT_PGOUT, (double)now);
		      continue;

		    case MACH_vmfault:
		      if (kd[i].arg4 == DBG_PAGEIN_FAULT)
			      exit_syscall("PAGE_IN", thread, type, 0, kd[i].arg1, kd[i].arg2, 0, FMT_PGIN, (double)now);
		      else if (kd[i].arg4 == DBG_CACHE_HIT_FAULT)
			      exit_syscall("CACHE_HIT", thread, type, 0, kd[i].arg1, kd[i].arg2, 0, FMT_CACHEHIT, (double)now);
		      else {
			      if ((ti = find_thread(thread, type))) {
				      destroy_thread(ti);
			      }
		      }
		      continue;

		    case MSC_map_fd:
		      exit_syscall("map_fd", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, FMT_FD, (double)now);
		      continue;
		      
		    case BSC_mmap_extended:
		    case BSC_mmap_extended2:
		    case BSC_msync_extended:
		    case BSC_pread_extended:
		    case BSC_pwrite_extended:
		      extend_syscall(thread, type, &kd[i]);
		      continue;
		}

		if ((type & CSC_MASK) == BSC_BASE) {

		        index = BSC_INDEX(type);

			if (index >= MAX_BSD_SYSCALL)
			        continue;

			if (bsd_syscalls[index].sc_name == NULL)
			        continue;

			if (type == BSC_execve)
			        execs_in_progress--;

			exit_syscall(bsd_syscalls[index].sc_name, thread, type, kd[i].arg1, kd[i].arg2, kd[i].arg3, kd[i].arg4,
				     bsd_syscalls[index].sc_format, (double)now);

			continue;
		}

		if ((type & CLASS_MASK) == FILEMGR_BASE) {
		
			index = filemgr_index(type);

			if (index >= MAX_FILEMGR)
			        continue;

			if (filemgr_calls[index].fm_name == NULL)
			        continue;

			exit_syscall(filemgr_calls[index].fm_name, thread, type, kd[i].arg1, kd[i].arg2, kd[i].arg3, kd[i].arg4,
				     FMT_DEFAULT, (double)now);
		}
	}
	fflush(0);
}




void
enter_syscall_now(int thread, int type, kd_buf *kd, char *name, double now)
{
       struct th_info *ti;
       int    secs;
       int    usecs;
       long long l_usecs;
       long curr_time;
       kd_threadmap *map;
       kd_threadmap *find_thread_map();
       int clen = 0;
       int tsclen = 0;
       int nmclen = 0;
       int argsclen = 0;
       char buf[MAXWIDTH];

       if (execs_in_progress) {
	       if ((ti = find_thread(thread, BSC_execve))) {
		       if (ti->pathptr) {
			       exit_syscall("execve", thread, BSC_execve, 0, 0, 0, 0, FMT_DEFAULT, (double)now);
			       execs_in_progress--;
		       }
	       }
       }
       if ((ti = find_empty()) == NULL)
	       return;
                   
       if ((type & CLASS_MASK) == FILEMGR_BASE) {

	       filemgr_in_progress++;
	       ti->in_filemgr = 1;

	       l_usecs = (long long)(now / divisor);
	       secs = l_usecs / 1000000;
	       curr_time = bias_secs + secs;
		   
	       sprintf(buf, "%-8.8s", &(ctime(&curr_time)[11]));
	       tsclen = strlen(buf);

	       if (columns > MAXCOLS || wideflag) {
		       usecs = l_usecs - (long long)((long long)secs * 1000000);
		       sprintf(&buf[tsclen], ".%03ld", (long)usecs / 1000);
		       tsclen = strlen(buf);
	       }

	       /*
		* Print timestamp column
		*/
	       printf("%s", buf);

	       map = find_thread_map(thread);
	       if (map) {
		       sprintf(buf, "  %-25.25s ", name);
		       nmclen = strlen(buf);
		       printf("%s", buf);

		       sprintf(buf, "(%d, 0x%x, 0x%x, 0x%x)", (short)kd->arg1, kd->arg2, kd->arg3, kd->arg4);
		       argsclen = strlen(buf);
		       
		       /*
			* Calculate white space out to command
			*/
		       if (columns > MAXCOLS || wideflag) {
			       clen = columns - (tsclen + nmclen + argsclen + 20);
		       } else
			       clen = columns - (tsclen + nmclen + argsclen + 12);

		       if (clen > 0) {
			       printf("%s", buf);   /* print the kdargs */
			       memset(buf, ' ', clen);
			       buf[clen] = '\0';
			       printf("%s", buf);
		       }
		       else if ((argsclen + clen) > 0) {
			       /*
				* no room so wipe out the kdargs
				*/
			       memset(buf, ' ', (argsclen + clen));
			       buf[argsclen + clen] = '\0';
			       printf("%s", buf);
		       }
		       if (columns > MAXCOLS || wideflag)
			       printf("%-20.20s\n", map->command); 
		       else
			       printf("%-12.12s\n", map->command); 
	       } else
		       printf("  %-24.24s (%5d, %#x, 0x%x, 0x%x)\n",         name, (short)kd->arg1, kd->arg2, kd->arg3, kd->arg4);
       }
       ti->thread = thread;
       ti->waited = 0;
       ti->type   = type;
       ti->stime  = now;
       ti->arg1   = kd->arg1;
       ti->arg2   = kd->arg2;
       ti->arg3   = kd->arg3;
       ti->arg4   = kd->arg4;
       ti->pathptr = (long *)NULL;
       ti->pathname[0] = 0;
}


void
enter_syscall(int thread, int type, kd_buf *kd, char *name, double now)
{
       int index;

       if (type == MACH_pageout || type == MACH_vmfault || type == MSC_map_fd) {
	       enter_syscall_now(thread, type, kd, name, now);
	       return;
       }
       if ((type & CSC_MASK) == BSC_BASE) {

	       index = BSC_INDEX(type);

	       if (index >= MAX_BSD_SYSCALL)
		       return;

	       if (type == BSC_execve)
		       execs_in_progress++;

	       if (bsd_syscalls[index].sc_name)
		       enter_syscall_now(thread, type, kd, name, now);

	       return;
       }
       if ((type & CLASS_MASK) == FILEMGR_BASE) {

	       index = filemgr_index(type);

	       if (index >= MAX_FILEMGR)
		       return;
	       
	       if (filemgr_calls[index].fm_name)
		       enter_syscall_now(thread, type, kd, name, now);
       }
}

/*
 * Handle system call extended trace data.
 * pread and pwrite:
 *     Wipe out the kd args that were collected upon syscall_entry
 *     because it is the extended info that we really want, and it
 *     is all we really need.
*/

void
extend_syscall(int thread, int type, kd_buf *kd)
{
       struct th_info *ti;

       switch (type) {
       case BSC_mmap_extended:
	   if ((ti = find_thread(thread, BSC_mmap)) == (struct th_info *)0)
		   return;
	   ti->arg8   = ti->arg3;  /* save protection */
	   ti->arg1   = kd->arg1;  /* the fd */
	   ti->arg3   = kd->arg2;  /* bottom half address */
	   ti->arg5   = kd->arg3;  /* bottom half size */	   
	   break;
       case BSC_mmap_extended2:
	   if ((ti = find_thread(thread, BSC_mmap)) == (struct th_info *)0)
		   return;
	   ti->arg2   = kd->arg1;  /* top half address */
	   ti->arg4   = kd->arg2;  /* top half size */
	   ti->arg6   = kd->arg3;  /* top half file offset */
	   ti->arg7   = kd->arg4;  /* bottom half file offset */
	   break;
       case BSC_msync_extended:
	   if ((ti = find_thread(thread, BSC_msync)) == (struct th_info *)0) {
	       if ((ti = find_thread(thread, BSC_msync_nocancel)) == (struct th_info *)0)
		   return;
	   }
	   ti->arg4   = kd->arg1;  /* top half address */
	   ti->arg5   = kd->arg2;  /* top half size */
	   break;
       case BSC_pread_extended:
	   if ((ti = find_thread(thread, BSC_pread)) == (struct th_info *)0) {
	       if ((ti = find_thread(thread, BSC_pread_nocancel)) == (struct th_info *)0)
		   return;
	   }
	   ti->arg1   = kd->arg1;  /* the fd */
	   ti->arg2   = kd->arg2;  /* nbytes */
	   ti->arg3   = kd->arg3;  /* top half offset */
	   ti->arg4   = kd->arg4;  /* bottom half offset */	   
	   break;
       case BSC_pwrite_extended:
	   if ((ti = find_thread(thread, BSC_pwrite)) == (struct th_info *)0) {
	       if ((ti = find_thread(thread, BSC_pwrite_nocancel)) == (struct th_info *)0)
		   return;
	   }
	   ti->arg1   = kd->arg1;  /* the fd */
	   ti->arg2   = kd->arg2;  /* nbytes */
	   ti->arg3   = kd->arg3;  /* top half offset */
	   ti->arg4   = kd->arg4;  /* bottom half offset */
	   break;
       default:
	   return;
       }
}


void
exit_syscall(char *sc_name, int thread, int type, int arg1, int arg2, int arg3, int arg4,
	                    int format, double now)
{
        struct th_info *ti;
      
	if ((ti = find_thread(thread, type)) == (struct th_info *)0)
	        return;

	if (check_filter_mode(ti, type, arg1, arg2, sc_name))
	        format_print(ti, sc_name, thread, type, arg1, arg2, arg3, arg4, format, now, ti->stime, ti->waited, (char *)ti->pathname, NULL);

	if ((type & CLASS_MASK) == FILEMGR_BASE) {
	        ti->in_filemgr = 0;

		if (filemgr_in_progress > 0)
		        filemgr_in_progress--;
	}
	destroy_thread(ti);
}


void
get_mode_nibble(char * buf, int smode, int special, char x_on, char x_off)
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


void
get_mode_string(int mode, char *buf)
{
        memset(buf, '-', 9);
	buf[9] = '\0';

	get_mode_nibble(&buf[6], mode, (mode & 01000), 't', 'T');
	get_mode_nibble(&buf[3], (mode>>3), (mode & 02000), 's', 'S');
	get_mode_nibble(&buf[0], (mode>>6), (mode & 04000), 's', 'S');
}


int clip_64bit(char *s, uint64_t value)
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
	
	return (clen);
}


void
format_print(struct th_info *ti, char *sc_name, int thread, int type, int arg1, int arg2, int arg3, int arg4,
	     int format, double now, double stime, int waited, char *pathname, struct diskio *dio)
{
        int secs;
	int usecs;
	int nopadding = 0;
	long long l_usecs;
	long curr_time;
	char *command_name;
	kd_threadmap *map;
	kd_threadmap *find_thread_map();
	int in_filemgr = 0;
	int len = 0;
	int clen = 0;
	int tlen = 0;
	int class;
	uint64_t user_addr;
	uint64_t user_size;
	char *framework_name;
	char *p1;
	char *p2;
	char buf[MAXWIDTH];
	command_name = "";
	int	need_msec_update = 0;
	static char timestamp[32];
	static int  last_timestamp = 0;
	static int  timestamp_len = 0;
	static int  last_msec = 0;


	class = type >> 24;

	if (dio)
	        command_name = dio->issuing_command;
	else {
	        if (map_is_the_same && thread == last_thread)
		        map = last_map;
		else {
		        if ((map = find_thread_map(thread))) {
				last_map = map;
				last_thread = thread;
				map_is_the_same = 1;
			}
		}
		if (map)
		        command_name = map->command;
	}
	l_usecs = (long long)(now / divisor);
	secs = l_usecs / 1000000;
	curr_time = bias_secs + secs;

	if (last_timestamp != curr_time) {
	        timestamp_len = sprintf(timestamp, "%-8.8s", &(ctime(&curr_time)[11]));
		last_timestamp = curr_time;
		need_msec_update = 1;
	}
	if (columns > MAXCOLS || wideflag) {
	        int msec;

		tlen = timestamp_len;
		nopadding = 0;
		msec = (l_usecs - (long long)((long long)secs * 1000000)) / 1000;

		if (msec != last_msec || need_msec_update) {
		        sprintf(&timestamp[tlen], ".%03ld", (long)msec);
			last_msec = msec;
		}
		tlen += 4;

		timestamp[tlen] = '\0';

		if (filemgr_in_progress) {
		        if (class != FILEMGR_CLASS) {
			        if (find_thread(thread, -1)) {
				        in_filemgr = 1;
				}
			}
		}
	} else
	        nopadding = 1;

	if ((class == FILEMGR_CLASS) && (columns > MAXCOLS || wideflag))
	        clen = printf("%s  %-20.20s", timestamp, sc_name);
	else if (in_filemgr)
	        clen = printf("%s    %-15.15s", timestamp, sc_name);
	else
	        clen = printf("%s  %-17.17s", timestamp, sc_name);
       
	framework_name = (char *)0;

	if (columns > MAXCOLS || wideflag) {

	        off_t offset_reassembled = 0LL;
		
		switch (format) {

		      case FMT_DEFAULT:
			/*
			 * pathname based system calls or 
			 * calls with no fd or pathname (i.e.  sync)
			 */
			if (arg1)
			        clen += printf("      [%3d]       ", arg1);
			else
			        clen += printf("                  ");
			break;

		      case FMT_FD:
			/*
			 * fd based system call... no I/O
			 */
			if (arg1)
			        clen += printf(" F=%-3d[%3d]", ti->arg1, arg1);
			else
			        clen += printf(" F=%-3d", ti->arg1);
			break;

		      case FMT_FD_2:
			/*
			 * accept, dup, dup2
			 */
			if (arg1)
			        clen += printf(" F=%-3d[%3d]", ti->arg1, arg1);
			else
			        clen += printf(" F=%-3d  F=%-3d", ti->arg1, arg2);
			break;

		      case FMT_FD_IO:
			/*
			 * system calls with fd's that return an I/O completion count
			 */
			if (arg1)
			        clen += printf(" F=%-3d[%3d]", ti->arg1, arg1);
			else
			        clen += printf(" F=%-3d  B=0x%-6x", ti->arg1, arg2);
			break;

		      case FMT_PGIN:
			/*
			 * pagein
			 */
			user_addr = ((uint64_t)arg2 << 32) | (uint32_t)arg3;
		        framework_name = lookup_name(user_addr);
			clen += clip_64bit(" A=", user_addr);
			break;

		      case FMT_CACHEHIT:
			/*
			 * cache hit
			 */
			user_addr = ((uint64_t)arg2 << 32) | (uint32_t)arg3;

		        framework_name = lookup_name(user_addr);
			clen += clip_64bit(" A=", user_addr);
			break;

		      case FMT_PGOUT:
			/*
			 * pageout
			 */
		        clen += printf("      B=0x%-8x", arg2);
			break;

		      case FMT_DISKIO:
		        /*
			 * physical disk I/O
			 */
		        if (dio->io_errno)
			        clen += printf(" D=0x%8.8x  [%3d]", dio->blkno, dio->io_errno);
			else
			        clen += printf(" D=0x%8.8x  B=0x%-6x   /dev/%s", dio->blkno, dio->iosize, find_disk_name(dio->dev));
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

			if (arg1)
			        clen += printf("      [%3d]", arg1);

			user_addr = (((off_t)(unsigned int)(ti->arg4)) << 32) | (unsigned int)(ti->arg1);
			clen += clip_64bit(" A=", user_addr);

			user_size = (((off_t)(unsigned int)(ti->arg5)) << 32) | (unsigned int)(ti->arg2);

		        clen += printf("  B=0x%-16qx  <%s>", user_size, buf);

			break;
		      }

		      case FMT_FCNTL:
		      {
			/*
			 * fcntl
			 */
			char *p = NULL;

			if (arg1)
			        clen += printf(" F=%-3d[%3d]", ti->arg1, arg1);
			else
			        clen += printf(" F=%-3d", ti->arg1);

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

			case F_NOCACHE:
			  if (ti->arg3)
			          p = "CACHING OFF";
			  else
			          p = "CACHING ON";
			  break;

			}
			if (p)
			        clen += printf(" <%s>", p);
			else
			        clen += printf(" <CMD=%d>", ti->arg2);

			break;
		      }

		      case FMT_IOCTL:
		      {
			/*
			 * fcntl
			 */
			if (arg1)
			        clen += printf(" F=%-3d[%3d]", ti->arg1, arg1);
			else
			        clen += printf(" F=%-3d", ti->arg1);

		        clen += printf(" <CMD=0x%x>", ti->arg2);

			break;
		      }

		      case FMT_SELECT:
			/*
			 * select
			 */
			if (arg1)
			        clen += printf("      [%3d]", arg1);
			else
			        clen += printf("        S=%-3d", arg2);

			break;

		      case FMT_LSEEK:
		      case FMT_PREAD:
			/*
			 * pread, pwrite, lseek
			 */
			clen += printf(" F=%-3d", ti->arg1);

			if (arg1)
			        clen += printf("[%3d]  ", arg1);
			else {
			        if (format == FMT_PREAD)
				        clen += printf("  B=0x%-8x ", arg2);
				else
				        clen += printf("  ");
			}	
			if (format == FMT_PREAD)
			        offset_reassembled = (((off_t)(unsigned int)(ti->arg3)) << 32) | (unsigned int)(ti->arg4);
			else
#ifdef __ppc__
			        offset_reassembled = (((off_t)(unsigned int)(arg2)) << 32) | (unsigned int)(arg3);
#else
			        offset_reassembled = (((off_t)(unsigned int)(arg3)) << 32) | (unsigned int)(arg2);
#endif
			clen += clip_64bit("O=", offset_reassembled);

			if (format == FMT_LSEEK) {
			        char *mode;

				if (ti->arg4 == SEEK_SET)
				        mode = "SEEK_SET";
				else if (ti->arg4 == SEEK_CUR)
				        mode = "SEEK_CUR";
				else if (ti->arg4 == SEEK_END)
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
			clen += printf(" F=%-3d  ", ti->arg1);

			if (arg1)
			        clen += printf("[%3d]  ", arg1);
			else {

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
			        clen += printf(" F=%-3d", ti->arg1);
			else
			        clen += printf("      ");

			if (arg1)
			        clen += printf("[%3d]", arg1);

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
			        if (arg1)
				        clen += printf(" F=%-3d[%3d]", ti->arg1, arg1);
				else
				        clen += printf(" F=%-3d", ti->arg1);
			} else {
			        if (arg1)
				        clen += printf(" [%3d] ", arg1);
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

			if (mlen < 21) {
			        memset(&buf[mlen], ' ', 21 - mlen);
				mlen = 21;
			}
			clen += printf("%s", buf);

			nopadding = 1;
			break;
		      }

		      case FMT_FCHMOD:
		      case FMT_FCHMOD_EXT:
		      case FMT_CHMOD:
		      case FMT_CHMOD_EXT:
		      {
			/*
			 * fchmod, fchmod_extended, chmod, chmod_extended
			 */
			int  mode;

			if (format == FMT_FCHMOD || format == FMT_FCHMOD_EXT) {
			        if (arg1)
				        clen += printf(" F=%-3d[%3d] ", ti->arg1, arg1);
				else
				        clen += printf(" F=%-3d ", ti->arg1);
			} else {
			        if (arg1)
				        clen += printf(" [%3d] ", arg1);
				else	
				        clen += printf(" ");
			}
			if (format == FMT_FCHMOD || format == FMT_CHMOD)
			        mode = ti->arg2;
			else
			        mode = ti->arg4;

			get_mode_string(mode, &buf[0]);

			if (arg1 == 0)
			        clen += printf("<%s>     ", buf);
			else
			        clen += printf("<%s>", buf);
			break;
		      }

		      case FMT_ACCESS:
		      {
			/*
			 * access
			 */
			char mode[4];
			
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

		        if (arg1)
			        clen += printf("      [%3d] (%s)   ", arg1, mode);
			else
			        clen += printf("            (%s)   ", mode);

			nopadding = 1;
			break;
		      }

		      case FMT_OPEN:
		      {
			/*
			 * open
			 */
		        char mode[7];
		  
			memset(mode, '_', 6);
			mode[6] = '\0';

			if (ti->arg2 & O_RDWR) {
			        mode[0] = 'R';
			        mode[1] = 'W';
			} else if (ti->arg2 & O_WRONLY)
			        mode[1] = 'W';
			else
			        mode[0] = 'R';

			if (ti->arg2 & O_CREAT)
			        mode[2] = 'C';
			  
			if (ti->arg2 & O_APPEND)
			        mode[3] = 'A';
			  
			if (ti->arg2 & O_TRUNC)
			        mode[4] = 'T';
			  
			if (ti->arg2 & O_EXCL)
			        mode[5] = 'E';

		        if (arg1)
			        clen += printf("      [%3d] (%s) ", arg1, mode);
			else
			        clen += printf(" F=%-3d      (%s) ", arg2, mode);

			nopadding = 1;
			break;
		      }

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

			case SOCK_SEQPACKET:
			  type = "SOCK_SEQPACKET";
			  break;

			case SOCK_RDM:
			  type = "SOCK_RDM";
			  break;
			  
			default:
			  type = "UNKNOWN";
			  break;
			}

			if (arg1)
			        clen += printf("      [%3d] <%s, %s, 0x%x>", arg1, domain, type, ti->arg3);
			else
			        clen += printf(" F=%-3d      <%s, %s, 0x%x>", arg2, domain, type, ti->arg3);
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

			if (arg1)
			        clen += printf("      [%3d] P=0x%8.8x  <%s>", arg1, ti->arg2, op);
			else
			        clen += printf("            P=0x%8.8x  <%s>", ti->arg2, op);
			break;
		      }

		      case FMT_AIO_RETURN:
			/*
			 * aio_return		[errno]   AIOCBP   IOSIZE
			 */
			if (arg1)
			        clen += printf("      [%3d] P=0x%8.8x", arg1, ti->arg1);
			else
			        clen += printf("            P=0x%8.8x  B=0x%-8x", ti->arg1, arg2);
			break;

		      case FMT_AIO_SUSPEND:
			/*
			 * aio_suspend		[errno]   NENTS
			 */
			if (arg1)
			        clen += printf("      [%3d] N=%d", arg1, ti->arg2);
			else
			        clen += printf("            N=%d", ti->arg2);
			break;

		      case FMT_AIO_CANCEL:
			/*
			 * aio_cancel	  	[errno]   FD or AIOCBP (if non-null)
			 */
			if (ti->arg2) {
			        if (arg1)
				        clen += printf("      [%3d] P=0x%8.8x", arg1, ti->arg2);
				else
				        clen += printf("            P=0x%8.8x", ti->arg2);
			} else {
			        if (arg1)
				        clen += printf(" F=%-3d[%3d]", ti->arg1, arg1);
				else
				        clen += printf(" F=%-3d", ti->arg1);
			}
			break;

		      case FMT_AIO:
			/*
			 * aio_error, aio_read, aio_write	[errno]  AIOCBP
			 */
			if (arg1)
			        clen += printf("      [%3d] P=0x%8.8x", arg1, ti->arg1);
			else
			        clen += printf("            P=0x%8.8x", ti->arg1);
			break;

		      case FMT_LIO_LISTIO:
		      {
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

			if (arg1)
			        clen += printf("      [%3d] N=%d  <%s>", arg1, ti->arg3, op);
			else
			        clen += printf("            N=%d  <%s>", ti->arg3, op);
			break;
		      }

		}
	}

	/*
	 * Calculate space available to print pathname
	 */
	if (columns > MAXCOLS || wideflag)
	        clen =  columns - (clen + 14 + 20);
	else
	        clen =  columns - (clen + 14 + 12);

	if (class != FILEMGR_CLASS && !nopadding)
	        clen -= 3;

	if (framework_name)
	        len = sprintf(&buf[0], " %s ", framework_name);
	else if (*pathname != '\0')
	        len = sprintf(&buf[0], " %s ", pathname);
	else
	        len = 0;

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
	usecs = (unsigned long)(((now - stime) + (divisor-1)) / divisor);
	secs = usecs / 1000000;
	usecs -= secs * 1000000;

	if (class != FILEMGR_CLASS && !nopadding)
	        p1 = "   ";
	else
	        p1 = "";
	       
	if (waited)
	        p2 = " W";
	else
	        p2 = "  ";

	if (columns > MAXCOLS || wideflag)
	        printf("%s%s %3ld.%06ld%s %-20.20s\n", p1, pathname, (unsigned long)secs, (unsigned long)usecs, p2, command_name);
	else
	        printf("%s%s %3ld.%06ld%s %-12.12s\n", p1, pathname, (unsigned long)secs, (unsigned long)usecs, p2, command_name);
}

int
quit(s)
char *s;
{
        if (trace_enabled)
	       set_enable(0);

	/* 
	 * This flag is turned off when calling
	 * quit() due to a set_remove() failure.
	 */
	if (set_remove_flag)
	        set_remove();

        fprintf(stderr, "fs_usage: ");
	if (s)
		fprintf(stderr, "%s", s);

	exit(1);
}


void getdivisor()
{
    struct mach_timebase_info mti;

    mach_timebase_info(&mti);

    divisor = ((double)mti.denom / (double)mti.numer) * 1000;
}


void read_command_map()
{
    size_t size;
    int i;
    int prev_total_threads;
    int mib[6];
  
    if (mapptr) {
	free(mapptr);
	mapptr = 0;
    }

    prev_total_threads = total_threads;
    total_threads = bufinfo.nkdthreads;
    size = bufinfo.nkdthreads * sizeof(kd_threadmap);

    if (size)
    {
        if ((mapptr = (kd_threadmap *) malloc(size)))
	{
	     bzero (mapptr, size);
 
	     /* Now read the threadmap */
	     mib[0] = CTL_KERN;
	     mib[1] = KERN_KDEBUG;
	     mib[2] = KERN_KDTHRMAP;
	     mib[3] = 0;
	     mib[4] = 0;
	     mib[5] = 0;		/* no flags */
	     if (sysctl(mib, 3, mapptr, &size, NULL, 0) < 0)
	     {
		 /* This is not fatal -- just means I cant map command strings */
		 free(mapptr);
		 mapptr = 0;
	     }
	}
    }

    if (mapptr && (filter_mode & (NETWORK_FILTER | FILESYS_FILTER)))
    {
	if (fdmapptr)
	{
	    /* We accept the fact that we lose file descriptor state if the
	       kd_buffer wraps */
	    for (i = 0; i < prev_total_threads; i++)
	    {
		if (fdmapptr[i].fd_setptr)
		    free (fdmapptr[i].fd_setptr);
	    }
	    free(fdmapptr);
	    fdmapptr = 0;
	}

	size = total_threads * sizeof(fd_threadmap);
	if ((fdmapptr = (fd_threadmap *) malloc(size)))
	{
	    bzero (fdmapptr, size);
	    /* reinitialize file descriptor state map */
	    for (i = 0; i < total_threads; i++)
	    {
		fdmapptr[i].fd_thread = mapptr[i].thread;
		fdmapptr[i].fd_valid = mapptr[i].valid;
		fdmapptr[i].fd_setsize = 0;
		fdmapptr[i].fd_setptr = 0;
	    }
	}
    }

    /* Resolve any LaunchCFMApp command names */
    if (mapptr && arguments)
    {
	for (i=0; i < total_threads; i++)
	{
	    int pid;

	    pid = mapptr[i].valid;
	    
	    if (pid == 0 || pid == 1)
		continue;
	    else if (!strncmp(mapptr[i].command,"LaunchCFMA", 10))
	    {
		(void)get_real_command_name(pid, mapptr[i].command, sizeof(mapptr[i].command));
	    }
	}
    }
}


void create_map_entry(int thread, int pid, char *command)
{
    int i, n;
    kd_threadmap *map;
    fd_threadmap *fdmap = 0;

    if (!mapptr)
        return;

    for (i = 0, map = 0; !map && i < total_threads; i++)
    {
        if ((int)mapptr[i].thread == thread )
	{
	    map = &mapptr[i];   /* Reuse this entry, the thread has been
				 * reassigned */
	    if ((filter_mode & (NETWORK_FILTER | FILESYS_FILTER)) && fdmapptr)
	    {
		fdmap = &fdmapptr[i];
		if (fdmap->fd_thread != thread)    /* This shouldn't happen */
		    fdmap = (fd_threadmap *)0;
	    }
	}
    }

    if (!map)   /* look for invalid entries that I can reuse*/
    {
        for (i = 0, map = 0; !map && i < total_threads; i++)
	{
	    if (mapptr[i].valid == 0 )	
	        map = &mapptr[i];   /* Reuse this invalid entry */
	    if ((filter_mode & (NETWORK_FILTER | FILESYS_FILTER)) && fdmapptr)
	    {
		fdmap = &fdmapptr[i];
	    }
	}
    }
  
    if (!map)
    {
        /*
	 * If reach here, then this is a new thread and 
	 * there are no invalid entries to reuse
	 * Double the size of the thread map table.
	 */
        n = total_threads * 2;
	mapptr = (kd_threadmap *) realloc(mapptr, n * sizeof(kd_threadmap));
	bzero(&mapptr[total_threads], total_threads*sizeof(kd_threadmap));
	map = &mapptr[total_threads];

	if ((filter_mode & (NETWORK_FILTER | FILESYS_FILTER)) && fdmapptr)
	{
	    fdmapptr = (fd_threadmap *)realloc(fdmapptr, n * sizeof(fd_threadmap));
	    bzero(&fdmapptr[total_threads], total_threads*sizeof(fd_threadmap));
	    fdmap = &fdmapptr[total_threads];	    
	}
	
	total_threads = n;
    }

    map->valid = 1;
    map->thread = thread;
    /*
     * The trace entry that returns the command name will hold
     * at most, MAXCOMLEN chars, and in that case, is not
     * guaranteed to be null terminated.
     */
    (void)strncpy (map->command, command, MAXCOMLEN);
    map->command[MAXCOMLEN] = '\0';

    if (fdmap)
    {
	fdmap->fd_valid = 1;
	fdmap->fd_thread = thread;
	if (fdmap->fd_setptr)
	{
	    free(fdmap->fd_setptr);
	    fdmap->fd_setptr = (unsigned long *)0;
	}
	fdmap->fd_setsize = 0;
    }

    if (pid == 0 || pid == 1)
	return;
    else if (!strncmp(map->command, "LaunchCFMA", 10))
	(void)get_real_command_name(pid, map->command, sizeof(map->command));
}


kd_threadmap *find_thread_map(int thread)
{
    int i;
    kd_threadmap *map;

    if (!mapptr)
        return((kd_threadmap *)0);

    for (i = 0; i < total_threads; i++)
    {
        map = &mapptr[i];
	if (map->valid && ((int)map->thread == thread))
	{
	    return(map);
	}
    }
    return ((kd_threadmap *)0);
}

fd_threadmap *find_fd_thread_map(int thread)
{
    int i;
    fd_threadmap *fdmap = 0;

    if (!fdmapptr)
        return((fd_threadmap *)0);

    for (i = 0; i < total_threads; i++)
    {
        fdmap = &fdmapptr[i];
	if (fdmap->fd_valid && ((int)fdmap->fd_thread == thread))
	{
	    return(fdmap);
	}
    }
    return ((fd_threadmap *)0);
}


void
kill_thread_map(int thread)
{
    kd_threadmap *map;
    fd_threadmap *fdmap;

    if (thread == last_thread)
        map_is_the_same = 0;

    if ((map = find_thread_map(thread))) {
        map->valid = 0;
	map->thread = 0;
	map->command[0] = '\0';
    }

    if ((filter_mode & (NETWORK_FILTER | FILESYS_FILTER)))
    {
	if ((fdmap = find_fd_thread_map(thread)))
	{
	    fdmap->fd_valid = 0;
	    fdmap->fd_thread = 0;
	    if (fdmap->fd_setptr)
	    {
		free (fdmap->fd_setptr);
		fdmap->fd_setptr = (unsigned long *)0;
	    }
	    fdmap->fd_setsize = 0;
	}	
    }
}

void
argtopid(str)
        char *str;
{
        char *cp;
        int ret;
	int i;

        ret = (int)strtol(str, &cp, 10);
        if (cp == str || *cp) {
	  /* Assume this is a command string and find matching pids */
	        if (!kp_buffer)
		        find_proc_names();

	        for (i=0; i < kp_nentries && num_of_pids < (MAX_PIDS - 1); i++) {
		      if(kp_buffer[i].kp_proc.p_stat == 0)
		          continue;
		      else {
			  if(!strncmp(str, kp_buffer[i].kp_proc.p_comm,
			      sizeof(kp_buffer[i].kp_proc.p_comm) -1))
			    pids[num_of_pids++] = kp_buffer[i].kp_proc.p_pid;
		      }
		}
	}
	else if (num_of_pids < (MAX_PIDS - 1))
	        pids[num_of_pids++] = ret;

        return;
}



char *lookup_name(uint64_t user_addr) 
{       
        register int i;
	register int start, last;
	
	if (numFrameworks && ((user_addr >= framework32.b_address && user_addr < framework32.e_address) ||
			      (user_addr >= framework64.b_address && user_addr < framework64.e_address))) {
			      
		start = 0;
		last  = numFrameworks;

		for (i = numFrameworks / 2; i >= 0 && i < numFrameworks; ) {

			if (user_addr >= frameworkInfo[i].b_address && user_addr < frameworkInfo[i].e_address)
				return(frameworkInfo[i].name);

			if (user_addr >= frameworkInfo[i].b_address) {
				start = i;
				i = start + ((last - i) / 2);
			} else {
				last = i;
				i = start + ((i - start) / 2);
			}
		}
	}
	return (0);
}


/*
 * Comparison routines for sorting
 */
static int compareFrameworkAddress(const void  *aa, const void *bb)
{
    LibraryInfo *a = (LibraryInfo *)aa;
    LibraryInfo *b = (LibraryInfo *)bb;

    if (a->b_address < b->b_address) return -1;
    if (a->b_address == b->b_address) return 0;
    return 1;
}


int scanline(char *inputstring, char **argv, int maxtokens)
{
     int n = 0;
     char **ap = argv, *p, *val;

     for (p = inputstring; n < maxtokens && p != NULL; ) 
     {
        while ((val = strsep(&p, " \t")) != NULL && *val == '\0');
        *ap++ = val;
        n++;
     }
     *ap = 0;
     return n;
}


int ReadSharedCacheMap(const char *path, LibraryRange *lr)
{
	char buf[1024];

	FILE		*fd;
	uint64_t	b_frameworkAddress, b_frameworkDataAddress;
	uint64_t	e_frameworkAddress, e_frameworkDataAddress;
	char frameworkName[256];
	char *tokens[64];
	int  ntokens;
	char *substring,*ptr;


	bzero(buf, sizeof(buf));
	bzero(tokens, sizeof(tokens));

	lr->b_address = 0;
	lr->e_address = 0;

    
	if ((fd = fopen(path, "r")) == 0)
	{
		return 0;
	}
	while (fgets(buf, 1023, fd)) {
		if (strncmp(buf, "mapping", 7))
			break;
	}
	buf[strlen(buf)-1] = 0;
	
	frameworkName[0] = 0;

	for (;;) {
		b_frameworkAddress = 0;
		b_frameworkDataAddress = 0;
		e_frameworkAddress = 0;
		e_frameworkDataAddress = 0;

		/*
		 * Extract lib name from path name
		 */
		if ((substring = strrchr(buf, '.')))
		{		
			/*
			 * There is a ".": name is whatever is between the "/" around the "." 
			 */
			while ( *substring != '/') {		    /* find "/" before "." */
				substring--;
			}
			substring++;
			strncpy(frameworkName, substring, 256);           /* copy path from "/" */
			frameworkName[255] = 0;
			substring = frameworkName;

			while ( *substring != '/' && *substring)    /* find "/" after "." and stop string there */
				substring++;
			*substring = 0;
		}
		else 
		{
			/*
			 * No ".": take segment after last "/"
			 */
			ptr = buf;
			substring = ptr;

			while (*ptr) 
			{
				if (*ptr == '/')
					substring = ptr + 1;
				ptr++;
			}
			strncpy(frameworkName, substring, 256);
			frameworkName[255] = 0;
		}
		while (fgets(buf, 1023, fd) && numFrameworks < (MAXINDEX - 2))
		{
			/*
			 * Get rid of EOL
			 */
			buf[strlen(buf)-1] = 0;

			ntokens = scanline(buf, tokens, 64);

			if (ntokens < 4)
				continue;

			if (strncmp(tokens[0], "__TEXT", 6) == 0) {
				b_frameworkAddress = strtoull(tokens[1], 0, 16);
				e_frameworkAddress = strtoull(tokens[3], 0, 16);
			} else if (strncmp(tokens[0], "__DATA", 6) == 0) {
				b_frameworkDataAddress = strtoull(tokens[1], 0, 16);
				e_frameworkDataAddress = strtoull(tokens[3], 0, 16);
			} else if (strncmp(tokens[0], "__LINKEDIT", 10) == 0)
				break;
		}
		/*
		 * Make sure that we have 2 addresses
		 */
		if (b_frameworkAddress && b_frameworkDataAddress) {

			frameworkInfo[numFrameworks].b_address   = b_frameworkAddress;
			frameworkInfo[numFrameworks].e_address   = e_frameworkAddress;

			frameworkInfo[numFrameworks+1].b_address = b_frameworkDataAddress;
			frameworkInfo[numFrameworks+1].e_address = e_frameworkDataAddress;

			frameworkInfo[numFrameworks].name = (char *)malloc(strlen(frameworkName) + 1);
			strcpy(frameworkInfo[numFrameworks].name, frameworkName);
			frameworkInfo[numFrameworks+1].name = frameworkInfo[numFrameworks].name;

			numFrameworks += 2;
#if 0
			printf("%s: %qx-%qx  %qx-%qx\n", frameworkName, b_frameworkAddress, e_frameworkAddress, b_frameworkDataAddress, e_frameworkDataAddress);
#endif
			if (lr->b_address == 0)
				lr->b_address = b_frameworkAddress;

			if (b_frameworkAddress < lr->b_address)
				lr->b_address = b_frameworkAddress;

			if (b_frameworkDataAddress < lr->b_address)
				lr->b_address = b_frameworkDataAddress;

			if (e_frameworkAddress > lr->e_address)
				lr->e_address = e_frameworkAddress;

			if (e_frameworkDataAddress > lr->e_address)
				lr->e_address = e_frameworkDataAddress;
		}
		if (fgets(buf, 1023, fd) == 0)
			break;

		buf[strlen(buf)-1] = 0;
	}
	fclose(fd);

	return 1;
}


void
SortFrameworkAddresses()
{

	frameworkInfo[numFrameworks].b_address = frameworkInfo[numFrameworks - 1].b_address + 0x800000;
	frameworkInfo[numFrameworks].e_address = frameworkInfo[numFrameworks].b_address;
	frameworkInfo[numFrameworks].name = (char *)0;

	qsort(frameworkInfo, numFrameworks, sizeof(LibraryInfo), compareFrameworkAddress);
}


struct diskio *insert_diskio(int type, int bp, int dev, int blkno, int io_size, int thread, double curtime)
{
    register struct diskio *dio;
    register kd_threadmap  *map;
    
    if ((dio = free_diskios))
        free_diskios = dio->next;
    else {
        if ((dio = (struct diskio *)malloc(sizeof(struct diskio))) == NULL)
            return (NULL);
    }
    dio->prev = NULL;
    
    dio->type = type;
    dio->bp = bp;
    dio->dev = dev;
    dio->blkno = blkno;
    dio->iosize = io_size;
    dio->issued_time = curtime;
    dio->issuing_thread = thread;
    
    if ((map = find_thread_map(thread)))
    {
        strncpy(dio->issuing_command, map->command, MAXCOMLEN);
	dio->issuing_command[MAXCOMLEN-1] = '\0';
    }
    else
        strcpy(dio->issuing_command, "");
    
    dio->next = busy_diskios;
    if (dio->next)
        dio->next->prev = dio;
    busy_diskios = dio;

    return (dio);
}


struct diskio *complete_diskio(int bp, int io_errno, int resid, int thread, double curtime)
{
    register struct diskio *dio;
    
    for (dio = busy_diskios; dio; dio = dio->next) {
        if (dio->bp == bp) {
        
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
            dio->completion_thread = thread;
            
	    return (dio);
        }    
    }
    return ((struct diskio *)0);
}


void free_diskio(struct diskio *dio)
{
    dio->next = free_diskios;
    free_diskios = dio;
}


void print_diskio(struct diskio *dio)
{
    register char  *p;

    switch (dio->type) {

        case P_RdMeta:
            p = "  RdMeta";
            break;
        case P_WrMeta:
            p = "  WrMeta";
            break;
        case P_RdData:
            p = "  RdData";
            break;
        case P_WrData:
            p = "  WrData";
            break;        
        case P_PgIn:
            p = "  PgIn";
            break;
        case P_PgOut:
            p = "  PgOut";
            break;
        case P_RdMetaAsync:
            p = "  RdMeta[async]";
            break;
        case P_WrMetaAsync:
            p = "  WrMeta[async]";
            break;
        case P_RdDataAsync:
            p = "  RdData[async]";
            break;
        case P_WrDataAsync:
            p = "  WrData[async]";
            break;        
        case P_PgInAsync:
            p = "  PgIn[async]";
            break;
        case P_PgOutAsync:
            p = "  PgOut[async]";
            break;
        default:
            p = "  ";
	    break;
    }
    if (check_filter_mode(NULL, dio->type, 0, 0, p))
	format_print(NULL, p, dio->issuing_thread, dio->type, 0, 0, 0, 0, FMT_DISKIO, dio->completed_time, dio->issued_time, 1, "", dio);
}


void cache_disk_names()
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

        if ((dnp = (struct diskrec *)malloc(sizeof(struct diskrec))) == NULL)
            continue;
            
        if ((dnp->diskname = (char *)malloc(dir->d_namlen + 1)) == NULL) {
            free(dnp);
            continue;
        }
        strncpy(dnp->diskname, dir->d_name, dir->d_namlen);
        dnp->diskname[dir->d_namlen] = 0;
        dnp->dev = st.st_rdev;
        
        dnp->next = disk_list;
        disk_list = dnp;
    }
    (void) closedir(dirp);
}


char *find_disk_name(int dev)
{
    struct diskrec *dnp;
    
    if (dev == NFS_DEV)
        return ("NFS");
        
    for (dnp = disk_list; dnp; dnp = dnp->next) {
        if (dnp->dev == dev)
            return (dnp->diskname);
    }
    return ("NOTFOUND");
}

void
fs_usage_fd_set(thread, fd)
    unsigned int thread;
    unsigned int fd;
{
    int n;
    fd_threadmap *fdmap;

    if(!(fdmap = find_fd_thread_map(thread)))
	return;

    /* If the map is not allocated, then now is the time */
    if (fdmap->fd_setptr == (unsigned long *)0)
    {
	fdmap->fd_setptr = (unsigned long *)malloc(FS_USAGE_NFDBYTES(FS_USAGE_FD_SETSIZE));
	if (fdmap->fd_setptr)
	{
	    fdmap->fd_setsize = FS_USAGE_FD_SETSIZE;
	    bzero(fdmap->fd_setptr,(FS_USAGE_NFDBYTES(FS_USAGE_FD_SETSIZE)));
	}
	else
	    return;
    }

    /* If the map is not big enough, then reallocate it */
    while (fdmap->fd_setsize <= fd)
    {
	fprintf(stderr, "reallocating bitmap for threadid %d, fd = %d, setsize = %d\n",
	  thread, fd, fdmap->fd_setsize);
	n = fdmap->fd_setsize * 2;
	fdmap->fd_setptr = (unsigned long *)realloc(fdmap->fd_setptr, (FS_USAGE_NFDBYTES(n)));
	bzero(&fdmap->fd_setptr[(fdmap->fd_setsize/FS_USAGE_NFDBITS)], (FS_USAGE_NFDBYTES(fdmap->fd_setsize)));
	fdmap->fd_setsize = n;
    }

    /* set the bit */
    fdmap->fd_setptr[fd/FS_USAGE_NFDBITS] |= (1 << ((fd) % FS_USAGE_NFDBITS));

    return;
}

/*
  Return values:
  0 : File Descriptor bit is not set
  1 : File Descriptor bit is set
*/
    
int
fs_usage_fd_isset(thread, fd)
    unsigned int thread;
    unsigned int fd;
{
    int ret = 0;
    fd_threadmap *fdmap;

    if(!(fdmap = find_fd_thread_map(thread)))
	return(ret);

    if (fdmap->fd_setptr == (unsigned long *)0)
	return (ret);

    if (fd < fdmap->fd_setsize)
	ret = fdmap->fd_setptr[fd/FS_USAGE_NFDBITS] & (1 << (fd % FS_USAGE_NFDBITS));
    
    return (ret);
}
    
void
fs_usage_fd_clear(thread, fd)
    unsigned int thread;
    unsigned int fd;
{
    fd_threadmap *map;

    if (!(map = find_fd_thread_map(thread)))
	return;

    if (map->fd_setptr == (unsigned long *)0)
	return;

    /* clear the bit */
    if (fd < map->fd_setsize)
	map->fd_setptr[fd/FS_USAGE_NFDBITS] &= ~(1 << (fd % FS_USAGE_NFDBITS));
    
    return;
}


/*
 * ret = 1 means print the entry
 * ret = 0 means don't print the entry
 */
int
check_filter_mode(struct th_info * ti, int type, int error, int retval, char *sc_name)
{
    int ret = 0;
    int network_fd_isset = 0;
    unsigned int fd;

    if (filter_mode == DEFAULT_DO_NOT_FILTER)
            return(1);

    if (sc_name[0] == 'C' && !strcmp (sc_name, "CACHE_HIT")) {
            if (filter_mode & CACHEHIT_FILTER)
	            /* Do not print if cachehit filter is set */
	            return(0);
	    return(1);
    }
	
    if (filter_mode & EXEC_FILTER)
    {
            if (type == BSC_execve)
	            return(1);
	    return(0);
    }

    if (filter_mode & PATHNAME_FILTER)
    {
            if (ti && ti->pathname[0])
	            return(1);
	    if (type == BSC_close || type == BSC_close_nocancel)
	            return(1);
	    return(0);
    }

    if ( !(filter_mode & (FILESYS_FILTER | NETWORK_FILTER)))
	return(1);

	
    if (ti == (struct th_info *)0)
    {
	if (filter_mode & FILESYS_FILTER)
	    ret = 1;
	else
	    ret = 0;
	return(ret);
    }

    switch (type) {
    case BSC_close:
    case BSC_close_nocancel:
	fd = ti->arg1;
	network_fd_isset = fs_usage_fd_isset(ti->thread, fd);
	if (error == 0)
	{
	    fs_usage_fd_clear(ti->thread,fd);
	}
	
	if (network_fd_isset)
	{
	    if (filter_mode & NETWORK_FILTER)
		ret = 1;
	}
	else if (filter_mode & FILESYS_FILTER)
	    ret = 1;
	break;
    case BSC_read:
    case BSC_write:
    case BSC_read_nocancel:
    case BSC_write_nocancel:
	/* we don't care about error in this case */
	fd = ti->arg1;
	network_fd_isset = fs_usage_fd_isset(ti->thread, fd);
	if (network_fd_isset)
	{
	    if (filter_mode & NETWORK_FILTER)
		ret = 1;
	}
	else if (filter_mode & FILESYS_FILTER)
	    ret = 1;	
	break;
    case BSC_accept:
    case BSC_accept_nocancel:
    case BSC_socket:
	fd = retval;
	if (error == 0)
	    fs_usage_fd_set(ti->thread, fd);
	if (filter_mode & NETWORK_FILTER)
	    ret = 1;
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
	    fs_usage_fd_set(ti->thread, fd);
	if (filter_mode & NETWORK_FILTER)
	    ret = 1;
	break;
    case BSC_select:
    case BSC_select_nocancel:
    case BSC_socketpair:
	/* Cannot determine info about file descriptors */
	if (filter_mode & NETWORK_FILTER)
	    ret = 1;
	break;
    case BSC_dup:
    case BSC_dup2:
	ret=0;   /* We track these cases for fd state only */
	fd = ti->arg1;  /* oldd */
	network_fd_isset = fs_usage_fd_isset(ti->thread, fd);
	if (error == 0 && network_fd_isset)
	{
	    /* then we are duping a socket descriptor */
	    fd = retval;  /* the new fd */
	    fs_usage_fd_set(ti->thread, fd);
	}
	break;

    default:
	if (filter_mode & FILESYS_FILTER)
	    ret = 1;
	break;
    }

    return(ret);
}

/*
 * Allocate a buffer that is large enough to hold the maximum arguments
 * to execve().  This is used when getting the arguments to programs
 * when we see LaunchCFMApps.  If this fails, it is not fatal, we will
 * simply not resolve the command name.
 */

void
init_arguments_buffer()
{

    int     mib[2];
    size_t  size;

    mib[0] = CTL_KERN;
    mib[1] = KERN_ARGMAX;
    size = sizeof(argmax);
    if (sysctl(mib, 2, &argmax, &size, NULL, 0) == -1)
	return;

#if 1
    /* Hack to avoid kernel bug. */
    if (argmax > 8192) {
	argmax = 8192;
    }
#endif

    arguments = (char *)malloc(argmax);
    
    return;
}


int
get_real_command_name(int pid, char *cbuf, int csize)
{
    /*
     *      Get command and arguments.
     */
    char  *cp;
    int             mib[4];
    char    *command_beg, *command, *command_end;

    if (cbuf == NULL) {
	return(0);
    }

    if (arguments)
	bzero(arguments, argmax);
    else
	return(0);

    /*
     * A sysctl() is made to find out the full path that the command
     * was called with.
     */
    mib[0] = CTL_KERN;
    mib[1] = KERN_PROCARGS;
    mib[2] = pid;
    mib[3] = 0;

    if (sysctl(mib, 3, arguments, (size_t *)&argmax, NULL, 0) < 0) {
	return(0);
    }

    /* Skip the saved exec_path. */
    for (cp = arguments; cp < &arguments[argmax]; cp++) {
	if (*cp == '\0') {
	    /* End of exec_path reached. */
	    break;
	}
    }
    if (cp == &arguments[argmax]) {
	return(0);
    }

    /* Skip trailing '\0' characters. */
    for (; cp < &arguments[argmax]; cp++) {
	if (*cp != '\0') {
	    /* Beginning of first argument reached. */
	    break;
	}
    }
    if (cp == &arguments[argmax]) {
	return(0);
    }
    command_beg = cp;

    /*
     * Make sure that the command is '\0'-terminated.  This protects
     * against malicious programs; under normal operation this never
     * ends up being a problem..
     */
    for (; cp < &arguments[argmax]; cp++) {
	if (*cp == '\0') {
	    /* End of first argument reached. */
	    break;
	}
    }
    if (cp == &arguments[argmax]) {
	return(0);
    }
    command_end = command = cp;

    /* Get the basename of command. */
    for (command--; command >= command_beg; command--) {
	if (*command == '/') {
	    command++;
	    break;
	}
    }

    (void) strncpy(cbuf, (char *)command, csize);
    cbuf[csize-1] = '\0';

    return(1);
}
