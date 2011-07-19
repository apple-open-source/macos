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
cc -I/System/Library/Frameworks/System.framework/Versions/B/PrivateHeaders -DPRIVATE -D__APPLE_PRIVATE -arch x86_64 -arch i386 -O -o fs_usage fs_usage.c
*/

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <strings.h>
#include <nlist.h>
#include <fcntl.h>
#include <aio.h>
#include <string.h>
#include <dirent.h>
#include <libc.h>
#include <termios.h>
#include <errno.h>
#include <err.h>
#include <libutil.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <sys/sysctl.h>
#include <sys/disk.h>

#ifndef KERNEL_PRIVATE
#define KERNEL_PRIVATE
#include <sys/kdebug.h>
#undef KERNEL_PRIVATE
#else
#include <sys/kdebug.h>
#endif /*KERNEL_PRIVATE*/

#import <mach/clock_types.h>
#import <mach/mach_time.h>



#define F_OPENFROM      56              /* SPI: open a file relative to fd (must be a dir) */
#define F_UNLINKFROM    57              /* SPI: open a file relative to fd (must be a dir) */
#define F_CHECK_OPENEVT 58              /* SPI: if a process is marked OPENEVT, or in O_EVTONLY on opens of this vnode */


#ifndef	RAW_VERSION1
typedef struct {
        int             version_no;
        int             thread_count;
        uint64_t        TOD_secs;
	uint32_t        TOD_usecs;
} RAW_header;

#define RAW_VERSION0    0x55aa0000
#define RAW_VERSION1    0x55aa0101
#endif


#define MAXINDEX 2048

typedef struct LibraryRange {
	uint64_t b_address;
	uint64_t e_address;
} LibraryRange;

LibraryRange framework32;
LibraryRange framework64;


#define	TEXT_R		0
#define DATA_R		1
#define OBJC_R		2
#define IMPORT_R	3
#define UNICODE_R	4
#define IMAGE_R		5
#define LINKEDIT_R	6


char *frameworkType[] = {
	"<TEXT>    ",
	"<DATA>    ",
	"<OBJC>    ",
	"<IMPORT>  ",
	"<UNICODE> ",
	"<IMAGE>   ",
	"<LINKEDIT>",
};


typedef struct LibraryInfo {
	uint64_t b_address;
	uint64_t e_address;
	int	 r_type;
	char     *name;
} LibraryInfo;

LibraryInfo frameworkInfo[MAXINDEX];
int numFrameworks = 0;


/* 
 * MAXCOLS controls when extra data kicks in.
 * MAX_WIDE_MODE_COLS controls -w mode to get even wider data in path.
 * If NUMPARMS changes to match the kernel, it will automatically
 * get reflected in the -w mode output.
 */
#define NUMPARMS 23
#define PATHLENGTH (NUMPARMS*sizeof(uintptr_t))

#define MAXCOLS 132
#define MAX_WIDE_MODE_COLS (PATHLENGTH + 80)
#define MAXWIDTH MAX_WIDE_MODE_COLS + 64


typedef struct th_info *th_info_t;

struct th_info {
	th_info_t  next;
        uintptr_t  thread;
        uintptr_t  child_thread;

        int  in_filemgr;
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
        int  waited;
        double stime;
        uintptr_t *pathptr;
        uintptr_t pathname[NUMPARMS + 1];	/* add room for null terminator */
        uintptr_t pathname2[NUMPARMS + 1];	/* add room for null terminator */
};


typedef struct threadmap * threadmap_t;

struct threadmap {
	threadmap_t	tm_next;

	uintptr_t	tm_thread;
	unsigned int	tm_setsize;	/* this is a bit count */
	unsigned long  *tm_setptr;	/* file descripter bitmap */
	char		tm_command[MAXCOMLEN + 1];
};


#define HASH_SIZE       1024
#define HASH_MASK       1023

th_info_t	th_info_hash[HASH_SIZE];
th_info_t	th_info_freelist;

threadmap_t	threadmap_hash[HASH_SIZE];
threadmap_t	threadmap_freelist;


int  filemgr_in_progress = 0;
int  need_new_map = 1;
int  bias_secs = 0;
long last_time;
int  wideflag = 0;
int  columns = 0;

int  one_good_pid = 0;    /* Used to fail gracefully when bad pids given */
int  select_pid_mode = 0;  /* Flag set indicates that output is restricted
			      to selected pids or commands */

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
#define CS_DEV	-2

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
        uintptr_t  issuing_thread;
        uintptr_t  completion_thread;
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

int		check_filter_mode(struct th_info *, int, int, int, char *);
void            format_print(struct th_info *, char *, uintptr_t, int, int, int, int, int, int, double, double, int, char *, struct diskio *);
void		enter_event_now(uintptr_t, int, kd_buf *, char *, double);
void		enter_event(uintptr_t, int, kd_buf *, char *, double);
void            exit_event(char *, uintptr_t, int, int, int, int, int, int, double);
void		extend_syscall(uintptr_t, int, kd_buf *);

char           *generate_cs_disk_name(int, char *s);
char           *find_disk_name(int);
void		cache_disk_names();
void		recache_disk_names();

void		lookup_name(uint64_t user_addr, char **type, char **name);
int 		ReadSharedCacheMap(const char *, LibraryRange *, char *);
void 		SortFrameworkAddresses();

void		fs_usage_fd_set(uintptr_t, unsigned int);
int		fs_usage_fd_isset(uintptr_t, unsigned int);
void		fs_usage_fd_clear(uintptr_t, unsigned int);

void		init_arguments_buffer();
int	        get_real_command_name(int, char *, int);

void		delete_all_events();
void	 	delete_event(th_info_t);
th_info_t 	add_event(uintptr_t, int);
th_info_t	find_event(uintptr_t, int);
void		mark_thread_waited(uintptr_t);

void		read_command_map();
void		delete_all_map_entries();
void            create_map_entry(uintptr_t, int, char *);
void		delete_map_entry(uintptr_t);
threadmap_t 	find_map_entry(uintptr_t);

void		getdivisor();
void		argtopid();
void		set_remove();
void		set_pidcheck();
void		set_pidexclude();
int		quit();


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
#define MACH_idle	0x01400024
#define VFS_LOOKUP      0x03010090

#define BSC_thread_terminate    0x040c05a4

#define Throttled	0x3010184
#define SPEC_ioctl	0x3060000
#define proc_exit	0x4010004

#define P_DISKIO	0x03020000
#define P_DISKIO_DONE	0x03020004
#define P_DISKIO_TYPE	0x03020068
#define P_DISKIO_MASK	(CSC_MASK | 0x4)

#define P_DISKIO_READ	  0x00000008
#define P_DISKIO_ASYNC	  0x00000010
#define P_DISKIO_META	  0x00000020
#define P_DISKIO_PAGING   0x00000040
#define P_DISKIO_THROTTLE 0x00000080
#define P_DISKIO_PASSIVE  0x00000100

#define P_WrData	0x03020000
#define P_RdData	0x03020008
#define P_WrMeta	0x03020020
#define P_RdMeta	0x03020028
#define P_PgOut		0x03020040
#define P_PgIn		0x03020048

#define P_CS_ReadChunk		0x0a000200
#define P_CS_ReadChunkDone	0x0a000280
#define P_CS_WriteChunk		0x0a000210
#define P_CS_WriteChunkDone	0x0a000290

