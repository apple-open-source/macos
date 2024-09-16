/*
 * Copyright (c) 2000-2021 Apple Inc. All rights reserved.
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
/* Copyright (c) 1995, 1997 Apple Computer, Inc. All Rights Reserved */
/*
 * Copyright (c) 1982, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)file.h	8.3 (Berkeley) 1/9/95
 */

#ifndef _SYS_FILE_INTERNAL_H_
#define _SYS_FILE_INTERNAL_H_

#include <sys/appleapiopts.h>
#include <sys/fcntl.h>
#include <sys/unistd.h>

#ifdef XNU_KERNEL_PRIVATE
#include <sys/errno.h>
#include <sys/queue.h>
#include <sys/cdefs.h>
#include <sys/constrained_ctypes.h>
#include <sys/lock.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/guarded.h>
#include <os/refcnt.h>

__BEGIN_DECLS

#pragma GCC visibility push(hidden)

struct proc;
struct uio;
struct knote;
struct kevent_qos_s;
struct file;
#ifndef _KAUTH_CRED_T
#define _KAUTH_CRED_T
typedef struct ucred *kauth_cred_t;
typedef struct posix_cred *posix_cred_t;
#endif  /* !_KAUTH_CRED_T */

__options_decl(fileproc_vflags_t, unsigned int, {
	FPV_NONE        = 0,
	FPV_DRAIN       = 0x01,
});

__options_decl(fileproc_flags_t, uint16_t, {
	FP_NONE         = 0,
	FP_CLOEXEC      = 0x01,
	FP_CLOFORK      = 0x02,
	FP_INSELECT     = 0x04,
	FP_AIOISSUED    = 0x08,
	FP_SELCONFLICT  = 0x10,  /* select conflict on an individual fp */
});

struct fileproc_guard {
	struct select_set *fpg_wset;
	guardid_t         fpg_guard;
};

/*
 * Kernel descriptor table.
 * One entry for each open kernel vnode and socket.
 */
struct fileproc {
	os_refcnt_t      fp_iocount;
	_Atomic fileproc_vflags_t fp_vflags;
	fileproc_flags_t fp_flags;
	uint16_t         fp_guard_attrs;
	struct fileglob *XNU_PTRAUTH_SIGNED_PTR("fileproc.fp_glob") fp_glob;
	union {
		struct select_set     *fp_wset;   /* fp_guard_attrs == 0 */
		struct fileproc_guard *XNU_PTRAUTH_SIGNED_PTR("fileproc.fp_guard") fp_guard;  /* fp_guard_attrs != 0 */
	};
};
__CCT_DECLARE_CONSTRAINED_PTR_TYPES(struct fileproc, fileproc);
#define FILEPROC_NULL ((struct fileproc *)0)

/* file types */
typedef enum {
	DTYPE_VNODE     = 1,    /* file */
	DTYPE_SOCKET,           /* communications endpoint */
	DTYPE_PSXSHM,           /* POSIX Shared memory */
	DTYPE_PSXSEM,           /* POSIX Semaphores */
	DTYPE_KQUEUE,           /* kqueue */
	DTYPE_PIPE,             /* pipe */
	DTYPE_FSEVENTS,         /* fsevents */
	DTYPE_ATALK,            /* (obsolete) */
	DTYPE_NETPOLICY,        /* networking policy */
	DTYPE_CHANNEL,          /* Skywalk Channel */
	DTYPE_NEXUS             /* Skywalk Nexus */
} file_type_t;

/* defines for fg_lflags */
// was  FG_TERM         0x01
#define FG_INSMSGQ      0x02    /* insert to msgqueue pending .. */
#define FG_WINSMSGQ     0x04    /* wait for the fielglob is in msgque */
#define FG_RMMSGQ       0x08    /* the fileglob is being removed from msgqueue */
#define FG_WRMMSGQ      0x10    /* wait for the fileglob to  be removed from msgqueue */
#define FG_PORTMADE     0x20    /* a port was at some point created for this fileglob */
#define FG_NOSIGPIPE    0x40    /* don't deliver SIGPIPE with EPIPE return */
#define FG_OFF_LOCKED   0x80    /* Used as a mutex for offset changes (for vnodes) */
#define FG_OFF_LOCKWANT 0x100   /* Somebody's wating for the lock */
#define FG_CONFINED     0x200   /* fileglob confined to process, immutably */
#define FG_HAS_OFDLOCK  0x400   /* Has or has had an OFD lock */

