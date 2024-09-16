/*
 * Copyright (c) 2000-2004 Apple Computer, Inc. All rights reserved.
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
/* Copyright (c) 1995 NeXT Computer, Inc. All Rights Reserved */
/*
 * Copyright (c) 1982, 1986, 1993, 1994
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
 *	@(#)uio.h	8.5 (Berkeley) 2/22/94
 */

#ifndef _SYS_UIO_INTERNAL_H_
#define _SYS_UIO_INTERNAL_H_

#include <sys/appleapiopts.h>

#ifdef KERNEL_PRIVATE
#include <sys/uio.h>
#include <sys/malloc.h>
#include <sys/param.h>

/*
 * user / kernel address space type flags.
 * WARNING - make sure to check when adding flags!  Be sure new flags
 * don't overlap the definitions in uio.h
 */
//      UIO_USERSPACE           0       defined in uio.h
#define UIO_USERISPACE          ((enum uio_seg)1)
//      UIO_SYSSPACE            2       defined in uio.h
#define UIO_PHYS_USERSPACE      ((enum uio_seg)3)
#define UIO_PHYS_SYSSPACE       ((enum uio_seg)4)
//      UIO_USERSPACE32         5       defined in uio.h
#define UIO_USERISPACE32        ((enum uio_seg)6)
#define UIO_PHYS_USERSPACE32    ((enum uio_seg)7)
//      UIO_USERSPACE64         8       defined in uio.h
#define UIO_USERISPACE64        ((enum uio_seg)9)
#define UIO_PHYS_USERSPACE64    ((enum uio_seg)10)
//      UIO_SYSSPACE32          11      defined in uio.h
//      UIO_PHYS_SYSSPACE32     12      reserved, never used. Use UIO_PHYS_SYSSPACE
//      UIO_SYSSPACE64          13      reserved, never used. Use UIO_SYSSPACE
//      UIO_PHYS_SYSSPACE64     14      reserved, never used. Use UIO_PHYS_SYSSPACE

enum {
	UIOF_USERISPACE          = (1 << UIO_USERISPACE),
	UIOF_PHYS_USERSPACE      = (1 << UIO_PHYS_USERSPACE),
	UIOF_PHYS_SYSSPACE       = (1 << UIO_PHYS_SYSSPACE),
	UIOF_USERISPACE32        = (1 << UIO_USERISPACE32),
	UIOF_PHYS_USERSPACE32    = (1 << UIO_PHYS_USERSPACE32),
	UIOF_USERISPACE64        = (1 << UIO_USERISPACE64),
	UIOF_PHYS_USERSPACE64    = (1 << UIO_PHYS_USERSPACE64),
};

__BEGIN_DECLS
struct user_iovec;

#ifdef XNU_KERNEL_PRIVATE
__private_extern__ struct user_iovec * uio_iovsaddr_user( uio_t a_uio );
__private_extern__ int uio_calculateresid_user(uio_t __attribute((nonnull)) a_uio);
__private_extern__ uio_t  uio_createwithbuffer( int a_iovcount, off_t a_offset, int a_spacetype, int a_iodirection, void *a_buf_p, size_t a_buffer_size );
__private_extern__ int copyin_user_iovec_array(user_addr_t uaddr, int spacetype, int count, struct user_iovec *dst);

/*
 * uio_reset_fast - reset an uio_t.
 *      Reset the given uio_t to initial values.  The uio_t is not fully initialized
 *      until all iovecs are added using uio_add_ov calls.
 *	The a_iovcount value passed in the uio_create is the maximum number of
 *	iovecs you may add.
 *	a_spacetype must NOT be UIO_USERSPACE, UIO_SYSSPACE32 or UIO_PHYS_USERSPACE.
 */
__private_extern__ void uio_reset_fast( uio_t a_uio,
    off_t a_offset,                                             /* current offset */
    int a_spacetype,                                            /* type of address space */
    int a_iodirection );                                /* read or write flag */

__private_extern__ int uio_copyout_user(const char *__sized_by(n) c_cp, int n, uio_t uio);
__private_extern__ int uio_copyin_user(const char *__sized_by(n) c_cp, int n, uio_t uio);
__private_extern__ int uio_copyout_sys(const char *__sized_by(n) c_cp, int n, uio_t uio);
__private_extern__ int uio_copyin_sys(const char *__sized_by(n) c_cp, int n, uio_t uio);
__private_extern__ int uio_copyout_phys_user(const char *__sized_by(n) c_cp, int n, uio_t uio);
__private_extern__ int uio_copyin_phys_user(const char *__sized_by(n) c_cp, int n, uio_t uio);
__private_extern__ int uio_copyout_phys_sys(const char *__sized_by(n) c_cp, int n, uio_t uio);
__private_extern__ int uio_copyin_phys_sys(const char *__sized_by(n) c_cp, int n, uio_t uio);

