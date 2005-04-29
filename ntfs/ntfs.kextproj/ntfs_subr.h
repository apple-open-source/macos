/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
/*	$NetBSD: ntfs_subr.h,v 1.8 1999/10/10 14:48:37 jdolecek Exp $	*/

/*-
 * Copyright (c) 1998, 1999 Semen Ustimenko
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/fs/ntfs/ntfs_subr.h,v 1.12 2002/03/19 22:20:11 alfred Exp $
 */

#define	VA_LOADED		0x0001
#define	VA_PRELOADED		0x0002

struct ntvattr {
	LIST_ENTRY(ntvattr) 	va_list;

	u_int32_t		va_vflag;
	vnode_t			va_vp;
	struct ntnode 	       *va_ip;

	u_int32_t		va_flag;
	u_int32_t		va_type;
	u_int8_t		va_namelen;
	char			va_name[NTFS_MAXATTRNAME];

	u_int32_t		va_compression;
	u_int32_t		va_compressalg;
	u_int64_t		va_datalen;
	u_int64_t		va_allocated;
	cn_t	 		va_vcnstart;
	cn_t	 		va_vcnend;
	u_int16_t		va_index;
	union {
		struct {
			cn_t *		cn;
			cn_t *		cl;
			u_long		cnt;
		} vrun;
		caddr_t		datap;
		struct attr_name *name;
		struct attr_indexroot *iroot;
		struct attr_indexalloc *ialloc;
	} va_d;
};
#define	va_vruncn	va_d.vrun.cn
#define va_vruncl	va_d.vrun.cl
#define va_vruncnt	va_d.vrun.cnt
#define va_datap	va_d.datap
#define va_a_name	va_d.name
#define va_a_iroot	va_d.iroot
#define va_a_ialloc	va_d.ialloc

struct componentname;
struct fnode;
struct uio;

int ntfs_parserun( cn_t *, cn_t *, u_int8_t *, u_long, u_long *);
int ntfs_runtocn( cn_t *, struct ntfsmount *, u_int8_t *, u_long, cn_t);
int ntfs_readattr( struct ntfsmount *, struct ntnode *, u_int32_t, char *, off_t, size_t, void *, struct uio *, proc_t);
int ntfs_filesize( struct ntfsmount *, struct fnode *, proc_t, u_int64_t *, u_int64_t *);
int ntfs_times( struct ntfsmount *, struct ntnode *, vfs_context_t, ntfs_times_t *);
struct timespec	ntfs_nttimetounix( u_int64_t );
int ntfs_ntreaddir( struct ntfsmount *, struct fnode *, u_int32_t, struct attr_indexentry **, proc_t);
int ntfs_loadntvattrs( struct ntfsmount *, vnode_t, caddr_t, struct ntvattr **);
struct ntvattr * ntfs_findntvattr( struct ntfsmount *, struct ntnode *, u_int32_t, cn_t );
int ntfs_ntlookupfile(struct ntfsmount *, vnode_t, struct componentname *, proc_t, vnode_t *);
int ntfs_isnamepermitted(struct ntfsmount *, struct attr_indexentry * );
int ntfs_ntvattrrele(struct ntvattr * );
int ntfs_ntvattrget(struct ntfsmount *, struct ntnode *, u_int32_t, const char *, cn_t , proc_t, struct ntvattr **);
int ntfs_ntlookup(struct ntfsmount *, ino_t, struct ntnode **);
int ntfs_ntget(struct ntnode *);
void ntfs_ntput(struct ntnode *);
int ntfs_loadntnode(struct ntfsmount *, struct ntnode *, proc_t);
int ntfs_writentvattr_plain(struct ntfsmount *, struct ntnode *, struct ntvattr *, off_t, size_t, void *, size_t *, struct uio *);
int ntfs_writeattr_plain(struct ntfsmount *, struct ntnode *, u_int32_t, char *, off_t, size_t, void *, size_t *, struct uio *);
void ntfs_toupper_init(void);
int ntfs_toupper_use(mount_t, struct ntfsmount *, proc_t);
void ntfs_toupper_unuse(void);
int ntfs_fget(struct ntfsmount *, struct ntnode *, int, char *, struct fnode **);
void ntfs_frele(struct fnode *);