struct fileops {
	file_type_t     fo_type;        /* descriptor type */
	int (*fo_read)      (struct fileproc *fp, struct uio *uio,
	    int flags, vfs_context_t ctx);
	int (*fo_write)     (struct fileproc *fp, struct uio *uio,
	    int flags, vfs_context_t ctx);
#define FOF_OFFSET      0x00000001      /* offset supplied to vn_write */
	int (*fo_ioctl)(struct fileproc *fp, u_long com,
	    caddr_t data, vfs_context_t ctx);
	int (*fo_select)    (struct fileproc *fp, int which,
	    void *wql, vfs_context_t ctx);
	int (*fo_close)     (struct fileglob *fg, vfs_context_t ctx);
	int (*fo_kqfilter)  (struct fileproc *fp, struct knote *, struct kevent_qos_s *);
	int (*fo_drain)     (struct fileproc *fp, vfs_context_t ctx);
};

struct fileglob {
	LIST_ENTRY(fileglob) f_msglist;     /* list of files in unix messages */
	uint32_t             fg_flag;       /* (atomic) see fcntl.h */
	os_ref_atomic_t      fg_count;      /* reference count */
	uint32_t             fg_msgcount;   /* references from message queue */
	int32_t              fg_lflags;     /* file global flags */
	kauth_cred_t         XNU_PTRAUTH_SIGNED_PTR("fileglob.fg_cred") fg_cred;        /* credentials associated with descriptor */
	const struct fileops *XNU_PTRAUTH_SIGNED_PTR("fileglob.fg_ops") fg_ops;
	off_t                fg_offset;
	uintptr_t            fg_data;       /* vnode or socket or SHM or semaphore */
	struct fd_vn_data   *XNU_PTRAUTH_SIGNED_PTR("fileglob.fg_vn_data") fg_vn_data;  /* Per fd vnode data, used for directories */
	lck_mtx_t            fg_lock;
#if CONFIG_MACF && CONFIG_VNGUARD
	struct vng_owner    *fg_vgo;        /* Used by the vnode guard MAC hook */
#endif
};

/* Disambiguate OFD ids from flock ids (fileglobs) */
__pure2
static inline caddr_t __unsafe_indexable
ofd_to_id(const struct fileglob *fg)
{
	return (caddr_t __unsafe_indexable)~(uintptr_t)fg;
}

extern int maxfiles;                    /* kernel limit on number of open files */
extern int nfiles;                      /* actual number of open files */
extern int maxfilesperproc;
os_refgrp_decl_extern(f_refgrp);        /* os_refgrp_t for file refcounts */

#define FILEGLOB_DTYPE(fg)              ((const file_type_t)((fg)->fg_ops->fo_type))

/* Special value to indicate "no process association". */
#define FG_NOPROC       ((struct proc *)~0UL)

#pragma mark files (struct fileglob)

/*!
 * @function fg_alloc_init
 *
 * @brief
 * Allocate and minimally initialize a file structure.
 *
 * @description
 * The fileglob is allocated as if with falloc_withinit(), but is not
 * assocated with any fileproc.
 */
struct fileglob *
fg_alloc_init(vfs_context_t ctx);

/*!
 * @function fg_ref
 *
 * @brief
 * Acquire a file reference on the specified file.
 *
 * @description
 * The @c proc must be locked while this operation is being performed
 * to avoid races with setting the FG_CONFINED flag.
 *
 * @param proc
 * The proc this file reference is taken on behalf of, or FG_NOPROC
 * if the file is guaranteed to not be associated with any fileproc.
 *
 * @param fg
 * The specified file
 */
void
fg_ref(proc_t proc, struct fileglob *fg);

/*!
 * @function fg_drop_live
 *
 * @brief
 * Drops a file reference on the specified file that isn't the last one.
 *
 * @param fg
 * The file whose reference is being dropped.
 */
void
fg_drop_live(struct fileglob *fg);

/*!
 * @function fg_drop
 *
 * @brief
 * Drops a file reference on the specified file.
 *
 * @discussion
 * No locks should be held when calling this function.
 *
 * @param p
 * The process making the request,
 * or FG_NOPROC if the file belongs to a message/fileport.
 *
 * @param fg
 * The file being closed.
 *
 * @returns
 * 0          Success
 * ???        Any error that @c fileops::fo_close can return
 */
int
fg_drop(proc_t p, struct fileglob *fg);

/*!
 * @function fg_sendable
 *
 * @brief
 * Returns whether a particular file can be sent over IPC.
 */
bool
fg_sendable(struct fileglob *fg);

/*!
 * @function fg_get_data_volatile
 *
 * @brief
 * Returns the fileglob opaque data pointer.
 *
 * @discussion
 * Unlike @c fg_get_data() this variant will
 * not be hoisted by the compiler.
 *
 * @param fg
 * The file whose data is being requested.
 */