#define P_CS_Originated_Read	0x0a000500
#define P_CS_Originated_ReadDone   0x0a000580
#define P_CS_Originated_Write	0x0a000510
#define P_CS_Originated_WriteDone  0x0a000590
#define P_CS_MetaRead		0x0a000900
#define P_CS_MetaReadDone	0x0a000980
#define P_CS_MetaWrite		0x0a000910
#define P_CS_MetaWriteDone	0x0a000990
#define P_CS_SYNC_DISK		0x0a008000


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
#define BSC_mkfifo		0x040c0210	
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
#define BSC_posix_spawn       	0x040C03D0
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
#define FMT_UMASK	32
#define FMT_SENDFILE	33
#define FMT_SPEC_IOCTL	34
#define FMT_MOUNT	35
#define FMT_UNMOUNT	36
#define FMT_DISKIO_CS	37
#define FMT_SYNC_DISK_CS	38

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
	BSC_exit,
	BSC_execve,
	BSC_posix_spawn,
	BSC_umask,
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
	BSC_mount,
	BSC_unmount,
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
	BSC_sendfile,
	BSC_msync,
	BSC_msync_nocancel,
	BSC_fcntl,
	BSC_fcntl_nocancel,
	BSC_ioctl,
	BSC_stat64,
	BSC_fstat64,
	BSC_lstat64,
	BSC_stat64_extended,
	BSC_lstat64_extended,
	BSC_fstat64_extended,
	BSC_getdirentries64,
	BSC_statfs64,
	BSC_fstatfs64,
	BSC_pthread_chdir,
	BSC_pthread_fchdir,
	BSC_getfsstat,
	BSC_getfsstat64,
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

#define EVENT_BASE 60000

int num_events = EVENT_BASE;


#define DBG_FUNC_ALL	(DBG_FUNC_START | DBG_FUNC_END)
#define DBG_FUNC_MASK	0xfffffffc

double divisor = 0.0;       /* Trace divisor converts to microseconds */

int mib[6];
size_t needed;
char  *my_buffer;

kbufinfo_t bufinfo = {0, 0, 0, 0, 0};


/* defines for tracking file descriptor state */
#define FS_USAGE_FD_SETSIZE 256		/* Initial number of file descriptors per
					   thread that we will track */

#define FS_USAGE_NFDBITS      (sizeof (unsigned long) * 8)
#define FS_USAGE_NFDBYTES(n)  (((n) / FS_USAGE_NFDBITS) * sizeof (unsigned long))

int trace_enabled = 0;
int set_remove_flag = 1;

char *RAW_file = (char *)0;
int   RAW_flag = 0;
int   RAW_fd = 0;

uint64_t sample_TOD_secs;
uint32_t sample_TOD_usecs;

double	bias_now = 0.0;
double start_time = 0.0;
double end_time = 999999999999.9;


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


void getdivisor()
{
	struct mach_timebase_info mti;

	mach_timebase_info(&mti);

	divisor = ((double)mti.denom / (double)mti.numer) * 1000;
}