#endif /* XNU_KERNEL_PRIVATE */

/* use kern_iovec for system space requests */
struct kern_iovec {
	u_int64_t       iov_base;       /* Base address; PAC signed, diversified with length. */
	u_int64_t       iov_len;        /* Length. */
};

/* use user_iovec for user space requests */
struct user_iovec {
	user_addr_t     iov_base;       /* Base address. */
	user_size_t     iov_len;        /* Length. */
};

/* use user32_iovec/user64_iovec for representing
 * in-memory structures in 32-64 processes during copyin */
struct user32_iovec {
	uint32_t        iov_base;       /* Base address. */
	uint32_t        iov_len;        /* Length. */
};

struct user64_iovec {
	uint64_t        iov_base;       /* Base address. */
	uint64_t        iov_len;        /* Length. */
};

union iovecs {
	struct kern_iovec       *kiovp;
	struct user_iovec       *uiovp;
};

/* WARNING - use accessor calls for uio_iov and uio_resid since these */
/* fields vary depending on the originating address space. */
struct uio {
	void * XNU_PTRAUTH_SIGNED_PTR("uio.uio_iovs") uio_iovs;       /* current iovec */
	void * XNU_PTRAUTH_SIGNED_PTR("uio.uio_iovbase") uio_iovbase; /* iovec base */
	int             uio_max_iovs;   /* max number of iovecs this uio_t can hold */
	int             uio_iovcnt;     /* active iovecs */
	off_t           uio_offset;
	enum uio_seg    uio_segflg;
	enum uio_rw     uio_rw;
	user_size_t     uio_resid_64;
	u_int32_t       uio_flags;
};

/* values for uio_flags */
#define UIO_FLAGS_INITED                0x00000001
#define UIO_FLAGS_WE_ALLOCED    0x00000002
#define UIO_FLAGS_IS_COMPRESSED_FILE    0x00000004

__END_DECLS

#define UIO_SIZEOF_IOVS( a_iovcount ) \
	( MAX(sizeof(struct user_iovec), sizeof(struct kern_iovec)) * (a_iovcount) )

/*
 * UIO_SIZEOF - return the amount of space a uio_t requires to
 *	contain the given number of iovecs.
 */
#ifndef P2ROUNDUP
#define P2ROUNDUP(x, align)     (-(-((long)x) & -((long)align)))
#endif
#define UIO_SIZEOF( a_iovcount ) \
	( P2ROUNDUP(sizeof(struct uio), sizeof(uint64_t)) \
	  + UIO_SIZEOF_IOVS(a_iovcount) )

/*
 * UIO_STACKBUF - use this macro to declare a stack buffer with the
 *	appropriate size an alignment for the given number of iovecs.
 *	The resulting buffer can then be passed to uio_createwithbuffer().
 */
#define UIO_STACKBUF(name, niov) \
	uint64_t name[P2ROUNDUP(UIO_SIZEOF(niov), sizeof(uint64_t)) \
	    / sizeof(uint64_t)]

#define UIO_IS_USER_SPACE32( a_uio_t )  \
	( (1 << (a_uio_t)->uio_segflg) & (UIOF_USERSPACE32 | UIOF_PHYS_USERSPACE32 | UIOF_USERISPACE32))
#define UIO_IS_USER_SPACE64( a_uio_t )  \
	( (1 << (a_uio_t)->uio_segflg) & (UIOF_USERSPACE64 | UIOF_PHYS_USERSPACE64 | UIOF_USERISPACE64))
#define UIO_IS_USER_SPACE( a_uio_t )  \
	( (1 << (a_uio_t)->uio_segflg) & (UIOF_USERSPACE32 | UIOF_PHYS_USERSPACE32 | UIOF_USERISPACE32 | \
	                                  UIOF_USERSPACE64 | UIOF_PHYS_USERSPACE64 | UIOF_USERISPACE64 | \
	                                  UIOF_USERSPACE | UIOF_PHYS_USERSPACE | UIOF_USERISPACE))
#define UIO_IS_SYS_SPACE( a_uio_t )  \
	( (1 << (a_uio_t)->uio_segflg) & (UIOF_SYSSPACE | UIOF_PHYS_SYSSPACE | UIOF_SYSSPACE32))

extern int ureadc(int c, struct uio *uio);

#endif /* KERNEL_PRIVATE */
#endif /* !_SYS_UIO_INTERNAL_H_ */