void *
fg_get_data_volatile(struct fileglob *fg);

/*!
 * @function fg_get_data
 *
 * @brief
 * Returns the fileglob opaque data pointer.
 *
 * @discussion
 * Unlike @c fg_get_data_volatile() this variant
 * will be hoisted by the compiler if it is called
 * repeatedly.
 *
 * @param fg
 * The file whose data is being requested.
 */
__pure2
static inline void *
fg_get_data(struct fileglob *fg)
{
	return fg_get_data_volatile(fg);
}

/*!
 * @function fg_set_data
 *
 * @brief
 * Sets the fileglob opaque data pointer.
 *
 * @param fg
 * The file whose data is being set.
 *
 * @param fg_data
 * Opaque file data value
 */
void
fg_set_data(struct fileglob *fg, void *fg_data);

#pragma mark file descriptor entries (struct fileproc)

/*!
 * @function fg_get_data
 *
 * @brief
 * Returns the fileproc fileglob opaque data pointer.
 *
 * @discussion
 * Unlike @c fp_get_data_volatile() this variant
 * will be hoisted by the compiler if it is called
 * repeatedly.
 *
 * @param fp
 * The fileproc whose data is being requested.
 */
__pure2
static inline void *
fp_get_data(struct fileproc *fp)
{
	return fg_get_data(fp->fp_glob);
}

/*!
 * @function fp_get_data
 *
 * @brief
 * Returns the fileproc fileglob opaque data pointer.
 *
 * @discussion
 * Unlike @c fp_get_data() this variant will
 * not be hoisted by the compiler.
 *
 * @param fp
 * The fileproc whose data is being requested.
 */
static inline void *
fp_get_data_volatile(struct fileproc *fp)
{
	return fg_get_data_volatile(fp->fp_glob);
}

/*!
 * @function fp_set_data
 *
 * @brief
 * Sets the fileproc opaque data pointer.
 *
 * @param fp
 * The fileproc whose data is being set.
 *
 * @param fg_data
 * Opaque file data value
 */
static inline void
fp_set_data(struct fileproc *fp, void *fg_data)
{
	fg_set_data(fp->fp_glob, fg_data);
}

/*!
 * @function fp_get_ftype
 *
 * @brief
 * Get the fileproc pointer for the given fd from the per process, with the
 * specified file type, and with an I/O reference.
 *
 * @param p
 * The process in which fd lives.
 *
 * @param fd
 * The file descriptor index to lookup.
 *
 * @param ftype
 * The required file type.
 *
 * @param err
 * The error to return if the file exists but isn't of the specified type.
 *
 * @param fpp
 * The returned fileproc when the call returns 0.
 *
 * @returns
 * 0            Success (@c fpp is set)
 * EBADF        Bad file descriptor
 * @c err       There is an entry, but it isn't of the specified type.
 */
extern int
fp_get_ftype(proc_t p, int fd, file_type_t ftype, int err, struct fileproc **fpp);

/*!
 * @function fp_get_noref_locked
 *
 * @brief
 * Get the fileproc pointer for the given fd from the per process
 * open file table without taking an explicit reference on it.
 *
 * @description
 * This function assumes that the @c proc_fdlock is held, as the caller
 * doesn't hold an I/O reference for the returned fileproc.
 *
 * Because there is no reference explicitly taken, the returned
 * fileproc pointer is only valid so long as the @c proc_fdlock
 * remains held by the caller.
 *
 * @param p
 * The process in which fd lives.
 *
 * @param fd
 * The file descriptor index to lookup.
 *
 * @returns
 * - the fileproc on success
 * - FILEPROC_NULL on error
 */
extern struct fileproc *
fp_get_noref_locked(proc_t p, int fd);

/*!
 * @function fp_get_noref_locked_with_iocount
 *
 * @brief
 * Similar to fp_get_noref_locked(), but allows returning files that are
 * closing.
 *
 * @discussion
 * Some parts of the kernel take I/O references on fileprocs but only remember
 * the file descriptor for which they did that.
 *
 * These interfaces later need to drop that reference, but if the file is
 * already closing, then fp_get_noref_locked() will refuse to resolve
 * the file descriptor.
 *
 * This interface allows the lookup (but will assert that the fileproc has
 * enouhg I/O references).
 *
 * @warning
 * New code should NOT use this function, it is required for interfaces
 * that acquire iocounts without remembering the fileproc pointer,
 * which is bad practice.
 */
extern struct fileproc *
fp_get_noref_locked_with_iocount(proc_t p, int fd);