int
exit_usage(char *myname) {

	fprintf(stderr, "Usage: %s [-e] [-w] [-f mode] [-R rawfile [-S start_time] [-E end_time]] [pid | cmd [pid | cmd]....]\n", myname);
	fprintf(stderr, "  -e    exclude the specified list of pids from the sample\n");
	fprintf(stderr, "        and exclude fs_usage by default\n");
	fprintf(stderr, "  -w    force wider, detailed, output\n");
	fprintf(stderr, "  -f    Output is based on the mode provided\n");
	fprintf(stderr, "          mode = \"network\"  Show only network related output\n");
	fprintf(stderr, "          mode = \"filesys\"  Show only file system related output\n");
	fprintf(stderr, "          mode = \"pathname\" Show only pathname related output\n");
	fprintf(stderr, "          mode = \"exec\"     Show only execs\n");
	fprintf(stderr, "          mode = \"cachehit\" In addition, show cachehits\n");
	fprintf(stderr, "  -R    specifies a raw trace file to process\n");
	fprintf(stderr, "  -S    if -R is specified, selects a start point in microseconds\n");
	fprintf(stderr, "  -E    if -R is specified, selects an end point in microseconds\n");
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

		    case BSC_sendfile:
		      bsd_syscalls[code].sc_name = "sendfile";
		      bsd_syscalls[code].sc_format = FMT_FD;		/* this should be changed to FMT_SENDFILE */
		      break;						/* once we add an extended info trace event */
		    
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
                    
		    case BSC_stat64:
		      bsd_syscalls[code].sc_name = "stat64";
		      break;
                    
		    case BSC_stat_extended:
		      bsd_syscalls[code].sc_name = "stat_extended";
		      break;
                    
		    case BSC_stat64_extended:
		      bsd_syscalls[code].sc_name = "stat_extended64";
		      break;

		    case BSC_mount:
		      bsd_syscalls[code].sc_name = "mount";
		      bsd_syscalls[code].sc_format = FMT_MOUNT;
		      break;

		    case BSC_unmount:
		      bsd_syscalls[code].sc_name = "unmount";
		      bsd_syscalls[code].sc_format = FMT_UNMOUNT;
		      break;

		    case BSC_exit:
		      bsd_syscalls[code].sc_name = "exit";
		      break;

		    case BSC_execve:
		      bsd_syscalls[code].sc_name = "execve";
		      break;
                    
		    case BSC_posix_spawn:
		      bsd_syscalls[code].sc_name = "posix_spawn";
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

		    case BSC_fstat64:
		      bsd_syscalls[code].sc_name = "fstat64";
		      bsd_syscalls[code].sc_format = FMT_FD;
		      break;

		    case BSC_fstat_extended:
		      bsd_syscalls[code].sc_name = "fstat_extended";
		      bsd_syscalls[code].sc_format = FMT_FD;
		      break;

		    case BSC_fstat64_extended:
		      bsd_syscalls[code].sc_name = "fstat64_extended";
		      bsd_syscalls[code].sc_format = FMT_FD;
		      break;

		    case BSC_lstat:
		      bsd_syscalls[code].sc_name = "lstat";
		      break;

		    case BSC_lstat64:
		      bsd_syscalls[code].sc_name = "lstat64";
		      break;

		    case BSC_lstat_extended:
		      bsd_syscalls[code].sc_name = "lstat_extended";
		      break;

		    case BSC_lstat64_extended:
		      bsd_syscalls[code].sc_name = "lstat_extended64";
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

		    case BSC_umask:
		      bsd_syscalls[code].sc_name = "umask";
		      bsd_syscalls[code].sc_format = FMT_UMASK;
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
                    
		    case BSC_pthread_chdir:
		      bsd_syscalls[code].sc_name = "pthread_chdir";
		      break;
                    
		    case BSC_chroot:
		      bsd_syscalls[code].sc_name = "chroot";
		      break;
                    
		    case BSC_utimes:
		      bsd_syscalls[code].sc_name = "utimes";
		      break;
                    
		    case BSC_delete:
		      bsd_syscalls[code].sc_name = "delete-Carbon";
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
                    
		    case BSC_pthread_fchdir:
		      bsd_syscalls[code].sc_name = "pthread_fchdir";
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

		    case BSC_statfs64:
		      bsd_syscalls[code].sc_name = "statfs64";
		      break;

		    case BSC_getfsstat:
		      bsd_syscalls[code].sc_name = "getfsstat";
		      break;

		    case BSC_getfsstat64:
		      bsd_syscalls[code].sc_name = "getfsstat64";
		      break;

		    case BSC_fstatfs:
		      bsd_syscalls[code].sc_name = "fstatfs";
		      bsd_syscalls[code].sc_format = FMT_FD;
		      break;

		    case BSC_fstatfs64:
		      bsd_syscalls[code].sc_name = "fstatfs64";
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

		    case BSC_getdirentries64:
		      bsd_syscalls[code].sc_name = "getdirentries64";
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

	if (0 != reexec_to_match_kernel()) {
		fprintf(stderr, "Could not re-execute: %d\n", errno);
		exit(1);
	}
	get_screenwidth();

	/*
	 * get our name
	 */
	if (argc > 0) {
		if ((myname = rindex(argv[0], '/')) == 0)
			myname = argv[0];
		else
			myname++;
	}
	
	while ((ch = getopt(argc, argv, "ewf:R:S:E:")) != EOF) {

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

	       case 'R':
		   RAW_flag = 1;
		   RAW_file = optarg;
		   break;
		       
	       case 'S':
		   start_time = atof(optarg);
		   break;

	       case 'E':
		   end_time = atof(optarg);
		   break;

	       default:
		 exit_usage(myname);		 
	       }
	}
	if (!RAW_flag) {
		if ( geteuid() != 0 ) {
			fprintf(stderr, "'fs_usage' must be run as root...\n");
			exit(1);
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
	if (exclude_pids || (!exclude_pids && argc == 0)) {
		if (num_of_pids < (MAX_PIDS - 1))
			pids[num_of_pids++] = getpid();
	}

	/*
	 * If we process any list of pids/cmds, then turn off the defaults
	 */
	if (argc > 0)
		exclude_default_pids = 0;

	while (argc > 0 && num_of_pids < (MAX_PIDS - 1)) {
		select_pid_mode++;
		argtopid(argv[0]);
		argc--;
		argv++;
	}
	/*
	 * Exclude a set of default pids
	 */
	if (exclude_default_pids) {
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
	for (i = 0; i < num_of_pids; i++) {
		if (exclude_pids)
			fprintf(stderr, "exclude pid %d\n", pids[i]);
		else
			fprintf(stderr, "pid %d\n", pids[i]);
	}
#endif
	if (!RAW_flag) {
		struct sigaction osa;
		int	num_cpus;
		size_t	len;

		/* set up signal handlers */
		signal(SIGINT, leave);
		signal(SIGQUIT, leave);

		sigaction(SIGHUP, (struct sigaction *)NULL, &osa);

		if (osa.sa_handler == SIG_DFL)
			signal(SIGHUP, leave);
		signal(SIGTERM, leave);
		/*
		 * grab the number of cpus
		 */
		mib[0] = CTL_HW;
		mib[1] = HW_NCPU;
		mib[2] = 0;
		len = sizeof(num_cpus);

		sysctl(mib, 2, &num_cpus, &len, NULL, 0);
		num_events = EVENT_BASE * num_cpus;
	}
	signal(SIGWINCH, sigwinch);

	if ((my_buffer = malloc(num_events * sizeof(kd_buf))) == (char *)0)
		quit("can't allocate memory for tracing info\n");

	if (ReadSharedCacheMap("/var/db/dyld/dyld_shared_cache_i386.map", &framework32, "/var/db/dyld/dyld_shared_cache_i386")) {
		ReadSharedCacheMap("/var/db/dyld/dyld_shared_cache_x86_64.map", &framework64, "/var/db/dyld/dyld_shared_cache_x86_64");
	} else {
		ReadSharedCacheMap("/var/db/dyld/dyld_shared_cache_ppc.map", &framework32, "/var/db/dyld/dyld_shared_cache_ppc");
		ReadSharedCacheMap("/var/db/dyld/dyld_shared_cache_ppc64.map", &framework64, "/var/db/dyld/dyld_shared_cache_ppc64");
	}
	SortFrameworkAddresses();

        cache_disk_names();

	if (!RAW_flag) {

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
		if (select_pid_mode && !one_good_pid) {
			/* 
			 *  An attempt to restrict output to a given
			 *  pid or command has failed. Exit gracefully
			 */
			set_remove();
			exit_usage(myname);
		}
		set_enable(1);

		init_arguments_buffer();
	}
	getdivisor();

	init_tables();

	/*
	 * main loop
	 */
	while (1) {
		if (!RAW_flag)		
			usleep(1000 * usleep_ms);

		sample_sc();

		last_time = time((long *)0);
	}
}


void
find_proc_names()
{
        size_t			bufSize = 0;
	struct kinfo_proc	*kp;

	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC;
	mib[2] = KERN_PROC_ALL;
	mib[3] = 0;

	if (sysctl(mib, 4, NULL, &bufSize, NULL, 0) < 0)
		quit("trace facility failure, KERN_PROC_ALL\n");

	if ((kp = (struct kinfo_proc *)malloc(bufSize)) == (struct kinfo_proc *)0)
		quit("can't allocate memory for proc buffer\n");
	
	if (sysctl(mib, 4, kp, &bufSize, NULL, 0) < 0)
		quit("trace facility failure, KERN_PROC_ALL\n");

        kp_nentries = bufSize/ sizeof(struct kinfo_proc);
	kp_buffer = kp;
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
	} else
		one_good_pid++;
}

/* 
 * on_off == 0 turns off pid exclusion
 * on_off == 1 turns on pid exclusion
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

	if (sysctl(mib, 3, NULL, &needed, NULL, 0) < 0) {
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
	uint32_t my_buffer_size = 0;

	if (!RAW_flag)
		get_bufinfo(&bufinfo);
	else
		my_buffer_size = num_events * sizeof(kd_buf);

	if (need_new_map) {
	        read_command_map();
	        need_new_map = 0;
	}
	if (!RAW_flag) {
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

		if (count > (num_events / 8)) {
			if (usleep_ms > USLEEP_BEHIND)
				usleep_ms = USLEEP_BEHIND;
			else if (usleep_ms > USLEEP_MIN)
				usleep_ms /= 2;

		} else	if (count < (num_events / 16)) {
			if (usleep_ms < USLEEP_MAX)
		                usleep_ms *= 2;
		}

		if (bufinfo.flags & KDBG_WRAPPED) {
			fprintf(stderr, "fs_usage: buffer overrun, events generated too quickly: %d\n", count);

			delete_all_events();

			need_new_map = 1;
		
			set_enable(0);
			set_enable(1);
		}
	} else {
		int bytes_read;

                if ((bytes_read = read(RAW_fd, my_buffer, my_buffer_size)) < sizeof(kd_buf))
			exit(0);
		count = bytes_read / sizeof(kd_buf);
	}
	kd = (kd_buf *)my_buffer;
#if 0
	fprintf(stderr, "READTR returned %d items\n", count);
#endif
	for (i = 0; i < count; i++) {
		uint32_t debugid;
		uintptr_t thread;
		int type;
		int index;
		uintptr_t *sargptr;
		uint64_t now;
		long long l_usecs;
		int secs;
		long curr_time;
		th_info_t	ti;
                struct diskio  *dio;


		thread  = kd[i].arg5;
		debugid = kd[i].debugid;
		type    = kd[i].debugid & DBG_FUNC_MASK;

		now = kdbg_get_timestamp(&kd[i]);

		if (i == 0 && !RAW_flag) {

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
		if (RAW_flag && bias_now == 0.0)
			bias_now = now;
		
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

		case P_CS_ReadChunk:
		case P_CS_WriteChunk:
		case P_CS_MetaRead:
		case P_CS_MetaWrite:
		    insert_diskio(type, kd[i].arg2, kd[i].arg1, kd[i].arg3, kd[i].arg4, thread, (double)now);
		    continue;

		case P_CS_Originated_Read:
		case P_CS_Originated_Write:
		    insert_diskio(type, kd[i].arg2, CS_DEV, kd[i].arg3, kd[i].arg4, thread, (double)now);
		    continue;

		case P_CS_ReadChunkDone:
		case P_CS_WriteChunkDone:
		case P_CS_Originated_ReadDone:
		case P_CS_Originated_WriteDone:
		case P_CS_MetaReadDone:
		case P_CS_MetaWriteDone:
		    if ((dio = complete_diskio(kd[i].arg2, kd[i].arg4, kd[i].arg3, thread, (double)now))) {
			    print_diskio(dio);
			    free_diskio(dio);
		    }
		    continue;

		case TRACE_DATA_NEWTHREAD:
		    if (kd[i].arg1) {
			    if ((ti = add_event(thread, TRACE_DATA_NEWTHREAD)) == NULL)
				    continue;
			    ti->child_thread = kd[i].arg1;
			    ti->pid = kd[i].arg2;
		    }
		    continue;

		case TRACE_STRING_NEWTHREAD:
		    if ((ti = find_event(thread, TRACE_DATA_NEWTHREAD)) == (struct th_info *)0)
		            continue;

		    create_map_entry(ti->child_thread, ti->pid, (char *)&kd[i].arg1);

		    delete_event(ti);
		    continue;
	
		case TRACE_DATA_EXEC:
		    if ((ti = add_event(thread, TRACE_DATA_EXEC)) == NULL)
		            continue;

		    ti->pid = kd[i].arg1;
		    continue;	    

		case TRACE_STRING_EXEC:
		    if ((ti = find_event(thread, BSC_execve))) {
			    if (ti->pathname[0])
				    exit_event("execve", thread, BSC_execve, 0, 0, 0, 0, FMT_DEFAULT, (double)now);

		    } else if ((ti = find_event(thread, BSC_posix_spawn))) {
			    if (ti->pathname[0])
				    exit_event("posix_spawn", thread, BSC_posix_spawn, 0, 0, 0, 0, FMT_DEFAULT, (double)now);
		    }
		    if ((ti = find_event(thread, TRACE_DATA_EXEC)) == (struct th_info *)0)
			    continue;

		    create_map_entry(thread, ti->pid, (char *)&kd[i].arg1);

		    delete_event(ti);
		    continue;

		case BSC_thread_terminate:
		    delete_map_entry(thread);
		    continue;

		case BSC_exit:
		    continue;

		case proc_exit:
		    kd[i].arg1 = kd[i].arg2 >> 8;
		    type = BSC_exit;
		    break;

		case BSC_mmap:
		    if (kd[i].arg4 & MAP_ANON)
			    continue;
		    break;

		case MACH_idle:
		case MACH_sched:
		case MACH_stkhandoff:
                    mark_thread_waited(thread);
		    continue;

		case VFS_LOOKUP:
		    if ((ti = find_event(thread, 0)) == (struct th_info *)0)
		            continue;

		    if (debugid & DBG_FUNC_START) {
			    if (ti->pathname2[0])
				    continue;
			    if (ti->pathname[0] == 0)
				    sargptr = ti->pathname;
			    else
				    sargptr = ti->pathname2;
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
			     * handle and we only handle 2 pathname lookups for
			     * a given system call
			     */
			    if (sargptr == 0)
				    continue;
			    if (ti->pathname2[0]) {
				    if ((uintptr_t)sargptr >= (uintptr_t)&ti->pathname2[NUMPARMS])
					    continue;
			    } else {
				    if ((uintptr_t)sargptr >= (uintptr_t)&ti->pathname[NUMPARMS])
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

			    if (debugid & DBG_FUNC_END) {
				    if (ti->pathname2[0])
					    ti->pathptr = 0;
				    else
					    ti->pathptr = ti->pathname2;
			    } else
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

			enter_event(thread, type, &kd[i], p, (double)now);
			continue;
		}

		switch (type) {

		case Throttled:
		     exit_event("  THROTTLED", thread, type, 0, 0, 0, 0, FMT_DEFAULT, (double)now);
		     continue;

		case P_CS_SYNC_DISK:
		     exit_event("  SyncCacheCS", thread, type, kd[i].arg1, 0, 0, 0, FMT_SYNC_DISK_CS, (double)now);
		     continue;

		case SPEC_ioctl:
		     if (kd[i].arg2 == DKIOCSYNCHRONIZECACHE)
			     exit_event("IOCTL", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, FMT_SPEC_IOCTL, (double)now);
		     else {
			     if ((ti = find_event(thread, type)))
				     delete_event(ti);
		     }
		     continue;

		case MACH_pageout:
		     if (kd[i].arg2) 
			     exit_event("PAGE_OUT_ANON", thread, type, 0, kd[i].arg1, 0, 0, FMT_PGOUT, (double)now);
		     else
			     exit_event("PAGE_OUT_FILE", thread, type, 0, kd[i].arg1, 0, 0, FMT_PGOUT, (double)now);
		     continue;

		case MACH_vmfault:
		     if (kd[i].arg4 == DBG_PAGEIN_FAULT)
			     exit_event("PAGE_IN", thread, type, 0, kd[i].arg1, kd[i].arg2, 0, FMT_PGIN, (double)now);
		     else if (kd[i].arg4 == DBG_PAGEINV_FAULT)
			     exit_event("PAGE_IN_FILE", thread, type, 0, kd[i].arg1, kd[i].arg2, 0, FMT_PGIN, (double)now);
		     else if (kd[i].arg4 == DBG_PAGEIND_FAULT)
			     exit_event("PAGE_IN_ANON", thread, type, 0, kd[i].arg1, kd[i].arg2, 0, FMT_PGIN, (double)now);
		     else if (kd[i].arg4 == DBG_CACHE_HIT_FAULT)
			     exit_event("CACHE_HIT", thread, type, 0, kd[i].arg1, kd[i].arg2, 0, FMT_CACHEHIT, (double)now);
		     else {
			     if ((ti = find_event(thread, type)))
				     delete_event(ti);
		     }
		     continue;

		case MSC_map_fd:
		     exit_event("map_fd", thread, type, kd[i].arg1, kd[i].arg2, 0, 0, FMT_FD, (double)now);
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

		        if ((index = BSC_INDEX(type)) >= MAX_BSD_SYSCALL)
			        continue;

			if (bsd_syscalls[index].sc_name) {
				exit_event(bsd_syscalls[index].sc_name, thread, type, kd[i].arg1, kd[i].arg2, kd[i].arg3, kd[i].arg4,
					   bsd_syscalls[index].sc_format, (double)now);

				if (type == BSC_exit)
					delete_map_entry(thread);
			}
		} else if ((type & CLASS_MASK) == FILEMGR_BASE) {
		
			if ((index = filemgr_index(type)) >= MAX_FILEMGR)
			        continue;

			if (filemgr_calls[index].fm_name) {
				exit_event(filemgr_calls[index].fm_name, thread, type, kd[i].arg1, kd[i].arg2, kd[i].arg3, kd[i].arg4,
					   FMT_DEFAULT, (double)now);
			}
		}
	}
	fflush(0);
}