/*!
 * @function fp_close_and_unlock
 *
 * @brief
 * Close the given file descriptor entry.
 *
 * @description
 * This function assumes that the @c proc_fdlock is held,
 * and that the caller holds no additional I/O reference
 * on the specified file descriptor entry.
 *
 * The @c proc_fdlock is unlocked upon return.
 *
 * @param p             The process in which fd lives.
 * @param p_cred        The proc's cred for this operation.
 * @param fd            The file descriptor index being closed.
 * @param fp            The fileproc entry associated with @c fd.
 * @param flags         FD_DUP2RESV or 0.
 *
 * @returns
 * 0            Success
 * EBADF        Bad file descriptor
 * ???          Any error that @c fileops::fo_close can return.
 */
extern int
fp_close_and_unlock(proc_t p, kauth_cred_t p_cred, int fd, struct fileproc *fp, int flags);

/* wrappers for fp->f_ops->fo_... */
int fo_read(struct fileproc *fp, struct uio *uio, int flags, vfs_context_t ctx);
int fo_write(struct fileproc *fp, struct uio *uio, int flags,
    vfs_context_t ctx);
int fo_ioctl(struct fileproc *fp, u_long com, caddr_t data, vfs_context_t ctx);
int fo_select(struct fileproc *fp, int which, void *wql, vfs_context_t ctx);
int fo_close(struct fileglob *fg, vfs_context_t ctx);
int fo_drain(struct fileproc *fp, vfs_context_t ctx);
int fo_kqfilter(struct fileproc *fp, struct knote *kn, struct kevent_qos_s *kev);

/* Functions to use for unsupported fileops */
int fo_no_read(struct fileproc *fp, struct uio *uio, int flags, vfs_context_t ctx);
int fo_no_write(struct fileproc *fp, struct uio *uio, int flags,
    vfs_context_t ctx);
int fo_no_ioctl(struct fileproc *fp, u_long com, caddr_t data, vfs_context_t ctx);
int fo_no_select(struct fileproc *fp, int which, void *wql, vfs_context_t ctx);
int fo_no_drain(struct fileproc *fp, vfs_context_t ctx);
int fo_no_kqfilter(struct fileproc *, struct knote *, struct kevent_qos_s *kev);

int fp_tryswap(proc_t, int fd, struct fileproc *nfp);
int fp_drop(struct proc *p, int fd, struct fileproc *fp, int locked);
void fp_free(struct proc * p, int fd, struct fileproc * fp);
int fp_lookup(struct proc *p, int fd, struct fileproc **resultfp, int locked);
int fp_lookup_guarded(struct proc *p, int fd, guardid_t guard, struct fileproc **resultfp, int locked);
int fp_isguarded(struct fileproc *fp, u_int attribs);
int fp_guard_exception(proc_t p, int fd, struct fileproc *fp, u_int attribs);
struct nameidata;
struct vnode_attr;
int open1(vfs_context_t ctx, struct nameidata *ndp, int uflags,
    struct vnode_attr *vap, fp_initfn_t fp_init, void *initarg,
    int32_t *retval, int authfd);
int chdir_internal(proc_t p, vfs_context_t ctx, struct nameidata *ndp, int per_thread);
int kqueue_internal(struct proc *p, fp_initfn_t, void *initarg, int32_t *retval);
void procfdtbl_releasefd(struct proc * p, int fd, struct fileproc * fp);
extern struct fileproc *fileproc_alloc_init(void);
extern void fileproc_free(struct fileproc *fp);
extern void guarded_fileproc_copy_guard(struct fileproc *ofp, struct fileproc *nfp);
extern void guarded_fileproc_unguard(struct fileproc *fp);
extern void fg_vn_data_free(void *fgvndata);
extern int nameiat(struct nameidata *ndp, int dirfd);
extern void vn_offset_lock(struct fileglob *fg);
extern void vn_offset_unlock(struct fileglob *fg);
extern int falloc_guarded(struct proc *p, struct fileproc **fp, int *fd,
    vfs_context_t ctx, const guardid_t *guard, u_int attrs);
extern void fileproc_modify_vflags(struct fileproc *fp, fileproc_vflags_t vflags, boolean_t clearflags);
fileproc_vflags_t fileproc_get_vflags(struct fileproc *fp);

#pragma mark internal version of syscalls

int fileport_makefd(proc_t p, ipc_port_t port, fileproc_flags_t fp_flags, int *fd);
int dup2(proc_t p, kauth_cred_t p_cred, int from, int to, int *fd);
int close_nocancel(proc_t p, kauth_cred_t p_cred, int fd);
int fchdir(proc_t p, vfs_context_t ctx, int fd, bool per_thread);

#pragma GCC visibility pop

__END_DECLS

#endif /* XNU_KERNEL_PRIVATE */

#endif /* !_SYS_FILE_INTERNAL_H_ */