void
enter_event_now(uintptr_t thread, int type, kd_buf *kd, char *name, double now)
{
	th_info_t	ti;
	threadmap_t	tme;
	int secs;
	int usecs;
	long long l_usecs;
	long curr_time;
	int clen = 0;
	int tsclen = 0;
	int nmclen = 0;
	int argsclen = 0;
	char buf[MAXWIDTH];

	if ((ti = add_event(thread, type)) == NULL)
		return;

	ti->stime  = now;
	ti->arg1   = kd->arg1;
	ti->arg2   = kd->arg2;
	ti->arg3   = kd->arg3;
	ti->arg4   = kd->arg4;

	if ((type & CLASS_MASK) == FILEMGR_BASE &&
	    (!RAW_flag || (now >= start_time && now <= end_time))) {

		filemgr_in_progress++;
		ti->in_filemgr = 1;

		if (RAW_flag) {
			l_usecs = (long long)((now - bias_now) / divisor);
			l_usecs += (sample_TOD_secs * 1000000) + sample_TOD_usecs;
		} else
			l_usecs = (long long)(now / divisor);
		secs = l_usecs / 1000000;
		curr_time = bias_secs + secs;
		   
		sprintf(buf, "%-8.8s", &(ctime(&curr_time)[11]));
		tsclen = strlen(buf);

		if (columns > MAXCOLS || wideflag) {
			usecs = l_usecs - (long long)((long long)secs * 1000000);
			sprintf(&buf[tsclen], ".%06ld", (long)usecs);
			tsclen = strlen(buf);
		}

		/*
		 * Print timestamp column
		 */
		printf("%s", buf);

		tme = find_map_entry(thread);
		if (tme) {
			sprintf(buf, "  %-25.25s ", name);
			nmclen = strlen(buf);
			printf("%s", buf);

			sprintf(buf, "(%d, 0x%lx, 0x%lx, 0x%lx)", (short)kd->arg1, kd->arg2, kd->arg3, kd->arg4);
			argsclen = strlen(buf);
		       
			/*
			 * Calculate white space out to command
			 */
			if (columns > MAXCOLS || wideflag) {
				clen = columns - (tsclen + nmclen + argsclen + 20 + 11);
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
				printf("%s.%d\n", tme->tm_command, (int)thread); 
			else
				printf("%-12.12s\n", tme->tm_command); 
		} else
			printf("  %-24.24s (%5d, %#lx, 0x%lx, 0x%lx)\n",         name, (short)kd->arg1, kd->arg2, kd->arg3, kd->arg4);
	}
}


void
enter_event(uintptr_t thread, int type, kd_buf *kd, char *name, double now)
{
	int index;

	if (type == MACH_pageout || type == MACH_vmfault || type == MSC_map_fd || type == SPEC_ioctl || type == Throttled || type == P_CS_SYNC_DISK) {
		enter_event_now(thread, type, kd, name, now);
		return;
	}
	if ((type & CSC_MASK) == BSC_BASE) {

		if ((index = BSC_INDEX(type)) >= MAX_BSD_SYSCALL)
			return;

		if (bsd_syscalls[index].sc_name)
			enter_event_now(thread, type, kd, name, now);
		return;
	}
	if ((type & CLASS_MASK) == FILEMGR_BASE) {

		if ((index = filemgr_index(type)) >= MAX_FILEMGR)
			return;
	       
		if (filemgr_calls[index].fm_name)
			enter_event_now(thread, type, kd, name, now);
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
extend_syscall(uintptr_t thread, int type, kd_buf *kd)
{
	th_info_t	ti;

	switch (type) {
	case BSC_mmap_extended:
	    if ((ti = find_event(thread, BSC_mmap)) == (struct th_info *)0)
		    return;
	    ti->arg8   = ti->arg3;  /* save protection */
	    ti->arg1   = kd->arg1;  /* the fd */
	    ti->arg3   = kd->arg2;  /* bottom half address */
	    ti->arg5   = kd->arg3;  /* bottom half size */	   
	    break;
	case BSC_mmap_extended2:
	    if ((ti = find_event(thread, BSC_mmap)) == (struct th_info *)0)
		    return;
	    ti->arg2   = kd->arg1;  /* top half address */
	    ti->arg4   = kd->arg2;  /* top half size */
	    ti->arg6   = kd->arg3;  /* top half file offset */
	    ti->arg7   = kd->arg4;  /* bottom half file offset */
	    break;
	case BSC_msync_extended:
	    if ((ti = find_event(thread, BSC_msync)) == (struct th_info *)0) {
		    if ((ti = find_event(thread, BSC_msync_nocancel)) == (struct th_info *)0)
			    return;
	    }
	    ti->arg4   = kd->arg1;  /* top half address */
	    ti->arg5   = kd->arg2;  /* top half size */
	    break;
	case BSC_pread_extended:
	    if ((ti = find_event(thread, BSC_pread)) == (struct th_info *)0) {
		    if ((ti = find_event(thread, BSC_pread_nocancel)) == (struct th_info *)0)
			    return;
	    }
	    ti->arg1   = kd->arg1;  /* the fd */
	    ti->arg2   = kd->arg2;  /* nbytes */
	    ti->arg3   = kd->arg3;  /* top half offset */
	    ti->arg4   = kd->arg4;  /* bottom half offset */	   
	    break;
	case BSC_pwrite_extended:
	    if ((ti = find_event(thread, BSC_pwrite)) == (struct th_info *)0) {
		    if ((ti = find_event(thread, BSC_pwrite_nocancel)) == (struct th_info *)0)
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
exit_event(char *sc_name, uintptr_t thread, int type, int arg1, int arg2, int arg3, int arg4,
	   int format, double now)
{
        th_info_t	ti;
      
	if ((ti = find_event(thread, type)) == (struct th_info *)0)
	        return;

	if (check_filter_mode(ti, type, arg1, arg2, sc_name))
	        format_print(ti, sc_name, thread, type, arg1, arg2, arg3, arg4, format, now, ti->stime, ti->waited, (char *)ti->pathname, NULL);

	if ((type & CLASS_MASK) == FILEMGR_BASE) {
	        ti->in_filemgr = 0;

		if (filemgr_in_progress > 0)
		        filemgr_in_progress--;
	}
	delete_event(ti);
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
format_print(struct th_info *ti, char *sc_name, uintptr_t thread, int type, int arg1, int arg2, int arg3, int arg4,
	     int format, double now, double stime, int waited, char *pathname, struct diskio *dio)
{
        int secs;
	int usecs;
	int nopadding = 0;
	long long l_usecs;
	long curr_time;
	char *command_name;
	int in_filemgr = 0;
	int len = 0;
	int clen = 0;
	int tlen = 0;
	int class;
	uint64_t user_addr;
	uint64_t user_size;
	char *framework_name;
	char *framework_type;
	char *p1;
	char *p2;
	char buf[MAXWIDTH];
	char cs_diskname[32];
	command_name = "";
	static char timestamp[32];
	static int  last_timestamp = -1;
	static int  timestamp_len = 0;

	if (RAW_flag) {
		l_usecs = (long long)((now - bias_now) / divisor);

		if ((double)l_usecs < start_time || (double)l_usecs > end_time)
			return;

		l_usecs += (sample_TOD_secs * 1000000) + sample_TOD_usecs;
	}
	else
		l_usecs = (long long)(now / divisor);
	secs = l_usecs / 1000000;
	curr_time = bias_secs + secs;

	class = type >> 24;

	if (dio)
	        command_name = dio->issuing_command;
	else {
		threadmap_t	tme;

		if ((tme = find_map_entry(thread)))
		        command_name = tme->tm_command;
	}
	if (last_timestamp != curr_time) {
	        timestamp_len = sprintf(timestamp, "%-8.8s", &(ctime(&curr_time)[11]));
		last_timestamp = curr_time;
	}
	if (columns > MAXCOLS || wideflag) {
	        int usec;

		tlen = timestamp_len;
		nopadding = 0;
		usec = (l_usecs - (long long)((long long)secs * 1000000));

		sprintf(&timestamp[tlen], ".%06ld", (long)usec);
		tlen += 7;

		timestamp[tlen] = '\0';

		if (filemgr_in_progress) {
		        if (class != FILEMGR_CLASS) {
			        if (find_event(thread, -1))
				        in_filemgr = 1;
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
       

	framework_name = NULL;

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

		        lookup_name(user_addr, &framework_type, &framework_name);
			clen += clip_64bit(" A=", user_addr);
			break;

		      case FMT_CACHEHIT:
			/*
			 * cache hit
			 */
			user_addr = ((uint64_t)arg2 << 32) | (uint32_t)arg3;

		        lookup_name(user_addr, &framework_type, &framework_name);
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

		      case FMT_DISKIO_CS:
		        /*
			 * physical disk I/O
			 */
		        if (dio->io_errno)
			        clen += printf(" D=0x%8.8x  [%3d]", dio->blkno, dio->io_errno);
			else
			        clen += printf(" D=0x%8.8x  B=0x%-6x   /dev/%s", dio->blkno, dio->iosize, generate_cs_disk_name(dio->dev, &cs_diskname[0]));
			break;

		      case FMT_SYNC_DISK_CS:
		        /*
			 * physical disk sync cache
			 */
			clen += printf("                            /dev/%s", generate_cs_disk_name(arg1, &cs_diskname[0]));

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
			int  fd = -1;

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

			case F_OPENFROM:
			  p = "OPENFROM";
			  
			  if (arg1 == 0)
				  fd = arg2;
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
			} else
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

		      case FMT_SPEC_IOCTL:
		      {
			/*
			 * fcntl
			 */
			clen += printf(" <DKIOCSYNCHRONIZECACHE>    /dev/%s", find_disk_name(arg1));

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

			if (mlen < 19) {
			        memset(&buf[mlen], ' ', 19 - mlen);
				mlen = 19;
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
			if (format == FMT_UMASK)
				mode = ti->arg1;
			else if (format == FMT_FCHMOD || format == FMT_CHMOD)
			        mode = ti->arg2;
			else
			        mode = ti->arg4;

			get_mode_string(mode, &buf[0]);

			if (arg1 == 0)
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

		      case FMT_MOUNT:
		      {
			      if (arg1)
				      clen += printf("      [%3d] <FLGS=0x%x> ", arg1, ti->arg3);
			      else
				      clen += printf("     <FLGS=0x%x> ", ti->arg3);

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

			      if (arg1)
				      clen += printf("      [%3d] %s  ", arg1, mountflag);
			      else
				      clen += printf("     %s         ", mountflag);

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
	        clen =  columns - (clen + 14 + 20 + 11);
	else
	        clen =  columns - (clen + 14 + 12);

	if (class != FILEMGR_CLASS && !nopadding)
	        clen -= 3;

	if (framework_name)
	        len = sprintf(&buf[0], " %s %s ", framework_type, framework_name);
	else if (*pathname != '\0') {
	        len = sprintf(&buf[0], " %s ", pathname);

		if (format == FMT_MOUNT && ti->pathname2[0]) {
			int	len2;

			memset(&buf[len], ' ', 2);

			len2 = sprintf(&buf[len+2], " %s ", (char *)ti->pathname2);
			len = len + 2 + len2;
		}
	} else
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
	        printf("%s%s %3ld.%06ld%s %s.%d\n", p1, pathname, (unsigned long)secs, (unsigned long)usecs, p2, command_name, (int)thread);
	else
	        printf("%s%s %3ld.%06ld%s %-12.12s\n", p1, pathname, (unsigned long)secs, (unsigned long)usecs, p2, command_name);
}


void
delete_event(th_info_t ti_to_delete) {
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

th_info_t
add_event(uintptr_t thread, int type) {
	th_info_t	ti;
	int		hashid;

	if ((ti = th_info_freelist))
		th_info_freelist = ti->next;
	else
		ti = (th_info_t)malloc(sizeof(struct th_info));

	hashid = thread & HASH_MASK;

	ti->next = th_info_hash[hashid];
	th_info_hash[hashid] = ti;

	ti->thread = thread;
	ti->type = type;

	ti->waited = 0;
	ti->in_filemgr = 0;
	ti->pathptr = ti->pathname;
	ti->pathname[0] = 0;
	ti->pathname2[0] = 0;

	return (ti);
}

th_info_t
find_event(uintptr_t thread, int type) {
        th_info_t	ti;
	int		hashid;

	hashid = thread & HASH_MASK;

	for (ti = th_info_hash[hashid]; ti; ti = ti->next) {
		if (ti->thread == thread) {
		        if (type == ti->type)
			        return (ti);
			if (ti->in_filemgr) {
			        if (type == -1)
				        return (ti);
				continue;
			}
			if (type == 0)
			        return (ti);
		}
	}
	return ((th_info_t) 0);
}

void
delete_all_events() {
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
mark_thread_waited(uintptr_t thread) {
        th_info_t	ti;
	int		hashid;

	hashid = thread & HASH_MASK;

	for (ti = th_info_hash[hashid]; ti; ti = ti->next) {
		if (ti->thread == thread)
			ti->waited = 1;
	}
}


void read_command_map()
{
	size_t	size;
	int	i;
	int total_threads = 0;
	kd_threadmap *mapptr = 0;

	delete_all_map_entries();

	if (!RAW_flag) {

		total_threads = bufinfo.nkdthreads;
		size = bufinfo.nkdthreads * sizeof(kd_threadmap);

		if (size) {
			if ((mapptr = (kd_threadmap *) malloc(size))) {
				int mib[6];

				bzero (mapptr, size);
				/*
				 * Now read the threadmap
				 */
				mib[0] = CTL_KERN;
				mib[1] = KERN_KDEBUG;
				mib[2] = KERN_KDTHRMAP;
				mib[3] = 0;
				mib[4] = 0;
				mib[5] = 0;		/* no flags */

				if (sysctl(mib, 3, mapptr, &size, NULL, 0) < 0) {
					/*
					 * This is not fatal -- just means I cant map command strings
					 */
					free(mapptr);
					return;
				}
			}
		}
	} else {
		RAW_header	header;
		off_t	offset;

		RAW_fd = open(RAW_file, O_RDONLY);

		if (RAW_fd < 0) {
			perror("Can't open RAW file");
			exit(1);
		}
		if (read(RAW_fd, &header, sizeof(RAW_header)) != sizeof(RAW_header)) {
			perror("read failed");
			exit(2);
		}
		if (header.version_no != RAW_VERSION1) {
			header.version_no = RAW_VERSION0;
			header.TOD_secs = time((long *)0);
			header.TOD_usecs = 0;

			lseek(RAW_fd, (off_t)0, SEEK_SET);

			if (read(RAW_fd, &header.thread_count, sizeof(int)) != sizeof(int)) {
				perror("read failed");
				exit(2);
			}
		}
		sample_TOD_secs = header.TOD_secs;
		sample_TOD_usecs = header.TOD_usecs;

		total_threads = header.thread_count;
		size = total_threads * sizeof(kd_threadmap);

		if (size) {
			if ((mapptr = (kd_threadmap *) malloc(size))) {
				bzero (mapptr, size);

				if (read(RAW_fd, mapptr, size) != size) {
					free(mapptr);
					return;
				}
			}
		}
		if (header.version_no != RAW_VERSION0) {
			offset = lseek(RAW_fd, (off_t)0, SEEK_CUR);
			offset = (offset + (4095)) & ~4095;

			lseek(RAW_fd, offset, SEEK_SET);
		}
	}
	for (i = 0; i < total_threads; i++)
		create_map_entry(mapptr[i].thread, mapptr[i].valid, &mapptr[i].command[0]);

        free(mapptr);
}


void delete_all_map_entries()
{
	threadmap_t     tme = 0;
        threadmap_t     tme_next = 0;
	int             i;

	for (i = 0; i < HASH_SIZE; i++) {

		for (tme = threadmap_hash[i]; tme; tme = tme_next) {
                        if (tme->tm_setptr)
				free(tme->tm_setptr);
			tme_next = tme->tm_next;
                        tme->tm_next = threadmap_freelist;
			threadmap_freelist = tme;
                }
                threadmap_hash[i] = 0;
	}
}


void create_map_entry(uintptr_t thread, int pid, char *command)
{
        threadmap_t     tme;
        int             hashid;

        if ((tme = threadmap_freelist))
                threadmap_freelist = tme->tm_next;
        else
                tme = (threadmap_t)malloc(sizeof(struct threadmap));

        tme->tm_thread  = thread;
	tme->tm_setsize = 0;
	tme->tm_setptr  = 0;

        (void)strncpy (tme->tm_command, command, MAXCOMLEN);
        tme->tm_command[MAXCOMLEN] = '\0';

        hashid = thread & HASH_MASK;

        tme->tm_next = threadmap_hash[hashid];
        threadmap_hash[hashid] = tme;

	if (pid != 0 && pid != 1) {
		if (!strncmp(command, "LaunchCFMA", 10))
			(void)get_real_command_name(pid, tme->tm_command, MAXCOMLEN);
	}
}


threadmap_t
find_map_entry(uintptr_t thread)
{
        threadmap_t     tme;
        int     hashid;

	hashid = thread & HASH_MASK;

        for (tme = threadmap_hash[hashid]; tme; tme = tme->tm_next) {
		if (tme->tm_thread == thread)
			return (tme);
	}
        return (0);
}


void
delete_map_entry(uintptr_t thread)
{
        threadmap_t     tme = 0;
        threadmap_t     tme_prev;
        int             hashid;

        hashid = thread & HASH_MASK;

        if ((tme = threadmap_hash[hashid])) {
                if (tme->tm_thread == thread)
			threadmap_hash[hashid] = tme->tm_next;
                else {
                        tme_prev = tme;

			for (tme = tme->tm_next; tme; tme = tme->tm_next) {
                                if (tme->tm_thread == thread) {
                                        tme_prev->tm_next = tme->tm_next;
                                        break;
				}
                                tme_prev = tme;
			}
		}
                if (tme) {
			if (tme->tm_setptr)
				free(tme->tm_setptr);

                        tme->tm_next = threadmap_freelist;
			threadmap_freelist = tme;
		}
	}
}


void
fs_usage_fd_set(uintptr_t thread, unsigned int fd)
{
	threadmap_t	tme;

	if ((tme = find_map_entry(thread)) == 0)
		return;
	/*
	 * If the map is not allocated, then now is the time
	 */
	if (tme->tm_setptr == (unsigned long *)0) {
		if ((tme->tm_setptr = (unsigned long *)malloc(FS_USAGE_NFDBYTES(FS_USAGE_FD_SETSIZE))) == 0)
			return;

		tme->tm_setsize = FS_USAGE_FD_SETSIZE;
		bzero(tme->tm_setptr, (FS_USAGE_NFDBYTES(FS_USAGE_FD_SETSIZE)));
	}
	/*
	 * If the map is not big enough, then reallocate it
	 */
	while (tme->tm_setsize <= fd) {
		int	n;

		n = tme->tm_setsize * 2;
		tme->tm_setptr = (unsigned long *)realloc(tme->tm_setptr, (FS_USAGE_NFDBYTES(n)));

		bzero(&tme->tm_setptr[(tme->tm_setsize/FS_USAGE_NFDBITS)], (FS_USAGE_NFDBYTES(tme->tm_setsize)));
		tme->tm_setsize = n;
	}
	/*
	 * set the bit
	 */
	tme->tm_setptr[fd/FS_USAGE_NFDBITS] |= (1 << ((fd) % FS_USAGE_NFDBITS));
}


/*
 * Return values:
 *  0 : File Descriptor bit is not set
 *  1 : File Descriptor bit is set
 */
int
fs_usage_fd_isset(uintptr_t thread, unsigned int fd)
{
	threadmap_t	tme;
	int		ret = 0;

	if ((tme = find_map_entry(thread))) {
		if (tme->tm_setptr && fd < tme->tm_setsize)
			ret = tme->tm_setptr[fd/FS_USAGE_NFDBITS] & (1 << (fd % FS_USAGE_NFDBITS));
	}
	return (ret);
}
    

void
fs_usage_fd_clear(uintptr_t thread, unsigned int fd)
{
	threadmap_t	tme;

	if ((tme = find_map_entry(thread))) {
		if (tme->tm_setptr && fd < tme->tm_setsize)
			tme->tm_setptr[fd/FS_USAGE_NFDBITS] &= ~(1 << (fd % FS_USAGE_NFDBITS));
	}
}



void
argtopid(char *str)
{
        char *cp;
        int ret;
	int i;

        ret = (int)strtol(str, &cp, 10);

        if (cp == str || *cp) {
		/*
		 * Assume this is a command string and find matching pids
		 */
	        if (!kp_buffer)
		        find_proc_names();

	        for (i = 0; i < kp_nentries && num_of_pids < (MAX_PIDS - 1); i++) {
			if (kp_buffer[i].kp_proc.p_stat == 0)
				continue;
			else {
				if (!strncmp(str, kp_buffer[i].kp_proc.p_comm,
					     sizeof(kp_buffer[i].kp_proc.p_comm) -1))
					pids[num_of_pids++] = kp_buffer[i].kp_proc.p_pid;
			}
		}
	}
	else if (num_of_pids < (MAX_PIDS - 1))
	        pids[num_of_pids++] = ret;
}



void
lookup_name(uint64_t user_addr, char **type, char **name) 
{       
        int i;
	int start, last;
	
	*name = NULL;
	*type = NULL;

	if (numFrameworks) {

		if ((user_addr >= framework32.b_address && user_addr < framework32.e_address) ||
		    (user_addr >= framework64.b_address && user_addr < framework64.e_address)) {
			      
			start = 0;
			last  = numFrameworks;

			for (i = numFrameworks / 2; start < last; i = start + ((last - start) / 2)) {
				if (user_addr > frameworkInfo[i].e_address)
					start = i+1;
				else
					last = i;
			}
			if (start < numFrameworks &&
			    user_addr >= frameworkInfo[start].b_address && user_addr < frameworkInfo[start].e_address) {
				*type = frameworkType[frameworkInfo[start].r_type];
				*name = frameworkInfo[start].name;
			}
		}
	}
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

	for (p = inputstring; n < maxtokens && p != NULL; ) {

		while ((val = strsep(&p, " \t")) != NULL && *val == '\0');

		*ap++ = val;
		n++;
	}
	*ap = 0;

	return n;
}


int ReadSharedCacheMap(const char *path, LibraryRange *lr, char *linkedit_name)
{
	uint64_t	b_address, e_address;
	char	buf[1024];
	char	*fnp;
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
		if ((substring = strrchr(buf, '.')))
		{		
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
		}
		else 
		{
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
		fnp = (char *)malloc(strlen(frameworkName) + 1);
		strcpy(fnp, frameworkName);

		while (fgets(buf, 1023, fd) && numFrameworks < (MAXINDEX - 2)) {
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

				frameworkInfo[numFrameworks].b_address	= b_address;
				frameworkInfo[numFrameworks].e_address	= e_address;
				frameworkInfo[numFrameworks].r_type	= type;
				
				if (type == LINKEDIT_R) {
					frameworkInfo[numFrameworks].name = linkedit_name;
					linkedit_found = 1;
				} else
					frameworkInfo[numFrameworks].name = fnp;
#if 0
				printf("%s(%d): %qx-%qx\n", frameworkInfo[numFrameworks].name, type, b_address, e_address);
#endif
				if (lr->b_address == 0 || b_address < lr->b_address)
					lr->b_address = b_address;

				if (lr->e_address == 0 || e_address > lr->e_address)
					lr->e_address = e_address;

				numFrameworks++;
			}
			if (type == LINKEDIT_R)
				break;
		}
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
SortFrameworkAddresses()
{

	frameworkInfo[numFrameworks].b_address = frameworkInfo[numFrameworks - 1].b_address + 0x800000;
	frameworkInfo[numFrameworks].e_address = frameworkInfo[numFrameworks].b_address;
	frameworkInfo[numFrameworks].name = (char *)0;

	qsort(frameworkInfo, numFrameworks, sizeof(LibraryInfo), compareFrameworkAddress);
}


struct diskio *insert_diskio(int type, int bp, int dev, int blkno, int io_size, uintptr_t thread, double curtime)
{
	struct diskio	*dio;
	threadmap_t	tme;
    
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
	
	if ((tme = find_map_entry(thread))) {
		strncpy(dio->issuing_command, tme->tm_command, MAXCOMLEN);
		dio->issuing_command[MAXCOMLEN-1] = '\0';
	} else
		strcpy(dio->issuing_command, "");
    
	dio->next = busy_diskios;
	if (dio->next)
		dio->next->prev = dio;
	busy_diskios = dio;

	return (dio);
}


struct diskio *complete_diskio(int bp, int io_errno, int resid, uintptr_t thread, double curtime)
{
	struct diskio *dio;
    
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
	char  *p = NULL;
	int   len = 0;
	int   type;
	int   format = FMT_DISKIO;
	char  buf[64];

	type = dio->type;
	

	if (type == P_CS_ReadChunk || type == P_CS_WriteChunk || type == P_CS_Originated_Read ||
	    type == P_CS_Originated_Write || type == P_CS_MetaRead || type == P_CS_MetaWrite) {

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
		case P_CS_Originated_Read:
			p = "  RdDataCS";
			len = 10;
			break;
		case P_CS_Originated_Write:
			p = "  WrDataCS";
			len = 10;
			break;
		}
		strncpy(buf, p, len);
	} else {

		switch (type & P_DISKIO_TYPE) {

		case P_RdMeta:
			p = "  RdMeta";
			len = 8;
			break;
		case P_WrMeta:
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

		if (type & (P_DISKIO_ASYNC | P_DISKIO_THROTTLE | P_DISKIO_PASSIVE)) {
			buf[len++] = '[';
		
			if (type & P_DISKIO_ASYNC) {
				memcpy(&buf[len], "async", 5);
				len += 5;
			}
			if (type & P_DISKIO_THROTTLE)
				buf[len++] = 'T';
			else if (type & P_DISKIO_PASSIVE)
				buf[len++] = 'P';

			buf[len++] = ']';
		}
	}
	buf[len] = 0;

	if (check_filter_mode(NULL, type, 0, 0, buf))
		format_print(NULL, buf, dio->issuing_thread, type, 0, 0, 0, 0, format, dio->completed_time, dio->issued_time, 1, "", dio);
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


void recache_disk_names()
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


char *find_disk_name(int dev)
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
	return ("NOTFOUND");
}


char *generate_cs_disk_name(int dev, char *s)
{
	if (dev == -1)
		return ("UNKNOWN");
	
	sprintf(s, "disk%ds%d", (dev >> 16) & 0xffff, dev & 0xffff);

	return (s);
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
	
	if (filter_mode & EXEC_FILTER) {
		if (type == BSC_execve || type == BSC_posix_spawn)
			return(1);
		return(0);
	}

	if (filter_mode & PATHNAME_FILTER) {
            if (ti && ti->pathname[0])
	            return(1);
	    if (type == BSC_close || type == BSC_close_nocancel)
	            return(1);
	    return(0);
	}

	if ( !(filter_mode & (FILESYS_FILTER | NETWORK_FILTER)))
		return(1);

	if (ti == (struct th_info *)0) {
		if (filter_mode & FILESYS_FILTER)
			return(1);
		return(0);
	}

	switch (type) {

	case BSC_close:
	case BSC_close_nocancel:
	    fd = ti->arg1;
	    network_fd_isset = fs_usage_fd_isset(ti->thread, fd);

	    if (error == 0)
		    fs_usage_fd_clear(ti->thread,fd);
	    
	    if (network_fd_isset) {
		    if (filter_mode & NETWORK_FILTER)
			    ret = 1;
	    } else if (filter_mode & FILESYS_FILTER)
		    ret = 1;
	    break;

	case BSC_read:
	case BSC_write:
	case BSC_read_nocancel:
	case BSC_write_nocancel:
	    /*
	     * we don't care about error in these cases
	     */
	    fd = ti->arg1;
	    network_fd_isset = fs_usage_fd_isset(ti->thread, fd);

	    if (network_fd_isset) {
		    if (filter_mode & NETWORK_FILTER)
			    ret = 1;
	    } else if (filter_mode & FILESYS_FILTER)
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
	    /*
	     * Cannot determine info about file descriptors
	     */
	    if (filter_mode & NETWORK_FILTER)
		    ret = 1;
	    break;

	case BSC_dup:
	case BSC_dup2:
	    /*
	     * We track these cases for fd state only
	     */
	    fd = ti->arg1;
	    network_fd_isset = fs_usage_fd_isset(ti->thread, fd);

	    if (error == 0 && network_fd_isset) {
		    /*
		     * then we are duping a socket descriptor
		     */
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
}


int
get_real_command_name(int pid, char *cbuf, int csize)
{
	/*
	 * Get command and arguments.
	 */
	char	*cp;
	int	mib[4];
	char    *command_beg, *command, *command_end;

	if (cbuf == NULL)
		return(0);

	if (arguments)
		bzero(arguments, argmax);
	else
		return(0);

	/*
	 * A sysctl() is made to find out the full path that the command
	 * was called with.
	 */
	mib[0] = CTL_KERN;
	mib[1] = KERN_PROCARGS2;
	mib[2] = pid;
	mib[3] = 0;

	if (sysctl(mib, 3, arguments, (size_t *)&argmax, NULL, 0) < 0)
		return(0);

	/*
	 * Skip the saved exec_path
	 */
	for (cp = arguments; cp < &arguments[argmax]; cp++) {
		if (*cp == '\0') {
			/*
			 * End of exec_path reached
			 */
			break;
		}
	}
	if (cp == &arguments[argmax])
		return(0);

	/*
	 * Skip trailing '\0' characters
	 */
	for (; cp < &arguments[argmax]; cp++) {
		if (*cp != '\0') {
			/*
			 * Beginning of first argument reached
			 */
			break;
		}
	}
	if (cp == &arguments[argmax])
		return(0);

	command_beg = cp;
	/*
	 * Make sure that the command is '\0'-terminated.  This protects
	 * against malicious programs; under normal operation this never
	 * ends up being a problem..
	 */
	for (; cp < &arguments[argmax]; cp++) {
		if (*cp == '\0') {
			/*
			 * End of first argument reached
			 */
			break;
		}
	}
	if (cp == &arguments[argmax])
		return(0);

	command_end = command = cp;

	/*
	 * Get the basename of command
	 */
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
